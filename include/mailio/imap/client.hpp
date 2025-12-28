/*

imap.hpp
--------

Copyright (C) 2016, Tomislav Karastojkovic (http://www.alepho.com).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/


#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <list>
#include <map>
#include <utility>
#include <optional>
#include <variant>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <locale>
#include <memory>
#include <chrono>
#include <format>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <mailio/net/dialog.hpp>
#include <mailio/mime/message.hpp>
#include <mailio/codec/codec.hpp>
#include <mailio/export.hpp>


namespace mailio
{


class MAILIO_EXPORT imap_error : public std::runtime_error
{
public:
    explicit imap_error(const std::string& msg) : std::runtime_error(msg)
    {
    }

    explicit imap_error(const char* msg) : std::runtime_error(msg)
    {
    }
};


class MAILIO_EXPORT imap_base
{
public:
    inline static const std::string UNTAGGED_RESPONSE{"*"};
    inline static const std::string CONTINUE_RESPONSE{"+"};
    inline static const std::string RANGE_SEPARATOR{":"};
    inline static const std::string RANGE_ALL{"*"};
    inline static const std::string LIST_SEPARATOR{","};
    inline static const std::string TOKEN_SEPARATOR_STR{" "};
    inline static const std::string QUOTED_STRING_SEPARATOR{"\""};

    struct mailbox_stat_t
    {
        unsigned long messages_no;
        unsigned long recent_messages_no;
        unsigned long uid_next;
        unsigned long uid_validity;
        unsigned long unseen_messages_no;

        mailbox_stat_t() : messages_no(0), recent_messages_no(0), uid_next(0), uid_validity(0), unseen_messages_no(0)
        {
        }
    };

    struct mailbox_folder_t
    {
        std::vector<std::string> attributes;
        std::string hierarchy_delimiter;
        std::string name;
    };

    struct fetch_msg_t
    {
        unsigned long uid;
        unsigned long size;
        std::vector<std::string> flags;
    };

    enum class response_status_t {OK, NO, BAD, PREAUTH, BYE, UNKNOWN};

    struct response_line_t
    {
        std::vector<std::string> fragments;
        std::vector<std::string> literals;
    };

    struct response_t
    {
        std::string tag;
        response_status_t status{response_status_t::UNKNOWN};
        std::string text;
        std::vector<std::string> literals;
        std::vector<response_line_t> lines;
    };

    enum class auth_method_t {LOGIN};

    typedef std::pair<unsigned long, std::optional<unsigned long>> messages_range_t;

    static std::string messages_range_to_string(messages_range_t id_pair)
    {
        return std::to_string(id_pair.first) + (id_pair.second.has_value() ? RANGE_SEPARATOR + std::to_string(id_pair.second.value()) : RANGE_SEPARATOR + RANGE_ALL);
    }

    static std::string messages_range_list_to_string(std::list<messages_range_t> ranges)
    {
        return boost::algorithm::join(ranges | boost::adaptors::transformed(static_cast<std::string(*)(messages_range_t)>(messages_range_to_string)), LIST_SEPARATOR);
    }

    static std::string to_astring(const std::string& text)
    {
        return codec::surround_string(codec::escape_string(text, "\"\\"));
    }

    /**
    Converting a date to IMAP date string format (dd-Mon-yyyy).
    
    @param date Date to convert.
    @return     Date as IMAP formatted string.
    **/
    static std::string imap_date_to_string(const std::chrono::year_month_day& date)
    {
        std::chrono::sys_days sd{date};
        return std::format("{:%d-%b-%Y}", sd);
    }

    struct search_condition_t
    {
        enum key_type {ALL, SID_LIST, UID_LIST, SUBJECT, BODY, FROM, TO, BEFORE_DATE, ON_DATE, SINCE_DATE, NEW, RECENT, SEEN, UNSEEN} key;

        typedef std::variant
        <
            std::monostate,
            std::string,
            std::list<messages_range_t>,
            std::chrono::year_month_day
        >
        value_type;

        value_type value;
        std::string imap_string;

        search_condition_t(key_type condition_key, value_type condition_value = value_type()) : key(condition_key), value(condition_value)
        {
            try
            {
                switch (key)
                {
                    case ALL:
                        imap_string = "ALL";
                        break;

                    case SID_LIST:
                    {
                        imap_string = messages_range_list_to_string(std::get<std::list<messages_range_t>>(value));
                        break;
                    }

                    case UID_LIST:
                    {
                        imap_string = "UID " + messages_range_list_to_string(std::get<std::list<messages_range_t>>(value));
                        break;
                    }

                    case SUBJECT:
                        imap_string = "SUBJECT " + QUOTED_STRING_SEPARATOR + std::get<std::string>(value) + QUOTED_STRING_SEPARATOR;
                        break;

                    case BODY:
                        imap_string = "BODY " + QUOTED_STRING_SEPARATOR + std::get<std::string>(value) + QUOTED_STRING_SEPARATOR;
                        break;

                    case FROM:
                        imap_string = "FROM " + QUOTED_STRING_SEPARATOR + std::get<std::string>(value) + QUOTED_STRING_SEPARATOR;
                        break;

                    case TO:
                        imap_string = "TO " + QUOTED_STRING_SEPARATOR + std::get<std::string>(value) + QUOTED_STRING_SEPARATOR;
                        break;

                    case BEFORE_DATE:
                        imap_string = "BEFORE " + imap_date_to_string(std::get<std::chrono::year_month_day>(value));
                        break;

                    case ON_DATE:
                        imap_string = "ON " + imap_date_to_string(std::get<std::chrono::year_month_day>(value));
                        break;

                    case SINCE_DATE:
                        imap_string = "SINCE " + imap_date_to_string(std::get<std::chrono::year_month_day>(value));
                        break;

                    case NEW:
                        imap_string = "NEW";
                        break;

                    case RECENT:
                        imap_string = "RECENT";
                        break;

                    case SEEN:
                        imap_string = "SEEN";
                        break;

                    case UNSEEN:
                        imap_string = "UNSEEN";
                        break;
                }
            }
            catch (...)
            {
                throw imap_error("Invalid search condition.");
            }
        }
    };
};


    template<typename Stream>
    class imap_client : public imap_base
    {
    public:
        struct response_token_t
        {
            enum class token_type_t {ATOM, LITERAL, LIST} token_type;
            std::string atom;
            std::string literal;
            std::string literal_size;
            std::list<std::shared_ptr<response_token_t>> parenthesized_list;
        };

        imap_client(Stream stream) : dlg_(std::move(stream))
        {
            reset_response_parser();
        }

        imap_client(dialog<Stream> dlg) : dlg_(std::move(dlg))
        {
            reset_response_parser();
        }

        boost::asio::awaitable<void> connect(const std::string& host, const std::string& service)
        {
            boost::asio::ip::tcp::resolver resolver(dlg_.stream().get_executor());
            auto endpoints = co_await resolver.async_resolve(host, service, boost::asio::use_awaitable);
            co_await boost::asio::async_connect(dlg_.stream().lowest_layer(), endpoints, boost::asio::use_awaitable);
        }

        boost::asio::awaitable<void> connect(const std::string& host, unsigned short port)
        {
            co_await connect(host, std::to_string(port));
        }

        boost::asio::awaitable<response_t> read_greeting()
        {
            response_line_t line = co_await read_response_line();
            if (line.fragments.empty())
                throw imap_error("Parser failure.");

            auto [status, text] = parse_untagged_status(line.fragments.front());
            if (status != response_status_t::OK && status != response_status_t::PREAUTH && status != response_status_t::BYE)
                throw imap_error("Invalid greeting.");

            response_t response;
            response.status = status;
            response.text = std::move(text);
            response.lines.push_back(std::move(line));
            co_return response;
        }

        boost::asio::awaitable<response_t> command(std::string command)
        {
            response_t response;
            response.tag = co_await send_command(command);
            while (true)
            {
                response_line_t line = co_await read_response_line();
                if (line.fragments.empty())
                    throw imap_error("Parser failure.");
                std::string head = line.fragments.front();
                bool tagged = is_tagged_response(head, response.tag);
                if (!line.literals.empty())
                {
                    response.literals.reserve(response.literals.size() + line.literals.size());
                    for (auto& literal : line.literals)
                        response.literals.push_back(std::move(literal));
                }
                if (tagged)
                {
                    auto [status, text] = parse_tagged_status(head, response.tag);
                    response.status = status;
                    response.text = std::move(text);
                    if (!is_tagged_status(status))
                        throw imap_error("Invalid response status.");
                    response.lines.push_back(std::move(line));
                    break;
                }
                response.lines.push_back(std::move(line));
            }
            co_return response;
        }

        boost::asio::awaitable<response_t> capability()
        {
            response_t response = co_await command("CAPABILITY");
            ensure_ok(response, "Capability");
            co_return response;
        }

        boost::asio::awaitable<response_t> login(std::string username, std::string password)
        {
            response_t response = co_await command("LOGIN " + to_astring(username) + " " + to_astring(password));
            ensure_ok(response, "Login");
            co_return response;
        }

        boost::asio::awaitable<response_t> logout()
        {
            response_t response = co_await command("LOGOUT");
            ensure_ok(response, "Logout");
            co_return response;
        }

        template<typename SSLContext>
        boost::asio::awaitable<imap_client<boost::asio::ssl::stream<Stream>>> start_tls(SSLContext& context)
        {
            response_t response = co_await command("STARTTLS");
            ensure_ok(response, "STARTTLS");

            Stream& socket = dlg_.stream();
            boost::asio::ssl::stream<Stream> ssl_stream(std::move(socket), context);
            co_await ssl_stream.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_awaitable);

            co_return imap_client<boost::asio::ssl::stream<Stream>>(std::move(ssl_stream));
        }

        template<typename CompletionToken>
        auto async_read_greeting(CompletionToken&& token)
        {
            return boost::asio::co_spawn(dlg_.stream().get_executor(),
                [this]() -> boost::asio::awaitable<void>
                {
                    co_await read_greeting();
                }, std::forward<CompletionToken>(token));
        }

        template<typename CompletionToken>
        auto async_authenticate(const std::string& username, const std::string& password, CompletionToken&& token)
        {
            return boost::asio::co_spawn(dlg_.stream().get_executor(),
                [this, username, password]() -> boost::asio::awaitable<void>
                {
                    co_await login(username, password);
                }, std::forward<CompletionToken>(token));
        }

        template<typename CompletionToken>
        auto async_logout(CompletionToken&& token)
        {
            return boost::asio::co_spawn(dlg_.stream().get_executor(),
                [this]() -> boost::asio::awaitable<void>
                {
                    co_await logout();
                }, std::forward<CompletionToken>(token));
        }

        template<typename SSLContext, typename CompletionToken>
        auto starttls(SSLContext& context, CompletionToken&& token)
        {
            return boost::asio::co_spawn(dlg_.stream().get_executor(),
                [this, &context]() -> boost::asio::awaitable<imap_client<boost::asio::ssl::stream<Stream>>>
                {
                    co_return co_await start_tls(context);
                }, std::forward<CompletionToken>(token));
        }

    protected:
        dialog<Stream> dlg_;
        
        enum class string_literal_state_t {NONE, SIZE, WAITING, READING, DONE};
        enum class atom_state_t {NONE, PLAIN, QUOTED};

        string_literal_state_t literal_state_;
        atom_state_t atom_state_;
        bool optional_part_state_;
        unsigned int parenthesis_list_counter_;
        std::list<std::shared_ptr<response_token_t>> mandatory_part_;
        std::list<std::shared_ptr<response_token_t>> optional_part_;
        unsigned int tag_{0};

        static const char OPTIONAL_BEGIN = '[';
        static const char OPTIONAL_END = ']';
        static const char LIST_BEGIN = '(';
        static const char LIST_END = ')';
        static const char STRING_LITERAL_BEGIN = '{';
        static const char STRING_LITERAL_END = '}';
        static const char TOKEN_SEPARATOR_CHAR = ' ';
        static const char QUOTED_ATOM = '"';

        static bool starts_with(std::string_view text, std::string_view prefix)
        {
            return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
        }

        static std::string_view ltrim(std::string_view text)
        {
            while (!text.empty() && text.front() == ' ')
                text.remove_prefix(1);
            return text;
        }

        static std::pair<std::string_view, std::string_view> split_token(std::string_view text)
        {
            text = ltrim(text);
            auto pos = text.find(' ');
            if (pos == std::string_view::npos)
                return {text, std::string_view{}};
            return {text.substr(0, pos), ltrim(text.substr(pos + 1))};
        }

        static bool iequals(std::string_view lhs, std::string_view rhs)
        {
            if (lhs.size() != rhs.size())
                return false;
            for (std::size_t i = 0; i < lhs.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i])))
                    return false;
            }
            return true;
        }

        static response_status_t parse_status_atom(std::string_view atom)
        {
            if (iequals(atom, "OK"))
                return response_status_t::OK;
            if (iequals(atom, "NO"))
                return response_status_t::NO;
            if (iequals(atom, "BAD"))
                return response_status_t::BAD;
            if (iequals(atom, "PREAUTH"))
                return response_status_t::PREAUTH;
            if (iequals(atom, "BYE"))
                return response_status_t::BYE;
            return response_status_t::UNKNOWN;
        }

        static bool is_tagged_status(response_status_t status)
        {
            return status == response_status_t::OK || status == response_status_t::NO || status == response_status_t::BAD;
        }

        static std::pair<response_status_t, std::string> parse_untagged_status(std::string_view line)
        {
            if (!starts_with(line, UNTAGGED_RESPONSE))
                throw imap_error("Invalid greeting.");

            std::string_view rest = line.substr(UNTAGGED_RESPONSE.size());
            auto [status_atom, text] = split_token(rest);
            response_status_t status = parse_status_atom(status_atom);
            if (status == response_status_t::UNKNOWN)
                throw imap_error("Invalid response status.");
            return {status, std::string(text)};
        }

        static std::pair<response_status_t, std::string> parse_tagged_status(std::string_view line, std::string_view tag)
        {
            if (!starts_with(line, tag))
                throw imap_error("Invalid response tag.");

            std::string_view rest = line.substr(tag.size());
            auto [status_atom, text] = split_token(rest);
            response_status_t status = parse_status_atom(status_atom);
            if (status == response_status_t::UNKNOWN)
                throw imap_error("Invalid response status.");
            return {status, std::string(text)};
        }

        static bool is_tagged_response(std::string_view line, std::string_view tag)
        {
            if (!starts_with(line, tag))
                return false;
            if (line.size() == tag.size())
                return true;
            return line[tag.size()] == ' ';
        }

        static void ensure_ok(const response_t& response, const std::string& context)
        {
            if (response.status == response_status_t::OK)
                return;
            std::string msg = context + " failure.";
            if (!response.text.empty())
                msg += " " + response.text;
            throw imap_error(msg);
        }

        void reset_response_parser()
        {
            optional_part_.clear();
            mandatory_part_.clear();
            optional_part_state_ = false;
            atom_state_ = atom_state_t::NONE;
            parenthesis_list_counter_ = 0;
            literal_state_ = string_literal_state_t::NONE;
        }

        std::list<std::shared_ptr<response_token_t>>* find_last_token_list(std::list<std::shared_ptr<response_token_t>>& token_list)
        {
            auto* list_ptr = &token_list;
            unsigned int depth = 1;
            while (!list_ptr->empty() && list_ptr->back()->token_type == response_token_t::token_type_t::LIST && depth <= parenthesis_list_counter_)
            {
                list_ptr = &(list_ptr->back()->parenthesized_list);
                depth++;
            }
            return list_ptr;
        }

        void parse_response(const std::string& response)
        {
            std::list<std::shared_ptr<response_token_t>>* token_list;

            std::shared_ptr<response_token_t> cur_token;
            for (auto ch : response)
            {
                switch (ch)
                {
                    case OPTIONAL_BEGIN:
                    {
                        if (atom_state_ == atom_state_t::QUOTED)
                            cur_token->atom +=ch;
                        else
                        {
                            if (optional_part_state_)
                                throw imap_error("Parser failure.");

                            optional_part_state_ = true;
                        }
                    }
                    break;

                    case OPTIONAL_END:
                    {
                        if (atom_state_ == atom_state_t::QUOTED)
                            cur_token->atom +=ch;
                        else
                        {
                            if (!optional_part_state_)
                                throw imap_error("Parser failure.");

                            optional_part_state_ = false;
                            atom_state_ = atom_state_t::NONE;
                        }
                    }
                    break;

                    case LIST_BEGIN:
                    {
                        if (atom_state_ == atom_state_t::QUOTED)
                            cur_token->atom +=ch;
                        else
                        {
                            cur_token = std::make_shared<response_token_t>();
                            cur_token->token_type = response_token_t::token_type_t::LIST;
                            token_list = optional_part_state_ ? find_last_token_list(optional_part_) : find_last_token_list(mandatory_part_);
                            token_list->push_back(cur_token);
                            parenthesis_list_counter_++;
                            atom_state_ = atom_state_t::NONE;
                        }
                    }
                    break;

                    case LIST_END:
                    {
                        if (atom_state_ == atom_state_t::QUOTED)
                            cur_token->atom +=ch;
                        else
                        {
                            if (parenthesis_list_counter_ == 0)
                                throw imap_error("Parser failure.");

                            parenthesis_list_counter_--;
                            atom_state_ = atom_state_t::NONE;
                        }
                    }
                    break;

                    case STRING_LITERAL_BEGIN:
                    {
                        if (atom_state_ == atom_state_t::QUOTED)
                            cur_token->atom +=ch;
                        else
                        {
                            if (literal_state_ == string_literal_state_t::SIZE)
                                throw imap_error("Parser failure.");

                            cur_token = std::make_shared<response_token_t>();
                            cur_token->token_type = response_token_t::token_type_t::LITERAL;
                            token_list = optional_part_state_ ? find_last_token_list(optional_part_) : find_last_token_list(mandatory_part_);
                            token_list->push_back(cur_token);
                            literal_state_ = string_literal_state_t::SIZE;
                            atom_state_ = atom_state_t::NONE;
                        }
                    }
                    break;

                    case STRING_LITERAL_END:
                    {
                        if (atom_state_ == atom_state_t::QUOTED)
                            cur_token->atom +=ch;
                        else
                        {
                            if (literal_state_ == string_literal_state_t::NONE)
                                throw imap_error("Parser failure.");

                            literal_state_ = string_literal_state_t::WAITING;
                        }
                    }
                    break;

                    case TOKEN_SEPARATOR_CHAR:
                    {
                        if (atom_state_ == atom_state_t::QUOTED)
                            cur_token->atom +=ch;
                        else
                        {
                            if (cur_token != nullptr)
                            {
                                boost::trim(cur_token->atom);
                                atom_state_ = atom_state_t::NONE;
                            }
                        }
                    }
                    break;

                    case QUOTED_ATOM:
                    {
                        if (atom_state_ == atom_state_t::NONE)
                        {
                            cur_token = std::make_shared<response_token_t>();
                            cur_token->token_type = response_token_t::token_type_t::ATOM;
                            token_list = optional_part_state_ ? find_last_token_list(optional_part_) : find_last_token_list(mandatory_part_);
                            token_list->push_back(cur_token);
                            atom_state_ = atom_state_t::QUOTED;
                        }
                        else if (atom_state_ == atom_state_t::QUOTED)
                        {
                            if (token_list->back()->atom.back() != codec::BACKSLASH_CHAR)
                                atom_state_ = atom_state_t::NONE;
                            else
                                token_list->back()->atom.back() = ch;
                        }
                    }
                    break;

                    default:
                    {
                        if (ch == codec::BACKSLASH_CHAR && atom_state_ == atom_state_t::QUOTED && token_list->back()->atom.back() == codec::BACKSLASH_CHAR)
                            break;

                        if (literal_state_ == string_literal_state_t::SIZE)
                        {
                            if (!isdigit(ch))
                                throw imap_error("Parser failure.");

                            cur_token->literal_size += ch;
                        }
                        else if (literal_state_ == string_literal_state_t::WAITING)
                        {
                            throw imap_error("Parser failure.");
                        }
                        else
                        {
                            if (atom_state_ == atom_state_t::NONE)
                            {
                                cur_token = std::make_shared<response_token_t>();
                                cur_token->token_type = response_token_t::token_type_t::ATOM;
                                token_list = optional_part_state_ ? find_last_token_list(optional_part_) : find_last_token_list(mandatory_part_);
                                token_list->push_back(cur_token);
                                atom_state_ = atom_state_t::PLAIN;
                            }
                            cur_token->atom += ch;
                        }
                    }
                }
            }
        }

        std::shared_ptr<response_token_t> pending_literal_token()
        {
            if (literal_state_ != string_literal_state_t::WAITING)
                throw imap_error("Parser failure.");

            auto* token_list = optional_part_state_ ? find_last_token_list(optional_part_) : find_last_token_list(mandatory_part_);
            if (token_list->empty() || token_list->back()->token_type != response_token_t::token_type_t::LITERAL)
                throw imap_error("Parser failure.");
            return token_list->back();
        }

        boost::asio::awaitable<response_line_t> read_response_line()
        {
            response_line_t response;
            reset_response_parser();
            std::string line = co_await dlg_.read_line(boost::asio::use_awaitable);
            response.fragments.push_back(line);
            parse_response(line);
            while (literal_state_ == string_literal_state_t::WAITING)
            {
                auto token = pending_literal_token();
                std::size_t literal_size = 0;
                try
                {
                    literal_size = std::stoul(token->literal_size);
                }
                catch (...)
                {
                    throw imap_error("Parser failure.");
                }
                std::string literal = co_await dlg_.read_exactly(literal_size, boost::asio::use_awaitable);
                response.literals.push_back(literal);
                token->literal = std::move(literal);
                literal_state_ = string_literal_state_t::NONE;
                std::string continuation = co_await dlg_.read_line(boost::asio::use_awaitable);
                response.fragments.push_back(continuation);
                parse_response(continuation);
            }
            co_return response;
        }

        boost::asio::awaitable<std::string> read_response()
        {
            response_line_t line = co_await read_response_line();
            if (line.fragments.empty())
                co_return std::string();
            co_return line.fragments.front();
        }

        boost::asio::awaitable<std::string> send_command(const std::string& command)
        {
            std::string tag = std::to_string(++tag_);
            std::string line = command.empty() ? tag : tag + " " + command;
            co_await dlg_.write_line(line, boost::asio::use_awaitable);
            co_return tag;
        }
    };

} // namespace mailio
