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
/* Copyright (c) 1992, 1997 NeXT Software, Inc.  All rights reserved.
 * 
 *	File:	libc/gen/ppc/strcmp.c
 *
 *	This file contains machine dependent code for string comparison
 *	on NeXT 88K-based products.
 *
 * HISTORY
 *  24-Jan-1997 Umesh Vaishampayan (umeshv@NeXT.com)
 *	Ported to PPC.
 * 24-Nov-92  Derek B Clegg (dclegg@next.com)
 *	Created.
 */
#import <string.h>

/* This routine should be optimized. */

/* ANSI sez:
 *   The `strcmp' function compares the string pointed to by `s1' to the
 *   string pointed to by `s2'.
 *   The `strcmp' function returns an integer greater than, equal to, or less
 *   than zero, according as the string pointed to by `s1' is greater than,
 *   equal to, or less than the string pointed to by `s2'.  [4.11.4.2]
 */
int
strcmp(const char *s1, const char *s2)
{
    for ( ; *s1 == *s2; s1++, s2++)
	if (*s1 == '\0')
	    return 0;
    return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
}
