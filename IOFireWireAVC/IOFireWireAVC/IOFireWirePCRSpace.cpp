/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
 
#include <IOKit/IOLib.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/avc/IOFireWireAVCConsts.h>
#include <IOKit/avc/IOFireWirePCRSpace.h>

OSDefineMetaClassAndStructors(IOFireWirePCRSpace, IOFWPseudoAddressSpace)
OSMetaClassDefineReservedUnused(IOFireWirePCRSpace, 0);
OSMetaClassDefineReservedUnused(IOFireWirePCRSpace, 1);
OSMetaClassDefineReservedUnused(IOFireWirePCRSpace, 2);
OSMetaClassDefineReservedUnused(IOFireWirePCRSpace, 3);

bool IOFireWirePCRSpace::init(IOFireWireBus *bus)
{
    //IOLog( "IOFireWirePCRSpace::init (0x%08X)\n",(int) this);

	if(!IOFWPseudoAddressSpace::initFixed(bus, 
            FWAddress(kCSRRegisterSpaceBaseAddressHi, kPCRBaseAddress),
            sizeof(fBuf), simpleReader, NULL, this))
        return false;

    fDesc = IOMemoryDescriptor::withAddress(fBuf, sizeof(fBuf), kIODirectionOutIn);
    if (fDesc == NULL) {
        return false;
    }

	IOReturn status = fDesc->prepare();
	if( status != kIOReturnSuccess )
	{
		fDesc->release();
		fDesc = NULL;
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

	fAVCTargetSpace = NULL;

	return true;
}

IOFireWirePCRSpace * IOFireWirePCRSpace::getPCRAddressSpace(IOFireWireBus *bus)
{
    IOFWAddressSpace *existing;
    IOFireWirePCRSpace *space;

	//IOLog( "IOFireWirePCRSpace::getPCRAddressSpace\n");
	
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
	//IOLog( "IOFireWirePCRSpace::doWrite (0x%08X)\n",(int) this);

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

	// Notify target space object of plug value modification
	if ((fAVCTargetSpace) && (offset > 0) && (offset < 32))
		fAVCTargetSpace->pcrModified(IOFWAVCPlugIsochOutputType,(offset-1),newVal);
	else if ((fAVCTargetSpace) && (offset > 32) && (offset < 64))
		fAVCTargetSpace->pcrModified(IOFWAVCPlugIsochInputType,(offset-33),newVal);

    return kFWResponseComplete;
}

IOReturn IOFireWirePCRSpace::activate()
{
    IOReturn res = kIOReturnSuccess;

	//IOLog( "IOFireWirePCRSpace::activate (0x%08X)\n",(int) this);

    if(!fActivations++)
        res = IOFWAddressSpace::activate();
        
    return res;
}

void IOFireWirePCRSpace::deactivate()
{
	//IOLog( "IOFireWirePCRSpace::deactivate (0x%08X)\n",(int) this);

    if(!--fActivations)
        IOFWAddressSpace::deactivate();
}

IOReturn IOFireWirePCRSpace::allocatePlug(void *refcon, IOFireWirePCRCallback func, UInt32 &plug, Client* head)
{
    UInt32 i;
    IOReturn res = kIOReturnNoResources;

	//IOLog( "IOFireWirePCRSpace::allocatePlug (0x%08X)\n",(int) this);
	
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
	//IOLog( "IOFireWirePCRSpace::freePlug (0x%08X)\n",(int) this);

    fControl->closeGate();
    client->func = NULL;
    fBuf[plug] = 0;
    fControl->openGate();
}

UInt32 IOFireWirePCRSpace::readPlug(UInt32 plug)
{
	//IOLog( "IOFireWirePCRSpace::readPlug (0x%08X)\n",(int) this);

    UInt32 val;
    fControl->closeGate();
    val = fBuf[plug];
    fControl->openGate();
    return val;
}


IOReturn IOFireWirePCRSpace::updatePlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
	//IOLog( "IOFireWirePCRSpace::updatePlug (0x%08X)\n",(int) this);

	IOReturn res;
    fControl->closeGate();
    if(oldVal == fBuf[plug]) {
        fBuf[plug] = newVal;
        res = kIOReturnSuccess;

		// Notify target space object of plug value modification
		if ((fAVCTargetSpace) && (plug > 0) && (plug < 32))
			fAVCTargetSpace->pcrModified(IOFWAVCPlugIsochOutputType,(plug-1),newVal);
		else if ((fAVCTargetSpace) && (plug > 32) && (plug < 64))
			fAVCTargetSpace->pcrModified(IOFWAVCPlugIsochInputType,(plug-33),newVal);
    }
    else
        res = kIOReturnCannotLock;
    fControl->openGate();
    return res;
}

IOReturn IOFireWirePCRSpace::allocateInputPlug(void *refcon, IOFireWirePCRCallback func, UInt32 &plug)
{
	//IOLog( "IOFireWirePCRSpace::allocateInputPlug (0x%08X)\n",(int) this);

    return allocatePlug(refcon, func, plug, fClients+33);
}

