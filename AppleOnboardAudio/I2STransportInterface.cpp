/*
 *	I2STransportInterface.cpp
 *
 *	Interface class for audio data transport
 *
 *  Created by Ray Montagne on Mon Mar 12 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 *
 *	This instance of a TransportInterface object supports the I2S I/O module
 *	that is available in KeyLargo, Pangea and the K2 system I/O controllers.
 *
 */
#include "I2STransportInterface.h"
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOTimerEventSource.h>
#include "AudioHardwareUtilities.h"
#include "PlatformInterface.h"

#define super TransportInterface

OSDefineMetaClassAndStructors ( I2STransportInterface, TransportInterface );

#pragma mark #--------------------
#pragma mark # PUBLIC METHODS
#pragma mark #--------------------

//	--------------------------------------------------------------------------------
bool		I2STransportInterface::init ( PlatformInterface * inPlatformInterface ) {
	bool			result;
	IOReturn		error = kIOReturnError;
	
	debug2IOLog ( "+ I2STransportInterface::init ( %d )\n", (unsigned int)inPlatformInterface );
	
	result = super::init ( inPlatformInterface );
	FailIf ( !result, Exit );	
	FailIf ( NULL == mPlatformObject, Exit );
	
	super::transportSetTransportInterfaceType ( kTransportInterfaceType_I2S );	

	requestClockSources ();

	mPlatformObject->setClockMux ( kGPIO_MuxSelectDefault );
	super::transportMakeClockSelect ( kTRANSPORT_MASTER_CLOCK );

	//	SET UP default operating variables.  There are codec dependencies on how the clock 
	//	divide ratios are determined for the I2S audio data transport bus.  THIS WILL NEED 
	//	TO ASK AOA WHAT CODEC IS ATTACHED AT SOME POINT IN THE FUTURE (i.e. when the PCM3052 
	//	controller is used).  FOR NOW, AN ASSUMPTION THAT ALL CODECS REQUIRE THE SAME BEHAVIOR IS MADE.
	
	mMclkMultiplier = 256;		//	TAS3004, CS84xx run MCLK at 256 * fs
	mSclkMultiplier =  64;		//	TAS3004, CS84xx run MCLK at 64 * fs
	
	//	Setup for a 48.000 Khz sample rate with stereo 16 bit channels on both input and output.
	mDataWordSize = ( kDataIn16 << kDataInSizeShift ) | ( kDataOut16 << kDataOutSizeShift );
	mDataWordSize |=( kI2sStereoChannels << kNumChannelsOutShift ) | ( kI2sStereoChannels << kNumChannelsInShift );

	//	Set the I2S cell to a clocks stopped mode so that the clocks
	//	stop low and then tristate so that a new format can be applied.
	mPlatformObject->setI2SIOMIntControl ( 1 << kClocksStoppedPendingShift );	//	Clear the clock stop status
	error = mPlatformObject->setI2SClockEnable ( false );
	FailIf ( kIOReturnSuccess != error, Exit );
	
	waitForClocksStopped ();
	
	mPlatformObject->setI2SSWReset ( true );
	IOSleep ( 10 );
	mPlatformObject->setI2SSWReset ( false );

	error = mPlatformObject->setDataWordSizes ( mDataWordSize );
	FailIf ( kIOReturnSuccess != error, Exit );
	
	error = mPlatformObject->setI2SCellEnable ( true );
	FailIf ( kIOReturnSuccess != error, Exit );
	
	error = mPlatformObject->setI2SEnable ( true );
	FailIf ( kIOReturnSuccess != error, Exit );
	
	error = transportSetSampleRate ( 44100 );
	FailIf ( kIOReturnSuccess != error, Exit );
	
Exit:
	if ( result ) { result = kIOReturnSuccess == error; }
	
	debug3IOLog ( "- I2STransportInterface::init ( %d ) = %d\n", (unsigned int)inPlatformInterface, (unsigned int)result );
	return result;
}


//	--------------------------------------------------------------------------------
void I2STransportInterface::free () {

	releaseClockSources ();	
	super::free();
	
	return;
}

