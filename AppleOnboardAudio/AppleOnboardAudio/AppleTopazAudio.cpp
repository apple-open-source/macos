/*
 *  AppleTopazAudio.cpp
 *  AppleOnboardAudio
 *
 *  Created by Matthew Xavier Mora on Thu Mar 13 2003.
 *  Copyright © 2003 Apple Computer, Inc. All rights reserved.
 *
 *	29 October 2003		rbm		[3446131]	Converted to plugin architecture.
 *
 */

#include "AppleTopazAudio.h"

OSDefineMetaClassAndStructors ( AppleTopazAudio, AudioHardwareObjectInterface )

#define super IOService

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// call into superclass and initialize.
bool AppleTopazAudio::init(OSDictionary *properties)
{
	debugIOLog (3,  "+ AppleTopazAudio::init" );
	if (!super::init(properties))
		return false;
		
	mUnlockErrorCount = 0;
	mCurrentMachine1State = kTopazState_Idle;
	mCurrentMachine2State = kMachine2_idleState;
	mUnlockStatus = false;
	mRecoveryInProcess = false;
	mDigitalInStatus = kGPIO_Connected;
	
	mClockSource = kTRANSPORT_MASTER_CLOCK;
	
	debugIOLog (3,  "- AppleTopazAudio::init" );
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTopazAudio::start (IOService * provider) {
	bool					result;

	debugIOLog (3, "+AppleTopazAudio[%p]::start(%p)", this, provider);

	result = FALSE;
	FailIf (!provider, Exit);
	mAudioDeviceProvider = (AppleOnboardAudio *)provider;

	result = super::start (provider);

	result = provider->open (this, kFamilyOption_OpenMultiple);
	
Exit:
	debugIOLog (3, "-AppleTopazAudio[%p]::start(%p) returns bool %d", this, provider, result);
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// call inherited free
void AppleTopazAudio::free()
{
	debugIOLog (3,  "+AppleTopazAudio::free" );

	CLEAN_RELEASE ( mTopazPlugin );
	
	super::free();
	debugIOLog (3,  "-AppleTopazAudio::free" );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazAudio::initPlugin(PlatformInterface* inPlatformObject) {

	
	debugIOLog (3,  "+AppleTopazAudio::initPlugin inPlatformObject = %X", (unsigned int)inPlatformObject);
	
	mPlatformInterface = inPlatformObject;

	debugIOLog (3,  "-AppleTopazAudio::initPlugin" );
		
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTopazAudio::willTerminate ( IOService * provider, IOOptionBits options ) {

	bool result = super::willTerminate( provider, options );

	debugIOLog (3, "AppleTopazAudio::willTerminate(%p) returns %d, mAudioDeviceProvider = %p", provider, result, mAudioDeviceProvider);

	if (provider == mAudioDeviceProvider) {
		debugIOLog (3, "closing our provider" );
		provider->close (this);
	}

	debugIOLog (3, "- AppleTopazAudio[%p]::willTerminate(%p) returns %s", this, provider, result == true ? "true" : "false" );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTopazAudio::requestTerminate ( IOService * provider, IOOptionBits options ) {

	bool result = super::requestTerminate( provider, options );
	debugIOLog (3, "AppleTopazAudio::requestTerminate(%p) returns %d, mAudioDeviceProvider = %p", provider, result, mAudioDeviceProvider);

	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTopazAudio::preDMAEngineInit () {
	UInt8			data;
	bool			result = false;
	
	debugIOLog (3,  "+AppleTopazAudio::preDMAEngineInit" );

	FailIf ( NULL == mPlatformInterface, Exit );
	FailIf ( NULL == mAudioDeviceProvider, Exit );									//  [3648867]
	
	//  [3648867]   Ask the Provider for the transport interface index.  There
	//  is a hard association or binding between the transport interface index
	//  and the CS84xx device address as follows:
	//
	//  AD2		AD1		AD0		I2C Address		Transport Index
	//
	//   0		 0		 0		0x20			0
	//   0		 0		 1		0x22			1
	//   0		 1		 0		0x24			2
	//   0		 1		 1		0x26			3
	//   1		 0		 0		0x28			4
	//   1		 0		 1		0x2A			5
	//   1		 1		 0		0x2C			6
	//   1		 1		 1		0x2E			7
	//
	//  Since the transport index shifted by one is equal to the offset from the
	//  I2C base address, the target address can be easily calculated as follows:
	
	mTopaz_I2C_Address = kCS84xx_I2C_ADDRESS + ( mAudioDeviceProvider->getTransportIndex () << 1 ); //  [3648867]

	mOptimizePollForUserClient_counter = kINITIAL_VALUE_FOR_POLL_FOR_USER_CLIENT_COUNT;

	CODEC_Reset ();
	data = CODEC_ReadID();
	FailIf ( 0 == data, Exit );
	
	data &= kCS84XX_ID_MASK;
	data >>= baID;
	
	switch ( data ) {
		case cs8406_id:			mCodecID = kCS8406_CODEC;			break;
		case cs8416_id:			mCodecID = kCS8416_CODEC;			break;
		case cs8420_id:			mCodecID = kCS8420_CODEC;			break;
		default:				FailIf ( true, Exit );				break;
	}
	
	mTopazPlugin = AppleTopazPluginFactory::createTopazPlugin ( mCodecID );
	FailIf (NULL == mTopazPlugin, Exit);
	
	mTopazPlugin->initPlugin ( mPlatformInterface, mAudioDeviceProvider, mTopaz_I2C_Address, mCodecID );   //  [3648867]
	
	mTopazPlugin->initCodecRegisterCache ();
	result = mTopazPlugin->preDMAEngineInit ();
	
Exit:
	debugIOLog (3,  "-AppleTopazAudio::preDMAEngineInit err = %X", result );

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTopazAudio::setCodecMute (bool muteState) {
	return setMute ( muteState, kDigitalAudioSelector );	//	[3435307]	
}

// --------------------------------------------------------------------------
//	[3435307]	
IOReturn AppleTopazAudio::setCodecMute (bool muteState, UInt32 streamType) {
	IOReturn		result = kIOReturnError;
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setMute ( %d, %4s ) using AppleTopazPluginCS8406", muteState, (char*)&streamType );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setMute ( %d, %4s ) using AppleTopazPluginCS8416", muteState, (char*)&streamType );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setMute ( %d, %4s ) using AppleTopazPluginCS8420", muteState, (char*)&streamType );		break;
		default:				debugIOLog ( 5,  "+ AppleTopazAudio::setMute ( %d, %4s ) using AppleTopazPluginUNKNOWN", muteState, (char*)&streamType );		break;
	}

	if ( ( 0 != mTopazPlugin ) && ( kDigitalAudioSelector == streamType ) ) {
		result = mTopazPlugin->setMute ( muteState );
	}

	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setMute ( %d, %4s ) using AppleTopazPluginCS8406", muteState, (char*)&streamType );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setMute ( %d, %4s ) using AppleTopazPluginCS8416", muteState, (char*)&streamType );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setMute ( %d, %4s ) using AppleTopazPluginCS8420", muteState, (char*)&streamType );		break;
		default:				debugIOLog ( 5,  "- AppleTopazAudio::setMute ( %d, %4s ) using AppleTopazPluginUNKNOWN", muteState, (char*)&streamType );		break;
	}
	return result;
}

// --------------------------------------------------------------------------
//	[3435307]	
bool AppleTopazAudio::hasDigitalMute ()
{
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Stop the internal clocks and set the transmit output to 0 volts for low 
//	power consumption.  Device register updates are applied only to the
//	device and avoid touching the register cache.
IOReturn AppleTopazAudio::performDeviceSleep () {
	IOReturn		result = kIOReturnError;
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::performDeviceSleep () using AppleTopazPluginCS8406" );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::performDeviceSleep () using AppleTopazPluginCS8416" );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::performDeviceSleep () using AppleTopazPluginCS8420" );		break;
		default:				debugIOLog ( 5,  "+ AppleTopazAudio::performDeviceSleep () using AppleTopazPluginUNKNOWN" );	break;
	}
	

	if ( 0 != mTopazPlugin ) {
		result = mTopazPlugin->performDeviceSleep ();
	}
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::performDeviceSleep () using AppleTopazPluginCS8406 returns %lX", result );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::performDeviceSleep () using AppleTopazPluginCS8416 returns %lX", result );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::performDeviceSleep () using AppleTopazPluginCS8420 returns %lX", result );		break;
		default:				debugIOLog ( 5,  "- AppleTopazAudio::performDeviceSleep () using AppleTopazPluginUNKNOWN returns %lX", result );	break;
	}
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Reset the device and flush the cached register set to the device.  This
//	is necessary for portable CPUs where power will have been removed from
//	the device during sleep.  After flushing the cached register values to
//	the device, place the device into the RUN state.
IOReturn AppleTopazAudio::performDeviceWake () {
	IOReturn		result = kIOReturnError;
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::performDeviceWake () using AppleTopazPluginCS8406" );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::performDeviceWake () using AppleTopazPluginCS8416" );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::performDeviceWake () using AppleTopazPluginCS8420" );		break;
		default:				debugIOLog ( 5,  "+ AppleTopazAudio::performDeviceWake () using AppleTopazPluginUNKNOWN" );		break;
	}
	
	CODEC_Reset ();
	if ( 0 != mTopazPlugin ) { 
		result = mTopazPlugin->performDeviceWake (); 
	}

	//	[3333215]	Codec operation may not be restored after wake from
	//	sleep so start state machine to recover codec operation.
	//	[3344893,3352595]	Recovery is only implemented if running on the internal 
	//	clock source since failure is associated with the sample rate converter 
	//	and the sample rate converter is not in use when running on the external 
	//	clock source.
	if ( kTRANSPORT_MASTER_CLOCK == mClockSource ) {
		mCurrentMachine2State = kMachine2_startState;
	}
	
	mOptimizePollForUserClient_counter = kINITIAL_VALUE_FOR_POLL_FOR_USER_CLIENT_COUNT; //  [3686032]   initialize so log will show register dump a while longer

	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::performDeviceWake () using AppleTopazPluginCS8406 returns %lX", result );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::performDeviceWake () using AppleTopazPluginCS8416 returns %lX", result );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::performDeviceWake () using AppleTopazPluginCS8420 returns %lX", result );		break;
		default:				debugIOLog ( 5,  "- AppleTopazAudio::performDeviceWake () using AppleTopazPluginUNKNOWN returns %lX", result );		break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazAudio::setSampleRate ( UInt32 sampleRate ) {
	IOReturn		result = kIOReturnBadArgument;

	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setSampleRate ( %ld ) using AppleTopazPluginCS8406", sampleRate );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setSampleRate ( %ld ) using AppleTopazPluginCS8416", sampleRate );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setSampleRate ( %ld ) using AppleTopazPluginCS8420", sampleRate );		break;
		default:				debugIOLog ( 5,  "+ AppleTopazAudio::setSampleRate ( %ld ) using AppleTopazPluginUNKNOWN", sampleRate );	break;
	}
	
	//  [3684994]   Reject sample rates that the hardware cannot lock to.  The CS84xx 
	//				family cannot lock below 32.000 kHz.
	if ( 0 != sampleRate ) {
		if ( ( kMinimumSupportedCS84xxSampleRate <= sampleRate ) && ( kMaximumSupportedCS84xxSampleRate >= sampleRate ) ) {	//  [3684994]
			mChannelStatus.sampleRate = sampleRate;
			
			//	Avoid general recovery when running on external clock as a reset
			//	will set the clocks back to internal.  Just indicate if the sample
			//	rate is valid so that AppleOnboardAudio's poll method that validates
			//	sample rate changes can operate correctly but leave the hardware alone!
			if ( kTRANSPORT_MASTER_CLOCK == mClockSource && !mTopazPlugin->canOnlyMasterTheClock() ) {
				generalRecovery();
			}
			
			if ( 0 != mTopazPlugin ) { 
				result = mTopazPlugin->setChannelStatus ( &mChannelStatus ); 
			}
		}
	}
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setSampleRate ( %ld ) using AppleTopazPluginCS8406 returns %lX", sampleRate, result );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setSampleRate ( %ld ) using AppleTopazPluginCS8416 returns %lX", sampleRate, result );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setSampleRate ( %ld ) using AppleTopazPluginCS8420 returns %lX", sampleRate, result );		break;
		default:				debugIOLog ( 5,  "- AppleTopazAudio::setSampleRate ( %ld ) using AppleTopazPluginUNKNOWN returns %lX", sampleRate, result );	break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazAudio::setSampleDepth ( UInt32 sampleDepth ) {
	IOReturn		result = kIOReturnBadArgument;
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setSampleDepth ( %ld ) using AppleTopazPluginCS8406", sampleDepth );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setSampleDepth ( %ld ) using AppleTopazPluginCS8416", sampleDepth );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setSampleDepth ( %ld ) using AppleTopazPluginCS8420", sampleDepth );		break;
		default:				debugIOLog ( 5,  "+ AppleTopazAudio::setSampleDepth ( %ld ) using AppleTopazPluginUNKNOWN", sampleDepth );		break;
	}
	
	FailIf ( !( 16 == sampleDepth || 24 == sampleDepth ), Exit );
	//	Avoid general recovery when running on external clock as a reset
	//	will set the clocks back to internal.  Just indicate if the bit
	//	depth is valid so that AppleOnboardAudio's poll method that validates
	//	sample rate changes can operate correctly but leave the hardware alone!
	if ( kTRANSPORT_MASTER_CLOCK == mClockSource ) {
		generalRecovery();
	}
	
	mChannelStatus.sampleDepth = sampleDepth;
	
	if ( 0 != mTopazPlugin ) { 
		result = mTopazPlugin->setChannelStatus ( &mChannelStatus ); 
	}
	
