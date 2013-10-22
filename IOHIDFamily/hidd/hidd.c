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
#include <mach/mach.h>

int main (int argc __unused, const char * argv[] __unused) 
{
    IOHIDEventSystemRef eventSystem = IOHIDEventSystemCreate(kCFAllocatorDefault);
    struct task_qos_policy qosinfo;
    
    if ( !eventSystem )
        goto finish;
    
    if ( !IOHIDEventSystemOpen(eventSystem, NULL, NULL, NULL, 0) )
        goto finish;
    
    // Make sure this process remains at tier 0 for maximum timer accuracy, since it provides user input events
    memset(&qosinfo, 0, sizeof(qosinfo));
    qosinfo.task_latency_qos_tier = LATENCY_QOS_TIER_0;
    qosinfo.task_throughput_qos_tier = THROUGHPUT_QOS_TIER_0;
    kern_return_t kr = task_policy_set(mach_task_self(), TASK_BASE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
    
    if ( kr != KERN_SUCCESS )
        goto finish;
    
    CFRunLoopRun();
    
finish:
    if ( eventSystem )
        CFRelease(eventSystem);
    
    return 0;
}
