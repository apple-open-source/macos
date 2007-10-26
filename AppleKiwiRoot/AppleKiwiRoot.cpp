/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 *	AppleKiwiRoot.cpp
 *	
	Need something to match the device registry entry.

 *
 *
 */
 


#include <ppc/proc_reg.h>
#include "AppleKiwiRoot.h"


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
//#define KIWI_ROOT_DEBUG 1

// define to enable debug of the bay handles
//#define KIWI_BAY_DEBUG 1

// define to enable debug of the interrupt controller. 
//#define KIWI_IC_DEBUG 1

#ifdef  KIWI_ROOT_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif


#define kDeviceTypeString "pci-ide"
#define kDevicetypeKey "device_type"
#define kNameKey "name"
#define kNameString "AppleKiwi"

#define kPCIInlineKey		"pci_inline"
#define kMode6Key			"mode6"

#define k100mhz_pll 0x0d2b
#define k133mhz_pll 0x0826

//property keys as defined in the 4.4.2a3 boot rom.
#define kTrayPresentStatusKey "platform-drivePresent"
#define kBayInUseStatusKey	"platform-driveInUse"
#define kBayPowerStatusKey "platform-drivePower"
#define kBayOpenSwitchStatusKey "platform-driveSwitch"

#define kBaySetActiveLEDKey "platform-setDriveInUse"
#define kBaySetFailLEDKey	"platform-setDriveFail"
#define kBaySetPowerKey	"platform-setDrivePower"

// enablers for event mechanism
#define kEnableInsertEvent "enable-drivePresent"
#define kEnableSwitchEvent "enable-driveSwitch"
#define kDisableInsertEvent "disable-drivePresent"
#define kDisableSwitchEvent "disable-driveSwitch"

// constants used for registering handlers for bay events.
#define kRegisterForInsertEvent "register-drivePresent"
#define kRegisterForSwitchEvent "register-driveSwitch"

// Bay event constants.
#define kInsertEvent 'Inst'
#define kSwitchEvent 'Swch'

//---------------------------------------------------------------------------

#define super IOService
OSDefineMetaClassAndStructors ( AppleKiwiRoot, IOService )

//---------------------------------------------------------------------------



//---------------------------------------------------------------------------

bool 
AppleKiwiRoot::init(OSDictionary* properties)
{
	DLOG("AppleKiwiRoot init start\n");


    // Initialize instance variables.
 	baseZeroMap = baseOneMap = baseTwoMap = baseThreeMap = baseFourMap = baseFiveMap = 0;
	baseAddrZero = baseAddrOne = baseAddrTwo = baseAddrThree = baseAddrFour = baseAddrFive = 0;
    pmRootDomain = 0;
    systemIsSleeping = false;
	chiplockOnBus = true;
	pdc271 = false;
	masterpllF = k100mhz_pll;
    if (super::init(properties) == false)
    {
        DLOG("AppleKiwiRoot: super::init() failed\n");
        return false;
    }
	
	
	DLOG("AppleKiwiRoot init done\n");

    return true;
}


/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
AppleKiwiRoot::probe(IOService* provider,	SInt32*	score)
{

    OSData		*compatibleEntry;
	
	DLOG("AppleKiwiRoot starting probe\n");


	compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "name" ) );
	if ( compatibleEntry == 0 ) 
	{
		// error unknown controller type.

		DLOG("AppleKiwiRoot failed getting compatible property\n");
		return 0;

	}

	
	// test the compatible property for a match to the controller type name.
	if ( compatibleEntry->isEqualTo( kNameString, sizeof(kNameString)-1 ) == false ) 
	{
		// not our type of controller		
		DLOG("AppleKiwiRoot compatible property doesn't match\n");
		return 0;
		
	}


	return this;


}

