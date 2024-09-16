/*
 * Copyright (c) 2000-2023 Apple Inc. All rights reserved.
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
 * FDSet.c
 * - contains FDCallout, a thin wrapper on CFSocketRef/CFFileDescriptorRef
 */
/* 
 * Modification History
 *
 * May 11, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 * June 12, 2000	Dieter Siegmund (dieter@apple.com)
 * - converted to use CFRunLoop
 * January 27, 2010	Dieter Siegmund (dieter@apple.com)
 * - use CFFileDescriptorRef for non-sockets
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <net/if_types.h>
#include <syslog.h>

#include "dynarray.h"
#include "FDSet.h"
#include "symbol_scope.h"
#include "mylog.h"
#include "IPConfigurationAgentUtil.h"

struct FDCallout {
	int			fd;
	dispatch_source_t	source;
	FDCalloutFuncRef	func;
	void *			arg1;
	void *			arg2;
};

PRIVATE_EXTERN FDCalloutRef
FDCalloutCreate(int fd, FDCalloutFuncRef func,
		void * arg1, void * arg2,
		dispatch_block_t cancel_block)
{
	FDCalloutRef		callout;
	dispatch_block_t	handler;
	struct stat		sb;

	if (fstat(fd, &sb) < 0) {
		my_log(LOG_ERR, "%s: fstat %s (%d)",
		       __func__, strerror(errno), errno);
		return (NULL);
	}
	callout = malloc(sizeof(*callout));
	bzero(callout, sizeof(*callout));
	callout->fd = fd;
	callout->func = func;
	callout->arg1 = arg1;
	callout->arg2 = arg2;
	callout->source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
						 fd, 0,
						 IPConfigurationAgentQueue());
	dispatch_source_set_cancel_handler(callout->source, cancel_block);
	handler = ^{
		if (callout->func != NULL) {
			(*callout->func)(callout->arg1, callout->arg2);
		}
	};
	dispatch_source_set_event_handler(callout->source, handler);
	dispatch_activate(callout->source);
	return (callout);
}

PRIVATE_EXTERN void
FDCalloutRelease(FDCalloutRef * callout_p)
{
	FDCalloutRef callout = *callout_p;

	if (callout == NULL) {
		return;
	}
	if (callout->source) {
		dispatch_source_cancel(callout->source);
		dispatch_release(callout->source);
		callout->source = NULL;
	}
	free(callout);
	*callout_p = NULL;
	return;
}

PRIVATE_EXTERN int
FDCalloutGetFD(FDCalloutRef callout)
{
	return (callout->fd);
}
