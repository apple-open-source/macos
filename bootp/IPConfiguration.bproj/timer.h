
#ifndef _S_TIMER_H
#define _S_TIMER_H

/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * timer_private.h
 * - timer functions
 */

/* 
 * Modification History
 *
 * May 8, 2000	Dieter Siegmund (dieter@apple)
 * - created
 */

#ifndef CFRUNLOOP_NEW_API
#define CFRUNLOOP_NEW_API	1
#endif

#include <CoreFoundation/CFRunLoop.h>
#include "dynarray.h"

typedef struct timer_callout timer_callout_t;

typedef void (timer_func_t)(void * arg1, void * arg2, void * arg3);

struct timer_callout {
    timer_func_t *	func;
    void *		arg1;
    void *		arg2;
    void *		arg3;
    CFRunLoopTimerRef	timer_source;
    boolean_t		enabled;
};

struct timeval		timer_current_time();
long			timer_current_secs();

/**
 ** callout functions
 **/
timer_callout_t *	timer_callout_init();
void			timer_callout_free(timer_callout_t * * callout_p);

int			timer_set_relative(timer_callout_t * entry, 
					   struct timeval rel_time, 
					   timer_func_t * func, 
					   void * arg1, void * arg2, 
					   void * arg3);
void			timer_cancel(timer_callout_t * entry);

#endif _S_TIMER_H
