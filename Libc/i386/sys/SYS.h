/*
 * Copyright (c) 1999-2005 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 *	File:	SYS.h
 *
 *	Definition of the user side of the UNIX system call interface
 *	for i386.
 *
 * HISTORY
 *  12-3-92	Bruce Martin (Bruce_Martin@next.com)
 *	Created.
 */
 
/*
 * Headers
 */
#include <sys/syscall.h>
#include <architecture/i386/asm_help.h>
#include <mach/i386/syscall_sw.h>

/*
 * We have two entry points. int's is used for syscalls which need to preserve
 * %ecx across the call, or return a 64-bit value in %eax:%edx. sysenter is used
 * for the majority of syscalls which just return a value in %eax.
 */

#define UNIX_SYSCALL_SYSENTER	SYSENTER_PAD call __sysenter_trap

/*
 * This is the same as UNIX_SYSCALL, but it can call an alternate error
 * return function.  It's generic to support potential future callers.
 */
#define UNIX_SYSCALL_ERR(name, nargs,error_ret)		\
	.globl	error_ret				;\
LEAF(_##name, 0)					;\
	movl	$ SYS_##name, %eax			;\
	UNIX_SYSCALL_SYSENTER				;\
	jnb	2f					;\
	BRANCH_EXTERN(error_ret)  			;\
2:

#define UNIX_SYSCALL(name, nargs)			\
	.globl	cerror					;\
LEAF(_##name, 0)					;\
	movl	$ SYS_##name, %eax			;\
	UNIX_SYSCALL_SYSENTER				;\
	jnb	2f					;\
	BRANCH_EXTERN(cerror)  				;\
2:

#define UNIX_SYSCALL_INT(name, nargs)			\
	.globl	cerror					;\
LEAF(_##name, 0)					;\
	movl	$ SYS_##name, %eax			;\
	UNIX_SYSCALL_TRAP				;\
	jnb	2f					;\
	BRANCH_EXTERN(cerror)  				;\
2:

#define UNIX_SYSCALL_NONAME(name, nargs)		\
	.globl	cerror					;\
	movl	$ SYS_##name, %eax			;\
	UNIX_SYSCALL_SYSENTER				;\
	jnb	2f					;\
	BRANCH_EXTERN(cerror)  				;\
2:

#define UNIX_SYSCALL_INT_NONAME(name, nargs)		\
	.globl	cerror					;\
	movl	$ SYS_##name, %eax			;\
	UNIX_SYSCALL_TRAP				;\
	jnb	2f					;\
	BRANCH_EXTERN(cerror)  				;\
2:

#define PSEUDO(pseudo, name, nargs)			\
LEAF(_##pseudo, 0)					;\
	UNIX_SYSCALL_NONAME(name, nargs)

#define PSEUDO_ERR(pseudo, name, nargs, error_ret)	\
	.globl	error_ret				;\
LEAF(_##pseudo, 0)					;\
	movl	$ SYS_##name, %eax			;\
	UNIX_SYSCALL_SYSENTER				;\
	jnb	2f					;\
	BRANCH_EXTERN(error_ret)  			;\
2:
