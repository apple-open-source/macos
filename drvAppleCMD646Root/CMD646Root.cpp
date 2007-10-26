/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
 *	CMD646Root.cpp
 *	
	Need something to match the device registry entry.

 *
 *
 */
 
#include "CMD646Root.h"

#include <IOKit/assert.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>

#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>
#include <IOKit/IODeviceTreeSupport.h>

 

#ifdef DLOG
#undef DLOG
#endif

//#define ATA_DEBUG 1

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif


#define kDeviceTypeString "pci-ide"
#define kDevicetypeKey "device_type"
#define kNameKey "name"
#define kNameString "pci-ata"

//---------------------------------------------------------------------------

#define super IOService
OSDefineMetaClassAndStructors ( CMD646Root, IOService )

//---------------------------------------------------------------------------


/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
CMD646Root::probe(IOService* provider,	SInt32*	score)
{

    OSData		*compatibleEntry;
	
	DLOG("CMD646Root starting probe\n");


	compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "name" ) );
	if ( compatibleEntry == 0 ) 
	{
		// error unknown controller type.

		DLOG("CMD646Root failed getting compatible property\n");
		return 0;

	}

	
	// test the compatible property for a match to the controller type name.
	if ( compatibleEntry->isEqualTo( kNameString, sizeof(kNameString)-1 ) == false ) 
	{
		// not our type of controller		
		DLOG("CMD646Root compatible property doesn't match\n");
		return 0;
		
	}


	return this;


}

bool 
CMD646Root::start( IOService * provider )
{
	DLOG("CMD646Root: starting\n");

    IOPCIDevice *pciNub = (IOPCIDevice *)provider;

    if( !super::start( provider))
	return( false);

    // Make sure IO space is on.
    pciNub->setIOEnable(true);
    // turn on the bus-mastering enable
 	pciNub->setBusMasterEnable( true );   
	
	// reset the bus
//	pciNub->configWrite8( (UInt8) 0x71, (UInt8) 0x7C ); 	
//	IODelay( 30 ); // hold for 30us
	
	// relese the hard reset line and 
	// disable interrupt propogation at the CMD controller chip.
	
//	pciNub->configWrite8( (UInt8) 0x71, (UInt8) 0x3C ); 

	// delay for at least 2ms
	
//	IOSleep( 2 );
	
	publishBelow(provider);

	DLOG("CMD646Root: started\n");
//    PMinit();		// initialize for power management
//    temporaryPowerClampOn();	// hold power on till we get children
    return( true);
}

IOService * 
CMD646Root::createNub( IORegistryEntry * from )
{
    IOService *	nub;

    nub = new CMD646Device;

    if( nub && !nub->init( from, gIODTPlane )) {
	nub->free();
	nub = 0;
    }

    return( nub);
}

void 
CMD646Root::processNub(IOService * /*nub*/)
{
}


void 
CMD646Root::publishBelow( IORegistryEntry * root )
{
	DLOG("CMD646Root publish below\n");

    OSCollectionIterator *	kids = 0;
    IORegistryEntry *		next;
    IOService *			nub;


    // publish everything below, minus excludeList
    kids = IODTFindMatchingEntries( root, kIODTRecursive | kIODTExclusive, "('ata-disk','atapi-disk')");
    
    if( kids) {
    
    DLOG("CMD646Root found kids\n");
    
	while( (next = (IORegistryEntry *)kids->getNextObject())) {

            if( 0 == (nub = createNub( next )))
                continue;

            nub->attach( this );
	    
	    processNub(nub);
	    
            nub->registerService();
        }
	kids->release();
    }
}

bool 
CMD646Root::compareNubName( const IOService * nub,
				OSString * name, OSString ** matched ) const
{
    return( IODTCompareNubName( nub, name, matched )
	  ||  nub->IORegistryEntry::compareName( name, matched ) );
}

IOReturn 
CMD646Root::getNubResources( IOService * nub )
{
    if( nub->getDeviceMemory())
	return( kIOReturnSuccess );

    IODTResolveAddressing( nub, "reg", getProvider()->getDeviceMemoryWithIndex(0) );

    return( kIOReturnSuccess);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService

OSDefineMetaClassAndStructors(CMD646Device, IOService);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool CMD646Device::compareName( OSString * name,
					OSString ** matched ) const
{
    return( ((CMD646Root *)getProvider())->
		compareNubName( this, name, matched ));
}

IOService* 
CMD646Device::matchLocation( IOService * /* client */ )
{
      return( this );
}

IOReturn 
CMD646Device::getResources( void )
{
    return( ((CMD646Root *)getProvider())->getNubResources( this ));
}

IOService* 
CMD646Device::getRootCMD( void )
{

	return getProvider();


}



IOService* 
CMD646Device::getPCINub( void )
{

	return getProvider()->getProvider();


}

