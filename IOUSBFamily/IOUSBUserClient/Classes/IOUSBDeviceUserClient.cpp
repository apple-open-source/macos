/*
 * Copyright © 1998-2010 Apple Inc.  All rights reserved.
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
#include <IOKit/IOKitKeys.h>

#include "IOUSBDeviceUserClient.h"
#include "USBTracepoints.h"

#if DEBUG_LEVEL != 0
	#include <sys/proc.h>
#endif

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUserClient

// ExpansionData members
#define FSLEEPPOWERALLOCATED					fIOUSBDeviceUserClientExpansionData->fSleepPowerAllocated
#define FWAKEPOWERALLOCATED						fIOUSBDeviceUserClientExpansionData->fWakePowerAllocated

#ifndef kIOUserClientCrossEndianKey
#define kIOUserClientCrossEndianKey "IOUserClientCrossEndian"
#endif

#ifndef kIOUserClientCrossEndianCompatibleKey
#define kIOUserClientCrossEndianCompatibleKey "IOUserClientCrossEndianCompatible"
#endif

/* Convert USBLog to use kprintf debugging */
#ifndef IOUSBDEVICEUSERCLIENT_USE_KPRINTF
#define IOUSBDEVICEUSERCLIENT_USE_KPRINTF 0
#endif

#if IOUSBDEVICEUSERCLIENT_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBDEVICEUSERCLIENT_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

//=============================================================================================
//
//	Note on the use of IncrementOutstandingIO(), DecrementOutstandingIO() and doing extra
//	retain()/release() on async calls:
//
//	The UserClient is a complex driver.  It is attached to its provider when it is instantiated
// 	due to a device/interface being created but it is ALSO used by the user land IOUSBLib.  
//	Thus, it effectively has 2 clients, one on the kernel side and one on the user side.  When
//	the user side closes the connection to this user client, we terminate ourselves.  When the
//	device is unplugged, we get terminated by our provider.  The Increment/DecrementOutstandingIO()
//	calls are used to keep track of any IO that has not finished so that when OUR provider attempts
//	to terminate us, we won't get released (because we have our provider open) until the IO completes.
//	However, when our user land client closes us, we terminate ourselves.  In this case the 
//	OutstandingIO() calls do not help us in preventing our object from going away.  That is why we
//	also use a retain()/release() pair on async calls;  if we are closed before the async request is
//	complete, we will not go away and hence won't panic when we try to execute the completion routine.
//
//	Yes, we could have use only retain()/release(), but all our other kernel drivers use the 
//	outstandingIO metaphor so I thought it would be complete to use it here as well.
//
//	Another interesting piece is that we wait 1 ms in our clientClose method.  This will allow other 
//	pending threads to run and possible enter our driver (See bug #2862199).
//
//=============================================================================================
//

//================================================================================================
//
//   IOUSBDeviceUserClientV2 Methods
//
//================================================================================================
//
OSDefineMetaClassAndStructors(IOUSBDeviceUserClientV2, super)

/*
struct IOExternalMethodDispatch
{
    IOExternalMethodAction function;
    uint32_t		   checkScalarInputCount;
    uint32_t		   checkStructureInputSize;
    uint32_t		   checkScalarOutputCount;
    uint32_t		   checkStructureOutputSize;
};
*/

