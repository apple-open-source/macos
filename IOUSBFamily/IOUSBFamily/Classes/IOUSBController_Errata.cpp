/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <IOKit/system.h>


#include <IOKit/usb/IOUSBController.h>

#define super IOUSBBus
#define self this

/*
 This table contains the list of errata that are necessary for known
 problems with particular silicon.  The format is vendorID, revisionID,
 lowest revisionID needing errata, highest rev needing errata, errataBits.
 The result of all matches is ORed together, so more than one entry may
 match.  Typically for a given errata a list of chips revisions that
 this applies to is supplied.
 */
static ErrataListEntry  errataList[] = {
    
    {0x1095, 0x0670, 0, 0x0004,	kErrataCMDDisableTestMode | kErrataOnlySinglePageTransfers | kErrataRetryBufferUnderruns}, // CMD 670 & 670a (revs 0-4)
    {0x1045, 0xc861, 0, 0x001f, kErrataLSHSOpti},									// Opti 1045
    {0x11C1, 0x5801, 0, 0xffff, kErrataDisableOvercurrent | kErrataLucentSuspendResume | kErrataNeedsWatchdogTimer},    // Lucent USS 302
    {0x11C1, 0x5802, 0, 0xffff, kErrataDisableOvercurrent | kErrataLucentSuspendResume | kErrataNeedsWatchdogTimer}, 	// Lucent USS 312
    {0x106b, 0x0019, 0, 0xffff, kErrataDisableOvercurrent | kErrataNeedsWatchdogTimer}, 				// Apple KeyLargo - all revs
    {0x106b, 0x0019, 0, 0, 	kErrataLucentSuspendResume }, 								// Apple KeyLargo - USB Rev 0 only
    {0x106b, 0x0026, 0, 0xffff, kErrataDisableOvercurrent | kErrataLucentSuspendResume | kErrataNeedsWatchdogTimer}, 	// Apple Pangea, all revs
    {0x106b, 0x003f, 0, 0xffff, kErrataDisableOvercurrent}, 								// Apple Intrepid, all revs
    {0x1033, 0x0035, 0, 0xffff, kErrataDisableOvercurrent }								// NEC
};

#define errataListLength (sizeof(errataList)/sizeof(ErrataListEntry))

UInt32 IOUSBController::GetErrataBits(UInt16 vendorID, UInt16 deviceID, UInt16 revisionID)
{
    ErrataListEntry	*entryPtr;
    UInt32		i, errata = 0;

    for(i = 0, entryPtr = errataList; i < errataListLength; i++, entryPtr++)
    {
        if (vendorID == entryPtr->vendID &&
            deviceID == entryPtr->deviceID &&
            revisionID >= entryPtr->revisionLo &&
            revisionID <= entryPtr->revisionHi)
        {
            // we match, add this errata to our list
            errata |= entryPtr->errata;
        }
    }
    return(errata);
}       


