/*
 * Copyright (c) 2012-2013 Apple Inc. All rights reserved.
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
 * EAPLog.c
 * - functions to log EAP-related information
 */
/* 
 * Modification History
 *
 * December 26, 2012	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <dispatch/dispatch.h>
#include <SystemConfiguration/SCPrivate.h>
#include "EAPClientPlugin.h"
#include "symbol_scope.h"

/*
 * kEAPOLLoggerID
 * - used with SCLoggerCreate to identify which ASL logging module to use
 */
#define kEAPOLLoggerID		CFSTR("com.apple.networking.eapol")

STATIC SCLoggerRef	S_eap_logger;

STATIC void
EAPLogInit(void)
{
    STATIC dispatch_once_t	once;

    dispatch_once(&once,
		  ^{
		      S_eap_logger = SCLoggerCreate(kEAPOLLoggerID);
		  });
    return;
}

void
EAPLog(int level, CFStringRef format, ...)
{
    va_list	args;

    EAPLogInit();
    va_start(args, format);
    SCLoggerVLog(S_eap_logger, level, format, args);
    va_end(args);
    return;
}

void
EAPLogSetVerbose(bool verbose)
{
    SCLoggerFlags	flags = kSCLoggerFlagsDefault;

    EAPLogInit();
    if (verbose) {
	flags |= kSCLoggerFlagsFile;
    }
    SCLoggerSetFlags(S_eap_logger, flags);
    return;
}
