/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 


#include <ppc/proc_reg.h>
#include "AppleK2SATARoot.h"


#include <IOKit/assert.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>

#include <IOKit/pci/IOPCIDevice.h>


#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#include <IOKit/ata/IOATATypes.h>

#include  <kern/clock.h>

#ifdef DLOG
#undef DLOG
#endif

// debugging features 

//  define to enable debug of the root class
//#define K2_SATA_ROOT_DEBUG 1


#ifdef  K2_SATA_ROOT_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif


#define kDeviceTypeString "pci-ide"
#define kDevicetypeKey "device_type"
#define kNameKey "name"
#define kCellRevisionKey "cell-revision"
#define kNameString "s-ata"
#define kCompatibleKey "compatible"
#define kCompatibleString "k2-s-ata"

//---------------------------------------------------------------------------

#define super IOService
OSDefineMetaClassAndStructors ( AppleK2SATARoot, IOService )

//---------------------------------------------------------------------------



//---------------------------------------------------------------------------

bool 
AppleK2SATARoot::init(OSDictionary* properties)
{
	DLOG("AppleK2SATARoot init start\n");


    // Initialize instance variables.
 	baseZeroMap = baseOneMap = baseTwoMap = baseThreeMap = baseFourMap = baseFiveMap = 0;
	baseAddrZero = baseAddrOne = baseAddrTwo = baseAddrThree = baseAddrFour = baseAddrFive = 0;
	nubIntSrc = 0;
	cellRevision = 0;
	   
    if (super::init(properties) == false)
    {
        DLOG("AppleK2SATARoot: super::init() failed\n");
        return false;
    }
	
	
	DLOG("AppleK2SATARoot init done\n");

    return true;
}


/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
AppleK2SATARoot::probe(IOService* provider,	SInt32*	score)
{

    OSData		*compatibleEntry;
	
	DLOG("AppleK2SATARoot starting probe\n");


	compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( kCompatibleKey ) );
	if ( compatibleEntry == 0 ) 
	{
		// error unknown controller type.

		DLOG("AppleK2SATARoot failed getting compatible property\n");
		return 0;

	}

	
	// test the compatible property for a match to the controller type name.
	if ( compatibleEntry->isEqualTo( kCompatibleString, sizeof(kCompatibleString)-1 ) == false ) 
	{
		// not our type of controller		
		DLOG("AppleK2SATARoot compatible property doesn't match\n");
		return 0;
		
	}

	
	OSData* cellVersion = OSDynamicCast( OSData, provider->getProperty( kCellRevisionKey ) );
	if(  cellVersion != 0)
	{
	
		cellRevision = 1;
	
	}


	IOPCIDevice* pciNub = (IOPCIDevice*) provider;
	
	UInt8 functNum = pciNub->getFunctionNumber();
	if( functNum > 0)
	{
	
		// ignore all but the function 0		
		DLOG("AppleK2SATARoot function number = %X\n", functNum);
		return 0;
	
	}

	return this;


}

