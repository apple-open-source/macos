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
#include <architecture/arm/asm_help.h>
#include <arm/arch.h>

.text

/* Number of times we spin in a spinlock before going to kernel */
#define	MP_SPIN_TRIES		1000
#define	MP_SPIN_TRIES_WFE	10

#if defined(VARIANT_DYLD)
  #if defined(_ARM_ARCH_7)
    /* This makes sure we pick up MP variants for dyld on armv7 */
    #define ENTRY_POINT_RESOLVER(symbol, variant) \
        ENTRY_POINT(symbol##$VARIANT$##variant) ;\
        .private_extern symbol##$VARIANT$##variant

    #define ENTRY_POINT_DEFAULT(symbol, variant) \
        ENTRY_POINT(symbol##$VARIANT$##variant) ;\
        .private_extern symbol##$VARIANT$##variant

    #define makeResolver_up_mp(name) \
      ENTRY_POINT(_##name) \
        ldr ip, L##name##$commpage ; \
        ldr ip, [ip] ; \
        tst ip, $(kUP) ; \
        beq _##name##$VARIANT$mp ; \
        b   _##name##$VARIANT$up ; \
        L##name##$commpage: .long _COMM_PAGE_CPU_CAPABILITIES ;

    #define makeResolver_up_mp_wfe(name) \
        makeResolver_up_mp(name)
  #else
    #define ENTRY_POINT_RESOLVER(symbol, variant) \
        ENTRY_POINT(symbol##$VARIANT$##variant) ;\
        .private_extern symbol##$VARIANT$##variant

    #define ENTRY_POINT_DEFAULT(symbol, variant)	ENTRY_POINT(symbol)
  #endif
#else
  #if defined(_ARM_ARCH_7)
    #define ENTRY_POINT_RESOLVER(symbol, variant) \
        ENTRY_POINT(symbol##$VARIANT$##variant) ;\
        .private_extern symbol##$VARIANT$##variant
    #define ENTRY_POINT_DEFAULT(symbol, variant) \
        ENTRY_POINT(symbol##$VARIANT$##variant) ;\
        .private_extern symbol##$VARIANT$##variant
  #else // !_ARM_ARCH_7
    /* _RESOLVER shouldn't be used on armv5/6, so intentionally plants bad text. */
    #define ENTRY_POINT_RESOLVER(symbol, variant)	.error
    #define ENTRY_POINT_DEFAULT(symbol, variant)	ENTRY_POINT(symbol)
  #endif
#endif // VARIANT_DYLD

#if defined(VARIANT_DYLD) && defined(_ARM_ARCH_7)
/*
 * In dyld's build only, we include the list of resolvers needed and
 * this generates entry points for dyld which are run on every execution
 * in order to pick the correct variant.
 */
#include "OSAtomic_resolvers.h"
#endif

#if defined(_ARM_ARCH_6)

/* Implement a generic atomic arithmetic operation:
 * operand is in R0, pointer is in R1.  Return new
 * value into R0 (or old valule in _ORIG cases).
 *
 * Return instructions are separate to the
 * _ATOMIC_ARITHMETIC macro.
 */
#define _ATOMIC_ARITHMETIC(op) \
	ldrex	r2, [r1]	/* load existing value and tag memory */	    ;\
	op	r3, r2, r0	/* compute new value */				    ;\
	strex	ip, r3, [r1]	/* store new value if memory is still tagged */	    ;\
	cmp	ip, #0		/* check if the store succeeded */		    ;\
	bne	1b		/* if not, try again */

#if defined(_ARM_ARCH_7)
/*
 * ARMv7 barrier operations:
 *  - Full Barrier (FB); store barrier before store exclusive, full barrier after op.
 */

#define ATOMIC_ARITHMETIC_FB(op) \
	dmb	ishst		/* store barrier before store exclusive */	    ;\
1:	_ATOMIC_ARITHMETIC(op)							    ;\
	dmb	ish		/* issue data memory barrier */			    ;\
	mov	r0, r3		/* return new value */

#define ATOMIC_ARITHMETIC_ORIG_FB(op) \
	dmb	ishst		/* store barrier before store exclusive */	    ;\
1:	_ATOMIC_ARITHMETIC(op)							    ;\
	dmb	ish		/* issue data memory barrier */			    ;\
	mov	r0, r2		/* return orig value */

#endif

/* 
 * For the non-MP ARMv7 cases, and ARMv5/6, these provide atomic arithmetic
 * without any barriers at all.
 */
#define ATOMIC_ARITHMETIC(op) \
1:	_ATOMIC_ARITHMETIC(op) ;\
	mov	r0, r3		/* return new value */

#define ATOMIC_ARITHMETIC_ORIG(op) \
	1:	_ATOMIC_ARITHMETIC(op) ;\
	mov	r0, r2		/* return orig value */

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicAdd32Barrier, mp)
	ATOMIC_ARITHMETIC_FB(add)
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicAdd32Barrier, up)
ENTRY_POINT(_OSAtomicAdd32)
	ATOMIC_ARITHMETIC(add)
	bx	lr

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicOr32Barrier, mp)
	ATOMIC_ARITHMETIC_FB(orr)
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicOr32Barrier, up)
ENTRY_POINT(_OSAtomicOr32)
	ATOMIC_ARITHMETIC(orr)
	bx	lr

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicOr32OrigBarrier, mp)
	ATOMIC_ARITHMETIC_ORIG_FB(orr)
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicOr32OrigBarrier, up)
ENTRY_POINT(_OSAtomicOr32Orig)
	ATOMIC_ARITHMETIC_ORIG(orr)
	bx	lr

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicAnd32Barrier, mp)
	ATOMIC_ARITHMETIC_FB(and)
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicAnd32Barrier, up)
ENTRY_POINT(_OSAtomicAnd32)
	ATOMIC_ARITHMETIC(and)
	bx	lr

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicAnd32OrigBarrier, mp)
	ATOMIC_ARITHMETIC_ORIG_FB(and)
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicAnd32OrigBarrier, up)
ENTRY_POINT(_OSAtomicAnd32Orig)
	ATOMIC_ARITHMETIC_ORIG(and)
	bx	lr

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicXor32Barrier, mp)
	ATOMIC_ARITHMETIC_FB(eor)
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicXor32Barrier, up)
ENTRY_POINT(_OSAtomicXor32)
	ATOMIC_ARITHMETIC(eor)
	bx	lr

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicXor32OrigBarrier, mp)
	ATOMIC_ARITHMETIC_ORIG_FB(eor)
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicXor32OrigBarrier, up)
ENTRY_POINT(_OSAtomicXor32Orig)
	ATOMIC_ARITHMETIC_ORIG(eor)
	bx	lr


