#pragma once

#include <mailio/net/dialog.hpp>

namespace mailio
{
namespace smtp
{

class error : public mailio::net::dialog_error
{
public:
    using mailio::net::dialog_error::dialog_error;
};

} // namespace smtp
} // namespace mailio
