/*

dialog.hpp
----------

Copyright (C) 2016, Tomislav Karastojkovic (http://www.alepho.com).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/


#pragma once

#include <string>
#include <string_view>
#include <stdexcept>
#include <memory>
#include <optional>
#include <chrono>
#include <atomic>
#include <utility>
#include <boost/asio.hpp>

namespace mailio
{

class dialog_error : public std::runtime_error
{
public:
    dialog_error(const std::string& msg, const std::string& details) : std::runtime_error(msg), details_(details)
    {
    }

    dialog_error(const char* msg, const std::string& details) : std::runtime_error(msg), details_(details)
    {
    }

    std::string details() const { return details_; }

protected:
    std::string details_;
};

/**
Dealing with network in a line oriented fashion.
Wraps a Boost.Asio stream (socket, ssl stream, etc.).
**/
template<typename Stream>
class dialog
{
public:
    using duration = std::chrono::steady_clock::duration;

    static constexpr std::size_t DEFAULT_MAX_LINE_LENGTH = 8192;

    dialog(Stream stream,
        std::size_t max_line_length = DEFAULT_MAX_LINE_LENGTH,
        std::optional<duration> timeout = std::nullopt)
        : stream_(std::move(stream)),
          max_line_length_(max_line_length),
          timeout_(timeout)
    {
    }

    virtual ~dialog() = default;

    /**
    Sending a line to network asynchronously.

    @param line  Line to send (CRLF added if missing).
    @param token Completion token (callback, use_awaitable, etc.).
    **/
    template<typename CompletionToken>
    auto write_line(std::string_view line, CompletionToken&& token)
    {
        std::string payload = normalize_line(line);
        return async_with_timeout<void(boost::system::error_code, std::size_t)>(
            [this, payload = std::move(payload)](auto handler) mutable
            {
                boost::asio::async_write(stream_, boost::asio::buffer(payload), std::move(handler));
            }, std::forward<CompletionToken>(token));
    }

    /**
    Writing raw buffers to network asynchronously.

    @param buffers Buffers to write.
    @param token   Completion token.
    **/
    template<typename ConstBufferSequence, typename CompletionToken>
    auto write_raw(const ConstBufferSequence& buffers, CompletionToken&& token)
    {
        return async_with_timeout<void(boost::system::error_code, std::size_t)>(
            [this, buffers](auto handler) mutable
            {
                boost::asio::async_write(stream_, buffers, std::move(handler));
            }, std::forward<CompletionToken>(token));
    }

    /**
    Receiving a line from network asynchronously.

    @param token Completion token.
    **/
    template<typename CompletionToken>
    auto read_line(CompletionToken&& token)
    {
        return boost::asio::async_compose<CompletionToken, void(boost::system::error_code, std::string)>(
            [this, started = false](auto& self, boost::system::error_code ec = {}, std::size_t = 0) mutable
            {
                if (!started)
                {
                    started = true;
                    auto pos = read_buffer_.find('\n');
                    if (pos != std::string::npos)
                    {
                        std::size_t line_length = (pos > 0 && read_buffer_[pos - 1] == '\r') ? pos - 1 : pos;
                        if (line_length > max_line_length_)
                        {
                            self.complete(boost::asio::error::message_size, std::string());
                            return;
                        }
                        std::string line = read_buffer_.substr(0, line_length);
                        read_buffer_.erase(0, pos + 1);
                        self.complete(ec, std::move(line));
                        return;
                    }

                    std::size_t max_size = max_line_length_ + 2;
                    async_with_timeout<void(boost::system::error_code, std::size_t)>(
                        [this, max_size](auto handler) mutable
                        {
                            auto buffer = boost::asio::dynamic_buffer(read_buffer_, max_size);
                            boost::asio::async_read_until(stream_, buffer, '\n', std::move(handler));
                        }, std::move(self));
                    return;
                }

                if (ec)
                {
                    self.complete(ec, std::string());
                    return;
                }

                auto pos = read_buffer_.find('\n');
                if (pos == std::string::npos)
                {
                    self.complete(boost::asio::error::invalid_argument, std::string());
                    return;
                }

                std::size_t line_length = (pos > 0 && read_buffer_[pos - 1] == '\r') ? pos - 1 : pos;
                if (line_length > max_line_length_)
                {
                    self.complete(boost::asio::error::message_size, std::string());
                    return;
                }
                std::string line = read_buffer_.substr(0, line_length);
                read_buffer_.erase(0, pos + 1);
                self.complete(ec, std::move(line));
            }, token, stream_);
    }