const IOExternalMethodDispatch 
IOUSBDeviceUserClientV2::sMethods[kIOUSBLibDeviceUserClientNumCommands] = {
    {	//    kUSBDeviceUserClientOpen
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_open,
		1, 0,
		0, 0
    },
    {	//    kUSBDeviceUserClientClose
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_close,
		0, 0,
		0, 0
    },
    {	//    kUSBDeviceUserClientSetConfig
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_SetConfiguration,
		1, 0,
		0, 0
    },
    {	//    kUSBDeviceUserClientGetConfig
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_GetConfiguration,
		0, 0,
		1, 0
    },
    {	//    kUSBDeviceUserClientGetConfigDescriptor
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_GetConfigDescriptor,
		1, 0,
		0, 0xffffffff
    },
    {	//    kUSBDeviceUserClientGetFrameNumber
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_GetFrameNumber,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
    {	//    kUSBDeviceUserClientDeviceRequestOut
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_DeviceRequestOut,
		9, 0xffffffff,
		0, 0
    },
    {	//    kUSBDeviceUserClientDeviceRequestIn (InLine and OOL, Sync and Async)
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_DeviceRequestIn,
		9, 0,
		1, 0xffffffff
    },
    {	//    kUSBDeviceUserClientCreateInterfaceIterator
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_CreateInterfaceIterator,
		4, 0,
		1, 0
    },
    {	//    kUSBDeviceUserClientResetDevice
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_ResetDevice,
		0, 0, 
		0, 0
    },
    {	//    kUSBDeviceUserClientSuspend
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_SuspendDevice,
		1, 0,
		0, 0,
    },
    {	//    kUSBDeviceUserClientAbortPipeZero
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_AbortPipeZero,
		0, 0,
		0, 0
    },
    {	//    kUSBDeviceUserClientReEnumerateDevice
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_ReEnumerateDevice,
		1, 0,
		0, 0,
    },
    {	//    kUSBDeviceUserClientGetMicroFrameNumber
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_GetMicroFrameNumber,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
    {	//    kUSBDeviceUserClientGetFrameNumberWithTime
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_GetFrameNumberWithTime,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
    {	//    kUSBDeviceUserClientSetAsyncPort
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_SetAsyncPort,
		0, 0,
		0, 0
    },
    {	//    kUSBDeviceUserClientGetDeviceInformation
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_GetDeviceInformation,
		0, 0,
		1, 0
    },
    {	//    kUSBDeviceUserClientRequestExtraPower
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_RequestExtraPower,
		2, 0,
		1, 0
    },
    {	//    kUSBDeviceUserClientReturnExtraPower
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_ReturnExtraPower,
		2, 0,
		0, 0
    },
    {	//    kUSBDeviceUserClientGetExtraPowerAllocated
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_GetExtraPowerAllocated,
		1, 0,
		1, 0
    }
};


#pragma mark -
 
IOReturn IOUSBDeviceUserClientV2::externalMethod( 
												uint32_t                    selector, 
												IOExternalMethodArguments * arguments,
												IOExternalMethodDispatch *  dispatch, 
												OSObject *                  target, 
												void *                      reference)
{
	
    if (selector < (uint32_t) kIOUSBLibDeviceUserClientNumCommands)
    {
        dispatch = (IOExternalMethodDispatch *) &sMethods[selector];
        
        if (!target)
            target = this;
    }
	
	return super::externalMethod(selector, arguments, dispatch, target, reference);
}

#pragma mark Async Support

IOReturn 
IOUSBDeviceUserClientV2::_SetAsyncPort(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7,"+IOUSBDeviceUserClientV2::_SetAsyncPort");
    return target->SetAsyncPort(arguments->asyncWakePort);
}


IOReturn IOUSBDeviceUserClientV2::SetAsyncPort(mach_port_t port)
{
	USBLog(7,"+IOUSBDeviceUserClientV2::SetAsyncPort");
    if (!fOwner)
        return kIOReturnNotAttached;
	
    fWakePort = port;
    return kIOReturnSuccess;
}



void
IOUSBDeviceUserClientV2::ReqComplete(void *obj, void *param, IOReturn res, UInt32 remaining)
{
    io_user_reference_t						args[1];
    IOUSBUserClientAsyncParamBlock *	pb = (IOUSBUserClientAsyncParamBlock *)param;
    IOUSBDeviceUserClientV2 *me =				OSDynamicCast(IOUSBDeviceUserClientV2, (OSObject*)obj);
	
    if (!me)
		return;
	
	USBLog(7,"+IOUSBDeviceUserClientV2::ReqComplete");
	USBTrace( kUSBTDeviceUserClient,  kTPDeviceUCReqComplete, (uintptr_t)me, res, remaining, pb->fMax );
	if ((res == kIOReturnSuccess) || (res == kIOReturnOverrun) )
    {
		// Return the len done anyway, its in the buffer
		args[0] = (io_user_reference_t)(pb->fMax - remaining);
    }
    else 
    {
        args[0] = 0;
    }

    if (pb->fMem)
    {
		pb->fMem->complete();
		pb->fMem->release();
    }
    if (!me->fDead)
		sendAsyncResult64(pb->fAsyncRef, res, args, 1);
	
    IOFree(pb, sizeof(*pb));
    me->DecrementOutstandingIO();
	me->release();
}



#pragma mark User Client Start/Close

// Don't add any USBLogs to this routine.   You will panic if you use getName().
bool
IOUSBDeviceUserClientV2::initWithTask(task_t owningTask, void *security_id , UInt32 type, OSDictionary * properties )
{
    // allocate our expansion data
    if (!fIOUSBDeviceUserClientExpansionData)
    {
		fIOUSBDeviceUserClientExpansionData = (IOUSBDeviceUserClientExpansionData *)IOMalloc(sizeof(IOUSBDeviceUserClientExpansionData));
		if (!fIOUSBDeviceUserClientExpansionData)
			return false;
		bzero(fIOUSBDeviceUserClientExpansionData, sizeof(IOUSBDeviceUserClientExpansionData));
    }
    

	if ( properties != NULL )
	{
		properties->setObject( kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue);
	}
	
    if (!super::initWithTask(owningTask, security_id , type, properties))
        return false;
    
    if (!owningTask)
		return false;
	
	
    fTask = owningTask;
	

#if DEBUG_LEVEL != 0
	char	nbuf[256];
	proc_t	p = proc_self();
	int		owningPID = proc_pid(p);
	
	proc_name(owningPID,nbuf,256);
	
	USBLog(5,"IOUSBDeviceUserClientV2[%p]::initWithTask  Owning PID is %d, name is %s", this, owningPID, nbuf);
	
	proc_rele(p);
	
#endif
	
    fOwner = NULL;
    fGate = NULL;
    fDead = false;
	
    return true;
}


bool 
IOUSBDeviceUserClientV2::start( IOService * provider )
{
    IOWorkLoop	*		workLoop = NULL;
    IOCommandGate *		commandGate = NULL;
	
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::start(%p)",  this, provider);
	
	// retain() ourselves so we don't go away while we are start()'ing
	retain();
	
    fOwner = OSDynamicCast(IOUSBDevice, provider);
	
    if (!fOwner)
    {
		USBError(1, "IOUSBDeviceUserClientV2[%p]::start - provider is NULL!",  this);
		goto ErrorExit;
    }
    
 	// Now, retain our provider since we will not open() it until our user-space client open()'s it
	fOwner->retain();

	if (!super::start(provider))
    {
		USBError(1, "IOUSBDeviceUserClientV2[%p]::start - super::start returned false!",  this);
        goto ErrorExit;
    }
    

    commandGate = IOCommandGate::commandGate(this);
	
    if (!commandGate)
    {
		USBError(1, "IOUSBDeviceUserClientV2[%p]::start - unable to create command gate",  this);
		goto ErrorExit;
    }
	
    workLoop = getWorkLoop();
    if (!workLoop)
    {
		USBError(1, "IOUSBDeviceUserClientV2[%p]::start - unable to find my workloop",  this);
		goto ErrorExit;
    }
    workLoop->retain();
	
    if (workLoop->addEventSource(commandGate) != kIOReturnSuccess)
    {
		USBError(1, "IOUSBDeviceUserClientV2[%p]::start - unable to add gate to work loop",  this);
		goto ErrorExit;
    }
		
	// Now that we have succesfully added our gate to the workloop, set our member variables
    //
    fGate = commandGate;
    fWorkLoop = workLoop;
    
	USBLog(7, "-IOUSBDeviceUserClientV2[%p]::start",  this);
	
    return true;
    
ErrorExit:
		
	if ( commandGate != NULL )
	{
		commandGate->release();
		commandGate = NULL;
	}
	
    if ( workLoop != NULL )
    {
        workLoop->release();
        workLoop = NULL;
    }
	
	if ( fOwner )
		fOwner->release();
	
	release();
	
    return false;
}



IOReturn IOUSBDeviceUserClientV2::_open(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    return target->open((bool)arguments->scalarInput[0]);
}

IOReturn 
IOUSBDeviceUserClientV2::open(bool seize)
{
    IOOptionBits	options = (seize ? (IOOptionBits)kIOServiceSeize : 0);
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::open",  this);
    IncrementOutstandingIO();
	
    if (fOwner && !isInactive())
    {
		if (fOwner->open(this, options))
			fNeedToClose = false;
		else
		{
			USBLog(5, "IOUSBDeviceUserClientV2[%p]::open fOwner->open() failed.  Returning kIOReturnExclusiveAccess",  this);
			ret = kIOReturnExclusiveAccess;
		}
    }
    else
        ret = kIOReturnNotAttached;
	
    USBLog(7, "-IOUSBDeviceUserClientV2[%p]::open - returning %x",  this, ret);    

    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBDeviceUserClientV2::_close(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference, arguments)
    return target->close();
}

// This is NOT the normal IOService::close(IOService*) method. It is intended to handle a close coming 
// in from the user side. Since we do not have any IOKit objects as children, we will just use this to
// terminate ourselves.
IOReturn 
IOUSBDeviceUserClientV2::close()
{
    IOReturn 	ret = kIOReturnSuccess;
    
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::close",  this);
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		if (fOwner->isOpen(this))
		{
			fNeedToClose = true;			// this will cause the DecrementOutstandingIO to close us
			if (GetOutstandingIO() > 1)
			{
				// we need to abort any outstanding IO to allow us to close
				USBLog(3, "IOUSBDeviceUserClientV2[%p]::close - outstanding IO - aborting pipe zero",  this);
				AbortPipeZero();
			}
		}
		else
			ret = kIOReturnNotOpen;
    }
    else
        ret = kIOReturnNotAttached;
	
    USBLog(7, "-IOUSBDeviceUserClientV2[%p]::close - returning %x",  this, ret);
    DecrementOutstandingIO();
    return ret;
}


#pragma mark DevRequestIn

IOReturn IOUSBDeviceUserClientV2::_DeviceRequestIn(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret;

    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_DeviceRequestIn",  target);
	if ( arguments->asyncWakePort ) 
	{
		IOUSBCompletion							tap;
		IOUSBUserClientAsyncParamBlock *	pb = (IOUSBUserClientAsyncParamBlock*) IOMalloc(sizeof(IOUSBUserClientAsyncParamBlock));
		
        if (!pb) 
            return kIOReturnNoMemory;
		
  		target->retain();
		target->IncrementOutstandingIO();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
		tap.target = target;
        tap.action = &IOUSBDeviceUserClientV2::ReqComplete;
        tap.parameter = pb;
		
		// DeviceRequestIn, Async, buffer in  clients task
		ret = target->DeviceRequestIn( (UInt8)arguments->scalarInput[1],			// bmRequestType,
									   (UInt8)arguments->scalarInput[2],			// bRequest,
									   (UInt16)arguments->scalarInput[3],			// wValue,
									   (UInt16)arguments->scalarInput[4],			// wIndex,
									   (mach_vm_size_t)arguments->scalarInput[5],	// pData (buffer),
									   (mach_vm_address_t)arguments->scalarInput[6],// wLength (bufferSize),
									   (UInt32)arguments->scalarInput[7],			// noDataTimeout,
									   (UInt32)arguments->scalarInput[8],			// completionTimeout,
									   &tap);										// completion
		
        if ( ret ) 
		{
            if ( pb )
				IOFree(pb, sizeof(*pb));
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	else if ( arguments->structureOutputDescriptor ) 
  	{
		// DeviceRequestIn, Sync, > 4K
        ret = target->DeviceRequestIn( (UInt8)arguments->scalarInput[1],			// bmRequestType,
									   (UInt8)arguments->scalarInput[2],			// bRequest,
									   (UInt16)arguments->scalarInput[3],			// wValue,
									   (UInt16)arguments->scalarInput[4],			// wIndex,
									   (UInt32)arguments->scalarInput[7],			// noDataTimeout,
									   (UInt32)arguments->scalarInput[8],			// completionTimeout,
									   arguments->structureOutputDescriptor,		// IOMD for request
									   &(arguments->structureOutputDescriptorSize));// IOMD size
	}
    else
	{
		// DeviceRequestInIn, Sync, < 4K
        ret = target->DeviceRequestIn( (UInt8)arguments->scalarInput[1],			// bmRequestType,
									   (UInt8)arguments->scalarInput[2],			// bRequest,
									   (UInt16)arguments->scalarInput[3],			// wValue,
									   (UInt16)arguments->scalarInput[4],			// wIndex,
									   (UInt32)arguments->scalarInput[7],			// noDataTimeout,
									   (UInt32)arguments->scalarInput[8],			// completionTimeout,
									   arguments->structureOutput,					// bufferPtr
									   &(arguments->structureOutputSize));			// buffer size
	}
	
	// If asked, and if we got an overrun, send a flag back and squash overrun status.
	if (arguments->scalarOutputCount > 0)
	{
		if (ret == kIOReturnOverrun)
		{
			USBLog(3, "+IOUSBDeviceUserClientV2[%p]::_DeviceRequestIn kIOReturnOverrun",  target);
			arguments->scalarOutput[0] = 1;
			ret = kIOReturnSuccess;
		}
		else 
		{
			arguments->scalarOutput[0] = 0;
		}
	}

	return ret;
}  


// This is an Async
IOReturn
IOUSBDeviceUserClientV2::DeviceRequestIn(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, mach_vm_size_t size, mach_vm_address_t buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion * completion)
{
	IOReturn				ret = kIOReturnSuccess;
	IOMemoryDescriptor *	mem = NULL;
	
	USBLog(7, "IOUSBDeviceUserClientV2::DeviceRequestIn (Async) : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x, noDataTimeout = %d completionTimeout = %d",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	if (fOwner && !isInactive())
    {
		if (  completion == NULL )
		{
			USBLog(1,"IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (async)  had a NULL completion",  this); 
			USBTrace( kUSBTDeviceUserClient,  kTPDeviceUCDeviceRequestIn, bmRequestType, bRequest, wValue, kIOReturnBadArgument );
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		// This is an Async request 
		IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
		
		if ( size != 0 )
		{
			
			USBLog(7,"IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (async) creating IOMD",  this); 
			mem = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionIn, fTask);
			if (!mem)
			{
				USBLog(1,"IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (async ) IOMemoryDescriptor::withAddressRange returned NULL",  this); 
				USBTrace( kUSBTDeviceUserClient,  kTPDeviceUCDeviceRequestIn, (uintptr_t)this, (uintptr_t)buffer, size, kIOReturnNoMemory );
				ret = kIOReturnNoMemory;
				goto Exit;
			}
			
			ret = mem->prepare();
			if ( ret != kIOReturnSuccess)
			{
				USBLog(4,"IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (async) mem->prepare() returned 0x%x",  this, ret); 
				mem->release();
				goto Exit;
			}
		}
		
		pb->req.bmRequestType = bmRequestType;
		pb->req.bRequest = bRequest;
		pb->req.wValue = wValue;
		pb->req.wIndex = wIndex;
		pb->req.wLength = size;
		pb->req.pData = mem;
		
		pb->fMax = size;
		pb->fMem = mem;
		
		ret = fOwner->DeviceRequest(&pb->req, noDataTimeout, completionTimeout, completion);
		if ( ret != kIOReturnSuccess)
		{
			USBLog(5,"IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (async) returned 0x%x",  this, ret); 
			if (mem != NULL)
			{
				mem->complete();
				mem->release();
			}
		}
	}
	
Exit:
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (async) - returning err %x",  this, ret);
	}
	
	return ret;
}


// This is an sync with < 4K request
IOReturn
IOUSBDeviceUserClientV2::DeviceRequestIn(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, void *requestBuffer, uint32_t *size)
{
	IOReturn				ret = kIOReturnSuccess;
	IOUSBDevRequest			req;
	
	USBLog(7, "IOUSBDeviceUserClientV2::DeviceRequestIn < 4K : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4x noDataTimeout = %d completionTimeout = %d",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   *size,
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	IncrementOutstandingIO();
	
	if (fOwner && !isInactive())
	{
			// This is a sync request that is < 4K
			req.bmRequestType = bmRequestType;
			req.bRequest = bRequest;
			req.wValue = wValue;
			req.wIndex = wIndex;
			req.wLength = *size;
			req.pData = requestBuffer;
			req.wLenDone = 0;
			
			ret = fOwner->DeviceRequest(&req, noDataTimeout, completionTimeout);
			
			if ( (ret == kIOReturnSuccess) || (ret == kIOReturnOverrun) )		
			{
				*size = req.wLenDone;
			}
			else 
			{
				USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (sync < 4k) err:0x%x",  this, ret);
				*size = 0;
			}
	}
	else
		ret = kIOReturnNotAttached;
	
	if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceRequestIn < 4K to %p - returning err %x, size: %d",  this, fOwner, ret, *size);
	}
	
	DecrementOutstandingIO();
	return ret;
}

//  This is a DeviceRequestIn (sync) that is OOL, so it has an IOMD
IOReturn
IOUSBDeviceUserClientV2::DeviceRequestIn(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem, uint32_t *pOutSize)
{
    IOUSBDevRequestDesc		req;
	IOReturn				ret;
	
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::DeviceRequestIn > 4K ",  this);
		
	USBLog(7, "IOUSBDeviceUserClientV2[%p]::DeviceRequestIn > 4K: bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4x noDataTimeout = %d completionTimeout = %d", this,
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   *pOutSize,
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	if ( mem == NULL )
	{
		USBLog(3,"+IOUSBDeviceUserClientV2[%p]::DeviceRequestIn > 4K  mem was NULL, returning kIOReturnBadArgument",  this);
		return kIOReturnBadArgument;
	}

    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		ret = mem->prepare();
		
		if (ret == kIOReturnSuccess)
		{
			req.bmRequestType = bmRequestType;
			req.bRequest = bRequest;
			req.wValue = wValue;
			req.wIndex = wIndex;
			req.wLength = *pOutSize;
			req.pData = mem;
			req.wLenDone = 0;
			
			ret = fOwner->DeviceRequest(&req, noDataTimeout, completionTimeout);
			
			if ( (ret == kIOReturnSuccess) || (ret == kIOReturnOverrun) )		
			{
				*pOutSize = req.wLenDone;
			}
			else 
			{
				*pOutSize = 0;
			}
			
			mem->complete();
		}
		else
		{
			USBLog(4,"IOUSBDeviceUserClientV2[%p]::DeviceRequestIn > 4K mem->prepare() returned 0x%x",  this, ret); 
		}
	}
    else
        ret = kIOReturnNotAttached;
	
	if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceRequestIn > 4K - returning err %x, pOutSize: %d",  this, ret, *pOutSize);
	}
	
	DecrementOutstandingIO();
	return ret;
}


#pragma mark DevRequestOut

IOReturn 
IOUSBDeviceUserClientV2::_DeviceRequestOut(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn								ret;

    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_DeviceRequestOut  ", target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	
	if ( arguments->asyncWakePort ) 
	{
		IOUSBCompletion							tap;
		IOUSBUserClientAsyncParamBlock *	pb = (IOUSBUserClientAsyncParamBlock*) IOMalloc(sizeof(IOUSBUserClientAsyncParamBlock));
		
        if (!pb) 
            return kIOReturnNoMemory;
		
 		target->retain();
		target->IncrementOutstandingIO();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
		tap.target = target;
        tap.action = &IOUSBDeviceUserClientV2::ReqComplete;
        tap.parameter = pb;
		
 		// DeviceRequestIn, Async, buffer in  clients task
		ret = target->DeviceRequestOut(	(UInt8)arguments->scalarInput[1],			// bmRequestType,
										(UInt8)arguments->scalarInput[2],			// bRequest,
										(UInt16)arguments->scalarInput[3],			// wValue,
										(UInt16)arguments->scalarInput[4],			// wIndex,
										(mach_vm_size_t)arguments->scalarInput[5],		// wLength (bufferSize),
										(mach_vm_address_t)arguments->scalarInput[6],	// pData (buffer),
										(UInt32)arguments->scalarInput[7],			// noDataTimeout,
										(UInt32)arguments->scalarInput[8],			// completionTimeout,
										&tap);										// completion
		
		
        if ( ret ) 
		{
            if ( pb )
				IOFree(pb, sizeof(*pb));
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	else if ( arguments->structureInputDescriptor ) 
 	{
		// DeviceRequestOut, Sync, > 4K
        ret = target->DeviceRequestOut( (UInt8)arguments->scalarInput[1],			// bmRequestType,
										(UInt8)arguments->scalarInput[2],			// bRequest,
										(UInt16)arguments->scalarInput[3],			// wValue,
										(UInt16)arguments->scalarInput[4],			// wIndex,
										(UInt32)arguments->scalarInput[7],			// noDataTimeout,
										(UInt32)arguments->scalarInput[8],			// completionTimeout,
										arguments->structureInputDescriptor);		// IOMD for request
	}
    else
	{
		// DeviceRequestOut, Sync, < 4K
        ret = target->DeviceRequestOut(	 (UInt8)arguments->scalarInput[1],			// bmRequestType,
										(UInt8)arguments->scalarInput[2],			// bRequest,
										(UInt16)arguments->scalarInput[3],			// wValue,
										(UInt16)arguments->scalarInput[4],			// wIndex,
										(UInt32)arguments->scalarInput[7],			// noDataTimeout,
										(UInt32)arguments->scalarInput[8],			// completionTimeout,
										arguments->structureInput,					// bufferPtr
										arguments->structureInputSize);				// buffer size
	}

return ret;

}  


// This is an Async
IOReturn
IOUSBDeviceUserClientV2::DeviceRequestOut(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, mach_vm_size_t size, mach_vm_address_t buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion * completion)
{
	IOReturn				ret = kIOReturnSuccess;
	IOMemoryDescriptor *	mem = NULL;
	
	USBLog(7, "IOUSBDeviceUserClientV2::DeviceRequestOut (Async) : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x, noDataTimeout = %d completionTimeout = %d",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	if (fOwner && !isInactive())
    {
		if (( completion == NULL) )
		{
			USBLog(1,"IOUSBDeviceUserClientV2[%p]::DeviceRequestOut (async) no completion!",  this); 
			USBTrace( kUSBTDeviceUserClient,  kTPDeviceUCDeviceRequestOut, bmRequestType, bRequest, wValue, kIOReturnBadArgument );
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		// This is an Async request 
		IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
		
		if ( size != 0 )
		{
			USBLog(7,"IOUSBDeviceUserClientV2[%p]::DeviceRequestOut (async) creating IOMD",  this); 
			mem = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionOut, fTask);
			if (!mem)
			{
				USBLog(1,"IOUSBDeviceUserClientV2[%p]::DeviceRequestOut (async ) IOMemoryDescriptor::withAddressRange returned NULL",  this); 
				USBTrace( kUSBTDeviceUserClient,  kTPDeviceUCDeviceRequestOut, (uintptr_t)this, (uintptr_t)buffer, size, kIOReturnNoMemory );
				ret = kIOReturnNoMemory;
				goto Exit;
			}
			
			ret = mem->prepare();
			if ( ret != kIOReturnSuccess)
			{
				USBLog(3,"IOUSBDeviceUserClientV2[%p]::DeviceRequestOut (async) mem->prepare() returned 0x%x",  this, ret); 
				mem->release();
				goto Exit;
			}
		}
		
		pb->req.bmRequestType = bmRequestType;
		pb->req.bRequest = bRequest;
		pb->req.wValue = wValue;
		pb->req.wIndex = wIndex;
		pb->req.wLength = size;
		pb->req.pData = mem;
		
		pb->fMax = size;
		pb->fMem = mem;
		
		ret = fOwner->DeviceRequest(&pb->req, noDataTimeout, completionTimeout, completion);
		if ( ret != kIOReturnSuccess)
		{
			USBLog(5,"IOUSBDeviceUserClientV2[%p]::DeviceRequestOut (async) returned 0x%x",  this, ret); 
			if (mem != NULL)
			{
				mem->complete();
				mem->release();
			}
		}
	}
	
Exit:
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceRequestOut (async) - returning err %x",  this, ret);
	}
	
	return ret;
}

// This is an sync with < 4K request
IOReturn
IOUSBDeviceUserClientV2::DeviceRequestOut(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, const void *requestBuffer, uint32_t size)
{
	IOReturn				ret = kIOReturnSuccess;
	IOUSBDevRequest			req;
	
	USBLog(7, "IOUSBDeviceUserClientV2::DeviceRequestOut < 4K : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4x noDataTimeout = %d completionTimeout = %d",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   size,
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	IncrementOutstandingIO();
	
	if (fOwner && !isInactive())
	{
		// This is a sync request that is < 4K
		req.bmRequestType = bmRequestType;
		req.bRequest = bRequest;
		req.wValue = wValue;
		req.wIndex = wIndex;
		req.wLength = size;
		req.pData = (void *)requestBuffer;
		req.wLenDone = 0;
		
		ret = fOwner->DeviceRequest(&req, noDataTimeout, completionTimeout);
		
	}
	else
		ret = kIOReturnNotAttached;
	
	if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceRequestOut - returning err %x",  this, ret);
	}

	DecrementOutstandingIO();
	return ret;
}

//  This is a DeviceReqOut (sync) that is OOL, so it has an IOMD
IOReturn
IOUSBDeviceUserClientV2::DeviceRequestOut(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem)
{
	IOUSBDevRequestDesc		req;
	IOReturn				ret;
	
	USBLog(7, "IOUSBDeviceUserClientV2::DeviceRequestOut > 4K: bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4qx noDataTimeout = %d completionTimeout = %d",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   (uint64_t)mem->getLength(),
		   (uint32_t)noDataTimeout,
		   (uint32_t)completionTimeout);
	
	if ( mem == NULL )
	{
		USBLog(3,"+IOUSBDeviceUserClientV2[%p]::DeviceRequestOut > 4K  mem was NULL, returning kIOReturnBadArgument",  this);
		return kIOReturnBadArgument;
	}

	IncrementOutstandingIO();
	if (fOwner && !isInactive())
	{
		ret = mem->prepare();
		
		if (ret == kIOReturnSuccess)
		{
			req.bmRequestType = bmRequestType;
			req.bRequest = bRequest;
			req.wValue = wValue;
			req.wIndex = wIndex;
			req.wLength = mem->getLength();
			req.pData = mem;
			req.wLenDone = 0;
			
			ret = fOwner->DeviceRequest(&req,noDataTimeout, completionTimeout);
			
			mem->complete();
		}
		else
		{
			USBLog(4,"IOUSBDeviceUserClientV2[%p]::DeviceRequestOut > 4K mem->prepare() returned 0x%x",  this, ret); 
		}
	}
	else
		ret = kIOReturnNotAttached;
	
	if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceReqOut (sync OOL) - returning err %x",  this, ret);
	}
	
	DecrementOutstandingIO();	
	return ret;
}


#pragma mark Configuration

IOReturn IOUSBDeviceUserClientV2::_SetConfiguration(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+%s[%p]::_SetConfiguration", target->getName(), target);
    return target->SetConfiguration((UInt8)arguments->scalarInput[0]);
}

IOReturn
IOUSBDeviceUserClientV2::SetConfiguration(UInt8 configIndex)
{
    IOReturn	ret;
	
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::SetConfiguration to %d",  this, configIndex);
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
		ret = fOwner->SetConfiguration(this, configIndex);
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::SetConfiguration - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}

IOReturn IOUSBDeviceUserClientV2::_GetConfiguration(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+%s[%p]::_GetConfiguration", target->getName(), target);
    return target->GetConfiguration(&(arguments->scalarOutput[0]));
}

IOReturn
IOUSBDeviceUserClientV2::GetConfiguration(uint64_t *configValue)
{
    IOReturn	ret;
	UInt8		theConfig = 255;
	
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::GetConfiguration",  this);
   IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
		ret = fOwner->GetConfiguration(&theConfig);
    else
        ret = kIOReturnNotAttached;
	
	*configValue = theConfig;
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetConfiguration - returning err %x",  this, ret);
	}

	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::GetConfiguration returns 0x%qx",  this, *configValue);

    DecrementOutstandingIO();
	return ret;
}


IOReturn 
IOUSBDeviceUserClientV2::_GetConfigDescriptor(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_GetConfigDescriptor", target);

    if ( arguments->structureOutputDescriptor ) 
        return target->GetConfigDescriptor((UInt8)arguments->scalarInput[0], arguments->structureOutputDescriptor, &(arguments->structureOutputDescriptorSize));
    else
        return target->GetConfigDescriptor((UInt8)arguments->scalarInput[0], (IOUSBConfigurationDescriptorPtr) arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
}  


IOReturn
IOUSBDeviceUserClientV2::GetConfigDescriptor(UInt8 configIndex, IOUSBConfigurationDescriptorPtr desc, UInt32 *size)
{
    UInt16 				length;
    const IOUSBConfigurationDescriptor	*cached;
    IOReturn				ret;
    
    USBLog(7,"+IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor (Config %d), with size %d, struct: %p",  this, configIndex, (uint32_t)*size, desc);
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		cached = fOwner->GetFullConfigurationDescriptor(configIndex);
		if ( cached == NULL )
		{
			USBLog(5,"+IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor GetFullConfigurationDescriptor returned NULL",  this);
			desc = NULL;
			ret = kIOReturnNotFound;
		}
		else
		{
			USBLog(7,"+IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor  got descriptor %p, length: %d",  this, cached, USBToHostWord(cached->wTotalLength));
			length = USBToHostWord(cached->wTotalLength);
			if (length < *size)
				*size = length;
			bcopy(cached, desc, *size);
			ret = kIOReturnSuccess;
		}
    }
    else
        ret = kIOReturnNotAttached;
	
   	if (ret)
	{
		USBLog(5, "IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
	return ret;
}

IOReturn
IOUSBDeviceUserClientV2::GetConfigDescriptor(UInt8 configIndex, IOMemoryDescriptor * mem, uint32_t *size)
{
    UInt16 				length;
    const IOUSBConfigurationDescriptor	*cached;
    IOReturn				ret;
    
    USBLog(7,"+IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor > 4K (Config %d), with size %d, mem: %p",  this, configIndex, *size, mem);
	
	if ( mem == NULL )
	{
		USBLog(3,"+IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor > 4K  mem was NULL, returning kIOReturnBadArgument",  this);
		return kIOReturnBadArgument;
	}

    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		cached = fOwner->GetFullConfigurationDescriptor(configIndex);
		if ( cached == NULL )
		{
			USBLog(5,"+IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor > 4K GetFullConfigurationDescriptor returned NULL",  this);
			ret = kIOReturnNotFound;
		}
		else
		{
			USBLog(7,"+IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor > 4K  got descriptor %p, length: %d",  this, cached, USBToHostWord(cached->wTotalLength));
			length = USBToHostWord(cached->wTotalLength);
			if (length < *size)
			{
				*size = length;
			}

			mem->prepare();
			mem->writeBytes(0, cached, *size);
			mem->complete();
			
			ret = kIOReturnSuccess;
		}
    }
    else
        ret = kIOReturnNotAttached;
	
   	if (ret)
	{
		USBLog(5, "IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
	return ret;

}	


#pragma mark State
IOReturn IOUSBDeviceUserClientV2::_ResetDevice(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference, arguments)
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_ResetDevice", target);
	return target->ResetDevice();
}  

IOReturn
IOUSBDeviceUserClientV2::ResetDevice()
{
    IOReturn	ret;
    
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::ResetDevice",  this);
    IncrementOutstandingIO();
	
    if (fOwner && !isInactive())
		ret = fOwner->ResetDevice();
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::ResetDevice - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBDeviceUserClientV2::_SuspendDevice(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_SuspendDevice", target);
    return target->SuspendDevice((bool)arguments->scalarInput[0]);
}

IOReturn
IOUSBDeviceUserClientV2::SuspendDevice(bool suspend)
{
    IOReturn 	ret;
    
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::SuspendDevice (%s)",  this, suspend?"suspend":"resume");
    IncrementOutstandingIO();

    if (fOwner && !isInactive())
	ret = fOwner->SuspendDevice(suspend);
    else
        ret = kIOReturnNotAttached;

    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::SuspendDevice - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBDeviceUserClientV2::_ReEnumerateDevice(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_ReEnumerateDevice", target);
    return target->ReEnumerateDevice((UInt32)arguments->scalarInput[0]);
}


IOReturn
IOUSBDeviceUserClientV2::ReEnumerateDevice(UInt32 options)
{
    IOReturn 	ret;

 	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::ReEnumerateDevice with options 0x%x",  this, (uint32_t)options);
	retain();
    
    if (fOwner && !isInactive())
		ret = fOwner->ReEnumerateDevice(options);
    else
		ret = kIOReturnNotAttached;

    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::ReEnumerateDevice - returning err %x",  this, ret);
	}
	
    release();
	
    return ret;
}


IOReturn IOUSBDeviceUserClientV2::_AbortPipeZero(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference, arguments)
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_AbortPipeZero", target);
	return target->AbortPipeZero();
}  

IOReturn
IOUSBDeviceUserClientV2::AbortPipeZero(void)
{
    IOReturn 	ret;
	
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::AbortPipeZero",  this);
    IncrementOutstandingIO();
	
    if (fOwner && !isInactive())
    {
		IOUSBPipe *pz = fOwner->GetPipeZero();
		
		if (pz)
			ret = pz->Abort();
		else
			ret = kIOReturnBadArgument;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::AbortPipeZero - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}

#pragma mark -
IOReturn IOUSBDeviceUserClientV2::_GetDeviceInformation(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::_GetDeviceInformation", target);
    return target->GetDeviceInformation(&(arguments->scalarOutput[0]));
}

IOReturn
IOUSBDeviceUserClientV2::GetDeviceInformation(uint64_t *info)
{
    IOReturn	ret;
	UInt32		deviceInfo = 0;
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::GetDeviceInformation",  this);
	IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
		ret = fOwner->GetDeviceInformation(&deviceInfo);
    else
        ret = kIOReturnNotAttached;
	
	*info = deviceInfo;
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetDeviceInformation - returning err %x",  this, ret);
	}
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::GetDeviceInformation returns 0x%qx",  this, *info);
	
    DecrementOutstandingIO();
	return ret;
}

IOReturn IOUSBDeviceUserClientV2::_RequestExtraPower(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::_RequestExtraPower", target);
    return target->RequestExtraPower((UInt32)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], &(arguments->scalarOutput[0]));
}

IOReturn
IOUSBDeviceUserClientV2::RequestExtraPower(UInt32 type, UInt32 requestedPower, uint64_t *powerAvailable)
{
	IOReturn	ret = kIOReturnSuccess;
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::RequestExtraPower  requesting type %d power = %d",  this, (uint32_t) type, (uint32_t) requestedPower);
	IncrementOutstandingIO();
    
	*powerAvailable = 0;
	
    if (fOwner && !isInactive())
	{
		*powerAvailable = (uint64_t) fOwner->RequestExtraPower(type, requestedPower);
		
		if ( type == kUSBPowerDuringSleep )
			FSLEEPPOWERALLOCATED += (UInt32) *powerAvailable;

		if ( type == kUSBPowerDuringWake )
			FWAKEPOWERALLOCATED += (UInt32) *powerAvailable;

	}
    else
        ret = kIOReturnNotAttached;
		
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::RequestExtraPower - returning err %x",  this, ret);
	}
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::RequestExtraPower returns %qd",  this, *powerAvailable);
	
    DecrementOutstandingIO();
	return ret;
}

IOReturn IOUSBDeviceUserClientV2::_ReturnExtraPower(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(5, "+%s[%p]::_ReturnExtraPower", target->getName(), target);
    return target->ReturnExtraPower((UInt32)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1]);
}

IOReturn
IOUSBDeviceUserClientV2::ReturnExtraPower(UInt32 type, UInt32 returnedPower)
{
    IOReturn	ret = kIOReturnSuccess;
	
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::ReturnExtraPower returning type %d power = %d",  this, (uint32_t) type, (uint32_t) returnedPower);
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
		ret = fOwner->ReturnExtraPower(type, returnedPower);
    else
        ret = kIOReturnNotAttached;
	
    if (ret == kIOReturnSuccess)
	{
		if ( type == kUSBPowerDuringSleep )
			FSLEEPPOWERALLOCATED -= returnedPower;
		
		if ( type == kUSBPowerDuringWake )
			FWAKEPOWERALLOCATED -= returnedPower;
	}
	else
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::ReturnExtraPower - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}

IOReturn IOUSBDeviceUserClientV2::_GetExtraPowerAllocated(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::_GetExtraPowerAllocated", target);
    return target->GetExtraPowerAllocated((UInt32)arguments->scalarInput[0], &(arguments->scalarOutput[0]));
}

IOReturn
IOUSBDeviceUserClientV2::GetExtraPowerAllocated(UInt32 type, uint64_t *powerAllocated)
{
	IOReturn	ret = kIOReturnSuccess;
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::GetExtraPowerAllocated  requesting type %d",  this, (uint32_t) type);
	IncrementOutstandingIO();
    
	*powerAllocated = 0;
	
    if (fOwner && !isInactive())
	{
		*powerAllocated = (uint64_t) fOwner->GetExtraPowerAllocated(type);
		
	}
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetExtraPowerAllocated - returning err %x",  this, ret);
	}
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::GetExtraPowerAllocated returns %qd",  this, *powerAllocated);
	
    DecrementOutstandingIO();
	return ret;
}



