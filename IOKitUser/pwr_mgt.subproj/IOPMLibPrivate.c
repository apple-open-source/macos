/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/vm_map.h>
#include <servers/bootstrap.h>
#include <SystemConfiguration/SCValidation.h>
#include "IOPMLibPrivate.h"
#include "powermanagement.h"

#define kAssertionsArraySize        5

static IOReturn _pm_connect(mach_port_t *newConnection)
{
    kern_return_t       kern_result = KERN_SUCCESS;
    
    if(!newConnection) return kIOReturnBadArgument;

    // open reference to PM configd
    kern_result = bootstrap_look_up(bootstrap_port, 
            kIOPMServerBootstrapName, newConnection);    
    if(KERN_SUCCESS != kern_result) {
        return kIOReturnError;
    }
    return kIOReturnSuccess;
}

static IOReturn _pm_disconnect(mach_port_t connection)
{
    if(!connection) return kIOReturnBadArgument;
    mach_port_destroy(mach_task_self(), connection);
    return kIOReturnSuccess;
}


IOReturn IOPMAssertionCreate(CFStringRef  assertion, int level, IOPMAssertionID *assertion_id)
{
    IOReturn                return_code = kIOReturnError;
    kern_return_t           kern_result = KERN_SUCCESS;
    char                    profile_str[50];
    int                     profile_len = 50;
    mach_port_t             pm_server = MACH_PORT_NULL;
    mach_port_t             task_self = mach_task_self();
    IOReturn                err;

    // Set assertion_id to a known invalid setting. If successful, it will
    // get a valid value later on.
    *assertion_id = -1;

    // The only supported assertion is kIOPMCPUBoundAssertion
    if(kCFCompareEqualTo != 
       CFStringCompare(assertion, kIOPMCPUBoundAssertion, 0)) 
    {
        return kIOReturnUnsupported;
    }

    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    CFStringGetCString(assertion, profile_str, profile_len, kCFStringEncodingMacRoman);
    profile_len = strlen(profile_str)+1;

    // io_pm_assertion_create mig's over to configd, and it's configd 
    // that actively tracks and manages the list of active power profiles.
    kern_result = io_pm_assertion_create(
            pm_server, 
            task_self,
            profile_str,
            profile_len,
            level, 
            assertion_id,
            &return_code);
            
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }

    _pm_disconnect(pm_server);
exit:
    return return_code;
}

IOReturn IOPMAssertionRelease(IOPMAssertionID assertion_id)
{
    IOReturn                return_code = kIOReturnError;
    kern_return_t           kern_result = KERN_SUCCESS;
    mach_port_t             pm_server = MACH_PORT_NULL;
    mach_port_t             task_self = mach_task_self();
    IOReturn                err;

    if((assertion_id < 0) || (assertion_id >= kAssertionsArraySize)) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    kern_result = io_pm_assertion_release(
            pm_server, 
            task_self,
            assertion_id,
            &return_code);
            
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }

    _pm_disconnect(pm_server);
exit:
    return return_code;
}

IOReturn IOPMCopyAssertionsByProcess(CFDictionaryRef *assertions_by_pid)
{
    IOReturn                return_code = kIOReturnError;
    kern_return_t           kern_result = KERN_SUCCESS;
    CFDictionaryRef         activeProfiles = NULL;
    mach_port_t             pm_server = MACH_PORT_NULL;
    void                    *out_buf;
    int                     buf_size;
    IOReturn                err;

    *assertions_by_pid = NULL;

    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    kern_result = io_pm_copy_active_assertions(
            pm_server,
            &out_buf,
            &buf_size);
            
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    activeProfiles = (CFDictionaryRef)IOCFUnserialize(out_buf, 0, 0, 0);
    if(isA_CFDictionary(activeProfiles)) 
    {
        CFMutableDictionaryRef      ret_dict;
        CFStringRef         *pid_keys;
        char                *endptr;
        CFArrayRef          *arr_vals;
        CFNumberRef         tmp_pid;
        char                pid_str[10];
        int                 the_pid;
        int                 dict_count;
        int                 i;

        // hackaround: Turn CFStringRef pids back into CFNumberRefs
        //   (workaround for IOCFSerialize bug)
        ret_dict = CFDictionaryCreateMutable(0, 0, 
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        dict_count = CFDictionaryGetCount(activeProfiles);
        pid_keys = (CFStringRef *)malloc(dict_count * sizeof(CFStringRef));
        arr_vals = (CFArrayRef *)malloc(dict_count * sizeof(CFArrayRef));
        if(!pid_keys || !arr_vals) goto exit;
        CFDictionaryGetKeysAndValues(
                        activeProfiles, 
                        (const void **)pid_keys, 
                        (const void **)arr_vals);
        for(i=0; i<dict_count; i++)
        {
            if(!pid_keys[i]) continue;
            CFStringGetCString(pid_keys[i], pid_str, 10, kCFStringEncodingMacRoman);
            the_pid = strtol(pid_str, &endptr, 10);
            if(0 != *endptr) continue;
            tmp_pid = CFNumberCreate(0, kCFNumberIntType, &the_pid);
            CFDictionarySetValue(ret_dict, tmp_pid, arr_vals[i]);
        }
        free(pid_keys);
        free(arr_vals);
        CFRelease(activeProfiles);
        // end of hack
        
        *assertions_by_pid = ret_dict;
        return_code = kIOReturnSuccess;
    } else {
        if(activeProfiles) CFRelease(activeProfiles);
    }

    vm_deallocate(mach_task_self(), (vm_address_t)out_buf, buf_size);

    _pm_disconnect(pm_server);
    return_code = kIOReturnSuccess;
exit:
    return return_code;
}

IOReturn IOPMCopyAssertionsStatus(CFDictionaryRef *assertions_status)
{
    IOReturn                return_code = kIOReturnError;
    kern_return_t           kern_result = KERN_SUCCESS;
    CFDictionaryRef         supported_assertions = NULL;
    mach_port_t             pm_server = MACH_PORT_NULL;
    void                    *out_buf = NULL;
    int                     buf_size = NULL;
    IOReturn                err;

    *assertions_status = NULL;

    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    kern_result = io_pm_copy_assertions_status(
            pm_server,
            &out_buf,
            &buf_size);
            
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    supported_assertions = (CFDictionaryRef)IOCFUnserialize(out_buf, 0, 0, 0);
    if(isA_CFDictionary(supported_assertions)) {
        *assertions_status = supported_assertions;
        return_code = kIOReturnSuccess;
    } else 
    {
        if(supported_assertions) CFRelease(supported_assertions);
        return_code = kIOReturnInternalError;
    }

    vm_deallocate(mach_task_self(), (vm_address_t)out_buf, buf_size);

    _pm_disconnect(pm_server);    
exit:
    return return_code;
}