#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicCompareAndSwap32Barrier, mp)
ENTRY_POINT_RESOLVER(_OSAtomicCompareAndSwapIntBarrier, mp)
ENTRY_POINT_RESOLVER(_OSAtomicCompareAndSwapLongBarrier, mp)
ENTRY_POINT_RESOLVER(_OSAtomicCompareAndSwapPtrBarrier, mp)
	ldrex	r3, [r2]	// load existing value and tag memory
	teq	r3, r0		// is it the same as oldValue?
	movne	r0, #0		// if not, return 0 immediately
	bxne	lr	    
	dmb	ishst		// store barrier before store exclusive
	strex	r3, r1, [r2]	// otherwise, try to store new value
	cmp	r3, #0		// check if the store succeeded
	bne	2f		// if not, try again
1:	dmb	ish		// memory barrier
	mov	r0, #1		// return true
	bx	lr
2:	ldrex	r3, [r2]	// load existing value and tag memory
	teq	r3, r0		// is it the same as oldValue?
	movne	r0, #0		// if not, return 0 immediately
	bxne	lr	    
	strex	r3, r1, [r2]	// otherwise, try to store new value
	cmp	r3, #0		// check if the store succeeded
	bne	2b		// if not, try again
	b	1b		// return
#endif

ENTRY_POINT_DEFAULT(_OSAtomicCompareAndSwap32Barrier, up)
ENTRY_POINT_DEFAULT(_OSAtomicCompareAndSwapIntBarrier, up)
ENTRY_POINT_DEFAULT(_OSAtomicCompareAndSwapLongBarrier, up)
ENTRY_POINT_DEFAULT(_OSAtomicCompareAndSwapPtrBarrier, up)
ENTRY_POINT(_OSAtomicCompareAndSwap32)
ENTRY_POINT(_OSAtomicCompareAndSwapInt)
ENTRY_POINT(_OSAtomicCompareAndSwapLong)
ENTRY_POINT(_OSAtomicCompareAndSwapPtr)
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
#define _BITOP(op)	\
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