bool 
AppleK2SATARoot::start( IOService * provider )
{
	DLOG("AppleK2SATARoot: starting\n");
	
	static const IOPMPowerState powerStates[ 2 ] = {
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, IOPMPowerOn, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
  

    IOPCIDevice *pciNub = (IOPCIDevice *)provider;

    if( !super::start( provider))
	return( false);

 	// enable mem access
	pciNub->setMemoryEnable(true);
    // turn on the bus-mastering enable
 	pciNub->setBusMasterEnable( true );   

	baseFiveMap = getProvider()->mapDeviceMemoryWithIndex( 0 );
	
	if( baseFiveMap == 0 )
	{
		DLOG("AppleK2SATARoot: Failed to get base address maps\n");
		return false;
	}
	
	baseAddrFive = (UInt8*) baseFiveMap->getVirtualAddress();
	DLOG("AppleK2SATARoot: baseAddrFive = %lx \n", (UInt32) baseAddrFive);

	// create the interrupt controller
	intController = new AppleK2SATAIC;
	if( ! intController )
	{
		DLOG("AppleKiwiRoot: failed to create intController\n");
		return false;
	}

	if( !intController->init(NULL) )
	{
		DLOG("AppleKiwiRoot: failed to init intController\n");
		return false;
	}

	if( !intController->start(provider,baseAddrFive) ) 
	{
		DLOG("AppleKiwiRoot: failed to start intController\n");
		return false;
	}
    
	// Show we are awake now:
    isSleeping = false;

    PMinit();
    registerPowerDriver(this,(IOPMPowerState *)powerStates,2);
    joinPMtree(this);
	
	// create the stack
	publishBelow(provider);

	DLOG("AppleK2SATARoot: started\n");
    return( true);
}

/*---------------------------------------------------------------------------
 *	free() - the pseudo destructor. Let go of what we don't need anymore.
 *
 *
 ---------------------------------------------------------------------------*/
void
AppleK2SATARoot::free()
{
 	if( baseZeroMap )
	{
		baseZeroMap->release();
	}
	if( baseOneMap )
	{
		baseOneMap->release();
	}
	if( baseTwoMap )
	{
		baseTwoMap->release();
	}
	if( baseThreeMap) 
	{
		baseThreeMap->release();
	}
	if( baseFourMap ) 
	{
		baseFourMap->release();
	}
	if( baseFiveMap )
	{
		baseFiveMap->release();
	}
	
	super::free();


}



AppleK2SATADevice * 
AppleK2SATARoot::createNub( IORegistryEntry * from )
{
    AppleK2SATADevice *	nub;

    nub = new AppleK2SATADevice;

    if( nub && !nub->init( from, gIODTPlane )) 
	{
		nub->free();
		nub = 0;
    }

    return( nub);
}

void 
AppleK2SATARoot::processNub(AppleK2SATADevice * nub)
{

	nub->setDeviceMemory( getProvider()->getDeviceMemory());
	
/*	OSString* string;	
	string = OSString::withCString( "k2sata-1" );
	nub->setProperty( "compatible", (OSObject *)string );
 	string->release();

	string = OSString::withCString( "k2sata-1" );
	nub->setProperty( "name", (OSObject *)string );
	string->release();
	
	string = OSString::withCString( "k2sata-1" );
	nub->setProperty( "model", (OSObject *)string );
	string->release();
*/
//	nub->initProperties();
}


void 
AppleK2SATARoot::publishBelow( IORegistryEntry * root )
{
	DLOG("AppleK2SATARoot publish below\n");

    AppleK2SATADevice *			nub;
    OSCollectionIterator *	kids = 0;
    IORegistryEntry *		next;
	SInt32 nubCount = 0;
    // publish everything below, minus excludeList
    kids = IODTFindMatchingEntries( root, kIODTRecursive | kIODTExclusive, "('ata-disk','atapi-disk','disk')");
    
    if( kids) 
	{

    
			DLOG("AppleK2SATARoot found kids\n");
		
			while( (next = (IORegistryEntry *)kids->getNextObject())) 
			{
				if( 0 == (nub = createNub( next )))
                continue;
				
				if( !nub )
				{
					DLOG("AppleK2SATARoot failed to create nub\n");
						return;
				}
				
				nubCount++;
				
				//IOSleep(100);
				nub->attach( this );	
				//nub->setLocation( (const char*) locString );    
				processNub(nub);
				nub->registerService();	    
			}	
		kids->release();
	}

	DLOG("AppleK2SATARoot nub count = %d \n", nubCount);
	
	// pinch off the ports not in use on this machine
	for( SInt32 i = 3; i >= nubCount; i--)
	{
		OSWriteLittleInt32( baseAddrFive, (i * 0x100) + 0x48, 0x3); // disable SATA port		
		DLOG( "K2 SATA Root disabling port %d not in use\n", i);
	}

}

bool 
AppleK2SATARoot::compareNubName( const IOService * nub,
				OSString * name, OSString ** matched ) const
{
    return( IODTCompareNubName( nub, name, matched )
	  ||  nub->IORegistryEntry::compareName( name, matched ) );
}

IOReturn 
AppleK2SATARoot::getNubResources( IOService * nub )
{
    if( nub->getDeviceMemory())
	return( kIOReturnSuccess );

    IODTResolveAddressing( nub, "reg", getProvider()->getDeviceMemoryWithIndex(5) );

    return( kIOReturnSuccess);
}


/*---------------------------------------------------------------------------
 *
 *  static "action" function to connect to our object
 *
 ---------------------------------------------------------------------------*/
void 
AppleK2SATARoot::sDeviceInterruptOccurred(OSObject * owner, IOInterruptEventSource *evtSrc, int count)
{
	return;


}

IOReturn 
AppleK2SATARoot::setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice )
{
	if( powerStateOrdinal == 0 
		&& !(isSleeping) )
	{
		DLOG("AppleK2SATARoot power ordinal 0\n");
		isSleeping = true;
		
		/*for( int i = 0; i < 4; i++)
		{
			restoreSCR2[i] = OSReadLittleInt( baseAddrFive, 0x48 + (i * 0x100) );
			restoreSICR1[i] = OSReadLittleInt( baseAddrFive, 0x80 + (i * 0x100) );
			OSWriteLittleInt32( baseAddrFive, (i * 0x100) + 0x48, restoreSCR2[i] & 0x00000001);  //set the phy into reset mode going into sleep
		}*/
	}
	

	if( powerStateOrdinal == 1
		&& (isSleeping) )
	{
		
		DLOG("AppleK2SATARoot power ordinal 1\n");
		isSleeping = false;
		// time to wake up

		// force a physical reset of the Phy/link layer per vendor instructions.
		// we need to do a read/modify/write of register 4, bit 3 
		
		// select port 1 & 2
//		writeMDIO( 0x7, 2);
//		UInt16 testReg = 0;
//		readMDIO( 0xA, testReg);
//		IOLog("AppleK2SATARoot testReg = %x\n", testReg);
//		writeMDIO(0xA, 0xA860);
//		readMDIO( 0xA, testReg);
//		IOLog("AppleK2SATARoot testReg = %x\n", testReg);
//		writeMDIO(0xA, 0x9820);
		
		if( cellRevision )
		{
			UInt16 reg4Val = 0;
			
			readMDIO( 0x04, reg4Val);
			writeMDIO( 0x04, reg4Val | 0x0008); // set bit 3
			IODelay(200);  // wait 200us
			writeMDIO( 0x04, reg4Val); //reset bit 3 to 0
			IODelay(250); // wait 250us
			
			DLOG("AppleK2SATARoot reset analog layer %x\n", reg4Val);
		}		
	}
	return IOPMAckImplied;

}


