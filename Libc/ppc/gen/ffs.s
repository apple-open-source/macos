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
 *      File:  libc/gen/ppc/ffs.s
 *
 *      Find the first bit set (starting with the least significant bit).
 *
 * HISTORY
 *
 *  24-Jan-1997 Umesh Vaishampayan (umeshv@NeXT.com)
 *      Ported to PPC. 
 *
 *  19-Jan-1998 Matt Watson (mwatson@apple.com)
 *      Simplified
 *
 *  10-Mar-1998 Matt Watson (mwatson@apple.com)
 *      Correctified
 *
 */

.text
.align 2
.globl _ffs
_ffs:
	neg    r0,r3
	and    r3,r0,r3
	cntlzw r3,r3
	subfic r3,r3,32
	blr
