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
 *  IOFWUserClientIsoch.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Mon Mar 12 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFWLocalIsochPort.h>
#include <IOKit/firewire/IOFWIsochChannel.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFireWireController.h>

#include "IOFireWireUserClient.h"
#include "IOFWUserIsochPort.h"
#include "IOFWUserIsochChannel.h"

void
IOFireWireUserClient::initIsochMethodTable()
{
	//
	// --- isoch methods ----------
	//
	fMethods[kFWIsochPort_Allocate].object	= this ;
	fMethods[kFWIsochPort_Allocate].func 	= (IOMethod) & IOFireWireUserClient::isochPortAllocate ;
	fMethods[kFWIsochPort_Allocate].count0	= sizeof(FWIsochPortAllocateParams) ;
	fMethods[kFWIsochPort_Allocate].count1	= sizeof(FWKernIsochPortRef) ;
	fMethods[kFWIsochPort_Allocate].flags	= kIOUCStructIStructO ;

	fMethods[kFWIsochPort_Release].object	= this ;
	fMethods[kFWIsochPort_Release].func		= (IOMethod) & IOFireWireUserClient::isochPortRelease ;
	fMethods[kFWIsochPort_Release].count0	= 1 ;
	fMethods[kFWIsochPort_Release].count1	= 0 ;
	fMethods[kFWIsochPort_Release].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kFWIsochPort_GetSupported].object	= this ;
	fMethods[kFWIsochPort_GetSupported].func	= (IOMethod) & IOFireWireUserClient::isochPortGetSupported ;
	fMethods[kFWIsochPort_GetSupported].count0	= 1 ;
	fMethods[kFWIsochPort_GetSupported].count1	= 3 ;
	fMethods[kFWIsochPort_GetSupported].flags	= kIOUCScalarIScalarO ;

	fMethods[kFWIsochPort_AllocatePort].object	= this ;
	fMethods[kFWIsochPort_AllocatePort].func	= (IOMethod) & IOFireWireUserClient::isochPortAllocatePort ;
	fMethods[kFWIsochPort_AllocatePort].count0	= 3 ;
	fMethods[kFWIsochPort_AllocatePort].count1	= 0 ;
	fMethods[kFWIsochPort_AllocatePort].flags	= kIOUCScalarIScalarO ;

	fMethods[kFWIsochPort_ReleasePort].object	= this ;
	fMethods[kFWIsochPort_ReleasePort].func		= (IOMethod) & IOFireWireUserClient::isochPortReleasePort ;
	fMethods[kFWIsochPort_ReleasePort].count0	= 1 ;
	fMethods[kFWIsochPort_ReleasePort].count1	= 0 ;
	fMethods[kFWIsochPort_ReleasePort].flags	= kIOUCScalarIScalarO ;

	fMethods[kFWIsochPort_Start].object	= this ;
	fMethods[kFWIsochPort_Start].func	= (IOMethod) & IOFireWireUserClient::isochPortStart ;
	fMethods[kFWIsochPort_Start].count0	= 1 ;
	fMethods[kFWIsochPort_Start].count1	= 0 ;
	fMethods[kFWIsochPort_Start].flags	= kIOUCScalarIScalarO ;

	fMethods[kFWIsochPort_Stop].object	= this ;
	fMethods[kFWIsochPort_Stop].func	= (IOMethod) & IOFireWireUserClient::isochPortStop ;
	fMethods[kFWIsochPort_Stop].count0	= 1 ;
	fMethods[kFWIsochPort_Stop].count1	= 0 ;
	fMethods[kFWIsochPort_Stop].flags	= kIOUCScalarIScalarO ;

	// --- local isoch port methods ----------------------
	fMethods[kFWLocalIsochPort_Allocate].object			= this ;
	fMethods[kFWLocalIsochPort_Allocate].func			= (IOMethod) & IOFireWireUserClient::localIsochPortAllocate ;
	fMethods[kFWLocalIsochPort_Allocate].count0			= sizeof(FWLocalIsochPortAllocateParams) ;
	fMethods[kFWLocalIsochPort_Allocate].count1			= sizeof(FWKernIsochPortRef) ;
	fMethods[kFWLocalIsochPort_Allocate].flags			= kIOUCStructIStructO ;

	fMethods[kFWLocalIsochPort_ModifyJumpDCL].object	= this ;
	fMethods[kFWLocalIsochPort_ModifyJumpDCL].func	= (IOMethod) & IOFireWireUserClient::localIsochPortModifyJumpDCL ;
	fMethods[kFWLocalIsochPort_ModifyJumpDCL].count0	= 3 ;
	fMethods[kFWLocalIsochPort_ModifyJumpDCL].count1	= 0 ;
	fMethods[kFWLocalIsochPort_ModifyJumpDCL].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kFWLocalIsochPort_ModifyTransferPacketDCLSize].object	= this ;
	fMethods[kFWLocalIsochPort_ModifyTransferPacketDCLSize].func	= (IOMethod) & IOFireWireUserClient::localIsochPortModifyJumpDCLSize ;
	fMethods[kFWLocalIsochPort_ModifyTransferPacketDCLSize].count0	= 3 ;
	fMethods[kFWLocalIsochPort_ModifyTransferPacketDCLSize].count1	= 0 ;
	fMethods[kFWLocalIsochPort_ModifyTransferPacketDCLSize].flags	= kIOUCScalarIScalarO ;

	// --- isoch channel methods -------------------------
	fMethods[kFWIsochChannel_Allocate].object	= this ;
	fMethods[kFWIsochChannel_Allocate].func		= (IOMethod) & IOFireWireUserClient::isochChannelAllocate ;
	fMethods[kFWIsochChannel_Allocate].count0	= 3 ;
	fMethods[kFWIsochChannel_Allocate].count1	= 1 ;
	fMethods[kFWIsochChannel_Allocate].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kFWIsochChannel_Release].object	= this ;
	fMethods[kFWIsochChannel_Release].func		= (IOMethod) & IOFireWireUserClient::isochChannelRelease ;
	fMethods[kFWIsochChannel_Release].count0	= 1 ;
	fMethods[kFWIsochChannel_Release].count1	= 0 ;
	fMethods[kFWIsochChannel_Release].flags		= kIOUCScalarIScalarO ;

	fMethods[kFWIsochChannel_UserAllocateChannelBegin].object	= this ;
	fMethods[kFWIsochChannel_UserAllocateChannelBegin].func		= (IOMethod) & IOFireWireUserClient::isochChannelUserAllocateChannelBegin ;
	fMethods[kFWIsochChannel_UserAllocateChannelBegin].count0	= 4 ;
	fMethods[kFWIsochChannel_UserAllocateChannelBegin].count1	= 2 ;
	fMethods[kFWIsochChannel_UserAllocateChannelBegin].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kFWIsochChannel_UserReleaseChannelComplete].object	= this ;
	fMethods[kFWIsochChannel_UserReleaseChannelComplete].func	= (IOMethod) & IOFireWireUserClient::isochChannelUserReleaseChannelComplete ;
	fMethods[kFWIsochChannel_UserReleaseChannelComplete].count0	= 1 ;
	fMethods[kFWIsochChannel_UserReleaseChannelComplete].count1	= 0 ;
	fMethods[kFWIsochChannel_UserReleaseChannelComplete].flags	= kIOUCScalarIScalarO ;
}