#pragma mark FrameNumber
IOReturn IOUSBDeviceUserClientV2::_GetFrameNumber(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_GetFrameNumber", target);
	return target->GetFrameNumber((IOUSBGetFrameStruct *)arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
}  

IOReturn 
IOUSBDeviceUserClientV2::GetFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size)
{
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::GetFrameNumber",  this);
	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		uint64_t	currentTime = mach_absolute_time();
		data->timeStamp = * (AbsoluteTime *)&currentTime;
		data->frame = fOwner->GetBus()->GetFrameNumber();
		USBLog(6,"IOUSBDeviceUserClientV2::GetFrameNumber frame: 0x%qx, timeStamp: 0x%qx", data->frame, currentTime);
		*size = sizeof(IOUSBGetFrameStruct);
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetFrameNumber - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBDeviceUserClientV2::_GetMicroFrameNumber(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_GetMicroFrameNumber", target);
	return target->GetMicroFrameNumber((IOUSBGetFrameStruct *)arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
}  

IOReturn
IOUSBDeviceUserClientV2::GetMicroFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size)
{
    // This method only available for v2 controllers
    //
    IOUSBControllerV2	*v2 = NULL;
    IOReturn		ret = kIOReturnSuccess;
    
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::GetMicroFrameNumber",  this);
	if (fOwner)
		v2 = OSDynamicCast(IOUSBControllerV2, fOwner->GetBus());
    
    if (!v2)
    {
        USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetMicroFrameNumber - Not a USB 2.0 controller!  Returning 0x%x",  this, kIOReturnNotAttached);
        return kIOReturnNotAttached;
    }
    	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		UInt64	microFrame;
 		uint64_t	currentTime = mach_absolute_time();
		data->timeStamp = * (AbsoluteTime *)&currentTime;
        microFrame = v2->GetMicroFrameNumber();
		if ( microFrame != 0)
		{
			data->frame = microFrame;
			USBLog(6,"IOUSBDeviceUserClientV2::GetMicroFrameNumber frame: 0x%qx, timeStamp:  0x%qx", data->frame, currentTime);
			*size = sizeof(IOUSBGetFrameStruct);
		}
		else
		{
			ret = kIOReturnUnsupported;
			*size = 0;
		}
    }
    else
    {
        USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetMicroFrameNumber - no fOwner(%p) or isInactive",  this, fOwner);
        ret = kIOReturnNotAttached;
    }
    
    if (ret)
	{
        USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetFrameNumber - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBDeviceUserClientV2::_GetFrameNumberWithTime(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_GetFrameNumberWithTime", target);
	return target->GetFrameNumberWithTime( (IOUSBGetFrameStruct*) arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
}  

IOReturn 
IOUSBDeviceUserClientV2::GetFrameNumberWithTime(IOUSBGetFrameStruct *data, UInt32 *size)
{
    IOReturn		ret = kIOReturnSuccess;
	
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::GetFrameNumberWithTime",  this);
	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		IOUSBControllerV2		*busV2 = OSDynamicCast(IOUSBControllerV2, fOwner->GetBus());
		if (busV2)
		{
			ret = busV2->GetFrameNumberWithTime(&data->frame, &data->timeStamp);
			USBLog(6,"IOUSBDeviceUserClientV2::GetFrameNumberWithTime frame: 0x%qx, timeStamp: 0x%qx", data->frame, AbsoluteTime_to_scalar(&data->timeStamp));
			*size = sizeof(IOUSBGetFrameStruct);
		}
		else
		{
			ret = kIOReturnUnsupported;
		}
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetFrameNumberWithTime - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}


#pragma mark Iterator
IOReturn IOUSBDeviceUserClientV2::_CreateInterfaceIterator(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOUSBFindInterfaceRequest		reqIn;
	IOReturn						ret;
	
	reqIn.bInterfaceClass = (UInt16)arguments->scalarInput[0];
	reqIn.bInterfaceSubClass = (UInt16)arguments->scalarInput[1];
	reqIn.bInterfaceProtocol = (UInt16)arguments->scalarInput[2];
	reqIn.bAlternateSetting = (UInt16)arguments->scalarInput[3];
	
	return target->CreateInterfaceIterator(&reqIn, &(arguments->scalarOutput[0]));
	
	return ret;
}  

IOReturn 
IOUSBDeviceUserClientV2::CreateInterfaceIterator(IOUSBFindInterfaceRequest *reqIn, uint64_t *returnIter)
{
    OSIterator		*iter;
	io_object_t		iterOut;
    IOReturn		ret = kIOReturnSuccess;
	
 	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::CreateInterfaceIterator   bInterfaceClass 0x%x, bInterfaceSubClass = 0x%x, bInterfaceProtocol = 0x%x, bAlternateSetting = 0x%x",  this,
		   reqIn->bInterfaceClass,
		   reqIn->bInterfaceSubClass,
		   reqIn->bInterfaceProtocol,
		   reqIn->bAlternateSetting);
	
	IncrementOutstandingIO();
	
	// Check for inCount size?
	
    if (fOwner && !isInactive())
    {
		iter = fOwner->CreateInterfaceIterator(reqIn);
		
		if (iter) 
		{
			USBLog(8, "IOUSBDeviceUserClientV2[%p]::CreateInterfaceIterator   CreateInterfaceIterator returned %p", this, iter);
			ret = exportObjectToClient(fTask, iter, &iterOut);
			*returnIter = (uint64_t) iterOut;
			USBLog(8, "IOUSBDeviceUserClientV2[%p]::CreateInterfaceIterator   exportObjectToClient returned 0x%x, iterOut = 0x%qx", this, ret, *returnIter);
		}
		else
		{
			USBLog(5, "IOUSBDeviceUserClientV2[%p]::CreateInterfaceIterator   CreateInterfaceIterator returned NULL",  this);

			ret = kIOReturnNoMemory;
		}
    }
    else
	{
		USBLog(5, "IOUSBDeviceUserClientV2[%p]::CreateInterfaceIterator   returning kIOReturnNotAttached (0x%x)",  this, kIOReturnNotAttached);
		ret = kIOReturnNotAttached;
	}
    
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::CreateInterfaceIterator - returning err %x",  this, ret);
	}
	
    DecrementOutstandingIO();
	return ret;
}


#pragma mark IOKit Methods
IOReturn 
IOUSBDeviceUserClientV2::clientClose( void )
{
	IOReturn	ret = kIOReturnSuccess;
	
    USBLog(6, "+IOUSBDeviceUserClientV2[%p]::clientClose(), isInactive = %d",  this, isInactive());

	if ( !fGate )
	{
		ret = ClientClose();
	}
	else 
	{
		ret = fGate->runAction(ClientCloseGated);
	}

    USBLog(6, "-IOUSBDeviceUserClientV2[%p]::clientClose 0x%x",  this, ret);
    return ret;		// DONT call super::clientClose, which just returns notSupported
}


IOReturn 
IOUSBDeviceUserClientV2::clientDied( void )
{
    IOReturn ret;
    
    USBLog(6, "+IOUSBDeviceUserClientV2[%p]::clientDied",  this);
	
    retain();                       // We will release once any outstandingIO is finished
    
    fDead = true;					// don't send mach messages in this case
	
	ret = super::clientDied();		// this just calls clientClose
	
    USBLog(6, "-IOUSBDeviceUserClientV2[%p]::clientDied, ret = 0x%x",  this, ret);
	
    return ret;
}


//
// stop
// 
// This IOService method is called AFTER we have closed our provider, assuming that the provider was 
// ever opened. If we issue I/O to the provider, then we must have it open, and we will not close
// our provider until all of that I/O is completed.
void 
IOUSBDeviceUserClientV2::stop(IOService * provider)
{
    
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::stop(%p)",  this, provider);

	if (fWorkLoop && fGate)
		fWorkLoop->removeEventSource(fGate);
	
	if ( fOwner )
		fOwner->release();
	
	release();
	
	super::stop(provider);

    USBLog(7, "-IOUSBDeviceUserClientV2[%p]::stop(%p)",  this, provider);

}



void 
IOUSBDeviceUserClientV2::free()
{
    if (fGate)
    {
		fGate->release();
		fGate = NULL;
    }

	if (fWorkLoop)
	{
		fWorkLoop->release();
		fWorkLoop = NULL;
	}
		
    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
	IOFree(fIOUSBDeviceUserClientExpansionData, sizeof(IOUSBDeviceUserClientExpansionData));
	fIOUSBDeviceUserClientExpansionData = NULL;

    super::free();
}


// This is an IOKit method which is called AFTER we close our parent, but BEFORE stop.
bool 
IOUSBDeviceUserClientV2::finalize( IOOptionBits options )
{
    bool ret;

    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::finalize(%08x)",  this, (int)options);
    
    ret = super::finalize(options);
    
    USBLog(7, "-IOUSBDeviceUserClientV2[%p]::finalize(%08x) - returning %s",  this, (int)options, ret ? "true" : "false");
    return ret;
}



// This is an IOKit method which lets us know that out PARENT is terminating, usually indicating that the
// USB device has been removed.
bool
IOUSBDeviceUserClientV2::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that 
    // we have begun getting our callbacks in order. by the time we get here, the 
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    USBLog(7, "IOUSBDeviceUserClientV2[%p]::willTerminate isInactive = %d",  this, isInactive());

    //  We have seen cases where our fOwner is not valid at this point.  This is strange
    //  but we'll code defensively and only execute if our provider (fOwner) is still around
    //
    if ( fOwner )
    {
        IncrementOutstandingIO();

        if ( (GetOutstandingIO() > 1) && fOwner )
        {
            IOUSBPipe *pipeZero = fOwner->GetPipeZero();

            if (pipeZero)
                (void) pipeZero->Abort();
        }

        DecrementOutstandingIO();
    }
    
    return super::willTerminate(provider, options);
}



