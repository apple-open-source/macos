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
	systhread.h

	Copyright (c) 1999, Apple Computer Inc.
	All rights reserved.
	
	Created June 1999 by Marc Majka
 */

#ifndef __SYSTHREAD_H__
#define __SYSTHREAD_H__

#include "config.h"
#include <rpc/types.h>
#ifdef _THREAD_TYPE_PTHREAD_
#include <stdlib.h>
#include <pthread.h>
#define thread_type pthread_t
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
#define thread_type cthread_t
#endif

#define ThreadStateTerminal -1
#define ThreadStateInitial 0
#define ThreadStateIdle 1
#define ThreadStateActive 2
#define ThreadStateSleeping 3

typedef struct
{
	thread_type thread;
	char *name;
	bool_t should_terminate;
	bool_t is_running;
	void *data;
	unsigned long state;
} systhread;

systhread *systhread_new(void);
void systhread_free(systhread *);

systhread *systhread_current(void);

unsigned int systhread_count(void);
systhread *systhread_with_name(char *);

systhread *systhread_at_index(unsigned int);

unsigned long systhread_state(systhread *t);
void systhread_set_state(systhread *, unsigned long);

char *systhread_name(systhread *);
void systhread_set_name(systhread *, char *);

void *systhread_data(systhread *);
void systhread_set_data(systhread *, void *);

void systhread_run(systhread *, void(*)(void *), void *);

bool_t systhread_is_running(systhread *);

void systhread_set_should_terminate(systhread *, bool_t);
bool_t systhread_should_terminate(systhread *);

void systhread_exit(void);
void systhread_yield(void);

void systhread_sleep(unsigned long);
void systhread_usleep(unsigned long);

#endif __SYSTHREAD_H__
