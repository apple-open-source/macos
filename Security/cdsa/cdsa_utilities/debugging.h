/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// debugging - non-trivial debug support
//
#ifndef _H_DEBUGGING
#define _H_DEBUGGING

#ifdef __cplusplus

#include <Security/utilities.h>
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
#if !defined(NDEBUG)
# define secdebug(scope, format...)	Security::Debug::debug(scope, ## format)
#else //NDEBUG
# define secdebug(scope, format...)	/* nothing */
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
