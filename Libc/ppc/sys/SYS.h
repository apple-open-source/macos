/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
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
/* Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved.
 *
 *	File:	SYS.h
 *
 *	Definition of the user side of the UNIX system call interface
 *	for M98K.
 *
 *	Errors are flagged by the location of the trap return (ie., which
 *	instruction is executed upon rfi):
 *
 *		SC PC + 4:	Error (typically branch to cerror())
 *		SC PC + 8:	Success
 *
 * HISTORY
 * 18-Nov-92	Ben Fathi (benf@next.com)
 *	Ported to m98k.
 *
 *  9-Jan-92	Peter King (king@next.com)
 *	Created.
 */

/*
 * Header files.
 */
#include <architecture/ppc/mode_independent_asm.h>
#import	<sys/syscall.h>

/* From rhapsody kernel mach/ppc/syscall_sw.h */
#define	kernel_trap_args_0
#define	kernel_trap_args_1
#define	kernel_trap_args_2
#define	kernel_trap_args_3
#define	kernel_trap_args_4
#define	kernel_trap_args_5
#define	kernel_trap_args_6
#define	kernel_trap_args_7

/*
 * simple_kernel_trap -- Mach system calls with 8 or less args
 * Args are passed in a0 - a7, system call number in r0.
 * Do a "sc" instruction to enter kernel.
 */	
#define simple_kernel_trap(trap_name, trap_number)	\
	.globl	_##trap_name				@\
_##trap_name:						@\
	li	r0,trap_number				 @\
	sc						 @\
	blr

#define kernel_trap_0(trap_name,trap_number)		 \
	simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_1(trap_name,trap_number)		 \
	simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_2(trap_name,trap_number)		 \
	simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_3(trap_name,trap_number)		 \
	simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_4(trap_name,trap_number)		 \
	simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_5(trap_name,trap_number)		 \
	simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_6(trap_name,trap_number)		 \
	simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_7(trap_name,trap_number)		 \
	simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_8(trap_name,trap_number)		 \
        simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_9(trap_name,trap_number)		 \
        simple_kernel_trap(trap_name,trap_number)

/* End of rhapsody kernel mach/ppc/syscall_sw.h */

/*
 * Macros.
 */

/*
 * This is the same as SYSCALL, but it can call an alternate error
 * return function.  It's generic to support potential future callers.
 */
#define	SYSCALL_ERR(name, nargs, error_ret)		\
	.globl	error_ret			@\
    MI_ENTRY_POINT(_##name)     @\
	kernel_trap_args_##nargs    @\
	li	r0,SYS_##name			@\
	sc                          @\
	b	1f                      @\
	blr                         @\
1:	MI_BRANCH_EXTERNAL(error_ret)

#define	SYSCALL(name, nargs)			\
	.globl	cerror				@\
    MI_ENTRY_POINT(_##name)     @\
	kernel_trap_args_##nargs    @\
	li	r0,SYS_##name			@\
	sc                          @\
	b	1f                      @\
	blr                         @\
1:	MI_BRANCH_EXTERNAL(cerror)


#define	SYSCALL_NONAME(name, nargs)		\
	.globl	cerror				@\
	kernel_trap_args_##nargs    @\
	li	r0,SYS_##name			@\
	sc                          @\
	b	1f                      @\
	b	2f                      @\
1:	MI_BRANCH_EXTERNAL(cerror)  @\
2:


#define	PSEUDO(pseudo, name, nargs)		\
    .globl  _##pseudo           @\
    .text                       @\
    .align  2                   @\
_##pseudo:                      @\
	SYSCALL_NONAME(name, nargs)

#define	PSEUDO_ERR(pseudo, name, nargs, error_ret)		\
    .globl  _##pseudo           @\
	.globl	error_ret			@\
    .text                       @\
    .align  2                   @\
_##pseudo:                      @\
	kernel_trap_args_##nargs    @\
	li	r0,SYS_##name			@\
	sc                          @\
	b	1f                      @\
	blr                         @\
1:	MI_BRANCH_EXTERNAL(error_ret)


#undef END
#import	<mach/ppc/syscall_sw.h>
