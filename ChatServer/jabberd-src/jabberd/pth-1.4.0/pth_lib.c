/*
**  GNU Pth - The GNU Portable Threads
**  Copyright (c) 1999-2001 Ralf S. Engelschall <rse@engelschall.com>
**
**  This file is part of GNU Pth, a non-preemptive thread scheduling
**  library which can be found at http://www.gnu.org/software/pth/.
**
**  This library is free software; you can redistribute it and/or
**  modify it under the terms of the GNU Lesser General Public
**  License as published by the Free Software Foundation; either
**  version 2.1 of the License, or (at your option) any later version.
**
**  This library is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**  Lesser General Public License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License along with this library; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
**  USA, or contact Ralf S. Engelschall <rse@engelschall.com>.
**
**  pth_lib.c: Pth main library code
*/
                             /* ``It took me fifteen years to discover
                                  I had no talent for programming, but
                                  I couldn't give it up because by that
                                  time I was too famous.''
                                            -- Unknown                */
#include "pth_p.h"

/* return the hexadecimal Pth library version number */
long pth_version(void)
{
    return PTH_VERSION;
}

/* implicit initialization support */
intern int pth_initialized = FALSE;
#if cpp
#define pth_implicit_init() \
    if (!pth_initialized) \
        pth_init();
#endif

/* initialize the package */
int pth_init(void)
{
    pth_attr_t t_attr;

    /* support for implicit initialization calls
       and to prevent multiple explict initialization, too */
    if (pth_initialized)
        return_errno(FALSE, EPERM);
    else
        pth_initialized = TRUE;

    pth_debug1("pth_init: enter");

    /* initialize the scheduler */
    pth_scheduler_init();

    /* spawn the scheduler thread */
    t_attr = pth_attr_new();
    pth_attr_set(t_attr, PTH_ATTR_PRIO,         PTH_PRIO_MAX);
    pth_attr_set(t_attr, PTH_ATTR_NAME,         "**SCHEDULER**");
    pth_attr_set(t_attr, PTH_ATTR_JOINABLE,     FALSE);
    pth_attr_set(t_attr, PTH_ATTR_CANCEL_STATE, PTH_CANCEL_DISABLE);
    pth_attr_set(t_attr, PTH_ATTR_STACK_SIZE,   64*1024);
    pth_attr_set(t_attr, PTH_ATTR_STACK_ADDR,   NULL);
    pth_sched = pth_spawn(t_attr, pth_scheduler, NULL);
    if (pth_sched == NULL) {
        errno_shield { 
            pth_attr_destroy(t_attr);
            pth_scheduler_kill();
        }
        return FALSE;
    }

    /* spawn a thread for the main program */
    pth_attr_set(t_attr, PTH_ATTR_PRIO,         PTH_PRIO_STD);
    pth_attr_set(t_attr, PTH_ATTR_NAME,         "main");
    pth_attr_set(t_attr, PTH_ATTR_JOINABLE,     TRUE);
    pth_attr_set(t_attr, PTH_ATTR_CANCEL_STATE, PTH_CANCEL_ENABLE|PTH_CANCEL_DEFERRED);
    pth_attr_set(t_attr, PTH_ATTR_STACK_SIZE,   0 /* special */);
    pth_attr_set(t_attr, PTH_ATTR_STACK_ADDR,   NULL);
    pth_main = pth_spawn(t_attr, (void *(*)(void *))(-1), NULL);
    if (pth_main == NULL) {
        errno_shield { 
            pth_attr_destroy(t_attr);
            pth_scheduler_kill();
        }
        return FALSE;
    }
    pth_attr_destroy(t_attr);

    /*
     * The first time we've to manually switch into the scheduler to start
     * threading. Because at this time the only non-scheduler thread is the
     * "main thread" we will come back immediately. We've to also initialize
     * the pth_current variable here to allow the pth_spawn_trampoline
     * function to find the scheduler.
     */
    pth_current = pth_sched;
    pth_mctx_switch(&pth_main->mctx, &pth_sched->mctx);

    /* came back, so let's go home... */
    pth_debug1("pth_init: leave");
    return TRUE;
}

