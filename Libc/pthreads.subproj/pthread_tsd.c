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
 *   Thread Specific Data support
 */

#include "pthread_internals.h"

static struct
{
	int  created;    /* Set TRUE if 'create_key' used this slot */
	void (*destructor)(void *);
} _pthread_keys[_POSIX_THREAD_KEYS_MAX];
static pthread_lock_t tds_lock = LOCK_INITIALIZER;

/*
 * Create a new key for thread specific data
 */
int       
pthread_key_create(pthread_key_t *key,
		   void (*destructor)(void *))
{
	int res, i;
	LOCK(tds_lock);
	res = ENOMEM;  /* No 'free' keys */
	for (i = 0;  i < _POSIX_THREAD_KEYS_MAX;  i++)
	{
		if (_pthread_keys[i].created == FALSE)
		{
			_pthread_keys[i].created = TRUE;
			_pthread_keys[i].destructor = destructor;
			*key = i+1;
			res = ESUCCESS;
			break;
		}
	}
	UNLOCK(tds_lock);
        return (res);
}

/*
 * Destroy a thread specific data key
 */
int       
pthread_key_delete(pthread_key_t key)
{
	int res;
	LOCK(tds_lock);
	if ((key >= 1) && (key <= _POSIX_THREAD_KEYS_MAX))
	{
		if (_pthread_keys[key-1].created)
		{
			_pthread_keys[key-1].created = FALSE;
			res = ESUCCESS;
		} else
		{
			res = EINVAL;
		}
	} else
	{ /* Invalid key */
		res = EINVAL;
	}
	UNLOCK(tds_lock);
	return (res);
}

/*
 * Set the thread private value for a given key.
 * We do not take the spinlock for this or pthread_getspecific.
 * The assignment to self->tsd[] is thread-safe because we never
 * refer to the tsd[] of a thread other than pthread_self().
 * The reference to _pthread_keys[...].created could race with a
 * pthread_key_delete() but in this case the behaviour is allowed
 * to be undefined.
 */
int       
pthread_setspecific(pthread_key_t key,
		    const void *value)
{
	int res;
	pthread_t self;
	if ((key >= 1) && (key <= _POSIX_THREAD_KEYS_MAX))
	{
		if (_pthread_keys[key-1].created)
		{
			self = pthread_self();
			self->tsd[key-1] = (void *) value;
			res = ESUCCESS;
		} else
		{
			res = EINVAL;
		}
	} else
	{ /* Invalid key */
		res = EINVAL;
	}
        return (res);
}

/*
 * Fetch the thread private value for a given key.
 * This is potentially a very heavily-used operation so we do only
 * a minimum of checks.
 */
void *
pthread_getspecific(pthread_key_t key)
{
	pthread_t self;
	void *res;
        if ((key >= 1) && (key <= _POSIX_THREAD_KEYS_MAX))
	{
		self = pthread_self();
		res = self->tsd[key-1];
	} else
	{ /* Invalid key - no error, just NULL */
		res = (void *)NULL;
	}
	return (res);
}

/*
 * Clean up thread specific data as thread 'dies'
 */
void
_pthread_tsd_cleanup(pthread_t self)
{
	int i, j;
	void *param;
	for (j = 0;  j < PTHREAD_DESTRUCTOR_ITERATIONS;  j++)
	{
		for (i = 0;  i < _POSIX_THREAD_KEYS_MAX;  i++)
		{
			if (_pthread_keys[i].created && (param = self->tsd[i]))
			{
				self->tsd[i] = (void *)NULL;
				if (_pthread_keys[i].destructor)
				{
					(_pthread_keys[i].destructor)(param);
				}
			}
		}
	}
}
