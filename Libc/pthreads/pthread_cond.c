/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
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
    
extern void _pthread_mutex_remove(pthread_mutex_t *, pthread_t);
extern int __unix_conforming;

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
	if((sig != _PTHREAD_COND_SIG) && (sig !=_PTHREAD_COND_SIG_init))
		return(EINVAL);

	LOCK(cond->lock);
	if (cond->sig == _PTHREAD_COND_SIG)
	{
		if (cond->busy == (pthread_mutex_t *)NULL)
		{
			cond->sig = _PTHREAD_NO_SIG;
			ret = ESUCCESS;
		} else
			ret = EBUSY;
	} else
		ret = EINVAL; /* Not an initialized condition variable structure */
	UNLOCK(cond->lock);
	return (ret);
}

/*
 * Initialize a condition variable.  Note: 'attr' is ignored.
 */
static int       
_pthread_cond_init(pthread_cond_t *cond,
		  const pthread_condattr_t *attr)
{
	cond->next = (pthread_cond_t *)NULL;
	cond->prev = (pthread_cond_t *)NULL;
	cond->busy = (pthread_mutex_t *)NULL;
	cond->waiters = 0;
	cond->sigspending = 0;
	cond->sem = SEMAPHORE_NULL;
	cond->sig = _PTHREAD_COND_SIG;
	return (ESUCCESS);
}

/*
 * Initialize a condition variable.  This is the public interface.
 * We can't trust the lock, so initialize it first before taking
 * it.
 */
