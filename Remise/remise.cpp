#include <assert.h>
#include <bsd/stdlib.h>
#include <iostream>

#include <emmintrin.h> // SSE2
#include <tmmintrin.h> // SSSE3

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <future>
#include <iomanip>
#include <memory>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <cstdlib>   // [FIG3A] std::getenv / std::strtol

// ---- Boost.Asio parallelism primitives for running lanes concurrently ----
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include "dpf.hpp"
#include "types.hpp"


#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <emmintrin.h>
#include <type_traits>

using namespace dpf;

static inline __m128i build_m128i_single_lane(int lane_bytes, int k, uint64_t value) {
    uint8_t tmp[16];
    std::memset(tmp, 0, sizeof tmp);

    if (lane_bytes == 1) {
        assert(k < 16);
        tmp[k] = static_cast<uint8_t>(value);
    } else if (lane_bytes == 4) {
        assert(k < 4);
        uint32_t v32 = static_cast<uint32_t>(value);
        std::memcpy(tmp + (k * 4), &v32, sizeof(v32));
    } else if (lane_bytes == 8) {
        assert(k < 2);
        uint64_t v64 = value;
        std::memcpy(tmp + (k * 8), &v64, sizeof(v64));
    } else {
        assert(!"lane_bytes must be 1, 4, or 8");
    }

    return _mm_loadu_si128(reinterpret_cast<const __m128i *>(tmp));
}

void print_m128i_all(__m128i v, const char *label) {
    alignas(16) uint8_t bytes[16];
    _mm_storeu_si128((__m128i *)bytes, v);

    const uint64_t *u64 = (const uint64_t *)bytes;
    const uint32_t *u32 = (const uint32_t *)bytes;

    printf("=== %s ===\n", label);
    printf("u64: [%016" PRIx64 ", %016" PRIx64 "]\n", u64[0], u64[1]);
    printf("u32: [%08" PRIx32 ", %08" PRIx32 ", %08" PRIx32 ", %08" PRIx32 "]\n", u32[0], u32[1],
           u32[2], u32[3]);

    printf("u8 :");
    for (int i = 0; i < 16; i++)
        printf(" %02" PRIx8, bytes[i]);
    printf("\n");
}


template <typename Peer, typename node_t>
awaitable<void> print_bulletin_board2(Role role, Peer &peer, node_t *output, size_t nitems) {
    std::cout << "\n===== Bulletin Board ===== " << nitems << "\n";

    for (size_t jj = 0; jj < nitems; ++jj) {
        node_t other_share{};

        if (role == Role::P0) {
            co_await (peer << output[jj]);
            co_await (peer >> other_share);
        } else {
            co_await (peer >> other_share);
            co_await (peer << output[jj]);
        }

        node_t value = other_share ^ output[jj];

        if (value[0] != 0 || value[1] != 0) {
            std::cout << "  • BB[" << std::setw(6) << jj << "]"
                      << " = (" << std::hex << (value[0]) << ", " << (value[1]) << ")\n";
        }
    }

    std::cout << "==========================\n";

    co_return;
}

template <typename Peer, typename leaf_t>
awaitable<void> print_bulletin_board(Role role, Peer &peer, leaf_t *output, size_t nitems) {
    std::cout << "\n===== Bulletin Board ===== " << nitems << "\n";

    for (size_t jj = 0; jj < nitems; ++jj) {
        leaf_t other_share{};

        if (role == Role::P0) {
            co_await (peer << output[jj]);
            co_await (peer >> other_share);
        } else {
            co_await (peer >> other_share);
            co_await (peer << output[jj]);
        }

        leaf_t value = other_share ^ output[jj];

        if (value[0][0] != 0 || value[1][0] != 0) {
            std::cout << "  • BB[" << std::setw(6) << jj << "]"
                      << " = (" << (value[0][0]) << ", " << (value[0][1]) << ")\n";
        }
    }

    std::cout << "==========================\n";

    co_return;
}

