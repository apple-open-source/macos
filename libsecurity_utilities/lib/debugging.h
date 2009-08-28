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


//
// Include DTrace static probe definitions
//
typedef const void *DTException;

#include <security_utilities/utilities_dtrace.h>


#ifdef __cplusplus

#include <security_utilities/utilities.h>
#include <cstdarg>
#include <typeinfo>


namespace Security {
namespace Debug {


//
// Debug-dumping functions always exist. They may be stubs depending on build options.
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
// We are still conditionally emitting debug-dumping code
//
#undef DEBUGGING
#if !defined(NDEBUG)
# define DEBUGGING 1
# define DEBUGDUMP  1
#else //NDEBUG
# define DEBUGGING 0
#endif //NDEBUG

#if defined(DEBUGDUMP)
# define IFDUMP(code)				code
# define IFDUMPING(scope,code)		if (Debug::dumping(scope)) code; else /* no */
#else
# define IFDUMP(code)				/* no-op */
# define IFDUMPING(scope,code)		/* no-op */
#endif


//
// We have some very, very old customers who call old debug facilities.
// Dummy them out for now.
//
inline bool debugging(const char *scope) DEPRECATED_ATTRIBUTE;
inline void debug(const char *scope, const char *format, ...) DEPRECATED_ATTRIBUTE;
inline void vdebug(const char *scope, const char *format, va_list args) DEPRECATED_ATTRIBUTE;

inline bool debugging(const char *scope) { return false; }
inline void debug(const char *scope, const char *format, ...) { }
inline void vdebug(const char *scope, const char *format, va_list args) { }





} // end namespace Debug
} // end namespace Security

// leak debug() into the global namespace because URLAccess et al rely on that
using Security::Debug::debug;


#else	//__cplusplus

#include <stdio.h>

#endif	//__cplusplus


//
// The debug-log macro is now unconditionally emitted as a DTrace static probe point.
//
#define secdebug(scope, format...) \
	if (__builtin_expect(SECURITY_DEBUG_LOG_ENABLED(), 0)) { \
		char __msg[500]; snprintf(__msg, sizeof(__msg), ## format); \
		volatile char c __attribute__((unused)) = scope[0]; \
		SECURITY_DEBUG_LOG((char *)(scope), (__msg)); \
	} else /* nothing */
#define secdebugf(scope, __msg)	SECURITY_DEBUG_LOG((char *)(scope), (__msg))


//
// The old secdelay() macro is also emitted as a DTrace probe (use destructive actions to handle this).
// Secdelay() should be considered a legacy feature; just put a secdebug at the intended delay point.
//
#define secdelay(file)	SECURITY_DEBUG_DELAY((char *)(file))


#endif //_H_DEBUGGING
