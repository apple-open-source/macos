/*
 *	PlatformInterfaceFCR.h
 *
 *	Defines base class for FCR support within the context of PlatformInterface derived classes.
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_FCR__
#define	__PLATFORMINTERFACE_FCR__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	<IOKit/IODeviceTreeSupport.h>
#include	<IOKit/IORegistryEntry.h>
#include	"AudioHardwareUtilities.h"
#include	"AudioHardwareConstants.h"
#include	"PlatformInterfaceSupportCommon.h"

class AppleOnboardAudio;

class PlatformInterfaceFCR : public OSObject {

    OSDeclareDefaultStructors ( PlatformInterfaceFCR );

public:	
	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free ();
	virtual	void						setWorkLoop ( IOWorkLoop* inWorkLoop ) { mWorkLoop = inWorkLoop; }					

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState ) { return kIOReturnSuccess; }

	//
	// I2S Methods: FCR1
	//
	virtual bool						getI2SCellEnable() { return FALSE; }
	virtual bool						getI2SClockEnable() { return FALSE; }
	virtual bool						getI2SEnable () { return FALSE; }
	virtual bool						getI2SSWReset () { return FALSE; }

	virtual IOReturn					setI2SCellEnable ( bool enable ) { return kIOReturnError; }
	virtual IOReturn					setI2SClockEnable ( bool enable ) { return kIOReturnError; }
	virtual IOReturn					setI2SEnable ( bool enable ) { return kIOReturnError; }
	virtual IOReturn					setI2SSWReset ( bool enable ) { return kIOReturnError; }

	//
	// I2S Methods: FCR3
	//
	virtual IOReturn					releaseI2SClockSource ( I2SClockFrequency inFrequency ) { return kIOReturnError; }
	virtual IOReturn					requestI2SClockSource ( I2SClockFrequency inFrequency ) { return kIOReturnError; }
	
protected:

	AppleOnboardAudio *					mProvider;
	IOWorkLoop *						mWorkLoop;

};

#endif	/*	__PLATFORMINTERFACE_FCR__	*/