#define ATOMIC_BITOP(op)    \
	_BITOP(op)								    ;\
1:										    ;\
	ldrex	r2, [r1]	/* load existing value and tag memory */	    ;\
	op	r3, r2, r0	/* compute new value */				    ;\
	strex	ip, r3, [r1]	/* attempt to store new value */		    ;\
	cmp	ip, #0		/* check if the store succeeded */		    ;\
	bne	1b		/* if not, try again */				    ;\
	ands	r0, r2, r0	/* mask off the bit from the old value */	    ;\
	movne	r0, #1		/* if non-zero, return exactly 1 */
	
#if defined(_ARM_ARCH_7)
#define ATOMIC_BITOP_FB(op)    \
	_BITOP(op)								    ;\
	dmb	ishst		/* store barrier before store exclusive */	    ;\
1:	ldrex	r2, [r1]	/* load existing value and tag memory */	    ;\
	op	r3, r2, r0	/* compute new value */				    ;\
	strex	ip, r3, [r1]	/* attempt to store new value */		    ;\
	cmp	ip, #0		/* check if the store succeeded */		    ;\
	bne	1b		/* if not, try again */				    ;\
	dmb	ish		/* memory barrier */				    ;\
	ands	r0, r2, r0	/* mask off the bit from the old value */	    ;\
	movne	r0, #1		/* if non-zero, return exactly 1 */
#endif

#if defined(_ARM_ARCH_7)	
ENTRY_POINT_RESOLVER(_OSAtomicTestAndSetBarrier, mp)
	ATOMIC_BITOP_FB(orr)
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicTestAndSetBarrier, up)
ENTRY_POINT(_OSAtomicTestAndSet)
	ATOMIC_BITOP(orr)
	bx	lr

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicTestAndClearBarrier, mp)
	ATOMIC_BITOP_FB(bic)
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicTestAndClearBarrier, up)
ENTRY_POINT(_OSAtomicTestAndClear)
	ATOMIC_BITOP(bic)
	bx	lr

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSMemoryBarrier, mp)
	dmb	ish
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_OSMemoryBarrier, up)
	bx	lr

/* void  OSAtomicEnqueue( OSQueueHead *__list, void *__new, size_t __offset); */
#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicEnqueue, mp)
        dmb     ishst
1:      ldrex   r3, [r0]        // get link to 1st on list
        str     r3, [r1, r2]    // hang list off new node
        strex   r3, r1, [r0]    // make new 1st on list
        cmp     r3, #0
        bne     1b
        dmb     ish
        bx      lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicEnqueue, up)
1:      ldrex   r3, [r0]        // get link to 1st on list
        str     r3, [r1, r2]    // hang list off new node
        strex   r3, r1, [r0]    // make new 1st on list
        cmp     r3, #0
        bne     1b
        bx      lr
        
