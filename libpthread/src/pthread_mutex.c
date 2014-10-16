/*
 * Copyright (c) 2000-2003, 2007, 2008 Apple Inc. All rights reserved.
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
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/*
 * MkLinux
 */

/*
 * POSIX Pthread Library
 * -- Mutex variable support
 */

#include "internal.h"
#include "kern/kern_trace.h"
#include <sys/syscall.h>

#ifdef PLOCKSTAT
#include "plockstat.h"
#else /* !PLOCKSTAT */
#define	PLOCKSTAT_MUTEX_SPIN(x)
#define	PLOCKSTAT_MUTEX_SPUN(x, y, z)
#define	PLOCKSTAT_MUTEX_ERROR(x, y)
#define	PLOCKSTAT_MUTEX_BLOCK(x)
#define	PLOCKSTAT_MUTEX_BLOCKED(x, y)
#define	PLOCKSTAT_MUTEX_ACQUIRE(x, y, z)
#define	PLOCKSTAT_MUTEX_RELEASE(x, y)
#endif /* PLOCKSTAT */

extern int __unix_conforming;

#ifndef BUILDING_VARIANT
PTHREAD_NOEXPORT int __mtx_markprepost(_pthread_mutex *mutex, uint32_t oupdateval, int firstfit);
#endif /* BUILDING_VARIANT */

#define DEBUG_TRACE_POINTS 0

#if DEBUG_TRACE_POINTS
extern int __syscall(int number, ...);
#define DEBUG_TRACE(x, a, b, c, d) __syscall(SYS_kdebug_trace, TRACE_##x, a, b, c, d)
#else
#define DEBUG_TRACE(x, a, b, c, d) do { } while(0)
#endif

#include <machine/cpu_capabilities.h>

static int _pthread_mutex_init(_pthread_mutex *mutex, const pthread_mutexattr_t *attr, uint32_t static_type);

#if !__LITTLE_ENDIAN__
#error MUTEX_GETSEQ_ADDR assumes little endian layout of 2 32-bit sequence words
#endif

static void
MUTEX_GETSEQ_ADDR(_pthread_mutex *mutex,
		  volatile uint64_t **seqaddr)
{
	if (mutex->mtxopts.options.misalign) {
		*seqaddr = (volatile uint64_t *)&mutex->m_seq[1];
	} else {
		*seqaddr = (volatile uint64_t *)&mutex->m_seq[0];
	}
}

static void
MUTEX_GETTID_ADDR(_pthread_mutex *mutex,
				  volatile uint64_t **tidaddr)
{
	if (mutex->mtxopts.options.misalign) {
		*tidaddr = (volatile uint64_t *)&mutex->m_tid[1];
	} else {
		*tidaddr = (volatile uint64_t *)&mutex->m_tid[0];
	}
}

#ifndef BUILDING_VARIANT /* [ */

#define BLOCK_FAIL_PLOCKSTAT    0
#define BLOCK_SUCCESS_PLOCKSTAT 1

/* This function is never called and exists to provide never-fired dtrace
 * probes so that user d scripts don't get errors.
 */
__private_extern__ __attribute__((used)) void
_plockstat_never_fired(void) 
{
	PLOCKSTAT_MUTEX_SPIN(NULL);
	PLOCKSTAT_MUTEX_SPUN(NULL, 0, 0);
}


/*
 * Initialize a mutex variable, possibly with additional attributes.
 * Public interface - so don't trust the lock - initialize it first.
 */
int
pthread_mutex_init(pthread_mutex_t *omutex, const pthread_mutexattr_t *attr)
{
#if 0
	/* conformance tests depend on not having this behavior */
	/* The test for this behavior is optional */
	if (mutex->sig == _PTHREAD_MUTEX_SIG)
		return EBUSY;
#endif
	_pthread_mutex *mutex = (_pthread_mutex *)omutex;
	LOCK_INIT(mutex->lock);
	return (_pthread_mutex_init(mutex, attr, 0x7));
}

int
pthread_mutex_getprioceiling(const pthread_mutex_t *omutex, int *prioceiling)
{
	int res = EINVAL;
	_pthread_mutex *mutex = (_pthread_mutex *)omutex;
	if (mutex->sig == _PTHREAD_MUTEX_SIG) {
		LOCK(mutex->lock);
		*prioceiling = mutex->prioceiling;
		res = 0;
		UNLOCK(mutex->lock);
	}
	return res;
}

int
pthread_mutex_setprioceiling(pthread_mutex_t *omutex, int prioceiling, int *old_prioceiling)
{
	int res = EINVAL;
	_pthread_mutex *mutex = (_pthread_mutex *)omutex;
	if (mutex->sig == _PTHREAD_MUTEX_SIG) {
		LOCK(mutex->lock);
		if (prioceiling >= -999 || prioceiling <= 999) {
			*old_prioceiling = mutex->prioceiling;
			mutex->prioceiling = prioceiling;
			res = 0;
		}
		UNLOCK(mutex->lock);
	}
	return res;
}

int
pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr, int *prioceiling)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG) {
		*prioceiling = attr->prioceiling;
		res = 0;
	}
	return res;
}

int
pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr, int *protocol)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG) {
		*protocol = attr->protocol;
		res = 0;
	}
	return res;
}

int
pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG) {
		*type = attr->type;
		res = 0;
	}
	return res;
}

int
pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG) {
		*pshared = (int)attr->pshared;
		res = 0;
	}
	return res;
}

int
pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
	attr->prioceiling = _PTHREAD_DEFAULT_PRIOCEILING;
	attr->protocol = _PTHREAD_DEFAULT_PROTOCOL;
	attr->policy = _PTHREAD_MUTEX_POLICY_FAIRSHARE;
	attr->type = PTHREAD_MUTEX_DEFAULT;
	attr->sig = _PTHREAD_MUTEX_ATTR_SIG;
	attr->pshared = _PTHREAD_DEFAULT_PSHARED;
	return 0;
}

int
pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int prioceiling)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG) {
		if (prioceiling >= -999 || prioceiling <= 999) {
			attr->prioceiling = prioceiling;
			res = 0;
		}
	}
	return res;
}

int
pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG) {
		switch (protocol) {
			case PTHREAD_PRIO_NONE:
			case PTHREAD_PRIO_INHERIT:
			case PTHREAD_PRIO_PROTECT:
				attr->protocol = protocol;
				res = 0;
				break;
		}
	}
	return res;
}

int
pthread_mutexattr_setpolicy_np(pthread_mutexattr_t *attr, int policy)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG) {
		switch (policy) {
			case _PTHREAD_MUTEX_POLICY_FAIRSHARE:
			case _PTHREAD_MUTEX_POLICY_FIRSTFIT:
				attr->policy = policy;
				res = 0;
				break;
		}
	}
	return res;
}

int
pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG) {
		switch (type) {
			case PTHREAD_MUTEX_NORMAL:
			case PTHREAD_MUTEX_ERRORCHECK:
			case PTHREAD_MUTEX_RECURSIVE:
			//case PTHREAD_MUTEX_DEFAULT:
				attr->type = type;
				res = 0;
				break;
		}
	}
	return res;
}

// XXX remove
void
cthread_yield(void) 
{
	sched_yield();
}

void
pthread_yield_np(void) 
{
	sched_yield();
}


/*
 * Temp: till pshared is fixed correctly
 */
int
pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
	int res = EINVAL;
#if __DARWIN_UNIX03
	if (__unix_conforming == 0) {
		__unix_conforming = 1;
	}
#endif /* __DARWIN_UNIX03 */

	if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG) {
#if __DARWIN_UNIX03
		if (( pshared == PTHREAD_PROCESS_PRIVATE) || (pshared == PTHREAD_PROCESS_SHARED))
#else /* __DARWIN_UNIX03 */
		if ( pshared == PTHREAD_PROCESS_PRIVATE)
#endif /* __DARWIN_UNIX03 */
		{
			attr->pshared = pshared; 
			res = 0;
		}
	}
	return res;
}

