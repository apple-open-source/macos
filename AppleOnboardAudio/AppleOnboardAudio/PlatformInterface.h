/*
 *	PlatformInterface.h
 *
 *	Defines an interface for IO controllers (eg. Key Largo)
 *
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 */
#ifndef __PLATFORMINTERFACE__
#define	__PLATFORMINTERFACE__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	<IOKit/IOTimerEventSource.h>
#include	<IOKit/IOWorkLoop.h>
#include	<IOKit/IOCommandGate.h>

#include	"AudioHardwareConstants.h"
#include	<IOKit/ppc/IODBDMA.h>

#include	"PlatformInterfaceSupportCommon.h"

#include	"PlatformInterfaceDBDMA.h"
#include	"PlatformInterfaceFCR.h"
#include	"PlatformInterfaceGPIO.h"
#include	"PlatformInterfaceI2C.h"
#include	"PlatformInterfaceI2S.h"

#define	kComboJackDelay		( NSEC_PER_SEC / 4 )

//	If this enumeration changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef enum PlatformInterfaceObjectType {
	kPlatformInterfaceType_Unknown			=	0,
	kPlatformInterfaceType_KeyLargo,
	kPlatformInterfaceType_K2,
	kPlatformInterfaceType_Shasta
} ;

//	If this enumeration changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef enum {
	gpioMessage_AnalogCodecReset_bitAddress		=   0,
	gpioMessage_ClockMux_bitAddress,
	gpioMessage_CodecInterrupt_bitAddress,
	gpioMessage_CodecErrorInterrupt_bitAddress,
	gpioMessage_ComboInJackType_bitAddress,
	gpioMessage_ComboOutJackType_bitAddress,
	gpioMessage_DigitalCodecReset_bitAddress,
	gpioMessage_DigitalInDetect_bitAddress,
	gpioMessage_DigitalOutDetect_bitAddress,
	gpioMessage_HeadphoneDetect_bitAddress,
	gpioMessage_HeadphoneMute_bitAddress,
	gpioMessage_InputDataMux_bitAddress,
	gpioMessage_LineInDetect_bitAddress,
	gpioMessage_LineOutDetect_bitAddress,
	gpioMessage_LineOutMute_bitAddress,
	gpioMessage_SpeakerDetect_bitAddress,
	gpioMessage_SpeakerMute_bitAddress,
	gpioMessage_InternalSpeakerID_bitAddress,
	gpioMessage_ComboInAssociation_bitAddress,
	gpioMessage_ComboOutAssociation_bitAddress,
} gpioMessages_bitAdddresses_bitAddresses;

//	If this structure changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef struct { 
	UInt32					intCtrl;
	UInt32					serialFmt;
	UInt32					codecMsgOut;
	UInt32					codecMsgIn;
	UInt32					frameCount;
	UInt32					frameCountToMatch;
	UInt32					dataWordSizes;
	UInt32					peakLevelSfSel;
	UInt32					peakLevelIn0;
	UInt32					peakLevelIn1;
	UInt32					newPeakLevelIn0;
	UInt32					newPeakLevelIn1;
	UInt32					reserved_13;
	UInt32					reserved_14;
	UInt32					reserved_15;
	UInt32					reserved_16;
	UInt32					reserved_17;
	UInt32					reserved_18;
	UInt32					reserved_19;
	UInt32					reserved_20;
	UInt32					reserved_21;
	UInt32					reserved_22;
	UInt32					reserved_23;
	UInt32					reserved_24;
	UInt32					reserved_25;
	UInt32					reserved_26;
	UInt32					reserved_27;
	UInt32					reserved_28;
	UInt32					reserved_29;
	UInt32					reserved_30;
	UInt32					reserved_31;
} i2sDescriptor ;
typedef i2sDescriptor * i2sDescriptorPtr;

//	If this structure changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef struct {
	UInt32					i2sEnable;
	UInt32					i2sClockEnable;
	UInt32					i2sReset;
	UInt32					i2sCellEnable;
	UInt32					clock18mHzEnable;
	UInt32					clock45mHzEnable;
	UInt32					clock49mHzEnable;
	UInt32					pll45mHzShutdown;
	UInt32					pll49mHzShutdown;
	UInt32					reserved_10;
	UInt32					reserved_11;
	UInt32					reserved_12;
	UInt32					reserved_13;
	UInt32					reserved_14;
	UInt32					reserved_15;
	UInt32					reserved_16;
	UInt32					reserved_17;
	UInt32					reserved_18;
	UInt32					reserved_19;
	UInt32					reserved_20;
	UInt32					reserved_21;
	UInt32					reserved_22;
	UInt32					reserved_23;
	UInt32					reserved_24;
	UInt32					reserved_25;
	UInt32					reserved_26;
	UInt32					reserved_27;
	UInt32					reserved_28;
	UInt32					reserved_29;
	UInt32					reserved_30;
	UInt32					reserved_31;
} fcrDescriptor ;
typedef fcrDescriptor * fcrDescriptorPtr ;