int       
pthread_cond_init(pthread_cond_t *cond,
		  const pthread_condattr_t *attr)
{
	LOCK_INIT(cond->lock);
	return (_pthread_cond_init(cond, attr));
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
	if((sig != _PTHREAD_COND_SIG) && (sig !=_PTHREAD_COND_SIG_init))
		return(EINVAL);

	LOCK(cond->lock);
	if (cond->sig != _PTHREAD_COND_SIG)
	{
		int res;

		if (cond->sig == _PTHREAD_COND_SIG_init)
		{
			_pthread_cond_init(cond, NULL);
			res = ESUCCESS;
		} else 
			res = EINVAL;  /* Not a condition variable */
		UNLOCK(cond->lock);
		return (res);
	}
	else if ((sem = cond->sem) == SEMAPHORE_NULL)
	{
		/* Avoid kernel call since there are no waiters... */
		UNLOCK(cond->lock);
		return (ESUCCESS);
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
	return (ESUCCESS);
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
	if((sig != _PTHREAD_COND_SIG) && (sig !=_PTHREAD_COND_SIG_init))
		return(EINVAL);

	LOCK(cond->lock);
	if (cond->sig != _PTHREAD_COND_SIG)
	{
		int ret;

		if (cond->sig == _PTHREAD_COND_SIG_init) 
		{
			_pthread_cond_init(cond, NULL);
			ret = ESUCCESS;
		}
		else
			ret = EINVAL; /* Not a condition variable */
		UNLOCK(cond->lock);
		return (ret);
	}
	else if ((sem = cond->sem) == SEMAPHORE_NULL)
	{
		/* Avoid kernel call since there are not enough waiters... */
		UNLOCK(cond->lock);
		return (ESUCCESS);
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
	return (ESUCCESS);
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

static void cond_cleanup(void *arg)
{
    pthread_cond_t *cond = (pthread_cond_t *)arg;
    pthread_mutex_t *mutex;
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
 */
__private_extern__ int       
_pthread_cond_wait(pthread_cond_t *cond, 
		   pthread_mutex_t *mutex,
		   const struct timespec *abstime,
		   int isRelative,
		    int isconforming)
{
	int res;
	kern_return_t kern_res;
	int wait_res;
	pthread_mutex_t *busy;
	mach_timespec_t then;
	struct timespec cthen = {0,0};
	int sig = cond->sig;

	/* to provide backwards compat for apps using united condtn vars */
	if((sig != _PTHREAD_COND_SIG) && (sig !=_PTHREAD_COND_SIG_init))
		return(EINVAL);
	LOCK(cond->lock);
	if (cond->sig != _PTHREAD_COND_SIG)
	{
		if (cond->sig != _PTHREAD_COND_SIG_init)
		{
			UNLOCK(cond->lock);
			return (EINVAL);        /* Not a condition variable */
		}
		_pthread_cond_init(cond, NULL);
	}

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
	
#if defined(DEBUG)
	_pthread_mutex_remove(mutex, pthread_self());
#endif
	LOCK(mutex->lock);
	if (--mutex->lock_count == 0)
	{
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
			cthen.tv_sec, cthen.tv_nsec);
            pthread_cleanup_pop(0);
		}
	} else {
		UNLOCK(mutex->lock);
		if (!isconforming) {
			if (abstime) {
				kern_res = semaphore_timedwait(cond->sem, then);
			} else {
				PTHREAD_MACH_CALL(semaphore_wait(cond->sem), kern_res);
			}
		 } else {
				pthread_cleanup_push(cond_cleanup, (void *)cond);
                wait_res = __semwait_signal(cond->sem, NULL, abstime != NULL, isRelative,
			cthen.tv_sec, cthen.tv_nsec);
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
	if ((res = pthread_mutex_lock(mutex)) != ESUCCESS)
		return (res);

	if (!isconforming) {
		/* KERN_ABORTED can be treated as a spurious wakeup */
		if ((kern_res == KERN_SUCCESS) || (kern_res == KERN_ABORTED))
			return (ESUCCESS);
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
        return (ESUCCESS);
}

int       
pthread_condattr_destroy(pthread_condattr_t *attr)
{
        attr->sig = _PTHREAD_NO_SIG;  /* Uninitialized */
        return (ESUCCESS);
}

int
pthread_condattr_getpshared(const pthread_condattr_t *attr,
				int *pshared)
{
        if (attr->sig == _PTHREAD_COND_ATTR_SIG)
        {
                *pshared = (int)PTHREAD_PROCESS_PRIVATE;
                return (ESUCCESS);
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }
}


int
pthread_condattr_setpshared(pthread_condattr_t * attr, int pshared)
{
        if (attr->sig == _PTHREAD_COND_ATTR_SIG)
        {
                if ( pshared == PTHREAD_PROCESS_PRIVATE)
                {
			/* attr->pshared = pshared */
                        return (ESUCCESS);
                } else
                {
                        return (EINVAL); /* Invalid parameter */
                }
        } else
        {
                return (EINVAL); /* Not an initialized 'attribute' structure */
        }

}

#else /* !BUILDING_VARIANT */
extern int _pthread_cond_wait(pthread_cond_t *cond, 
			pthread_mutex_t *mutex,
			const struct timespec *abstime,
			int isRelative,
			int isconforming);

#endif /* !BUILDING_VARIANT ] */

int       
pthread_cond_wait(pthread_cond_t *cond, 
		  pthread_mutex_t *mutex)
{
	int conforming;
#if __DARWIN_UNIX03

	if (__unix_conforming == 0)
		__unix_conforming = 1;

	conforming = 1;
#else /* __DARWIN_UNIX03 */
	conforming = 0;
#endif /* __DARWIN_UNIX03 */
	return (_pthread_cond_wait(cond, mutex, (struct timespec *)NULL, 0, conforming));
}

int       
pthread_cond_timedwait(pthread_cond_t *cond, 
		       pthread_mutex_t *mutex,
		       const struct timespec *abstime)
{
	int conforming;
#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;

        conforming = 1;
#else /* __DARWIN_UNIX03 */
        conforming = 0;
#endif /* __DARWIN_UNIX03 */

	return (_pthread_cond_wait(cond, mutex, abstime, 0, conforming));
}