bool 
AppleKiwiRoot::start( IOService * provider )
{
	DLOG("AppleKiwiRoot: starting\n");
  
	  static const IOPMPowerState powerStates[ 2 ] = {
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, IOPMPowerOn, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

    IOPCIDevice *pciNub = (IOPCIDevice *)provider;

    if( !super::start( provider))
	return( false);

	// test if pci inline is available
	if(pciNub->configRead8( kIOPCIConfigRevisionID) < 0x03)
	{
		chiplockOnBus = true;
		kiwiChipLock = IORecursiveLockAlloc();
		if( !kiwiChipLock )
			return false;

		DLOG("AppleKiwiRoot: pci inline not detected\n");
	
	} else {
	
		DLOG("AppleKiwiRoot: pci inline available\n");
		chiplockOnBus = false;
		conf40Val = pciNub->configRead8( 0x40);
		conf40Val |= 0x01;
		pciNub->configWrite8( 0x40, conf40Val);
	}
	
	// test if 20271 controller
	if(pciNub->configRead16( kIOPCIConfigDeviceID) == 0x6269)
	{
		pdc271 = true;
		masterpllF = k133mhz_pll;
		DLOG("AppleKiwiRoot: pdc271 detected\n");
		
	} 


    // Make sure IO space is on.
    pciNub->setIOEnable(true);
    // turn on the bus-mastering enable
 	pciNub->setBusMasterEnable( true );   
	// enable mem access
	pciNub->setMemoryEnable(true);
	setupPDC270(provider);
	
	AppleKiwiIC* kiwiIC = new AppleKiwiIC;
	if( !kiwiIC )
	{
		DLOG("AppleKiwiRoot: failed to create KiwiIC\n");
		return false;
	}

	if( !kiwiIC->init(NULL) )
	{
		DLOG("AppleKiwiRoot: failed to init KiwiIC\n");
		return false;
	}

	//kiwiIC->attach( provider );

	if( !kiwiIC->start(provider,baseAddrFive) ) 
	{
		DLOG("AppleKiwiRoot: failed to start KiwiIC\n");
		return false;
	}

    pmRootDomain = getPMRootDomain();
    if (pmRootDomain != 0) {
        pmRootDomain->registerInterestedDriver(this);
    }
    
	// Show we are awake now:
    systemIsSleeping = false;

    PMinit();
    registerPowerDriver(this,(IOPMPowerState *)powerStates,2);
    joinPMtree(this);


	// create the stack
	publishBelow(provider);

	DLOG("AppleKiwiRoot: started\n");
    return( true);
}

/*---------------------------------------------------------------------------
 *	free() - the pseudo destructor. Let go of what we don't need anymore.
 *
 *
 ---------------------------------------------------------------------------*/
void
AppleKiwiRoot::free()
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

	if( kiwiChipLock )
	{
		IORecursiveLockFree( kiwiChipLock);
		kiwiChipLock = 0;
	}
	
	super::free();


}



AppleKiwiDevice * 
AppleKiwiRoot::createNub( IORegistryEntry * from )
{
    AppleKiwiDevice *	nub;

    nub = new AppleKiwiDevice;

    if( nub && !nub->init( from, gIODTPlane )) 
	{
		nub->free();
		nub = 0;
    }

    return( nub);
}

void 
AppleKiwiRoot::processNub(AppleKiwiDevice * nub)
{

	if(!chiplockOnBus)
	{	
		nub->setProperty(kPCIInlineKey, true  );
	}
	
	if( pdc271 )
	{
		nub->setProperty(kMode6Key, true  );
	}

	nub->setDeviceMemory( getProvider()->getDeviceMemory());
	nub->initProperties();
}


void 
AppleKiwiRoot::publishBelow( IORegistryEntry * root )
{
	DLOG("AppleKiwiRoot publish below\n");

    OSCollectionIterator *	kids = 0;
    IORegistryEntry *		next;
    AppleKiwiDevice *			nub;


    // publish everything below, minus excludeList
    kids = IODTFindMatchingEntries( root, kIODTRecursive | kIODTExclusive, "('ata-disk','atapi-disk','disk')");
    
    if( kids) {
    
    DLOG("AppleKiwiRoot found kids\n");
    
	while( (next = (IORegistryEntry *)kids->getNextObject())) {

            if( 0 == (nub = createNub( next )))
                continue;

            nub->attach( this );	    
			processNub(nub);	    
            if( !nub->deviceIsPresent() )
			{
				
				// float the bus pins if no drive is present;
				// figure out which bus this child is
				int busNum = 0;
				UInt32* gcrReg = (UInt32*) ( baseAddrFive + 0x1108 );
				OSString* locationCompare = OSString::withCString( "1" );
				if( locationCompare->isEqualTo( nub->getLocation() ))
				{ 
					gcrReg = (UInt32*) ( baseAddrFive + 0x1208 );
					busNum = 1;
				}
				locationCompare->release();

				*gcrReg |= 0x00000800; //LE format
				OSSynchronizeIO();
				DLOG("AppleKiwiRoot - turning off empty bus %d\n", busNum);
			}
		}
		kids->release();
    }
}

bool 
AppleKiwiRoot::compareNubName( const IOService * nub,
				OSString * name, OSString ** matched ) const
{
    return( IODTCompareNubName( nub, name, matched )
	  ||  nub->IORegistryEntry::compareName( name, matched ) );
}

IOReturn 
AppleKiwiRoot::getNubResources( IOService * nub )
{
    if( nub->getDeviceMemory())
	return( kIOReturnSuccess );

    IODTResolveAddressing( nub, "reg", getProvider()->getDeviceMemoryWithIndex(0) );

    return( kIOReturnSuccess);
}

void
AppleKiwiRoot::setupPDC270(IOService* provider)
{

	IOPCIDevice *pciNub = (IOPCIDevice *)provider;


	baseFiveMap = pciNub->mapDeviceMemoryWithRegister( kIOPCIConfigBaseAddress5 ); 
	
	if( baseFiveMap == 0 )
	{
		DLOG("Failed to get base address maps\n");
		return;
	}
	
	baseAddrFive = (UInt8*) baseFiveMap->getVirtualAddress();
	DLOG("baseAddrFive = %lx \n", (UInt32) baseAddrFive);



	//Set pll program value
	OSWriteLittleInt16((void*) baseAddrFive, 0x1202, masterpllF); 
	IOSleep( 10 );  // give it 10ms to stabilize.

	return;

}

void
AppleKiwiRoot::getLock(bool lock)
{

	if(!chiplockOnBus)
		return;
	
	if( lock )
	{
	
		IORecursiveLockLock( kiwiChipLock);
	
	} else {
	
		IORecursiveLockUnlock( kiwiChipLock);
	
	}


}


IOReturn 
AppleKiwiRoot::powerStateWillChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService* whichDevice)
{
    if ( (whichDevice == pmRootDomain) && systemIsSleeping )
	{

		if ((theFlags & IOPMPowerOn) || (theFlags & IOPMSoftSleep) ) 
		{
			DLOG("KiwiRoot::powerStateWillChangeTo waking up\n");		
			systemIsSleeping = false;
		}
	
		//DLOG("KiwiRoot::powerStateDidChangeTo acknoledging power change flags = %lx\n", theFlags);
	
	}
    return IOPMAckImplied;
}

IOReturn 
AppleKiwiRoot::powerStateDidChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService* whichDevice)
{
    if ( ( whichDevice == pmRootDomain) && !systemIsSleeping )
	{
	
		if (!(theFlags & IOPMPowerOn) && !(theFlags & IOPMSoftSleep) ) 
		{
			DLOG("KiwiRoot::powerStateDidChangeTo - going to sleep\n");		
			// time to go to sleep
			getLock(true);
			systemIsSleeping = true;
		}
	
		//DLOG("KiwiRoot::powerStateDidChangeTo acknoledging power change flags = %lx\n", theFlags);
	
    }
    return IOPMAckImplied;
}

IOReturn 
AppleKiwiRoot::setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice )
{
	//DLOG( "AppleKiwiRoot::setPowerState - entered: ordinal = %ld, whatDevice = %p\n", powerStateOrdinal, whatDevice );

	if( powerStateOrdinal == 0 && systemIsSleeping )
	{
		DLOG("AppleKiwiRoot::setPowerState - power ordinal 0\n");
	}
	

	if( powerStateOrdinal == 1 && !(systemIsSleeping) )
	{
		
		DLOG("AppleKiwiRoot::setPowerState - power ordinal 1\n");
	
		// time to wake up
		IOPCIDevice *pciNub = (IOPCIDevice *)getProvider();
		if(!chiplockOnBus)
		{
			pciNub->configWrite8( 0x40, conf40Val);
		}
		
		OSWriteLittleInt16((void*) baseAddrFive, 0x1202, masterpllF); 
		IODelay( 250 ); // allow PLL to stabilise
		getLock(false);
	}

	return IOPMAckImplied;

}

#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifdef DLOG
#undef DLOG
#endif

//#define KIWI_BAY_DEBUG 1

#ifdef  KIWI_BAY_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif
#undef super
#define super IOService