/* void* OSAtomicDequeue( OSQueueHead *list, size_t offset); */
#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicDequeue, mp)
        mov     r2, r0
        dmb     ishst
1:      ldrex   r0, [r2]        // get 1st in list
        cmp     r0, #0          // null?
        bxeq    lr              // yes, list empty
        ldr     r3, [r0, r1]    // get 2nd
        strex   ip, r3, [r2]    // make 2nd first
        cmp     ip, #0
        bne     1b
        dmb     ish
        bx      lr
#endif

ENTRY_POINT_DEFAULT(_OSAtomicDequeue, up)
        mov     r2, r0
1:      ldrex   r0, [r2]        // get 1st in list
        cmp     r0, #0          // null?
        bxeq    lr              // yes, list empty
        ldr     r3, [r0, r1]    // get 2nd
        strex   ip, r3, [r2]    // make 2nd first
        cmp     ip, #0
        bne     1b
        bx      lr

#if defined(_ARM_ARCH_6K)
/* If we can use LDREXD/STREXD, then we can implement 64-bit atomic operations */

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicAdd64Barrier, mp)
	// R0,R1 contain the amount to add
	// R2 contains the pointer
	stmfd	sp!, {r4, r5, r8, r9, lr}
	dmb	ishst		// store memory barrier before store exclusive
1:	ldrexd	r4, r5, [r2]	// load existing value to R4/R5 and tag memory
	adds	r8, r4, r0	// add lower half of new value into R8 and set carry bit
	adc	r9, r5, r1	// add upper half of new value into R9 with carry
	strexd	r3, r8, r9, [r2]	// store new value if memory is still tagged
	cmp	r3, #0		// check if store succeeded
	bne	1b		// if not, try again
	dmb	ish		// memory barrier
	mov	r0, r8		// return new value
	mov	r1, r9
	ldmfd	sp!, {r4, r5, r8, r9, pc}
#endif

ENTRY_POINT_DEFAULT(_OSAtomicAdd64Barrier, up)
ENTRY_POINT(_OSAtomicAdd64)
	// R0,R1 contain the amount to add
	// R2 contains the pointer
	stmfd	sp!, {r4, r5, r8, r9, lr}
1:	ldrexd	r4, r5, [r2]	// load existing value to R4/R5 and tag memory
	adds	r8, r4, r0	// add lower half of new value into R8 and set carry bit
	adc	r9, r5, r1	// add upper half of new value into R9 with carry
	strexd	r3, r8, r9, [r2]	// store new value if memory is still tagged
	cmp	r3, #0		// check if store succeeded
	bne	1b		// if not, try again
	mov	r0, r8		// return new value
	mov	r1, r9
	ldmfd	sp!, {r4, r5, r8, r9, pc}
	
