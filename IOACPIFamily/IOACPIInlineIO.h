/*
 * Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
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

#ifndef __IOKIT_IOACPIINLINEIO_H
#define __IOKIT_IOACPIINLINEIO_H

#ifdef  __i386__

#define __IN(s, u)                           \
static inline unsigned u                     \
in##s(unsigned short port)                   \
{                                            \
    unsigned u data;                         \
    asm volatile (                           \
        "in" #s " %1, %0"                    \
        : "=a" (data)                        \
        : "d" (port));                       \
    return (data);                           \
}

#define __OUT(s, u)                          \
static inline void                           \
out##s(unsigned short port, unsigned u data) \
{                                            \
    asm volatile (                           \
        "out" #s " %1, %0"                   \
        :                                    \
        : "d" (port), "a" (data));           \
}

#else  /* ! __i386__ */

#define __IN(s, u)                           \
static inline unsigned u                     \
in##s(unsigned short port)                   \
{                                            \
    return (0);                              \
}

#define __OUT(s, u)                          \
static inline void                           \
out##s(unsigned short port, unsigned u data) \
{                                            \
}

#endif /* ! __i386__ */

__IN(b, char)
__IN(w, short)
__IN(l, long)

__OUT(b, char)
__OUT(w, short)
__OUT(l, long)

#endif /* !__IOKIT_IOACPIINLINEIO_H */