/* kill the package internals */
int pth_kill(void)
{
    if (pth_current != pth_main) 
        return_errno(FALSE, EPERM);
    pth_debug1("pth_kill: enter");
    pth_thread_cleanup(pth_main);
    pth_scheduler_kill();
    pth_initialized = FALSE;
    pth_tcb_free(pth_sched);
    pth_tcb_free(pth_main);
    pth_debug1("pth_kill: leave");
    return TRUE;
}

/* scheduler control/query */
long pth_ctrl(unsigned long query, ...)
{
    long rc;
    va_list ap;

    rc = 0;
    va_start(ap, query);
    if (query & PTH_CTRL_GETTHREADS) {
        if (query & PTH_CTRL_GETTHREADS_NEW)
            rc += pth_pqueue_elements(&pth_NQ);
        if (query & PTH_CTRL_GETTHREADS_READY)
            rc += pth_pqueue_elements(&pth_RQ);
        if (query & PTH_CTRL_GETTHREADS_RUNNING)
            rc += 1; /* pth_current only */
        if (query & PTH_CTRL_GETTHREADS_WAITING)
            rc += pth_pqueue_elements(&pth_WQ);
        if (query & PTH_CTRL_GETTHREADS_SUSPENDED) 
            rc += pth_pqueue_elements(&pth_SQ);
        if (query & PTH_CTRL_GETTHREADS_DEAD)
            rc += pth_pqueue_elements(&pth_DQ);
    }
    else if (query & PTH_CTRL_GETAVLOAD) {
        float *pload = va_arg(ap, float *);
        *pload = pth_loadval;
    }
    else if (query & PTH_CTRL_GETPRIO) {
        pth_t t = va_arg(ap, pth_t);
        rc = t->prio;
    }
    else if (query & PTH_CTRL_GETNAME) {
        pth_t t = va_arg(ap, pth_t);
        rc = (long)t->name;
    }
    else if (query & PTH_CTRL_DUMPSTATE) {
        FILE *fp = va_arg(ap, FILE *);
        pth_dumpstate(fp);
    }
    else
        rc = -1;
    va_end(ap);
    if (rc == -1)
        return_errno(-1, EINVAL);
    return rc;
}

