/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * To allow the DYLD_MEM_PROTECT feature to work the writing of the lock must
 * not happen.  Since mutex_unlock() is a macro for some architectures
 * overriding the function will not work.  So this is copied here as the best
 * solution to make vm_protect() call work without writing any data in the
 * data segment.  Also had to copy cthread_internals.h into here.
 */
#define DYLD

#ifndef NeXT
#define NeXT 1
#endif

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
/*
 * This does not look like it is used in Mach 3.0 so for now it is ifdef'ed
 * out.
 */
#ifndef __MACH30__
#include "stuff/openstep_mach.h"
#include <mach/cthreads.h>
#include "cthread_internals.h"

private struct mutex reply_port_lock = MUTEX_INITIALIZER;
#if	NeXT
#else	NeXT
private int multithreaded = 0;
#endif	NeXT

#if NeXT
/*
 * called in new child...
 * clear lock to cover case where the parent had
 * a thread holding this lock while another thread
 * did the fork()
 */
void
/*
 * The extra '_' is needed is the OPENSTEP case as libc is properly indr(l)'ed
 * but the MacOS X Server version is not.
 */
#ifdef __OPENSTEP__
_mig_fork_child()
#else
mig_fork_child()
#endif
{
	mutex_unlock(&reply_port_lock);
}
#endif

/*
 * Called by mach_init with 0 before cthread_init is
 * called and again with 1 at the end of cthread_init.
 */
void
#ifdef __OPENSTEP__
_mig_init(
#else
mig_init(
#endif
int init_done)
{
#if	NeXT
#else	NeXT
	multithreaded = init_done;
#endif	NeXT
}

/*
 * Called by mig interface code whenever a reply port is needed.
 * Tracing is masked during this call; otherwise, a call to printf()
 * can result in a call to malloc() which eventually reenters
 * mig_get_reply_port() and deadlocks.
 */
mach_port_t
#ifdef __OPENSTEP__
_mig_get_reply_port()
#else
mig_get_reply_port()
#endif
{
	register cproc_t self;
	register kern_return_t r;
	mach_port_t port;
#ifdef	CTHREADS_DEBUG
	int d = cthread_debug;
#endif	CTHREADS_DEBUG

#if	NeXT
#else	NeXT
	if (! multithreaded)
		return thread_reply();
#endif	NeXT
#ifdef	CTHREADS_DEBUG
	cthread_debug = FALSE;
#endif	CTHREADS_DEBUG
	self = cproc_self();
#if	NeXT
	if (self == NO_CPROC) {
#ifdef	CTHREADS_DEBUG
		cthread_debug = d;
#endif	CTHREADS_DEBUG
		return(thread_reply());
	}
#endif	NeXT
	if (self->reply_port == MACH_PORT_NULL) {
#ifndef DYLD
		mutex_lock(&reply_port_lock);
#endif
		if (self->reply_port == MACH_PORT_NULL) {
			self->reply_port = thread_reply();
			MACH_CALL(port_allocate(mach_task_self(), (int *)&port), r);
			self->reply_port = port;
		}
#ifndef DYLD
		mutex_unlock(&reply_port_lock);
#endif
	}
#ifdef	CTHREADS_DEBUG
	cthread_debug = d;
#endif	CTHREADS_DEBUG
	return self->reply_port;
}

/*
 * Called by mig interface code after a timeout on the reply port.
 * May also be called by user.
 */
void
#ifdef __OPENSTEP__
_mig_dealloc_reply_port()
#else
mig_dealloc_reply_port()
#endif
{
	register cproc_t self;
	register mach_port_t port;
#ifdef	CTHREADS_DEBUG
	int d = cthread_debug;
#endif	CTHREADS_DEBUG

#if	NeXT
#else	NeXT
	if (! multithreaded)
		return;
#endif	NeXT
#ifdef	CTHREADS_DEBUG
	cthread_debug = FALSE;
#endif	CTHREADS_DEBUG
	self = cproc_self();
#if	NeXT
	if (self == NO_CPROC) {
#ifdef	CTHREADS_DEBUG
		cthread_debug = d;
#endif	CTHREADS_DEBUG
		return;
	}
#endif	NeXT
	ASSERT(self != NO_CPROC);
	port = self->reply_port;
	if (port != MACH_PORT_NULL && port != thread_reply()) {
#ifndef DYLD
		mutex_lock(&reply_port_lock);
#endif
		if (self->reply_port != MACH_PORT_NULL &&
		    self->reply_port != thread_reply()) {
			self->reply_port = thread_reply();
			(void)port_deallocate(mach_task_self(), port);
			self->reply_port = MACH_PORT_NULL;
		}
#ifndef DYLD
		mutex_unlock(&reply_port_lock);
#endif
	}
#ifdef	CTHREADS_DEBUG
	cthread_debug = d;
#endif	CTHREADS_DEBUG
}
#endif /* __MACH30__ */
