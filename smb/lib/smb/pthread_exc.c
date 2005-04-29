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
**
**  NAME:
**
**      pthread_exc.c
**
**  FACILITY:
**
**      pthreads
**
**  ABSTRACT:
**
**  Pthread based exception package support routines.
** 
**  The exception-raising Pthread API is just like the regular Pthread API
**  except that errors in Pthread functions are indicated by raising exceptions
**  rather than by returning error codes.
** 
**  
**  %a%private_end  
**
*/

#include <errno.h>
#include <pthread.h>

#define _PTDEXC_NOWRAP_
#include <dce/pthread_exc.h>

/* -------------------------------------------------------------------- */

EXCEPTION pthread_badparam_e;           /* Bad parameter */
EXCEPTION pthread_existence_e;          /* Object does not exist */
EXCEPTION pthread_in_use_e;             /* Object is in use */
EXCEPTION pthread_use_error_e;          /* Object inappropriate for operation */
EXCEPTION pthread_nostackmem_e;         /* No memory to allocate stack */
EXCEPTION pthread_exit_thread_e;        /* Used to terminate a thread */

/* -------------------------------------------------------------------- */
/*
 * Make sure NULL is defined for exc_once_block
 */
#ifndef NULL
#define NULL 0
#endif

static pthread_once_t exc_once_block = PTHREAD_ONCE_INIT; 

/* -------------------------------------------------------------------- */
#ifdef apollo
/*
 * For apollos, tell the compiler to place all static data, declared
 * within the scope of a function, in a section named exc_pure_data$.
 * This section will be loaded as a R/O, shared, initialized data section.
 * All other data, global or statics at the file scope, will be loaded
 * as R/W, per-process, and zero-filled.
 */

#pragma HP_SECTION( , exc_pure_data$)
#endif
/* -------------------------------------------------------------------- */

static void raise_error
    (
        int r
    );

#define RETURN(_r) \
{ \
    if ((_r) == 0) \
        return (_r); \
    else { \
        raise_error(r); \
        return 0; \
    } \
}

#define RETURN2(_r, _v) \
{ \
    if ((_r) == 0) \
        return (_v); \
    else { \
        raise_error(r); \
        return 0; \
    } \
}

#define VRETURN(_r) \
{ \
    if ((_r) == 0) \
        return; \
    else \
        raise_error(r); \
}

static void exc_init_once()
{
    EXCEPTION_INIT(pthread_badparam_e);
    EXCEPTION_INIT(pthread_existence_e);
    EXCEPTION_INIT(pthread_in_use_e);
    EXCEPTION_INIT(pthread_use_error_e);
    EXCEPTION_INIT(pthread_nostackmem_e);
    EXCEPTION_INIT(pthread_exit_thread_e);
}

static void raise_error(r)
int r;
{
    EXCEPTION e;
       
    pthread_once (&exc_once_block, exc_init_once);

    switch (r)
    {
        case ENOMEM:    e = exc_insfmem_e;          break;
        case EINVAL:    e = pthread_use_error_e;    break;
        default:        e = pthread_use_error_e;    break;
    }

    RAISE(e);
}

/* -------------------------------------------------------------------- */

void
ptdexc_attr_setprio(attr, priority)
pthread_attr_t  *attr;
int             priority;
{
    int r;
    struct sched_param scd;

    scd.sched_priority = priority;
    r = pthread_attr_setschedparam(attr, &scd);
    VRETURN(r);
}


int
ptdexc_attr_getprio(attr)
pthread_attr_t  attr;
{
    int r;
    struct sched_param scd;
    
    r = pthread_attr_getschedparam(&attr, &scd);
    RETURN2(r, scd.sched_priority);
}


void
ptdexc_attr_setsched(attr, scheduler)
pthread_attr_t  *attr;
int             scheduler;
{
    int r = pthread_attr_setschedpolicy(attr, scheduler);
    VRETURN(r);
}


int
ptdexc_attr_getsched(attr)
pthread_attr_t  attr;
{
    int r, scheduler;
    r = pthread_attr_getschedpolicy(&attr, &scheduler);
    RETURN2(r, scheduler);
}


void
ptdexc_attr_setinheritsched(attr, inherit)
pthread_attr_t  *attr;
int             inherit;
{
    int r = pthread_attr_setinheritsched(attr, inherit);
    VRETURN(r);
}


int
ptdexc_attr_getinheritsched(attr)
pthread_attr_t          attr;
{
    int r, inheritsched;
    r = pthread_attr_getinheritsched(&attr, &inheritsched);
    RETURN2(r, inheritsched);
}


void
ptdexc_attr_setstacksize(attr, stacksize)
pthread_attr_t  *attr;
long            stacksize;
{
    int r = pthread_attr_setstacksize(attr, stacksize);
    VRETURN(r);
}


long
ptdexc_attr_getstacksize(attr)
pthread_attr_t  attr;
{
    int r;
    size_t stacksize;
    r = pthread_attr_getstacksize(&attr, &stacksize);
    RETURN2(r, stacksize);
}

int
ptdexc_attr_create(attr)
pthread_attr_t  *attr;
{
    pthread_attr_init(attr);
    return(0);
}

void
ptdexc_create(thread, attr, start_routine, arg)
pthread_t               *thread;
pthread_attr_t          *attr;
void                    *(*start_routine)(void *srarg);
void                    *arg;
{
    int r = pthread_create(thread, attr, start_routine, arg);
    VRETURN(r);
}


