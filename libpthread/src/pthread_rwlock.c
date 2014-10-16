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
/*-
 * Copyright (c) 1998 Alex Nash
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc_r/uthread/uthread_rwlock.c,v 1.6 2001/04/10 04:19:20 deischen Exp $
 */

/* 
 * POSIX Pthread Library 
 * -- Read Write Lock support
 * 4/24/02: A. Ramesh
 *	   Ported from FreeBSD
 */

#include "internal.h"
#include <stdio.h>      /* For printf(). */

extern int __unix_conforming;

#ifdef PLOCKSTAT
#include "plockstat.h"
#else /* !PLOCKSTAT */
#define PLOCKSTAT_RW_ERROR(x, y, z)
#define PLOCKSTAT_RW_BLOCK(x, y)
#define PLOCKSTAT_RW_BLOCKED(x, y, z)
#define PLOCKSTAT_RW_ACQUIRE(x, y)
#define PLOCKSTAT_RW_RELEASE(x, y)
#endif /* PLOCKSTAT */

#define READ_LOCK_PLOCKSTAT  0
#define WRITE_LOCK_PLOCKSTAT 1

#define BLOCK_FAIL_PLOCKSTAT    0
#define BLOCK_SUCCESS_PLOCKSTAT 1

/* maximum number of times a read lock may be obtained */
#define	MAX_READ_LOCKS		(INT_MAX - 1) 

#include <platform/string.h>
#include <platform/compat.h>

__private_extern__ int __pthread_rwlock_init(_pthread_rwlock *rwlock, const pthread_rwlockattr_t *attr);
__private_extern__ void _pthread_rwlock_updateval(_pthread_rwlock *rwlock, uint32_t updateval);

static void
RWLOCK_GETSEQ_ADDR(_pthread_rwlock *rwlock,
		   volatile uint32_t **lcntaddr,
		   volatile uint32_t **ucntaddr,
		   volatile uint32_t **seqaddr)
{
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		if (rwlock->misalign) {
			*lcntaddr = &rwlock->rw_seq[1];
			*seqaddr = &rwlock->rw_seq[2];
			*ucntaddr = &rwlock->rw_seq[3];
		} else {
			*lcntaddr = &rwlock->rw_seq[0];
			*seqaddr = &rwlock->rw_seq[1];
			*ucntaddr = &rwlock->rw_seq[2];
		}
	} else {
		*lcntaddr = rwlock->rw_lcntaddr;
		*seqaddr = rwlock->rw_seqaddr;
		*ucntaddr = rwlock->rw_ucntaddr;
	}
}

#ifndef BUILDING_VARIANT /* [ */
static uint32_t modbits(uint32_t lgenval, uint32_t updateval, uint32_t savebits);

int
pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
	attr->sig = _PTHREAD_RWLOCK_ATTR_SIG;
	attr->pshared = _PTHREAD_DEFAULT_PSHARED;
	return 0;
}

int	
pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
	attr->sig = _PTHREAD_NO_SIG;
	attr->pshared = 0;
	return 0;
}

int
pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr, int *pshared)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_RWLOCK_ATTR_SIG) {
		*pshared = (int)attr->pshared;
		res = 0;
	}
	return res;
}

int
pthread_rwlockattr_setpshared(pthread_rwlockattr_t * attr, int pshared)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_RWLOCK_ATTR_SIG) {
#if __DARWIN_UNIX03
		if (( pshared == PTHREAD_PROCESS_PRIVATE) || (pshared == PTHREAD_PROCESS_SHARED))
#else /* __DARWIN_UNIX03 */
		if ( pshared == PTHREAD_PROCESS_PRIVATE)
#endif /* __DARWIN_UNIX03 */
		{
			attr->pshared = pshared ;
			res = 0;
		}
	}
	return res;
}

__private_extern__ int
__pthread_rwlock_init(_pthread_rwlock *rwlock, const pthread_rwlockattr_t *attr)
{
	// Force RWLOCK_GETSEQ_ADDR to calculate addresses by setting pshared.
	rwlock->pshared = PTHREAD_PROCESS_SHARED;
	rwlock->misalign = (((uintptr_t)&rwlock->rw_seq[0]) & 0x7) != 0;
	RWLOCK_GETSEQ_ADDR(rwlock, &rwlock->rw_lcntaddr, &rwlock->rw_ucntaddr, &rwlock->rw_seqaddr);
	*rwlock->rw_lcntaddr = PTHRW_RWLOCK_INIT;
	*rwlock->rw_seqaddr = PTHRW_RWS_INIT;
	*rwlock->rw_ucntaddr = 0;

	if (attr != NULL && attr->pshared == PTHREAD_PROCESS_SHARED) {
		rwlock->pshared = PTHREAD_PROCESS_SHARED;
		rwlock->rw_flags = PTHRW_KERN_PROCESS_SHARED;
	} else {
		rwlock->pshared = _PTHREAD_DEFAULT_PSHARED;
		rwlock->rw_flags = PTHRW_KERN_PROCESS_PRIVATE;
	}
		
	rwlock->rw_owner = NULL;
	bzero(rwlock->_reserved, sizeof(rwlock->_reserved));

	// Ensure all contents are properly set before setting signature.
	OSMemoryBarrier();
	rwlock->sig = _PTHREAD_RWLOCK_SIG;
	
	return 0;
}

static uint32_t
modbits(uint32_t lgenval, uint32_t updateval, uint32_t savebits)
{
	uint32_t lval = lgenval & PTHRW_BIT_MASK;
	uint32_t uval = updateval & PTHRW_BIT_MASK;
	uint32_t rval, nlval;

	nlval = (lval | uval) & ~(PTH_RWL_MBIT);
	
	/* reconcile bits on the lock with what kernel needs to set */
	if ((uval & PTH_RWL_KBIT) == 0 && (lval & PTH_RWL_WBIT) == 0) {
		nlval &= ~PTH_RWL_KBIT;
	}

	if (savebits != 0) {
		if ((savebits & PTH_RWS_WSVBIT) != 0 && (nlval & PTH_RWL_WBIT) == 0 && (nlval & PTH_RWL_EBIT) == 0) {
			nlval |= (PTH_RWL_WBIT | PTH_RWL_KBIT);
		}
	}
	rval = (lgenval & PTHRW_COUNT_MASK) | nlval;
	return(rval);
}

__private_extern__ void
_pthread_rwlock_updateval(_pthread_rwlock *rwlock, uint32_t updateval)
{
	bool isoverlap = (updateval & PTH_RWL_MBIT) != 0;

	uint64_t oldval64, newval64;
	volatile uint32_t *lcntaddr, *ucntaddr, *seqaddr;

	/* TBD: restore U bit */
	RWLOCK_GETSEQ_ADDR(rwlock, &lcntaddr, &ucntaddr, &seqaddr);

	do {
		uint32_t lcntval = *lcntaddr;
		uint32_t rw_seq = *seqaddr;
		
		uint32_t newval, newsval;
		if (isoverlap || is_rws_setunlockinit(rw_seq) != 0) {
			// Set S word to the specified value
			uint32_t savebits = (rw_seq & PTHRW_RWS_SAVEMASK);
			newval = modbits(lcntval, updateval, savebits);
			newsval = rw_seq + (updateval & PTHRW_COUNT_MASK);
			if (!isoverlap) {
				newsval &= PTHRW_COUNT_MASK;
			}
			newsval &= ~PTHRW_RWS_SAVEMASK;
		} else {
			newval = lcntval;
			newsval = rw_seq;
		}

		oldval64 = (((uint64_t)rw_seq) << 32);
		oldval64 |= lcntval;
		newval64 = (((uint64_t)newsval) << 32);
		newval64 |= newval;
	} while (OSAtomicCompareAndSwap64Barrier(oldval64, newval64, (volatile int64_t *)lcntaddr) != TRUE);
}

#endif /* !BUILDING_VARIANT ] */

