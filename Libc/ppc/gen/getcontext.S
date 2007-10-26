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
LABEL(_getcontext)
	mr   r4, r1
        CALL_EXTERN(_getmcontext)
	stw  r1,  44(r3)
	stw  r2,  48(r3) 
	stw r13,  92(r3)
	stw r14,  96(r3)
	stw r15, 100(r3)
	stw r16, 104(r3)
	stw r17, 108(r3)
	stw r18, 112(r3)
	stw r19, 116(r3)
	stw r21, 124(r3)
	stw r22, 128(r3)
	stw r23, 132(r3)
	stw r24, 136(r3)
	stw r25, 140(r3)
	stw r26, 144(r3)
	stw r27, 148(r3)
	stw r28, 152(r3)
	stw r29, 156(r3)
	stw r30, 160(r3)
	stw r31, 164(r3)
	mfcr r0
	stw  r0, 168(r3)
	mflr r4
	stw  r4, 176(r3)
	stw  r4,  32(r3)
	li   r5, 0
	stw  r5,  52(r3)
        li   r3, 0
        blr

#endif /* __ppc__ */
