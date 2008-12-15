/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <IOKit/system.h>


#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBLog.h>

#define super IOUSBBus
#define self this

/*
 This table contains the list of errata that are necessary for known
 problems with particular silicon.  The format is vendorID, deviceID,
 lowest revisionID needing errata, highest rev needing errata, errataBits.
 The result of all matches is ORed together, so more than one entry may
 match.  Typically for a given errata a list of chips revisions that
 this applies to is supplied.
 */
static ErrataListEntry  errataList[] = {
    
    {0x1095, 0x0670, 0, 0x0004,	kErrataCMDDisableTestMode | kErrataOnlySinglePageTransfers | kErrataRetryBufferUnderruns},	// CMD 670 & 670a (revs 0-4)
    {0x1045, 0xc861, 0, 0x001f, kErrataLSHSOpti},																			// Opti 1045
    {0x11C1, 0x5801, 0, 0xffff, kErrataDisableOvercurrent | kErrataLucentSuspendResume | kErrataNeedsWatchdogTimer},		// Lucent USS 302
    {0x11C1, 0x5802, 0, 0xffff, kErrataDisableOvercurrent | kErrataLucentSuspendResume | kErrataNeedsWatchdogTimer},		// Lucent USS 312
    {0x106b, 0x0019, 0, 0xffff, kErrataDisableOvercurrent | kErrataNeedsWatchdogTimer},										// Apple KeyLargo - all revs
    {0x106b, 0x0019, 0, 0, 	kErrataLucentSuspendResume },																	// Apple KeyLargo - USB Rev 0 only
    {0x106b, 0x0026, 0, 0xffff, kErrataDisableOvercurrent | kErrataLucentSuspendResume | kErrataNeedsWatchdogTimer},		// Apple Pangea, all revs
    {0x106b, 0x003f, 0, 0xffff, kErrataDisableOvercurrent | kErrataNeedsWatchdogTimer},										// Apple Intrepid, all revs
    {0x1033, 0x0035, 0, 0xffff, kErrataDisableOvercurrent | kErrataNECOHCIIsochWraparound | kErrataNECIncompleteWrite },	// NEC OHCI
    {0x1033, 0x00e0, 0, 0xffff, kErrataDisableOvercurrent | kErrataNECIncompleteWrite},										// NEC EHCI
    {0x1131, 0x1561, 0x30, 0x30, kErrataNeedsPortPowerOff },																// Philips, USB 2
    {0x11C1, 0x5805, 0x11, 0x11, kErrataAgereEHCIAsyncSched },																// Agere, Async Schedule bug
	
	{0x8086, 0x2658, 0x03, 0x04, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// ICH6 UHCI #1
	{0x8086, 0x2659, 0x03, 0x04, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// ICH6 UHCI #2
	{0x8086, 0x265A, 0x03, 0x04, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// ICH6 UHCI #3
	{0x8086, 0x265B, 0x03, 0x04, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// ICH6 UHCI #4
	{0x8086, 0x265C, 0x03, 0x04, kErrataICH6PowerSequencing | kErrataNeedsOvercurrentDebounce },									// ICH6 EHCI
	
	{0x8086, 0x2688, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// Southbridge UHCI #1
	{0x8086, 0x2689, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// Southbridge UHCI #2
	{0x8086, 0x268A, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// Southbridge UHCI #3
	{0x8086, 0x268B, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// Southbridge UHCI #4
	{0x8086, 0x268C, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataNeedsOvercurrentDebounce },									// Southbridge EHCI
	
	{0x8086, 0x27C8, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// ICH7 UHCI #1
	{0x8086, 0x27C9, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// ICH7 UHCI #2
	{0x8086, 0x27CA, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// ICH7 UHCI #3
	{0x8086, 0x27CB, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },	// ICH7 UHCI #4
	{0x8086, 0x27CC, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataICH7ISTBuffer  | kErrataNeedsOvercurrentDebounce },			// ICH7 EHCI

	{0x8086, 0x2830, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH8 UHCI #1
	{0x8086, 0x2831, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH8 UHCI #2
	{0x8086, 0x2832, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH8 UHCI #3
	{0x8086, 0x2834, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH8 UHCI #4
	{0x8086, 0x2835, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH8 UHCI #5
	{0x8086, 0x2836, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataNeedsOvercurrentDebounce },			// ICH8 EHCI #1
	{0x8086, 0x283a, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataNeedsOvercurrentDebounce },			// ICH8 EHCI #2

	{0x8086, 0x8114, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // Poulsbo UHCI #1
	{0x8086, 0x8115, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // Poulsbo UHCI #2
	{0x8086, 0x8116, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // Poulsbo UHCI #3
	{0x8086, 0x8117, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataNeedsOvercurrentDebounce }	,			// Pouslbo EHCI #2

	{0x10de, 0x0aa6, 0x00, 0xff, kErrataMCP79SplitIsoch | kErrataMissingPortChangeInt | kErrataMCP79IgnoreDisconnect | kErrataUse32bitEHCI },			// MCP79 EHCI #1
	{0x10de, 0x0aa9, 0x00, 0xff, kErrataMCP79SplitIsoch | kErrataMissingPortChangeInt | kErrataMCP79IgnoreDisconnect | kErrataUse32bitEHCI},			// MCP79 EHCI #2
	{0x10de, 0x0aa5, 0x00, 0xff, kErrataOHCINoGlobalSuspendOnSleep | kErrataMCP79IgnoreDisconnect },														// MCP79 OHCI #1
	{0x10de, 0x0aa7, 0x00, 0xff, kErrataOHCINoGlobalSuspendOnSleep | kErrataMCP79IgnoreDisconnect },														// MCP79 OHCI #2

	{0x8086, 0x3a34, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH10 UHCI #1
	{0x8086, 0x3a35, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH10 UHCI #2
	{0x8086, 0x3a36, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH10 UHCI #3
	{0x8086, 0x3a37, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH10 UHCI #4
	{0x8086, 0x3a38, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH10 UHCI #5
	{0x8086, 0x3a39, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataUHCISupportsOvercurrent | kErrataNeedsOvercurrentDebounce | kErrataSupportsPortResumeEnable },   // ICH10 UHCI #6
	{0x8086, 0x3a3a, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataNeedsOvercurrentDebounce },			// ICH10 EHCI #1
	{0x8086, 0x3a3c, 0x00, 0xff, kErrataICH6PowerSequencing | kErrataNeedsOvercurrentDebounce }				// ICH10 EHCI #2

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
	//USBError(1, "Errata bits for controller 0x%x/0x%x(rev 0x%x) are 0x%x", vendorID, deviceID, revisionID, errata);
    return(errata);
}       


