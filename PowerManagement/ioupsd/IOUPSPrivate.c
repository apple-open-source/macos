/*
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

// mig generated header
#include "ioupspluginmig.h"

Boolean IOUPSMIGServerIsRunning(mach_port_t * bootstrap_port_ref, mach_port_t * upsd_port_ref)
{
    mach_port_t     active = MACH_PORT_NULL;
    kern_return_t   kern_result = KERN_SUCCESS;
    mach_port_t     bootstrap_port;

    if (bootstrap_port_ref && (*bootstrap_port_ref != MACH_PORT_NULL)) {
        bootstrap_port = *bootstrap_port_ref;
    } else {
        /* Get the bootstrap server port */
        kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
        if (kern_result != KERN_SUCCESS) {
            return false;
        }
        if (bootstrap_port_ref) {
            *bootstrap_port_ref = bootstrap_port;
        }
    }

    /* Check "upsd" server status */
    kern_result = bootstrap_look_up(
                        bootstrap_port, 
                        kIOUPSPlugInServerName, 
                        &active);

    if (BOOTSTRAP_SUCCESS == kern_result) {
        return true;
    } else {
        // For any result other than SUCCESS, we presume the server is
        // not running. We expect the most common failure result to be:
        // kern_result == BOOTSTRAP_UNKNOWN_SERVICE
        return false;
    }
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
        
    ret = io_ups_send_command(connect, upsID, 
                (vm_offset_t)CFDataGetBytePtr(serializedData), 
                (mach_msg_type_number_t) CFDataGetLength(serializedData));
        
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

    ret = io_ups_get_event(connect, upsID, 
                (vm_offset_t *)&buffer, 
                (mach_msg_type_number_t *)&bufferSize);
    
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

    ret = io_ups_get_capabilities(connect, upsID, 
                (vm_offset_t *)&buffer, 
                (mach_msg_type_number_t *)&bufferSize);
    
    if ( ret != kIOReturnSuccess )
        return ret;

    *capabilities = IOCFUnserialize(buffer, kCFAllocatorDefault, kNilOptions, NULL);

    vm_deallocate(mach_task_self(), (vm_address_t)buffer, bufferSize);

    return ret;
}

