
#ifndef _S_FDSET_H
#define _S_FDSET_H
/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * FDSet.h
 * - maintains a list of file descriptors to watch and corresponding
 *   functions to call when the file descriptor is ready
 */

/* 
 * Modification History
 *
 * May 11, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef CFRUNLOOP_NEW_API
#define CFRUNLOOP_NEW_API 1
#endif

#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFSocket.h>

/*
 * Type: FDCallout_func_t
 * Purpose:
 *   Client registers a function to call when file descriptor is ready.
 */

typedef void (FDCallout_func_t)(void * arg1, void * arg2);

typedef struct {
    dynarray_t		callouts;
    int			debug;
} FDSet_t;

typedef struct {
    CFRunLoopSourceRef	rls;
    CFSocketRef		socket;
    int			fd;
    FDCallout_func_t *	func;
    void *		arg1;
    void *		arg2;
} FDCallout_t;

FDCallout_t *
FDSet_add_callout(FDSet_t * set, int fd, FDCallout_func_t * func, 
		  void * arg1, void * arg2);

void
FDSet_remove_callout(FDSet_t * set, FDCallout_t * * callout);

FDSet_t * 
FDSet_init();

void
FDSet_free(FDSet_t * * set);

fd_set
FDSet_fd_set(FDSet_t * set);

int
FDSet_max_fd(FDSet_t * set);

#endif _S_FDSET_H
