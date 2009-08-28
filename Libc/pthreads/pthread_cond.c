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
 */
/*
 * MkLinux
 */

/*
 * POSIX Pthread Library
 */

#include "pthread_internals.h"
#include <sys/time.h>              /* For struct timespec and getclock(). */
#include <stdio.h>

#ifdef PLOCKSTAT
#include "plockstat.h"
#else /* !PLOCKSTAT */
#define PLOCKSTAT_MUTEX_RELEASE(x, y)
#endif /* PLOCKSTAT */


extern int __semwait_signal(int, int, int, int, int64_t, int32_t);
extern int _pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *, int);
extern int __unix_conforming;

#ifdef PR_5243343
/* 5243343 - temporary hack to detect if we are running the conformance test */
extern int PR_5243343_flag;
#endif /* PR_5243343 */

#if  defined(__i386__) || defined(__x86_64__)
__private_extern__ int __new_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime, int isRelative, int isconforming);
extern int _new_pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *, int);
extern int _new_pthread_cond_destroy(pthread_cond_t *);
extern int _new_pthread_cond_destroy_locked(pthread_cond_t *);
int _new_pthread_cond_broadcast(pthread_cond_t *cond);
int _new_pthread_cond_signal_thread_np(pthread_cond_t *cond, pthread_t thread);
int _new_pthread_cond_signal(pthread_cond_t *cond);
int _new_pthread_cond_timedwait_relative_np(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
int _new_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int _new_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
static void _new_cond_cleanup(void *arg);
static void _new_cond_dropwait(npthread_cond_t * cond);


#if defined(__LP64__)
#define  COND_GETSEQ_ADDR(cond, c_lseqcnt, c_useqcnt) \
{ \
	if (cond->misalign != 0) { \
		c_lseqcnt = &cond->c_seq[1]; \
		c_useqcnt = &cond->c_seq[2]; \
	} else { \
		/* aligned */ \
		c_lseqcnt = &cond->c_seq[0]; \
		c_useqcnt = &cond->c_seq[1]; \
	} \
}
#else /* __LP64__ */
#define  COND_GETSEQ_ADDR(cond, c_lseqcnt, c_useqcnt) \
{ \
	if (cond->misalign != 0) { \
		c_lseqcnt = &cond->c_seq[1]; \
		c_useqcnt = &cond->c_seq[2]; \
	} else { \
		/* aligned */ \
		c_lseqcnt = &cond->c_seq[0]; \
		c_useqcnt = &cond->c_seq[1]; \
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
#define _KSYN_TRACE_UM_CVWAIT   0x9000070
#define _KSYN_TRACE_UM_CVSIG    0x9000074
#define _KSYN_TRACE_UM_CVBRD    0x9000078

#endif /* _KSYN_TRACE_ */
#endif /* __i386__ || __x86_64__ */


#ifndef BUILDING_VARIANT /* [ */

/*
 * Destroy a condition variable.
 */
int       
pthread_cond_destroy(pthread_cond_t *cond)
{
	int ret;
	int sig = cond->sig;

	/* to provide backwards compat for apps using united condtn vars */
	if((sig != _PTHREAD_COND_SIG) && (sig != _PTHREAD_COND_SIG_init))
		return(EINVAL);

	LOCK(cond->lock);
	if (cond->sig == _PTHREAD_COND_SIG)
	{
#if  defined(__i386__) || defined(__x86_64__)
		if (cond->pshared == PTHREAD_PROCESS_SHARED) {
			ret = _new_pthread_cond_destroy_locked(cond);
			UNLOCK(cond->lock);
			return(ret);
		}
#endif /* __i386__ || __x86_64__ */
		if (cond->busy == (pthread_mutex_t *)NULL)
		{
			cond->sig = _PTHREAD_NO_SIG;
			ret = 0;
		} else
			ret = EBUSY;
	} else
		ret = EINVAL; /* Not an initialized condition variable structure */
	UNLOCK(cond->lock);
	return (ret);
}


/*
 * Signal a condition variable, waking up all threads waiting for it.
 */
int       
pthread_cond_broadcast(pthread_cond_t *cond)
{
	kern_return_t kern_res;
	semaphore_t sem;
	int sig = cond->sig;

	/* to provide backwards compat for apps using united condtn vars */
	if((sig != _PTHREAD_COND_SIG) && (sig != _PTHREAD_COND_SIG_init))
		return(EINVAL);

	LOCK(cond->lock);
	if (cond->sig != _PTHREAD_COND_SIG)
	{
		int res;

		if (cond->sig == _PTHREAD_COND_SIG_init)
		{
			_pthread_cond_init(cond, NULL, 0);
			res = 0;
		} else 
			res = EINVAL;  /* Not a condition variable */
		UNLOCK(cond->lock);
		return (res);
	}
#if  defined(__i386__) || defined(__x86_64__)
	else if (cond->pshared == PTHREAD_PROCESS_SHARED) {
		UNLOCK(cond->lock);
		return(_new_pthread_cond_broadcast(cond));
	}
#endif /* __i386__ || __x86_64__ */
	else if ((sem = cond->sem) == SEMAPHORE_NULL)
	{
		/* Avoid kernel call since there are no waiters... */
		UNLOCK(cond->lock);
		return (0);
	}
	cond->sigspending++;
	UNLOCK(cond->lock);

	PTHREAD_MACH_CALL(semaphore_signal_all(sem), kern_res);

	LOCK(cond->lock);
	cond->sigspending--;
	if (cond->waiters == 0 && cond->sigspending == 0)
	{
		cond->sem = SEMAPHORE_NULL;
		restore_sem_to_pool(sem);
	}
	UNLOCK(cond->lock);
	if (kern_res != KERN_SUCCESS)
		return (EINVAL);
	return (0);
}

/*
 * Signal a condition variable, waking a specified thread.
 */
int       
pthread_cond_signal_thread_np(pthread_cond_t *cond, pthread_t thread)
{
	kern_return_t kern_res;
	semaphore_t sem;
	int sig = cond->sig;

	/* to provide backwards compat for apps using united condtn vars */

	if((sig != _PTHREAD_COND_SIG) && (sig != _PTHREAD_COND_SIG_init))
		return(EINVAL);
	LOCK(cond->lock);
	if (cond->sig != _PTHREAD_COND_SIG)
	{
		int ret;

		if (cond->sig == _PTHREAD_COND_SIG_init) 
		{
			_pthread_cond_init(cond, NULL, 0);
			ret = 0;
		} else 
			ret = EINVAL; /* Not a condition variable */
		UNLOCK(cond->lock);
		return (ret);
	}
#if  defined(__i386__) || defined(__x86_64__)
	else if (cond->pshared == PTHREAD_PROCESS_SHARED) {
		UNLOCK(cond->lock);
		return(_new_pthread_cond_signal_thread_np(cond, thread));
	}
#endif /* __i386__ || __x86_64__ */
	else if ((sem = cond->sem) == SEMAPHORE_NULL)
	{
		/* Avoid kernel call since there are not enough waiters... */
		UNLOCK(cond->lock);
		return (0);
	}
	cond->sigspending++;
	UNLOCK(cond->lock);

	if (thread == (pthread_t)NULL)
	{
		kern_res = semaphore_signal_thread(sem, THREAD_NULL);
		if (kern_res == KERN_NOT_WAITING)
			kern_res = KERN_SUCCESS;
	}
	else if (thread->sig == _PTHREAD_SIG)
	{
	        PTHREAD_MACH_CALL(semaphore_signal_thread(
			sem, pthread_mach_thread_np(thread)), kern_res);
	}
	else
		kern_res = KERN_FAILURE;

	LOCK(cond->lock);
	cond->sigspending--;
	if (cond->waiters == 0 && cond->sigspending == 0)
	{
		cond->sem = SEMAPHORE_NULL;
		restore_sem_to_pool(sem);
	}
	UNLOCK(cond->lock);
	if (kern_res != KERN_SUCCESS)
		return (EINVAL);
	return (0);
}

/*
 * Signal a condition variable, waking only one thread.
 */
int
pthread_cond_signal(pthread_cond_t *cond)
{
	return pthread_cond_signal_thread_np(cond, NULL);
}

/*
 * Manage a list of condition variables associated with a mutex
 */

static void
_pthread_cond_add(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	pthread_cond_t *c;
	LOCK(mutex->lock);
	if ((c = mutex->busy) != (pthread_cond_t *)NULL)
	{
		c->prev = cond;
	} 
	cond->next = c;
	cond->prev = (pthread_cond_t *)NULL;
	mutex->busy = cond;
	UNLOCK(mutex->lock);
	if (cond->sem == SEMAPHORE_NULL)
		cond->sem = new_sem_from_pool();
}

static void
_pthread_cond_remove(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	pthread_cond_t *n, *p;

	LOCK(mutex->lock);
	if ((n = cond->next) != (pthread_cond_t *)NULL)
	{
		n->prev = cond->prev;
	}
	if ((p = cond->prev) != (pthread_cond_t *)NULL)
	{
		p->next = cond->next;
	} 
	else
	{ /* This is the first in the list */
		mutex->busy = n;
	}
	UNLOCK(mutex->lock);
	if (cond->sigspending == 0)
	{
		restore_sem_to_pool(cond->sem);
		cond->sem = SEMAPHORE_NULL;
	}
}

static void 
cond_cleanup(void *arg)
{
    pthread_cond_t *cond = (pthread_cond_t *)arg;
    pthread_mutex_t *mutex;
// 4597450: begin
    pthread_t thread = pthread_self();
	int thcanceled = 0;

	LOCK(thread->lock);
	thcanceled = (thread->detached & _PTHREAD_WASCANCEL);
	UNLOCK(thread->lock);

	if (thcanceled == 0)
		return;

// 4597450: end
    LOCK(cond->lock);
    mutex = cond->busy;
    cond->waiters--;
    if (cond->waiters == 0) {
        _pthread_cond_remove(cond, mutex);
        cond->busy = (pthread_mutex_t *)NULL;
    }
    UNLOCK(cond->lock);

    /*
    ** Can't do anything if this fails -- we're on the way out
    */
    (void)pthread_mutex_lock(mutex);
}

/*
 * Suspend waiting for a condition variable.
 * Note: we have to keep a list of condition variables which are using
 * this same mutex variable so we can detect invalid 'destroy' sequences.
 * If isconforming < 0, we skip the _pthread_testcancel(), but keep the
 * remaining conforming behavior..
 */
__private_extern__ int       
_pthread_cond_wait(pthread_cond_t *cond, 
		   pthread_mutex_t *mutex,
		   const struct timespec *abstime,
		   int isRelative,
		    int isconforming)
{
	int res;
	kern_return_t kern_res = KERN_SUCCESS;
	int wait_res = 0;
	pthread_mutex_t *busy;
	mach_timespec_t then = {0, 0};
	struct timespec cthen = {0,0};
	int sig = cond->sig;
	int msig = mutex->sig;
extern void _pthread_testcancel(pthread_t thread, int isconforming);

	/* to provide backwards compat for apps using united condtn vars */
	if((sig != _PTHREAD_COND_SIG) && (sig != _PTHREAD_COND_SIG_init))
		return(EINVAL);

	if (isconforming) {
		if((msig != _PTHREAD_MUTEX_SIG) && (msig != _PTHREAD_MUTEX_SIG_init))
			return(EINVAL);
		if (isconforming > 0)
			_pthread_testcancel(pthread_self(), 1);
	}
	LOCK(cond->lock);
	if (cond->sig != _PTHREAD_COND_SIG)
	{
		if (cond->sig != _PTHREAD_COND_SIG_init)
		{
				UNLOCK(cond->lock);
				return (EINVAL);        /* Not a condition variable */
		}
		_pthread_cond_init(cond, NULL, 0);
	}
#if  defined(__i386__) || defined(__x86_64__)
	else if (cond->pshared == PTHREAD_PROCESS_SHARED) {
		UNLOCK(cond->lock);
		return(__new_pthread_cond_wait(cond, mutex, abstime, isRelative, isconforming));
	}
#endif /* __i386__ || __x86_64__ */

	if (abstime) {
		if (!isconforming)
		{
			if (isRelative == 0) {
				struct timespec now;
				struct timeval tv;
				gettimeofday(&tv, NULL);
				TIMEVAL_TO_TIMESPEC(&tv, &now);

				/* Compute relative time to sleep */
				then.tv_nsec = abstime->tv_nsec - now.tv_nsec;
				then.tv_sec = abstime->tv_sec - now.tv_sec;
				if (then.tv_nsec < 0)
				{
					then.tv_nsec += NSEC_PER_SEC;
					then.tv_sec--;
				}
				if (((int)then.tv_sec < 0) ||
					((then.tv_sec == 0) && (then.tv_nsec == 0)))
				{
					UNLOCK(cond->lock);
					return ETIMEDOUT;
				}
			} else {
				then.tv_sec = abstime->tv_sec;
				then.tv_nsec = abstime->tv_nsec;
			}
			if (then.tv_nsec >= NSEC_PER_SEC) {
				UNLOCK(cond->lock);
				return EINVAL;
			}
		} else {
			if (isRelative == 0) {
				/* preflight the checks for failures */
				struct timespec now;
				struct timeval tv;
				gettimeofday(&tv, NULL);
				TIMEVAL_TO_TIMESPEC(&tv, &now);

				/* Compute relative time to sleep */
				then.tv_nsec = abstime->tv_nsec - now.tv_nsec;
				then.tv_sec = abstime->tv_sec - now.tv_sec;
				if (then.tv_nsec < 0)
				{
					then.tv_nsec += NSEC_PER_SEC;
					then.tv_sec--;
				}
				if (((int)then.tv_sec < 0) ||
					((then.tv_sec == 0) && (then.tv_nsec == 0)))
				{
					UNLOCK(cond->lock);
					return ETIMEDOUT;
				}
				if (then.tv_nsec >= NSEC_PER_SEC) {
					UNLOCK(cond->lock);
					return EINVAL;
				}
			}
			/* we can cleanup this code and pass the calculated time
			 * to the kernel. But kernel is going to do the same. TILL
			 * we change the kernel do this anyway
			 */
			cthen.tv_sec = abstime->tv_sec;
            		cthen.tv_nsec = abstime->tv_nsec;
            		if ((cthen.tv_sec < 0) || (cthen.tv_nsec < 0)) {
                		UNLOCK(cond->lock);
                		return EINVAL;
            		}
            		if (cthen.tv_nsec >= NSEC_PER_SEC) {
                		UNLOCK(cond->lock);
                		return EINVAL;
            		}
        	}
	}

	if (++cond->waiters == 1)
	{
		_pthread_cond_add(cond, mutex);
		cond->busy = mutex;
	}
	else if ((busy = cond->busy) != mutex)
	{
		/* Must always specify the same mutex! */
		cond->waiters--;
		UNLOCK(cond->lock);
		return (EINVAL);
	}
	UNLOCK(cond->lock);
	
	LOCK(mutex->lock);
	if (--mutex->mtxopts.options.lock_count == 0)
	{
		PLOCKSTAT_MUTEX_RELEASE(mutex, (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE)? 1:0);

		if (mutex->sem == SEMAPHORE_NULL)
			mutex->sem = new_sem_from_pool();
		mutex->owner = _PTHREAD_MUTEX_OWNER_SWITCHING;
		UNLOCK(mutex->lock);

		if (!isconforming) {
			if (abstime) {
				kern_res = semaphore_timedwait_signal(cond->sem, mutex->sem, then);
			} else {
				PTHREAD_MACH_CALL(semaphore_wait_signal(cond->sem, mutex->sem), kern_res);
			}
		} else {
            pthread_cleanup_push(cond_cleanup, (void *)cond);
            wait_res = __semwait_signal(cond->sem, mutex->sem, abstime != NULL, isRelative,
                                        (int64_t)cthen.tv_sec, (int32_t)cthen.tv_nsec);
            pthread_cleanup_pop(0);
		}
	} else {
		PLOCKSTAT_MUTEX_RELEASE(mutex, (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE)? 1:0);
		UNLOCK(mutex->lock);
		if (!isconforming) {
			if (abstime) {
				kern_res = semaphore_timedwait(cond->sem, then);
			} else {
				PTHREAD_MACH_CALL(semaphore_wait(cond->sem), kern_res);
			}
		 } else {
				pthread_cleanup_push(cond_cleanup, (void *)cond);
                wait_res = __semwait_signal(cond->sem, 0, abstime != NULL, isRelative,
                                            (int64_t)cthen.tv_sec, (int32_t)cthen.tv_nsec);
                pthread_cleanup_pop(0);
		}

	}

	LOCK(cond->lock);
	cond->waiters--;
	if (cond->waiters == 0)
	{
		_pthread_cond_remove(cond, mutex);
		cond->busy = (pthread_mutex_t *)NULL;
	}
	UNLOCK(cond->lock);
	if ((res = pthread_mutex_lock(mutex)) != 0)
		return (res);

	if (!isconforming) {
		/* KERN_ABORTED can be treated as a spurious wakeup */
		if ((kern_res == KERN_SUCCESS) || (kern_res == KERN_ABORTED))
			return (0);
		else if (kern_res == KERN_OPERATION_TIMED_OUT)
			return (ETIMEDOUT);
		return (EINVAL);
	} else {
    	if (wait_res < 0) {
			if (errno == ETIMEDOUT) {
				return ETIMEDOUT;
			} else if (errno == EINTR) {
				/*
				**  EINTR can be treated as a spurious wakeup unless we were canceled.
				*/
				return 0;	
				}
			return EINVAL;
    	}
    	return 0;
	}
}


int       
pthread_cond_timedwait_relative_np(pthread_cond_t *cond, 
		       pthread_mutex_t *mutex,
		       const struct timespec *abstime)
{
	return (_pthread_cond_wait(cond, mutex, abstime, 1, 0));
}

int
pthread_condattr_init(pthread_condattr_t *attr)
{
        attr->sig = _PTHREAD_COND_ATTR_SIG;
        attr->pshared = _PTHREAD_DEFAULT_PSHARED;
        return (0);
}

int       
pthread_condattr_destroy(pthread_condattr_t *attr)
{
        attr->sig = _PTHREAD_NO_SIG;  /* Uninitialized */
        return (0);
}

int
pthread_condattr_getpshared(const pthread_condattr_t *attr,
				int *pshared)
{
        if (attr->sig == _PTHREAD_COND_ATTR_SIG)
        {
                *pshared = (int)attr->pshared;
                return (0);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}


__private_extern__ int       
_pthread_cond_init(pthread_cond_t *cond,
		  const pthread_condattr_t *attr,
		  int conforming)
{
	cond->next = (pthread_cond_t *)NULL;
	cond->prev = (pthread_cond_t *)NULL;
	cond->busy = (pthread_mutex_t *)NULL;
	cond->waiters = 0;
	cond->sigspending = 0;
	if (conforming) {
		if (attr)
			cond->pshared = attr->pshared;
		else
			cond->pshared = _PTHREAD_DEFAULT_PSHARED;
	} else
		cond->pshared = _PTHREAD_DEFAULT_PSHARED;
	cond->sem = SEMAPHORE_NULL;
	cond->sig = _PTHREAD_COND_SIG;
	return (0);
}


/* temp home till pshared is fixed correctly */
int
pthread_condattr_setpshared(pthread_condattr_t * attr, int pshared)
{

        if (attr->sig == _PTHREAD_COND_ATTR_SIG)
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

__private_extern__ int       
_new_pthread_cond_init(pthread_cond_t *ocond,
		  const pthread_condattr_t *attr,
		  int conforming)
{
	npthread_cond_t * cond = (npthread_cond_t *)ocond;

	cond->busy = (npthread_mutex_t *)NULL;
	cond->c_seq[0] = 0;
	cond->c_seq[1] = 0;
	cond->c_seq[2] = 0;

	cond->rfu = 0;
	if (((uintptr_t)cond & 0x07) != 0) {
		cond->misalign = 1;
	} else {
		cond->misalign = 0;
	}
	if (conforming) {
		if (attr)
			cond->pshared = attr->pshared;
		else
			cond->pshared = _PTHREAD_DEFAULT_PSHARED;
	} else
		cond->pshared = _PTHREAD_DEFAULT_PSHARED;
	cond->sig = _PTHREAD_COND_SIG;
	return (0);
}

int
_new_pthread_cond_destroy(pthread_cond_t * ocond)
{
	npthread_cond_t *cond = (npthread_cond_t *)ocond;
	int ret;

	LOCK(cond->lock);
	ret = _new_pthread_cond_destroy_locked(ocond);
	UNLOCK(cond->lock);
	
	return(ret);
}

int       
_new_pthread_cond_destroy_locked(pthread_cond_t * ocond)
{
	npthread_cond_t *cond = (npthread_cond_t *)ocond;
	int ret;
	int sig = cond->sig;
	uint32_t * c_lseqcnt;
	uint32_t * c_useqcnt;
	uint32_t lgenval , ugenval;

	/* to provide backwards compat for apps using united condtn vars */
	if((sig != _PTHREAD_COND_SIG) && (sig != _PTHREAD_COND_SIG_init))
		return(EINVAL);

	if (cond->sig == _PTHREAD_COND_SIG)
	{
		COND_GETSEQ_ADDR(cond, c_lseqcnt, c_useqcnt);
retry:
		lgenval = *c_lseqcnt;
		ugenval = *c_useqcnt;
		if (lgenval == ugenval)
		{
			cond->sig = _PTHREAD_NO_SIG;
			ret = 0;
		} else
			ret = EBUSY;
	} else
		ret = EINVAL; /* Not an initialized condition variable structure */
	return (ret);
}

/*
 * Signal a condition variable, waking up all threads waiting for it.
 */
int       
_new_pthread_cond_broadcast(pthread_cond_t *ocond)
{
	npthread_cond_t * cond = (npthread_cond_t *)ocond;
	int sig = cond->sig;
	npthread_mutex_t * mutex;
	uint32_t lgenval, ugenval, mgen, ugen, flags, mtxgen, mtxugen, notify;
	int diffgen, retval, dropcount, mutexrefs;
	uint64_t oldval64, newval64;
	uint32_t * c_lseqcnt;
	uint32_t * c_useqcnt;
	uint32_t * pmtx = NULL;


	/* to provide backwards compat for apps using united condtn vars */
	if((sig != _PTHREAD_COND_SIG) && (sig != _PTHREAD_COND_SIG_init))
		return(EINVAL);

	if (sig != _PTHREAD_COND_SIG)
	{
		int res;

		LOCK(cond->lock);
		if (cond->sig == _PTHREAD_COND_SIG_init)
		{
			_new_pthread_cond_init(ocond, NULL, 0);
			res = 0;
		} else  if (cond->sig != _PTHREAD_COND_SIG) {
			res = EINVAL;  /* Not a condition variable */
			UNLOCK(cond->lock);
			return (res);
		}
		UNLOCK(cond->lock);
	}

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVBRD | DBG_FUNC_START, (uint32_t)cond, 0, 0, 0, 0);
#endif

	COND_GETSEQ_ADDR(cond, c_lseqcnt, c_useqcnt);
retry:
	lgenval = *c_lseqcnt;
	ugenval = *c_useqcnt;
	diffgen = lgenval - ugenval;	/* pendig waiters */
 
	if (diffgen <= 0) {
		return(0);
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVBRD | DBG_FUNC_END, (uint32_t)cond, 0, 0, 0, 0);
#endif
	}
	
	mutex = cond->busy;
	
	if (OSAtomicCompareAndSwap32(ugenval, ugenval+diffgen, (volatile int *)c_useqcnt) != TRUE) 
		goto retry;

#ifdef COND_MTX_WAITQUEUEMOVE

	if ((mutex != NULL) && cond->pshared != PTHREAD_PROCESS_SHARED) {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVBRD | DBG_FUNC_NONE, (uint32_t)cond, 1, diffgen, 0, 0);
#endif
		(void)__mtx_holdlock(mutex, diffgen, &flags, &pmtx, &mgen, &ugen);
		mutexrefs = 1;	
	} else {
		if (cond->pshared != PTHREAD_PROCESS_SHARED)
			flags = _PTHREAD_MTX_OPT_NOHOLD;
		else
			flags = _PTHREAD_MTX_OPT_NOHOLD | _PTHREAD_MTX_OPT_PSHARED;
		mgen = ugen = 0;
		mutexrefs = 0;	
		pmtx = NULL;
	}
#else /* COND_MTX_WAITQUEUEMOVE */
	
	if (cond->pshared != PTHREAD_PROCESS_SHARED)
		flags = _PTHREAD_MTX_OPT_NOHOLD;
	else
		flags = _PTHREAD_MTX_OPT_NOHOLD | _PTHREAD_MTX_OPT_PSHARED;
	pmtx = NULL;
	mgen = ugen = 0;
	mutexrefs = 0;	
#endif /* COND_MTX_WAITQUEUEMOVE */

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVBRD | DBG_FUNC_NONE, (uint32_t)cond, 3, diffgen, 0, 0);
#endif
	retval = __psynch_cvbroad(ocond, lgenval, diffgen, (pthread_mutex_t *)pmtx, mgen, ugen , (uint64_t)0,  flags);

#ifdef COND_MTX_WAITQUEUEMOVE
	if ((retval != -1) && (retval != 0)) {
		if ((mutexrefs != 0) && (retval <= PTHRW_MAX_READERS/2)) {
			dropcount = (retval);
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVBRD | DBG_FUNC_NONE, (uint32_t)cond, 2, dropcount, 0, 0);
#endif
			retval = __mtx_droplock(mutex, dropcount, &flags, &pmtx, &mtxgen, &mtxugen, &notify);
		}
	}
#endif /* COND_MTX_WAITQUEUEMOVE */

	oldval64 = (((uint64_t)(ugenval+diffgen)) << 32);
	oldval64 |= lgenval;
	newval64 = 0;

	OSAtomicCompareAndSwap64(oldval64, newval64, (volatile int64_t *)c_lseqcnt);

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVBRD | DBG_FUNC_END, (uint32_t)cond, 0, 0, 0, 0);
#endif
	return(0);
}


