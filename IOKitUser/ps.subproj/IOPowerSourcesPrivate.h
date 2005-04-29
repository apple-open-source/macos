/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOPowerSourcesPrivate_h_
#define _IOPowerSourcesPrivate_h_

#include <CoreFoundation/CoreFoundation.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/* 
    @header IOPowerSources.h
    Functions for interpreting power source info
    Provided as internal, publicly unsupported helper functions. These WILL change
    sometime in the Merlot or Later timeframe as the IOPowerSources API itself evolves.
 
    Use with caution.
 */

/*! @function IOPSCopyInternalBatteriesArray
    @abstract Returns a CFArray of all batteries attached to the system.
    @param snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
    @result NULL if no batteriess are attached to the system. A CFArray of CFTypeRef's that
        reference battery descriptions. De-reference each CFTypeRef member of the array
        using IOPSGetPowerSourceDescription.
*/
extern CFArrayRef           IOPSCopyInternalBatteriesArray(CFTypeRef snapshot);


/*! @function IOPSCopyUPSArray
    @abstract Returns a CFArray of all UPS's attached to the system.
    @param snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
    @result NULL if no UPS's are attached to the system. A CFArray of CFTypeRef's that
        reference UPS descriptions. De-reference each CFTypeRef member of the array
        using IOPSGetPowerSourceDescription.
*/
extern CFArrayRef           IOPSCopyUPSArray(CFTypeRef snapshot);

/*! @function IOPSGetActiveBattery
    @abstract Returns the active battery.
    @discussion Call to determine the active battery on the system. On single battery
        systems this returns the 1 battery. On two battery systems this returns a reference
        to either battery.
    @param snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
    @result NULL if no batteries are present, a CFTypeRef indicating the active battery 
        otherwise. You must dereference this CFTypeRef with IOPSGetPowerSourceDescription().
*/
extern CFTypeRef            IOPSGetActiveBattery(CFTypeRef snapshot);

/*! @function IOPSGetActiveUPS
    @abstract Returns the active UPS. 
    @discussion You should call this to determine which UPS the system is connected to.
        This is trivial on single UPS systems, but on machines with multiple UPS's attached,
        it's important to track which one is actively providing power.
    @param snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
    @result NULL if no UPS's are present, a CFTypeRef indicating the active UPS otherwise.
        You must dereference this CFTypeRef with IOPSGetPowerSourceDescription().
*/
extern CFTypeRef            IOPSGetActiveUPS(CFTypeRef snapshot);

/*! @function IOPSGetProvidingPowerSourceType
    @abstract Indicates the power source the computer is currently drawing from.
    @discussion Determines which power source is providing power.
    @param snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
    @result One of: CFSTR(kIOPMACPowerKey), CFSTR(kIOPMBatteryPowerKey), CFSTR(kIOPMUPSPowerKey)
*/
extern CFStringRef          IOPSGetProvidingPowerSourceType(CFTypeRef snapshot);

/*! @function IOPSPowerSourceSupported
    @abstract Indicates whether a power source is present on a given system.
    @discussion For determining if you should show UPS-specific UI
    @param snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
    @param type A CFStringRef describing the type of power source (AC Power, Battery Power, UPS Power)
    @result kCFBooleanTrue if the power source is supported, kCFBooleanFalse otherwise.
*/
extern CFBooleanRef         IOPSPowerSourceSupported(CFTypeRef snapshot, CFStringRef type);

__END_DECLS

#endif
