/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#ifdef __ppc__
/*
 * ppc_fp_save() and ppc_fp_restore() is passed a pointer to a buffer of 14
 * doubles into which the floating point registers are saved and restored.
 */
	.text
	.align 2
	.globl _ppc_fp_save
_ppc_fp_save:
	stfd	f0,0(r3)	; save all registers that could contain
	stfd	f1,8(r3)	;  parameters to the routine that is being
	stfd	f2,16(r3)	;  bound.
	stfd	f3,24(r3)
	stfd	f4,32(r3)
	stfd	f5,40(r3)
	stfd	f6,48(r3)
	stfd	f7,56(r3)
	stfd	f8,64(r3)
	stfd	f9,72(r3)
	stfd	f10,80(r3)
	stfd	f11,88(r3)
	stfd	f12,96(r3)
	stfd	f13,104(r3)
	blr

	.text
	.align 2
	.globl _ppc_fp_restore
_ppc_fp_restore:
	lfd	f0,0(r3)	; restore all registers that could contain
	lfd	f1,8(r3)	;  parameters to the routine that is being
	lfd	f2,16(r3)	;  bound.
	lfd	f3,24(r3)
	lfd	f4,32(r3)
	lfd	f5,40(r3)
	lfd	f6,48(r3)
	lfd	f7,56(r3)
	lfd	f8,64(r3)
	lfd	f9,72(r3)
	lfd	f10,80(r3)
	lfd	f11,88(r3)
	lfd	f12,96(r3)
	lfd	f13,104(r3)
	blr
	
#define VRSAVE_SUPPORTED
/*
 * ppc_vec_save() and ppc_vec_restore() is passed a pointer to a buffer of 18
 * 128 bit vectors (16 byte aligned) into which the vector registers are saved
 * and restored.
 */
	.text
	.align 2
	.globl _ppc_vec_save
_ppc_vec_save:
#ifdef VRSAVE_SUPPORTED
	mfspr	r8,VRsave	; get the VRsave register
				; Extract just the bits for registers v2-v19
	rlwinm.	r8,r8,0,2,19	;  0011 1111 1111 1111 1111 0000 0000 0000
				;  and test for none of these vector registers
				;  in use.
	beqlr			; if none of these vector regs in use return
	mfcr	r7		; save the CR
	mtcr	r8		; move the VRsave register to the CR

	li	r4,0
	li	r5,16
	li	r6,32
	bf	2,Ls2
	stvx	v2,r4,r3
Ls2:	addi    r4,r4,48
	bf	3,Ls3
	stvx	v3,r5,r3
Ls3:	addi    r5,r5,48
	bf	4,Ls4
	stvx	v4,r6,r3
Ls4:	addi    r6,r6,48

	bf	5,Ls5
	stvx	v5,r4,r3
Ls5:	addi	r4,r4,48
	bf	6,Ls6
	stvx	v6,r5,r3
Ls6:	addi	r5,r5,48
	bf	7,Ls7
	stvx	v7,r6,r3
Ls7:	addi	r6,r6,48

	bf	8,Ls8
	stvx	v8,r4,r3
Ls8:	addi	r4,r4,48
	bf	9,Ls9
	stvx	v9,r5,r3
Ls9:	addi	r6,r6,48
	bf	10,Ls10
	stvx	v10,r6,r3
Ls10:	addi	r5,r5,48

	bf	11,Ls11
	stvx	v11,r4,r3
Ls11:	addi	r4,r4,48
	bf	12,Ls12
	stvx	v12,r5,r3
Ls12:	addi	r5,r5,48
	bf	13,Ls13
	stvx	v13,r6,r3
Ls13:	addi	r6,r6,48

	bf	14,Ls14
	stvx	v14,r4,r3
Ls14:	addi	r4,r4,48
	bf	15,Ls15
	stvx	v15,r5,r3
Ls15:	addi	r5,r5,48
	bf	16,Ls16
	stvx	v16,r6,r3
Ls16:	addi	r6,r6,48

	bf	17,Ls17
	stvx	v17,r4,r3
Ls17:	bf	18,Ls18
	stvx	v18,r5,r3
