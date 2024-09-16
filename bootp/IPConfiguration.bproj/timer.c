/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
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
 * timer.c
 * - timer functions
 */

/* 
 * Modification History
 *
 * May 8, 2000	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include <syslog.h>
#include <notify.h>
#include <notify_keys.h>
#ifndef kNotifyClockSet
#define kNotifyClockSet "com.apple.system.clock_set"
#endif
#include "globals.h"
#include "util.h"
#include "timer.h"

#include <CoreFoundation/CFDate.h>

struct timer_callout {
    char *		name;
    timer_func_t *	func;
    void *		arg1;
    void *		arg2;
    void *		arg3;
    dispatch_source_t	timer;
    uint32_t		time_generation;
    boolean_t		enabled;
    boolean_t		suspended;
};

/*
 * Use notify(3) APIs to detect date/time changes
 */

static boolean_t	S_time_change_registered;
static int		S_time_change_token;
static uint32_t		S_time_generation;

static void
timer_cancel_common(timer_callout_t * callout);

static void
timer_suspend(timer_callout_t * callout)
{
    if (callout->timer == NULL) {
	return;
    }
    if (!callout->suspended) {
	dispatch_suspend(callout->timer);
	callout->suspended = TRUE;
    }
    return;
}

static void
timer_resume(timer_callout_t * callout)
{
    if (callout->suspended) {
	dispatch_resume(callout->timer);
	callout->suspended = FALSE;
    }
}

static void
timer_notify_check(void)
{
    int		check = 0;
    int		status;

    if (S_time_change_registered == FALSE) {
	return;
    }
    status = notify_check(S_time_change_token, &check);
    if (status != NOTIFY_STATUS_OK) {
	my_log(LOG_NOTICE, "timer: notify_check failed with %d", status);
    }
    else if (check != 0) {
	S_time_generation++;
    }
    return;
}

static void
timer_register_time_change(void)
{
    int		status;

    if (S_time_change_registered) {
	return;
    }
    status = notify_register_check(kNotifyClockSet, &S_time_change_token);
    if (status != NOTIFY_STATUS_OK) {
	my_log(LOG_NOTICE, "timer: notify_register_check(%s) failed, %d",
	       kNotifyClockSet, status);
	return;
    }
    S_time_change_registered = TRUE;

    /* throw away the first check, since it always says check=1 */
    timer_notify_check();
    return;
}

/**
 ** Timer callout functions
 **/
static const char *
timer_name(timer_callout_t * callout)
{
    return (callout->name);
}

static void 
timer_callout_process(timer_callout_t *	callout)
{
    if (callout->func && callout->enabled) {
	callout->enabled = FALSE;
	(*callout->func)(callout->arg1, callout->arg2, callout->arg3);
    }
    return;
}

timer_callout_t *
timer_callout_init(const char * name)
{
    timer_callout_t * callout;

    callout = malloc(sizeof(*callout));
    if (callout == NULL)
	return (NULL);
    bzero(callout, sizeof(*callout));
    callout->name = strdup(name);
    timer_register_time_change();
    callout->time_generation = S_time_generation;
    return (callout);
}

void
timer_callout_free(timer_callout_t * * callout_p)
{
    timer_callout_t * callout = *callout_p;

    if (callout == NULL) {
	return;
    }
    if (callout->timer != NULL) {
	dispatch_source_cancel(callout->timer);
	/* must resume a source before releasing it (see rdar://24585472 */
	timer_resume(callout);
	dispatch_release(callout->timer);
	callout->timer = NULL;
    }
    free(callout->name);
    free(callout);
    *callout_p = NULL;
    return;
}

