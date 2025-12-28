/*

mailio.codec.cppm
-----------------

C++20 module interface for mailio codec components.

Copyright (C) 2025, Sylvain Guinebert (github.com/sguinebert).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/

module;

// Global module fragment - non-modular dependencies
#include <string>
#include <vector>
#include <stdexcept>
#include <tuple>
#include <algorithm>
#include <boost/algorithm/string.hpp>

export module mailio.codec;

// Export all codec headers
export {
    #include <mailio/codec/codec.hpp>
    #include <mailio/codec/base64.hpp>
    #include <mailio/codec/binary.hpp>
    #include <mailio/codec/bit7.hpp>
    #include <mailio/codec/bit8.hpp>
    #include <mailio/codec/percent.hpp>
    #include <mailio/codec/quoted_printable.hpp>
    #include <mailio/codec/q_codec.hpp>
}
