/*
 *  IOFireWireLocalNode.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Fri Aug 16 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
/*
	$Log: IOFireWireLocalNode.cpp,v $
	Revision 1.6  2003/02/20 02:00:12  collin
	*** empty log message ***
	
	Revision 1.5  2003/02/17 21:47:52  collin
	*** empty log message ***
	
	Revision 1.4  2002/10/31 18:53:13  wgulland
	Fix kernel panic when unloading family while reading ROMs
	
	Revision 1.3  2002/10/18 23:29:44  collin
	fix includes, fix cast which fails on new compiler
	
	Revision 1.2  2002/09/25 00:27:24  niels
	flip your world upside-down
	
*/

// public
#import <IOKit/firewire/IOFireWireBus.h>
#import <IOKit/firewire/IOFireWireController.h>

// private
#import "IOFireWireMagicMatchingNub.h"
#import "IOFireWireLocalNode.h"

OSDefineMetaClassAndStructors(IOFireWireLocalNodeAux, IOFireWireNubAux);
OSMetaClassDefineReservedUnused(IOFireWireLocalNodeAux, 0);
OSMetaClassDefineReservedUnused(IOFireWireLocalNodeAux, 1);
OSMetaClassDefineReservedUnused(IOFireWireLocalNodeAux, 2);
OSMetaClassDefineReservedUnused(IOFireWireLocalNodeAux, 3);

#pragma mark -

// init
//
//

bool IOFireWireLocalNodeAux::init( IOFireWireLocalNode * primary )
{
	bool success = true;		// assume success
	
	// init super
	
    if( !IOFireWireNubAux::init( primary ) )
        success = false;
	
	if( success )
	{
	}
	
	return success;
}

// free
//
//

void IOFireWireLocalNodeAux::free()
{	    
	IOFireWireNubAux::free();
}

#pragma mark -

OSDefineMetaClassAndStructors(IOFireWireLocalNode, IOFireWireNub)

// init
//
//

bool IOFireWireLocalNode::init(OSDictionary * propTable)
{
    if(!IOFireWireNub::init(propTable))
       return false;
    fMaxReadROMPackLog = 11;
    fMaxReadPackLog = 11;
    fMaxWritePackLog = 11;
    return true;
}

// createAuxiliary
//
// virtual method for creating auxiliary object.  subclasses needing to subclass 
// the auxiliary object can override this.

IOFireWireNubAux * IOFireWireLocalNode::createAuxiliary( void )
{
	IOFireWireLocalNodeAux * auxiliary;
    
	auxiliary = new IOFireWireLocalNodeAux;

    if( auxiliary != NULL && !auxiliary->init(this) ) 
	{
        auxiliary->release();
        auxiliary = NULL;
    }
	
    return auxiliary;
}

// attach
//
//

bool IOFireWireLocalNode::attach(IOService * provider )
{
    assert(OSDynamicCast(IOFireWireController, provider));
    if( !IOFireWireNub::attach(provider))
        return (false);
    fControl = (IOFireWireController *)provider;
    fControl->retain();

    return(true);
}

// setNodeProperties
//
//

void IOFireWireLocalNode::setNodeProperties(UInt32 gen, UInt16 nodeID,
                                        UInt32 *selfIDs, int numSelfIDs)
{
    OSObject *prop;
    
    fLocalNodeID = fNodeID = nodeID;
    fGeneration = gen;
	
	prop = OSNumber::withNumber(nodeID, 16);
    setProperty(gFireWireNodeID, prop);
    prop->release();

    // Store selfIDs
    prop = OSData::withBytes(selfIDs, numSelfIDs*sizeof(UInt32));
    setProperty(gFireWireSelfIDs, prop);
    prop->release();

    prop = OSNumber::withNumber((selfIDs[0] & kFWSelfID0SP) >> kFWSelfID0SPPhase, 32);
    setProperty(gFireWireSpeed, prop);
    prop->release();
}

IOReturn IOFireWireLocalNode::message( UInt32 mess, IOService * provider,
                                    void * argument )
{
	if( kIOFWMessagePowerStateChanged == mess )
	{
		messageClients( mess );
		return kIOReturnSuccess;
	}
	    
    return IOService::message(mess, provider, argument );
}

// handleOpen
//
//

bool IOFireWireLocalNode::handleOpen( 	IOService *	  forClient,
                            IOOptionBits	  options,
                            void *		  arg )
{
	bool ok = true ;

	if ( fOpenCount == 0)
		ok = IOFireWireNub::handleOpen( this, 0, NULL ) ;
	
	if ( ok )
		fOpenCount++ ;

    return ok;
}

// handleClose
//
//

void IOFireWireLocalNode::handleClose(   IOService *	  forClient,
                            IOOptionBits	  options )
{
	if ( fOpenCount )
	{
		fOpenCount-- ;
		if ( fOpenCount == 0)
			IOFireWireNub::handleClose( this, 0 );
	}
}

// handleIsOpen
//
//

bool IOFireWireLocalNode::handleIsOpen( const IOService * forClient ) const
{
	return (fOpenCount > 0 ) ;
}

// setProperties
//
//

IOReturn IOFireWireLocalNode::setProperties( OSObject * properties )
{
    OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
    OSDictionary *summon;
    if(!dict)
        return kIOReturnUnsupported;
    summon = OSDynamicCast(OSDictionary, dict->getObject("SummonNub"));
    if(!summon) {
        return kIOReturnBadArgument;
    }
    IOFireWireMagicMatchingNub *nub = NULL;
    IOReturn ret = kIOReturnBadArgument;
    do {
        nub = new IOFireWireMagicMatchingNub;
        if(!nub->init(summon))
            break;
        if (!nub->attach(this))	
            break;
        nub->registerService(kIOServiceSynchronous);
        // Kill nub if nothing matched
        if(!nub->getClient()) {
            nub->detach(this);
        }
        ret = kIOReturnSuccess;
    } while (0);
    if(nub)
        nub->release();
    return ret;
}
