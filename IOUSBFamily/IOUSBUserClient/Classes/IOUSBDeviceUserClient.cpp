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
#define FOPENED_FOR_EXCLUSIVEACCESS				fIOUSBDeviceUserClientExpansionData->fOpenedForExclusiveAccess
#define FDELAYED_WORKLOOP_FREE					fIOUSBDeviceUserClientExpansionData->fDelayedWorkLoopFree
#define FOWNER_WAS_RELEASED						fIOUSBDeviceUserClientExpansionData->fOwnerWasReleased


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
    ,{	 //    kUSBDeviceUserClientGetBandwidthAvailableForDevice
		(IOExternalMethodAction) &IOUSBDeviceUserClientV2::_GetBandwidthAvailableForDevice,
		0, 0,
		1, 0
    },
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
	
	if (fWakePort != MACH_PORT_NULL)
	{
		super::releaseNotificationPort(fWakePort);
		fWakePort = MACH_PORT_NULL;
	}

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
	
	releaseAsyncReference64(pb->fAsyncRef);
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
    fDead = false;
	fWakePort = MACH_PORT_NULL;

#if DEBUG_LEVEL != 0
	char	nbuf[256];
	proc_t	p = proc_self();
	int		owningPID = proc_pid(p);
	
	proc_name(owningPID,nbuf,256);
	
	USBLog(5,"IOUSBDeviceUserClientV2[%p]::initWithTask  Owning PID is %d, name is %s", this, owningPID, nbuf);
	
	proc_rele(p);
	
#endif
		
    return true;
}


