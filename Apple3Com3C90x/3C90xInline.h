/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __APPLE_3COM_3C90X_INLINE_H
#define __APPLE_3COM_3C90X_INLINE_H

//------------------------------------------------------------------------
// x86 IN/OUT I/O inline functions.
//
// IN :  inb, inw, inl
//       IN(port)
//
// OUT:  outb, outw, outl
//       OUT(port, data)

#define __IN(s, u)         \
static __inline__ UInt##u  \
in##s(UInt16 port)         \
{                          \
    UInt##u data;          \
    asm volatile (         \
        "in" #s " %1,%0"   \
        : "=a" (data)      \
        : "d" (port));     \
    return (data);         \
}

#define __OUT(s, u)                  \
static __inline__ void               \
out##s(UInt16 port, UInt##u data)    \
{                                    \
    asm volatile (                   \
        "out" #s " %1,%0"            \
        :                            \
        : "d" (port), "a" (data));   \
}

__IN( b,  8 )
__IN( w, 16 )
__IN( l, 32 )

__OUT( b,  8 )
__OUT( w, 16 )
__OUT( l, 32 )

#endif /* !__APPLE_3COM_3C90X_INLINE_H */
