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
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 08-Mar-90  Avadis Tevanian, Jr. (avie) at NeXT
 *	Added errno field to cproc structure.
 *
 * $Log: cthread_internals.h,v $
 * Revision 1.1.1.1  1997/09/03 20:54:11  roland
 * Initial checkin of SGS release 244
 *
 * Revision 1.5  89/06/21  13:03:01  mbj
 * 	Removed the old (! IPC_WAIT) form of condition waiting/signalling.
 * 
 * Revision 1.4  89/05/19  13:02:58  mbj
 * 	Add cproc flags.
 * 
 * Revision 1.3  89/05/05  18:48:17  mrt
 * 	Cleanup for Mach 2.5
 * 
 * 24-Mar-89  Michael Jones (mbj) at Carnegie-Mellon University
 *	Implement fork() for multi-threaded programs.
 *	Made MTASK version work correctly again.
 */
/*
 * cthread_internals.h - by Eric Cooper
 *
 * Private definitions for the C Threads implementation.
 *
 * The cproc structure is used for different implementations
 * of the basic schedulable units that execute cthreads.
 *
 *	MTHREAD		MACH threads; single address space,
 *			kernel-mode preemptive scheduling
 */
#import <mach/mach.h>

/*
 * Low-level thread implementation.
 * This structure must agree with struct ur_cthread in cthreads.h
 */
typedef struct cproc {
	struct cproc *next;		/* for lock, condition, and ready queues */
	cthread_t incarnation;		/* for cthread_self() */
	int state;
	mach_port_t reply_port;		/* for mig_get_reply_port() */

	mach_port_t wait_port;

	int id;
	struct cproc *link;		/* so all cprocs can be found
					   after a fork() */
	int flags;

	unsigned int stack_base;
	unsigned int stack_size;
#if	NeXT
	int	error;
#endif	NeXT

} *cproc_t;

#define	NO_CPROC		((cproc_t) 0)
#define	cproc_self()		((cproc_t) ur_cthread_self())

/*
 * Possible cproc states.
 */
#define	CPROC_RUNNING		0
#define	CPROC_SPINNING		1
#define	CPROC_BLOCKED		2

/*
 * The cproc flag bits.
 */
#define CPROC_INITIAL_STACK	0x1
#if	NeXT
#define	CPROC_NOCACHE_THREAD	/* Don't try to cache this cthread on exit */
#endif	NeXT

/*
 * C Threads imports:
 */
#ifdef __STRICT_BSD__
extern char *malloc();
#endif __STRICT_BSD__

/*
 * Mach imports:
 */
extern void mach_error();

/*
 * C library imports:
 */
#ifdef __STRICT_BSD__
extern exit();
#else
#include <stdlib.h>
#endif __STRICT_BSD__

/*
 * Macro for MACH kernel calls.
 */
#define	MACH_CALL(expr, ret) {						\
	if (((ret) = (expr)) != KERN_SUCCESS) {				\
		mach_error("expr", (ret));				\
		ASSERT(SHOULDNT_HAPPEN);				\
		exit(1);						\
	}								\
}

/*
 * Debugging support.
 */
#ifdef	DEBUG

#define	private
#define	TRACE(x)	if (cthread_debug) x ; else
extern int cthread_debug;

/*
 * C library imports:
 */
extern printf(), fprintf(), abort();

#else	DEBUG

#define	private static
#define	TRACE(x)

#endif	DEBUG
