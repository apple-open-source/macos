/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#include "IOFireWireIPUnit.h"
#include "IOFireWireIP.h"
#include <IOKit/IOMessage.h>
#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/firewire/IOConfigDirectory.h>
#include "ip_firewire.h"

OSDefineMetaClassAndStructors(IOFireWireIPUnit, IOService)
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 0);
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 1);
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 2);
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 3);

bool IOFireWireIPUnit::start(IOService *provider)
{
    UInt32   value;
    IOReturn ioStat = kIOReturnSuccess;
    
	fDrb = NULL;
    fDevice = OSDynamicCast(IOFireWireNub, provider);
    
    if(!fDevice)
        return false;

    hwAddr = NULL;
    buf    = NULL;
    dir    = NULL;
    fIPUnitState = false;
	special = false;
    
    fDevice->getConfigDirectoryRef(dir);
    
    if(!dir){
        IOLog("IOFireWireIPunit: directory creation failure\n");
        return false;
    }

    ioStat = dir->getKeyValue(kConfigUnitSpecIdKey, value, NULL);
    
    ioStat = dir->getKeyValue(kConfigUnitSwVersionKey, value, NULL);
	
	// IOLog("%s %u Version 0x%lx\n", __FILE__, __LINE__, value);
    
    if(ioStat == kIOReturnSuccess)
        ioStat = dir->getKeyValue(kConfigUnitDependentInfoKey, buf, NULL);
 
    // IOLog("%d %s %d getkey \n", ioStat, __FILE__, __LINE__);
    
    /* if we see a UnitDependentInfoKey, we realize that we can speak a special language */
    if(ioStat == kIOReturnSuccess){
        hwAddr = (IP1394_HDW_ADDR*)buf->getBytesNoCopy(0,sizeof(IP1394_HDW_ADDR));
    
        if(hwAddr){
            special = true;
		// IOLog("IOFireWireIPUnit GUID = 0x%lx:0x%lx \nmaxRec  = %d \nspeed   = %d  \nFifo Hi = 										0x%x\nFifo Lo = 0x%8lx\n",
        //        hwAddr->eui64.hi, hwAddr->eui64.lo, hwAddr->maxRec, 
        //        hwAddr->spd, hwAddr->unicastFifoHi, hwAddr->unicastFifoLo);
        }
    }
    // end of remote config rom directory  

    if( !IOService::start(provider))
        return (false);

    // Finally enable matching on this object.
    registerService();

    return true;
}

void IOFireWireIPUnit::free(void)
{
    // IOLog("IOFireWireIPUnit:: free\n");
    
    IOService::free();
}

/**
 ** Matching methods
 **/
bool IOFireWireIPUnit::matchPropertyTable(OSDictionary * table)
{
    //
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!IOService::matchPropertyTable(table))  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.
    
	bool res = true;
    // bool res = compareProperty(table, gFireWireVendor_ID) &&
    //    compareProperty(table, gFireWire_GUID);
        
    // IOLog("IOFireWireIPUnit: After Checking Unit, match is %d\n", res);
    
    return res;
}


//
// handleOpen / handleClose
//

bool IOFireWireIPUnit::handleOpen( IOService * forClient, IOOptionBits options, void * arg )
{
	bool ok = false;
	
	if( !isOpen() )
	{
		ok = fDevice->open(this, options, arg);
		if(ok)
			ok = IOService::handleOpen(forClient, options, arg);
	}
	
	return ok;
}

void IOFireWireIPUnit::handleClose( IOService * forClient, IOOptionBits options )
{
	if( isOpen( forClient ) )
	{
		IOService::handleClose(forClient, options);
		fDevice->close(this, options);
	}
}

IOReturn IOFireWireIPUnit::message(UInt32 type, IOService *provider, void *argument)
{
    IOReturn res = kIOReturnUnsupported;

    if( kIOReturnUnsupported == res )
    {
        switch (type)
        {                
            case kIOMessageServiceIsTerminated:
				if(fIPLocalNode != NULL)
					fIPLocalNode->deviceDetach(this);
                res = kIOReturnSuccess;
                break;

            case kIOMessageServiceIsSuspended:
                res = kIOReturnSuccess;
                break;

            case kIOMessageServiceIsResumed:
                res = kIOReturnSuccess;
				updateDrb();
				if(fIPLocalNode != NULL)
				{
					fIPLocalNode->updateBroadcastValues(true);
					fIPLocalNode->updateLinkStatus();
				}
				break;

            default: // default the action to return kIOReturnUnsupported
                break;
        }
    }

    return res;
}

/*!
    @function updateDrb
    @abstract Updates the device reference block in the FireWire IP unit
*/
void IOFireWireIPUnit::updateDrb()
{
	if(fDrb == NULL)
		return;
		
	fDrb->maxSpeed = fDevice->FWSpeed();
	fDrb->maxPayload = fDevice->maxPackLog(true);
	//IOLog(" IPUNIT device false %d and true %d\n", 1 << fDevice->maxPackLog(false), 
	//														1 << fDevice->maxPackLog(true));
}

