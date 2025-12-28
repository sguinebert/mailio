# CMakeLists.txt for mailio C++20 modules
#
# This is EXPERIMENTAL and requires:
# - CMake 3.28+
# - A compiler with C++20 modules support (MSVC 2022, GCC 14+, Clang 16+)
# - Ninja generator recommended
#
# Usage:
#   cmake -G Ninja -B build -DMAILIO_BUILD_MODULES=ON
#   cmake --build build

cmake_minimum_required(VERSION 3.28)

# Check if modules are enabled
if(NOT MAILIO_BUILD_MODULES)
    return()
endif()

message(STATUS "Building mailio C++20 modules (EXPERIMENTAL)")

# Module source files
set(MAILIO_MODULE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/mailio.codec.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/mailio.net.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/mailio.mime.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/mailio.smtp.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/mailio.pop3.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/mailio.imap.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/mailio.cppm
)

# Create module library
add_library(mailio_modules)

target_sources(mailio_modules
    PUBLIC
        FILE_SET CXX_MODULES FILES
            ${MAILIO_MODULE_SOURCES}
)

target_compile_features(mailio_modules PUBLIC cxx_std_20)

target_include_directories(mailio_modules
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
)

target_link_libraries(mailio_modules
    PUBLIC
        ${Boost_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        Threads::Threads
)

# Alias for consistent naming
add_library(mailio::modules ALIAS mailio_modules)
