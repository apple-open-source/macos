/*
 *	TransportInterface.cpp
 *
 *	Interface class for audio data transport
 *
 *  Created by Ray Montagne on Mon Mar 12 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 */

#include "TransportInterface.h"
#include "AudioHardwareUtilities.h"

#define super OSObject

UInt32 TransportInterface::sInstanceCount = 0;

OSDefineMetaClassAndAbstractStructors ( TransportInterface, OSObject );

//	--------------------------------------------------------------------------------
bool TransportInterface::init ( PlatformInterface* inPlatformInterface ) {
	bool	result;
	
	result = super::init ();

	if ( result ) {
		mPlatformObject = inPlatformInterface;
		result = NULL == mPlatformObject ? false : true ;
	}


	TransportInterface::sInstanceCount++;	
	mInstanceIndex = TransportInterface::sInstanceCount;

	return result;
}

//	--------------------------------------------------------------------------------
void TransportInterface::free () {
	return super::free ();
}

//	--------------------------------------------------------------------------------
//	Must be called from the inherited class after successfully setting the
//	audio data transport to the target sample rate.  Should not be called
//	from the inherited class if the target sample rate cannot be set on the
//	audio data transport.
IOReturn	TransportInterface::transportSetSampleRate ( UInt32 sampleRate ) {

	debugIOLog (3,  "± TransportInterface[%ld]::transportSetSampleRate ( %d )", mInstanceIndex, (unsigned int)sampleRate );
	mTransportState.transportSampleRate = sampleRate;
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
//	Must be called from the inherited class after successfully setting the
//	audio data transport to the target sample width.  Should not be called
//	from the inherited class if the target sample width cannot be set on the
//	audio data transport.
IOReturn	TransportInterface::transportSetSampleWidth ( UInt32 sampleDepth, UInt32 dmaWidth ) {
	mTransportState.transportSampleDepth = sampleDepth;
	mTransportState.transportDMAWidth = dmaWidth;
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
//	Must be called from the inherited class after successfully setting the
//	audio data transport to the target sample width.  Should not be called
//	from the inherited class if the target sample width cannot be set on the
//	audio data transport.
IOReturn	TransportInterface::transportBreakClockSelect ( UInt32 clockSource ) {
	mTransportState.clockSource = clockSource;
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
//	Must be called from the inherited class after successfully setting the
//	audio data transport to the target sample width.  Should not be called
//	from the inherited class if the target sample width cannot be set on the
//	audio data transport.
IOReturn	TransportInterface::transportMakeClockSelect ( UInt32 clockSource ) {
	mTransportState.clockSource = clockSource;
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
//	Must be called from the inherited class.
void	TransportInterface::transportSetTransportInterfaceType ( UInt32 transportType ) {
	mTransportState.transportInterfaceType = transportType;
}


#pragma mark #--------------------
#pragma mark # USER CLIENT
#pragma mark #--------------------

//	--------------------------------------------------------------------------------
IOReturn	TransportInterface::getTransportInterfaceState ( TransportStateStructPtr outState ) {
	IOReturn		result = kIOReturnError;
	
	if ( NULL != outState ) {
		((TransportStateStructPtr)outState)->transportInterfaceType = mTransportState.transportInterfaceType;
		((TransportStateStructPtr)outState)->transportSampleRate = transportGetSampleRate ();
		((TransportStateStructPtr)outState)->transportSampleDepth = transportGetSampleWidth ();
		((TransportStateStructPtr)outState)->transportDMAWidth = transportGetDMAWidth ();
		((TransportStateStructPtr)outState)->clockSource = transportGetClockSelect ();
		result = kIOReturnSuccess;
	}
	return result;
}

//	--------------------------------------------------------------------------------
//	NOTE:	Setting of the sample rate, format or clock source is not allowed due
//			to external dependencies on hardware plugin manipulations of codec
//			registers and the dma engine.  These settings can only be changed by
//			invoking the controls made available through core audio.
IOReturn	TransportInterface::setTransportInterfaceState ( TransportStateStructPtr inState ) {
	return kIOReturnError;
}


