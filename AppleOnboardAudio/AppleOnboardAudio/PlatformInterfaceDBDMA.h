/*
 *	PlatformInterfaceDBDMA.h
 *
 *	Defines base class for DBDMA support within the context of PlatformInterface derived classes.
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
 
#ifndef __PLATFORMINTERFACE_DBDMA__
#define	__PLATFORMINTERFACE_DBDMA__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	"AudioHardwareConstants.h"
#include	<IOKit/ppc/IODBDMA.h>
#include	<IOKit/IODeviceTreeSupport.h>
#include	<IOKit/IORegistryEntry.h>
#include	"AudioHardwareUtilities.h"
#include	"PlatformInterfaceSupportCommon.h"
//#include	"AppleDBDMAAudio.h"

#define	kIODMAInputOffset				0x00000100					/*	offset from I2S 0 Tx DMA channel registers to Rx DMA channel registers	*/
#define kIODMASizeOfChannelBuffer		256
	
class AppleOnboardAudio;
class AppleDBDMAAudio;

class PlatformInterfaceDBDMA : public OSObject {

    OSDeclareDefaultStructors ( PlatformInterfaceDBDMA );

public:	
	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free ();
	virtual	void						setWorkLoop ( IOWorkLoop* inWorkLoop ) { mWorkLoop = inWorkLoop; }					

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState ) { return kIOReturnSuccess; }

	//
	// DBDMA Memory Address Acquisition Methods
	//
	virtual	IODBDMAChannelRegisters *	GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) { return 0; }
	virtual	IODBDMAChannelRegisters *	GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) { return 0; }

protected:

	AppleOnboardAudio *					mProvider;
	IOWorkLoop *						mWorkLoop;

};

#endif	/*	__PLATFORMINTERFACE_DBDMA__	*/



