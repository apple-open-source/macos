// header.h --
// $Id: header.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * The internal header included in all source files
 */

#ifndef __HEADER_H__
#define __HEADER_H__

/////////////////////////////////////////////////////////////////////////////

#include "config.h"

/////////////////////////////////////////////////////////////////////////////
// A number of preprocessor options are used in the source code
//
//  q4_DOS      MS-DOS real-mode OS
//  q4_MAC      Apple Macintosh OS
//  q4_UNIX     Unix, any flavor
//  q4_VMS      DEC OpenVMS OS
//  q4_WIN      Microsoft Windows OS, any flavor
//  q4_WIN32    Microsoft Windows OS, 32-bit
//  q4_WINCE    Microsoft Windows OS, embedded
//
//  q4_MFC      Microsoft MFC framework
//  q4_STD      Standard STL version
//  q4_UNIV     Universal version
//
//  q4_BOOL     compiler supports bool datatype
//  q4_CHECK    enable assertion checks
//  q4_FIX      manual header fix (see above)
//  q4_INLINE   enable inline expansion
//  q4_KITDLL   compile as DLL (shared library)
//  q4_MULTI    compile for multi-threading
//  q4_NOLIB    do not add automatic lib linkage (MSVC5)
//  q4_NO_NS    don't use namespaces for STL
//  q4_OK       assume all software is perfect
//  q4_STRICT   do not disable any compiler warnings
//  q4_TINY     small version, no floating point
//
/////////////////////////////////////////////////////////////////////////////

#define __K4CONF_H__    // skip section in "mk4.h", since we use "header.h"

// if neither MFC nor STD are specified, default to Universal version
#if !q4_MFC && !q4_STD && !defined (q4_UNIV)
#define q4_UNIV 1
#endif 

/////////////////////////////////////////////////////////////////////////////
// You can either use '#define q4_xxx 1' to flag the choice of an OS, or
// use a '#define d4_OS_H "file.h"' to force inclusion of a header later.

#if defined (__MINGW32__)
#define d4_OS_H "win.h"
#elif defined (MSDOS) && defined (__GNUC__)
#define q4_DOS 1
#elif defined(unix) || defined(__unix__) || defined(__GNUC__) || \
defined(_AIX) || defined(__hpux)
#define q4_UNIX 1
#elif defined (__VMS)
#define q4_VMS 1
#elif defined (macintosh)
#define q4_MAC 1
#elif !defined (d4_OS_H)
#define d4_OS_H "win.h"
#endif 

/////////////////////////////////////////////////////////////////////////////
// Use '#define q4_xxx 1' to flag the choice of a CPU.

#if defined (_M_I86) || defined (_M_IX86) || defined (i386)
#define q4_I86 1
#if defined (_M_I86SM)
#define q4_TINY 1
#endif 
#elif defined (__powerc)
#define q4_PPC 1
#elif defined (__alpha)
#define q4_AXP 1
#define q4_LONG64 1
#elif defined (__VMS)
#define q4_VAX 1
#else 
#define q4_M68K 1
#endif 

/////////////////////////////////////////////////////////////////////////////
// Use '#define q4_xxx 1' to flag the choice of an IDE, and optionally also
// add '#include "file.h"' to force inclusion of a header file right here.

#if defined (__BORLANDC__)                  // Borland C++
#include "borc.h"
#elif defined (__DECCXX)                    // DEC C++
#define q4_DECC 1
#elif defined (__GNUC__)                    // GNU C++
#include "gnuc.h"
#elif defined (__MWERKS__)                  // Metrowerks CodeWarrior C++
#include "mwcw.h"
#elif defined (_MSC_VER)                    // Microsoft Visual C++
#include "msvc.h"
#elif defined (__SC__)                      // Symantec C++
#define q4_SYMC 1
#elif defined (__WATCOMC__)                 // Watcom C++
#define q4_WATC 1
#endif 

/////////////////////////////////////////////////////////////////////////////
// Some of the options take precedence over others

#if !q4_BOOL && !q4_STD         // define a bool datatype
#define false 0
#define true 1
#define bool int
#endif 

#if !q4_CHECK                   // disable assertions
#undef d4_assert
#define d4_dbgdef(x)
#define d4_assert(x)
#endif 

#if q4_NO_NS                    // don't use namespaces
#define d4_std
#else 
#define d4_std std
#endif 

#if HAVE_MEMMOVE
#define d4_memmove(d,s,n)   memmove(d,s,n)
#elif HAVE_BCOPY
#define d4_memmove(d,s,n)   bcopy(s,d,n)
#else 
#define d4_memmove f4_memmove
extern void f4_memmove(void *d, const void *s, int n);
#endif 

typedef unsigned char t4_byte; // create typedefs for t4_byte, etc.

#if (!defined(__APPLE__) && SIZEOF_LONG == 8) || (defined(__APPLE__) && defined(__LP64__))
typedef int t4_i32; // longs are 64b, so int must be 32b
#else 
typedef long t4_i32; // longs aren't 64b, so they are 32b
#endif 

/////////////////////////////////////////////////////////////////////////////
// Include header files which contain additional os/cpu/ide/fw specifics

#ifdef d4_OS_H                  // operating system dependencies
#include d4_OS_H
#endif 

/////////////////////////////////////////////////////////////////////////////
// Several defines should always be set

#ifndef d4_assert               // assertion macro
#include <assert.h>
#define d4_assert assert
#endif 

#ifndef d4_dbgdef               // conditionally compiled
#ifdef NDEBUG
#define d4_dbgdef(x)
#else 
#define d4_dbgdef(x) x
#endif 
#endif 

#ifndef d4_new                  // heap allocator
#define d4_new new
#endif 

#ifndef d4_reentrant            // thread-local storage
#define d4_reentrant
#endif 

/////////////////////////////////////////////////////////////////////////////
// Debug logging option, called internally where properties are modified

#if q4_LOGPROPMODS
void f4_DoLogProp(const c4_Handler *, int, const char *, int);
#else 
#define f4_LogPropMods(a,b) 0
#endif 

/////////////////////////////////////////////////////////////////////////////
// Public definitions, plus a few more framework-specific ones

#include "mk4.h"

#if q4_MFC
#include "mfc.h"
#elif q4_STD
#include "std.h"
#elif q4_UNIV
#include "univ.h"
#endif 

#ifdef _MSC_VER
#pragma warning(disable: 4100 4127 4135 4244 4511 4512 4514)
#endif 

#include <string.h>

/////////////////////////////////////////////////////////////////////////////
// Report unexpected combinations of settings

#if !q4_FIX
#if (q4_DOS+q4_MAC+q4_UNIX+q4_VMS+q4_WIN) != 1
#error Exactly one operating system should have been defined
#endif 
#if (q4_MFC+q4_STD+q4_UNIV) != 1
#error Exactly one container library should have been defined
#endif 
#endif 

/////////////////////////////////////////////////////////////////////////////

#endif
