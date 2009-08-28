/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IOFIREWIREIPUNIT_H
#define _IOKIT_IOFIREWIREIPUNIT_H

#include <IOKit/IOService.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFWRegs.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include "IOFireWireIP.h"
#include "IOFWIPDefinitions.h"

class IOFireWireNub;
/*!
    @class IOFireWireIPUnit
    @abstract nub for IP devices
*/
class IOFireWireIPUnit : public IOService
{
    OSDeclareDefaultStructors(IOFireWireIPUnit)

protected:
    IOFireWireNub		*fDevice;
    IOFireWireIP		*fIPLocalNode;
	IOFWIPBusInterface	*fFWBusInterface;
	DRB					*fDrb;
	bool				fStarted;
	IONotifier			*fTerminateNotifier;

    
/*! @struct ExpansionData
    @discussion This structure will be used to expand the capablilties of the class in the future.
    */    
    struct ExpansionData { };

/*! @var reserved
    Reserved for future use.  (Internal use only)  */
    ExpansionData *reserved;
    
    virtual void free(void);
    
public:
    // IOService overrides
    virtual bool start(IOService *provider);
	bool finalize(IOOptionBits options);
    virtual IOReturn message(UInt32 type, IOService *provider, void *argument);

/*!
    @function updateDrb
    @abstract Updates the device reference block in the FireWire IP unit
*/
    void updateDrb();
	
	IOFireWireIP *getIPNode(IOFireWireController *control);
	
	bool configureFWBusInterface(IOFireWireController *controller);

	IOFWIPBusInterface *getIPTransmitInterface(IOFireWireIP *fIPLocalNode);

	static bool busInterfaceTerminate(void *target, void *refCon, IOService *newService, IONotifier * notifier);
    
private:
    OSMetaClassDeclareReservedUnused(IOFireWireIPUnit, 0);
    OSMetaClassDeclareReservedUnused(IOFireWireIPUnit, 1);
    OSMetaClassDeclareReservedUnused(IOFireWireIPUnit, 2);
    OSMetaClassDeclareReservedUnused(IOFireWireIPUnit, 3);

};

#endif // _IOKIT_IOFIREWIREIPUNIT_H

