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
 * Thread.m
 * A simple Thread abstraction.
 *
 * Copyright (c) 1999, Apple Computer Inc.
 * All rights reserved.
 * 
 * Created May 1999 by Marc Majka
 */

#import "Thread.h"
#import "sys.h"
#import <string.h>
#import <unistd.h>
#import <NetInfo/dsutil.h>

static Thread **thread_list = NULL;
static unsigned int initialized = 0;
static unsigned int thread_count = 0;

typedef struct
{
	Thread *launchThread;
	SEL launchSEL;
	id launchContext;
	int launchArgCount;
	void *launchArg1;
	void *launchArg2;
} launch_args;

#ifdef _THREAD_TYPE_PTHREAD_
#define sys_mutex_type pthread_mutex_t
#define thread_arg_type void *
#define sys_mutex_lock pthread_mutex_lock
#define sys_mutex_unlock pthread_mutex_unlock
#else
#define sys_mutex_type mutex_t
#define thread_arg_type any_t
#define sys_mutex_lock mutex_lock
#define sys_mutex_unlock mutex_unlock
#endif

#ifdef _THREAD_TYPE_PTHREAD_
static sys_mutex_type *big_thread_lock;
#else
static sys_mutex_type big_thread_lock;
#endif

static thread_type
sys_thread_self(void)
{
#ifdef _THREAD_TYPE_PTHREAD_
	return pthread_self();
#else
	return cthread_self();
#endif
}

void
lock_threads(void)
{
	sys_mutex_lock(big_thread_lock);
}

void
unlock_threads(void)
{
	sys_mutex_unlock(big_thread_lock);
}

static void
launchpad(launch_args *x)
{
	launch_args args;

	args.launchThread = x->launchThread;
	args.launchSEL = x->launchSEL;
	args.launchContext = x->launchContext;
	args.launchArgCount = x->launchArgCount;
	args.launchArg1 = x->launchArg1;
	args.launchArg2 = x->launchArg2;

	free(x);

	[args.launchThread setThread:sys_thread_self()];
	
	if (args.launchArgCount == 0)
		[args.launchContext perform:args.launchSEL];
	else if (args.launchArgCount == 1)
		[args.launchContext perform:args.launchSEL with:args.launchArg1];
	else if (args.launchArgCount == 2)
		[args.launchContext perform:args.launchSEL with:args.launchArg1 with:args.launchArg2];
}

static void
launch_thread(launch_args *args)
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_t t;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&t, &attr, (void *(*)(void *))launchpad, (void *)args);
	pthread_attr_destroy(&attr);
	sched_yield();
#else
	cthread_detach(cthread_fork((cthread_fn_t)launchpad, (any_t)args));
	cthread_yield();
#endif
}

@implementation Thread

/*
 * WARNING - threadAtIndex is used to enumerate threads.
 * call lock_threads() first, then iterate through threads, then
 * call unlock_threads().
 */
+ (Thread *)threadAtIndex:(unsigned int)i
{
	if (i < thread_count) return thread_list[i];
	return nil;
}

- (thread_type)thread
{
	return thread;
}

- (void)setThread:(thread_type)t
{
	thread = t;
}

+ initialize
{
	Thread *mainThread;

	if (initialized++ == 0)
	{
#ifdef _THREAD_TYPE_PTHREAD_
		big_thread_lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(big_thread_lock, NULL);
#else
		big_thread_lock = mutex_alloc();
		mutex_init(big_thread_lock);
#endif

		mainThread = [[Thread alloc] init];
		[mainThread setBanner:"Main Thread"];
		mainThread->name = malloc(strlen("Main Thread") + 1);
		strcpy(mainThread->name, "Main Thread");
		mainThread->shouldTerminate = NO;
		mainThread->isRunning = YES;
		mainThread->thread = sys_thread_self();
	}

	return self;
}

+ (void)shutdown
{
	unsigned int i;

	lock_threads();
	for (i = 0; i < thread_count; i++) [thread_list[i] release];
	free(thread_list);
	unlock_threads();
	
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_mutex_destroy(big_thread_lock);
	free(big_thread_lock);
#else
	mutex_free(big_thread_lock);
#endif
}

+ (Thread *)currentThread
{
	thread_type me;
	unsigned int i;
	Thread *t;

	t = nil;
	
	lock_threads();
	me = sys_thread_self();
	for (i = 0; i < thread_count; i++)
	{
		if ([thread_list[i] thread] == me)
		{
			t = thread_list[i];
			break;
		}
	}
	unlock_threads();

	return t;
}

+ (unsigned int)threadCount
{
	unsigned int n;
	
	lock_threads();
	n = thread_count;
	unlock_threads();

	return n;
}

+ (Thread *)threadWithName:(char *)n
{
	unsigned int i;
	Thread *t;

	t = nil;
	
	lock_threads();
	for (i = 0; i < thread_count; i++)
	{
		if (streq(thread_list[i]->name, n))
		{
			t = thread_list[i];
			break;
		}
	}
	unlock_threads();

	return t;
}

