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

#include "plockstat.h"

extern int __unix_conforming;

#ifndef BUILDING_VARIANT /* [ */

#define BLOCK_FAIL_PLOCKSTAT    0
#define BLOCK_SUCCESS_PLOCKSTAT 1

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
		if (mutex->owner == (pthread_t)NULL &&
		    mutex->busy == (pthread_cond_t *)NULL)
		{
			mutex->sig = _PTHREAD_NO_SIG;
			res = 0;
		}
		else
			res = EBUSY;
	} else if (mutex->sig  == _PTHREAD_KERN_MUTEX_SIG) {
				int mutexid = mutex->_pthread_mutex_kernid;
				UNLOCK(mutex->lock);
				if( __pthread_mutex_destroy(mutexid) == -1)
					return(errno);
				mutex->sig = _PTHREAD_NO_SIG;	
				return(0);
	} else 
		res = EINVAL;
	UNLOCK(mutex->lock);
	return (res);
}

#ifdef PR_5243343
/* 5243343 - temporary hack to detect if we are running the conformance test */
extern int PR_5243343_flag;
#endif /* PR_5243343 */
/*
 * Initialize a mutex variable, possibly with additional attributes.
 */
static int
_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	if (attr)
	{
		if (attr->sig != _PTHREAD_MUTEX_ATTR_SIG)
			return (EINVAL);
		mutex->prioceiling = attr->prioceiling;
		mutex->protocol = attr->protocol;
		mutex->type = attr->type;
		mutex->pshared = attr->pshared;
		if (attr->pshared == PTHREAD_PROCESS_SHARED) {
			mutex->lock_count = 0;
			mutex->owner = (pthread_t)NULL;
			mutex->next = (pthread_mutex_t *)NULL;
			mutex->prev = (pthread_mutex_t *)NULL;
			mutex->busy = (pthread_cond_t *)NULL;
			mutex->waiters = 0;
			mutex->sem = SEMAPHORE_NULL;
			mutex->order = SEMAPHORE_NULL;
			mutex->sig = 0;
			if( __pthread_mutex_init(mutex, attr) == -1)
				return(errno);
			mutex->sig = _PTHREAD_KERN_MUTEX_SIG;
			return(0);
		}
	} else {
		mutex->prioceiling = _PTHREAD_DEFAULT_PRIOCEILING;
		mutex->protocol = _PTHREAD_DEFAULT_PROTOCOL;
		mutex->type = PTHREAD_MUTEX_DEFAULT;
		mutex->pshared = _PTHREAD_DEFAULT_PSHARED;
	}
	mutex->lock_count = 0;
	mutex->owner = (pthread_t)NULL;
	mutex->next = (pthread_mutex_t *)NULL;
	mutex->prev = (pthread_mutex_t *)NULL;
	mutex->busy = (pthread_cond_t *)NULL;
	mutex->waiters = 0;
	mutex->sem = SEMAPHORE_NULL;
	mutex->order = SEMAPHORE_NULL;
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
 * Manage a list of mutex variables owned by a thread
 */
#if defined(DEBUG)
static void
_pthread_mutex_add(pthread_mutex_t *mutex, pthread_t self)
{
        pthread_mutex_t *m;
	if (self != (pthread_t)0)
	{
		if ((m = self->mutexes) != (pthread_mutex_t *)NULL)
                { /* Add to list */
			m->prev = mutex;
                }
		mutex->next = m;
		mutex->prev = (pthread_mutex_t *)NULL;
		self->mutexes = mutex;
	}
}

