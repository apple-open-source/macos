/*
 *	PlatformInterfaceDBDMA_Mapped.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_DBDMA_Mapped__
#define	__PLATFORMINTERFACE_DBDMA_Mapped__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	"AudioHardwareConstants.h"
#include	<IOKit/ppc/IODBDMA.h>
#include	"PlatformInterfaceDBDMA.h"
#include	"PlatformInterfaceSupportMappedCommon.h"

class PlatformInterfaceDBDMA_Mapped : public PlatformInterfaceDBDMA {

    OSDeclareDefaultStructors ( PlatformInterfaceDBDMA_Mapped );

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

    IOService *							mKeyLargoService;
	IODBDMAChannelRegisters *			mIOBaseDMAOutput;
	const OSSymbol *					mKLI2SPowerSymbolName;
	void *								mSoundConfigSpace;		  		// address of sound config space
	void *								mIOBaseAddress;		   			// base address of our I/O controller
	void *								mIOConfigurationBaseAddress;	// base address for the configuration registers
	void *								mI2SBaseAddress;				//	base address of I2S I/O Module
	IODeviceMemory *					mIOBaseAddressMemory;			// Have to free this in free()
	IODeviceMemory *					mIOI2SBaseAddressMemory;
	I2SCell								mI2SInterfaceNumber;

	static const UInt16 kAPPLE_IO_CONFIGURATION_SIZE;
	static const UInt16 kI2S_IO_CONFIGURATION_SIZE;

	static const UInt32 kI2S0BaseOffset;							/*	mapped by AudioI2SControl	*/
	static const UInt32 kI2S1BaseOffset;							/*	mapped by AudioI2SControl	*/
	
};

#endif	/*	__PLATFORMINTERFACE_DBDMA_Mapped__	*/
