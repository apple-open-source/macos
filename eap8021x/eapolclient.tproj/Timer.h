/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
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
 * Timer.h
 * - timer functions
 */

/* 
 * Modification History
 *
 * October 26, 2001	Dieter Siegmund (dieter@apple)
 * - created (from bootp/IPConfiguration/timer.h)
 */

#ifndef _S_TIMER_H
#define _S_TIMER_H
#include <sys/time.h>

typedef struct Timer_s Timer;

typedef void (Timer_func_t)(void * arg1, void * arg2, void * arg3);

struct timeval		Timer_current_time();
long			Timer_current_secs();
Timer *			Timer_create();
void			Timer_free(Timer * * callout_p);
int			Timer_set_relative(Timer * entry,
					   struct timeval rel_time,
					   Timer_func_t * func,
					   void * arg1, void * arg2,
					   void * arg3);
void			Timer_cancel(Timer * entry);

#endif _S_TIMER_H

