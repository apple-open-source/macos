/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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

#define KERNEL_PRIVATE	1
/*
 * Header files.
 */
#import	<architecture/ppc/asm_help.h>
#import	<architecture/ppc/pseudo_inst.h>
#import	<mach/ppc/exception.h>
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
	blr						 @\
	END(trap_name)

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

#define	SYSCALL(name, nargs)			\
	.globl	cerror				@\
LEAF(_##name)					@\
	kernel_trap_args_##nargs		@\
	li	r0,SYS_##name			@\
	sc					@\
	b	1f   				@\
	b	2f				@\
1:	BRANCH_EXTERN(cerror)			@\
.text						\
2:	nop

#define	SYSCALL_NONAME(name, nargs)		\
	.globl	cerror				@\
	kernel_trap_args_##nargs		@\
	li	r0,SYS_##name			@\
	sc					@\
	b	1f   				@\
	b	2f				@\
1:	BRANCH_EXTERN(cerror)			@\
.text						\
2:	nop

#define	PSEUDO(pseudo, name, nargs)		\
LEAF(_##pseudo)					@\
	SYSCALL_NONAME(name, nargs)


#undef END
#import	<mach/ppc/syscall_sw.h>

#if !defined(SYS_getdirentriesattr)
#define SYS_getdirentriesattr 222
#endif

#if !defined(SYS_semsys)
#define SYS_semsys	251
#define SYS_msgsys	252
#define SYS_shmsys	253
#define SYS_semctl	254
#define SYS_semget	255
#define SYS_semop	256
#define SYS_semconfig	257
#define SYS_msgctl	258
#define SYS_msgget	259
#define SYS_msgsnd	260
#define SYS_msgrcv	261
#define SYS_shmat	262
#define SYS_shmctl	263
#define SYS_shmdt	264
#define SYS_shmget	265
#endif
 
