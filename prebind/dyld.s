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
#if __m68k__
#ifdef CRT1
/*
 * After the dynamic linker initialization is done the value at offset 0 in
 * the (__DATA,__dyld) section (in this case the .long at the symbol
 * dyld_lazy_symbol_binding_entry_point) is filled in.  If it was not most
 * likely this program was run under a kernel without support for mapping in
 * and jumping to the dynamic linker at start up.
 */
	.text
	.align 1
	.private_extern __dyld_init_check
__dyld_init_check:
	tstl	dyld_lazy_symbol_binding_entry_point
	beq	1f
	rts
1:
/*
 * At this point the dynamic linker initialization was not run so print a
 * message on stderr and exit non-zero.  Since we can't use any libraries the
 * raw system call interfaces must be used.
 *
 *	write(stderr, error_message, sizeof(error_message));
 */
	movel	#78,d3
	movel	#error_message,d2
	movel   #2,d1
	moveq	#4,d0	| write() is system call number 4
	trap	#4
/*
 *	_exit(59);
 */
	movel	#59,d1
	moveq	#1,d0	| _exit() is system call number 1
	trap	#4
	| this call to _exit() should not fall through

	.cstring
error_message:
	.ascii	"The kernel support for the dynamic linker is not present "
	.ascii	"to run this program.\n\0"
/*
 * This is part of the code generation for the dynamic link editor's interface.
 * The stub_binding_helper for m68k executables.  On transfer to this point the
 * address of the lazy pointer to be has been pushed on the stack.  Here we push
 * the address of the mach header for the image and transfer control to the lazy
 * symbol binding entry point.  A pointer to the lazy symbol binding entry point
 * is set at launch time by the dynamic link editor.  This pointer is located at
 * offset 0 (zero) in the (__DATA,__dyld) section and declared here.  A pointer
 * to the function address of _dyld_func_lookup in the dynamic link editor is
 * also set at launch time by the dynamic link editor.  This pointer is located
 * at offset 4 in the in the (__DATA,__dyld) section and declared here.  A
 * definition of the 'C' function _dyld_func_lookup() is defined here as a
 * private_extern to jump through the pointer.
 */
	.text
	.align 1
	.private_extern dyld_stub_binding_helper
dyld_stub_binding_helper:
	movel	#__mh_execute_header,sp@-
	movel	dyld_lazy_symbol_binding_entry_point,a0
	jmp	a0@

	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	movel	dyld_func_lookup_pointer,a0
	jmp	a0@

	.dyld
	.align	2
dyld_lazy_symbol_binding_entry_point:
	.long	0	| filled in at launch time by dynamic link editor
dyld_func_lookup_pointer:
	.long	0	| filled in at launch time by dynamic link editor
	| the next three are used for the debugger interface
	.long	0	| start_debug_thread
	.long	0	| debug_port
	.long	0	| debug_thread
	.long	dyld_stub_binding_helper
	.long	0	| core debug
#else /* !defined(CRT1) */
	.text
	.align 1
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	illegal
#endif /* CRT1 */
#endif /* __m68k__ */

#ifdef __i386__
#ifdef CRT1
/*
 * After the dynamic linker initialization is done the value at offset 0 in
 * the (__DATA,__dyld) section (in this case the .long at the symbol
 * dyld_lazy_symbol_binding_entry_point) is filled in.  If it was not most
 * likely this program was run under a kernel without support for mapping in
 * and jumping to the dynamic linker at start up.
 */
	.text
	.align	2,0x90
	.private_extern	__dyld_init_check
__dyld_init_check:
	cmpl	$0,dyld_lazy_symbol_binding_entry_point
	je	1f
	ret
1:
/*
 * At this point the dynamic linker initialization was not run so print a
 * message on stderr and exit non-zero.  Since we can't use any libraries the
 * raw system call interfaces must be used.
 *
 *	write(stderr, error_message, sizeof(error_message));
 */
	pushl	$78
	pushl	$error_message
	pushl   $2
	push	$0
	movl	$4,%eax	# write() is system call number 4
	lcall	$0x2b,$0
	addl	$16,%esp
/*
 *	_exit(59);
 */
	pushl	$59
	push	$0
	movl	$1,%eax	# _exit() is system call number 1
	lcall	$0x2b,$0
	# this call to _exit() should not fall through

	.cstring
error_message:
	.ascii	"The kernel support for the dynamic linker is not present "
	.ascii	"to run this program.\n\0"