bool 
IOUSBDeviceUserClientV2::start( IOService * provider )
{
    IOWorkLoop	*		workLoop = NULL;
    IOCommandGate *		commandGate = NULL;
	
    USBLog(6, "+IOUSBDeviceUserClientV2[%p]::start(%p)",  this, provider);
	
	// See comment below for when this is release()'d. 
	retain();
	
	if (!super::start(provider))
    {
		USBError(1, "IOUSBDeviceUserClientV2[%p]::start - super::start returned false!",  this);
		release();
		return false;
    }
    
    fOwner = OSDynamicCast(IOUSBDevice, provider);
    if (!fOwner)
    {
		USBError(1, "IOUSBDeviceUserClientV2[%p]::start - provider is NULL!",  this);
		release();
        return false;
    }
    
    // We will retain our owner/provider (above) and ourselves here.  We will release those when
    // we are done:  In clientCloseGated() when our user space client goes away (nicely or not), or in didTerminate() 
    // when our iokit device goes away.  If we have i/o pending when those 2 methods are called, we will do the release's
    // in Decrement/ChangeOutstandingIO() 
    
	fOwner->retain();
	FOWNER_WAS_RELEASED = false;
	
 	// Now, open() our provider, but not exclusively.  This will keep us from being release()'d until we close() it
	if ( !fOwner->open(this))
	{
		USBError(1, "IOUSBDeviceUserClientV2[%p]::start -  could not open() our provider(%p)!",  this, fOwner);
		if ( !FOWNER_WAS_RELEASED )
		{
			USBLog(6, "IOUSBDeviceUserClientV2[%p]::start -  releasing fOwner(%p) and UC!",  this, fOwner);
			FOWNER_WAS_RELEASED = true;
			fOwner->release();
			release();
		}
		return false;
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
		
	if ( isInactive() )
	{
		USBLog(6, "IOUSBDeviceUserClientV2[%p]::start  we are inActive, so bailing out",  this);
        goto ErrorExit;
	}
	
	// Now that we have succesfully added our gate to the workloop, set our member variables
    //
    fGate = commandGate;
    fWorkLoop = workLoop;
    
	USBLog(6, "-IOUSBDeviceUserClientV2[%p]::start",  this);
	
    return true;
    
ErrorExit:
		
	USBLog(6, "IOUSBDeviceUserClientV2[%p]::start  ErrorExit",  this);
	
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
	
	if ( !FOWNER_WAS_RELEASED )
	{
		FOWNER_WAS_RELEASED = true;
		if (fOwner)
		{
			fOwner->close(this);
			fOwner->release();
		}
		
		release();
	}
	
    return false;
}



IOReturn IOUSBDeviceUserClientV2::_open(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
 	
	target->retain();
	IOReturn kr = target->open((bool)arguments->scalarInput[0]);
	target->release();
	
	return kr;
}

IOReturn 
IOUSBDeviceUserClientV2::open(bool seize)
{
    IOOptionBits	options = (seize ? (IOOptionBits)kIOServiceSeize : 0);
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::open",  this);
    IncrementOutstandingIO();
	
	// Open our provider exclusively, to be closed in ::close()
    if (fOwner)
    {
		if (fOwner->open(this, options | kUSBOptionBitOpenExclusivelyMask))
		{
			FOPENED_FOR_EXCLUSIVEACCESS = true;
		}
		else
		{
			USBLog(5, "IOUSBDeviceUserClientV2[%p]::open fOwner->open(kUSBOptionBitOpenExclusivelyMask) failed.  Returning kIOReturnExclusiveAccess",  this);
			ret = kIOReturnExclusiveAccess;
		}
    }
    else
        ret = kIOReturnNotAttached;
	
    USBLog(7, "-IOUSBDeviceUserClientV2[%p]::open - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));    

    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBDeviceUserClientV2::_close(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference, arguments)
	IOReturn		kr;
	
	target->retain();
	
	if (!target->isInactive() && target->fGate && target->fWorkLoop)
	{
		IOCommandGate *	gate = target->fGate;
		IOWorkLoop *	workLoop = target->fWorkLoop;

		workLoop->retain();
		gate->retain();
		
		kr = gate->runAction(closeGated);
		if ( kr != kIOReturnSuccess)
		{
			USBLog(3, "IOUSBDeviceUserClientV2[%p]::_close  runAction returned 0x%x, isInactive(%s)",  target, kr, target->isInactive() ? "true" : "false");
			
		}
		gate->release();
		workLoop->release();
	}
	else
	{
		USBLog(1, "+IOUSBDeviceUserClientV2[%p]::_close  no fGate, calling close() directly", target);
		kr = target->close();
	}
	
	target->release();
	
	return kr;
}


IOReturn  
IOUSBDeviceUserClientV2::closeGated(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param1, param2, param3, param4)
	IOUSBDeviceUserClientV2 *			me = OSDynamicCast(IOUSBDeviceUserClientV2, target);
    if (!me)
    {
		USBLog(1, "IOUSBDeviceUserClientV2::closeGated - invalid target");
		return kIOReturnBadArgument;
    }
	
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::closeGated",  me);
	return me->close();
}


// This is NOT the normal IOService::close(IOService*) method. It is intended to handle a close coming 
// in from the user side. Since we do not have any IOKit objects as children, we will just use this to
// terminate ourselves.
IOReturn 
IOUSBDeviceUserClientV2::close()
{
    IOReturn 	ret = kIOReturnSuccess;
    
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::close",  this);
    
    if (fOwner)
	{
		if (fOutstandingIO > 0)
		{
			// we need to abort any outstanding IO to allow us to close
			USBLog(5, "IOUSBDeviceUserClientV2[%p]::close - outstanding IO - aborting pipe zero",  this);
			AbortPipeZero();
		}
		
		if (FOPENED_FOR_EXCLUSIVEACCESS)
		{
			IOOptionBits	options = kUSBOptionBitOpenExclusivelyMask;
			
			FOPENED_FOR_EXCLUSIVEACCESS = false;
			
			fOwner->close(this, options);
		}
		else
		{
			USBLog(5, "IOUSBDeviceUserClientV2[%p]::close - device was not open for exclusive access",  this);
			ret = kIOReturnNotOpen;
		}
    }
    else
	{
		USBLog(5, "IOUSBDeviceUserClientV2[%p]::close - fOwner was NULL",  this);
        ret = kIOReturnNotAttached;
	}
	
    USBLog(5, "-IOUSBDeviceUserClientV2[%p]::close - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	
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
		
		target->release();
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
				USBLog(4,"IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (async) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
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
			USBLog(5,"IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (async) returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
			if (mem != NULL)
			{
				mem->complete();
				mem->release();
			}
		}
	}
	else
		ret = kIOReturnNotAttached;
	
Exit:
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (async) - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
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
			USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceRequestIn (sync < 4k) err:0x%x (%s)", this, ret, USBStringFromReturn(ret));
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
			USBLog(4,"IOUSBDeviceUserClientV2[%p]::DeviceRequestIn > 4K mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
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
		
		target->release();
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
		if (completion == NULL)
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
				USBLog(3,"IOUSBDeviceUserClientV2[%p]::DeviceRequestOut (async) mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
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
			USBLog(5,"IOUSBDeviceUserClientV2[%p]::DeviceRequestOut (async) returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
			if (mem != NULL)
			{
				mem->complete();
				mem->release();
			}
		}
	}
	else
		ret = kIOReturnNotAttached;
	
Exit:
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceRequestOut (async) - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
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
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceRequestOut - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
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
			USBLog(4,"IOUSBDeviceUserClientV2[%p]::DeviceRequestOut > 4K mem->prepare() returned 0x%x (%s)", this, ret, USBStringFromReturn(ret)); 
		}
	}
	else
		ret = kIOReturnNotAttached;
	
	if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::DeviceReqOut (sync OOL) - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	DecrementOutstandingIO();	
	return ret;
}


#pragma mark Configuration

IOReturn IOUSBDeviceUserClientV2::_SetConfiguration(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+%s[%p]::_SetConfiguration", target->getName(), target);
	
	target->retain();
    IOReturn kr = target->SetConfiguration((UInt8)arguments->scalarInput[0]);
	target->release();
	
	return kr;
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
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::SetConfiguration - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}

IOReturn IOUSBDeviceUserClientV2::_GetConfiguration(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+%s[%p]::_GetConfiguration", target->getName(), target);
	
	target->retain();
    IOReturn kr = target->GetConfiguration(&(arguments->scalarOutput[0]));
	target->release();
	
	return kr;
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
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetConfiguration - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}

	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::GetConfiguration returns 0x%qx",  this, *configValue);

    DecrementOutstandingIO();
	return ret;
}


