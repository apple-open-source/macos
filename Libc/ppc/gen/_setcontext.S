/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#if defined(__ppc__)

#include <architecture/ppc/asm_help.h>

TEXT
LABEL(__setcontext)
	mr  r31, r3			/* save ucontext across call */
	lwz  r3, 4(r3)			/* load up signal mask */
	CALL_EXTERN(_sigsetmask)	/* set signal mask */
	mr   r3, r31			/* restore ucontext */
	lwz  r3,  28(r3)		/* load up mcontext */
	lwz  r1,  44(r3)		/* start restoring regs */
	lwz  r2,  48(r3) 
	lwz  r4,  56(r3) 
	lwz  r5,  60(r3) 
	lwz  r6,  64(r3) 
	lwz  r7,  68(r3) 
	lwz  r8,  72(r3) 
	lwz  r9,  76(r3) 
	lwz r10,  80(r3) 
	lwz r11,  84(r3) 
	lwz r12,  88(r3) 
	lwz r13,  92(r3)
	lwz r14,  96(r3)
	lwz r15, 100(r3)
	lwz r16, 104(r3)
	lwz r17, 108(r3)
	lwz r18, 112(r3)
	lwz r19, 116(r3)
	lwz r20, 120(r3)
	lwz r21, 124(r3)
	lwz r22, 128(r3)
	lwz r23, 132(r3)
	lwz r24, 136(r3)
	lwz r25, 140(r3)
	lwz r26, 144(r3)
	lwz r27, 148(r3)
	lwz r28, 152(r3)
	lwz r29, 156(r3)
	lwz r30, 160(r3)
	lwz r31, 164(r3)
	lwz  r0, 168(r3)		/* only restore non-vol CR's */
	mtcrf 0x20, r0
	mtcrf 0x10, r0
	mtcrf 0x08, r0
	lwz  r0, 176(r3)
	mtlr r0
	lwz  r0, 32(r3)
	mtctr r0
	lwz  r3, 52(r3)			/* restore r3 last */
	bctrl

#endif /* __ppc__ */
