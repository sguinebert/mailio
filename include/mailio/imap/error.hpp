#pragma once

#include <mailio/net/dialog.hpp>

namespace mailio::imap
{

class error : public mailio::net::dialog_error
{
public:
    using mailio::net::dialog_error::dialog_error;
};

} // namespace mailio::imap
