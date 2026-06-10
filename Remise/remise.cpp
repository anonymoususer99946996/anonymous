#include <assert.h>
#include <bsd/stdlib.h>
#include <iostream>

#include <emmintrin.h> // SSE2
#include <tmmintrin.h> // SSSE3

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
                      << " = (" << (value[0]) << ", " << (value[1]) << ")\n";
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
                      << " = (" << (value[0][0]) << ", " << (value[1][0]) << ")\n";
        }
    }

    std::cout << "==========================\n";

    co_return;
}

template <size_t LEAF_SIZE>
awaitable<void> run_party_impl(boost::asio::io_context &io, NetContext &ctx, Role role,
                               const size_t log_nitems, double authorized_fraction,
                               size_t num_requests) {
    using leaf_t = std::array<__m128i, LEAF_SIZE>;

    // =====================================================
    // Shared helpers
    // =====================================================

    // =====================================================
    // Common setup
    // =====================================================

    const size_t nitems = 1ULL << log_nitems;
    const size_t target_ind = 44;

    leaf_t target_val;
    leaf_t target_val_recv;

    if (role == Role::P0) {
        arc4random_buf(&target_val, sizeof(leaf_t));
    }

    std::cout << "target_val = " << target_val[0][0] << std::endl;
    leaf_t *DB = (leaf_t *)aligned_alloc(16, nitems * sizeof(leaf_t));

    __m128i *ProofDB = (__m128i *)aligned_alloc(16, nitems * sizeof(__m128i));

    arc4random_buf(ProofDB, nitems * sizeof(__m128i));

    uint8_t *a = static_cast<uint8_t *>(malloc(log_nitems));

    uint8_t *b = static_cast<uint8_t *>(malloc(log_nitems));

    if (!a || !b) {
        throw std::bad_alloc();
    }

    std::mt19937_64 gen2(999);

    std::uniform_int_distribution<int> bitdist(0, 1);

    for (size_t i = 0; i < log_nitems; ++i) {

        size_t bitpos = log_nitems - 1 - i;

        uint8_t bit = (target_ind >> bitpos) & 1;

        a[i] = bitdist(gen2);
        b[i] = a[i] ^ bit;
    }

    size_t reconstructed = 0;

    for (size_t i = 0; i < log_nitems; ++i) {

        reconstructed |= (size_t(a[i] ^ b[i]) << (log_nitems - 1 - i));
    }

    assert(reconstructed == target_ind);

    AES_KEY prgkey;

    leaf_t target_value;
    leaf_t FCW;

    set_target_values(target_value, 100, 300);

    auto [k0, k1] =
        dpf_key<leaf_t, __m128i, AES_KEY>::gen(prgkey, nitems, target_ind, target_value);

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

    auto run_online = [&](NetPeer &peer, MPCContext &mpc_ctx, auto &key,
                          uint8_t *mask_bits) -> awaitable<void> {


                
        std::mt19937 rng(12345);

        std::bernoulli_distribution auth_dist(authorized_fraction);

        leaf_t *output = new leaf_t[nitems];

        uint8_t *t = new uint8_t[nitems];

        auto experiment_start = std::chrono::high_resolution_clock::now();

        for (size_t iter = 0; iter < num_requests; ++iter) {
            bool authorized = auth_dist(rng);

            if (authorized)
                ++authorized_count;
            else
                ++cover_count;

            // ============================================
            // Eval
            // ============================================

            uint64_t eval_sent_before = ctx.bytes_sent();

            uint64_t eval_recv_before = ctx.bytes_received();

            auto start_eval = std::chrono::high_resolution_clock::now();

            using node_t = __m128i;

            node_t *final_nodes;

            uint8_t *final_flags;

            size_t nodes_in_interval;

            co_await __evalinterval_mpc(mpc_ctx, peer, key, 0, nitems - 1, output, t, mask_bits,
                                        final_nodes, final_flags, nodes_in_interval, 8);


            auto end_eval = std::chrono::high_resolution_clock::now();

            auto eval_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_eval - start_eval)
                    .count();
            uint64_t eval_sent_after = ctx.bytes_sent();

            uint64_t eval_recv_after = ctx.bytes_received();

            eval_bytes +=
                (eval_sent_after - eval_sent_before) + (eval_recv_after - eval_recv_before);

            total_eval_ms += eval_ms;

            auto online_start = std::chrono::high_resolution_clock::now();

            // ============================================
            // Audit
            // ============================================

            uint64_t audit_sent_before = ctx.bytes_sent();

            uint64_t audit_recv_before = ctx.bytes_received();

            auto start_audit = std::chrono::high_resolution_clock::now();

            __m128i result = audit_tags(t, ProofDB, nitems);

            asm volatile("" ::"x"(result));

            auto end_audit = std::chrono::high_resolution_clock::now();

            uint64_t audit_sent_after = ctx.bytes_sent();

            uint64_t audit_recv_after = ctx.bytes_received();

            audit_bytes +=
                (audit_sent_after - audit_sent_before) + (audit_recv_after - audit_recv_before);

            auto audit_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_audit - start_audit)
                    .count();

            total_audit_ms += audit_ms;

            // ============================================
            // Write
            // ============================================

            auto start_write = std::chrono::high_resolution_clock::now();
            

            if (authorized) {
                

                std::cout << "iter = " << iter << std::endl;
                
                auto t1 = std::chrono::high_resolution_clock::now();

                finalize(key.prgkey, key.finalizer, output, final_nodes, nodes_in_interval,
                         final_flags);

                FCW = output[0];

                for (size_t j = 1; j < nodes_in_interval; ++j) {
                    FCW ^= output[j];
                }
                auto t2 = std::chrono::high_resolution_clock::now();

                 total_finalize_ms +=
                        std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
                uint64_t write_sent_before = ctx.bytes_sent();

                uint64_t write_recv_before = ctx.bytes_received();

               auto comm_start = std::chrono::high_resolution_clock::now();

               co_await ((peer << (FCW ^ target_val)) && (peer >> target_val_recv));

               auto comm_end = std::chrono::high_resolution_clock::now();




		auto total_ms =
			std::chrono::duration_cast<std::chrono::milliseconds>(comm_end - comm_start).count();

		std::cerr
			<< "total (Finalize!) : " << total_ms << " ms\n";


              total_comm_ms +=
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                    comm_end - comm_start)
                    .count();

                uint64_t write_sent_after = ctx.bytes_sent();

                uint64_t write_recv_after = ctx.bytes_received();

                write_bytes +=
                    (write_sent_after - write_sent_before) + (write_recv_after - write_recv_before);

                target_val_recv ^= (FCW ^ target_val);

                auto t3 = std::chrono::high_resolution_clock::now();

                for (size_t j = 0; j < nitems; ++j) {
                    DB[j] ^= output[j];

                    if (t[j])
                        DB[j] ^= target_val_recv;
                }

                auto t4 = std::chrono::high_resolution_clock::now();

                total_dbupdate_ms +=
                    std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();             
                
            }

    

            
            auto online_end = std::chrono::high_resolution_clock::now();

            auto online_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    online_end - online_start)
                    .count();

            total_online_ms += online_ms;

            auto end_write = std::chrono::high_resolution_clock::now();
            
            //co_await print_bulletin_board(role, peer, DB, nitems);
            
            auto write_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_write - start_write)
                    .count();

            total_write_ms += write_ms;
        }

        auto experiment_end = std::chrono::high_resolution_clock::now();

        double total_runtime_sec =
            std::chrono::duration_cast<std::chrono::milliseconds>(experiment_end - experiment_start)
                .count() /
            1000.0;

        double throughput = double(num_requests) / total_runtime_sec;

        double goodput = double(authorized_count) / total_runtime_sec;

        double online_runtime_sec =
            double(total_online_ms) / 1000.0;

        double online_throughput =
            double(num_requests) / online_runtime_sec;

        double online_goodput =
            double(authorized_count) / online_runtime_sec;

        uint64_t total_bytes = eval_bytes + audit_bytes + write_bytes;

        std::cout << "\n========== Experiment Summary ==========\n";

        std::cout << "Total requests           : " << num_requests << std::endl;

        std::cout << "Authorized requests      : " << authorized_count << std::endl;

        std::cout << "Cover requests           : " << cover_count << std::endl;

        std::cout << "Authorized fraction      : "
                  << double(authorized_count) / double(num_requests) << std::endl;

        std::cout << "\n--- Latency ---\n";

        std::cout << "Average eval latency     : " << double(total_eval_ms) / double(num_requests)
                  << " ms\n";

        std::cout << "Average audit latency    : " << double(total_audit_ms) / double(num_requests)
                  << " ms\n";

        std::cout << "Average write latency    : " << double(total_write_ms) / double(num_requests)
                  << " ms\n";
        

        
        std::cout << "Average write latency (for PACL)    : " << double((total_finalize_ms  + total_dbupdate_ms)) / double(num_requests)
                  << " ms\n";

        std::cout << "Average FCW exchange latency : "
                << double(total_comm_ms) / num_requests 
                << " ms\n";
        std::cout << "\n--- Bandwidth ---\n";

        std::cout << "Eval bandwidth           : " << eval_bytes << " bytes\n";

        std::cout << "Audit bandwidth          : " << audit_bytes << " bytes\n";

        std::cout << "Write bandwidth          : " << write_bytes << " bytes\n";

        std::cout << "Total bandwidth          : " << total_bytes << " bytes\n";

        std::cout << "\n--- Throughput ---\n";

        std::cout << "Throughput               : " << throughput << " req/sec\n";

        std::cout << "Goodput                  : " << goodput << " authorized req/sec\n";

        std::cout << "\n--- Online Throughput ---\n";

        std::cout << "Online throughput        : "
                  << online_throughput
                  << " req/sec\n";

        std::cout << "Online goodput           : "
                  << online_goodput
                  << " authorized req/sec\n";

        std::cout << "========================================\n";

                    std::ofstream ofs("online_compute.csv", std::ios::app);

            ofs << log_nitems << ","
                << authorized_fraction << ","
                << num_requests << ","
                << double((total_finalize_ms  + total_dbupdate_ms)) / double(num_requests) << "\n";

        delete[] output;
        delete[] t;

        co_return;
    };

    // =====================================================
    // P0
    // =====================================================

    if (role == Role::P0) {

        auto s_peer = co_await make_server(io, 9200);

        ctx.add_peer(Role::P0, std::move(s_peer));

        NetPeer &peer = ctx.peer(Role::P0);

        MPCContext mpc_ctx(Role::P1, peer, &peer);

        // dummy sync
        co_await peer.send(uint8_t{1});

        co_await run_online(peer, mpc_ctx, k0, a);

        co_return;
    }

    // =====================================================
    // P1
    // =====================================================

    if (role == Role::P1) {

        auto s_peer = co_await connect_with_retry(io, "127.0.0.1", 9200);

        ctx.add_peer(Role::P0, std::move(s_peer));

        NetPeer &peer = ctx.peer(Role::P0);

        MPCContext mpc_ctx(Role::P1, peer, &peer);

        // dummy sync
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

    case 2:
        co_return co_await run_party_impl<2>(io, ctx, role, log_nitems, authorized_fraction,
                                             num_requests);

    case 4:
        co_return co_await run_party_impl<4>(io, ctx, role, log_nitems, authorized_fraction,
                                             num_requests);

    case 8:
        co_return co_await run_party_impl<8>(io, ctx, role, log_nitems, authorized_fraction,
                                             num_requests);

    case 16:
        co_return co_await run_party_impl<16>(io, ctx, role, log_nitems, authorized_fraction,
                                              num_requests);

    case 64:
        co_return co_await run_party_impl<64>(io, ctx, role, log_nitems, authorized_fraction,
                                              num_requests);

    case 256:
        co_return co_await run_party_impl<256>(io, ctx, role, log_nitems, authorized_fraction,
                                               num_requests);

    case 1024:
        co_return co_await run_party_impl<1024>(io, ctx, role, log_nitems, authorized_fraction,
                                                num_requests);

    case 640:
        co_return co_await run_party_impl<640>(io, ctx, role, log_nitems, authorized_fraction,
                                               num_requests);

    case 2048:
        co_return co_await run_party_impl<2048>(io, ctx, role, log_nitems, authorized_fraction,
                                                num_requests);

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
    // ------------------------------------------------------------------------
    // Case 1: __m128i
    // ------------------------------------------------------------------------
    if constexpr (std::is_same_v<T, __m128i>) {
        print_m128i(x);
    }

    // ------------------------------------------------------------------------
    // Case 2: scalar integrals uint8_t / uint32_t / uint64_t
    // ------------------------------------------------------------------------
    else if constexpr (std::is_integral_v<T>) {
        print_scalar(x);
    }

    // ------------------------------------------------------------------------
    // Case 3: std::array<__m128i, K>
    // ------------------------------------------------------------------------
    else if constexpr (std::is_same_v<typename T::value_type, __m128i> &&
                       std::is_array_v<T> == false && std::tuple_size<T>::value > 0) {
        printf("[ ");
        for (size_t i = 0; i < x.size(); i++) {
            print_m128i(x[i]);
            if (i + 1 < x.size())
                printf(", ");
        }
        printf(" ]");
    }

    // ------------------------------------------------------------------------
    // Case 4: std::array<U, K> for scalar U
    // ------------------------------------------------------------------------
    else if constexpr (std::tuple_size<T>::value > 0 &&
                       std::is_integral_v<typename T::value_type>) {
        printf("[ ");
        for (size_t i = 0; i < x.size(); i++) {
            print_scalar(x[i]);
            if (i + 1 < x.size())
                printf(", ");
        }
        printf(" ]");
    }

    // ------------------------------------------------------------------------
    // Unsupported
    // ------------------------------------------------------------------------
    else {
        static_assert(sizeof(T) == 0, "print_leaf(): unsupported leaf type");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 6) {

        std::cerr << "usage: ./remise "
                  << "[p0|p1] "
                  << "<log_num_items> "
                  << "<leafsize> "
                  << "<authorized_fraction> "
                  << "<num_requests>\n";

        return 1;
    }

    Role role;

    std::string r = argv[1];

    size_t log_nitems = 0;
    size_t leafsize = 0;
    size_t num_requests = 0;

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
        std::cerr << "authorized_fraction must be in [0,1]\n";

        return 1;
    }

    if (num_requests == 0) {

        std::cerr << "num_requests must be > 0\n";

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

    boost::asio::co_spawn(
        io, run_party(io, ctx, role, log_nitems, leafsize, authorized_fraction, num_requests),
        boost::asio::detached);

    io.run();

    ctx.print_stats();

    return 0;
}
