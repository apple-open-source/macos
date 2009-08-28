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

#include "pthread_internals.h"
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

#if  defined(__i386__) || defined(__x86_64__)

#ifndef BUILDING_VARIANT /* [ */
int usenew_impl = 0;
#else /* BUILDING_VARIANT */
extern int usenew_impl;
#endif /* BUILDING_VARIANT */


#if defined(__LP64__)
#define RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr) \
{ \
                if (rwlock->misalign != 0) { \
                        lseqaddr = &rwlock->rw_seq[1]; \
			wcaddr = &rwlock->rw_seq[2]; \
                        useqaddr = &rwlock->rw_seq[3]; \
                 } else { \
                        lseqaddr = &rwlock->rw_seq[0]; \
			wcaddr = &rwlock->rw_seq[1]; \
                        useqaddr = &rwlock->rw_seq[2]; \
                } \
}
#else /* __LP64__ */
#define RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr) \
{ \
                if (rwlock->misalign != 0) { \
                        lseqaddr = &rwlock->rw_seq[0]; \
			wcaddr = &rwlock->rw_seq[1]; \
                        useqaddr = &rwlock->rw_seq[2]; \
                 }else { \
                        lseqaddr = &rwlock->rw_seq[1]; \
			wcaddr = &rwlock->rw_seq[2]; \
                        useqaddr = &rwlock->rw_seq[3]; \
                } \
}
#endif /* __LP64__ */

int _new_pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int _new_pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr);
int _new_pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int _new_pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int _new_pthread_rwlock_longrdlock_np(pthread_rwlock_t *rwlock);
int _new_pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
int _new_pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int _new_pthread_rwlock_yieldwrlock_np(pthread_rwlock_t *rwlock);
int _new_pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
int _new_pthread_rwlock_downgrade_np(pthread_rwlock_t *rwlock);
int _new_pthread_rwlock_upgrade_np(pthread_rwlock_t *rwlock);

#define _KSYN_TRACE_ 0

#if _KSYN_TRACE_
/* The Function qualifiers  */
#define DBG_FUNC_START          1
#define DBG_FUNC_END            2
#define DBG_FUNC_NONE           0

int __kdebug_trace(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

#define _KSYN_TRACE_RW_RDLOCK     0x9000080
#define _KSYN_TRACE_RW_WRLOCK	 0x9000084
#define _KSYN_TRACE_RW_UNLOCK    0x9000088
#define _KSYN_TRACE_RW_UNACT1    0x900808c
#define _KSYN_TRACE_RW_UNACT2    0x9008090
#define _KSYN_TRACE_RW_UNACTK    0x9008094
#define _KSYN_TRACE_RW_UNACTE    0x9008098
#endif /* _KSYN_TRACE_ */
#endif /* __i386__ || __x86_64__ */

#ifndef BUILDING_VARIANT /* [ */

#if  defined(__i386__) || defined(__x86_64__)
static int rwlock_unlock_action_onread(pthread_rwlock_t * rwlock, uint32_t updateval);
static int rwlock_unlock_action1(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t updateval);
static int rwlock_unlock_action2(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t updateval);
static uint32_t modbits(uint32_t lgenval, uint32_t updateval);
static int rwlock_unlock_action_k(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t updateval);
static int rwlock_exclusive_lockreturn(pthread_rwlock_t * rwlock, uint32_t updateval);
static int rw_diffgenseq(uint32_t x, uint32_t y);
#endif /* __i386__ || __x86_64__ */


int
pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
        attr->sig = _PTHREAD_RWLOCK_ATTR_SIG;
	attr->pshared = _PTHREAD_DEFAULT_PSHARED;
        return (0);
}

int       
pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
        attr->sig = _PTHREAD_NO_SIG;  /* Uninitialized */
	attr->pshared = 0;
        return (0);
}

int
pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr,
				int *pshared)
{
        if (attr->sig == _PTHREAD_RWLOCK_ATTR_SIG)
        {
		*pshared = (int)attr->pshared; 
                return (0);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}


int
pthread_rwlockattr_setpshared(pthread_rwlockattr_t * attr, int pshared)
{
        if (attr->sig == _PTHREAD_RWLOCK_ATTR_SIG)
        {
#if __DARWIN_UNIX03
                if (( pshared == PTHREAD_PROCESS_PRIVATE) || (pshared == PTHREAD_PROCESS_SHARED))
#else /* __DARWIN_UNIX03 */
                if ( pshared == PTHREAD_PROCESS_PRIVATE)
#endif /* __DARWIN_UNIX03 */
                {
						attr->pshared = pshared ;
                        return (0);
                } else
                {
                        return (EINVAL); /* Invalid parameter */
                }
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }

}

#if defined(__i386__) || defined(__x86_64__)  /* [ */
int
_new_pthread_rwlock_destroy(pthread_rwlock_t *orwlock)
{
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
#if __DARWIN_UNIX03
	uint32_t rw_lseqcnt, rw_useqcnt;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
#endif /* __DARWIN_UNIX03 */
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		return(EINVAL);
	} else {
#if __DARWIN_UNIX03
		if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
			RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
		} else {
			lseqaddr = rwlock->rw_lseqaddr;
			useqaddr = rwlock->rw_useqaddr;
			wcaddr = rwlock->rw_wcaddr;
		}

		rw_lseqcnt = *lseqaddr;
		rw_useqcnt = *useqaddr;
		
		if((rw_lseqcnt & PTHRW_COUNT_MASK) != rw_useqcnt)
			return(EBUSY);
		
#endif /* __DARWIN_UNIX03 */
		//bzero(rwlock, sizeof(npthread_rwlock_t));
		rwlock->sig = _PTHREAD_NO_SIG;
		return(0);
	}
}


int
_new_pthread_rwlock_init(pthread_rwlock_t * orwlock, const pthread_rwlockattr_t *attr)
{
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
#if __DARWIN_UNIX03
	uint32_t rw_lseqcnt, rw_useqcnt;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
#endif /* __DARWIN_UNIX03 */
	
#if __DARWIN_UNIX03
	if (attr && (attr->sig != _PTHREAD_RWLOCK_ATTR_SIG)) {
		return(EINVAL);
	}
	
	/* if already inited  check whether it is in use, then return EBUSY */
	if (rwlock->sig == _PTHREAD_RWLOCK_SIG) {
		if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
			RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
		} else {
			lseqaddr = rwlock->rw_lseqaddr;
			useqaddr = rwlock->rw_useqaddr;
			wcaddr = rwlock->rw_wcaddr;
		}
		rw_lseqcnt = *lseqaddr;
		rw_useqcnt = *useqaddr;
		
		if ((rw_lseqcnt & PTHRW_COUNT_MASK) != rw_useqcnt)
			return(EBUSY);
		
	}
#endif /* __DARWIN_UNIX03 */
	
	/* initialize the lock */
	bzero(rwlock, sizeof(pthread_rwlock_t));
	
	if ((attr != NULL) && (attr->pshared == PTHREAD_PROCESS_SHARED)) {
		rwlock->pshared = PTHREAD_PROCESS_SHARED;
		rwlock->rw_flags = PTHRW_KERN_PROCESS_SHARED;
		
	 } else {
		rwlock->pshared = _PTHREAD_DEFAULT_PSHARED;
		rwlock->rw_flags = PTHRW_KERN_PROCESS_PRIVATE;
	}
	
	if (((uintptr_t)rwlock & 0x07) != 0) {
		rwlock->misalign = 1;
#if defined(__LP64__)
		rwlock->rw_lseqaddr = &rwlock->rw_seq[1];
		rwlock->rw_wcaddr = &rwlock->rw_seq[2];
		rwlock->rw_useqaddr = &rwlock->rw_seq[3];
		rwlock->rw_seq[1]= PTHRW_RW_INIT;
#else /* __LP64__ */
		rwlock->rw_lseqaddr = &rwlock->rw_seq[0];
		rwlock->rw_wcaddr = &rwlock->rw_seq[1];
		rwlock->rw_useqaddr = &rwlock->rw_seq[2];
		rwlock->rw_seq[0]= PTHRW_RW_INIT;
#endif /* __LP64__ */
		
	} else {
		rwlock->misalign = 0;
#if defined(__LP64__)
		rwlock->rw_lseqaddr = &rwlock->rw_seq[0];
		rwlock->rw_wcaddr = &rwlock->rw_seq[1];
		rwlock->rw_useqaddr = &rwlock->rw_seq[2];
		rwlock->rw_seq[0]= PTHRW_RW_INIT;
#else /* __LP64__ */
		rwlock->rw_lseqaddr = &rwlock->rw_seq[1];
		rwlock->rw_wcaddr = &rwlock->rw_seq[2];
		rwlock->rw_useqaddr = &rwlock->rw_seq[3];
		rwlock->rw_seq[1]= PTHRW_RW_INIT;
#endif /* __LP64__ */
		
	}
	rwlock->sig = _PTHREAD_RWLOCK_SIG;
	
	return(0);
}

