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
#include "IOFireWireAVCConsts.h"
#include "IOFireWirePCRSpace.h"

OSDefineMetaClassAndStructors(IOFireWirePCRSpace, IOFWPseudoAddressSpace)
OSMetaClassDefineReservedUnused(IOFireWirePCRSpace, 0);
OSMetaClassDefineReservedUnused(IOFireWirePCRSpace, 1);
OSMetaClassDefineReservedUnused(IOFireWirePCRSpace, 2);
OSMetaClassDefineReservedUnused(IOFireWirePCRSpace, 3);

bool IOFireWirePCRSpace::init(IOFireWireBus *bus)
{
    if(!IOFWPseudoAddressSpace::initFixed(bus, 
            FWAddress(kCSRRegisterSpaceBaseAddressHi, kPCRBaseAddress),
            sizeof(fBuf), simpleReader, NULL, this))
        return false;

    fDesc = IOMemoryDescriptor::withAddress(fBuf, sizeof(fBuf), kIODirectionOutIn);
    if (fDesc == NULL) {
        return false;
    }

	// Output Master Control - 400 Mbit, broadcast channel base 63, 31 output plugs
    fBuf[0] = (2 << kIOFWPCRDataRatePhase) |
                (63 << kIOFWPCRBroadcastBasePhase) |
                (0xff << kIOFWPCRExtensionPhase) |
                (31 << kIOFWPCRNumPlugsPhase);
                
	// Input Master Control - 400 Mbit, 31 output plugs
    fBuf[32] = (2 << kIOFWPCRDataRatePhase) |
                (0xff << kIOFWPCRExtensionPhase) |
                (31 << kIOFWPCRNumPlugsPhase);
    
    return true;
}

IOFireWirePCRSpace * IOFireWirePCRSpace::getPCRAddressSpace(IOFireWireBus *bus)
{
    IOFWAddressSpace *existing;
    IOFireWirePCRSpace *space;
    existing = bus->getAddressSpace(FWAddress(kCSRRegisterSpaceBaseAddressHi, kPCRBaseAddress));
    if(existing && OSDynamicCast(IOFireWirePCRSpace, existing)) {
        existing->retain();
        return OSDynamicCast(IOFireWirePCRSpace, existing);
    }
    space = new IOFireWirePCRSpace;
    if(space) {
        if(!space->init(bus)) {
            space->release();
            space = NULL;
        }
    }
    return space;
}


UInt32 IOFireWirePCRSpace::doWrite(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
                           const void *buf, IOFWRequestRefCon refcon)
{
    if(addr.addressHi != kCSRRegisterSpaceBaseAddressHi)
        return kFWResponseAddressError;
    if((addr.addressLo < kPCRBaseAddress) || (addr.addressLo + len > kPCRBaseAddress + 64*4))
        return kFWResponseAddressError;
    
    //IOLog("PCRSpace write, addr %x len %d\n", addr.addressLo, len);
    // Writes to Plug Control registers not allowed.
    if(!fControl->isLockRequest(refcon))
        return kFWResponseTypeError;

    // Only allow update of one register.
    if(len != 4 || (addr.addressLo & 3))
        return kFWResponseTypeError;
        
    UInt32 newVal = *(const UInt32 *)buf;
    UInt32 offset = (addr.addressLo - kPCRBaseAddress)/4;
    UInt32 oldVal = fBuf[offset];
    
    fBuf[offset] = newVal;
    if(fClients[offset].func)
        (fClients[offset].func)(fClients[offset].refcon, nodeID, (offset-1) & 31, oldVal, newVal);

    return kFWResponseComplete;
}

IOReturn IOFireWirePCRSpace::activate()
{
    IOReturn res = kIOReturnSuccess;
    if(!fActivations++)
        res = IOFWAddressSpace::activate();
        
    return res;
}

void IOFireWirePCRSpace::deactivate()
{
    if(!--fActivations)
        IOFWAddressSpace::deactivate();
}

IOReturn IOFireWirePCRSpace::allocatePlug(void *refcon, IOFireWirePCRCallback func, UInt32 &plug, Client* head)
{
    UInt32 i;
    IOReturn res = kIOReturnNoResources;
    
    fControl->closeGate();
    for(i=0; i<32; i++) {
        if(!head[i].func) {
            head[i].func = func;
            head[i].refcon = refcon;
            plug = i;
            res = kIOReturnSuccess;
            break;
        }
    }
    fControl->openGate();
    return res;
}

void IOFireWirePCRSpace::freePlug(UInt32 plug, Client* client)
{
    fControl->closeGate();
    client->func = NULL;
    fBuf[plug] = 0;
    fControl->openGate();
}

UInt32 IOFireWirePCRSpace::readPlug(UInt32 plug)
{
    UInt32 val;
    fControl->closeGate();
    val = fBuf[plug];
    fControl->openGate();
    return val;
}


IOReturn IOFireWirePCRSpace::updatePlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    IOReturn res;
    fControl->closeGate();
    if(oldVal == fBuf[plug]) {
        fBuf[plug] = newVal;
        res = kIOReturnSuccess;
    }
    else
        res = kIOReturnCannotLock;
    fControl->openGate();
    return res;
}

IOReturn IOFireWirePCRSpace::allocateInputPlug(void *refcon, IOFireWirePCRCallback func, UInt32 &plug)
{
    return allocatePlug(refcon, func, plug, fClients+33);
}

void IOFireWirePCRSpace::freeInputPlug(UInt32 plug)
{
    freePlug(plug+33, fClients+plug+33);
}


UInt32 IOFireWirePCRSpace::readInputPlug(UInt32 plug)
{
    return readPlug(plug+33);
}


IOReturn IOFireWirePCRSpace::updateInputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    return updatePlug(plug+33, oldVal, newVal);
}

IOReturn IOFireWirePCRSpace::allocateOutputPlug(void *refcon, IOFireWirePCRCallback func, UInt32 &plug)
{
    return allocatePlug(refcon, func, plug, fClients+1);
}

void IOFireWirePCRSpace::freeOutputPlug(UInt32 plug)
{
    freePlug(plug+1, fClients+plug+1);
}


UInt32 IOFireWirePCRSpace::readOutputPlug(UInt32 plug)
{
    return readPlug(plug+1);
}


IOReturn IOFireWirePCRSpace::updateOutputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    return updatePlug(plug+1, oldVal, newVal);
}

UInt32 IOFireWirePCRSpace::readOutputMasterPlug()
{
    return readPlug(0);
}


IOReturn IOFireWirePCRSpace::updateOutputMasterPlug(UInt32 oldVal, UInt32 newVal)
{
    return updatePlug(0, oldVal, newVal);
}

UInt32 IOFireWirePCRSpace::readInputMasterPlug()
{
    return readPlug(32);
}


IOReturn IOFireWirePCRSpace::updateInputMasterPlug(UInt32 oldVal, UInt32 newVal)
{
    return updatePlug(32, oldVal, newVal);
}


