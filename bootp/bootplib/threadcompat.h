/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef MOSX
#import <mach/cthreads.h>
#define MUTEX_STRUCT		struct mutex
#define CONDITION_STRUCT	struct condition
#define UTHREAD_T		cthread_t
#define UTHREAD_FUNC_T		cthread_fn_t

static __inline__ int
UTHREAD_CREATE(cthread_t * thread, cthread_fn_t func, void * arg)
{
    *thread = cthread_fork(func, arg);
    if (*thread == NULL)
	return (-1);
    return 0;
}

static __inline__ int
UTHREAD_JOIN(cthread_t thread, void * * result)
{
    void * res;
    res = (void *)cthread_join(thread);
    if (result)
	*result = res;
    return (0);
}

#define UTHREAD_EXIT		cthread_exit

#else MOSX
#import <pthread.h>

typedef void * (*pthread_fn_t)(void *);

#define MUTEX_STRUCT		pthread_mutex_t
#define CONDITION_STRUCT	pthread_cond_t
#define UTHREAD_T		pthread_t
#define UTHREAD_FUNC_T		pthread_fn_t

static __inline__ int
UTHREAD_CREATE(pthread_t * thread, pthread_fn_t func, void * arg)
{
    return (pthread_create(thread, NULL, func, arg));
}

static __inline__ int
UTHREAD_JOIN(pthread_t thread, void * * result_p)
{
    int 	r;
    void * 	result;

    r = pthread_join(thread, &result);
    if (result_p)
	*result_p = result;
    return (r);
}

#define UTHREAD_EXIT		pthread_exit

#define mutex_init(m)		pthread_mutex_init(m, NULL)
#define mutex_clear(m)		pthread_mutex_destroy(m)
#define mutex_lock(m)		pthread_mutex_lock(m)
#define mutex_unlock(m)		pthread_mutex_unlock(m)

#define condition_init(c)	pthread_cond_init(c, NULL)
#define condition_clear(c)	pthread_cond_destroy(c)
#define condition_wait(c, m)	pthread_cond_wait(c, m)
#define condition_signal(c)	pthread_cond_signal(c)
#define condition_broadcast(c)	pthread_cond_broadcast(c)
#endif MOSX
