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

#ifndef _IOKIT_PM_IOUPSPRIVATE_H
#define _IOKIT_PM_IOUPSPRIVATE_H

#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <IOKit/IOReturn.h>

/*!
    @defined kIOUPSDeviceKey
    @abstract Key for IOService object that denotes a UPS device.
    @discussion It is expected that every IOService module that contains
    a IOUPSCFPlugIn will at least define this key in it property table.
*/
#define kIOUPSDeviceKey			"UPSDevice"

/*!
    @defined kIOUPSPlugInServerName
    @abstract Key for UPS Mig server.
    @discussion Used for identifying UPS mig server.
*/
#define kIOUPSPlugInServerName		"com.apple.IOUPSPlugInServer" 


Boolean IOUPSMIGServerIsRunning(mach_port_t * bootstrap_port_ref, mach_port_t * upsd_port_ref);

IOReturn IOUPSSendCommand(mach_port_t connect, int upsID, CFDictionaryRef command);

IOReturn IOUPSGetEvent(mach_port_t connect, int upsID, CFDictionaryRef *event);

IOReturn IOUPSGetCapabilities(mach_port_t connect, int upsID, CFSetRef *capabilities);

#endif /* !_IOKIT_PM_IOUPSPRIVATE_H */
