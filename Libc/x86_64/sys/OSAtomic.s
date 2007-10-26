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


// uint32_t OSAtomicAnd32( uint32_t mask, uint32_t *value);
DECLARE(_OSAtomicAnd32)
	movq	$(_COMM_PAGE_COMPARE_AND_SWAP32), %rcx
	movl	%edi, %r11d	// save mask
	movl	(%rsi), %eax	// get value
	movq	%rsi, %rdx	// put ptr where compare-and-swap expects it
1:
	movl	%r11d, %esi	// original mask
	movl	%eax, %edi	// old value
	andl	%eax, %esi	// new value
	call	*%rcx		// %edi=old value,  %esi=new value. %rdx=ptr
	jnz	1b
	movl	%esi, %eax
	ret


// uint32_t OSAtomicOr32( uint32_t mask, uint32_t *value);
DECLARE(_OSAtomicOr32)
	movq	$(_COMM_PAGE_COMPARE_AND_SWAP32), %rcx
	movl	%edi, %r11d	// save mask
	movl	(%rsi), %eax	// get value
	movq	%rsi, %rdx	// put ptr where compare-and-swap expects it
1:
	movl	%r11d, %esi	// original mask
	movl	%eax, %edi	// old value
	orl	%eax, %esi	// new value
	call	*%rcx		// %edi=old value,  %esi=new value. %rdx=ptr
	jnz	1b
	movl	%esi, %eax
	ret
	

// uint32_t OSAtomicXor32( uint32_t mask, uint32_t *value);
DECLARE(_OSAtomicXor32)
	movq	$(_COMM_PAGE_COMPARE_AND_SWAP32), %rcx
	movl	%edi, %r11d	// save mask
	movl	(%rsi), %eax	// get value
	movq	%rsi, %rdx	// put ptr where compare-and-swap expects it
1:
	movl	%r11d, %esi	// original mask
	movl	%eax, %edi	// old value
	xorl	%eax, %esi	// new value
	call	*%rcx		// %edi=old value,  %esi=new value. %rdx=ptr
	jnz	1b
	movl	%esi, %eax
	ret


// uint32_t OSAtomicAnd32Orig( uint32_t mask, uint32_t *value);
DECLARE(_OSAtomicAnd32Orig)
	movq	$(_COMM_PAGE_COMPARE_AND_SWAP32), %rcx
	movl	%edi, %r11d	// save mask
	movl	(%rsi), %eax	// get value
	movq	%rsi, %rdx	// put ptr where compare-and-swap expects it
1:
	movl	%r11d, %esi	// original mask
	movl	%eax, %edi	// old value
	andl	%eax, %esi	// new value
	call	*%rcx		// %edi=old value,  %esi=new value. %rdx=ptr
	jnz	1b
	movl	%edi, %eax
	ret


// uint32_t OSAtomicOr32Orig( uint32_t mask, uint32_t *value);
DECLARE(_OSAtomicOr32Orig)
	movq	$(_COMM_PAGE_COMPARE_AND_SWAP32), %rcx
	movl	%edi, %r11d	// save mask
	movl	(%rsi), %eax	// get value
	movq	%rsi, %rdx	// put ptr where compare-and-swap expects it
1:
	movl	%r11d, %esi	// original mask
	movl	%eax, %edi	// old value
	orl	%eax, %esi	// new value
	call	*%rcx		// %edi=old value,  %esi=new value. %rdx=ptr
	jnz	1b
	movl	%edi, %eax
	ret
	

// uint32_t OSAtomicXor32Orig( uint32_t mask, uint32_t *value);
DECLARE(_OSAtomicXor32Orig)
	movq	$(_COMM_PAGE_COMPARE_AND_SWAP32), %rcx
	movl	%edi, %r11d	// save mask
	movl	(%rsi), %eax	// get value
	movq	%rsi, %rdx	// put ptr where compare-and-swap expects it
1:
	movl	%r11d, %esi	// original mask
	movl	%eax, %edi	// old value
	xorl	%eax, %esi	// new value
	call	*%rcx		// %edi=old value,  %esi=new value. %rdx=ptr
	jnz	1b
	movl	%edi, %eax
	ret


// bool OSAtomicCompareAndSwap32( int32_t old, int32_t new, int32_t *value);
DECLARE(_OSAtomicCompareAndSwapInt)
DECLARE(_OSAtomicCompareAndSwap32)
	movq	$(_COMM_PAGE_COMPARE_AND_SWAP32), %rcx
	call	*%rcx		// %edi=old value,  %esi=new value. %rdx=ptr
	sete	%al
	movzbl	%al,%eax	// widen in case caller assumes we return an int
	ret