void IOFireWirePCRSpace::freeInputPlug(UInt32 plug)
{
	//IOLog( "IOFireWirePCRSpace::freeInputPlug (0x%08X)\n",(int) this);

    freePlug(plug+33, fClients+plug+33);
}


UInt32 IOFireWirePCRSpace::readInputPlug(UInt32 plug)
{
	//IOLog( "IOFireWirePCRSpace::readInputPlug (0x%08X)\n",(int) this);

    return readPlug(plug+33);
}


IOReturn IOFireWirePCRSpace::updateInputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
	//IOLog( "IOFireWirePCRSpace::updateInputPlug (0x%08X)\n",(int) this);

    return updatePlug(plug+33, oldVal, newVal);
}

IOReturn IOFireWirePCRSpace::allocateOutputPlug(void *refcon, IOFireWirePCRCallback func, UInt32 &plug)
{
    IOReturn result;

	//IOLog( "IOFireWirePCRSpace::allocateOutputPlug (0x%08X)\n",(int) this);

    result = allocatePlug(refcon, func, plug, fClients+1);
    return result;
}

void IOFireWirePCRSpace::freeOutputPlug(UInt32 plug)
{
	//IOLog( "IOFireWirePCRSpace::freeOutputPlug (0x%08X)\n",(int) this);

    freePlug(plug+1, fClients+plug+1);
}


UInt32 IOFireWirePCRSpace::readOutputPlug(UInt32 plug)
{
	//IOLog( "IOFireWirePCRSpace::readOutputPlug (0x%08X)\n",(int) this);

    return readPlug(plug+1);
}


IOReturn IOFireWirePCRSpace::updateOutputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
	//IOLog( "IOFireWirePCRSpace::updateOutputPlug (0x%08X)\n",(int) this);

    return updatePlug(plug+1, oldVal, newVal);
}

UInt32 IOFireWirePCRSpace::readOutputMasterPlug()
{
	//IOLog( "IOFireWirePCRSpace::readOutputMasterPlug (0x%08X)\n",(int) this);

    return readPlug(0);
}


IOReturn IOFireWirePCRSpace::updateOutputMasterPlug(UInt32 oldVal, UInt32 newVal)
{
	//IOLog( "IOFireWirePCRSpace::updateOutputMasterPlug (0x%08X)\n",(int) this);

    return updatePlug(0, oldVal, newVal);
}

UInt32 IOFireWirePCRSpace::readInputMasterPlug()
{
	//IOLog( "IOFireWirePCRSpace::readInputMasterPlug (0x%08X)\n",(int) this);

    return readPlug(32);
}


IOReturn IOFireWirePCRSpace::updateInputMasterPlug(UInt32 oldVal, UInt32 newVal)
{
	//IOLog( "IOFireWirePCRSpace::updateInputMasterPlug (0x%08X)\n",(int) this);

    return updatePlug(32, oldVal, newVal);
}

void IOFireWirePCRSpace::setAVCTargetSpacePointer(IOFireWireAVCTargetSpace *pAVCTargetSpace)
{
	//IOLog( "IOFireWirePCRSpace::setAVCTargetSpacePointer (0x%08X)\n",(int) this);

	fAVCTargetSpace = pAVCTargetSpace;
	return;
}

void IOFireWirePCRSpace::clearAllP2PConnections(void)
{
	int i;
	UInt32 oldVal;
	
	//IOLog( "IOFireWirePCRSpace::clearAllP2PConnections (0x%08X)\n",(int) this);

	// Handle oPCRs
    for(i=0; i<32; i++)
	{
		fControl->closeGate();
		oldVal = fBuf[i+1];
		if ((oldVal & 0x3F000000) != 0)
		{
			fBuf[i+1] &= 0xC0FFFFFF;	// Clear P2P field

			// If this plug has a client, notify it
			if(fClients[i+1].func)
				(fClients[i+1].func)(fClients[i+1].refcon, 0xFFFF, i, oldVal, fBuf[i+1]);

			// Notify the AVC Target Space Object of the change
			if (fAVCTargetSpace)
				fAVCTargetSpace->pcrModified(IOFWAVCPlugIsochOutputType,i,fBuf[i+1]);
		}
		fControl->openGate();
	}

	// Handle iPCRs
    for(i=0; i<32; i++)
	{
		fControl->closeGate();
		oldVal = fBuf[i+33];
		if ((oldVal & 0x3F000000) != 0)
		{
			fBuf[i+33] &= 0xC0FFFFFF;	// Clear P2P field

			// If this plug has a client, notify it
			if(fClients[i+33].func)
				(fClients[i+33].func)(fClients[i+33].refcon, 0xFFFF, i, oldVal, fBuf[i+33]);

			// Notify the AVC Target Space Object of the change
			if (fAVCTargetSpace)
				fAVCTargetSpace->pcrModified(IOFWAVCPlugIsochInputType,i,fBuf[i+33]);
		}
		fControl->openGate();
	}
	
	return;
}
