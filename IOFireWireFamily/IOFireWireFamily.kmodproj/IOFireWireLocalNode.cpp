/*
 *  IOFireWireLocalNode.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Fri Aug 16 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
	$Log: IOFireWireLocalNode.cpp,v $
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

OSDefineMetaClassAndStructors(IOFireWireLocalNode, IOFireWireNub)

bool IOFireWireLocalNode::init(OSDictionary * propTable)
{
    if(!IOFireWireNub::init(propTable))
       return false;
    fMaxReadROMPackLog = 11;
    fMaxReadPackLog = 11;
    fMaxWritePackLog = 11;
    return true;
}

bool IOFireWireLocalNode::attach(IOService * provider )
{
    assert(OSDynamicCast(IOFireWireController, provider));
    if( !IOFireWireNub::attach(provider))
        return (false);
    fControl = (IOFireWireController *)provider;
    fControl->retain();

    return(true);
}


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

bool IOFireWireLocalNode::handleIsOpen( const IOService * forClient ) const
{
	return (fOpenCount > 0 ) ;
}

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
