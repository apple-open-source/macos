/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
/*
 * cthreads.c - by Eric Cooper
 *
 * Implementation of cthread_fork, cthread_join, cthread_exit, etc.
 * HISTORY
 * 22-July-93 Blaine Garst
 *	fixed association of read_thread info
 *	fixed kernel cache set up of cproc info
 *
 */
#include <stdlib.h>
#include "cthreads.h"
#include "cthread_internals.h"
#include "pthread_internals.h"
/*
 * C Threads imports:
 */
extern void cproc_init();
extern thread_port_t cproc_create();
extern void mig_init();
extern void _pthread_set_self(pthread_t);

/*
 * Mach imports:
 */
extern void mig_fork_child();

/*
 * C library imports:
 */
extern int _setjmp(jmp_buf env);
extern void _longjmp(jmp_buf env, int val);

/*
 * Thread status bits.
 */
#define	T_MAIN		0x1
#define	T_RETURNED	0x2
#define	T_DETACHED	0x4

#ifdef	CTHREADS_DEBUG
int cthread_debug = FALSE;
#endif	/* CTHREADS_DEBUG */

/*
 * Routines for supporting fork() of multi-threaded programs.
 */


extern void _malloc_fork_prepare(), _malloc_fork_parent();
extern void _malloc_fork_child();
extern void fork_mach_init();
extern void _cproc_fork_child(), _stack_fork_child();
extern void _lu_fork_child(void);
extern void _pthread_fork_child(pthread_t);
extern void _notify_fork_child(void);

static pthread_t psaved_self = 0;
static pthread_lock_t psaved_self_global_lock = LOCK_INITIALIZER;

void _cthread_fork_prepare()
/*
 * Prepare cthreads library to fork() a multi-threaded program.  All cthread
 * library critical section locks are acquired before a fork() and released
 * afterwards to insure no cthread data structure is left in an inconsistent
 * state in the child, which comes up with only the forking thread running.
 */
{
       _spin_lock(&psaved_self_global_lock);
       psaved_self = pthread_self();
       _spin_lock(&psaved_self->lock);
	_malloc_fork_prepare();
}

void _cthread_fork_parent()
/*
 * Called in the parent process after a fork syscall.
 * Releases locks acquired by cthread_fork_prepare().
 */
{
	_malloc_fork_parent();
        _spin_unlock(&psaved_self->lock);
        _spin_unlock(&psaved_self_global_lock);

}

void _cthread_fork_child()
/*
 * Called in the child process after a fork syscall.  Releases locks acquired
 * by cthread_fork_prepare().  Deallocates cthread data structures which
 * described other threads in our parent.  Makes this thread the main thread.
 */
{
	pthread_t p = psaved_self;
        
	_pthread_set_self(p);
        _spin_unlock(&psaved_self_global_lock);   
	mig_fork_child();
	_malloc_fork_child();
	p->kernel_thread = mach_thread_self();
	p->reply_port = mach_reply_port();
	p->mutexes = NULL;
	p->cleanup_stack = NULL;
	p->death = MACH_PORT_NULL;
	p->joiner = NULL;
	p->detached |= _PTHREAD_CREATE_PARENT;
        _spin_unlock(&p->lock);

        fork_mach_init();
	_pthread_fork_child(p);

	_cproc_fork_child();

	_lu_fork_child();

	_notify_fork_child();

	__is_threaded = 0;

	mig_init(1);		/* enable multi-threaded mig interfaces */
	
}