// -----------------------------------------------------------------------------
// Accept / connect N sockets on a single port so each parallel lane gets its
// own TCP connection. A shared NetPeer cannot carry concurrent lanes because
// overlapping async_write / async_read on one socket is undefined in Asio.
// -----------------------------------------------------------------------------
inline awaitable<std::vector<tcp::socket>>
accept_n(boost::asio::io_context &io, uint16_t port, size_t n) {
    tcp::acceptor acc(io, tcp::endpoint(tcp::v4(), port));

    std::vector<tcp::socket> socks;
    socks.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        tcp::socket s(io);
        co_await acc.async_accept(s, use_awaitable);
        socks.push_back(std::move(s));
    }

    co_return socks;
}

inline awaitable<std::vector<tcp::socket>>
connect_n(boost::asio::io_context &io, const std::string &host, uint16_t port, size_t n) {
    std::vector<tcp::socket> socks;
    socks.reserve(n);

    // Sequential connects pair 1:1 with the server's sequential accepts.
    for (size_t i = 0; i < n; ++i) {
        auto s = co_await connect_with_retry(io, host, port);
        socks.push_back(std::move(s));
    }

    co_return socks;
}

template <size_t LEAF_SIZE>
awaitable<void> run_party_impl(boost::asio::io_context &io, NetContext &ctx, Role role,
                               const size_t log_nitems, double authorized_fraction,
                               size_t num_requests, size_t batch_size) {
    using leaf_t = std::array<__m128i, LEAF_SIZE>;
    using dpf_t = dpf_key<leaf_t, __m128i, AES_KEY>;
    using node_t = __m128i;

    if (batch_size == 0)
        batch_size = 1;

    // =====================================================
    // Common setup
    // =====================================================

    const size_t nitems = 1ULL << log_nitems;

    leaf_t *DB = (leaf_t *)aligned_alloc(16, nitems * sizeof(leaf_t));

    __m128i *ProofDB = (__m128i *)aligned_alloc(16, nitems * sizeof(__m128i));

    arc4random_buf(ProofDB, nitems * sizeof(__m128i));

    // =====================================================
    // Per-lane OFFLINE material: one DISTINCT key per lane.
    //
    // Every per-lane input is derived from fixed seeds so P0 and P1 generate
    // identical (k0_i, k1_i), identical mask-bit pairs (a_i, b_i) and identical
    // target indices; each party then keeps only its own share. This mirrors
    // the original single-key dealer-faking pattern, generalised to `batch_size`
    // independent keys.
    // =====================================================

    std::vector<dpf_t> lane_keys;                            // role's key share, per lane
    std::vector<std::vector<uint8_t>> lane_mask(batch_size); // role's path-bit share, per lane
    std::vector<leaf_t> lane_target_val(batch_size);         // write payload share, per lane

    lane_keys.reserve(batch_size);

    AES_KEY prgkey; // single instance, reused exactly as in the original

    std::mt19937_64 idx_gen(0xC0FFEEULL);
    std::uniform_int_distribution<size_t> idx_dist(0, nitems - 1);

    for (size_t i = 0; i < batch_size; ++i) {

        const size_t ti = idx_dist(idx_gen); // distinct target index for this lane

        leaf_t tval;
        set_target_values(tval, 100, 300);

        auto [kk0, kk1] = dpf_t::gen(prgkey, nitems, ti, tval);

        // additive shares of the path bits of ti
        std::vector<uint8_t> ai(log_nitems), bi(log_nitems);

        std::mt19937_64 g(999ULL + i);
        std::uniform_int_distribution<int> bd(0, 1);

        for (size_t k = 0; k < log_nitems; ++k) {
            const size_t bitpos = log_nitems - 1 - k;
            const uint8_t bit = (ti >> bitpos) & 1;
            ai[k] = static_cast<uint8_t>(bd(g));
            bi[k] = ai[k] ^ bit;
        }

        // sanity: shares reconstruct the chosen index
        {
            size_t recon = 0;
            for (size_t k = 0; k < log_nitems; ++k)
                recon |= (size_t(ai[k] ^ bi[k]) << (log_nitems - 1 - k));
            assert(recon == ti);
            (void)recon;
        }

        // write payload: P0 randomises its share, P1 holds the zero share
        leaf_t tv{};
        if (role == Role::P0)
            arc4random_buf(&tv, sizeof(leaf_t));
        lane_target_val[i] = tv;

        if (role == Role::P0) {
            lane_keys.push_back(std::move(kk0));
            lane_mask[i] = std::move(ai);
        } else {
            lane_keys.push_back(std::move(kk1));
            lane_mask[i] = std::move(bi);
        }

        std::cout << "ti = " << ti << std::endl; 
        std::cout << "tv = " << tv[0][0] << " , " << tv[0][1] << std::endl;
    }

    auto audit_tags = [](const uint8_t *t, const __m128i *ProofDB, size_t nitems) -> __m128i {
        __m128i acc0 = _mm_setzero_si128();
        __m128i acc1 = _mm_setzero_si128();
        __m128i acc2 = _mm_setzero_si128();
        __m128i acc3 = _mm_setzero_si128();

        size_t j = 0;

        for (; j + 3 < nitems; j += 4) {
            if (t[j + 0])
                acc0 = _mm_xor_si128(acc0, ProofDB[j + 0]);
            if (t[j + 1])
                acc1 = _mm_xor_si128(acc1, ProofDB[j + 1]);
            if (t[j + 2])
                acc2 = _mm_xor_si128(acc2, ProofDB[j + 2]);
            if (t[j + 3])
                acc3 = _mm_xor_si128(acc3, ProofDB[j + 3]);
        }

        for (; j < nitems; ++j) {
            if (t[j])
                acc0 = _mm_xor_si128(acc0, ProofDB[j]);
        }

        return _mm_xor_si128(_mm_xor_si128(acc0, acc1), _mm_xor_si128(acc2, acc3));
    };

    uint64_t total_eval_ms = 0;
    uint64_t total_audit_ms = 0;
    uint64_t total_write_ms = 0;
    uint64_t total_online_ms = 0;
    uint64_t eval_bytes = 0;
    uint64_t audit_bytes = 0;
    uint64_t write_bytes = 0;
    uint64_t total_finalize_ms = 0;
    uint64_t total_dbupdate_ms = 0;
    uint64_t total_comm_ms = 0;
    size_t authorized_count = 0;
    size_t cover_count = 0;

    // [FIG3A] microsecond-resolution eval/audit accumulators (ms truncates to 0
    // at small DB sizes); latency + trial labels come from run.sh via env vars.
    uint64_t total_eval_us = 0;
    uint64_t total_audit_us = 0;
    const char *rtt_env = std::getenv("REMISE_RTT_MS");
    const long fig3a_latency_ms = rtt_env ? std::strtol(rtt_env, nullptr, 10) : 0;
    const char *trial_env = std::getenv("REMISE_TRIAL");
    const long fig3a_trial = trial_env ? std::strtol(trial_env, nullptr, 10) : 0;

    // -----------------------------------------------------------------
    // Online phase. `lane_peers[i]` / `lane_ctx[i]` are lane i's private
    // connection + MPC context. Within each batch the eval, audit and FCW
    // exchanges all run concurrently across lanes; only the shared-DB XOR
    // update is sequential.
    // -----------------------------------------------------------------
    auto run_online = [&](std::vector<NetPeer *> &lane_peers,
                          std::vector<std::unique_ptr<MPCContext>> &lane_ctx) -> awaitable<void> {

        std::mt19937 rng(12345);
        std::bernoulli_distribution auth_dist(authorized_fraction);

        // Per-lane scratch, reused across batches.
        std::vector<leaf_t *> output(batch_size);
        std::vector<uint8_t *> t(batch_size);
        std::vector<node_t *> final_nodes(batch_size, nullptr);
        std::vector<uint8_t *> final_flags(batch_size, nullptr);
        std::vector<size_t> nodes_in_interval(batch_size, 0);
        std::vector<char> authorized(batch_size, 0);
        std::vector<leaf_t> target_val_recv(batch_size); // reconstructed FCW^payload, per lane

        for (size_t i = 0; i < batch_size; ++i) {
            output[i] = new leaf_t[nitems];
            t[i] = new uint8_t[nitems];
        }

        auto ex = co_await boost::asio::this_coro::executor;

        // -------------------------------------------------------------
        // Lane bodies. Each touches only lane i's buffers and socket.
        // -------------------------------------------------------------

        auto eval_lane = [&](size_t i) -> awaitable<void> {
            co_await __evalinterval_mpc(*lane_ctx[i], *lane_peers[i], lane_keys[i], 0, nitems - 1,
                                        output[i], t[i], lane_mask[i].data(), final_nodes[i],
                                        final_flags[i], nodes_in_interval[i], 8);
            co_return;
        };

        auto audit_lane = [&](size_t i) -> awaitable<void> {
            NetPeer &peer = *lane_peers[i];

            __m128i result = audit_tags(t[i], ProofDB, nitems);
            asm volatile("" ::"x"(result));

            __m128i recv;
            co_await ((peer << result) && (peer >> recv));
            result ^= recv;
            asm volatile("" ::"x"(result));

            co_return;
        };

        // finalize + FCW (local) then exchange FCW^payload (the overlapping comm).
        // Reconstructed value is stashed in target_val_recv[i] for the DB update.
        auto write_comm_lane = [&](size_t i, size_t global_iter) -> awaitable<void> {
            NetPeer &peer = *lane_peers[i];
            
            #ifdef VERBOSE
            std::cout << "iter = " << global_iter << std::endl;
            #endif

            auto f1 = std::chrono::high_resolution_clock::now();

            finalize(lane_keys[i].prgkey, lane_keys[i].finalizer, output[i], final_nodes[i],
                     nodes_in_interval[i], final_flags[i]);

            leaf_t fcw = output[i][0];
            for (size_t j = 1; j < nodes_in_interval[i]; ++j)
                fcw ^= output[i][j];

            auto f2 = std::chrono::high_resolution_clock::now();
            total_finalize_ms +=
                std::chrono::duration_cast<std::chrono::milliseconds>(f2 - f1).count();

            leaf_t recv{};
            co_await ((peer << (fcw ^ lane_target_val[i])) && (peer >> recv));
            recv ^= (fcw ^ lane_target_val[i]);

            target_val_recv[i] = recv;
            co_return;
        };

        std::vector<size_t> auth_idx; // authorized lanes in the current batch
        auth_idx.reserve(batch_size);

        auto experiment_start = std::chrono::high_resolution_clock::now();

        for (size_t base = 0; base < num_requests; base += batch_size) {

            const size_t lanes = std::min(batch_size, num_requests - base);

            auth_idx.clear();
            for (size_t i = 0; i < lanes; ++i) {
                authorized[i] = auth_dist(rng) ? 1 : 0;
                if (authorized[i]) {
                    ++authorized_count;
                    auth_idx.push_back(i);
                } else {
                    ++cover_count;
                }
            }

            // ============================================
            // (1) Eval  ->  `lanes` evalintervals in PARALLEL
            // ============================================

            uint64_t eval_sent_before = ctx.bytes_sent();
            uint64_t eval_recv_before = ctx.bytes_received();

            auto start_eval = std::chrono::high_resolution_clock::now();

            if (lanes == 1) {
                co_await eval_lane(0);
            } else {
                using op_t =
                    decltype(boost::asio::co_spawn(ex, eval_lane(0), boost::asio::deferred));
                std::vector<op_t> ops;
                ops.reserve(lanes);
                for (size_t i = 0; i < lanes; ++i)
                    ops.push_back(boost::asio::co_spawn(ex, eval_lane(i), boost::asio::deferred));

                auto [order, exc] =
                    co_await boost::asio::experimental::make_parallel_group(std::move(ops))
                        .async_wait(boost::asio::experimental::wait_for_all(),
                                    boost::asio::use_awaitable);
                for (auto &e : exc)
                    if (e)
                        std::rethrow_exception(e);
            }

            auto end_eval = std::chrono::high_resolution_clock::now();
            total_eval_ms +=
                std::chrono::duration_cast<std::chrono::milliseconds>(end_eval - start_eval).count();
            total_eval_us +=
                std::chrono::duration_cast<std::chrono::microseconds>(end_eval - start_eval).count(); // [FIG3A]
            eval_bytes += (ctx.bytes_sent() - eval_sent_before) +
                          (ctx.bytes_received() - eval_recv_before);

            auto online_start = std::chrono::high_resolution_clock::now();

            // ============================================
            // (2) Audit  ->  exchanges in PARALLEL across lanes
            // ============================================

            uint64_t audit_sent_before = ctx.bytes_sent();
            uint64_t audit_recv_before = ctx.bytes_received();

            auto start_audit = std::chrono::high_resolution_clock::now();

            if (lanes == 1) {
                co_await audit_lane(0);
            } else {
                using op_t =
                    decltype(boost::asio::co_spawn(ex, audit_lane(0), boost::asio::deferred));
                std::vector<op_t> ops;
                ops.reserve(lanes);
                for (size_t i = 0; i < lanes; ++i)
                    ops.push_back(boost::asio::co_spawn(ex, audit_lane(i), boost::asio::deferred));

                auto [order, exc] =
                    co_await boost::asio::experimental::make_parallel_group(std::move(ops))
                        .async_wait(boost::asio::experimental::wait_for_all(),
                                    boost::asio::use_awaitable);
                for (auto &e : exc)
                    if (e)
                        std::rethrow_exception(e);
            }

            auto end_audit = std::chrono::high_resolution_clock::now();
            total_audit_ms +=
                std::chrono::duration_cast<std::chrono::milliseconds>(end_audit - start_audit)
                    .count();
            total_audit_us +=
                std::chrono::duration_cast<std::chrono::microseconds>(end_audit - start_audit)
                    .count(); // [FIG3A]
            audit_bytes += (ctx.bytes_sent() - audit_sent_before) +
                           (ctx.bytes_received() - audit_recv_before);

            // ============================================
            // (3) Write
            //     (3a) FCW exchanges in PARALLEL (authorized lanes)
            //     (3b) shared-DB XOR update, SEQUENTIAL
            // ============================================

            uint64_t write_sent_before = ctx.bytes_sent();
            uint64_t write_recv_before = ctx.bytes_received();

            auto start_write = std::chrono::high_resolution_clock::now();

            // (3a) parallel comm
            auto comm_start = std::chrono::high_resolution_clock::now();

            if (auth_idx.size() == 1) {
                co_await write_comm_lane(auth_idx[0], base + auth_idx[0]);
            } else if (auth_idx.size() > 1) {
                using op_t = decltype(boost::asio::co_spawn(
                    ex, write_comm_lane(size_t{0}, size_t{0}), boost::asio::deferred));
                std::vector<op_t> ops;
                ops.reserve(auth_idx.size());
                for (size_t i : auth_idx)
                    ops.push_back(boost::asio::co_spawn(ex, write_comm_lane(i, base + i),
                                                        boost::asio::deferred));

                auto [order, exc] =
                    co_await boost::asio::experimental::make_parallel_group(std::move(ops))
                        .async_wait(boost::asio::experimental::wait_for_all(),
                                    boost::asio::use_awaitable);
                for (auto &e : exc)
                    if (e)
                        std::rethrow_exception(e);
            }

            auto comm_end = std::chrono::high_resolution_clock::now();
            total_comm_ms +=
                std::chrono::duration_cast<std::chrono::milliseconds>(comm_end - comm_start).count();

            // (3b) sequential DB update (shared DB; XOR is order-independent)
            for (size_t i : auth_idx) {
                auto d1 = std::chrono::high_resolution_clock::now();
                for (size_t j = 0; j < nitems; ++j) {
                    DB[j] ^= output[i][j];
                    if (t[i][j])
                        DB[j] ^= target_val_recv[i];
                }
                auto d2 = std::chrono::high_resolution_clock::now();
                total_dbupdate_ms +=
                    std::chrono::duration_cast<std::chrono::milliseconds>(d2 - d1).count();
            }

            auto end_write = std::chrono::high_resolution_clock::now();
            total_write_ms +=
                std::chrono::duration_cast<std::chrono::milliseconds>(end_write - start_write)
                    .count();
            write_bytes += (ctx.bytes_sent() - write_sent_before) +
                           (ctx.bytes_received() - write_recv_before);

            auto online_end = std::chrono::high_resolution_clock::now();
            total_online_ms +=
                std::chrono::duration_cast<std::chrono::milliseconds>(online_end - online_start)
                    .count();

            // Debug reconstruction once per batch (was once per request before).
            //co_await print_bulletin_board(role, *lane_peers[0], DB, nitems);
        }

        auto experiment_end = std::chrono::high_resolution_clock::now();

        double total_runtime_sec =
            std::chrono::duration_cast<std::chrono::milliseconds>(experiment_end - experiment_start)
                .count() /
            1000.0;

        double throughput = double(num_requests) / total_runtime_sec;
        double goodput = double(authorized_count) / total_runtime_sec;

        double online_runtime_sec = double(total_online_ms) / 1000.0;
        double online_throughput = double(num_requests) / online_runtime_sec;
        double online_goodput = double(authorized_count) / online_runtime_sec;

        uint64_t total_bytes = eval_bytes + audit_bytes + write_bytes;

        std::cout << "\n========== Experiment Summary ==========\n";

        std::cout << "Batch size               : " << batch_size << std::endl;
        std::cout << "Total requests           : " << num_requests << std::endl;
        std::cout << "Authorized requests      : " << authorized_count << std::endl;
        std::cout << "Unauthorized requests           : " << cover_count << std::endl;
        std::cout << "Authorized fraction      : "
                  << double(authorized_count) / double(num_requests) << std::endl;

        std::cout << "\n--- Latency (amortized per request) ---\n";

        std::cout << "Average eval latency     : " << double(total_eval_ms) / double(num_requests)
                  << " ms\n";
        std::cout << "Average audit latency    : " << double(total_audit_ms) / double(num_requests)
                  << " ms\n";
        std::cout << "Average write latency    : "
                  << double(total_write_ms) / double(authorized_count) << " ms\n";
        std::cout << "Average write latency (for PACL)    : "
                  << double((total_finalize_ms + total_dbupdate_ms)) / double(authorized_count)
                  << " ms\n";
        std::cout << "Average FCW exchange latency : "
                  << double(total_comm_ms) / double(authorized_count) << " ms\n";

        std::cout << "\n--- Bandwidth ---\n";

        std::cout << "Eval bandwidth           : " << eval_bytes << " bytes\n";
        std::cout << "Audit bandwidth          : " << audit_bytes << " bytes\n";
        std::cout << "Write bandwidth          : " << write_bytes << " bytes\n";
        std::cout << "Total bandwidth          : " << total_bytes << " bytes\n";

        std::cout << "\n--- Throughput ---\n";

        std::cout << "Throughput               : " << throughput << " req/sec\n";
        std::cout << "Goodput                  : " << goodput << " authorized req/sec\n";

        std::cout << "\n--- Online Throughput ---\n";

        std::cout << "Online throughput        : " << online_throughput << " req/sec\n";
        std::cout << "Online goodput           : " << online_goodput << " authorized req/sec\n";
        std::cout << "========================================\n";

        std::ofstream ofs("online_compute.csv", std::ios::app);
        // NOTE: trailing batch_size column appended to the original schema.
        ofs << log_nitems << "," << authorized_fraction << "," << num_requests << ","
            << double((total_finalize_ms + total_dbupdate_ms)) / double(authorized_count) << ","
            << batch_size << "\n";

        // [FIG3A] one row per trial: variant,latency_ms,log_nitems,trial,online_ms,total_ms
        //   online = audit only;  total = eval + audit  (per-request average).
        //   For Fig 3a, num_requests = 1, so this is the single request's timing.
        {
            const double denom = double(num_requests);
            const double online_ms = double(total_audit_us) / 1000.0 / denom;
            const double total_ms  = double(total_eval_us + total_audit_us) / 1000.0 / denom;
            std::ofstream f3a("results/fig3a_remisebb.csv", std::ios::app);
            f3a << "RemiseBB" << ","
                << fig3a_latency_ms << ","
                << log_nitems << ","
                << fig3a_trial << ","
                << online_ms << ","
                << total_ms << "\n";
        }

        // [FIG4A] one row per trial: all four curves from a single run.
        //   throughput/goodput          = on-the-fly DPFs (total_runtime denom)
        //   online_throughput/goodput   = with preproc DPFs (online_runtime denom)
        //   unauth_frac = 1 - authorized_fraction (the figure's x-grouping)
        {
            const double unauth_frac = 1.0 - authorized_fraction;
            std::ofstream f4("results/fig4a_remisebb.csv", std::ios::app);
            f4 << "RemiseBB" << ","
               << unauth_frac << ","
               << log_nitems << ","
               << fig3a_trial << ","
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
            const double adenom = (authorized_count > 0) ? double(authorized_count) : 1.0;
            const double access_ms = double(total_finalize_ms + total_comm_ms + total_dbupdate_ms) / adenom;
            std::ofstream fm("results/msgsweep_remisebb.csv", std::ios::app);
            fm << "RemiseBB" << ","
               << fig3a_latency_ms << ","
               << LEAF_SIZE << ","
               << log_nitems << ","
               << fig3a_trial << ","
               << preprocess_ms << ","
               << audit_ms << ","
               << access_ms << "\n";
        }

        for (size_t i = 0; i < batch_size; ++i) {
            delete[] output[i];
            delete[] t[i];
        }

        co_return;
    };

    // =====================================================
    // P0  (server): accept `batch_size` connections
    // =====================================================

    if (role == Role::P0) {

        auto socks = co_await accept_n(io, 9200, batch_size);

        for (size_t i = 0; i < batch_size; ++i)
            ctx.add_peer(Role::P0, std::move(socks[i]));

        std::vector<NetPeer *> lane_peers;
        std::vector<std::unique_ptr<MPCContext>> lane_ctx;
        lane_peers.reserve(batch_size);
        lane_ctx.reserve(batch_size);

        for (size_t i = 0; i < batch_size; ++i) {
            NetPeer *p = ctx.peers[i].get();
            lane_peers.push_back(p);
            lane_ctx.push_back(std::make_unique<MPCContext>(Role::P1, *p, p));
        }

        // dummy sync barrier on lane 0
        co_await lane_peers[0]->send(uint8_t{1});

        co_await run_online(lane_peers, lane_ctx);

        co_return;
    }

    // =====================================================
    // P1  (client): open `batch_size` connections
    // =====================================================

    if (role == Role::P1) {

        const char *host = std::getenv("P0_HOST");
        if (!host)
            host = "p0";

        auto socks = co_await connect_n(io, host, 9200, batch_size);

        for (size_t i = 0; i < batch_size; ++i)
            ctx.add_peer(Role::P0, std::move(socks[i]));

        std::vector<NetPeer *> lane_peers;
        std::vector<std::unique_ptr<MPCContext>> lane_ctx;
        lane_peers.reserve(batch_size);
        lane_ctx.reserve(batch_size);

        for (size_t i = 0; i < batch_size; ++i) {
            NetPeer *p = ctx.peers[i].get();
            lane_peers.push_back(p);
            lane_ctx.push_back(std::make_unique<MPCContext>(Role::P1, *p, p));
        }

        // dummy sync barrier on lane 0
        auto dummy = co_await lane_peers[0]->recv<uint8_t>();
        std::cout << "[P1] dummy = " << int(dummy) << std::endl;

        co_await run_online(lane_peers, lane_ctx);

        co_return;
    }
}

