#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <vector>
#include <cstdint>
#include <string>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <type_traits>
#include <tuple>
#include <utility>
#include <thread>

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::async_write;
using boost::asio::async_read;
using boost::asio::buffer;
using boost::asio::ip::tcp;
using namespace boost::asio::experimental::awaitable_operators;

// -----------------------------------------------------------------------------
// Role enum
// -----------------------------------------------------------------------------
enum class Role { P0, P1, P2 };

// -----------------------------------------------------------------------------
// Communication statistics
// -----------------------------------------------------------------------------
struct CommStats {
    uint64_t bytes_sent = 0;
    uint64_t bytes_recv = 0;

    uint64_t total() const {
        return bytes_sent + bytes_recv;
    }

    void print(const std::string& prefix = "") const {
        std::cout
            << prefix
            << "sent=" << bytes_sent
            << " bytes, recv=" << bytes_recv
            << " bytes, total=" << total()
            << " bytes\n";
    }
};

// -----------------------------------------------------------------------------
// Bundle
// -----------------------------------------------------------------------------
template<typename... Ts>
struct Bundle {
    std::tuple<Ts...> data;

    Bundle() = default;

    Bundle(Ts... xs)
        : data(std::move(xs)...)
    {}
};

// -----------------------------------------------------------------------------
// Type trait: detect std::vector
// -----------------------------------------------------------------------------
template<typename T>
struct is_std_vector : std::false_type {};

template<typename T, typename A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_vector_v = is_std_vector<T>::value;

// -----------------------------------------------------------------------------
// Coroutine-safe tuple iteration
// -----------------------------------------------------------------------------
namespace net_detail {

template<std::size_t I = 0, typename Tuple, typename F>
awaitable<void> tuple_for_each(Tuple& t, F&& f)
{
    if constexpr (I < std::tuple_size_v<Tuple>) {
        co_await f(std::get<I>(t));
        co_await tuple_for_each<I + 1>(
            t,
            std::forward<F>(f));
    } else {
        co_return;
    }
}

} // namespace net_detail

// -----------------------------------------------------------------------------
// NetPeer
// -----------------------------------------------------------------------------
struct NetPeer {

    Role role;
    tcp::socket sock;

    CommStats stats;

    explicit NetPeer(Role r, tcp::socket&& s)
        : role(r),
          sock(std::move(s))
    {
        sock.set_option(tcp::no_delay(true));
    }

    // -----------------------------------------------------------------
    // Primitive send
    // -----------------------------------------------------------------
    template<typename T>
    awaitable<void> send(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        stats.bytes_sent += sizeof(T);

        co_await async_write(
            sock,
            buffer(&value, sizeof(T)),
            use_awaitable);

        co_return;
    }

    // -----------------------------------------------------------------
    // Primitive recv
    // -----------------------------------------------------------------
    template<typename T>
    awaitable<T> recv()
    {
        static_assert(std::is_trivially_copyable_v<T>);

        T value{};

        stats.bytes_recv += sizeof(T);

        co_await async_read(
            sock,
            buffer(&value, sizeof(T)),
            use_awaitable);

        co_return value;
    }

    // -----------------------------------------------------------------
    // Vector send
    // -----------------------------------------------------------------
    template<typename T>
    awaitable<void> send(const std::vector<T>& vec)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        uint64_t sz = vec.size();

        stats.bytes_sent += sizeof(uint64_t);
        stats.bytes_sent += sizeof(T) * sz;

        co_await send(sz);

        if (sz > 0) {
            co_await async_write(
                sock,
                buffer(vec.data(), sizeof(T) * sz),
                use_awaitable);
        }

