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
 
 
#ifndef _DRV_KEY_LARGO_ATA_H
#define _DRV_KEY_LARGO_ATA_H

#include <IOKit/IOTypes.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATABusInfo.h>
#include  <IOKit/ata/MacIOATA.h>
#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/IOMemoryCursor.h>


/*! @class KeyLargoATA : public MacIOATA
    @abstract The specific driver for keylargo-ata controllers.
    @discussion class contains all of the code specific to matching and
    running keylargo ata controllers.

*/    

class KeyLargoATA : public MacIOATA
{
    OSDeclareDefaultStructors(KeyLargoATA)

public:

	/*--- Overrides from IOService ---*/
	virtual bool init(OSDictionary* properties);

	// checks for the compatible property of "keylargo-ata" 
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


	struct ATABusTimings
	{
		UInt32			cycleRegValue;					// hardware register setting
		UInt8			ataPIOSpeedMode;				// PIO Mode Timing class (bit-significant)
		UInt16			ataPIOCycleTime;				// Cycle time for PIO mode
		UInt8			ataMultiDMASpeed;				// Multiple Word DMA Timing Class (bit-significant)
		UInt16			ataMultiCycleTime;				// Cycle time for Multiword DMA mode
		UInt16			ataUltraDMASpeedMode;			// Ultra Timing class (bit-significant)
	};


	ATABusTimings busTimings[2];
	bool	isUltraCell;
	bool	cableIs80Conductor;
	bool _needsResync;
	// calculate the correct binary configuration for the desired bus timings.
	virtual IOReturn selectIOTimerValue( IOATADevConfig* configRequest, UInt32 unitNumber);
	bool isExtLBA;
	// overrides
	// set the timing config for a specific device.	
	virtual void selectIOTiming( ataUnitID unit );


	// override because we need to set the timing config to an initial value
	// first.
	virtual bool configureTFPointers(void);


	// overriden to allow forced clearing of by reading the status register.
	virtual IOReturn handleDeviceInterrupt(void);

	// override for hardware specific issue.
	virtual IOReturn synchronousIO(void);	

	// override for hardware specific issue.
	virtual IOReturn selectDevice( ataUnitID unit );

	//OSObject overrides
	virtual void free();
	
	// override to handle special case device.
	virtual IOReturn handleBusReset(void);
	// override for special case device.
	virtual IOATAController::transState KeyLargoATA::determineATAPIState(void);
	// override for special case device
	virtual void processDMAInterrupt (void);

	

};

#endif	// _DRV_KEY_LARGO_ATA_H