static int
_pthread_rwlock_check_busy(_pthread_rwlock *rwlock)
{
	int res = 0;
	
	volatile uint32_t *lcntaddr, *ucntaddr, *seqaddr;
	
	RWLOCK_GETSEQ_ADDR(rwlock, &lcntaddr, &ucntaddr, &seqaddr);
	
	uint32_t rw_lcnt = *lcntaddr;
	uint32_t rw_ucnt = *ucntaddr;
	
	if ((rw_lcnt & PTHRW_COUNT_MASK) != rw_ucnt) {
		res = EBUSY;
	}
	
	return res;
}

int
pthread_rwlock_destroy(pthread_rwlock_t *orwlock)
{
	int res = 0;
	_pthread_rwlock *rwlock = (_pthread_rwlock *)orwlock;

	if (rwlock->sig == _PTHREAD_RWLOCK_SIG) {
#if __DARWIN_UNIX03
		res = _pthread_rwlock_check_busy(rwlock);
#endif /* __DARWIN_UNIX03 */
	} else if (rwlock->sig != _PTHREAD_RWLOCK_SIG_init) {
		res = EINVAL;
	}
	if (res == 0) {
		rwlock->sig = _PTHREAD_NO_SIG;
	}
	return res;
}


int
pthread_rwlock_init(pthread_rwlock_t *orwlock, const pthread_rwlockattr_t *attr)
{
	int res = 0;
	_pthread_rwlock *rwlock = (_pthread_rwlock *)orwlock;
	
#if __DARWIN_UNIX03
	if (attr && attr->sig != _PTHREAD_RWLOCK_ATTR_SIG) {
		res = EINVAL;
	}

	if (res == 0 && rwlock->sig == _PTHREAD_RWLOCK_SIG) {
		res = _pthread_rwlock_check_busy(rwlock);
	}
#endif
	if (res == 0) {
		LOCK_INIT(rwlock->lock);
		res = __pthread_rwlock_init(rwlock, attr);
	}
	return res;
}

static int
_pthread_rwlock_check_init(pthread_rwlock_t *orwlock)
{
	int res = 0;
	_pthread_rwlock *rwlock = (_pthread_rwlock *)orwlock;
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		res = EINVAL;
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
			LOCK(rwlock->lock);
			if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
				res = __pthread_rwlock_init(rwlock, NULL);
			} else if (rwlock->sig == _PTHREAD_RWLOCK_SIG){
				res = 0;
			}
			UNLOCK(rwlock->lock);
		}
		if (res != 0) {
			PLOCKSTAT_RW_ERROR(orwlock, READ_LOCK_PLOCKSTAT, res);
		}
	}
	return res;
}