/*
 * This is part of the code generation for the dynamic link editor's interface.
 * The stub_binding_helper for i386 executables.  On transfer to this point the
 * address of the lazy pointer to be has been pushed on the stack.  Here we push
 * the address of the mach header for the image and transfer control to the lazy
 * symbol binding entry point.  A pointer to the lazy symbol binding entry point
 * is set at launch time by the dynamic link editor.  This pointer is located at
 * offset 0 (zero) in the (__DATA,__dyld) section and declared here.  A pointer
 * to the function address of _dyld_func_lookup in the dynamic link editor is
 * also set at launch time by the dynamic link editor.  This pointer is located
 * at offset 4 in the in the (__DATA,__dyld) section and declared here.  A
 * definition of the 'C' function _dyld_func_lookup() is defined here as a
 * private_extern to jump through the pointer.
 */
	.text
	.align	2,0x90
	.private_extern dyld_stub_binding_helper
dyld_stub_binding_helper:
	pushl   $__mh_execute_header
	jmpl    *dyld_lazy_symbol_binding_entry_point

	.align	2,0x90
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	jmpl	*dyld_func_lookup_pointer

	.dyld
	.align	2
dyld_lazy_symbol_binding_entry_point:
	.long	0	# filled in at launch time by dynamic link editor
dyld_func_lookup_pointer:
	.long	0	# filled in at launch time by dynamic link editor
	# the next three are used for the debugger interface
	.long	0	# start_debug_thread
	.long	0	# debug_port
	.long	0	# debug_thread
	.long	dyld_stub_binding_helper
	.long	0	# core debug
#else /* !defined(CRT1) */
	.text
	.align 2,0x90
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	hlt
#endif /* CRT1 */
#endif /* __i386__ */

#ifdef __hppa__
#ifdef CRT1
/*
 * After the dynamic linker initialization is done the value at offset 0 in
 * the (__DATA,__dyld) section (in this case the .long at the symbol
 * dyld_lazy_symbol_binding_entry_point) is filled in.  If it was not most
 * likely this program was run under a kernel without support for mapping in
 * and jumping to the dynamic linker at start up.
 */
	.text
	.align	2
	.private_extern	__dyld_init_check
__dyld_init_check:
	ldil	L`dyld_lazy_symbol_binding_entry_point,%r20
	ldw	R`dyld_lazy_symbol_binding_entry_point(%r20),%r20
	combt,=,n	%r20,%r0,1f
	bv,n	0(%r2)
1:
/*
 * At this point the dynamic linker initialization was not run so print a
 * message on stderr and exit non-zero.  Since we can't use any libraries the
 * raw system call interfaces must be used.
 *
 *	write(stderr, error_message, sizeof(error_message));
 */
	ldi	2,%r26
	ldil	L`error_message,%r19
	ldo	R`error_message(%r19),%r25
	ldi	78,%r24
#define SYSCALL_TRAP    0xc0000004
L1:	ldil    L`SYSCALL_TRAP,%r1
        ble     R`SYSCALL_TRAP(%sr4,%r1)
        ldi     4,%r22	; write() is system call number 4
	b,n	L2	; success go on to call _exit()
        b,n     L1	; signal happened retry system call
	b,n	L2	; system called failed, go on to call _exit()
/*
 *	_exit(59);
 */
L2:	ldi	59,%r26
        ldil    L`SYSCALL_TRAP,%r1
        ble     R`SYSCALL_TRAP(%sr4,%r1)
        ldi     1,%r22	; _exit() is system call number 1
	break	0,0	; success, this call to _exit() should not return
        b,n     L2	; signal happened retry system call
	break	0,0	; system called failed

	.cstring
error_message:
	.ascii	"The kernel support for the dynamic linker is not present "
	.ascii	"to run this program.\n\0"

/*
 * This is part of the code generation for the dynamic link editor's interface.
 * The stub_binding_helper for hppa executables.  On transfer to this point the
 * address of the lazy pointer to be bound is in r21.  The return address on
 * call to the stub is still in r2 or (r31 or a millicode call).  Here we place
 * the address of the mach header for the image in r22 and transfer control to
 * the lazy symbol binding entry point.  A pointer to the lazy symbol binding
 * entry point is set at launch time by the dynamic link editor.  This pointer
 * is located at offset 0 (zero) in the (__DATA,__dyld) section and declared
 * here.  A pointer to the function address of _dyld_func_lookup in the dynamic
 * link editor is also set at launch time by the dynamic link editor.  This
 * pointer is located at offset 4 in the in the (__DATA,__dyld) section and
 * declared here.  A definition of the 'C' function _dyld_func_lookup() is
 * defined here as a private_extern to jump through the pointer.
 */
	.text
	.align	2
	.private_extern dyld_stub_binding_helper
dyld_stub_binding_helper:
	ldil	L`__mh_execute_header,%r22
	ldo	R`__mh_execute_header(%r22),%r22
	ldil	L`dyld_lazy_symbol_binding_entry_point,%r1
	ldw	R`dyld_lazy_symbol_binding_entry_point(%r1),%r1
	be,n	0(4,%r1)

	.text
	.align	2
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	ldil	L`dyld_func_lookup_pointer,%r1
	ldw	R`dyld_func_lookup_pointer(%r1),%r1
	be,n	0(4,%r1)

	.dyld
	.align	2
