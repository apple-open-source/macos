/*
 * Copyright (c) 1998-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <IOKit/system.h>

#include <IOKit/IOReturn.h>
#include <IOKit/IOLib.h>
#include <IOKit/assert.h>

#include <IOKit/IOLocksPrivate.h>

extern "C" {
#include <kern/locks.h>

#if defined(__x86_64__)
/* Synthetic event if none is specified, for backwards compatibility only. */
static bool IOLockSleep_NO_EVENT __attribute__((used)) = 0;
#endif

void
IOLockInitWithState( IOLock * lock, IOLockState state)
{
	if (state == kIOLockStateLocked) {
		lck_mtx_lock( lock);
	}
}

IOLock *
IOLockAlloc( void )
{
	return lck_mtx_alloc_init(IOLockGroup, LCK_ATTR_NULL);
}

void
IOLockInlineInit( IOLock *lock )
{
	lck_mtx_init(lock, IOLockGroup, LCK_ATTR_NULL);
}

void
IOLockInlineDestroy( IOLock * lock)
{
	lck_mtx_destroy( lock, IOLockGroup);
}

void
IOLockFree( IOLock * lock)
{
	lck_mtx_free( lock, IOLockGroup);
}

lck_mtx_t *
IOLockGetMachLock( IOLock * lock)
{
	return (lck_mtx_t *)lock;
}

int
IOLockSleep( IOLock * lock, void *event, UInt32 interType)
{
	return (int) lck_mtx_sleep(lock, LCK_SLEEP_PROMOTED_PRI, (event_t) event, (wait_interrupt_t) interType);
}

int
IOLockSleepDeadline( IOLock * lock, void *event,
    AbsoluteTime deadline, UInt32 interType)
{
	return (int) lck_mtx_sleep_deadline(lock, LCK_SLEEP_PROMOTED_PRI, (event_t) event,
	           (wait_interrupt_t) interType, __OSAbsoluteTime(deadline));
}

int
IOLockSleepWithInheritor( IOLock *lock, UInt32 lck_sleep_action,
    void *event, thread_t inheritor, UInt32 interType, uint64_t deadline)
{
	return (int) lck_mtx_sleep_with_inheritor(lock, (lck_sleep_action_t) lck_sleep_action, (event_t) event, inheritor,
	           (wait_interrupt_t) interType, deadline);
}

void
IOLockWakeup(IOLock * lock, void *event, bool oneThread)
{
	thread_wakeup_prim((event_t) event, oneThread, THREAD_AWAKENED);
}

void
IOLockWakeupAllWithInheritor(IOLock * lock, void *event)
{
	wakeup_all_with_inheritor((event_t) event, THREAD_AWAKENED);
}


#if defined(__x86_64__)
/*
 * For backwards compatibility, kexts built against pre-Darwin 14 headers will bind at runtime to this function,
 * which supports a NULL event,
 */
int     IOLockSleep_legacy_x86_64( IOLock * lock, void *event, UInt32 interType) __asm("_IOLockSleep");
int     IOLockSleepDeadline_legacy_x86_64( IOLock * lock, void *event,
    AbsoluteTime deadline, UInt32 interType) __asm("_IOLockSleepDeadline");
void    IOLockWakeup_legacy_x86_64(IOLock * lock, void *event, bool oneThread) __asm("_IOLockWakeup");

int
IOLockSleep_legacy_x86_64( IOLock * lock, void *event, UInt32 interType)
{
	if (event == NULL) {
		event = (void *)&IOLockSleep_NO_EVENT;
	}

	return IOLockSleep(lock, event, interType);
}

int
IOLockSleepDeadline_legacy_x86_64( IOLock * lock, void *event,
    AbsoluteTime deadline, UInt32 interType)
{
	if (event == NULL) {
		event = (void *)&IOLockSleep_NO_EVENT;
	}

	return IOLockSleepDeadline(lock, event, deadline, interType);
}

void
IOLockWakeup_legacy_x86_64(IOLock * lock, void *event, bool oneThread)
{
	if (event == NULL) {
		event = (void *)&IOLockSleep_NO_EVENT;
	}

	IOLockWakeup(lock, event, oneThread);
}
#endif /* defined(__x86_64__) */


struct _IORecursiveLock {
	lck_mtx_t       mutex;
	lck_grp_t       *group;
	thread_t        thread;
	UInt32          count;
};

IORecursiveLock *
IORecursiveLockAllocWithLockGroup( lck_grp_t * lockGroup )
{
	_IORecursiveLock * lock;

	if (lockGroup == NULL) {
		return NULL;
	}

	lock = IOMallocType( _IORecursiveLock );
	if (!lock) {
		return NULL;
	}

	lck_mtx_init( &lock->mutex, lockGroup, LCK_ATTR_NULL );
	lock->group = lockGroup;
	lock->thread = NULL;
	lock->count  = 0;

	return (IORecursiveLock *) lock;
}


IORecursiveLock *
IORecursiveLockAlloc( void )
{
	return IORecursiveLockAllocWithLockGroup( IOLockGroup );
}

void
IORecursiveLockFree( IORecursiveLock * _lock )
{
	_IORecursiveLock * lock = (_IORecursiveLock *)_lock;

	lck_mtx_destroy(&lock->mutex, lock->group);
	IOFreeType( lock, _IORecursiveLock );
}

lck_mtx_t *
IORecursiveLockGetMachLock( IORecursiveLock * lock )
{
	return &lock->mutex;
}

