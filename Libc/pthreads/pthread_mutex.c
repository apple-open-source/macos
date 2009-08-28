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

#include "pthread_internals.h"

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
extern int __unix_conforming;
int _pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);

#if  defined(__i386__) || defined(__x86_64__)
#define USE_COMPAGE 1

#include <machine/cpu_capabilities.h>

extern int _commpage_pthread_mutex_lock(uint32_t * lvalp, int flags, uint64_t mtid, uint32_t mask, uint64_t * tidp, int *sysret);

int _new_pthread_mutex_destroy(pthread_mutex_t *mutex);
int _new_pthread_mutex_destroy_locked(pthread_mutex_t *mutex);
int _new_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int _new_pthread_mutex_lock(pthread_mutex_t *omutex);
int _new_pthread_mutex_trylock(pthread_mutex_t *omutex);
int _new_pthread_mutex_unlock(pthread_mutex_t *omutex);

#if defined(__LP64__)
#define MUTEX_GETSEQ_ADDR(mutex, lseqaddr, useqaddr) \
{ \
		if (mutex->mtxopts.options.misalign != 0) { \
			lseqaddr = &mutex->m_seq[0]; \
			useqaddr = &mutex->m_seq[1]; \
		 } else { \
			lseqaddr = &mutex->m_seq[1]; \
			useqaddr = &mutex->m_seq[2]; \
		} \
}
#else /* __LP64__ */
#define MUTEX_GETSEQ_ADDR(mutex, lseqaddr, useqaddr) \
{ \
		if (mutex->mtxopts.options.misalign != 0) { \
			lseqaddr = &mutex->m_seq[1]; \
			useqaddr = &mutex->m_seq[2]; \
		 }else { \
			lseqaddr = &mutex->m_seq[0]; \
			useqaddr = &mutex->m_seq[1]; \
		} \
}
#endif /* __LP64__ */

#define _KSYN_TRACE_ 0

#if _KSYN_TRACE_
/* The Function qualifiers  */
#define DBG_FUNC_START          1
#define DBG_FUNC_END            2
#define DBG_FUNC_NONE           0

int __kdebug_trace(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

#define _KSYN_TRACE_UM_LOCK     0x9000060
#define _KSYN_TRACE_UM_UNLOCK   0x9000064
#define _KSYN_TRACE_UM_MHOLD    0x9000068
#define _KSYN_TRACE_UM_MDROP    0x900006c
#define _KSYN_TRACE_UM_MUBITS    0x900007c

#endif /* _KSYN_TRACE_ */

#endif /* __i386__ || __x86_64__ */

#ifndef BUILDING_VARIANT /* [ */

#define BLOCK_FAIL_PLOCKSTAT    0
#define BLOCK_SUCCESS_PLOCKSTAT 1

#ifdef PR_5243343
/* 5243343 - temporary hack to detect if we are running the conformance test */
extern int PR_5243343_flag;
#endif /* PR_5243343 */

/* This function is never called and exists to provide never-fired dtrace
 * probes so that user d scripts don't get errors.
 */
__private_extern__ void _plockstat_never_fired(void) 
{
	PLOCKSTAT_MUTEX_SPIN(NULL);
	PLOCKSTAT_MUTEX_SPUN(NULL, 0, 0);
}

/*
 * Destroy a mutex variable.
 */
int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	int res;

	LOCK(mutex->lock);
	if (mutex->sig == _PTHREAD_MUTEX_SIG)
	{

#if  defined(__i386__) || defined(__x86_64__)
		if(mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED){

			res = _new_pthread_mutex_destroy_locked(mutex);
			UNLOCK(mutex->lock);
			return(res);
		}
#endif /* __i386__ || __x86_64__ */

		if (mutex->owner == (pthread_t)NULL &&
		    mutex->busy == (pthread_cond_t *)NULL)
		{
			mutex->sig = _PTHREAD_NO_SIG;
			res = 0;
		}
		else
			res = EBUSY;
	} else 
		res = EINVAL;
	UNLOCK(mutex->lock);
	return (res);
}

/*
 * Initialize a mutex variable, possibly with additional attributes.
 */
int
_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	if (attr)
	{
		if (attr->sig != _PTHREAD_MUTEX_ATTR_SIG)
			return (EINVAL);
#if  defined(__i386__) || defined(__x86_64__)
		if (attr->pshared == PTHREAD_PROCESS_SHARED) {
			return(_new_pthread_mutex_init(mutex, attr));
		} else 
#endif /* __i386__ || __x86_64__ */
		{
			mutex->prioceiling = attr->prioceiling;
			mutex->mtxopts.options.protocol = attr->protocol;
			mutex->mtxopts.options.policy = attr->policy;
			mutex->mtxopts.options.type = attr->type;
			mutex->mtxopts.options.pshared = attr->pshared;
		}
	} else {
		mutex->prioceiling = _PTHREAD_DEFAULT_PRIOCEILING;
		mutex->mtxopts.options.protocol = _PTHREAD_DEFAULT_PROTOCOL;
		mutex->mtxopts.options.policy = _PTHREAD_MUTEX_POLICY_FAIRSHARE;
		mutex->mtxopts.options.type = PTHREAD_MUTEX_DEFAULT;
		mutex->mtxopts.options.pshared = _PTHREAD_DEFAULT_PSHARED;
	}
	mutex->mtxopts.options.lock_count = 0;
	mutex->owner = (pthread_t)NULL;
	mutex->next = (pthread_mutex_t *)NULL;
	mutex->prev = (pthread_mutex_t *)NULL;
	mutex->busy = (pthread_cond_t *)NULL;
	mutex->waiters = 0;
	mutex->sem = SEMAPHORE_NULL;
	mutex->order = SEMAPHORE_NULL;
	mutex->prioceiling = 0;
	mutex->sig = _PTHREAD_MUTEX_SIG;
	return (0);
}

/*
 * Initialize a mutex variable, possibly with additional attributes.
 * Public interface - so don't trust the lock - initialize it first.
 */
int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
#if 0
	/* conformance tests depend on not having this behavior */
	/* The test for this behavior is optional */
	if (mutex->sig == _PTHREAD_MUTEX_SIG)
		return EBUSY;
