#include <assert.h>
#include <bsd/stdlib.h>
#include <iostream>

#include <emmintrin.h> // SSE2
#include <tmmintrin.h> // SSSE3
#include <immintrin.h> // AVX2 (block<__m256i> used by the LowMC audit)

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <future>
#include <tuple>
#include <utility>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "dpf.hpp"
#include "types.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <emmintrin.h>
#include <type_traits>

#include <cstdlib>   // [FIG3B] std::getenv / std::strtol
static const AES_KEY default_aes_key{};

// ============================================================================
// LowMC bitsliced audit (merged from prg_mpc.cpp / prg-bitsliced)
// ============================================================================
#include "lowmc/lowmc.h"
#include "lowmc/constants_b128_r29_s11.h"
#include "lowmc/streams.h"
#include "block.h"

using namespace dpf;

// ----------------------------------------------------------------------------
// LowMC audit types
// ----------------------------------------------------------------------------
using BitslicedLowMC = lowmc::bitsliced_lowmc<128, 29, 11, 256>;

constexpr size_t kLowMCRounds = BitslicedLowMC::num_rounds;        // 29
constexpr size_t kLowMCSboxes = BitslicedLowMC::sboxes_per_round;  // 11

using LowMCBlockArray = std::array<block<__m256i>, 128>;
using LowMCBlindArray = std::array<block<__m256i>, 3 * kLowMCSboxes>;

// ----------------------------------------------------------------------------
// Synchronous channel that lets the (blocking) LowMC MPC ride NetPeer's socket.
// Used only inside the (synchronous) encrypt2_p0p1 audit; folds byte counts into
// NetPeer::stats so ctx.bytes_sent()/received() include the audit traffic.
// ----------------------------------------------------------------------------
struct NetPeerLowMCChannel {
    NetPeer &peer;
    explicit NetPeerLowMCChannel(NetPeer &p) : peer(p) {}

    template <std::size_t N>
    NetPeerLowMCChannel &operator<<(const std::array<block<__m256i>, N> &v) {
        const std::size_t nbytes = N * sizeof(block<__m256i>);
        boost::asio::write(peer.sock, boost::asio::buffer(v.data(), nbytes));
        peer.stats.bytes_sent += nbytes;
        return *this;
    }

    template <std::size_t N>
    NetPeerLowMCChannel &operator>>(std::array<block<__m256i>, N> &v) {
        const std::size_t nbytes = N * sizeof(block<__m256i>);
        boost::asio::read(peer.sock, boost::asio::buffer(v.data(), nbytes));
        peer.stats.bytes_recv += nbytes;
        return *this;
    }
};

