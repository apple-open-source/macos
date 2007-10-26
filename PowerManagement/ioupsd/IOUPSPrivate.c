/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include <CoreFoundation/CoreFoundation.h>

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>

#include <libc.h>
#include <servers/bootstrap.h>
#include <sysexits.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFURLAccess.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOMessage.h>
#include <IOKit/ps/IOPSKeys.h>

#include "IOUPSPrivate.h"
#include "IOUPSPlugIn.h"

Boolean IOUPSMIGServerIsRunning(mach_port_t * bootstrap_port_ref, mach_port_t * upsd_port_ref)
{
    boolean_t active = FALSE;
    Boolean result = false;
    kern_return_t kern_result = KERN_SUCCESS;
    mach_port_t   bootstrap_port;

    if (bootstrap_port_ref && (*bootstrap_port_ref != MACH_PORT_NULL)) {
        bootstrap_port = *bootstrap_port_ref;
    } else {
        /* Get the bootstrap server port */
        kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
        if (kern_result != KERN_SUCCESS) {
            return FALSE;
        }
        if (bootstrap_port_ref) {
            *bootstrap_port_ref = bootstrap_port;
        }
    }

    /* Check "upsd" server status */
    kern_result = bootstrap_status(bootstrap_port, kIOUPSPlugInServerName, &active);
    switch (kern_result) {
      case BOOTSTRAP_SUCCESS:
        if (active) {
            result = true;

            if (upsd_port_ref)
            {
                bootstrap_look_up(bootstrap_port, kIOUPSPlugInServerName, upsd_port_ref);
            }
            goto finish;
        }
        break;

      case BOOTSTRAP_UNKNOWN_SERVICE:
        result = false;
        goto finish;
        break;

      default:
        return FALSE;
    }

finish:
    return result;
}

IOReturn IOUPSSendCommand(mach_port_t connect, int upsID, CFDictionaryRef command)
{
    IOReturn 		ret;
    CFDataRef		serializedData;

    if (!connect || !command)
        return kIOReturnBadArgument;

    serializedData = (CFDataRef)IOCFSerialize( command, kNilOptions );

    if (!serializedData)
        return kIOReturnError;
        
    ret = io_ups_send_command(connect, upsID, CFDataGetBytePtr(serializedData), CFDataGetLength(serializedData));
        
    CFRelease( serializedData );

    return ret;
}

IOReturn IOUPSGetEvent(mach_port_t connect, int upsID, CFDictionaryRef *event)
{
    IOReturn        ret;
    void *          buffer = NULL;
    IOByteCount     bufferSize;

    if (!connect || !event)
        return kIOReturnBadArgument;

    ret = io_ups_get_event(connect, upsID, &buffer, &bufferSize);
    
    if ( ret != kIOReturnSuccess )
        return ret;

    *event = IOCFUnserialize(buffer, kCFAllocatorDefault, kNilOptions, NULL);

    vm_deallocate(mach_task_self(), (vm_address_t)buffer, bufferSize);
    
    return ret;
}

IOReturn IOUPSGetCapabilities(mach_port_t connect, int upsID, CFSetRef *capabilities)
{
    IOReturn 		ret;
    void *		buffer = NULL;
    IOByteCount		bufferSize;

    if (!connect || !capabilities)
        return kIOReturnBadArgument;

    ret = io_ups_get_capabilities(connect, upsID, &buffer, &bufferSize);
    
    if ( ret != kIOReturnSuccess )
        return ret;

    *capabilities = IOCFUnserialize(buffer, kCFAllocatorDefault, kNilOptions, NULL);

    vm_deallocate(mach_task_self(), (vm_address_t)buffer, bufferSize);

    return ret;
}

