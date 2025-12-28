/*

mailio.pop3.cppm
----------------

C++20 module interface for mailio POP3 client.

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
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

export module mailio.pop3;

// Import dependencies
export import mailio.net;
export import mailio.mime;

// Export POP3 headers
export {
    #include <mailio/pop3/types.hpp>
    #include <mailio/pop3/error.hpp>
    #include <mailio/pop3/client.hpp>
}
