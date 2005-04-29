/*
 * 
 * (c) Copyright 1991 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1991 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1991 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
 */
/*
*/
#ifndef PTHREAD_EXC_H
#define PTHREAD_EXC_H
/*
**
**  NAME:
**
**      pthread_exc.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
** 
**  Exception-raising Pthread API header file.
**
**  The exception-raising Pthread API is just like the regular Pthread API
**  except that errors in Pthread functions are indicated by raising exceptions
**  rather than by returning error codes.
** 
**
**  %a%private_end  
** 
*/

/*
 * So that, on Mac OS X, we don't drag in a pile of Mach stuff that
 * defines pointer_t, we define _POSIX_C_SOURCE.
 */
#define _POSIX_C_SOURCE 1
#include <pthread.h>
#include <dce/exc_handling.h>
#include <dce/pthread_np.h>

/* --------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------- */

extern EXCEPTION pthread_badparam_e;            /* Bad parameter */
extern EXCEPTION pthread_existence_e;           /* Object does not exist */
extern EXCEPTION pthread_in_use_e;              /* Object is in use */
extern EXCEPTION pthread_use_error_e;           /* Object inappropriate for operation */
extern EXCEPTION pthread_nostackmem_e;          /* No memory to allocate stack */
extern EXCEPTION pthread_exit_thread_e;         /* Used to terminate a thread */

/* --------------------------------------------------------------------------- */

#ifndef _PTDEXC_NOWRAP_

#define pthread_attr_create(attr) \
    ptdexc_attr_create(attr)

#define pthread_attr_delete(attr) \
    ptdexc_attr_delete(attr)

#define pthread_attr_setprio(attr,priority) \
    ptdexc_attr_setprio(attr,priority)

#define pthread_attr_getprio(attr) \
    ptdexc_attr_getprio(attr)

#define pthread_attr_setsched(attr,scheduler) \
    ptdexc_attr_setsched(attr,scheduler)

#define pthread_attr_getsched(attr) \
    ptdexc_attr_getsched(attr)

#define pthread_attr_setinheritsched(attr,inherit) \
    ptdexc_attr_setinheritsched(attr,inherit)

#define pthread_attr_getinheritsched(attr) \
    ptdexc_attr_getinheritsched(attr)

#define pthread_attr_setstacksize(attr,stacksize) \
    ptdexc_attr_setstacksize(attr,stacksize)

#define pthread_attr_getstacksize(attr) \
    ptdexc_attr_getstacksize(attr)

#define pthread_create(thread,attr,start_routine,arg) \
    ptdexc_create(thread,attr,start_routine,arg)

#define pthread_detach(thread) \
    ptdexc_detach(thread)

#define pthread_join(thread,status) \
    ptdexc_join(thread,status)

#define pthread_setprio(thread,priority) \
    ptdexc_setprio(thread,priority)

#define pthread_getprio(thread) \
    ptdexc_getprio(thread)

#define pthread_mutexattr_init(attr) \
    ptdexc_mutexattr_init(attr)

#define pthread_mutexattr_destroy(attr) \
    ptdexc_mutexattr_destroy(attr)

#define pthread_mutex_init(mutex,attr) \
    ptdexc_mutex_init(mutex,attr)

#define pthread_mutex_destroy(mutex) \
    ptdexc_mutex_destroy(mutex)

#define pthread_mutex_lock(mutex) \
    ptdexc_mutex_lock(mutex)

#define pthread_mutex_trylock(mutex) \
    ptdexc_mutex_trylock(mutex)

#define pthread_mutex_unlock(mutex) \
    ptdexc_mutex_unlock(mutex)

#define pthread_condattr_init(attr) \
    ptdexc_condattr_init(attr)

#define pthread_condattr_destroy(attr) \
    ptdexc_condattr_destroy(attr)

#define pthread_cond_init(cond,attr) \
    ptdexc_cond_init(cond,attr)

#define pthread_cond_destroy(cond) \
    ptdexc_cond_destroy(cond)

#define pthread_cond_broadcast(cond) \
    ptdexc_cond_broadcast(cond)

#define pthread_cond_signal(cond) \
    ptdexc_cond_signal(cond)

#define pthread_cond_timedwait(cond,mutex,abstime) \
    ptdexc_cond_timedwait(cond,mutex,abstime)

#define pthread_once(once_block,init_routine) \
    ptdexc_once(once_block,init_routine)

#define pthread_keycreate(key,destructor) \
    ptdexc_keycreate(key,destructor)

#define pthread_setspecific(key,value) \
    ptdexc_setspecific(key,value)

#define pthread_getspecific(key) \
    ptdexc_getspecific(key)

#define pthread_cancel(thread) \
    ptdexc_cancel(thread)

#ifdef OSF
#ifdef pthread_setcanceltype
#undef pthread_setcanceltype
#endif
#ifdef pthread_setcancelstate
#undef pthread_setcancelstate
#endif
#endif

#define pthread_setcanceltype(type, oldtype) \
    ptdexc_setcanceltype(type, oldtype)

#define pthread_setcancelstate(state, oldstate) \
    ptdexc_setcancelstate(state, oldstate)

#endif

/* --------------------------------------------------------------------------- */

/*
 * Operations to define thread creation attributes
 */
           
/*
 * Create the attribute.
 */
int
ptdexc_attr_create
    (
        pthread_attr_t          *attr
    );

/*
 * Set or obtain the default thread priority.
 */
void
ptdexc_attr_setprio
    (
        pthread_attr_t          *attr,
        int                     priority
    );