/*
 * Signal a condition variable, waking a specified thread.
 */
int       
_new_pthread_cond_signal_thread_np(pthread_cond_t *ocond, pthread_t thread)
{
	npthread_cond_t * cond = (npthread_cond_t *)ocond;
	int sig = cond->sig;
	npthread_mutex_t  * mutex;
	int retval, dropcount;
	uint32_t lgenval, ugenval, diffgen, mgen, ugen, flags, mtxgen, mtxugen, notify;
	uint32_t * c_lseqcnt;
	uint32_t * c_useqcnt;
	uint64_t oldval64, newval64;
	int mutexrefs;
	uint32_t * pmtx = NULL;

	/* to provide backwards compat for apps using united condtn vars */

	if((sig != _PTHREAD_COND_SIG) && (sig != _PTHREAD_COND_SIG_init))
		return(EINVAL);
	if (cond->sig != _PTHREAD_COND_SIG) {
		LOCK(cond->lock);
		if (cond->sig != _PTHREAD_COND_SIG) {
			if  (cond->sig == _PTHREAD_COND_SIG_init) {
				_new_pthread_cond_init(ocond, NULL, 0);
			} else   {
				UNLOCK(cond->lock);
				return(EINVAL);
			}
		}
		UNLOCK(cond->lock);
	}

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_START, (uint32_t)cond, 0, 0, 0, 0);
#endif
	COND_GETSEQ_ADDR(cond, c_lseqcnt, c_useqcnt);