#endif
	LOCK_INIT(mutex->lock);
	return (_pthread_mutex_init(mutex, attr));
}

/*
 * Lock a mutex.
 * TODO: Priority inheritance stuff
 */
int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
	kern_return_t kern_res;
	pthread_t self;
	int sig = mutex->sig; 

	/* To provide backwards compat for apps using mutex incorrectly */
	if ((sig != _PTHREAD_MUTEX_SIG) && (sig != _PTHREAD_MUTEX_SIG_init)) {
		PLOCKSTAT_MUTEX_ERROR(mutex, EINVAL);
		return(EINVAL);
	}
		
	LOCK(mutex->lock);
	if (mutex->sig != _PTHREAD_MUTEX_SIG)
	{
		if (mutex->sig != _PTHREAD_MUTEX_SIG_init)
		{
				UNLOCK(mutex->lock);
				PLOCKSTAT_MUTEX_ERROR(mutex, EINVAL);
				return (EINVAL);
		}
		_pthread_mutex_init(mutex, NULL);
		self = _PTHREAD_MUTEX_OWNER_SELF;
	} 
#if  defined(__i386__) || defined(__x86_64__)
	else if(mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED){
			UNLOCK(mutex->lock);
			return(_new_pthread_mutex_lock(mutex));
	}
#endif /* __i386__ || __x86_64__ */
	else if (mutex->mtxopts.options.type != PTHREAD_MUTEX_NORMAL)
	{
		self = pthread_self();
		if (mutex->owner == self)
		{
			int res;

			if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE)
			{
				if (mutex->mtxopts.options.lock_count < USHRT_MAX)
				{
					mutex->mtxopts.options.lock_count++;
					PLOCKSTAT_MUTEX_ACQUIRE(mutex, 1, 0);
					res = 0;
				} else {
					res = EAGAIN;
					PLOCKSTAT_MUTEX_ERROR(mutex, res);
				}
			} else	{ /* PTHREAD_MUTEX_ERRORCHECK */
				res = EDEADLK;
				PLOCKSTAT_MUTEX_ERROR(mutex, res);
			}
			UNLOCK(mutex->lock);
			return (res);
		}
	} else 
		self = _PTHREAD_MUTEX_OWNER_SELF;

	if (mutex->owner != (pthread_t)NULL) {
		if (mutex->waiters || mutex->owner != _PTHREAD_MUTEX_OWNER_SWITCHING)
		{
			semaphore_t sem, order;

			if (++mutex->waiters == 1)
			{
				mutex->sem = sem = new_sem_from_pool();
				mutex->order = order = new_sem_from_pool();
			}
			else
			{
				sem = mutex->sem;
				order = mutex->order;
				do {
					PTHREAD_MACH_CALL(semaphore_wait(order), kern_res);
				} while (kern_res == KERN_ABORTED);
			} 
			UNLOCK(mutex->lock);

			PLOCKSTAT_MUTEX_BLOCK(mutex);
			PTHREAD_MACH_CALL(semaphore_wait_signal(sem, order), kern_res);
			while (kern_res == KERN_ABORTED)
			{
				PTHREAD_MACH_CALL(semaphore_wait(sem), kern_res);
			} 

			PLOCKSTAT_MUTEX_BLOCKED(mutex, BLOCK_SUCCESS_PLOCKSTAT);

			LOCK(mutex->lock);
			if (--mutex->waiters == 0)
			{
				PTHREAD_MACH_CALL(semaphore_wait(order), kern_res);
				mutex->sem = mutex->order = SEMAPHORE_NULL;
				restore_sem_to_pool(order);
				restore_sem_to_pool(sem);
			}
		} 
		else if (mutex->owner == _PTHREAD_MUTEX_OWNER_SWITCHING)
		{
			semaphore_t sem = mutex->sem;
			do {
				PTHREAD_MACH_CALL(semaphore_wait(sem), kern_res);
			} while (kern_res == KERN_ABORTED);
			mutex->sem = SEMAPHORE_NULL;
			restore_sem_to_pool(sem);
		}
	}

	mutex->mtxopts.options.lock_count = 1;
	mutex->owner = self;
	UNLOCK(mutex->lock);
	PLOCKSTAT_MUTEX_ACQUIRE(mutex, 0, 0);
	return (0);
}

/*
 * Attempt to lock a mutex, but don't block if this isn't possible.
 */
int
pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	kern_return_t kern_res;
	pthread_t self;
	
	LOCK(mutex->lock);
	if (mutex->sig != _PTHREAD_MUTEX_SIG)
	{
		if (mutex->sig != _PTHREAD_MUTEX_SIG_init)
		{
				PLOCKSTAT_MUTEX_ERROR(mutex, EINVAL);
				UNLOCK(mutex->lock);
				return (EINVAL);
		}
		_pthread_mutex_init(mutex, NULL);
		self = _PTHREAD_MUTEX_OWNER_SELF;
	}
#if  defined(__i386__) || defined(__x86_64__)
		else if(mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED){
			UNLOCK(mutex->lock);
			return(_new_pthread_mutex_trylock(mutex));
		}
#endif /* __i386__ || __x86_64__ */
	else if (mutex->mtxopts.options.type != PTHREAD_MUTEX_NORMAL)
	{
		self = pthread_self();
		if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE)
		{
			if (mutex->owner == self)
			{
				int res;

				if (mutex->mtxopts.options.lock_count < USHRT_MAX)
				{
					mutex->mtxopts.options.lock_count++;
					PLOCKSTAT_MUTEX_ACQUIRE(mutex, 1, 0);
					res = 0;
				} else {
					res = EAGAIN;
					PLOCKSTAT_MUTEX_ERROR(mutex, res);
				}
				UNLOCK(mutex->lock);
				return (res);
			}
		}
	} else
		self = _PTHREAD_MUTEX_OWNER_SELF;

	if (mutex->owner != (pthread_t)NULL)
	{
		if (mutex->waiters || mutex->owner != _PTHREAD_MUTEX_OWNER_SWITCHING)
		{
			PLOCKSTAT_MUTEX_ERROR(mutex, EBUSY);
			UNLOCK(mutex->lock);
			return (EBUSY);
		}
		else if (mutex->owner == _PTHREAD_MUTEX_OWNER_SWITCHING)
		{
			semaphore_t sem = mutex->sem;

			do {
				PTHREAD_MACH_CALL(semaphore_wait(sem), kern_res);
			} while (kern_res == KERN_ABORTED);
			restore_sem_to_pool(sem);
			mutex->sem = SEMAPHORE_NULL;
		}
	}

	mutex->mtxopts.options.lock_count = 1;
	mutex->owner = self;
	UNLOCK(mutex->lock);
	PLOCKSTAT_MUTEX_ACQUIRE(mutex, 0, 0);
	return (0);
}