    /**
    Receiving exactly N bytes from network asynchronously.

    @param n     Number of bytes to read.
    @param token Completion token.
    **/
    template<typename CompletionToken>
    auto read_exactly(std::size_t n, CompletionToken&& token)
    {
        return boost::asio::async_compose<CompletionToken, void(boost::system::error_code, std::string)>(
            [this, n, started = false](auto& self, boost::system::error_code ec = {}, std::size_t = 0) mutable
            {
                if (!started)
                {
                    started = true;
                    if (n == 0)
                    {
                        self.complete(ec, std::string());
                        return;
                    }
                    if (read_buffer_.size() >= n)
                    {
                        std::string out(read_buffer_.data(), n);
                        read_buffer_.erase(0, n);
                        self.complete(ec, std::move(out));
                        return;
                    }
                    std::size_t remaining = n - read_buffer_.size();
                    async_with_timeout<void(boost::system::error_code, std::size_t)>(
                        [this, remaining](auto handler) mutable
                        {
                            auto buffer = boost::asio::dynamic_buffer(read_buffer_);
                            boost::asio::async_read(stream_, buffer, boost::asio::transfer_exactly(remaining), std::move(handler));
                        }, std::move(self));
                    return;
                }

                if (ec)
                {
                    self.complete(ec, std::string());
                    return;
                }
                if (read_buffer_.size() < n)
                {
                    self.complete(boost::asio::error::operation_aborted, std::string());
                    return;
                }
                std::string out(read_buffer_.data(), n);
                read_buffer_.erase(0, n);
                self.complete(ec, std::move(out));
            }, token, stream_);
    }

    template<typename Signature, typename Initiation, typename CompletionToken>
    auto async_with_timeout(Initiation initiation, CompletionToken&& token)
    {
        if (timeout_.has_value())
            return async_with_timeout<Signature>(*timeout_, std::move(initiation), std::forward<CompletionToken>(token));

        return boost::asio::async_compose<CompletionToken, Signature>(
            [initiation = std::move(initiation), started = false](auto& self, boost::system::error_code ec = {}, auto... results) mutable
            {
                if (!started)
                {
                    started = true;
                    initiation(std::move(self));
                    return;
                }
                self.complete(ec, std::move(results)...);
            }, token, stream_);
    }

    template<typename Signature, typename Initiation, typename CompletionToken>
    auto async_with_timeout(duration timeout, Initiation initiation, CompletionToken&& token)
    {
        struct timeout_state
        {
            std::atomic_bool timed_out{false};
        };

        return boost::asio::async_compose<CompletionToken, Signature>(
            [this, initiation = std::move(initiation), timeout,
                state = std::make_shared<timeout_state>(),
                timer = std::shared_ptr<boost::asio::steady_timer>(),
                started = false](auto& self, boost::system::error_code ec = {}, auto... results) mutable
            {
                if (!started)
                {
                    started = true;
                    timer = std::make_shared<boost::asio::steady_timer>(stream_.get_executor());
                    timer->expires_after(timeout);
                    timer->async_wait([this, state](boost::system::error_code timer_ec)
                    {
                        if (timer_ec)
                            return;
                        state->timed_out.store(true);
                        boost::system::error_code ignore_ec;
                        boost::asio::get_lowest_layer(stream_).cancel(ignore_ec);
                    });
                    initiation(std::move(self));
                    return;
                }
                if (timer)
                    timer->cancel();
                if (state->timed_out.load() && ec == boost::asio::error::operation_aborted)
                    ec = boost::asio::error::timed_out;
                self.complete(ec, std::move(results)...);
            }, token, stream_);
    }

    Stream& stream() { return stream_; }
    const Stream& stream() const { return stream_; }

    void max_line_length(std::size_t value) { max_line_length_ = value; }
    std::size_t max_line_length() const { return max_line_length_; }

    void timeout(std::optional<duration> value) { timeout_ = value; }
    std::optional<duration> timeout() const { return timeout_; }

protected:
    static std::string normalize_line(std::string_view line)
    {
        if (line.size() >= 2 && line.substr(line.size() - 2) == "\r\n")
            return std::string(line);
        if (!line.empty() && line.back() == '\n')
        {
            std::string out(line.substr(0, line.size() - 1));
            out += "\r\n";
            return out;
        }
        if (!line.empty() && line.back() == '\r')
        {
            std::string out(line);
            out += "\n";
            return out;
        }
        std::string out(line);
        out += "\r\n";
        return out;
    }

    Stream stream_;
    std::string read_buffer_;
    std::size_t max_line_length_;
    std::optional<duration> timeout_;
};

} // namespace mailio
