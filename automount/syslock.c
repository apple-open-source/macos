/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

#include "syslock.h"
#include <string.h>
#ifdef _THREAD_TYPE_PTHREAD_
#include <stdlib.h>
#endif

static thread_id_t
_thread_id(void)
{
#ifdef _THREAD_TYPE_PTHREAD_
	return (unsigned int)pthread_self();
#else
	return (unsigned int)cthread_self();
#endif
}

static void
_internal_lock(syslock *s)
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_lock(s->internal);
#else
	spin_lock(&(s->internal));
#endif
}

static void
_internal_unlock(syslock *s)
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_unlock(s->internal);
#else
	spin_unlock(&(s->internal));
#endif
}

static void
_main_lock(syslock *s)
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_lock(s->mutex);
#else
	mutex_lock(s->mutex);
#endif
}

static void
_main_unlock(syslock *s)
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_unlock(s->mutex);
#else
	mutex_unlock(s->mutex);
#endif
}

syslock *
syslock_new(bool_t recursive)
{
	syslock *s;
	
	s = (syslock *)malloc(sizeof(syslock));
	memset(s, 0, sizeof(syslock));

#ifdef _THREAD_TYPE_PTHREAD_
	s->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(s->mutex, NULL);
	s->internal = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(s->internal, NULL);

	s->condition = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	pthread_cond_init(s->condition, NULL);
#else
	s->mutex = mutex_alloc();
	mutex_init(s->mutex);
	s->internal = 0;

	s->condition = condition_alloc();
	condition_init(s->condition);
#endif

	s->thread = NO_THREAD;
	s->recursive = recursive;
	return s;
}

void
syslock_free(syslock *s)
{
	if (s == NULL) return;
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_destroy(s->mutex);
	pthread_mutex_destroy(s->internal);
	pthread_cond_destroy(s->condition);
#else
	mutex_free(s->mutex);
	condition_free(s->condition);
#endif
}

void
syslock_lock(syslock *s)
{
	thread_id_t t;

	if (s == NULL) return;

	t = _thread_id();

	_internal_lock(s);
	if (s->locked && s->recursive && (s->thread == t))
	{
		_internal_unlock(s);
		return;
	}
	_internal_unlock(s);

	_main_lock(s);

	_internal_lock(s);
	s->locked = TRUE;
	s->thread = _thread_id();
	_internal_unlock(s);
}

void
syslock_unlock(syslock *s)
{
	if (s == NULL) return;

	_internal_lock(s);
	s->locked = FALSE;
	s->thread = NO_THREAD;
	_internal_unlock(s);

	_main_unlock(s);
}

bool_t
syslock_trylock(syslock *s)
{
	int t;

	if (s == NULL) return FALSE;

	_internal_lock(s);
	if (s->locked && s->recursive && (s->thread == _thread_id()))
	{
		_internal_unlock(s);
		return TRUE;
	}
	_internal_unlock(s);

#ifdef _THREAD_TYPE_PTHREAD_
	t = pthread_mutex_trylock(s->mutex);
#else
	t = mutex_try_lock(s->mutex);
#endif

	if (t != 0) return FALSE;

	_internal_lock(s);
	s->locked = TRUE;
	s->thread = _thread_id();
	_internal_unlock(s);

	return TRUE;
}

bool_t
syslock_is_locked(syslock *s)
{
	bool_t ret;

	_internal_lock(s);
	ret = s->locked;
	_internal_unlock(s);

	return ret;
}

void
syslock_signal_wait(syslock *s)
{
	if (s == NULL) return;
	
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_cond_wait(s->condition, s->mutex);
#else
	condition_wait(s->condition, s->mutex);
#endif
}

void
syslock_signal_send(syslock *s)
{
	if (s == NULL) return;

#ifdef _THREAD_TYPE_PTHREAD_
	pthread_cond_signal(s->condition);
#else
	condition_signal(s->condition);
#endif
}

void
syslock_signal_broadcast(syslock *s)
{
	if (s == NULL) return;

#ifdef _THREAD_TYPE_PTHREAD_
	pthread_cond_broadcast(s->condition);
#else
	condition_broadcast(s->condition);
#endif
}
