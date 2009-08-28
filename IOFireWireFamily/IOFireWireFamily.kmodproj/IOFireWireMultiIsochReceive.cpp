/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@ 
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireMultiIsochReceive.h>
///////////////////////////////////////////////////////////////////////////////////
//
// Definition of objects used by the Multi-Isoch Receiver
//
///////////////////////////////////////////////////////////////////////////////////
OSDefineMetaClassAndStructors(IOFireWireMultiIsochReceiveListener, OSObject)
OSDefineMetaClassAndStructors(IOFireWireMultiIsochReceivePacket, OSObject)

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceiveListener::init
///////////////////////////////////////////////////////////////////////////////////
bool IOFireWireMultiIsochReceiveListener::init(IOFireWireController *fwController,
											   UInt32 receiveChannel,
											   FWMultiIsochReceiveListenerCallback callback,
											   void *pCallbackRefCon,
											   FWMultiIsochReceiveListenerParams *pListenerParams)
{
	bool success = true;
	
	//IOLog( "IOFireWireMultiIsochReceiveListener::init (0x%08X)\n",(int) this);
	
	// init super
    if( !OSObject::init() )
        success = false;
	
	if( success )
	{
		// Initialize this object
		fControl = fwController;
		fChannel = receiveChannel;
		fClientCallback = callback;
		fClientCallbackRefCon = pCallbackRefCon;
		fActivated = false;
		
		if (pListenerParams)
		{
			fListenerParams = new FWMultiIsochReceiveListenerParams;
			if (fListenerParams)
			{
				// copy over the parameters specified by the client.
				fListenerParams->maxLatencyInFireWireCycles = pListenerParams->maxLatencyInFireWireCycles;
				fListenerParams->expectedStreamBitRate = pListenerParams->expectedStreamBitRate;
				fListenerParams->clientPacketReturnLatencyInFireWireCycles = pListenerParams->clientPacketReturnLatencyInFireWireCycles;
			}
		}
		else
			fListenerParams = NULL;
	}
	
	return success;
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceiveListener::free
///////////////////////////////////////////////////////////////////////////////////
void IOFireWireMultiIsochReceiveListener::free()
{
	if (fListenerParams)
	{
		delete fListenerParams;
		fListenerParams = NULL;
	}
	
	OSObject::free();
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceiveListener::create
///////////////////////////////////////////////////////////////////////////////////
IOFireWireMultiIsochReceiveListener * IOFireWireMultiIsochReceiveListener::create(IOFireWireController *fwController,
																				  UInt32 channel,
																				  FWMultiIsochReceiveListenerCallback callback,
																				  void *pCallbackRefCon,
																				  FWMultiIsochReceiveListenerParams *pListenerParams)
{
	IOFireWireMultiIsochReceiveListener * listener;
	
	listener = OSTypeAlloc( IOFireWireMultiIsochReceiveListener );
	
    if( listener != NULL && !listener->init(fwController,
											channel,
											callback,
											pCallbackRefCon,
											pListenerParams))
	{
        listener->release();
        listener = NULL;
    }
	
    return listener;
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceiveListener::Activate
///////////////////////////////////////////////////////////////////////////////////
IOReturn IOFireWireMultiIsochReceiveListener::Activate()
{
	return fControl->activateMultiIsochReceiveListener(this);
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceiveListener::Deactivate
///////////////////////////////////////////////////////////////////////////////////
IOReturn IOFireWireMultiIsochReceiveListener::Deactivate()
{
	return fControl->deactivateMultiIsochReceiveListener(this);
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceiveListener::SetCallback
///////////////////////////////////////////////////////////////////////////////////
IOReturn IOFireWireMultiIsochReceiveListener::SetCallback(FWMultiIsochReceiveListenerCallback callback,
														  void *pCallbackRefCon)
{
	if (!fActivated)
	{
		fClientCallback = callback;
		fClientCallbackRefCon = pCallbackRefCon;
		return kIOReturnSuccess;
	}
	else
		return kIOReturnNotPermitted;
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceivePacket::init
///////////////////////////////////////////////////////////////////////////////////
bool IOFireWireMultiIsochReceivePacket::init(IOFireWireController *fwController)
{
	bool success = true;
	
	//IOLog( "IOFireWireMultiIsochReceivePacket::init (0x%08X)\n",(int) this);
	
	// init super
    if( !OSObject::init() )
        success = false;
	
	if( success )
	{
		// Initialize this object
		fControl = fwController;
		numRanges = 0;
		numClientReferences = 0;
	}
	
	return success;
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceivePacket::free
///////////////////////////////////////////////////////////////////////////////////
void IOFireWireMultiIsochReceivePacket::free()
{
	OSObject::free();
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceivePacket::create
///////////////////////////////////////////////////////////////////////////////////
IOFireWireMultiIsochReceivePacket * IOFireWireMultiIsochReceivePacket::create( IOFireWireController *fwController )
{
	IOFireWireMultiIsochReceivePacket * packet;
	
	packet = OSTypeAlloc( IOFireWireMultiIsochReceivePacket );
	
    if( packet != NULL && !packet->init(fwController))
	{
        packet->release();
        packet = NULL;
    }
	
    return packet;
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceivePacket::clientDone
///////////////////////////////////////////////////////////////////////////////////
void IOFireWireMultiIsochReceivePacket::clientDone( void )
{
	fControl->clientDoneWithMultiIsochReceivePacket(this);
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceivePacket::isochChannel
///////////////////////////////////////////////////////////////////////////////////
UInt32 
IOFireWireMultiIsochReceivePacket::isochChannel(void)
{
	// The isoch channel number is in the first quad,
	// which is guarranted to be in the first range.
	
	UInt32 *pIsochPacketHeader = (UInt32*)  ranges[0].address;
	UInt32 isochPacketHeader = OSSwapLittleToHostInt32(*pIsochPacketHeader);
	return ((isochPacketHeader & 0x00003F00) >> 8);
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceivePacket::isochPayloadSize
///////////////////////////////////////////////////////////////////////////////////
UInt32 
IOFireWireMultiIsochReceivePacket::isochPayloadSize(void)
{
	// The isoch payload size is in the first quad,
	// which is guarranted to be in the first range.
	
	UInt32 *pIsochPacketHeader = (UInt32*)  ranges[0].address;
	UInt32 isochPacketHeader = OSSwapLittleToHostInt32(*pIsochPacketHeader);
	return ((isochPacketHeader & 0xFFFF0000) >> 16);
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceivePacket::packetReceiveTime
///////////////////////////////////////////////////////////////////////////////////
UInt32 
IOFireWireMultiIsochReceivePacket::packetReceiveTime(void)
{
	// The isoch timestamp is in the last quad,
	// which is guarranted to be in the last range.
	
	UInt32 *pIsochPacketTrailer = (UInt32*) ranges[numRanges-1].address;
	pIsochPacketTrailer += ((ranges[numRanges-1].length/4)-1);	// Bump to the last quadlet in the range
	UInt32 isochPacketTrailer = OSSwapLittleToHostInt32(*pIsochPacketTrailer);
	
	// We shift the 16-bit time-stamp into the normal 7:13:12 FireWire cycle-time format!
	return ((isochPacketTrailer & 0x0000FFFF) << 12);
}

///////////////////////////////////////////////////////////////////////////////////
// IOFireWireMultiIsochReceivePacket::createMemoryDescriptorForRanges
///////////////////////////////////////////////////////////////////////////////////
IOMemoryDescriptor*
IOFireWireMultiIsochReceivePacket::createMemoryDescriptorForRanges(void)
{
	IOMemoryDescriptor * bufferDesc = NULL ;
	IOReturn error;
	
	bufferDesc = IOMemoryDescriptor::withAddressRanges (ranges, numRanges, kIODirectionOut, kernel_task) ;
	if ( ! bufferDesc )
	{
		error = kIOReturnNoMemory ;
	}
	else
	{
		error = bufferDesc->prepare() ;
		if (error != kIOReturnSuccess)
		{
			bufferDesc->release();
			bufferDesc = NULL;
		}
	}
	
	return bufferDesc;
}
