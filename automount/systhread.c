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
	systhread.c
	
	Copyright (c) 1999, Apple Computer Inc.
	All rights reserved.

	Created June 1999 by Marc Majka
 */

#import "systhread.h"
#import "syslock.h"
#import <string.h>
#import <unistd.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#ifdef _IPC_UNTYPED_
#define SYS_PORT_NULL MACH_PORT_NULL
#define sys_port_type mach_port_t
#define sys_msg_timeout_type mach_msg_timeout_t
#define sys_msg_return_type mach_msg_return_t
#define sys_msg_header_type mach_msg_header_t
#define sys_msg_local_port msgh_local_port
#define sys_task_self mach_task_self
#else
#define SYS_PORT_NULL PORT_NULL
#define sys_port_type port_t
#define sys_msg_timeout_type msg_timeout_t
#define sys_msg_return_type msg_return_t
#define sys_msg_header_type msg_header_t
#define sys_msg_local_port msg_local_port
#define sys_task_self task_self
#endif

static systhread **systhread_list = NULL;
static unsigned int initialized = 0;
static unsigned int systhread_list_count = 0;

static syslock *big_systhread_lock;

static thread_type
systhread_self(void)
{
#ifdef _THREAD_TYPE_PTHREAD_
	return pthread_self();
#else
	return cthread_self();
#endif
}

void
lock_systhreads(void)
{
	syslock_lock(big_systhread_lock);
}

void
unlock_systhreads(void)
{
	syslock_unlock(big_systhread_lock);
}

/*
 * WARNING - systhread_at_index is used to enumerate systhreads.
 * call lock_systhreads() first, then iterate through systhreads, then
 * call unlock_systhreads().
 */
systhread *
systhread_at_index(unsigned int i)
{
	if (i < systhread_list_count) return systhread_list[i];
	return NULL;
}

static void
systhreads_initialize(void)
{
	systhread *main_systhread;
	int len;

	if (initialized++ == 0)
	{
		big_systhread_lock = syslock_new(0);

		systhread_list = (systhread **)malloc(sizeof(systhread *));
		main_systhread = (systhread *)malloc(sizeof(systhread));
		len = strlen("Main Thread");
		main_systhread->name = malloc(len + 1);
		memmove(main_systhread->name, "Main Thread", len);
		main_systhread->name[len] = '\0';
		main_systhread->data = NULL;
		main_systhread->should_terminate = FALSE;
		main_systhread->is_running = TRUE;
		main_systhread->thread = systhread_self();

		systhread_list[0] = main_systhread;
		systhread_list_count = 1;
	}
}

systhread *
systhread_current(void)
{
	thread_type me;
	unsigned int i;
	systhread *t;

	t = NULL;
	
	lock_systhreads();
	me = systhread_self();
	for (i = 0; i < systhread_list_count; i++)
	{
		if (systhread_list[i]->thread == me)
		{
			t = systhread_list[i];
			break;
		}
	}
	unlock_systhreads();

	return t;
}

static systhread *
systhread_check(systhread *t)
{
	unsigned int i;
	
	for (i = 0; i < systhread_list_count; i++)
	{
		if (systhread_list[i] == t) return t;
	}

	return NULL;
}

unsigned int
systhread_count(void)
{
	unsigned int n;
	
	lock_systhreads();
	n = systhread_list_count;
	unlock_systhreads();

	return n;
}

systhread *
systhread_with_name(char *n)
{
	unsigned int i;
	systhread *t;

	t = NULL;
	
	lock_systhreads();
	for (i = 0; i < systhread_list_count; i++)
	{
		if (!strcmp(systhread_list[i]->name, n))
		{
			t = systhread_list[i];
			break;
		}
	}
	unlock_systhreads();

	return t;
}

systhread *
systhread_new(void)
{
	unsigned int i;
	systhread *me;

	systhreads_initialize();

	lock_systhreads();

	me = (systhread *)malloc(sizeof(systhread));
	memset(me, 0, sizeof(systhread));

	me->thread = systhread_self();
	me->name = NULL;
	me->should_terminate = FALSE;
	me->is_running = FALSE;
	me->data = NULL;
	me->state = ThreadStateInitial;

	i = systhread_list_count;
	systhread_list_count++;
	systhread_list = (systhread **)realloc(systhread_list, systhread_list_count * sizeof(systhread *));
	systhread_list[i] = me;

	unlock_systhreads();

	return me;
}

void
systhread_free(systhread *t)
{
	if (t == NULL) return;

	if (t->name != NULL) free(t->name);
	free(t);
}

void
systhread_run(systhread *t, void(*func)(void *), void *arg)
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_attr_t attr;
#endif

	if (t == NULL) return;

	lock_systhreads();

	if (systhread_check(t) == NULL)
	{
		unlock_systhreads();
		return;
	}

	t->is_running = TRUE;

#ifdef _THREAD_TYPE_PTHREAD_
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&(t->thread), &attr, (void *(*)(void *))func, arg);
	pthread_attr_destroy(&attr);
	sched_yield();
#else
	t->thread = cthread_fork((cthread_fn_t)func, (any_t)arg);
	cthread_detach(t->thread);
	cthread_yield();
#endif

	unlock_systhreads();
}

unsigned long
systhread_state(systhread *t)
{
	unsigned long s;

	if (t == NULL) return ThreadStateTerminal;

	lock_systhreads();

	if (systhread_check(t) == NULL)
	{
		unlock_systhreads();
		return ThreadStateTerminal;
	}

	s = t->state;
	unlock_systhreads();

	return s;
}

