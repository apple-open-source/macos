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
/*
 * Copyright (c) 1998 Apple Computer, Inc. All rights reserved.
 *
 *	File: sys/ppc/setjmp.s
 *
 *	Implements sigsetjmp(), setjmp(), _setjmp()
 *
 *	NOTE:	Scatterloading this file will BREAK the functions.
 *
 *	History:
 *	30-Aug-1998	Umesh Vaishampayan	(umeshv@apple.com)
 *		Created. Derived from _setjmp.s, setjmp.c and setjmp.s
 */

#include "SYS.h"
#include <architecture/ppc/asm_help.h>
#include "_setjmp.h"

/*
 * setjmp  routines
 */

/*	int sigsetjmp(sigjmp_buf env, int savemask); */

LEAF(_sigsetjmp)
	cmpwi cr1,r4,0			; this changes cr1[EQ] which is volatile
	stw r4, JMP_SIGFLAG(r3)	; save the sigflag for use by siglongjmp()
	beq- cr1, L__setjmp		; if r4 == 0 do _setjmp()
	; else *** fall through ***  to setjmp()

/*	int setjmp(jmp_buf env); */

LEAF(_setjmp)
L_setjmp:
	mflr r0
	stw r31, JMP_r31(r3)
	stw r0, JMP_lr(r3)
	mr r31, r3
	li r3, 1				; get the previous signal mask
	li r4, 0
	la r5, JMP_sig(r31)	; get address where previous mask needs to be
	CALL_EXTERN(_sigprocmask)
	mr r3, r31
	lwz r0, JMP_lr(r3)
	mtlr r0
	lwz r31, JMP_r31(r3)
L__setjmp:
	BRANCH_EXTERN(__setjmp)
