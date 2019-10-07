/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/OT.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTManager.h"

#import "utilities/debugging.h"

void OctagonInitialize(void)
{
    OTManager* manager = [OTManager manager];
    [manager initializeOctagon];
}

// If you want octagon to be initialized in your daemon/tests, you must set this to be true
static bool OctagonPerformInitialization = false;
bool OctagonShouldPerformInitialization(void)
{
    return OctagonPerformInitialization;
}

void OctagonSetShouldPerformInitialization(bool value)
{
    OctagonPerformInitialization = value;
}

void SecOctagon24hrNotification(void) {
#if OCTAGON
    @autoreleasepool {
        [[OTManager manager] xpc24HrNotification:OTCKContainerName context:OTDefaultContext skipRateLimitingCheck:NO reply: ^(NSError * error) {
            if(error){
                secerror("error attempting to check octagon health: %@", error);
            }
        }];
    }
#endif
}