void
IOFireWireUserClient::initIsochAsyncMethodTable()
{
	fAsyncMethods[kFWSetAsyncRef_IsochChannelForceStop].object	= this ;
	fAsyncMethods[kFWSetAsyncRef_IsochChannelForceStop].func	= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_IsochChannelForceStop ;
	fAsyncMethods[kFWSetAsyncRef_IsochChannelForceStop].count0	= 3 ;
	fAsyncMethods[kFWSetAsyncRef_IsochChannelForceStop].count1	= 0 ;
	fAsyncMethods[kFWSetAsyncRef_IsochChannelForceStop].flags	= kIOUCScalarIScalarO ;

	fAsyncMethods[kFWSetAsyncRef_DCLCallProc].object			= this ;
	fAsyncMethods[kFWSetAsyncRef_DCLCallProc].func				= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_DCLCallProc ;
	fAsyncMethods[kFWSetAsyncRef_DCLCallProc].count0			= 2 ;
	fAsyncMethods[kFWSetAsyncRef_DCLCallProc].count1			= 0 ;
	fAsyncMethods[kFWSetAsyncRef_DCLCallProc].flags				= kIOUCScalarIScalarO ;
}
	
//
// isoch port
//

IOReturn
IOFireWireUserClient::isochPortAllocate(
	FWIsochPortAllocateParams*		inParams,
	FWKernIsochPortRef*				outPortRef)
{
	IOFWUserIsochPortProxy*	newPort	= new IOFWUserIsochPortProxy() ;
	
	if (!newPort)
	{
		IOLog("%s %u: newPort==nil!\n", __FILE__, __LINE__) ;
		return kIOReturnNoMemory ;
	}
	if (!newPort->init(this))
	{
		IOLog("IOFireWireUserClient::isochPortAllocate: port init failed\n") ;
		newPort->release() ;
		
		return kIOReturnInternalError ;
	}

	IOReturn	result = addObjectToSet( newPort, fUserIsochPorts ) ;
	
	// pass new port out to user
	*outPortRef		= newPort ;

	return result ;
}

