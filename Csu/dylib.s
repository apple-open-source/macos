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
 * To avoid "relocation overflow" errors from the static link editor we want
 * to make sure that the .text section and the stub sections don't have any 
 * sections between them (like the .cstring section).  Since this is the first
 * file loaded we can force the order by ordering the section directives.
 */
.text
.picsymbol_stub

#ifdef __m68k__
/*
 * This is part of the code generation for the dynamic link editor's interface.
 * The dyld_stub_binding_helper for a m68k dynamicly linked shared library.  On
 * transfer to this point the address of the lazy pointer to be bound has been
 * pushed on the stack.  Here we push the address of the mach header for the
 * image on the stack and transfer control to the lazy symbol binding entry
 * point.  A pointer to the lazy symbol binding entry point is set at launch
 * time by the dynamic link editor.  This pointer is located at offset 0 (zero)
 * in the (__DATA,__dyld) section and declared here.  A pointer to the function
 * address of _dyld_func_lookup in the dynamic link editor is also set at launch
 * time by the dynamic link editor.  This pointer is located at offset 4 in the
 * in the (__DATA,__dyld) section and declared here.  A definition of the 'C'
 * function _dyld_func_lookup() is defined here as a private_extern to jump
 * through the pointer.
 */
	.text
	.align	1
	.private_extern dyld_stub_binding_helper
dyld_stub_binding_helper:
	movel	pc@(dyld__mh_dylib_header-.),sp@-
	movel	pc@(dyld_lazy_symbol_binding_entry_point-.),a0
	jmp	a0@

	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	movel	pc@(dyld_func_lookup_pointer-.),a0
	jmp	a0@

	.data
	.align	2
dyld__mh_dylib_header:
	.long	__mh_dylib_header

	.dyld
	.align	2
dyld_lazy_symbol_binding_entry_point:
	.long	0	| filled in at launch time by dynamic link editor
dyld_func_lookup_pointer:
	.long	0	| filled in at launch time by dynamic link editor
#endif /* __m68k__ */

#ifdef __i386__
/*
 * This is part of the code generation for the dynamic link editor's interface.
 * The dyld_stub_binding_helper for a i386 dynamicly linked shared library.  On
 * transfer to this point the address of the lazy pointer to be bound has been
 * pushed on the stack.  Here we push the address of the mach header for the
 * image on the stack and transfer control to the lazy symbol binding entry
 * point.  A pointer to the lazy symbol binding entry point is set at launch
 * time by the dynamic link editor.  This pointer is located at offset 0 (zero)
 * in the (__DATA,__dyld) section and declared here.  A pointer to the function
 * address of _dyld_func_lookup in the dynamic link editor is also set at
 * launch time by the dynamic link editor.  This pointer is located at offset 4
 * in the in the (__DATA,__dyld) section and declared here.  A definition of
 * the 'C' function _dyld_func_lookup() is defined here as a private_extern to
 * jump through the pointer.
 */
	.text
	.align	2,0x90
	.private_extern dyld_stub_binding_helper
dyld_stub_binding_helper:
	call	L1
L1:	popl	%eax
	pushl	dyld__mh_dylib_header-L1(%eax)
	movl    dyld_lazy_symbol_binding_entry_point-L1(%eax),%eax
	jmpl    %eax

	.align	2,0x90
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	call	L2
L2:	popl	%eax
	movl    dyld_func_lookup_pointer-L2(%eax),%eax
	jmpl    %eax

	.data
	.align	2
dyld__mh_dylib_header:
	.long	__mh_dylib_header

	.dyld
	.align	2
dyld_lazy_symbol_binding_entry_point:
	.long	0	# filled in at launch time by dynamic link editor
dyld_func_lookup_pointer:
	.long	0	# filled in at launch time by dynamic link editor
#endif /* __i386__ */

#ifdef __hppa__
/*
 * This is part of the code generation for the dynamic link editor's interface.
 * The stub_binding_helper for an hppa dynamicly linked shared library.  On
 * transfer to this point the address of the lazy pointer to be bound is in r21.
 * The return address on call to the stub is still in r2 or (r31 or a millicode
 * call).  Here we place the address of the mach header for the image in r22 and
 * transfer control to the lazy symbol binding entry point.  A pointer to the
 * lazy symbol binding entry point is set at launch time by the dynamic link
 * editor.  This pointer is located at offset 0 (zero) in the (__DATA,__dyld)
 * section and declared here.  A pointer to the function address of
 * _dyld_func_lookup in the dynamic link editor is also set at launch time by
 * the dynamic link editor.  This pointer is located at offset 4 in the in the
 * (__DATA,__dyld) section and declared here.  A definition of the 'C' function
 * _dyld_func_lookup() is defined here as a private_extern to jump through the
 * pointer.
 */
	.text
	.align	2
	.private_extern dyld_stub_binding_helper
dyld_stub_binding_helper:
	bl,n	L1,%r19
	nop
L1:	depi	0,31,2,%r19
	addil	L`dyld__mh_dylib_header-L1,%r19
	ldw	R`dyld__mh_dylib_header-L1(%r1),%r22
	addil	L`dyld_lazy_symbol_binding_entry_point-L1,%r19
	ldw	R`dyld_lazy_symbol_binding_entry_point-L1(%r1),%r1
	be,n	0(4,%r1)

	.align	2
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	bl,n	L2,%r19
	nop
