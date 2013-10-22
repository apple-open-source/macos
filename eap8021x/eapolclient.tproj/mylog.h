/*
 * Copyright (c) 2001-2013 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */

#ifndef _S_MYLOG_H
#define _S_MYLOG_H

#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include "EAPLog.h"

/**
 ** eapolclient logging
 **/
enum {
    kLogFlagBasic 		= 0x00000001,
    kLogFlagConfig		= 0x00000002,
    kLogFlagStatus		= 0x00000004,
    kLogFlagTunables		= 0x00000008,
    kLogFlagPacketDetails 	= 0x00000010,
    kLogFlagDisableInnerDetails	= 0x00001000, /* e.g. LogFlags 0xffffefff */

};

uint32_t
eapolclient_log_flags(void);

void
eapolclient_log_set_flags(uint32_t log_flags, bool log_it);

bool
eapolclient_should_log(uint32_t flags);

#define eapolclient_log(__flags, __format, ...)			\
    if (eapolclient_should_log(__flags))			\
	EAPLog(LOG_DEBUG, CFSTR(__format), ## __VA_ARGS__)

#endif /* _S_MYLOG_H */