int
_new_pthread_rwlock_rdlock(pthread_rwlock_t * orwlock)
{
#if __DARWIN_UNIX03
	pthread_t self;
#endif /* __DARWIN_UNIX03 */
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	uint32_t lgenval, ugenval, rw_wc, newval, updateval;
	int error = 0, ret;
	uint64_t oldval64, newval64;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				PLOCKSTAT_RW_ERROR(orwlock, READ_LOCK_PLOCKSTAT, error);
				return(error);
			}
		} else {
			PLOCKSTAT_RW_ERROR(orwlock, READ_LOCK_PLOCKSTAT, EINVAL);
			return(EINVAL);
		}
	}
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}
loop:
	lgenval = *lseqaddr;
	ugenval = *useqaddr;
	rw_wc = *wcaddr;
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_RDLOCK | DBG_FUNC_START, (uint32_t)rwlock, lgenval, newval, rw_wc, 0);
#endif
	
	if (is_rw_lbit_set(lgenval))
		goto gotlock;
	if(is_rw_ewubit_clear(lgenval))
		goto gotlock;
	
#if __DARWIN_UNIX03
	if (is_rw_ebit_set(lgenval)) {
		self = pthread_self();
		if(rwlock->rw_owner == self) {
			error = EDEADLK;
			goto out;
		}
	}
#endif /* __DARWIN_UNIX03 */
	
	/* mean Lbit is set and R bit not set; block in kernel */
	newval  = (lgenval + PTHRW_INC);
	
	oldval64 = (((uint64_t)rw_wc) << 32);
	oldval64 |= lgenval;
	
	newval64 = (((uint64_t)(rw_wc + 1)) << 32);
	newval64 |= newval;
	
	if (OSAtomicCompareAndSwap64(oldval64, newval64, (volatile int64_t *)lseqaddr) != TRUE)
		goto loop;

	/* give writers priority over readers */
	PLOCKSTAT_RW_BLOCK(orwlock, READ_LOCK_PLOCKSTAT);

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_RDLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, lgenval, newval, rw_wc+1, 0);
#endif

retry:
	updateval = __psynch_rw_rdlock(orwlock, (newval & ~PTHRW_RW_INIT), ugenval, rw_wc, rwlock->rw_flags);
	
	if (updateval == (uint32_t)-1) {
		error = errno;
	} else
		error = 0;
	
	if (error == EINTR)
		goto retry;
	
	OSAtomicDecrement32((volatile int32_t *)wcaddr);



	if (error == 0)  {
		if ((updateval & PTHRW_RW_HUNLOCK) != 0) {
			ret = rwlock_unlock_action_onread(orwlock, (updateval & ~PTHRW_RW_HUNLOCK));	
			if  (ret != 0) {
				LIBC_ABORT("rdlock_unlock handling failed");
			}
		}
		PLOCKSTAT_RW_BLOCKED(orwlock, READ_LOCK_PLOCKSTAT, BLOCK_SUCCESS_PLOCKSTAT);
		PLOCKSTAT_RW_ACQUIRE(orwlock, READ_LOCK_PLOCKSTAT);    
		return(0);
	} else {
		PLOCKSTAT_RW_BLOCKED(orwlock, READ_LOCK_PLOCKSTAT, BLOCK_FAIL_PLOCKSTAT);
		goto out;
	}
	/* Not reached */	
	
gotlock:
	/* check for max readers */
	ugenval = *useqaddr;
	if (rw_diffgenseq(lgenval, ugenval) >= PTHRW_MAX_READERS) {
		error = EAGAIN;
		goto out;
	}
	
	newval  = (lgenval + PTHRW_INC);

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_RDLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, lgenval, newval, 0);
#endif
	
	if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) == TRUE) {
		PLOCKSTAT_RW_ACQUIRE(orwlock, READ_LOCK_PLOCKSTAT);
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_RDLOCK | DBG_FUNC_END, (uint32_t)rwlock, 0xAAAAAAAA, 0, 0, 0);
#endif
		return(0);
	} else
		goto loop;
out:
	PLOCKSTAT_RW_ERROR(orwlock, READ_LOCK_PLOCKSTAT, error);    
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_RDLOCK | DBG_FUNC_END, (uint32_t)rwlock, 0xAAAAAAAA, error, 0, 0);
#endif
	return(error);
}


int
_new_pthread_rwlock_tryrdlock(pthread_rwlock_t * orwlock)
{
	uint32_t lgenval, newval, ugenval;
	int error = 0;
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		/* check for static initialization */
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				PLOCKSTAT_RW_ERROR(orwlock, READ_LOCK_PLOCKSTAT, error);
				return(error);
			}
		} else {
			PLOCKSTAT_RW_ERROR(orwlock, READ_LOCK_PLOCKSTAT, EINVAL);
			return(EINVAL);
		}
	}
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}

loop:
	lgenval = *lseqaddr;
	if (is_rw_lbit_set(lgenval))
		goto gotlock;
	if (is_rw_ewubit_clear(lgenval))
		goto gotlock;
	
	
	error = EBUSY;
	goto out;
	
gotlock:
	ugenval = *useqaddr;
	if (rw_diffgenseq(lgenval, ugenval) >= PTHRW_MAX_READERS) {
		error = EAGAIN;
		goto out;
	}
	
	newval  = (lgenval + PTHRW_INC);
	if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) == TRUE) {
		PLOCKSTAT_RW_ACQUIRE(orwlock, READ_LOCK_PLOCKSTAT);
		return(0);
	} else
		goto loop;
out:
	PLOCKSTAT_RW_ERROR(orwlock, READ_LOCK_PLOCKSTAT, error);    
	return(error);
}

#ifdef NOTYET
/*****************************************************************************/
/* TBD need to add towards MAX_READERS */
int
_new_pthread_rwlock_longrdlock_np(pthread_rwlock_t * orwlock)
{
	pthread_t self;
	uint32_t lgenval, ugenval, rw_wc, newval, updateval;
	int error = 0, ret;
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	uint64_t oldval64, newval64;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				PLOCKSTAT_RW_ERROR(orwlock, READ_LOCK_PLOCKSTAT, error);
				return(error);
			}
		} else {
			PLOCKSTAT_RW_ERROR(orwlock, READ_LOCK_PLOCKSTAT, EINVAL);
			return(EINVAL);
		}
	}
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}

loop:
	
	lgenval = *lseqaddr;
	ugenval = *useqaddr;
	rw_wc = *wcaddr;
	
	if (is_rw_ewuybit_clear(lgenval))
		goto gotlock;
	
	/* if w bit is set ensure there is no deadlock */
	if (is_rw_ebit_set(lgenval)) {
		self = pthread_self();
		if(rwlock->rw_owner == self) {
			error = EDEADLK;
			goto out;
		}
	}
	
	newval  = (lgenval + PTHRW_INC);
	/* update lock seq and  block in kernel */
	
	oldval64 = (((uint64_t)rw_wc) << 32);
	oldval64 |= lgenval;
	
	newval64 = (((uint64_t)(rw_wc + 1)) << 32);
	newval64 |= newval;
	
	if (OSAtomicCompareAndSwap64(oldval64, newval64, (volatile int64_t *)lseqaddr) != TRUE)
		goto loop;