dyld_lazy_symbol_binding_entry_point:
	.long	0	; filled in at launch time by dynamic link editor
dyld_func_lookup_pointer:
	.long	0	; filled in at launch time by dynamic link editor
	; the next three are used for the debugger interface
	.long	0	; start_debug_thread
	.long	0	; debug_port
	.long	0	; debug_thread
	.long	dyld_stub_binding_helper
	.long	0	; core debug
#else /* !defined(CRT1) */
	.text
	.align 2
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	break	0,0
#endif /* CRT1 */
#endif /* __hppa__ */

#ifdef __sparc__
#ifdef CRT1
/*
 * After the dynamic linker initialization is done the value at offset 0 in
 * the (__DATA,__dyld) section (in this case the .long at the symbol
 * dyld_lazy_symbol_binding_entry_point) is filled in.  If it was not, most
 * likely this program was run under a kernel without support for mapping in
 * and jumping to the dynamic linker at start up.
 */
	.text
	.align	2
	.private_extern	__dyld_init_check
__dyld_init_check:
	sethi	%hi(dyld_lazy_symbol_binding_entry_point),%o0
	ld	[%o0+%lo(dyld_lazy_symbol_binding_entry_point)],%o1
	tst	%o1
	bz,a	1f
	mov	2,%o0		! stderr for write() below
	retl
	nop
1:
/*
 * At this point the dynamic linker initialization was not run so print a
 * message on stderr and exit non-zero.  Since we can't use any libraries the
 * raw system call interfaces must be used.
 *
 *	write(stderr, error_message, sizeof(error_message));
 *
 * write() is syscall #4
 */
	.text
	.align	2
dyld_init_error:
	set	error_message,%o1			! error msg ptr
	set	(78),%o2	! error msg len
	mov	4,%g1		! write()
        ta	0x80		! SYSCALL trap
	retl
	nop
	mov	1,%g1		! exit()
        ta	0x80		! SYSCALL trap
	ta	0x81		! debugger - we should never get here

	.cstring
error_message:
	.ascii	"The kernel support for the dynamic linker is not present "
	.ascii	"to run this program.\n\0"

/*
 * This is part of the code generation for the dynamic link editor's interface.
 * The stub_binding_helper for sparc executables.  On transfer to this point the
 * address of the lazy pointer to be bound is in %g5. Here we place
 * the address of the mach header for the image in %g6 and transfer control to
 * the lazy symbol binding entry point.  A pointer to the lazy symbol binding
 * entry point is set at launch time by the dynamic link editor.  This pointer
 * is located at offset 0 (zero) in the (__DATA,__dyld) section and declared
 * here.  A pointer to the function address of _dyld_func_lookup in the dynamic
 * link editor is also set at launch time by the dynamic link editor.  This
 * pointer is located at offset 4 in the in the (__DATA,__dyld) section and
 * declared here.  A definition of the 'C' function _dyld_func_lookup() is
 * defined here as a private_extern to jump through the pointer.
 */
	.text
	.align	2
	.private_extern dyld_stub_binding_helper
dyld_stub_binding_helper:
	set	__mh_execute_header,%g6
! we use %o7 as a scratch register as the stub has already saved the
! original caller return register and thrashed %o7
	sethi	%hi(dyld_lazy_symbol_binding_entry_point),%o7
	ld	[%o7+%lo(dyld_lazy_symbol_binding_entry_point)],%o7
	jmp	%o7
	nop

	.text
	.align	2
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	sethi	%hi(dyld_func_lookup_pointer),%g3
	ld	[%g3+%lo(dyld_func_lookup_pointer)],%g3
	jmp	%g3
	nop

	.dyld
	.align	2
dyld_lazy_symbol_binding_entry_point:
	.long	0	! filled in at launch time by dynamic link editor
dyld_func_lookup_pointer:
	.long	0	! filled in at launch time by dynamic link editor
	! the next three are used for the debugger interface
	.long	0	! start_debug_thread
	.long	0	! debug_port
	.long	0	! debug_thread
	.long	dyld_stub_binding_helper
	.long	0	! core debug
#else /* !defined(CRT1) */
	.text
	.align 2
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	ta	9
#endif /* CRT1 */
#endif /* __sparc__ */

#if defined(__ppc__) || defined(__ppc64__)
#ifdef CRT1
#include <architecture/ppc/mode_independent_asm.h>
#ifdef __ppc64__
/*
 * __dyld_init_check does nothing for ppc64
 */
	.text
	.align 2
	.private_extern __dyld_init_check
