/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * This is the interface for the stub_binding_helper for the m68k and i386:
 * The caller has pushed the address of the a lazy pointer to be filled in with
 * the value for the defined symbol and pushed the address of the the mach
 * header this pointer comes from.
 *
 * sp+4	address of lazy pointer
 * sp+0	address of mach header
 *
 * After the symbol has been resolved and the pointer filled in this is to pop
 * these arguments off the stack and jump to the address of the defined symbol.
 */
#ifdef m68k
	.text
	.even
	.globl _stub_binding_helper_interface
_stub_binding_helper_interface:
	/*
	 * a1 is a pointer to an area to return structures and needs to be
	 * saved.  Since this wasn't accounted for we are forced to save it
	 * here and shuffle the arguments.
	 */
	subl	#12,sp
	movl	a1,sp@(8)
	movl	sp@(16),sp@(4)
	movl	sp@(12),sp@
	bsr	_bind_lazy_symbol_reference
	movl	sp@(8),a1
	addl	#20,sp
	movl	d0,a0
	jmp	a0@
#endif /* m68k */

#ifdef __i386__
	.text
	.align 4,0x90
	.globl _stub_binding_helper_interface
_stub_binding_helper_interface:
	call	_bind_lazy_symbol_reference
	addl	$8,%esp
	jmpl	%eax
#endif /* __i386__ */

#ifdef hppa
/*
 * This is the interface for the stub_binding_helper for the hppa:
 * The caller has placed in r21 the address of the a lazy pointer to be filled
 * in with the value for the defined symbol and placed in r22 the address of
 * the the mach header this pointer comes from.
 * 
 * r21 address of lazy pointer
 * r22 address of mach header
 */
	.text
	.align 2
	.globl _stub_binding_helper_interface
_stub_binding_helper_interface:
	ldo	64(%r30),%r30
	stw	%r2,-20(0,%r30)	; store return pointer in previous frame
	copy	%r4,%r1		; move old frame pointer into r1
	copy	%r30,%r4	; move stack pointer in frame pointer
	stwm	%r1,128(0,%r30)	; store old frame pointer and move stack pointer
	stw	%r26,-36(0,%r4)	; save argument register 0
	stw	%r25,-40(0,%r4)	; save argument register 1
	stw	%r24,-44(0,%r4)	; save argument register 2
	stw	%r23,-48(0,%r4)	; save argument register 3
	stw	%r28,-52(0,%r30); save structure return addess
	stw	%r31,-56(0,%r30); save millicode return addess
	stw	%r20,-60(0,%r30)
	stw	%r27,-64(0,%r30)
	stw	%r29,-68(0,%r30)

	; call bind_lazy_symbol_reference(mh, lazy_symbol_pointer_address)
	copy	%r22,%r26	; copy address of mach header to argument reg 0
	bl	_bind_lazy_symbol_reference,%r2
	copy	%r21,%r25	; copy address of lazy pointer to argument reg 1

	copy	%r28,%r1	; copy return value of
				;  bind_lazy_symbol_reference() into r1
	ldw	-68(0,%r30),%r29
	ldw	-64(0,%r30),%r27
	ldw	-60(0,%r30),%r20
	ldw	-56(0,%r30),%r31; restore millicode return addess
	ldw	-52(0,%r30),%r28; restore structure return addess
	ldw	-48(0,%r4),%r23	; restore argument register 3
	ldw	-44(0,%r4),%r24	; restore argument register 2
	ldw	-40(0,%r4),%r25	; restore argument register 1
	ldw	-36(0,%r4),%r26	; restore argument register 0
	ldwm	-128(0,%r30),%r4; restore frame pointer and move stack pointer
	ldw	-20(0,%r30),%r2	; restore return pointer from previous frame
	bv	0(%r1)		; jump to the symbol`s address that was bound
	ldo	-64(%r30),%r30
#endif /* hppa */

#ifdef sparc
/*
 * This is the interface for the stub_binding_helper for the sparc:
 * The caller has placed in %g5 the address of the lazy pointer to be filled
 * in with the value for the defined symbol and placed in %g6 the address of
 * the mach header this pointer comes from.
 * %g4 contains a saved copy of the return pointer to the caller and is
 * saved here locally as %g4 will get garbled during binding 
 * %o0 address of mach header
 * %o1 address of lazy pointer
 */
	.text
	.align 2
	.globl _stub_binding_helper_interface
_stub_binding_helper_interface:
	save	%sp,-96,%sp
	mov	%g4,%l0			! save the return ptr
	mov	%g6,%o0			! mach header
	call	_bind_lazy_symbol_reference
	mov	%g5,%o1			! lazy sym to bind
	jmp	%o0			! ptr to bound sym
	restore %l0,%g0,%o7		! restore ret ptr for caller

#endif /* sparc */

#ifdef __ppc__
/*
 * This is the interface for the stub_binding_helper for the ppc:
 * The caller has placed in r11 the address of the a lazy pointer to be filled
 * in with the value for the defined symbol and placed in r12 the address of
 * the the mach header this pointer comes from.
 *
 * r11 address of lazy pointer
 * r12 address of mach header
 */
	.text
	.align 2
	.globl _stub_binding_helper_interface
_stub_binding_helper_interface:
	mflr	r0		; get link register value
	stw	r31,-4(r1)	; save r31 (frame pointer) in reg save area
	stw	r0,8(r1)	; save link register value in the linkage area
	stwu	r1,-128(r1)	; save stack pointer and update it
	mr	r31,r1		; set new frame pointer from stack pointer

	stw	r3,56(r1)	; save all registers that could contain
	stw	r4,60(r1)	;  parameters to the routine that is being
	stw	r5,64(r1)	;  bound.
	stw	r6,68(r1)
	stw	r7,72(r1)
	stw	r8,76(r1)
	stw	r9,80(r1)
	stw	r10,84(r1)

	mr	r3,r12		; move address of mach header to 1st parameter
	mr	r4,r11		; move address of lazy pointer to 2nd parameter
	; call bind_lazy_symbol_reference(mh, lazy_symbol_pointer_address)
	bl	_bind_lazy_symbol_reference
	mr	r12,r3		; move the symbol`s address into r12
	mtctr	r12		; move the symbol`s address into count register

	lwz	r0,136(r1)	; get old link register value

	lwz	r3,56(r1)	; restore all registers that could contain
	lwz	r4,60(r1)	;  parameters to the routine that was bound.
	lwz	r5,64(r1)
	lwz	r6,68(r1)
	lwz	r7,72(r1)
	lwz	r8,76(r1)
	lwz	r9,80(r1)
	lwz	r10,84(r1)

	addi	r1,r1,128	; restore old stack pointer
	mtlr	r0		; restore link register
	lwz	r31,-4(r1)	; restore r31 (frame pointer) from reg save

	bctr			; jump to the symbol`s address that was bound

#endif /* __ppc__ */