Ls18:	bf	19,Ls19
	stvx	v19,r6,r3
Ls19:
	mtcr	r7		; restore the CR
Leave_ppc_vec_save:
	blr

#else /* VRSAVE_ not SUPPORTED */
	li	r4,0
	li	r5,16
	li	r6,32
	stvx	v2,r4,r3
	stvx	v3,r5,r3
	stvx	v4,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v5,r4,r3
	stvx	v6,r5,r3
	stvx	v7,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v8,r4,r3
	stvx	v9,r5,r3
	stvx	v10,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v11,r4,r3
	stvx	v12,r5,r3
	stvx	v13,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v14,r4,r3
	stvx	v15,r5,r3
	stvx	v16,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v17,r4,r3
	stvx	v18,r5,r3
	stvx	v19,r6,r3

	blr
#endif /* VRSAVE_SUPPORTED */

	.text
	.align 2
	.globl _ppc_vec_restore
_ppc_vec_restore:
#ifdef VRSAVE_SUPPORTED
	mfspr	r8,VRsave	; get the VRsave register
				; Extract just the bits for registers v2-v19
	rlwinm.	r8,r8,0,2,19	;  0011 1111 1111 1111 1111 0000 0000 0000
				;  and test for none of these vector registers
				;  in use.
	beqlr			; if none of these vector regs in use return
	mfcr	r7		; save the CR
	mtcr	r8		; move the VRsave register to the CR

	li	r4,0
	li	r5,16
	li	r6,32
	bf	2,Lr2
	lvx	v2,r4,r3
Lr2:	addi	r4,r4,48
	bf	3,Lr3
	lvx	v3,r5,r3
Lr3:	addi	r5,r5,48
	bf	4,Lr4
	lvx	v4,r6,r3
Lr4:	addi	r6,r6,48

	bf	5,Lr5
	lvx	v5,r4,r3
Lr5:	addi	r4,r4,48
	bf	6,Lr6
	lvx	v6,r5,r3
Lr6:	addi	r5,r5,48
	bf	7,Lr7
	lvx	v7,r6,r3
Lr7:	addi	r6,r6,48

	bf	8,Lr8
	lvx	v8,r4,r3
Lr8:	addi	r4,r4,48
	bf	9,Lr9
	lvx	v9,r5,r3
Lr9:	addi	r5,r5,48
	bf	10,Lr10
	lvx	v10,r6,r3
Lr10:	addi	r6,r6,48

	bf	11,Lr11
	lvx	v11,r4,r3
Lr11:	addi	r4,r4,48
	bf	12,Lr12
	lvx	v12,r5,r3
Lr12:	addi	r5,r5,48
	bf	13,Lr13
	lvx	v13,r6,r3
Lr13:	addi	r6,r6,48

	bf	14,Lr14
	lvx	v14,r4,r3
Lr14:	addi	r4,r4,48
	bf	15,Lr15
	lvx	v15,r5,r3
Lr15:	addi	r5,r5,48
	bf	16,Lr16
	lvx	v16,r6,r3
Lr16:	addi	r6,r6,48

	bf	17,Lr17
	stvx	v17,r4,r3
Lr17:	bf	18,Lr18
	stvx	v18,r5,r3
Lr18:	bf	19,Lr19
	stvx	v19,r6,r3
Lr19:
	mtcr	r7		; restore the CR
Leave_ppc_vec_restore:
	blr

#endif /* VRSAVE_SUPPORTED */

	li	r4,0
	li	r5,16
	li	r6,32
	lvx	v2,r4,r3
	lvx	v3,r5,r3
	lvx	v4,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	lvx	v5,r4,r3
	lvx	v6,r5,r3
	lvx	v7,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	lvx	v8,r4,r3
	lvx	v9,r5,r3
	lvx	v10,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	lvx	v11,r4,r3
	lvx	v12,r5,r3
	lvx	v13,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	lvx	v14,r4,r3
	lvx	v15,r5,r3
	lvx	v16,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v17,r4,r3
	stvx	v18,r5,r3
	stvx	v19,r6,r3
	blr
#end /* VRSAVE_SUPPORTED */
#endif /* __ppc__ */
