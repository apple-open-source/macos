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
 *	File:	libc/gen/ppc/bzero.s
 *
 * HISTORY
 *  24-Jan-1997 Umesh Vaishampayan (umeshv@NeXT.com)
 *	Ported to PPC.
 *  18-Jan-93  Derek B Clegg (dclegg@next.com)
 *	Created.
 *
 * void bzero(void *s, size_t n);
 *
 * Description:
 *     The `bzero' function copies a zero into the first `n' bytes of
 *   the object pointed to by `s'.
 * Returns:
 *     The `bzero' function does not return a value.
 */
#import	<architecture/ppc/asm_help.h>
#import	<architecture/ppc/pseudo_inst.h>

#define a8	ep
#define a9	at

.at_off

LEAF(_bzero)
	li32	a2,0		// Clear registers a2 - a9.
	li32	a3,0
	li32	a4,0
	li32	a5,0
	li32	a6,0
	li32	a7,0
	li32	a8,0
	li32	a9,0
	andi.	zt,a0,31	// We want `s' to be 32-byte aligned.
	subfic	zt,zt,32	// Get the number of bytes to start with.
	cmplw	a1,zt		// Don't move more than we were asked to.
	ble-	L_finish
	mtxer	zt		// Load xer with the number of bytes to move.
	stswx	a2,0,a0		// Store into `s'.
	add	a0,a0,zt	// Advance the pointer; a0 is now aligned.
	sub	a1,a1,zt	// Calculate the bytes remaining to move.
	srwi.	zt,a1,5		// Get the number of 32-byte blocks to move.
	beq-	L_finish	// Branch if fewer than 32 bytes are left.
	mtctr	zt		// Setup counter for loop.
	andi.	a1,a1,31	// Get the number of bytes left over.
L_loop:
	dcbz	0,a0		// Clear 32 bytes.
	addi	a0,a0,32	// Advance the pointer.
	bdnz+	L_loop		// Continue if there are still blocks to move.

	/* At this point, `a1' contains the number of bytes left to move. */
L_finish:
	mtxer	a1		// Setup xer with # of bytes left to move.
	stswx	a2,0,a0		// Store.
	blr
END(_bzero)