kblock:
	updateval = __psynch_rw_longrdlock(orwlock, newval, ugenval, (rw_wc+1), rwlock->rw_flags);
	if (updateval == (uint32_t)-1) {
		error = errno;
	} else
		error = 0;
	
	if (error == EINTR)
		goto kblock;
	
	OSAtomicDecrement32((volatile int32_t *)wcaddr);
	if (error == 0) {
	
		if ((updateval & PTHRW_RW_HUNLOCK) != 0) {
			ret = rwlock_unlock_action_onread(orwlock, (updateval & ~PTHRW_RW_HUNLOCK));	
			if  (ret != 0) {
				LIBC_ABORT("rdlock_unlock handling failed");
			}
		}

		error = FALSE;
		while (error == FALSE)	{
			lgenval = *lseqaddr;
			newval = lgenval | PTHRW_LBIT;
			error = OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr);
		}
	
		goto successout;
	} else
		goto out;
	goto successout;
	
gotlock:
	newval = ((lgenval + PTHRW_INC)| PTHRW_LBIT);	
	if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) != TRUE)
		goto loop;
	
successout:
	PLOCKSTAT_RW_ACQUIRE(orwlock, READ_LOCK_PLOCKSTAT);
	return(0);
out:
	PLOCKSTAT_RW_ERROR(orwlock, READ_LOCK_PLOCKSTAT, error);    
	return(error);
}
/**************************************************************/
#endif /* NOTYET */

int
_new_pthread_rwlock_trywrlock(pthread_rwlock_t * orwlock)
{
	int error = 0;
	uint32_t lgenval, newval;
#if __DARWIN_UNIX03
	pthread_t self = pthread_self();
#endif /* __DARWIN_UNIX03 */
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		/* check for static initialization */
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				PLOCKSTAT_RW_ERROR(orwlock, WRITE_LOCK_PLOCKSTAT, error);
				return(error);
			}
		} else {
			PLOCKSTAT_RW_ERROR(orwlock, WRITE_LOCK_PLOCKSTAT, EINVAL);
			return(EINVAL);
		}
	}
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}

	lgenval = PTHRW_RW_INIT;
	newval  = PTHRW_RW_INIT | PTHRW_INC | PTHRW_EBIT;
	if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) == TRUE) {
#if __DARWIN_UNIX03
		rwlock->rw_owner = self;
#endif /* __DARWIN_UNIX03 */
		PLOCKSTAT_RW_ACQUIRE(orwlock, WRITE_LOCK_PLOCKSTAT);    
		return(0);
	}
	PLOCKSTAT_RW_ERROR(orwlock, WRITE_LOCK_PLOCKSTAT, EBUSY);    
	return(EBUSY);
}

int
_new_pthread_rwlock_wrlock(pthread_rwlock_t * orwlock)
{
	uint32_t lgenval, newval, ugenval, updateval, rw_wc;
	int error = 0;
#if __DARWIN_UNIX03
	pthread_t self = pthread_self();
#endif /* __DARWIN_UNIX03 */
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	uint64_t oldval64, newval64;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		/* check for static initialization */
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				PLOCKSTAT_RW_ERROR(orwlock, WRITE_LOCK_PLOCKSTAT, error);
				return(error);
			}
		} else {
			PLOCKSTAT_RW_ERROR(orwlock, WRITE_LOCK_PLOCKSTAT, EINVAL);
			return(EINVAL);
		}
	}
	
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_WRLOCK | DBG_FUNC_START, (uint32_t)rwlock, 0, 0, 0, 0);
#endif
loop:
	lgenval = *lseqaddr;
	ugenval = *useqaddr;
	rw_wc = *wcaddr;
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_WRLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, lgenval, ugenval, rw_wc, 0);
#endif
#if __DARWIN_UNIX03
	if (is_rw_ebit_set(lgenval)) {
		if(rwlock->rw_owner == self) {
			error = EDEADLK;
			goto out;
		}
	}
#endif /* __DARWIN_UNIX03 */
	
	if (lgenval  == PTHRW_RW_INIT) {
		newval  = ( PTHRW_RW_INIT | PTHRW_INC | PTHRW_EBIT);
		if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) == TRUE) {
			goto gotit;
		}
	}
	
	newval = (lgenval + PTHRW_INC) | PTHRW_WBIT | PTHRW_SHADOW_W;	
	
	/* update lock seq and  block in kernel */
	oldval64 = (((uint64_t)rw_wc) << 32);
	oldval64 |= lgenval;
	
	newval64 = (((uint64_t)(rw_wc + 1)) << 32);
	newval64 |= newval;
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_WRLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, lgenval, newval, 0);
#endif
	if (OSAtomicCompareAndSwap64(oldval64, newval64, (volatile int64_t *)lseqaddr) != TRUE)
		goto loop;
		
retry:
	PLOCKSTAT_RW_BLOCK(orwlock, WRITE_LOCK_PLOCKSTAT);
retry1:
	updateval = __psynch_rw_wrlock(orwlock, newval, ugenval, (rw_wc+1), rwlock->rw_flags);
	if (updateval == (uint32_t)-1) {
		error = errno;
	} else
		error = 0;
	
	if (error == EINTR) {
		goto retry1;
	}
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_WRLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x33333333, newval, updateval, 0);
#endif
	PLOCKSTAT_RW_BLOCKED(orwlock, WRITE_LOCK_PLOCKSTAT, BLOCK_SUCCESS_PLOCKSTAT);
	if (error != 0) {
		OSAtomicDecrement32((volatile int32_t *)wcaddr);
		goto out;
	}
	
	if (is_rw_ebit_clear(updateval)) {
		/* kernel cannot wakeup without granting E bit */
		abort();
	}
	
	error = rwlock_exclusive_lockreturn(orwlock, updateval);
	if (error == EAGAIN)
		goto retry;
	
	OSAtomicDecrement32((volatile int32_t *)wcaddr);
	if (error == 0) {
gotit:
#if __DARWIN_UNIX03
		rwlock->rw_owner = self;
#endif /* __DARWIN_UNIX03 */
		PLOCKSTAT_RW_ACQUIRE(orwlock, WRITE_LOCK_PLOCKSTAT);
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_WRLOCK | DBG_FUNC_END, (uint32_t)rwlock, 0xAAAAAAAA, error, 0, 0);
#endif
		return(0);
	} 
out:
	PLOCKSTAT_RW_ERROR(orwlock, WRITE_LOCK_PLOCKSTAT, error);    
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_WRLOCK | DBG_FUNC_END, (uint32_t)rwlock, 0xAAAAAAAA, error, 0, 0);
#endif
	return(error);
}


#ifdef NOTYET
/*****************************************************************************/
int
_new_pthread_rwlock_yieldwrlock_np(pthread_rwlock_t * orwlock)
{
	uint32_t lgenval, newval, ugenval, updateval, rw_wc;
	int error = 0;
#if __DARWIN_UNIX03
	pthread_t self = pthread_self();
#endif /* __DARWIN_UNIX03 */
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	uint64_t oldval64, newval64;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		/* check for static initialization */
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				PLOCKSTAT_RW_ERROR(orwlock, WRITE_LOCK_PLOCKSTAT, error);
				return(error);
			}
		} else {
			PLOCKSTAT_RW_ERROR(orwlock, WRITE_LOCK_PLOCKSTAT, EINVAL);
			return(EINVAL);
		}
	}
	
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}
	
	lgenval = *lseqaddr;
	ugenval = *useqaddr;
	rw_wc = *wcaddr;
	
#if __DARWIN_UNIX03
	if (is_rw_ebit_set(lgenval)) {
		if (rwlock->rw_owner == self) {
			error = EDEADLK;
			goto out;
		}
	}
#endif /* __DARWIN_UNIX03 */
	
	if (lgenval == PTHRW_RW_INIT) {
		newval  = PTHRW_RW_INIT | PTHRW_INC | PTHRW_EBIT;
		if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) == TRUE) {
			goto gotit;
		}
	}
	
	newval = (lgenval + PTHRW_INC);	
	if ((lgenval & PTHRW_WBIT) == 0)
		newval |= PTHRW_YBIT;
	
	oldval64 = (((uint64_t)rw_wc) << 32);
	oldval64 |= lgenval;
	
	newval64 = (((uint64_t)(rw_wc + 1)) << 32);
	newval64 |= newval;
	
	if (OSAtomicCompareAndSwap64(oldval64, newval64, (volatile int64_t *)lseqaddr) != TRUE)
		PLOCKSTAT_RW_BLOCK(orwlock, WRITE_LOCK_PLOCKSTAT);
