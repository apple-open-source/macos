/*
 * Copyright (c) 2003-2010 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOPowerSourcesPrivate_h_
#define _IOPowerSourcesPrivate_h_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/*! 
 *  @header     IOPowerSources.h
 *  @abstract   Functions for interpreting power source info
 *  @discussion Provided as internal, publicly unsupported helper functions.  
 *
 *              Use with caution.
 */

/*! 
 * @function    IOPSCopyInternalBatteriesArray
 * @abstract    Returns a CFArray of all batteries attached to the system.
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @result      NULL if no batteriess are attached to the system. A CFArray of CFTypeRef's that
 *              reference battery descriptions. De-reference each CFTypeRef member of the array
 *              using IOPSGetPowerSourceDescription.
 */
CFArrayRef      IOPSCopyInternalBatteriesArray(CFTypeRef snapshot);

/*! 
 * @function    IOPSCopyUPSArray
 * @abstract    Returns a CFArray of all UPS's attached to the system.
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @result      NULL if no UPS's are attached to the system. A CFArray of CFTypeRef's that
 *              reference UPS descriptions. De-reference each CFTypeRef member of the array
 *              using IOPSGetPowerSourceDescription.
 */
CFArrayRef      IOPSCopyUPSArray(CFTypeRef snapshot);

/*! 
 * @function    IOPSGetActiveBattery
 * @abstract    Returns the active battery.
 * @discussion  Call to determine the active battery on the system. On single battery
 *              systems this returns the 1 battery. On two battery systems this returns a reference
 *              to either battery.
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @result      NULL if no batteries are present, a CFTypeRef indicating the active battery 
 *              otherwise. You must dereference this CFTypeRef with IOPSGetPowerSourceDescription().
 */
CFTypeRef       IOPSGetActiveBattery(CFTypeRef snapshot);

/*! 
 * @function    IOPSGetActiveUPS
 * @abstract    Returns the active UPS. 
 * @discussion  You should call this to determine which UPS the system is connected to.
 *              This is trivial on single UPS systems, but on machines with multiple UPS's attached,
 *              it's important to track which one is actively providing power.
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @result      NULL if no UPS's are present, a CFTypeRef indicating the active UPS otherwise.
 *              You must dereference this CFTypeRef with IOPSGetPowerSourceDescription().
 */
CFTypeRef       IOPSGetActiveUPS(CFTypeRef snapshot);

/*! 
 * @function    IOPSGetProvidingPowerSourceType
 * @abstract    Indicates the power source the computer is currently drawing from.
 * @discussion  Determines which power source is providing power.
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @result      One of: CFSTR(kIOPMACPowerKey), CFSTR(kIOPMBatteryPowerKey), CFSTR(kIOPMUPSPowerKey)
 */
CFStringRef     IOPSGetProvidingPowerSourceType(CFTypeRef snapshot);

/*! 
 * @function    IOPSPowerSourceSupported
 * @abstract    Indicates whether a power source is present on a given system.
 * @discussion  For determining if you should show UPS-specific UI
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @param       type A CFStringRef describing the type of power source (AC Power, Battery Power, UPS Power)
 * @result      kCFBooleanTrue if the power source is supported, kCFBooleanFalse otherwise.
 */
CFBooleanRef    IOPSPowerSourceSupported(CFTypeRef snapshot, CFStringRef type);


/*! 
 *  @typedef    IOPSPowerSourceID
 *  @abstract   An object of type IOPSPowerSourceID refers to a published power source. 
 *  @discussion May be NULL. The IOPSPowerSourceID contains no visible itself; it may
 *              only be passed as an argument to IOPS API.
 */
typedef struct  OpaqueIOPSPowerSourceID *IOPSPowerSourceID;

/*! 
 * @function    IOPSCreatePowerSource
 * @abstract    Call this once per publishable power source to announce the presence of the power source.
 * @discussion  This call will not make the power source visible to the clients of IOPSCopyPowerSourcesInfo();
 *              call IOPSSetPowerSourceDetails to share details.
 * @param       outPS Upon success, this parameter outPS will contain a reference to the new power source.
 *              This reference must be released with IOPSReleasePowerSource when (and if) the power source is no longer available
 *              as a power source to the OS.
 * @param       powerSourceType is the caller's type of power source, defined in IOPSKeys.h
 * @result      Returns kIOReturnSuccess on success, see IOReturn.h for possible failure codes.
 */
IOReturn        IOPSCreatePowerSource(IOPSPowerSourceID *outPS, CFStringRef powerSourceType);

/*! 
 *  @function   IOPSSetPowerSourceDetails
 *  @abstract   Call this when a managed power source's state has substantially changed,
 *              and that state should be reflected in the OS.
 *  @discussion Generally you should not call this more than once a minute - most power sources
 *              change state so slowly that once per minute is enough to provide accurate UI.
 *
 *              You may call this more frequently/immediately on any sudden changes in state,
 *              like sudden removal, or emergency low power warnings.
 *
 * @param       whichPS argument is the IOPSPowerSourceID returned from IOPSCreatePowerSource().
 *              Only the process that created this IOPSPowerSourceID may update its details.
 *
 * @param       details Caller should populate the details dictionary with information describing the power source,
 *              using dictionary keys in IOPSKeys.h
 *              Every dictionary provided here completely replaces any prior published dictionary.
 *  
 * @result      Returns kIOReturnSuccess on success, see IOReturn.h for possible failure codes.
 */
IOReturn        IOPSSetPowerSourceDetails(IOPSPowerSourceID whichPS, CFDictionaryRef details);

/*! 
 * @function    IOPSReleasePowerSource
 * @abstract    Call this when the managed power source has been physically removed from the system,
 *              or is no longer available as a power source.
 *
 * @param       whichPS The whichPS argument is the IOPSPowerSourceID returned from IOPSCreatePowerSource().
 * @result      Returns kIOReturnSuccess on success, see IOReturn.h for possible failure codes.
 */
IOReturn        IOPSReleasePowerSource(IOPSPowerSourceID whichPS);

__END_DECLS

#endif
