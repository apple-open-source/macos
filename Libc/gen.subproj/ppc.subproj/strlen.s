/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/* Copyright (c) 1992, 1997 NeXT Software, Inc.  All rights reserved.
 *
 *	File:	libc/gen/ppc/strlen.s
 *
 * HISTORY
 *  24-Jan-1997 Umesh Vaishampayan (umeshv@NeXT.com)
 *	Ported to PPC.
 *  12-Nov-92  Derek B Clegg (dclegg@next.com)
 *	Created.
 *
 * size_t strlen(const char *s);
 *
 * Description:
 *
 * Returns:
 */
#import	<architecture/ppc/asm_help.h>
#import	<architecture/ppc/pseudo_inst.h>

/* size_t strlen(const char *s); */

#define a8 ep
#define a9 at

.at_off

#define check_for_zero(reg, tmp, found_zero, checked_all) \
	extrwi.	tmp,reg,8,0						@\
	beq-	found_zero						@\
	bdz-	checked_all						@\
	extrwi.	tmp,reg,8,8						@\
	beq-	found_zero						@\
	bdz-	checked_all						@\
	extrwi.	tmp,reg,8,16						@\
	beq-	found_zero						@\
	bdz-	checked_all						@\
	extrwi.	tmp,reg,8,24						@\
	beq-	found_zero						@\
	bdz-	checked_all

#define check_32(tmp, found_zero) \
	check_for_zero(a2, tmp, found_zero, 1f)		@\
	check_for_zero(a3, tmp, found_zero, 1f)		@\
	check_for_zero(a4, tmp, found_zero, 1f)		@\
	check_for_zero(a5, tmp, found_zero, 1f)		@\
	check_for_zero(a6, tmp, found_zero, 1f)		@\
	check_for_zero(a7, tmp, found_zero, 1f)		@\
	check_for_zero(a8, tmp, found_zero, 1f)		@\
	check_for_zero(a9, tmp, found_zero, 1f)		@\
1:

LEAF(_strlen)
	dcbt	0,a0		// Try to load the cache line.
	andi.	a1,a0,31	// We want to be 32-byte aligned.
	subfic	a1,a1,32	// Get the number of bytes to start with.
	mtxer	a1		// Setup xer for load.
	lswx	a2,0,a0		// Load the bytes.
	mtctr	a1		// Setup ctr for zero check.
	check_32(zt, L_found_zero)

	/* Now check 32-byte blocks. At this point, ctr is zero, so we can
	 * use it as our running count. */
	dcbt	a1,a0		// Try to load the cache line.
	li32	a2,32
	mtxer	a2
	li32	a2,0
	mtctr	a2

2:	lswx	a2,a1,a0	// Load 32 bytes.
	check_32(zt, L_found_zero)
	addi	a0,a0,32	// Increment the pointer.
	dcbt	a1,a0		// Try to load the cache.
	b	2b		// Loop

L_found_zero:
	mfctr	a0
	sub	a0,a1,a0	// Get the number of bytes before 0.
	blr			// Return.
END(_strlen)
