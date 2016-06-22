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
#ifdef _WIN32
#define TOTEM_WIN

#ifdef _WIN64
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
#define TOTEM_INLINE __forceinline
#define TOTEM_CDECL _cdecl
#endif

// clang
#ifdef __clang__
#define TOTEM_INLINE __attribute__((always_inline))
#define TOTEM_CDECL __attribute__((cdecl))
#endif

// winlib
#ifdef TOTEM_WIN
#include <Windows.h>
#include <direct.h>
#include <io.h>
#include <Shlwapi.h>

#define getcwd _getcwd
#define PATH_MAX (_MAX_PATH)

#define totemLock CRITICAL_SECTION
#define totemLock_Init InitializeCriticalSection
#define totemLock_Cleanup DeleteCriticalSection
#define totemLock_Acquire EnterCriticalSection
#define totemLock_Release LeaveCriticalSection

#define totem_snprintf(dst, dstlen, format, ...) _snprintf_s(dst, dstlen, _TRUNCATE, format, __VA_ARGS__)
#endif

// apple
#ifdef TOTEM_APPLE
#include <sys/syslimits.h>
#include <fcntl.h>
#include <sys/param.h>
#include <dirent.h>
#include <libkern/OSAtomic.h>

#define totem_snprintf snprintf
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

#endif