/* create a new thread of execution by spawning a cooperative thread */
static void pth_spawn_trampoline(void)
{
    void *data;

    /* just jump into the start routine */
    data = (*pth_current->start_func)(pth_current->start_arg);
    /* and do an implicit exit of the tread with the result value */
    pth_exit(data);
    /* no return! */
    abort();
}
pth_t pth_spawn(pth_attr_t attr, void *(*func)(void *), void *arg)
{
    pth_t t;
    unsigned int stacksize;
    void *stackaddr;
    pth_time_t ts;

    pth_debug1("pth_spawn: enter");

    /* consistency */
    if (func == NULL)
        return_errno(NULL, EINVAL);

    /* support the special case of main() */
    if (func == (void *(*)(void *))(-1))
        func = NULL;

    /* allocate a new thread control block */
    stacksize = (attr == PTH_ATTR_DEFAULT ? 64*1024 : attr->a_stacksize);
    stackaddr = (attr == PTH_ATTR_DEFAULT ? NULL : attr->a_stackaddr);
    if ((t = pth_tcb_alloc(stacksize, stackaddr)) == NULL)
        return NULL; /* errno is inherited */

    /* configure remaining attributes */
    if (attr != PTH_ATTR_DEFAULT) {
        /* overtake fields from the attribute structure */
        t->prio        = attr->a_prio;
        t->joinable    = attr->a_joinable;
        t->cancelstate = attr->a_cancelstate;
        pth_util_cpystrn(t->name, attr->a_name, PTH_TCB_NAMELEN);
    }
    else if (pth_current != NULL) {
        /* overtake some fields from the parent thread */
        t->prio        = pth_current->prio;
        t->joinable    = pth_current->joinable;
        t->cancelstate = pth_current->cancelstate;
        pth_snprintf(t->name, PTH_TCB_NAMELEN, "%s.child@%d=0x%lx", 
                     pth_current->name, (unsigned int)time(NULL), 
                     (unsigned long)pth_current);
    }
    else {
        /* defaults */
        t->prio        = PTH_PRIO_STD;
        t->joinable    = TRUE;
        t->cancelstate = PTH_CANCEL_DEFAULT;
        pth_snprintf(t->name, PTH_TCB_NAMELEN,
                     "user/%x", (unsigned int)time(NULL));
    }

    /* initialize the time points and ranges */
    pth_time_set(&ts, PTH_TIME_NOW);
    pth_time_set(&t->spawned, &ts);
    pth_time_set(&t->lastran, &ts);
    pth_time_set(&t->running, PTH_TIME_ZERO);

    /* initialize events */
    t->events = NULL;

    /* clear raised signals */
    sigemptyset(&t->sigpending);
    t->sigpendcnt = 0;

    /* remember the start routine and arguments for our trampoline */
    t->start_func = func;
    t->start_arg  = arg;

    /* initialize join argument */
    t->join_arg = NULL;

    /* initialize thread specific storage */
    t->data_value = NULL;
    t->data_count = 0;

    /* initialize cancellaton stuff */
    t->cancelreq   = FALSE;
    t->cleanups    = NULL;

    /* initialize mutex stuff */
    pth_ring_init(&t->mutexring);

    /* initialize the machine context of this new thread */
    if (t->stacksize > 0) { /* the "main thread" (indicated by == 0) is special! */
        if (!pth_mctx_set(&t->mctx, pth_spawn_trampoline,
                          t->stack, ((char *)t->stack+t->stacksize))) {
            errno_shield { pth_tcb_free(t); }
            return NULL;
        }
    }

    /* finally insert it into the "new queue" where
       the scheduler will pick it up for dispatching */
    if (func != pth_scheduler) {
        t->state = PTH_STATE_NEW;
        pth_pqueue_insert(&pth_NQ, t->prio, t);
    }

    pth_debug1("pth_spawn: leave");

    /* the returned thread id is just the pointer
       to the thread control block... */
    return t;
}

/* returns the current thread */
pth_t pth_self(void)
{
    return pth_current;
}

/* raise a signal for a thread */
int pth_raise(pth_t t, int sig)
{
    struct sigaction sa;

    if (t == NULL || t == pth_current || (sig < 0 || sig > PTH_NSIG))
        return_errno(FALSE, EINVAL);
    if (sig == 0)
        /* just test whether thread exists */
        return pth_thread_exists(t);
    else {
        /* raise signal for thread */
        if (sigaction(sig, NULL, &sa) != 0)
            return FALSE;
        if (sa.sa_handler == SIG_IGN)
            return TRUE; /* fine, nothing to do, sig is globally ignored */
        if (!sigismember(&t->sigpending, sig)) {
            sigaddset(&t->sigpending, sig);
            t->sigpendcnt++;
        }
        pth_yield(t);
        return TRUE;
    }
}

/* check whether a thread exists */
intern int pth_thread_exists(pth_t t)
{
    if (!pth_pqueue_contains(&pth_NQ, t))
        if (!pth_pqueue_contains(&pth_RQ, t))
            if (!pth_pqueue_contains(&pth_WQ, t))
                if (!pth_pqueue_contains(&pth_SQ, t))
                    if (!pth_pqueue_contains(&pth_DQ, t))
                        return_errno(FALSE, ESRCH); /* not found */
    return TRUE;
}

/* cleanup a particular thread */
intern void pth_thread_cleanup(pth_t thread)
{
    /* run the cleanup handlers */
    if (thread->cleanups != NULL)
        pth_cleanup_popall(thread, TRUE);

    /* run the specific data destructors */
    if (thread->data_value != NULL)
        pth_key_destroydata(thread);

    /* release still acquired mutex variables */
    pth_mutex_releaseall(thread);

    return;
}

