/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 
 
#ifndef _APPLE_KIWI_ATA_
#define _APPLE_KIWI_ATA_

#include <IOKit/IOTypes.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATABusInfo.h>
#include <IOKit/ata/IOPCIATA.h>
#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOFilterInterruptEventSource.h>

#include "AppleKiwiRoot.h"


class AppleKiwiATA : public IOPCIATA
{
    OSDeclareDefaultStructors(AppleKiwiATA)

public:

	/*--- Overrides from IOService ---*/
	virtual bool init(OSDictionary* properties);

	virtual IOService* probe( IOService* provider,	SInt32*	score );
	virtual bool start( IOService* provider );
	virtual IOWorkLoop*	getWorkLoop() const;

	// set and get bus timing configuration for a specific unit 
	virtual IOReturn selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber);
	virtual IOReturn getConfig( IOATADevConfig* configRequest, UInt32 unitNumber); 

	// provide information on bus capability
	virtual IOReturn provideBusInfo( IOATABusInfo* infoOut);
	// listen for device removed message
	virtual IOReturn message (UInt32 type, IOService* provider, void* argument = 0);
	virtual void getLock(bool lock); // true to lock, false to unlock.


protected:




	struct ATABusTimings
	{

		UInt8			ataPIOSpeedMode;				// PIO Mode Timing class (bit-significant)
		UInt16			ataPIOCycleTime;				// Cycle time for PIO mode
		UInt8			ataMultiDMASpeed;				// Multiple Word DMA Timing Class (bit-significant)
		UInt16			ataMultiCycleTime;				// Cycle time for Multiword DMA mode
		UInt16			ataUltraDMASpeedMode;			// Ultra Timing class (bit-significant)
	};

	ATABusTimings busTimings[2];

	// base address mappings for the PCI regs. 0 = cmd block, 1 = ctrl block, 2 = BusMaster block, 3 = mmio area
	IOMemoryMap*			ioBaseAddrMap[4];
	UInt32	busChildNumber;
 	bool	isBusOnline; // true = drive in slot, false = ejected
	
	AppleKiwiRoot* chipRoot;

 	// interrupt event source
	IOInterruptEventSource* _devIntSrc;
		
	volatile UInt32* globalControlReg; // extended control area in memspace from BAR5
	volatile UInt32* timingAReg0;
	volatile UInt32* timingBReg0;
	volatile UInt32* timingAReg1;
	volatile UInt32* timingBReg1;
	bool forcePCIInline;
	bool mode6Capable;
	bool reconfigureTiming; 

	// calculate the correct binary configuration for the desired bus timings.
	virtual IOReturn selectIOTimerValue( IOATADevConfig* configRequest, UInt32 unitNumber);

	// overrides
	// set the timing config for a specific device.	
	virtual void selectIOTiming( ataUnitID unit );

	// override because we need to set the timing config to an initial value
	// first.
	virtual bool configureTFPointers(void);

	// connect the device (drive) interrupt to our workloop
	virtual bool createDeviceInterrupt(void);
	// c to c++ glue code.
	static void sDeviceInterruptOccurred(OSObject*, IOInterruptEventSource *, int count);
//	static bool sFilterInterrupt(OSObject *, IOFilterInterruptEventSource *);

	virtual IOReturn handleDeviceInterrupt(void);
	bool interruptIsValid( IOFilterInterruptEventSource* source);
	
	virtual void handleTimeout( void );

	// override from IOATAController
	// defer activating the DMA engine until after the command is written to the drive. chip-specific.
	virtual IOReturn startDMA( void );
	virtual IOReturn issueCommand( void );

	// removable bus support
	virtual IOReturn executeCommand(IOATADevice* nub, IOATABusCommand* command); //
	virtual IOReturn handleCommand(	void* param0, void* param1, void* param2,void* param3 );	
	virtual IOReturn handleQueueFlush( void ); //
	virtual bool checkTimeout( void );  //
	static void cleanUpAction(OSObject * owner, void*, void*, void*, void*); //
	virtual void cleanUpBus(void); //

	//OSObject overrides
	virtual void free();


};

#endif	// _APPLE_KIWI_ATA_
