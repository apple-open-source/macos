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
/* Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved.
 *
 *	File:	libc/ppc/sys/fork.s
 *
 * HISTORY
 * 18-Nov-92  Ben Fathi (benf@next.com)
 *	Created from M88K sources
 *
 * 11-Jan-92  Peter King (king@next.com)
 *	Created from M68K sources
 */

#import <sys/syscall.h>
#import <architecture/ppc/asm_help.h>
#import	<architecture/ppc/pseudo_inst.h>
#import	<mach/ppc/syscall_sw.h>

/* We use 8 bytes for LOCAL_VAR(1) and LOCAL_VAR(2) */
NESTED(_fork, 8, 0, 0, 0)
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
	li	r0,SYS_fork
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
	li	r3,0x0			// clear cached pid in child
	REG_TO_EXTERN(r3,__current_pid)
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
END(_fork)