bool
IOUSBDeviceUserClientV2::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
    USBLog(6, "IOUSBDeviceUserClientV2[%p]::didTerminate isInactive = %d, outstandingIO = %d",  this, isInactive(), fOutstandingIO);

    if ( fOwner && fOwner->isOpen(this) )
    {
        if ( fOutstandingIO == 0 )
		{
            fOwner->close(this);
			fNeedToClose = false;
			if ( isInactive() )
				fOwner = NULL;
		}
        else
            fNeedToClose = true;
    }
    
    return super::didTerminate(provider, options, defer);
}


IOReturn 
IOUSBDeviceUserClientV2::message( UInt32 type, IOService * provider,  void * argument )
{
#pragma unused (provider, argument)
    IOReturn	err = kIOReturnSuccess;
    
    switch ( type )
    {
        case kIOUSBMessagePortHasBeenSuspended:
			USBLog(6, "IOUSBDeviceUserClientV2[%p]::message - received kIOUSBMessagePortHasBeenSuspended",  this);
            break;
			
        case kIOUSBMessagePortHasBeenReset:
 			USBLog(6, "IOUSBDeviceUserClientV2[%p]::message - received kIOUSBMessagePortHasBeenReset",  this);
            break;
			
		case kIOUSBMessagePortHasBeenResumed:
			USBLog(6, "IOUSBDeviceUserClientV2[%p]::message - received kIOUSBMessagePortHasBeenResumed",  this);
            break;
			
        case kIOMessageServiceIsTerminated: 
			USBLog(6, "IOUSBDeviceUserClientV2[%p]::message - received kIOMessageServiceIsTerminated",  this);
            break;
            
        default:
            break;
    }
    
    return err;
}

