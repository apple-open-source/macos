/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
 
#include <IOKit/IOLib.h>
#include <IOKit/firewire/IOFireWireController.h>
#include "IOFireWireAVCRequestSpace.h"
#include "IOFireWireAVCConsts.h"

OSDefineMetaClassAndStructors(IOFireWireAVCRequestSpace, IOFWPseudoAddressSpace)
OSMetaClassDefineReservedUnused(IOFireWireAVCRequestSpace, 0);
OSMetaClassDefineReservedUnused(IOFireWireAVCRequestSpace, 1);
OSMetaClassDefineReservedUnused(IOFireWireAVCRequestSpace, 2);
OSMetaClassDefineReservedUnused(IOFireWireAVCRequestSpace, 3);

bool IOFireWireAVCRequestSpace::init(IOFireWireBus *bus, UInt32 subUnitType, UInt32 subUnitID,
                                                            FWWriteCallback writer, void * refcon)
{
    if(!IOFWPseudoAddressSpace::initFixed(bus, 
            FWAddress(kCSRRegisterSpaceBaseAddressHi, kFCPCommandAddress),
            512, NULL, writer, refcon))
        return false;
    fUnitAddr = IOAVCAddress(subUnitType, subUnitID);
    return true;
}

IOFireWireAVCRequestSpace *
IOFireWireAVCRequestSpace::withSubUnit(IOFireWireBus *bus, UInt32 subUnitType, UInt32 subUnitID,
                                                            FWWriteCallback writer, void * refcon)
{
    IOFireWireAVCRequestSpace *me;
    me = new IOFireWireAVCRequestSpace;
    if(me) {
        if(!me->init(bus, subUnitType, subUnitID, writer, refcon)) {
            me->release();
            me = NULL;
        }
    }
    return me;
}

UInt32 IOFireWireAVCRequestSpace::doWrite(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
                           const void *buf, IOFWRequestRefCon refcon)
{
    const UInt8 *hdr = (UInt8 *)buf;
    if(addr.addressHi != kCSRRegisterSpaceBaseAddressHi)
        return kFWResponseAddressError;
    if(addr.addressLo != kFCPCommandAddress)
        return kFWResponseAddressError;
    
    IOLog("IOFireWireAVCRequestSpace write, header %x len %d\n", *(const UInt32 *)buf, len);

    if(hdr[0] & 0xf0)
        return kFWResponseAddressError;	// Wrong cts

	// Don't check for a match on (sub)unit type/id if fUnitAddr is 0xEE
    if (fUnitAddr != 0xEE)    
		if(hdr[1] != fUnitAddr)
			return kFWResponseAddressError;	// Wrong subunit
    
    return fWriter(fRefCon, nodeID, speed, addr, len, buf, refcon);
}

