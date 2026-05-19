#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_ASIO_ASIO_AWAITABLES_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_ASIO_ASIO_AWAITABLES_H

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <span>

namespace Firelands::Asio {

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;

/// Read up to @p buffer.size() bytes from @p socket (non-blocking async).
inline awaitable<std::size_t>
AsyncReadSome(boost::asio::ip::tcp::socket &socket, std::span<std::byte> buffer) {
  co_return co_await socket.async_read_some(
      boost::asio::buffer(buffer.data(), buffer.size()), use_awaitable);
}

inline awaitable<std::size_t>
AsyncReadSome(boost::asio::ip::tcp::socket &socket, std::span<uint8_t> buffer) {
  co_return co_await socket.async_read_some(
      boost::asio::buffer(buffer.data(), buffer.size()), use_awaitable);
}

/// Write the full @p data buffer to @p socket.
inline awaitable<std::size_t>
AsyncWrite(boost::asio::ip::tcp::socket &socket, std::span<const uint8_t> data) {
  co_return co_await boost::asio::async_write(
      socket, boost::asio::buffer(data.data(), data.size()), use_awaitable);
}

inline awaitable<std::size_t>
AsyncWrite(boost::asio::ip::tcp::socket &socket, std::span<const std::byte> data) {
  co_return co_await boost::asio::async_write(
      socket, boost::asio::buffer(data.data(), data.size()), use_awaitable);
}

/// Spawn a coroutine on @p executor (detached; log errors in the factory).
template <typename Executor, typename AwaitableFactory>
void SpawnDetached(Executor &&executor, AwaitableFactory &&factory) {
  co_spawn(std::forward<Executor>(executor),
           std::forward<AwaitableFactory>(factory), detached);
}

} // namespace Firelands::Asio

#endif // FIRELANDS_INFRASTRUCTURE_NETWORK_ASIO_ASIO_AWAITABLES_H