Exit:
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setSampleDepth ( %ld ) using AppleTopazPluginCS8406", sampleDepth );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setSampleDepth ( %ld ) using AppleTopazPluginCS8416", sampleDepth );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setSampleDepth ( %ld ) using AppleTopazPluginCS8420", sampleDepth );		break;
		default:				debugIOLog ( 5,  "- AppleTopazAudio::setSampleDepth ( %ld ) using AppleTopazPluginUNKNOWN", sampleDepth );		break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTopazAudio::setSampleType ( UInt32 sampleType ) {
	IOReturn			result;
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setSampleType ( %ld ) using AppleTopazPluginCS8406", sampleType );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setSampleType ( %ld ) using AppleTopazPluginCS8416", sampleType );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::setSampleType ( %ld ) using AppleTopazPluginCS8420", sampleType );		break;
		default:				debugIOLog ( 5,  "+ AppleTopazAudio::setSampleType ( %ld ) using AppleTopazPluginUNKNOWN", sampleType );	break;
	}
	
	result = kIOReturnSuccess;
	switch ( sampleType ) {
		case kIOAudioStreamSampleFormat1937AC3:		mChannelStatus.nonAudio = TRUE;		break;
		case kIOAudioStreamSampleFormatLinearPCM:	mChannelStatus.nonAudio = FALSE;	break;
		default:									result = kIOReturnBadArgument;		break;
	}
	if ( kIOReturnSuccess == result ) {
		if ( 0 != mTopazPlugin ) { 
			result = mTopazPlugin->setChannelStatus ( &mChannelStatus ); 
		}
	}

	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setSampleType ( %ld ) using AppleTopazPluginCS8406", sampleType );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setSampleType ( %ld ) using AppleTopazPluginCS8416", sampleType );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::setSampleType ( %ld ) using AppleTopazPluginCS8420", sampleType );		break;
		default:				debugIOLog ( 5,  "- AppleTopazAudio::setSampleType ( %ld ) using AppleTopazPluginUNKNOWN", sampleType );	break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	AppleTopazAudio::getClockLock ( void ) {
	UInt32		result = 1;
	
	if ( 0 != mTopazPlugin ) { 
		result = mTopazPlugin->getClockLock (); 
	}
	return result;
}