retry:
	lgenval = *c_lseqcnt;
	ugenval = *c_useqcnt;
	diffgen = lgenval - ugenval;	/* pendig waiters */
	if (diffgen <= 0) {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_END, (uint32_t)cond, 0, 0, 0, 0);
#endif
		return(0);
	}
	
	mutex = cond->busy;

	if (OSAtomicCompareAndSwap32(ugenval, ugenval+1, (volatile int *)c_useqcnt) != TRUE) 
		goto retry;

#ifdef COND_MTX_WAITQUEUEMOVE
	if ((mutex != NULL) && (cond->pshared != PTHREAD_PROCESS_SHARED)) {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_NONE, (uint32_t)cond, 1, 0, 0, 0);
#endif
		(void)__mtx_holdlock(mutex, 1, &flags, &pmtx, &mgen, &ugen);
		mutexrefs = 1;
	} else {
		if (cond->pshared != PTHREAD_PROCESS_SHARED)
			flags = _PTHREAD_MTX_OPT_NOHOLD;
		else
			flags = _PTHREAD_MTX_OPT_NOHOLD | _PTHREAD_MTX_OPT_PSHARED;
		mgen = ugen = 0;
		mutexrefs = 0;
	}
#else /* COND_MTX_WAITQUEUEMOVE */
	if (cond->pshared != PTHREAD_PROCESS_SHARED)
		flags = _PTHREAD_MTX_OPT_NOHOLD;
	else
		flags = _PTHREAD_MTX_OPT_NOHOLD | _PTHREAD_MTX_OPT_PSHARED;
	mgen = ugen = 0;
	mutexrefs = 0;	