template <size_t LEAF_SIZE>
awaitable<void> run_party_impl(boost::asio::io_context &io, NetContext &ctx, Role role,
                               const size_t log_nitems, double authorized_fraction,
                               size_t num_requests) {
    using leaf_t = std::array<__m128i, LEAF_SIZE>;

    // =====================================================
    // Common setup
    // =====================================================
    const size_t nitems = 1ULL << log_nitems;
    const size_t target_ind = 44;

    leaf_t target_val{};
    leaf_t target_val_recv{};
    if (role == Role::P0) arc4random_buf(&target_val, sizeof(leaf_t));

    std::cout << "target_val = " << target_val[0][0] << std::endl;
    leaf_t *DB = (leaf_t *)aligned_alloc(16, nitems * sizeof(leaf_t));
    __m128i *ProofDB = (__m128i *)aligned_alloc(16, nitems * sizeof(__m128i));
    arc4random_buf(ProofDB, nitems * sizeof(__m128i));

    uint8_t *a = static_cast<uint8_t *>(malloc(log_nitems));
    uint8_t *b = static_cast<uint8_t *>(malloc(log_nitems));
    if (!a || !b) throw std::bad_alloc();

    std::mt19937_64 gen2(999);
    std::uniform_int_distribution<int> bitdist(0, 1);
    for (size_t i = 0; i < log_nitems; ++i) {
        size_t bitpos = log_nitems - 1 - i;
        uint8_t bit = (target_ind >> bitpos) & 1;
        a[i] = bitdist(gen2);
        b[i] = a[i] ^ bit;
    }
    size_t reconstructed = 0;
    for (size_t i = 0; i < log_nitems; ++i)
        reconstructed |= (size_t(a[i] ^ b[i]) << (log_nitems - 1 - i));
    assert(reconstructed == target_ind);

    AES_KEY prgkey;
    leaf_t target_value;
    leaf_t FCW;
    set_target_values(target_value, 100, 300);

    auto [k0, k1] =
        dpf_key<leaf_t, __m128i, AES_KEY>::gen(prgkey, nitems, target_ind, target_value);

    // ---- LowMC audit state (set up once) ----
    BitslicedLowMC lowmc_prgkey;
    static LowMCBlindArray lowmc_blinds[kLowMCRounds];
    static LowMCBlindArray lowmc_gamma_masks[kLowMCRounds];
    for (size_t i = 0; i < kLowMCRounds; ++i) {
        lowmc_blinds[i].fill(block<__m256i>{});
        lowmc_gamma_masks[i].fill(block<__m256i>{});
    }
    LowMCBlockArray lowmc_seed{};
    if (role == Role::P0) {
        for (size_t bit = 0; bit < 128; ++bit)
            for (size_t col = 0; col < 256; ++col)
                lowmc_seed[bit].bits[col] = (bit < 8) ? ((col >> bit) & 1) : 0;
    }
    const bool lowmc_party = (role == Role::P1);  // P1 -> true, P0 -> false

    // ---- Accumulators ----
    uint64_t total_eval_ms = 0;       // (eval placeholder)
    uint64_t total_audit_ms = 0;      // LowMC audit, per request
    uint64_t total_write_ms = 0;      // whole write block, per authorized request
    uint64_t total_online_ms = 0;     // ONLINE = finalize -> FCW -> DB update, per authorized
    uint64_t total_finalize_ms = 0;
    uint64_t total_dbupdate_ms = 0;
    uint64_t total_comm_ms = 0;       // FCW exchange only
    uint64_t eval_bytes = 0, audit_bytes = 0, write_bytes = 0;
    size_t authorized_count = 0, cover_count = 0;

    // [FIG3B] microsecond-resolution audit accumulator (ms truncates to 0 at
    // small DB sizes) + latency/trial labels supplied by run_remise_sabre.sh.
    uint64_t total_audit_us = 0;
    uint64_t total_eval_us = 0;   // [MSGSWEEP] preprocess = eval (us precision)
    const char *rtt_env = std::getenv("REMISE_RTT_MS");
    const long fig3b_latency_ms = rtt_env ? std::strtol(rtt_env, nullptr, 10) : 0;
    const char *trial_env = std::getenv("REMISE_TRIAL");
    const long fig3b_trial = trial_env ? std::strtol(trial_env, nullptr, 10) : 0;

    auto run_online = [&](NetPeer &peer, MPCContext &mpc_ctx, auto &key,
                          uint8_t *mask_bits) -> awaitable<void> {

        std::mt19937 rng(12345);
        std::bernoulli_distribution auth_dist(authorized_fraction);

        leaf_t *output = new leaf_t[nitems];
        uint8_t *t = new uint8_t[nitems];

        NetPeerLowMCChannel lowmc_chan(peer);

        auto experiment_start = std::chrono::high_resolution_clock::now();

        for (size_t iter = 0; iter < num_requests; ++iter) {
            bool authorized = auth_dist(rng);
            if (authorized) ++authorized_count; else ++cover_count;

            using node_t = __m128i;
            node_t *final_nodes;
            uint8_t *final_flags;
            size_t nodes_in_interval;

            // ---- Eval  ----
            uint64_t eval_sent_before = ctx.bytes_sent();
            uint64_t eval_recv_before = ctx.bytes_received();
            auto start_eval = std::chrono::high_resolution_clock::now();
            auto end_eval = std::chrono::high_resolution_clock::now();
            total_eval_ms += std::chrono::duration_cast<std::chrono::milliseconds>(end_eval - start_eval).count();
            eval_bytes += (ctx.bytes_sent() - eval_sent_before) + (ctx.bytes_received() - eval_recv_before);

            // ---- Audit (bitsliced-LowMC MPC) ----
            uint64_t audit_sent_before = ctx.bytes_sent();
            uint64_t audit_recv_before = ctx.bytes_received();
            auto start_audit = std::chrono::high_resolution_clock::now();

            LowMCBlockArray lowmc_state = lowmc_seed;
            lowmc_state = lowmc_prgkey.encrypt2_p0p1(
                lowmc_state, lowmc_blinds, lowmc_gamma_masks, lowmc_chan, lowmc_party);
            asm volatile("" : : "m"(lowmc_state) : "memory");

            auto end_audit = std::chrono::high_resolution_clock::now();
            audit_bytes += (ctx.bytes_sent() - audit_sent_before) + (ctx.bytes_received() - audit_recv_before);
            const uint64_t audit_ms_this = std::chrono::duration_cast<std::chrono::milliseconds>(end_audit - start_audit).count();
            total_audit_ms += audit_ms_this;
            total_audit_us += std::chrono::duration_cast<std::chrono::microseconds>(end_audit - start_audit).count(); // [FIG3B]

            // ---- Write (authorized only) ----
            // ONLINE phase = finalize -> FCW exchange -> DB update.
            auto start_write = std::chrono::high_resolution_clock::now();

            if (authorized) {
                auto eval_t0 = std::chrono::high_resolution_clock::now();   // [MSGSWEEP] preprocess = eval
                co_await __evalinterval_mpc(mpc_ctx, peer, key, 0, nitems - 1, output, t, mask_bits,
                                            final_nodes, final_flags, nodes_in_interval, 8);
                auto eval_t1 = std::chrono::high_resolution_clock::now();
                total_eval_us += std::chrono::duration_cast<std::chrono::microseconds>(eval_t1 - eval_t0).count();

                // ===== ONLINE PHASE BEGINS (finalize) =====
                auto online_start = std::chrono::high_resolution_clock::now();

                auto t1 = online_start;
                finalize(key.prgkey, key.finalizer, output, final_nodes, nodes_in_interval, final_flags);
                FCW = output[0];
                for (size_t j = 1; j < nodes_in_interval; ++j) FCW ^= output[j];
                auto t2 = std::chrono::high_resolution_clock::now();
                total_finalize_ms += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

                // FCW exchange
                uint64_t write_sent_before = ctx.bytes_sent();
                uint64_t write_recv_before = ctx.bytes_received();
                auto comm_start = std::chrono::high_resolution_clock::now();

                leaf_t fcw_masked = FCW ^ target_val;   // named: outlives the co_await
                co_await ((peer << fcw_masked) && (peer >> target_val_recv));

                auto comm_end = std::chrono::high_resolution_clock::now();
                total_comm_ms += std::chrono::duration_cast<std::chrono::milliseconds>(comm_end - comm_start).count();
                write_bytes += (ctx.bytes_sent() - write_sent_before) + (ctx.bytes_received() - write_recv_before);

                target_val_recv ^= (FCW ^ target_val);

                // DB update
                auto t3 = std::chrono::high_resolution_clock::now();
                for (size_t j = 0; j < nitems; ++j) {
                    DB[j] ^= output[j];
                    if (t[j]) DB[j] ^= target_val_recv;
                }
                auto t4 = std::chrono::high_resolution_clock::now();
                total_dbupdate_ms += std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();

                // ===== ONLINE PHASE ENDS (after DB update) =====
                // online = audit + finalize + FCW + DB update (eval excluded).
                // The audit ran before this branch for every request; fold its
                // duration in here so authorized requests count audit in online.
                auto online_end = std::chrono::high_resolution_clock::now();
                total_online_ms += audit_ms_this/256.0 +
                    std::chrono::duration_cast<std::chrono::milliseconds>(online_end - online_start).count();
            }

            auto end_write = std::chrono::high_resolution_clock::now();
            if (authorized)
                total_write_ms += std::chrono::duration_cast<std::chrono::milliseconds>(end_write - start_write).count();
        }
        
        total_audit_us = total_audit_us/256;
        total_audit_ms = total_audit_ms/256;

        auto experiment_end = std::chrono::high_resolution_clock::now();
        double total_runtime_sec = (total_write_ms + total_audit_ms)/1000.0;
        
        std::cout << "Total Experiment Time (this includes a batch of 256 PRF evaluations): " << std::chrono::duration_cast<std::chrono::milliseconds>(experiment_end - experiment_start).count() / 1000.0;

        const double authd = double(authorized_count);

        // Wall-clock throughput over the whole experiment
        double throughput = double(num_requests) / total_runtime_sec;
        double goodput     = authd / total_runtime_sec;

        // Online phase = finalize + FCW + DB update only (per authorized request)
        double online_runtime_sec  = double(total_online_ms) / 1000.0;
        double online_throughput   = (online_runtime_sec > 0) ? double(num_requests) / online_runtime_sec : 0.0;
        double online_goodput      = (online_runtime_sec > 0) ? authd / online_runtime_sec : 0.0;

        uint64_t total_bytes = eval_bytes + audit_bytes + write_bytes;

        std::cout << "\n========== Experiment Summary ==========\n";
        std::cout << "Total requests           : " << num_requests << std::endl;
        std::cout << "Authorized requests      : " << authorized_count << std::endl;
        std::cout << "Unauthorized requests    : " << cover_count << std::endl;
        std::cout << "Authorized fraction      : " << authd / double(num_requests) << std::endl;

        std::cout << "\n--- Latency (per authorized request) ---\n";
        std::cout << "Average audit latency        : " << double(total_audit_ms) / num_requests << " ms\n";
        std::cout << "Average write latency        : " << double(total_write_ms) / authd << " ms\n";
        std::cout << "Average online latency       : " << double(total_online_ms) / authd
                  << " ms   (finalize + FCW + DB update)\n";
        std::cout << "  - finalize                 : " << double(total_finalize_ms) / authd << " ms\n";
        std::cout << "  - FCW exchange             : " << double(total_comm_ms) / authd << " ms\n";
        std::cout << "  - DB update                : " << double(total_dbupdate_ms) / authd << " ms\n";

        std::cout << "\n--- Bandwidth ---\n";
        std::cout << "Eval bandwidth           : " << eval_bytes << " bytes\n";
        std::cout << "Audit bandwidth          : " << audit_bytes << " bytes\n";
        std::cout << "Write bandwidth          : " << write_bytes << " bytes\n";
        std::cout << "Total bandwidth          : " << total_bytes << " bytes\n";

        std::cout << "\n--- Throughput ---\n";
        std::cout << "Throughput               : " << throughput << " req/sec\n";
        std::cout << "Goodput                  : " << goodput << " authorized req/sec\n";
        std::cout << "Online throughput        : " << online_throughput
                  << " req/sec   (online phase only)\n";
        std::cout << "Online goodput           : " << online_goodput
                  << " authorized req/sec   (online phase only)\n";
        std::cout << "========================================\n";

        std::ofstream ofs("results/online_compute.csv", std::ios::app);
        ofs << log_nitems << "," << authorized_fraction << "," << num_requests << ","
            << double(total_online_ms) / authd << "\n";

        // [FIG3B] one row per trial: variant,latency_ms,log_nitems,trial,audit_ms
        //   audit = encrypt2_p0p1 (LowMC MPC) wall-clock; no preproc for Sabre.
        //   For Fig 3b, num_requests = 1, so this is the single request's audit.
        {
            const double audit_ms = double(total_audit_us) / 1000.0 / double(num_requests);
            std::ofstream f3b("results/fig3b_remisebb_sabre.csv", std::ios::app);
            f3b << "RemiseBB-Sabre" << ","
                << fig3b_latency_ms << ","
                << log_nitems << ","
                << fig3b_trial << ","
                << audit_ms << "\n";
        }

        // [FIG4B] one row per trial: all four curves from a single run.
        //   throughput/goodput        = on-the-fly DPFs (total_runtime denom)
        //   online_throughput/goodput = with preproc DPFs (online_runtime denom)
        //   unauth_frac = 1 - authorized_fraction
        {
            const double unauth_frac = 1.0 - authorized_fraction;
            std::ofstream f4("results/fig4b_remisebb_sabre.csv", std::ios::app);
            f4 << "RemiseBB-Sabre" << ","
               << unauth_frac << ","
               << log_nitems << ","
               << fig3b_trial << ","
               << throughput << ","
               << goodput << ","
               << online_throughput << ","
               << online_goodput << "\n";
        }

        // [MSGSWEEP] message-size experiment: preprocess=eval, audit=audit,
        //   access=finalize+FCW+DB-update. One row per trial.
        //   schema: variant,latency_ms,leafsize,log_nitems,trial,preprocess_ms,audit_ms,access_ms
        {
            const double denom = double(num_requests);
            const double preprocess_ms = double(total_eval_us) / 1000.0 / denom;
            const double audit_ms      = double(total_audit_us) / 1000.0 / denom;
            const double access_ms     = double(total_finalize_ms + total_comm_ms + total_dbupdate_ms) / authd;
            std::ofstream fm("results/msgsweep_remisebb_sabre.csv", std::ios::app);
            fm << "RemiseBB-Sabre" << ","
               << fig3b_latency_ms << ","
               << LEAF_SIZE << ","
               << log_nitems << ","
               << fig3b_trial << ","
               << preprocess_ms << ","
               << audit_ms << ","
               << access_ms << "\n";
        }

        delete[] output;
        delete[] t;
        co_return;
    };

    if (role == Role::P0) {
        auto s_peer = co_await make_server(io, 9200);
        ctx.add_peer(Role::P0, std::move(s_peer));
        NetPeer &peer = ctx.peer(Role::P0);
        MPCContext mpc_ctx(Role::P1, peer, &peer);
        co_await peer.send(uint8_t{1});
        co_await run_online(peer, mpc_ctx, k0, a);
        co_return;
    }

    if (role == Role::P1) {
        const char *host = std::getenv("P0_HOST");
        if (!host) host = "127.0.0.1";
        auto s_peer = co_await connect_with_retry(io, host, 9200);
        ctx.add_peer(Role::P0, std::move(s_peer));
        NetPeer &peer = ctx.peer(Role::P0);
        MPCContext mpc_ctx(Role::P1, peer, &peer);
        auto dummy = co_await peer.recv<uint8_t>();
        std::cout << "[P1] dummy = " << int(dummy) << std::endl;
        co_await run_online(peer, mpc_ctx, k1, b);
        co_return;
    }
}

