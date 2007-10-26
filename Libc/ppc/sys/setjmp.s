/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

/* We use mode-independent "g" opcodes such as "stg", and/or
 * mode-independent macros such as MI_GET_ADDRESS.  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>

#include "SYS.h"
#include "_setjmp.h"

/*
 * setjmp  routines
 */

/*	int sigsetjmp(sigjmp_buf env, int savemask); */

MI_ENTRY_POINT(_sigsetjmp)
	cmpgi   cr1,r4,0		; this changes cr1 which is volatile
	stg     r4, JMP_SIGFLAG(r3)	; save the sigflag for use by siglongjmp()
	beq--   cr1, L__exit		; if r4 == 0 do _setjmp()
	; else *** fall through ***  to setjmp()

/*	int setjmp(jmp_buf env); */

MI_ENTRY_POINT(_setjmp)
	mflr    r0
	stg     r31, JMP_r31(r3)
	stg     r0, JMP_lr(r3)
	mr      r31, r3			; save ptr to jmpbuf across calls
	
	/* call sigprocmask() to get signal mask */
	
	li      r3, 1			; get the previous signal mask
	li      r4, 0
	la      r5, JMP_sig(r31)	; get address where previous mask needs to be
	MI_CALL_EXTERNAL(_sigprocmask)	; make a syscall to get mask
	
	/* call sigaltstack() to get SS_ONSTACK flag */
	
	li	r3,0			; ss is NULL
	la	r4,JMP_vr_base_addr(r31); oss is a temp buffer in jmp_buf
	MI_CALL_EXTERNAL(_sigaltstack)	; make a syscall to get current stack state
	la	r4,JMP_vr_base_addr(r31); recreate temp buffer ptr
	lwz	r5,2*GPR_BYTES(r4)	; get ss_flags (an int) from stack_t
	stw	r5,JMP_ss_flags(r31)	; save ss_flags in jmp_buf
	
	mr      r3, r31			; restore jmp_buf ptr
	lg      r0, JMP_lr(r31)
	lg      r31, JMP_r31(r31)
	mtlr    r0
L__exit:
	MI_BRANCH_EXTERNAL(__setjmp)
