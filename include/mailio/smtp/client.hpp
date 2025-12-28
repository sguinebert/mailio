/*

smtp/client.hpp
---------------

Copyright (C) 2025, Sylvain Guinebert (github.com/sguinebert).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/


#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <utility>
#include <optional>
#include <cctype>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <mailio/net/dialog.hpp>
#include <mailio/net/upgradable_stream.hpp>
#include <mailio/mime/message.hpp>
#include <mailio/mime/mailboxes.hpp>
#include <mailio/codec/base64.hpp>
#include <mailio/codec/codec.hpp>
#include <mailio/smtp/types.hpp>
#include <mailio/smtp/error.hpp>

namespace mailio::smtp
{

class client
{
public:
    using executor_type = boost::asio::any_io_executor;
    using tcp = boost::asio::ip::tcp;

    explicit client(executor_type executor)
        : executor_(executor)
    {
    }

    explicit client(boost::asio::io_context& context)
        : client(context.get_executor())
    {
    }

    executor_type get_executor() const { return executor_; }

    boost::asio::awaitable<void> connect(const std::string& host, unsigned short port)
    {
        co_await connect(host, std::to_string(port));
    }

    boost::asio::awaitable<void> connect(const std::string& host, const std::string& service)
    {
        remote_host_ = host;

        tcp::resolver resolver(executor_);
        auto endpoints = co_await resolver.async_resolve(host, service, boost::asio::use_awaitable);

        mailio::upgradable_stream stream(executor_);
        co_await boost::asio::async_connect(stream.lowest_layer(), endpoints, boost::asio::use_awaitable);

        dialog_.emplace(std::move(stream));
    }

    boost::asio::awaitable<reply> read_greeting()
    {
        reply rep = co_await read_reply();
        if (rep.status != 220)
            throw error("Connection rejection.", rep.message());
        co_return rep;
    }

    boost::asio::awaitable<reply> ehlo(std::string domain = {})
    {
        const std::string helo_name = domain.empty() ? default_hostname() : std::move(domain);
        reply rep = co_await command("EHLO " + helo_name);
        if (!rep.is_positive_completion())
        {
            rep = co_await command("HELO " + helo_name);
            if (!rep.is_positive_completion())
                throw error("Initial message rejection.", rep.message());
            capabilities_.entries.clear();
            co_return rep;
        }

        parse_capabilities(rep);
        co_return rep;
    }

    boost::asio::awaitable<void> start_tls(boost::asio::ssl::context& context, std::string sni = {})
    {
        reply rep = co_await command("STARTTLS");
        if (rep.status != 220)
            throw error("STARTTLS failure.", rep.message());

        dialog_type& dlg = dialog();
        const std::size_t max_len = dlg.max_line_length();
        const auto timeout = dlg.timeout();

        mailio::upgradable_stream stream = std::move(dlg.stream());
        if (sni.empty())
            sni = remote_host_;
        co_await stream.start_tls(context, std::move(sni));

        dialog_.emplace(std::move(stream), max_len, timeout);
    }

    boost::asio::awaitable<void> authenticate(const std::string& username, const std::string& password, auth_method method)
    {
        switch (method)
        {
            case auth_method::plain:
                co_await authenticate_plain(username, password);
                break;
            case auth_method::login:
                co_await authenticate_login(username, password);
                break;
        }
    }

    boost::asio::awaitable<reply> send(const mailio::message& msg, const envelope& env = envelope{})
    {
        std::string mail_from = env.mail_from;
        if (mail_from.empty())
        {
            const auto sender = msg.sender();
            if (!sender.address.empty())
                mail_from = sender.address;
            else
            {
                const auto from = msg.from();
                if (!from.addresses.empty())
                    mail_from = from.addresses.front().address;
            }
        }
        if (mail_from.empty())
            throw error("Mail sender is missing.", "");

        std::vector<std::string> recipients = env.rcpt_to;
        if (recipients.empty())
            recipients = collect_recipients(msg);
        recipients = dedup(recipients);
        if (recipients.empty())
            throw error("No recipients.", "");

        reply rep = co_await command("MAIL FROM: <" + mail_from + ">");
        if (!rep.is_positive_completion())
            throw error("Mail sender rejection.", rep.message());

        for (const auto& rcpt : recipients)
        {
            rep = co_await command("RCPT TO: <" + rcpt + ">");
            if (!rep.is_positive_completion())
                throw error("Mail recipient rejection.", rep.message());
        }

        rep = co_await command("DATA");
        if (!rep.is_positive_intermediate())
            throw error("Mail message rejection.", rep.message());

        std::string data;
        message_format_options_t opts;
        opts.dot_escape = true;
        opts.add_bcc_header = false;
        msg.format(data, opts);
        data += "\r\n.\r\n";

        co_await dialog().write_raw(boost::asio::buffer(data), boost::asio::use_awaitable);

        rep = co_await read_reply();
        if (!rep.is_positive_completion())
            throw error("Mail message rejection.", rep.message());

        co_return rep;
    }

    boost::asio::awaitable<reply> noop()
    {
        co_return co_await command("NOOP");
    }

    boost::asio::awaitable<reply> rset()
    {
        co_return co_await command("RSET");
    }

    boost::asio::awaitable<reply> quit()
    {
        co_return co_await command("QUIT");
    }

    const capabilities& server_capabilities() const { return capabilities_; }

private:
    using dialog_type = mailio::dialog<mailio::upgradable_stream>;

    dialog_type& dialog()
    {
        if (!dialog_.has_value())
            throw error("Connection is not established.", "");
        return *dialog_;
    }

    boost::asio::awaitable<reply> command(std::string_view line)
    {
        co_await dialog().write_line(line, boost::asio::use_awaitable);
        co_return co_await read_reply();
    }

    boost::asio::awaitable<reply> read_reply()
    {
        reply rep;

        while (true)
        {
            std::string line = co_await dialog().read_line(boost::asio::use_awaitable);
            if (line.size() < 3)
                throw error("Parsing server failure.", line);

            if (!std::isdigit(static_cast<unsigned char>(line[0])) ||
                !std::isdigit(static_cast<unsigned char>(line[1])) ||
                !std::isdigit(static_cast<unsigned char>(line[2])))
                throw error("Parsing server failure.", line);

            const int code = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');

            bool last = true;
            if (line.size() >= 4)
            {
                if (line[3] == '-')
                    last = false;
                else if (line[3] != ' ')
                    throw error("Parsing server failure.", line);
            }

            std::string text;
            if (line.size() > 4)
                text = line.substr(4);

            if (rep.status == 0)
                rep.status = code;
            else if (rep.status != code)
                throw error("Parsing server failure.", line);

            rep.lines.push_back(std::move(text));

            if (last)
                break;
        }

        co_return rep;
    }

    boost::asio::awaitable<void> authenticate_plain(const std::string& username, const std::string& password)
    {
        std::string auth;
        auth.reserve(username.size() + password.size() + 2);
        auth.push_back('\0');
        auth += username;
        auth.push_back('\0');
        auth += password;

        const auto policy = static_cast<std::string::size_type>(mailio::codec::line_len_policy_t::NONE);
        mailio::base64 b64(policy, policy);
        const auto encoded_lines = b64.encode(auth);
        const std::string encoded = join_lines(encoded_lines);

        reply rep = co_await command("AUTH PLAIN " + encoded);
        if (rep.status == 334)
            rep = co_await command(encoded);

        if (!rep.is_positive_completion())
            throw error("Authentication rejection.", rep.message());
    }

    boost::asio::awaitable<void> authenticate_login(const std::string& username, const std::string& password)
    {
        reply rep = co_await command("AUTH LOGIN");
        if (rep.status != 334)
            throw error("Authentication rejection.", rep.message());

        const auto policy = static_cast<std::string::size_type>(mailio::codec::line_len_policy_t::NONE);
        mailio::base64 b64(policy, policy);
        std::string encoded_user = join_lines(b64.encode(username));
        std::string encoded_pass = join_lines(b64.encode(password));

        rep = co_await command(encoded_user);
        if (rep.status != 334)
            throw error("Username rejection.", rep.message());

        rep = co_await command(encoded_pass);
        if (!rep.is_positive_completion())
            throw error("Password rejection.", rep.message());
    }

    static std::string join_lines(const std::vector<std::string>& lines)
    {
        std::string out;
        for (const auto& line : lines)
            out += line;
        return out;
    }

    static std::string default_hostname()
    {
        try
        {
            return boost::asio::ip::host_name();
        }
        catch (...)
        {
            return "localhost";
        }
    }

    void parse_capabilities(const reply& rep)
    {
        capabilities_.entries.clear();
        if (rep.lines.empty())
            return;

        std::size_t start = 0;
        if (rep.lines.size() > 1)
        {
            const std::string& first = rep.lines.front();
            if (first.find(' ') == std::string::npos)
                start = 1;
        }

        for (std::size_t i = start; i < rep.lines.size(); ++i)
        {
            const std::string& line = rep.lines[i];
            if (line.empty())
                continue;

            const auto space_pos = line.find(' ');
            const std::string key = to_upper_ascii(space_pos == std::string::npos ?
                std::string_view(line) : std::string_view(line.data(), space_pos));

            std::vector<std::string> params;
            if (space_pos != std::string::npos && space_pos + 1 < line.size())
            {
                std::string_view rest(line.data() + space_pos + 1, line.size() - space_pos - 1);
                std::string current;
                for (char ch : rest)
                {
                    if (ch == ' ')
                    {
                        if (!current.empty())
                        {
                            params.push_back(current);
                            current.clear();
                        }
                        continue;
                    }
                    current.push_back(ch);
                }
                if (!current.empty())
                    params.push_back(current);
            }

            auto& slot = capabilities_.entries[key];
            slot.insert(slot.end(), params.begin(), params.end());
        }
    }

    static std::vector<std::string> collect_recipients(const mailio::message& msg)
    {
        std::vector<std::string> recipients;
        append_mailboxes(recipients, msg.recipients());
        append_mailboxes(recipients, msg.cc_recipients());
        append_mailboxes(recipients, msg.bcc_recipients());
        return recipients;
    }

    static void append_mailboxes(std::vector<std::string>& out, const mailio::mailboxes& boxes)
    {
        for (const auto& addr : boxes.addresses)
        {
            if (!addr.address.empty())
                out.push_back(addr.address);
        }
        for (const auto& group : boxes.groups)
        {
            for (const auto& member : group.members)
            {
                if (!member.address.empty())
                    out.push_back(member.address);
            }
        }
    }

    static std::vector<std::string> dedup(const std::vector<std::string>& addresses)
    {
        std::vector<std::string> out;
        std::unordered_set<std::string> seen;

        for (const auto& addr : addresses)
        {
            if (addr.empty())
                continue;
            std::string key = to_lower_ascii(addr);
            if (seen.insert(key).second)
                out.push_back(addr);
        }

        return out;
    }

    static std::string to_lower_ascii(std::string_view input)
    {
        std::string out;
        out.reserve(input.size());
        for (char ch : input)
        {
            if (ch >= 'A' && ch <= 'Z')
                out.push_back(static_cast<char>(ch + ('a' - 'A')));
            else
                out.push_back(ch);
        }
        return out;
    }

    static std::string to_upper_ascii(std::string_view input)
    {
        std::string out;
        out.reserve(input.size());
        for (char ch : input)
        {
            if (ch >= 'a' && ch <= 'z')
                out.push_back(static_cast<char>(ch - ('a' - 'A')));
            else
                out.push_back(ch);
        }
        return out;
    }

    executor_type executor_;
    std::optional<dialog_type> dialog_;
    std::string remote_host_;
    capabilities capabilities_;
};

} // namespace mailio::smtp
