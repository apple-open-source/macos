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
 *	File: sys/ppc/_longjmp.s
 *
 *	Implements _longjmp()
 *
 *	History:
 *	8 September 1998	Matt Watson (mwatson@apple.com)
 *		Created. Derived from longjmp.s
 */

/* We use mode-independent "g" opcodes such as "lg", and/or
 * mode-independent macros such as MI_CALL_EXTERNAL.  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>

#include "_setjmp.h"

#define __APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#undef  __APPLE_API_PRIVATE

#define	VRSave	256


/*	int _longjmp(jmp_buf env, int val); */
            
MI_ENTRY_POINT(__longjmp)
        lg      r6,JMP_addr_at_setjmp(r3)
        lbz     r7, _COMM_PAGE_ALTIVEC(0)
        cmpg	cr1,r3,r6           ; jmpbuf still at same address?
        cmpwi   cr2,r7,0            ; Altivec available? (using non-volatile cr)
        beq++   cr1,LRestoreVRs     ; jmpbuf has not moved
        
        ; jmp_buf was moved since setjmp (or is uninitialized.)
        ; We must move VRs and FPRs to be quadword aligned at present address.
        
        stg     r3,JMP_addr_at_setjmp(r3) ; update, in case we longjmp to this again
        mr      r31,r4              ; save "val" arg across memmove
        mr      r30,r3              ; and jmp_buf ptr
        addi	r3,r3,JMP_vr_base_addr
        addi	r4,r6,JMP_vr_base_addr
        clrrgi  r3,r3,4             ; r3 <- QW aligned addr where they should be
        clrrgi  r4,r4,4             ; r4 <- QW aligned addr where they originally were
        sub     r7,r4,r6            ; r7 <- offset of VRs/FPRs within jmp_buf
        add     r4,r30,r7           ; r4 <- where they are now
        li	r5,(JMP_buf_end - JMP_vr_base_addr)
        
        MI_CALL_EXTERNAL(_memmove)

        mr      r3,r30              ; restore parameters
        mr      r4,r31
        
        ; Restore VRs iff any
        ;	cr2 - beq if AltiVec not available
        
LRestoreVRs:
        lg      r0,JMP_vrsave(r3)   ; get VRSAVE at setjmp()
        addi	r6,r3,JMP_vr_base_addr
        beq--   cr2,LRestoreFPRs    ; AltiVec not available so skip
        cmpwi	r0,0                ; any live VRs?
        mtspr	VRSave,r0           ; update VRSAVE whether 0 or not
        beq++	LRestoreFPRs        ; VRSAVE is 0 so no VRs to reload
        lvx     v20,0,r6
        li      r7,16*1
        lvx     v21,r7,r6
        li      r7,16*2
        lvx     v22,r7,r6
        li      r7,16*3
        lvx     v23,r7,r6
        li      r7,16*4
        lvx     v24,r7,r6
        li      r7,16*5
        lvx     v25,r7,r6
        li      r7,16*6
        lvx     v26,r7,r6
        li      r7,16*7
        lvx     v27,r7,r6
        li      r7,16*8
        lvx     v28,r7,r6
        li      r7,16*9
        lvx     v29,r7,r6
        li      r7,16*10
        lvx     v30,r7,r6
        li      r7,16*11
        lvx     v31,r7,r6
        
        ; Restore FPRs
        
LRestoreFPRs:
        addi	r6,r3,JMP_fp_base_addr
        lfd     f0,JMP_fpscr(r3)    ; get FPSCR from non-sliding section of jmpbuf
        clrrgi	r6,r6,4             ; mask off low 4 bits to qw align
        lfd     f14,0*8(r6)
        lfd     f15,1*8(r6)
        lfd     f16,2*8(r6)
        lfd     f17,3*8(r6)
        lfd     f18,4*8(r6)
        lfd     f19,5*8(r6)
        lfd     f20,6*8(r6)
        lfd     f21,7*8(r6)
        lfd     f22,8*8(r6)
        lfd     f23,9*8(r6)
        lfd     f24,10*8(r6)
        lfd     f25,11*8(r6)
        lfd     f26,12*8(r6)
        lfd     f27,13*8(r6)
        lfd     f28,14*8(r6)
        lfd     f29,15*8(r6)
        lfd     f30,16*8(r6)
        lfd     f31,17*8(r6)
        mtfsf   0xFF,f0             ; restore entire FPSCR
        
        ; Restore GPRs
        
        lg      r5,  JMP_cr(r3)     ; r5 <- CR
        lg      r6,  JMP_lr(r3)     ; r6 <- LR (ie, return addres)
        cmplgi  r4,0                ; is return value 0? (not permitted)
        lg      r1,  JMP_r1 (r3)
        lg      r2,  JMP_r2 (r3)
        lg      r13, JMP_r13(r3)
        lg      r14, JMP_r14(r3)
        lg      r15, JMP_r15(r3)
        lg      r16, JMP_r16(r3)
        lg      r17, JMP_r17(r3)
        mtcrf   0x20,r5             ; restore cr2 (we only restore non-volatile CRs)
        lg      r18, JMP_r18(r3)
        lg      r19, JMP_r19(r3)
        lg      r20, JMP_r20(r3)
        lg      r21, JMP_r21(r3)
        mtctr   r6                  ; set up return address, avoiding LR since it will mispredict
        lg      r22, JMP_r22(r3)
        lg      r23, JMP_r23(r3)
        lg      r24, JMP_r24(r3)
        lg      r25, JMP_r25(r3)
        mtcrf   0x10,r5             ; restore cr3
        lg      r26, JMP_r26(r3)
        lg      r27, JMP_r27(r3)
        lg      r28, JMP_r28(r3)
        lg      r29, JMP_r29(r3)
        mtcrf   0x08,r5             ; restore cr4
        lg      r30, JMP_r30(r3)
        lg      r31, JMP_r31(r3)
        mr      r3,r4               ; move return code into position (cr0 is set on r4)
        bnectr++                    ; return code was not 0
        li      r3, 1               ; cannot return zero from longjmp(), so return 1 instead
        bctr