/*
 * Unlock a mutex.
 * TODO: Priority inheritance stuff
 */
int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	kern_return_t kern_res;
	int waiters;
	int sig = mutex->sig; 

	
	/* To provide backwards compat for apps using mutex incorrectly */
	
	if ((sig != _PTHREAD_MUTEX_SIG) && (sig != _PTHREAD_MUTEX_SIG_init)) {
		PLOCKSTAT_MUTEX_ERROR(mutex, EINVAL);
		return(EINVAL);
	}
	LOCK(mutex->lock);
	if (mutex->sig != _PTHREAD_MUTEX_SIG)
	{
		if (mutex->sig != _PTHREAD_MUTEX_SIG_init)
		{
				PLOCKSTAT_MUTEX_ERROR(mutex, EINVAL);
				UNLOCK(mutex->lock);
				return (EINVAL);
		}
		_pthread_mutex_init(mutex, NULL);
	}
#if  defined(__i386__) || defined(__x86_64__)
		else if(mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED){
			UNLOCK(mutex->lock);
			return(_new_pthread_mutex_unlock(mutex));
		}
#endif /* __i386__ || __x86_64__ */
	else if (mutex->mtxopts.options.type != PTHREAD_MUTEX_NORMAL)
	{
		pthread_t self = pthread_self();
		if (mutex->owner != self)
		{
			PLOCKSTAT_MUTEX_ERROR(mutex, EPERM);
			UNLOCK(mutex->lock);
			return EPERM;
		} else if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE &&
		    --mutex->mtxopts.options.lock_count)
		{
			PLOCKSTAT_MUTEX_RELEASE(mutex, 1);
			UNLOCK(mutex->lock);
			return(0);
		}
	}

	mutex->mtxopts.options.lock_count = 0;

	waiters = mutex->waiters;
	if (waiters)
	{
		mutex->owner = _PTHREAD_MUTEX_OWNER_SWITCHING;
		PLOCKSTAT_MUTEX_RELEASE(mutex, 0);
		UNLOCK(mutex->lock);
		PTHREAD_MACH_CALL(semaphore_signal(mutex->sem), kern_res);
	}
	else
	{
		mutex->owner = (pthread_t)NULL;
		PLOCKSTAT_MUTEX_RELEASE(mutex, 0);
		UNLOCK(mutex->lock);
	}
	return (0);
}

/*
 * Fetch the priority ceiling value from a mutex variable.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutex_getprioceiling(const pthread_mutex_t *mutex,
                             int *prioceiling)
{
	int res;

	LOCK(mutex->lock);
        if (mutex->sig == _PTHREAD_MUTEX_SIG)
        {
                *prioceiling = mutex->prioceiling;
                res = 0;
        } else
                res = EINVAL; /* Not an initialized 'attribute' structure */
	UNLOCK(mutex->lock);
	return (res);
}

/*
 * Set the priority ceiling for a mutex.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutex_setprioceiling(pthread_mutex_t *mutex,
                             int prioceiling,
                             int *old_prioceiling)
{
	int res;

	LOCK(mutex->lock);
        if (mutex->sig == _PTHREAD_MUTEX_SIG)
        {
                if ((prioceiling >= -999) ||
                    (prioceiling <= 999))
                {
                        *old_prioceiling = mutex->prioceiling;
                        mutex->prioceiling = prioceiling;
                        res = 0;
                } else
                        res = EINVAL; /* Invalid parameter */
        } else
                res = EINVAL; /* Not an initialized 'attribute' structure */
	UNLOCK(mutex->lock);
	return (res);
}

/*
 * Get the priority ceiling value from a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr,
                                 int *prioceiling)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                *prioceiling = attr->prioceiling;
                return (0);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

/*
 * Get the mutex 'protocol' value from a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr,
                              int *protocol)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                *protocol = attr->protocol;
                return (0);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}
/*
 * Get the mutex 'type' value from a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_gettype(const pthread_mutexattr_t *attr,
                              int *type)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                *type = attr->type;
                return (0);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

/*
 *
 */
int
pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                *pshared = (int)attr->pshared;
                return (0);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}

/*
 * Initialize a mutex attribute structure to system defaults.
 */
int
pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
        attr->prioceiling = _PTHREAD_DEFAULT_PRIOCEILING;
        attr->protocol = _PTHREAD_DEFAULT_PROTOCOL;
        attr->policy = _PTHREAD_MUTEX_POLICY_FAIRSHARE;
        attr->type = PTHREAD_MUTEX_DEFAULT;
        attr->sig = _PTHREAD_MUTEX_ATTR_SIG;
        attr->pshared = _PTHREAD_DEFAULT_PSHARED;
        return (0);
}

