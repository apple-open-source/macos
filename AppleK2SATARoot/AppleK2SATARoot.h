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
 
 
#ifndef _APPLEK2SATAROOT_H
#define _APPLEK2SATAROOT_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOFilterInterruptEventSource.h>

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOFilterInterruptEventSource.h>

class AppleK2SATADevice;
class AppleK2SATAIC;

class AppleK2SATARoot : public IOService
{
    OSDeclareDefaultStructors(AppleK2SATARoot)

public:

	/*--- Overrides from IOService ---*/
	virtual bool init(OSDictionary* properties);

	virtual IOService* probe( IOService* provider,	SInt32*	score );

	virtual bool start( IOService * provider );

    virtual AppleK2SATADevice * createNub( IORegistryEntry * from );

    virtual void processNub( AppleK2SATADevice * nub );

	void publishBelow( IORegistryEntry * root );


    virtual bool compareNubName( const IOService * nub, OSString * name,
				 OSString ** matched = 0 ) const;

    virtual IOReturn getNubResources( IOService * nub );	
	
	// pm stuff
	virtual IOReturn setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice ); 	

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
	
	AppleK2SATAIC*			intController;	
	
	bool					isSleeping;
	
	IOInterruptEventSource*  nubIntSrc;
	
	UInt32					restoreSCR2[4];
	UInt32					restoreSICR1[4];
	
	UInt32					cellRevision;

	
	//OSObject overrides
	virtual void free();
	static void sDeviceInterruptOccurred(OSObject*, IOInterruptEventSource *, int count);

    //specialty stuff 
	IOReturn readMDIO( UInt8 registerAddr, UInt16& value );
	IOReturn writeMDIO( UInt8 registerAddr, UInt16 value );

};



class AppleK2SATADevice : public IOService
{
  OSDeclareDefaultStructors(AppleK2SATADevice);
  
public:
  
//  virtual bool compareName( OSString * name, OSString ** matched = 0 ) const;
  virtual IOService *matchLocation(IOService *client);
  virtual IOReturn getResources( void );

	      
protected:
	
    
 
};


class AppleK2SATAIC : public IOInterruptController
{
	OSDeclareDefaultStructors(AppleK2SATAIC);
	
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
 	

protected:

	volatile UInt32* gcr0;
	volatile UInt32* gcr1;
	OSSymbol				*interruptControllerName;
};

#endif // _APPLEK2SATAROOT_H