static int
timer_callout_set_common(timer_callout_t * callout,
			 absolute_time_t relative_time,
			 timer_func_t * func,
			 void * arg1, void * arg2, void * arg3)
{
    boolean_t	first = FALSE;
    uint64_t	delta;

    if (callout == NULL) {
	return (0);
    }
    timer_cancel_common(callout);
    if (func == NULL) {
	return (0);
    }
    callout->func = func;
    callout->arg1 = arg1;
    callout->arg2 = arg2;
    callout->arg3 = arg3;
    callout->enabled = TRUE;
    callout->time_generation = S_time_generation;
    if (callout->timer == NULL) {
	dispatch_block_t		b;

	first = TRUE;
	callout->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
						0,
						0,
						IPConfigurationAgentQueue());
	b = ^{
	    timer_callout_process(callout);
	};
	dispatch_source_set_event_handler(callout->timer, b);
    }
    delta = relative_time * NSEC_PER_SEC;
    dispatch_source_set_timer(callout->timer,
			      dispatch_time(DISPATCH_TIME_NOW, delta),
			      DISPATCH_TIME_FOREVER,
			      0);
    if (first) {
	dispatch_activate(callout->timer);
    }
    else {
	timer_resume(callout);
    }
    my_log(LOG_DEBUG, "timer(%s): %0.09gs",
	   timer_name(callout),
	   relative_time);
    return (1);
}

int
timer_callout_set_absolute(timer_callout_t * callout,
			   absolute_time_t wakeup_time, timer_func_t * func,
			   void * arg1, void * arg2, void * arg3)
{
    absolute_time_t	now = CFAbsoluteTimeGetCurrent();

    return (timer_callout_set_common(callout, wakeup_time - now,
				     func, arg1, arg2, arg3));
}

int
timer_callout_set(timer_callout_t * callout,
		  absolute_time_t relative_time, timer_func_t * func,
		  void * arg1, void * arg2, void * arg3)
{
    return (timer_callout_set_common(callout, relative_time,
				     func, arg1, arg2, arg3));
}

int
timer_set_relative(timer_callout_t * callout, 
		   struct timeval rel_time, timer_func_t * func, 
		   void * arg1, void * arg2, void * arg3)
{
    CFTimeInterval	relative_time;

    if (rel_time.tv_sec < 0) {
	rel_time.tv_sec = 0;
	rel_time.tv_usec = 1;
    }
    relative_time = rel_time.tv_sec 
	+ ((double)rel_time.tv_usec / USECS_PER_SEC);
    return (timer_callout_set(callout, relative_time, func, arg1, arg2, arg3));
}

static void
timer_cancel_common(timer_callout_t * callout)
{
    callout->func = NULL;
    callout->time_generation = S_time_generation;
    timer_suspend(callout);
    callout->enabled = FALSE;
    return;
}

void
timer_cancel(timer_callout_t * callout)
{
    if (callout == NULL) {
	return;
    }
    if (callout->enabled) {
	my_log(LOG_DEBUG, "timer(%s): cancelled", timer_name(callout));
    }
    timer_cancel_common(callout);
    return;
}


/**
 ** timer functions
 **/
absolute_time_t
timer_get_current_time(void)
{
    return (CFAbsoluteTimeGetCurrent());
}

boolean_t
timer_time_changed(timer_callout_t * entry)
{
    timer_notify_check();
    return (entry->time_generation != S_time_generation);
}

boolean_t
timer_still_pending(timer_callout_t * entry)
{
    return (entry->enabled);
}

#ifdef TEST_TIMER
static void
second_timer(void * arg1, void * arg2, void * arg3)
{
    timer_callout_t *	timer2 = (timer_callout_t *)arg1;

    printf("%s\n", __func__);
    timer_cancel(timer2);
    timer_callout_free(&timer2);
    exit(0);
}

static void
first_timer(void * arg1, void * arg2, void * arg3)
{
    timer_callout_t *	timer1 = (timer_callout_t *)arg1;
    timer_callout_t *	timer2 = (timer_callout_t *)arg2;

    printf("%s\n", __func__);
    timer_callout_free(&timer1);
    timer_callout_set(timer2, 1,
		      second_timer,
		      timer2,
		      NULL,
		      NULL);
}

int
main(int argc, char * argv[])
{
    timer_callout_t *	timer1;
    timer_callout_t *	timer2;

    timer1 = timer_callout_init("test timer1");
    timer2 = timer_callout_init("test timer2");
    timer_callout_set(timer1, 1,
		      first_timer,
		      timer1,
		      timer2,
		      NULL);
    dispatch_main();
    exit(0);
    return (0);
}

#endif /* TEST_TIMER */
