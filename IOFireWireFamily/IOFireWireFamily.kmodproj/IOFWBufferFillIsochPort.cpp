/*
 *  IOFWBufferFillIsochPort.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Mon Sep 09 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * $Log: IOFWBufferFillIsochPort.cpp,v $
 * Revision 1.2  2003/07/21 06:52:58  niels
 * merge isoch to TOT
 *
 * Revision 1.1.2.2  2003/07/18 00:17:41  niels
 * *** empty log message ***
 *
 * Revision 1.1.2.1  2003/07/01 20:54:06  niels
 * isoch merge
 *
 */

#import "IOFWBufferFillIsochPort.h"

#undef super
#define super IOFWIsochPort

bool
IOFWBufferFillIsochPort :: init( 
	IOFireWireBus &			bus, 
	IOByteCount				expectedBytesPerSecond, 
	UInt32					interruptMicroseconds, 
	PacketProc				packetProc,
	OSObject *				target )
{
	if ( ! super::init() )
	{
		return false ;
	}
	
	fBytesPerSecond = expectedBytesPerSecond ;
	fIntUSec = interruptMicroseconds ;
	fPacketProc = packetProc ;
	fTarget = target ;
	
	return true ;
}


// Return maximum speed and channels supported
// (bit n set = chan n supported)
IOReturn
IOFWBufferFillIsochPort :: getSupported (
	IOFWSpeed &			maxSpeed, 
	UInt64 &			chanSupported)
{
    maxSpeed = kFWSpeedMaximum;
    chanSupported = ~(UInt64)0;
    return kIOReturnSuccess;
}
