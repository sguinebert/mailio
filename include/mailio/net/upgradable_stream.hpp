#pragma once

#include <variant>
#include <string>
#include <utility>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace mailio
{

/**
Stable stream type that can be upgraded to TLS without changing the type.
**/
class upgradable_stream
{
public:
    using tcp = boost::asio::ip::tcp;
    using ssl_stream = boost::asio::ssl::stream<tcp::socket>;
    using executor_type = boost::asio::any_io_executor;
    using lowest_layer_type = tcp::socket;

    explicit upgradable_stream(tcp::socket socket)
        : stream_(std::move(socket))
    {
    }

    explicit upgradable_stream(executor_type executor)
        : stream_(tcp::socket(executor))
    {
    }

    executor_type get_executor() const
    {
        return std::visit([](const auto& stream) -> executor_type
        {
            return stream.get_executor();
        }, stream_);
    }

    lowest_layer_type& lowest_layer()
    {
        return std::visit([](auto& stream) -> lowest_layer_type&
        {
            return stream.lowest_layer();
        }, stream_);
    }

    const lowest_layer_type& lowest_layer() const
    {
        return std::visit([](const auto& stream) -> const lowest_layer_type&
        {
            return stream.lowest_layer();
        }, stream_);
    }

    bool is_tls() const noexcept
    {
        return std::holds_alternative<ssl_stream>(stream_);
    }

    template<typename MutableBufferSequence, typename CompletionToken>
    auto async_read_some(const MutableBufferSequence& buffers, CompletionToken&& token)
    {
        return std::visit([&](auto& stream) -> decltype(auto)
        {
            return stream.async_read_some(buffers, std::forward<CompletionToken>(token));
        }, stream_);
    }

    template<typename ConstBufferSequence, typename CompletionToken>
    auto async_write_some(const ConstBufferSequence& buffers, CompletionToken&& token)
    {
        return std::visit([&](auto& stream) -> decltype(auto)
        {
            return stream.async_write_some(buffers, std::forward<CompletionToken>(token));
        }, stream_);
    }

    boost::asio::awaitable<void> start_tls(boost::asio::ssl::context& context, std::string sni)
    {
        if (is_tls())
            co_return;

        auto socket = std::move(std::get<tcp::socket>(stream_));
        stream_.template emplace<ssl_stream>(std::move(socket), context);

        auto& tls_stream = std::get<ssl_stream>(stream_);
        if (!sni.empty())
        {
#if defined(SSL_CTRL_SET_TLSEXT_HOSTNAME)
            SSL_set_tlsext_host_name(tls_stream.native_handle(), sni.c_str());
#endif
        }

        co_await tls_stream.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_awaitable);
    }

private:
    std::variant<tcp::socket, ssl_stream> stream_;
};

} // namespace mailio
