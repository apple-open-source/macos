/*
 * Copyright (c) 2000-2001,2003-2004,2011,2013-2014 Apple Inc. All Rights Reserved.
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
// threading_internal - internal support classes and functions for threading implementation
//
#ifndef _H_THREADING_INTERNAL
#define _H_THREADING_INTERNAL

#include <security_utilities/utilities.h>
#include <libkern/OSAtomic.h>


namespace Security {


//
// Do we have 64-bit atomic operations?
//
#define _HAVE_64BIT_ATOMIC (defined(__ppc64__) || defined(__i386__) || defined(__x86_64__))


//
// The AtomicTypes class is an implementation detail.
// Do not use it.
//
template <unsigned wordsize>
struct AtomicTypes {
	// unsupported word size (this will cause compilation errors if used)
};

template <>
struct AtomicTypes<32> {
	typedef int32_t Integer;
	
	static Integer add(int delta, Integer &base)
	{ return OSAtomicAdd32(delta, &base); }
	static Integer addb(int delta, Integer &base) { return OSAtomicAdd32Barrier(delta, &base); }
	
	static bool cas(Integer oldValue, Integer newValue, Integer &base)
	{ return OSAtomicCompareAndSwap32(oldValue, newValue, &base); }
	static bool casb(Integer oldValue, Integer newValue, Integer &base)
	{ return OSAtomicCompareAndSwap32Barrier(oldValue, newValue, &base); }
};

#if _HAVE_64BIT_ATOMIC

template <>
struct AtomicTypes<64> {
	typedef int64_t Integer;
	
	static Integer add(int delta, Integer &base) { return OSAtomicAdd64(delta, &base); }
	static Integer addb(int delta, Integer &base) { return OSAtomicAdd64Barrier(delta, &base); }
	
	static bool cas(Integer oldValue, Integer newValue, Integer &base)
	{ return OSAtomicCompareAndSwap64(oldValue, newValue, &base); }
	static bool casb(Integer oldValue, Integer newValue, Integer &base)
	{ return OSAtomicCompareAndSwap64Barrier(oldValue, newValue, &base); }
};

#endif //_HAVE_64BIT_ATOMIC


//
// Atomic<Type> is a set of (static) operations that can atomically access memory.
// This is not a wrapper object. Think of it as a generator class that produces
// the proper atomic memory operations for arbitrary data types.
// If the underlying system does not support atomicity for a particular type
// (e.g. 64 bits on ppc, or 16 bits anywhere), you will get compilation errors.
//
template <class Type>
class Atomic {
	typedef AtomicTypes<sizeof(Type) * 8> _Ops;
	typedef typename _Ops::Integer _Type;

public:
	static Type add(int delta, Type &store)
	{ return Type(_Ops::add(delta, (_Type &)store)); }
	static Type addb(int delta, Type &store)
	{ return Type(_Ops::addb(delta, (_Type &)store)); }
	
	static bool cas(Type oldValue, Type newValue, Type &store)
	{ return _Ops::cas(_Type(oldValue), _Type(newValue), (_Type &)store); }
	static bool casb(Type oldValue, Type newValue, Type &store)
	{ return _Ops::casb(_Type(oldValue), _Type(newValue), (_Type &)store); }
	
	static void barrier() { OSMemoryBarrier(); }
	static void readBarrier() { OSMemoryBarrier(); }
	static void writeBarrier() { OSMemoryBarrier(); }
	
	// convenience additions (expressed in terms above)
	
	static Type increment(Type &store) { return add(1, store); }
	static Type decrement(Type &store) { return add(-1, store); }
	
	static Type load(const Type &store) { readBarrier(); return store; }
	static Type store(Type value, Type &store)
	{ while (!casb(store, value, store)) {}; return value; }
};


} // end namespace Security

#endif //_H_THREADING_INTERNAL
