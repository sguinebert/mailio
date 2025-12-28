#pragma once

// Use only when the toolchain supports C++20 modules and the mailio module interface is built.
#if defined(MAILIO_USE_MODULES)
import mailio;
#else
#include <mailio/mailio.hpp>
#endif
