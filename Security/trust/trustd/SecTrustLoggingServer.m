/*
 * Copyright (c) 2016-2024 Apple Inc. All Rights Reserved.
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
 *
 * SecTrustLoggingServer.c - logging for certificate trust evaluation engine
 *
 */

#include <AssertMacros.h>
#include "SecTrustLoggingServer.h"


uint64_t TimeSinceSystemStartup(void) {
    struct timespec uptime;
    clock_gettime(CLOCK_UPTIME_RAW, &uptime);
    return (uint64_t)uptime.tv_sec;
}

uint64_t TimeSinceProcessLaunch(void) {
    return mach_absolute_time() - launchTime;
}

int64_t TimeUntilProcessUptime(int64_t uptime_nsecs) {
    int64_t uptime = (int64_t)TimeSinceProcessLaunch();
    if (uptime > 0 && uptime < uptime_nsecs) {
        return uptime_nsecs - uptime;
    }
    return 0;
}