void
IORecursiveLockLock( IORecursiveLock * _lock)
{
	_IORecursiveLock * lock = (_IORecursiveLock *)_lock;

	if (lock->thread == IOThreadSelf()) {
		lock->count++;
	} else {
		lck_mtx_lock( &lock->mutex );
		assert( lock->thread == NULL );
		assert( lock->count == 0 );
		lock->thread = IOThreadSelf();
		lock->count = 1;
	}
}

boolean_t
IORecursiveLockTryLock( IORecursiveLock * _lock)
{
	_IORecursiveLock * lock = (_IORecursiveLock *)_lock;

	if (lock->thread == IOThreadSelf()) {
		lock->count++;
		return true;
	} else {
		if (lck_mtx_try_lock( &lock->mutex )) {
			assert( lock->thread == NULL );
			assert( lock->count == 0 );
			lock->thread = IOThreadSelf();
			lock->count = 1;
			return true;
		}
	}
	return false;
}

void
IORecursiveLockUnlock( IORecursiveLock * _lock)
{
	_IORecursiveLock * lock = (_IORecursiveLock *)_lock;

	assert( lock->thread == IOThreadSelf());

	if (0 == (--lock->count)) {
		lock->thread = NULL;
		lck_mtx_unlock( &lock->mutex );
	}
}

boolean_t
IORecursiveLockHaveLock( const IORecursiveLock * _lock)
{
	_IORecursiveLock * lock = (_IORecursiveLock *)_lock;

	return lock->thread == IOThreadSelf();
}

int
IORecursiveLockSleep(IORecursiveLock *_lock, void *event, UInt32 interType)
{
	_IORecursiveLock * lock = (_IORecursiveLock *)_lock;
	UInt32 count = lock->count;
	int res;

	assert(lock->thread == IOThreadSelf());

	lock->count = 0;
	lock->thread = NULL;
	res = lck_mtx_sleep(&lock->mutex, LCK_SLEEP_PROMOTED_PRI, (event_t) event, (wait_interrupt_t) interType);

	// Must re-establish the recursive lock no matter why we woke up
	// otherwise we would potentially leave the return path corrupted.
	assert(lock->thread == NULL);
	assert(lock->count == 0);
	lock->thread = IOThreadSelf();
	lock->count = count;
	return res;
}

int
IORecursiveLockSleepDeadline( IORecursiveLock * _lock, void *event,
    AbsoluteTime deadline, UInt32 interType)
{
	_IORecursiveLock * lock = (_IORecursiveLock *)_lock;
	UInt32 count = lock->count;
	int res;

	assert(lock->thread == IOThreadSelf());

	lock->count = 0;
	lock->thread = NULL;
	res = lck_mtx_sleep_deadline(&lock->mutex, LCK_SLEEP_PROMOTED_PRI, (event_t) event,
	    (wait_interrupt_t) interType, __OSAbsoluteTime(deadline));

	// Must re-establish the recursive lock no matter why we woke up
	// otherwise we would potentially leave the return path corrupted.
	assert(lock->thread == NULL);
	assert(lock->count == 0);
	lock->thread = IOThreadSelf();
	lock->count = count;
	return res;
}

void
IORecursiveLockWakeup(IORecursiveLock *, void *event, bool oneThread)
{
	thread_wakeup_prim((event_t) event, oneThread, THREAD_AWAKENED);
}

/*
 * Complex (read/write) lock operations
 */

IORWLock *
IORWLockAlloc( void )
{
	return lck_rw_alloc_init(IOLockGroup, LCK_ATTR_NULL);
}

void
IORWLockInlineInit( IORWLock *lock )
{
	lck_rw_init(lock, IOLockGroup, LCK_ATTR_NULL);
}

void
IORWLockInlineDestroy( IORWLock * lock)
{
	lck_rw_destroy( lock, IOLockGroup);
}

void
IORWLockFree( IORWLock * lock)
{
	lck_rw_free( lock, IOLockGroup);
}

lck_rw_t *
IORWLockGetMachLock( IORWLock * lock)
{
	return (lck_rw_t *)lock;
}


/*
 * Spin locks
 */

IOSimpleLock *
IOSimpleLockAlloc( void )
{
	return lck_spin_alloc_init( IOLockGroup, LCK_ATTR_NULL);
}

void
IOSimpleLockInit( IOSimpleLock * lock)
{
	lck_spin_init( lock, IOLockGroup, LCK_ATTR_NULL);
}

void
IOSimpleLockDestroy( IOSimpleLock * lock )
{
	lck_spin_destroy(lock, IOLockGroup);
}

void
IOSimpleLockFree( IOSimpleLock * lock )
{
	lck_spin_free( lock, IOLockGroup);
}

lck_spin_t *
IOSimpleLockGetMachLock( IOSimpleLock * lock)
{
	return (lck_spin_t *)lock;
}

#ifndef IOLOCKS_INLINE
/*
 * Lock assertions
 */

void
IOLockAssert(IOLock * lock, IOLockAssertState type)
{
	LCK_MTX_ASSERT(lock, type);
}

void
IORWLockAssert(IORWLock * lock, IORWLockAssertState type)
{
	LCK_RW_ASSERT(lock, type);
}

void
IOSimpleLockAssert(IOSimpleLock *lock, IOSimpleLockAssertState type)
{
	LCK_SPIN_ASSERT(l, type);
}
#endif /* !IOLOCKS_INLINE */
} /* extern "C" */