/*
 * Set the priority ceiling value in a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr,
                                 int prioceiling)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                if ((prioceiling >= -999) ||
                    (prioceiling <= 999))
                {
                        attr->prioceiling = prioceiling;
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

/*
 * Set the mutex 'protocol' value in a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr,
                              int protocol)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                if ((protocol == PTHREAD_PRIO_NONE) ||
                    (protocol == PTHREAD_PRIO_INHERIT) ||
                    (protocol == PTHREAD_PRIO_PROTECT))
                {
                        attr->protocol = protocol;
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

#ifdef NOTYET
int
pthread_mutexattr_setpolicy_np(pthread_mutexattr_t *attr,
                              int policy)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                if ((policy == _PTHREAD_MUTEX_POLICY_FAIRSHARE) ||
                    (policy == _PTHREAD_MUTEX_POLICY_FIRSTFIT) ||
                    (policy == _PTHREAD_MUTEX_POLICY_REALTIME) ||
                    (policy == _PTHREAD_MUTEX_POLICY_ADAPTIVE) ||
                    (policy == _PTHREAD_MUTEX_POLICY_PRIPROTECT) ||
                    (policy == _PTHREAD_MUTEX_POLICY_PRIINHERIT))
                {
                        attr->policy = policy;
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
#endif /* NOTYET */

/*
 * Set the mutex 'type' value in a mutex attribute structure.
 * Note: written as a 'helper' function to hide implementation details.
 */
int
pthread_mutexattr_settype(pthread_mutexattr_t *attr,
                              int type)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                if ((type == PTHREAD_MUTEX_NORMAL) ||
                    (type == PTHREAD_MUTEX_ERRORCHECK) ||
                    (type == PTHREAD_MUTEX_RECURSIVE) ||
                    (type == PTHREAD_MUTEX_DEFAULT))
                {
                        attr->type = type;
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


int mutex_try_lock(int *x) {
        return _spin_lock_try((pthread_lock_t *)x);
}

void mutex_wait_lock(int *x) {
        for (;;) {
                if( _spin_lock_try((pthread_lock_t *)x)) {
                        return;
                }
                swtch_pri(0);
        }
}

void 
cthread_yield(void) 
{
        sched_yield();
}

void 
pthread_yield_np (void) 
{
        sched_yield();
}


/*
 * Temp: till pshared is fixed correctly
 */
int
pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;
#endif /* __DARWIN_UNIX03 */

        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
#if __DARWIN_UNIX03
#ifdef PR_5243343
                if (( pshared == PTHREAD_PROCESS_PRIVATE) || (pshared == PTHREAD_PROCESS_SHARED && PR_5243343_flag))
#else /* !PR_5243343 */
                if (( pshared == PTHREAD_PROCESS_PRIVATE) || (pshared == PTHREAD_PROCESS_SHARED))
#endif /* PR_5243343 */
#else /* __DARWIN_UNIX03 */
                if ( pshared == PTHREAD_PROCESS_PRIVATE)
#endif /* __DARWIN_UNIX03 */
			  {
                         attr->pshared = pshared; 
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

#if  defined(__i386__) || defined(__x86_64__)

/* 
 * Acquire lock seq for condition var  signalling/broadcast
 */
__private_extern__ void
__mtx_holdlock(npthread_mutex_t * mutex, uint32_t diff, uint32_t * flagp, uint32_t **pmtxp, uint32_t * mgenp, uint32_t * ugenp)
{
	uint32_t mgen, ugen, ngen;
	int hold = 0;
	int firstfit = (mutex->mtxopts.options.policy == _PTHREAD_MUTEX_POLICY_FIRSTFIT);
	uint32_t * lseqaddr;
	uint32_t * useqaddr;
	

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MHOLD | DBG_FUNC_START, (uint32_t)mutex, diff, firstfit, 0, 0);
#endif
	if (mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED) {
		/* no holds for shared mutexes */
		hold = 2;
		mgen = 0;
		ugen = 0;
		MUTEX_GETSEQ_ADDR(mutex, lseqaddr, useqaddr);
		goto out;
	} else {
		lseqaddr = mutex->m_lseqaddr;
		useqaddr = mutex->m_useqaddr;
	}

retry:
	mgen = *lseqaddr;
	ugen = *useqaddr;
	/* no need to do extra wrap */
	ngen = mgen + (PTHRW_INC * diff);
	hold = 0;

	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MHOLD | DBG_FUNC_NONE, (uint32_t)mutex, 0, mgen, ngen, 0);
#endif
	/* can we acquire the lock ? */
	if ((mgen & PTHRW_EBIT) == 0) { 
		/* if it is firstfit, no need to hold till the cvar returns */
		if (firstfit == 0) {
			ngen |= PTHRW_EBIT;
			hold = 1;
		}
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MHOLD | DBG_FUNC_NONE, (uint32_t)mutex, 1, mgen, ngen, 0);
#endif
	}

	/* update lockseq */
	if (OSAtomicCompareAndSwap32(mgen, ngen, (volatile int32_t *)lseqaddr) != TRUE)
		goto retry;
	if (hold == 1) {
		mutex->m_tid = PTHREAD_MTX_TID_SWITCHING ;
	}
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MHOLD | DBG_FUNC_NONE, (uint32_t)mutex, 2, hold, 0, 0);
#endif
	
out:
	if (flagp != NULL) {
		if (hold == 1) {
			*flagp = (mutex->mtxopts.value | _PTHREAD_MTX_OPT_HOLD);
		 } else if (hold == 2) {
			*flagp = (mutex->mtxopts.value | _PTHREAD_MTX_OPT_NOHOLD);
		 } else  {
			*flagp = mutex->mtxopts.value;
		}
	}
	if (mgenp != NULL)
		*mgenp = mgen;
	if (ugenp != NULL)
		*ugenp = ugen;
	if (pmtxp != NULL)
		*pmtxp = lseqaddr;
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MHOLD | DBG_FUNC_END, (uint32_t)mutex, hold, 0, 0, 0);
#endif
}


/*
 * Drop the mutex unlock references(from cond wait or mutex_unlock().
 * mgenp and ugenp valid only if notifyp is set 
 * 
 */