/*
 * Sequence numbers and TID:
 *
 * In steady (and uncontended) state, an unlocked mutex will
 * look like A=[L4 U4 TID0]. When it is being locked, it transitions
 * to B=[L5+KE U4 TID0] and then C=[L5+KE U4 TID940]. For an uncontended mutex,
 * the unlock path will then transition to D=[L5 U4 TID0] and then finally
 * E=[L5 U5 TID0].
 *
 * If a contender comes in after B, the mutex will instead transition to E=[L6+KE U4 TID0]
 * and then F=[L6+KE U4 TID940]. If a contender comes in after C, it will transition to
 * F=[L6+KE U4 TID940] directly. In both cases, the contender will enter the kernel with either
 * mutexwait(U4, TID0) or mutexwait(U4, TID940). The first owner will unlock the mutex
 * by first updating the owner to G=[L6+KE U4 TID-1] and then doing the actual unlock to
 * H=[L6+KE U5 TID=-1] before entering the kernel with mutexdrop(U5, -1) to signal the next waiter
 * (potentially as a prepost). When the waiter comes out of the kernel, it will update the owner to
 * I=[L6+KE U5 TID941]. An unlock at this point is simply J=[L6 U5 TID0] and then K=[L6 U6 TID0].
 *
 * At various points along these timelines, since the sequence words and TID are written independently,
 * a thread may get preempted and another thread might see inconsistent data. In the worst case, another
 * thread may see the TID in the SWITCHING (-1) state or unlocked (0) state for longer because the
 * owning thread was preempted.

/*
 * Drop the mutex unlock references from cond_wait. or mutex_unlock.
 */
__private_extern__ int
__mtx_droplock(_pthread_mutex *mutex, uint32_t *flagsp, uint32_t **pmtxp, uint32_t *mgenp, uint32_t *ugenp)
{
	bool firstfit = (mutex->mtxopts.options.policy == _PTHREAD_MUTEX_POLICY_FIRSTFIT);
	uint32_t lgenval, ugenval, flags;
	uint64_t oldtid, newtid;
	volatile uint64_t *tidaddr;
	MUTEX_GETTID_ADDR(mutex, &tidaddr);

	flags = mutex->mtxopts.value;
	flags &= ~_PTHREAD_MTX_OPT_NOTIFY; // no notification by default

	if (mutex->mtxopts.options.type != PTHREAD_MUTEX_NORMAL) {
		uint64_t selfid = _pthread_selfid_direct();

		if (*tidaddr != selfid) {
			//PTHREAD_ABORT("dropping recur or error mutex not owned by the thread\n");
			PLOCKSTAT_MUTEX_ERROR((pthread_mutex_t *)mutex, EPERM);
			return EPERM;
		} else if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE &&
			   --mutex->mtxopts.options.lock_count) {
			PLOCKSTAT_MUTEX_RELEASE((pthread_mutex_t *)mutex, 1);
			if (flagsp != NULL) {
				*flagsp = flags;
			}
			return 0;
		}
	}

	uint64_t oldval64, newval64;
	volatile uint64_t *seqaddr;
	MUTEX_GETSEQ_ADDR(mutex, &seqaddr);

	bool clearprepost, clearnotify, spurious;
	do {
		oldval64 = *seqaddr;
		oldtid = *tidaddr;
		lgenval = (uint32_t)oldval64;
		ugenval = (uint32_t)(oldval64 >> 32);

		clearprepost = false;
		clearnotify = false;
		spurious = false;

		int numwaiters = diff_genseq(lgenval, ugenval); // pending waiters

		if (numwaiters == 0) {
			// spurious unlock; do not touch tid
			spurious = true;
		} else {
			ugenval += PTHRW_INC;

			if ((lgenval & PTHRW_COUNT_MASK) == (ugenval & PTHRW_COUNT_MASK)) {
				// our unlock sequence matches to lock sequence, so if the CAS is successful, the mutex is unlocked

				/* do not reset Ibit, just K&E */
				lgenval &= ~(PTH_RWL_KBIT | PTH_RWL_EBIT);
				clearnotify = true;
				newtid = 0; // clear owner
			} else {
				if (firstfit) {
					lgenval &= ~PTH_RWL_EBIT; // reset E bit so another can acquire meanwhile
					newtid = 0;
				} else {
					newtid = PTHREAD_MTX_TID_SWITCHING;
				}
				// need to signal others waiting for mutex
				flags |= _PTHREAD_MTX_OPT_NOTIFY;
			}
			
			if (newtid != oldtid) {
				// We're giving up the mutex one way or the other, so go ahead and update the owner to SWITCHING
				// or 0 so that once the CAS below succeeds, there is no stale ownership information.
				// If the CAS of the seqaddr fails, we may loop, but it's still valid for the owner
				// to be SWITCHING/0
				if (!OSAtomicCompareAndSwap64(oldtid, newtid, (volatile int64_t *)tidaddr)) {
					// we own this mutex, nobody should be updating it except us
					__builtin_trap();
				}
			}
		}

		if (clearnotify || spurious) {
			flags &= ~_PTHREAD_MTX_OPT_NOTIFY;
			if (firstfit && ((lgenval & PTH_RWL_PBIT) != 0)) {
				clearprepost = true;
				lgenval &= ~PTH_RWL_PBIT;
			}
		}
		
		newval64 = (((uint64_t)ugenval) << 32);
		newval64 |= lgenval;

	} while (OSAtomicCompareAndSwap64Barrier(oldval64, newval64, (volatile int64_t *)seqaddr) != TRUE);

	if (clearprepost) {
		 __psynch_cvclrprepost(mutex, lgenval, ugenval, 0, 0, lgenval, (flags | _PTHREAD_MTX_OPT_MUTEX));
	}

	if (mgenp != NULL) {
		*mgenp = lgenval;
	}
	if (ugenp != NULL) {
		*ugenp = ugenval;
	}
	if (pmtxp != NULL) {
		*pmtxp = (uint32_t *)mutex;
	}
	if (flagsp != NULL) {
		*flagsp = flags;
	}

	return 0;
}

