/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
/*
 *
 *	IOFWAddressSpace.cpp
 *
 * Classes which describe addresses in the local node which are accessable to other nodes
 * via firewire asynchronous read/write/lock requests.
 *
 * HISTORY
 *
 */
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/firewire/IOFireWireController.h>

OSData *IOFWPseudoAddressSpace::allocatedAddresses = NULL;

/*
 * Base class for FireWire address space objects
 */
OSDefineMetaClass( IOFWAddressSpace, OSObject )
OSDefineAbstractStructors(IOFWAddressSpace, OSObject)
//OSMetaClassDefineReservedUnused(IOFWAddressSpace, 0);
OSMetaClassDefineReservedUnused(IOFWAddressSpace, 1);

bool IOFWAddressSpace::init(IOFireWireBus *bus)
{
    if(!OSObject::init())
        return false;

    fControl = OSDynamicCast(IOFireWireController, bus);
    return fControl != NULL;
}

UInt32 IOFWAddressSpace::doLock(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 inLen,
                        const UInt32 *newVal, UInt32 &outLen, UInt32 *oldVal, UInt32 type,
                          IOFWRequestRefCon refcon)
{
    UInt32 ret = kFWResponseAddressError;
    bool ok;
    int size;
    int i;
    IOMemoryDescriptor *desc = NULL;
    IOByteCount offset;

    size = inLen/8;	// Depends on type, right for 'compare and swap'
    outLen = inLen/2;	// right for 'compare and swap'
    ret = doRead(nodeID, speed, addr, size*4, &desc, &offset, refcon);
     if(ret != kFWResponseComplete)
            return ret;

    desc->readBytes(offset, oldVal, size*4);
    
    switch (type) {
        case kFWExtendedTCodeCompareSwap:
            ok = true;
            for(i=0; i<size; i++)
                ok = ok && oldVal[i] == newVal[i];
            if(ok)
                ret = doWrite(nodeID, speed, addr, size*4, newVal+size, refcon);
            break;

        default:
            ret = kFWResponseTypeError;
    }
    return ret;
}

IOReturn IOFWAddressSpace::activate()
{
    return fControl->allocAddress(this);
}

void IOFWAddressSpace::deactivate()
{
    fControl->freeAddress(this);
}

UInt32 IOFWAddressSpace::contains(FWAddress addr)
{
    return 0;
}

/*
 * Direct physical memory <-> FireWire address.
 * Accesses to these addresses will be handled automatically by the
 * hardware without notification.
 *
 * The 64 bit FireWire address of (32 bit) physical addr xxxx:xxxx is hostNode:0000:xxxx:xxxx
 */
OSDefineMetaClassAndStructors(IOFWPhysicalAddressSpace, IOFWAddressSpace)

bool IOFWPhysicalAddressSpace::initWithDesc(IOFireWireBus *control,
                                            IOMemoryDescriptor *mem)
{
    if(!IOFWAddressSpace::init(control))
        return false;
    fMem = mem;
    fMem->retain();
    fLen = mem->getLength();

    return true;
}

void IOFWPhysicalAddressSpace::free()
{
    if(fMem)
        fMem->release();
    IOFWAddressSpace::free();
}


UInt32 IOFWPhysicalAddressSpace::doRead(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len, 
					IOMemoryDescriptor **buf, IOByteCount * offset, IOFWRequestRefCon refcon)
{
    UInt32 res = kFWResponseAddressError;
    vm_size_t pos;
    IOPhysicalAddress phys;
    if(addr.addressHi != 0)
	return kFWResponseAddressError;

    pos = 0;
    while(pos < fLen) {
        IOPhysicalLength lengthOfSegment;
        phys = fMem->getPhysicalSegment(pos, &lengthOfSegment);
        if(addr.addressLo >= phys && addr.addressLo+len <= phys+lengthOfSegment) {
            // OK, block is in space and is within one VM page
	    // Set position to exact start
	    *offset = pos + addr.addressLo - phys;
            *buf = fMem;
            res = kFWResponseComplete;
            break;
        }
        pos += lengthOfSegment;
    }
    return res;
}

UInt32 IOFWPhysicalAddressSpace::doWrite(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
                                         const void *buf, IOFWRequestRefCon refcon)
{
    UInt32 res = kFWResponseAddressError;
    vm_size_t pos;
    IOPhysicalAddress phys;
    if(addr.addressHi != 0)
	return kFWResponseAddressError;

    pos = 0;
    while(pos < fLen) {
        IOPhysicalLength lengthOfSegment;
        phys = fMem->getPhysicalSegment(pos, &lengthOfSegment);
        if(addr.addressLo >= phys && addr.addressLo+len <= phys+lengthOfSegment) {
            // OK, block is in space and is within one VM page
	    // Set position to exact start
            fMem->writeBytes(pos + addr.addressLo - phys, buf, len);
            res = kFWResponseComplete;
            break;
        }
        pos += lengthOfSegment;
    }
    return res;
}