retry:
	updateval = __psynch_rw_yieldwrlock(orwlock, newval, ugenval, (rw_wc+1), rwlock->rw_flags);
	if (updateval == (uint32_t)-1) {
		error = errno;
	} else
		error = 0;
	
	if (error == EINTR)
		goto retry;
	
	
	PLOCKSTAT_RW_BLOCKED(orwlock, WRITE_LOCK_PLOCKSTAT, BLOCK_SUCCESS_PLOCKSTAT);
	if (error != 0) {
		OSAtomicDecrement32((volatile int32_t *)wcaddr);
		goto out;
	}
	
	if (is_rw_ebit_clear(updateval)) {
		/* kernel cannot wakeup without granting E bit */
		abort();
	}
	
	error = rwlock_exclusive_lockreturn(orwlock, updateval);
	if (error == EAGAIN)
		goto retry;
	
	OSAtomicDecrement32((volatile int32_t *)wcaddr);
	if (error == 0) {
	gotit:
#if __DARWIN_UNIX03
		rwlock->rw_owner = self;
#endif /* __DARWIN_UNIX03 */
		PLOCKSTAT_RW_ACQUIRE(orwlock, WRITE_LOCK_PLOCKSTAT);
		return(0);
	} else {
		PLOCKSTAT_RW_ERROR(orwlock, WRITE_LOCK_PLOCKSTAT, error);    
	}
	return(error);
out:
	PLOCKSTAT_RW_ERROR(orwlock, WRITE_LOCK_PLOCKSTAT, error);    
	return(error);
}
/**************************************************************/
#endif /* NOTYET */

int
_new_pthread_rwlock_unlock(pthread_rwlock_t * orwlock)
{
	uint32_t lgenval, ugenval, rw_wc, newval, nlval, ulval;
	int error = 0;
	int wrlock = 0, kern_trans;
	uint32_t updateval, bits, newbits;
	uint32_t isupgrade = 0;
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	int retry_count = 0, retry_count1 = 0;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	pthread_t self = NULL;
	uint64_t threadid = 0;
	int ubitchanged = 0, initbitset = 0, num;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		/* check for static initialization */
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				PLOCKSTAT_RW_ERROR(orwlock, wrlock, error);
				return(error);
			}
		} else {
			PLOCKSTAT_RW_ERROR(orwlock, wrlock, EINVAL);
			return(EINVAL);
		}
	}
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_START, (uint32_t)rwlock, 0, 0, 0, 0);
#endif
loop:
	lgenval = *lseqaddr;
	ugenval = *useqaddr;
	rw_wc = *wcaddr;
	

loop1:
	if ((lgenval & PTHRW_COUNT_MASK) == (ugenval & PTHRW_COUNT_MASK)) {
		retry_count++;
		sched_yield();
		if (retry_count < 1024)
			goto loop;
		error = EINVAL;
		goto out;
	}
	retry_count = 0;
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, lgenval, ugenval, 0);
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, rw_wc, 0, 0);
#endif
	if (is_rw_ebit_set(lgenval)) {
		wrlock = 1;
#if __DARWIN_UNIX03
		rwlock->rw_owner = (pthread_t)0;
#endif /* __DARWIN_UNIX03 */
	}
	
	/* last unlock ? */
	if((lgenval & PTHRW_COUNT_MASK) == (ugenval + PTHRW_INC)) {
		if (OSAtomicCompareAndSwap32(ugenval, 0, (volatile int32_t *)useqaddr) != TRUE)  {
			goto loop;
		}
		if (OSAtomicCompareAndSwap32(lgenval, PTHRW_RW_INIT, (volatile int32_t *)lseqaddr) != TRUE)  {
			if (OSAtomicCompareAndSwap32(0, ugenval, (volatile int32_t *)useqaddr) != TRUE) {
lp1:
				ulval = *useqaddr;
				nlval = ugenval+ulval;
				if (OSAtomicCompareAndSwap32(ulval, nlval, (volatile int32_t *)useqaddr) != TRUE)
					goto lp1;
			}
			
			goto loop;
		}
		
		goto succout;
	}
	
	/* do we need kernel trans? */
	
lp11:
	nlval = lgenval & PTHRW_COUNT_MASK;
	if (ubitchanged == 0)
		ulval = (ugenval + PTHRW_INC) & PTHRW_COUNT_MASK;
	else
		ulval = ugenval  & PTHRW_COUNT_MASK;
		
	num = rw_diffgenseq(nlval, ulval);
	kern_trans = ( num == (rw_wc << PTHRW_COUNT_SHIFT));
	/* if three more waiters than needed for kernel tras*/
	if ((ubitchanged ==0) && (kern_trans == 0) && (num < (rw_wc << PTHRW_COUNT_SHIFT))) {
			retry_count1++;
			sched_yield();
			if (retry_count1 < 1024)
				goto loop;
	}
	retry_count1 = 0;
	
	if (ubitchanged == 0) {
		if (OSAtomicCompareAndSwap32(ugenval, ugenval+PTHRW_INC, (volatile int32_t *)useqaddr) != TRUE)
			goto loop;
		ubitchanged = 1;
	}
	

	if (kern_trans == 0) {
		goto succout;
	}
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 1, ugenval+PTHRW_INC, 0);
#endif
	initbitset = 0;
	bits = lgenval & PTHRW_BIT_MASK;
	newbits = bits;
	/* if this is first unlock to kernel, notify kernel of init status */
	if ((bits & PTHRW_RW_INIT) != 0) {
		/* reset the initbit if present */
		newbits &= ~PTHRW_RW_INIT;
		initbitset = PTHRW_RW_INIT;
	}
	if (((bits & PTHRW_EBIT) != 0) && ((bits & PTHRW_WBIT) == 0)) {
		/* reset E bit is no U bit is set */
		newbits &= ~PTHRW_EBIT;
	}
	/* clear shadow bit, as W is going to be sent to kernel */
	if ((bits & PTHRW_WBIT) != 0) {
		newbits &= ~PTHRW_SHADOW_W;
	}

	/* reset L bit */
	if (bits & PTHRW_LBIT)
		newbits &= ~PTHRW_LBIT;
	if (bits & PTHRW_UBIT) {
		/* reset U and set E bit */
		newbits &= ~PTHRW_LBIT;
		newbits |= PTHRW_EBIT;
		isupgrade = PTHRW_UBIT;
	}
	
	/* updates bits  on the L */
	newval = (lgenval & PTHRW_COUNT_MASK) | newbits;
	if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) != TRUE) {
		/* reread the value */
		lgenval = *lseqaddr;
		ugenval = *useqaddr;
		rw_wc = *wcaddr;
		/* since lgen changed check for trans again */
		goto lp11;
	}
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 2, newval, 0);
#endif
	
	/* send upgrade bit to kernel */	
	newval |= (isupgrade | initbitset);
	updateval = __psynch_rw_unlock(orwlock, newval, ugenval+PTHRW_INC, rw_wc, rwlock->rw_flags);
	if (updateval == (uint32_t)-1) {
		error = errno;
	} else
		error = 0;
	
	if(error != 0) {
		/* not sure what is the scenario */
		if(error != EINTR)
			goto out;
	}
	
	/*
	 * If the unlock is spurious return. Also if the
	 * exclusive lock is being granted, let that thread
	 * manage the status bits, otherwise stale bits exclusive 
	 * bit can be set, if that thread had already unlocked.
	 */
	if ((updateval & (PTHRW_RW_SPURIOUS | PTHRW_EBIT)) != 0) {
		goto succout;
	}