OSDefineMetaClassAndStructors(AppleKiwiDevice, IOService);
enum {
	kKiwiDevCPowerStateCount = 2
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool
AppleKiwiDevice::init( IORegistryEntry * from,
				const IORegistryPlane * inPlane )
{
	bool result = super::init(from, inPlane);
	
	bayPHandle = 0;
	bayState = kBayInitialState;
	eventGate = kEventsOff;
	childBusState = kChildBusNone;
	currPwrState = 1; // initial state is power on and running.
	return result;
}


// setup the cookies from the properties published for this nub.
void 
AppleKiwiDevice::initProperties(void)
{
	OSData* registryData = 0;
    IOReturn retval;
	static const IOPMPowerState powerStates[ kKiwiDevCPowerStateCount ] = {
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, IOPMPowerOn, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
	
	registryData = OSDynamicCast( OSData, getProperty("AAPL,phandle") );
	if( registryData )
	{
		bayPHandle = * ((UInt32*) registryData->getBytesNoCopy(0,4) );
		DLOG("Kiwi Device found %s with data %lx\n", kTrayPresentStatusKey, bayPHandle);
	}
	
	
	// register for Bay Events here	
	// we care about insertion events and removal handle switch events.

    
	char callName[255];
	
	if( bayPHandle )
	{
		snprintf(callName, sizeof(callName), "%s-%8lx", kRegisterForInsertEvent, bayPHandle);
		retval = callPlatformFunction(callName, true, (void*) &sBayEventOccured, (void*)this,
                (void*)kInsertEvent, 0  );
		DLOG("AppleKiwiDevice registering handler %s returned %x\n", callName, retval); 


	} else {
		
		DLOG( "AppleKiwiDevice bayPHandle not found, handler not registerd\n" );
	}


	if( bayPHandle )
	{
		snprintf(callName, sizeof(callName),"%s-%8lx", kRegisterForSwitchEvent, bayPHandle);
		retval = callPlatformFunction(callName, true, (void*) &sBayEventOccured, (void*) this, (void*)kSwitchEvent, 0  );
		DLOG("AppleKiwiDevice registering handler %s returned %x\n", callName, retval); 

	
	} else {
		
		DLOG( "AppleKiwiDevice bayPHandle not found, handler not registerd\n" );
	}
 
	pmRootDomain = getPMRootDomain();
    if (pmRootDomain != 0) {
        pmRootDomain->registerInterestedDriver(this);
    }
    
	// Show we are awake now:
    systemIsSleeping = false;

    PMinit();
    // register as controlling driver
    registerPowerDriver( this, (IOPMPowerState *) powerStates,
                         kKiwiDevCPowerStateCount);
    // join the tree
    getProvider()->joinPMtree( this);
//	OSCompareAndSwap(kEventsOff, kEventsOn, &eventGate  );
	enableBayEvents();



}



IOService* 
AppleKiwiDevice::matchLocation( IOService * /* client */ )
{
     // DLOG("KiwiDevice matchLocation\n");
	  return( this );
}

IOReturn 
AppleKiwiDevice::getResources( void )
{
	return( ((AppleKiwiRoot *)getProvider())->getNubResources( this ));
}


IOReturn 
AppleKiwiDevice::message (UInt32 type, IOService* provider, void* argument)
{

	// look for a sneaky message from the child
	if( provider == 0 )
	{
		switch( type )
		{
			case 'onli':
				// device has succesfully come up.
				DLOG( "AppleKiwiRoot::message - message 'onli' received.  Setting LED = green.\n" );
				childBusState = kChildBusOnline;
				setLEDColor( kLEDGreen );	
				break;
			
			case 'fail':
				// device is now down
				DLOG( "AppleKiwiRoot::message - message 'fail' received.  Setting LED = red.\n" );
				childBusState = kChildBusNone;
				setLEDColor( kLEDRed );
				break;
			
			case 'ofln':
				DLOG( "AppleKiwiRoot::message - message 'ofln' received.  Setting LED = OFF.\n" );
				setBayPower( kBayPowerOff);
				IOSleep(3000);
				setLEDColor( kLEDOff );
				break;
			
			default:
				//DLOG( "AppleKiwiRoot::message - provider == 0, unhandled message (0x%08lX) received and ignored\n", type );
				break;
		}

	}
	else
	{
		//DLOG( "AppleKiwiRoot::message - provider != 0, unhandled message (0x%08lX) being passed to ::super\n", type );
		return super::message(type, provider, argument);
	}


	return kATANoErr;

}

/// Hot bay features

// test to see if there's a device in the bay.

bool
AppleKiwiDevice::deviceIsPresent(void)
{
	UInt32 isPresent = 0;
    IOReturn retval = 0;
	char callName[255];
	
	
	if( bayPHandle )
	{
		snprintf(callName, sizeof(callName),"%s-%8lx", kTrayPresentStatusKey, bayPHandle  );
        retval = callPlatformFunction(callName, false, (void *)&isPresent, 0, 0, 0);
		
		DLOG("AppleKiwiDevice::deviceIsPresent - call platform  %s returned %08X, isPresent = %lu\n", callName, retval, isPresent); 
	}
	else
	{
		DLOG( "AppleKiwiDevice::deviceIsPresent - bayPHandle not found. isPresent == 0\n" );
	}

	if(isPresent)
	{
		
		bayState = kBayDriveNotLocked;
		
		snprintf(callName, sizeof( callName ),"%s-%8lx", kBayOpenSwitchStatusKey, bayPHandle  );
		retval = callPlatformFunction(callName, false, (void *)&isPresent, 0, 0, 0);
		DLOG("AppleKiwiDevice::deviceIsPresent - call platform  %s returned %08X, isPresent = %lu\n", callName, retval, isPresent); 		
		if(isPresent)
		{		
			bayState = kBayDriveLocked;
			makeBayReady(0);
			registerService();
		
		}
		else
		{
			//DLOG( "AppleKiwiDevice::deviceIsPresent - isPresent && kBayDriveNotLocked.  callPlatformFunction returned isPresent = 0. calling handleBayRemoved()\n" );
			handleBayRemoved();
		}
	}
	else
	{
		//DLOG( "AppleKiwiDevice::deviceIsPresent - isPresent == 0; calling handlBayRemoved()\n" );
		handleBayRemoved();
		bayState = kBayEmpty;
	}
	
	// now enable processing from the handle/bay switches.
	OSCompareAndSwap(kEventsOff, kEventsOn, &eventGate  );

	return isPresent;
}


// prepare bay by turning on power and enabling the interface
void 
AppleKiwiDevice::makeBayReady(UInt32 delayMS)
{
	setBayPower( kBayPowerOn);
	switch( childBusState )
	{
		case kChildBusNone:
		
		case kChildBusStarting: // leave the LED alone. 
		break;
		
		case kChildBusOnline:
			setLEDColor( kLEDGreen );		
		break;
		
		case kChildBusFail:
			setLEDColor(kLEDRed);
		break;
	}
}



// shutdown the bay when user starts removal
void 
AppleKiwiDevice::handleBayRemoved(void)
{
	

	// light is red now
	setLEDColor( kLEDRed );

	// message the client (ata driver) with a kATARemovedEvent (IOATATypes.h)
	IORegistryEntry *myTempchild = getChildEntry(gIOServicePlane);
    // It mUst be an IOService to be terminated:
    IOService *ioServiceChild = OSDynamicCast(IOService, myTempchild);
    
    if (ioServiceChild != NULL) 
	{
		DLOG("AppleKiwiDevice::handleBayRemoved - Sending removal message\n");
		messageClient(kATARemovedEvent, ioServiceChild, (void*)OSString::withCString( kNameString ), 0);	
		// the child driver will message us when tear down is complete
	
	}
	else
	{
		DLOG("AppleKiwiDevice::handleBayRemoved - removal event but no ioService child to message\n");
		setBayPower( kBayPowerOff);
		setLEDColor( kLEDOff );
	}
			
	childBusState = kChildBusNone;	
}


void 
AppleKiwiDevice::enableBayEvents(void)
{
	IOReturn retval;
	char callName[256];

	snprintf(callName, sizeof( callName ), "%s-%8lx", kEnableInsertEvent, bayPHandle);
	retval = callPlatformFunction(callName, true, (void*) &sBayEventOccured, (void*)this, (void*)kInsertEvent, 0  );
	snprintf(callName, sizeof( callName ), "%s-%8lx", kEnableSwitchEvent, bayPHandle);
	retval = callPlatformFunction(callName, true, (void*) &sBayEventOccured, (void*) this,(void*)kSwitchEvent, 0  ); 
	DLOG("AppleKiwiDevice::enableBayEvents - enable handler '%s' returned %x\n", callName, retval); 

}

void 
AppleKiwiDevice::disableBayEvents(void)
{

	IOReturn retval;
	char callName[256];

	snprintf(callName, sizeof( callName ), "%s-%8lx", kDisableInsertEvent, bayPHandle);
	retval = callPlatformFunction(callName, true, (void*) &sBayEventOccured, (void*)this, (void*)kInsertEvent, 0  );
	snprintf(callName, sizeof( callName ), "%s-%8lx", kDisableSwitchEvent, bayPHandle);
	retval = callPlatformFunction(callName, true, (void*) &sBayEventOccured, (void*) this,(void*)kSwitchEvent, 0  ); 
	DLOG("AppleKiwiDevice::disableBayEvents - disable handler '%s' returned %x\n", callName, retval); 


}


// bring up a newly inserted device
void 
AppleKiwiDevice::handleBayEvent(UInt32 event, UInt32 newData)
{

	if( childBusState == kChildBusStarting )
	{
		DLOG("AppleKiwiDevice::handleBayEvent - bus is still starting - handle bay event ignored.\n");
		return;
	}

	// test for an unexpected removal event first, in case the drive is forced from the 
	// socket or the handle is not working correctly as in EVT2 build.	
	if( (event == kInsertEvent) && (newData == 0)  // drive remove event 
		&& (bayState == kBayDriveLocked ) )     // wrong state!!!
	{
		DLOG("AppleKiwiDevice::handleBayEvent - drive was removed while bay in locked state!!!!\n");
		handleBayRemoved();
		bayState = kBayEmpty;
		return;
	}

	// normal operation is expected to be orderly with two kinds of events altering the state of the bay

	switch( bayState )
	{
		case kBayInitialState:  	// initial state - unknown condition
			DLOG("AppleKiwiDevice::handleBayEvent - handleBay event status unknown!\n");
			break;
		
		case kBayEmpty:    			// no drive present in bay
			// only legal event is device inserted
			if( (event == kInsertEvent) && (newData != 0) )
			{
				bayState = kBayDriveNotLocked;
				DLOG( "AppleKiwiDevice::handleBayEvent - drive inserted\n");
				
				// late change in handle design allows insertion with handle already closed.
				// this means that we won't get an event later. Check for this state and send ourself
				// a handle lock message if that happens.
				UInt32 bayBits = getBayStatus();
				if( bayBits & 0x02)
				{
					// send lock event
					handleBayEvent( kSwitchEvent, 1);
				}
			}
			else
			{
				DLOG("AppleKiwiDevice::handleBayEvent - bay empty but got some other event!\n");
			}
			break;
		
		case kBayDriveNotLocked: 	// drive present but handle unlocked.
			// two legal events - handle switch event or removal
			if( ( event == kInsertEvent ) && (newData == 0) )
			{
				// bay is now empty
				bayState = kBayEmpty;
				DLOG("AppleKiwiDevice::handleBayEvent - bay empty\n");
			}
			else if( (event == kSwitchEvent) && (newData != 0) )
			{
				// bay is locked.
				childBusState = kChildBusStarting;
				setBayPower( kBayPowerOn);
				setLEDColor( kLEDOrange );	// when the ATADriver start succeeds, the light will turn green. 	
				DLOG("AppleKiwiDevice::handleBayEvent - bay latching. Data = %x\n", (unsigned int)newData);
				IOSleep( 1000 );
				registerService();
				bayState = kBayDriveLocked;
				DLOG("AppleKiwiDevice::handleBayEvent - handle locked, starting disk.\n");
				
			}
			else
			{			
				DLOG("AppleKiwiDevice::handleBayEvent - kBayDriveNotLocked got unexpected event %08lX, data %08lX\n", event, newData);
			}
		break;
		
		
		case kBayDriveLocked : 		// drive is present and locked in place.
			// only legal event is switch
			if( event == kSwitchEvent && newData == 0) 
			{					
				DLOG("AppleKiwiDevice::handleBayEvent - handle is being unlocked, stopping disk. Data = %x\n", (unsigned int) newData);
				handleBayRemoved();
				bayState = kBayDrivePendRemoval;
				
			}
			else
			{			
				DLOG("AppleKiwiDevice::handleBayEvent - kBayDriveLocked got unexpected event %08lX, data %08lX\n", event, newData);
			}
		
		break;
		
		case kBayDrivePendRemoval: // drive has been unlocked and must be removed for bay to activate again.
		if( (event == kInsertEvent) && (newData == 0) ) // drive remove event 	
		{
			// bay is now empty
			bayState = kBayEmpty;
			DLOG("AppleKiwiDevice::handleBayEvent - bay empty from pend-removal\n");
		}
		else
		{
			DLOG("AppleKiwiDevice::handleBayEvent - event while in pend-removal state\n");
			
			// ignore the event and leave the state in place. 
			;
		}
		break;
		
		// should never get here.
		default:    
			DLOG("AppleKiwiDevice::handleBayEvent - (default case) handled unexpected default event %08lX, data %08lX\n", event, newData);
			break;
	}

//	enableBayEvents();



}	

// static handler registered with the system 	
void 
AppleKiwiDevice::sBayEventOccured( void* p1, void* p2, void* p3, void* p4)
{

	AppleKiwiDevice* me = (AppleKiwiDevice*) p1;

	if( OSCompareAndSwap( kEventsOn, kEventsOff, &(me->eventGate)  ) )
	{
		me->handleBayEvent( (UInt32)p2, (UInt32)p4 );	
		
		OSCompareAndSwap( kEventsOff, kEventsOn,  &(me->eventGate)  );
	}

}


void
AppleKiwiDevice::setLEDColor( UInt32 color)
{

	char callGreen[255];
	char callRed[255];
	// enable power by calling platform function
	if( bayPHandle )
	{
	
		snprintf(callGreen, sizeof( callGreen ), "%s-%8lx", kBaySetActiveLEDKey, bayPHandle );
		snprintf(callRed,   sizeof( callRed ),   "%s-%8lx", kBaySetFailLEDKey,   bayPHandle );
	
		switch( color )
		{
		
			case kLEDOff:
				callPlatformFunction(callGreen, false, (void *)0, 0, 0, 0);
				callPlatformFunction(callRed, false, (void *)0, 0, 0, 0);
			break;
			
			case kLEDGreen:
				callPlatformFunction(callGreen, false, (void *)1, 0, 0, 0);
				callPlatformFunction(callRed, false, (void *)0, 0, 0, 0);
			break;
			
			case kLEDOrange:
				callPlatformFunction(callGreen, false, (void *)1, 0, 0, 0);
				callPlatformFunction(callRed, false, (void *)1, 0, 0, 0);
			break;
			
			case kLEDRed:
				callPlatformFunction(callGreen, false, (void *)0, 0, 0, 0);
				callPlatformFunction(callRed, false, (void *)1, 0, 0, 0);
			break;
			
	
		default:
		break;		
		}
	}

}

void 
AppleKiwiDevice::setBayPower( UInt32 powerState )
{

	// disable power
	char callName[255];

	// enable power by calling platform function
	if( bayPHandle )
	{

		snprintf(callName, sizeof( callName ),"%s-%8lx", kBaySetPowerKey, bayPHandle  );
		
		switch( powerState )
		{
		
			case kBayPowerOn:
				DLOG( "AppleKiwiDevice::setBayPower - turning on drive bay power\n" );
				callPlatformFunction(callName, false, (void*)1, 0, 0, 0);  // pass 1 in first parm to enable power
				break;
				
			case kBayPowerOff:
				DLOG( "AppleKiwiDevice::setBayPower - turning OFF drive bay power\n" );
				callPlatformFunction(callName, false, 0, 0, 0, 0);  // pass 0 in first parm to disable power
				break;
			
			default:
				DLOG( "AppleKiwiDevice::setBayPower - power state not ON or OFF.  Ignoring...\n" );
				break;
		}
	}

}

UInt32 
AppleKiwiDevice::getBayStatus( void )
{

	IOReturn errVal = 0;
	UInt32 platformStatus = 0;
	int retries;
	
	UInt32 bayBits = 0;  // bit 0 = presence, bit 1 = handle state
	char callName[255];

	snprintf(callName, sizeof( callName ), "%s-%8lx", kTrayPresentStatusKey, bayPHandle  );

	for( retries = 0; retries < 100; retries ++ )
	{
		errVal = callPlatformFunction(callName, false, (void *)&platformStatus, 0, 0, 0);
		DLOG( "AppleKiwiDevice::getBayStatus(retry %d) - callPlatformFunction('%s') returned %d (0x%08X)\n", retries, callName, errVal, errVal );
		if ( errVal == kIOReturnSuccess )
			break;
		IOSleep( 100 );	// wait 100ms and try again
	}
	
	if( platformStatus )
	{
		bayBits |= 0x01;
	}
	
	platformStatus = 0;
	snprintf(callName, sizeof( callName ), "%s-%8lx", kBayOpenSwitchStatusKey, bayPHandle  );
	for( retries = 0; retries < 100; retries ++ )
	{
		errVal = callPlatformFunction(callName, false, (void *)&platformStatus, 0, 0, 0);
		DLOG( "AppleKiwiDevice::getBayStatus(retry %d) - callPlatformFunction('%s') returned %d (0x%08X)\n", retries, callName, errVal, errVal );
		if ( errVal == kIOReturnSuccess )
			break;
		IOSleep( 100 );	// wait 100ms and try again
	}

	if( platformStatus )
	{
		bayBits |= (0x02);
	}

	
	if(bayBits == 0x02 )
	{
		DLOG("AppleKiwiDevice::getBayStatus - bay handles in invalid state!!!\n");
		;
	}

	return bayBits;
}


IOReturn 
AppleKiwiDevice::setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice )
{
	if( powerStateOrdinal == 0 && systemIsSleeping )
	{
		DLOG("AppleKiwiDevice::setPowerState - power ordinal 0\n");
	
		currPwrState = 0;  // offline or sleep.  
	
	}
	
	UInt32 states[4] = { kBayEmpty, kBayDriveNotLocked, 0xffffffff, kBayDriveLocked};  // 00=empty, 01=present but not locked, 10=invalid state, 11=locked

	if( powerStateOrdinal == 1
		&& (currPwrState == 0)
		&& !(systemIsSleeping) )
	{
		
		DLOG("AppleKiwiDevice::setPowerState - power ordinal 1\n");
		if( bayState == kBayDrivePendRemoval )
		{
			// qualify a sleep event as the same as removal and insert back to the unlocked state
			bayState = kBayDriveNotLocked;
		
		}
		UInt32 bayBits = getBayStatus();
		UInt32 bayStatNow = states[bayBits];
		if( bayBits == 0x02 )
		{
			// invalid state - handle locked but no carrier present?
			bayStatNow = kBayEmpty;
		}
		
		if( bayStatNow == bayState )
		{
			//everything is cool just restore the previous power state
			if( bayState == kBayDriveLocked)
			{
				makeBayReady( 0 );
			}
			else
			{
				setLEDColor( kLEDOff );
				setBayPower( kBayPowerOff );
			}		
		}
	
		// something has changed across sleep - we will now callHandleBayEvent with
		// the event and the state to move the bay state machine along to reflect the current bay status. 	
		switch( bayState )
		{
		
			case kBayEmpty:
				if( bayBits & 0x01 )
				{ 
					// send insert event
					DLOG( "AppleKiwiDevice::setPowerState - send insert event\n" );
					handleBayEvent( kInsertEvent, 1);
				}
				if( bayBits & 0x02)
				{
					// send lock event
					DLOG( "AppleKiwiDevice::setPowerState - send lock event\n" );
					handleBayEvent( kSwitchEvent, 1);
				}
			break;

			case kBayDriveNotLocked:
				if( !(bayBits & 0x01) )
				{ 
					// send remove event
					DLOG( "AppleKiwiDevice::setPowerState - send remove event\n" );
					handleBayEvent( kInsertEvent, 0);
				
				}
				else if( bayBits & 0x02)
				{
					// send lock event
					DLOG( "AppleKiwiDevice::setPowerState - send lock event\n" );
					handleBayEvent( kSwitchEvent, 1);
				}
			break;
			
			case kBayDriveLocked:
				if( !(bayBits & 0x02))
				{
					//send unlock event
					DLOG( "AppleKiwiDevice::setPowerState - send unlock event\n" );
					handleBayEvent( kSwitchEvent, 0);
				}
				
				if( !(bayBits & 0x01) )
				{
					// send remove event
					DLOG( "AppleKiwiDevice::setPowerState - send remove event\n" );
					handleBayEvent( kInsertEvent, 0);				
				}
				break;

			default:
				// nonsensical state
				//DLOG( "AppleKiwiDevice::setPowerState - unexpected bay state (%ld, 0x%08lX).  Ignoring ...\n", bayState, bayState );
				break;
		}
		
		
				
	}
	return IOPMAckImplied;

}

IOReturn 
AppleKiwiDevice::powerStateWillChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService* whichDevice)
{
    if ( ( whichDevice == pmRootDomain) && ! systemIsSleeping )
	{
		if (!(theFlags & IOPMPowerOn) && !(theFlags & IOPMSoftSleep) ) 
		{
			DLOG("AppleKiwiDevice::powerStateWillChangeTo - going to sleep\n");		
			// time to go to sleep
			systemIsSleeping = true;
		}
    }
	else
	{
		DLOG( "AppleKiwiDevice::powerStateWillChangeTo - state change not handled!\n" );
		;
	}
	
    return IOPMAckImplied;
}