__private_extern__ int
__mtx_droplock(npthread_mutex_t * mutex, int count, uint32_t * flagp, uint32_t ** pmtxp, uint32_t * mgenp, uint32_t * ugenp, uint32_t *notifyp)
{
	int oldval, newval, lockval, unlockval;
	uint64_t oldtid;
	pthread_t self = pthread_self();
	uint32_t notify = 0;
	uint64_t oldval64, newval64;
	uint32_t * lseqaddr;
	uint32_t * useqaddr;
	int firstfit = (mutex->mtxopts.options.policy == _PTHREAD_MUTEX_POLICY_FIRSTFIT);

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MDROP | DBG_FUNC_START, (uint32_t)mutex, count, 0, 0, 0);
#endif
	if (mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED) {
		MUTEX_GETSEQ_ADDR(mutex, lseqaddr, useqaddr);
	} else {
		lseqaddr = mutex->m_lseqaddr;
		useqaddr = mutex->m_useqaddr;
	}
	
	if (flagp != NULL)
		*flagp = mutex->mtxopts.value;
	
	if (firstfit != 0) 
		notify |= 0x80000000;
	if (mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED)
		notify |= 0x40000000;
	
	if (mutex->mtxopts.options.type != PTHREAD_MUTEX_NORMAL)
	{
		if (mutex->m_tid != (uint64_t)((uintptr_t)self))
		{
			PLOCKSTAT_MUTEX_ERROR((pthread_mutex_t *)mutex, EPERM);
			return(EPERM);
		} else if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE &&
				   --mutex->mtxopts.options.lock_count)
		{
			PLOCKSTAT_MUTEX_RELEASE((pthread_mutex_t *)mutex, 1);
			goto out;
		}
	}
	
	
	if (mutex->m_tid != (uint64_t)((uintptr_t)self)) 
		return(EINVAL);
	
	
ml0:
	oldval = *useqaddr;
	unlockval =  oldval + (PTHRW_INC * count);
	lockval = *lseqaddr;


#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MDROP | DBG_FUNC_NONE, (uint32_t)mutex, 10, lockval, oldval, 0);
#endif
#if 1
	if (lockval == oldval) 
		LIBC_ABORT("same unlock and lockseq \n");
#endif
	
	if ((lockval & PTHRW_COUNT_MASK) == unlockval) {
		oldtid = mutex->m_tid;

		mutex->m_tid = 0;

		oldval64 = (((uint64_t)oldval) << 32);
		oldval64 |= lockval;

		newval64 = 0;

		if (OSAtomicCompareAndSwap64(oldval64, newval64, (volatile int64_t *)lseqaddr) == TRUE) {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MDROP | DBG_FUNC_NONE, (uint32_t)mutex, 1, 0, 0, 0);
#endif
			goto out;
		} else {
			mutex->m_tid = oldtid;
			/* fall thru for kernel call */
			goto ml0;
		}
	} 

	if (firstfit != 0) {
		/* reset ebit along with unlock */
		newval = (lockval & ~PTHRW_EBIT);

		lockval = newval;
		oldval64 = (((uint64_t)oldval) << 32);
		oldval64 |= lockval;

		newval64 = (((uint64_t)unlockval) << 32);
		newval64 |= newval;

		if (OSAtomicCompareAndSwap64(oldval64, newval64, (volatile int64_t *)lseqaddr) != TRUE) {
			goto ml0;
		}
		lockval = newval;	
	} else  {
		/* fairshare , just update and go to kernel */
		if (OSAtomicCompareAndSwap32(oldval, unlockval, (volatile int32_t *)useqaddr) != TRUE)
		goto ml0;

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MDROP | DBG_FUNC_NONE, (uint32_t)mutex, 2, oldval, unlockval, 0);
#endif
	}

	notify |= 1;

	if (notifyp != 0) {
		if (mgenp != NULL)
			*mgenp = lockval;		
		if (ugenp != NULL)
			*ugenp = unlockval;		
		if (pmtxp != NULL)
			*pmtxp = lseqaddr;
		*notifyp = notify;
	}
out:
	if (notifyp != 0) {
		*notifyp = notify;
	}
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MDROP | DBG_FUNC_END, (uint32_t)mutex, 0, 0, 0, 0);
#endif
	return(0);
}

int
__mtx_updatebits(npthread_mutex_t *mutex, uint32_t oupdateval, int firstfit, int fromcond)
{
        uint32_t lgenval, newval, bits;
	int isebit = 0;
	uint32_t updateval = oupdateval;
	pthread_mutex_t * omutex = (pthread_mutex_t *)mutex;
	uint32_t * lseqaddr;
	uint32_t * useqaddr;

	if (mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED) {
		MUTEX_GETSEQ_ADDR(mutex, lseqaddr, useqaddr);
	} else {
		lseqaddr = mutex->m_lseqaddr;
		useqaddr = mutex->m_useqaddr;
	}
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MUBITS | DBG_FUNC_START, (uint32_t)mutex, oupdateval, firstfit, fromcond, 0);
#endif

retry:
        lgenval = *lseqaddr;
        bits = updateval & PTHRW_BIT_MASK;

        if (lgenval == updateval) 
		goto out;

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MUBITS | DBG_FUNC_NONE, (uint32_t)mutex, 1, lgenval, updateval, 0);
#endif
        if ((lgenval & PTHRW_BIT_MASK) == bits)
                goto out;

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MUBITS | DBG_FUNC_NONE, (uint32_t)mutex, 2, lgenval, bits, 0);
#endif
	/* firsfit might not have EBIT */
	if (firstfit != 0) {
		lgenval  &= ~PTHRW_EBIT;	/* see whether EBIT is set */
		if ((lgenval & PTHRW_EBIT) != 0)
			isebit = 1;
	}

        if ((lgenval & PTHRW_COUNT_MASK) == (updateval & PTHRW_COUNT_MASK)) {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MUBITS | DBG_FUNC_NONE, (uint32_t)mutex, 3, lgenval, updateval, 0);
#endif
		updateval |= PTHRW_EBIT;  /* just in case.. */
                if (OSAtomicCompareAndSwap32(lgenval, updateval, (volatile int32_t *)lseqaddr) != TRUE) {
			if (firstfit == 0)
                        	goto retry;
			goto handleffit;
		}
		/* update succesfully */
		goto out;
        }


        if (((lgenval & PTHRW_WBIT) != 0) && ((updateval & PTHRW_WBIT) == 0)) {
                newval = lgenval | (bits | PTHRW_WBIT | PTHRW_EBIT);
         } else {
                newval = lgenval | (bits | PTHRW_EBIT);
	}

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MUBITS | DBG_FUNC_NONE, (uint32_t)mutex, 4, lgenval, newval, 0);
#endif
        if (OSAtomicCompareAndSwap32(lgenval, newval, (volatile int32_t *)lseqaddr) != TRUE)  {
			if (firstfit == 0)
                        	goto retry;
			goto handleffit;
	}
