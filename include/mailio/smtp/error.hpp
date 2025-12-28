#pragma once

#include <mailio/net/dialog.hpp>

namespace mailio
{
namespace smtp
{

class error : public mailio::dialog_error
{
public:
    using mailio::dialog_error::dialog_error;
};

} // namespace smtp
} // namespace mailio