awaitable<void> run_party(boost::asio::io_context &io, NetContext &ctx, Role role,
                          size_t log_nitems, size_t leaf_size, double authorized_fraction,
                          size_t num_requests) {
    switch (leaf_size) {
    case 2:    co_return co_await run_party_impl<2>(io, ctx, role, log_nitems, authorized_fraction, num_requests);
    case 4:    co_return co_await run_party_impl<4>(io, ctx, role, log_nitems, authorized_fraction, num_requests);
    case 8:    co_return co_await run_party_impl<8>(io, ctx, role, log_nitems, authorized_fraction, num_requests);
    case 10:   co_return co_await run_party_impl<10>(io, ctx, role, log_nitems, authorized_fraction, num_requests);
    case 16:   co_return co_await run_party_impl<16>(io, ctx, role, log_nitems, authorized_fraction, num_requests);
    case 64:   co_return co_await run_party_impl<64>(io, ctx, role, log_nitems, authorized_fraction, num_requests);
    case 256:  co_return co_await run_party_impl<256>(io, ctx, role, log_nitems, authorized_fraction, num_requests);
    case 1024: co_return co_await run_party_impl<1024>(io, ctx, role, log_nitems, authorized_fraction, num_requests);
    case 640:  co_return co_await run_party_impl<640>(io, ctx, role, log_nitems, authorized_fraction, num_requests);
    case 2048: co_return co_await run_party_impl<2048>(io, ctx, role, log_nitems, authorized_fraction, num_requests);
    default:   throw std::invalid_argument("Unsupported leaf_size");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        std::cerr << "usage: ./remise_sabre [p0|p1] <log_num_items> <leafsize> "
                  << "<authorized_fraction> <num_requests>\n";
        return 1;
    }

    Role role;
    std::string r = argv[1];
    size_t log_nitems = 0, leafsize = 0, num_requests = 0;
    double authorized_fraction = 1.0;

    try {
        log_nitems = std::stoull(argv[2]);
        leafsize = std::stoull(argv[3]);
        authorized_fraction = std::stod(argv[4]);
        num_requests = std::stoull(argv[5]);
    } catch (const std::exception &e) {
        std::cerr << "Invalid numeric argument\n";
        return 1;
    }

    if (authorized_fraction < 0.0 || authorized_fraction > 1.0) {
        std::cerr << "authorized_fraction must be in [0,1]\n"; return 1;
    }
    if (num_requests == 0) { std::cerr << "num_requests must be > 0\n"; return 1; }

    if (r == "p0") role = Role::P0;
    else if (r == "p1") role = Role::P1;
    else { std::cerr << "invalid role (use p0 or p1)\n"; return 1; }

    boost::asio::io_context io;
    NetContext ctx(role);
    boost::asio::co_spawn(
        io, run_party(io, ctx, role, log_nitems, leafsize, authorized_fraction, num_requests),
        boost::asio::detached);
    io.run();
    ctx.print_stats();
    return 0;
}