IOReturn 
AppleK2SATARoot::readMDIO( UInt8 registerAddr, UInt16& value )
{
		UInt32 mdioRet = 0;
		UInt32 mdioControl = (registerAddr & 0x1F) | 0x00004000;  // register selection goes in 4:0, read command is bit 14

		OSWriteLittleInt32( baseAddrFive, 0x8c, mdioControl);

		// MDIO register is at base addr + 0x8c
		
		mdioRet = OSReadLittleInt32(baseAddrFive, 0x8c );
		while( !(mdioRet & 0x00008000) )
		{
		
			mdioRet = OSReadLittleInt32(baseAddrFive, 0x8c );
			IODelay(100);
		}

	value =  (UInt16) (mdioRet >> 16);

	return 0;
}

IOReturn 
AppleK2SATARoot::writeMDIO( UInt8 registerAddr, UInt16 value )
{

	UInt32 mdioRet = 0;
	UInt32 mdioControl = (registerAddr & 0x1F) | 0x00002000 | ((UInt32)value << 16);  // register selection goes in 4:0, write command is bit 13, data goes in upper word 31:16

	OSWriteLittleInt32( baseAddrFive, 0x8c, mdioControl);

	// MDIO register is at base addr + 0x8c
	
	mdioRet = OSReadLittleInt32(baseAddrFive, 0x8c );
	while( !(mdioRet & 0x00008000) )
	{
	
		mdioRet = OSReadLittleInt32(baseAddrFive, 0x8c );
		IODelay(100);
	}



	return 0;
	

}


#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifdef DLOG
#undef DLOG
#endif

#undef super
#define super IOService
OSDefineMetaClassAndStructors ( AppleK2SATADevice, IOService )



IOService* 
AppleK2SATADevice::matchLocation( IOService * /* client */ )
{
    // DLOG("AppleK2SATADevice matchLocation\n");
	  return( this );
}

IOReturn 
AppleK2SATADevice::getResources( void )
{
	return( ((AppleK2SATARoot *)getProvider())->getNubResources( this ));
}


#pragma mark -
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifdef DLOG
#undef DLOG
#endif

//#define K2SATA_IC_DEBUG 1

#ifdef  K2SATA_IC_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif
#undef super
#define super IOInterruptController

#define K2SATANumVectors 4

OSDefineMetaClassAndStructors(AppleK2SATAIC, IOInterruptController);

