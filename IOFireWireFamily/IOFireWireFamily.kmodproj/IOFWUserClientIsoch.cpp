/*
* Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
* 
* Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
* 
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this
* file.
* 
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
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
	fMethods[kIsochPort_Allocate].object	= this ;
	fMethods[kIsochPort_Allocate].func 	= (IOMethod) & IOFireWireUserClient::isochPortAllocate ;
	fMethods[kIsochPort_Allocate].count0	= sizeof(IsochPortAllocateParams) ;
	fMethods[kIsochPort_Allocate].count1	= sizeof(KernIsochPortRef) ;
	fMethods[kIsochPort_Allocate].flags	= kIOUCStructIStructO ;

	fMethods[kIsochPort_Release].object	= this ;
	fMethods[kIsochPort_Release].func		= (IOMethod) & IOFireWireUserClient::isochPortRelease ;
	fMethods[kIsochPort_Release].count0	= 1 ;
	fMethods[kIsochPort_Release].count1	= 0 ;
	fMethods[kIsochPort_Release].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kIsochPort_GetSupported].object	= this ;
	fMethods[kIsochPort_GetSupported].func	= (IOMethod) & IOFireWireUserClient::isochPortGetSupported ;
	fMethods[kIsochPort_GetSupported].count0	= 1 ;
	fMethods[kIsochPort_GetSupported].count1	= 3 ;
	fMethods[kIsochPort_GetSupported].flags	= kIOUCScalarIScalarO ;

	fMethods[kIsochPort_AllocatePort].object	= this ;
	fMethods[kIsochPort_AllocatePort].func	= (IOMethod) & IOFireWireUserClient::isochPortAllocatePort ;
	fMethods[kIsochPort_AllocatePort].count0	= 3 ;
	fMethods[kIsochPort_AllocatePort].count1	= 0 ;
	fMethods[kIsochPort_AllocatePort].flags	= kIOUCScalarIScalarO ;

	fMethods[kIsochPort_ReleasePort].object	= this ;
	fMethods[kIsochPort_ReleasePort].func		= (IOMethod) & IOFireWireUserClient::isochPortReleasePort ;
	fMethods[kIsochPort_ReleasePort].count0	= 1 ;
	fMethods[kIsochPort_ReleasePort].count1	= 0 ;
	fMethods[kIsochPort_ReleasePort].flags	= kIOUCScalarIScalarO ;

	fMethods[kIsochPort_Start].object	= this ;
	fMethods[kIsochPort_Start].func	= (IOMethod) & IOFireWireUserClient::isochPortStart ;
	fMethods[kIsochPort_Start].count0	= 1 ;
	fMethods[kIsochPort_Start].count1	= 0 ;
	fMethods[kIsochPort_Start].flags	= kIOUCScalarIScalarO ;

	fMethods[kIsochPort_Stop].object	= this ;
	fMethods[kIsochPort_Stop].func	= (IOMethod) & IOFireWireUserClient::isochPortStop ;
	fMethods[kIsochPort_Stop].count0	= 1 ;
	fMethods[kIsochPort_Stop].count1	= 0 ;
	fMethods[kIsochPort_Stop].flags	= kIOUCScalarIScalarO ;

	// --- local isoch port methods ----------------------
	fMethods[kLocalIsochPort_Allocate].object			= this ;
	fMethods[kLocalIsochPort_Allocate].func			= (IOMethod) & IOFireWireUserClient::localIsochPortAllocate ;
	fMethods[kLocalIsochPort_Allocate].count0			= sizeof(LocalIsochPortAllocateParams) ;
	fMethods[kLocalIsochPort_Allocate].count1			= sizeof(KernIsochPortRef) ;
	fMethods[kLocalIsochPort_Allocate].flags			= kIOUCStructIStructO ;

	fMethods[kLocalIsochPort_ModifyJumpDCL].object	= this ;
	fMethods[kLocalIsochPort_ModifyJumpDCL].func	= (IOMethod) & IOFireWireUserClient::localIsochPortModifyJumpDCL ;
	fMethods[kLocalIsochPort_ModifyJumpDCL].count0	= 3 ;
	fMethods[kLocalIsochPort_ModifyJumpDCL].count1	= 0 ;
	fMethods[kLocalIsochPort_ModifyJumpDCL].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kLocalIsochPort_ModifyTransferPacketDCLSize].object	= this ;
	fMethods[kLocalIsochPort_ModifyTransferPacketDCLSize].func	= (IOMethod) & IOFireWireUserClient::localIsochPortModifyJumpDCLSize ;
	fMethods[kLocalIsochPort_ModifyTransferPacketDCLSize].count0	= 3 ;
	fMethods[kLocalIsochPort_ModifyTransferPacketDCLSize].count1	= 0 ;
	fMethods[kLocalIsochPort_ModifyTransferPacketDCLSize].flags	= kIOUCScalarIScalarO ;

	// --- isoch channel methods -------------------------
	fMethods[kIsochChannel_Allocate].object	= this ;
	fMethods[kIsochChannel_Allocate].func		= (IOMethod) & IOFireWireUserClient::isochChannelAllocate ;
	fMethods[kIsochChannel_Allocate].count0	= 3 ;
	fMethods[kIsochChannel_Allocate].count1	= 1 ;
	fMethods[kIsochChannel_Allocate].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kIsochChannel_Release].object	= this ;
	fMethods[kIsochChannel_Release].func		= (IOMethod) & IOFireWireUserClient::isochChannelRelease ;
	fMethods[kIsochChannel_Release].count0	= 1 ;
	fMethods[kIsochChannel_Release].count1	= 0 ;
	fMethods[kIsochChannel_Release].flags		= kIOUCScalarIScalarO ;

	fMethods[kIsochChannel_UserAllocateChannelBegin].object	= this ;
	fMethods[kIsochChannel_UserAllocateChannelBegin].func		= (IOMethod) & IOFireWireUserClient::isochChannelUserAllocateChannelBegin ;
	fMethods[kIsochChannel_UserAllocateChannelBegin].count0	= 4 ;
	fMethods[kIsochChannel_UserAllocateChannelBegin].count1	= 2 ;
	fMethods[kIsochChannel_UserAllocateChannelBegin].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kIsochChannel_UserReleaseChannelComplete].object	= this ;
	fMethods[kIsochChannel_UserReleaseChannelComplete].func	= (IOMethod) & IOFireWireUserClient::isochChannelUserReleaseChannelComplete ;
	fMethods[kIsochChannel_UserReleaseChannelComplete].count0	= 1 ;
	fMethods[kIsochChannel_UserReleaseChannelComplete].count1	= 0 ;
	fMethods[kIsochChannel_UserReleaseChannelComplete].flags	= kIOUCScalarIScalarO ;
}

void
IOFireWireUserClient::initIsochAsyncMethodTable()
{
	fAsyncMethods[kSetAsyncRef_IsochChannelForceStop].object	= this ;
	fAsyncMethods[kSetAsyncRef_IsochChannelForceStop].func	= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_IsochChannelForceStop ;
	fAsyncMethods[kSetAsyncRef_IsochChannelForceStop].count0	= 3 ;
	fAsyncMethods[kSetAsyncRef_IsochChannelForceStop].count1	= 0 ;
	fAsyncMethods[kSetAsyncRef_IsochChannelForceStop].flags	= kIOUCScalarIScalarO ;

	fAsyncMethods[kSetAsyncRef_DCLCallProc].object			= this ;
	fAsyncMethods[kSetAsyncRef_DCLCallProc].func				= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_DCLCallProc ;
	fAsyncMethods[kSetAsyncRef_DCLCallProc].count0			= 2 ;
	fAsyncMethods[kSetAsyncRef_DCLCallProc].count1			= 0 ;
	fAsyncMethods[kSetAsyncRef_DCLCallProc].flags				= kIOUCScalarIScalarO ;
}
	
//
// isoch port
//

IOReturn
IOFireWireUserClient::isochPortAllocate(
	IsochPortAllocateParams*		inParams,
	KernIsochPortRef*				outPortRef)
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
	KernIsochPortRef		inPortRef)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;
	
	removeObjectFromSet( inPortRef, fUserIsochPorts ) ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::isochPortGetSupported(
	KernIsochPortRef		inPortRef,
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
	KernIsochPortRef		inPortRef,
	IOFWSpeed				inSpeed,
	UInt32					inChannel)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;

	return inPortRef->allocatePort(inSpeed, inChannel) ;
}

IOReturn
IOFireWireUserClient::isochPortReleasePort(
	KernIsochPortRef		inPortRef)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;

	return inPortRef->releasePort() ;
}

IOReturn
IOFireWireUserClient::isochPortStart(
	KernIsochPortRef		inPortRef)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;

	return inPortRef->start() ;
}

IOReturn
IOFireWireUserClient::isochPortStop(
	KernIsochPortRef		inPortRef)
{
	if (!OSDynamicCast(IOFWUserIsochPortProxy, inPortRef))
		return kIOReturnBadArgument ;

	return inPortRef->stop() ;
}

IOReturn
IOFireWireUserClient::localIsochPortAllocate(
	LocalIsochPortAllocateParams*	inParams,
	KernIsochPortRef*				outPortRef)
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
	KernIsochPortRef		inPortRef,
	UInt32					inJumpDCLCompilerData,
	UInt32					inLabelDCLCompilerData)
{
	IOFWUserLocalIsochPortProxy*	portProxy ;
	if (NULL == (portProxy = OSDynamicCast(IOFWUserLocalIsochPortProxy, inPortRef)) )
		return kIOReturnBadArgument ;
	
	return portProxy->modifyJumpDCL(inJumpDCLCompilerData, inLabelDCLCompilerData) ;
}

IOReturn
IOFireWireUserClient::localIsochPortModifyJumpDCLSize( KernIsochPortRef inPortRef, UInt32 dclCompilerData,
	IOByteCount newSize )
{
	IOFWUserLocalIsochPortProxy*	portProxy ;
	if (NULL == (portProxy = OSDynamicCast( IOFWUserLocalIsochPortProxy, inPortRef)) )
		return kIOReturnBadArgument ;
	
	return portProxy->modifyJumpDCLSize( dclCompilerData, newSize ) ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_DCLCallProc( OSAsyncReference asyncRef, KernIsochPortRef inPortRef, 
	DCLCallCommandProc* inProc )
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
	KernIsochChannelRef*	outIsochChannelRef)
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
	KernIsochChannelRef	inChannelRef)
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
	KernIsochChannelRef	inChannelRef,
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
	KernIsochChannelRef	inChannelRef)
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