#pragma mark Ref Counting

void
IOUSBDeviceUserClientV2::DecrementOutstandingIO(void)
{
    if (!fGate)
    {
		if (!--fOutstandingIO && fNeedToClose)
		{
			USBLog(3, "IOUSBDeviceUserClientV2[%p]::DecrementOutstandingIO isInactive(%s), outstandingIO = %d - closing device",  this, isInactive() ? "true" : "false" , (int)fOutstandingIO);
			
			if ( fOwner && fOwner->isOpen(this))
			{
				fOwner->close(this);
				fNeedToClose = false;
				if ( isInactive() )
					fOwner = NULL;
			}
			
            if ( fDead) 
			{
				fDead = false;
				release();
			}
		}
		return;
    }
	USBTrace( kUSBTOutstandingIO, kTPDeviceUCDecrement , (uintptr_t)this, (int)fOutstandingIO, 0, 0);
    fGate->runAction(ChangeOutstandingIO, (void*)-1);
}


void
IOUSBDeviceUserClientV2::IncrementOutstandingIO(void)
{
    if (!fGate)
    {
		fOutstandingIO++;
		return;
    }
	USBTrace( kUSBTOutstandingIO, kTPDeviceUCIncrement , (uintptr_t)this, (int)fOutstandingIO, 0, 0);
    fGate->runAction(ChangeOutstandingIO, (void*)1);
}



