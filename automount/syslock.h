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

#ifndef __SYSLOCK_H__
#define __SYSLOCK_H__

#include "config.h"
#include <rpc/types.h>
#ifdef _THREAD_TYPE_PTHREAD_
#include <pthread.h>
#else
#ifndef	_CTHREADS_
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#include <mach/cthreads.h>
#endif
#endif

#define thread_id_t unsigned int
#define NO_THREAD (thread_id_t)-1

typedef struct
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_t *internal;
	pthread_mutex_t *mutex;
	pthread_cond_t *condition;
#else
	int internal;
	mutex_t mutex;
	condition_t condition;
#endif
	thread_id_t thread;
	bool_t recursive;
	bool_t locked;
} syslock;

syslock *syslock_new(bool_t recursive);
void syslock_free(syslock *s);
void syslock_lock(syslock *s);
void syslock_unlock(syslock *s);
bool_t syslock_trylock(syslock *s);
bool_t syslock_is_locked(syslock *s);

void syslock_signal_wait(syslock *s);
void syslock_signal_send(syslock *s);
void syslock_signal_broadcast(syslock *s);

#endif __SYSLOCK_H__