awaitable<void> run_party(boost::asio::io_context &io, NetContext &ctx, Role role,
                          size_t log_nitems, size_t leaf_size, double authorized_fraction,
                          size_t num_requests, size_t batch_size) {
    switch (leaf_size) {

    case 2:
        co_return co_await run_party_impl<2>(io, ctx, role, log_nitems, authorized_fraction,
                                             num_requests, batch_size);
    case 4:
        co_return co_await run_party_impl<4>(io, ctx, role, log_nitems, authorized_fraction,
                                             num_requests, batch_size);
    case 8:
        co_return co_await run_party_impl<8>(io, ctx, role, log_nitems, authorized_fraction,
                                             num_requests, batch_size);
    case 10:
        co_return co_await run_party_impl<10>(io, ctx, role, log_nitems, authorized_fraction,
                                              num_requests, batch_size);
    case 16:
        co_return co_await run_party_impl<16>(io, ctx, role, log_nitems, authorized_fraction,
                                              num_requests, batch_size);
    case 64:
        co_return co_await run_party_impl<64>(io, ctx, role, log_nitems, authorized_fraction,
                                              num_requests, batch_size);
    case 256:
        co_return co_await run_party_impl<256>(io, ctx, role, log_nitems, authorized_fraction,
                                               num_requests, batch_size);
    case 1024:
        co_return co_await run_party_impl<1024>(io, ctx, role, log_nitems, authorized_fraction,
                                                num_requests, batch_size);
    case 640:
        co_return co_await run_party_impl<640>(io, ctx, role, log_nitems, authorized_fraction,
                                               num_requests, batch_size);
    case 2048:
        co_return co_await run_party_impl<2048>(io, ctx, role, log_nitems, authorized_fraction,
                                                num_requests, batch_size);
    default:
        throw std::invalid_argument("Unsupported leaf_size");
    }
}

