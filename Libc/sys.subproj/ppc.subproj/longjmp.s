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
/*
 * Copyright (c) 1998 Apple Computer, Inc. All rights reserved.
 *
 *	File: sys/ppc/longjmp.s
 *
 *	Implements siglongjmp(), longjmp(), _longjmp() 
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
 *	longjmp routines
 */

/*	void siglongjmp(sigjmp_buf env, int val); */

LEAF(_siglongjmp)
	lwz r0, JMP_SIGFLAG(r3)	; load sigflag saved by siglongjmp()
	cmpwi cr1,r0,0			; this changes cr1[EQ] which is volatile
	beq- cr1, L__longjmp	; if r0 == 0 do _longjmp()
	; else *** fall through *** to longjmp()

/*	void longjmp(jmp_buf env, int val); */

LEAF(_longjmp)
L_longjmp:
	mr r30, r3
	mr r31, r4
	lwz r3, JMP_sig(r3)		; restore the signal mask
	CALL_EXTERN(_sigsetmask)
	mr r4, r31
	mr r3, r30
L__longjmp:
	BRANCH_EXTERN(__longjmp)
