/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * global_lock is a pointer to the global lock for the dynamic link editor data
 * structures.  global_lock is initlized to the address of the lock data.  What
 * is pointed to by global_lock is passed to try_to_get_lock() and clear_lock()
 * below.  The lock itself is initialized to the unlocked state.
 */
	.data
	.align 2
	.globl	_global_lock
_global_lock:
	.long	lock

	.globl	_debug_thread_lock
_debug_thread_lock:
	.long	debug_lock
/*
 * To allow the DYLD_MEM_PROTECT feature to work a segment containing just the
 * data for a lock is used.  When this feature on the contents of the lock that
 * is pointed to by global_lock is copied to mem_prot_lock.  Then the address of
 * mem_prot_lock is stored in global_lock.  This allows dyld's original data
 * segment to be vm_protect()'ed.
 */
	.globl	_mem_prot_lock
	.zerofill __LOCK, __mem_prot_lock, _mem_prot_lock, 32, 5
	.globl	_mem_prot_debug_lock
	.zerofill __LOCK, __mem_prot_lock, _mem_prot_debug_lock, 32, 5

	.data
#ifdef m68k
	.align	2
	.globl	lock
lock:
	.long	0	/* 0 is unlocked, non-zero is locked */
	.align	2
	.globl	debug_lock
debug_lock:
	.long	0	/* 0 is unlocked, non-zero is locked */
#endif
#ifdef __i386__
	.align	2
	.globl	lock
lock:
	.long	0	/* 0 is unlocked, 1 is locked */
	.globl	debug_lock
debug_lock:
	.long	0	/* 0 is unlocked, 1 is locked */
#endif
#ifdef hppa
	.align	4	/* locks must be 16 byte aligned for hppa */
	.globl	lock
lock:
	.long	1	/* 1 is unlocked, 0 is locked */
	.align	4	/* locks must be 16 byte aligned for hppa */
	.globl	debug_lock
debug_lock:
	.long	1	/* 1 is unlocked, 0 is locked */
#endif
#ifdef sparc
	.align	2
	.globl	lock
lock:
	.long	0	/* 0 is unlocked, 1 is locked */
	.align	2
	.globl	debug_lock
debug_lock:
	.long	0	/* 0 is unlocked, 1 is locked */
#endif
#ifdef __ppc__
	.align	5	/* locks must be 32 byte aligned for ppc */
	.globl	lock
lock:
	.long	0	/* 0 is unlocked, 1 is locked */
	.fill	28,1,0  /* fill remaining 28 bytes with unused data */

	.align	5	/* locks must be 32 byte aligned for ppc */
	.globl	debug_lock
debug_lock:
	.long	0	/* 0 is unlocked, 1 is locked */
	.fill	28,1,0  /* fill remaining 28 bytes with unused data */
#endif

/*
 * try_to_get_lock() is passed the address of a lock and trys to take it out.
 * It returns TRUE if it got the lock and FALSE if it didn't.
 */
	.text
#ifdef m68k
	.even
	.globl	_try_to_get_lock
_try_to_get_lock:
	movl	sp@(4),a0
	tas	a0@
	bne	1f
	moveq	#1,d0
	rts
1:
	clrl	d0
	rts
#endif
#ifdef __i386__
	.align	2, 0x90
	.globl	_try_to_get_lock
_try_to_get_lock:
#ifdef OLD
	movl	4(%esp),%ecx
    lock/bts	$1,(%ecx)
	jc	1f
	movl	$1,%eax
	ret
1:
	movl	$0,%eax
	ret
#else /* NEW */
	movl	4(%esp),%ecx
	movl	$1,%eax
	xchgl	(%ecx),%eax
	xorl	$1,%eax
	ret
#endif /* OLD-NEW */
#endif
#ifdef hppa
	.align	2
	.globl	_try_to_get_lock
_try_to_get_lock:
	bv	0(%r2)
	ldcwx	0(0,%r26),%r28
#endif
#ifdef sparc
	.align	2
	.globl	_try_to_get_lock
_try_to_get_lock:
	set	1,%o1
	swap	[%o0],%o1
	retl
	xor	%o1,1,%o0
#endif
#ifdef __ppc__
	.align	2
	.globl	_try_to_get_lock
_try_to_get_lock:
	li	r4,1	; Lock value
1:
	lwarx   r5,0,r3	; Read the lock and reserve
	stwcx.  r4,0,r3	; Try to lock the lock
	bne-    1b	; Lost reservation, try again
	isync		; Workaround some buggy CPUs
	xori    r3,r5,1	; Return opposite of what read
	blr
#endif

/*
 * lock_is_set() is passed the address of a lock and returns non-zero if the
 * lock is locked and zero if it is not.
 */
	.text
#ifdef m68k
	.even
	.globl	_lock_is_set
_lock_is_set:
	movl	sp@(4),a0
	movl	a0@,d0
	roll	#1,d0
	rts
#endif
#ifdef __i386__
	.align	2, 0x90
	.globl	_lock_is_set
_lock_is_set:
	movl	$0,%eax
	movl	4(%esp),%ecx
	cmpl    $0,(%ecx)
	setne	%al
	ret
#endif
#ifdef hppa
	.align	2
.globl _lock_is_set
_lock_is_set:
	ldw	0(0,%r26),%r28
	comiclr,<> 0,%r28,%r28
	ldi	1,%r28
	bv,n	0(%r2)
#endif
#ifdef sparc
	.align	2
.globl _lock_is_set
_lock_is_set:
	retl
	ld	[%o0],%o0
#endif
#ifdef __ppc__
	.align	2
.globl _lock_is_set
_lock_is_set:
	lwz	r3,0(r3) ; move lock value to return value as it is 1 or 0
	blr
#endif

/*
 * clear_lock() is passed the address of a lock and sets it to the unlocked
 * value.
 */
	.text
#ifdef m68k
	.even
	.globl	_clear_lock
_clear_lock:
	movel	sp@(4),a0
	clrl	a0@
	rts
#endif
#ifdef __i386__
	.align	2, 0x90
	.globl	_clear_lock
_clear_lock:
	movl    4(%esp),%eax
	movl    $0,(%eax)
	ret
#endif
#ifdef hppa
	.align	2
	.globl	_clear_lock
_clear_lock:
	ldi	1,%r19
	bv	0(%r2)
	stw	%r19,0(%r26)
#endif
#ifdef sparc
	.align	2
	.globl	_clear_lock
_clear_lock:
	retl
	swap	[%o0],%g0
#endif
#ifdef __ppc__
	.align	2
	.globl	_clear_lock
_clear_lock:
	li	r4,0      ; the unlock value
	sync		  ; insure that all previous stores are done
	stw	r4,0(r3)  ; unlock it
	blr
#endif
