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
 
#define KERNEL_PRIVATE	1
/*
 * Headers
 */
#include <sys/syscall.h>
#include <architecture/i386/asm_help.h>
#include <mach/i386/syscall_sw.h>


#define UNIX_SYSCALL_TRAP	lcall	$0x2b, $0
#define MACHDEP_SYSCALL_TRAP	lcall	$0x7, $0


#define UNIX_SYSCALL(name, nargs)			\
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
	UNIX_SYSCALL_TRAP				;\
	jnb	2f					;\
	BRANCH_EXTERN(cerror)  				;\
2:

#define PSEUDO(pseudo, name, nargs)			\
LEAF(_##pseudo, 0)					;\
	UNIX_SYSCALL_NONAME(name, nargs)

#if !defined(SYS_getdirentriesattr)
#define SYS_getdirentriesattr 222
#endif

#if !defined(SYS_semsys)
#define SYS_semsys      251
#define SYS_msgsys      252
#define SYS_shmsys      253
#define SYS_semctl      254
#define SYS_semget      255
#define SYS_semop       256
#define SYS_semconfig   257
#define SYS_msgctl      258
#define SYS_msgget      259
#define SYS_msgsnd      260
#define SYS_msgrcv      261
#define SYS_shmat       262
#define SYS_shmctl      263
#define SYS_shmdt       264
#define SYS_shmget      265
#endif