#endif /* COND_MTX_WAITQUEUEMOVE */
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_NONE, (uint32_t)cond, 3, lgenval, ugenval+1, 0);
#endif
	retval = __psynch_cvsignal(ocond, lgenval, ugenval+1,(pthread_mutex_t *)mutex, mgen, ugen, pthread_mach_thread_np(thread), flags);

#ifdef COND_MTX_WAITQUEUEMOVE
	if ((retval != -1) && (retval != 0) && (mutexrefs != 0)) {
		dropcount = retval;
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_NONE, (uint32_t)cond, 4, dropcount, 0, 0);
#endif
		retval = __mtx_droplock(mutex, dropcount, &flags, &pmtx, &mtxgen, &mtxugen, &notify);
	}
#endif /* COND_MTX_WAITQUEUEMOVE */

	if (lgenval == ugenval+1){
		oldval64 = (((uint64_t)(ugenval+1)) << 32);
		oldval64 |= lgenval;
		newval64 = 0;
		OSAtomicCompareAndSwap64(oldval64, newval64, (volatile int64_t *)c_lseqcnt);
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_NONE, (uint32_t)cond, 5, 0, 0, 0);
#endif
	}
			
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_END, (uint32_t)cond, 0, 0, 0, 0);
#endif
	return (0);
}