//	If this structure changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef struct {
	GpioAttributes			gpio_AnalogCodecReset;
	GpioAttributes			gpio_ClockMux;
	GpioAttributes			gpio_CodecInterrupt;
	GpioAttributes			gpio_CodecErrorInterrupt;
	GpioAttributes			gpio_ComboInJackType;
	GpioAttributes			gpio_ComboOutJackType;
	GpioAttributes			gpio_DigitalCodecReset;
	GpioAttributes			gpio_DigitalInDetect;
	GpioAttributes			gpio_DigitalOutDetect;
	GpioAttributes			gpio_HeadphoneDetect;
	GpioAttributes			gpio_HeadphoneMute;
	GpioAttributes			gpio_InputDataMux;
	GpioAttributes			gpio_LineInDetect;
	GpioAttributes			gpio_LineOutDetect;
	GpioAttributes			gpio_LineOutMute;
	GpioAttributes			gpio_SpeakerDetect;
	GpioAttributes			gpio_SpeakerMute;
	GpioAttributes			gpio_InternalSpeakerID;
	GPIOSelector			gpio_ComboInAssociation;
	GPIOSelector			gpio_ComboOutAssociation;
	GpioAttributes			reserved_20;
	GpioAttributes			reserved_21;
	GpioAttributes			reserved_22;
	GpioAttributes			reserved_23;
	GpioAttributes			reserved_24;
	GpioAttributes			reserved_25;
	GpioAttributes			reserved_26;
	GpioAttributes			reserved_27;
	GpioAttributes			reserved_28;
	GpioAttributes			reserved_29;
	GpioAttributes			reserved_30;
	UInt32					gpioMessageFlags;				//  bit mapped array indicating interrupt control success (TRUE) or failure (FALSE)
} gpioDescriptor;
typedef gpioDescriptor * gpioDescriptorPtr;

typedef struct {
	UInt32					i2c_pollingMode;
	UInt32					i2c_errorStatus;
	UInt32					reserved_02;
	UInt32					reserved_03;
	UInt32					reserved_04;
	UInt32					reserved_05;
	UInt32					reserved_06;
	UInt32					reserved_07;
	UInt32					reserved_08;
	UInt32					reserved_09;
	UInt32					reserved_0A;
	UInt32					reserved_0B;
	UInt32					reserved_0C;
	UInt32					reserved_0D;
	UInt32					reserved_1E;
	UInt32					reserved_1F;
} i2cDescriptor;
typedef i2cDescriptor * i2cDescriptorPtr;

//	[3517297]

typedef enum ComboStateMachineState {
	kComboStateMachine_handle_jack_insert				=	0,
	kComboStateMachine_handle_metal_change,
	kComboStateMachine_handle_plastic_change
};

//	If this structure changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef struct {
	PlatformInterfaceObjectType				platformType;
	fcrDescriptor							fcr;
	i2sDescriptor							i2s;
	gpioDescriptor							gpio;
	i2cDescriptor							i2c;
} PlatformStateStruct ;
typedef PlatformStateStruct * PlatformStateStructPtr;

#define kAnalogCodecResetSel kCodecResetSel

void		comboDelayTimerCallback ( OSObject *owner, IOTimerEventSource *device );						//	[3787193]
IOReturn	runComboDelayTasks ( OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4 );	//	[3787193]

class AppleOnboardAudio;
class AppleOnboardAudioUserClient;

class PlatformInterface : public OSObject {

