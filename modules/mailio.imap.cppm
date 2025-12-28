/*

mailio.imap.cppm
----------------

C++20 module interface for mailio IMAP client.

Copyright (C) 2025, Sylvain Guinebert (github.com/sguinebert).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/

module;

// Global module fragment - non-modular dependencies
#include <string>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <tuple>
#include <variant>
#include <optional>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

export module mailio.imap;

// Import dependencies
export import mailio.net;
export import mailio.mime;

// Export IMAP headers
export {
    #include <mailio/imap/types.hpp>
    #include <mailio/imap/error.hpp>
    #include <mailio/imap/client.hpp>
}