void
systhread_set_state(systhread *t, unsigned long s)
{
	if (t == NULL) return;

	lock_systhreads();

	if (systhread_check(t) == NULL)
	{
		unlock_systhreads();
		return;
	}

	t->state = s;
	unlock_systhreads();
}

bool_t
systhread_is_running(systhread *t)
{
	bool_t r;

	if (t == NULL) return FALSE;

	lock_systhreads();

	if (systhread_check(t) == NULL)
	{
		unlock_systhreads();
		return FALSE;
	}

	r = t->is_running;
	unlock_systhreads();
	
	return r;
}

char *
systhread_name(systhread *t)
{
	if (t == NULL) return NULL;

	lock_systhreads();

	if (systhread_check(t) == NULL)
	{
		unlock_systhreads();
		return NULL;
	}

	unlock_systhreads();
	return t->name;
}

void
systhread_set_name(systhread *t, char *n)
{
	int len;

	if (t == NULL) return;

	lock_systhreads();

	if (systhread_check(t) == NULL)
	{
		unlock_systhreads();
		return;
	}

	if (t->name != NULL) free(t->name);
	t->name = NULL;
	if (n == NULL)
	{
		unlock_systhreads();
		return;
	}

	len = strlen(n);
	t->name = malloc(len + 1);
	memmove(t->name, n, len);
	t->name[len] = '\0';
	unlock_systhreads();
}

void *
systhread_data(systhread *t)
{
	void *d;

	if (t == NULL) return NULL;

	lock_systhreads();

	if (systhread_check(t) == NULL)
	{
		unlock_systhreads();
		return NULL;
	}

	d = t->data;
	unlock_systhreads();
	
	return d;
}

void
systhread_set_data(systhread *t, void *d)
{
	if (t == NULL) return;

	lock_systhreads();

	if (systhread_check(t) == NULL)
	{
		unlock_systhreads();
		return;
	}

	t->data = d;
	unlock_systhreads();
}

void
systhread_set_should_terminate(systhread *t, bool_t yn)
{
	if (t == NULL) return;

	lock_systhreads();

	if (systhread_check(t) == NULL)
	{
		unlock_systhreads();
		return;
	}

	t->should_terminate = yn;
	unlock_systhreads();
}

bool_t
systhread_should_terminate(systhread *t)
{
	bool_t x;

	if (t == NULL) return FALSE;

	lock_systhreads();

	if (systhread_check(t) == NULL)
	{
		unlock_systhreads();
		return FALSE;
	}

	x = t->should_terminate;
	unlock_systhreads();

	return x;
}

void
systhread_exit(void)
{
	thread_type me;
	unsigned int i, j;
	systhread *t;

	t = NULL;

	lock_systhreads();
	
	me = systhread_self();
	for (i = 0; i < systhread_list_count; i++)
	{
		if (systhread_list[i]->thread == me)
		{
			t = systhread_list[i];

			for (j = i + 1; j < systhread_list_count; j++)
			{
				systhread_list[j - 1] = systhread_list[j];
			}
			systhread_list_count--;
			systhread_list = (systhread **)realloc(systhread_list, systhread_list_count * sizeof(systhread *));
			
			break;
		}
	}

	unlock_systhreads();

	if (t != NULL) systhread_free(t);

#ifdef _THREAD_TYPE_PTHREAD_
	pthread_exit(NULL);
#else
	cthread_exit(0);
#endif
}

void
systhread_yield(void)
{
#ifdef _THREAD_TYPE_PTHREAD_
	sched_yield();
#else
	cthread_yield();
#endif
}

static kern_return_t 
sys_receive_port_alloc(sys_msg_header_type *h, unsigned int size)
{
#ifdef _IPC_UNTYPED_
	h->msgh_size = size;
	return mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(h->msgh_local_port));
#else
	h->msg_size = size;
	return port_allocate(task_self(), &(h->msg_local_port));
#endif
}

static sys_msg_return_type
sys_receive_message(sys_msg_header_type *h, unsigned int size, sys_port_type p, sys_msg_timeout_type t)
{
	unsigned int flags;
#ifdef _IPC_UNTYPED_
	h->msgh_local_port = p;
	h->msgh_size = size;
	flags = MACH_RCV_MSG;
	if (t != 0) flags |= MACH_RCV_TIMEOUT;
	return mach_msg(h, flags, 0, size, p, t, SYS_PORT_NULL);
#else
	flags = MSG_OPTION_NONE;
	if (t != 0) flags = RCV_TIMEOUT;
	h->msg_local_port = p;
	h->msg_size = size;
	return msg_receive(h, flags, t);
#endif
}

static void
sys_port_free(sys_port_type p)
{
#ifdef _IPC_UNTYPED_
	mach_port_deallocate(sys_task_self(), p);
#else
	port_deallocate(sys_task_self(), p);
#endif
}

void
systhread_usleep(unsigned long msec)
{
	unsigned long old_state;
	sys_msg_timeout_type snooze;
	struct no_msg {
		sys_msg_header_type head;
	} no_msg;
	systhread *t;

	if (msec == 0) return;
	t = systhread_current();
	if (t == NULL) return;

	snooze = msec;

	sys_receive_port_alloc(&(no_msg.head), sizeof(no_msg));

	old_state = t->state;

	t->state = ThreadStateSleeping;
	sys_receive_message(&(no_msg.head), sizeof(no_msg), no_msg.head.sys_msg_local_port, snooze);
	sys_port_free(no_msg.head.sys_msg_local_port);
	t->state = old_state;
}

void
systhread_sleep(unsigned long sec)
{
	if (sec == 0) return;
	systhread_usleep(sec * 1000);
}