static int
_pthread_rwlock_lock(pthread_rwlock_t *orwlock, bool readlock, bool trylock)
{
	int res;
	_pthread_rwlock *rwlock = (_pthread_rwlock *)orwlock;

	res = _pthread_rwlock_check_init(orwlock);
	if (res != 0) {
		return res;
	}

	uint64_t oldval64, newval64;
	volatile uint32_t *lcntaddr, *ucntaddr, *seqaddr;
	RWLOCK_GETSEQ_ADDR(rwlock, &lcntaddr, &ucntaddr, &seqaddr);

	uint32_t newval, newsval;
	uint32_t lcntval, ucntval, rw_seq;

	bool gotlock;
	bool retry;
	int retry_count = 0;

	do {
		res = 0;
		retry = false;
		
		lcntval = *lcntaddr;
		ucntval = *ucntaddr;
		rw_seq = *seqaddr;

#if __DARWIN_UNIX03
		if (is_rwl_ebit_set(lcntval)) {
			if (rwlock->rw_owner == pthread_self()) {
				res = EDEADLK;
				break;
			}
		}
#endif /* __DARWIN_UNIX03 */

		oldval64 = (((uint64_t)rw_seq) << 32);
		oldval64 |= lcntval;

		/* if l bit is on or u and k bit is clear, acquire lock in userland */
		if (readlock) {
			gotlock = can_rwl_readinuser(lcntval);
		} else {
			gotlock = (lcntval & PTH_RWL_RBIT) != 0;
		}

		uint32_t bits = 0;
		uint32_t mask = ~0ul;
		
		newval = lcntval + PTHRW_INC;

		if (gotlock) {
			if (readlock) {
				if (diff_genseq(lcntval, ucntval) >= PTHRW_MAX_READERS) {
					/* since ucntval may be newer, just redo */
					retry_count++;
					if (retry_count > 1024) {
						res = EAGAIN;
						break;
					} else {
						sched_yield();
						retry = true;
						continue;
					}
				}
				
				// Need to update L (remove R bit) and S word
				mask = PTH_RWLOCK_RESET_RBIT;
			} else {
				mask = PTHRW_COUNT_MASK;
				bits = PTH_RWL_IBIT | PTH_RWL_KBIT | PTH_RWL_EBIT;
			}
			newsval = rw_seq + PTHRW_INC;
		} else if (trylock) {
			res = EBUSY;
			break;
		} else {
			if (readlock) {
				// Need to block in kernel. Remove R bit.
				mask = PTH_RWLOCK_RESET_RBIT;
			} else {
				bits = PTH_RWL_KBIT | PTH_RWL_WBIT;
			}
			newsval = rw_seq;
			if (is_rws_setseq(rw_seq)) {
				newsval &= PTHRW_SW_Reset_BIT_MASK;
				newsval |= (newval & PTHRW_COUNT_MASK);
			}
		}
		newval = (newval & mask) | bits;
		newval64 = (((uint64_t)newsval) << 32);
		newval64 |= newval;

	} while (retry || OSAtomicCompareAndSwap64Barrier(oldval64, newval64, (volatile int64_t *)lcntaddr) != TRUE);

#ifdef PLOCKSTAT
	int plockstat = readlock ? READ_LOCK_PLOCKSTAT : WRITE_LOCK_PLOCKSTAT;
#endif

	// Unable to acquire in userland, transition to kernel.
	if (res == 0 && !gotlock) {
		uint32_t updateval;

		PLOCKSTAT_RW_BLOCK(orwlock, plockstat);
		
		do {
			if (readlock) {
				updateval = __psynch_rw_rdlock(orwlock, newval, ucntval, newsval, rwlock->rw_flags);
			} else {
				updateval = __psynch_rw_wrlock(orwlock, newval, ucntval, newsval, rwlock->rw_flags);
			}
			if (updateval == (uint32_t)-1) {
				res = errno;
			} else {
				res = 0;
			}
		} while (res == EINTR);
		
		if (res == 0) {
			_pthread_rwlock_updateval(rwlock, updateval);
			PLOCKSTAT_RW_BLOCKED(orwlock, plockstat, BLOCK_SUCCESS_PLOCKSTAT);
		} else {
			PLOCKSTAT_RW_BLOCKED(orwlock, plockstat, BLOCK_FAIL_PLOCKSTAT);
			uint64_t myid;
			(void)pthread_threadid_np(pthread_self(), &myid);
			PTHREAD_ABORT("kernel lock returned unknown error %x with tid %x\n", updateval, (uint32_t)myid);
		}
	}
	
	if (res == 0) {
#if __DARWIN_UNIX03
		if (!readlock) {
			rwlock->rw_owner = pthread_self();
		}
#endif /* __DARWIN_UNIX03 */
		PLOCKSTAT_RW_ACQUIRE(orwlock, plockstat);
	} else {
		PLOCKSTAT_RW_ERROR(orwlock, plockstat, res);
	}
	
	return res;
}

int
pthread_rwlock_rdlock(pthread_rwlock_t *orwlock)
{
	// read lock, no try
	return _pthread_rwlock_lock(orwlock, true, false);
}

int
pthread_rwlock_tryrdlock(pthread_rwlock_t *orwlock)
{
	// read lock, try lock
	return _pthread_rwlock_lock(orwlock, true, true);
}

int
pthread_rwlock_wrlock(pthread_rwlock_t *orwlock)
{
	// write lock, no try
	return _pthread_rwlock_lock(orwlock, false, false);
}

int
pthread_rwlock_trywrlock(pthread_rwlock_t *orwlock)
{
	// write lock, try lock
	return _pthread_rwlock_lock(orwlock, false, true);
}

