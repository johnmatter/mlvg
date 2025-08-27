// mlvg: MLVG platform header
// Copyright (c) 2024 MLVG

#ifndef _MLVG_PLATFORM_H
#define _MLVG_PLATFORM_H

// Windows threading setup - must be included before <thread> to ensure
// _beginthreadex and other threading functions are available to std::thread
#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7 minimum
#endif
#ifndef WINVER
#define WINVER 0x0601        // Windows version
#endif
#ifndef _MT
#define _MT                   // Multithreaded CRT
#endif
#include <process.h>
#include <windows.h>
#endif

#endif  // _MLVG_PLATFORM_H