#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_OSAtomicCompareAndSwap64Barrier, mp)
	// R0,R1 contains the old value
	// R2,R3 contains the new value
	// the pointer is pushed onto the stack
	ldr	ip, [sp, #0]	// load pointer into IP
	stmfd	sp!, {r4, r5, lr}
	ldrexd	r4, [ip]	// load existing value into R4/R5 and tag memory
	teq	r0, r4		// check low word
	teqeq	r1, r5		// if low words match, check high word
	movne	r0, #0		// if either match fails, return 0
	bne	2f
	dmb	ishst		// store barrier before store exclusive
	strexd	r4, r2, [ip]	// otherwise, try to store new values
	cmp	r4, #0		// check if store succeeded
	bne	3f		// if not, try again
1:	dmb	ish		// memory barrier
	mov	r0, #1		// return true
2:	ldmfd	sp!, {r4, r5, pc}
3:	ldrexd	r4, [ip]	// load existing value into R4/R5 and tag memory
	teq	r0, r4		// check low word
	teqeq	r1, r5		// if low words match, check high word
	movne	r0, #0		// if either match fails, return 0
	bne	2b
	strexd	r4, r2, [ip]	// otherwise, try to store new values
	cmp	r4, #0		// check if store succeeded
	bne	3f		// if not, try again
	b	1b		// return
#endif

ENTRY_POINT_DEFAULT(_OSAtomicCompareAndSwap64Barrier, up)
ENTRY_POINT(_OSAtomicCompareAndSwap64)
	// R0,R1 contains the old value
	// R2,R3 contains the new value
	// the pointer is pushed onto the stack
	ldr	ip, [sp, #0]	// load pointer into IP
	stmfd	sp!, {r4, r5, lr}
1:	ldrexd	r4, [ip]	// load existing value into R4/R5 and tag memory
	teq	r0, r4		// check low word
	teqeq	r1, r5		// if low words match, check high word
	movne	r0, #0		// if either match fails, return 0
	bne	2f
	strexd	r4, r2, [ip]	// otherwise, try to store new values
	cmp	r4, #0		// check if store succeeded
	bne	1b		// if not, try again
	mov	r0, #1		// return true
2:	ldmfd	sp!, {r4, r5, pc}
			
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

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_spin_lock, mp)
ENTRY_POINT_RESOLVER(__spin_lock, mp)
ENTRY_POINT_RESOLVER(_OSSpinLockLock, mp)
	mov	r1, #1
1:	ldrex	r2, [r0]	// load the value of [r0] into r2
	cmp	r2, #0		// compare the lock value to zero
	bne	2f		// jump to the spin if we don't own the lock
	strex	r3, r1, [r0]	// try to store the one
	cmp	r3, #0		// test to see if we stored our value
	bne	2f		// if not, jump to the spin too
	dmb	ish		// memory barrier if we acquired the lock
	bx	lr		// and return
2:	mov	r3, $(MP_SPIN_TRIES)	// load up r3 with spin counter
3:	ldr	r2, [r0]	// load the lock
	cmp	r2, #0		// if unlocked
	beq	1b		// then go back to the top
	subs	r3, r3, #1	// counter--
	bne	3b		// if nonzero, back to 3:
	
	mov	r3, r0		// r0 is clobbered by the syscall return value
	mov	r0, #0		// THREAD_NULL
				// SWITCH_OPTION_DEPRESS (r1==1 already)
	mov	r2, #1		// timeout (ms)
	mov	r12, #-61	// SYSCALL_THREAD_SWITCH
	swi	0x80
	mov	r0, r3		// restore state of r0
	b	1b

#if !defined(VARIANT_DYLD)
/*
 This sucks from a code sharing PoV. The only difference in this version is
 the presence of a WFE instruction in the spin loop. This is only used on
 CPU's which get woken up regularly out of WFE waits.

 Additionally, completely compiled out of the dyld variant so we can easily
 use macros to pick the normal MP version for dyld on armv7 platforms.
 */
ENTRY_POINT_RESOLVER(_spin_lock, wfe)
ENTRY_POINT_RESOLVER(__spin_lock, wfe)
ENTRY_POINT_RESOLVER(_OSSpinLockLock, wfe)
	mov	r1, #1
1:	ldrex	r2, [r0]	// load the value of [r0] into r2
	cmp	r2, #0		// compare the lock value to zero
	bne	2f		// jump to the spin if we don't own the lock
	strex	r3, r1, [r0]	// try to store the one
	cmp	r3, #0		// test to see if we stored our value
	bne 	2f		// if not, jump to the spin too
	dmb	ish		// memory barrier if we acquired the lock
	bx	lr		// and return
2:	mov	r3, $(MP_SPIN_TRIES_WFE)	// load up r3 with spin counter
3:	wfe			// sleepy time
	ldr	r2, [r0]	// load the lock
	cmp	r2, #0		// if unlocked
	beq	1b		// then go back to the top
	subs	r3, r3, #1	// counter--
	bne	3b		// if nonzero, back to 3:

	mov	r3, r0		// r0 is clobbered by the syscall return value
	mov	r0, #0		// THREAD_NULL
				// SWITCH_OPTION_DEPRESS (r1==1 already)
	mov	r2, #1		// timeout (ms)
	mov	r12, #-61	// SYSCALL_THREAD_SWITCH
	swi	0x80
	mov	r0, r3		// restore state of r0
	b	1b
#endif // VARIANT_DYLD
#endif // _ARM_ARCH_7

ENTRY_POINT_DEFAULT(_spin_lock, up)
ENTRY_POINT_DEFAULT(__spin_lock, up)
ENTRY_POINT_DEFAULT(_OSSpinLockLock, up)
	mov	r1, #1
1:
#if !defined(_ARM_ARCH_7)
	swp	r2, r1, [r0]
	cmp	r2, #0
#else
	ldrex	r2, [r0]	// load the value of [r0] into r2
	cmp	r2, #0		// compare the lock value to zero
	bne	2f		// jump to the spin if we don't own the lock
	strex	r3, r1, [r0]	// try to store the one
	cmp	r3, #0		// test to see if we stored our value
#endif // !_ARM_ARCH_6
	bxeq	lr		// if so, return
2:	mov	r3, r0		// r0 is clobbered by the syscall return value
	mov	r0, #0		// THREAD_NULL
				// SWITCH_OPTION_DEPRESS (r1==1 already)
	mov	r2, #1		// timeout (ms)
	mov	r12, #-61	// SYSCALL_THREAD_SWITCH
	swi	0x80
	mov	r0, r3		// restore state of r0
	b	1b

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_spin_lock_try, mp)
ENTRY_POINT_RESOLVER(__spin_lock_try, mp)
ENTRY_POINT_RESOLVER(_OSSpinLockTry, mp)
	mov	r1, #1
