/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1991 NeXT Computer, Inc.  All rights reserved.
 *
 *      File:   libc/i386/ansi/memset.c
 *      Author: Bruce Martin, NeXT Computer, Inc.
 *
 *      This file contains machine dependent code for block compares
 *      on NeXT i386-based products.  Currently tuned for the i486.
 *
 *	This code is loosely based on some work done by Mike DeMoney
 *	for the m88k.
 *
 * HISTORY
 * 14-Dec-93  Curtis Galloway
 *	Modified for the booter; simplified simple_char_set.
 *
 * 14-Aug-92  Bruce Martin (Bruce_Martin@NeXT.COM)
 *	Created.
 */

#define MIN_TO_ALIGN	32

/*
 * Convert a void * into a unsigned int so arithmetic can be done
 * (Technically, the gnu C compiler is tolerant of arithmetic
 * on void *, but ansi isn't; so we do this.)
 */
#define UNS(voidp)      ((unsigned int)(voidp))

/*
 * Number of bytes addr is past an object of 'align' size.
 */
#define BYTES_PAST_ALIGNMENT(voidp, align) \
	(UNS(voidp) & (align - 1))

/*
 * Bytes moved by an unrolled loop
 */
#define LOOP_STRIDE(type, unroll)	((unroll) * sizeof(type))


/*
 * Len modulo LOOP_STRIDE
 */
#define MODULO_LOOP_UNROLL(len, type, unroll)	\
	((len) & (LOOP_STRIDE(type, unroll) - 1) & ~(sizeof(type) - 1))


/*
 * Convert a void * + offset into char, short, int, or long long reference
 * based at that address
 */
#define CHAR(voidp, offset)	(*(char *)(UNS(voidp) + (offset)))
#define SHORT(voidp, offset)	(*(short *)(UNS(voidp) + (offset)))
#define INT(voidp, offset)	(*(int *)(UNS(voidp) + (offset)))



static inline void
simple_char_set(char *dst, int val, int len)
{
    asm("rep; stosb"
	: /* no outputs */
	: "&c" (len), "D" (dst), "a" (val)
	: "ecx", "edi", "eax");
}



void *memset(void *, int, unsigned long);

int bzero(void *dst, unsigned long ulen)
{
    (void) memset(dst, 0, ulen);
    return 0;
}


void *memset(void *dst, int val, unsigned long ulen)
{
    int len = ulen;
    void *orig_dst = dst;
    int need_to_set;
    int alignment;

    val |= (val << 8);
    val |= (val << 16);

    /* Always do a forward copy */

    if (len < MIN_TO_ALIGN) {
	simple_char_set(dst, val, len);
	return orig_dst;
    }

    if (need_to_set = BYTES_PAST_ALIGNMENT(dst, 2 * sizeof(int))) {
	need_to_set = (2 * sizeof(int)) - need_to_set;
	simple_char_set(dst, val, need_to_set);
	len -= need_to_set;
	UNS(dst) += need_to_set;
    }

    alignment = MODULO_LOOP_UNROLL(len, int, 8);
    UNS(dst) += alignment - LOOP_STRIDE(int, 8);
    switch (alignment) {
	    do {
			INT(dst, 0) = val;
        case 28:	INT(dst, 4) = val;
        case 24:	INT(dst, 8) = val;
        case 20:	INT(dst, 12) = val;
        case 16:	INT(dst, 16) = val;
        case 12:	INT(dst, 20) = val;
        case 8:		INT(dst, 24) = val;
        case 4:         INT(dst, 28) = val;
        case 0:
			UNS(dst) += LOOP_STRIDE(int, 8);
			len -= LOOP_STRIDE(int, 8);
	    } while (len >= 0);
	len &= sizeof(int) - 1;
    }
	
    switch (len) {
	case 3:	CHAR(dst, 2) = val;
	case 2:	CHAR(dst, 1) = val;
	case 1:	CHAR(dst, 0) = val;
    }

    return orig_dst;
}