static int
__mtx_updatebits(_pthread_mutex *mutex, uint64_t selfid)
{
	int res = 0;
	int firstfit = (mutex->mtxopts.options.policy == _PTHREAD_MUTEX_POLICY_FIRSTFIT);
	int isebit = 0;

	uint32_t lgenval, ugenval;
	uint64_t oldval64, newval64;
	volatile uint64_t *seqaddr;
	MUTEX_GETSEQ_ADDR(mutex, &seqaddr);
	uint64_t oldtid;
	volatile uint64_t *tidaddr;
	MUTEX_GETTID_ADDR(mutex, &tidaddr);

	do {
		do {
			oldval64 = *seqaddr;
			oldtid = *tidaddr;
			lgenval = (uint32_t)oldval64;
			ugenval = (uint32_t)(oldval64 >> 32);

			// E bit was set on first pass through the loop but is no longer
			// set. Apparently we spin until it arrives.
			// XXX: verify this is desired behavior.
		} while (isebit && (lgenval & PTH_RWL_EBIT) == 0);

		if (isebit) {
			// first fit mutex now has the E bit set. Return 1.
			res = 1;
			break;
		}

		if (firstfit) {
			isebit = (lgenval & PTH_RWL_EBIT) != 0;
		} else if ((lgenval & (PTH_RWL_KBIT|PTH_RWL_EBIT)) == (PTH_RWL_KBIT|PTH_RWL_EBIT)) {
			// fairshare mutex and the bits are already set, just update tid
			break;
		}

		// either first fit or no E bit set
		// update the bits
		lgenval |= PTH_RWL_KBIT | PTH_RWL_EBIT;

		newval64 = (((uint64_t)ugenval) << 32);
		newval64 |= lgenval;

		// set s and b bit
		// Retry if CAS fails, or if it succeeds with firstfit and E bit already set
	} while (OSAtomicCompareAndSwap64Barrier(oldval64, newval64, (volatile int64_t *)seqaddr) != TRUE ||
		 (firstfit && isebit));

	if (res == 0) {
		if (!OSAtomicCompareAndSwap64Barrier(oldtid, selfid, (volatile int64_t *)tidaddr)) {
			// we own this mutex, nobody should be updating it except us
			__builtin_trap();
		}
	}

	return res;
}

int
__mtx_markprepost(_pthread_mutex *mutex, uint32_t updateval, int firstfit)
{
	uint32_t flags;
	uint32_t lgenval, ugenval;
	uint64_t oldval64, newval64;

	volatile uint64_t *seqaddr;
	MUTEX_GETSEQ_ADDR(mutex, &seqaddr);

	if (firstfit != 0 && (updateval & PTH_RWL_PBIT) != 0) {
		int clearprepost;
		do {				
			clearprepost = 0;

			flags = mutex->mtxopts.value;

			oldval64 = *seqaddr;
			lgenval = (uint32_t)oldval64;
			ugenval = (uint32_t)(oldval64 >> 32);

			/* update the bits */
			if ((lgenval & PTHRW_COUNT_MASK) == (ugenval & PTHRW_COUNT_MASK)) {
				clearprepost = 1;	
				lgenval &= ~PTH_RWL_PBIT;
			} else {
				lgenval |= PTH_RWL_PBIT;
			}
			newval64 = (((uint64_t)ugenval) << 32);
			newval64 |= lgenval;
		} while (OSAtomicCompareAndSwap64Barrier(oldval64, newval64, (volatile int64_t *)seqaddr) != TRUE);
		
		if (clearprepost != 0) {
			__psynch_cvclrprepost(mutex, lgenval, ugenval, 0, 0, lgenval, (flags | _PTHREAD_MTX_OPT_MUTEX));
		}
	}
	return 0;
}

