/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 * Copyright (c) 2004-2006 Apple Computer, Inc. All rights reserved.
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

DECLARE(_OSAtomicAnd32Orig)
	movl 8(%esp), %ecx
	movl (%ecx), %eax
1:
	movl 4(%esp), %edx
	andl %eax, %edx
	call *_COMM_PAGE_COMPARE_AND_SWAP32
	jnz  1b
	ret

DECLARE(_OSAtomicOr32Orig)
	movl 8(%esp), %ecx
	movl (%ecx), %eax
1:
	movl 4(%esp), %edx
	orl %eax, %edx
	call *_COMM_PAGE_COMPARE_AND_SWAP32
	jnz  1b
	ret

DECLARE(_OSAtomicXor32Orig)
	movl 8(%esp), %ecx
	movl (%ecx), %eax
1:
	movl 4(%esp), %edx
	xorl %eax, %edx
	call *_COMM_PAGE_COMPARE_AND_SWAP32
	jnz  1b
	ret

DECLARE(_OSAtomicCompareAndSwapPtr)
DECLARE(_OSAtomicCompareAndSwapInt)
DECLARE(_OSAtomicCompareAndSwapLong)
DECLARE(_OSAtomicCompareAndSwap32)
	movl     4(%esp), %eax
	movl     8(%esp), %edx
	movl    12(%esp), %ecx
	call	*_COMM_PAGE_COMPARE_AND_SWAP32
	sete	%al
	movzbl	%al,%eax	// widen in case caller assumes we return an int
	ret

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
	movzbl	%al,%eax	// widen in case caller assumes we return an int
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
	movl	%ecx, %edx
	popl	%esi
	popl	%ebx	
	ret

DECLARE(_OSAtomicTestAndSet)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movl	%eax, %ecx
	andl	$-8, %ecx
	notl	%eax
	andl	$7, %eax
	orl	%ecx, %eax
	call	*_COMM_PAGE_BTS
	setc	%al
	movzbl	%al,%eax	// widen in case caller assumes we return an int
	ret

DECLARE(_OSAtomicTestAndClear)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movl	%eax, %ecx
	andl	$-8, %ecx
	notl	%eax
	andl	$7, %eax
	orl	%ecx, %eax
	call	*_COMM_PAGE_BTC
	setc	%al
	movzbl	%al,%eax	// widen in case caller assumes we return an int
	ret

	.align	2, 0x90
	.globl	_OSSpinLockTry
	.globl	__spin_lock_try
_OSSpinLockTry:
__spin_lock_try:
	movl	$(_COMM_PAGE_SPINLOCK_TRY), %eax
	jmpl	*%eax

	.align	2, 0x90
	.globl	_OSSpinLockLock
	.globl	_spin_lock
	.globl	__spin_lock
_OSSpinLockLock:
_spin_lock:
__spin_lock:
	movl	$(_COMM_PAGE_SPINLOCK_LOCK), %eax
	jmpl	*%eax

	.align	2, 0x90
	.globl	_OSSpinLockUnlock
	.globl	_spin_unlock
	.globl	__spin_unlock
_OSSpinLockUnlock:
_spin_unlock:
__spin_unlock:
	movl	4(%esp), %eax
	movl	$0, (%eax)
	ret

	.align 2, 0x90
	.globl _OSMemoryBarrier
_OSMemoryBarrier:
	movl	$(_COMM_PAGE_MEMORY_BARRIER), %eax
	jmpl	*%eax

/*
 *	typedef	volatile struct {
 *		void	*opaque1;  <-- ptr to 1st queue element or null
 *		long	 opaque2;  <-- generation count
 *	} OSQueueHead;
 *
 * void  OSAtomicEnqueue( OSQueueHead *list, void *new, size_t offset);
 */
	.align	2
	.globl	_OSAtomicEnqueue
_OSAtomicEnqueue:
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	movl	16(%esp),%edi	// %edi == ptr to list head
	movl	20(%esp),%ebx	// %ebx == new
	movl	24(%esp),%esi	// %esi == offset
	movl	(%edi),%eax	// %eax == ptr to 1st element in Q
	movl	4(%edi),%edx	// %edx == current generation count
1:
	movl	%eax,(%ebx,%esi)// link to old list head from new element
	movl	%edx,%ecx
	incl	%ecx		// increment generation count
	lock			// always lock for now...
	cmpxchg8b (%edi)	// ...push on new element
	jnz	1b
	popl	%ebx
	popl	%esi
	popl	%edi
	ret
	
	
/* void* OSAtomicDequeue( OSQueueHead *list, size_t offset); */
	.align	2
	.globl	_OSAtomicDequeue
_OSAtomicDequeue:
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	movl	16(%esp),%edi	// %edi == ptr to list head
	movl	20(%esp),%esi	// %esi == offset
	movl	(%edi),%eax	// %eax == ptr to 1st element in Q
	movl	4(%edi),%edx	// %edx == current generation count
1:
	testl	%eax,%eax	// list empty?
	jz	2f		// yes
	movl	(%eax,%esi),%ebx // point to 2nd in Q
	movl	%edx,%ecx
	incl	%ecx		// increment generation count
	lock			// always lock for now...
	cmpxchg8b (%edi)	// ...pop off 1st element
	jnz	1b
2:
	popl	%ebx
	popl	%esi
	popl	%edi
	ret			// ptr to 1st element in Q still in %eax

