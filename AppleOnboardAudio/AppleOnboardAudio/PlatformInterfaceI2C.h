/*
 *	PlatformInterfaceI2C.h
 *
 *	Defines base class for I2C support within the context of PlatformInterface derived classes.
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_I2C__
#define	__PLATFORMINTERFACE_I2C__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IODeviceTreeSupport.h>
#include	<IOKit/IORegistryEntry.h>
#include	"AudioHardwareUtilities.h"
#include	"AudioHardwareConstants.h"
#include	"PlatformInterfaceSupportCommon.h"
#include	"AppleOnboardAudioUserClient.h"

class AppleOnboardAudio;

class PlatformInterfaceI2C : public OSObject {

    OSDeclareDefaultStructors ( PlatformInterfaceI2C );

public:	
	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free ();
	virtual	void						setWorkLoop ( IOWorkLoop* inWorkLoop ) { mWorkLoop = inWorkLoop; }					

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState ) { return kIOReturnSuccess; }

	//
	// Codec Methods
	//
	virtual IOReturn					readCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength ) { return kIOReturnError; } 
	virtual IOReturn					WriteCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength ) { return kIOReturnError; } 
	virtual IOReturn					setMAP ( UInt32 codecRef, UInt8 subAddress ) { return kIOReturnError; }
	
	virtual	UInt32						getSavedMAP ( UInt32 codecRef );
	virtual void						saveMAP ( UInt32 codecRef, UInt32 map );
	
protected:

	AppleOnboardAudio *					mProvider;
	IOWorkLoop *						mWorkLoop;
	UInt32								mMAPArray[kCodec_NumberOfTypes];	//	memory address pointer

};

#endif	/*	__PLATFORMINTERFACE_I2C__	*/