static inline bool
_pthread_mutex_check_init_fast(_pthread_mutex *mutex)
{
	return (mutex->sig == _PTHREAD_MUTEX_SIG);
}

static int __attribute__((noinline))
_pthread_mutex_check_init(pthread_mutex_t *omutex)
{
	int res = 0;
	_pthread_mutex *mutex = (_pthread_mutex *)omutex;
	
	if (mutex->sig != _PTHREAD_MUTEX_SIG) {
		res = EINVAL;
		if ((mutex->sig & _PTHREAD_MUTEX_SIG_init_MASK) == _PTHREAD_MUTEX_SIG_CMP) {
			LOCK(mutex->lock);
			if ((mutex->sig & _PTHREAD_MUTEX_SIG_init_MASK) == _PTHREAD_MUTEX_SIG_CMP) {
				// initialize a statically initialized mutex to provide
				// compatibility for misbehaving applications.
				// (unlock should not be the first operation on a mutex)
				res = _pthread_mutex_init(mutex, NULL, (mutex->sig & 0xf));
			} else if (mutex->sig == _PTHREAD_MUTEX_SIG) {
				res = 0;
			}
			UNLOCK(mutex->lock);
		}
		if (res != 0) {
			PLOCKSTAT_MUTEX_ERROR(omutex, res);
		}
	}
	return res;
}

static int
_pthread_mutex_lock(pthread_mutex_t *omutex, bool trylock)
{
	int res;
	_pthread_mutex *mutex = (_pthread_mutex *)omutex;

	if (os_slowpath(!_pthread_mutex_check_init_fast(mutex))) {
		res = _pthread_mutex_check_init(omutex);
		if (res != 0) {
			return res;
		}
	}

	uint64_t oldtid;
	volatile uint64_t *tidaddr;
	MUTEX_GETTID_ADDR(mutex, &tidaddr);
	uint64_t selfid = _pthread_selfid_direct();

	if (mutex->mtxopts.options.type != PTHREAD_MUTEX_NORMAL) {
		if (*tidaddr == selfid) {
			if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE) {
				if (mutex->mtxopts.options.lock_count < USHRT_MAX) {
					mutex->mtxopts.options.lock_count++;
					PLOCKSTAT_MUTEX_ACQUIRE(omutex, 1, 0);
					res = 0;
				} else {
					res = EAGAIN;
					PLOCKSTAT_MUTEX_ERROR(omutex, res);
				}
			} else if (trylock) { /* PTHREAD_MUTEX_ERRORCHECK */
				// <rdar://problem/16261552> as per OpenGroup, trylock cannot
				// return EDEADLK on a deadlock, it should return EBUSY.
				res = EBUSY;
				PLOCKSTAT_MUTEX_ERROR(omutex, res);
			} else	{ /* PTHREAD_MUTEX_ERRORCHECK */
				res = EDEADLK;
				PLOCKSTAT_MUTEX_ERROR(omutex, res);
			}
			return res;
		}
	}

	uint64_t oldval64, newval64;
	volatile uint64_t *seqaddr;
	MUTEX_GETSEQ_ADDR(mutex, &seqaddr);

	uint32_t lgenval, ugenval;
	bool gotlock = false;

	do {
		oldval64 = *seqaddr;
		oldtid = *tidaddr;
		lgenval = (uint32_t)oldval64;
		ugenval = (uint32_t)(oldval64 >> 32);

		gotlock = ((lgenval & PTH_RWL_EBIT) == 0);

		if (trylock && !gotlock) {
			// A trylock on a held lock will fail immediately. But since
			// we did not load the sequence words atomically, perform a
			// no-op CAS64 to ensure that nobody has unlocked concurrently.
		} else {
			// Increment the lock sequence number and force the lock into E+K
			// mode, whether "gotlock" is true or not.
			lgenval += PTHRW_INC;
			lgenval |= PTH_RWL_EBIT | PTH_RWL_KBIT;
		}

		newval64 = (((uint64_t)ugenval) << 32);
		newval64 |= lgenval;
		
		// Set S and B bit
	} while (OSAtomicCompareAndSwap64Barrier(oldval64, newval64, (volatile int64_t *)seqaddr) == FALSE);

	if (gotlock) {
		if (!OSAtomicCompareAndSwap64Barrier(oldtid, selfid, (volatile int64_t *)tidaddr)) {
			while (!OSAtomicCompareAndSwap64Barrier(*tidaddr, selfid, (volatile int64_t *)tidaddr));
		}
		res = 0;
		DEBUG_TRACE(psynch_mutex_ulock, omutex, lgenval, ugenval, selfid);
		PLOCKSTAT_MUTEX_ACQUIRE(omutex, 0, 0);
	} else if (trylock) {
		res = EBUSY;
		DEBUG_TRACE(psynch_mutex_utrylock_failed, omutex, lgenval, ugenval, oldtid);
		PLOCKSTAT_MUTEX_ERROR(omutex, res);
	} else {
		PLOCKSTAT_MUTEX_BLOCK(omutex);
		do {
			uint32_t updateval;
			do {
				updateval = __psynch_mutexwait(omutex, lgenval, ugenval, oldtid, mutex->mtxopts.value);
				oldtid = *tidaddr;
			} while (updateval == (uint32_t)-1);

			// returns 0 on succesful update; in firstfit it may fail with 1
		} while (__mtx_updatebits(mutex, selfid) == 1);
		res = 0;
		PLOCKSTAT_MUTEX_BLOCKED(omutex, BLOCK_SUCCESS_PLOCKSTAT);
	}

	if (res == 0 && mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE) {
		mutex->mtxopts.options.lock_count = 1;
	}

	PLOCKSTAT_MUTEX_ACQUIRE(omutex, 0, 0);

	return res;
}