//	--------------------------------------------------------------------------------
//	Maps the sample rate request to register values and register sequences
IOReturn	I2STransportInterface::transportSetSampleRate ( UInt32 sampleRate ) {
	IOReturn	result = kIOReturnError;
	
	debug2IOLog ( "+ I2STransportInterface::transportSetSampleRate ( %d )\n", (unsigned int)sampleRate );
	
	result = calculateSerialFormatRegisterValue ( sampleRate );
	FailIf ( kIOReturnSuccess != result, Exit );
	FailIf ( NULL == mPlatformObject, Exit );
	
	//	Set the I2S cell to a clocks stopped mode so that the clocks
	//	stop low and then tristate so that a new format can be applied.
	mPlatformObject->setI2SIOMIntControl ( 1 << kClocksStoppedPendingShift );	//	Clear the clock stop status
	result = mPlatformObject->setI2SClockEnable ( false );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	waitForClocksStopped ();
	
	result = mPlatformObject->setSerialFormatRegister ( mSerialFormat );
	FailIf ( kIOReturnSuccess != result, Exit );
	debug3IOLog ( "mPlatformObject->setSerialFormatRegiste ( %X ) returns %X\n", (unsigned int)mSerialFormat, (unsigned int)result );
	
	result = mPlatformObject->setDataWordSizes ( mDataWordSize );
	FailIf ( kIOReturnSuccess != result, Exit );
	debug3IOLog ( "mPlatformObject->setDataWordSizes ( %X ) returns %X\n", (unsigned int)mDataWordSize, (unsigned int)result );
	
	
	result = mPlatformObject->setI2SEnable ( true );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = mPlatformObject->setI2SClockEnable ( true );
	FailIf ( kIOReturnSuccess != result, Exit );
		
	result = super::transportSetSampleRate ( sampleRate );
#ifdef kVERBOSE_LOG
	mPlatformObject->LogFCR ();
	mPlatformObject->LogI2S ();
#endif
Exit:	
	debug2IOLog ( "- transportSetSampleRate ( %d )\n", (unsigned int)sampleRate );
	return result;
}

//	--------------------------------------------------------------------------------
//	Sets the data word sizes to configure the data transfer sizes between the
//	DMA engine and the I2S bus.  SampleWidth indicates the width of the sample
//	being conveyed on the I2S audio transport bus while DMA width indicates the
//	width of data being transacted between the I2S I/O Module and the DMA engine.
//
//	Sequence:	1.	Stop I2S Clocks
//				2.	Wait for I2S clocks stopped
//				3.	Set data word sizes
//				4.	Enable I2S Clocks
//
IOReturn	I2STransportInterface::transportSetSampleWidth ( UInt32 sampleDepth, UInt32 dmaWidth ) {
	IOReturn	result = kIOReturnError;
	
	FailIf ( NULL == mPlatformObject, Exit );
	if ( 16 == sampleDepth && 16 == dmaWidth ) {
		mDataWordSize = ( kDataIn16 << kDataInSizeShift ) | ( kDataOut16 << kDataOutSizeShift );
	} else if ( 24 == sampleDepth && 32 == dmaWidth ) {
		mDataWordSize = ( kDataIn24 << kDataInSizeShift ) | ( kDataOut24 << kDataOutSizeShift );
	} else {
		FailIf ( true, Exit );
	}
	mDataWordSize |= ( ( kI2sStereoChannels << kNumChannelsOutShift ) | ( kI2sStereoChannels << kNumChannelsInShift ) );

	//	Set the I2S cell to a clocks stopped mode so that the clocks
	//	stop low and then tristate so a new data word size can be applied
	result = mPlatformObject->setI2SClockEnable ( false );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	waitForClocksStopped ();
	
	result = mPlatformObject->setDataWordSizes ( mDataWordSize );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = mPlatformObject->setI2SEnable ( true );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = mPlatformObject->setI2SClockEnable ( true );
	FailIf ( kIOReturnSuccess != result, Exit );

	result = super::transportSetSampleWidth ( sampleDepth, dmaWidth );

Exit:	
	return result;
}