int
pthread_rwlock_unlock(pthread_rwlock_t *orwlock)
{
	int res;
	_pthread_rwlock *rwlock = (_pthread_rwlock *)orwlock;
#ifdef PLOCKSTAT
	int wrlock = 0;
#endif

	res = _pthread_rwlock_check_init(orwlock);
	if (res != 0) {
		return res;
	}

	uint64_t oldval64 = 0, newval64 = 0;
	volatile uint32_t *lcntaddr, *ucntaddr, *seqaddr;
	RWLOCK_GETSEQ_ADDR(rwlock, &lcntaddr, &ucntaddr, &seqaddr);

	bool droplock;
	bool reload;
	bool incr_ucnt = true;
	bool check_spurious = true;
	uint32_t lcntval, ucntval, rw_seq, ulval = 0, newval, newsval;

	do {
		reload = false;
		droplock = true;

		lcntval = *lcntaddr;
		ucntval = *ucntaddr;
		rw_seq = *seqaddr;

		oldval64 = (((uint64_t)rw_seq) << 32);
		oldval64 |= lcntval;

		// check for spurious unlocks
		if (check_spurious) {
			if ((lcntval & PTH_RWL_RBIT) != 0) {
				droplock = false;

				newval64 = oldval64;
				continue;
			}
			check_spurious = false;
		}

		if (is_rwl_ebit_set(lcntval)) {
#ifdef PLOCKSTAT
			wrlock = 1;
#endif
#if __DARWIN_UNIX03
			rwlock->rw_owner = NULL;
#endif /* __DARWIN_UNIX03 */
		}

		// update U
		if (incr_ucnt) {
			ulval = (ucntval + PTHRW_INC);
			incr_ucnt = (OSAtomicCompareAndSwap32Barrier(ucntval, ulval, (volatile int32_t *)ucntaddr) != TRUE);
			newval64 = oldval64;
			reload = true;
			continue;
		}

		// last unlock, note U is already updated ?
		if ((lcntval & PTHRW_COUNT_MASK) == (ulval & PTHRW_COUNT_MASK)) {
			/* Set L with R and init bits and set S to L */
			newval  = (lcntval & PTHRW_COUNT_MASK)| PTHRW_RWLOCK_INIT;
			newsval = (lcntval & PTHRW_COUNT_MASK)| PTHRW_RWS_INIT;

			droplock = false;
		} else {
			/* if it is not exclusive or no Writer/yield pending, skip */
			if ((lcntval & (PTH_RWL_EBIT | PTH_RWL_WBIT | PTH_RWL_KBIT)) == 0) {
				droplock = false;
				break;
			}

			/* kernel transition needed? */
			/* U+1 == S? */
			if ((ulval + PTHRW_INC) != (rw_seq & PTHRW_COUNT_MASK)) {
				droplock = false;
				break;
			}

			/* reset all bits and set k */
			newval = (lcntval & PTHRW_COUNT_MASK) | PTH_RWL_KBIT;
			/* set I bit on S word */	
			newsval = rw_seq | PTH_RWS_IBIT;
			if ((lcntval & PTH_RWL_WBIT) != 0) {
				newsval |= PTH_RWS_WSVBIT;
			}
		}

		newval64 = (((uint64_t)newsval) << 32);
		newval64 |= newval;

	} while (OSAtomicCompareAndSwap64Barrier(oldval64, newval64, (volatile int64_t *)lcntaddr) != TRUE || reload);

	if (droplock) {
		uint32_t updateval;
		do {
			updateval = __psynch_rw_unlock(orwlock, lcntval, ulval, newsval, rwlock->rw_flags);
			if (updateval == (uint32_t)-1) {
				res = errno;
			} else {
				res = 0;
			}
		} while (res == EINTR);

		if (res != 0) {
			uint64_t myid = 0;
			(void)pthread_threadid_np(pthread_self(), &myid);
			PTHREAD_ABORT("rwunlock from kernel with unknown error %x: tid %x\n", res, (uint32_t)myid);
		}
	}

	PLOCKSTAT_RW_RELEASE(orwlock, wrlock);

	return res;
}