L2:	depi	0,31,2,%r19
	addil	L`dyld_func_lookup_pointer-L2,%r19
	ldw	R`dyld_func_lookup_pointer-L2(%r1),%r1
	be,n	0(4,%r1)

	.data
	.align	2
dyld__mh_dylib_header:
	.long	__mh_dylib_header

	.dyld
	.align	2
dyld_lazy_symbol_binding_entry_point:
	.long	0	; filled in at launch time by dynamic link editor
dyld_func_lookup_pointer:
	.long	0	; filled in at launch time by dynamic link editor
#endif /* __hppa__ */

#ifdef __sparc__
/*
 * This is part of the code generation for the dynamic link editor's interface.
 * The stub_binding_helper for an sparc dynamicly linked shared library.  On
 * transfer to this point the address of the lazy pointer to be bound is in %g5.
 * Here we place the address of the mach header for the image in %g6 and
 * transfer control to the lazy symbol binding entry point.  A pointer to the
 * lazy symbol binding entry point is set at launch time by the dynamic link
 * editor.  This pointer is located at offset 0 (zero) in the (__DATA,__dyld)
 * section and declared here.  A pointer to the function address of
 * _dyld_func_lookup in the dynamic link editor is also set at launch time by
 * the dynamic link editor.  This pointer is located at offset 4 in the in the
 * (__DATA,__dyld) section and declared here.  A definition of the 'C' function
 * _dyld_func_lookup() is defined here as a private_extern to jump through the
 * pointer.
 */

	.text
	.align	2
	.private_extern dyld_stub_binding_helper
dyld_stub_binding_helper:
	mov %o7,%g2
L0:	call	L1
	nop
L1:
	set	(dyld__mh_dylib_header-L0),%g3
	ld	[%o7+%g3],%g6
	
	set	(dyld_lazy_symbol_binding_entry_point-L0),%g3
	ld	[%o7+%g3],%g3
	jmp	%g3
	mov %g2,%o7

	.private_extern __dyld_func_lookup
	.align	2
__dyld_func_lookup:
	mov	%o7,%g2
L00:	call	L2
	nop
L2:
	set	(dyld_func_lookup_pointer-L00),%g3
	ld	[%o7+%g3],%g3
	jmp	%g3
	mov %g2,%o7

	.data
	.align 2
dyld__mh_dylib_header:
	.long	__mh_dylib_header

	.dyld
	.align	2
dyld_lazy_symbol_binding_entry_point:
	.long	0	! filled in at launch time by dynamic link editor
dyld_func_lookup_pointer:
	.long	0	! filled in at launch time by dynamic link editor
#endif /* __sparc__ */

#ifdef __ppc__
/*
 * This is part of the code generation for the dynamic link editor's interface.
 * The stub_binding_helper for an ppc dynamicly linked shared library.  On
 * transfer to this point the address of the lazy pointer to be bound is in r11.
 * Here we place the address of the mach header for the image in r12 and
 * transfer control to the lazy symbol binding entry point.  A pointer to the
 * lazy symbol binding entry point is set at launch time by the dynamic link
 * editor.  This pointer is located at offset 0 (zero) in the (__DATA,__dyld)
 * section and declared here.  A pointer to the function address of
 * _dyld_func_lookup in the dynamic link editor is also set at launch time by
 * the dynamic link editor.  This pointer is located at offset 4 in the in the
 * (__DATA,__dyld) section and declared here.  A definition of the 'C' function
 * _dyld_func_lookup() is defined here as a private_extern to jump through the
 * pointer.
 */
	.text
	.align	2
	.private_extern dyld_stub_binding_helper
dyld_stub_binding_helper:
	mflr	r0
	bcl	20,31,L1
L1:	mflr    r12
	mtlr	r0
	mr	r0,r12
	addis	r12,r12,ha16(dyld_lazy_symbol_binding_entry_point-L1)
	lwz	r12,lo16(dyld_lazy_symbol_binding_entry_point-L1)(r12)
	mtctr	r12
	mr	r12,r0
	addis	r12,r12,ha16(dyld__mh_dylib_header-L1)
	lwz	r12,lo16(dyld__mh_dylib_header-L1)(r12)
	bctr

	.align 2
	.private_extern     cfm_stub_binding_helper
cfm_stub_binding_helper:
	mr	r11, r12 ; The TVector address is the binding pointer address.
	b	dyld_stub_binding_helper  ; Let the normal code handle the rest.

	.align	2
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	mflr	r0
	bcl	20,31,L2
L2:	mflr    r11
	mtlr	r0
	addis	r11,r11,ha16(dyld_func_lookup_pointer-L2)
	lwz	r11,lo16(dyld_func_lookup_pointer-L2)(r11)
	mtctr	r11
	bctr

	.data
	.align	2
dyld__mh_dylib_header:
	.long	__mh_dylib_header

	.dyld
	.align	2
dyld_lazy_symbol_binding_entry_point:
	.long	0	; filled in at launch time by dynamic link editor
dyld_func_lookup_pointer:
	.long	0	; filled in at launch time by dynamic link editor
#endif /* __ppc__ */
