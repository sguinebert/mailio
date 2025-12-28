/*

pop3.hpp
--------

Copyright (C) 2016, Tomislav Karastojkovic (http://www.alepho.com).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/


#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <utility>
#include <istream>
#include <sstream>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/streambuf.hpp>
#include <mailio/net/upgradable_stream.hpp>
#include <mailio/net/dialog.hpp>
#include <mailio/mime/message.hpp>

namespace mailio
{


/**
Error thrown by POP3 client.
**/
class pop3_error : public dialog_error
{
public:
    pop3_error(const std::string& msg, const std::string& details) : dialog_error(msg, details) {}
    pop3_error(const char* msg, const std::string& details) : dialog_error(msg, details) {}
};


/**
Base class for POP3 client containing common logic and constants.
**/
class pop3_base
{
public:
    enum class auth_method_t {LOGIN};

    typedef std::map<unsigned, unsigned long> message_list_t;
    typedef std::map<unsigned, std::string> uidl_list_t;

    struct mailbox_stat_t
    {
        unsigned int messages_no;
        unsigned long mailbox_size;
        mailbox_stat_t() : messages_no(0), mailbox_size(0) {}
    };

protected:
    static const char TOKEN_SEPARATOR_CHAR = ' ';
    inline static const std::string OK_RESPONSE = "+OK";
    inline static const std::string ERR_RESPONSE = "-ERR";
    inline static const std::string END_OF_DATA = ".";

    static std::tuple<std::string, std::string> parse_status(const std::string& line)
    {
        std::string::size_type pos = line.find(TOKEN_SEPARATOR_CHAR);
        std::string status = line.substr(0, pos);
        std::string rest = (pos != std::string::npos) ? line.substr(pos + 1) : "";
        if (status != OK_RESPONSE && status != ERR_RESPONSE)
            throw pop3_error("Unknown response status.", line);
        return std::make_tuple(status, rest);
    }
    
    static bool is_ok(const std::string& status) { return status == OK_RESPONSE; }
};


/**
Stable POP3 client implementation.
**/
class pop3 : public pop3_base
{
public:
    using dialog_type = dialog<upgradable_stream>;
    using capabilities_t = std::vector<std::string>;

    explicit pop3(boost::asio::any_io_executor executor)
        : dlg_(upgradable_stream(executor))
    {
    }

    explicit pop3(boost::asio::io_context& io_context)
        : pop3(io_context.get_executor())
    {
    }

    virtual ~pop3() = default;

    boost::asio::awaitable<void> connect(const std::string& host, uint16_t port)
    {
        host_ = host;
        boost::asio::ip::tcp::resolver resolver(dlg_.stream().get_executor());
        auto endpoints = co_await resolver.async_resolve(host, std::to_string(port), boost::asio::use_awaitable);
        co_await boost::asio::async_connect(dlg_.stream().lowest_layer(), endpoints, boost::asio::use_awaitable);
    }

    boost::asio::awaitable<std::string> read_greeting()
    {
        std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
        auto [status, msg] = parse_status(line);
        if (!is_ok(status))
            throw pop3_error("Connection to server failure.", msg);
        co_return msg;
    }

    boost::asio::awaitable<capabilities_t> capa()
    {
        co_await send_command("CAPA");
        (void)co_await read_ok_response("Capabilities failure.");

        capabilities_t caps;
        while (true)
        {
            std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
            if (line == END_OF_DATA)
                break;
            if (!line.empty() && line[0] == '.')
                line.erase(0, 1);
            caps.push_back(std::move(line));
        }
        co_return caps;
    }

    boost::asio::awaitable<void> start_tls(boost::asio::ssl::context& context, std::string sni = {})
    {
        co_await send_command("STLS");
        std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
        auto [status, msg] = parse_status(line);
        if (!is_ok(status))
            throw pop3_error("STARTTLS failure.", msg);

        if (sni.empty())
            sni = host_;
        co_await dlg_.stream().start_tls(context, std::move(sni));
    }

