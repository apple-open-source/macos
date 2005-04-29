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

/*	int _setjmp(jmp_buf env); */
/*
 * Copyright (c) 1998 Apple Computer, Inc. All rights reserved.
 *
 *	File: sys/ppc/_setjmp.s
 *
 *	Implements _setjmp()
 *
 *	History:
 *	8 September 1998	Matt Watson (mwatson@apple.com)
 *		Created. Derived from setjmp.s
 */

/* We use mode-independent "g" opcodes such as "stg", and/or
 * mode-independent macros such as MI_GET_ADDRESS.  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>

#define __APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#undef  __APPLE_API_PRIVATE

#include "_setjmp.h"

#define	VRSave	256


MI_ENTRY_POINT(__setjmp)
        lbz     r7, _COMM_PAGE_ALTIVEC(0)   ; get "AltiVec available" flag

        stg     r1, JMP_r1(r3)
        stg     r2, JMP_r2(r3)
        stg     r13, JMP_r13(r3)
        stg     r14, JMP_r14(r3)
        stg     r15, JMP_r15(r3)
        stg     r16, JMP_r16(r3)
        stg     r17, JMP_r17(r3)
        stg     r18, JMP_r18(r3)
        stg     r19, JMP_r19(r3)
        mfcr    r0                      ; we only need to save cr2-cr4
        stg     r20, JMP_r20(r3)
        stg     r21, JMP_r21(r3)
        stg     r22, JMP_r22(r3)
        stg     r23, JMP_r23(r3)
        stg     r24, JMP_r24(r3)
        mflr    r5
        stg     r25, JMP_r25(r3)
        stg     r26, JMP_r26(r3)
        stg     r27, JMP_r27(r3)
        stg     r28, JMP_r28(r3)
        stg     r29, JMP_r29(r3)
        stg     r30, JMP_r30(r3)
        stg     r31, JMP_r31(r3)
        stg     r0, JMP_cr(r3)
        stg     r5, JMP_lr(r3)

        addi	r6,r3,JMP_fp_base_addr  ; point to base of FPR save area
        stg     r3,JMP_addr_at_setjmp(r3)   ; remember original address of jmpbuf
        clrrgi	r6,r6,4                 ; mask off low 4 bits to qw align
        mffs    f0                      ; get FPSCR
        stfd	f14,0*8(r6)
        stfd	f15,1*8(r6)
        stfd	f16,2*8(r6)
        stfd	f17,3*8(r6)
        stfd	f18,4*8(r6)
        stfd	f19,5*8(r6)
        stfd	f20,6*8(r6)
        stfd	f21,7*8(r6)
        stfd	f22,8*8(r6)
        stfd	f23,9*8(r6)
        stfd	f24,10*8(r6)
        stfd	f25,11*8(r6)
        stfd	f26,12*8(r6)
        stfd	f27,13*8(r6)
        stfd	f28,14*8(r6)
        stfd	f29,15*8(r6)
        stfd	f30,16*8(r6)
        stfd	f31,17*8(r6)
        stfd    f0,JMP_fpscr(r3)        ; save fpscr in non-sliding region of jmpbuf
        
        cmpwi   r7,0                    ; is AltiVec available? (test _COMM_PAGE_ALTIVEC)
        addi	r6,r3,JMP_vr_base_addr  ; get base address of VR save area
        addi    r8,r3,JMP_vrsave        ; we'll need this address below
        li      r3,0                    ; set return value (always 0 on setjmp)
        beqlr--                         ; exit if no Altivec
        
        mfspr	r4,VRSave               ; AltiVec available, so get VRSAVE mask
        andi.	r0,r4,0xFFF             ; we only care about v20-v31
        stg     r0,0(r8)                ; save effective VRSAVE in JMP_vrsave
        beqlr++                         ; if no live non-volatile VRs, we're done

        stvx	v20,0,r6
        li      r4,16*1
        stvx	v21,r4,r6
        li      r4,16*2
        stvx	v22,r4,r6
        li      r4,16*3
        stvx	v23,r4,r6
        li      r4,16*4
        stvx	v24,r4,r6
        li      r4,16*5
        stvx	v25,r4,r6
        li      r4,16*6
        stvx	v26,r4,r6
        li      r4,16*7
        stvx	v27,r4,r6
        li      r4,16*8
        stvx	v28,r4,r6
        li      r4,16*9
        stvx	v29,r4,r6
        li      r4,16*10
        stvx	v30,r4,r6
        li      r4,16*11
        stvx	v31,r4,r6

        blr

