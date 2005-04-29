/*
 *	PlatformInterfaceGPIO_PlatformFunction.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_GPIO_PlatformFunction__
#define	__PLATFORMINTERFACE_GPIO_PlatformFunction__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	"AudioHardwareConstants.h"
#include	"PlatformInterfaceGPIO.h"
#include	<IOKit/i2c/PPCI2CInterface.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	<IOKit/IOFilterInterruptEventSource.h>
#include	<IOKit/IOWorkLoop.h>
#include	<IOKit/IORegistryEntry.h>
#include	<IOKit/IOCommandGate.h>
#include	<IOKit/ppc/IODBDMA.h>
#include	"AppleOnboardAudio.h"
#include	"PlatformInterfaceSupportPlatformFunctionCommon.h"

#define super PlatformInterfaceGPIO

class PlatformInterfaceGPIO_PlatformFunction : public PlatformInterfaceGPIO {

    OSDeclareDefaultStructors ( PlatformInterfaceGPIO_PlatformFunction );

public:	
	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free();

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState );

	virtual	bool						needsUnregisterInterruptsOnSleep ( void ) { return FALSE; }
	virtual	bool						needsRegisterInterruptsOnWake ( void ) { return FALSE; }
	virtual	bool						needsCheckDetectStatusOnWake ( void ) { return TRUE; }

	//
	// GPIO Methods
	//
	virtual IOReturn					setClockMux ( GpioAttributes muxState );
	virtual GpioAttributes				getClockMux ();

	virtual GpioAttributes				getCodecErrorInterrupt ();

	virtual GpioAttributes				getCodecInterrupt ();

	virtual	GpioAttributes				getComboInJackTypeConnected ();
	virtual	GpioAttributes				getComboOutJackTypeConnected ();

	virtual	GpioAttributes				getDigitalInConnected ( GPIOSelector association );
	virtual	GpioAttributes				getDigitalOutConnected ( GPIOSelector association );

	virtual GpioAttributes				getHeadphoneConnected ();

	virtual IOReturn 					setHeadphoneMuteState ( GpioAttributes muteState );
	virtual GpioAttributes 				getHeadphoneMuteState ();
	
	virtual IOReturn					setInputDataMux (GpioAttributes muxState);
	virtual GpioAttributes				getInputDataMux ();

	virtual GpioAttributes				getInternalSpeakerID ();

	virtual	GpioAttributes				getLineInConnected ();
	virtual	GpioAttributes				getLineOutConnected ();

	virtual IOReturn 					setLineOutMuteState ( GpioAttributes muteState );
	virtual GpioAttributes 				getLineOutMuteState ();
	
	virtual GpioAttributes				getSpeakerConnected ();

	virtual IOReturn 					setSpeakerMuteState ( GpioAttributes muteState );
	virtual GpioAttributes 				getSpeakerMuteState ();
	
	virtual IOReturn					setCodecReset ( CODEC_RESET target, GpioAttributes reset );
	virtual GpioAttributes				getCodecReset ( CODEC_RESET target );
	
	virtual void						enableAmplifierMuteRelease ( void );		//	[3514762]

	//
	// Set Interrupt Handler Methods
	//
	virtual	IOReturn					disableInterrupt ( IOService * device, PlatformInterruptSource source );
	virtual	IOReturn					enableInterrupt ( IOService * device, PlatformInterruptSource source );
	virtual	IOReturn					registerInterruptHandler ( IOService * device, void * interruptHandler, PlatformInterruptSource source );
	virtual	IOReturn					unregisterInterruptHandler (IOService * device, void * interruptHandler, PlatformInterruptSource source );

	virtual	bool						interruptUsesTimerPolling( PlatformInterruptSource source );
	virtual void						poll ( void );
	
protected:

	virtual IOReturn					makeSymbolAndCallPlatformFunctionNoWait ( const char * name, void * param1, void * param2, void * param3, void * param4 );
	inline 	const OSSymbol*				makeFunctionSymbolName ( const char * name,UInt32 pHandle );
	GpioAttributes						GetCachedAttribute ( GPIOSelector selector, GpioAttributes defaultResult );
	GpioAttributes						readGpioState ( GPIOSelector selector );
	IOReturn							writeGpioState ( GPIOSelector selector, GpioAttributes gpioState );
	IOReturn							translateGpioAttributeToGpioState ( GPIOType gpioType, GpioAttributes gpioAttribute, UInt32 * valuePtr );

    IOService *							mSystemIOControllerService;
	UInt32								mI2SCell;
	IORegistryEntry *					mI2S;
	UInt32								mI2SPHandle;
	UInt32								mI2SOffset;
	UInt32								mMacIOPHandle;
	UInt32								mMacIOOffset;
	UInt32								mGpioMessageFlag;
	I2SCell								mI2SInterfaceNumber;
	
	static const char *					kAppleGPIO_DisableSpeakerDetect;			
	static const char *					kAppleGPIO_EnableSpeakerDetect;			
	static const char *					kAppleGPIO_GetSpeakerDetect;
	static const char *					kAppleGPIO_RegisterSpeakerDetect;			
	static const char *					kAppleGPIO_UnregisterSpeakerDetect;		

	static const char *					kAppleGPIO_DisableDigitalInDetect;
	static const char *					kAppleGPIO_EnableDigitalInDetect;
	static const char *					kAppleGPIO_GetDigitalInDetect;	
	static const char *					kAppleGPIO_RegisterDigitalInDetect;
	static const char *					kAppleGPIO_UnregisterDigitalInDetect;

	static const char *					kAppleGPIO_GetComboInJackType;	
	static const char *					kAppleGPIO_DisableComboInSense;
	static const char *					kAppleGPIO_EnableComboInSense;
	static const char *					kAppleGPIO_RegisterComboInSense;
	static const char *					kAppleGPIO_UnregisterComboInSense;
	
	static const char *					kAppleGPIO_GetComboOutJackType;	
	static const char *					kAppleGPIO_DisableComboOutSense;
	static const char *					kAppleGPIO_EnableComboOutSense;
	static const char *					kAppleGPIO_RegisterComboOutSense;
	static const char *					kAppleGPIO_UnregisterComboOutSense;

	static const char *					kAppleGPIO_DisableDigitalOutDetect;
	static const char *					kAppleGPIO_EnableDigitalOutDetect;
	static const char *					kAppleGPIO_GetDigitalOutDetect;
	static const char *					kAppleGPIO_RegisterDigitalOutDetect;
	static const char *					kAppleGPIO_UnregisterDigitalOutDetect;

	static const char *					kAppleGPIO_DisableLineInDetect;			
	static const char *					kAppleGPIO_EnableLineInDetect;			
	static const char *					kAppleGPIO_GetLineInDetect;				
	static const char *					kAppleGPIO_RegisterLineInDetect;			
	static const char *					kAppleGPIO_UnregisterLineInDetect;		

	static const char *					kAppleGPIO_DisableLineOutDetect;			
	static const char *					kAppleGPIO_EnableLineOutDetect;			
	static const char *					kAppleGPIO_GetLineOutDetect;				
	static const char *					kAppleGPIO_RegisterLineOutDetect;		
	static const char *					kAppleGPIO_UnregisterLineOutDetect;		

	static const char *					kAppleGPIO_DisableHeadphoneDetect;		
	static const char *					kAppleGPIO_EnableHeadphoneDetect;		
	static const char *					kAppleGPIO_GetHeadphoneDetect;			
	static const char *					kAppleGPIO_RegisterHeadphoneDetect;		
	static const char *					kAppleGPIO_UnregisterHeadphoneDetect;	

	static const char *					kAppleGPIO_SetHeadphoneMute;				
	static const char *					kAppleGPIO_GetHeadphoneMute;				

	static const char *					kAppleGPIO_SetAmpMute;					
	static const char *					kAppleGPIO_GetAmpMute;					

	static const char *					kAppleGPIO_SetAudioHwReset;
	static const char *					kAppleGPIO_GetAudioHwReset;				

	static const char *					kAppleGPIO_SetAudioDigHwReset;			
	static const char *					kAppleGPIO_GetAudioDigHwReset;			

	static const char *					kAppleGPIO_SetLineOutMute;				
	static const char *					kAppleGPIO_GetLineOutMute;				

	static const char *					kAppleGPIO_DisableCodecIRQ;				
	static const char *					kAppleGPIO_EnableCodecIRQ;				
	static const char *					kAppleGPIO_GetCodecIRQ;					
	static const char *					kAppleGPIO_RegisterCodecIRQ;				
	static const char *					kAppleGPIO_UnregisterCodecIRQ;			

	static const char *					kAppleGPIO_EnableCodecErrorIRQ;			
	static const char *					kAppleGPIO_DisableCodecErrorIRQ;			
	static const char *					kAppleGPIO_GetCodecErrorIRQ;				
	static const char *					kAppleGPIO_RegisterCodecErrorIRQ;			
	static const char *					kAppleGPIO_UnregisterCodecErrorIRQ;			

	static const char *					kAppleGPIO_SetCodecClockMux;				
	static const char *					kAppleGPIO_GetCodecClockMux;				

	static const char *					kAppleGPIO_SetCodecInputDataMux;
	static const char *					kAppleGPIO_GetCodecInputDataMux;
	
	static const char *					kAppleGPIO_GetInternalSpeakerID;

	GpioAttributes						mAppleGPIO_AmpMute;
	GpioAttributes						mAppleGPIO_AnalogCodecReset;
	GpioAttributes						mAppleGPIO_CodecClockMux;
	GpioAttributes						mAppleGPIO_CodecInputDataMux;
	GpioAttributes						mAppleGPIO_DigitalCodecReset;
	GpioAttributes						mAppleGPIO_HeadphoneMute;
	GpioAttributes						mAppleGPIO_LineOutMute;
	GpioAttributes						mAppleGPIO_InternalSpeakerID;
	GpioAttributes						mAppleGPIO_SpeakerID;
	
	void *								mCodecInterruptHandler;
	void *								mCodecErrorInterruptHandler;
	void *								mComboInDetectInterruptHandler;
	void *								mComboOutDetectInterruptHandler;
	void *								mDigitalInDetectInterruptHandler;
	void *								mDigitalOutDetectInterruptHandler;
	void *								mHeadphoneDetectInterruptHandler;
	void *								mLineInputDetectInterruptHandler;
	void *								mLineOutputDetectInterruptHandler;
	void *								mSpeakerDetectInterruptHandler;

	bool								mCodecInterruptEnable;
	bool								mCodecErrorInterruptEnable;
	bool								mComboInDetectInterruptEnable;
	bool								mComboOutDetectInterruptEnable;
	bool								mDigitalInDetectInterruptEnable;
	bool								mDigitalOutDetectInterruptEnable;
	bool								mHeadphoneDetectInterruptEnable;
	bool								mLineInputDetectInterruptEnable;
	bool								mLineOutputDetectInterruptEnable;
	bool								mSpeakerDetectInterruptEnable;

};

#endif	/*	__PLATFORMINTERFACE_GPIO_PlatformFunction__	*/