IOReturn 
AppleKiwiDevice::powerStateDidChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService* whichDevice)
{
    if ( (whichDevice == pmRootDomain) && systemIsSleeping )
	{
		if ((theFlags & IOPMPowerOn) || (theFlags & IOPMSoftSleep) ) 
		{
			DLOG("AppleKiwiDevice::powerStateDidChangeTo - waking up\n");		
			// time to wake up
			systemIsSleeping = false;
		}
	}
	else
	{
		DLOG( "AppleKiwiDevice::powerStateDidChangeTo - state change not handled!\n" );
		;
	}

    return IOPMAckImplied;
}


#pragma mark -
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifdef DLOG
#undef DLOG
#endif


#ifdef  KIWI_IC_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif
#undef super
#define super IOService

#undef super
#define super IOInterruptController

OSDefineMetaClassAndStructors(AppleKiwiIC, IOInterruptController);

enum {
	kIOSLICPowerStateCount = 2
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool 
AppleKiwiIC::start(IOService *provider, UInt8* inBar5)
{
	if( !provider || !inBar5 )
		return false;
 
//	static const IOPMPowerState powerStates[ kIOSLICPowerStateCount ] = {
//        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
//        { 1, IOPMPowerOn, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
//    };
		
	gcr0 = (volatile UInt32*) (inBar5 + 0x1108);
	gcr1 = (volatile UInt32*) (inBar5 + 0x1208);
	
	if (!super::start(provider)) 
	{
		DLOG ("AppleKiwiIC::start - super::start(provider) returned false\n");
		return false;
	}

	// remember this, as we will need it to call disable/enableInterrupt
	myProvider = provider;

	interruptControllerName = (OSSymbol *)IODTInterruptControllerName( provider );
	if (interruptControllerName == 0) {
		DLOG ("AppleKiwiIC::start - interruptControllerName is NULL\n");
		return false;
	}

	UInt32 numVectors = 2;
	UInt32 sizeofVectors = numVectors * sizeof(IOInterruptVector);
  
	// Allocate the memory for the vectors.
	vectors = (IOInterruptVector *)IOMalloc(sizeofVectors);
	if (vectors == NULL) {
		DLOG ("AppleKiwiIC::start - cannot allocate vectors\n");
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
			DLOG ("AppleKiwiIC::start - cannot allocate lock for vectors[%d]\n", cnt);
			return false;
		}
	}


