/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 
/*!
 *	@header NSLDebugLog
 *  Definition of simple DebugLog controlled by environment variable
 */

#ifndef _DEBUGLOG_H_
#define _DEBUGLOG_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>			// getpid
#include <syslog.h>			// syslog
#include <pthread.h>	// for pthread_*_t

// Set compile flag BUILDING_NSLDEBUG=1 to enable debug logging
// Set environment variable NSLDEBUG at run time to see messages

#ifndef BUILDING_NSLDEBUG
#define BUILDING_NSLDEBUG	0
#endif

#if BUILDING_NSLDEBUG
#warning "BUILDING_NSLDEBUG is defined, DO NOT SUBMIT THIS WAY!"	
	#define DBGLOG(format, args...) \
		if (true) \
		{ \
			syslog( LOG_ERR, format , ## args); \
			fflush(NULL); \
		} \
            else
#else
    #define DBGLOG(format, args...)
#endif

#endif
