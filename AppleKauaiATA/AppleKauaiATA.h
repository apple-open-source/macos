/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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
 
 
#ifndef _DRV_AAPL_KAUAI_ATA_H
#define _DRV_AAPL_KAUAI_ATA_H

#include <IOKit/IOTypes.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATABusInfo.h>
#include  <IOKit/ata/MacIOATA.h>
#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

//permanently enabled for Tiger and later
//#ifdef _FN_KPRINTF_DECLARED  
#include <IOKit/IOPolledInterface.h>
#define __KAUAI_POLLED__
#define kPolledPropertyKey "has-safe-sleep"
//#endif


/*! @class AppleKauaiATA : public MacIOATA
    @abstract The specific driver for AppleKauai-ata controllers.
    @discussion class contains all of the code specific to matching and
    running AppleKauai ata controllers.

*/    

class KauaiPolledAdapter;

class AppleKauaiATA : public MacIOATA
{
    OSDeclareDefaultStructors(AppleKauaiATA)

public:

	/*--- Overrides from IOService ---*/
	virtual bool init(OSDictionary* properties);

	// checks for the compatible property of "AppleKauai-ata" 
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
		UInt32			pioMWRegValue;					// PIO and MWDMA timing control hardware register setting
		UInt32			ultraRegValue;					// ultra mode timing control hardware register setting
		UInt8			ataPIOSpeedMode;				// PIO Mode Timing class (bit-significant)
		UInt16			ataPIOCycleTime;				// Cycle time for PIO mode
		UInt8			ataMultiDMASpeed;				// Multiple Word DMA Timing Class (bit-significant)
		UInt16			ataMultiCycleTime;				// Cycle time for Multiword DMA mode
		UInt16			ataUltraDMASpeedMode;			// Ultra Timing class (bit-significant)
	};

	volatile UInt32* _kauaiATAFCR;	// base
	volatile UInt32* _ultraTimingControl;  //base + 0x2210
	volatile UInt32* _autoPollingControl; //base + 0x2220
	volatile UInt32* _interruptPendingReg; //base + 0x2300
	
	IOBufferMemoryDescriptor* dmaBuffer;
	IOMemoryDescriptor* clientBuffer;
	bool bufferRX;
	bool rxFeatureOn;
	bool ultra133;
	
	ATABusTimings busTimings[2];
	bool _needsResync;
	
	IOService *myProvider;
	
	// calculate the correct binary configuration for the desired bus timings.
	virtual IOReturn selectIOTimerValue( IOATADevConfig* configRequest, UInt32 unitNumber);

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
	virtual IOATAController::transState AppleKauaiATA::determineATAPIState(void);
	// override for special case device
	virtual void processDMAInterrupt (void);

	// override for new location of DMA base address
	virtual bool allocDMAChannel(void);

	// preflight dma
	virtual IOReturn handleExecIO( void );
	// postflight dma
	virtual void completeIO( IOReturn commandResult );

	// activate the DMA engine
	virtual void activateDMAEngine(void);
	// setup the CC with IO commands
	virtual IOReturn createChannelCommands(void);	
	
	virtual void handleTimeout( void );

#ifdef __KAUAI_POLLED__	
protected:
	//override for polling
	virtual IOReturn startTimer( UInt32 inMS);
	//disable and clear a running timer.
	virtual void stopTimer( void );

	KauaiPolledAdapter* polledAdapter;
	bool polledMode;

public:
	void pollEntry( void );
	void transitionFixup( void );

#endif

};

#ifdef __KAUAI_POLLED__
class KauaiPolledAdapter : public IOPolledInterface

{
    OSDeclareDefaultStructors(KauaiPolledAdapter)

public:
	virtual IOReturn probe(IOService * target);

    virtual IOReturn open( IOOptionBits state, IOMemoryDescriptor * buffer);
    virtual IOReturn close(IOOptionBits state);

    virtual IOReturn startIO(uint32_t 	        operation,
                             uint32_t           bufferOffset,
                             uint64_t	        deviceOffset,
                             uint64_t	        length,
                             IOPolledCompletion completion) ;

    virtual IOReturn checkForWork(void);
	
	bool isPolling( void );
	
	void setOwner( AppleKauaiATA* owner );

protected:
	AppleKauaiATA* owner;
	bool pollingActive;


};

#endif

#endif	// _DRV_AAPL_KAUAI_ATA_H