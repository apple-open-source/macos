/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 */
#include "SYS.h"

#if defined(__DYNAMIC__)
#define GET_CURRENT_PID	PICIFY(__current_pid)

        NON_LAZY_STUB(__current_pid)
#define __current_pid	(%edx)
#else
#define GET_CURRENT_PID
#endif

/*
 * If __current_pid >= 0, we want to put a -1 in there
 * otherwise we just decrement it
 */

LEAF(_vfork, 0)
	GET_CURRENT_PID
	movl		__current_pid, %eax
0:
	xorl		%ecx, %ecx
	testl		%eax, %eax
	cmovs		%eax, %ecx
	decl		%ecx
	lock
	cmpxchgl	%ecx, __current_pid
	jne		0b
	popl		%ecx
	movl		$(SYS_vfork), %eax	// code for vfork -> eax
	UNIX_SYSCALL_TRAP			// do the system call
	jnb		L1                     	// jump if CF==0
	GET_CURRENT_PID
	lock
	incl		__current_pid
	pushl		%ecx
	BRANCH_EXTERN(cerror)

L1:
	testl		%edx, %edx		// CF=OF=0,  ZF set if zero result
	jz		L2			// parent, since r1 == 0 in parent, 1 in child
	xorl		%eax, %eax		// zero eax
	jmp		*%ecx

L2:
	GET_CURRENT_PID
	lock
	incl		__current_pid
	jmp		*%ecx
