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
 
 
#ifndef _DRV_HEATHROW_ATA_H
#define _DRV_HEATHROW_ATA_H

#include <IOKit/IOTypes.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATABusInfo.h>
#include <IOKit/ata/MacIOATA.h>
#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/IOMemoryCursor.h>


/*! @class HeathrowATA : public MacIOATA
    @abstract The specific driver for heathrow-ata controllers.
    @discussion class contains all of the code specific to matching and
    running heathrow ata controllers.

*/    

class HeathrowATA : public MacIOATA
{
    OSDeclareDefaultStructors(HeathrowATA)

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


	struct ATABusTimings
	{
		UInt32			cycleRegValue;					// hardware register setting
		UInt8			ataPIOSpeedMode;				// PIO Mode Timing class (bit-significant)
		UInt16			ataPIOCycleTime;				// Cycle time for PIO mode
		UInt8			ataMultiDMASpeed;				// Multiple Word DMA Timing Class (bit-significant)
		UInt16			ataMultiCycleTime;				// Cycle time for Multiword DMA mode
	};


	ATABusTimings busTimings[2];
	
	// calculate the correct binary configuration for the desired bus timings.
	virtual IOReturn selectIOTimerValue( IOATADevConfig* configRequest, UInt32 unitNumber);

	// overrides
	// set the timing config for a specific device.	
	virtual void selectIOTiming( ataUnitID unit );


	// override because we need to set the timing config to an initial value
	// first.
	virtual bool configureTFPointers(void);
	
	// override because the booter may not have reset the bus prior to turning control over 
	// to the kernel.
	virtual UInt32 scanForDrives( void );
	
	// override for hardware specific issue.
	virtual IOReturn synchronousIO(void);	
	virtual IOReturn selectDevice( ataUnitID unit );
	virtual IOReturn handleDeviceInterrupt(void);

	//OSObject overrides
	virtual void free();


};

#endif	// _DRV_HEATHROW_ATA_H
