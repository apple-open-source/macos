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
 * FDSet.c
 * - maintains a list of file descriptors to watch and corresponding
 *   functions to call when the file descriptor is ready
 */
/* 
 * Modification History
 *
 * May 11, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 * June 12, 2000	Dieter Siegmund (dieter@apple.com)
 * - converted to use CFRunLoop
 */


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <net/if_types.h>
#include <syslog.h>

#include "dynarray.h"
#include "FDSet.h"

static void
FDSet_process(CFSocketRef s, CFSocketCallBackType type, 
	      CFDataRef address, const void *data, void *info);

FDCallout_t *
FDSet_add_callout(FDSet_t * set, int fd, FDCallout_func_t * func,
		  void * arg1, void * arg2)
{
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };
    FDCallout_t *	callout;

    callout = malloc(sizeof(*callout));
    if (callout == NULL)
	return (NULL);
    bzero(callout, sizeof(*callout));

    if (dynarray_add(&set->callouts, callout) == FALSE) {
	free(callout);
	return (NULL);
    }
    context.info = callout;
    callout->socket 
	= CFSocketCreateWithNative(NULL, fd, kCFSocketReadCallBack,
				   FDSet_process, &context);
    callout->rls = CFSocketCreateRunLoopSource(NULL, callout->socket, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), callout->rls, 
		       kCFRunLoopDefaultMode);
    callout->fd = fd;
    callout->func = func;
    callout->arg1 = arg1;
    callout->arg2 = arg2;
    return (callout);
}

void
FDSet_remove_callout(FDSet_t * set, FDCallout_t * * callout_p)
{
    int 	  i;
    FDCallout_t * callout = *callout_p;

    i = dynarray_index(&set->callouts, callout);
    if (i == -1) {
	/* this can't happen */
	syslog(LOG_ERR, "FDSet_remove_callout: got back -1 index");
	return;
    }
    dynarray_free_element(&set->callouts, i);
    *callout_p = NULL;
    return;
}

static void
FDSet_callout_free(void * arg)
{
    FDCallout_t * callout = arg;

    if (callout->rls) {
	/* cancel further callouts */
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), callout->rls, 
			      kCFRunLoopDefaultMode);

	/* remove one socket reference, close the file descriptor */
	CFSocketInvalidate(callout->socket);

	/* release the socket */
	CFRelease(callout->socket);
	callout->socket = NULL;

	/* release the run loop source */
	CFRelease(callout->rls);
	callout->rls = NULL;
    }
    free(callout);
    return;
}

FDSet_t *
FDSet_init()
{
    FDSet_t * set = malloc(sizeof(*set));
    if (set == NULL)
	return (NULL);
    bzero(set, sizeof(*set));
    dynarray_init(&set->callouts, FDSet_callout_free, NULL);
    return (set);
}

void
FDSet_free(FDSet_t * * set_p)
{
    FDSet_t * set = *set_p;

    dynarray_free(&set->callouts);
    bzero(set, sizeof(*set));
    free(set);
    *set_p = NULL;
    return;
}

static void
FDSet_process(CFSocketRef s, CFSocketCallBackType type, 
	      CFDataRef address, const void *data, void *info)
{
    FDCallout_t * 	callout = (FDCallout_t *)info;

    if (callout->func) {
	(*callout->func)(callout->arg1, callout->arg2);
    }
    return;
}

