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
	
	CODEC_Reset ();
	data = CODEC_ReadID();
	FailIf ( 0 == data, Exit );
	
	data &= kCS84XX_ID_MASK;
	data >>= baID;
	
	switch ( data ) {
		case cs8420_id:			mCodecID = kCS8420_CODEC;			break;
		case cs8406_id:			mCodecID = kCS8406_CODEC;			break;
		case cs8416_id:			mCodecID = kCS8416_CODEC;			break;
		default:				FailIf ( true, Exit );				break;
	}
	
	mTopazPlugin = AppleTopazPluginFactory::createTopazPlugin ( mCodecID );
	FailIf (NULL == mTopazPlugin, Exit);
	mTopazPlugin->initPlugin ( mPlatformInterface );
	
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
	
	debugIOLog (3, "+ AppleTopazAudio::setMute (%d, %4s)", muteState, (char*)&streamType);

	if ( ( 0 != mTopazPlugin ) && ( kDigitalAudioSelector == streamType ) ) {
		result = mTopazPlugin->setMute ( muteState );
	}

	debugIOLog (3, "- AppleTopazAudio::setMute (%d, %4s) returns %X", muteState, (char*)&streamType, result);
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
	
	debugIOLog (3, "+ AppleTopazAudio::performDeviceSleep()");

	if ( 0 != mTopazPlugin ) {
		result = mTopazPlugin->performDeviceSleep ();
	}
	
	debugIOLog (3, "- AppleTopazAudio::performDeviceSleep()");
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Reset the device and flush the cached register set to the device.  This
//	is necessary for portable CPUs where power will have been removed from
//	the device during sleep.  After flushing the cached register values to
//	the device, place the device into the RUN state.
IOReturn AppleTopazAudio::performDeviceWake () {
	IOReturn		result = kIOReturnError;
	
	debugIOLog (3,  "+ AppleTopazAudio::performDeviceWake()" );

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
	debugIOLog (3,  "- AppleTopazAudio::performDeviceWake()" );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazAudio::setSampleRate ( UInt32 sampleRate ) {
	IOReturn		result = kIOReturnBadArgument;

	debugIOLog ( 6, "+ AppleTopazAudio::setSampleRate ( %ld )", sampleRate );
	FailIf ( 0 == sampleRate, Exit );
	FailIf ( ( 198000 < sampleRate || sampleRate < 7800 ), Exit );
	
	mChannelStatus.sampleRate = sampleRate;
	
	//	Avoid general recovery when running on external clock as a reset
	//	will set the clocks back to internal.  Just indicate if the sample
	//	rate is valid so that AppleOnboardAudio's poll method that validates
	//	sample rate changes can operate correctly but leave the hardware alone!
	if ( kTRANSPORT_MASTER_CLOCK == mClockSource ) {
		generalRecovery();
	}
	
	if ( 0 != mTopazPlugin ) { 
		result = mTopazPlugin->setChannelStatus ( &mChannelStatus ); 
	}
	
Exit:
	debugIOLog ( 6, "- AppleTopazAudio::setSampleRate ( %ld ) returns %lX", sampleRate, result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazAudio::setSampleDepth ( UInt32 sampleDepth ) {
	IOReturn		result = kIOReturnBadArgument;
	
	debugIOLog ( 6, "+ AppleTopazAudio::setSampleDepth ( %ld )", sampleDepth );
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
	debugIOLog ( 6, "- AppleTopazAudio::setSampleDepth ( %ld ) returns %lX", sampleDepth, result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTopazAudio::setSampleType ( UInt32 sampleType ) {
	IOReturn			result;
	
	debugIOLog ( 6, "+ AppleTopazAudio::setSampleType ( %ld )", sampleType );
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
	debugIOLog ( 6, "- AppleTopazAudio::setSampleType ( %ld ) returns %lX", sampleType, result );
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

	debugIOLog (7,  "+ AppleTopazAudio::breakClockSelect ( %d )", (unsigned int)clockSource );
	
	if (mAttemptingExternalLock) {
		mAttemptingExternalLock = false;
	}
	
	if ( 0 != mTopazPlugin ) { 
		result = mTopazPlugin->breakClockSelect ( clockSource ); 
	}
	
	mUnlockErrorCount = 0;
	debugIOLog (7,  "- AppleTopazAudio::breakClockSelect ( %d ) returns %d", (unsigned int)clockSource, (unsigned int)result );

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
	
	debugIOLog (7,  "+ AppleTopazAudio::makeClockSelect ( %d )", (unsigned int)clockSource );

	FailIf ( NULL == mTopazPlugin, Exit );
	FailIf ( NULL == mAudioDeviceProvider, Exit );
	result = mTopazPlugin->makeClockSelectPreLock ( clockSource ); 
	
	if ( kTRANSPORT_SLAVE_CLOCK == clockSource && mTopazPlugin->supportsDigitalInput() ) {
		//	It is necessary to restart the I2S cell here after the clocks have been
		//	established using the CS8420 as the clock source.  Ask AOA to restart
		//	the I2S cell.
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
	debugIOLog (7,  "- AppleTopazAudio::makeClockSelect ( %d ) returns %d", (unsigned int)clockSource, (unsigned int)result );

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
	data = mTopazPlugin->setStopMode ();
	CODEC_Reset();
	mTopazPlugin->flushControlRegisters ();
	mTopazPlugin->setRunMode ( data );
	mCurrentMachine2State = kMachine2_startState;
Exit:
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Fatal error recovery 
#pragma WARNING NEED TO IMPLEMENT RECEIVER IN AOA AND BROADCASTER IN HW PLUGINS
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
void AppleTopazAudio::notifyHardwareEvent ( UInt32 statusSelector, UInt32 newValue ) {
	IOReturn		err;
	UInt32			data;
	UInt8			saveMAP = 0;
	
	FailIf ( NULL == mTopazPlugin, Exit );
	FailIf ( NULL == mAudioDeviceProvider, Exit );
	
	saveMAP = mTopazPlugin->getMemoryAddressPointer();
	if ( kCodecInterruptStatus == statusSelector ) {
		
	} else if ( kCodecErrorInterruptStatus == statusSelector ) {
		if ( 0 != mTopazPlugin ) { err = mTopazPlugin->getCodecErrorStatus ( &data ); }
		//	Only process data bits that have interrupts enabled

#if 0	// !!!FIX missing defines!!!
		debugIOLog (7,  "AppleTopazAudio::notifyHardwareEvent ### Receiver Error: 0x%X = %s %s %s %s %s %s %s",
							data,
							0 == ( data & ( 1 << baQCRC ) ) ? "qcrc" : "QCRC" ,
							0 == ( data & ( 1 << baCCRC ) ) ? "ccrc" : "CCRC" ,
							0 == ( data & ( 1 << baUNLOCK ) ) ? "unlock" : "UNLOCK" ,
							0 == ( data & ( 1 << baVALID ) ) ? "v" : "V" ,
							0 == ( data & ( 1 << baCONF ) ) ? "conf" : "CONF" ,
							0 == ( data & ( 1 << baBIP ) ) ? "bip" : "BIP" ,
							0 == ( data & ( 1 << baPARITY ) ) ? "par" : "PAR" 
						);
#endif
		//	The CS8420 generates UNLOCK errors immediately after switching the clock source.  Posting
		//	an error immediately would result in automatically switching the clock source from SLAVE
		//	to MASTER after the user selects SLAVE, even if a valid clock source is present.  Accumulate
		//	the number of errors toward a threshold trigger level before reporting an error.
		if ( !mTopazPlugin->phaseLocked() ) {
			if ( kCLOCK_UNLOCK_ERROR_TERMINAL_COUNT > mUnlockErrorCount ) {
				mUnlockErrorCount++;
			}
			if ( kCLOCK_UNLOCK_ERROR_TERMINAL_COUNT == mUnlockErrorCount ) {
				mUnlockStatus = true;
				debugIOLog (7,  "ERROR RECOVERY: mAudioDeviceProvider->interruptEventHandler ( kClockLockStatus, (UInt32)1 );" );
				if ( 0 != mTopazPlugin ) { mTopazPlugin->disableReceiverError(); }
				mAudioDeviceProvider->interruptEventHandler ( kClockLockStatus, (UInt32)1 );
				mUnlockErrorCount = 0;
				mAttemptingExternalLock = false;
				// [3253678], lock failed, don't look for success anymore
			}
		} else {
			mUnlockErrorCount = 0;
			// [3253678], broadcast lock success to AOA so it can unmute analog part
			if (mAttemptingExternalLock) {
				mAudioDeviceProvider->interruptEventHandler ( kClockUnLockStatus, (UInt32)0 );
				mAttemptingExternalLock = false;
			}
		}
		
		//	Radar 3264127 requires that the recovered clock be derived from the I2S I/O 
		//	Module LRCLK in the absence of an AES3 stream in order to guarantee that the
		//	digital output is operational.  The AES3 input stream is detected by monitoring
		//	the 'confidence' and 'bi-phase' error bits.  An AES3 stream is present if there
		//	are no 'confidence' or 'bi-phase' errors.  The AES3 connection status is
		//	maintained in the mAES3detected member variable which governs operation of
		//	a state machine that is implemented in the 'poll' method which periodically
		//	attempts to use the AES3 input to derive the recovered clock and reverts to
		//	the I2S I/O Module if the AES3 input is unavailable.
		if ( !mTopazPlugin->confidenceError() && !mTopazPlugin->biphaseError() ) {
			mAES3detected = true;
		} else {
			mAES3detected = false;
		}
	}

	if ( mTopazPlugin->getMemoryAddressPointer() != saveMAP ) {
		mTopazPlugin->setMemoryAddressPointer ( saveMAP );
	}
	IOSleep ( 10 );
Exit:
	return;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazAudio::poll ( void ) {
	if ( mTopazPlugin ) { mTopazPlugin->poll (); }
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
			mCurrentMachine2State = kMachine2_delay1State;
			break;
		case kMachine2_delay1State:
			mCurrentMachine2State = kMachine2_setRxd_ILRCK;
			break;
		case kMachine2_setRxd_ILRCK:
			mTopazPlugin->useInternalCLK();
			mCurrentMachine2State = kMachine2_setRxd_AES3;
			break;
		case kMachine2_setRxd_AES3:
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

	result = kIOReturnError;
	regAddr = map_ID_VERSION;
	FailIf ( NULL == mPlatformInterface, Exit );
	result = 0;
	success = mPlatformInterface->writeCodecRegister( kCS84xx_I2C_ADDRESS, 0, &regAddr, 1, kI2C_StandardMode);
	FailIf ( !success, Exit );
	success = mPlatformInterface->readCodecRegister( kCS84xx_I2C_ADDRESS, 0, &result, 1, kI2C_StandardMode );
	FailIf ( !success, Exit );
 
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




