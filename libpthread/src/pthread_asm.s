/*
 * Copyright (c) 2012 Apple Computer, Inc. All rights reserved.
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

#if defined(__x86_64__)

#include <mach/i386/syscall_sw.h>

	.text
	.align 2, 0x90
	.globl ___pthread_set_self
___pthread_set_self:
	movl	$0, %esi	// 0 as the second argument
	movl    $ SYSCALL_CONSTRUCT_MDEP(3), %eax	// Machine-dependent syscall number 3
	MACHDEP_SYSCALL_TRAP
	ret

#ifndef VARIANT_DYLD

	.align 2, 0x90
	.globl _start_wqthread
_start_wqthread:
	// This routine is never called directly by user code, jumped from kernel
	push   %rbp
	mov    %rsp,%rbp
	sub    $24,%rsp		// align the stack
	call   __pthread_wqthread
	leave
	ret

	.align 2, 0x90
	.globl _thread_start
_thread_start:
	// This routine is never called directly by user code, jumped from kernel
	push   %rbp
	mov    %rsp,%rbp
	sub    $24,%rsp		// align the stack
	call   __pthread_start
	leave
	ret

#endif

#elif defined(__i386__)

#include <mach/i386/syscall_sw.h>

	.text
	.align 2, 0x90
	.globl ___pthread_set_self
___pthread_set_self:
	pushl   4(%esp)
	pushl   $0
	movl    $3,%eax
	MACHDEP_SYSCALL_TRAP
	addl    $8,%esp
	ret

#ifndef VARIANT_DYLD

	.align 2, 0x90
	.globl _start_wqthread
_start_wqthread:
	// This routine is never called directly by user code, jumped from kernel
	push   %ebp
	mov    %esp,%ebp
	sub    $28,%esp		// align the stack
	mov    %edi,16(%esp)    //arg5
	mov    %edx,12(%esp)    //arg4
	mov    %ecx,8(%esp)             //arg3
	mov    %ebx,4(%esp)             //arg2
	mov    %eax,(%esp)              //arg1
	call   __pthread_wqthread
	leave
	ret

	.align 2, 0x90
	.globl _thread_start
_thread_start:
	// This routine is never called directly by user code, jumped from kernel
	push   %ebp
	mov    %esp,%ebp
	sub    $28,%esp		// align the stack
	mov    %esi,20(%esp)    //arg6
	mov    %edi,16(%esp)    //arg5
	mov    %edx,12(%esp)    //arg4
	mov    %ecx,8(%esp)     //arg3
	mov    %ebx,4(%esp)     //arg2
	mov    %eax,(%esp)      //arg1
	call   __pthread_start
	leave
	ret

#endif

#elif defined(__arm__)

#include <mach/arm/syscall_sw.h>

	.text
	.align 2
	.globl ___pthread_set_self
___pthread_set_self:
	/* fast trap for thread_set_cthread */
	mov	r3, #2
	mov	r12, #0x80000000
	swi	#SWI_SYSCALL
	bx	lr

#ifndef VARIANT_DYLD

// This routine is never called directly by user code, jumped from kernel
// args 0 to 3 are already in the regs 0 to 3
// should set stack with the 2 extra args before calling pthread_wqthread()
// arg4 is in r[4]
// arg5 is in r[5]

	.text
	.align 2
	.globl _start_wqthread
_start_wqthread:
	stmfd sp!, {r4, r5}
	bl __pthread_wqthread
// Stackshots will show the routine that happens to link immediately following
// _start_wqthread.  So we add an extra instruction (nop) to make stackshots
// more readable.
	nop

	.text
	.align 2
	.globl _thread_start
_thread_start:
	stmfd sp!, {r4, r5}
	bl __pthread_start
// See above
	nop

#endif

#else
#error Unsupported architecture
#endif
