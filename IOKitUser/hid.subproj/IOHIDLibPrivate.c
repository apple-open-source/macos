/*
 * Copyright (c) 1999-2008 Apple Computer, Inc.  All Rights Reserved.
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

#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <stdarg.h>
#include <asl.h>
#include <mach/mach_time.h>

#include "IOHIDLibPrivate.h"
#include "IOHIDBase.h"

void _IOObjectCFRelease(        CFAllocatorRef          allocator  __unused, 
                                const void *            value)
{
    IOObjectRelease((io_object_t)(uintptr_t) value);
}

const void * _IOObjectCFRetain( CFAllocatorRef          allocator  __unused, 
                                const void *            value)
{
    if (kIOReturnSuccess != IOObjectRetain((io_object_t)(uintptr_t)value))
        return NULL;
    
    return value;
}

void _IOHIDCallbackApplier(const void *callback, 
                           const void *callbackContext, 
                           void *applierContext)
{
    IOHIDCallbackApplierContext *context = (IOHIDCallbackApplierContext*)applierContext;
    if (callback && context)
        ((IOHIDCallback)callback)((void *)callbackContext, context->result, context->sender);
}

//------------------------------------------------------------------------------
// _IOHIDLog
//------------------------------------------------------------------------------
os_log_t _IOHIDLog(void)
{
    static os_log_t log = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log = os_log_create("com.apple.iohid", "default");
    });
    return log;
}

//------------------------------------------------------------------------------
// _IOHIDLogCategory
//------------------------------------------------------------------------------
os_log_t _IOHIDLogCategory(IOHIDLogCategory category)
{
    assert(category < kIOHIDLogCategoryCount);
    static os_log_t log[kIOHIDLogCategoryCount];
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log[kIOHIDLogCategoryDefault]       = os_log_create(kIOHIDLogSubsytem, "default");
        log[kIOHIDLogCategoryTrace]         = os_log_create(kIOHIDLogSubsytem, "trace");
        log[kIOHIDLogCategoryProperty]      = os_log_create(kIOHIDLogSubsytem, "property");
        log[kIOHIDLogCategoryActivity]      = os_log_create(kIOHIDLogSubsytem, "activity");
    });
    return log[category];
}

//------------------------------------------------------------------------------
// _IOHIDUnitlCopyTimeString
//------------------------------------------------------------------------------
CFStringRef _IOHIDCreateTimeString(struct timeval *tv)
{
    struct tm tmd;
    struct tm *local_time;
    char time_str[32] = { 0, };

    local_time = localtime_r(&tv->tv_sec, &tmd);
    if (local_time == NULL) {
        local_time = gmtime_r(&tv->tv_sec, &tmd);
    }

    if (local_time) {
        strftime(time_str, sizeof(time_str), "%F %H:%M:%S", local_time);
    }
  
  
    CFStringRef time = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s.%06d"), time_str, tv->tv_usec);
    return time;
}

//------------------------------------------------------------------------------
// _IOHIDGetMonotonicTime (in ns)
//------------------------------------------------------------------------------
uint64_t  _IOHIDGetMonotonicTime () {
    static mach_timebase_info_data_t    timebaseInfo;

    if (timebaseInfo.denom == 0)
        mach_timebase_info(&timebaseInfo);

    return ((mach_absolute_time( ) * timebaseInfo.numer) / timebaseInfo.denom);
}