enum {
	kIOSLICPowerStateCount = 2
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool 
AppleK2SATAIC::start(IOService *provider, UInt8* inBar5)
{
	if( !provider || !inBar5 )
		return false;
 
		
	
	if (!super::start(provider)) 
	{
		DLOG ("AppleK2SATAIC::start - super::start(provider) returned false\n");
		return false;
	}

	interruptControllerName = (OSSymbol *)IODTInterruptControllerName( provider );
	if (interruptControllerName == 0) {
		DLOG ("AppleK2SATAIC::start - interruptControllerName is NULL\n");
		return false;
	}

	UInt32 numVectors = K2SATANumVectors;
	UInt32 sizeofVectors = numVectors * sizeof(IOInterruptVector);
  
	// Allocate the memory for the vectors.
	vectors = (IOInterruptVector *)IOMalloc(sizeofVectors);
	if (vectors == NULL) {
		DLOG ("AppleK2SATAIC::start - cannot allocate vectors\n");
		return false;
	}
	
	bzero(vectors, sizeofVectors);
  
	// Allocate locks for the vectors.
	for (unsigned int cnt = 0; cnt < numVectors ; cnt++) {
		vectors[cnt].interruptLock = IOLockAlloc();
		if (vectors[cnt].interruptLock == NULL) {
			for (cnt = 0; cnt < numVectors; cnt++) {
				if (vectors[cnt].interruptLock != NULL)
				IOLockFree(vectors[cnt].interruptLock);
			}
			DLOG ("AppleK2SATAIC::start - cannot allocate lock for vectors[%d]\n", cnt);
			return false;
		}
	}


	
	provider->registerInterrupt(0, this, getInterruptHandlerAddress(), 0);
	
	// Register this interrupt controller so clients can find it.
	getPlatform()->registerInterruptController(interruptControllerName, this);

	provider->enableInterrupt(0);
       
	DLOG ("AppleK2SATAIC::start - finished\n");

	return true;

}
  
IOReturn
AppleK2SATAIC::getInterruptType(IOService *nub, int source,
				    int *interruptType)
{

	if (interruptType == 0) return kIOReturnBadArgument;
  
	*interruptType = kIOInterruptTypeLevel;
  
	return kIOReturnSuccess;

}
  
IOInterruptAction 
AppleK2SATAIC::getInterruptHandlerAddress(void)
{

	return  (IOInterruptAction) &AppleK2SATAIC::handleInterrupt ;

}

IOReturn 
AppleK2SATAIC::handleInterrupt( 	void* /*refCon*/,
								IOService* /*nub*/,
								int /*source*/ )
{

	long				vectorNumber = 0;
	IOInterruptVector	*vector;
//	unsigned short		events;


	// iterate each possible interrupt vector and let it decide whether it wants to 
	// handle the interrupt. 
	for(int i = 0; i < K2SATANumVectors; i++)
	{
		vectorNumber = i;
		vector = &vectors[vectorNumber];
    
		vector->interruptActive = 1;
		sync();
		isync();
		if (!vector->interruptDisabledSoft) {
			isync();
      			
			// Call the handler if it exists.
			if (vector->interruptRegistered) {
				vector->handler(vector->target, vector->refCon, vector->nub, vector->source);
				
				if (vector->interruptDisabledSoft) {
					// Hard disable the source.
					vector->interruptDisabledHard = 1;
					disableVectorHard(vectorNumber, vector);
				}
			}
		} else {
			// Hard disable the source.
			vector->interruptDisabledHard = 1;
			disableVectorHard(vectorNumber, vector);
		}
		vector->interruptActive = 0;
	
		
	}

	return kIOReturnSuccess;


}
  
bool 
AppleK2SATAIC::vectorCanBeShared(long vectorNumber, IOInterruptVector *vector)
{

	return false;
}



void 
AppleK2SATAIC::initVector(long vectorNumber, IOInterruptVector *vector)
{

	return;
}

void 
AppleK2SATAIC::disableVectorHard(long vectorNumber, IOInterruptVector *vector)
{
	switch( vectorNumber )
	{
		
		default:
		break; // There is no means to enable/disable interrupts in this hardware. 
	}


}

void 
AppleK2SATAIC::enableVector(long vectorNumber, IOInterruptVector *vector)
{

	switch( vectorNumber )
	{
		
		default:
		break; // There is no means to enable/disable interrupts in this hardware. 
	}



}





