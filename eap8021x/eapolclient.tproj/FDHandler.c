/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
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
 * FDHandler.c
 * - hides the details of how to get a callback when a file descriptor
 *   has data available
 * - wraps the CFSocket run-loop source
 */

/* 
 * Modification History
 *
 * October 26, 2001	Dieter Siegmund (dieter@apple.com)
 * - created (based on bootp/IPConfiguration.tproj/FDSet.c)
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

#include "FDHandler.h"

#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFSocket.h>

struct FDHandler_s {
    CFRunLoopSourceRef	rls;
    CFSocketRef		socket;
    int			fd;
    FDHandler_func *	func;
    void *		arg1;
    void *		arg2;
};

static void
FDHandler_callback(CFSocketRef s, CFSocketCallBackType type, 
		  CFDataRef address, const void *data, void *info)
{
    FDHandler * 	handler = (FDHandler *)info;

    if (handler->func) {
	(*handler->func)(handler->arg1, handler->arg2);
    }
    return;
}

FDHandler *
FDHandler_create(int fd)
{
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };
    FDHandler *	handler;

    handler = malloc(sizeof(*handler));
    if (handler == NULL)
	return (NULL);
    bzero(handler, sizeof(*handler));

    context.info = handler;
    handler->fd = fd;
    handler->socket 
	= CFSocketCreateWithNative(NULL, fd, kCFSocketReadCallBack,
				   FDHandler_callback, &context);
    handler->rls = CFSocketCreateRunLoopSource(NULL, handler->socket, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), handler->rls, 
		       kCFRunLoopDefaultMode);
    return (handler);
}

void
FDHandler_free(FDHandler * * handler_p)
{
    FDHandler * handler;

    if (handler_p == NULL) {
	return;
    }
    handler = *handler_p;
    if (handler) {
	if (handler->rls) {
	    /* cancel further handlers */
	    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), handler->rls, 
				  kCFRunLoopDefaultMode);

	    /* remove one socket reference, close the file descriptor */
	    CFSocketInvalidate(handler->socket);

	    /* release the socket */
	    CFRelease(handler->socket);
	    handler->socket = NULL;

	    /* release the run loop source */
	    CFRelease(handler->rls);
	    handler->rls = NULL;
	}
	free(handler);
    }
    *handler_p = NULL;
    return;
}

void
FDHandler_enable(FDHandler * handler, FDHandler_func * func, 
		 void * arg1, void * arg2)
{
    handler->func = func;
    handler->arg1 = arg1;
    handler->arg2 = arg2;
    return;
}

void
FDHandler_disable(FDHandler * handler)
{
    handler->func = NULL;
    handler->arg1 = NULL;
    handler->arg2 = NULL;
    return;
}


int
FDHandler_fd(FDHandler * handler)
{
    return (handler->fd);
}
