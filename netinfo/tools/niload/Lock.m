/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Lock.m
 *
 * A lock abstraction that uses either pthreads or cthreads.
 *
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 * Written by Marc Majka
 */

#import "Lock.h"
#ifdef _THREAD_TYPE_PTHREAD_
#import <stdlib.h>
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

@implementation Lock

- (void)_internal_lock
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_lock(internal);
#else
#ifdef USE_INTERNAL_SPIN_LOCK
	spin_lock(&internal);
#else
	mutex_lock(internal);
#endif
#endif
}

- (void)_internal_unlock
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_unlock(internal);
#else
#ifdef USE_INTERNAL_SPIN_LOCK
	spin_unlock(&internal);
#else
	mutex_unlock(internal);
#endif
#endif
}

- (void)_main_lock
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_lock(mutex);
#else
	mutex_lock(mutex);
#endif
}

- (void)_main_unlock
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_unlock(mutex);
#else
	mutex_unlock(mutex);
#endif
}

- (Lock *)initRecursive:(BOOL)yn
{
	[super init];

#ifdef _THREAD_TYPE_PTHREAD_
	mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutex, NULL);
	internal = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(internal, NULL);
#else
	mutex = mutex_alloc();
	mutex_init(mutex);
#ifdef USE_INTERNAL_SPIN_LOCK
	internal = 0;
#else
	internal = mutex_alloc();
	mutex_init(internal);
#endif
#endif

	thread = NO_THREAD;
	recursive = yn;
	return self;
}

- (id)init
{
	return [self initRecursive:NO];
}

- (Lock *)initThreadLock
{
	return [self initRecursive:YES];
}

- free
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_destroy(mutex);
	free(mutex);
	pthread_mutex_destroy(internal);
	free(internal);
#else
	mutex_free(mutex);
#ifndef USE_INTERNAL_SPIN_LOCK
	mutex_free(internal);
#endif
#endif
	return [super free];
}

- (void)lock
{
	thread_id_t t;

	t = _thread_id();

	[self _internal_lock];
	if (locked && recursive && (thread == t))
	{
		[self _internal_unlock];
		return;
	}
	[self _internal_unlock];

	[self _main_lock];

	[self _internal_lock];
	locked = YES;
	thread = _thread_id();
	[self _internal_unlock];
}

- (void)unlock
{
	[self _internal_lock];
	if (!locked)
	{
		[self _internal_unlock];
		return;
	}

	locked = NO;
	thread = NO_THREAD;
	[self _internal_unlock];

	[self _main_unlock];

}

- (BOOL)tryLock
{
	int t;

	[self _internal_lock];
	if (locked && recursive && (thread == _thread_id()))
	{
		[self _internal_unlock];
		return YES;
	}
	[self _internal_unlock];

#ifdef _THREAD_TYPE_PTHREAD_
	t = pthread_mutex_trylock(mutex);
#else
	t = mutex_try_lock(mutex);
#endif

	if (t != 0) return NO;

	[self _internal_lock];
	locked = YES;
	thread = _thread_id();
	[self _internal_unlock];

	return YES;
}

- (BOOL)isLocked
{
	BOOL ret;

	[self _internal_lock];
	ret = locked;
	[self _internal_unlock];

	return ret;
}

@end
