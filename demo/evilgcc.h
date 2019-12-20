#pragma once

#ifndef __GNUC__
#    error This header only works in GCC/Clang
#endif

// https://gcc.gnu.org/onlinedocs/gcc/Local-Labels.html
#define local_label __label__

// https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html
#define scoped(f) __attribute__((cleanup(f)))

#define defer(name, ...)                                                                                               \
    void defer_##name() { __VA_ARGS__; };                                                                              \
    struct {                                                                                                           \
    } __defer_##name scoped(defer_##name)

#define defer_(thing, fn) defer(thing, fn(thing))
#define defer_ref(thing, fn) defer(thing, fn(&thing))

#define lam(ret, ...)                                                                                                  \
    ({                                                                                                                 \
        ret __fn__ __VA_ARGS__;                                                                                        \
        __fn__;                                                                                                        \
    })

#define lam0(...) lam(void, __VA_ARGS__)
#define vproxy(func, ...) lam0((void const*__ptr__) { func(__ptr__, ##__VA_ARGS__); })

// https://gcc.gnu.org/onlinedocs/gcc/Alignment.html
#define alignof __alignof__

#ifndef __cplusplus
#    ifndef static_assert
#        define static_assert _Static_assert
#    endif
#    ifndef thread_local
#        define thread_local __thread
#    endif
#    ifndef auto
#        define auto __auto_type
#    endif
#endif

#define DO_PRAGMA(x) _Pragma(#x)
#define TODO(x) DO_PRAGMA(message("TODO - " #x))