/*
 * Signal a condition variable, waking only one thread.
 */
int
_new_pthread_cond_signal(pthread_cond_t *cond)
{
	return _new_pthread_cond_signal_thread_np(cond, NULL);
}

/*
 * Manage a list of condition variables associated with a mutex
 */


/*
 * Suspend waiting for a condition variable.
 * Note: we have to keep a list of condition variables which are using
 * this same mutex variable so we can detect invalid 'destroy' sequences.
 * If isconforming < 0, we skip the _pthread_testcancel(), but keep the
 * remaining conforming behavior..
 */
__private_extern__ int       
__new_pthread_cond_wait(pthread_cond_t *ocond, 
		   pthread_mutex_t *omutex,
		   const struct timespec *abstime,
		   int isRelative,
		    int isconforming)
{
	int retval;
	npthread_cond_t * cond = (npthread_cond_t *)ocond;
	npthread_mutex_t * mutex = (npthread_mutex_t * )omutex;
	mach_timespec_t then = {0,0};
	struct timespec cthen = {0,0};
	int sig = cond->sig;
	int msig = mutex->sig;
	int firstfit = 0;
	npthread_mutex_t * pmtx;
	uint32_t mtxgen, mtxugen, flags, updateval, notify;
	uint32_t lgenval, ugenval;
	uint32_t * c_lseqcnt;
	uint32_t * c_useqcnt;
	uint32_t * npmtx = NULL;

extern void _pthread_testcancel(pthread_t thread, int isconforming);

	/* to provide backwards compat for apps using united condtn vars */
	if((sig != _PTHREAD_COND_SIG) && (sig != _PTHREAD_COND_SIG_init))
		return(EINVAL);

	if (isconforming) {
		if((msig != _PTHREAD_MUTEX_SIG) && (msig != _PTHREAD_MUTEX_SIG_init))
			return(EINVAL);
		if (isconforming > 0)
			_pthread_testcancel(pthread_self(), 1);
	}
	if (cond->sig != _PTHREAD_COND_SIG)
	{
		LOCK(cond->lock);
		if (cond->sig != _PTHREAD_COND_SIG_init)
		{
				UNLOCK(cond->lock);
				return (EINVAL);        /* Not a condition variable */
		}
		_new_pthread_cond_init(ocond, NULL, 0);
		UNLOCK(cond->lock);
	}

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVWAIT | DBG_FUNC_START, (uint32_t)cond, 0, 0, (uint32_t)abstime, 0);
#endif
	COND_GETSEQ_ADDR(cond, c_lseqcnt, c_useqcnt);

	/* send relative time to kernel */
	if (abstime) {
		if (isRelative == 0) {
			struct timespec now;
			struct timeval tv;
			gettimeofday(&tv, NULL);
			TIMEVAL_TO_TIMESPEC(&tv, &now);

			/* Compute relative time to sleep */
			then.tv_nsec = abstime->tv_nsec - now.tv_nsec;
			then.tv_sec = abstime->tv_sec - now.tv_sec;
			if (then.tv_nsec < 0)
			{
				then.tv_nsec += NSEC_PER_SEC;
				then.tv_sec--;
			}
			if (((int)then.tv_sec < 0) ||
				((then.tv_sec == 0) && (then.tv_nsec == 0)))
			{
				UNLOCK(cond->lock);
				return ETIMEDOUT;
			}
			if (isconforming != 0) {
				cthen.tv_sec = abstime->tv_sec;
            			cthen.tv_nsec = abstime->tv_nsec;
            			if ((cthen.tv_sec < 0) || (cthen.tv_nsec < 0)) {
                			UNLOCK(cond->lock);
                			return EINVAL;
            			}
            			if (cthen.tv_nsec >= NSEC_PER_SEC) {
                			UNLOCK(cond->lock);
                			return EINVAL;
            			}
			}
		} else {
			then.tv_sec = abstime->tv_sec;
			then.tv_nsec = abstime->tv_nsec;
		}
		if(isconforming && ((then.tv_sec < 0) || (then.tv_nsec < 0))) {
			return EINVAL;
		}
		if (then.tv_nsec >= NSEC_PER_SEC) {
			return EINVAL;
		}
	}

	cond->busy = mutex;
	pmtx = mutex; 

	ugenval = *c_useqcnt;
	lgenval = OSAtomicIncrement32((volatile int32_t *)c_lseqcnt);
	

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVWAIT | DBG_FUNC_NONE, (uint32_t)cond, 1, lgenval, ugenval, 0);
#endif
	notify = 0;
	retval = __mtx_droplock(pmtx, 1, &flags, &npmtx, &mtxgen, &mtxugen, &notify);
	if (retval != 0)
		return(EINVAL);
	if ((notify & 1) == 0) {
		npmtx = NULL;
	}
	if ((notify & 0xc0000000) != 0)
		then.tv_nsec |= (notify & 0xc0000000);
	
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVWAIT | DBG_FUNC_NONE, (uint32_t)cond, 3, (uint32_t)mutex, 0, 0);
#endif
	
	if (isconforming) {
		pthread_cleanup_push(_new_cond_cleanup, (void *)cond);
		updateval = __psynch_cvwait(ocond, lgenval, ugenval, (pthread_mutex_t *)npmtx, mtxgen, mtxugen, (uint64_t)then.tv_sec, (uint64_t)then.tv_nsec);
		pthread_cleanup_pop(0);
	} else {
		updateval = __psynch_cvwait(ocond, lgenval, ugenval, (pthread_mutex_t *)npmtx, mtxgen, mtxugen, (uint64_t)then.tv_sec, (uint64_t)then.tv_nsec);

	}

	retval = 0;

