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

/*	void _longjmp(jmp_buf env, int val); */

/*	int _longjmp(jmp_buf env); */
/*
 * Copyright (c) 1998 Apple Computer, Inc. All rights reserved.
 *
 *	File: sys/ppc/_longjmp.s
 *
 *	Implements _longjmp()
 *
 *	History:
 *	8 September 1998	Matt Watson (mwatson@apple.com)
 *		Created. Derived from longjmp.s
 */
#include "SYS.h"
#include <architecture/ppc/asm_help.h>
#include "_setjmp.h"

LEAF(__longjmp)
	lwz r31, JMP_r31(r3)
	/* r1, r14-r30 */
	lwz r1,  JMP_r1 (r3)
	lwz r2,  JMP_r2 (r3)
	lwz r13, JMP_r13(r3)
	lwz r14, JMP_r14(r3)
	lwz r15, JMP_r15(r3)
	lwz r16, JMP_r16(r3)
	lwz r17, JMP_r17(r3)
	lwz r18, JMP_r18(r3)
	lwz r19, JMP_r19(r3)
	lwz r20, JMP_r20(r3)
	lwz r21, JMP_r21(r3)
	lwz r22, JMP_r22(r3)
	lwz r23, JMP_r23(r3)
	lwz r24, JMP_r24(r3)
	lwz r25, JMP_r25(r3)
	lwz r26, JMP_r26(r3)
	lwz r27, JMP_r27(r3)
	lwz r28, JMP_r28(r3)
	lwz r29, JMP_r29(r3)
	lwz r30, JMP_r30(r3)
	lwz r0, JMP_cr(r3)
	mtcrf 0xff,r0
	lwz r0, JMP_lr(r3)
	mtlr r0
	lwz r0, JMP_ctr(r3)			; XXX	ctr is volatile
	mtctr r0
	lwz r0, JMP_xer(r3)			; XXX	xer is volatile
	mtxer r0
	mr. r3, r4
	bnelr
	li  r3, 1
	blr

