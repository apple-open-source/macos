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
 
 
#ifndef _DRV_CMD646_ATA_H
#define _DRV_CMD646_ATA_H

#include <IOKit/IOTypes.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATABusInfo.h>
#include <IOKit/ata/IOPCIATA.h>
#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOFilterInterruptEventSource.h>


/*! @class CMD646ATA : public MacIOATA
    @abstract The specific driver for cmd646-ata controllers as shipped in apple equipment.
    @discussion class contains all of the code specific to matching and
    running heathrow ata controllers.

*/    

class CMD646ATA : public IOPCIATA
{
    OSDeclareDefaultStructors(CMD646ATA)

public:

	/*--- Overrides from IOService ---*/
	virtual bool init(OSDictionary* properties);

	// checks for the compatible property of "heathrow-ata" 
	// in the device tree.
	virtual IOService* probe( IOService* provider,	SInt32*	score );
	virtual bool start( IOService* provider );
	virtual IOWorkLoop*	getWorkLoop() const;

	// set and get bus timing configuration for a specific unit 
	virtual IOReturn selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber);
	virtual IOReturn getConfig( IOATADevConfig* configRequest, UInt32 unitNumber); 

	// provide information on bus capability
	virtual IOReturn provideBusInfo( IOATABusInfo* infoOut);



protected:

	enum
	{
			kUltraDev0Mask = 0x31,
			kUltraDev1Mask = 0xC2
	};


// offsets into PCI config space for various CMD registers
// see manual for definitions.
enum{

	kPCIStatusCmd = 0x04,

	kPrimaryCmd = 0x10,
	kPrimaryCntrl = 0x14,
	kSecondCmd = 0x18,
	kSecondCtrl = 0x1C,	
	kBusMaster = 0x20,

	kCRNTL 		= 0x51,
	kCMDTIM 	= 0x52,
	kARTTIM0	= 0x53,
	kDRWTIM0 	= 0x54,
	kARTTIM1	= 0x55,
	kDRWTIM1 	= 0x56,
	
	kARTTIM23 	= 0x57,
	kDRWTIM2 	= 0x58,
	kDRWTIM3 	= 0x5B,

	kUDIDETCR0 	= 0x73,
	kUDIDETCR1 	= 0x7B
	

};



	struct ATABusTimings
	{

		UInt8		pioAddrSetupValue;
		UInt8		pioActiveRecoveryValue;

		UInt8		dmaAddrSetupValue;
		UInt8		dmaActiveRecoveryValue;

		UInt8		ultraTimingValue;

		UInt8			ataPIOSpeedMode;				// PIO Mode Timing class (bit-significant)
		UInt16			ataPIOCycleTime;				// Cycle time for PIO mode
		UInt8			ataMultiDMASpeed;				// Multiple Word DMA Timing Class (bit-significant)
		UInt16			ataMultiCycleTime;				// Cycle time for Multiword DMA mode
		UInt16			ataUltraDMASpeedMode;			// Ultra Timing class (bit-significant)
	};

	ATABusTimings busTimings[2];

	// base address mappings for the PCI regs. 0 = cmd block, 1 = ctrl block, 2 = BusMaster block
	IOMemoryMap*			ioBaseAddrMap[3];
 
 	// interrupt event source
	IOFilterInterruptEventSource* _devIntSrc;

	IOService* _cmdRoot;
	IOPCIDevice* _pciNub;
	
	volatile UInt8* _mrdModeReg; // CMD interrupt and control reg
	volatile UInt8* _udideTCR0;  // ultra timing mode register.
 	UInt8		currentActiveRecoveryValue[2]; 
	
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
	static bool sFilterInterrupt(OSObject *, IOFilterInterruptEventSource *);

	virtual IOReturn handleDeviceInterrupt(void);
	bool interruptIsValid( IOFilterInterruptEventSource* source);
	
	virtual void handleTimeout( void );

	//OSObject overrides
	virtual void free();


};

#endif	// _DRV_CMD646_ATA_H
