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
#else
#define TOTEM_IOS32
#endif

#else
#define TOTEM_MAC
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
#endif

#ifdef _M_IX86
#define TOTEM_X86
#endif

#ifdef _M_X64
#define TOTEM_X64
#endif

#endif // windows

// linux
#ifdef __linux__
#define TOTEM_LINUX
#define TOTEM_POSIX
#endif // linux

// visual studio compiler
#ifdef _MSC_VER
#define TOTEM_MSC
#define TOTEM_INLINE __forceinline
#define TOTEM_CDECL _cdecl
#define totem_snprintf(dst, dstlen, format, ...) _snprintf_s(dst, dstlen, _TRUNCATE, format, __VA_ARGS__)
#define PATH_MAX (_MAX_PATH)
#define totem_chdir _chdir
#endif

// clang
#if defined(__clang__)
#define TOTEM_CLANG
#define TOTEM_INLINE __attribute__((always_inline))
#define TOTEM_CDECL __attribute__((cdecl))
#define TOTEM_THREADED_DISPATCH
#define totem_snprintf snprintf
#define totem_chdir chdir
#endif

#if defined(__GNUC__)
#define TOTEM_GNUC
#define TOTEM_INLINE __attribute__((always_inline))
#define TOTEM_CDECL __attribute__((cdecl))
#define TOTEM_THREADED_DISPATCH
#define totem_snprintf snprintf
#define totem_chdir chdir
#endif

// winlib
#ifdef TOTEM_WIN

#ifdef TOTEM_MINGW
#define _WIN32_WINNT 0x0601 // target win7
#define PATH_MAX (260)
#define VOLUME_NAME_DOS (0x0)
#define FILE_NAME_NORMALIZED (0x0)
#endif

#include <Windows.h>
#include <direct.h>
#include <io.h>
#include <Shlwapi.h>

typedef int totemCwdSize_t;
#define getcwd _getcwd

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

#endif

// apple
#ifdef TOTEM_APPLE
#include <sys/syslimits.h>
#include <fcntl.h>
#include <sys/param.h>
#include <dirent.h>
#include <libkern/OSAtomic.h>

typedef size_t totemCwdSize_t;

#define totem_setjmp(jmp) setjmp((int*)jmp)
#define totem_longjmp(jmp) longjmp((int*)jmp, 1)
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

// compiliation options

// global values are cached in local scope
// globals are accessed less often, but synchronization is required when exiting scope
#define TOTEM_EVALOPT_GLOBAL_CACHE (1)

// vm options

// globals, functions & constants up to TOTEM_MAX_LOCAL_REGISTERS don't need moving to local scope to be accessible
// all register accesses become slightly more expensive
// halves maximum number of local registers, but uses far less of them
// lowers total number of instructions
#define TOTEM_VMOPT_GLOBAL_OPERANDS (1)

// forces bytecode interpreter to use computed gotos instead of a switch statement
// theoretically better performance (better branch prediction), but not supported on every platform
#define TOTEM_VMOPT_THREADED_DISPATCH (defined(TOTEM_THREADED_DISPATCH))

#endif