//	------------------------------------------------------------------------------------------------------------------------------------
//	Switching bewteen a system mastered clock and an external clock, such as a recovered clock from an S/PDIF AES3 stream, requires a 
//	"BREAK BEFORE MAKE" sequence to avoid having two hardware drivers connected together.  If selecting the master clock then the 
//	external MUX must be disconnected prior to enabling the system master clock.  If selecting an external MUX clock source then the 
//	internal system master clock must be disconnected first.  Sequences are:
//
//	TRANSITION						CYCLE					ACTION
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_MASTER_CLOCK to	|	1 Transport Break	|	Set MUX to alternate clock source, set I2S to SLAVE (BCLKMaster = SLAVE).
//	kTRANSPORT_SLAVE_CLOCK		|	2 Topaz Break		|	Stop CS84xx & mute TX.  Set all registers to act as a clock master.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 10, SRCD = 0
//								|						|	B.	Clock Source Control Register:	OUTC = 1, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 1
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	No Action.
//								|	5 Topaz Make		|	Start CS84xx.  Send request to restart transport hardware.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_SLAVE_CLOCK to	|	1 Transport Break	|	No Action.
//	kTRANSPORT_MASTER_CLOCK		|	2 Topaz Break		|	Stop CS84xx & disable TX.  Set all registers to act as a clock slave.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 00, SRCD = 1
//								|						|	B.	Clock Source Control Register:	OUTC = 0, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 0
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	Set MUX to default clock source, set I2S to SLAVE (BCLKMaster = MASTER).
//								|	5 Topaz Make		|	Start CS84xx & unmute TX.  Send request to restart transport hardware.
//								|						|	A.	Clear pending receiver errors.
//								|						|	B.	Enable receiver errors.
//								|						|	C.	Set CS8420 to RUN.
//								|						|	D.	Request a restart of the I2S transport.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
IOReturn	AppleTopazAudio::breakClockSelect ( UInt32 clockSource ) {
	IOReturn			result = kIOReturnError;

	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::breakClockSelect ( %d ) using AppleTopazPluginCS8406", (unsigned int)clockSource );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::breakClockSelect ( %d ) using AppleTopazPluginCS8416", (unsigned int)clockSource );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::breakClockSelect ( %d ) using AppleTopazPluginCS8420", (unsigned int)clockSource );		break;
		default:				debugIOLog ( 5,  "+ AppleTopazAudio::breakClockSelect ( %d ) using AppleTopazPluginUNKNOWN", (unsigned int)clockSource );		break;
	}
	
	if (mAttemptingExternalLock) {
		mAttemptingExternalLock = false;
	}
	
	if ( 0 != mTopazPlugin ) { 
		result = mTopazPlugin->breakClockSelect ( clockSource ); 
	} else {
		debugIOLog ( 5, " AppleTopazAudio::breakClockSelect attempt to redirect clock source with no plugin present" );
	}
	
	mUnlockErrorCount = 0;
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::breakClockSelect ( %d ) using AppleTopazPluginCS8406 returns %lX", (unsigned int)clockSource, (unsigned int)result );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::breakClockSelect ( %d ) using AppleTopazPluginCS8416 returns %lX", (unsigned int)clockSource, (unsigned int)result );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::breakClockSelect ( %d ) using AppleTopazPluginCS8420 returns %lX", (unsigned int)clockSource, (unsigned int)result );		break;
		default:				debugIOLog ( 5,  "- AppleTopazAudio::breakClockSelect ( %d ) using AppleTopazPluginUNKNOWN returns %lX", (unsigned int)clockSource, (unsigned int)result );		break;
	}
	return result;
}

