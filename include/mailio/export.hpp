#pragma once

#if defined(MAILIO_STATIC_DEFINE)
#  ifndef MAILIO_EXPORT
#    define MAILIO_EXPORT
#  endif
#  ifndef MAILIO_NO_EXPORT
#    define MAILIO_NO_EXPORT
#  endif
#else
#  ifndef MAILIO_EXPORT
#    if defined(_WIN32) || defined(__CYGWIN__)
#      ifdef MAILIO_EXPORTS
#        define MAILIO_EXPORT __declspec(dllexport)
#      else
#        define MAILIO_EXPORT __declspec(dllimport)
#      endif
#      define MAILIO_NO_EXPORT
#    else
#      if defined(__GNUC__) && __GNUC__ >= 4
#        define MAILIO_EXPORT __attribute__((visibility("default")))
#        define MAILIO_NO_EXPORT __attribute__((visibility("hidden")))
#      else
#        define MAILIO_EXPORT
#        define MAILIO_NO_EXPORT
#      endif
#    endif
#  endif
#endif
