/*
 *	PlatformInterfaceDBDMA_PlatformFunctionK2.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_DBDMA_ROM_FIX_PlatformFunction__
#define	__PLATFORMINTERFACE_DBDMA_ROM_FIX_PlatformFunction__
 
#include <IOKit/IOService.h>
#include <IOKit/IOInterruptEventSource.h>
#include "AudioHardwareConstants.h"
#include <IOKit/ppc/IODBDMA.h>
#include "PlatformInterfaceDBDMA.h"

class PlatformInterfaceDBDMA_PlatformFunctionK2 : public PlatformInterfaceDBDMA {

    OSDeclareDefaultStructors ( PlatformInterfaceDBDMA_PlatformFunctionK2 );

public:	
	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free();

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState );

	//
	// DBDMA Memory Address Acquisition Methods
	//
	virtual	IODBDMAChannelRegisters *	GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );
	virtual	IODBDMAChannelRegisters *	GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );

protected:

	IODBDMAChannelRegisters *			mIOBaseDMA[4];

    IOService *							mSystemIOControllerService;
	IORegistryEntry *					mI2S;
	UInt32								mI2SPHandle;
	UInt32								mI2SOffset;
	UInt32								mMacIOPHandle;
	UInt32								mMacIOOffset;

};

#endif	/*	__PLATFORMINTERFACE_DBDMA_ROM_FIX_PlatformFunction__	*/
