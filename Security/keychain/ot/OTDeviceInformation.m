/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#if OCTAGON

#import <utilities/debugging.h>

#import "keychain/ot/OTDeviceInformation.h"

#import "keychain/ot/ObjCImprovements.h"
#import <SystemConfiguration/SystemConfiguration.h>

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#include <MobileGestalt.h>
#else
#include <AppleSystemInfo/AppleSystemInfo.h>
#endif

@implementation OTDeviceInformation

- (instancetype)initForContainerName:(NSString*)containerName
                           contextID:(NSString*)contextID
                               epoch:(uint64_t)epoch
                           machineID:(NSString*)machineID
                             modelID:(NSString*)modelID
                          deviceName:(NSString*)deviceName
                        serialNumber:(NSString*)serialNumber
                           osVersion:(NSString*)osVersion
{
    if((self = [super init])) {
        //otcuttlefish context
        self.containerName = containerName;
        self.contextID = contextID;
        //our epoch
        self.epoch = epoch;

        self.machineID = machineID;
        self.modelID = modelID;
        self.deviceName = deviceName;
        self.serialNumber = serialNumber;
        self.osVersion = osVersion;
    }
    return self;
}

@end

#endif // OCTAGON
