/*
 *  I2SOpaqueSlaveOnlyTransportInterface.cpp
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Friday 14 May 2004.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *
 *  The I2SOpaqueSlaveOnlyTransportInterface object is subclassed from the
 *  I2STransportInterface object but restricts operation of the I2S
 *  I/O Module to slave only mode.  This is intended for support of
 *  I2S devices such as the Crystal Semiconductor CS8416 where the
 *  CS8416 operates ONLY as a clock master.  
 *
 *  This object assumes that no external clock mux GPIO controll is
 *  associated with the I2S I/O Module for which this object is supporting.
 */

#include "I2SOpaqueSlaveOnlyTransportInterface.h"
//#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOTimerEventSource.h>
#include "AudioHardwareUtilities.h"
#include "PlatformInterface.h"

#define super I2STransportInterface

OSDefineMetaClassAndStructors ( I2SOpaqueSlaveOnlyTransportInterface, I2STransportInterface );


#pragma mark #--------------------
#pragma mark # PUBLIC METHODS
#pragma mark #--------------------

//	--------------------------------------------------------------------------------
bool		I2SOpaqueSlaveOnlyTransportInterface::init ( PlatformInterface * inPlatformInterface ) {
	bool			success;
	IOReturn		result = kIOReturnError;
	
	debugIOLog (3,  "+ I2SOpaqueSlaveOnlyTransportInterface[%p]::init ( %d )", this, (unsigned int)inPlatformInterface );

	//  NOTE:   'init' of the super class will result in configuring the I2S I/O Module as a bus master.
	//			This configuration must be changed to a bus slave here.
	success = super::init ( inPlatformInterface );
	FailIf ( !success, Exit );
	
	super::transportSetTransportInterfaceType ( kTransportInterfaceType_I2S_Opaque_Slave_Only );
	
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
void I2SOpaqueSlaveOnlyTransportInterface::free () {
	IOReturn		err;

	err = super::transportBreakClockSelect ( kTRANSPORT_MASTER_CLOCK );
	FailMessage ( kIOReturnSuccess != err );
	
	err = super::transportMakeClockSelect ( kTRANSPORT_MASTER_CLOCK );
	FailMessage ( kIOReturnSuccess != err );

	super::free();
	return;
}

//	--------------------------------------------------------------------------------
//	The sample rate must be managed in the context of a slave only device.  The goal
//  here is to provide an MCLK frequency that is appropriate for a CODEC running at
//  256 * fs where the CODEC will generate the SCLK and LRCLK based on the MCLK frequency.
//  Any CPU requiring this transport interface subclass will need to specify an
//  <XML> dictionary entry with a <key>TransporObject</key> with a value of
//  <string>i2sOpaqueSlaveOnly</string>.
//  
IOReturn	I2SOpaqueSlaveOnlyTransportInterface::transportSetSampleRate ( UInt32 sampleRate ) {
	IOReturn		result;
	
	result = super::transportSetSampleRate ( sampleRate );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	I2SOpaqueSlaveOnlyTransportInterface::transportSetSampleWidth ( UInt32 sampleWidth, UInt32 dmaWidth ) {
	IOReturn		result;
	
	result = super::transportSetSampleWidth ( sampleWidth, dmaWidth );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	I2SOpaqueSlaveOnlyTransportInterface::performTransportSleep ( void ) {
	IOReturn		result;
	
	result = super::performTransportSleep ();
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	I2SOpaqueSlaveOnlyTransportInterface::performTransportWake ( void ) {
	IOReturn		result;
	
	result = super::performTransportWake ();
	result = super::transportBreakClockSelect ( kTRANSPORT_SLAVE_CLOCK );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = super::transportMakeClockSelect ( kTRANSPORT_SLAVE_CLOCK );
	FailIf ( kIOReturnSuccess != result, Exit );

Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	I2SOpaqueSlaveOnlyTransportInterface::transportBreakClockSelect ( UInt32 clockSource ) {
	return super::transportBreakClockSelect ( clockSource );
}

//	--------------------------------------------------------------------------------
IOReturn	I2SOpaqueSlaveOnlyTransportInterface::transportMakeClockSelect ( UInt32 clockSource ) {
	return super::transportMakeClockSelect ( clockSource );
}

//	--------------------------------------------------------------------------------
//	The transport sample rate should be available from the hardware at all times
//	when operating as a slave only transport interface.  If a zero value sample
//	rate is obtained from the hardware then the hardware has not completed initialization.
//	To avoid error conditions (i.e. zero value sample rate setting) from being processed
//	at higher levels, the current sample rate setting is returned when the hardware is
//	not yet ready.
UInt32		I2SOpaqueSlaveOnlyTransportInterface::transportGetSampleRate ( void ) {
	UInt32		result;
	
	result = super::transportGetSampleRate ();
	if ( 0 == result ) { result = TransportInterface::transportGetSampleRate (); }
	return result;
}


