/*
 *  I2SSlaveOnlyTranportInterface.cpp
 *  AppleOnboardAudio
 *
 *  Created by R Montagne on Mon Dec 01 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *
 *  The I2SSlaveOnlyTransportInterface object is subclassed from the
 *  I2STransportInterface object but restricts operation of the I2S
 *  I/O Module to slave only mode.  This is intended for support of
 *  I2S devices such as the Crystal Semiconductor CS8416 where the
 *  CS8416 operates ONLY as a clock master.  
 *
 *  This object assumes that no external clock mux GPIO controll is
 *  associated with the I2S I/O Module for which this object is supporting.
 */

#include "I2SSlaveOnlyTranportInterface.h"
//#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOTimerEventSource.h>
#include "AudioHardwareUtilities.h"
#include "PlatformInterface.h"

#define super I2STransportInterface

OSDefineMetaClassAndStructors ( I2SSlaveOnlyTransportInterface, I2STransportInterface );


#pragma mark #--------------------
#pragma mark # PUBLIC METHODS
#pragma mark #--------------------

//	--------------------------------------------------------------------------------
bool		I2SSlaveOnlyTransportInterface::init ( PlatformInterface * inPlatformInterface ) {
	bool			success;
	IOReturn		result = kIOReturnError;
	
	debugIOLog (3,  "+ I2SSlaveOnlyTransportInterface[%p]::init ( %d )", this, (unsigned int)inPlatformInterface );

	//  NOTE:   'init' of the super class will result in configuring the I2S I/O Module as a bus master.
	//			This configuration must be changed to a bus slave here.
	success = super::init ( inPlatformInterface );
	FailIf ( !success, Exit );
	
	super::transportSetTransportInterfaceType ( kTransportInterfaceType_I2S_Slave_Only );
	
	result = super::transportSetSampleRate ( 44100 );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = super::transportBreakClockSelect ( kTRANSPORT_SLAVE_CLOCK );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = super::transportMakeClockSelect ( kTRANSPORT_SLAVE_CLOCK );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	success = true;
Exit:
	
	debugIOLog (3,  "- I2STransportInterface[%p (%ld)]::init ( %d ) = %d", this, mInstanceIndex, (unsigned int)inPlatformInterface, (unsigned int)success );
	return success;
}


//	--------------------------------------------------------------------------------
void I2SSlaveOnlyTransportInterface::free () {
	IOReturn		err;

	err = super::transportBreakClockSelect ( kTRANSPORT_MASTER_CLOCK );
	FailMessage ( kIOReturnSuccess != err );
	
	err = super::transportMakeClockSelect ( kTRANSPORT_MASTER_CLOCK );
	FailMessage ( kIOReturnSuccess != err );

	super::free();
	return;
}

//	--------------------------------------------------------------------------------
//	The sample rate cannot be set when the target I2S I/O Module is run as a slave.
//  
IOReturn	I2SSlaveOnlyTransportInterface::transportSetSampleRate ( UInt32 sampleRate ) {
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
IOReturn	I2SSlaveOnlyTransportInterface::transportSetSampleWidth ( UInt32 sampleWidth, UInt32 dmaWidth ) {
	IOReturn		result;
	
	result = super::transportSetSampleWidth ( sampleWidth, dmaWidth );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	I2SSlaveOnlyTransportInterface::performTransportSleep ( void ) {
	IOReturn		result;
	
	result = super::performTransportSleep ();
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	I2SSlaveOnlyTransportInterface::performTransportWake ( void ) {
	IOReturn		result;
	
	result = mPlatformObject->setSerialFormatRegister ( mSerialFormat );
	FailIf ( kIOReturnSuccess != result, Exit );

	result = super::performTransportWake ();
Exit:
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn	I2SSlaveOnlyTransportInterface::performTransportPostWake ( void ) {
	IOReturn		result;
	result = super::transportBreakClockSelect ( kTRANSPORT_SLAVE_CLOCK );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = super::transportMakeClockSelect ( kTRANSPORT_SLAVE_CLOCK );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = mPlatformObject->setI2SClockEnable ( true );
	FailIf ( kIOReturnSuccess != result, Exit );
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	I2SSlaveOnlyTransportInterface::transportBreakClockSelect ( UInt32 clockSource ) {
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
IOReturn	I2SSlaveOnlyTransportInterface::transportMakeClockSelect ( UInt32 clockSource ) {
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
//	The transport sample rate should be available from the hardware at all times
//	when operating as a slave only transport interface.  If a zero value sample
//	rate is obtained from the hardware then the hardware has not completed initialization.
//	To avoid error conditions (i.e. zero value sample rate setting) from being processed
//	at higher levels, the current sample rate setting is returned when the hardware is
//	not yet ready.
UInt32		I2SSlaveOnlyTransportInterface::transportGetSampleRate ( void ) {
	UInt32		result;
	
	result = super::transportGetSampleRate ();
	if ( 0 == result ) { result = TransportInterface::transportGetSampleRate (); }
	return result;
}