//	--------------------------------------------------------------------------------
//	Sets the I2S module to sleep state by stopping the clocks, disabling the I2S
//	I/O Module and releasing the clock source.
//
//	Sequence:	1.	Stop I2S Clocks
//				2.	Wait for I2S clocks stopped
//				3.	Disable the I2S I/O Module
//				4.	Release the source clock
//
IOReturn	I2STransportInterface::performTransportSleep ( void ) {
	IOReturn			result = kIOReturnError;

	debugIOLog ( "+ I2STransportInterface::performTransportSleep ()\n" );
	
	FailIf ( NULL == mPlatformObject, Exit );
	
	//	Set the I2S cell to a clocks stopped mode so that the clocks
	//	stop low and then tristate.
	result = mPlatformObject->setI2SClockEnable ( false );
	FailIf ( kIOReturnSuccess != result, Exit );

	waitForClocksStopped ();
	
	result = mPlatformObject->setI2SCellEnable ( false );		
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = mPlatformObject->releaseI2SClockSource ( mClockSelector );

	releaseClockSources ();
Exit:	
	debug2IOLog ( "- I2STransportInterface::performTransportSleep () = %d\n", (unsigned int)result );
	return result;
}

//	--------------------------------------------------------------------------------
//	Sets the I2S module to the active state by stopping the clocks, disabling the
//	I2S I/O Module and releasing the clock source.
//
IOReturn	I2STransportInterface::performTransportWake ( void ) {
	IOReturn			result = kIOReturnError;
	
	debugIOLog ( "+ I2STransportInterface::performTransportWake ()\n" );

	requestClockSources ();
	
	result = mPlatformObject->setDataWordSizes ( mDataWordSize );
	FailIf (kIOReturnSuccess != result, Exit);

	result = mPlatformObject->setI2SCellEnable ( true );		
	FailIf ( kIOReturnSuccess != result, Exit );

	result = transportSetSampleRate ( mTransportState.transportSampleRate );
	FailIf (kIOReturnSuccess != result, Exit);

	debug2IOLog ( "- I2STransportInterface::performTransportWake () = %d\n", (unsigned int)result );

Exit:
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
IOReturn	I2STransportInterface::transportBreakClockSelect ( UInt32 clockSource ) {
	IOReturn	result = kIOReturnError;
	
	debug2IrqIOLog ( "+ I2STransportInterface::transportBreakClockSelect ( %d )\n", (unsigned int)clockSource );
	
	FailIf ( NULL == mPlatformObject, Exit );
	
	//	Update the super so that setting the sample rate will correctly manipulate
	//	the BCLKMaster control bit in the Serial Format Register
	super::transportBreakClockSelect ( clockSource );
	
	result = calculateSerialFormatRegisterValue ( mTransportState.transportSampleRate );
	FailIf ( kIOReturnSuccess != result, Exit );

	IOSleep ( 10 );

	result = mPlatformObject->setSerialFormatRegister ( mSerialFormat );
	FailIf ( kIOReturnSuccess != result, Exit );
	debug3IrqIOLog ( "mPlatformObject->setSerialFormatRegiste ( %X ) returns %X\n", (unsigned int)mSerialFormat, (unsigned int)result );

	result = mPlatformObject->setDataWordSizes ( mDataWordSize );
	FailIf ( kIOReturnSuccess != result, Exit );
	debug3IrqIOLog ( "mPlatformObject->setDataWordSizes ( %X ) returns %X\n", (unsigned int)mDataWordSize, (unsigned int)result );
	
	IOSleep ( 10 );

	//	Set the I2S cell to a clocks stopped mode so that the clocks
	//	stop low and then tristate.  This performs the 'break'!!!
	mPlatformObject->setI2SIOMIntControl ( 1 << kClocksStoppedPendingShift );	//	Clear the clock stop status
	result = mPlatformObject->setI2SClockEnable ( false );
	FailIf ( kIOReturnSuccess != result, Exit );
	waitForClocksStopped ();
	
	IOSleep ( 10 );

	switch ( clockSource ) {
		case kTRANSPORT_MASTER_CLOCK:
			debugIrqIOLog ( "... kTRANSPORT_MASTER_CLOCK requires no action\n" );
			break;
		case kTRANSPORT_SLAVE_CLOCK:
			//	Set the clock mux to the ALTERNATE clock source (i.e. externally generated)
			debugIrqIOLog ( "... setting clock mux to ALTERNATE source\n" );
			result = mPlatformObject->setClockMux ( kGPIO_MuxSelectAlternate );
			FailIf ( kIOReturnSuccess != result, Exit );
			break;
		default:
			FailIf ( true, Exit );
			break;
	}
	
Exit:
#ifdef kVERBOSE_LOG
	mPlatformObject->LogFCR ();
	mPlatformObject->LogI2S ();
#endif
	debug3IrqIOLog ( "- I2STransportInterface::transportBreakClockSelect ( %d ) = %X\n", (unsigned int)clockSource, (unsigned int)result );
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
IOReturn	I2STransportInterface::transportMakeClockSelect ( UInt32 clockSource ) {
	IOReturn	result = kIOReturnError;
	
	debug2IrqIOLog ( "+ I2STransportInterface::transportMakeClockSelect ( %d )\n", (unsigned int)clockSource );
	
	FailIf ( NULL == mPlatformObject, Exit );
	
	switch ( clockSource ) {
		case kTRANSPORT_MASTER_CLOCK:
			//	Set the clock mux to the DEFAULT clock source (i.e. system generated)
			debugIrqIOLog ( "... setting clock mux to DEFAULT source\n" );
			result = mPlatformObject->setClockMux ( kGPIO_MuxSelectDefault );
			FailIf ( kIOReturnSuccess != result, Exit );

			//	Stop the cell and restart the cell to get I2S running
			debugIrqIOLog ( "... setting kBClkMasterShift to MASTER\n" );
			break;
		case kTRANSPORT_SLAVE_CLOCK:
			debugIrqIOLog ( "... kTRANSPORT_SLAVE_CLOCK requires no action\n" );
			result = kIOReturnSuccess;
			break;
		default:
			FailIf ( true, Exit );
			break;
	}

Exit:
	result = mPlatformObject->setI2SClockEnable ( true );
	
#ifdef kVERBOSE_LOG
	mPlatformObject->LogFCR ();
	mPlatformObject->LogI2S ();
#endif
	debug3IrqIOLog ( "- I2STransportInterface::transportMakeClockSelect ( %d ) = %X\n", (unsigned int)clockSource, (unsigned int)result );
	return result;
}


//	--------------------------------------------------------------------------------
UInt32		I2STransportInterface::transportGetSampleRate ( void ) {
	UInt32			curSerialFormat;
	UInt32			result = 0;
	
	if ( kTRANSPORT_SLAVE_CLOCK == mTransportState.clockSource ) {
		curSerialFormat = mPlatformObject->getSerialFormatRegister ();
		curSerialFormat &= 0x00000FFF;
		if ( ( kSampleRate_32Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_32Khz_UpperLimt ) ) {
			result = 32000;
		} else if ( ( kSampleRate_44Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_44Khz_UpperLimt ) ) {
			result = 44100;
		} else if ( ( kSampleRate_48Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_48Khz_UpperLimt ) ) {
			result = 48000;
		} else if ( ( kSampleRate_88Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_88Khz_UpperLimt ) ) {
			result = 88200;
		} else if ( ( kSampleRate_96Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_96Khz_UpperLimt ) ) {
			result = 96000;
		} else if ( ( kSampleRate_192Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_192Khz_UpperLimt ) ) {
			result = 19200;
		} else {
			//	The rate is not in the standard set so return the rate verbatim.  The
			//	number of counts in the least sigificant 12 bits of the serial format
			//	register is the number of 18.4320 Mhz non-synchronized clock cycles that 
			//	were counted during a single sample subframe.  That is 54.250 nanoseconds
			//	per clock.  Since the 18.4320 Mhz clock is not synchronized, these rates
			//	are only approximate so only integer math is being used here to generate
			//	the sample rate approximation.
			curSerialFormat *= 5425;
			curSerialFormat = 100000000 / curSerialFormat;
			result = curSerialFormat * 1000;
		}
	} else {
		result = super::transportGetSampleRate ();
	}
	return result;
}


#pragma mark #--------------------
#pragma mark # PRIVATE METHODS
#pragma mark #--------------------


IOReturn I2STransportInterface::requestClockSources () {
	IOReturn	result = kIOReturnError;

	FailMessage (mHave18MHzClock || mHave45MHzClock || mHave49MHzClock);
	//	Audio needs all three clocks in order to support setting sample 
	//	rates and/or clock sources.
	if (FALSE == mHave45MHzClock) {
		result = mPlatformObject->requestI2SClockSource (kI2S_45MHz);
		if (kIOReturnSuccess != result) {
			IOLog ("I2STransportInterface failed to acquire 45 MHz clock.\n");
		} else {
			mHave45MHzClock = TRUE;
		}
	}
	if (FALSE == mHave49MHzClock) {
		result = mPlatformObject->requestI2SClockSource (kI2S_49MHz);
		if (kIOReturnSuccess != result) {
			IOLog ("I2STransportInterface failed to acquire 49 MHz clock.\n");
		} else {
			mHave49MHzClock = TRUE;
		}
	}
	if (FALSE == mHave18MHzClock) {
		result = mPlatformObject->requestI2SClockSource (kI2S_18MHz);
		if (kIOReturnSuccess != result) {
			IOLog ("I2STransportInterface failed to acquire 18 MHz clock.\n");
		} else {
			mHave18MHzClock = TRUE;
		}
	}
	if (mHave18MHzClock && mHave45MHzClock && mHave49MHzClock) {
		result = kIOReturnSuccess;
	}

	return result;
}


IOReturn I2STransportInterface::releaseClockSources () {
	IOReturn	result = kIOReturnError;

	//	Audio needs all three clocks in order to support setting sample 
	//	rates and/or clock sources.
	
	if (TRUE == mHave45MHzClock) {
		result = mPlatformObject->releaseI2SClockSource (kI2S_45MHz);
		if (kIOReturnSuccess != result) {
			IOLog ("Failed to release 45 MHz clock.\n");
		} else {
			mHave45MHzClock = FALSE;
		}
	}
	if (TRUE == mHave49MHzClock) {
		result = mPlatformObject->releaseI2SClockSource (kI2S_49MHz);
		if (kIOReturnSuccess != result) {
			IOLog ("Failed to release 49 MHz clock.\n");
		} else {
			mHave49MHzClock = FALSE;
		}
	}
	if (TRUE == mHave18MHzClock) {
		result = mPlatformObject->releaseI2SClockSource (kI2S_18MHz);
		if (kIOReturnSuccess != result) {
			IOLog ("Failed to release 18 MHz clock.\n");
		} else {
			mHave18MHzClock = FALSE;
		}
	}

	result = kIOReturnSuccess;

	return result;
}


//	--------------------------------------------------------------------------------
IOReturn		I2STransportInterface::calculateSerialFormatRegisterValue ( UInt32 sampleRate ) {
	IOReturn	result = kIOReturnError;
	
	FailIf ( NULL == mPlatformObject, Exit );
	
	mMClkFrequency = sampleRate * mMclkMultiplier;
	mSClkFrequency = sampleRate * mSclkMultiplier;
	
	//	The sample rate can be derived from 45.1584 MHz, 49.1520 MHz or 18.432 Mhz.
	//	The clock that the sample rate is derived from must be an integral multiple
	//	of the sample rate.  Some common sample rates are setup as follows:
	//
	//	SAMPLE RATE		CLOCK SOURCE	MCLK DIVISOR	SCLK DIVISOR	CODEC		MCLK RATIO
	//
	//	 32.000 Khz		49.1520 Mhz			6				4			TAS3004		256 * fs
	//	 44.100 Khz		45.1584 Mhz			4				4			TAS3004		256 * fs
	//	 48.000 Khz		49.1520 Mhz			4				4			TAS3004		256 * fs
	//	 96.000 Khz		49.1520 Mhz			2				4			PCM3052		256 * fs
	//	192.000 Khz		49.1520 Mhz			1				4			PCM3052		256 * fs
	//
	if ( 0 == ( kClock18MHz % mMClkFrequency ) ) {
		mClockSourceFrequency = kClock18MHz;
		mClockSelector = kI2S_18MHz;
		mSerialFormat = ( kClockSource18MHz << kClockSourceShift );
	} else if ( 0 == ( kClock45MHz % mMClkFrequency ) ) {
		mClockSourceFrequency = kClock45MHz;
		mClockSelector = kI2S_45MHz;
		mSerialFormat = ( kClockSource45MHz << kClockSourceShift );
	} else if ( 0 == ( kClock49MHz % mMClkFrequency ) ) {
		mClockSourceFrequency = kClock49MHz;
		mClockSelector = kI2S_49MHz;
		mSerialFormat = ( kClockSource49MHz << kClockSourceShift );
	} else {
		//	The sample rate cannot be derived from available clock sources
		FailIf ( true, Exit );
	}

	//	The BCLkMaster bit is set according to the clock source (i.e. system mastered
	//	clock or externally mastered clock such as a recovered clock from a S/PDIF AES3 stream.
	if ( kTRANSPORT_MASTER_CLOCK == mTransportState.clockSource ) {
		mSerialFormat |= ( kSClkMaster << kBClkMasterShift );
	} else if ( kTRANSPORT_SLAVE_CLOCK == mTransportState.clockSource ) {
		mSerialFormat |= ( kSClkSlave << kBClkMasterShift );
	} else {
		FailIf ( true, Exit );
	}
	//	The I2S I/O Module has exceptions for register configurations based on the clock 
	//	divisor which must be resolved to determine register confuration MCLK values.
	mMClkDivisor = mClockSourceFrequency / mMClkFrequency;
	switch ( mMClkDivisor ) {
		case 1:			mMClkDivider = 0x00000014;									break;
		case 3:			mMClkDivider = 0x00000013;									break;
		case 5:			mMClkDivider = 0x00000012;									break;
		default:		mMClkDivider = ( ( mMClkDivisor / 2 ) - 1 );				break;
	}
	mSerialFormat |= ( mMClkDivider << kMClkDivisorShift );
	//	The I2S I/O Module has exceptions for register configurations based on the clock 
	//	divisor which must be resolved to determine register confuration SCLK values.
	mSClkDivisor = ( mClockSourceFrequency / mMClkDivisor ) / mSClkFrequency;
	switch ( mSClkDivisor ) {
		case 1:			mSClkDivider = 0x00000008;									break;
		case 3:			mSClkDivider = 0x00000009;									break;
		default:		mSClkDivider = ( ( mSClkDivisor / 2 ) - 1 );				break;
	}
	mSerialFormat |= ( mSClkDivider << kSClkDivisorShift );
	switch ( mSclkMultiplier ) {
		case 32:	mSerialFormat |= ( kSerialFormat32x << kSerialFormatShift );	break;
		case 64:	mSerialFormat |= ( kSerialFormat64x << kSerialFormatShift );	break;
		default:	FailIf ( true, Exit );											break;
	}
	result = kIOReturnSuccess;
Exit:	
	return result;
}


//	--------------------------------------------------------------------------------
void		I2STransportInterface::waitForClocksStopped ( void ) {
	UInt32		timeOut = 100;
	
	while ( ( ( mPlatformObject->getI2SIOMIntControl () & ( 1 << kClocksStoppedPendingShift ) ) == 0 ) && timeOut ) {
		IODelay ( 10 );
		timeOut--;
	}
	mPlatformObject->setI2SIOMIntControl ( 1 << kClocksStoppedPendingShift );	//	Clear the clock stop status
}


#pragma mark #--------------------
#pragma mark # USER CLIENT
#pragma mark #--------------------

//	--------------------------------------------------------------------------------
IOReturn	I2STransportInterface::getTransportInterfaceState ( TransportStateStructPtr outState ) {
	IOReturn		result;
	
	result = super::getTransportInterfaceState ( outState );
	if ( NULL != outState && kIOReturnSuccess == result ) {
		((TransportStateStructPtr)outState)->instanceState[0] = mMclkMultiplier;
		((TransportStateStructPtr)outState)->instanceState[1] = mSclkMultiplier;
		((TransportStateStructPtr)outState)->instanceState[2] = mMClkFrequency;
		((TransportStateStructPtr)outState)->instanceState[3] = mSClkFrequency;
		((TransportStateStructPtr)outState)->instanceState[4] = mClockSourceFrequency;
		((TransportStateStructPtr)outState)->instanceState[5] = mMClkDivisor;
		((TransportStateStructPtr)outState)->instanceState[6] = mSClkDivisor;
		((TransportStateStructPtr)outState)->instanceState[7] = mMClkDivider;
		((TransportStateStructPtr)outState)->instanceState[8] = mSClkDivider;
		((TransportStateStructPtr)outState)->instanceState[9] = mSerialFormat;
		((TransportStateStructPtr)outState)->instanceState[10] = mDataWordSize;
		((TransportStateStructPtr)outState)->instanceState[11] = mClockSelector;
		((TransportStateStructPtr)outState)->instanceState[12] = (UInt32)mHave45MHzClock;
		((TransportStateStructPtr)outState)->instanceState[13] = (UInt32)mHave49MHzClock;
		((TransportStateStructPtr)outState)->instanceState[14] = (UInt32)mHave18MHzClock;
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	I2STransportInterface::setTransportInterfaceState ( TransportStateStructPtr inState ) {
	return super::setTransportInterfaceState ( inState );
}