int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
	return _pthread_mutex_lock(mutex, false);
}

int
pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	return _pthread_mutex_lock(mutex, true);
}

/*
 * Unlock a mutex.
 * TODO: Priority inheritance stuff
 */
int
pthread_mutex_unlock(pthread_mutex_t *omutex)
{
	int res;
	_pthread_mutex *mutex = (_pthread_mutex *)omutex;
	uint32_t mtxgen, mtxugen, flags;

	// Initialize static mutexes for compatibility with misbehaving
	// applications (unlock should not be the first operation on a mutex).
	if (os_slowpath(!_pthread_mutex_check_init_fast(mutex))) {
		res = _pthread_mutex_check_init(omutex);
		if (res != 0) {
			return res;
		}
	}

	res = __mtx_droplock(mutex, &flags, NULL, &mtxgen, &mtxugen);
	if (res != 0) {
		return res;
	}

	if ((flags & _PTHREAD_MTX_OPT_NOTIFY) != 0) {
		uint32_t updateval;
		int firstfit = (mutex->mtxopts.options.policy == _PTHREAD_MUTEX_POLICY_FIRSTFIT);
		volatile uint64_t *tidaddr;
		MUTEX_GETTID_ADDR(mutex, &tidaddr);

		updateval = __psynch_mutexdrop(omutex, mtxgen, mtxugen, *tidaddr, flags);

		if (updateval == (uint32_t)-1) {
			res = errno;

			if (res == EINTR) {
				res = 0;
			}
			if (res != 0) {
				PTHREAD_ABORT("__p_mutexdrop failed with error %d\n", res);
			}
			return res;
		} else if (firstfit == 1) {
			if ((updateval & PTH_RWL_PBIT) != 0) {
				__mtx_markprepost(mutex, updateval, firstfit);
			}
		}
	} else {
		volatile uint64_t *tidaddr;
		MUTEX_GETTID_ADDR(mutex, &tidaddr);
		DEBUG_TRACE(psynch_mutex_uunlock, omutex, mtxgen, mtxugen, *tidaddr);
	}

	return 0;
}