lp2:
	lgenval = *lseqaddr;
	

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 3, lgenval, 0);
#endif
	/* if the kernel antcipated seq and one on the lock are same, set the one from kernel */
	if ((lgenval & PTHRW_COUNT_MASK) == (updateval & PTHRW_COUNT_MASK)) {
		if (OSAtomicCompareAndSwap32(lgenval, updateval, (volatile int32_t *)lseqaddr) != TRUE)
			goto lp2;
		goto succout;
	}
	
	/* state bits are same? */
	if ((lgenval & PTHRW_BIT_MASK) == (updateval & PTHRW_BIT_MASK)) {
		/* nothing to do */
		goto succout;
	}
	
	newval = ((lgenval & PTHRW_UN_BIT_MASK) << PTHRW_COUNT_SHIFT) | (updateval & PTHRW_BIT_MASK);
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 4, newval, 0);
#endif
	/* high bits are state on the lock; lowbits are one kernel need to  set */
	switch (newval) {
			/* W States */
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action_k(orwlock, lgenval, updateval);
			//goto ktrans;
		}
			break;
			
			
			/* L states */
		case ((PTHRW_LBIT << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
			
			/* Y states */
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action_k(orwlock, lgenval, updateval);
			//goto ktrans;
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action_k(orwlock, lgenval, updateval);
			//goto ktrans;
		}
			break;
			
			/* YU states */
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action_k(orwlock, lgenval, updateval);
			//goto ktrans;
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action_k(orwlock, lgenval, updateval);
			//goto ktrans;
		}
			break;
			
			/* E states */
		case ((PTHRW_EBIT << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
			
			/* WE states */
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
			
			/* WL states */
		case (((PTHRW_WBIT | PTHRW_LBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_LBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_LBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
			
		default:
			/* illegal states */
			self = pthread_self();
			threadid = self->thread_id;
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 6, lgenval, 0);
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 7, updateval, 0);
#endif
			LIBC_ABORT("incorect state on return 0x%x: lgenval 0x%x, updateval 0x%x; threadid (0x%x)\n", newval, lgenval, updateval, (uint32_t)threadid);
	
	};
	
	if (error != 0)
		goto lp2;
succout:
	PLOCKSTAT_RW_RELEASE(orwlock, wrlock);
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_END, (uint32_t)rwlock, 0xAAAAAAAA, error, 0, 0);
#endif
	return(0);
out:
	PLOCKSTAT_RW_ERROR(orwlock, wrlock, error);    
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_END, (uint32_t)rwlock, 0xAAAAAAAA, error, 0, 0);
#endif
	return(error);
}

#ifdef NOTYET
/*****************************************************************************/
int
_new_pthread_rwlock_downgrade_np(pthread_rwlock_t * orwlock)
{
	uint32_t lgenval, newval, ugenval, rw_wc;
	int error = 0;
	pthread_t self = pthread_self();
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		/* check for static initialization */
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				return(error);
			}
		} else {
			return(EINVAL);
		}
	}
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}
	
loop:
	lgenval = *lseqaddr;
	ugenval = *useqaddr;
	rw_wc = *wcaddr;
	
	if ((is_rw_ebit_set(lgenval )) && (rwlock->rw_owner != self)) {
		return(EINVAL);
	}
	
	if ((lgenval & PTHRW_COUNT_MASK) != ugenval) {
		
		newval = lgenval & ~PTHRW_EBIT;
		
		if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) == TRUE) {
#if __DARWIN_UNIX03
			rwlock->rw_owner = 0;
#endif /* __DARWIN_UNIX03 */
			if (rw_wc != 0) {
				error = __psynch_rw_downgrade(orwlock, newval, ugenval, rw_wc, rwlock->rw_flags);
				
			}
			return(0);
		} else {
			goto loop;
		}
	}
	return(EINVAL);	
}


int
_new_pthread_rwlock_upgrade_np(pthread_rwlock_t * orwlock)
{
	uint32_t lgenval, newval, ugenval, ulval, updateval, rw_wc;
	int error = 0, kern_trans;
	pthread_t self = pthread_self();
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	uint64_t oldval64, newval64;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		/* check for static initialization */
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				return(error);
			}
		} else {
			return(EINVAL);
		}
	}
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}
loop:
	lgenval = *lseqaddr;
	ugenval = *useqaddr;
	rw_wc = *wcaddr;
	
	if (is_rw_uebit_set(lgenval)) {
		return(EINVAL);
		
	}
	
	if ((lgenval & PTHRW_COUNT_MASK) == ugenval)
		return(EINVAL);
	
	if (lgenval > ugenval)
		ulval = (lgenval & PTHRW_COUNT_MASK) - (ugenval & PTHRW_COUNT_MASK);
	else
		ulval = (ugenval & PTHRW_COUNT_MASK) - (lgenval & PTHRW_COUNT_MASK);
	
	
 	newval = lgenval | PTHRW_UBIT;
	
	kern_trans = 1;
	if (rw_wc != 0)  {
		if (ulval == ((rw_wc - 1) << PTHRW_COUNT_SHIFT))
			kern_trans = 0;
	} else if (ulval == 1)
		kern_trans = 0;
	
	if (kern_trans == 0) {
		newval = ((lgenval | PTHRW_EBIT) & ~PTHRW_LBIT);
	} else {
		newval = lgenval | PTHRW_UBIT;
	}
	if (kern_trans == 0) {
		if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) != TRUE)
			goto loop;
		
	} else {
		newval  = (lgenval + PTHRW_INC);
		
		oldval64 = (((uint64_t)rw_wc) << 32);
		oldval64 |= lgenval;
		
		newval64 = (((uint64_t)(rw_wc + 1)) << 32);
		newval64 |= newval;
		
		if (OSAtomicCompareAndSwap64(oldval64, newval64, (volatile int64_t *)lseqaddr) != TRUE)
			goto loop;
		/* kern_trans == 1 */
	retry:
		updateval = __psynch_rw_upgrade(orwlock, newval, ugenval, rw_wc+1, rwlock->rw_flags);
		if (updateval == (uint32_t)-1) {
			error = errno;
		} else
			error = 0;
		
		if (error == EINTR)
			goto retry;
		
		if (error != 0)  {
			OSAtomicDecrement32((volatile int32_t *)wcaddr);
			goto out;
		}
		
		if (is_rw_ebit_set(updateval)) {
			/* kernel cannot wakeup without granting E bit */
			abort();
		}
		
		error = rwlock_exclusive_lockreturn(orwlock, updateval);
		if (error == EAGAIN)
			goto retry;
		
		OSAtomicDecrement32((volatile int32_t *)wcaddr);
		
	}
	if (error == 0) {
		rwlock->rw_owner = self;
		PLOCKSTAT_RW_ACQUIRE(orwlock, WRITE_LOCK_PLOCKSTAT);
		return(0);
	}
	
out:
	return(error);	
}

int
pthread_rwlock_tryupgrade_np(pthread_rwlock_t *orwlock)
{
	pthread_t self = pthread_self();
	uint32_t lgenval, newval, ugenval, ulval, rw_wc;
	int error = 0, kern_trans;
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				return(error);
			}
		} else {
			return(EINVAL);
		}
	}
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}

loop:
	lgenval = *lseqaddr;
	ugenval = *useqaddr;
	rw_wc = *wcaddr;
	
	if (is_rw_uebit_set(lgenval)) {
		return(EBUSY);
	}
	
	if ((lgenval & PTHRW_COUNT_MASK) == ugenval)
		return(EINVAL);
	
	if (lgenval > ugenval)
		ulval = (lgenval & PTHRW_COUNT_MASK) - (ugenval & PTHRW_COUNT_MASK);
	else
		ulval = (ugenval & PTHRW_COUNT_MASK) - (lgenval & PTHRW_COUNT_MASK);
	
	
 	newval = lgenval | PTHRW_UBIT;
	
	kern_trans = 1;
	if (rw_wc != 0)  {
		/* there is only one reader thread */
		if (ulval == (rw_wc - 1)) 
			kern_trans = 0;
	} else if (ulval == 1)
		kern_trans = 0;
	
	if (kern_trans == 0) {
		newval = (lgenval | PTHRW_EBIT) & ~PTHRW_LBIT;
		if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) != TRUE)
			goto loop;
		
		rwlock->rw_owner = self;
		PLOCKSTAT_RW_ACQUIRE(orwlock, WRITE_LOCK_PLOCKSTAT);
		return(0);
	}
	return(EBUSY);	
}

