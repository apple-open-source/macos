/*
 *  AppleTopazAudio.cpp
 *  AppleOnboardAudio
 *
 *  Created by Matthew Xavier Mora on Thu Mar 13 2003.
 *  Copyright © 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AppleTopazAudio.h"
#include "PlatformInterface.h"
#include "AudioHardwareUtilities.h"

// uncomment to get more logging
//#define kVERBOSE_LOG

OSDefineMetaClassAndStructors(AppleTopazAudio, AudioHardwareObjectInterface)

#define super IOService

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// call into superclass and initialize.
bool AppleTopazAudio::init(OSDictionary *properties)
{
	debugIOLog ( "+ AppleTopazAudio::init\n" );
	if (!super::init(properties))
		return false;
		
	mCurrentMAP = 0;
	mUnlockErrorCount = 0;
	mCurrentMachine1State = kTopazState_Idle;
	mCurrentMachine2State = kMachine2_idleState;
	mUnlockStatus = false;
	mRecoveryInProcess = false;
	
	mClockSource = kTRANSPORT_MASTER_CLOCK;
	
	debugIOLog ( "- AppleTopazAudio::init\n" );
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTopazAudio::start (IOService * provider) {
	bool					result;

	debug3IOLog ("+AppleTopazAudio[%p]::start(%p)\n", this, provider);

	FailIf (!provider, Exit);
	mAudioDeviceProvider = (AppleOnboardAudio *)provider;

	result = super::start (provider);

	result = provider->open (this, kFamilyOption_OpenMultiple);
	
Exit:
	debug4IOLog ("-AppleTopazAudio[%p]::start(%p) returns bool %d\n", this, provider, result);
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// call inherited free
void AppleTopazAudio::free()
{
	debugIOLog ( "+AppleTopazAudio::free\n" );

	super::free();
	debugIOLog ( "-AppleTopazAudio::free\n" );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazAudio::initPlugin(PlatformInterface* inPlatformObject) {

	
	debug2IOLog( "+AppleTopazAudio::initPlugin inPlatformObject = %X\n", (unsigned int)inPlatformObject);
	
	mPlatformInterface = inPlatformObject;

	debugIOLog ( "-AppleTopazAudio::initPlugin\n" );
		
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTopazAudio::willTerminate ( IOService * provider, IOOptionBits options ) {

	bool result = super::willTerminate( provider, options );

	debug4IOLog("AppleTopazAudio::willTerminate(%p) returns %d, mAudioDeviceProvider = %p\n", provider, result, mAudioDeviceProvider);

	if (provider == mAudioDeviceProvider) {
		debugIOLog ("closing our provider\n" );
		provider->close (this);
	}

	debug4IOLog ("- AppleTopazAudio[%p]::willTerminate(%p) returns %s\n", this, provider, result == true ? "true" : "false" );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTopazAudio::requestTerminate ( IOService * provider, IOOptionBits options ) {

	bool result = super::requestTerminate( provider, options );
	debug4IOLog("AppleTopazAudio::requestTerminate(%p) returns %d, mAudioDeviceProvider = %p\n", provider, result, mAudioDeviceProvider);

	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTopazAudio::preDMAEngineInit () {
	IOReturn		err;
	UInt8			data;
	bool			result = false;
	
	//	Initialize the register cache to the current hardware register values by iterating
	//	through read accesses to all registers.  Passing a NULL result pointer will result
	//	in updating the register cache without copying data from the cache to a destination
	//	buffer.
	debugIOLog ( "+AppleTopazAudio::preDMAEngineInit\n" );

	FailIf ( NULL == mPlatformInterface, Exit );
	
	CODEC_Reset ();
	
	mShadowRegs[map_ID_VERSION] = 0;
	err = CODEC_ReadRegister ( map_ID_VERSION, NULL, 1 );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	if ( ( cs8420_id << baID ) == ( mShadowRegs[map_ID_VERSION] & kCS84XX_ID_MASK ) ) {
		debugIOLog ( "Topaz Codec = CS8420\n" );
		mCodecID = kCS8420_CODEC;
	} else if ( ( cs8406_id << baID ) == ( mShadowRegs[map_ID_VERSION] & kCS84XX_ID_MASK ) ) {
		debugIOLog ( "Topaz Codec = CS8406\n" );
		mCodecID = kCS8406_CODEC;
	} else {
		debugIOLog ( "Topaz Codec = UNKNOWN\n" );
		FailIf ( true, Exit );
	}
	for ( UInt32 loopCnt = map_MISC_CNTRL_1; loopCnt <= map_BUFFER_23; loopCnt++ ) {
		if ( map_RX_ERROR != loopCnt && 0x1F != loopCnt ) {												//	avoid hole in register address space
			err = CODEC_ReadRegister ( loopCnt, NULL, 1 );												//	read I2C register into cache only
		}
	}
	
	//	The CS8420 does not require the I2S clocks be running in order to access the I2C registers so
	//	initialization of the registers is not deferred until after an instance of the AudioI2SControl
	//	exists (unlike the TAS3004 for example).
	if ( kCS8406_CODEC == mCodecID ) {
		debug2IOLog ( "kMISC_CNTRL_1_INIT_8406          = %x\n", kMISC_CNTRL_1_INIT_8406 );
	} else if ( kCS8420_CODEC == mCodecID ) {
		debug2IOLog ( "kMISC_CNTRL_1_INIT_8420          = %x\n", kMISC_CNTRL_1_INIT_8420 );
	}
	
	//	Place device into power down state prior to initialization
	err = CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, kCLOCK_SOURCE_CTRL_INIT_STOP );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	data = kCS8420_CODEC == mCodecID ? kMISC_CNTRL_1_INIT_8420 : kMISC_CNTRL_1_INIT_8406 ;
	err = CODEC_WriteRegister ( map_MISC_CNTRL_1, data );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_MISC_CNTRL_2, kMISC_CNTRL_2_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_DATA_FLOW_CTRL, kDATA_FLOW_CTRL_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_SERIAL_INPUT_FMT, kSERIAL_AUDIO_INPUT_FORMAT_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	if ( kCS8420_CODEC == mCodecID ) {
		err = CODEC_WriteRegister ( map_SERIAL_OUTPUT_FMT, kSERIAL_AUDIO_OUTPUT_FORMAT_INIT );
		FailIf ( kIOReturnSuccess != err, Exit );
	
		//	Enable receiver error (i.e. RERR) interrupts
		err = CODEC_WriteRegister ( map_RX_ERROR_MASK, kRX_ERROR_MASK_ENABLE_RERR );
		FailIf ( kIOReturnSuccess != err, Exit );
		
		//	Clear any pending error interrupt
		err = CODEC_ReadRegister ( map_RX_ERROR, NULL, 1 );
	}

	err = CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, kCLOCK_SOURCE_CTRL_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_USER_DATA_BUF_CTRL, ubmBlock << baUBM );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	result = true;
Exit:
	debug2IOLog( "-AppleTopazAudio::preDMAEngineInit err = %X\n", err );

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazAudio::postDMAEngineInit () {

	debugIOLog ( "+ AppleTopazAudio::postDMAEngineInit\n" );
	debugIOLog ( "- AppleTopazAudio::postDMAEngineInit\n" );

	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTopazAudio::getMute () {
	return ( mCurMuteState );	
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTopazAudio::setMute (bool muteState) {
	UInt8			data;
	IOReturn		result;
	
	result = kIOReturnSuccess;
	debug2IOLog ( "+ AppleTopazAudio::setMute ( %d )\n", muteState );

	data = kCS8420_CODEC == mCodecID ? kMISC_CNTRL_1_INIT_8420 : kMISC_CNTRL_1_INIT_8406 ;
	data &= ~( 1 << baMuteAES );
	data |= muteState ? ( muteAES3 << baMuteAES ) : ( normalAES3 << baMuteAES ) ;
	result = CODEC_WriteRegister ( map_MISC_CNTRL_1, data );
	FailIf ( kIOReturnSuccess != result, Exit );

	mCurMuteState = muteState;
	
Exit:
	debug3IOLog ( "-AppleTopazAudio::setMute ( %d ) result %d\n", muteState, result );
	return ( result );	
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Stop the internal clocks and set the transmit output to 0 volts for low 
//	power consumption.  Device register updates are applied only to the
//	device and avoid touching the register cache.
IOReturn AppleTopazAudio::performDeviceSleep () {
	IOReturn			result;
	
	debugIOLog ("+ AppleTopazAudio::performDeviceSleep()\n");

	mShadowRegs[map_DATA_FLOW_CTRL] &= ~( kCS84XX_BIT_MASK << baTXOFF );
	mShadowRegs[map_DATA_FLOW_CTRL] |= ( aes3TX0v << baTXOFF );
	result = CODEC_WriteRegister ( map_DATA_FLOW_CTRL, mShadowRegs[map_DATA_FLOW_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	mShadowRegs[map_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_BIT_MASK << baRUN );
	mShadowRegs[map_CLOCK_SOURCE_CTRL] |= ( runSTOP << baRUN );
	result = CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, mShadowRegs[map_CLOCK_SOURCE_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );

Exit:
	debugIOLog ("- AppleTopazAudio::performDeviceSleep()\n");
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Reset the device and flush the cached register set to the device.  This
//	is necessary for portable CPUs where power will have been removed from
//	the device during sleep.  After flushing the cached register values to
//	the device, place the device into the RUN state.
IOReturn AppleTopazAudio::performDeviceWake () {
	IOReturn			result;
	
	debugIOLog ( "+ AppleTopazAudio::performDeviceWake()\n" );

	CODEC_Reset ();
	for ( UInt32 regAddr = map_MISC_CNTRL_1; regAddr <= map_BUFFER_23; regAddr++ ) {
		if ( kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) ) {
			result = CODEC_WriteRegister ( regAddr, mShadowRegs[regAddr] );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
	
	mShadowRegs[map_DATA_FLOW_CTRL] &= ~( kCS84XX_BIT_MASK << baTXOFF );
	mShadowRegs[map_DATA_FLOW_CTRL] |= ( aes3TXNormal << baTXOFF );
	result = CODEC_WriteRegister ( map_DATA_FLOW_CTRL, mShadowRegs[map_DATA_FLOW_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );

	mShadowRegs[map_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_BIT_MASK << baRUN );
	mShadowRegs[map_CLOCK_SOURCE_CTRL] |= ( runNORMAL << baRUN );
	result = CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, mShadowRegs[map_CLOCK_SOURCE_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );

	//	[3333215]	Codec operation may not be restored after wake from
	//	sleep so start state machine to recover codec operation.
	//	[3344893]	Recovery is only implemented if running on the internal 
	//	clock source since failure is associated with the sample rate converter 
	//	and the sample rate converter is not in use when running on the external 
	//	clock source.
	if ( kTRANSPORT_MASTER_CLOCK == mClockSource ) {
		mCurrentMachine2State = kMachine2_startState;
	}
Exit:
	debugIOLog ( "- AppleTopazAudio::performDeviceWake()\n" );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazAudio::setSampleRate ( UInt32 sampleRate ) {
	IOReturn		result = kIOReturnBadArgument;

	FailIf ( ( 198000 < sampleRate || sampleRate < 30000 ), Exit );
	
	mSampleRate = sampleRate;
	
	//	Avoid general recovery when running on external clock as a reset
	//	will set the clocks back to internal.  Just indicate if the sample
	//	rate is valid so that AppleOnboardAudio's poll method that validates
	//	sample rate changes can operate correctly but leave the hardware alone!
	if ( kTRANSPORT_MASTER_CLOCK == mClockSource ) {
		generalRecovery();
	}
	
	result = CODEC_SetChannelStatus();
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazAudio::setSampleDepth ( UInt32 sampleDepth ) {
	IOReturn		result = kIOReturnBadArgument;
	
	FailIf ( !( 16 == sampleDepth || 24 == sampleDepth ), Exit );
	//	Avoid general recovery when running on external clock as a reset
	//	will set the clocks back to internal.  Just indicate if the bit
	//	depth is valid so that AppleOnboardAudio's poll method that validates
	//	sample rate changes can operate correctly but leave the hardware alone!
	if ( kTRANSPORT_MASTER_CLOCK == mClockSource ) {
		generalRecovery();
	}
	
	mSampleDepth = sampleDepth;
	
	result = CODEC_SetChannelStatus();
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTopazAudio::setSampleType ( UInt32 sampleType ) {
	IOReturn			result;
	
	result = kIOReturnSuccess;
	if ( kIOAudioStreamSampleFormat1937AC3 == sampleType ) {
		mNonAudio = TRUE;
		result = CODEC_SetChannelStatus();
	} else if ( kIOAudioStreamSampleFormatLinearPCM == sampleType ) {
		mNonAudio = FALSE;
		result = CODEC_SetChannelStatus();
	} else {
		result = kIOReturnBadArgument;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	AppleTopazAudio::getClockLock ( void ) {
	return 1;
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
	UInt8				data;
	IOReturn			result;

#ifdef kVERBOSE_LOG	
	debug2IrqIOLog ( "+ AppleTopazAudio::breakClockSelect ( %d )\n", (unsigned int)clockSource );
#endif
	
	if (mAttemptingExternalLock) {
		mAttemptingExternalLock = false;
	}
	
	//	Disable error interrupts during completing clock source selection
	result = CODEC_WriteRegister ( map_RX_ERROR_MASK, kRX_ERROR_MASK_DISABLE_RERR );
	FailIf ( kIOReturnSuccess != result, Exit );

	//	Mute the output port
	data = mShadowRegs[map_MISC_CNTRL_1];
	data &= ~( kCS84XX_BIT_MASK << baMuteAES );
	data |= ( muteAES3 << baMuteAES );
	result = CODEC_WriteRegister ( map_MISC_CNTRL_1, data );
	FailIf ( result != kIOReturnSuccess, Exit );

	//	STOP the codec while switching clocks
	data = mShadowRegs[map_CLOCK_SOURCE_CTRL];
	data &= ~( 1 << baRUN );
	data |= ( runSTOP << baRUN );
	result = CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, data );

	switch ( clockSource ) {
		case kTRANSPORT_MASTER_CLOCK:
			if ( kCS8420_CODEC == mCodecID ) {
				//	Set input data source for SRC to serial audio input port
				data = mShadowRegs[map_DATA_FLOW_CTRL];
				data &= ~( spdMASK << baSPD );
				data |= ( spdSAI << baSPD );
				result = CODEC_WriteRegister ( map_DATA_FLOW_CTRL, data );
				FailIf ( result != kIOReturnSuccess, Exit );
				
				//	Set the input time base to the OMCK input pin
				data = mShadowRegs[map_CLOCK_SOURCE_CTRL];
				data &= ~( kCS84XX_BIT_MASK << baOUTC );
				data |= ( outcOmckXbaCLK << baOUTC );
				result = CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, data );
				FailIf ( result != kIOReturnSuccess, Exit );

				//	Set the input port data format to slave mode
				data = mShadowRegs[map_SERIAL_INPUT_FMT];
				data &= ~( kCS84XX_BIT_MASK << baSIMS );
				data |= ( inputSlave << baSIMS );
				result = CODEC_WriteRegister ( map_SERIAL_INPUT_FMT, data );
				FailIf ( result != kIOReturnSuccess, Exit );
				
				//	Set the output port data format to slave mode
				data = mShadowRegs[map_SERIAL_OUTPUT_FMT];
				data &= ~( kCS84XX_BIT_MASK << baSOMS );
				data |= ( somsSlave << baSOMS );
				result = CODEC_WriteRegister ( map_SERIAL_OUTPUT_FMT, data );
				FailIf ( result != kIOReturnSuccess, Exit );
			}
			break;
		case kTRANSPORT_SLAVE_CLOCK:
			if ( kCS8420_CODEC == mCodecID ) {
				//	Set input data source for SRC to AES3 receiver
				data = mShadowRegs[map_DATA_FLOW_CTRL];
				data &= ~( spdMASK << baSPD );
				data |= ( spdSrcOut << baSPD );
				result = CODEC_WriteRegister ( map_DATA_FLOW_CTRL, data );
				FailIf ( result != kIOReturnSuccess, Exit );

				//	Set the input time base to the OMCK input pin
				data = mShadowRegs[map_CLOCK_SOURCE_CTRL];
				data &= ~( kCS84XX_BIT_MASK << baOUTC );
				data |= ( outcRecIC << baOUTC );
				result = CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, data );
				FailIf ( result != kIOReturnSuccess, Exit );
				
				//	Set the input port data format to slave mode
				data = mShadowRegs[map_SERIAL_INPUT_FMT];
				data &= ~( kCS84XX_BIT_MASK << baSIMS );
				data |= ( inputSlave << baSIMS );
				result = CODEC_WriteRegister ( map_SERIAL_INPUT_FMT, data );
				FailIf ( result != kIOReturnSuccess, Exit );
				
				//	Set the output port data format to master mode
				data = mShadowRegs[map_SERIAL_OUTPUT_FMT];
				data &= ~( kCS84XX_BIT_MASK << baSOMS );
				data |= ( somsMaster << baSOMS );
				result = CODEC_WriteRegister ( map_SERIAL_OUTPUT_FMT, data );
				FailIf ( result != kIOReturnSuccess, Exit );
			}
			break;
		default:
#ifdef kVERBOSE_LOG	
			debugIrqIOLog ( "breakClockSelect clockSource UNKNOWN\n" );
#endif
			result = kIOReturnBadArgument;
			break;
	}
Exit:
	mUnlockErrorCount = 0;
#ifdef kVERBOSE_LOG	
	debug3IrqIOLog ( "- AppleTopazAudio::breakClockSelect ( %d ) returns %d\n", (unsigned int)clockSource, (unsigned int)result );
#endif
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
	IOReturn			result;
	UInt8				data;
	
#ifdef kVERBOSE_LOG	
	debug2IrqIOLog ( "+ AppleTopazAudio::makeClockSelect ( %d )\n", (unsigned int)clockSource );
#endif
	//	Clear any pending error interrupt status and re-enable error interrupts after completing clock source selection
	result = CODEC_ReadRegister ( map_RX_ERROR, &data, 1 );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	//	Enable error (i.e. RERR) interrupts ONLY IF C28420 IS CLOCK MASTER
	if ( ( kTRANSPORT_SLAVE_CLOCK == clockSource ) && ( kCS8420_CODEC == mCodecID ) ) {
		result = CODEC_WriteRegister ( map_RX_ERROR_MASK, kRX_ERROR_MASK_ENABLE_RERR );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	
	switch ( clockSource ) {
		case kTRANSPORT_MASTER_CLOCK:
			data = mShadowRegs[map_DATA_FLOW_CTRL];
			data &= ~( spdMASK << baSPD );
			data |= ( spdSrcOut << baSPD );
			result = CODEC_WriteRegister ( map_DATA_FLOW_CTRL, data );

			data = mShadowRegs[map_CLOCK_SOURCE_CTRL];
			data &= ~( 1 << baOUTC );
			data |= ( outcOmckXbaCLK << baOUTC );
			result = CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, data );
			break;
		case kTRANSPORT_SLAVE_CLOCK:
			data = mShadowRegs[map_DATA_FLOW_CTRL];
			data &= ~( spdMASK << baSPD );
			data |= ( spdAES3 << baSPD );
			result = CODEC_WriteRegister ( map_DATA_FLOW_CTRL, data );

			data = mShadowRegs[map_CLOCK_SOURCE_CTRL];
			data &= ~( 1 << baOUTC );
			data |= ( outcRecIC << baOUTC );
			result = CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, data );
			break;
	}
	
	//	restart the codec after switching clocks
	data = mShadowRegs[map_CLOCK_SOURCE_CTRL];
	data &= ~( 1 << baRUN );
	data |= ( runNORMAL << baRUN );
	result = CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, data );

	if ( ( kTRANSPORT_SLAVE_CLOCK == clockSource ) && ( kCS8420_CODEC == mCodecID ) ) {
		//	It is necessary to restart the I2S cell here after the clocks have been
		//	established using the CS8420 as the clock source.  Ask AOA to restart
		//	the I2S cell.
		FailIf ( NULL == mAudioDeviceProvider, Exit );
		mAudioDeviceProvider->interruptEventHandler ( kRestartTransport, (UInt32)0 );

		// [3253678], set flag to broadcast lock success to AOA so it can unmute analog part
		mAttemptingExternalLock = true;
	}

	//	Unmute the coded output
	data = mShadowRegs[map_MISC_CNTRL_1];
	data &= ~( kCS84XX_BIT_MASK << baMuteAES );
	if ( mCurMuteState ) {
		data |= ( muteAES3 << baMuteAES );
	} else {
		data |= ( normalAES3 << baMuteAES );
	}
	result = CODEC_WriteRegister ( map_MISC_CNTRL_1, data );
	FailIf ( result != kIOReturnSuccess, Exit );
	
	if ( ( kTRANSPORT_MASTER_CLOCK == clockSource ) && ( kCS8420_CODEC == mCodecID ) ) {
		if ( mUnlockStatus ) {
			mUnlockStatus = false;
		}
		mCurrentMachine2State = kMachine2_startState;
	}
	
	mClockSource = clockSource;
	
Exit:
	mUnlockErrorCount = 0;
#ifdef kVERBOSE_LOG	
	debug3IrqIOLog ( "- AppleTopazAudio::makeClockSelect ( %d ) returns %d\n", (unsigned int)clockSource, (unsigned int)result );
#endif
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
	UInt8			data;
	
	//	Stop the device during recovery while preserving the original run state
	data = mShadowRegs[map_CLOCK_SOURCE_CTRL];
	CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, data & ~( 1 << baRUN ) );

	CODEC_Reset ();
	
	for ( UInt32 regAddr = map_MISC_CNTRL_1; regAddr <= map_BUFFER_23; regAddr++ ) {
		if ( kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) ) {
			FailIf ( kIOReturnSuccess != CODEC_WriteRegister ( regAddr, mShadowRegs[regAddr] ), Exit );
		}
	}
	
	//	Restore the original run state
	CODEC_WriteRegister ( map_CLOCK_SOURCE_CTRL, data );
	mCurrentMachine2State = kMachine2_startState;

Exit:
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Fatal error recovery 
#pragma WARNING NEED TO IMPLEMENT RECEIVER IN AOA AND BROADCASTER IN HW PLUGINS
IOReturn AppleTopazAudio::recoverFromFatalError ( FatalRecoverySelector selector ) {

	FailIf ( NULL == mPlatformInterface, Exit );
	if ( mRecoveryInProcess ) { debugIrqIOLog ( "REDUNDANT RECOVERY FROM FATAL ERROR\n" ); }
	
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

UInt32 AppleTopazAudio::getCurrentSampleFrame (void) {
	return mPlatformInterface->getFrameCount ();
}

void AppleTopazAudio::setCurrentSampleFrame (UInt32 value) {
	mPlatformInterface->setFrameCount (value);
}

#pragma mark ---------------------
#pragma mark •	INTERRUPT HANDLERS
#pragma mark ---------------------
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazAudio::notifyHardwareEvent ( UInt32 statusSelector, UInt32 newValue ) {
	IOReturn		err;
	UInt8			data;
	UInt8			saveMAP = mCurrentMAP;
	
	if ( kCodecInterruptStatus == statusSelector ) {
		
	} else if ( kCodecErrorInterruptStatus == statusSelector ) {
		err = CODEC_ReadRegister ( map_RX_ERROR, &data, 1 );
		FailIf ( kIOReturnSuccess != err, Exit );
		//	Only process data bits that have interrupts enabled

#ifdef kVERBOSE_LOG
		debug9IrqIOLog ( "AppleTopazAudio::notifyHardwareEvent ### Receiver Error: 0x%X = %s %s %s %s %s %s %s\n",
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
		if ( 0 != ( data & ( 1 << baUNLOCK ) ) ) {
			if ( kCLOCK_UNLOCK_ERROR_TERMINAL_COUNT > mUnlockErrorCount ) {
				mUnlockErrorCount++;
			}
			if ( kCLOCK_UNLOCK_ERROR_TERMINAL_COUNT == mUnlockErrorCount ) {
				mUnlockStatus = true;
				debugIrqIOLog ( "ERROR RECOVERY: mAudioDeviceProvider->interruptEventHandler ( kClockLockStatus, (UInt32)1 );\n" );
				err = CODEC_WriteRegister ( map_RX_ERROR_MASK, kRX_ERROR_MASK_DISABLE_RERR );
				mAudioDeviceProvider->interruptEventHandler ( kClockLockStatus, (UInt32)1 );
				mUnlockErrorCount = 0;

				mAttemptingExternalLock = false;
				// [3253678], lock failed, don't look for success anymore
			}
		} else {
			mUnlockErrorCount = 0;
			// [3253678], broadcast lock success to AOA so it can unmute analog part
			if (mAttemptingExternalLock) {
				mAudioDeviceProvider->interruptEventHandler ( kClockLockStatus, (UInt32)0 );
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
		if ( ( 0 == ( data & ( 1 << baCONF ) ) ) && ( 0 == ( data & ( 1 << baBIP ) ) ) ) {
			mAES3detected = true;
		} else {
			mAES3detected = false;
		}
	}
Exit:
	if ( mCurrentMAP != saveMAP ) {
		mPlatformInterface->writeCodecRegister( kCS84xx_I2C_ADDRESS, 0, &saveMAP, 1, kI2C_StandardMode);
		mCurrentMAP = saveMAP;
	}
	IOSleep ( 10 );
	return;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazAudio::poll ( void ) {
	stateMachine1 ();
	stateMachine2 ();
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This state machine was moved to a separate method rather than having
//	the state machine execute within the 'poll' method.
void AppleTopazAudio::stateMachine1 ( void ) {
	IOReturn			err;
	
	err = kIOReturnSuccess;
	
	//	This state machine supports radar 3264127 by periodically attempting to 
	//	derive the recovered clock from an AES3 stream.  If no AES3 stream is present
	//	then the derived clock source will revert back to the I2S I/O Module LRCLK.
	//	If the AES3 stream is present then the clock will continue to use the AES3
	//	stream to derive the recovered clock until the AES3 stream is removed.
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
				mShadowRegs[map_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_TWO_BIT_MASK << baRXD );
				mShadowRegs[map_CLOCK_SOURCE_CTRL] |= ( rxd256fsiILRCLK << baRXD );
				CODEC_WriteRegister( map_CLOCK_SOURCE_CTRL, mShadowRegs[map_CLOCK_SOURCE_CTRL] );
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
				mShadowRegs[map_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_TWO_BIT_MASK << baRXD );
				mShadowRegs[map_CLOCK_SOURCE_CTRL] |= ( rxd256fsiAES3 << baRXD );
				CODEC_WriteRegister( map_CLOCK_SOURCE_CTRL, mShadowRegs[map_CLOCK_SOURCE_CTRL] );
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
			mShadowRegs[map_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_TWO_BIT_MASK << baRXD );
			mShadowRegs[map_CLOCK_SOURCE_CTRL] |= ( rxd256fsiILRCLK << baRXD );
			CODEC_WriteRegister( map_CLOCK_SOURCE_CTRL, mShadowRegs[map_CLOCK_SOURCE_CTRL] );
			mCurrentMachine2State = kMachine2_setRxd_AES3;
			break;
		case kMachine2_setRxd_AES3:
			mShadowRegs[map_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_TWO_BIT_MASK << baRXD );
			mShadowRegs[map_CLOCK_SOURCE_CTRL] |= ( rxd256fsiAES3 << baRXD );
			CODEC_WriteRegister( map_CLOCK_SOURCE_CTRL, mShadowRegs[map_CLOCK_SOURCE_CTRL] );
			mCurrentMachine2State = kMachine2_idleState;
			break;
	}
}


#pragma mark ---------------------
#pragma mark • CODEC Functions
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Mask off CS8420 bits that are not supported by the CS8406.
//
UInt8	AppleTopazAudio::CODEC_GetDataMask ( UInt8 regAddr ) {
	UInt8		mask;
	
	mask = kMASK_NONE;
	if ( kCS8406_CODEC == mCodecID ) {
		switch ( regAddr ) {
			case map_MISC_CNTRL_1:					mask = kMISC_CNTRL_1_INIT_8406_MASK;			break;
			case map_MISC_CNTRL_2:					mask = kMISC_CNTRL_2_INIT_8406_MASK;			break;
			case map_DATA_FLOW_CTRL:				mask = kDATA_FLOW_CTR_8406_MASK;				break;
			case map_CLOCK_SOURCE_CTRL:				mask = kCLOCK_SOURCE_CTR_8406_MASK;				break;
			case map_SERIAL_INPUT_FMT:				mask = kSERIAL_AUDIO_INPUT_FORMAT_8406_MASK;	break;
			case map_IRQ1_MASK:						mask = kIRQ1_8406_MASK_8406_MASK;				break;
			case map_IRQ1_MODE_MSB:					mask = kIRQ1_8406_MASK_8406_MASK;				break;
			case map_IRQ1_MODE_LSB:					mask = kIRQ1_8406_MASK_8406_MASK;				break;
			case map_IRQ2_MASK:						mask = kIRQ2_8406_MASK_8406_MASK;				break;
			case map_IRQ2_MODE_MSB:					mask = kIRQ2_8406_MASK_8406_MASK;				break;
			case map_IRQ2_MODE_LSB:					mask = kIRQ2_8406_MASK_8406_MASK;				break;
			case map_CH_STATUS_DATA_BUF_CTRL:		mask = kCH_STATUS_DATA_BUF_CTRL_8406_MASK;		break;
			case map_USER_DATA_BUF_CTRL:			mask = kUSER_DATA_BUF_CTRLL_8406_MASK;			break;
			default:								mask = kMASK_ALL;								break;
		}
	}
	return mask;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazAudio::CODEC_GetRegSize ( UInt32 regAddr, UInt32 * codecRegSizePtr )
{
	IOReturn		result;
	
	result = kIOReturnError;
	if ( NULL != codecRegSizePtr ) {
		if ( map_BUFFER_0 == regAddr ) {
			* codecRegSizePtr = 24;
			result = kIOReturnSuccess;
		} else if ( CODEC_IsControlRegister( (char)regAddr ) || CODEC_IsStatusRegister( (char)regAddr ) ) {
			* codecRegSizePtr = 1;
			result = kIOReturnSuccess;
		} else {
			* codecRegSizePtr = 0;
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTopazAudio::CODEC_IsControlRegister ( UInt8 regAddr ) {
	IOReturn	result;

	switch ( regAddr ) {
		case map_SERIAL_OUTPUT_FMT:
		case map_RX_ERROR_MASK:
			switch ( mCodecID ) {
				case kCS8406_CODEC:					result = kIOReturnError;			break;
				case kCS8420_CODEC:					result = kIOReturnSuccess;			break;
			};
			break;
		case map_MISC_CNTRL_1:
		case map_MISC_CNTRL_2:
		case map_DATA_FLOW_CTRL:
		case map_CLOCK_SOURCE_CTRL:
		case map_SERIAL_INPUT_FMT:
		case map_IRQ1_MASK:
		case map_IRQ1_MODE_MSB:
		case map_IRQ1_MODE_LSB:
		case map_IRQ2_MASK:
		case map_IRQ2_MODE_MSB:
		case map_IRQ2_MODE_LSB:
		case map_CH_STATUS_DATA_BUF_CTRL:
		case map_USER_DATA_BUF_CTRL:
		case map_BUFFER_0:
		case map_BUFFER_1:
		case map_BUFFER_2:
		case map_BUFFER_3:
		case map_BUFFER_4:
		case map_BUFFER_5:
		case map_BUFFER_6:
		case map_BUFFER_7:
		case map_BUFFER_8:
		case map_BUFFER_9:
		case map_BUFFER_10:
		case map_BUFFER_11:
		case map_BUFFER_12:
		case map_BUFFER_13:
		case map_BUFFER_14:
		case map_BUFFER_15:
		case map_BUFFER_16:
		case map_BUFFER_17:
		case map_BUFFER_18:
		case map_BUFFER_19:
		case map_BUFFER_20:
		case map_BUFFER_21:
		case map_BUFFER_22:
		case map_BUFFER_23:							result = kIOReturnSuccess;			break;
		default:									result = kIOReturnError;			break;
	}

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn 	AppleTopazAudio::CODEC_IsStatusRegister ( UInt8 regAddr ) {
	IOReturn	result;
	
	switch ( regAddr ) {
		case map_IRQ1_STATUS:
		case map_IRQ2_STATUS:
		case map_RX_CH_STATUS:
		case map_RX_ERROR:
		case map_ID_VERSION:						result = kIOReturnSuccess;			break;
		case map_Q_CHANNEL_SUBCODE_AC:
		case map_Q_CHANNEL_SUBCODE_TRK:
		case map_Q_CHANNEL_SUBCODE_INDEX:
		case map_Q_CHANNEL_SUBCODE_MIN:
		case map_Q_CHANNEL_SUBCODE_SEC:
		case map_Q_CHANNEL_SUBCODE_FRAME:
		case map_Q_CHANNEL_SUBCODE_ZERO:
		case map_Q_CHANNEL_SUBCODE_ABS_MIN:
		case map_Q_CHANNEL_SUBCODE_ABS_SEC:
		case map_Q_CHANNEL_SUBCODE_ABS_FRAME:
		case map_SAMPLE_RATE_RATIO:
			switch ( mCodecID ) {
				case kCS8406_CODEC:					result = kIOReturnError;			break;
				case kCS8420_CODEC:					result = kIOReturnSuccess;			break;
			};
			break;
		default:									result = kIOReturnError;			break;
	}
	
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Read operations can source data from the device or from a write through
//	cache.  By specifying a 'mode' of kTOPAZ_AccessMode_FORCE_UPDATE_ALL, the device will be
//	accessed.  The size of the access may be passed in.  For most cases, the
//	size will be a single byte.  Channel status or user status may stipulate
//	a size of 24 bytes to optimize the register access.
IOReturn 	AppleTopazAudio::CODEC_ReadRegister ( UInt8 regAddr, UInt8 * registerData, UInt32 size ) {
	IOReturn		result;
	Boolean			success;
	UInt32			index;
	UInt32			codecRegSize;

	FailIf ( NULL == mPlatformInterface, Exit );
	result = kIOReturnSuccess;
	success = false;
	if ( 1 < size ) { regAddr |= (UInt8)kMAP_AUTO_INCREMENT_ENABLE; }
	
	if ( kIOReturnSuccess == CODEC_GetRegSize ( regAddr, &codecRegSize ) ) {
		if ( 0 != size && size <= codecRegSize ) {
			//	Write through to the shadow register as a 'write through' cache would and
			//	then write the data to the hardware;
			if ( kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) || kIOReturnSuccess == CODEC_IsStatusRegister ( regAddr ) ) {
				success = true;
				//	Performance optimization:	If the memory address pointer (MAP) has already
				//								been set then there is no need to set it again.
				//								This is important for interrupt services!!!
				if ( regAddr != mCurrentMAP ) {
					mCurrentMAP = regAddr;
					//	Must write the MAP register prior to performing the READ access
					success = mPlatformInterface->writeCodecRegister( kCS84xx_I2C_ADDRESS, 0, &regAddr, 1, kI2C_StandardMode);
					FailIf ( !success, Exit );
				}
				//	Always read data into the cache.
				success = mPlatformInterface->readCodecRegister(kCS84xx_I2C_ADDRESS, 0, &mShadowRegs[regAddr & ~kMAP_AUTO_INCREMENT_ENABLE], size, kI2C_StandardMode);
				FailIf ( !success, Exit );
				//	Then return data from the cache.
				if ( NULL != registerData && success ) {
					for ( index = 0; index < size; index++ ) {
						registerData[index] = mShadowRegs[(regAddr & ~kMAP_AUTO_INCREMENT_ENABLE) + index];
					}
				}
			} else {
				debug2IrqIOLog ( "not a control or status register at register %X\n", regAddr );
			}
		} else {
			debugIrqIOLog ( "codec register size is invalid\n" );
		}
	}
 
Exit:	
	if ( !success ) { result = kIOReturnError; }
	if ( kIOReturnSuccess != result ) {
#ifdef kVERBOSE_LOG	
		if ( NULL == registerData ) {
			debug6IrqIOLog("-AppleTopazAudio::CODEC_ReadRegister regAddr = %X registerData = %p size = %ul, returns %d\n", regAddr, registerData, (unsigned int)size, result);
		} else {
			debug6IrqIOLog("-AppleTopazAudio::CODEC_ReadRegister regAddr = %X registerData = %X size = %ul, returns %d\n", regAddr, *registerData, (unsigned int)size, result);
		}
#endif
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazAudio::CODEC_Reset ( void ) {

	FailIf ( NULL == mPlatformInterface, Exit );

	mPlatformInterface->setCodecReset ( kCODEC_RESET_Digital, kGPIO_Run );
	IODelay ( 250 );
	mPlatformInterface->setCodecReset ( kCODEC_RESET_Digital, kGPIO_Reset );
	IODelay ( 250 );
	mPlatformInterface->setCodecReset ( kCODEC_RESET_Digital, kGPIO_Run );
	IODelay ( 250 );
Exit:
	return;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazAudio::CODEC_SetChannelStatus ( void ) {
	UInt8				data;
	IOReturn			result;

	//	Assumes consumer mode
	
	data = ( ( kCopyPermited << ( 7 - kBACopyright ) ) | ( kConsumer << ( 7 -  kBAProConsumer ) ) );
	if ( mNonAudio ) {
		data |= ( kConsumerMode_nonAudio << ( 7 - kBANonAudio ) );	//	consumer mode encoded
	} else {
		data |= ( kConsumerMode_audio << ( 7 - kBANonAudio ) );		//	consumer mode linear PCM
	}
	result = CODEC_WriteRegister ( map_BUFFER_0, data );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	if ( mNonAudio ) {
		result = CODEC_WriteRegister ( map_BUFFER_1, 0 );		//	category code is not valid
		FailIf ( kIOReturnSuccess != result, Exit );
		
		result = CODEC_WriteRegister ( map_BUFFER_2, 0 );		//	source & channel are not valid
		FailIf ( kIOReturnSuccess != result, Exit );
			
		result = CODEC_WriteRegister ( map_BUFFER_3, 0 );		//	not valid
		FailIf ( kIOReturnSuccess != result, Exit );
		
		result = CODEC_WriteRegister ( map_BUFFER_4, 0 );		//	not valid
		FailIf ( kIOReturnSuccess != result, Exit );
	} else {
		result = CODEC_WriteRegister ( map_BUFFER_1, 0x01 );	//	category code is CD
		FailIf ( kIOReturnSuccess != result, Exit );
		
		result = CODEC_WriteRegister ( map_BUFFER_2, 0 );		//	source & channel are not specified
		FailIf ( kIOReturnSuccess != result, Exit );
			
		switch ( mSampleRate ) {
			case 32000:		result = CODEC_WriteRegister ( map_BUFFER_3, cSampleFrequency_32Khz );			break;
			case 44100:		result = CODEC_WriteRegister ( map_BUFFER_3, cSampleFrequency_44Khz );			break;
			case 48000:		result = CODEC_WriteRegister ( map_BUFFER_3, cSampleFrequency_48Khz );			break;
			default:		result = CODEC_WriteRegister ( map_BUFFER_3, cSampleFrequency_44Khz );			break;
		}
		FailIf ( kIOReturnSuccess != result, Exit );
		
		switch ( mSampleDepth ) {
			case 16:		result = CODEC_WriteRegister ( map_BUFFER_4, cWordLength_20Max_16bits );			break;
			case 24:		result = CODEC_WriteRegister ( map_BUFFER_4, cWordLength_24Max_24bits );			break;
			default:		result = CODEC_WriteRegister ( map_BUFFER_4, cWordLength_20Max_16bits );			break;
		}
		FailIf ( kIOReturnSuccess != result, Exit );
	}

Exit:
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	All CODEC write operations pass through this function so that a shadow
//	copy of the registers can be kept in global storage.  All CS8420 control
//	registers are one byte in length.
IOReturn 	AppleTopazAudio::CODEC_WriteRegister ( UInt8 regAddr, UInt8 registerData ) {
	IOReturn		result;
	Boolean			updateRequired;
	bool			success = true;

	FailIf ( NULL == mPlatformInterface, Exit );
	result = kIOReturnError;
	updateRequired = false;

#ifdef kVERBOSE_LOG
	debug3IrqIOLog ( "><><>< AppleTopazAudio::CODEC_WriteRegister ( regAddr %X, registerData %X )\n", regAddr, registerData );
#endif

	//	Write through to the shadow register as a 'write through' cache would and
	//	then write the data to the hardware;
	if ( kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) ) {
		registerData &= CODEC_GetDataMask ( regAddr );
		mCurrentMAP = regAddr;
		success = mPlatformInterface->writeCodecRegister( kCS84xx_I2C_ADDRESS, regAddr, &registerData, 1, kI2C_StandardSubMode );
		FailIf ( !success, Exit );
		mShadowRegs[regAddr] = registerData;
	}
	result = kIOReturnSuccess;
	
Exit:
	if ( !success ) { result = kIOReturnError; }
	if ( kIOReturnSuccess != result && !mRecoveryInProcess) {
		debug4IrqIOLog ( "AppleTopazAudio::CODEC_WriteRegister ( regAddr %X, registerData %X ) result = %X\n", regAddr, registerData, result );
		mAudioDeviceProvider->interruptEventHandler ( kRequestCodecRecoveryStatus, (UInt32)kControlBusFatalErrorRecovery );
	}
    return result;
}

#pragma mark ---------------------
#pragma mark • USER CLIENT SUPPORT
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTopazAudio::getPluginState ( HardwarePluginDescriptorPtr outState ) {
	IOReturn		result = kIOReturnBadArgument;
	
	FailIf ( NULL == outState, Exit );
	outState->hardwarePluginType = getPluginType();
	outState->registerCacheSize = sizeof ( mShadowRegs );
	for ( UInt32 registerAddress = 0; registerAddress < outState->registerCacheSize; registerAddress++ ) {
		outState->registerCache[registerAddress] = mShadowRegs[registerAddress];
	}
	outState->recoveryRequest = 0;
	result = kIOReturnSuccess;
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Compares the register cache being passed in via 'inState' to the register
//	cache maintained by the driver.  If a miscompare results then writes the
//	register cache passed in via 'inState' to the target register that has a
//	different value than the current driver maintained register cache.
IOReturn AppleTopazAudio::setPluginState ( HardwarePluginDescriptorPtr inState ) {
	IOReturn		result = kIOReturnBadArgument;
	
	debug2IOLog ( "+ UC AppleTopazAudio::setPluginState ( %p )\n", inState );
	FailIf ( NULL == inState, Exit );
	FailIf ( sizeof ( mShadowRegs ) != inState->registerCacheSize, Exit );
	result = kIOReturnSuccess;
	for ( UInt32 registerAddress = map_MISC_CNTRL_1; ( registerAddress < map_ID_VERSION ) && ( kIOReturnSuccess == result ); registerAddress++ ) {
		if ( inState->registerCache[registerAddress] != mShadowRegs[registerAddress] ) {
			if ( kIOReturnSuccess == CODEC_IsControlRegister ( (UInt8)registerAddress ) ) {
				result = CODEC_WriteRegister ( registerAddress, inState->registerCache[registerAddress] );
			}
		}
	}
Exit:
	debug3IOLog ( "- UC AppleTopazAudio::setPluginState ( %p ) returns %X\n", inState, result );
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
HardwarePluginType	AppleTopazAudio::getPluginType ( void ) {
	HardwarePluginType			result = kCodec_Unknown;
	
	switch ( mCodecID ) {
		case kCS8406_CODEC:					result = kCodec_CS8406;				break;
		case kCS8420_CODEC:					result = kCodec_CS8420;				break;
		default:							result = kCodec_Unknown;			break;
	};
	return result;
}




