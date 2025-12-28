/*

mailio.mime.cppm
----------------

C++20 module interface for mailio MIME components.

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
#include <utility>
#include <tuple>
#include <optional>
#include <chrono>
#include <istream>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

export module mailio.mime;

// Import codec module dependency
export import mailio.codec;

// Export MIME headers
export {
    #include <mailio/mime/mailboxes.hpp>
    #include <mailio/mime/mime.hpp>
    #include <mailio/mime/message.hpp>
}
