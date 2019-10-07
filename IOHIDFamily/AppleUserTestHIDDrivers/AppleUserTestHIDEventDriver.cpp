//
//  AppleUserTestHIDEventDriver.cpp
//  AppleUserTestHIDDrivers
//
//  Created by dekom on 1/14/19.
//

#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <mach/mach_time.h>
#include <libkern/OSAtomic.h>
#include <IOKit/IOUserServer.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/IOUserHIDEventDriver.h>

#include "Implementation/IOKitUser/AppleUserTestHIDEventDriver.h"

#undef  super
#define super IOUserHIDEventDriver

#include <os/log.h>

kern_return_t
IMPL(AppleUserTestHIDEventDriver, Start)
{
    kern_return_t  ret;
    
    //printf("AppleUserHIDEventDriver start");
    os_log_error(OS_LOG_DEFAULT, "AppleUserTestHIDEventDriver calling start");
    
    ret = Start(provider, SUPERDISPATCH);
    
    os_log_error(OS_LOG_DEFAULT, "AppleUserTestHIDEventDriver start ret: 0x%x", ret);
    
    return ret;
}