1:	ldrex	r2, [r0]
	strex	r3, r1, [r0]
	cmp	r3, #0
	bne	1b
	dmb	ish
	bic	r0, r1, r2
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_spin_lock_try, up)
ENTRY_POINT_DEFAULT(__spin_lock_try, up)
ENTRY_POINT_DEFAULT(_OSSpinLockTry, up)
	mov	r1, #1
#if !defined(_ARM_ARCH_7)
	swp	r2, r1, [r0]
#else
1:	ldrex	r2, [r0]
	strex	r3, r1, [r0]
	cmp	r3, #0
	bne	1b
#endif // !_ARM_ARCH_6
	bic	r0, r1, r2
	bx	lr

/*
 * void
 * _spin_unlock(p)
 *      int *p;
 *
 * Unlock the lock pointed to by p.
 */

#if defined(_ARM_ARCH_7)
ENTRY_POINT_RESOLVER(_spin_unlock, mp)
ENTRY_POINT_RESOLVER(__spin_unlock, mp)
ENTRY_POINT_RESOLVER(_OSSpinLockUnlock, mp)
	mov	r1, #0
	dmb	ish		// barrier so that previous accesses are observed before unlock
1:	ldrex	r2, [r0]	// load the lock to get exclusive access
	strex	r3, r1, [r0]	// strex is instantly visible to (at least) {st,ld}rex
	cmp	r3, #0		// did the unlock succeed?
	bne	1b		// if not, try try again.
	bx	lr
#endif

ENTRY_POINT_DEFAULT(_spin_unlock, up)
ENTRY_POINT_DEFAULT(__spin_unlock, up)
ENTRY_POINT_DEFAULT(_OSSpinLockUnlock, up)
	mov	r1, #0
#if !defined(_ARM_ARCH_7)
	str	r1, [r0]
#else
1:	ldrex	r2, [r0]	// load the lock to get exclusive access
	strex	r3, r1, [r0]	// store zero to the lock
	cmp	r3, #0		// did the unlock succeed?
	bne	1b		// if not, try try again.
#endif // !_ARM_ARCH_6
	bx	lr