#ifdef COND_MTX_WAITQUEUEMOVE
	/* Needs to handle timedout */
	if (updateval == (uint32_t)-1) {
		retval = errno;
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVWAIT | DBG_FUNC_NONE, (uint32_t)cond, 4, retval, 0, 0);
#endif
		/* add unlock ref to show one less waiter */
		_new_cond_dropwait(cond);

		pthread_mutex_lock(omutex);

	} else if ((updateval & PTHRW_MTX_NONE) != 0) {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVWAIT | DBG_FUNC_NONE, (uint32_t)cond, 5, updateval, 0, 0);
#endif
		pthread_mutex_lock(omutex);
	} else {
		/* on successful return mutex held */
		/* returns 0 on succesful update */
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVWAIT | DBG_FUNC_NONE, (uint32_t)cond, 6, updateval, 0, 0);
#endif
		firstfit = (mutex->mtxopts.options.policy == _PTHREAD_MUTEX_POLICY_FIRSTFIT);
		if (__mtx_updatebits( mutex, updateval, firstfit, 1) == 1) {
			/* not expected to  be here */
			LIBC_ABORT("CONDWAIT mutex acquire mishap");
		}
		if (mutex->mtxopts.options.type == PTHREAD_MUTEX_RECURSIVE)
			mutex->mtxopts.options.lock_count++;
	}
