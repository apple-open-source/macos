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
 
 
#ifndef _AppleKiwiROOT_H
#define _AppleKiwiROOT_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOFilterInterruptEventSource.h>
/*! @class AppleKiwiRoot : public IOService
    @abstract The specific driver for AppleKiwi-ata controllers as shipped in apple equipment.
    @serves as the place holder in the device tree for the pdc20270 chip vs. the kiwi-ATA controllers.

*/    

class AppleKiwiDevice;
class AppleKiwiIC;

class AppleKiwiRoot : public IOService
{
    OSDeclareDefaultStructors(AppleKiwiRoot)

public:

	/*--- Overrides from IOService ---*/
	virtual bool init(OSDictionary* properties);

	virtual IOService* probe( IOService* provider,	SInt32*	score );

	virtual bool start( IOService * provider );

    virtual AppleKiwiDevice * createNub( IORegistryEntry * from );

    virtual void processNub( AppleKiwiDevice * nub );

	void publishBelow( IORegistryEntry * root );


    virtual bool compareNubName( const IOService * nub, OSString * name,
				 OSString ** matched = 0 ) const;

    virtual IOReturn getNubResources( IOService * nub );	

	virtual void getLock(bool lock);  // true to lock, false to unlock
// PM stuff
    virtual IOReturn powerStateWillChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService*);
    virtual IOReturn powerStateDidChangeTo ( IOPMPowerFlags theFlags, unsigned long, IOService*); 
	virtual IOReturn setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice ); 	

/// changes for Promise
protected:

	IOMemoryMap*			baseZeroMap;
	IOMemoryMap*			baseOneMap;
	IOMemoryMap*			baseTwoMap;
	IOMemoryMap*			baseThreeMap;
	IOMemoryMap*			baseFourMap;
	IOMemoryMap*			baseFiveMap;
	
	UInt8*					baseAddrZero;
	UInt8*					baseAddrOne;
	UInt8*					baseAddrTwo;
	UInt8*					baseAddrThree;
	UInt8*					baseAddrFour;
	UInt8*					baseAddrFive;
	AppleKiwiIC*			kiwiInterruptController;
	IORecursiveLock*		kiwiChipLock;
    IOPMrootDomain			*pmRootDomain;
    bool 					systemIsSleeping;
	bool					chiplockOnBus;
	bool					pdc271;
	UInt16					masterpllF;
	void setupPDC270(IOService* _pciNub);
	UInt8 					conf40Val;
	
//	virtual AppleKiwiIC* createInterruptController(void);
	
	//OSObject overrides
	virtual void free();


};


/*! @class AppleKiwiDevice : public IOService
	@abstract The nub published by the pdc20270 chip, which represents the nub
	to which the IOATAcontroller will attach in the device tree.
*/

class AppleKiwiDevice : public IOService
{
  OSDeclareDefaultStructors(AppleKiwiDevice);
  
public:
	/*--- Overrides from IOService ---*/
	virtual bool init( IORegistryEntry * from,
				const IORegistryPlane * inPlane );
  
	// name matching
	virtual IOService *matchLocation(IOService *client);
	virtual IOReturn getResources( void );

	// hot bay feature.
	// test whether tray is occupied. 
	virtual void initProperties(void);
	virtual bool deviceIsPresent(void);
 
// PM stuff
    virtual IOReturn powerStateWillChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService*);
    virtual IOReturn powerStateDidChangeTo ( IOPMPowerFlags theFlags, unsigned long, IOService*); 

	virtual IOReturn setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice ); 	

	      
protected:
	
	enum { 	
			kBayInitialState = 'Init',  // initial state - unknown condition
			kBayEmpty = 'Emty',     // no drive present in bay
			kBayDriveNotLocked = 'nLoc', // drive present but handle unlocked.
			kBayDriveLocked = 'Lock', // drive is present and locked in place.
			kBayDrivePendRemoval = 'Rmvl', //drive still in bay and will not function until removed
			kLEDOff = 'Loff',
			kLEDGreen = 'Lgrn',
			kLEDOrange = 'Lorg',
			kLEDRed = 'Lred',
			kBayPowerOn = 'Bay+',
			kBayPowerOff = 'Bay-',
			kBusEnable = 'Bus+',
			kBusDisable = 'Bus-',
			kEventsOn = 'Evt+',
			kEventsOff = 'Evt-',
			kChildBusNone = 'none',
			kChildBusStarting = 'strt',
			kChildBusOnline = 'onli',
			kChildBusFail = 'fail'
	};
    
	UInt32 bayPHandle;
   	
	UInt32 bayState;
	
	UInt32 eventGate;
	
	UInt32 childBusState;

	UInt32 currPwrState;  // 0 = off or sleep. 1 = power on. 
    IOPMrootDomain			*pmRootDomain;
    bool 					systemIsSleeping;
	
	// override of IOService

	virtual IOReturn message (UInt32 type, IOService* provider, void* argument = 0);

	
	// prepare bay by turning on power and enabling the interface
	virtual void makeBayReady(UInt32 delayMS);
	
	// shutdown the bay when user starts removal
	virtual void handleBayRemoved(void);
	
	// respond to bay events, insert, remove. 
	virtual void handleBayEvent(UInt32 event, UInt32 newData);
	
	virtual void setLEDColor( UInt32 color);
	virtual void setBayPower( UInt32 powerState );
	virtual UInt32 getBayStatus( void );
	virtual void enableBayEvents(void);
	virtual void disableBayEvents(void);
	
	static void sBayEventOccured( void* p1, void* p2, void* p3, void* p4);
	
 
};

class AppleKiwiIC : public IOInterruptController
{
	OSDeclareDefaultStructors(AppleKiwiIC);
	
public:
	virtual bool start(IOService *provider, UInt8* bar5);
  
	virtual IOReturn getInterruptType(IOService *nub, int source,
				    int *interruptType);
  
	virtual IOInterruptAction getInterruptHandlerAddress(void);

	virtual IOReturn handleInterrupt(   void *refCon,
										IOService *nub,
										int source );
  
	virtual bool vectorCanBeShared(long vectorNumber, IOInterruptVector *vector);
	virtual void initVector(long vectorNumber, IOInterruptVector *vector);
	virtual void disableVectorHard(long vectorNumber, IOInterruptVector *vector);
	virtual void enableVector(long vectorNumber, IOInterruptVector *vector);
	virtual IOReturn setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice ); 	

protected:

	volatile UInt32* gcr0;
	volatile UInt32* gcr1;
	OSSymbol		*interruptControllerName;
	IOService		* myProvider;
};

#endif // _AppleKiwiROOT_H