//	------------------------------------------------------------------------------------------------------------------------------------
//	Switching bewteen a system mastered clock and an external clock, such as a recovered clock from an S/PDIF AES3 stream, requires a 
//	"BREAK BEFORE MAKE" sequence to avoid having two hardware drivers connected together.  If selecting the master clock then the 
//	external MUX must be disconnected prior to enabling the system master clock.  If selecting an external MUX clock source then the 
//	internal system master clock must be disconnected first.  Sequences are:
//
//	TRANSITION						CYCLE					ACTION
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_MASTER_CLOCK to	|	1 Transport Break	|	Set MUX to alternate clock source, set I2S to SLAVE (BCLKMaster = SLAVE).
//	kTRANSPORT_SLAVE_CLOCK		|	2 Topaz Break		|	Stop CS84xx & mute TX.  Set all registers to act as a clock master.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 10, SRCD = 0
//								|						|	B.	Clock Source Control Register:	OUTC = 1, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 1
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	No Action.
//								|	5 Topaz Make		|	Start CS84xx.  Send request to restart transport hardware.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_SLAVE_CLOCK to	|	1 Transport Break	|	No Action.
//	kTRANSPORT_MASTER_CLOCK		|	2 Topaz Break		|	Stop CS84xx & disable TX.  Set all registers to act as a clock slave.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 00, SRCD = 1
//								|						|	B.	Clock Source Control Register:	OUTC = 0, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 0
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	Set MUX to default clock source, set I2S to SLAVE (BCLKMaster = MASTER).
//								|	5 Topaz Make		|	Start CS84xx & unmute TX.  Send request to restart transport hardware.
//								|						|	A.	Clear pending receiver errors.
//								|						|	B.	Enable receiver errors.
//								|						|	C.	Set CS8420 to RUN.
//								|						|	D.	Request a restart of the I2S transport.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
IOReturn	AppleTopazAudio::makeClockSelect ( UInt32 clockSource ) {
	IOReturn			result = kIOReturnError;
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::makeClockSelect ( %d ) using AppleTopazPluginCS8406", (unsigned int)clockSource );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::makeClockSelect ( %d ) using AppleTopazPluginCS8416", (unsigned int)clockSource );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::makeClockSelect ( %d ) using AppleTopazPluginCS8420", (unsigned int)clockSource );		break;
		default:				debugIOLog ( 5,  "+ AppleTopazAudio::makeClockSelect ( %d ) using AppleTopazPluginUNKNOWN", (unsigned int)clockSource );	break;
	}

	FailIf ( NULL == mTopazPlugin, Exit );
	FailIf ( NULL == mAudioDeviceProvider, Exit );
	result = mTopazPlugin->makeClockSelectPreLock ( clockSource ); 
	
	if ( kTRANSPORT_SLAVE_CLOCK == clockSource && mTopazPlugin->supportsDigitalInput() ) {
		//	It is necessary to restart the I2S cell here after the clocks have been
		//	established using the CS8420 as the clock source.  Ask AOA to restart
		//	the I2S cell.  This is only done if the CODEC can provide a clock source
		//  and should not be done if the CODEC is an output only device such as the CS8406.
		debugIOLog ( 4, "  *** AppleTopazAudio::makeClockSelect about to post kRestartTransport request" );
		mAudioDeviceProvider->interruptEventHandler ( kRestartTransport, (UInt32)0 );
		// [3253678], set flag to broadcast lock success to AOA so it can unmute analog part
		mAttemptingExternalLock = true;
	}
	
	result = mTopazPlugin->makeClockSelectPostLock ( clockSource ); 
	mUnlockErrorCount = 0;

	if ( kTRANSPORT_MASTER_CLOCK == clockSource && mTopazPlugin->supportsDigitalInput() ) {
		if ( mUnlockStatus ) {
			mUnlockStatus = false;
		}
		mCurrentMachine2State = kMachine2_startState;
	}
	
	mClockSource = clockSource;
	
