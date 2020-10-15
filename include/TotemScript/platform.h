//
//  platform.h
//  TotemScript
//
//  Created by Timothy Smale on 27/06/2016
//  Copyright (c) 2015 Timothy Smale. All rights reserved.
//

#ifndef TotemScript_platform_h
#define TotemScript_platform_h

// mac / ios
#if defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#define TOTEM_APPLE
#define TOTEM_POSIX

#if TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1
#define TOTEM_IOS

#if !defined(TARGET_IPHONE_SIMULATOR) || TARGET_IPHONE_SIMULATOR != 1
#define TOTEM_IOSSIM
#endif

#if __LP64__
#define TOTEM_IOS64
#define TOTEM_64
#define TOTEM_JIT_ARM64
#else
#define TOTEM_IOS32
#define TOTEM_JIT_ARM32
#endif

#else
#define TOTEM_MAC

#if __LP64__
#define TOTEM_JIT_X64_SYSV
#else
#define TOTEM_JIT_X86_CDECL
#endif

#endif

#endif // mac / ios

// windows
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
#define TOTEM_WIN

#if defined(__MINGW32__) || defined(__MINGW64__)
#define TOTEM_MINGW
#endif

#if defined(_WIN64) || defined(__MINGW64__)
#define TOTEM_WIN64
#define TOTEM_64

#else
#define TOTEM_WIN32
#define TOTEM_JIT_X86_CDECL
#endif

#ifdef _M_IX86
#define TOTEM_X86
#endif

#ifdef _M_X64
#define TOTEM_X64
#define TOTEM_JIT_X64_WIN
//#define TOTEM_JIT
#endif

#endif // windows

// linux
#ifdef __linux__
#define TOTEM_LINUX
#define TOTEM_POSIX
#endif // linux

// visual studio
#ifdef _MSC_VER
#define TOTEM_MSC
#define TOTEM_INLINE __inline
#define TOTEM_FORCE_INLINE __forceinline
#define TOTEM_CDECL _cdecl
#define TOTEM_UNREACHABLE() __assume(0)
#define totem_snprintf(dst, dstlen, format, ...) _snprintf_s(dst, dstlen, _TRUNCATE, format, __VA_ARGS__)
#define TOTEM_SUPPRESS_NO_RETURN_WARNING
#define TOTEM_SUPPRESS_LIFT
#endif

// clang
#if defined(__clang__) || defined(__GNUC__)
#define TOTEM_CLANG
#define TOTEM_INLINE inline
#define TOTEM_FORCE_INLINE __attribute__((always_inline)) inline
#define TOTEM_CDECL __attribute__((cdecl))
#define TOTEM_THREADED_DISPATCH
#define TOTEM_UNREACHABLE() __builtin_unreachable()
#define totem_snprintf snprintf
#define TOTEM_SUPPRESS_NO_RETURN_WARNING \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wreturn-type\"")
#define TOTEM_SUPPRESS_LIFT \
    _Pragma("GCC diagnostic pop")

#endif

// winlib
#ifdef TOTEM_WIN

#include <Windows.h>
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <Shlwapi.h>

#ifdef TOTEM_MINGW

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 // target win7
#endif

#ifndef VOLUME_NAME_DOS
#define VOLUME_NAME_DOS (0x0)
#endif

#ifndef FILE_NAME_NORMALIZED
#define FILE_NAME_NORMALIZED (0x0)
#endif

#endif

typedef int totemCwdSize_t;

#define totemLock CRITICAL_SECTION
#define totemLock_Init InitializeCriticalSection
#define totemLock_Cleanup DeleteCriticalSection
#define totemLock_Acquire EnterCriticalSection
#define totemLock_Release LeaveCriticalSection

#ifdef TOTEM_64
#define totem_setjmp(jmp) setjmp(jmp)
#define totem_longjmp(jmp) longjmp(jmp, 1)
#else
#define totem_setjmp(jmp) setjmp((int*)jmp)
#define totem_longjmp(jmp) longjmp((int*)jmp, 1)
#endif

#ifndef PATH_MAX
#define PATH_MAX (_MAX_PATH)
#endif

#define totem_chdir _chdir
#define getcwd _getcwd
#define PRISize "Iu"

#endif

// apple
#ifdef TOTEM_APPLE
#include <sys/syslimits.h>
#include <fcntl.h>
#include <sys/param.h>
#include <dirent.h>
#include <libkern/OSAtomic.h>
#include <stdio.h>

typedef size_t totemCwdSize_t;

#define totem_setjmp(jmp) setjmp((int*)jmp)
#define totem_longjmp(jmp) longjmp((int*)jmp, 1)

#define totem_snprintf snprintf
#define totem_chdir chdir
#define PRISize "zu"

#endif

#ifdef TOTEM_LINUX
#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <linux/limits.h>
#include <unistd.h>

typedef size_t totemCwdSize_t;

#define totem_setjmp(jmp) setjmp(jmp)
#define totem_longjmp(jmp) longjmp(jmp, 1)

#define totem_snprintf snprintf
#define totem_chdir chdir

#endif

// posix
#ifdef TOTEM_POSIX
#include <pthread.h>
#include <unistd.h>

#define totemLock pthread_mutex_t
#define totemLock_Init(x) pthread_mutex_init(x, NULL)
#define totemLock_Cleanup pthread_mutex_destroy
#define totemLock_Acquire pthread_mutex_lock
#define totemLock_Release pthread_mutex_unlock
#endif

// gc options

// ref-counting frees memory quickly and deterministically, but adds more program overhead
#define TOTEM_GCTYPE_REFCOUNTING (0)

// mark-and-sweep adds less overall program overhead, but makes it more difficult to predict when memory is actually freed
#define TOTEM_GCTYPE_MARKANDSWEEP (1)

#define TOTEM_GCTYPE (TOTEM_GCTYPE_MARKANDSWEEP)
#define TOTEM_GCTYPE_ISREFCOUNTING (TOTEM_GCTYPE == TOTEM_GCTYPE_REFCOUNTING)
#define TOTEM_GCTYPE_ISMARKANDSWEEP (TOTEM_GCTYPE == TOTEM_GCTYPE_MARKANDSWEEP)

// vm options

// bytecode interpreter uses computed gotos for instruction dispatch instead of a switch statement
// theoretically better performance (better branch prediction), but not supported on every platform
#ifdef TOTEM_THREADED_DISPATCH
#define TOTEM_VMOPT_THREADED_DISPATCH (1)
#else
#define TOTEM_VMOPT_THREADED_DISPATCH (0)
#endif

// all possible values are encoded as a single 8-byte IEEE-754 double
// halves the size of registers
// more work is needed to encode/decode values
// integers can only be 32-bits in width
// mini-strings are smaller
#ifdef TOTEM_X64
#define TOTEM_VMOPT_NANBOXING (1)
#else
#define TOTEM_VMOPT_NANBOXING (0)
#endif

// debug options

#define TOTEM_DEBUGOPT_ASSERT_REGISTER_VALUES (0)
#define TOTEM_DEBUGOPT_ASSERT_GC (0)
#define TOTEM_DEBUGOPT_PRINT_VM_ACTIVITY (0)
#define TOTEM_DEBUGOPT_ASSERT_HASHMAP_LISTS (0)

#ifdef TOTEM_DEBUG
#define TOTEM_DEBUG_INLINE TOTEM_INLINE
#else
#define TOTEM_DEBUG_INLINE TOTEM_FORCE_INLINE
#endif

#ifndef TOTEM_64
#define TOTEM_32
#endif

#endif
