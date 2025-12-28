#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>

namespace mailio
{
namespace smtp
{

struct reply
{
    int status = 0;
    std::vector<std::string> lines;

    bool is_positive_completion() const { return status / 100 == 2; }
    bool is_positive_intermediate() const { return status / 100 == 3; }
    bool is_transient_negative() const { return status / 100 == 4; }
    bool is_permanent_negative() const { return status / 100 == 5; }

    std::string message() const
    {
        if (lines.empty())
            return std::string();

        std::string out = lines.front();
        for (std::size_t i = 1; i < lines.size(); ++i)
        {
            out += "\n";
            out += lines[i];
        }
        return out;
    }
};

struct capabilities
{
    std::map<std::string, std::vector<std::string>> entries;

    bool empty() const { return entries.empty(); }

    bool supports(std::string_view capability) const
    {
        const std::string key = normalize_key(capability);
        return entries.find(key) != entries.end();
    }

    const std::vector<std::string>* parameters(std::string_view capability) const
    {
        const std::string key = normalize_key(capability);
        auto it = entries.find(key);
        return it == entries.end() ? nullptr : &it->second;
    }

private:
    static std::string normalize_key(std::string_view key)
    {
        std::string out;
        out.reserve(key.size());
        for (char ch : key)
        {
            if (ch >= 'a' && ch <= 'z')
                out.push_back(static_cast<char>(ch - ('a' - 'A')));
            else
                out.push_back(ch);
        }
        return out;
    }
};

struct envelope
{
    std::string mail_from;
    std::vector<std::string> rcpt_to;
};

enum class auth_method
{
    plain,
    login
};

} // namespace smtp
} // namespace mailio