IOReturn
IOUSBDeviceUserClientV2::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param2, param3, param4)
   IOUSBDeviceUserClientV2 	*me = OSDynamicCast(IOUSBDeviceUserClientV2, target);
    UInt32						direction = (uintptr_t)param1;
    
    if (!me)
    {
		USBLog(1, "IOUSBDeviceUserClientV2::ChangeOutstandingIO - invalid target");
		USBTrace( kUSBTDeviceUserClient,  kTPDeviceUCChangeOutstandingIO, (uintptr_t)me, direction, kIOReturnSuccess, 1 );
		return kIOReturnSuccess;
    }
    switch (direction)
    {
		case 1:
			me->fOutstandingIO++;
			break;
			
		case -1:
			if (!--me->fOutstandingIO && me->fNeedToClose)
			{
				USBLog(6, "IOUSBDeviceUserClientV2[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me, me->isInactive(), me->fOutstandingIO);
				
				if (me->fOwner && me->fOwner->isOpen(me)) 
				{
					me->fOwner->close(me);
					me->fNeedToClose = false;
					if ( me->isInactive() )
						me->fOwner = NULL;
				}
				
                if ( me->fDead) 
				{
					me->fDead = false;
					me->release();
				}
			}
			break;
			
		default:
			USBLog(1, "IOUSBDeviceUserClientV2[%p]::ChangeOutstandingIO - invalid direction", me);
			USBTrace( kUSBTDeviceUserClient,  kTPDeviceUCChangeOutstandingIO, (uintptr_t)me, direction, 0, 2 );
    }
    return kIOReturnSuccess;
}

