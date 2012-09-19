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
	Revision 1.11.12.1  2012/06/07 18:46:05  calderon
	Fix leak and partial protect against failed poweredStart()
	<rdar://11005341> or <rdar://11411007>

	Revision 1.11  2009/03/26 22:45:17  calderon
	<rdar://6728033> User client fails to terminate with unexpected consuequences

	Revision 1.10  2008/04/24 00:01:39  collin
	more K640
	
	Revision 1.9  2006/11/08 22:33:38  ayanowit
	Changed the SetProperty(...) function on the IOFireWireLocalNode, which is used to instantiate objects with the IOFireWireMatchingNub, to add a new way
	of instantiating IOFireWireMatchingNub matched objects, where we ensure that only one of those objects gets instantiated on the LocalNode (SummonNubExclusive).
	This is part of the changes needed to get AppleFWAudio to start using the IOFireWireMatchingNub instead of directly matching on IOFireWireLocalNode.
	
	Revision 1.8  2005/02/18 22:56:53  gecko1
	3958781 Q45C EVT: FireWire ASP reporter says port speed is 800 Mb/sec
	
	Revision 1.7  2003/10/16 00:57:20  collin
	*** empty log message ***
	
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
	
	fOpenClients = OSSet::withCapacity( 2 );
	if( fOpenClients == NULL )
		return false;
	
    return true;
}

// createAuxiliary
//
// virtual method for creating auxiliary object.  subclasses needing to subclass 
// the auxiliary object can override this.

IOFireWireNubAux * IOFireWireLocalNode::createAuxiliary( void )
{
	IOFireWireLocalNodeAux * auxiliary;
    
	auxiliary = OSTypeAlloc( IOFireWireLocalNodeAux );

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

void IOFireWireLocalNode::free()
{
    if( fOpenClients != NULL )
	{
        fOpenClients->release();
		fOpenClients = NULL;
	}
	
    if ( fControl )
    {
        fControl->release();
        fControl = NULL;
    }
    
    IOFireWireNub::free();
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
                                        UInt32 *selfIDs, int numSelfIDs, IOFWSpeed maxSpeed )
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

    prop = OSNumber::withNumber(maxSpeed, 32);
    setProperty(gFireWireSpeed, prop);
    prop->release();
}

// message
//
//

IOReturn IOFireWireLocalNode::message( UInt32 mess, IOService * provider,
                                    void * argument )
{
	if( kIOFWMessagePowerStateChanged == mess )
	{
		messageClients( mess );
		return kIOReturnSuccess;
	}

	if( kIOFWMessageTopologyChanged == mess )
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

	if ( fOpenClients->getCount() == 0)
		ok = IOFireWireNub::handleOpen( this, 0, NULL ) ;
	
	if ( ok )
	{
		fOpenClients->setObject( forClient );
	}

    return ok;
}

// handleClose
//
//

void IOFireWireLocalNode::handleClose(   IOService *	  forClient,
                            IOOptionBits	  options )
{
	if( fOpenClients->containsObject( forClient ) )
	{
		fOpenClients->removeObject( forClient );
		
		if ( fOpenClients->getCount() == 0 )
			IOFireWireNub::handleClose( this, 0 );
	}
}

// handleIsOpen
//
//

bool IOFireWireLocalNode::handleIsOpen( const IOService * forClient ) const
{
	return fOpenClients->containsObject( forClient );
}

// setProperties
//
//

IOReturn IOFireWireLocalNode::setProperties( OSObject * properties )
{
	IOFireWireMagicMatchingNub *nub = NULL;
    IOReturn ret = kIOReturnBadArgument;
	bool doSummon = false;
	OSIterator *localNodeChildIterator;
	OSIterator *magicMatchingNubChildIterator;
	OSObject *localNodeChild;
	OSObject *matchingNubChild;
	OSObject *desiredChild;
	OSString *desiredChildString;
	
    OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
    OSDictionary *summon;
    if(!dict)
        return kIOReturnUnsupported;
	
	// Take the FireWire workloop lock, to prevent multi-thread issues
	if (fControl)
		fControl->closeGate();
	else
		return kIOReturnNoDevice;

	summon = OSDynamicCast(OSDictionary, dict->getObject("SummonNubExclusive"));
    if(summon)
	{
		//IOLog("SummonNubExclusive\n");
		
		// For now, assume we'll need to summon the object
		doSummon = true;
		
		localNodeChildIterator = getClientIterator();
		if( localNodeChildIterator ) 
		{
			while( (localNodeChild = localNodeChildIterator->getNextObject()) ) 
			{
				//IOLog("found a localNodeChild\n");
				
				IOFireWireMagicMatchingNub * matchingNub = OSDynamicCast(IOFireWireMagicMatchingNub, localNodeChild);
				if(matchingNub)
				{
					//IOLog("found a matchingNub\n");

					desiredChild = matchingNub->getProperty( "IODesiredChild" );
					if (desiredChild)
					{
						desiredChildString = OSDynamicCast(OSString,desiredChild);
						if (desiredChildString)
						{
							//IOLog("desiredChildString = %s\n",desiredChildString->getCStringNoCopy());
							
							magicMatchingNubChildIterator = matchingNub->getClientIterator();
							if( magicMatchingNubChildIterator ) 
							{
								while( (matchingNubChild = magicMatchingNubChildIterator->getNextObject()) ) 
								{
									//IOLog("found a matchingNubChild\n");
									
									// See if this matching nub's child is of the IODesiredChild class
									if (matchingNubChild->metaCast(desiredChildString))
									{
										//IOLog("matchingNubChild is IODesiredChild\n");
										doSummon = false;
									}
								}
								magicMatchingNubChildIterator->release();
							}
						}
					}
				}
			}
			localNodeChildIterator->release();
		}
	}
	else
	{
		summon = OSDynamicCast(OSDictionary, dict->getObject("SummonNub"));
		if(!summon) 
			ret = kIOReturnBadArgument;
		else
			doSummon = true;
	}

    if (doSummon)
	{
		do {
			nub = OSTypeAlloc( IOFireWireMagicMatchingNub );
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
	}
	
	if (fControl)
		fControl->openGate();

    return ret;
}