int
ptdexc_attr_getprio
    (
        pthread_attr_t          attr
    );

/*
 * Set or obtain the default scheduling algorithm
 */
void
ptdexc_attr_setsched
    (
        pthread_attr_t          *attr,
        int                     scheduler
    );

int
ptdexc_attr_getsched
    (
        pthread_attr_t          attr
    );

/*
 * Set or obtain whether a thread will use the default scheduling attributes,
 * or inherit them from the creating thread.
 */
void
ptdexc_attr_setinheritsched
    (
        pthread_attr_t          *attr,
        int                     inherit
    );

int
ptdexc_attr_getinheritsched
    (
        pthread_attr_t          attr
    );

/*
 * Set or obtain the default stack size
 */
void
ptdexc_attr_setstacksize
    (
        pthread_attr_t          *attr,
        long                    stacksize
    );

long
ptdexc_attr_getstacksize
    (
        pthread_attr_t          attr
    );

/*
 * The following procedures can be used to control thread creation,
 * termination and deletion.
 */

/*
 * To create a thread object and runnable thread, a routine must be specified
 * as the new thread's start routine.  An argument may be passed to this
 * routine, as an untyped address; an untyped address may also be returned as
 * the routine's value.  An attributes object may be used to specify details
 * about the kind of thread being created.
 */
void
ptdexc_create
    (
        pthread_t               *thread,
        pthread_attr_t          *attr,
        void                    *(*start_routine)(void *srarg),
        void                    *arg
    );

/*
 * A thread object may be "detached" to specify that the return value and
 * completion status will not be requested.
 */
void
ptdexc_detach
    (
        pthread_t               *thread
    );

/* 
 * A thread can await termination of another thread and retrieve the return
 * value of the thread.
 */
void
ptdexc_join
    (
        pthread_t               thread,
        void                    **status
    );

/*
 * Thread Scheduling Operations
 */

/*
 * The current user_assigned priority of a thread can be changed.
 */
int
ptdexc_setprio
    (
        pthread_t               thread,
        int                     priority
    );

/*
 * Thread Information Operations
 */

/*
 * The current user_assigned priority of a thread can be read.
 */
int
ptdexc_getprio
    (
        pthread_t               thread
    );

/*
 * Operations on Mutexes
 */

void
ptdexc_mutexattr_create
    (
        pthread_mutexattr_t     *attr
    );

void
ptdexc_mutexattr_delete
    (
        pthread_mutexattr_t     *attr
    );

/* 
 * The following routines create, delete, lock and unlock mutexes.
 */
void
ptdexc_mutex_init
    (
        pthread_mutex_t         *mutex,
        pthread_mutexattr_t     *attr
    );

void
ptdexc_mutex_destroy
    (
        pthread_mutex_t         *mutex
    );

void
ptdexc_mutex_lock
    (
        pthread_mutex_t         *mutex
    );

int
ptdexc_mutex_trylock
    (
        pthread_mutex_t         *mutex
    );

void
ptdexc_mutex_unlock
    (
        pthread_mutex_t         *mutex
    );

/*
 * Operations on condition variables
 */

void
ptdexc_condattr_create
    (
        pthread_condattr_t      *attr
    );

void
ptdexc_condattr_delete
    (
        pthread_condattr_t      *attr
    );

/*
 * A thread can create and delete condition variables.
 */
void
ptdexc_cond_init
    (
        pthread_cond_t          *cond,
        pthread_condattr_t      *attr
    );

void
ptdexc_cond_destroy
    (
        pthread_cond_t          *cond
    );

/*
 * A thread can signal to and broadcast on a condition variable.
 */
void
ptdexc_cond_broadcast
    (
        pthread_cond_t          *cond
    );

void
ptdexc_cond_signal
    (
        pthread_cond_t          *cond
    );

/*
 * A thread can wait for a condition variable to be signalled or broadcast.
 */
void
ptdexc_cond_wait
    (
        pthread_cond_t          *cond,
        pthread_mutex_t         *mutex
    );

/*
 * Operations for timed waiting
 */

/*
 * A thread can perform a timed wait on a condition variable.
 */
int
ptdexc_cond_timedwait
    (
        pthread_cond_t          *cond,
        pthread_mutex_t         *mutex,
        struct timespec         *abstime
    );

/*
 * Operations for client initialization.
 */

void
ptdexc_once
    (
        pthread_once_t          *once_block,
        void (*init_routine)()
    );

/*
 * Operations for per-thread context
 */

/*
 * A unique per-thread context key can be obtained for the process
 */
void
ptdexc_keycreate
    (
        pthread_key_t           *key,
        void                    (*destructor)(void *value)
    );

/*
 * A thread can set a per-thread context value identified by a key.
 */
void
ptdexc_setspecific
    (
        pthread_key_t           key,
        void                    *value
    );

/*
 * A thread can retrieve a per-thread context value identified by a key.
 */
void *
ptdexc_getspecific
    (
        pthread_key_t           key
    );

/*
 * Operations for alerts.
 */

/*
 * The current thread can request that a thread terminate it's execution.
 */

void
ptdexc_cancel
    (
        pthread_t               thread
    );

/*
 * The current thread can enable or disable alert delivery (PTHREAD
 * "cancels"); it can control "general cancelability" (CMA "defer") or
 * just "asynchronous cancelability" (CMA "asynch disable").
 */
int
ptdexc_setcanceltype
    (
        int                     state,
        int                     *oldstate
    );

int
ptdexc_setcancelstate
    (
        int                     state,
        int                     *oldstate
    );

#endif
