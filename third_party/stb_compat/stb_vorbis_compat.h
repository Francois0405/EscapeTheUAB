#pragma once

#if defined(__has_include)
#if __has_include(<stb_vorbis.c>)
#include <stb_vorbis.c>
#elif __has_include(<stb_vorbis.h>)
#include <stb_vorbis.h>
#else
#error "Neither stb_vorbis.c nor stb_vorbis.h is available in the include path."
#endif
#else
#error "The compiler must support __has_include to locate stb_vorbis."
#endif