/*
 * Pseudo firewire addresses usually represent emulated registers of some kind.
 * Accesses to these addresses will result in the owner being notified.
 * 
 * Virtual addresses should not have zero as the top 16 bits of the 48 bit local address,
 * since that may look like a physical address to hardware (eg. OHCI).
 * if reader is NULL then reads will not be allowed.
 * if writer is NULL then writes will not be allowed.
 * if either is NULL then lock requests will not be allowed.
 * refcon is passed back as the first argument of read and write callbacks.
 */
OSDefineMetaClassAndStructors(IOFWPseudoAddressSpace, IOFWAddressSpace)
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpace, 0);
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpace, 1);

IOReturn IOFWPseudoAddressSpace::allocateAddress(FWAddress *addr, UInt32 lenDummy)
{
    unsigned int i, len;
    UInt8 * data;
    UInt8 used = 1;
    if(allocatedAddresses == NULL) {
        allocatedAddresses = OSData::withCapacity(4);	// SBP2 + some spare
        allocatedAddresses->appendBytes(&used, 1);	// Physical always allocated
    }
    if(!allocatedAddresses)
        return kIOReturnNoMemory;

    len = allocatedAddresses->getLength();
    data = (UInt8*)allocatedAddresses->getBytesNoCopy();
    for(i=0; i<len; i++) {
        if(data[i] == 0) {
            data[i] = 1;
            addr->addressHi = i;
            addr->addressLo = 0;
            return kIOReturnSuccess;
        }
    }
    if(len >= 0xfffe)
        return kIOReturnNoMemory;

    if(allocatedAddresses->appendBytes(&used, 1)) {
        addr->addressHi = len;
        addr->addressLo = 0;
        return kIOReturnSuccess;
    }

    return kIOReturnNoMemory;      
}

void	IOFWPseudoAddressSpace::freeAddress(FWAddress addr, UInt32 lenDummy)
{
    unsigned int len;
    UInt8 * data;
    assert(allocatedAddresses != NULL);
    
    len = allocatedAddresses->getLength();
    assert(addr.addressHi < len);
    data = (UInt8*)allocatedAddresses->getBytesNoCopy();
    assert(data[addr.addressHi]);
    data[addr.addressHi] = 0;
}

UInt32 IOFWPseudoAddressSpace::simpleReader(void *refcon, UInt16 nodeID, IOFWSpeed &speed,
					FWAddress addr, UInt32 len, IOMemoryDescriptor **buf,
                        IOByteCount * offset, IOFWRequestRefCon reqrefcon)
{
    IOFWPseudoAddressSpace * space = (IOFWPseudoAddressSpace *)refcon;
    *buf = space->fDesc;
    *offset = addr.addressLo - space->fBase.addressLo;

    return kFWResponseComplete;
}

UInt32 IOFWPseudoAddressSpace::simpleWriter(void *refcon, UInt16 nodeID, IOFWSpeed &speed,
                    FWAddress addr, UInt32 len, const void *buf, IOFWRequestRefCon reqrefcon)
{
    IOFWPseudoAddressSpace * space = (IOFWPseudoAddressSpace *)refcon;
    space->fDesc->writeBytes(addr.addressLo - space->fBase.addressLo, buf, len);

    return kFWResponseComplete;
}

IOFWPseudoAddressSpace *
IOFWPseudoAddressSpace::simpleRead(IOFireWireBus *control,
                                   FWAddress *addr, UInt32 len, const void *data)
{
    IOFWPseudoAddressSpace * me;
    me = new IOFWPseudoAddressSpace;
    do {
        if(!me)
            break;
        if(!me->initAll(control, addr, len, simpleReader, NULL, (void *)me)) {
            me->release();
            me = NULL;
            break;
        }
        me->fDesc = IOMemoryDescriptor::withAddress((void *)data, len, kIODirectionOut);
        if(!me->fDesc) {
            me->release();
            me = NULL;
        }
    } while(false);

    return me;
}

IOFWPseudoAddressSpace *
IOFWPseudoAddressSpace::simpleReadFixed(IOFireWireBus *control,
                                   FWAddress addr, UInt32 len, const void *data)
{
    IOFWPseudoAddressSpace * me;
    me = new IOFWPseudoAddressSpace;
    do {
        if(!me)
            break;
        if(!me->initFixed(control, addr, len, simpleReader, NULL, (void *)me)) {
            me->release();
            me = NULL;
            break;
        }
        me->fDesc = IOMemoryDescriptor::withAddress((void *)data, len, kIODirectionOut);
        if(!me->fDesc) {
            me->release();
            me = NULL;
        }
    } while(false);

    return me;
}


