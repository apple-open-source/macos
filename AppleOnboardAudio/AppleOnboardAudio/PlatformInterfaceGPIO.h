/*
 *	PlatformInterfaceGPIO.h
 *
 *	Defines base class for GPIO support within the context of PlatformInterface derived classes.
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_GPIO__
#define	__PLATFORMINTERFACE_GPIO__
 
#include	<IOKit/i2c/PPCI2CInterface.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	<IOKit/IOFilterInterruptEventSource.h>
#include	<IOKit/IOWorkLoop.h>
#include	<IOKit/IODeviceTreeSupport.h>
#include	<IOKit/IORegistryEntry.h>
#include	<IOKit/IOCommandGate.h>
#include	<IOKit/ppc/IODBDMA.h>
#include	"PlatformInterfaceSupportCommon.h"

#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	"AudioHardwareUtilities.h"
#include	"AudioHardwareConstants.h"

class AppleOnboardAudio;

class PlatformInterfaceGPIO : public OSObject {

    OSDeclareDefaultStructors ( PlatformInterfaceGPIO );

public:	
	virtual bool						init ( IOService * device, AppleOnboardAudio * provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free ();
	virtual	void						setWorkLoop ( IOWorkLoop* inWorkLoop ) { mWorkLoop = inWorkLoop; }					

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState ) { return kIOReturnSuccess; }
	
	virtual	bool						needsUnregisterInterruptsOnSleep ( void ) { return FALSE; }
	virtual	bool						needsRegisterInterruptsOnWake ( void ) { return FALSE; }
	virtual	bool						needsCheckDetectStatusOnWake ( void ) { return FALSE; }

	//
	// GPIO Methods
	//
	virtual IOReturn					setClockMux ( GpioAttributes muxState )  { return kIOReturnSuccess; }
	virtual GpioAttributes				getClockMux ()  { return kGPIO_Unknown; }

	virtual GpioAttributes				getCodecErrorInterrupt () { return kGPIO_Unknown; }

	virtual GpioAttributes				getCodecInterrupt () { return kGPIO_Unknown; }

	virtual	GpioAttributes				getComboInJackTypeConnected () { return kGPIO_Unknown; }
	virtual	GpioAttributes				getComboOutJackTypeConnected () { return kGPIO_Unknown; }

	virtual	GpioAttributes				getDigitalInConnected ( GPIOSelector association ) { return kGPIO_Unknown; }
	virtual	GpioAttributes				getDigitalOutConnected ( GPIOSelector association ) { return kGPIO_Unknown; }

	virtual GpioAttributes				getHeadphoneConnected () { return kGPIO_Unknown; }

	virtual IOReturn 					setHeadphoneMuteState ( GpioAttributes muteState )  { return kIOReturnSuccess; }
	virtual GpioAttributes 				getHeadphoneMuteState () { return kGPIO_Unknown; }
	
	virtual IOReturn					setInputDataMux (GpioAttributes muxState)  { return kIOReturnSuccess; }
	virtual GpioAttributes				getInputDataMux () { return kGPIO_Unknown; }

	virtual GpioAttributes				getInternalSpeakerID () { return kGPIO_Unknown; }

	virtual	GpioAttributes				getLineInConnected () { return kGPIO_Unknown; }
	virtual	GpioAttributes				getLineOutConnected () { return kGPIO_Unknown; }

	virtual IOReturn 					setLineOutMuteState ( GpioAttributes muteState )  { return kIOReturnSuccess; }
	virtual GpioAttributes 				getLineOutMuteState () { return kGPIO_Unknown; }
	
	virtual GpioAttributes				getSpeakerConnected () { return kGPIO_Unknown; }

	virtual IOReturn 					setSpeakerMuteState ( GpioAttributes muteState )  { return kIOReturnSuccess; }
	virtual GpioAttributes 				getSpeakerMuteState () { return kGPIO_Unknown; }
	
	virtual IOReturn					setCodecReset ( CODEC_RESET target, GpioAttributes reset )  { return kIOReturnSuccess; }
	virtual GpioAttributes				getCodecReset ( CODEC_RESET target ) { return kGPIO_Unknown; }

	virtual void						enableAmplifierMuteRelease ( void ) { return; }		//	[3514762]

	//
	// Set Interrupt Handler Methods
	//
	virtual	IOReturn					disableInterrupt ( IOService * device, PlatformInterruptSource source )  { return kIOReturnError; }
	virtual	IOReturn					enableInterrupt ( IOService * device, PlatformInterruptSource source )  { return kIOReturnError; }
	virtual	IOReturn					registerInterruptHandler ( IOService * device, void * interruptHandler, PlatformInterruptSource source ) { return kIOReturnError; }
	virtual	IOReturn					unregisterInterruptHandler (IOService * device, void * interruptHandler, PlatformInterruptSource source ) { return kIOReturnError; }

	virtual	bool						interruptUsesTimerPolling( PlatformInterruptSource source ) { return FALSE; }
	virtual void						poll ( void ) { return; }
	
protected:

	AppleOnboardAudio *					mProvider;
	IOWorkLoop *						mWorkLoop;
	GPIOSelector						mComboInAssociation;										//	[3453799]
	GPIOSelector						mComboOutAssociation;										//	[3453799]
	bool								mEnableAmplifierMuteRelease;								//	[3514762]
	bool								mInterruptsHaveBeenRegistered;								//	[3585556]	Don't allow multiple registrations or unregistrations of interrupts!

};

#endif	/*	__PLATFORMINTERFACE_GPIO__	*/
