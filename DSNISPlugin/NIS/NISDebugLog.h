/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 
/*!
 *	@header NISDebugLog
 *  Definition of simple DebugLog controlled by environment variable
 */

#ifndef _DEBUGLOG_H_
#define _DEBUGLOG_H_

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>			// syslog

// Set compile flag BUILDING_NSLDEBUG=1 to enable debug logging
// Set environment variable NSLDEBUG at run time to see messages

#define LOG_ALL				0

#if LOG_ALL
	#define DBGLOG(format, args...) \
		if (true) \
		{ \
			syslog( LOG_ERR, format , ## args); \
			fflush(NULL); \
		} \
		else
#else
	#define DBGLOG(format, args...) \
		if (getenv( "NSLDEBUG" )) \
		{ \
			fprintf (stderr, format , ## args); \
			fflush(NULL); \
		} \
		else
#endif

#endif