/* terminates the current thread */
static int pth_exit_cb(void *arg)
{
    int rc;

    /* NOTICE: THIS FUNCTION EXECUTES
       FROM WITHIN THE SCHEDULER THREAD! */
    rc = 0;
    rc += pth_pqueue_elements(&pth_NQ);
    rc += pth_pqueue_elements(&pth_RQ);
    rc += pth_pqueue_elements(&pth_WQ);
    rc += pth_pqueue_elements(&pth_SQ);
    rc += pth_pqueue_elements(&pth_DQ);
    if (rc == 1 /* just our main thread */)
        return TRUE;
    else
        return FALSE;
}
void pth_exit(void *value)
{
    pth_event_t ev;

    pth_debug2("pth_exit: marking thread \"%s\" as dead", pth_current->name);

    /* main thread is special:
       wait until it is the last thread */
    if (pth_current == pth_main) {
        ev = pth_event(PTH_EVENT_FUNC, pth_exit_cb);
        pth_wait(ev);
        pth_event_free(ev, PTH_FREE_THIS);
    }

    /* execute cleanups */
    pth_thread_cleanup(pth_current);

    /* mark the current thread as dead, so the scheduler removes us */
    pth_current->join_arg = value;
    pth_current->state = PTH_STATE_DEAD;

    if (pth_current != pth_main) {
        /*
         * Now we explicitly switch into the scheduler and let it
         * reap the current thread structure; we can't free it here,
         * or we'd be running on a stack which malloc() regards as
         * free memory, which would be a somewhat perilous situation.
         */
        pth_debug2("pth_exit: switching from thread \"%s\" to scheduler", pth_current->name);
        pth_mctx_switch(&pth_current->mctx, &pth_sched->mctx);
        abort(); /* not reached! */
    }
    else {
        /* 
         * main thread is special: exit the _process_ 
         * [double-cast to avoid warnings because of size] 
         */ 
        pth_kill();
        exit((int)((long)value));
        abort(); /* not reached! */
    }
}

/* waits for the termination of the specified thread */
int pth_join(pth_t tid, void **value)
{
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;

    pth_debug2("pth_join: joining thread \"%s\"", tid == NULL ? "-ANY-" : tid->name);
    if (tid == pth_current)
        return_errno(FALSE, EDEADLK);
    if (tid != NULL && !tid->joinable)
        return_errno(FALSE, EINVAL);
    if (pth_ctrl(PTH_CTRL_GETTHREADS) == 1)
        return_errno(FALSE, EDEADLK);
    if (tid == NULL)
        tid = pth_pqueue_head(&pth_DQ);
    if (tid == NULL || (tid != NULL && tid->state != PTH_STATE_DEAD)) {
        ev = pth_event(PTH_EVENT_TID|PTH_UNTIL_TID_DEAD|PTH_MODE_STATIC, &ev_key, tid);
        pth_wait(ev);
    }
    if (tid == NULL)
        tid = pth_pqueue_head(&pth_DQ);
    if (tid == NULL || (tid != NULL && tid->state != PTH_STATE_DEAD))
        return_errno(FALSE, EIO);
    if (value != NULL)
        *value = tid->join_arg;
    pth_pqueue_delete(&pth_DQ, tid);
    pth_tcb_free(tid);
    return TRUE;
}

/* delegates control back to scheduler for context switches */
int pth_yield(pth_t to)
{
    pth_pqueue_t *q = NULL;

    pth_debug2("pth_yield: enter from thread \"%s\"", pth_current->name);

    /* a given thread has to be new or ready or we ignore the request */
    if (to != NULL) {
        switch (to->state) {
            case PTH_STATE_NEW:    q = &pth_NQ; break;
            case PTH_STATE_READY:  q = &pth_RQ; break;
            default:               q = NULL;
        }
        if (q == NULL || !pth_pqueue_contains(q, to))
            return_errno(FALSE, EINVAL);
    }

    /* give a favored thread maximum priority in his queue */
    if (to != NULL && q != NULL)
        pth_pqueue_favorite(q, to);

    /* switch to scheduler */
    if (to != NULL)
        pth_debug2("pth_yield: give up control to scheduler "
                   "in favour of thread \"%s\"", to->name);
    else
        pth_debug1("pth_yield: give up control to scheduler");
    pth_mctx_switch(&pth_current->mctx, &pth_sched->mctx);
    pth_debug1("pth_yield: got back control from scheduler");

    pth_debug2("pth_yield: leave to thread \"%s\"", pth_current->name);
    return TRUE;
}

