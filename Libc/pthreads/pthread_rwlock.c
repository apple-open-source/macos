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

/* maximum number of times a read lock may be obtained */
#define	MAX_READ_LOCKS		(INT_MAX - 1)

int
pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) {
		return(EINVAL);
	} else {
		pthread_mutex_destroy(&rwlock->lock);
		pthread_cond_destroy(&rwlock->read_signal);
		pthread_cond_destroy(&rwlock->write_signal);
		rwlock->sig = _PTHREAD_NO_SIG;
		return(ESUCCESS);
	}
}

int
pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
	int			ret;

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
				rwlock->blocked_writers = 0;
				rwlock->sig = _PTHREAD_RWLOCK_SIG;
				return(ESUCCESS);
			}
		}
	}
}

int
pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
	int			ret;

	if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		if ((ret = pthread_rwlock_init(rwlock, NULL)) != 0)  {
			return(ret);
		}
	}

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG)
		return(EINVAL);
	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0)
		return(ret);

	/* give writers priority over readers */
	while (rwlock->blocked_writers || rwlock->state < 0) {
		ret = pthread_cond_wait(&rwlock->read_signal, &rwlock->lock);

		if (ret != 0) {
			/* can't do a whole lot if this fails */
			pthread_mutex_unlock(&rwlock->lock);
			return(ret);
		}
	}

	/* check lock count */
	if (rwlock->state == MAX_READ_LOCKS)
		ret = EAGAIN;
	else
		++rwlock->state; /* indicate we are locked for reading */

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

	/* check for static initialization */
	if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		if ((ret = pthread_rwlock_init(rwlock, NULL)) != 0)  {
			return(ret);
		}
	}

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG)
		return(EINVAL);
	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0)
		return(ret);

	/* give writers priority over readers */
	if (rwlock->blocked_writers || rwlock->state < 0)
		ret = EBUSY;
	else if (rwlock->state == MAX_READ_LOCKS)
		ret = EAGAIN; /* too many read locks acquired */
	else
		++rwlock->state; /* indicate we are locked for reading */

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&rwlock->lock);

	return(ret);
}

int
pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
	int			ret;

	/* check for static initialization */
	if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		if ((ret = pthread_rwlock_init(rwlock, NULL)) != 0)  {
			return(ret);
		}
	}

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG)
		return(EINVAL);
	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0)
		return(ret);

	if (rwlock->state != 0)
		ret = EBUSY;
	else
		/* indicate we are locked for writing */
		rwlock->state = -1;

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&rwlock->lock);

	return(ret);
}

int
pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
	int			ret;

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG) 
		return(EINVAL);
	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0)
		return(ret);

	if (rwlock->state > 0) {
		if (--rwlock->state == 0 && rwlock->blocked_writers)
			ret = pthread_cond_signal(&rwlock->write_signal);
	} else if (rwlock->state < 0) {
		rwlock->state = 0;

		if (rwlock->blocked_writers)
			ret = pthread_cond_signal(&rwlock->write_signal);
		else
			ret = pthread_cond_broadcast(&rwlock->read_signal);
	} else
		ret = EINVAL;

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&rwlock->lock);

	return(ret);
}

int
pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
	int			ret;

	/* check for static initialization */
	if (rwlock->sig == _PTHREAD_RWLOCK_SIG_init) {
		if ((ret = pthread_rwlock_init(rwlock, NULL)) != 0)  {
			return(ret);
		}
	}

	if (rwlock->sig != _PTHREAD_RWLOCK_SIG)
		return(EINVAL);
	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&rwlock->lock)) != 0)
		return(ret);

	while (rwlock->state != 0) {
		++rwlock->blocked_writers;

		ret = pthread_cond_wait(&rwlock->write_signal, &rwlock->lock);

		if (ret != 0) {
			--rwlock->blocked_writers;
			pthread_mutex_unlock(&rwlock->lock);
			return(ret);
		}

		--rwlock->blocked_writers;
	}

	/* indicate we are locked for writing */
	rwlock->state = -1;

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&rwlock->lock);

	return(ret);
}

int
pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
        attr->sig = _PTHREAD_RWLOCK_ATTR_SIG;
	attr->pshared = PTHREAD_PROCESS_PRIVATE;
        return (ESUCCESS);
}

int       
pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
        attr->sig = _PTHREAD_NO_SIG;  /* Uninitialized */
	attr->pshared = 0;
        return (ESUCCESS);
}

int
pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr,
				int *pshared)
{
        if (attr->sig == _PTHREAD_RWLOCK_ATTR_SIG)
        {
		/* *pshared = (int)attr->pshared; */
                *pshared = (int)PTHREAD_PROCESS_PRIVATE;
                return (ESUCCESS);
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
                if ( pshared == PTHREAD_PROCESS_PRIVATE)
                {
			attr->pshared = pshared ;
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

