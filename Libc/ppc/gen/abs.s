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
/* Copyright (c) 1992,1997 NeXT Software, Inc.  All rights reserved.
 *
 *	File:	libc/gen/ppc/abs.s
 *	Author:	Derek B Clegg, NeXT Software, Inc.
 *
 * HISTORY
 *  24-Jan-1997 Umesh Vaishampayan (umeshv@NeXT.com)
 *	Ported to PPC.
 *  10-Nov-92  Derek B Clegg (dclegg@next.com)
 *	Created.
 *  13-Jan-93  Derek B Clegg (dclegg@next.com)
 *      Optimized.
 *
 * ANSI X3.159-1989:
 *   int abs(int j);
 *
 * Description:
 *   The `abs' function computes the absolute value of an integer `j'.
 *   If the result cannot be represented, the behavior is undefined.
 * Returns:
 *   The `abs' function returns the absolute value.
 */
#include <architecture/ppc/asm_help.h>
#include <architecture/ppc/pseudo_inst.h>

/* We calculate abs(x) as
 *   s = x >> 31;
 *   y = x + s;
 *   return y ^ s;
 *
 * If x >= 0, then s = 0, so clearly we return x.  On the other hand, if
 * x < 0, then we may write x as ~z + 1, where z = -x.  In this case,
 * s = -1, so y = x - 1 = ~z, and hence we return -1 ^ (x - 1) = -1 ^ ~z
 * = z = -x.
 */
LEAF(_abs)
	srawi	a1,a0,31
	add	a2,a1,a0
	xor	a0,a2,a1
	blr
END(_abs)
