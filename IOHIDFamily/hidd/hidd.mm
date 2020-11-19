/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDEventSystem.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <CoreFoundation/CFLogUtilities.h>
#include "IOHIDDebug.h"
#include <AssertMacros.h>
#if TARGET_OS_OSX
#import <HIDPreferences/HIDPreferencesHelperListener.h>
#endif

#if TARGET_OS_OSX

static HIDPreferencesHelperListener *__xpcListener  = nil;
#pragma mark - setup xpc helper
static bool setupXPCHelper(void) {
    
    HIDLog("HIDPreferencesHelper Process Start");
    
    __xpcListener = [[HIDPreferencesHelperListener alloc] init];
    if (!__xpcListener) {
        HIDLogError("HIDPreferencesHelper Failed to create service delegate");
        return false;
    }
    
    return true;
    
}
#endif

#pragma mark - load parameters
static void IOHIDEventSystemLoadDefaultParameters (IOHIDEventSystemRef eventSystem) {
  
    io_service_t service = IORegistryEntryFromPath(kIOMasterPortDefault, kIOServicePlane ":/IOResources/IOHIDSystem" );
    if( !service) {
        return;
    }
    CFTypeRef hidParams = IORegistryEntryCreateCFProperty (service, CFSTR(kIOHIDParametersKey), kCFAllocatorDefault, 0);
    if (hidParams && CFDictionaryGetTypeID() == CFGetTypeID(hidParams)) {
        CFDictionaryRemoveValue((CFMutableDictionaryRef)hidParams, CFSTR(kIOHIDKeyboardModifierMappingPairsKey));
        IOHIDEventSystemSetProperty(eventSystem, CFSTR(kIOHIDParametersKey), hidParams);
    }
    if (hidParams) {
        CFRelease(hidParams);
    }
    IOObjectRelease (service);
}

#pragma mark - qos
static bool setQos(void) {
    struct task_qos_policy qosinfo;
    kern_return_t kr = kIOReturnError;
    bool ret = false;
    
    // Make sure this process remains at tier 0 for maximum timer accuracy, since it provides user input events
    memset(&qosinfo, 0, sizeof(qosinfo));
    qosinfo.task_latency_qos_tier = LATENCY_QOS_TIER_0;
    qosinfo.task_throughput_qos_tier = THROUGHPUT_QOS_TIER_0;
    kr = task_policy_set(mach_task_self(), TASK_BASE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
    require_action(kr == kIOReturnSuccess, exit, HIDLogError("Failed to set HIDEventSystem QOS"));
    
    ret = true;
exit:
    return ret;
}

#pragma mark - init hid system
static bool initHIDSystem(void) {
    
    bool ret = false;
    
    IOHIDEventSystemRef eventSystem = IOHIDEventSystemCreate(kCFAllocatorDefault);
    require_action(eventSystem, exit, HIDLogError("Failed to create HIDEventSystem"));
   
    ret = IOHIDEventSystemOpen(eventSystem, NULL, NULL, NULL, 0);
    require_action(ret == true, exit, HIDLogError("Failed to open HIDEventSystem"));
    
    IOHIDEventSystemLoadDefaultParameters (eventSystem);
    
    ret = setQos();
    require(ret == true, exit);
    
    ret = true;
exit:
    if (eventSystem && ret == false) {
        CFRelease(eventSystem);
    }
    
    return ret;
}



#pragma mark -
int main (int argc , const char * argv[]) {
    
    
#if TARGET_OS_OSX
    bool supportEventSystem = false;
    
    if (argc > 1 && strcmp(argv[1], "eventSystem") == 0) {
        supportEventSystem = true;
    }
    
    if (!setupXPCHelper()) {
        HIDLogError("HIDPreferencesHelper(hidd) failed to setup XPC Handler");
        // We should exit now , since no point in having this process around without
        // XPC listener and event system hosted
        if (supportEventSystem == false) {
            exit(EXIT_FAILURE);
        }
    }
    
    if (supportEventSystem) {
        HIDLog("hidd setup event system");
        
        if (!initHIDSystem()) {
            
            if (__xpcListener) {
                [__xpcListener release];
            }
            
            exit(EXIT_FAILURE);
        }
    }
    
    
#else
    
    if (!initHIDSystem()) {
        exit(EXIT_FAILURE);
    }
#endif
    
    CFRunLoopRun();
    
    return 0;
}