UInt32
IOUSBDeviceUserClientV2::GetOutstandingIO()
{
    UInt32	count = 0;
    
    if (!fGate)
    {
		return fOutstandingIO;
    }
    
    fGate->runAction(GetGatedOutstandingIO, (void*)&count);
    
    return count;
}

IOReturn
IOUSBDeviceUserClientV2::GetGatedOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param2, param3, param4)
    IOUSBDeviceUserClientV2 *me = OSDynamicCast(IOUSBDeviceUserClientV2, target);
    
    if (!me)
    {
	USBLog(1, "IOUSBDeviceUserClientV2::GetGatedOutstandingIO - invalid target");
	USBTrace( kUSBTDeviceUserClient,  kTPDeviceUCGetGatedOutstandingIO, (uintptr_t)me, kIOReturnSuccess, 0, 0 );
	return kIOReturnSuccess;
    }

    *(UInt32 *) param1 = me->fOutstandingIO;

    return kIOReturnSuccess;
}


//================================================================================================
//
//   ClientClose
//
//   Notice the capital "C" in the method name.  This method is called by our implementation of 
//   IOKit's clientClose().  Usually we call it through the gate using ClientCloseGated(), but in case
//   we don't have an fGate, we call it directly.
//
//================================================================================================
//
IOReturn 
IOUSBDeviceUserClientV2::ClientClose( void )
{
    USBLog(6, "+IOUSBDeviceUserClientV2[%p]::ClientClose()",  this);
	
	// Sleep for 1 ms to allow other threads that are pending to run
	//
	IOSleep(1);
	
	// If we are already inactive, it means that our IOUSBDevice is going/has gone away.  In that case
	// we really do not need to do anything as the IOKit termination will take care of cleaning things.
	if ( !isInactive() )
	{
		if ( fOutstandingIO == 0 )
		{
			USBLog(6, "+IOUSBDeviceUserClientV2[%p]::ClientClose closing provider, setting fNeedToClose to false",  this);
			if ( fOwner && fOwner->isOpen(this) )
			{
				// Since this is call that tells us that our user space client has gone away, we can
				// close our provider.  We don't set it to NULL because the IOKit object representing
				// it has not gone away.  That will come in thru did/willTerminate.  Also, we should
				// be checking whether fOwner was open before closing it, but we will do that later.
				fOwner->close(this);
				fNeedToClose = false;
				
			}
			if ( fDead) 
			{
				fDead = false;
				release();
			}
		}
		else
		{
			USBLog(5, "+IOUSBDeviceUserClientV2[%p]::ClientClose will close provider later",  this);
			fNeedToClose = true;
		}
		
		fTask = NULL;
		
		terminate();			// this will call stop eventually
	}
	
	USBLog(7, "-IOUSBDeviceUserClientV2[%p]::ClientClose()",  this);
	
	return kIOReturnSuccess;
}


