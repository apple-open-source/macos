/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/IOTypes.h>

#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBEHCIHubInfo.h"
#include "AppleUSBEHCI.h"

AppleUSBEHCIHubInfoPtr
AppleUSBEHCI::GetHubInfo(UInt8 hubAddr, UInt8 hubPort)
{
    AppleUSBEHCIHubInfoPtr 	hiPtr = _hsHubs;
    
    while (hiPtr)
    {
	if ((hiPtr->hubAddr == hubAddr) && (hiPtr->hubPort == hubPort))
	    break;
	hiPtr = hiPtr->next;
    }
    
    USBLog(5, "%s[%p]::GetHubInfo(%d, %d), returning %p", getName(), this, hubAddr, hubPort, hiPtr);
    
    return hiPtr;
}



AppleUSBEHCIHubInfoPtr
AppleUSBEHCI::NewHubInfo(UInt8 hubAddr, UInt8 hubPort)
{
    AppleUSBEHCIHubInfoPtr 	hiPtr = (AppleUSBEHCIHubInfoPtr)IOMalloc(sizeof(AppleUSBEHCIHubInfo));
    AppleUSBEHCIHubInfoPtr	linkPtr = _hsHubs;
    
    if (!hiPtr)
	return NULL;
    
    hiPtr->hubAddr = hubAddr;
    hiPtr->hubPort = hubPort;
    hiPtr->next = NULL;
    hiPtr->flags = 0;
    hiPtr->bandwidthAvailable = 0;
    
    if (!_hsHubs)
    {
	_hsHubs = hiPtr;
	USBLog(5, "%s[%p]::NewHubInfo(%d, %d), creating new _hsHubs list with %p", getName(), this, hubAddr, hubPort, hiPtr);
    }
    else if ((_hsHubs->hubAddr > hubAddr) || ((_hsHubs->hubAddr == hubAddr) && (_hsHubs->hubPort > hubPort)))
    {
	USBLog(5, "%s[%p]::NewHubInfo(%d, %d), linking new hubInfo %p at beginning of list", getName(), this, hubAddr, hubPort, hiPtr);
	hiPtr->next = _hsHubs;
	_hsHubs = hiPtr;
    }
    else
    {
	while (linkPtr->next && (linkPtr->next->hubAddr < hubAddr))
	    linkPtr = linkPtr->next;

	while (linkPtr->next && (linkPtr->next->hubAddr == hubAddr) && (linkPtr->next->hubPort < hubPort))
	    linkPtr = linkPtr->next;

	USBLog(5, "%s[%p]::NewHubInfo(%d, %d), linking new hubInfo %p between %p and %p", getName(), this, hubAddr, hubPort, hiPtr, linkPtr, linkPtr->next);
	hiPtr->next = linkPtr->next;
	linkPtr->next = hiPtr;
    }
    return hiPtr;
}



IOReturn
AppleUSBEHCI::DeleteHubInfo(UInt8 hubAddr, UInt8 hubPort)
{
    AppleUSBEHCIHubInfoPtr 	hiPtr = _hsHubs;

    // first get rid of the ones off the top
    while (hiPtr && ((hiPtr->hubAddr == hubAddr) && ((hiPtr->hubPort == hubPort) || (hubPort == 0xff))))
    {
	USBLog(5, "%s[%p]::DeleteHubInfo(%d, %d), removing  hubInfo %p from beginning of list", getName(), this, hubAddr, hubPort, hiPtr);
	_hsHubs = hiPtr->next;
	IOFree(hiPtr, sizeof(AppleUSBEHCIHubInfo));
	hiPtr = _hsHubs;
    }
    
    if (!hiPtr)
	return kIOReturnSuccess;		// all done!
	
    if (hiPtr->hubAddr > hubAddr)
	return kIOReturnSuccess;
	
    // now advance to the next candidate
    while (hiPtr->next && (hiPtr->next->hubAddr < hubAddr))
	hiPtr = hiPtr->next;
	
    while (hiPtr->next && (hiPtr->next->hubAddr == hubAddr) && ((hiPtr->next->hubPort == hubPort) || (hubPort == 0xff)))
    {
	AppleUSBEHCIHubInfoPtr 	temp = _hsHubs;
	
	USBLog(5, "%s[%p]::DeleteHubInfo(%d, %d), removing hubInfo %p from between %p and %p", getName(), this, hubAddr, hubPort, hiPtr->next, hiPtr, hiPtr->next->next);
	temp = hiPtr->next;
	hiPtr->next = temp->next;
	IOFree(temp, sizeof(AppleUSBEHCIHubInfo));
    }
	    
    return kIOReturnSuccess;
}

