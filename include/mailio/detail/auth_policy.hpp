/*

auth_policy.hpp
---------------

Copyright (C) 2025, Sylvain Guinebert (github.com/sguinebert).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/

#pragma once

#include <mailio/detail/log.hpp>
#include <mailio/net/dialog.hpp>

namespace mailio::detail
{

template <typename Error = mailio::net::dialog_error, typename Options>
inline void ensure_auth_allowed(bool is_tls, const Options& options)
{
    if (is_tls || !options.require_tls_for_auth)
        return;
    if (options.allow_cleartext_auth)
    {
        MAILIO_WARN("AUTH without TLS allowed by configuration.");
        return;
    }
    throw Error("TLS required for authentication; call start_tls() or use tls_mode::implicit", "");
}

} // namespace mailio::detail
