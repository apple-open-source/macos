/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// debugging - non-trivial debug support
//
#ifndef _H_DEBUGGING
#define _H_DEBUGGING

#ifdef __cplusplus

#include <security_utilities/utilities.h>
#include <cstdarg>
#include <typeinfo>


namespace Security {
namespace Debug {


//
// Debug logging functions always exist.
// They may be stubs depending on build options.
//
bool debugging(const char *scope);
void debug(const char *scope, const char *format, ...) __attribute__((format(printf,2,3)));
void vdebug(const char *scope, const char *format, va_list args);


//
// Ditto with debug dumping functions.
//
bool dumping(const char *scope);
void dump(const char *format, ...) __attribute((format(printf,1,2)));
void dumpData(const void *data, size_t length);
void dumpData(const char *title, const void *data, size_t length);
template <class Data> inline void dumpData(const Data &obj)
{ dumpData(obj.data(), obj.length()); }
template <class Data> inline void dumpData(const char *title, const Data &obj) 
{ dumpData(title, obj.data(), obj.length()); }


//
// If the file exists, delay (sleep) as many seconds as its first line indicates,
// to allow attaching with a debugger.
//
void delay(const char *file);


//
// The following functions perform runtime recovery of type names.
// This is meant for debugging ONLY. Don't even THINK of depending
// on this for program correctness. For all you know, we may replace
// all those names with "XXX" tomorrow.
//
string makeTypeName(const type_info &info);

template <class Object>
string typeName(const Object &obj)
{
	return makeTypeName(typeid(obj));
}

template <class Object>
string typeName()
{
	return makeTypeName(typeid(Object));
}


//
// Now for the conditional inline code
//
#undef DEBUGGING
#if !defined(NDEBUG)
# define DEBUGGING 1
# define secdebug(scope, format...)	Security::Debug::debug(scope, ## format)
# define secdelay(file) Security::Debug::delay(file)
// Enable debug dumping
# define DEBUGDUMP  1
#else //NDEBUG
# define DEBUGGING 0
# define secdebug(scope, format...)	/* nothing */
# define secdelay(file) /* nothing */
#endif //NDEBUG


//
// Conditional dump code
//
#if defined(DEBUGDUMP)
# define IFDUMP(code)				code
# define IFDUMPING(scope,code)		if (Debug::dumping(scope)) code; else /* no */
#else
# define IFDUMP(code)				/* no-op */
# define IFDUMPING(scope,code)		/* no-op */
#endif


//
// Kernel trace support
//
inline void trace(int code, int arg1 = 0, int arg2 = 0, int arg3 = 0, int arg4 = 0)
{
#if defined(ENABLE_SECTRACE)
	syscall(180, code, arg1, arg2, arg3, arg4);
#endif
}


} // end namespace Debug
} // end namespace Security

// We intentionally leak a few functions into the global namespace
// @@@ (not much longer: after the switch to secdebug(), this will go)
using Security::Debug::debug;


#else	//!__cplusplus, C code


extern void __security_debug(const char *scope, const char *format, ...);
extern int __security_debugging(const char *scope);

#if !defined(NDEBUG)
# define secdebug(scope, format...)	__security_debug(scope, ## format)
#else
# define secdebug(scope, format...)	/* nothing */
#endif


// ktrace support (C style)
extern void security_ktrace(int code);

#endif	//__cplusplus

#endif //_H_DEBUGGING