    OSDeclareDefaultStructors ( PlatformInterface );

public:	

	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex, UInt32 supportSelectors );
	virtual void						free ( void );
	
	//
	// Power Management Support
	//
	
	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState );
	
	//
	// Interrupt Support
	//
	
	virtual void						checkDetectStatus ( IOService * device );
	virtual	IOReturn					registerInterruptHandler ( IOService * theDevice, void * interruptHandler, PlatformInterruptSource source );
	virtual	void						setWorkLoop ( IOWorkLoop* inWorkLoop );					
	
	virtual bool						registerDetectInterrupts ( IOService * device );
	virtual void						threadedMemberRegisterDetectInterrupts (IOService * device );
	static void							threadedRegisterDetectInterrupts ( PlatformInterface *self, IOService * device );
	
	virtual bool						registerNonDetectInterrupts ( IOService * device );
	virtual void						threadedMemberRegisterNonDetectInterrupts (IOService * device );
	static void							threadedRegisterNonDetectInterrupts ( PlatformInterface *self, IOService * device );
	
	virtual	IOReturn					unregisterInterruptHandler ( IOService * theDevice, void * interruptHandler, PlatformInterruptSource source );
	
	virtual void						unregisterDetectInterrupts ( void );
	virtual void						unregisterNonDetectInterrupts ( void );
	
	//
	// Interrupt Handler Methods
	//
	
	static void							codecErrorInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 );
	static void							codecInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 );
	static void							comboInDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 );
	static void							comboOutDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 );
	static void							digitalInDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 );
	static void							digitalOutDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 );
	static void							headphoneDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 );
	static void							lineInDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 );
	static void							lineOutDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 );
	virtual void						poll ( void );
	static void							speakerDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 );

	
	//
	// Codec Methods
	//
	
	virtual IOReturn					readCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength ); 
	virtual IOReturn					writeCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength ); 
	virtual UInt32						getSavedMAP ( UInt32 codecRef );
	virtual IOReturn					setMAP ( UInt32 codecRef, UInt8 subAddress );

	//
	// FCR Methods
	//
	
	virtual bool						getI2SCellEnable ();
	virtual bool						getI2SClockEnable ();
	virtual bool						getI2SEnable ();
	virtual bool						getI2SSWReset ();
	
	virtual IOReturn					releaseI2SClockSource ( I2SClockFrequency inFrequency );
	virtual IOReturn					requestI2SClockSource ( I2SClockFrequency inFrequency );
	
	virtual IOReturn					setI2SCellEnable ( bool enable );
	virtual IOReturn					setI2SClockEnable ( bool enable );
	virtual IOReturn					setI2SEnable ( bool enable );
	virtual IOReturn					setI2SSWReset ( bool enable );

	//
	// I2S Methods: IOM Control
	//
	
	virtual UInt32						getDataWordSizes ();
	virtual UInt32						getFrameCount ();
	virtual UInt32						getI2SIOM_CodecMsgIn ();
	virtual UInt32						getI2SIOM_CodecMsgOut ();
	virtual UInt32						getI2SIOM_FrameMatch ();
	virtual UInt32						getI2SIOM_PeakLevelIn0 ();
	virtual UInt32						getI2SIOM_PeakLevelIn1 ();
	virtual UInt32						getI2SIOM_PeakLevelSel ();
	virtual UInt32						getI2SIOMIntControl ();
	virtual UInt32						getPeakLevel ( UInt32 channelTarget );
	virtual UInt32						getSerialFormatRegister ();

	virtual IOReturn					setDataWordSizes ( UInt32 dataWordSizes );
	virtual IOReturn					setFrameCount ( UInt32 value );
	virtual IOReturn					setI2SIOM_CodecMsgIn ( UInt32 value );
	virtual IOReturn					setI2SIOM_CodecMsgOut ( UInt32 value );
	virtual IOReturn					setI2SIOM_FrameMatch ( UInt32 value );
	virtual IOReturn					setI2SIOM_PeakLevelIn0 ( UInt32 value );
	virtual IOReturn					setI2SIOM_PeakLevelIn1 ( UInt32 value );
	virtual IOReturn					setI2SIOM_PeakLevelSel ( UInt32 value );
	virtual IOReturn					setI2SIOMIntControl ( UInt32 intCntrl );
	virtual IOReturn					setPeakLevel ( UInt32 channelTarget, UInt32 levelMeterValue );
	virtual IOReturn					setSerialFormatRegister ( UInt32 serialFormat );

	//
	// GPIO Methods
	//

	virtual	IOReturn					disableInterrupt ( IOService * device, PlatformInterruptSource source );
	virtual void						enableAmplifierMuteRelease ( void );		//	[3514762]
	virtual	IOReturn					enableInterrupt ( IOService * device, PlatformInterruptSource source );
	virtual GpioAttributes				getClockMux ( );
	virtual GpioAttributes				getCodecErrorInterrupt ();
	virtual GpioAttributes				getCodecInterrupt ();
	virtual GpioAttributes				getCodecReset ( CODEC_RESET target );
	GpioAttributes						getComboIn ( void );												//	[3453799]
	GPIOSelector						getComboInAssociation ( void );										//	[3453799]
	virtual	GpioAttributes				getComboInJackTypeConnected ();										//	for combo digital/analog connector
	GpioAttributes						getComboOut ( void );												//	[3453799]
	GPIOSelector						getComboOutAssociation ( void );									//	[3453799]
	virtual	GpioAttributes				getComboOutJackTypeConnected ();		//	for combo digital/analog connector
	virtual	GpioAttributes				getDigitalInConnected ();
	virtual	GpioAttributes				getDigitalOutConnected ();
	virtual GpioAttributes				getHeadphoneConnected ();
	virtual GpioAttributes 				getHeadphoneMuteState ();
	virtual GpioAttributes				getInputDataMux ();
	virtual GpioAttributes				getInternalSpeakerID ();
	virtual	GpioAttributes				getLineInConnected ();
	virtual	GpioAttributes				getLineOutConnected ();
	virtual GpioAttributes 				getLineOutMuteState ();
	virtual GpioAttributes				getSpeakerConnected ();
	virtual GpioAttributes 				getSpeakerMuteState ();
	void								setAssociateComboInTo ( GPIOSelector theDetectInterruptGpio );		//	[3453799]
	void								setAssociateComboOutTo ( GPIOSelector theDetectInterruptGpio );		//	[3453799]
	virtual IOReturn					setClockMux ( GpioAttributes muxState );
	virtual IOReturn					setCodecReset ( CODEC_RESET target, GpioAttributes reset );
	void								setComboIn ( GpioAttributes jackState );							//	[3453799]
	void								setComboOut ( GpioAttributes jackState );							//	[3453799]
	virtual IOReturn 					setHeadphoneMuteState ( GpioAttributes muteState );
	virtual IOReturn					setInputDataMux ( GpioAttributes muxState );
	virtual IOReturn 					setLineOutMuteState ( GpioAttributes muteState );
	virtual IOReturn 					setSpeakerMuteState ( GpioAttributes muteState );

	//
	// DBDMA Memory Address Acquisition Methods
	//
	
	virtual	IODBDMAChannelRegisters *	GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );
	virtual	IODBDMAChannelRegisters *	GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );

	//	
	//	User Client Support
	//
	
	virtual IOReturn					getPlatformState ( PlatformStateStructPtr outState );
	virtual IOReturn					setPlatformState ( PlatformStateStructPtr inState );
	
	virtual void						platformRunComboDelayTasks ( void );								//	[3787193]

	UInt32								mComboInInterruptsProduced;											//	[3787193]
	UInt32								mComboInInterruptsConsumed;											//	[3787193]
	UInt32								mComboOutInterruptsProduced;										//	[3787193]
	UInt32								mComboOutInterruptsConsumed;										//	[3787193]

	virtual	void						triggerComboOneShot ();												//	[3787193]

