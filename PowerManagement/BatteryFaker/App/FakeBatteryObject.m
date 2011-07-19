/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
#include <IOKit/pwr_mgt/IOPM.h>
#import "FakeBatteryObject.h"

#define kGoodConf   @"Good Confidence"
#define kFairConf   @"Fair Confidence"
#define kPoorConf   @"Poor Confidence"

#define kGoodHealth   @"Good Health"
#define kFairHealth   @"Fair Health"
#define kPoorHealth   @"Poor Health"


@implementation FakeBatteryObject

- (id)init
{
    if(![super init]) {
        return nil;
    }
    
    properties = [[NSMutableDictionary alloc] init];

    return self;
}

- (void)free
{
    [properties release];
}


- (void)setPropertiesAndUpdate:(NSDictionary *)newProperties
{
    IOReturn ret = kIOReturnSuccess;
    io_registry_entry_t faker_kext_registry_entry;


    if (!newProperties) return;
    
    /* Merge properties in newProperties into properties.
       Do not overwrite proprties dictionaly altgother.
     */

    // Stuff the resulting properties down to the kext.
    
    faker_kext_registry_entry = IOServiceGetMatchingService(
                                MACH_PORT_NULL,
                                IOServiceMatching("BatteryFaker"));
    
    if(!faker_kext_registry_entry) {
        goto exit;
    }
    ret = IORegistryEntrySetCFProperty( faker_kext_registry_entry,
                                CFSTR("Battery Properties"), 
                                (CFTypeRef) newProperties );

exit:
    if(MACH_PORT_NULL != faker_kext_registry_entry) {
        IOObjectRelease(faker_kext_registry_entry);
    }
}

- (NSDictionary *)properties
{
    if(!properties) {
        return [NSDictionary dictionary];
    }
    
    return properties;
}

@end