    boost::asio::awaitable<void> login(const std::string& username, const std::string& password)
    {
        co_await send_command("USER " + username);
        (void)co_await read_ok_response("Username rejection.");

        co_await send_command("PASS " + password);
        (void)co_await read_ok_response("Password rejection.");
    }

    boost::asio::awaitable<mailbox_stat_t> stat()
    {
        co_await send_command("STAT");
        std::string msg = co_await read_ok_response("Reading statistics failure.");
        mailbox_stat_t stat;
        std::istringstream iss(msg);
        if (!(iss >> stat.messages_no >> stat.mailbox_size))
            throw pop3_error("Parser failure.", msg);
        co_return stat;
    }

    boost::asio::awaitable<message_list_t> list()
    {
        co_await send_command("LIST");
        (void)co_await read_ok_response("Listing all messages failure.");

        message_list_t msg_list;
        while (true)
        {
            std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
            if (line == END_OF_DATA)
                break;
            if (!line.empty() && line[0] == '.')
                line.erase(0, 1);
            std::istringstream iss(line);
            unsigned num = 0;
            unsigned long size = 0;
            if (iss >> num >> size)
                msg_list[num] = size;
        }
        co_return msg_list;
    }

    boost::asio::awaitable<message_list_t> list(unsigned message_no)
    {
        co_await send_command("LIST " + std::to_string(message_no));
        std::string msg = co_await read_ok_response("Listing message failure.");

        message_list_t msg_list;
        std::istringstream iss(msg);
        unsigned num = 0;
        unsigned long size = 0;
        if (iss >> num >> size)
            msg_list[num] = size;
        co_return msg_list;
    }

    boost::asio::awaitable<std::string> retr(unsigned long message_no)
    {
        co_await send_command("RETR " + std::to_string(message_no));
        (void)co_await read_ok_response("Fetching message failure.");

        std::string msg_str;
        while (true)
        {
            std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
            if (line == END_OF_DATA)
                break;
            if (!line.empty() && line[0] == '.')
                line.erase(0, 1);
            msg_str += line + "\r\n";
        }
        co_return msg_str;
    }

    boost::asio::awaitable<void> dele(unsigned long message_no)
    {
        co_await send_command("DELE " + std::to_string(message_no));
        (void)co_await read_ok_response("Removing message failure.");
    }

    boost::asio::awaitable<void> rset()
    {
        co_await send_command("RSET");
        (void)co_await read_ok_response("Reset failure.");
    }

    boost::asio::awaitable<void> noop()
    {
        co_await send_command("NOOP");
        (void)co_await read_ok_response("Noop failure.");
    }

    boost::asio::awaitable<void> quit()
    {
        co_await send_command("QUIT");
        (void)co_await read_ok_response("Quit failure.");
    }

    dialog_type& dialog() { return dlg_; }
    const dialog_type& dialog() const { return dlg_; }

private:
    boost::asio::awaitable<void> send_command(const std::string& command)
    {
        co_await dlg_.write_line(command, boost::asio::use_awaitable);
    }

    boost::asio::awaitable<std::string> read_ok_response(const std::string& error_message)
    {
        std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
        auto [status, msg] = parse_status(line);
        if (!is_ok(status))
            throw pop3_error(error_message, msg);
        co_return msg;
    }

    dialog_type dlg_;
    std::string host_;
};


/**
POP3 client implementation template.
**/
template<typename Stream>
class pop3_client : public pop3_base
{
public:
    using dialog_type = dialog<Stream>;

    pop3_client(Stream stream) : dlg_(std::move(stream)) {}
    virtual ~pop3_client() = default;

    template<typename CompletionToken>
    auto async_read_greeting(CompletionToken&& token)
    {
        return boost::asio::co_spawn(dlg_.stream().get_executor(),
            [this]() -> boost::asio::awaitable<std::string> {
                co_return co_await this->read_greeting_impl();
            }, std::forward<CompletionToken>(token));
    }

    template<typename CompletionToken>
    auto async_authenticate(std::string username, std::string password, auth_method_t method, CompletionToken&& token)
    {
        return boost::asio::co_spawn(dlg_.stream().get_executor(),
            [this, username = std::move(username), password = std::move(password), method]() -> boost::asio::awaitable<std::string> {
                std::string greeting = co_await this->read_greeting_impl();
                co_await this->auth_login_impl(username, password);
                co_return greeting;
            }, std::forward<CompletionToken>(token));
    }