/* Returns true if the rwlock is held for reading by any thread or held for writing by the current thread */
int 
pthread_rwlock_held_np(pthread_rwlock_t * orwlock)
{
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	uint32_t lgenval, ugenval;
	int error = 0;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				return(0);
			}
		} else {
			return(-1);
		}
	}
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}
	
	lgenval = *lseqaddr;
	ugenval = *useqaddr;
	
	if ((lgenval & PTHRW_COUNT_MASK) == (ugenval & PTHRW_COUNT_MASK))
		return(0);
	
	return(1);
}

/* Returns true if the rwlock is held for reading by any thread */
int 
pthread_rwlock_rdheld_np(pthread_rwlock_t * orwlock)
{
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	uint32_t lgenval;
	int error = 0;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				return(0);
			}
		} else {
			return(-1);
		}
	}
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}

	lgenval = *lseqaddr;
	
	if (is_rw_ebit_set(lgenval)) {
		return(0);
	}
	return(0);
}

/* Returns true if the rwlock is held for writing by the current thread */
int 
pthread_rwlock_wrheld_np(pthread_rwlock_t * orwlock)
{
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	pthread_t self;
	uint32_t lgenval;
	int error = 0;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		 	if ((error = pthread_rwlock_init(orwlock, NULL)) != 0)  {
				return(0);
			}
		} else {
			return(-1);
		}
	}
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}

	self = pthread_self();

	lgenval = *lseqaddr;
	if ((is_rw_ebit_set(lgenval)) && (rwlock->rw_owner == self)) {
		return(1);
	}
	return(0);
}
/**************************************************************/
#endif /* NOTYET */

static int
rwlock_unlock_action_onread(pthread_rwlock_t * orwlock, uint32_t updateval)
{
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	int error = 0;
	uint32_t lgenval, newval;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	pthread_t self;
	uint64_t threadid;
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}

	lgenval = *lseqaddr;

lp2:
	lgenval = *lseqaddr;
	

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 3, lgenval, 0);
#endif
	/* if the kernel antcipated seq and one on the lock are same, set the one from kernel */
	if ((lgenval & PTHRW_COUNT_MASK) == (updateval & PTHRW_COUNT_MASK)) {
		if (OSAtomicCompareAndSwap32(lgenval, updateval, (volatile int32_t *)lseqaddr) != TRUE)
			goto lp2;
		goto succout;
	}
	
	/* state bits are same? */
	if ((lgenval & PTHRW_BIT_MASK) == (updateval & PTHRW_BIT_MASK)) {
		/* nothing to do */
		goto succout;
	}
	
	newval = ((lgenval & PTHRW_UN_BIT_MASK) << PTHRW_COUNT_SHIFT) | (updateval & PTHRW_BIT_MASK);
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 4, newval, 0);
#endif
	/* high bits are state on the lock; lowbits are one kernel need to  set */
	switch (newval) {
			/* W States */
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action_k(orwlock, lgenval, updateval);
			//goto ktrans;
		}
			break;
			
			
			/* L states */
		case ((PTHRW_LBIT << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
			
			/* Y states */
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action_k(orwlock, lgenval, updateval);
			//goto ktrans;
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action_k(orwlock, lgenval, updateval);
			//goto ktrans;
		}
			break;
			
			/* YU states */
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action_k(orwlock, lgenval, updateval);
			//goto ktrans;
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action_k(orwlock, lgenval, updateval);
			//goto ktrans;
		}
			break;
			
			/* E states */
		case ((PTHRW_EBIT << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
			
			/* WE states */
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action2(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
			
			/* WL states */
		case (((PTHRW_WBIT | PTHRW_LBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_LBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_LBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_LBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
			
		default:
			/* illegal states */
			self = pthread_self();
			threadid = self->thread_id;
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 6, lgenval, 0);
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNLOCK | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 7, updateval, 0);
#endif
			LIBC_ABORT("incorect state on return 0x%x: lgenval 0x%x, updateval 0x%x; threadid (0x%x)\n", newval, lgenval, updateval, (uint32_t)threadid);
	};
	
	if (error != 0)
		goto lp2;
	
succout:
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACT1 | DBG_FUNC_NONE, lgenval, newval, 0, 0, 0);
#endif
	return(0);
}


static uint32_t
modbits(uint32_t lgenval, uint32_t updateval)
{
	uint32_t lval = lgenval & PTHRW_BIT_MASK;
	uint32_t uval = updateval & PTHRW_BIT_MASK;
	uint32_t rval, nlval;
	
	nlval = (lval | uval);
	if ((uval & PTHRW_EBIT) == 0)
		nlval &= ~PTHRW_EBIT;
	if ((nlval & (PTHRW_WBIT | PTHRW_YBIT)) == (PTHRW_WBIT | PTHRW_YBIT))
		nlval &= ~PTHRW_YBIT;
	/* no new writers and kernel resets w bit, reset W bit on the lock */
	if (((nlval & (PTHRW_WBIT | PTHRW_SHADOW_W)) == PTHRW_WBIT) && ((updateval & PTHRW_WBIT) == 0))
		nlval &= ~PTHRW_WBIT;

	rval = (lgenval & PTHRW_COUNT_MASK) | nlval;
	return(rval);
}

static int
rwlock_unlock_action1(pthread_rwlock_t * orwlock, uint32_t lgenval, uint32_t updateval)
{
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	int error = 0;
	uint32_t newval;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}

	newval = modbits(lgenval, updateval);
	if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) != TRUE) 
		error = EINVAL;
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACT1 | DBG_FUNC_NONE, lgenval, newval, 0, 0, 0);
#endif
	return(error);
}

static int
rwlock_unlock_action2(pthread_rwlock_t * orwlock, uint32_t lgenval, uint32_t updateval)
{
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	uint32_t newval;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}

	newval = modbits(lgenval, updateval);
	if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) == TRUE) {
		/* roundtrip kernel */
		
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACT2 | DBG_FUNC_NONE, lgenval, newval, 0, 0, 0);
#endif
		(void) __psynch_rw_unlock2(orwlock, lgenval, *useqaddr, *wcaddr, rwlock->rw_flags);
		return(0);
	}
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACT2 | DBG_FUNC_NONE, 0xffffffff, 0, 0, 0, 0);
#endif
	
	return(EINVAL);
}

/* This is used when an exclusive write lock of any kind is being granted. For unlock thread, it needs to try to set the bit, if not move on */
static int
rwlock_unlock_action_k(pthread_rwlock_t * orwlock, uint32_t lgenval, uint32_t updateval)
{
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	uint32_t newval;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}

	newval = modbits(lgenval, updateval);
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACTK | DBG_FUNC_NONE, lgenval, updateval, newval, 0, 0);
#endif
	/* try to set, if not not a prolem as the thread taking exclusive will take care of the discrepency */

	if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) == TRUE) {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACTK | DBG_FUNC_NONE, 0x55555555, lgenval, newval, 0, 0);
#endif

	} else {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACTK | DBG_FUNC_NONE, 0xAAAAAAAA, lgenval, newval, 0, 0);
#endif

	}

	return(0);
}

static int
rwlock_exclusive_lockreturn(pthread_rwlock_t * orwlock, uint32_t updateval)
{
	npthread_rwlock_t * rwlock = (npthread_rwlock_t *)orwlock;
	uint32_t lgenval, newval;
	volatile uint32_t * lseqaddr, *useqaddr, *wcaddr;
	pthread_t self;
	uint64_t threadid;
	
	int error = 0;

	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		RWLOCK_GETSEQ_ADDR(rwlock, lseqaddr, useqaddr, wcaddr);
	} else {
		lseqaddr = rwlock->rw_lseqaddr;
		useqaddr = rwlock->rw_useqaddr;
		wcaddr = rwlock->rw_wcaddr;
	}
	
