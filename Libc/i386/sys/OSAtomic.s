/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <machine/cpu_capabilities.h>

#define DECLARE(x)   \
.align 2, 0x90      ; \
.globl x            ; \
.globl x ## Barrier ; \
x:                  ; \
x ## Barrier:

.text

DECLARE(_OSAtomicAnd32)
	movl 8(%esp), %ecx
	movl (%ecx), %eax
1:
	movl 4(%esp), %edx
	andl %eax, %edx
	call *_COMM_PAGE_COMPARE_AND_SWAP32
	jnz  1b
	movl %edx, %eax
	ret

DECLARE(_OSAtomicOr32)
	movl 8(%esp), %ecx
	movl (%ecx), %eax
1:
	movl 4(%esp), %edx
	orl %eax, %edx
	call *_COMM_PAGE_COMPARE_AND_SWAP32
	jnz  1b
	movl %edx, %eax
	ret

DECLARE(_OSAtomicXor32)
	movl 8(%esp), %ecx
	movl (%ecx), %eax
1:
	movl 4(%esp), %edx
	xorl %eax, %edx
	call *_COMM_PAGE_COMPARE_AND_SWAP32
	jnz  1b
	movl %edx, %eax
	ret

DECLARE(_OSAtomicCompareAndSwap32)
	movl     4(%esp), %eax
	movl     8(%esp), %edx
	movl    12(%esp), %ecx
	call	*_COMM_PAGE_COMPARE_AND_SWAP32
	sete	%al
	ret

.align 2, 0x90
DECLARE(_OSAtomicCompareAndSwap64)
	pushl	%ebx
	pushl	%esi
	movl    12(%esp), %eax
	movl    16(%esp), %edx
	movl    20(%esp), %ebx
	movl    24(%esp), %ecx
	movl	28(%esp), %esi
	call	*_COMM_PAGE_COMPARE_AND_SWAP64
	sete	%al
	popl	%esi
	popl	%ebx
	ret

DECLARE(_OSAtomicAdd32)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movl	%eax, %ecx
	call	*_COMM_PAGE_ATOMIC_ADD32
	addl	%ecx, %eax
	ret

DECLARE(_OSAtomicAdd64)
	pushl	%ebx
	pushl	%esi
	movl	20(%esp), %esi
	movl	0(%esi), %eax
	movl	4(%esi), %edx
1:	movl	12(%esp), %ebx
	movl	16(%esp), %ecx
	addl	%eax, %ebx
	adcl	%edx, %ecx
	call	*_COMM_PAGE_COMPARE_AND_SWAP64
	jnz	1b
	movl	%ebx, %eax
	movl	%ecx, %ebx
	popl	%esi
	popl	%ebx	
	ret

DECLARE(_OSAtomicTestAndSet)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	call	*_COMM_PAGE_BTS
	setc	%al
	ret

DECLARE(_OSAtomicTestAndClear)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	call	*_COMM_PAGE_BTC
	setc	%al
	ret

.align 2, 0x90
.globl _OSSpinLockTry
_OSSpinLockTry:
	movl	$(_COMM_PAGE_SPINLOCK_TRY), %eax
	jmpl	%eax

.align 2, 0x90
.globl _OSSpinLockLock
_OSSpinLockLock:
	movl	$(_COMM_PAGE_SPINLOCK_LOCK), %eax
	jmpl	%eax

.align 2, 0x90
.globl _OSSpinLockUnlock
_OSSpinLockUnlock:
	movl	4(%esp), %eax
	movl	$0, (%eax)
	ret

.align 2, 0x90
.globl _OSMemoryBarrier
_OSMemoryBarrier:
	ret
