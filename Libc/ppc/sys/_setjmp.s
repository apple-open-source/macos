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


#include <architecture/ppc/asm_help.h>
#include "_setjmp.h"

#define	VRSave	256

/* special flag bit definitions copied from /osfmk/ppc/thread_act.h */

#define floatUsedbit	1
#define vectorUsedbit	2

#define	FlagsFastTrap	0x7FF3


LEAF(__setjmp)
	stw r31, JMP_r31(r3)
	/* r1, r2, r13-r30 */
	stw r1, JMP_r1(r3)
	stw r2, JMP_r2(r3)
	stw r13, JMP_r13(r3)
	stw r14, JMP_r14(r3)
	stw r15, JMP_r15(r3)
	stw r16, JMP_r16(r3)
	stw r17, JMP_r17(r3)
	stw r18, JMP_r18(r3)
	stw r19, JMP_r19(r3)
	stw r20, JMP_r20(r3)
	stw r21, JMP_r21(r3)
	stw r22, JMP_r22(r3)
	mfcr r0
	stw r23, JMP_r23(r3)
	stw r24, JMP_r24(r3)
	mflr r5
	stw r25, JMP_r25(r3)
	stw r26, JMP_r26(r3)
	mfctr r6				; XXX	ctr is volatile
	stw r27, JMP_r27(r3)
	stw r28, JMP_r28(r3)
	mfxer r7				; XXX	xer is volatile
	stw r29, JMP_r29(r3)
	stw r30, JMP_r30(r3)
	stw r0, JMP_cr(r3)
	stw r5, JMP_lr(r3)
	stw r6, JMP_ctr(r3)
	stw r7, JMP_xer(r3)
        
        mr	r31,r3				; save jmp_buf ptr
        li	r0,FlagsFastTrap
        sc					; get FPR-inuse and VR-inuse flags from kernel
        rlwinm	r4,r3,0,floatUsedbit,floatUsedbit
        rlwinm.	r5,r3,0,vectorUsedbit,vectorUsedbit
        cmpwi	cr1,r4,0			; set CR1 bne iff FPRs in use
        stw	r3,JMP_flags(r31)
        stw	r31,JMP_addr_at_setjmp(r31)
        mr	r3,r31				; restore jmp_buf ptr
        lwz	r31,JMP_r31(r31)
        beq	LSaveFPRsIfNecessary		; skip if vectorUsedbit was 0
        
        ; must save VRs and VRSAVE
        
        mfspr	r4,VRSave
        andi.	r0,r4,0xFFF			; we only care about v20-v31
        stw	r0,JMP_vrsave(r3)		; set up effective VRSAVE
        beq	LSaveFPRsIfNecessary		; no live non-volatile VRs
        addi	r6,r3,JMP_vr_base_addr
        stvx	v20,0,r6
        li	r4,16*1
        stvx	v21,r4,r6
        li	r4,16*2
        stvx	v22,r4,r6
        li	r4,16*3
        stvx	v23,r4,r6
        li	r4,16*4
        stvx	v24,r4,r6
        li	r4,16*5
        stvx	v25,r4,r6
        li	r4,16*6
        stvx	v26,r4,r6
        li	r4,16*7
        stvx	v27,r4,r6
        li	r4,16*8
        stvx	v28,r4,r6
        li	r4,16*9
        stvx	v29,r4,r6
        li	r4,16*10
        stvx	v30,r4,r6
        li	r4,16*11
        stvx	v31,r4,r6
        
        ; must save FPRs if they are live in this thread
        ;	CR1 = bne iff FPRs are in use
        
LSaveFPRsIfNecessary:
        beq	cr1,LExit			; FPRs not in use
        addi	r6,r3,JMP_fp_base_addr
        rlwinm	r6,r6,0,0,27			; mask off low 4 bits to qw align
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

LExit:
	li 	r3, 0
	blr

