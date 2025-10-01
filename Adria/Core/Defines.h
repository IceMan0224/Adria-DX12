#pragma once
#include <cassert>

#define _ADRIA_STRINGIFY_IMPL(a) #a
#define _ADRIA_CONCAT_IMPL(x, y) x##y

#define ADRIA_STRINGIFY(a) _ADRIA_STRINGIFY_IMPL(a)
#define ADRIA_CONCAT(x, y) _ADRIA_CONCAT_IMPL(x, y)

#define ADRIA_TODO(...)
#define ADRIA_HACK(stmt, msg) stmt

#define ADRIA_ASSERT(expr)            assert(expr)
#define ADRIA_ASSERT_MSG(expr, msg)   assert((expr) && (msg))

#if defined(_MSC_VER) 
#define ADRIA_DEBUGBREAK()       __debugbreak()
#define ADRIA_UNREACHABLE()      __assume(false)
#define ADRIA_FORCEINLINE        __forceinline
#define ADRIA_NOINLINE           __declspec(noinline)
#define ADRIA_DEBUGZONE_BEGIN    __pragma(optimize("", off))
#define ADRIA_DEBUGZONE_END      __pragma(optimize("", on))

#elif defined(__GNUC__) || defined(__clang__) 
#define ADRIA_DEBUGBREAK()       __builtin_trap()
#define ADRIA_UNREACHABLE()      __builtin_unreachable()
#define ADRIA_FORCEINLINE        inline __attribute__((always_inline))
#define ADRIA_NOINLINE           __attribute__((noinline))
#define ADRIA_DEBUGZONE_BEGIN    _Pragma("GCC push_options") \
                                 _Pragma("GCC optimize(\"O0\")")
#define ADRIA_DEBUGZONE_END      _Pragma("GCC pop_options")

#else 
#define ADRIA_DEBUGBREAK()       ((void)0)
#define ADRIA_UNREACHABLE()      ((void)0)
#define ADRIA_FORCEINLINE        inline
#define ADRIA_NOINLINE
#define ADRIA_DEBUGZONE_BEGIN
#define ADRIA_DEBUGZONE_END
#endif

#define ADRIA_NODISCARD              [[nodiscard]]
#define ADRIA_NORETURN               [[noreturn]]
#define ADRIA_DEPRECATED             [[deprecated]]
#define ADRIA_MAYBE_UNUSED           [[maybe_unused]]
#define ADRIA_DEPRECATED_MSG(msg)    [[deprecated(msg)]]


#define ADRIA_NONCOPYABLE(Class)                 \
        Class(Class const&)            = delete; \
        Class& operator=(Class const&) = delete;

#define ADRIA_NONMOVABLE(Class)                      \
        Class(Class&&) noexcept            = delete; \
        Class& operator=(Class&&) noexcept = delete;

#define ADRIA_NONCOPYABLE_NONMOVABLE(Class) \
        ADRIA_NONCOPYABLE(Class)            \
        ADRIA_NONMOVABLE(Class)

#define ADRIA_DEFAULT_COPYABLE(Class)             \
        Class(Class const&)            = default; \
        Class& operator=(Class const&) = default;

#define ADRIA_DEFAULT_MOVABLE(Class)                  \
        Class(Class&&) noexcept            = default; \
        Class& operator=(Class&&) noexcept = default;

#define ADRIA_DEFAULT_COPYABLE_MOVABLE(Class) \
        ADRIA_DEFAULT_COPYABLE(Class)         \
        ADRIA_DEFAULT_MOVABLE(Class)


#if defined(_WIN32) || defined(_WIN64)
    #define ADRIA_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define ADRIA_PLATFORM_LINUX 1
#elif defined(__APPLE__)
    #define ADRIA_PLATFORM_MACOS 1
#endif

#ifndef ADRIA_PLATFORM_WINDOWS
    #define ADRIA_PLATFORM_WINDOWS 0
#endif

#ifndef ADRIA_PLATFORM_LINUX
    #define ADRIA_PLATFORM_LINUX 0
#endif

#ifndef ADRIA_PLATFORM_MACOS
    #define ADRIA_PLATFORM_MACOS 0
#endif