/* suspend a thread until its again manually resumed */
int pth_suspend(pth_t t)
{
    pth_pqueue_t *q;

    if (t == NULL)
        return_errno(FALSE, EINVAL);
    if (t == pth_sched || t == pth_current)
        return_errno(FALSE, EPERM);
    switch (t->state) {
        case PTH_STATE_NEW:     q = &pth_NQ; break;
        case PTH_STATE_READY:   q = &pth_RQ; break;
        case PTH_STATE_WAITING: q = &pth_WQ; break;
        default:                q = NULL;
    }
    if (q == NULL)
        return_errno(FALSE, EPERM);
    if (!pth_pqueue_contains(q, t))
        return_errno(FALSE, ESRCH);
    pth_pqueue_delete(q, t);
    pth_pqueue_insert(&pth_SQ, PTH_PRIO_STD, t);
    pth_debug2("pth_suspend: suspend thread \"%s\"\n", t->name);
    return TRUE;
}

/* resume a previously suspended thread */
int pth_resume(pth_t t)
{
    pth_pqueue_t *q;

    if (t == NULL)
        return_errno(FALSE, EINVAL);
    if (t == pth_sched || t == pth_current)
        return_errno(FALSE, EPERM);
    if (!pth_pqueue_contains(&pth_SQ, t))
        return_errno(FALSE, EPERM);
    pth_pqueue_delete(&pth_SQ, t);
    switch (t->state) {
        case PTH_STATE_NEW:     q = &pth_NQ; break;
        case PTH_STATE_READY:   q = &pth_RQ; break;
        case PTH_STATE_WAITING: q = &pth_WQ; break;
        default:                q = NULL;
    }
    pth_pqueue_insert(q, PTH_PRIO_STD, t);
    pth_debug2("pth_resume: resume thread \"%s\"\n", t->name);
    return TRUE;
}

/* switch a filedescriptor's I/O mode */
int pth_fdmode(int fd, int newmode)
{
    int fdmode;
    int oldmode;

    /* retrieve old mode (usually cheap) */
    if ((fdmode = fcntl(fd, F_GETFL, NULL)) == -1)
        oldmode = PTH_FDMODE_ERROR;
    else if (fdmode & O_NONBLOCKING)
        oldmode = PTH_FDMODE_NONBLOCK;
    else
        oldmode = PTH_FDMODE_BLOCK;

    /* set new mode (usually expensive) */
    if (oldmode == PTH_FDMODE_BLOCK && newmode == PTH_FDMODE_NONBLOCK)
        fcntl(fd, F_SETFL, (fdmode | O_NONBLOCKING));
    if (oldmode == PTH_FDMODE_NONBLOCK && newmode == PTH_FDMODE_BLOCK)
        fcntl(fd, F_SETFL, (fdmode & ~(O_NONBLOCKING)));

    /* return old mode */
    return oldmode;
}

/* wait for specific amount of time */
int pth_nap(pth_time_t naptime)
{
    pth_time_t until;
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;

    if (pth_time_cmp(&naptime, PTH_TIME_ZERO) == 0)
        return_errno(FALSE, EINVAL);
    pth_time_set(&until, PTH_TIME_NOW);
    pth_time_add(&until, &naptime);
    ev = pth_event(PTH_EVENT_TIME|PTH_MODE_STATIC, &ev_key, until);
    pth_wait(ev);
    return TRUE;
}

/* runs a constructor once */
int pth_once(pth_once_t *oncectrl, void (*constructor)(void *), void *arg)
{
    if (oncectrl == NULL || constructor == NULL)
        return_errno(FALSE, EINVAL);
    if (*oncectrl != TRUE)
        constructor(arg);
    *oncectrl = TRUE;
    return TRUE;
}

