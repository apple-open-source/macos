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
 * Thread.h
 *
 * Copyright (c) 1999, Apple Computer Inc.  All rights reserved.
 * Created May 1999 by Marc Majka
 */

#import "Root.h"

#ifdef _THREAD_TYPE_PTHREAD_
#import <stdlib.h>
#import <pthread.h>
#define thread_type pthread_t
#else
#import <mach/cthreads.h>
#define thread_type cthread_t
#endif

#define ThreadStateTerminal -1
#define ThreadStateInitial 0
#define ThreadStateIdle 1
#define ThreadStateActive 2
#define ThreadStateSleeping 3

@interface Thread : Root
{
	thread_type thread;
	char *name;
	BOOL shouldTerminate;
	BOOL isRunning;
	void *data;
	void *server;
	unsigned long dataLen;
	unsigned long state;
}

+ (void)shutdown;

+ (Thread *)currentThread;

+ (unsigned int)threadCount;
+ (Thread *)threadWithName:(char *)n;

+ (Thread *)threadAtIndex:(unsigned int)i;

+ (void)releaseThread:(Thread *)t;
+ (void)threadExit;

- (thread_type)thread;
- (void)setThread:(thread_type)t;

- (unsigned long)state;
- (void)setState:(unsigned long)s;

- (void)setName:(char *)n;

- (void *)data;
- (void)setData:(void *)d;

- (unsigned long)dataLen;
- (void)setDataLen:(unsigned long)l;

- (void *)server;
- (void)setServer:(void *)s;

/*
 * Thread objects are created with [[Thread alloc] init]
 * but a real system [cp]thread is not created until the
 * run:context: method is invoked.
 * E.G.:
 *
 *   Thread *t;
 *   t = [[Thread alloc] init];
 *   [t run:@selector(threadLoop) context:self];
 *
 */
- (void)run:(SEL)aSelector context:(id)anObject;

/*
 * shouldTerminate is an advisory flag.  It allows you to advise a thread
 * to exit.  The thread will only exit when it calls terminateSelf.
 * The default value is NO;
 * E.G.:
 *
 * [[Thread threadWithName:"WorkerBee"] shouldTerminate:YES];
 * ...
 * myThread = [Thread currentThread];
 * if ([myThread shouldTerminate]) [myThread terminateSelf];
 *
 */
- (void)shouldTerminate:(BOOL)yn;
- (BOOL)shouldTerminate;

- (void)terminateSelf;

- (void)yield;

- (void)sleep:(unsigned long)sec;
- (void)usleep:(unsigned long)msec;

@end
