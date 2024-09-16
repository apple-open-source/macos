/*
 * Copyright (c) 2000-2017 Apple Inc. All rights reserved.
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
 * mylog.h
 * - logging related functions
 */

#ifndef _S_MYLOG_H
#define _S_MYLOG_H

/* 
 * Modification History
 *
 * June 15, 2009		Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */

#if MYLOG_STDOUT
#include <SystemConfiguration/SCPrivate.h>

#define my_log(pri, format, ...)	do {		\
	struct timeval	tv;				\
	struct tm       tm;				\
	time_t		t;				\
							\
	(void)gettimeofday(&tv, NULL);					\
	t = tv.tv_sec;							\
	(void)localtime_r(&t, &tm);					\
									\
	SCPrint(TRUE, stdout,						\
		CFSTR("%04d/%02d/%02d %2d:%02d:%02d.%06d " format "\n"), \
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,		\
		tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec,		\
		## __VA_ARGS__ );					\
    } while (0)

#define my_log_fl(pri, format, ...)	do {		\
	struct timeval	tv;				\
	struct tm       tm;				\
	time_t		t;				\
							\
	(void)gettimeofday(&tv, NULL);					\
	t = tv.tv_sec;							\
	(void)localtime_r(&t, &tm);					\
									\
	SCPrint(TRUE, stdout,						\
		CFSTR("%04d/%02d/%02d %2d:%02d:%02d.%06d " format "\n"), \
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,		\
		tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec,		\
		## __VA_ARGS__ );					\
    } while (0)

#else /* MYLOG_STDOUT */

#include "IPConfigurationLog.h"
#define my_log		SC_log
#define my_log_fl	SC_log
#endif /* MYLOG_STDOUT */

#endif /* _S_MYLOG_H */