#else /* COND_MTX_WAITQUEUEMOVE */
	if (updateval == (uint32_t)-1) {
		if (errno == ETIMEDOUT) {
			retval = ETIMEDOUT;
		} else if (errno == EINTR) {
			/*
			**  EINTR can be treated as a spurious wakeup unless we were canceled.
			*/
			retval = 0;
		} else 
			retval =  EINVAL;

		/* add unlock ref to show one less waiter */
		_new_cond_dropwait(cond);
	} else
		retval = 0;
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVWAIT | DBG_FUNC_NONE, (uint32_t)cond, 4, retval, 0, 0);
#endif
		pthread_mutex_lock(omutex);

#endif /* COND_MTX_WAITQUEUEMOVE */

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVWAIT | DBG_FUNC_END, (uint32_t)cond, 0, 0, retval, 0);
#endif
	return(retval);
}

static void 
_new_cond_cleanup(void *arg)
{
	npthread_cond_t *cond = (npthread_cond_t *)arg;
	pthread_mutex_t *mutex;

// 4597450: begin
	pthread_t thread = pthread_self();
	int thcanceled = 0;

	LOCK(thread->lock);
	thcanceled = (thread->detached & _PTHREAD_WASCANCEL);
	UNLOCK(thread->lock);

	if (thcanceled == 0)
		return;

// 4597450: end
    	mutex = cond->busy;
	
	/* add unlock ref to show one less waiter */
	_new_cond_dropwait(cond);

	/*
	** Can't do anything if this fails -- we're on the way out
	*/
	if (mutex != NULL)
    		(void)pthread_mutex_lock(mutex);

}