    template<typename CompletionToken>
    auto async_list(unsigned message_no, CompletionToken&& token)
    {
        return boost::asio::co_spawn(dlg_.stream().get_executor(),
            [this, message_no]() -> boost::asio::awaitable<message_list_t> {
                co_return co_await this->list_impl(message_no);
            }, std::forward<CompletionToken>(token));
    }

    template<typename CompletionToken>
    auto async_uidl(unsigned message_no, CompletionToken&& token)
    {
        return boost::asio::co_spawn(dlg_.stream().get_executor(),
            [this, message_no]() -> boost::asio::awaitable<uidl_list_t> {
                co_return co_await this->uidl_impl(message_no);
            }, std::forward<CompletionToken>(token));
    }

    template<typename CompletionToken>
    auto async_statistics(CompletionToken&& token)
    {
        return boost::asio::co_spawn(dlg_.stream().get_executor(),
            [this]() -> boost::asio::awaitable<mailbox_stat_t> {
                co_return co_await this->statistics_impl();
            }, std::forward<CompletionToken>(token));
    }

    template<typename CompletionToken>
    auto async_fetch(unsigned long message_no, bool header_only, CompletionToken&& token)
    {
        return boost::asio::co_spawn(dlg_.stream().get_executor(),
            [this, message_no, header_only]() -> boost::asio::awaitable<message> {
                co_return co_await this->fetch_impl(message_no, header_only);
            }, std::forward<CompletionToken>(token));
    }

    template<typename CompletionToken>
    auto async_remove(unsigned long message_no, CompletionToken&& token)
    {
        return boost::asio::co_spawn(dlg_.stream().get_executor(),
            [this, message_no]() -> boost::asio::awaitable<void> {
                co_await this->remove_impl(message_no);
            }, std::forward<CompletionToken>(token));
    }

    template<typename CompletionToken>
    auto async_quit(CompletionToken&& token)
    {
        return boost::asio::co_spawn(dlg_.stream().get_executor(),
            [this]() -> boost::asio::awaitable<void> {
                co_await this->quit_impl();
            }, std::forward<CompletionToken>(token));
    }

    template<typename SSLContext>
    boost::asio::awaitable<pop3_client<boost::asio::ssl::stream<Stream>>> starttls(SSLContext& context)
    {
        co_await dlg_.write_line("STLS", boost::asio::use_awaitable);
        std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
        auto [status, msg] = parse_status(line);
        if (!is_ok(status))
            throw pop3_error("STARTTLS failure.", msg);

        Stream plain_stream = std::move(dlg_.stream());
        boost::asio::ssl::stream<Stream> ssl_stream(std::move(plain_stream), context);
        co_await ssl_stream.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_awaitable);
        co_return pop3_client<boost::asio::ssl::stream<Stream>>(std::move(ssl_stream));
    }

