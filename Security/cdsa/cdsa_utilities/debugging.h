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

#include <Security/utilities.h>
#include <cstdarg>
#include <typeinfo>

namespace Security {
namespace Debug {


#if !defined(NDEBUG)


// Debug to standard target
void debug(const char *scope, const char *format, ...) __attribute__((format(printf,2,3)));
void vdebug(const char *scope, const char *format, va_list args);
bool debugging(const char *scope);

// Stream dumping to standard target
bool dumping(const char *scope);
void dump(const char *format, ...) __attribute((format(printf,1,2)));
void dumpData(const void *data, size_t length);
void dumpData(const char *title, const void *data, size_t length);
template <class Data> inline void dumpData(const Data &obj)
{ dumpData(obj.data(), obj.length()); }
template <class Data> inline void dumpData(const char *title, const Data &obj) 
{ dumpData(title, obj.data(), obj.length()); }

#if defined(DEBUGDUMP)
# define IFDUMP(code)				code
# define IFDUMPING(scope,code)		if (Debug::dumping(scope)) code; else /* no */
#else
# define IFDUMP(code)				/* no-op */
# define IFDUMPING(scope,code)		/* no-op */
#endif


//
// A (prepared) debug scope object.
//
class Scope {
public:
	Scope(const char *string)	{ mScope = string; }
	
	void operator () (const char *format, ...);
	
private:
	const char *mScope;
};


//
// Given an object of any type, produce the proper name of its type.
//
string makeTypeName(const type_info &info);

template <class Object>
string typeName(const Object &obj)
{
	return makeTypeName(typeid(obj));
}


#else // NDEBUG


//
// If NDEBUG is defined, we try to make all debugging functions weightless
//

#if __GNUC__ > 2
inline void debug(const char *, const char *, ...) { }
#else
// @@@ Hack to work around the fact that gcc2 can't inline empty varargs functions.
extern "C" inline void debug() { }
#endif

inline void vdebug(const char *, const char *, va_list) { }
inline bool debugging(const char *) { return false; }

class Scope {
public:
	Scope(const char *)		{ }

	// @@@ Hack to work around the fact that gcc can't inline empty varargs functions.
	//void operator () (const char *, ...)	{ }
	void operator () (const char *, ...);
};

inline bool dumping(const char *) { return false; }

// @@@ Hack to work around the fact that gcc can't inline empty varargs functions.
//inline void dump(const char *, ...) { }
extern "C" inline void dump() { }

inline void dumpData(const void *, size_t) { }
void dumpData(const char *, const void *, size_t);
template <class Data> inline void dumpData(const Data &) { }
template <class Data> inline void dumpData(const char *, const Data &) { }

// debugdumping is forced off
#if defined(DEBUGDUMP)
# undef DEBUGDUMP
#endif
# define IFDUMP(code)				/* no-op */
# define IFDUMPING(scope,code)		/* no-op */

// no debug typeName; don't call this if NDEBUG

#endif // NDEBUG


} // end namespace Debug
} // end namespace Security

// We intentionally leak a few functions into the global namespace
using Security::Debug::debug;


#endif //_H_DEBUGGING
