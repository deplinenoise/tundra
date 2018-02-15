#ifndef CONFIG_HPP
#define CONFIG_HPP

// Preprocessor helpers to check for definedness giving compile errors if used
// against features with missing #defines.
#define YES      -1
#define NO       -2
#define ENABLED(feature)  (1 == 2 feature)
#define DISABLED(feature)  (0 == 2 feature)

// Set up build features

#define USE_SHA1_HASH NO
#define USE_FAST_HASH YES

#if defined(_DEBUG)
#define CHECKED_BUILD YES
#else
#define CHECKED_BUILD NO
#endif

// Platform macros.
#if defined(__GNUC__)
#define RESTRICT __restrict
#define NORETURN __attribute__((noreturn))
#define ALIGNOF(t) __alignof(t)
#define ALIGN(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define RESTRICT __restrict
#define NORETURN __declspec(noreturn)
#define ALIGNOF(t) __alignof(t)
#define ALIGN(n) __declspec(align(n))
#else
#error unsupported compiler
#endif

#if defined(__powerpc__)
#define USE_LITTLE_ENDIAN NO
#elif defined(_WIN32) || defined(__x86__) || defined(__x86_64__) || defined(i386) || defined(__i386__)
#define USE_LITTLE_ENDIAN YES
#else
#error add endian detection here
#endif

#if defined(__APPLE__)
#define TUNDRA_UNIX 1
#define TUNDRA_APPLE 1
#define TUNDRA_CASE_INSENSITIVE_FILESYSTEM YES
#define TUNDRA_EXE_SUFFIX ""
#elif defined(_WIN32)
#define TUNDRA_WIN32 1
#define TUNDRA_CASE_INSENSITIVE_FILESYSTEM YES
#define TUNDRA_EXE_SUFFIX ".exe"
#if defined(__GNUC__)
#  define TUNDRA_WIN32_MINGW 1
#endif
#if _WIN32_WINNT >= 0x600
#define TUNDRA_WIN32_VISTA_APIS YES
#else
#define TUNDRA_WIN32_VISTA_APIS NO
#endif
#elif defined(__linux__)
#define TUNDRA_UNIX 1
#define TUNDRA_LINUX 1
#define TUNDRA_CASE_INSENSITIVE_FILESYSTEM NO
#define TUNDRA_EXE_SUFFIX ""
#elif defined(__FreeBSD__)
#define TUNDRA_UNIX 1
#define TUNDRA_FREEBSD 1
#define TUNDRA_CASE_INSENSITIVE_FILESYSTEM NO
#define TUNDRA_EXE_SUFFIX ""
#elif defined(__NetBSD__)
#define TUNDRA_UNIX 1
#define TUNDRA_NETBSD 1
#define TUNDRA_CASE_INSENSITIVE_FILESYSTEM NO
#define TUNDRA_EXE_SUFFIX ""
#elif defined(__OpenBSD__)
#define TUNDRA_UNIX 1
#define TUNDRA_OPENBSD 1
#define TUNDRA_CASE_INSENSITIVE_FILESYSTEM NO
#define TUNDRA_EXE_SUFFIX ""
#else
#error Unsupported OS
#endif

#if defined(TUNDRA_WIN32)
#define TUNDRA_STDCALL __stdcall
#else
#define TUNDRA_STDCALL
#endif

#if defined(TUNDRA_APPLE)
#define TUNDRA_PLATFORM_STRING "macosx"
#elif defined(TUNDRA_LINUX)
#define TUNDRA_PLATFORM_STRING "linux"
#elif defined(TUNDRA_WIN32)
#define TUNDRA_PLATFORM_STRING "windows"
#elif defined(TUNDRA_FREEBSD)
#define TUNDRA_PLATFORM_STRING "freebsd"
#elif defined(TUNDRA_OPENBSD)
#define TUNDRA_PLATFORM_STRING "openbsd"
#else
#error Unsupported OS
#endif

#if defined(TUNDRA_WIN32)
#define TD_PATHSEP     '\\'
#define TD_PATHSEP_STR "\\"
#else
#define TD_PATHSEP     '/'
#define TD_PATHSEP_STR "/"
#endif

#if defined(_DEBUG) && defined(TUNDRA_LINUX)
#define USE_VALGRIND YES
#else
#define USE_VALGRIND NO
#endif

#endif