IOReturn 
IOUSBDeviceUserClientV2::_GetConfigDescriptor(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	IOReturn	kr;
	
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_GetConfigDescriptor", target);

	target->retain();
 
	if ( arguments->structureOutputDescriptor ) 
        kr = target->GetConfigDescriptor((UInt8)arguments->scalarInput[0], arguments->structureOutputDescriptor, &(arguments->structureOutputDescriptorSize));
    else
        kr = target->GetConfigDescriptor((UInt8)arguments->scalarInput[0], (IOUSBConfigurationDescriptorPtr) arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));

	target->release();
	
	return kr;
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
		USBLog(5, "IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
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
		USBLog(5, "IOUSBDeviceUserClientV2[%p]::GetConfigDescriptor - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
	return ret;

}	


#pragma mark State
IOReturn IOUSBDeviceUserClientV2::_ResetDevice(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference, arguments)
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_ResetDevice", target);
	
	target->retain();
	IOReturn kr = target->ResetDevice();
	target->release();
	
	return kr;
}  

IOReturn
IOUSBDeviceUserClientV2::ResetDevice()
{
    IOReturn	ret;
    
    USBLog(6, "+IOUSBDeviceUserClientV2[%p]::ResetDevice",  this);
    IncrementOutstandingIO();
	
    if (fOwner && !isInactive())
		ret = fOwner->ResetDevice();
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::ResetDevice - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBDeviceUserClientV2::_SuspendDevice(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_SuspendDevice", target);
	
	target->retain();
    IOReturn kr = target->SuspendDevice((bool)arguments->scalarInput[0]);
	target->release();
	
	return kr;
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
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::SuspendDevice - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBDeviceUserClientV2::_ReEnumerateDevice(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_ReEnumerateDevice", target);
	
	target->retain();
    IOReturn kr = target->ReEnumerateDevice((UInt32)arguments->scalarInput[0]);
	target->release();
	
	return kr;
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
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::ReEnumerateDevice - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    release();
	
    return ret;
}


IOReturn IOUSBDeviceUserClientV2::_AbortPipeZero(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference, arguments)
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_AbortPipeZero", target);
	
	target->retain();
	IOReturn kr = target->AbortPipeZero();
	target->release();
	
	return kr;
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
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::AbortPipeZero - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}

#pragma mark -
IOReturn IOUSBDeviceUserClientV2::_GetDeviceInformation(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::_GetDeviceInformation", target);
	
	target->retain();
    IOReturn kr = target->GetDeviceInformation(&(arguments->scalarOutput[0]));
	target->release();
	
	return kr;
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
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetDeviceInformation - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::GetDeviceInformation returns 0x%qx",  this, *info);
	
    DecrementOutstandingIO();
	return ret;
}

IOReturn IOUSBDeviceUserClientV2::_RequestExtraPower(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::_RequestExtraPower", target);
	
	target->retain();
    IOReturn kr = target->RequestExtraPower((UInt32)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], &(arguments->scalarOutput[0]));
	target->release();
	
	return kr;
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
		// Make all power from user clients be revocable
		if (type == kUSBPowerDuringWake)
			type = kUSBPowerDuringWakeRevocable;
		
		*powerAvailable = (uint64_t) fOwner->RequestExtraPower(type, requestedPower);
		
		if ( type == kUSBPowerDuringSleep )
			FSLEEPPOWERALLOCATED += (UInt32) *powerAvailable;

		if ( type == kUSBPowerDuringWakeRevocable )
			FWAKEPOWERALLOCATED += (UInt32) *powerAvailable;

	}
    else
        ret = kIOReturnNotAttached;
		
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::RequestExtraPower - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::RequestExtraPower returns %qd",  this, *powerAvailable);
	
    DecrementOutstandingIO();
	return ret;
}

