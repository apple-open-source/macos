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
 * Event handling
 * Copyright (C) 1989 by NeXT, Inc.
 *
 * Simple-minded event handling. Just writes things to a pipe and expects
 * to be called back by the main loop when it detects data ready on the
 * pipe (select() is king).
 */
#include <NetInfo/config.h>
#include <netinfo/ni.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "event.h"
#include <NetInfo/socket_lock.h>

int event_pipe[2] = { -1, -1 };

static void (*event_callback)(void);

/*
 * Handle an event
 */
void
event_handle(
	     void
	     )
{
	char c;

	/*
	 * Clear event
	 */
	read(event_pipe[0], &c, sizeof(c));

	/*
	 * And callback
	 */
	(*event_callback)();

}

/*
 * Post an event
 */
void
event_post(
	   void
	   )
{
	char c;

	c = 'x'; /* not really necessary */
	write(event_pipe[1], &c, sizeof(c));
	fsync(event_pipe[1]);
}


/*
 * Initialize things
 */
void
event_init(
	   void (*callback)(void)
	   )
{
	if (event_pipe[0] < 0) {
		socket_lock();
		pipe(event_pipe);
		socket_unlock();
	}
	event_callback = callback;
}
