/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
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
 */
#include "SYS.h"

	.data
	.private_extern __current_pid
__current_pid:
	.long 0
L__current_pid_addr = __current_pid

#if defined(__DYNAMIC__)
#define GET_CURRENT_PID				\
	call	0f				; \
0:						; \
	popl	%ecx				; \
	leal	L__current_pid_addr-0b(%ecx), %ecx

#define __current_pid (%ecx)

#else
#define GET_CURRENT_PID
#endif

/*
 * If __current_pid is > 0, return it, else make syscall.
 * If __current_pid is 0, cache result of syscall.
 */
TEXT
LEAF(_getpid, 0)
	GET_CURRENT_PID
	movl		__current_pid, %eax
	testl		%eax, %eax
	jle		1f
	ret
1:
	UNIX_SYSCALL_NONAME(getpid, 0)
	movl		%eax, %edx
	xorl		%eax, %eax
	lock
	cmpxchgl	%edx, __current_pid
	movl		%edx, %eax
	ret
