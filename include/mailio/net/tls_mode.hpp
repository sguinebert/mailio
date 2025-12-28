#pragma once

namespace mailio::net
{

/**
TLS mode for protocol connections.
**/
enum class tls_mode
{
    none,
    starttls,
    implicit
};

} // namespace mailio::net
