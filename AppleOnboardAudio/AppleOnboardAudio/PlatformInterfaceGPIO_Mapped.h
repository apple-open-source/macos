/*
 *	PlatformInterfaceGPIO_Mapped.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_GPIO_Mapped__
#define	__PLATFORMINTERFACE_GPIO_Mapped__
 
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
#include	"PlatformInterfaceSupportMappedCommon.h"

#define super PlatformInterfaceGPIO

class PlatformInterfaceGPIO_Mapped : public PlatformInterfaceGPIO {

    OSDeclareDefaultStructors ( PlatformInterfaceGPIO_Mapped );

	typedef bool GpioActiveState;
	typedef UInt8* GpioPtr;

public:	

	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free ();

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState );

	virtual	bool						needsUnregisterInterruptsOnSleep ( void ) { return TRUE; }
	virtual	bool						needsRegisterInterruptsOnWake ( void ) { return TRUE; }

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

	virtual	bool						interruptUsesTimerPolling ( PlatformInterruptSource source );
	virtual void						poll ( void );
	
protected:

	virtual	void						setWorkLoop ( IOWorkLoop* inWorkLoop ) { mWorkLoop = inWorkLoop; return; }					

	virtual IOReturn					setHeadphoneDetectInterruptHandler(IOService* device, void* interruptHandler);
	virtual	IOReturn 					setSpeakerDetectInterruptHandler (IOService* device, void* interruptHandler);
	virtual IOReturn					setLineOutDetectInterruptHandler(IOService* device, void* interruptHandler);
	virtual IOReturn					setLineInDetectInterruptHandler(IOService* device, void* interruptHandler);
	virtual IOReturn					setDigitalOutDetectInterruptHandler(IOService* device, void* interruptHandler);
	virtual IOReturn					setDigitalInDetectInterruptHandler(IOService* device, void* interruptHandler);
	virtual IOReturn					setCodecInterruptHandler(IOService* device, void* interruptHandler);
	virtual IOReturn					setCodecErrorInterruptHandler(IOService* device, void* interruptHandler);

	IORegistryEntry*					FindEntryByProperty ( const IORegistryEntry * start, const char * key, const char * value );
	virtual void						initAudioGpioPtr ( const IORegistryEntry * start, const char * gpioName, GpioPtr* gpioH, GpioActiveState* gpioActiveStatePtr, IOService ** intProvider );
	virtual IOReturn					getGpioPtrAndActiveState ( GPIOSelector theGpio, GpioPtr * gpioPtrPtr, GpioActiveState * activeStatePtr ) ;
	virtual GpioAttributes				getGpioAttributes ( GPIOSelector theGpio );
	virtual IOReturn					setGpioAttributes ( GPIOSelector theGpio, GpioAttributes attributes );
	virtual IOReturn					gpioWrite( UInt8* gpioAddress, UInt8 data );

	inline UInt8						assertGPIO(GpioActiveState inState) { return ( ( 0 == inState ) ? 0 : 1 ); }
	inline UInt8						negateGPIO(GpioActiveState inState) { return ( ( 0 == inState ) ? 1 : 0 ); }

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

	IOWorkLoop*							mWorkLoop;

	bool								mEnableAmplifierMuteRelease;

	IOService *							mCodecIntProvider;								
	IOService *							mCodecErrorIntProvider;								
	IOService *							mComboInDetectIntProvider;								
	IOService *							mComboOutDetectIntProvider;								
	IOService *							mDigitalInDetectIntProvider;								
	IOService *							mDigitalOutDetectIntProvider;								
	IOService *							mHeadphoneDetectIntProvider;
	IOService *							mLineInDetectIntProvider;								
	IOService *							mLineOutDetectIntProvider;								
	IOService *							mSpeakerDetectIntProvider;


	IOInterruptEventSource *			mHeadphoneDetectIntEventSource;
	IOInterruptEventSource *			mSpeakerDetectIntEventSource;
	IOInterruptEventSource *			mLineOutDetectIntEventSource;							
	IOInterruptEventSource *			mLineInDetectIntEventSource;							
	IOInterruptEventSource *			mDigitalOutDetectIntEventSource;								
	IOInterruptEventSource *			mDigitalInDetectIntEventSource;								
	IOInterruptEventSource *			mCodecInterruptEventSource;								
	IOInterruptEventSource *			mCodecErrorInterruptEventSource;								

	GpioPtr								mAmplifierMuteGpio;
	GpioPtr								mAnalogResetGpio;
	GpioPtr								mClockMuxGpio;
	GpioPtr								mComboInJackTypeGpio;							
	GpioPtr								mComboOutJackTypeGpio;							
	GpioPtr								mCodecErrorInterruptGpio;
	GpioPtr								mCodecInterruptGpio;
	GpioPtr								mDigitalInDetectGpio;							
	GpioPtr								mDigitalOutDetectGpio;							
	GpioPtr								mDigitalResetGpio;
	GpioPtr								mHeadphoneDetectGpio;
	GpioPtr								mHeadphoneMuteGpio;
	GpioPtr								mInputDataMuxGpio;
	GpioPtr								mInternalSpeakerIDGpio;
	GpioPtr								mLineInDetectGpio;								
	GpioPtr								mLineOutDetectGpio;	
	GpioPtr								mLineOutMuteGpio;								
	GpioPtr								mSpeakerDetectGpio;

	GpioActiveState						mAmplifierMuteActiveState;									
	GpioActiveState						mAnalogResetActiveState;
	GpioActiveState						mClockMuxActiveState;
	GpioActiveState						mCodecErrorInterruptActiveState;					
	GpioActiveState						mCodecInterruptActiveState;					
	GpioActiveState						mDigitalInDetectActiveState;					
	GpioActiveState						mComboInJackTypeActiveState;					
	GpioActiveState						mDigitalOutDetectActiveState;					
	GpioActiveState						mComboOutJackTypeActiveState;					
	GpioActiveState						mDigitalResetActiveState;
	GpioActiveState						mHeadphoneDetectActiveState;
	GpioActiveState						mHeadphoneMuteActiveState;	
	GpioActiveState						mInputDataMuxActiveState;							
	GpioActiveState						mInternalSpeakerIDActiveState;					
	GpioActiveState						mLineInDetectActiveState;						
	GpioActiveState						mLineOutDetectActiveState;						
	GpioActiveState						mLineOutMuteActiveState;							
	GpioActiveState						mSpeakerDetectActiveState;									

	static const char*  kAudioGPIO;
	static const char*  kAudioGPIOActiveState;
	
	static const UInt16 kAPPLE_IO_CONFIGURATION_SIZE;
	static const UInt16 kI2S_IO_CONFIGURATION_SIZE;

	static const UInt32 kI2S0BaseOffset;							/*	mapped by AudioI2SControl	*/
	static const UInt32 kI2S1BaseOffset;							/*	mapped by AudioI2SControl	*/

	static const UInt32 kI2SIntCtlOffset;
	static const UInt32 kI2SSerialFormatOffset;
	static const UInt32 kI2SCodecMsgOutOffset;
	static const UInt32 kI2SCodecMsgInOffset;
	static const UInt32 kI2SFrameCountOffset;
	static const UInt32 kI2SFrameMatchOffset;
	static const UInt32 kI2SDataWordSizesOffset;
	static const UInt32 kI2SPeakLevelSelOffset;
	static const UInt32 kI2SPeakLevelIn0Offset;
	static const UInt32 kI2SPeakLevelIn1Offset;

	static const UInt32 kI2SClockOffset;
	static const UInt32 kI2S0ClockEnable;
	static const UInt32 kI2S1ClockEnable;
	static const UInt32 kI2S0CellEnable;
	static const UInt32 kI2S1CellEnable;
	static const UInt32 kI2S0InterfaceEnable;
	static const UInt32 kI2S1InterfaceEnable;
	static const UInt32 kI2S0SwReset;
	static const UInt32 kI2S1SwReset;

	static const UInt32 kFCR0Offset;
	static const UInt32 kFCR1Offset;
	static const UInt32 kFCR2Offset;
	static const UInt32 kFCR3Offset;
	static const UInt32 kFCR4Offset;
	
	static const char*	kAmpMuteEntry;
	static const char*  kAnalogHWResetEntry;
	static const char*  kClockMuxEntry;
	static const char*	kCodecErrorIrqTypeEntry;
	static const char*	kCodecIrqTypeEntry;
	static const char*  kComboInJackTypeEntry;
	static const char*  kComboOutJackTypeEntry;
	static const char*  kDigitalHWResetEntry;
	static const char*  kDigitalInDetectEntry;
	static const char*  kDigitalOutDetectEntry;
	static const char*  kHeadphoneDetectInt;
	static const char* 	kHeadphoneMuteEntry;
	static const char*	kInternalSpeakerIDEntry;
	static const char*  kLineInDetectInt;
	static const char*  kLineOutDetectInt;
	static const char*  kLineOutMuteEntry;
	static const char*  kSpeakerDetectEntry;

	enum extInt_gpio {
			intEdgeSEL				=	7,		//	bit address:	R/W Enable Dual Edge
			positiveEdge			=	0,		//		0 = positive edge detect for ExtInt interrupt sources (default)
			dualEdge				=	1		//		1 = enable both edges
	};

	enum gpio {
			gpioOS					=	4,		//	bit address:	output select
			gpioBit0isOutput		=	0,		//		use gpio bit 0 as output (default)
			gpioMediaBayIsOutput	=	1,		//		use media bay power
			gpioReservedOutputSel	=	2,		//		reserved
			gpioMPICopenCollector	=	3,		//		MPIC CPUInt2_1 (open collector)
			
			gpioAltOE				=	3,		//	bit address:	alternate output enable
			gpioOE_DDR				=	0,		//		use DDR for output enable
			gpioOE_Use_OS			=	1,		//		use gpioOS for output enable
			
			gpioDDR					=	2,		//	bit address:	r/w data direction
			gpioDDR_INPUT			=	0,		//		use for input (default)
			gpioDDR_OUTPUT			=	1,		//		use for output
			
			gpioPIN_RO				=	1,		//	bit address:	read only level on pin
			
			gpioDATA				=	0,		//	bit address:	the gpio itself

			gpioBIT_MASK			=	1		//	value shifted by bit position to be used to determine a GPIO bit state
	};

};

#endif	/*	__PLATFORMINTERFACE_GPIO_Mapped__	*/