lp2:
	lgenval = *lseqaddr;
	
	/* if the kernel antcipated seq and one on the lock are same, set the one from kernel */
	if ((lgenval & PTHRW_COUNT_MASK) == (updateval & PTHRW_COUNT_MASK)) {
		if (OSAtomicCompareAndSwap32(lgenval, updateval, (volatile int32_t *)lseqaddr) != TRUE)
			goto lp2;
		goto out;
	}
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACTE | DBG_FUNC_NONE, lgenval, updateval, 1, 0, 0);
#endif
	/* state bits are same? */
	if ((lgenval & PTHRW_BIT_MASK) == (updateval & PTHRW_BIT_MASK)) {
		/* nothing to do */
		goto out;
	}
	
	
	newval = ((lgenval & PTHRW_UN_BIT_MASK) << PTHRW_COUNT_SHIFT) | (updateval & PTHRW_BIT_MASK);
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACTE | DBG_FUNC_NONE, newval, 0, 2, 0, 0);
#endif
	/* high bits are state on the lock; lowbits are one kernel need to  set */
	switch (newval) {
			/* W States */
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case ((PTHRW_WBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = EAGAIN;
		}
			break;
			
			
			/* All  L states illegal here */
			
			/* Y states */
		case (PTHRW_YBIT << PTHRW_COUNT_SHIFT) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = EAGAIN;
		}
			break;
		case ((PTHRW_YBIT << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = EAGAIN;
		}
			break;
			
			/* YU states */
		case ((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = EAGAIN;
		}
			break;
			
		case (((PTHRW_YBIT | PTHRW_UBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = EAGAIN;
		}
			break;
			
			/* E states */
		case ((PTHRW_EBIT << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
			
			/* WE states */
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_WBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
		case (((PTHRW_WBIT | PTHRW_EBIT) << PTHRW_COUNT_SHIFT) | (PTHRW_YBIT | PTHRW_EBIT)) : {
			error = rwlock_unlock_action1(orwlock, lgenval, updateval);
		}
			break;
			
			/* All WL states are illegal*/
			
		default:
			/* illegal states */
			self = pthread_self();
			threadid = self->thread_id;
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACTE | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 6, lgenval, 0);
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACTE | DBG_FUNC_NONE, (uint32_t)rwlock, 0x55555555, 7, updateval, 0);
#endif
			LIBC_ABORT("rwlock_exclusive_lockreturn: incorect state on return 0x%x: lgenval 0x%x, updateval 0x%x; threadid (0x%x)\n", newval, lgenval, updateval, (uint32_t)threadid);
	};
	
	if (error == EINVAL)
		goto lp2;
out:
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_RW_UNACTE | DBG_FUNC_NONE, error, 0, 0xffffffff, 0, 0);
#endif
	return(error);
}

/* returns are not bit shifted */
static int
rw_diffgenseq(uint32_t x, uint32_t y)
{
	uint32_t lx = (x & PTHRW_COUNT_MASK);
	uint32_t ly = (y &PTHRW_COUNT_MASK);

	if (lx  > ly) {
		return(lx-ly);
	} else {
		return((PTHRW_MAX_READERS - y) + lx + PTHRW_INC);
	}

}

#endif /* i386 || x86_64  ] */


#endif /* !BUILDING_VARIANT ] */

int
pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
#if  defined(__i386__) || defined(__x86_64__) ||  defined(__DARWIN_UNIX03)
	int ret;
#endif /* __i386__ || __x86_64__ */
	

#if  defined(__i386__) || defined(__x86_64__)
	if ((usenew_impl != 0)) {
		return(_new_pthread_rwlock_destroy(rwlock));
	}
#endif /* __i386__ || __x86_64__ */

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		return(EINVAL);
	} 
#if  defined(__i386__) || defined(__x86_64__)
	else if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		ret = _new_pthread_rwlock_destroy(rwlock);
		return(ret);
	}
#endif /* __i386__ || __x86_64__ */
	else {
#if __DARWIN_UNIX03
	    /* grab the monitor lock */
    	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0)
        return(ret);

    	if (rwlock->state != 0) {
        	pthread_mutex_unlock(&rwlock->lock);
        	return(EBUSY);
    	}
		pthread_mutex_unlock(&rwlock->lock);
#endif /* __DARWIN_UNIX03 */

		pthread_mutex_destroy(&rwlock->lock);
		pthread_cond_destroy(&rwlock->read_signal);
		pthread_cond_destroy(&rwlock->write_signal);
		rwlock->sig = _PTHREAD_NO_SIG;
		return(0);
	}
}

int
pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
	int			ret;

#if  defined(__i386__) || defined(__x86_64__)
	if ((usenew_impl != 0)) {
		return(_new_pthread_rwlock_init(rwlock, attr));
	}
#endif /* __i386__ || __x86_64__ */

#if __DARWIN_UNIX03
		if (attr && (attr->sig != _PTHREAD_RWLOCK_ATTR_SIG)) {
			return(EINVAL);
		}
#endif /* __DARWIN_UNIX03 */

#if  defined(__i386__) || defined(__x86_64__)
	 if ((attr != NULL) && (attr->pshared == PTHREAD_PROCESS_SHARED)) {
		ret = _new_pthread_rwlock_init(rwlock, attr);
		return(ret);
	}
#endif /* __i386__ || __x86_64__ */

#if __DARWIN_UNIX03
		/* if already inited  check whether it is in use, then return EBUSY */
		if ((rwlock->sig == _PTHREAD_RWLOCK_SIG) && (rwlock->state !=0 )) {
			return(EBUSY);
		}
#endif /* __DARWIN_UNIX03 */

	/* initialize the lock */
	if ((ret = pthread_mutex_init(&rwlock->lock, NULL)) != 0)
		return(ret);
	else {
		/* initialize the read condition signal */
		ret = pthread_cond_init(&rwlock->read_signal, NULL);

		if (ret != 0) {
			pthread_mutex_destroy(&rwlock->lock);
			return(ret);
		} else {
			/* initialize the write condition signal */
			ret = pthread_cond_init(&rwlock->write_signal, NULL);

			if (ret != 0) {
				pthread_cond_destroy(&rwlock->read_signal);
				pthread_mutex_destroy(&rwlock->lock);
				return(ret);
			} else {
				/* success */
				rwlock->state = 0;
				rwlock->owner = (pthread_t)0;
				rwlock->blocked_writers = 0;
				if (attr)
					rwlock->pshared = attr->pshared;
				else
					rwlock->pshared = _PTHREAD_DEFAULT_PSHARED;
					
				rwlock->sig = _PTHREAD_RWLOCK_SIG;
				return(0);
			}
		}
	}
}

int
pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
	int			ret;
#if __DARWIN_UNIX03
	pthread_t self = pthread_self();	
#endif

#if  defined(__i386__) || defined(__x86_64__)
	if ((usenew_impl != 0)) {
		return(_new_pthread_rwlock_rdlock(rwlock));
	}
#endif /* __i386__ || __x86_64__ */

	if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		if ((ret = pthread_rwlock_init(rwlock, NULL)) != 0)  {
			PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, ret);
			return(ret);
		}
	}

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, EINVAL);
		return(EINVAL);
	}
#if  defined(__i386__) || defined(__x86_64__)
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		ret = _new_pthread_rwlock_rdlock(rwlock);
		return(ret);
	}
#endif /* __i386__ || __x86_64__ */
	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0) {
		PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, ret);    
		return(ret);
	}

#if __DARWIN_UNIX03
	if ((rwlock->state < 0) && (rwlock->owner == self)) {
		pthread_mutex_unlock(&rwlock->lock);
		PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, EDEADLK);    
		return(EDEADLK);
	}
#endif /* __DARWIN_UNIX03 */

#if __DARWIN_UNIX03
	while (rwlock->blocked_writers || ((rwlock->state < 0) && (rwlock->owner != self))) 
#else /* __DARWIN_UNIX03 */
	while (rwlock->blocked_writers || rwlock->state < 0) 

#endif /* __DARWIN_UNIX03 */
	{
	/* give writers priority over readers */
		PLOCKSTAT_RW_BLOCK(rwlock, READ_LOCK_PLOCKSTAT);
		ret = pthread_cond_wait(&rwlock->read_signal, &rwlock->lock);

		if (ret != 0) {
			/* can't do a whole lot if this fails */
			pthread_mutex_unlock(&rwlock->lock);
			PLOCKSTAT_RW_BLOCKED(rwlock, READ_LOCK_PLOCKSTAT, BLOCK_FAIL_PLOCKSTAT);
			PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, ret);    
			return(ret);
		}

		PLOCKSTAT_RW_BLOCKED(rwlock, READ_LOCK_PLOCKSTAT, BLOCK_SUCCESS_PLOCKSTAT);
	}

	/* check lock count */
	if (rwlock->state == MAX_READ_LOCKS) {
		ret = EAGAIN;
		PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, ret);    
	}
	else {
		++rwlock->state; /* indicate we are locked for reading */
		PLOCKSTAT_RW_ACQUIRE(rwlock, READ_LOCK_PLOCKSTAT);    
	}

	/*
	 * Something is really wrong if this call fails.  Returning
	 * error won't do because we've already obtained the read
	 * lock.  Decrementing 'state' is no good because we probably
	 * don't have the monitor lock.
	 */
	pthread_mutex_unlock(&rwlock->lock);

	return(ret);
}

