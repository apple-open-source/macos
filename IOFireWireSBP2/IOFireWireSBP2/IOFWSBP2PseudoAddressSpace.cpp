/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

// public
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireDevice.h>

// private
#include "IOFWSBP2PseudoAddressSpace.h"

OSDefineMetaClassAndStructors(IOFWSBP2PseudoAddressSpace, IOFWPseudoAddressSpace);

OSMetaClassDefineReservedUnused(IOFWSBP2PseudoAddressSpace, 0);
OSMetaClassDefineReservedUnused(IOFWSBP2PseudoAddressSpace, 1);
OSMetaClassDefineReservedUnused(IOFWSBP2PseudoAddressSpace, 2);
OSMetaClassDefineReservedUnused(IOFWSBP2PseudoAddressSpace, 3);
OSMetaClassDefineReservedUnused(IOFWSBP2PseudoAddressSpace, 4);
OSMetaClassDefineReservedUnused(IOFWSBP2PseudoAddressSpace, 5);
OSMetaClassDefineReservedUnused(IOFWSBP2PseudoAddressSpace, 6);
OSMetaClassDefineReservedUnused(IOFWSBP2PseudoAddressSpace, 7);
OSMetaClassDefineReservedUnused(IOFWSBP2PseudoAddressSpace, 8);
OSMetaClassDefineReservedUnused(IOFWSBP2PseudoAddressSpace, 9);

#pragma mark -

// setAddressLo
//
//

void IOFWSBP2PseudoAddressSpace::setAddressLo( UInt32 addressLo )
{
	fBase.addressLo = addressLo;
}

// simpleRead
//
//

IOFWSBP2PseudoAddressSpace * IOFWSBP2PseudoAddressSpace::simpleRead(	IOFireWireBus *	control,
																		FWAddress *		addr, 
																		UInt32 			len, 
																		const void *	data)
{
    IOFWSBP2PseudoAddressSpace * me = new IOFWSBP2PseudoAddressSpace;
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

// simpleRW
//
//

IOFWSBP2PseudoAddressSpace * IOFWSBP2PseudoAddressSpace::simpleRW(	IOFireWireBus *	control,
																	FWAddress *		addr, 
																	UInt32 			len, 
																	void *			data )
{
    IOFWSBP2PseudoAddressSpace * me = new IOFWSBP2PseudoAddressSpace;
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
        
		me->fDesc = IOMemoryDescriptor::withAddress(data, len, kIODirectionOutIn);
        if(!me->fDesc) 
		{
            me->release();
            me = NULL;
        }
		
    } while(false);

    return me;
}

// createPseudoAddressSpace
//
//

IOFWSBP2PseudoAddressSpace * IOFWSBP2PseudoAddressSpace::createPseudoAddressSpace( 	IOFireWireBus * control,
																					IOFireWireUnit * unit,
																					FWAddress *		addr, 
																					UInt32 			len, 
																					FWReadCallback 	reader, 
																					FWWriteCallback	writer, 
																					void *			refcon )
{
 
    IOFWSBP2PseudoAddressSpace *	space = NULL;
    IOFireWireDevice * 				device = NULL;
	
	space = new IOFWSBP2PseudoAddressSpace;
    
	if( space != NULL )
	{
		if( !space->initAll( control, addr, len, reader, writer, refcon ) ) 
		{
			space->release();
			space = NULL;
		}
	}
	
 	if( space != NULL )
	{
		device = OSDynamicCast( IOFireWireDevice, unit->getProvider() );
		if( device == NULL )
		{
			space->release();
			space = NULL;
		}
	}
	
	if( space != NULL )
	{
		space->addTrustedNode( device );
	}
	
	return space;
	
}
