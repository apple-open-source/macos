/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * Copyright (c) 1992 NeXT Computer, Inc.
 *
 * Inlines for io space access.
 *
 * HISTORY
 *
 * 20 May 1992 ? at NeXT
 *	Created.
 */
 
#import <architecture/i386/io.h>

#if	!defined(DEFINE_INLINE_FUNCTIONS)
static
#endif
inline
unsigned char
inb(
    io_addr_t		port
)
{
    unsigned char	data;
    
    asm volatile(
    	"inb %1,%0"
	
	: "=a" (data)
	: "d" (port));
	
    return (data);
}
 
#if	!defined(DEFINE_INLINE_FUNCTIONS)
static
#endif
inline
unsigned short
inw(
    io_addr_t		port
)
{
    unsigned short	data;
    
    asm volatile(
    	"inw %1,%0"
	
	: "=a" (data)
	: "d" (port));
	
    return (data);
}

#if	!defined(DEFINE_INLINE_FUNCTIONS)
static
#endif
inline
void
outb(
    io_addr_t		port,
    unsigned char	data
)
{
    static int		xxx;

    asm volatile(
    	"outb %2,%1; lock; incl %0"
	
	: "=m" (xxx)
	: "d" (port), "a" (data), "0" (xxx)
	: "cc");
}

#if	!defined(DEFINE_INLINE_FUNCTIONS)
static
#endif
inline
void
outw(
    io_addr_t		port,
    unsigned short	data
)
{
    static int		xxx;

    asm volatile(
    	"outw %2,%1; lock; incl %0"
	
	: "=m" (xxx)
	: "d" (port), "a" (data), "0" (xxx)
	: "cc");
}

