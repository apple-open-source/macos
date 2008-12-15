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
#include "SYS.h"
#include <arm/arch.h>

.text

/*
 * Use LDREX/STREX to perform atomic operations.
 * Memory barriers are not needed on a UP system
 */

#if defined(_ARM_ARCH_6)

/* Implement a generic atomic arithmetic operation:
 * operand is in R0, pointer is in R1.  Return new
 * value into R0
 */
#define ATOMIC_ARITHMETIC(op) \
1:	ldrex	r2, [r1]	/* load existing value and tag memory */	    ;\
	op	r3, r2, r0	/* compute new value */				    ;\
	strex	r2, r3, [r1]	/* store new value if memory is still tagged */	    ;\
	cmp	r2, #0		/* check if the store succeeded */		    ;\
	bne	1b		/* if not, try again */				    ;\
	mov	r0, r3		/* return new value */

MI_ENTRY_POINT(_OSAtomicAdd32Barrier)
MI_ENTRY_POINT(_OSAtomicAdd32)
	ATOMIC_ARITHMETIC(add)
	bx	lr

MI_ENTRY_POINT(_OSAtomicOr32Barrier)
MI_ENTRY_POINT(_OSAtomicOr32)
	ATOMIC_ARITHMETIC(orr)
	bx	lr

MI_ENTRY_POINT(_OSAtomicAnd32Barrier)
MI_ENTRY_POINT(_OSAtomicAnd32)
	ATOMIC_ARITHMETIC(and)
	bx	lr

MI_ENTRY_POINT(_OSAtomicXor32Barrier)
MI_ENTRY_POINT(_OSAtomicXor32)
	ATOMIC_ARITHMETIC(eor)
	bx	lr

MI_ENTRY_POINT(_OSAtomicCompareAndSwap32Barrier)
MI_ENTRY_POINT(_OSAtomicCompareAndSwap32)
MI_ENTRY_POINT(_OSAtomicCompareAndSwapIntBarrier)
MI_ENTRY_POINT(_OSAtomicCompareAndSwapInt)
MI_ENTRY_POINT(_OSAtomicCompareAndSwapLongBarrier)
MI_ENTRY_POINT(_OSAtomicCompareAndSwapLong)
MI_ENTRY_POINT(_OSAtomicCompareAndSwapPtrBarrier)
MI_ENTRY_POINT(_OSAtomicCompareAndSwapPtr)
1:	ldrex	r3, [r2]	// load existing value and tag memory
	teq	r3, r0		// is it the same as oldValue?
	movne	r0, #0		// if not, return 0 immediately
	bxne	lr	    
	strex	r3, r1, [r2]	// otherwise, try to store new value
	cmp	r3, #0		// check if the store succeeded
	bne	1b		// if not, try again
	mov	r0, #1		// return true
	bx	lr


/* Implement a generic test-and-bit-op operation:
 * bit to set is in R0, base address is in R1.  Return
 * previous value (0 or 1) of the bit in R0.
 */
#define ATOMIC_BITOP(op)    \
	/* Adjust pointer to point at the correct word				    ;\
	 * R1 = R1 + 4 * (R0 / 32)						    ;\
	 */									    ;\
        mov     r3, r0, lsr #5							    ;\
        add     r1, r1, r3, asl #2						    ;\
	/* Generate a bit mask for the bit we want to test			    ;\
	 * R0 = (0x80 >> (R0 & 7)) << (R0 & ~7 & 31)				    ;\
	 */									    ;\
        and     r2, r0, #7							    ;\
        mov     r3, #0x80							    ;\
        mov     r3, r3, asr r2							    ;\
        and     r0, r0, #0x18							    ;\
        mov     r0, r3, asl r0							    ;\
1:										    ;\
	ldrex	r2, [r1]	/* load existing value and tag memory */	    ;\
	op	r3, r2, r0	/* compute new value */				    ;\
	strex	ip, r3, [r1]	/* attempt to store new value */		    ;\
	cmp	ip, #0		/* check if the store succeeded */		    ;\
	bne	1b		/* if so, try again */				    ;\
	ands	r0, r2, r0	/* mask off the bit from the old value */	    ;\
	movne	r0, #1		/* if non-zero, return exactly 1 */
	