void
_new_cond_dropwait(npthread_cond_t * cond)
{
	int sig = cond->sig;
	int retval;
	uint32_t lgenval, ugenval, diffgen, mgen, ugen, flags;
	uint32_t * c_lseqcnt;
	uint32_t * c_useqcnt;
	uint64_t oldval64, newval64;

	/* to provide backwards compat for apps using united condtn vars */

	if (sig != _PTHREAD_COND_SIG) 
		return;

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_START, (uint32_t)cond, 0, 0, 0xee, 0);
#endif
	COND_GETSEQ_ADDR(cond, c_lseqcnt, c_useqcnt);
retry:
	lgenval = *c_lseqcnt;
	ugenval = *c_useqcnt;
	diffgen = lgenval - ugenval;	/* pending waiters */

	if (diffgen <= 0) {
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_END, (uint32_t)cond, 1, 0, 0xee, 0);
#endif
		return;
	}
	
	if (OSAtomicCompareAndSwap32(ugenval, ugenval+1, (volatile int *)c_useqcnt) != TRUE) 
		goto retry;

	if (lgenval == ugenval+1) {
		/* last one */
		/* send last drop  notify to erase pre post */
		flags =  _PTHREAD_MTX_OPT_LASTDROP;

		if (cond->pshared == PTHREAD_PROCESS_SHARED)
			flags |= _PTHREAD_MTX_OPT_PSHARED;
		mgen = ugen = 0;

#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_NONE, (uint32_t)cond, 1, 0, 0xee, 0);
#endif
		retval = __psynch_cvsignal((pthread_cond_t *)cond, lgenval, ugenval+1,(pthread_mutex_t *)NULL, mgen, ugen, MACH_PORT_NULL, flags);

		oldval64 = (((uint64_t)(ugenval+1)) << 32);
		oldval64 |= lgenval;
		newval64 = 0;
		OSAtomicCompareAndSwap64(oldval64, newval64, (volatile int64_t *)c_lseqcnt);
	}
			
#if _KSYN_TRACE_
	(void)__kdebug_trace(_KSYN_TRACE_UM_CVSIG | DBG_FUNC_END, (uint32_t)cond, 2, 0, 0xee, 0);
#endif
	return;
}


int       
_new_pthread_cond_timedwait_relative_np(pthread_cond_t *cond, 
		       pthread_mutex_t *mutex,
		       const struct timespec *abstime)
{
	return (__new_pthread_cond_wait(cond, mutex, abstime, 1, 0));
}


int       
_new_pthread_cond_wait(pthread_cond_t *cond, 
		  pthread_mutex_t *mutex)
{
	return(__new_pthread_cond_wait(cond, mutex, 0, 0, 1));
}

int       
_new_pthread_cond_timedwait(pthread_cond_t *cond, 
		       pthread_mutex_t *mutex,
		       const struct timespec *abstime)
{
	return(__new_pthread_cond_wait(cond, mutex, abstime, 0, 1));
}

#endif /* __i386__ || __x86_64__ */

#else /* !BUILDING_VARIANT */

extern int _pthread_cond_wait(pthread_cond_t *cond, 
			pthread_mutex_t *mutex,
			const struct timespec *abstime,
			int isRelative,
			int isconforming);

#endif /* !BUILDING_VARIANT ] */
/*
 * Initialize a condition variable.  Note: 'attr' is ignored.
 */

/*
 * Initialize a condition variable.  This is the public interface.
 * We can't trust the lock, so initialize it first before taking
 * it.
 */
int       
pthread_cond_init(pthread_cond_t *cond,
		  const pthread_condattr_t *attr)
{
	int conforming;

#if __DARWIN_UNIX03
        conforming = 1;
#else /* __DARWIN_UNIX03 */
        conforming = 0;
#endif /* __DARWIN_UNIX03 */

	LOCK_INIT(cond->lock);
#if  defined(__i386__) || defined(__x86_64__)
	if ((attr != NULL) && (attr->pshared == PTHREAD_PROCESS_SHARED)) {
		return(_new_pthread_cond_init(cond, attr, conforming));
	}
#endif /* __i386__ || __x86_64__ */
	
	return (_pthread_cond_init(cond, attr, conforming));
}

/*
int       
pthread_cond_wait(pthread_cond_t *cond, 
		  pthread_mutex_t *mutex)

int       
pthread_cond_timedwait(pthread_cond_t *cond, 
		       pthread_mutex_t *mutex,
		       const struct timespec *abstime)

moved to pthread_cancelable.c */