	// initialize the interrupt control bits to mask propogation
	*gcr0 &= (~ 0x00000200);  // little endian format
	*gcr1 &=  (~ 0x00000400); // little endian format
	
	provider->registerInterrupt(0, this, getInterruptHandlerAddress(), 0);
	
	// Register this interrupt controller so clients can find it.
	getPlatform()->registerInterruptController(interruptControllerName, this);

	provider->enableInterrupt(0);
       
	DLOG ("AppleKiwiIC::start - finished\n");

	return true;

}
  
IOReturn
AppleKiwiIC::getInterruptType(IOService *nub, int source,
				    int *interruptType)
{

	if (interruptType == 0) return kIOReturnBadArgument;
  
	*interruptType = kIOInterruptTypeLevel;
  
	return kIOReturnSuccess;

}
  
IOInterruptAction 
AppleKiwiIC::getInterruptHandlerAddress(void)
{

//	return  (IOInterruptAction) &AppleKiwiIC::handleInterrupt ;
	// change for gcc 4
	return  OSMemberFunctionCast(IOInterruptAction, this, &AppleKiwiIC::handleInterrupt) ;
	


}

IOReturn 
AppleKiwiIC::handleInterrupt( 	void* /*refCon*/,
								IOService* /*nub*/,
								int /*source*/ )
{

	long				vectorNumber = 0;
	IOInterruptVector	*vector;
//	unsigned short		events;


	// iterate each possible interrupt vector and let it decide whether it wants to 
	// handle the interrupt. 
	for(int i = 0; i < 2; i++)
	{
		vectorNumber = i;
		vector = &vectors[vectorNumber];
    
		vector->interruptActive = 1;
#if defined( __PPC__ )
		sync();
		isync();
#endif
		if (!vector->interruptDisabledSoft)
		{
#if defined( __PPC__ )
			isync();
#endif
      			
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
AppleKiwiIC::vectorCanBeShared(long vectorNumber, IOInterruptVector *vector)
{

	return false;
}



void 
AppleKiwiIC::initVector(long vectorNumber, IOInterruptVector *vector)
{

	return;
}

void 
AppleKiwiIC::disableVectorHard(long vectorNumber, IOInterruptVector *vector)
{
	myProvider->disableInterrupt( 0 );

#if 0
	switch( vectorNumber )
	{
		case 0:
			//*gcr0 |= 0x00000200; // little endian formatted
			OSSynchronizeIO();
		break;
		
		case 1:
			//*gcr1 |= 0x00000400; // little endian formatted
			OSSynchronizeIO();
		break;
		
		default:
		break; // ??? should not happen
	}


}
#endif
}


void 
AppleKiwiIC::enableVector(long vectorNumber, IOInterruptVector *vector)
{
	myProvider->enableInterrupt( 0 );

#if 0
	switch( vectorNumber )
	{
		case 0:
			//*gcr0 &= (~ 0x00000200);  // little endian formatted
			OSSynchronizeIO();
		break;
		
		case 1:
			//*gcr1 &=  (~ 0x00000400); // little endian formatted
			OSSynchronizeIO();
		break;
		
		default:
		break; // ??? should not happen
	}
#endif



}


IOReturn 
AppleKiwiIC::setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice )
{

	return IOPMAckImplied;

}