void
ptdexc_detach(thread)
pthread_t               *thread;
{
    int r = pthread_detach(*thread);
    VRETURN(r);
}


void
ptdexc_join(thread, status)
pthread_t               thread;
void                    **status;
{
    int r = pthread_join(thread, status);
    VRETURN(r);
}


int
ptdexc_setprio(thread, priority)
pthread_t       thread;
int             priority;
{
    struct sched_param scd;
    int r, policy, old_priority;
    r = pthread_getschedparam(thread, &policy, &scd);
    if (r > 0)
        raise_error(r);
    old_priority = scd.sched_priority;
    scd.sched_priority = priority;
    r = pthread_setschedparam(thread, policy, &scd);
    RETURN2(r, old_priority);
}


int
ptdexc_getprio(thread)
pthread_t       thread;
{
    int r;
    struct sched_param scd;
    r = pthread_getschedparam(thread, &r, &scd);
    RETURN2(r, scd.sched_priority);
}


int
ptdexc_mutexattr_init(attr)
pthread_mutexattr_t     *attr;
{
    int r = pthread_mutexattr_init(attr);
    RETURN(r);
}


int
ptdexc_mutexattr_destroy(attr)
pthread_mutexattr_t     *attr;
{
    int r = pthread_mutexattr_destroy(attr);
    RETURN(r);
}


void
ptdexc_mutex_init(mutex, attr)
pthread_mutex_t         *mutex;
pthread_mutexattr_t     *attr;
{
    int r = pthread_mutex_init(mutex, attr);
    VRETURN(r);
}


void
ptdexc_mutex_destroy(mutex)
pthread_mutex_t         *mutex;
{
    int r = pthread_mutex_destroy(mutex);
    VRETURN(r);
}


void
ptdexc_mutex_lock(mutex)
pthread_mutex_t         *mutex;
{
    int r = pthread_mutex_lock(mutex);
    VRETURN(r);
}


int
ptdexc_mutex_trylock(mutex)
pthread_mutex_t         *mutex;
{
    int r = pthread_mutex_trylock(mutex);
    RETURN(r);
}


void
ptdexc_mutex_unlock(mutex)
pthread_mutex_t         *mutex;
{
    int r = pthread_mutex_unlock(mutex);
    VRETURN(r);
}


int
ptdexc_condattr_init(attr)
pthread_condattr_t      *attr;
{
    int r = pthread_condattr_init(attr);
    RETURN(r);
}


int
ptdexc_condattr_destroy(attr)
pthread_condattr_t      *attr;
{
    int r = pthread_condattr_destroy(attr);
    RETURN(r);
}


void
ptdexc_cond_init(cond, attr)
pthread_cond_t          *cond;
pthread_condattr_t      *attr;
{
    int r = pthread_cond_init(cond, attr);
    VRETURN(r);
}


void
ptdexc_cond_destroy(cond)
pthread_cond_t          *cond;
{
    int r = pthread_cond_destroy(cond);
    VRETURN(r);
}


void
ptdexc_cond_broadcast(cond)
pthread_cond_t          *cond;
{
    int r = pthread_cond_broadcast(cond);
    VRETURN(r);
}


void
ptdexc_cond_signal(cond)
pthread_cond_t          *cond;
{
    int r = pthread_cond_signal(cond);
    VRETURN(r);
}


void
ptdexc_cond_wait(cond, mutex)
pthread_cond_t          *cond;
pthread_mutex_t         *mutex;
{
    int r = pthread_cond_wait(cond, mutex);
    VRETURN(r);
}


/*
 * !!! Not clear what to do here, -1 could mean an error of could mean
 * that the time expired (non-error).  To tell the difference we could
 * look at errno, but in a threaded world that doesn't always work
 * right.
 */            
int
ptdexc_cond_timedwait(cond, mutex, abstime)
pthread_cond_t          *cond;
pthread_mutex_t         *mutex;
struct timespec         *abstime;
{
    int r = pthread_cond_timedwait(cond, mutex, abstime);
    
    return(r);
}


void
ptdexc_once(once_block, init_routine)
pthread_once_t          *once_block;
void                    (*init_routine)();
{
    int r = pthread_once(once_block, init_routine);
    VRETURN(r);
}


void
ptdexc_keycreate(key, destructor)
pthread_key_t           *key;
void                    (*destructor)(void *value);
{
    int r = pthread_key_create(key, destructor);
    VRETURN(r);
}


void
ptdexc_setspecific(key, value)
pthread_key_t   key;
void            *value;
{
    int r = pthread_setspecific(key, value);
    VRETURN(r);
}


void *
ptdexc_getspecific(key)
pthread_key_t   key;
{
    /*
     * XXX - throw an exception if this fails?
     * Given that this is used by the exception code,
     * that probably won't work very well....
     */
    return pthread_getspecific(key);
}


void
ptdexc_cancel(thread)
pthread_t       thread;
{
    int r = pthread_cancel(thread);
    VRETURN(r);
}
       
int
ptdexc_setcanceltype(type, oldtype)
int     type;
int     *oldtype;
{
    int r = pthread_setcanceltype(type, oldtype);
    RETURN(r);
}


int
ptdexc_setcancelstate(state, oldstate)
int     state;
int     *oldstate;
{
    int r = pthread_setcancelstate(state, oldstate);
    RETURN(r);
}