IOReturn IOUSBDeviceUserClientV2::_ReturnExtraPower(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(5, "+%s[%p]::_ReturnExtraPower", target->getName(), target);
	
	target->retain();
    IOReturn kr = target->ReturnExtraPower((UInt32)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1]);
	target->release();
	
	return kr;
}

IOReturn
IOUSBDeviceUserClientV2::ReturnExtraPower(UInt32 type, UInt32 returnedPower)
{
    IOReturn	ret = kIOReturnSuccess;
	
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::ReturnExtraPower returning type %d power = %d",  this, (uint32_t) type, (uint32_t) returnedPower);
    IncrementOutstandingIO();
    
	// Make all power from user clients be revocable
	if (type == kUSBPowerDuringWake)
		type = kUSBPowerDuringWakeRevocable;

    if (fOwner && !isInactive())
		ret = fOwner->ReturnExtraPower(type, returnedPower);
    else
        ret = kIOReturnNotAttached;
	
    if (ret == kIOReturnSuccess)
	{
		if ( type == kUSBPowerDuringSleep )
			FSLEEPPOWERALLOCATED -= returnedPower;
		
	if ( type == kUSBPowerDuringWakeRevocable )
			FWAKEPOWERALLOCATED -= returnedPower;
	}
	else
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::ReturnExtraPower - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}

IOReturn IOUSBDeviceUserClientV2::_GetExtraPowerAllocated(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::_GetExtraPowerAllocated", target);
	
	target->retain();
    IOReturn kr = target->GetExtraPowerAllocated((UInt32)arguments->scalarInput[0], &(arguments->scalarOutput[0]));
	target->release();
	
	return kr;
}

IOReturn
IOUSBDeviceUserClientV2::GetExtraPowerAllocated(UInt32 type, uint64_t *powerAllocated)
{
	IOReturn	ret = kIOReturnSuccess;
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::GetExtraPowerAllocated  requesting type %d",  this, (uint32_t) type);
	IncrementOutstandingIO();
    
	// Make all power from user clients be revocable
	if (type == kUSBPowerDuringWake)
		type = kUSBPowerDuringWakeRevocable;

	*powerAllocated = 0;
	
    if (fOwner && !isInactive())
	{
		*powerAllocated = (uint64_t) fOwner->GetExtraPowerAllocated(type);
		
		// Workaround for 8001347.  We know that the user client only asks for 500, so if we get 400, set it to 0
		if ( *powerAllocated == 400 && type == kUSBPowerDuringWakeRevocable )
		{
			USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetExtraPowerAllocated - setting  *powerAllocated to 0 from 400 for bug workaround",  this);
			*powerAllocated = 0;
		}
	}
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetExtraPowerAllocated - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::GetExtraPowerAllocated returns %qd",  this, *powerAllocated);
	
    DecrementOutstandingIO();
	return ret;
}