__dyld_init_check:
	blr

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
	bcl     20,31,L1
L1:	mflr    r12
	mtlr	r0
	mr      r0,r12
	addis	r12,r12,ha16(dyld_lazy_symbol_binding_entry_point-L1)
	lg      r12,lo16(dyld_lazy_symbol_binding_entry_point-L1)(r12)
	mtctr	r12
	mr      r12,r0
       addis   r12,r12,ha16(dyld__mh_execute_header-L1)
       lg      r12,lo16(dyld__mh_execute_header-L1)(r12)
	bctr

	.align	2
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	mflr	r0
	bcl     20,31,L2
L2:	mflr    r11
	mtlr	r0
	addis	r11,r11,ha16(dyld_func_lookup_pointer-L2)
	lg      r11,lo16(dyld_func_lookup_pointer-L2)(r11)
	mtctr	r11
	bctr

	.data
	.align	LOG2_GPR_BYTES
dyld__mh_execute_header:
       .g_long __mh_execute_header
#else /* __ppc__ */
/*
 * After the dynamic linker initialization is done the value at offset 0 in
 * the (__DATA,__dyld) section (in this case the .long at the symbol
 * dyld_lazy_symbol_binding_entry_point) is filled in.  If it was not most
 * likely this program was run under a kernel without support for mapping in
 * and jumping to the dynamic linker at start up.
 */
	.text
	.align 2
	.private_extern __dyld_init_check
__dyld_init_check:
	lis     r11,ha16(dyld_lazy_symbol_binding_entry_point)
	lg      r11,lo16(dyld_lazy_symbol_binding_entry_point)(r11)
	cmpgi   cr1,r11,0
    bnelr++ cr1

/*
 * At this point the dynamic linker initialization was not run so print a
 * message on stderr and exit non-zero.  Since we can't use any libraries the
 * raw system call interfaces must be used.
 *
 *	write(stderr, error_message, sizeof(error_message));
 */
	li	r5,78
	lis	r4,hi16(error_message)
	ori	r4,r4,lo16(error_message)
	li	r3,2
	li	r0,4	; write() is system call number 4
	sc
    nop         ; return here on error
/*
 *	_exit(59);
 */
	li	r3,59
	li	r0,1	; exit() is system call number 1
	sc
	trap		; this call to _exit() should not fall through
    trap

	.cstring
error_message:
	.ascii	"The kernel support for the dynamic linker is not present "
	.ascii	"to run this program.\n\0"

/*
 * This is part of the code generation for the dynamic link editor's interface.
 * The stub_binding_helper for ppc executables.  On transfer to this point the
 * address of the lazy pointer to be bound is in r11.  The return address on
 * call to the stub is still in lr.  Here we place the address of the mach
 * header for the image in r12 and transfer control to the lazy symbol binding
 * entry point.  A pointer to the lazy symbol binding entry point is set at
 * launch time by the dynamic link editor.  This pointer is located at offset 0
 * (zero) in the (__DATA,__dyld) section and declared here.  A pointer to
 * the function address of _dyld_func_lookup in the dynamic link editor is also
 * set at launch time by the dynamic link editor.  This pointer is located at
 * offset 4 in the in the (__DATA,__dyld) section and declared here.  A
 * definition of the 'C' function _dyld_func_lookup() is defined here as a
 * private_extern to jump through the pointer.
 */
	.text
	.align	2
	.private_extern dyld_stub_binding_helper
dyld_stub_binding_helper:
	lis	r12,ha16(dyld_lazy_symbol_binding_entry_point)
	lg	r0,lo16(dyld_lazy_symbol_binding_entry_point)(r12)
	mtctr	r0
	lis	r12,hi16(__mh_execute_header)
	ori	r12,r12,lo16(__mh_execute_header)
	bctr

	.align	2
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	lis	r11,ha16(dyld_func_lookup_pointer)
	lg	r11,lo16(dyld_func_lookup_pointer)(r11)
	mtctr	r11
	bctr
#endif

	.dyld
	.align	LOG2_GPR_BYTES      /* 2 in 32-bit, 3 in 64-bit */
dyld_lazy_symbol_binding_entry_point:
	.g_long	0	; filled in at launch time by dynamic link editor
dyld_func_lookup_pointer:
	.g_long	0	; filled in at launch time by dynamic link editor
	; the next three are used for the debugger interface
	.g_long	0	; start_debug_thread
	.g_long	0	; debug_port
	.g_long	0	; debug_thread
	.g_long	dyld_stub_binding_helper
	.g_long	0	; core debug
#else /* !defined(CRT1) */
	.text
	.align 2
	.private_extern __dyld_func_lookup
__dyld_func_lookup:
	trap
#endif /* CRT1 */
#endif /* __ppc__ || __ppc64__ */