out:
	/* succesful bits updation */
	mutex->m_tid = (uint64_t)((uintptr_t)pthread_self());
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_MUBITS | DBG_FUNC_END, (uint32_t)mutex, 0, 0, 0, 0);
#endif
	return(0);

handleffit:
	/* firstfit failure */
	newval = *lseqaddr;
	if ((newval & PTHRW_EBIT) == 0)
		goto retry;
	if (((lgenval & PTHRW_COUNT_MASK) == (newval & PTHRW_COUNT_MASK)) && (isebit == 1)) {
		if (fromcond == 0)
			return(1);
		else {
			/* called from condition variable code  block again */
ml1:
#if  USE_COMPAGE /* [ */
			updateval = __psynch_mutexwait((pthread_mutex_t *)lseqaddr, newval | PTHRW_RETRYBIT, *useqaddr, (uint64_t)0,
							   mutex->mtxopts.value);
#else /* USECOMPAGE ][ */
			updateval = __psynch_mutexwait(omutex, newval | PTHRW_RETRYBIT, *useqaddr, (uint64_t)0,
#endif /* USE_COMPAGE ] */
			if (updateval == (uint32_t)-1) {
				goto ml1;
			}

			goto retry;
		}
	}
	/* seqcount changed, retry */
	goto retry;
}

int
_new_pthread_mutex_lock(pthread_mutex_t *omutex)
{
	pthread_t self;
	npthread_mutex_t * mutex = (npthread_mutex_t *)omutex;
	int sig = mutex->sig; 
	int retval;
	uint32_t oldval, newval, uval, updateval;
	int gotlock = 0;
	int firstfit = 0;
	int retrybit = 0;
	uint32_t * lseqaddr;
	uint32_t * useqaddr;
	int updatebitsonly = 0;
#if USE_COMPAGE
	uint64_t mytid;
	int sysret = 0;
	uint32_t mask;
#else

#endif
	
	/* To provide backwards compat for apps using mutex incorrectly */
	if ((sig != _PTHREAD_MUTEX_SIG) && (sig != _PTHREAD_MUTEX_SIG_init)) {
		PLOCKSTAT_MUTEX_ERROR(omutex, EINVAL);
		return(EINVAL);
	}
	if (sig != _PTHREAD_MUTEX_SIG) {
		LOCK(mutex->lock);
		if ((sig != _PTHREAD_MUTEX_SIG) && (sig == _PTHREAD_MUTEX_SIG_init)) {
			/* static initializer, init the mutex */
			_new_pthread_mutex_init(omutex, NULL);
			self = _PTHREAD_MUTEX_OWNER_SELF;
		} else {
			UNLOCK(mutex->lock);
			PLOCKSTAT_MUTEX_ERROR(omutex, EINVAL);
			return(EINVAL);
		}
		UNLOCK(mutex->lock);
	}
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_LOCK | DBG_FUNC_START, (uint32_t)mutex, 0, 0, 0, 0);
#endif
	if (mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED) {
		MUTEX_GETSEQ_ADDR(mutex, lseqaddr, useqaddr);
	} else {
		lseqaddr = mutex->m_lseqaddr;
		useqaddr = mutex->m_useqaddr;
	}

	self = pthread_self();
	if (mutex->mtxopts.options.type != PTHREAD_MUTEX_NORMAL) {
		if (mutex->m_tid == (uint64_t)((uintptr_t)self)) {
			if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE)
			{
				if (mutex->mtxopts.options.lock_count < USHRT_MAX)
				{
					mutex->mtxopts.options.lock_count++;
					PLOCKSTAT_MUTEX_ACQUIRE(omutex, 1, 0);
					retval = 0;
				} else {
					retval = EAGAIN;
					PLOCKSTAT_MUTEX_ERROR(omutex, retval);
				}
			} else	{ /* PTHREAD_MUTEX_ERRORCHECK */
				retval = EDEADLK;
				PLOCKSTAT_MUTEX_ERROR(omutex, retval);
			}
			return (retval);
		}
	}
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_LOCK | DBG_FUNC_NONE, (uint32_t)mutex, 1, 0, 0, 0);
#endif
loop:
#if  USE_COMPAGE /* [ */

	mytid = (uint64_t)((uintptr_t)pthread_self());

ml0:
	mask = PTHRW_EBIT;
	retval = _commpage_pthread_mutex_lock(lseqaddr, mutex->mtxopts.value, mytid, mask, &mutex->m_tid, &sysret);
	if (retval == 0) {
		gotlock = 1;	
	} else if (retval == 1) {
		gotlock = 1;	
		updateval = sysret;
		/* returns 0 on succesful update */
		if (__mtx_updatebits( mutex, updateval, firstfit, 0) == 1) {
			/* could not acquire, may be locked in ffit case */
#if USE_COMPAGE
			LIBC_ABORT("comapge implementatin looping in libc \n");
#endif
			goto ml0;
		}
	} 
#if NEVERINCOMPAGE
	else if  (retval == 3) {
		cthread_set_errno_self(sysret);
		oldval = *lseqaddr;
		uval = *useqaddr;
		newval = oldval + PTHRW_INC;
		gotlock = 0;
		/* to block in the kerenl again */
	} 
#endif
	else {
		LIBC_ABORT("comapge implementatin bombed \n");
	}
		

