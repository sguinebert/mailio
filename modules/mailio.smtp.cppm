/*

mailio.smtp.cppm
----------------

C++20 module interface for mailio SMTP client.

Copyright (C) 2025, Sylvain Guinebert (github.com/sguinebert).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/

module;

// Global module fragment - non-modular dependencies
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/algorithm/string.hpp>

export module mailio.smtp;

// Import dependencies
export import mailio.net;
export import mailio.mime;

// Export SMTP headers
export {
    #include <mailio/smtp/types.hpp>
    #include <mailio/smtp/error.hpp>
    #include <mailio/smtp/client.hpp>
}