+ (void)releaseThread:(Thread *)t
{
	unsigned int i, j;
	Thread *x;

	x = nil;
	
	lock_threads();
	
	for (i = 0; i < thread_count; i++)
	{
		if (thread_list[i] == t)
		{
			x = thread_list[i];

			for (j = i + 1; j < thread_count; j++)
			{
				thread_list[j - 1] = thread_list[j];
			}

			thread_count--;
			if (thread_count == 0)
			{
				free(thread_list);
				thread_list = NULL;
			}
			else
			{
				thread_list = (Thread **)realloc(thread_list, thread_count * sizeof(Thread *));
			}

			break;
		}
	}

	unlock_threads();

	if (t != nil) [t release];
}

+ (void)threadExit
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_exit(NULL);
#else
	cthread_exit(0);
#endif
}

- (Thread *)init
{
	unsigned int i;

	[super init];

	lock_threads();

	i = thread_count;
	thread_count++;
	if (thread_count == 1)
	{
		thread_list = (Thread **)malloc(sizeof(Thread *));
	}
	else
	{
		thread_list = (Thread **)realloc(thread_list, thread_count * sizeof(Thread *));
	}
	thread_list[i] = self;
	
	thread = (thread_type)-1;
	name = NULL;
	shouldTerminate = NO;
	isRunning = NO;
	data = NULL;
	dataLen = 0;
	server = NULL;
	state = ThreadStateInitial;
	unlock_threads();

	return self;
}

- (void)dealloc
{
	if (name != NULL) free(name);
	[super dealloc];
}

- (unsigned long)state
{
	return state;
}

- (void)setState:(unsigned long)s
{
	state = s;
}

- (const char *)name
{
	return name;
}

- (void)setName:(char *)n
{
	if (name != NULL) free(name);
	name = NULL;
	if (n == NULL) return;

	[self setBanner:n];
	name = malloc(strlen(n) + 1);
	strcpy(name, n);
}

- (void *)data
{
	return data;
}

- (void)setData:(void *)d
{
	data = d;
}

- (void *)server
{
	return server;
}

- (void)setServer:(void *)s
{
	server = s;
}

- (unsigned long)dataLen
{
	return dataLen;
}

- (void)setDataLen:(unsigned long)l
{
	dataLen = l;
}

- (void)run:(SEL)aSelector context:(id)anObject
{
	launch_args *args;

	if (anObject == nil) return;
	if (![anObject respondsTo:aSelector]) return;

	if (isRunning)
	{
		[anObject perform:aSelector];
		return;
	}

	isRunning = YES;

	args = (launch_args *)malloc(sizeof(launch_args));
	args->launchThread = self;
	args->launchSEL = aSelector;
	args->launchContext = anObject;
	args->launchArgCount = 0;
	args->launchArg1 = NULL;
	args->launchArg2 = NULL;

	launch_thread(args);
}

- (void)run:(SEL)aSelector context:(id)anObject with:(id)arg1
{
	launch_args *args;
	
	if (anObject == nil) return;
	if (![anObject respondsTo:aSelector]) return;

	if (isRunning)
	{
		[anObject perform:aSelector with:arg1];
		return;
	}

	isRunning = YES;

	args = (launch_args *)malloc(sizeof(launch_args));
	args->launchThread = self;
	args->launchSEL = aSelector;
	args->launchContext = anObject;
	args->launchArgCount = 1;
	args->launchArg1 = arg1;
	args->launchArg2 = NULL;
	
	launch_thread(args);
}

- (void)run:(SEL)aSelector context:(id)anObject with:(id)arg1 with:(id)arg2
{
	launch_args *args;
	
	if (anObject == nil) return;
	if (![anObject respondsTo:aSelector]) return;

	if (isRunning)
	{
		[anObject perform:aSelector with:arg1 with:arg2];
		return;
	}

	isRunning = YES;

	args = (launch_args *)malloc(sizeof(launch_args));
	args->launchThread = self;
	args->launchSEL = aSelector;
	args->launchContext = anObject;
	args->launchArgCount = 2;
	args->launchArg1 = arg1;
	args->launchArg2 = arg2;
	
	launch_thread(args);
}

- (void)shouldTerminate:(BOOL)yn
{
	shouldTerminate = yn;
}

- (BOOL)shouldTerminate
{
	return shouldTerminate;
}

- (void)terminateSelf
{
	[Thread releaseThread:[Thread currentThread]];
	[Thread threadExit];
}


- (void)yield
{
#ifdef _THREAD_TYPE_PTHREAD_
	sched_yield();
#else
	cthread_yield();
#endif
}

- (void)usleep:(unsigned long)msec
{
	unsigned long oldState;
	sys_msg_timeout_type snooze;
	struct no_msg {
		sys_msg_header_type head;
	} no_msg;

	if (msec == 0) return;

	snooze = msec;

	sys_receive_port_alloc(&(no_msg.head), sizeof(no_msg));

	oldState = state;

	state = ThreadStateSleeping;
	sys_receive_message(&(no_msg.head), sizeof(no_msg), no_msg.head.sys_msg_local_port, snooze);
	sys_port_free(no_msg.head.sys_msg_local_port);
	state = oldState;
}

- (void)sleep:(unsigned long)sec
{
	if (sec == 0) return;
	[self usleep:sec * 1000];
}

@end
