/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
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
/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 *	File:	libc/ppc/sys/vfork.s
 *
 * HISTORY
 * 23-Jun-1998	Umesh Vaishampayan (umeshv@apple.com)
 *	Created from fork.s
 *
 */

/* We use mode-independent "g" opcodes such as "srgi", and/or
 * mode-independent macros such as MI_GET_ADDRESS.  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>

/* In vfork(), the child runs in parent's address space.  */

#include "SYS.h"

MI_ENTRY_POINT(_vfork)
    MI_GET_ADDRESS(r5,__current_pid)  // get address of __current_pid in r5
2:
	lwarx	r6,0,r5			// don't cache pid across vfork
	cmpwi	r6,0
	ble--	3f              // is another vfork in progress
	li      r6,0			// if not, erase the stored pid
3:	
	addi	r6,r6,-1		// count the parallel vforks in
	stwcx.	r6,0,r5			// negative cached pid values
	bne--	2b
	
	li      r0,SYS_vfork
	sc
	b       Lbotch			// error return

	cmpwi	r4,0
	beq     Lparent			// parent, since a1 == 0 in parent,

	li      r3,0			// child
	blr

Lparent:                    // r3 == child's pid
	lwarx	r6,0,r5			// we're back, decrement vfork count
	addi	r6,r6,1
	stwcx.	r6,0,r5
	bne--	Lparent
	blr                     // return pid

Lbotch:
	lwarx	r6,0,r5			// never went, decrement vfork count
	addi	r6,r6,1
	stwcx.	r6,0,r5
	bne--	Lbotch

	MI_BRANCH_EXTERNAL(cerror)