int
pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
	int			ret;

#if  defined(__i386__) || defined(__x86_64__)
	if ((usenew_impl != 0)) {
		return(_new_pthread_rwlock_tryrdlock(rwlock));
	}
#endif /* __i386__ || __x86_64__ */

	/* check for static initialization */
	if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		if ((ret = pthread_rwlock_init(rwlock, NULL)) != 0)  {
			PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, ret);    
			return(ret);
		}
	}

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, EINVAL);    
		return(EINVAL);
	}
#if  defined(__i386__) || defined(__x86_64__)
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		ret = _new_pthread_rwlock_tryrdlock(rwlock);
		return(ret);
	}
#endif /* __i386__ || __x86_64__ */

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0) {
		PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, ret);    
		return(ret);
	}

	/* give writers priority over readers */
	if (rwlock->blocked_writers || rwlock->state < 0) {
		ret = EBUSY;
		PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, ret);    
	}
	else if (rwlock->state == MAX_READ_LOCKS) {
		ret = EAGAIN; /* too many read locks acquired */
		PLOCKSTAT_RW_ERROR(rwlock, READ_LOCK_PLOCKSTAT, ret);    
	}
	else {
		++rwlock->state; /* indicate we are locked for reading */
		PLOCKSTAT_RW_ACQUIRE(rwlock, READ_LOCK_PLOCKSTAT);    
	}

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&rwlock->lock);

	return(ret);
}

int
pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
	int			ret;
#if __DARWIN_UNIX03
	pthread_t self = pthread_self();
#endif /* __DARWIN_UNIX03 */

#if  defined(__i386__) || defined(__x86_64__)
	if ((usenew_impl != 0)) {
		return(_new_pthread_rwlock_trywrlock(rwlock));
	}
#endif /* __i386__ || __x86_64__ */

	/* check for static initialization */
	if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		if ((ret = pthread_rwlock_init(rwlock, NULL)) != 0)  {
			PLOCKSTAT_RW_ERROR(rwlock, WRITE_LOCK_PLOCKSTAT, ret);    
			return(ret);
		}
	}

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		PLOCKSTAT_RW_ERROR(rwlock, WRITE_LOCK_PLOCKSTAT, EINVAL);    
		return(EINVAL);
	}

#if  defined(__i386__) || defined(__x86_64__)
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		ret = _new_pthread_rwlock_trywrlock(rwlock);
		return(ret);
	}
#endif /* __i386__ || __x86_64__ */

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0) {
		PLOCKSTAT_RW_ERROR(rwlock, WRITE_LOCK_PLOCKSTAT, ret);    
		return(ret);
	}


	if (rwlock->state != 0) {
		ret = EBUSY;
		PLOCKSTAT_RW_ERROR(rwlock, WRITE_LOCK_PLOCKSTAT, ret);    
	}
	else {
		/* indicate we are locked for writing */
		rwlock->state = -1;
#if __DARWIN_UNIX03
		rwlock->owner = self;
#endif /* __DARWIN_UNIX03 */
		PLOCKSTAT_RW_ACQUIRE(rwlock, WRITE_LOCK_PLOCKSTAT);    
	}

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&rwlock->lock);

	return(ret);
}

int
pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
	int			ret;
	int			writer = (rwlock < 0) ? 1:0;

#if  defined(__i386__) || defined(__x86_64__)
	if ((usenew_impl != 0)) {
		return(_new_pthread_rwlock_unlock(rwlock));
	}
#endif /* __i386__ || __x86_64__ */

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		PLOCKSTAT_RW_ERROR(rwlock, writer, EINVAL);    
		return(EINVAL);
	}

#if  defined(__i386__) || defined(__x86_64__)
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		ret = _new_pthread_rwlock_unlock(rwlock);
		return(ret);
	}
#endif /* __i386__ || __x86_64__ */


	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0) {
		PLOCKSTAT_RW_ERROR(rwlock, writer, ret);    
		return(ret);
	}

	if (rwlock->state > 0) {
		if (--rwlock->state == 0 && rwlock->blocked_writers)
			ret = pthread_cond_signal(&rwlock->write_signal);
	} else if (rwlock->state < 0) {
		rwlock->state = 0;
#if __DARWIN_UNIX03
		rwlock->owner = (pthread_t)0;
#endif /* __DARWIN_UNIX03 */

		if (rwlock->blocked_writers)
			ret = pthread_cond_signal(&rwlock->write_signal);
		else
			ret = pthread_cond_broadcast(&rwlock->read_signal);
	} else
		ret = EINVAL;

	if (ret == 0) {
		PLOCKSTAT_RW_RELEASE(rwlock, writer);
	} else {
		PLOCKSTAT_RW_ERROR(rwlock, writer, ret);
	}

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&rwlock->lock);

	return(ret);
}

int
pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
	int			ret;
#if __DARWIN_UNIX03
	pthread_t self = pthread_self();
#endif /* __DARWIN_UNIX03 */

#if  defined(__i386__) || defined(__x86_64__)
	if ((usenew_impl != 0)) {
		return(_new_pthread_rwlock_wrlock(rwlock));
	}
#endif /* __i386__ || __x86_64__ */

	/* check for static initialization */
	if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		if ((ret = pthread_rwlock_init(rwlock, NULL)) != 0)  {
			PLOCKSTAT_RW_ERROR(rwlock, WRITE_LOCK_PLOCKSTAT, ret);
			return(ret);
		}
	}

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		PLOCKSTAT_RW_ERROR(rwlock, WRITE_LOCK_PLOCKSTAT, EINVAL);
		return(EINVAL);
	}

#if  defined(__i386__) || defined(__x86_64__)
	if (rwlock->pshared == PTHREAD_PROCESS_SHARED) {
		ret = _new_pthread_rwlock_wrlock(rwlock);
		return(ret);
	}
#endif /* __i386__ || __x86_64__ */


	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0) {
		PLOCKSTAT_RW_ERROR(rwlock, WRITE_LOCK_PLOCKSTAT, ret);
		return(ret);
  	}

#if __DARWIN_UNIX03
	if ((rwlock->state < 0) && (rwlock->owner == self)) {
		pthread_mutex_unlock(&rwlock->lock);
		PLOCKSTAT_RW_ERROR(rwlock, WRITE_LOCK_PLOCKSTAT, EDEADLK);
		return(EDEADLK);
	}
#endif /* __DARWIN_UNIX03 */
	while (rwlock->state != 0) {
		++rwlock->blocked_writers;

		PLOCKSTAT_RW_BLOCK(rwlock, WRITE_LOCK_PLOCKSTAT);
		ret = pthread_cond_wait(&rwlock->write_signal, &rwlock->lock);

		if (ret != 0) {
			--rwlock->blocked_writers;
			pthread_mutex_unlock(&rwlock->lock);
			PLOCKSTAT_RW_BLOCKED(rwlock, WRITE_LOCK_PLOCKSTAT, BLOCK_FAIL_PLOCKSTAT);
			PLOCKSTAT_RW_ERROR(rwlock, WRITE_LOCK_PLOCKSTAT, ret);
			return(ret);
		}

		PLOCKSTAT_RW_BLOCKED(rwlock, WRITE_LOCK_PLOCKSTAT, BLOCK_SUCCESS_PLOCKSTAT);

		--rwlock->blocked_writers;
	}

	/* indicate we are locked for writing */
	rwlock->state = -1;
#if __DARWIN_UNIX03
	rwlock->owner = self;
#endif /* __DARWIN_UNIX03 */
	PLOCKSTAT_RW_ACQUIRE(rwlock, WRITE_LOCK_PLOCKSTAT);

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&rwlock->lock);

	return(ret);
}

