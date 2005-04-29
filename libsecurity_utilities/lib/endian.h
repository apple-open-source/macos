/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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


/*
 * cssm utilities
 */
#ifndef _H_ENDIAN
#define _H_ENDIAN

#include <security_utilities/utilities.h>
#include <security_utilities/memutils.h>
#include <security_utilities/debugging.h>

namespace Security {


//
// Encode/decode operations by type, overloaded.
// You can use these functions directly, but consider using
// the higher-level constructs below instead.
//
inline UInt32 h2n(UInt32 v)	{ return htonl(v); }
inline SInt32 h2n(SInt32 v)	{ return htonl(v); }
inline UInt16 h2n(UInt16 v)	{ return htons(v); }
inline SInt16 h2n(SInt16 v)	{ return htons(v); }
inline UInt8 h2n(UInt8 v)	{ return v; }
inline SInt8 h2n(SInt8 v)	{ return v; }

inline UInt32 n2h(UInt32 v)	{ return ntohl(v); }
inline SInt32 n2h(SInt32 v)	{ return ntohl(v); }
inline UInt16 n2h(UInt16 v)	{ return ntohs(v); }
inline SInt16 n2h(SInt16 v)	{ return ntohs(v); }
inline UInt8 n2h(UInt8 v)	{ return v; }
inline SInt8 n2h(SInt8 v)	{ return v; }


//
// Flip pointers
//
template <class Base>
inline Base *h2n(Base *p)	{ return (Base *)h2n(uintptr_t(p)); }

template <class Base>
inline Base *n2h(Base *p)	{ return (Base *)n2h(uintptr_t(p)); }


//
// Generic template - do nothing, issue debug warning
//
template <class Type>
inline const Type &h2n(const Type &v)
{
	secdebug("endian", "generic h2n called for type %s", Debug::typeName(v).c_str());
	return v;
}

template <class Type>
inline const Type &n2h(const Type &v)
{
	secdebug("endian", "generic n2h called for type %s", Debug::typeName(v).c_str());
	return v;
}


//
// In-place fix operations
//
template <class Type>
inline void h2ni(Type &v)	{ v = h2n(v); }

template <class Type>
inline void n2hi(Type &v)	{ v = n2h(v); }


//
// Endian<SomeType> keeps NBO values in memory and converts
// during loads and stores. This presumes that you are using
// memory blocks thare are read/written/mapped as amorphous byte
// streams, but want to be byte-order clean using them.
//
// The generic definition uses h2n/n2h to flip bytes. Feel free
// to declare specializations of Endian<T> as appropriate.
//
// Note well that the address of an Endian<T> is not an address-of-T,
// and there is no conversion available.
//
template <class Type>
class Endian {
public:
    typedef Type Value;
    Endian() : mValue(0) { }
    Endian(Value v) : mValue(h2n(v)) { }
    
    operator Value () const		{ return n2h(mValue); }
    Endian &operator = (Value v)	{ mValue = h2n(v); return *this; }
    
private:
    Value mValue;
};

}	// end namespace Security


#endif //_H_ENDIAN