private:
    boost::asio::awaitable<std::string> read_greeting_impl()
    {
        std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
        auto [status, msg] = parse_status(line);
        if (!is_ok(status))
            throw pop3_error("Connection to server failure.", msg);
        co_return msg;
    }

    boost::asio::awaitable<void> auth_login_impl(const std::string& username, const std::string& password)
    {
        co_await dlg_.write_line("USER " + username, boost::asio::use_awaitable);
        std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
        auto [status, msg] = parse_status(line);
        if (!is_ok(status))
            throw pop3_error("Username rejection.", msg);

        co_await dlg_.write_line("PASS " + password, boost::asio::use_awaitable);
        line = co_await dlg_.read_line(boost::asio::use_awaitable);
        std::tie(status, msg) = parse_status(line);
        if (!is_ok(status))
            throw pop3_error("Password rejection.", msg);
    }

    boost::asio::awaitable<message_list_t> list_impl(unsigned message_no)
    {
        message_list_t msg_list;
        if (message_no > 0) {
            co_await dlg_.write_line("LIST " + std::to_string(message_no), boost::asio::use_awaitable);
            std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
            auto [status, msg] = parse_status(line);
            if (!is_ok(status)) throw pop3_error("Listing message failure.", msg);
            std::istringstream iss(msg);
            unsigned num; unsigned long size;
            if (iss >> num >> size) msg_list[num] = size;
        } else {
            co_await dlg_.write_line("LIST", boost::asio::use_awaitable);
            std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
            auto [status, msg] = parse_status(line);
            if (!is_ok(status)) throw pop3_error("Listing all messages failure.", msg);
            while (true) {
                line = co_await dlg_.read_line(boost::asio::use_awaitable);
                if (line == END_OF_DATA) break;
                std::istringstream iss(line);
                unsigned num; unsigned long size;
                if (iss >> num >> size) msg_list[num] = size;
            }
        }
        co_return msg_list;
    }

    boost::asio::awaitable<uidl_list_t> uidl_impl(unsigned message_no)
    {
        uidl_list_t uidl_list;
        if (message_no > 0) {
            co_await dlg_.write_line("UIDL " + std::to_string(message_no), boost::asio::use_awaitable);
            std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
            auto [status, msg] = parse_status(line);
            if (!is_ok(status)) throw pop3_error("Listing message failure.", msg);
            std::istringstream iss(msg);
            unsigned num; std::string uid;
            if (iss >> num >> uid) uidl_list[num] = uid;
        } else {
            co_await dlg_.write_line("UIDL", boost::asio::use_awaitable);
            std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
            auto [status, msg] = parse_status(line);
            if (!is_ok(status)) throw pop3_error("Listing all messages failure.", msg);
            while (true) {
                line = co_await dlg_.read_line(boost::asio::use_awaitable);
                if (line == END_OF_DATA) break;
                std::istringstream iss(line);
                unsigned num; std::string uid;
                if (iss >> num >> uid) uidl_list[num] = uid;
            }
        }
        co_return uidl_list;
    }

    boost::asio::awaitable<mailbox_stat_t> statistics_impl()
    {
        co_await dlg_.write_line("STAT", boost::asio::use_awaitable);
        std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
        auto [status, msg] = parse_status(line);
        if (!is_ok(status)) throw pop3_error("Reading statistics failure.", msg);
        mailbox_stat_t stat;
        std::istringstream iss(msg);
        if (!(iss >> stat.messages_no >> stat.mailbox_size))
            throw pop3_error("Parser failure.", msg);
        co_return stat;
    }

    boost::asio::awaitable<message> fetch_impl(unsigned long message_no, bool header_only)
    {
        message msg;
        if (header_only)
            co_await dlg_.write_line("TOP " + std::to_string(message_no) + " 0", boost::asio::use_awaitable);
        else
            co_await dlg_.write_line("RETR " + std::to_string(message_no), boost::asio::use_awaitable);

        std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
        auto [status, resp_msg] = parse_status(line);
        if (!is_ok(status)) throw pop3_error("Fetching message failure.", resp_msg);

        std::string msg_str;
        while (true) {
            line = co_await dlg_.read_line(boost::asio::use_awaitable);
            if (line == END_OF_DATA) break;
            if (line.size() > 1 && line[0] == '.') line = line.substr(1);
            msg_str += line + "\r\n";
        }
        msg.parse(msg_str);
        co_return msg;
    }

    boost::asio::awaitable<void> remove_impl(unsigned long message_no)
    {
        co_await dlg_.write_line("DELE " + std::to_string(message_no), boost::asio::use_awaitable);
        std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
        auto [status, msg] = parse_status(line);
        if (!is_ok(status)) throw pop3_error("Removing message failure.", msg);
    }

    boost::asio::awaitable<void> quit_impl()
    {
        co_await dlg_.write_line("QUIT", boost::asio::use_awaitable);
        std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
        auto [status, msg] = parse_status(line);
        if (!is_ok(status)) throw pop3_error("Quit failure.", msg);
    }

    dialog_type dlg_;
};

} // namespace mailio


#ifdef _MSC_VER
#pragma warning(pop)
#endif