IOReturn
IOFireWireUserClient::isochPortRelease(
	FWKernIsochPortRef		inPortRef)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;
	
	removeObjectFromSet( inPortRef, fUserIsochPorts ) ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::isochPortGetSupported(
	FWKernIsochPortRef		inPortRef,
	IOFWSpeed*				outMaxSpeed,
	UInt32*					outChanSupportedHi,
	UInt32*					outChanSupportedLo)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;
		
	UInt64		chanSupported ;
	IOReturn	result			= kIOReturnSuccess ;

	result = inPortRef->getSupported(*outMaxSpeed, chanSupported) ;

	*outChanSupportedHi = (UInt32)(chanSupported >> 32) ;
	*outChanSupportedLo	= (UInt32)(chanSupported & 0xFFFFFFFF) ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::isochPortAllocatePort(
	FWKernIsochPortRef		inPortRef,
	IOFWSpeed				inSpeed,
	UInt32					inChannel)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;

	return inPortRef->allocatePort(inSpeed, inChannel) ;
}

IOReturn
IOFireWireUserClient::isochPortReleasePort(
	FWKernIsochPortRef		inPortRef)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;

	return inPortRef->releasePort() ;
}

IOReturn
IOFireWireUserClient::isochPortStart(
	FWKernIsochPortRef		inPortRef)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;

	return inPortRef->start() ;
}

IOReturn
IOFireWireUserClient::isochPortStop(
	FWKernIsochPortRef		inPortRef)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;

	return inPortRef->stop() ;
}

IOReturn
IOFireWireUserClient::localIsochPortAllocate(
	FWLocalIsochPortAllocateParams*	inParams,
	FWKernIsochPortRef*				outPortRef)
{
	IOReturn						result		= kIOReturnSuccess ;
	IOFWUserLocalIsochPortProxy*	newPort 	= new IOFWUserLocalIsochPortProxy ;
	if (!newPort)
	{
		IOLog("%s %u: newPort == nil!\n", __FILE__, __LINE__) ;
		return kIOReturnNoMemory ;
	}

	if (!newPort->initWithUserDCLProgram(inParams, this))
	{
		IOLog("%s %u: newPort->initWithUserDCLProgram failed!\n", __FILE__, __LINE__) ;
		newPort->release() ;
		result = kIOReturnError ;
	}
	else
	{
		result = addObjectToSet( newPort, fUserIsochPorts ) ;
		
		// pass new port out to user
		*outPortRef = newPort ;
	}

	return result ;
}

IOReturn
IOFireWireUserClient::localIsochPortModifyJumpDCL(
	FWKernIsochPortRef		inPortRef,
	UInt32					inJumpDCLCompilerData,
	UInt32					inLabelDCLCompilerData)
{
	IOFWUserLocalIsochPortProxy*	portProxy ;
	if (NULL == (portProxy = OSDynamicCast(IOFWUserLocalIsochPortProxy, inPortRef)) )
		return kIOReturnBadArgument ;
	
	return portProxy->modifyJumpDCL(inJumpDCLCompilerData, inLabelDCLCompilerData) ;
}

