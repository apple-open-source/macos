/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/firewire/IOFireWireController.h>

#include "FWDebugging.h"

OSData *IOFWPseudoAddressSpace::allocatedAddresses = NULL;  // unused

OSDefineMetaClassAndStructors(IOFWPseudoAddressSpaceAux, IOFWAddressSpaceAux);

OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpaceAux, 0);
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpaceAux, 1);
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpaceAux, 2);
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpaceAux, 3);
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpaceAux, 4);
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpaceAux, 5);
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpaceAux, 6);
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpaceAux, 7);
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpaceAux, 8);
OSMetaClassDefineReservedUnused(IOFWPseudoAddressSpaceAux, 9);

#pragma mark -

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

#pragma mark -

// allocateAddress
//
//

IOReturn IOFWPseudoAddressSpace::allocateAddress(FWAddress *addr, UInt32 lenDummy)
{
	return fControl->allocatePseudoAddress( addr, lenDummy );     
}

// freeAddress
//
//

void IOFWPseudoAddressSpace::freeAddress(FWAddress addr, UInt32 lenDummy)
{
    fControl->freePseudoAddress( addr, lenDummy );
}

// simpleReader
//
//

UInt32 IOFWPseudoAddressSpace::simpleReader(void *refcon, UInt16 nodeID, IOFWSpeed &speed,
					FWAddress addr, UInt32 len, IOMemoryDescriptor **buf,
                        IOByteCount * offset, IOFWRequestRefCon reqrefcon)
{
    IOFWPseudoAddressSpace * space = (IOFWPseudoAddressSpace *)refcon;

    *buf = space->fDesc;
    *offset = addr.addressLo - space->fBase.addressLo;

    return kFWResponseComplete;
}

// simpleWriter
//
//

UInt32 IOFWPseudoAddressSpace::simpleWriter(void *refcon, UInt16 nodeID, IOFWSpeed &speed,
                    FWAddress addr, UInt32 len, const void *buf, IOFWRequestRefCon reqrefcon)
{
    IOFWPseudoAddressSpace * space = (IOFWPseudoAddressSpace *)refcon;

    space->fDesc->writeBytes(addr.addressLo - space->fBase.addressLo, buf, len);

    return kFWResponseComplete;
}

// simpleRead
//
//

IOFWPseudoAddressSpace *
IOFWPseudoAddressSpace::simpleRead(IOFireWireBus *control,
                                   FWAddress *addr, UInt32 len, const void *data)
{
    IOFWPseudoAddressSpace * me = new IOFWPseudoAddressSpace;
    do 
	{
        if(!me)
            break;
        
		if(!me->initAll(control, addr, len, simpleReader, NULL, (void *)me)) 
		{
            me->release();
            me = NULL;
            break;
        }
        
		me->fDesc = IOMemoryDescriptor::withAddress((void *)data, len, kIODirectionOut);
        if(!me->fDesc) 
		{
            me->release();
            me = NULL;
        }
		
    } while(false);

    return me;
}

// simpleReadFixed
//
//

IOFWPseudoAddressSpace *
IOFWPseudoAddressSpace::simpleReadFixed(IOFireWireBus *control,
                                   FWAddress addr, UInt32 len, const void *data)
{
    IOFWPseudoAddressSpace * me = new IOFWPseudoAddressSpace;
    do 
	{
        if(!me)
            break;
        
		if(!me->initFixed(control, addr, len, simpleReader, NULL, (void *)me)) 
		{
            me->release();
            me = NULL;
            break;
        }
        
		me->fDesc = IOMemoryDescriptor::withAddress((void *)data, len, kIODirectionOut);
        if(!me->fDesc) 
		{
            me->release();
            me = NULL;
        }
		
    } while(false);

    return me;
}


// simpleRW
//
//

IOFWPseudoAddressSpace *IOFWPseudoAddressSpace::simpleRW(IOFireWireBus *control,
                                                         FWAddress *addr, UInt32 len, void *data)
{
    IOFWPseudoAddressSpace * me = new IOFWPseudoAddressSpace;
    do 
	{
        if(!me)
            break;
    
		if(!me->initAll(control, addr, len, simpleReader, simpleWriter, (void *)me)) 
		{
            me->release();
            me = NULL;
            break;
        }
        
		me->fDesc = IOMemoryDescriptor::withAddress(data, len, kIODirectionOut);
        if(!me->fDesc) 
		{
            me->release();
            me = NULL;
        }
		
    } while(false);

    return me;
}

// simpleRW
//
//

IOFWPseudoAddressSpace *IOFWPseudoAddressSpace::simpleRW(IOFireWireBus *control,
								FWAddress *addr, IOMemoryDescriptor * data)
{
    IOFWPseudoAddressSpace * me = new IOFWPseudoAddressSpace;
	do 
	{
        if(!me)
            break;
    
		if(!me->initAll(control, addr, data->getLength(), simpleReader, simpleWriter, (void *)me)) 
		{
            me->release();
            me = NULL;
            break;
        }
        
		data->retain();
        me->fDesc = data;
    
	} while(false);

    return me;
}

// initAll
//
//

bool IOFWPseudoAddressSpace::initAll(IOFireWireBus *control, FWAddress *addr, UInt32 len, 
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

// initFixed
//
//

bool IOFWPseudoAddressSpace::initFixed(IOFireWireBus *control, FWAddress addr, UInt32 len, 
		FWReadCallback reader, FWWriteCallback writer, void *refCon)
{	
    if( !IOFWAddressSpace::init(control) )
        return false;

    // Only allow fixed addresses at top of address space
    if( addr.addressHi != kCSRRegisterSpaceBaseAddressHi )
        return false;

    fBase = addr;
    fLen = len;
    fDesc = NULL;	// Only used by simpleRead case.
    fRefCon = refCon;
    fReader = reader;
    fWriter = writer;
	
    return true;
}

// createAuxiliary
//
// virtual method for creating auxiliary object.  subclasses needing to subclass 
// the auxiliary object can override this.

IOFWAddressSpaceAux * IOFWPseudoAddressSpace::createAuxiliary( void )
{
	IOFWPseudoAddressSpaceAux * auxiliary;
    
	auxiliary = new IOFWPseudoAddressSpaceAux;

    if( auxiliary != NULL && !auxiliary->init(this) ) 
	{
        auxiliary->release();
        auxiliary = NULL;
    }
	
    return auxiliary;
}

// free
//
//

void IOFWPseudoAddressSpace::free()
{
    if(fDesc)
		fDesc->release();
    
	if(fBase.addressHi != kCSRRegisterSpaceBaseAddressHi)
        freeAddress(fBase, fLen);
    
	IOFWAddressSpace::free();
}

// doRead
//
//

UInt32 IOFWPseudoAddressSpace::doRead( UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len, 
			IOMemoryDescriptor **buf, IOByteCount * offset, IOFWRequestRefCon refcon)
{
	if( !isTrustedNode( nodeID ) )
		return kFWResponseAddressError;

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

// doWrite
//
//

UInt32 IOFWPseudoAddressSpace::doWrite(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
                                       const void *buf, IOFWRequestRefCon refcon)
{
	if( !isTrustedNode( nodeID ) )
		return kFWResponseAddressError;

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

// contains
//
//

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