int
_pthread_mutex_init(_pthread_mutex *mutex, const pthread_mutexattr_t *attr, uint32_t static_type)
{
	if (attr) {
		if (attr->sig != _PTHREAD_MUTEX_ATTR_SIG) {
			return EINVAL;
		}
		mutex->prioceiling = attr->prioceiling;
		mutex->mtxopts.options.protocol = attr->protocol;
		mutex->mtxopts.options.policy = attr->policy;
		mutex->mtxopts.options.type = attr->type;
		mutex->mtxopts.options.pshared = attr->pshared;
	} else {
		switch (static_type) {
			case 1:
				mutex->mtxopts.options.type = PTHREAD_MUTEX_ERRORCHECK;
				break;
			case 2:
				mutex->mtxopts.options.type = PTHREAD_MUTEX_RECURSIVE;
				break;
			case 3:
				/* firstfit fall thru */
			case 7:
				mutex->mtxopts.options.type = PTHREAD_MUTEX_DEFAULT;
				break;
			default:
				return EINVAL;
		}

		mutex->prioceiling = _PTHREAD_DEFAULT_PRIOCEILING;
		mutex->mtxopts.options.protocol = _PTHREAD_DEFAULT_PROTOCOL;
		if (static_type != 3) {
			mutex->mtxopts.options.policy = _PTHREAD_MUTEX_POLICY_FAIRSHARE;
		} else {
			mutex->mtxopts.options.policy = _PTHREAD_MUTEX_POLICY_FIRSTFIT;
		}
		mutex->mtxopts.options.pshared = _PTHREAD_DEFAULT_PSHARED;
	}
	
	mutex->mtxopts.options.notify = 0;
	mutex->mtxopts.options.unused = 0;
	mutex->mtxopts.options.hold = 0;
	mutex->mtxopts.options.mutex = 1;
	mutex->mtxopts.options.lock_count = 0;

	mutex->m_tid[0] = 0;
	mutex->m_tid[1] = 0;
	mutex->m_seq[0] = 0;
	mutex->m_seq[1] = 0;
	mutex->m_seq[2] = 0;
	mutex->prioceiling = 0;
	mutex->priority = 0;

	mutex->mtxopts.options.misalign = (((uintptr_t)&mutex->m_seq[0]) & 0x7) != 0;
	
	// Ensure all contents are properly set before setting signature.
	OSMemoryBarrier();

	mutex->sig = _PTHREAD_MUTEX_SIG;

	return 0;
}

int
pthread_mutex_destroy(pthread_mutex_t *omutex)
{
	_pthread_mutex *mutex = (_pthread_mutex *)omutex;

	int res = EINVAL;

	LOCK(mutex->lock);
	if (mutex->sig == _PTHREAD_MUTEX_SIG) {
		uint32_t lgenval, ugenval;
		uint64_t oldval64;
		volatile uint64_t *seqaddr;
		MUTEX_GETSEQ_ADDR(mutex, &seqaddr);
		volatile uint64_t *tidaddr;
		MUTEX_GETTID_ADDR(mutex, &tidaddr);

		oldval64 = *seqaddr;
		lgenval = (uint32_t)oldval64;
		ugenval = (uint32_t)(oldval64 >> 32);
		if ((*tidaddr == (uint64_t)0) &&
		    ((lgenval & PTHRW_COUNT_MASK) == (ugenval & PTHRW_COUNT_MASK))) {
			mutex->sig = _PTHREAD_NO_SIG;
			res = 0;
		} else {
			res = EBUSY;
		}
	} else if ((mutex->sig & _PTHREAD_MUTEX_SIG_init_MASK ) == _PTHREAD_MUTEX_SIG_CMP) {
		mutex->sig = _PTHREAD_NO_SIG;
		res = 0;
	}
	UNLOCK(mutex->lock);
	
	return res;	
}

#endif /* !BUILDING_VARIANT ] */

/*
 * Destroy a mutex attribute structure.
 */
int
pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
#if __DARWIN_UNIX03
	if (__unix_conforming == 0) {
		__unix_conforming = 1;
	}
	if (attr->sig != _PTHREAD_MUTEX_ATTR_SIG) {
		return EINVAL;
	}
#endif /* __DARWIN_UNIX03 */

	attr->sig = _PTHREAD_NO_SIG;
	return 0;
}


