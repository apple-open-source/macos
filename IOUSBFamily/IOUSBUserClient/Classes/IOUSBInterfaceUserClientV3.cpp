/*
 * Copyright © 1997-2011 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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


//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include "IOUSBInterfaceUserClientV3.h"

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUSBInterfaceUserClientV3

//================================================================================================
//
//   IOKit Constructors and Destructors
//
//================================================================================================
//
OSDefineMetaClassAndStructors( IOUSBInterfaceUserClientV3, IOUSBInterfaceUserClientV2 )

#pragma mark IOUserClient
//================================================================================================
//
//   IOUSBInterfaceUserClientV3 Methods
//
//================================================================================================

const IOExternalMethodDispatch
IOUSBInterfaceUserClientV3::sV3Methods[kIOUSBLibInterfaceUserClientV3NumCommands] = {
	{ //    kUSBInterfaceUserClientOpen
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_open,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientClose
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_close,
		0, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetDevice
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetDevice,
		0, 0,
		1, 0
    },
    { //    kUSBInterfaceUserClientSetAlternateInterface
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_SetAlternateInterface,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetFrameNumber
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetFrameNumber,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
    { //    kUSBInterfaceUserClientGetPipeProperties
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetPipeProperties,
		1, 0,
		5, 0
    },
    { //    kUSBInterfaceUserClientReadPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_ReadPipe,
		5, 0,
		0, 0xffffffff
    },
    { //    kUSBInterfaceUserClientWritePipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_WritePipe,
		5, 0xffffffff,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetPipeStatus
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetPipeStatus,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientAbortPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_AbortPipe,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientResetPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_ResetPipe,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientClearPipeStall
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_ClearPipeStall,
		2, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientControlRequestOut
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_ControlRequestOut,
		9, 0xffffffff,
		0, 0
    },
    { //    kUSBInterfaceUserClientControlRequestIn
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_ControlRequestIn,
		9, 0,
		1, 0xffffffff
    },
    { //    kUSBInterfaceuserClientSetPipePolicy
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_SetPipePolicy,
		3, 0,
		0, 0
    },
    { //    kUSBInterfaceuserClientGetBandwidthAvailable
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetBandwidthAvailable,
		0, 0,
		1, 0
    },
    { //    kUSBInterfaceuserClientGetEndpointProperties
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetEndpointProperties,
		3, 0,
		3, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyPrepareBuffer
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_LowLatencyPrepareBuffer,
		7, 0,
		1, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyReleaseBuffer
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_LowLatencyReleaseBuffer,
		7, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetMicroFrameNumber
        (IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetMicroFrameNumber,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
    { //    kUSBInterfaceUserClientGetFrameListTime
        (IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetFrameListTime,
		0, 0,
		1, 0
    },
	{ //    kUSBInterfaceUserClientGetFrameNumberWithTime
        (IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetFrameNumberWithTime,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
	{ //    kUSBInterfaceUserClientSetAsyncPort
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_SetAsyncPort,
		0, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientReadIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_ReadIsochPipe,
		6, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientWriteIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_WriteIsochPipe,
		6, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyReadIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_LowLatencyReadIsochPipe,
		9, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyWriteIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_LowLatencyWriteIsochPipe,
		9, 0,
		0, 0
    },
    {	//    kUSBInterfaceUserClientGetConfigDescriptor
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetConfigDescriptor,
		1, 0,
		0, 0xffffffff
    },
    {	 //    kUSBInterfaceUserClientGetPipePropertiesV2
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetPipePropertiesV2,
		1, 0,
		8, 0
    },
    {	 //    kUSBInterfaceUserClientGetPipePropertiesV3
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetPipePropertiesV3,
		2, 0,
		0, 0xffffffff
    },
    {	 //    kUSBInterfaceUserClientGetEndpointPropertiesV3
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_GetEndpointPropertiesV3,
		4, 0,
		0, 0xffffffff
    },
	
	// IOUSBUserClientV3 methods from here on
	
   { //    kUSBInterfaceUserClientSupportsStreams
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_supportsStreams,
		1, 0,
		1, 0
    },
    { //    kUSBInterfaceUserClientCreateStreams
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_createStreams,
		2, 0,
		0, 0
    },
	{
		//kUSBInterfaceUserClientGetConfiguredStreams
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_getConfiguredStreams,
		1, 0,
		1, 0
	},
    { //    kUSBInterfaceUserClientReadStreamsPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_ReadStreamsPipe,
		6, 0,
		0, 0xffffffff
    },
    { //    kUSBInterfaceUserClientWriteStreamsPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_WriteStreamsPipe,
		6, 0xffffffff,
		0, 0
    },
    { //    kUSBInterfaceUserClientAbortStreamsPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV3::_AbortStreamsPipe,
		2, 0,
		0, 0
    }
};


#pragma mark -

IOReturn
IOUSBInterfaceUserClientV3::externalMethod(
													uint32_t                    selector,
													IOExternalMethodArguments * arguments,
													IOExternalMethodDispatch *  dispatch,
													OSObject *                  target,
													void *                      reference)
{
	
    if (selector < (uint32_t) kIOUSBLibInterfaceUserClientV3NumCommands)
    {
        dispatch = (IOExternalMethodDispatch *) &sV3Methods[selector];
        
        if (!target)
            target = this;
    }
	
	return IOUserClient::externalMethod(selector, arguments, dispatch, target, reference);
}

#pragma mark Streams

IOReturn IOUSBInterfaceUserClientV3::_supportsStreams(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::_supportsStreams",  target);
	
	target->retain();
	IOReturn kr = target->SupportsStreams((UInt8)arguments->scalarInput[0], &(arguments->scalarOutput[0]));
	target->release();
	
	return kr;
}

IOReturn
IOUSBInterfaceUserClientV3::SupportsStreams(UInt8 pipeRef, uint64_t *supportsStreams)
{
    IOUSBPipeV2 		*pipeObjV2;
    IOReturn			ret = kIOReturnSuccess;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::SupportsStreams for pipe %d, isInactive: %d",  this, pipeRef, isInactive());
	
	IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		pipeObjV2 = (IOUSBPipeV2 *) GetPipeObj(pipeRef);
		
		if (pipeObjV2 && OSDynamicCast(IOUSBPipeV2, pipeObjV2))
		{
			*supportsStreams = pipeObjV2->SupportsStreams();
			pipeObjV2->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV3[%p]::SupportsStreams - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	else
	{
		USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::SupportsStreams for pipe %d: %qd",  this, pipeRef, *supportsStreams);
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBInterfaceUserClientV3::_createStreams(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::_createStreams",  target);
	
	target->retain();
	IOReturn kr = target->CreateStreams((UInt8)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1]);
	target->release();
	
	return kr;
}

IOReturn
IOUSBInterfaceUserClientV3::CreateStreams(UInt8 pipeRef, UInt32 maxStreams)
{
    IOUSBPipeV2 		*pipeObjV2;
    IOReturn			ret = kIOReturnSuccess;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::CreateStreams for pipe %d, isInactive: %d",  this, pipeRef, isInactive());
	
	IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		pipeObjV2 = (IOUSBPipeV2 *) GetPipeObj(pipeRef);
		
		if (pipeObjV2 && OSDynamicCast(IOUSBPipeV2, pipeObjV2))
		{
			ret = pipeObjV2->CreateStreams(maxStreams);
			pipeObjV2->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV3[%p]::CreateStreams - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBInterfaceUserClientV3::_getConfiguredStreams(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::_getConfiguredStreams",  target);
	
	target->retain();
	IOReturn kr = target->GetConfiguredStreams((UInt8)arguments->scalarInput[0], &(arguments->scalarOutput[0]));
	target->release();
	
	return kr;
}

IOReturn
IOUSBInterfaceUserClientV3::GetConfiguredStreams(UInt8 pipeRef, uint64_t *configuredStreams)
{
    IOUSBPipeV2 		*pipeObjV2;
    IOReturn			ret = kIOReturnSuccess;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::GetConfiguredStreams for pipe %d, isInactive: %d",  this, pipeRef, isInactive());
	
	IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		pipeObjV2 = (IOUSBPipeV2 *) GetPipeObj(pipeRef);
		
		if (pipeObjV2 && OSDynamicCast(IOUSBPipeV2, pipeObjV2))
		{
			*configuredStreams = pipeObjV2->SupportsStreams();
			pipeObjV2->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV3[%p]::GetConfiguredStreams - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	else
	{
		USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::GetConfiguredStreams for pipe %d: %qd",  this, pipeRef, *configuredStreams);
	}
	
    DecrementOutstandingIO();
    return ret;
}


#pragma mark Overriden Methods
IOReturn
IOUSBInterfaceUserClientV3::ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion)
{
	return ReadStreamsPipe(pipeRef, 0, noDataTimeout, completionTimeout, buffer, size, completion);
}


IOReturn
IOUSBInterfaceUserClientV3::ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size)
{
	return ReadStreamsPipe(pipeRef, 0, noDataTimeout, completionTimeout, buf, size);
}


IOReturn
IOUSBInterfaceUserClientV3::ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem, IOByteCount *bytesRead)
{
	return ReadStreamsPipe(pipeRef, 0, noDataTimeout, completionTimeout, mem, bytesRead);
}


IOReturn
IOUSBInterfaceUserClientV3::WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion)
{
	return WriteStreamsPipe(pipeRef, 0, noDataTimeout, completionTimeout, buffer, size, completion);
}


IOReturn
IOUSBInterfaceUserClientV3::WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, const void *buf, UInt32 size)
{
	return WriteStreamsPipe(pipeRef, 0, noDataTimeout, completionTimeout, buf, size);
}


IOReturn
IOUSBInterfaceUserClientV3::WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem)
{
	return WriteStreamsPipe(pipeRef, 0, noDataTimeout, completionTimeout, mem);
}


#pragma mark Pipe Methods

//================================================================================================
//
//   _ReadStreamsPipe
//
//   This method is called for both sync and async writes.  In the case of async, the parameters
//   will only be 6 scalars.  In the sync case, it will be 3 scalars and, depending on the size,
//   an inputStructure or an inputIOMD.

//================================================================================================
//
IOReturn IOUSBInterfaceUserClientV3::_ReadStreamsPipe(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::_ReadPipe",  target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort )
	{
		IOUSBCompletion							tap;
		IOUSBUserClientAsyncParamBlock *		pb = (IOUSBUserClientAsyncParamBlock*) IOMalloc(sizeof(IOUSBUserClientAsyncParamBlock));
		
        if (!pb)
            return kIOReturnNoMemory;
		
		target->retain();
        target->IncrementOutstandingIO();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV3::ReqComplete;
        tap.parameter = pb;
		
		ret = target->ReadStreamsPipe(	(UInt8) arguments->scalarInput[0],				// pipeRef
									  (UInt32) arguments->scalarInput[1],				// streamID
									  (UInt32) arguments->scalarInput[2],				// noDataTimeout
									  (UInt32) arguments->scalarInput[3],				// completionTimeout
									  (mach_vm_address_t) arguments->scalarInput[4],	// buffer (in user task)
									  (mach_vm_size_t) arguments->scalarInput[5],		// size of buffer
									  &tap);											// completion
		
        if ( ret )
		{
            if ( pb )
			{
				IOFree(pb, sizeof(*pb));
			}
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	else
	{
		target->retain();
		
		if ( arguments->structureOutputDescriptor )
		{
			ret = target->ReadStreamsPipe(	(UInt8)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2], (UInt32)arguments->scalarInput[3],
								   arguments->structureOutputDescriptor, (IOByteCount *)&(arguments->structureOutputDescriptorSize));
		}
		else
		{
			ret = target->ReadStreamsPipe((UInt8)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2], (UInt32)arguments->scalarInput[3],
								   arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
		}
		
		target->release();
	}
	
	return ret;
}

IOReturn
IOUSBInterfaceUserClientV3::ReadStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion)
{
	IOReturn				ret = kIOReturnNotAttached;
    IOMemoryDescriptor *	iomd = NULL;
    IOUSBPipe *				pipeObj = NULL;
    IOUSBPipeV2 *			pipeV2Obj = NULL;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (Async) (pipeRef: %d, streamID: %d, %d, %d, buffer: 0x%qx, size: %qd, completion: %p)",  this, pipeRef, streamID, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, buffer, size, completion);
    
	if (fOwner && !isInactive())
    {
		if (completion == NULL)
		{
			USBLog(1,"IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (async) bad arguments (%qd, %qx, %p)",  this, size, buffer, completion);
			//USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCReadPipe, size, (uintptr_t)buffer, (uintptr_t)completion, kIOReturnBadArgument );
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// This is an Async request
			IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
			
			USBLog(7,"IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (Async) creating IOMD:  buffer: 0x%qx, size: %qd", this, buffer, size);
			iomd = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionIn, fTask);
			if (!iomd)
			{
				USBLog(1,"IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (async ) IOMemoryDescriptor::withAddressRange returned NULL",  this);
				//USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCReadPipe, (uintptr_t)this, size, (uintptr_t)mem, kIOReturnNoMemory );
				ret = kIOReturnNoMemory;
				goto Exit;
			}
			
			USBLog(7,"IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (async) created IOMD %p",  this, iomd);
			ret = iomd->prepare();
			if ( ret != kIOReturnSuccess)
			{
				USBLog(3,"IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (async) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
				iomd->release();
				goto Exit;
			}
			
			pb->fMax = size;
			pb->fMem = iomd;
			
			pipeV2Obj = OSDynamicCast(IOUSBPipeV2, pipeObj);
			if ( pipeV2Obj)
				ret = pipeV2Obj->Read(streamID, iomd, noDataTimeout, completionTimeout, size, completion, NULL);
			else
				ret = pipeObj->Read(iomd, noDataTimeout, completionTimeout, completion, NULL);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(5,"IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (async) returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
				if (iomd != NULL)
				{
					iomd->complete();
					iomd->release();
				}
			}
		}
		else
		{
			USBLog(5,"IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (async) can't find pipeRef, returning kIOUSBUnknownPipeErr",  this);
			ret = kIOUSBUnknownPipeErr;
		}
	}
	else
		ret = kIOReturnNotAttached;
	
Exit:
	
	if (pipeObj)
		pipeObj->release();
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (async - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	return ret;
}


IOReturn
IOUSBInterfaceUserClientV3::ReadStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, void *buffer, UInt32 *size)
{
    IOReturn				ret = kIOReturnSuccess;
    IOMemoryDescriptor *	iomd = NULL;
    IOUSBPipe *				pipeObj = NULL;
    IOUSBPipeV2 *			pipeV2Obj = NULL;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe < 4K (pipeRef: %d, streamID: %d, data timeout: %d, completionTimeout: %d, buffer: %p, size: %d)",  this, pipeRef, streamID, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, buffer, (uint32_t)*size);
    
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			IOByteCount count = *size;
			iomd = IOMemoryDescriptor::withAddress( (void *)buffer, *size, kIODirectionIn);
			if (iomd)
			{
				*size = 0;
				ret = iomd->prepare();
				if ( ret == kIOReturnSuccess)
				{
					pipeV2Obj = OSDynamicCast(IOUSBPipeV2, pipeObj);
					if ( pipeV2Obj)
						ret = pipeV2Obj->Read(streamID, iomd, noDataTimeout, completionTimeout, count, NULL, &count);
					else
						ret = pipeObj->Read(iomd, noDataTimeout, completionTimeout, NULL, &count);
					*size = count;
					iomd->complete();
				}
				else
				{
					USBLog(3,"IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (sync < 4K) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
				}
				iomd->release();
				iomd = NULL;
			}
			else
				ret =  kIOReturnNoMemory;
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe  (sync < 4K) - returning err %x, size read: %d",  this, ret, (uint32_t)*size);
	}
	
    DecrementOutstandingIO();
	
    return ret;
}



IOReturn
IOUSBInterfaceUserClientV3::ReadStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *iomd, IOByteCount *bytesRead)
{
    IOReturn				ret = kIOReturnSuccess;
    IOUSBPipe 			*	pipeObj = NULL;
    IOUSBPipeV2 *			pipeV2Obj = NULL;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe > 4K (pipeRef: %d, streamID: %d, %d, %d, IOMD: %p)",  this, pipeRef, streamID, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, iomd);
	
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			if (iomd)
			{
				ret = iomd->prepare();
				if (ret == kIOReturnSuccess)
				{
					pipeV2Obj = OSDynamicCast(IOUSBPipeV2, pipeObj);
					if ( pipeV2Obj)
						ret = pipeV2Obj->Read(streamID, iomd, noDataTimeout, completionTimeout, iomd->getLength(), NULL, bytesRead);
					else
						ret = pipeObj->Read(iomd, noDataTimeout, completionTimeout, 0, bytesRead );
					iomd->complete();
				}
				else
				{
					USBLog(3,"IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe (sync > 4K) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
				}
			}
			else
			{
				ret = kIOReturnNoMemory;
			}
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV3[%p]::ReadStreamsPipe > 4K - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	DecrementOutstandingIO();
	return ret;
}



//================================================================================================
//
//   _WriteStreamsPipe
//
//   This method is called for both sync and async writes.  In the case of async, the parameters
//   will only be 6 scalars.  In the sync case, it will be 4 scalars and, depending on the size,
//   an inputStructure or an inputIOMD.

//================================================================================================
//
IOReturn IOUSBInterfaceUserClientV3::_WriteStreamsPipe(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::_WriteStreamsPipe",  target);
	
	if ( arguments->asyncWakePort )
	{
		IOUSBCompletion							tap;
		IOUSBUserClientAsyncParamBlock *		pb = (IOUSBUserClientAsyncParamBlock*) IOMalloc(sizeof(IOUSBUserClientAsyncParamBlock));
		
        if (!pb)
            return kIOReturnNoMemory;
		
		target->retain();
        target->IncrementOutstandingIO();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV3::ReqComplete;
        tap.parameter = pb;
		
		ret = target->WriteStreamsPipe(	(UInt8) arguments->scalarInput[0],		// pipeRef
								(UInt32) arguments->scalarInput[1],				// streamID
								(UInt32) arguments->scalarInput[2],				// noDataTimeout
								(UInt32) arguments->scalarInput[3],				// completionTimeout
								(mach_vm_address_t) arguments->scalarInput[4],	// buffer (in user task)
								(mach_vm_size_t) arguments->scalarInput[5],		// size of buffer
								&tap);											// completion
		
        if ( ret )
		{
            if ( pb )
			{
				IOFree(pb, sizeof(*pb));
			}
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	else
	{
		target->retain();
		
		if ( arguments->structureInputDescriptor )
		{
			ret = target->WriteStreamsPipe(	(UInt8)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2], (UInt32)arguments->scalarInput[3],
									arguments->structureInputDescriptor);
		}
		else
		{
			ret = target->WriteStreamsPipe((UInt16)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2], (UInt32)arguments->scalarInput[3],
									arguments->structureInput, arguments->structureInputSize);
		}
		
		target->release();
	}
	
	return ret;
}


IOReturn
IOUSBInterfaceUserClientV3::WriteStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion)
{
	IOReturn				ret = kIOReturnNotAttached;
    IOMemoryDescriptor *	iomd = NULL;
    IOUSBPipe *				pipeObj = NULL;
    IOUSBPipeV2 *			pipeV2Obj = NULL;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (Async) (pipeRef: %d, streamID: %d, %d, %d, buffer: 0x%qx, size: %qd, completion: %p)",  this, pipeRef, streamID, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, buffer, size, completion);
    
	if (fOwner && !isInactive())
    {
		if (completion == NULL )
		{
			USBLog(1,"IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (async) bad arguments (%qd, %qx, %p)",  this, size, buffer, completion);
			//USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCWritePipe, size, buffer, (uintptr_t)completion, kIOReturnBadArgument );
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// This is an Async request
			IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
			
			iomd = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionOut, fTask);
			if (!iomd)
			{
				USBLog(1,"IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (async ) IOMemoryDescriptor::withAddressRange returned NULL",  this);
				//USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCWritePipe, (uintptr_t)this, size, (uintptr_t)mem, kIOReturnNoMemory );
				ret = kIOReturnNoMemory;
				goto Exit;
			}
			
			USBLog(7,"IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (async) created IOMD %p",  this, iomd);
			ret = iomd->prepare();
			if ( ret != kIOReturnSuccess)
			{
				USBLog(3,"IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (async) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
				iomd->release();
				goto Exit;
			}
			
			pb->fMax = size;
			pb->fMem = iomd;
			
			pipeV2Obj = OSDynamicCast(IOUSBPipeV2, pipeObj);
			if ( pipeV2Obj)
				ret = pipeV2Obj->Write(streamID, iomd, noDataTimeout, completionTimeout, size, completion);
			else
				ret = pipeObj->Write(iomd, noDataTimeout, completionTimeout, completion);
			
			if ( ret != kIOReturnSuccess)
			{
				USBLog(5,"IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (async) returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
				if (iomd != NULL)
				{
					iomd->complete();
					iomd->release();
				}
			}
		}
		else
		{
			USBLog(5,"IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (async) can't find pipeRef, returning kIOUSBUnknownPipeErr",  this);
			ret = kIOUSBUnknownPipeErr;
		}
	}
	else
		ret = kIOReturnNotAttached;
	
Exit:
	if (pipeObj)
		pipeObj->release();
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (async - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	return ret;
}


IOReturn
IOUSBInterfaceUserClientV3::WriteStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, const void *buffer, UInt32 size)
{
    IOReturn				ret = kIOReturnSuccess;
    IOMemoryDescriptor *	iomd = NULL;
    IOUSBPipe *				pipeObj = NULL;
    IOUSBPipeV2 *			pipeV2Obj = NULL;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe < 4K (pipeRef: %d, streamID: %d, %d, %d, buffer: %p, size: %d)",  this, pipeRef, streamID, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, buffer, (uint32_t)size);
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
	
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// Sync, < 4K
			iomd = IOMemoryDescriptor::withAddress( (void *) buffer, size, kIODirectionOut);
			if (iomd)
			{
				ret = iomd->prepare();
				if ( ret == kIOReturnSuccess)
				{
					pipeV2Obj = OSDynamicCast(IOUSBPipeV2, pipeObj);
					if ( pipeV2Obj)
						ret = pipeV2Obj->Write(streamID, iomd, noDataTimeout, completionTimeout, size, NULL);
					else
					ret = pipeObj->Write(iomd, noDataTimeout, completionTimeout);
					if ( ret != kIOReturnSuccess)
					{
						USBLog(5,"IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (sync < 4K) returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
					}
					iomd->complete();
				}
				else
				{
					USBLog(3,"IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (sync < 4K) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
				}
				
				iomd->release();
				iomd = NULL;
			}
			else
				ret = kIOReturnNoMemory;
			
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe (sync < 4K) - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn
IOUSBInterfaceUserClientV3::WriteStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *iomd)
{
    IOReturn				ret;
    IOUSBPipe *				pipeObj = NULL;
    IOUSBPipeV2 *			pipeV2Obj = NULL;
	
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe > 4K (pipeRef: %d, streamID: %d, %d, %d, IOMD: %p)",  this, pipeRef, streamID, (uint32_t)noDataTimeout, (uint32_t)completionTimeout, iomd);
	
	if ( iomd == NULL )
	{
		USBLog(3,"+IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe > 4K  mem was NULL, returning kIOReturnBadArgument",  this);
		return kIOReturnBadArgument;
	}
	
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			if (iomd)
			{
				ret = iomd->prepare();
				if (ret == kIOReturnSuccess)
				{
					pipeV2Obj = OSDynamicCast(IOUSBPipeV2, pipeObj);
					if ( pipeV2Obj)
						ret = pipeV2Obj->Write(streamID, iomd, noDataTimeout, completionTimeout, iomd->getLength(), NULL);
					else
						ret = pipeObj->Write(iomd, noDataTimeout, completionTimeout );
					iomd->complete();
				}
				else
				{
					USBLog(1,"IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe > (sync > 4K) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
					//USBTrace( kUSBTInterfaceUserClient,  kTPInterfaceUCWritePipe, (uintptr_t)this, ret, 0, 0 );
				}
			}
			else
			{
				ret = kIOReturnNoMemory;
			}
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBInterfaceUserClientV3[%p]::WriteStreamsPipe > 4K - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn
IOUSBInterfaceUserClientV3::_AbortStreamsPipe(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::_AbortStreamsPipe",  target);
	
	target->retain();
    IOReturn kr = target->AbortStreamsPipe((UInt8)arguments->scalarInput[0], arguments->scalarInput[1]);
	target->release();
	
	return kr;
}

IOReturn
IOUSBInterfaceUserClientV3::AbortStreamsPipe(UInt8 pipeRef, UInt32 streamID)
{
    IOUSBPipe *				pipeObj = NULL;
    IOUSBPipeV2 *			pipeV2Obj = NULL;
    IOReturn		ret;
    
    USBLog(7, "+IOUSBInterfaceUserClientV3[%p]::AbortStreamsPipe (pipeRef: %d, streamID: %d)",  this, pipeRef, streamID);
    
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			pipeV2Obj = OSDynamicCast(IOUSBPipeV2, pipeObj);
			if ( pipeV2Obj)
				ret = pipeV2Obj->Abort(streamID);
			else
				ret =  pipeObj->Abort();
			
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
    {
        if ( ret == kIOUSBUnknownPipeErr || ret == kIOReturnNotAttached)
		{
            USBLog(6, "IOUSBInterfaceUserClientV3[%p]::AbortStreamsPipe(%d) - returning err %x (%s)",  this, pipeRef, ret, USBStringFromReturn(ret));
		}
        else
		{
            USBLog(3, "IOUSBInterfaceUserClientV2[%p]::AbortStreamsPipe(%d) - returning err %x (%s)",  this, pipeRef, ret, USBStringFromReturn(ret));
		}
    }
    
    DecrementOutstandingIO();
    return ret;
}



#pragma mark Padding Methods

OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 0);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 1);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 2);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 3);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 4);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 5);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 6);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 7);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 8);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 9);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 10);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 11);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 12);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 13);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 14);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 15);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 16);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 17);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 18);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 19);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 20);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 21);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 22);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 23);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV3, 24);


