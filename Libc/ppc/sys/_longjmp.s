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

#include <architecture/ppc/asm_help.h>
#include "_setjmp.h"

#define	VRSave	256

/* special flag bit definitions copied from /osfmk/ppc/thread_act.h */

#define floatUsedbit	1
#define vectorUsedbit	2


#if defined(__DYNAMIC__)
        .data
	.non_lazy_symbol_pointer
	.align 2
L_memmove$non_lazy_ptr:
	.indirect_symbol _memmove
	.long 0
	.non_lazy_symbol_pointer
	.align 2
L__cpu_has_altivec$non_lazy_ptr:
	.indirect_symbol __cpu_has_altivec
	.long 0
        .text
#endif        
        
LEAF(__longjmp)

        ; need to restore FPRs or VRs?
        
        lwz	r5,JMP_flags(r3)
        lwz	r6,JMP_addr_at_setjmp(r3)
        rlwinm	r7,r5,0,vectorUsedbit,vectorUsedbit
        rlwinm	r8,r5,0,floatUsedbit,floatUsedbit
        cmpw	cr1,r3,r6		; jmp_buf still at same address?
        cmpwi	cr3,r7,0		; set cr3 iff VRs in use (non-volatile CR)
        cmpwi	cr4,r8,0		; set cr4 iff FPRs in use (non-volatile CR)
        beq+	cr1,LRestoreVRs
        
        ; jmp_buf was moved since setjmp (or is uninitialized.)
        ; We must move VRs and FPRs to be quadword aligned at present address.
        
        stw	r3,JMP_addr_at_setjmp(r3) ; update, in case we longjmp to this again
        mr	r31,r4			; save "val" arg across memmove
        mr	r30,r3			; and jmp_buf ptr
        addi	r3,r3,JMP_vr_base_addr
        addi	r4,r6,JMP_vr_base_addr
        rlwinm	r3,r3,0,0,27		; r3 <- QW aligned addr where they should be
        rlwinm	r4,r4,0,0,27		; r4 <- QW aligned addr where they originally were
        sub	r7,r4,r6		; r7 <- offset of VRs/FPRs within jmp_buf
        add	r4,r30,r7		; r4 <- where they are now
        li	r5,(JMP_buf_end - JMP_vr_base_addr)
#if defined(__DYNAMIC__)
        bcl     20,31,1f		; Get pic-base
1:      mflr    r12			
        addis   r12, r12, ha16(L_memmove$non_lazy_ptr - 1b)
        lwz     r12, lo16(L_memmove$non_lazy_ptr - 1b)(r12)
        mtctr   r12			; Get address left by dyld
        bctrl
#else
	bl	_memmove
#endif
        mr	r3,r30
        mr	r4,r31
        
        ; Restore VRs iff any
        ;	cr3 - bne if VRs
        ;	cr4 - bne if FPRs
        
LRestoreVRs:
        beq+	cr3,LZeroVRSave		; no VRs
        lwz	r0,JMP_vrsave(r3)
        addi	r6,r3,JMP_vr_base_addr
        cmpwi	r0,0			; any live VRs?
        mtspr	VRSave,r0
        beq+	LRestoreFPRs
        lvx	v20,0,r6
        li	r7,16*1
        lvx	v21,r7,r6
        li	r7,16*2
        lvx	v22,r7,r6
        li	r7,16*3
        lvx	v23,r7,r6
        li	r7,16*4
        lvx	v24,r7,r6
        li	r7,16*5
        lvx	v25,r7,r6
        li	r7,16*6
        lvx	v26,r7,r6
        li	r7,16*7
        lvx	v27,r7,r6
        li	r7,16*8
        lvx	v28,r7,r6
        li	r7,16*9
        lvx	v29,r7,r6
        li	r7,16*10
        lvx	v30,r7,r6
        li	r7,16*11
        lvx	v31,r7,r6
        b	LRestoreFPRs		; skip zeroing VRSave
        
        ; Zero VRSave iff Altivec is supported, but VRs were not in use
        ; at setjmp time.  This covers the case where VRs are first used after
        ; the setjmp but before the longjmp, and where VRSave is nonzero at
        ; the longjmp.  We need to zero it now, or it will always remain
        ; nonzero since they are sticky bits.

LZeroVRSave:
#if defined(__DYNAMIC__)
        bcl	20,31,1f
1:	mflr	r9			; get our address
        addis	r6,r9,ha16(L__cpu_has_altivec$non_lazy_ptr - 1b)
        lwz	r7,lo16(L__cpu_has_altivec$non_lazy_ptr - 1b)(r6)
        lwz	r7,0(r7)		; load the flag
#else
        lis	r7, ha16(__cpu_has_altivec)
	lwz	r7, lo16(__cpu_has_altivec)(r7)
#endif
	cmpwi	r7,0
        li	r8,0
        beq	LRestoreFPRs		; no Altivec, so skip
        mtspr	VRSave,r8
        
        ; Restore FPRs if any
        ;	cr4 - bne iff FPRs
        
LRestoreFPRs:
        beq	cr4,LRestoreGPRs	; FPRs not in use at setjmp
        addi	r6,r3,JMP_fp_base_addr
        rlwinm	r6,r6,0,0,27		; mask off low 4 bits to qw align
        lfd	f14,0*8(r6)
        lfd	f15,1*8(r6)
        lfd	f16,2*8(r6)
        lfd	f17,3*8(r6)
        lfd	f18,4*8(r6)
        lfd	f19,5*8(r6)
        lfd	f20,6*8(r6)
        lfd	f21,7*8(r6)
        lfd	f22,8*8(r6)
        lfd	f23,9*8(r6)
        lfd	f24,10*8(r6)
        lfd	f25,11*8(r6)
        lfd	f26,12*8(r6)
        lfd	f27,13*8(r6)
        lfd	f28,14*8(r6)
        lfd	f29,15*8(r6)
        lfd	f30,16*8(r6)
        lfd	f31,17*8(r6)
        
        ; Restore GPRs
        
LRestoreGPRs:
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