IOReturn IOUSBDeviceUserClientV2::_GetBandwidthAvailableForDevice(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::_GetBandwidthAvailableForDevice", target);
	
	target->retain();
    IOReturn kr = target->GetBandwidthAvailableForDevice(&(arguments->scalarOutput[0]));
	target->release();
	
	return kr;
}

IOReturn
IOUSBDeviceUserClientV2::GetBandwidthAvailableForDevice(uint64_t *pBandwidth)
{
    IOReturn		ret = kIOReturnUnsupported;
	UInt32			bandwidth;
	
    USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::GetBandwidthAvailableForDevice",  this);
	
    if (fOwner && !isInactive())
    {

		if (fOwner && fOwner->_expansionData)
		{
			IOUSBControllerV3	*myController = OSDynamicCast(IOUSBControllerV3, fOwner->GetBus());
			IOUSBHubDevice *parent = OSDynamicCast(IOUSBHubDevice, fOwner->_expansionData->_usbPlaneParent);
			
			if (myController && parent)
			{
				ret = myController->GetBandwidthAvailableForDevice(fOwner, &bandwidth);
				if ( ret == kIOReturnSuccess )
				{
					USBLog(5, "+IOUSBInterfaceUserClientV2[%p]::GetBandwidthAvailableForDevice  got %d bytes",  this, (uint32_t)bandwidth);
					*pBandwidth = bandwidth;
				}
			}
		}
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
        USBLog(3, "IOUSBInterfaceUserClientV2[%p]::GetBandwidthAvailableForDevice - returning err 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    return ret;
}


#pragma mark FrameNumber
IOReturn IOUSBDeviceUserClientV2::_GetFrameNumber(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_GetFrameNumber", target);
	
	target->retain();
	IOReturn kr = target->GetFrameNumber((IOUSBGetFrameStruct *)arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
	target->release();
	
	return kr;
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
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetFrameNumber - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBDeviceUserClientV2::_GetMicroFrameNumber(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
    USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_GetMicroFrameNumber", target);
	
	target->retain();
	IOReturn kr = target->GetMicroFrameNumber((IOUSBGetFrameStruct *)arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
	target->release();
	
	return kr;
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
        USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetFrameNumber - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBDeviceUserClientV2::_GetFrameNumberWithTime(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
#pragma unused (reference)
	USBLog(7, "+IOUSBDeviceUserClientV2[%p]::_GetFrameNumberWithTime", target);
	
	target->retain();
	IOReturn kr = target->GetFrameNumberWithTime( (IOUSBGetFrameStruct*) arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
	target->release();
	
	return kr;
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
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetFrameNumberWithTime - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
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
	
	target->retain();
	
	reqIn.bInterfaceClass = (UInt16)arguments->scalarInput[0];
	reqIn.bInterfaceSubClass = (UInt16)arguments->scalarInput[1];
	reqIn.bInterfaceProtocol = (UInt16)arguments->scalarInput[2];
	reqIn.bAlternateSetting = (UInt16)arguments->scalarInput[3];
	
	ret = target->CreateInterfaceIterator(&reqIn, &(arguments->scalarOutput[0]));
	
	target->release();
	
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
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::CreateInterfaceIterator - returning 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	}
	
    DecrementOutstandingIO();
	return ret;
}


#pragma mark IOKit Methods

//
// clientClose - my client on the user side has released the mach port, so I will no longer
// be talking to him
//
IOReturn  
IOUSBDeviceUserClientV2::clientClose( void )
{
	IOReturn ret = kIOReturnNoDevice;
	
	retain();
	
	USBLog(6, "IOUSBDeviceUserClientV2[%p]::clientClose", this);

	// If we are inActive(), this means that we have been terminated.  This should not happen, as the user client connection should be severed by now
	if ( isInactive())
	{
		USBLog(1, "IOUSBDeviceUserClientV2[%p]::clientClose  We are inactive, returning",  this);
	}
	else 
	{
		IOCommandGate *	gate = fGate;
		IOWorkLoop *	workLoop = fWorkLoop;

		if (gate && workLoop)
		{
			workLoop->retain();
			gate->retain();
			
			ret = fGate->runAction(ClientCloseEntry);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(3, "IOUSBDeviceUserClientV2[%p]::clientClose  runAction returned 0x%x, isInactive(%s)",  this, ret, isInactive() ? "true" : "false");
				
			}
			gate->release();
			workLoop->release();
		}
		else
			ret = kIOReturnNoResources;
	}
	
	release();
	
	return ret;
}

IOReturn  
IOUSBDeviceUserClientV2::ClientCloseEntry(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param1, param2, param3, param4)
	IOUSBDeviceUserClientV2 *			me = OSDynamicCast(IOUSBDeviceUserClientV2, target);
    if (!me)
    {
		USBLog(1, "IOUSBDeviceUserClientV2::ClientCloseEntry - invalid target");
		return kIOReturnBadArgument;
    }
	
	return me->ClientCloseGated();
}

IOReturn 
IOUSBDeviceUserClientV2::ClientCloseGated( void )
{
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::ClientCloseGated  isInactive = %d, fOutstandingIO = %d, FOWNER_WAS_RELEASED: %d",  this, isInactive(), fOutstandingIO, FOWNER_WAS_RELEASED);
	
	// If we are inActive(), this means that we have been terminated.
	if ( isInactive())
	{
		USBLog(6, "IOUSBDeviceUserClientV2[%p]::ClientCloseGated  We are inactive, returning kIOReturnNoDevice",  this);
		return kIOReturnNoDevice;
	}
	
	// retain()/release() ourselves because we might close our fOwner and hence could be terminated
	retain();
	
	// Check to see if the client forgot to call ::close()
	if ( fOwner && FOPENED_FOR_EXCLUSIVEACCESS )
	{
		IOOptionBits	options = kUSBOptionBitOpenExclusivelyMask;
		
		USBLog(6, "IOUSBDeviceUserClientV2[%p]::ClientCloseGated  we were open exclusively, closing exclusively",  this);
		FOPENED_FOR_EXCLUSIVEACCESS = false;
		fOwner->close(this, options);
	}
	
	// Note that the terminate will end up calling provider->close(), so this is where the fOwner->open() from start() is balanced.  We don't want to do this
	// if we are inActive() or we have pendingIO.
	
	if (!isInactive() && (fOutstandingIO == 0) && !FOWNER_WAS_RELEASED)
	{
		USBLog(6, "IOUSBDeviceUserClientV2[%p]::ClientCloseGated  calling fOwner(%p)->release()",  this, fOwner);
		FOWNER_WAS_RELEASED = true;
		fOwner->release();
		release();
	}
	
	fTask = NULL;
	
	USBLog(5, "+IOUSBDeviceUserClientV2[%p]::ClientCloseGated  calling terminate()",  this);

	terminate();
	
    USBLog(6, "-IOUSBDeviceUserClientV2[%p]::ClientCloseGated",  this);
	
	release();
	
    return kIOReturnSuccess;		// DONT call super::clientClose, which just returns notSupported
}


IOReturn 
IOUSBDeviceUserClientV2::clientDied( void )
{
    IOReturn ret;
    
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::clientDied",  this);
    
    fDead = true;					// don't send mach messages in this case
	
	ret = super::clientDied();		// this just calls clientClose
	
    USBLog(6, "-IOUSBDeviceUserClientV2[%p]::clientDied, ret = 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	
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
    
    USBLog(6, "+IOUSBDeviceUserClientV2[%p]::stop(%p), isInactive = %d ",  this, provider, isInactive());

	if (fOutstandingIO == 0)
	{
		ReleaseWorkLoopAndGate();
	}
	else 
	{
		USBLog(6, "+IOUSBDeviceUserClientV2[%p]::stop, fOutstandingIO was not 0, so delaying free()'ing fGate and fWorkLoop",  this);
		FDELAYED_WORKLOOP_FREE = true;
	}
	
	super::stop(provider);

    USBLog(6, "-IOUSBDeviceUserClientV2[%p]::stop(%p)",  this, provider);

}



void 
IOUSBDeviceUserClientV2::free()
{
    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::free",  this);
			
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

    USBLog(5, "+IOUSBDeviceUserClientV2[%p]::finalize(%08x)",  this, (int)options);
    
    ret = super::finalize(options);
    
    USBLog(6, "-IOUSBDeviceUserClientV2[%p]::finalize(%08x) - returning %s",  this, (int)options, ret ? "true" : "false");
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
    USBLog(5, "IOUSBDeviceUserClientV2[%p]::willTerminate isInactive = %d, outstandingIO = %d inGate: %s",  this, isInactive(), (uint32_t)fOutstandingIO, fWorkLoop ? (fWorkLoop->inGate() ? "Yes" : "No"): "No workloop!");

    if ( fWorkLoop && fOwner )
    {
        if ( fOutstandingIO > 0 )
        {
   			USBLog(6, "IOUSBDeviceUserClientV2[%p]::willTerminate aborting pipe zero",  this);
			AbortPipeZero();
        }
    }
    
    return super::willTerminate(provider, options);
}



bool
IOUSBDeviceUserClientV2::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
	USBLog(5, "IOUSBDeviceUserClientV2[%p]::didTerminate isInactive = %d, outstandingIO = %d, FOWNER_WAS_RELEASED: %d, inGate: %s",  this, isInactive(), fOutstandingIO, FOWNER_WAS_RELEASED, fWorkLoop ? (fWorkLoop->inGate() ? "Yes" : "No"): "No workloop!");
	
	if ( fWorkLoop)
	{
		// At this point, we are inactive, so if we have been opened for exclusive acces, we need to close it
		if ( FOPENED_FOR_EXCLUSIVEACCESS && fOwner)
		{
			IOOptionBits	options = kUSBOptionBitOpenExclusivelyMask;
			
			FOPENED_FOR_EXCLUSIVEACCESS = false;
			USBLog(5, "IOUSBDeviceUserClientV2[%p]::didTerminate  FOPENED_FOR_EXCLUSIVEACCESS was true, closing fOwner",  this);
			fOwner->close(this, options);
		}
		
		// now, if we still have our owner open, then we need to close it
		if ( fOutstandingIO == 0 )
		{
			bool	releaseIt = !FOWNER_WAS_RELEASED;
			
			USBLog(6, "+IOUSBDeviceUserClientV2[%p]::didTerminate  fOutstandingIO is 0, releaseIt: %d",  this, releaseIt);
			
			if (fOwner && fOwner->isOpen(this))
			{
				USBLog(6, "+IOUSBDeviceUserClientV2[%p]::didTerminate  closing fOwner",  this);
				
				fNeedToClose = false;
				fOwner->close(this);
			}
			
			if (fOwner && releaseIt)
			{
				USBLog(6, "+IOUSBDeviceUserClientV2[%p]::didTerminate  releasing fOwner(%p) and UC",  this, fOwner);
				FOWNER_WAS_RELEASED = true;
				fOwner->release();
				release();
			}
		}
		else
		{
			USBLog(5, "+IOUSBDeviceUserClientV2[%p]::didTerminate  will close fOwner later because IO is %d",  this, (uint32_t)fOutstandingIO);
			fNeedToClose = true;
		}
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
 	IOCommandGate *	gate = fGate;
	IOWorkLoop *	workLoop = fWorkLoop;

    if (!fGate)
    {
		if (!--fOutstandingIO && fNeedToClose && !FOWNER_WAS_RELEASED)
		{
			USBLog(3, "IOUSBDeviceUserClientV2[%p]::DecrementOutstandingIO isInactive(%s), outstandingIO = %d - closing device",  this, isInactive() ? "true" : "false" , (int)fOutstandingIO);
			
			// Want to set this one if fOwner is not valid, just in case
			FOWNER_WAS_RELEASED = true;
			if (fOwner)
			{
				USBLog(6, "IOUSBDeviceUserClientV2[%p]::DecrementOutstandingIO  closing and releasing fOwner(%p)",  this, fOwner);
				fOwner->close(this);
				fOwner->release();
				fNeedToClose = false;
			}
			release();
		}
    }
	else
	{
		USBTrace( kUSBTOutstandingIO, kTPDeviceUCDecrement , (uintptr_t)this, (int)fOutstandingIO, 0, 0);
		
		workLoop->retain();
		gate->retain();
		
		IOReturn kr = gate->runAction(ChangeOutstandingIO, (void*)-1);
		if ( kr != kIOReturnSuccess)
		{
			USBLog(3, "IOUSBDeviceUserClientV2[%p]::DecrementOutstandingIO  runAction returned 0x%x, isInactive(%s)",  this, kr, isInactive() ? "true" : "false");
			
		}
		gate->release();
		workLoop->release();
	}
	
	if (fOutstandingIO == 0 && FDELAYED_WORKLOOP_FREE)
	{
		FDELAYED_WORKLOOP_FREE = false;
		
		USBLog(6, "IOUSBDeviceUserClientV2[%p]::DecrementOutstandingIO  calling ReleaseWorkLoopAndGate()", this);
		ReleaseWorkLoopAndGate();
	}
	
}


void
IOUSBDeviceUserClientV2::IncrementOutstandingIO(void)
{
  	IOCommandGate *	gate = fGate;
	IOWorkLoop *	workLoop = fWorkLoop;

	if ( isInactive() )
	{
		USBLog(5, "IOUSBDeviceUserClientV2[%p]::IncrementOutstandingIO  but we are inActive!",  this);
	}
	
	if (!fGate)
    {
		fOutstandingIO++;
		return;
    }
	USBTrace( kUSBTOutstandingIO, kTPDeviceUCIncrement , (uintptr_t)this, (int)fOutstandingIO, 0, 0);

	workLoop->retain();
	gate->retain();
	
	IOReturn kr = gate->runAction(ChangeOutstandingIO, (void*)1);
	if ( kr != kIOReturnSuccess)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::IncrementOutstandingIO  runAction returned 0x%x, isInactive(%s)",  this, kr, isInactive() ? "true" : "false");
		
	}
	gate->release();
	workLoop->release();
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
			if (!--me->fOutstandingIO && me->fNeedToClose && !me->FOWNER_WAS_RELEASED)
			{
				USBLog(6, "IOUSBDeviceUserClientV2[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me, me->isInactive(), me->fOutstandingIO);
				
				// Want to set this one if fOwner is not valid, just in case
				me->FOWNER_WAS_RELEASED = true;
				
				if (me->fOwner) 
				{
					me->fOwner->close(me);
					me->fOwner->release();
					me->fNeedToClose = false;
				}
				me->release();
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
   	IOCommandGate *	gate = fGate;
	IOWorkLoop *	workLoop = fWorkLoop;
	UInt32	count = 0;
    
    if (!fGate)
    {
		return fOutstandingIO;
    }
    
 	workLoop->retain();
	gate->retain();
	
	IOReturn kr = gate->runAction(GetGatedOutstandingIO, (void*)&count);
	if ( kr != kIOReturnSuccess)
	{
		USBLog(3, "IOUSBDeviceUserClientV2[%p]::GetGatedOutstandingIO  runAction returned 0x%x, isInactive(%s)",  this, kr, isInactive() ? "true" : "false");
		
	}
	gate->release();
	workLoop->release();
    
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


#pragma mark Helper Methods
void
IOUSBDeviceUserClientV2::ReleaseWorkLoopAndGate()
{
	USBLog(6, "IOUSBDeviceUserClientV2[%p]::ReleaseWorkLoopAndGate", this);
	
	if (fWorkLoop && fGate)
	{
		fWorkLoop->removeEventSource(fGate);
		
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
	}
	
	if (fWakePort != MACH_PORT_NULL)
	{
		super::releaseNotificationPort(fWakePort);
		fWakePort = MACH_PORT_NULL;
	}
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
OSMetaClassDefineReservedUsed(IOUSBDeviceUserClientV2,  2);

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