Exit:
	mUnlockErrorCount = 0;
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::makeClockSelect ( %d ) using AppleTopazPluginCS8406 returns %lX", (unsigned int)clockSource, (unsigned int)result );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::makeClockSelect ( %d ) using AppleTopazPluginCS8416 returns %lX", (unsigned int)clockSource, (unsigned int)result );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::makeClockSelect ( %d ) using AppleTopazPluginCS8420 returns %lX", (unsigned int)clockSource, (unsigned int)result );		break;
		default:				debugIOLog ( 5,  "- AppleTopazAudio::makeClockSelect ( %d ) using AppleTopazPluginUNKNOWN returns %lX", (unsigned int)clockSource, (unsigned int)result );		break;
	}

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	generalErrorRecovery:
//
//		Save away the current run state before stopping the S/PDIF codec.
//		Then reset the S/PDIF codec and restore all the register states.  
//		Complete the recovery by restoring the original S/PDIF codec run state.
//
void AppleTopazAudio::generalRecovery ( void ) {
	UInt8		data;
	
	FailIf ( 0 == mTopazPlugin, Exit );
	if ( !mGeneralRecoveryInProcess ) {
		mGeneralRecoveryInProcess = TRUE;
		data = mTopazPlugin->setStopMode ();
		CODEC_Reset();
		mTopazPlugin->flushControlRegisters ();
		mTopazPlugin->setRunMode ( data );
		mCurrentMachine2State = kMachine2_startState;
		mGeneralRecoveryInProcess = FALSE;
	}
Exit:
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Fatal error recovery 
IOReturn AppleTopazAudio::recoverFromFatalError ( FatalRecoverySelector selector ) {

	FailIf ( NULL == mPlatformInterface, Exit );
	if ( mRecoveryInProcess ) { debugIOLog (7,  "REDUNDANT RECOVERY FROM FATAL ERROR" ); }
	
	mRecoveryInProcess = true;
	switch ( selector ) {
		case kControlBusFatalErrorRecovery:
			generalRecovery();
			break;
		case kClockSourceInterruptedRecovery:
			generalRecovery();
			break;
		default:
			break;
	}
	mRecoveryInProcess = false;
	
Exit:
	return kIOReturnSuccess;
}

#pragma mark ---------------------
#pragma mark •	INTERRUPT HANDLERS
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  This method is invoked from the 'codecErrorInterruptHandler' residing in the
//  platform interface object.  The 'codecErrorInterruptHandler' may be invoked
//  through GPIO hardware interrupt dispatch services or throught timer polled
//  services.
void AppleTopazAudio::notifyHardwareEvent ( UInt32 statusSelector, UInt32 newValue ) {
	UInt8					saveMAP = 0;
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::notifyHardwareEvent ( %d, %d ) using AppleTopazPluginCS8406", statusSelector, newValue );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::notifyHardwareEvent ( %d, %d ) using AppleTopazPluginCS8416", statusSelector, newValue );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "+ AppleTopazAudio::notifyHardwareEvent ( %d, %d ) using AppleTopazPluginCS8420", statusSelector, newValue );		break;
		default:				debugIOLog ( 5,  "+ AppleTopazAudio::notifyHardwareEvent ( %d, %d ) using AppleTopazPluginUNKNOWN", statusSelector, newValue );		break;
	}
	
	FailIf ( NULL == mTopazPlugin, Exit );
	FailIf ( NULL == mAudioDeviceProvider, Exit );

	saveMAP = mTopazPlugin->getMemoryAddressPointer();		//  Preserve illusion of atomic I2C register access outside of this interrupt handler
	
	//  [3629501]   If there is a digital input detect (either a dedicated detect or
	//  a combo jack detect) then indicate 'Unlock' when the detect indicates that
	//  the digital plug is removed.  If no digital input is located then the initial
	//  digital detect status that was written to the 'mDigitalInStatus' member
	//  variable, which indicates that a device is connected to the digital input, will
	//  be used.  NOTE:  The digital input detect may not be associated with this
	//  AppleOnboardAudio instance.  It is necessary to receive notifications from
	//  the AppleOnboardAudio object of the current digital input detect status and
	//  the source of these messages may be from a broadcast message conveyed from
	//  another AppleOnboardAudio instance!
	
	if ( kDigitalInStatus == statusSelector ) {
		mDigitalInStatus = newValue;
		debugIOLog ( 6, "  AppleTopazAudio::notifyHardwareEvent ( %d, %d ) updates mDigitalInStatus to %d", statusSelector, newValue, mDigitalInStatus );
	}
	
	if ( ( kCodecErrorInterruptStatus == statusSelector ) || ( kCodecInterruptStatus == statusSelector ) ) {
		switch ( mDigitalInStatus ) {
			case kGPIO_Unknown:			mTopazPlugin->notifyHardwareEvent( statusSelector, newValue );						break;
			case kGPIO_Connected:		mTopazPlugin->notifyHardwareEvent( statusSelector, newValue );						break;
			case kGPIO_Disconnected:	mAudioDeviceProvider->interruptEventHandler ( kClockUnLockStatus, (UInt32)0 );		break;
		}
	}
	
	if ( mTopazPlugin->getMemoryAddressPointer() != saveMAP ) {
		mTopazPlugin->setMemoryAddressPointer ( saveMAP );		//  Preserve illusion of atomic I2C register access outside of this interrupt handler
	}
