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
/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 *	File:	libc/ppc/sys/vfork.s
 *
 * HISTORY
 * 23-Jun-1998	Umesh Vaishampayan (umeshv@apple.com)
 *	Created from fork.s
 *
 */
#if 0
#import <sys/syscall.h>
#import <architecture/ppc/asm_help.h>
#import	<architecture/ppc/pseudo_inst.h>
#import	<mach/ppc/syscall_sw.h>

/* We use 8 bytes for LOCAL_VAR(1) and LOCAL_VAR(2) */
NESTED(_vfork, 8, 0, 0, 0)
	CALL_EXTERN(__cthread_fork_prepare)
#if defined(__DYNAMIC__)
.cstring
LC1:
	.ascii	"__dyld_fork_prepare\0"
.text
	.align 2
	mflr	r0
	bl	1f
1:	mflr	r3
	mtlr	r0
	addis	r3,r3,ha16(LC1-1b)
	addi	r3,r3,lo16(LC1-1b)
	addi 	r4,r1,LOCAL_VAR(1)
	bl 	__dyld_func_lookup
	lwz	r3,LOCAL_VAR(1)(r1)
	mtspr 	ctr,r3
	bctrl	
#endif
	li	r0,SYS_vfork
	sc
	b	Lbotch			// error return

	cmpwi	r4,0
	beq	Lparent			// parent, since a1 == 0 in parent,
					//		       1 in child
#if defined(__DYNAMIC__)
.cstring
LC3:
	.ascii	"__dyld_fork_child\0"
.text
	.align 2
	mflr	r0
	bl	1f
1:	mflr	r3
	mtlr	r0
	addis	r3,r3,ha16(LC3-1b)
	addi	r3,r3,lo16(LC3-1b)
	addi 	r4,r1,LOCAL_VAR(1)
	bl 	__dyld_func_lookup
	lwz	r3,LOCAL_VAR(1)(r1)
	mtspr 	ctr,r3
	bctrl	
#endif
	li	r3,0
	REG_TO_EXTERN(r3, EXT(_current_pid))
	CALL_EXTERN(_fork_mach_init)
	CALL_EXTERN(__cthread_fork_child)
#if defined(__DYNAMIC__)
.cstring
LC4:
	.ascii	"__dyld_fork_child_final\0"
.text
	.align 2
	mflr	r0
	bl	1f
1:	mflr	r3
	mtlr	r0
	addis	r3,r3,ha16(LC4-1b)
	addi	r3,r3,lo16(LC4-1b)
	addi 	r4,r1,LOCAL_VAR(1)
	bl 	__dyld_func_lookup
	lwz	r3,LOCAL_VAR(1)(r1)
	mtspr 	ctr,r3
	bctrl	
#endif

	li	r3,0
	b	Lreturn

Lparent:
#if defined(__DYNAMIC__)
	stw	r3,LOCAL_VAR(2)(r1)	// save child pid
	mflr	r0
	bl	1f
1:	mflr	r3
	mtlr	r0
	addis	r3,r3,ha16(LC2-1b)
	addi	r3,r3,lo16(LC2-1b)
	addi 	r4,r1,LOCAL_VAR(1)
	bl 	__dyld_func_lookup
	lwz	r3,LOCAL_VAR(1)(r1)
	mtspr 	ctr,r3
	bctrl		
#endif
	CALL_EXTERN(__cthread_fork_parent)
#if defined(__DYNAMIC__)
	lwz	r3,LOCAL_VAR(2)(r1)
#endif
	b	Lreturn

Lbotch:	
#if defined(__DYNAMIC__)
.cstring
LC2:
	.ascii	"__dyld_fork_parent\0"
.text
	.align 2
	stw	r3,LOCAL_VAR(2)(r1)	// save error value
	mflr	r0
	bl	1f
1:	mflr	r3
	mtlr	r0
	addis	r3,r3,ha16(LC2-1b)
	addi	r3,r3,lo16(LC2-1b)
	addi 	r4,r1,LOCAL_VAR(1)
	bl 	__dyld_func_lookup
	lwz	r3,LOCAL_VAR(1)(r1)
	mtspr 	ctr,r3
	bctrl
	lwz	r3,LOCAL_VAR(2)(r1)	// restore error value for cerror
		
#endif
	CALL_EXTERN(cerror)
	/*
	 * We use cthread_fork_parent() to clean up after a fork error
	 * (unlock cthreads and mailloc packages) so the parent
	 * process can Malloc() after fork() errors without
	 * deadlocking.
	 */
	CALL_EXTERN_AGAIN(__cthread_fork_parent)
	li32	r3,-1			// error return
Lreturn:	RETURN
END(_vfork)
#else
#include "SYS.h"

LEAF(_vfork)
#if defined(__DYNAMIC__)
	PICIFY(__current_pid)
	mr	r5,PICIFY_REG
	NON_LAZY_STUB(__current_pid)
#else
	lis	r5,ha16(__current_pid)
	ori	r5,r5,lo16(__current_pid)
#endif
2:
	lwarx	r6,0,r5			// dont cache pid across vfork
	cmpwi	r6,0
	ble-	3f			// is another vfork in progress
	li	r6,0			// if not, erase the stored pid
3:	
	addi	r6,r6,-1		// count the parallel vforks in
	stwcx.	r6,0,r5			// negative cached pid values
	bne-	2b
	
	li	r0,SYS_vfork
	sc
	b	Lbotch			// error return

	cmpwi	r4,0
	beq	Lparent			// parent, since a1 == 0 in parent,

	li	r3,0			// child
	blr

Lparent:
	lwarx	r6,0,r5			// were back, decrement vfork count
	addi	r6,r6,1
	stwcx.	r6,0,r5
	bne-	Lparent
	blr

Lbotch:
	lwarx	r6,0,r5			// never went, decrement vfork count
	addi	r6,r6,1
	stwcx.	r6,0,r5
	bne-	Lbotch

	BRANCH_EXTERN(cerror)
END(_vfork)
#endif

