/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import <mach/mach_time.h>

#import "OctagonSignPosts.h"

os_log_t _OctagonSignpostLogSystem(void) {
    static os_log_t log = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log = os_log_create("com.apple.security", "signpost");
    });
    return log;
}

#pragma mark - Signpost Methods

OctagonSignpost _OctagonSignpostCreate(os_log_t subsystem) {
    os_signpost_id_t identifier = os_signpost_id_generate(subsystem);
    uint64_t timestamp = mach_continuous_time();
    return (OctagonSignpost){
        .identifier = identifier,
        .timestamp = timestamp,
    };
}

uint64_t _OctagonSignpostGetNanoseconds(OctagonSignpost signpost) {
    static struct mach_timebase_info timebase_info;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        mach_timebase_info(&timebase_info);
    });

    uint64_t interval = mach_continuous_time() - signpost.timestamp;

    return (uint64_t)(interval *
                      ((double)timebase_info.numer / timebase_info.denom));
}