// ============================================================================
// Low-level printer for __m128i
// ============================================================================
inline void print_m128i(const __m128i &v) {
    alignas(16) uint8_t bytes[16];
    _mm_storeu_si128((__m128i *)bytes, v);

    printf("__m128i: [");
    for (int i = 0; i < 16; i++) {
        printf("%02x", bytes[i]);
        if (i < 15)
            printf(" ");
    }
    printf("]");
}

// ============================================================================
// Generic print for scalar integral types
// ============================================================================
template <typename T> inline void print_scalar(const T &x) {
    if constexpr (std::is_same_v<T, uint8_t>) {
        printf("%u", (unsigned)x);
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        printf("%u", x);
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        printf("%llu", (unsigned long long)x);
    } else {
        static_assert(sizeof(T) == 0, "Unsupported scalar type in print_scalar");
    }
}

// ============================================================================
// Master leaf printer: works for all required types
// ============================================================================
template <typename T> void print_leaf(const T &x) {
    if constexpr (std::is_same_v<T, __m128i>) {
        print_m128i(x);
    } else if constexpr (std::is_integral_v<T>) {
        print_scalar(x);
    } else if constexpr (std::is_same_v<typename T::value_type, __m128i> &&
                         std::is_array_v<T> == false && std::tuple_size<T>::value > 0) {
        printf("[ ");
        for (size_t i = 0; i < x.size(); i++) {
            print_m128i(x[i]);
            if (i + 1 < x.size())
                printf(", ");
        }
        printf(" ]");
    } else if constexpr (std::tuple_size<T>::value > 0 &&
                         std::is_integral_v<typename T::value_type>) {
        printf("[ ");
        for (size_t i = 0; i < x.size(); i++) {
            print_scalar(x[i]);
            if (i + 1 < x.size())
                printf(", ");
        }
        printf(" ]");
    } else {
        static_assert(sizeof(T) == 0, "print_leaf(): unsupported leaf type");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 6 && argc != 7) {

        std::cerr << "usage: ./remise "
                  << "[p0|p1] "
                  << "<log_num_items> "
                  << "<leafsize> "
                  << "<authorized_fraction> "
                  << "<num_requests> "
                  << "[batch_size]\n"
                  << "  batch_size defaults to 64; use 1 to disable batching.\n";

        return 1;
    }

    Role role;

    std::string r = argv[1];

    size_t log_nitems = 0;
    size_t leafsize = 0;
    size_t num_requests = 0;
    size_t batch_size = 64; // default

    double authorized_fraction = 1.0;

    try {

        log_nitems = std::stoull(argv[2]);
        leafsize = std::stoull(argv[3]);
        authorized_fraction = std::stod(argv[4]);
        num_requests = std::stoull(argv[5]);
        if (argc == 7)
            batch_size = std::stoull(argv[6]);

    } catch (const std::exception &e) {

        std::cerr << "Invalid numeric argument\n";
        return 1;
    }

    if (authorized_fraction < 0.0 || authorized_fraction > 1.0) {
        std::cerr << "authorized_fraction must be in [0,1]\n";
        return 1;
    }

    if (num_requests == 0) {
        std::cerr << "num_requests must be > 0\n";
        return 1;
    }

    if (batch_size == 0) {
        std::cerr << "batch_size must be > 0\n";
        return 1;
    }

    if (r == "p0") {
        role = Role::P0;
    } else if (r == "p1") {
        role = Role::P1;
    } else {
        std::cerr << "invalid role (use p0 or p1)\n";
        return 1;
    }

    boost::asio::io_context io;

    NetContext ctx(role);

    boost::asio::co_spawn(io,
                          run_party(io, ctx, role, log_nitems, leafsize, authorized_fraction,
                                    num_requests, batch_size),
                          boost::asio::detached);

    io.run();

    ctx.print_stats();

    return 0;
}