        co_return;
    }

    // -----------------------------------------------------------------
    // Vector recv
    // -----------------------------------------------------------------
    template<typename T>
    awaitable<std::vector<T>> recv_vector()
    {
        static_assert(std::is_trivially_copyable_v<T>);

        uint64_t sz = co_await recv<uint64_t>();

        std::vector<T> v(sz);

        stats.bytes_recv += sizeof(T) * sz;

        if (sz > 0) {
            co_await async_read(
                sock,
                buffer(v.data(), sizeof(T) * sz),
                use_awaitable);
        }

        co_return v;
    }

    // -----------------------------------------------------------------
    // Bundle send
    // -----------------------------------------------------------------
    template<typename... Ts>
    awaitable<void> send_bundle(const Bundle<Ts...>& b)
    {
        co_await net_detail::tuple_for_each(
            b.data,
            [&](auto const& x) -> awaitable<void>
            {
                co_await send(x);
                co_return;
            });

        co_return;
    }

    // -----------------------------------------------------------------
    // Bundle recv
    // -----------------------------------------------------------------
    template<typename... Ts>
    awaitable<Bundle<Ts...>> recv_bundle()
    {
        Bundle<Ts...> b;

        co_await net_detail::tuple_for_each(
            b.data,
            [&](auto& x) -> awaitable<void>
            {
                using T = std::decay_t<decltype(x)>;

                if constexpr (is_std_vector_v<T>) {

                    using Elem = typename T::value_type;

                    x = co_await recv_vector<Elem>();

                } else {

                    x = co_await recv<T>();

                }

                co_return;
            });

        co_return b;
    }

    // -----------------------------------------------------------------
    // Operator <<
    // -----------------------------------------------------------------
    template<typename T>
    awaitable<void> operator<<(const T& v)
    {
        co_await send(v);
        co_return;
    }

    // -----------------------------------------------------------------
    // Operator >>
    // -----------------------------------------------------------------
    template<typename T>
    awaitable<void> operator>>(T& v)
    {
        v = co_await recv<T>();
        co_return;
    }

    // -----------------------------------------------------------------
    // Vector <<
    // -----------------------------------------------------------------
    template<typename T>
    awaitable<void> operator<<(const std::vector<T>& v)
    {
        co_await send(v);
        co_return;
    }

    // -----------------------------------------------------------------
    // Vector >>
    // -----------------------------------------------------------------
    template<typename T>
    awaitable<void> operator>>(std::vector<T>& v)
    {
        v = co_await recv_vector<T>();
        co_return;
    }

    // -----------------------------------------------------------------
    // Bundle <<
    // -----------------------------------------------------------------
    template<typename... Ts>
    awaitable<void> operator<<(const Bundle<Ts...>& b)
    {
        co_await send_bundle(b);
        co_return;
    }

    // -----------------------------------------------------------------
    // Bundle >>
    // -----------------------------------------------------------------
    template<typename... Ts>
    awaitable<void> operator>>(Bundle<Ts...>& b)
    {
        b = co_await recv_bundle<Ts...>();
        co_return;
    }
};

// -----------------------------------------------------------------------------
// connect_with_retry
// -----------------------------------------------------------------------------
inline boost::asio::awaitable<tcp::socket>
connect_with_retry(
    boost::asio::io_context& io,
    const std::string& host,
    uint16_t port,
    int max_tries = -1)
{
    tcp::resolver resolver(io);
    tcp::socket sock(io);

    int attempt = 0;

    for (;;) {

        try {

            auto endpoints =
                co_await resolver.async_resolve(
                    host,
                    std::to_string(port),
                    use_awaitable);

            co_await boost::asio::async_connect(
                sock,
                endpoints,
                use_awaitable);

            co_return std::move(sock);

        } catch (const std::exception& e) {

            ++attempt;

            if (max_tries > 0 && attempt >= max_tries)
                throw;

            std::cerr
                << "[connect_with_retry] "
                << host << ":" << port
                << " failed (" << e.what()
                << "), retrying...\n";

            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));
        }
    }
}

// -----------------------------------------------------------------------------
// make_server
// -----------------------------------------------------------------------------
inline awaitable<tcp::socket>
make_server(
    boost::asio::io_context& io,
    uint16_t port)
{
    tcp::acceptor acc(
        io,
        tcp::endpoint(tcp::v4(), port));

    tcp::socket sock(io);

    co_await acc.async_accept(
        sock,
        use_awaitable);

    co_return std::move(sock);
}

// -----------------------------------------------------------------------------
// make_client
// -----------------------------------------------------------------------------
inline awaitable<tcp::socket>
make_client(
    boost::asio::io_context& io,
    const std::string& host,
    uint16_t port)
{
    tcp::resolver resolver(io);

    auto eps =
        co_await resolver.async_resolve(
            host,
            std::to_string(port),
            use_awaitable);

    tcp::socket sock(io);

    co_await boost::asio::async_connect(
        sock,
        eps,
        use_awaitable);

    co_return std::move(sock);
}

// -----------------------------------------------------------------------------
// NetContext
// -----------------------------------------------------------------------------
struct NetContext {

    Role self_role;

    std::vector<std::unique_ptr<NetPeer>> peers;

    explicit NetContext(Role r)
        : self_role(r)
    {}

    void add_peer(Role r, tcp::socket&& sock)
    {
        peers.emplace_back(
            std::make_unique<NetPeer>(
                r,
                std::move(sock)));
    }

    NetPeer& peer(Role r)
    {
        for (auto& p : peers)
            if (p->role == r)
                return *p;

        throw std::runtime_error("peer not found");
    }

    uint64_t bytes_sent() const
    {
        uint64_t total = 0;

        for (const auto& p : peers) {
            total += p->stats.bytes_sent;
        }

        return total;
    }

    uint64_t bytes_received() const
    {
        uint64_t total = 0;

        for (const auto& p : peers) {
            total += p->stats.bytes_recv;
        }

        return total;
    }

    void print_stats() const
    {
        for (const auto& p : peers) {

            std::string role_name;

            switch (p->role) {
                case Role::P0:
                    role_name = "P0";
                    break;

                case Role::P1:
                    role_name = "P1";
                    break;

                case Role::P2:
                    role_name = "P2";
                    break;
            }

            p->stats.print(role_name + " : ");
        }
    }
};