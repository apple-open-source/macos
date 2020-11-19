/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
/*
 *  IOFWUserIsochChannel.cpp
 *  IOFireWireFamily
 *
 *  Created by noggin on Tue May 15 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#import <IOKit/firewire/IOFireWireController.h>
#import <IOKit/firewire/IOFWCommand.h>
#import <IOKit/firewire/IOFWLocalIsochPort.h>
#import <IOKit/firewire/IOFWDCLProgram.h>

#import "IOFireWireUserClient.h"
#import "IOFWUserIsochChannel.h"

OSDefineMetaClassAndStructors(IOFWUserIsochChannel, IOFWIsochChannel)

bool IOFWUserIsochChannel::init(	
	IOFireWireController *		control, 
	bool 						doIRM,
	UInt32 						packetSize, 
	IOFWSpeed 					prefSpeed )
{
	DebugLog("IOFWUserIsochChannel<%p>::init - packetSize = %d, doIRM = %d\n", this, packetSize, doIRM );
	
	return super::init( control, doIRM, packetSize, prefSpeed, &IOFWUserIsochChannel::isochChannel_ForceStopHandler, this ) ;
}

IOReturn
IOFWUserIsochChannel::allocateChannel()
{
	// maybe we should call user space lib here?
//	IOLog("IOFWUserIsochChannel::allocateChannel called!\n") ;
	return kIOReturnUnsupported ;
}

IOReturn
IOFWUserIsochChannel::releaseChannel()
{
	// maybe we should call user space lib here?
//	IOLog("IOFWUserIsochChannel::releaseChannel called!\n") ;
	return kIOReturnUnsupported ;
}


IOReturn
IOFWUserIsochChannel::start()
{
	// maybe we should call user space lib here?
//	IOLog("IOFWUserIsochChannel::start called!\n") ;
	return kIOReturnUnsupported ;
}

IOReturn
IOFWUserIsochChannel::stop()
{
	// maybe we should call user space lib here?
//	IOLog("IOFWUserIsochChannel::stop called!\n") ;
	return kIOReturnUnsupported ;
}

IOReturn
IOFWUserIsochChannel::allocateListenerPorts()
{
	IOFWIsochPort*		listen;
	IOReturn			result 			= kIOReturnSuccess ;
	OSIterator*			listenIterator	= OSCollectionIterator::withCollection(fListeners) ;

	if(listenIterator) {
		listenIterator->reset();
		while( (listen = (IOFWIsochPort *) listenIterator->getNextObject()) && (result == kIOReturnSuccess)) {
			result = listen->allocatePort(fSpeed, fChannel);
		}
		listenIterator->release();
	}
	
	return result ;
}

IOReturn
IOFWUserIsochChannel::allocateTalkerPort()
{
	IOReturn	result	= kIOReturnSuccess ;

	if(fTalker)
		result = fTalker->allocatePort(fSpeed, fChannel);
	
	return result ;
}

void
IOFWUserIsochChannel::s_exporterCleanup ( IOFWUserIsochChannel * channel )
{
	DebugLog( "IOFWUserIsochChannel::s_exporterCleanup - channel = %p\n", channel) ;
	
	channel->fControl->removeAllocatedChannel( channel ) ;

	channel->stop() ;
	channel->releaseChannel() ;
}

IOReturn
IOFWUserIsochChannel::isochChannel_ForceStopHandler( void * self, IOFWIsochChannel*, UInt32 stopCondition )
{
	IOFWUserIsochChannel * me = (IOFWUserIsochChannel*)self;
	
#if INFO
	natural_t userProc = me->fStopRefCon ? ((natural_t*)me->fStopRefCon)[ kIOAsyncCalloutFuncIndex ] : 0 ;
	natural_t userRef =  me->fStopRefCon ? ((natural_t*)me->fStopRefCon)[ kIOAsyncCalloutRefconIndex ] : 0 ;

	InfoLog("+ IOFireWireUserClient::s_IsochChannel_ForceStopHandler() -- fStopRefCon=%p, userProc=%p, userRef=0x%x\n", me->fStopRefCon, userProc, userRef ) ;
#endif


	if ( !me->getUserAsyncRef() )
	{
		return kIOReturnSuccess ;
	}
	
	return IOFireWireUserClient::sendAsyncResult64( (io_user_reference_t *)me->getUserAsyncRef(), stopCondition, NULL, 0 ) ;
}
