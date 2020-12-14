/* -*- mode: c++; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2006-2010 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Taken from WebKit: JavaScriptCore/wtf/Platform.h. */

#ifndef PLATFORM_STD_PLATFORM_H
#define PLATFORM_STD_PLATFORM_H	
	
/*!
 * @header Platform and configuration macros.
 */

/* PLATFORM handles OS, operating environment, graphics API, and CPU */
#define PLATFORM(WTF_FEATURE) \
    (defined( WTF_PLATFORM_##WTF_FEATURE ) && WTF_PLATFORM_##WTF_FEATURE)

#define COMPILER(WTF_FEATURE) \
    (defined( WTF_COMPILER_##WTF_FEATURE ) && WTF_COMPILER_##WTF_FEATURE)

#define HAVE(WTF_FEATURE) \
    (defined( HAVE_##WTF_FEATURE ) && HAVE_##WTF_FEATURE)

#define USE(WTF_FEATURE) \
    (defined( WTF_USE_##WTF_FEATURE ) && WTF_USE_##WTF_FEATURE)

#define ENABLE(WTF_FEATURE) \
    (defined( ENABLE_##WTF_FEATURE ) && ENABLE_##WTF_FEATURE)

/* Operating systems - low-level dependencies */

/* PLATFORM(DARWIN) */
/* Operating system level dependencies for Mac OS X / Darwin that should */
/* be used regardless of operating environment */
#ifdef __APPLE__
#define WTF_PLATFORM_DARWIN 1
#endif

/* PLATFORM(WIN_OS) */
/* Operating system level dependencies for Windows that should be used */
/* regardless of operating environment */
#if defined(WIN32) || defined(_WIN32)
#define WTF_PLATFORM_WIN_OS 1
#endif

/* PLATFORM(FREEBSD) */
/* Operating system level dependencies for FreeBSD-like systems that */
/* should be used regardless of operating environment */
#ifdef __FreeBSD__
#define WTF_PLATFORM_FREEBSD 1
#endif

/* PLATFORM(SOLARIS) */
/* Operating system level dependencies for Solaris that should be used */
/* regardless of operating environment */
#if defined(sun) || defined(__sun)
#define WTF_PLATFORM_SOLARIS 1
#endif

/* PLATFORM(UNIX) */
/* Operating system level dependencies for Unix-like systems that */
/* should be used regardless of operating environment */
#if   PLATFORM(DARWIN)     \
   || PLATFORM(FREEBSD)    \
   || PLATFORM(SOLARIS)    \
   || defined(unix)        \
   || defined(__unix)      \
   || defined(__unix__)    \
   || defined (__NetBSD__) \
   || defined(_AIX)
#define WTF_PLATFORM_UNIX 1
#endif

/* Operating environments */

/* CPU */

/* PLATFORM(PPC) */
#if   defined(__ppc__)     \
   || defined(__PPC__)     \
   || defined(__powerpc__) \
   || defined(__powerpc)   \
   || defined(__POWERPC__) \
   || defined(_M_PPC)      \
   || defined(__PPC)
#define WTF_PLATFORM_PPC 1
#define WTF_PLATFORM_BIG_ENDIAN 1
#endif

/* PLATFORM(PPC64) */
#if   defined(__ppc64__) \
   || defined(__PPC64__)
#define WTF_PLATFORM_PPC64 1
#define WTF_PLATFORM_BIG_ENDIAN 1
#endif

/* PLATFORM(ARM) */
#if   defined(arm) \
   || defined(__arm__)
#define WTF_PLATFORM_ARM 1
#if defined(__ARMEB__)
#define WTF_PLATFORM_BIG_ENDIAN 1
#elif !defined(__ARM_EABI__) && !defined(__ARMEB__) && !defined(__VFP_FP__)
#define WTF_PLATFORM_MIDDLE_ENDIAN 1
#endif
#if !defined(__ARM_EABI__)
#define WTF_PLATFORM_FORCE_PACK 1
#endif
#endif

/* PLATFORM(X86) */
#if   defined(__i386__) \
   || defined(i386)     \
   || defined(_M_IX86)  \
   || defined(_X86_)    \
   || defined(__THW_INTEL)
#define WTF_PLATFORM_X86 1
#endif

/* PLATFORM(X86_64) */
#if   defined(__x86_64__) \
   || defined(__ia64__) \
   || defined(_M_X64)
#define WTF_PLATFORM_X86_64 1
#endif

/* Some compilers (eg. GCC), have a builtin endianness definition ... */
#if defined(__LITTLE_ENDIAN__) && !defined(WTF_PLATFORM_LITTLE_ENDIAN)
#define WTF_PLATFORM_LITTLE_ENDIAN 1
#endif

#if defined(__BIG_ENDIAN__) && !defined(WTF_PLATFORM_BIG_ENDIAN)
#define WTF_PLATFORM_BIG_ENDIAN 1
#endif

#if defined(WTF_PLATFORM_BIG_ENDIAN) && defined(WTF_PLATFORM_LITTLE_ENDIAN)
#error indeterminate platform endianness
#endif

/* Compiler */

/* COMPILER(GCC) */
#if defined(__GNUC__)
#define WTF_COMPILER_GCC 1

/* Define GCC_VERSION as per
        http://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
 */
#define GCC_VERSION (__GNUC__ * 10000 \
                        + __GNUC_MINOR__ * 100 \
                        + __GNUC_PATCHLEVEL__)

#endif

/* multiple threads only supported on Darwin for now */
#if PLATFORM(DARWIN) || PLATFORM(WIN)
#define WTF_USE_MULTIPLE_THREADS 1
#define WTF_USE_DTRACE 1
#endif

#if PLATFORM(DARWIN)
#include <Availability.h>
#define WTF_PLATFORM_CF 1
#define WTF_USE_PTHREADS 1
#define WTF_USE_LIBPCAP 1
#define HAVE_READLINE 1
#define HAVE_LAUNCHD 1
#define HAVE_LIBDISPATCH 1

/* We have DTrace on 10.5 and later. */
#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
#define HAVE_DTRACE 1
#endif

/* We have the Heimdal framework on 10.6 or later. */
#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
#define HAVE_HEIMDAL 1
#endif

#endif

#if COMPILER(GCC)
#define HAVE_COMPUTED_GOTO 1
#endif

#if PLATFORM(DARWIN)

#define HAVE_ERRNO_H 1
#define HAVE_MMAP 1
#define HAVE_MERGESORT 1
#define HAVE_SBRK 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TIMEB_H 1

#endif

#if !defined(errno_t_DEFINED)
#define errno_t_DEFINED
typedef int errno_t;
#endif

#endif /* PLATFORM_STD_PLATFORM_H */

/* vim: set cindent et ts=4 sw=4 tw=79 : */
