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
 * Lock.h
 *
 * Simple and thread (recursive) locks.
 *
 * Copyright (c) 1999, Apple Computer Inc.  All rights reserved.
 * Written by Marc Majka
 */

#import "Root.h"
#ifdef _THREAD_TYPE_PTHREAD_
#include <pthread.h>
#else
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#include <mach/cthreads.h>
#endif

#define USE_INTERNAL_SPIN_LOCK

#define thread_id_t unsigned int
#define NO_THREAD (thread_id_t)-1

@interface Lock : Root
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_t *internal;
	pthread_mutex_t *mutex;
#else
#ifdef USE_INTERNAL_SPIN_LOCK
	int internal;
#else
	mutex_t internal;
#endif
	mutex_t mutex;
#endif
	thread_id_t thread;
	BOOL recursive;
	BOOL locked;
}

- (Lock *)initRecursive:(BOOL)yn;
- (Lock *)initThreadLock;
- (void)lock;
- (void)unlock;
- (BOOL)tryLock;
- (BOOL)isLocked;

@end
