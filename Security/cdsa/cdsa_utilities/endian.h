/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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


/*
 * cssm utilities
 */
#ifndef _H_ENDIAN
#define _H_ENDIAN

#include <Security/utilities.h>
#include <Security/memutils.h>
#include <Security/debugging.h>

namespace Security {


//
// Encode/decode operations by type, overloaded.
// You can use these functions directly, but consider using
// the higher-level constructs below instead.
//
inline uint32 h2n(uint32 v)	{ return htonl(v); }
inline sint32 h2n(sint32 v)	{ return htonl(v); }
inline uint16 h2n(uint16 v)	{ return htons(v); }
inline sint16 h2n(sint16 v)	{ return htons(v); }
inline uint8 h2n(uint8 v)	{ return v; }
inline sint8 h2n(sint8 v)	{ return v; }

inline uint32 n2h(uint32 v)	{ return ntohl(v); }
inline sint32 n2h(sint32 v)	{ return ntohl(v); }
inline uint16 n2h(uint16 v)	{ return ntohs(v); }
inline sint16 n2h(sint16 v)	{ return ntohs(v); }
inline uint8 n2h(uint8 v)	{ return v; }
inline sint8 n2h(sint8 v)	{ return v; }


//
// Flip pointers
//
template <class Base>
inline Base *h2n(Base *p)	{ return (Base *)h2n(LowLevelMemoryUtilities::PointerInt(p)); }

template <class Base>
inline Base *n2h(Base *p)	{ return (Base *)n2h(LowLevelMemoryUtilities::PointerInt(p)); }


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
// Some structs we may want swapped in-place
//
void n2hi(CssmKey::Header &key);
void h2ni(CssmKey::Header &key);

inline void n2hi(CSSM_KEYHEADER &key) { n2hi(CssmKey::Header::overlay (key));}
inline void h2ni(CSSM_KEYHEADER &key) { h2ni(CssmKey::Header::overlay (key));}


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
