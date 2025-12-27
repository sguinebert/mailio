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
#include <vector>
#include <list>
#include <map>
#include <utility>
#include <optional>
#include <variant>
#include <sstream>
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
#include <mailio/dialog.hpp>
#include <mailio/message.hpp>
#include <mailio/codec.hpp>
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

        template<typename CompletionToken>
        auto async_read_greeting(CompletionToken&& token)
        {
            return boost::asio::co_spawn(dlg_.stream().get_executor(),
                [this]() -> boost::asio::awaitable<void>
                {
                    std::string line = co_await dlg_.async_receive(false, boost::asio::use_awaitable);
                    if (line.find("* OK") == std::string::npos)
                        throw imap_error("Invalid greeting.");
                    parse_response(line);
                }, std::forward<CompletionToken>(token));
        }

        template<typename CompletionToken>
        auto async_authenticate(const std::string& username, const std::string& password, CompletionToken&& token)
        {
            return boost::asio::co_spawn(dlg_.stream().get_executor(),
                [this, username, password]() -> boost::asio::awaitable<void>
                {
                    std::string command = "LOGIN " + to_astring(username) + " " + to_astring(password);
                    co_await send_command(command);
                    std::string response = co_await dlg_.async_receive(false, boost::asio::use_awaitable);
                    parse_response(response);
                }, std::forward<CompletionToken>(token));
        }

        template<typename CompletionToken>
        auto async_logout(CompletionToken&& token)
        {
            return boost::asio::co_spawn(dlg_.stream().get_executor(),
                [this]() -> boost::asio::awaitable<void>
                {
                    co_await send_command("LOGOUT");
                    std::string response = co_await dlg_.async_receive(false, boost::asio::use_awaitable);
                    parse_response(response);
                }, std::forward<CompletionToken>(token));
        }

        template<typename SSLContext, typename CompletionToken>
        auto starttls(SSLContext& context, CompletionToken&& token)
        {
            return boost::asio::co_spawn(dlg_.stream().get_executor(),
                [this, &context]() -> boost::asio::awaitable<imap_client<boost::asio::ssl::stream<Stream>>>
                {
                    co_await send_command("STARTTLS");
                    std::string response = co_await dlg_.async_receive(false, boost::asio::use_awaitable);
                    parse_response(response);
                    
                    Stream& socket = dlg_.stream();
                    boost::asio::ssl::stream<Stream> ssl_stream(std::move(socket), context);
                    co_await ssl_stream.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_awaitable);
                    
                    co_return imap_client<boost::asio::ssl::stream<Stream>>(std::move(ssl_stream));
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
        std::string::size_type literal_bytes_read_;
        std::string::size_type eols_no_;
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

        void reset_response_parser()
        {
            optional_part_.clear();
            mandatory_part_.clear();
            optional_part_state_ = false;
            atom_state_ = atom_state_t::NONE;
            parenthesis_list_counter_ = 0;
            literal_state_ = string_literal_state_t::NONE;
            literal_bytes_read_ = 0;
            eols_no_ = 2;
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

            if (literal_state_ == string_literal_state_t::READING)
            {
                token_list = optional_part_state_ ? find_last_token_list(optional_part_) : find_last_token_list(mandatory_part_);
                if (token_list->back()->token_type == response_token_t::token_type_t::LITERAL && literal_bytes_read_ > token_list->back()->literal.size())
                    throw imap_error("Parser failure.");
                unsigned long literal_size = std::stoul(token_list->back()->literal_size);
                if (literal_bytes_read_ + response.size() < literal_size)
                {
                    token_list->back()->literal += response + codec::END_OF_LINE;
                    literal_bytes_read_ += response.size() + eols_no_;
                    if (literal_bytes_read_ == literal_size)
                        literal_state_ = string_literal_state_t::DONE;
                    return;
                }
                else
                {
                    std::string::size_type resp_len = response.size();
                    token_list->back()->literal += response.substr(0, literal_size - literal_bytes_read_);
                    literal_bytes_read_ += literal_size - literal_bytes_read_;
                    literal_state_ = string_literal_state_t::DONE;
                    parse_response(response.substr(resp_len - (literal_size - literal_bytes_read_) - 1));
                    return;
                }
            }

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

            if (literal_state_ == string_literal_state_t::WAITING)
                literal_state_ = string_literal_state_t::READING;
        }

        boost::asio::awaitable<void> send_command(const std::string& command)
        {
            std::string line = std::to_string(++tag_) + " " + command + "\r\n";
            co_await dlg_.async_send(line, boost::asio::use_awaitable);
        }
    };

} // namespace mailio
