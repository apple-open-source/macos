/*
 *	PlatformInterfaceI2S.h
 *
 *	Defines base class for I2S support within the context of PlatformInterface derived classes.
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_I2S__
#define	__PLATFORMINTERFACE_I2S__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	<IOKit/IODeviceTreeSupport.h>
#include	<IOKit/IORegistryEntry.h>
#include	"AudioHardwareUtilities.h"
#include	"AudioHardwareConstants.h"
#include	"PlatformInterfaceSupportCommon.h"

class AppleOnboardAudio;

class PlatformInterfaceI2S : public OSObject {

    OSDeclareDefaultStructors ( PlatformInterfaceI2S );

public:	
	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free ();
	virtual	void						setWorkLoop ( IOWorkLoop* inWorkLoop ) { mWorkLoop = inWorkLoop; }					

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState ) { return kIOReturnSuccess; }

	//
	// I2S Methods: IOM Control
	//
	virtual UInt32						getDataWordSizes() { return 0; }
	virtual UInt32						getFrameCount () { return 0; }
	virtual UInt32						getI2SIOM_CodecMsgIn () { return 0; }
	virtual UInt32						getI2SIOM_CodecMsgOut () { return 0; }
	virtual UInt32						getI2SIOM_FrameMatch () { return 0; }
	virtual UInt32						getI2SIOM_PeakLevelIn0 () { return 0; }
	virtual UInt32						getI2SIOM_PeakLevelIn1 () { return 0; }
	virtual UInt32						getI2SIOM_PeakLevelSel () { return 0; }
	virtual UInt32						getI2SIOMIntControl () { return 0; }
	virtual UInt32						getPeakLevel ( UInt32 channelTarget ) { return 0; }
	virtual UInt32						getSerialFormatRegister () { return 0; }



	virtual IOReturn					setDataWordSizes ( UInt32 dataWordSizes ) { return kIOReturnError; }
	virtual IOReturn					setFrameCount ( UInt32 value ) { return kIOReturnError; }
	virtual IOReturn					setI2SIOM_CodecMsgOut ( UInt32 value ) { return kIOReturnError; }
	virtual IOReturn					setI2SIOM_CodecMsgIn ( UInt32 value ) { return kIOReturnError; }
	virtual IOReturn					setI2SIOM_FrameMatch ( UInt32 value ) { return kIOReturnError; }
	virtual IOReturn					setI2SIOM_PeakLevelIn0 ( UInt32 value ) { return kIOReturnError; }
	virtual IOReturn					setI2SIOM_PeakLevelIn1 ( UInt32 value ) { return kIOReturnError; }
	virtual IOReturn					setI2SIOM_PeakLevelSel ( UInt32 value ) { return kIOReturnError; }
	virtual IOReturn					setI2SIOMIntControl ( UInt32 intCntrl ) { return kIOReturnError; }
	virtual IOReturn					setPeakLevel ( UInt32 channelTarget, UInt32 levelMeterValue ) { return kIOReturnError; }
	virtual IOReturn					setSerialFormatRegister ( UInt32 serialFormat ) { return kIOReturnError; }

	
	
protected:
	
	AppleOnboardAudio *					mProvider;
	IOWorkLoop *						mWorkLoop;

};

#endif	/*	__PLATFORMINTERFACE_I2S__	*/