__private_extern__ void
_pthread_mutex_remove(pthread_mutex_t *mutex, pthread_t self)
{
        pthread_mutex_t *n, *prev;
        if ((n = mutex->next) != (pthread_mutex_t *)NULL)
        {
                n->prev = mutex->prev;
        }
        if ((prev = mutex->prev) != (pthread_mutex_t *)NULL)
        {
                prev->next = mutex->next;
        } else
        { /* This is the first in the list */
		if (self != (pthread_t)0) {
			self->mutexes = n;
		}
        }
}
#endif

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
	if ((sig != _PTHREAD_MUTEX_SIG) && (sig != _PTHREAD_MUTEX_SIG_init) && (sig != _PTHREAD_KERN_MUTEX_SIG)) {
		PLOCKSTAT_MUTEX_ERROR(mutex, EINVAL);
		return(EINVAL);
	}
	LOCK(mutex->lock);
	if (mutex->sig != _PTHREAD_MUTEX_SIG)
	{
		if (mutex->sig != _PTHREAD_MUTEX_SIG_init)
		{
			if (mutex->sig  == _PTHREAD_KERN_MUTEX_SIG) {
				int mutexid = mutex->_pthread_mutex_kernid;
				UNLOCK(mutex->lock);

				PLOCKSTAT_MUTEX_BLOCK(mutex);
				if( __pthread_mutex_lock(mutexid) == -1) {
					PLOCKSTAT_MUTEX_BLOCKED(mutex, BLOCK_FAIL_PLOCKSTAT);
					PLOCKSTAT_MUTEX_ERROR(mutex, errno);
					return(errno);
				}

				PLOCKSTAT_MUTEX_BLOCKED(mutex, BLOCK_SUCCESS_PLOCKSTAT);
				PLOCKSTAT_MUTEX_ACQUIRE(mutex, 0, 0);
				return(0);
			} else { 
				UNLOCK(mutex->lock);
				PLOCKSTAT_MUTEX_ERROR(mutex, EINVAL);
				return (EINVAL);
			}
		}
		_pthread_mutex_init(mutex, NULL);
		self = _PTHREAD_MUTEX_OWNER_SELF;
	} 
	else if (mutex->type != PTHREAD_MUTEX_NORMAL)
	{
		self = pthread_self();
		if (mutex->owner == self)
		{
			int res;

			if (mutex->type == PTHREAD_MUTEX_RECURSIVE)
			{
				if (mutex->lock_count < USHRT_MAX)
				{
					mutex->lock_count++;
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

	mutex->lock_count = 1;
	mutex->owner = self;
#if defined(DEBUG)
	_pthread_mutex_add(mutex, self);
#endif
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

			if (mutex->sig  == _PTHREAD_KERN_MUTEX_SIG) {
				int mutexid = mutex->_pthread_mutex_kernid;
				UNLOCK(mutex->lock);
				if( __pthread_mutex_trylock(mutexid) == -1) {
					PLOCKSTAT_MUTEX_ERROR(mutex, errno);
					return(errno);
				}
				PLOCKSTAT_MUTEX_ACQUIRE(mutex, 0, 0);
				return(0);
			} else { 
				PLOCKSTAT_MUTEX_ERROR(mutex, EINVAL);
				UNLOCK(mutex->lock);
				return (EINVAL);
			}
		}
		_pthread_mutex_init(mutex, NULL);
		self = _PTHREAD_MUTEX_OWNER_SELF;
	}
	else if (mutex->type != PTHREAD_MUTEX_NORMAL)
	{
		self = pthread_self();
		if (mutex->type == PTHREAD_MUTEX_RECURSIVE)
		{
			if (mutex->owner == self)
			{
				int res;

				if (mutex->lock_count < USHRT_MAX)
				{
					mutex->lock_count++;
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

	mutex->lock_count = 1;
	mutex->owner = self;
#if defined(DEBUG)
	_pthread_mutex_add(mutex, self);
#endif
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
	
	if ((sig != _PTHREAD_MUTEX_SIG) && (sig != _PTHREAD_MUTEX_SIG_init) && (sig != _PTHREAD_KERN_MUTEX_SIG)) {
		PLOCKSTAT_MUTEX_ERROR(mutex, EINVAL);
		return(EINVAL);
	}
	LOCK(mutex->lock);
	if (mutex->sig != _PTHREAD_MUTEX_SIG)
	{
		if (mutex->sig != _PTHREAD_MUTEX_SIG_init)
		{
			if (mutex->sig  == _PTHREAD_KERN_MUTEX_SIG) {
				int mutexid = mutex->_pthread_mutex_kernid;
				UNLOCK(mutex->lock);
				if( __pthread_mutex_unlock(mutexid) == -1) {
					PLOCKSTAT_MUTEX_ERROR(mutex, errno);
					return(errno);
				}
				PLOCKSTAT_MUTEX_RELEASE(mutex, 0);
				return(0);
			} else { 
				PLOCKSTAT_MUTEX_ERROR(mutex, EINVAL);
				UNLOCK(mutex->lock);
				return (EINVAL);
			}
		}
		_pthread_mutex_init(mutex, NULL);
	} else

#if !defined(DEBUG)
	if (mutex->type != PTHREAD_MUTEX_NORMAL)
#endif
	{
		pthread_t self = pthread_self();
		if (mutex->owner != self)
		{
#if defined(DEBUG)
			abort();
#endif
			PLOCKSTAT_MUTEX_ERROR(mutex, EPERM);
			UNLOCK(mutex->lock);
			return EPERM;
		} else if (mutex->type == PTHREAD_MUTEX_RECURSIVE &&
		    --mutex->lock_count)
		{
			PLOCKSTAT_MUTEX_RELEASE(mutex, 1);
			UNLOCK(mutex->lock);
			return(0);
		}
	}

	mutex->lock_count = 0;
#if defined(DEBUG)
	_pthread_mutex_remove(mutex, mutex->owner);
#endif /* DEBUG */

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