MI_ENTRY_POINT(_OSAtomicTestAndSetBarrier)
MI_ENTRY_POINT(_OSAtomicTestAndSet)
	ATOMIC_BITOP(orr)
	bx	lr

MI_ENTRY_POINT(_OSAtomicTestAndClearBarrier)
MI_ENTRY_POINT(_OSAtomicTestAndClear)
	ATOMIC_BITOP(bic)
	bx	lr

MI_ENTRY_POINT(_OSMemoryBarrier)
	bx	lr


#if defined(_ARM_ARCH_6K)
/* If we can use LDREXD/STREXD, then we can implement 64-bit atomic operations */

MI_ENTRY_POINT(_OSAtomicAdd64)
	// R0,R1 contain the amount to add
	// R2 contains the pointer
	stmfd	sp!, {r4, r5, r6, r8, lr}
1:	
	ldrexd	r4, [r2]	// load existing value to R4/R5 and tag memory
	adds	r6, r4, r0	// add lower half of new value into R6 and set carry bit
	adc	r8, r5, r1	// add upper half of new value into R8 with carry
	strexd	r3, r6, [r2]	// store new value if memory is still tagged
	cmp	r3, #0		// check if store succeeded
	bne	1b		// if so, try again
	mov	r0, r6		// return new value
	mov	r1, r8
	ldmfd	sp!, {r4, r5, r6, r8, pc}
	
MI_ENTRY_POINT(_OSAtomicCompareAndSwap64)
	// R0,R1 contains the old value
	// R2,R3 contains the new value
	// the pointer is pushed onto the stack
	ldr	ip, [sp, #0]	// load pointer into IP
	stmfd	sp!, {r4, r5, lr}
1:	
	ldrexd	r4, [ip]	// load existing value into R4/R5 and tag memory
	teq	r0, r4		// check low word
	teqeq	r1, r5		// if low words match, check high word
	movne	r0, #0		// if either match fails, return 0
	bne	2f
	strexd	r4, r2, [ip]	// otherwise, try to store new values
	cmp	r3, #0		// check if store succeeded
	bne	1b		// if so, try again
	mov	r0, #1		// return true
2:	
	ldmfd	sp!, {r4, r5, pc}
			
#endif /* defined(_ARM_ARCH_6K) */

#endif /* defined(_ARM_ARCH_6) */

/*    
 * void
 * _spin_lock(p)
 *      int *p;
 *
 * Lock the lock pointed to by p.  Spin (possibly forever) until the next
 * lock is available.
 */
MI_ENTRY_POINT(_spin_lock)
MI_ENTRY_POINT(__spin_lock)
MI_ENTRY_POINT(_OSSpinLockLock)
L_spin_lock_loop:
	mov	r1, #1
	swp	r2, r1, [r0]
	cmp	r2, #0
	bxeq	lr
	mov	ip, sp
	stmfd   sp!, {r0, r8}
	mov	r0, #0	// THREAD_NULL
	mov	r1, #1	// SWITCH_OPTION_DEPRESS
	mov	r2, #1	// timeout (ms)
	mov	r12, #-61    // SYSCALL_THREAD_SWITCH
	swi	0x80
	ldmfd   sp!, {r0, r8}
	b	L_spin_lock_loop

MI_ENTRY_POINT(_spin_lock_try)
MI_ENTRY_POINT(__spin_lock_try)
MI_ENTRY_POINT(_OSSpinLockTry)
	mov	r1, #1
	swp	r2, r1, [r0]
	bic	r0, r1, r2
	bx	lr

/*
 * void
 * _spin_unlock(p)
 *      int *p;
 *
 * Unlock the lock pointed to by p.
 */
MI_ENTRY_POINT(_spin_unlock)
MI_ENTRY_POINT(__spin_unlock)
MI_ENTRY_POINT(_OSSpinLockUnlock)
	mov	r1, #0
	str	r1, [r0]
	bx	lr

