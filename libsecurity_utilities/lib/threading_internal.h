/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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


namespace Security {


//
// Architecture-specific atomic operation primitives.
// AtomicWord is an integer type that works with these;
// we'll assume that a pointer fits into it (using reinterpret_cast).
//
#if TARGET_CPU_PPC

#define _HAVE_ATOMIC_OPERATIONS

typedef unsigned int AtomicWord;

inline AtomicWord atomicLoad(AtomicWord &atom)
{
    AtomicWord result;
    asm volatile (
        "0:	lwarx %0,0,%1 \n"
        "	stwcx. %0,0,%1 \n"
        "	bne- 0b"
        : "=&r"(result)
        : "b"(&atom)
        : "cc"
    );
    return result;
}

inline AtomicWord atomicStore(AtomicWord &atom, AtomicWord newValue, AtomicWord oldValue)
{
    register bool result;
    asm volatile (
        "0:	lwarx %0,0,%1 \n"		// load and reserve -> %0
        "	cmpw %0,%3 \n"		// compare to old
        "	bne 1f \n"				// fail if not equal
        "	stwcx. %2,0,%1 \n"	// store and check
        "	bne 0b \n"				// retry if contended
        "1:	"
        : "=&r"(result)
        : "b"(&atom), "r"(newValue), "r"(oldValue)
        : "cc"
    );
    return result;
}

inline AtomicWord atomicOffset(AtomicWord &atom, int offset)
{
    AtomicWord result;
    asm volatile (
        "0:	lwarx %0,0,%1 \n"
        "	add %0,%0,%2 \n"
        "	stwcx. %0,0,%1 \n"
        "	bne- 0b"
        : "=&r"(result)
        : "b"(&atom), "r"(offset)
        : "cc"
    );
    return result;
}

inline AtomicWord atomicIncrement(AtomicWord &atom)
{ return atomicOffset(atom, +1); }

inline AtomicWord atomicDecrement(AtomicWord &atom)
{ return atomicOffset(atom, -1); }

#endif //TARGET_CPU_PPC

} // end namespace Security

#endif //_H_THREADING_INTERNAL
