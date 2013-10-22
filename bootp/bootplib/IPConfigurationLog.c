/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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
 * IPConfigurationLog.c
 * - logging related functions
 */

/* 
 * Modification History
 *
 * March 25, 2013		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <SystemConfiguration/SCPrivate.h>
#include "symbol_scope.h"
#include "IPConfigurationLog.h"

/*
 * kIPConfigurationLoggerID
 * - used with SCLoggerCreate to identify which ASL logging module to use
 */
#define kIPConfigurationLoggerID CFSTR("com.apple.networking.IPConfiguration")

STATIC SCLoggerRef	S_logger;

STATIC void
IPConfigurationLogInit(void)
{
    if (S_logger == NULL) {
	S_logger = SCLoggerCreate(kIPConfigurationLoggerID);
    }
    return;
}

PRIVATE_EXTERN void
IPConfigurationLog(int level, CFStringRef format, ...)
{
    va_list	args;

    IPConfigurationLogInit();
    va_start(args, format);
    SCLoggerVLog(S_logger, level, format, args);
    va_end(args);
    return;
}

PRIVATE_EXTERN void
IPConfigurationLogSetVerbose(bool verbose)
{
    SCLoggerFlags	flags = kSCLoggerFlagsDefault;

    IPConfigurationLogInit();
    if (verbose) {
	flags |= kSCLoggerFlagsFile;
    }
    SCLoggerSetFlags(S_logger, flags);
    return;
}