//================================================================================================
//
//   ClientCloseGated
//
//	 This is called by clientClose through the USB workloop.  Doing so will avoid race conditions
//   when a user space app is shutting down a connetion to the user client and a device is falling
//   off the bus.
//
//================================================================================================
//
IOReturn
IOUSBDeviceUserClientV2::ClientCloseGated(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param1, param2, param3, param4)
	IOUSBDeviceUserClientV2 	*me = OSDynamicCast(IOUSBDeviceUserClientV2, target);
	
	if (!me)
	{
		USBLog(1, "IOUSBDeviceUserClientV2::ClientCloseGated - invalid target");
		return kIOReturnSuccess;
	}
	
	USBLog(6, "+IOUSBDeviceUserClientV2[%p]::ClientCloseGated",  me);
	
	return me->ClientClose();
	
}
	
	
#pragma mark Debugging

void
IOUSBDeviceUserClientV2::PrintExternalMethodArgs( IOExternalMethodArguments * arguments, UInt32 level )
{
#pragma unused (level)
	USBLog(level,"ExternalMethodArguments\n\tversion:\t0x%x\n\tselector:\t0x%x\n\tasyncWakePort:\t%p\n\tasyncReference:\t%p\n\tasyncReferenceCount:0x%x\n\tscalarInput:\t%p\n\tscalarInputCount:\t0x%x\n\tstructureInput:\t%p\n\tstructureInputSize:\t0x%x\n\tstructureInputMemoryDesc:\t%p\n\tscalarOutput:\t%p\n\tscalarOutputSize:\t0x%x\n\tstructureOutput:\t%p\n\tstructureOutputSize:\t0x%x\n\tstructureOutputDescriptor:\t%p\n\tstructureOutputDescriptorSize:\t0x%x\n",
	arguments->version,
	arguments->selector,
	arguments->asyncWakePort,
	arguments->asyncReference,
	arguments->asyncReferenceCount,
	arguments->scalarInput,
	arguments->scalarInputCount,
	arguments->structureInput,
	arguments->structureInputSize,
	arguments->structureInputDescriptor,
	arguments->scalarOutput,
	arguments->scalarOutputCount,
	arguments->structureOutput,
	arguments->structureOutputSize,
	arguments->structureOutputDescriptor,
	arguments->structureOutputDescriptorSize
	);

	for ( uint32_t i = 0; i < arguments->scalarInputCount; i++ )
	{
		USBLog(level,"\targuments->scalarInput[%d]: 0x%qx", i,  arguments->scalarInput[i]);
	}
}
#pragma mark Deprecated Methods

IOReturn 
IOUSBDeviceUserClientV2::CreateInterfaceIterator(IOUSBFindInterfaceRequest *reqIn, io_object_t *iterOut, IOByteCount inCount, IOByteCount *outCount)
{
#pragma unused (reqIn, iterOut, inCount, outCount)
	USBLog(3, "%s[%p]::CreateInterfaceIterator called DEPRECATED version of the method", getName(), this);
	
	return kIOReturnUnsupported;
}

#pragma mark Padding Methods
// padding methods
//
OSMetaClassDefineReservedUsed(IOUSBDeviceUserClientV2,  0);
OSMetaClassDefineReservedUsed(IOUSBDeviceUserClientV2,  1);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2,  2);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2,  3);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2,  4);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2,  5);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2,  6);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2,  7);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2,  8);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2,  9);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2, 10);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2, 11);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2, 12);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2, 13);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2, 14);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2, 15);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2, 16);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2, 17);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2, 18);
OSMetaClassDefineReservedUnused(IOUSBDeviceUserClientV2, 19);

