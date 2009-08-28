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

/* We use mode-independent "g" opcodes such as "stg", and/or
 * mode-independent macros such as MI_GET_ADDRESS.  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>

#include "_setjmp.h"

/*
 *	longjmp routines
 */

/*	void siglongjmp(sigjmp_buf env, int val); */

MI_ENTRY_POINT(_siglongjmp)
	lg      r0, JMP_SIGFLAG(r3)	; load sigflag saved by siglongjmp()
	cmpgi   cr1,r0,0			; this changes cr1 which is volatile
	mr      r30, r3             ; preserve args across _sigsetmask
   	mr      r31, r4
	beq--   cr1, L__exit        ; if r0 == 0 do _longjmp()
	; else *** fall through *** to longjmp()

/*	void longjmp(jmp_buf env, int val); */

MI_ENTRY_POINT(_longjmp)
	mr      r30, r3             ; preserve args across _sigsetmask
   	mr      r31, r4

    /* NB: this code assumes the signal mask is an int.  Change the "lwz" below
     * if not. The JMP_sig field is already 8 bytes in the jmpbuf.
     */
	lwz     r3, JMP_sig(r3)		; restore the signal mask
	MI_CALL_EXTERNAL(_sigsetmask)   // make a (deprecated!) syscall to set the mask
L__exit:	
	lwz		r3,JMP_ss_flags(r30)
	MI_CALL_EXTERNAL(__sigunaltstack)
L__exit2:
	mr      r3, r30
	mr      r4, r31
	MI_BRANCH_EXTERNAL(__longjmp)






