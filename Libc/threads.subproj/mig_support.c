/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * mig_support.c  - by Mary Thompson
 *
 * Routines to set and deallocate the mig reply port for the current thread.
 * Called from mig-generated interfaces.
 */
#include <mach/mach.h>
#include <pthread_internals.h>
#include <pthread.h>

#include "cthreads.h"
#include "cthread_internals.h"

pthread_lock_t reply_port_lock;
extern mach_port_t _pthread_reply_port(pthread_t);
static mach_port_t _task_reply_port = MACH_PORT_NULL;

/*
 * called in new child...
 * clear lock to cover case where the parent had
 * a thread holding this lock while another thread
 * did the fork()
 */
void mig_fork_child()
{
	UNLOCK(reply_port_lock);
}

/*
 * Called by mach_init with 0 before cthread_init is
 * called and again with 1 at the end of cthread_init.
 */
void
mig_init(init_done)
	int init_done;
{
    if (init_done == 0) {
        LOCK_INIT(reply_port_lock);
        _task_reply_port = mach_reply_port();
    }
}

/*
 * Called by mig interface code whenever a reply port is needed.
 * Tracing is masked during this call; otherwise, a call to printf()
 * can result in a call to malloc() which eventually reenters
 * mig_get_reply_port() and deadlocks.
 */
mach_port_t
mig_get_reply_port()
{
	register cproc_t self;
        pthread_t pself;
#ifdef	CTHREADS_DEBUG
	int d = cthread_debug;
#endif	CTHREADS_DEBUG

#ifdef	CTHREADS_DEBUG
	cthread_debug = FALSE;
#endif	CTHREADS_DEBUG
        pself = pthread_self();
        if ((pself != (pthread_t)NULL) && (pself->sig == _PTHREAD_SIG)) {
            if (pself->reply_port == MACH_PORT_NULL) {
                pself->reply_port = mach_reply_port();
            }
            return pself->reply_port;
        }
	self = cproc_self();
	if (self == NO_CPROC) {
#ifdef	CTHREADS_DEBUG
		cthread_debug = d;
#endif	CTHREADS_DEBUG
		return(_task_reply_port);
	}
        if (self->reply_port == MACH_PORT_NULL) {
            self->reply_port = mach_reply_port();
        }
#ifdef	CTHREADS_DEBUG
	cthread_debug = d;
#endif	CTHREADS_DEBUG
	return self->reply_port;
}

/*
 * Called by mig interface code after a timeout on the reply port.
 * May also be called by user. The new mig calls with port passed in
 * We are ignoring this , so is osfmk cthreads code
 */
void
mig_dealloc_reply_port(mach_port_t migport)
{
	register cproc_t self;
        pthread_t pself;
	register mach_port_t port;
#ifdef	CTHREADS_DEBUG
	int d = cthread_debug;
#endif	CTHREADS_DEBUG

#ifdef	CTHREADS_DEBUG
	cthread_debug = FALSE;
#endif	CTHREADS_DEBUG
        pself = pthread_self();
        if ((pself != (pthread_t)NULL) && (pself->sig == _PTHREAD_SIG)) {
            port = pself->reply_port;
            if (port != MACH_PORT_NULL && port != _task_reply_port) {
                    LOCK(reply_port_lock);
                    pself->reply_port = _task_reply_port;
                    (void) mach_port_destroy(mach_task_self(), port);
                    pself->reply_port = MACH_PORT_NULL;
                    UNLOCK(reply_port_lock);
            }
            return;
        }
	self = cproc_self();
	if (self == NO_CPROC) {
#ifdef	CTHREADS_DEBUG
		cthread_debug = d;
#endif	CTHREADS_DEBUG
		return;
	}
	ASSERT(self != NO_CPROC);
	port = self->reply_port;
        if (port != MACH_PORT_NULL && port != _task_reply_port) {
		LOCK(reply_port_lock);
                self->reply_port = _task_reply_port;
                (void) mach_port_destroy(mach_task_self(), port);
                self->reply_port = MACH_PORT_NULL;
		UNLOCK(reply_port_lock);
	}
#ifdef	CTHREADS_DEBUG
	cthread_debug = d;
#endif	CTHREADS_DEBUG
}

/*************************************************************
 *  Called by mig interfaces after each RPC.
 *  Could be called by user.
 ***********************************************************/

void
mig_put_reply_port(
	mach_port_t	reply_port)
{
}