IOFWPseudoAddressSpace *IOFWPseudoAddressSpace::simpleRW(IOFireWireBus *control,
                                                         FWAddress *addr, UInt32 len, void *data)
{
    IOFWPseudoAddressSpace * me;
    me = new IOFWPseudoAddressSpace;
    do {
        if(!me)
            break;
        if(!me->initAll(control, addr, len, simpleReader, simpleWriter, (void *)me)) {
            me->release();
            me = NULL;
            break;
        }
        me->fDesc = IOMemoryDescriptor::withAddress(data, len, kIODirectionOut);
        if(!me->fDesc) {
            me->release();
            me = NULL;
        }
    } while(false);

    return me;
}

IOFWPseudoAddressSpace *IOFWPseudoAddressSpace::simpleRW(IOFireWireBus *control,
                                                         FWAddress *addr, IOMemoryDescriptor *	data)
{
    IOFWPseudoAddressSpace * me;
    me = new IOFWPseudoAddressSpace;
    do {
        if(!me)
            break;
        if(!me->initAll(control, addr, data->getLength(), simpleReader, simpleWriter, (void *)me)) {
            me->release();
            me = NULL;
            break;
        }
        data->retain();
        me->fDesc = data;
    } while(false);

    return me;
}

bool IOFWPseudoAddressSpace::initAll(IOFireWireBus *control,
                FWAddress *addr, UInt32 len, 
		FWReadCallback reader, FWWriteCallback writer, void *refCon)
{
    if(!IOFWAddressSpace::init(control))
		return false;
    if(allocateAddress(addr, len) != kIOReturnSuccess)
        return false;

    fBase = *addr;
    fBase.addressHi &= 0xFFFF;	// Mask off nodeID part.
    fLen = len;
    fDesc = NULL;	// Only used by simpleRead case.
    fRefCon = refCon;
    fReader = reader;
    fWriter = writer;
    return true;
}

bool IOFWPseudoAddressSpace::initFixed(IOFireWireBus *control,
                FWAddress addr, UInt32 len,
                FWReadCallback reader, FWWriteCallback writer, void *refCon)
{
    if(!IOFWAddressSpace::init(control))
        return false;

    // Only allow fixed addresses at top of address space
    if(addr.addressHi != kCSRRegisterSpaceBaseAddressHi)
        return false;

    fBase = addr;
    fLen = len;
    fDesc = NULL;	// Only used by simpleRead case.
    fRefCon = refCon;
    fReader = reader;
    fWriter = writer;
    return true;
}


void IOFWPseudoAddressSpace::free()
{
    if(fDesc)
	fDesc->release();
    if(fBase.addressHi != kCSRRegisterSpaceBaseAddressHi)
        freeAddress(fBase, fLen);
    IOFWAddressSpace::free();
}

UInt32 IOFWPseudoAddressSpace::doRead(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len, 
					IOMemoryDescriptor **buf, IOByteCount * offset, IOFWRequestRefCon refcon)
{
    if(addr.addressHi != fBase.addressHi)
	return kFWResponseAddressError;
    if(addr.addressLo < fBase.addressLo)
	return kFWResponseAddressError;
    if(addr.addressLo + len > fBase.addressLo+fLen)
	return kFWResponseAddressError;
    if(!fReader)
        return kFWResponseTypeError;

    return fReader(fRefCon, nodeID, speed, addr, len, buf, offset, refcon);
}

UInt32 IOFWPseudoAddressSpace::doWrite(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
                                       const void *buf, IOFWRequestRefCon refcon)
{
    if(addr.addressHi != fBase.addressHi)
	return kFWResponseAddressError;
    if(addr.addressLo < fBase.addressLo)
	return kFWResponseAddressError;
    if(addr.addressLo + len > fBase.addressLo+fLen)
	return kFWResponseAddressError;
    if(!fWriter)
        return kFWResponseTypeError;

    return fWriter(fRefCon, nodeID, speed, addr, len, buf, refcon);
}

UInt32 IOFWPseudoAddressSpace::contains(FWAddress addr)
{
    UInt32 offset;
    if(addr.addressHi != fBase.addressHi)
        return 0;
    if(addr.addressLo < fBase.addressLo)
        return 0;
    offset = addr.addressLo - fBase.addressLo;
    if(offset > fLen)
        return 0;
    return fLen - offset;
}