#else /* USECOMPAGE ][ */
	oldval = *lseqaddr;
	uval = *useqaddr;
	newval = oldval + PTHRW_INC;
	
	(void)__kdebug_trace(_KSYN_TRACE_UM_LOCK | DBG_FUNC_NONE, (uint32_t)mutex, 2, oldval, uval, 0);
	
	if((oldval & PTHRW_EBIT) == 0) {
		gotlock = 1;
		newval |= PTHRW_EBIT;
	} else {
		gotlock = 0;
		newval |= PTHRW_WBIT;
	}
	
	if (OSAtomicCompareAndSwap32(oldval, newval, (volatile int32_t *)lseqaddr) == TRUE) {
		if (gotlock != 0)
			mutex->m_tid = (uint64_t)((uintptr_t)self);
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_LOCK | DBG_FUNC_NONE, (uint32_t)mutex, 2, oldval, newval, 0);
#endif
	} else 
		goto loop;
	

	retrybit = 0;
	if (gotlock == 0) {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_LOCK | DBG_FUNC_NONE, (uint32_t)mutex, 3, 0, 0, 0);
#endif
		firstfit = (mutex->mtxopts.options.policy == _PTHREAD_MUTEX_POLICY_FIRSTFIT);
ml1:
		updateval = __psynch_mutexwait(omutex, newval | retrybit, uval, (uint64_t)0,
							   mutex->mtxopts.value);
		
		if (updateval == (uint32_t)-1) {
			updatebitsonly = 0;
			goto ml1;
		}

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_LOCK | DBG_FUNC_NONE, (uint32_t)mutex, 4, updateval, 0, 0);
#endif
		/* returns 0 on succesful update */
		if (__mtx_updatebits( mutex, updateval, firstfit, 0) == 1) {
			/* could not acquire, may be locked in ffit case */
			retrybit = PTHRW_RETRYBIT;
#if USE_COMPAGE
		LIBC_ABORT("comapge implementatin looping in libc \n");

#endif
			goto  ml1;
		}
	}
#endif /* USE_COMPAGE ] */
	
	if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE)
		mutex->mtxopts.options.lock_count++;

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_LOCK | DBG_FUNC_END, (uint32_t)mutex, 0, 0, 0, 0);
#endif
	return (0);
}

/*
 * Attempt to lock a mutex, but don't block if this isn't possible.
 */
int
_new_pthread_mutex_trylock(pthread_mutex_t *omutex)
{
	npthread_mutex_t * mutex = (npthread_mutex_t *)omutex;
	int sig = mutex->sig;
	uint32_t oldval, newval;
	int error = 0;
	pthread_t self;
	uint32_t * lseqaddr;
	uint32_t * useqaddr;
	
	/* To provide backwards compat for apps using mutex incorrectly */
	if ((sig != _PTHREAD_MUTEX_SIG) && (sig != _PTHREAD_MUTEX_SIG_init)) {
		PLOCKSTAT_MUTEX_ERROR(omutex, EINVAL);
		return(EINVAL);
	}
	
	if (sig != _PTHREAD_MUTEX_SIG) {
		LOCK(mutex->lock);
		if ((sig != _PTHREAD_MUTEX_SIG) && (sig == _PTHREAD_MUTEX_SIG_init)) {
			/* static initializer, init the mutex */
			_new_pthread_mutex_init(omutex, NULL);
			self = _PTHREAD_MUTEX_OWNER_SELF;
		} else {
			UNLOCK(mutex->lock);
			PLOCKSTAT_MUTEX_ERROR(omutex, EINVAL);
			return(EINVAL);
		}
		UNLOCK(mutex->lock);
	}
	
	if (mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED) {
		MUTEX_GETSEQ_ADDR(mutex, lseqaddr, useqaddr);
	} else {
		lseqaddr = mutex->m_lseqaddr;
		useqaddr = mutex->m_useqaddr;
	}

	self = pthread_self();
	if (mutex->mtxopts.options.type != PTHREAD_MUTEX_NORMAL) {
		if (mutex->m_tid == (uint64_t)((uintptr_t)self)) {
			if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE)
			{
				if (mutex->mtxopts.options.lock_count < USHRT_MAX)
				{
					mutex->mtxopts.options.lock_count++;
					PLOCKSTAT_MUTEX_ACQUIRE(omutex, 1, 0);
					error = 0;
				} else {
					error = EAGAIN;
					PLOCKSTAT_MUTEX_ERROR(omutex, error);
				}
			} else	{ /* PTHREAD_MUTEX_ERRORCHECK */
				error = EDEADLK;
				PLOCKSTAT_MUTEX_ERROR(omutex, error);
			}
			return (error);
		}
	}
retry: 
	oldval = *lseqaddr;

	if ((oldval & PTHRW_EBIT) != 0) {
		newval = oldval | PTHRW_TRYLKBIT;
		if (OSAtomicCompareAndSwap32(oldval, newval, (volatile int32_t *)lseqaddr) == TRUE) {
			error = EBUSY;
		} else
			goto retry;
	} else {
		newval = (oldval  + PTHRW_INC)| PTHRW_EBIT;
		if ((OSAtomicCompareAndSwap32(oldval, newval, (volatile int32_t *)lseqaddr) == TRUE)) {
			mutex->m_tid  = (uint64_t)((uintptr_t)self);
			if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE)
				mutex->mtxopts.options.lock_count++;
		} else
			goto retry;
	}
	
	return(error);
}

/*
 * Unlock a mutex.
 * TODO: Priority inheritance stuff
 */
int
_new_pthread_mutex_unlock(pthread_mutex_t *omutex)
{
	npthread_mutex_t * mutex = (npthread_mutex_t *)omutex;
	int retval;
	uint32_t mtxgen, mtxugen, flags, notify;
	int sig = mutex->sig; 
	pthread_t self = pthread_self();
	uint32_t * lseqaddr;
	uint32_t * useqaddr;
	
	/* To provide backwards compat for apps using mutex incorrectly */
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_UNLOCK | DBG_FUNC_START, (uint32_t)mutex, 0, 0, 0, 0);
#endif
	if ((sig != _PTHREAD_MUTEX_SIG) && (sig != _PTHREAD_MUTEX_SIG_init)) {
		PLOCKSTAT_MUTEX_ERROR(omutex, EINVAL);
		return(EINVAL);
	}
	if (sig != _PTHREAD_MUTEX_SIG) {
		LOCK(mutex->lock);
		if ((sig != _PTHREAD_MUTEX_SIG) && (sig == _PTHREAD_MUTEX_SIG_init)) {
			/* static initializer, init the mutex */
			_new_pthread_mutex_init(omutex, NULL);
			self = _PTHREAD_MUTEX_OWNER_SELF;
		} else {
			UNLOCK(mutex->lock);
			PLOCKSTAT_MUTEX_ERROR(omutex, EINVAL);
			return(EINVAL);
		}
		UNLOCK(mutex->lock);
	}
	
	if (mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED) {
		MUTEX_GETSEQ_ADDR(mutex, lseqaddr, useqaddr);
	} else {
		lseqaddr = mutex->m_lseqaddr;
		useqaddr = mutex->m_useqaddr;
	}
	notify = 0;
	retval = __mtx_droplock(mutex, 1, &flags, NULL, &mtxgen, &mtxugen, &notify);
	if (retval != 0)
		return(retval);
	
	if ((notify & 1) != 0) {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_UNLOCK | DBG_FUNC_NONE, (uint32_t)mutex, 1, 0, 0, 0);
