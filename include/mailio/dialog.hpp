/*

dialog.hpp
----------

Copyright (C) 2016, Tomislav Karastojkovic (http://www.alepho.com).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/


#pragma once

#include <string>
#include <stdexcept>
#include <memory>
#include <istream>
#include <boost/asio.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/classification.hpp>

#ifndef MAILIO_EXPORT
#define MAILIO_EXPORT
#endif

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
    dialog(Stream stream) : stream_(std::move(stream))
    {
    }

    virtual ~dialog() = default;

    /**
    Sending a line to network asynchronously.
    
    @param line  Line to send.
    @param token Completion token (callback, use_awaitable, etc.).
    **/
    template<typename CompletionToken>
    auto async_send(std::string line, CompletionToken&& token)
    {
        return boost::asio::async_compose<CompletionToken, void(boost::system::error_code, std::size_t)>(
            [this, line = std::move(line) + "\r\n"](auto& self, boost::system::error_code ec = {}, std::size_t bytes_transferred = 0) mutable
            {
                if (bytes_transferred == 0 && !ec)
                {
                    // Start write
                    // We need to keep the string alive. The lambda capture 'line' does that.
                    // We pass a view (buffer) to async_write.
                    boost::asio::async_write(stream_, boost::asio::buffer(line), std::move(self));
                }
                else
                {
                    // Finished
                    if (ec)
                        self.complete(ec, bytes_transferred);
                    else
                        self.complete(ec, bytes_transferred);
                }
            }, token, stream_);
    }

    /**
    Receiving a line from network asynchronously.

    @param raw   Flag if the receiving is raw (no CRLF is truncated) or not.
    @param token Completion token.
    **/
    template<typename CompletionToken>
    auto async_receive(bool raw, CompletionToken&& token)
    {
        return boost::asio::async_compose<CompletionToken, void(boost::system::error_code, std::string)>(
            [this, raw](auto& self, boost::system::error_code ec = {}, std::size_t bytes_transferred = 0) mutable
            {
                if (bytes_transferred == 0 && !ec)
                {
                    boost::asio::async_read_until(stream_, strmbuf_, "\n", std::move(self));
                }
                else
                {
                    std::string line;
                    if (!ec)
                    {
                        std::istream is(&strmbuf_);
                        std::getline(is, line);
                        if (!raw)
                            boost::algorithm::trim_if(line, boost::algorithm::is_any_of("\r\n"));
                    }
                    self.complete(ec, line);
                }
            }, token, stream_);
    }

    Stream& stream() { return stream_; }

protected:
    Stream stream_;
    boost::asio::streambuf strmbuf_;
};

} // namespace mailio