Exit:
	switch ( mCodecID ) {
		case kCS8406_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::notifyHardwareEvent ( %d, %d ) using AppleTopazPluginCS8406", statusSelector, newValue );		break;
		case kCS8416_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::notifyHardwareEvent ( %d, %d ) using AppleTopazPluginCS8416", statusSelector, newValue );		break;
		case kCS8420_CODEC:		debugIOLog ( 5,  "- AppleTopazAudio::notifyHardwareEvent ( %d, %d ) using AppleTopazPluginCS8420", statusSelector, newValue );		break;
		default:				debugIOLog ( 5,  "- AppleTopazAudio::notifyHardwareEvent ( %d, %d ) using AppleTopazPluginUNKNOWN", statusSelector, newValue );		break;
	}
	return;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazAudio::poll ( void ) {
	//  Time optimization:  Only poll the hardware plugins to collect ALL of
	//  the CODEC register (excluding interrupt status) contents into the 
	//  register cache when not using the user client interface to view the
	//  register data.
	if ( 0 != mOptimizePollForUserClient_counter ) {
		if ( mTopazPlugin ) {
			mTopazPlugin->poll ();
		}
		mOptimizePollForUserClient_counter--;
	}
	stateMachine1 ();
	stateMachine2 ();
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This state machine supports radar 3264127 by periodically attempting to 
//	derive the recovered clock from an AES3 stream.  If no AES3 stream is present
//	then the derived clock source will revert back to the I2S I/O Module LRCLK.
//	If the AES3 stream is present then the clock will continue to use the AES3
//	stream to derive the recovered clock until the AES3 stream is removed.
void AppleTopazAudio::stateMachine1 ( void ) {
	switch ( mCurrentMachine1State ) {
		case kTopazState_Idle:
			//mCurrentMachine1State = kTopazState_PrepareToArmTryAES3 ;
			break;
		case kTopazState_PrepareToArmLossOfAES3:
			mCurrentMachine1State = mAES3detected ? kTopazState_PrepareToArmLossOfAES3 : kTopazState_ArmLossOfAES3 ;
			break;
		case kTopazState_ArmLossOfAES3:
			mCurrentMachine1State = mAES3detected ? kTopazState_PrepareToArmLossOfAES3 : kTopazState_TriggerLossOfAES3 ;
			break;
		case kTopazState_TriggerLossOfAES3:
			if ( mAES3detected ) {
				mCurrentMachine1State = kTopazState_PrepareToArmLossOfAES3;
			} else {
				mCurrentMachine1State = kTopazState_PrepareToArmTryAES3;
				//	If the recovered clock is derived from an external source and there is no external source 
				//	then switch the recovered clock to derive from the I2S I/O Module LRCLK.
				mTopazPlugin->useInternalCLK ();
			}
			break;
		case kTopazState_PrepareToArmTryAES3:
			if ( mAES3detected ) {
				mCurrentMachine1State = kTopazState_ArmTryAES3 ;
			}
			break;
		case kTopazState_ArmTryAES3:
			if ( mAES3detected ) {
				mCurrentMachine1State = kTopazState_TriggerTryAES3 ;
			}
			break;
		case kTopazState_TriggerTryAES3:
			if ( mAES3detected ) {
				mCurrentMachine1State = kTopazState_PrepareToArmLossOfAES3 ;
				//	If the recovered clock is derived from the I2S I/O Module LRCLK and there is an external source 
				//	then switch the recovered clock to derive from the external source.
				mTopazPlugin->useExternalCLK ();
			}
			break;
	}
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This state machine attempts to recover operation of the CS8420 output
//	after switching clock sources.
void AppleTopazAudio::stateMachine2 ( void ) {
	switch ( mCurrentMachine2State ) {
		case kMachine2_idleState:
			break;
		case kMachine2_startState:
			debugIOLog ( 5, "± AppleTopazAudio::stateMachine2 advancing from kMachine2_startState to kMachine2_delay1State" );
			mOptimizePollForUserClient_counter = kINITIAL_VALUE_FOR_POLL_FOR_USER_CLIENT_COUNT; //  [3686032]   initialize so log will show register dump a while longer
			mCurrentMachine2State = kMachine2_delay1State;
			break;
		case kMachine2_delay1State:
			debugIOLog ( 5, "± AppleTopazAudio::stateMachine2 advancing from kMachine2_delay1State to kMachine2_setRxd_ILRCK" );
			mOptimizePollForUserClient_counter = kINITIAL_VALUE_FOR_POLL_FOR_USER_CLIENT_COUNT; //  [3686032]   initialize so log will show register dump a while longer
			mCurrentMachine2State = kMachine2_setRxd_ILRCK;
			break;
		case kMachine2_setRxd_ILRCK:
			debugIOLog ( 5, "± AppleTopazAudio::stateMachine2 advancing from kMachine2_setRxd_ILRCK to kMachine2_setRxd_AES3" );
			mOptimizePollForUserClient_counter = kINITIAL_VALUE_FOR_POLL_FOR_USER_CLIENT_COUNT; //  [3686032]   initialize so log will show register dump a while longer
			mTopazPlugin->useInternalCLK();
			mCurrentMachine2State = kMachine2_setRxd_AES3;
			break;
		case kMachine2_setRxd_AES3:
			debugIOLog ( 5, "± AppleTopazAudio::stateMachine2 advancing from kMachine2_setRxd_AES3 to kMachine2_idleState" );
			mOptimizePollForUserClient_counter = kINITIAL_VALUE_FOR_POLL_FOR_USER_CLIENT_COUNT; //  [3686032]   initialize so log will show register dump a while longer
			mTopazPlugin->useExternalCLK();
			mCurrentMachine2State = kMachine2_idleState;
			break;
	}
}


#pragma mark ---------------------
#pragma mark • CODEC Functions
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt8 	AppleTopazAudio::CODEC_ReadID ( void ) {
	UInt8			result;
	Boolean			success;
	UInt8			regAddr;
	UInt32			retryCount;

	result = kIOReturnError;
	regAddr = map_ID_VERSION;
	FailIf ( NULL == mPlatformInterface, Exit );
	result = 0;
	
	//  [3648867]   Attempt to use the specified I2C address.  If failure occurs
	//  AND the I2C address is not the default address then switch to the default
	//  address and try again.  This will allow CS84xx devices that are not wired
	//  compliant with the dynamic probing specification to continue to operate.
	retryCount = kCS84xx_I2C_ADDRESS == mTopaz_I2C_Address ? 1 : 2 ;
	do {
		success = mPlatformInterface->writeCodecRegister( mTopaz_I2C_Address, 0, &regAddr, 1, kI2C_StandardMode);
		if ( success ) {
			success = mPlatformInterface->readCodecRegister( mTopaz_I2C_Address, 0, &result, 1, kI2C_StandardMode );
		}
		if ( !success ) {
			mTopaz_I2C_Address = kCS84xx_I2C_ADDRESS;
		}
		retryCount--;
	} while ( !success && ( 0 != retryCount ) );
	 
Exit:	
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazAudio::CODEC_Reset ( void ) {
	IOReturn		err;
	
	FailIf ( NULL == mPlatformInterface, Exit );

	err = mPlatformInterface->setCodecReset ( kCODEC_RESET_Digital, kGPIO_Run );
	FailIf ( kIOReturnSuccess != err, Exit );
	IODelay ( 250 );
	err = mPlatformInterface->setCodecReset ( kCODEC_RESET_Digital, kGPIO_Reset );
	FailIf ( kIOReturnSuccess != err, Exit );
	IODelay ( 250 );
	err = mPlatformInterface->setCodecReset ( kCODEC_RESET_Digital, kGPIO_Run );
	FailIf ( kIOReturnSuccess != err, Exit );
	IODelay ( 250 );
Exit:
	return;
}


#pragma mark ---------------------
#pragma mark • USER CLIENT SUPPORT
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazAudio::getPluginState ( HardwarePluginDescriptorPtr outState ) {
	IOReturn		result = kIOReturnError;
	
	mOptimizePollForUserClient_counter = kINITIAL_VALUE_FOR_POLL_FOR_USER_CLIENT_COUNT;
	if ( mTopazPlugin ) {
		result = mTopazPlugin->getPluginState ( outState );
	}
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazAudio::setPluginState ( HardwarePluginDescriptorPtr inState ) {
	IOReturn		result = kIOReturnError;
	
	if ( mTopazPlugin ) {
		result = mTopazPlugin->setPluginState ( inState );
	}
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
HardwarePluginType	AppleTopazAudio::getPluginType ( void ) {
	HardwarePluginType			result = kCodec_Unknown;
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:					result = kCodec_CS8406;				break;
		case kCS8416_CODEC:					result = kCodec_CS8416;				break;
		case kCS8420_CODEC:					result = kCodec_CS8420;				break;
		default:							result = kCodec_Unknown;			break;
	};
	return result;
}




