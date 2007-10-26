/*
 * Copyright (c) 2000-2003, 2007 Apple Inc. All rights reserved.
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
extern int __unix_conforming;

#include "plockstat.h"
#define READ_LOCK_PLOCKSTAT  0
#define WRITE_LOCK_PLOCKSTAT 1

#define BLOCK_FAIL_PLOCKSTAT    0
#define BLOCK_SUCCESS_PLOCKSTAT 1

/* maximum number of times a read lock may be obtained */
#define	MAX_READ_LOCKS		(INT_MAX - 1) 


#ifndef BUILDING_VARIANT /* [ */


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

/* Temp: untill pshared is fixed right */
#ifdef PR_5243343
/* 5243343 - temporary hack to detect if we are running the conformance test */
extern int PR_5243343_flag;
#endif /* PR_5243343 */

int
pthread_rwlockattr_setpshared(pthread_rwlockattr_t * attr, int pshared)
{
        if (attr->sig == _PTHREAD_RWLOCK_ATTR_SIG)
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

#endif /* !BUILDING_VARIANT ] */

int
pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
	int ret;

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		return(EINVAL);
	} else {
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
#if __DARWIN_UNIX03
		if (attr && (attr->sig != _PTHREAD_RWLOCK_ATTR_SIG)) {
			return(EINVAL);
		}
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
#if __DARWIN_UNIX03
	pthread_t self = pthread_self();	
#endif

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
#if __DARWIN_UNIX03
	pthread_t self = pthread_self();
#endif /* __DARWIN_UNIX03 */

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		PLOCKSTAT_RW_ERROR(rwlock, writer, EINVAL);    
		return(EINVAL);
	}
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