#endif
#if  USE_COMPAGE /* [ */
		if ( __psynch_mutexdrop((pthread_mutex_t *)lseqaddr, mtxgen, mtxugen, (uint64_t)0, flags)== (uint32_t)-1) 
#else /* USECOMPAGE ][ */
		if ( __psynch_mutexdrop(omutex, mtxgen, mtxugen, (uint64_t)0, flags)== (uint32_t)-1) 
#endif /* USE_COMPAGE ] */
		{
			if (errno == EINTR)
				return(0);
			else
				return(errno);
		}
	}
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_UNLOCK | DBG_FUNC_END, (uint32_t)mutex, 0, 0, 0, 0);
#endif
	return(0);
}


/*
 * Initialize a mutex variable, possibly with additional attributes.
 */
int
_new_pthread_mutex_init(pthread_mutex_t *omutex, const pthread_mutexattr_t *attr)
{
	npthread_mutex_t * mutex = (npthread_mutex_t *)omutex;
		
	if (attr)
	{
		if (attr->sig != _PTHREAD_MUTEX_ATTR_SIG)
			return (EINVAL);
		mutex->prioceiling = attr->prioceiling;
		mutex->mtxopts.options.protocol = attr->protocol;
		mutex->mtxopts.options.policy = attr->policy;
		mutex->mtxopts.options.type = attr->type;
		mutex->mtxopts.options.pshared = attr->pshared;
	} else {
		mutex->prioceiling = _PTHREAD_DEFAULT_PRIOCEILING;
		mutex->mtxopts.options.protocol = _PTHREAD_DEFAULT_PROTOCOL;
		mutex->mtxopts.options.policy = _PTHREAD_MUTEX_POLICY_FAIRSHARE;
		mutex->mtxopts.options.type = PTHREAD_MUTEX_DEFAULT;
		mutex->mtxopts.options.pshared = _PTHREAD_DEFAULT_PSHARED;
	}
	
	mutex->mtxopts.options.lock_count = 0;
	/* address 8byte aligned? */
	if (((uintptr_t)mutex & 0x07) != 0) {
		/* 4byte alinged */
		mutex->mtxopts.options.misalign = 1;
#if defined(__LP64__)
		mutex->m_lseqaddr = &mutex->m_seq[0];
		mutex->m_useqaddr = &mutex->m_seq[1];
#else /* __LP64__ */
		mutex->m_lseqaddr = &mutex->m_seq[1];
		mutex->m_useqaddr = &mutex->m_seq[2];
#endif /* __LP64__ */
	} else {
		/* 8byte alinged */
		mutex->mtxopts.options.misalign = 0;
#if defined(__LP64__)
		mutex->m_lseqaddr = &mutex->m_seq[1];
		mutex->m_useqaddr = &mutex->m_seq[2];
#else /* __LP64__ */
		mutex->m_lseqaddr = &mutex->m_seq[0];
		mutex->m_useqaddr = &mutex->m_seq[1];
#endif /* __LP64__ */
	}
	mutex->m_tid = 0;
	mutex->m_seq[0] = 0;
	mutex->m_seq[1] = 0;
	mutex->m_seq[2] = 0;
	mutex->prioceiling = 0;
	mutex->priority = 0;
	mutex->sig = _PTHREAD_MUTEX_SIG;
	return (0);
}



/*
 * Destroy a mutex variable.
 */
int
_new_pthread_mutex_destroy(pthread_mutex_t *omutex)
{
	int res;
	npthread_mutex_t * mutex = (npthread_mutex_t *)omutex;

	LOCK(mutex->lock);
	res = _new_pthread_mutex_destroy_locked(omutex);
	UNLOCK(mutex->lock);
	
	return(res);	
}


int
_new_pthread_mutex_destroy_locked(pthread_mutex_t *omutex)
{
	int res;
	npthread_mutex_t * mutex = (npthread_mutex_t *)omutex;
	uint32_t lgenval;
	uint32_t * lseqaddr;
	uint32_t * useqaddr;


	if (mutex->sig == _PTHREAD_MUTEX_SIG)
	{
		if (mutex->mtxopts.options.pshared == PTHREAD_PROCESS_SHARED) {
			MUTEX_GETSEQ_ADDR(mutex, lseqaddr, useqaddr);
		} else {
			lseqaddr = mutex->m_lseqaddr;
			useqaddr = mutex->m_useqaddr;
		}

		lgenval = *(lseqaddr);
		if ((mutex->m_tid == (uint64_t)0) &&
		    ((lgenval &  PTHRW_COUNT_MASK) == 0))
		{
			mutex->sig = _PTHREAD_NO_SIG;
			res = 0;
		}
		else
			res = EBUSY;
	} else 
		res = EINVAL;

	return (res);
}

#endif /* __i386__ || __x86_64__ */

#endif /* !BUILDING_VARIANT ] */

/*
 * Destroy a mutex attribute structure.
 */
int
pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;
	if (attr->sig != _PTHREAD_MUTEX_ATTR_SIG)
		return (EINVAL);
#endif /* __DARWIN_UNIX03 */

        attr->sig = _PTHREAD_NO_SIG;  /* Uninitialized */
        return (0);
}


