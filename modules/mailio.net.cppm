/*

mailio.net.cppm
---------------

C++20 module interface for mailio networking components.

Copyright (C) 2025, Sylvain Guinebert (github.com/sguinebert).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/

module;

// Global module fragment - non-modular dependencies
#include <string>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

export module mailio.net;

// Export networking headers
export {
    #include <mailio/net/upgradable_stream.hpp>
    #include <mailio/net/dialog.hpp>
}