IOReturn
IOFireWireUserClient::localIsochPortModifyJumpDCLSize( FWKernIsochPortRef inPortRef, UInt32 dclCompilerData,
		IOByteCount newSize )
{
	IOFWUserLocalIsochPortProxy*	portProxy ;
	if (NULL == (portProxy = OSDynamicCast( IOFWUserLocalIsochPortProxy, inPortRef)) )
		return kIOReturnBadArgument ;
	
	return portProxy->modifyJumpDCLSize( dclCompilerData, newSize ) ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_DCLCallProc(
	OSAsyncReference		asyncRef,
	FWKernIsochPortRef		inPortRef,
	DCLCallCommandProcPtr	inProc )
{
	IOFWUserLocalIsochPortProxy*	port = OSDynamicCast(IOFWUserLocalIsochPortProxy, inPortRef) ;
	if (!port)
		return kIOReturnBadArgument ;
	
	return port->setAsyncRef_DCLCallProc(asyncRef, inProc ) ;
}

//
// --- isoch channel ----------
//
IOReturn
IOFireWireUserClient::isochChannelForceStopHandler(
	void*					refCon,
	IOFWIsochChannel*		isochChannelID,
	UInt32					stopCondition)
{
	return kIOReturnSuccess ;
}
 
IOReturn
IOFireWireUserClient::isochChannelAllocate(
	bool					inDoIRM,
	UInt32					inPacketSize,
	IOFWSpeed				inPrefSpeed,
	FWKernIsochChannelRef*	outIsochChannelRef)
{
	IOReturn	result		= kIOReturnSuccess ;

	// this code the same as IOFireWireController::createIsochChannel
	// must update this code when controller changes. We do this because
	// we are making IOFWUserIsochChannel objects, not IOFWIsochChannel
	// objects
	IOFWUserIsochChannel*	newChannel	= new IOFWUserIsochChannel ;
	if (!newChannel)
	{
		IOLog("%s %u: couldn't make newChannel == nil!\n", __FILE__, __LINE__) ;
		result = kIOReturnNoMemory ;
	}	
	if (kIOReturnSuccess == result)
		if (!newChannel->init(fOwner->getController(), inDoIRM, inPacketSize, inPrefSpeed, & IOFireWireUserClient::isochChannelForceStopHandler, this))
		{
			newChannel->release() ;
			newChannel = NULL ;
			result = kIOReturnError ;
		}

	if (kIOReturnSuccess == result)
	{
		result = addObjectToSet( newChannel, fUserIsochChannels ) ;
	}
	
	*outIsochChannelRef = newChannel ;
	return result ;
}

IOReturn
IOFireWireUserClient::isochChannelRelease(
	FWKernIsochChannelRef	inChannelRef)
{
	if (!OSDynamicCast(IOFWUserIsochChannel, inChannelRef))
		return kIOReturnBadArgument ;
	
//	IOLockLock(fSetLock) ;
//	fUserIsochChannels->removeObject(inChannelRef) ;
//	IOLockUnlock(fSetLock) ;
	removeObjectFromSet( inChannelRef, fUserIsochChannels ) ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::isochChannelUserAllocateChannelBegin(
	FWKernIsochChannelRef	inChannelRef,
	IOFWSpeed				inSpeed,
	UInt32					inAllowedChansHi,
	UInt32					inAllowedChansLo,
	IOFWSpeed*				outSpeed,
	UInt32*					outChannel)
{
	if (!OSDynamicCast(IOFWUserIsochChannel, inChannelRef))
		return kIOReturnBadArgument ;
	
	return inChannelRef->userAllocateChannelBegin(inSpeed, inAllowedChansHi, inAllowedChansLo, outSpeed, outChannel) ;
}

IOReturn
IOFireWireUserClient::isochChannelUserReleaseChannelComplete(
	FWKernIsochChannelRef	inChannelRef)
{
	if (!OSDynamicCast(IOFWUserIsochChannel, inChannelRef))
		return kIOReturnBadArgument ;
	
	return inChannelRef->userReleaseChannelComplete() ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_IsochChannelForceStop(
	OSAsyncReference		asyncRef,
	void*					inCallback,
	void*					inUserRefCon,
	void*,
	void*,
	void*,
	void*)
{
	return kIOReturnUnsupported ;
}
