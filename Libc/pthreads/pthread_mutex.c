/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
			res = ESUCCESS;
		}
		else
			res = EBUSY;
	}
	else
		res = EINVAL;
	UNLOCK(mutex->lock);
	return (res);
}

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
	} else {
		mutex->prioceiling = _PTHREAD_DEFAULT_PRIOCEILING;
		mutex->protocol = _PTHREAD_DEFAULT_PROTOCOL;
		mutex->type = PTHREAD_MUTEX_DEFAULT;
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
	return (ESUCCESS);
}

/*
 * Initialize a mutex variable, possibly with additional attributes.
 * Public interface - so don't trust the lock - initialize it first.
 */
int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
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
	if ((sig != _PTHREAD_MUTEX_SIG) && (sig != _PTHREAD_MUTEX_SIG_init))
		return(EINVAL);
	LOCK(mutex->lock);
	if (mutex->sig != _PTHREAD_MUTEX_SIG)
	{
		if (mutex->sig != _PTHREAD_MUTEX_SIG_init)
		{
			UNLOCK(mutex->lock);
			return (EINVAL);
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
					res = ESUCCESS;
				} else
					res = EAGAIN;
			} else	/* PTHREAD_MUTEX_ERRORCHECK */
				res = EDEADLK;
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

			PTHREAD_MACH_CALL(semaphore_wait_signal(sem, order), kern_res);
			while (kern_res == KERN_ABORTED)
			{
				PTHREAD_MACH_CALL(semaphore_wait(sem), kern_res);
			} 

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
	return (ESUCCESS);
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
			UNLOCK(mutex->lock);
			return (EINVAL);
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
					res = ESUCCESS;
				} else
					res = EAGAIN;
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
	return (ESUCCESS);
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
	
	if ((sig != _PTHREAD_MUTEX_SIG) && (sig != _PTHREAD_MUTEX_SIG_init))
		return(EINVAL);
	LOCK(mutex->lock);
	if (mutex->sig != _PTHREAD_MUTEX_SIG)
	{
		if (mutex->sig != _PTHREAD_MUTEX_SIG_init)
		{
			UNLOCK(mutex->lock);
			return (EINVAL);        /* Not a mutex variable */
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
			UNLOCK(mutex->lock);
			return EPERM;
		} else if (mutex->type == PTHREAD_MUTEX_RECURSIVE &&
		    --mutex->lock_count)
		{
			UNLOCK(mutex->lock);
			return ESUCCESS;
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
		UNLOCK(mutex->lock);
		PTHREAD_MACH_CALL(semaphore_signal(mutex->sem), kern_res);
	}
	else
	{
		mutex->owner = (pthread_t)NULL;
		UNLOCK(mutex->lock);
	}
	return (ESUCCESS);
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
                res = ESUCCESS;
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
                        res = ESUCCESS;
                } else
                        res = EINVAL; /* Invalid parameter */
        } else
                res = EINVAL; /* Not an initialized 'attribute' structure */
	UNLOCK(mutex->lock);
	return (res);
}

/*
 * Destroy a mutex attribute structure.
 */
int
pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
        attr->sig = _PTHREAD_NO_SIG;  /* Uninitialized */
        return (ESUCCESS);
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
                return (ESUCCESS);
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
                return (ESUCCESS);
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
                return (ESUCCESS);
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
                *pshared = (int)PTHREAD_PROCESS_PRIVATE;
                return (ESUCCESS);
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
        return (ESUCCESS);
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

/*
 *
 */
int
pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
        if (attr->sig == _PTHREAD_MUTEX_ATTR_SIG)
        {
                if (pshared == PTHREAD_PROCESS_PRIVATE)
                {
                        /* attr->pshared = protocol; */
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

