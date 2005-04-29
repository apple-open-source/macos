/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

	.section __TEXT,__text,regular,pure_instructions
	.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.machine ppc
.lcomm _funcptr,4,2
	.cstring
	.align 2
LC0:
	.ascii "vasprintf\0"
	.text
	.align 2
	.globl _vasprintf$LDBLStub
	.private_extern _vasprintf$LDBLStub
_vasprintf$LDBLStub:
	mflr r0
	bcl 20,31,"L00000000001$pb"
"L00000000001$pb":
	mflr r2
	mtlr r0
	addis r12,r2,ha16(_funcptr-"L00000000001$pb")
	la r12,lo16(_funcptr-"L00000000001$pb")(r12)
	lwz r12,0(r12)
	cmpwi cr7,r12,0
	bne cr7,L2

	stmw r30,-8(r1)
	stw r0,8(r1)
	stwu r1,-80(r1)
	mr r30,r1
	mr r31,r2
	stw r4,108(r30)
	stw r5,112(r30)
	stw r6,116(r30)
	stw r7,120(r30)
	stw r8,124(r30)
	stw r9,128(r30)
	stw r10,132(r30)
	stw r3,104(r30)
	addis r2,r31,ha16(LC0-"L00000000001$pb")
	la r3,lo16(LC0-"L00000000001$pb")(r2)
	bl L___stub_getrealaddr$stub
	mr r12,r3
	addis r2,r31,ha16(_funcptr-"L00000000001$pb")
	la r2,lo16(_funcptr-"L00000000001$pb")(r2)
	stw r12,0(r2)

	lwz r3,104(r30)
	lwz r10,132(r30)
	lwz r9,128(r30)
	lwz r8,124(r30)
	lwz r7,120(r30)
	lwz r6,116(r30)
	lwz r5,112(r30)
	lwz r4,108(r30)
	lwz r1,0(r1)
	lwz r0,8(r1)
	mtlr r0
	lmw r30,-8(r1)
L2:
	mtctr r12
	bctr

	.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 5
L___stub_getrealaddr$stub:
	.indirect_symbol ___stub_getrealaddr
	mflr r0
	bcl 20,31,"L00000000001$spb"
"L00000000001$spb":
	mflr r11
	addis r11,r11,ha16(L___stub_getrealaddr$lazy_ptr-"L00000000001$spb")
	mtlr r0
	lwzu r12,lo16(L___stub_getrealaddr$lazy_ptr-"L00000000001$spb")(r11)
	mtctr r12
	bctr
	.lazy_symbol_pointer
L___stub_getrealaddr$lazy_ptr:
	.indirect_symbol ___stub_getrealaddr
	.long	dyld_stub_binding_helper
	.subsections_via_symbols

#endif