// bool OSAtomicCompareAndSwap64( int64_t old, int64_t new, int64_t *value);
DECLARE(_OSAtomicCompareAndSwapPtr)
DECLARE(_OSAtomicCompareAndSwapLong)
DECLARE(_OSAtomicCompareAndSwap64)
	movq	$(_COMM_PAGE_COMPARE_AND_SWAP64), %rcx
	call	*%rcx		// %rdi=old value,  %rsi=new value. %rdx=ptr
	sete	%al
	movzbl	%al,%eax	// widen in case caller assumes we return an int
	ret


// int32_t OSAtomicAdd32( int32_t amt, int32_t *value );
DECLARE(_OSAtomicAdd32)
	movq	$(_COMM_PAGE_ATOMIC_ADD32), %rcx
	movl	%edi, %eax	// save amt to add
	call	*%rcx
	addl	%edi,%eax	// new value
	ret


// int64_t OSAtomicAdd64( int64_t amt, int64_t *value );
DECLARE(_OSAtomicAdd64)
	movq	$(_COMM_PAGE_ATOMIC_ADD64), %rcx
	movq	%rdi, %rax	// save amt to add
	call	*%rcx
	addq	%rdi, %rax	// new value
	ret


// bool OSAtomicTestAndSet( uint32_t n, void *value );
DECLARE(_OSAtomicTestAndSet)
	movq	$(_COMM_PAGE_BTS), %rax
	xorl	$7, %edi	// bit position is numbered big endian
	call	*%rax
	setc	%al
	movzbl	%al,%eax	// widen in case caller assumes we return an int
	ret


// bool OSAtomicTestAndClear( uint32_t n, void *value );
DECLARE(_OSAtomicTestAndClear)
	movq	$(_COMM_PAGE_BTC), %rax
	xorl	$7, %edi	// bit position is numbered big endian
	call	*%rax
	setc	%al
	movzbl	%al,%eax	// widen in case caller assumes we return an int
	ret

// bool OSSpinLockTry( OSSpinLock *lock );
	.align	2, 0x90
	.globl	_OSSpinLockTry
	.globl	__spin_lock_try
_OSSpinLockTry:
__spin_lock_try:
	movq	$(_COMM_PAGE_SPINLOCK_TRY), %rax
	jmp	*%rax


// void OSSpinLockLock( OSSpinLock *lock );
	.align	2, 0x90
	.globl	_OSSpinLockLock
	.globl	_spin_lock
	.globl	__spin_lock
_OSSpinLockLock:
_spin_lock:
__spin_lock:
	movq	$(_COMM_PAGE_SPINLOCK_LOCK), %rax
	jmp	*%rax


// void OSSpinLockUnlock( OSSpinLock *lock );
	.align	2, 0x90
	.globl	_OSSpinLockUnlock
	.globl	_spin_unlock
	.globl	__spin_unlock
_OSSpinLockUnlock:
_spin_unlock:
__spin_unlock:
	movl	$0, (%rdi)
	ret


// void OSMemoryBarrier( void );
	.align	2, 0x90
	.globl	_OSMemoryBarrier
_OSMemoryBarrier:
	movq	$(_COMM_PAGE_MEMORY_BARRIER), %rax
	jmp	*%rax


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
_OSAtomicEnqueue:		// %rdi == list head, %rsi == new, %rdx == offset
	pushq	%rbx
	movq	%rsi,%rbx	// %rbx == new
	movq	%rdx,%rsi	// %rsi == offset
	movq	(%rdi),%rax	// %rax == ptr to 1st element in Q
	movq	8(%rdi),%rdx	// %rdx == current generation count
1:
	movq	%rax,(%rbx,%rsi)// link to old list head from new element
	movq	%rdx,%rcx
	incq	%rcx		// increment generation count
	lock			// always lock for now...
	cmpxchg16b (%rdi)	// ...push on new element
	jnz	1b
	popq	%rbx
	ret
	
	
/* void* OSAtomicDequeue( OSQueueHead *list, size_t offset); */
	.align	2
	.globl	_OSAtomicDequeue
_OSAtomicDequeue:		// %rdi == list head, %rsi == offset
	pushq	%rbx
	movq	(%rdi),%rax	// %rax == ptr to 1st element in Q
	movq	8(%rdi),%rdx	// %rdx == current generation count
1:
	testq	%rax,%rax	// list empty?
	jz	2f		// yes
	movq	(%rax,%rsi),%rbx // point to 2nd in Q
	movq	%rdx,%rcx
	incq	%rcx		// increment generation count
	lock			// always lock for now...
	cmpxchg16b (%rdi)	// ...pop off 1st element
	jnz	1b
2:
	popq	%rbx
	ret			// ptr to 1st element in Q still in %rax