protected:

	UInt32								mGpioMessageFlag;
	static UInt32						sInstanceCount;
	UInt32								mInstanceIndex;
	Boolean								mDetectInterruptsHaveBeenRegistered;								//	[3585556]	Don't allow multiple registrations or unregistrations of interrupts!
	Boolean								mNonDetectInterruptsHaveBeenRegistered;								//	[3585556]	Don't allow multiple registrations or unregistrations of interrupts!
	
	AppleOnboardAudio *					mProvider;

    thread_call_t						mRegisterDetectInterruptsThread;									//  [3517442] mpc
    thread_call_t						mRegisterNonDetectInterruptsThread;									//  [3517442] mpc

	GPIOSelector						mComboInAssociation;												//	[3453799]
	GPIOSelector						mComboOutAssociation;												//	[3453799]
	GpioAttributes						mComboInJackState;													//	[3453799]
	GpioAttributes						mComboOutJackState;													//	[3453799]

	IOTimerEventSource *				comboDelayTimer;													//	[3787193]

	PlatformInterfaceDBDMA *			platformInterfaceDBDMA;
	PlatformInterfaceFCR *				platformInterfaceFCR;
	PlatformInterfaceGPIO *				platformInterfaceGPIO;
	PlatformInterfaceI2C *				platformInterfaceI2C;
	PlatformInterfaceI2S *				platformInterfaceI2S;
	
	IOWorkLoop *						mWorkLoop;

	UInt32								mSupportSelectors;

	enum {
		kPlatformSupportDBDMA_bitAddress		= 0,
		kPlatformSupportFCR_bitAddress			= 4,
		kPlatformSupportGPIO_bitAddress			= 8,
		kPlatformSupportI2C_bitAddress			= 12,
		kPlatformSupportI2S_bitAddress			= 16,
		KPlatformSupport_bitAddress_mask		= 0x0000000F
	} PlatformSupportSelectorBitAddress;

	enum {
		kPlatformSupport_NoIO					= 0,
		kPlatformSupport_MAPPED					= 1,
		kPlatformSupport_PLATFORM				= 2,
		kPlatformSupportDBDMA_K2				= 3
	} PlatformSupportSelectors;

};

#endif	/*	__PLATFORMINTERFACE__	*/
