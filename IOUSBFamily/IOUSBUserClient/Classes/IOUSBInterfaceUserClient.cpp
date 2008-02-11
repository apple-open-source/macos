/*
 * Copyright (c) 1998-2007 Apple Inc. All rights reserved.
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
#include <IOKit/usb/IOUSBController.h>
#include <IOKit/IOKitKeys.h>

#include "IOUSBInterfaceUserClient.h"

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

#ifndef kIOUserClientCrossEndianKey
#define kIOUserClientCrossEndianKey "IOUserClientCrossEndian"
#endif

#ifndef kIOUserClientCrossEndianCompatibleKey
#define kIOUserClientCrossEndianCompatibleKey "IOUserClientCrossEndianCompatible"
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IOUSBInterfaceUserClientV2, super)
OSDefineMetaClassAndStructors(IOUSBLowLatencyCommand, IOCommand)


const IOExternalMethodDispatch 
IOUSBInterfaceUserClientV2::sMethods[kIOUSBLibInterfaceUserClientNumCommands] = {
    { //    kUSBInterfaceUserClientOpen
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_open,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientClose
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_close,
		0, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetDevice
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetDevice,
		0, 0,
		1, 0
    },
    { //    kUSBInterfaceUserClientSetAlternateInterface
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_SetAlternateInterface,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetFrameNumber
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetFrameNumber,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
    { //    kUSBInterfaceUserClientGetPipeProperties
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetPipeProperties,
		1, 0,
		5, 0
    },
    { //    kUSBInterfaceUserClientReadPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ReadPipe,
		5, 0,
		0, 0xffffffff
    },
    { //    kUSBInterfaceUserClientWritePipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_WritePipe,
		5, 0xffffffff,
		0, 0
    },
    { //    kUSBInterfaceUserClientGetPipeStatus
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetPipeStatus,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientAbortPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_AbortPipe,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientResetPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ResetPipe,
		1, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientClearPipeStall
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ClearPipeStall,
		2, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientControlRequestOut
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ControlRequestOut,
		9, 0xffffffff,
		0, 0
    },
    { //    kUSBInterfaceUserClientControlRequestIn
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ControlRequestIn,
		9, 0,
		0, 0xffffffff
    },
    { //    kUSBInterfaceuserClientSetPipePolicy
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_SetPipePolicy,
		3, 0,
		0, 0
    },
    { //    kUSBInterfaceuserClientGetBandwidthAvailable
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetBandwidthAvailable,
		0, 0,
		1, 0
    },
    { //    kUSBInterfaceuserClientGetEndpointProperties
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetEndpointProperties,
		3, 0,
		3, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyPrepareBuffer
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_LowLatencyPrepareBuffer,
		0, sizeof(LowLatencyUserBufferInfoV2),
		1, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyReleaseBuffer
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_LowLatencyReleaseBuffer,
		0, sizeof(LowLatencyUserBufferInfoV2),
		0, 0
    },
    { //    kUSBInterfaceUserClientGetMicroFrameNumber
        (IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetMicroFrameNumber,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
    { //    kUSBInterfaceUserClientGetFrameListTime
        (IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetFrameListTime,
		0, 0,
		1, 0
    },
	{ //    kUSBInterfaceUserClientGetFrameNumberWithTime
        (IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetFrameNumberWithTime,
		0, 0,
		0, sizeof(IOUSBGetFrameStruct)
    },
	{ //    kUSBInterfaceUserClientSetAsyncPort
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_SetAsyncPort,
		0, 0,
		0, 0
    },
    { //    kUSBInterfaceUserClientReadIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_ReadIsochPipe,
		0, sizeof(IOUSBIsocStruct),
		0, 0
    },
    { //    kUSBInterfaceUserClientWriteIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_WriteIsochPipe,
		0, sizeof(IOUSBIsocStruct),
		0, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyReadIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_LowLatencyReadIsochPipe,
		0, sizeof(IOUSBLowLatencyIsocStruct),
		0, 0
    },
    { //    kUSBInterfaceUserClientLowLatencyWriteIsochPipe
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_LowLatencyWriteIsochPipe,
		0, sizeof(IOUSBLowLatencyIsocStruct),
		0, 0
    },
    {	//    kUSBInterfaceUserClientGetConfigDescriptor
		(IOExternalMethodAction) &IOUSBInterfaceUserClientV2::_GetConfigDescriptor,
		1, 0,
		0, 0xffffffff
    },
};



#pragma mark -

IOReturn IOUSBInterfaceUserClientV2::externalMethod( 
													uint32_t                    selector, 
													IOExternalMethodArguments * arguments,
													IOExternalMethodDispatch *  dispatch, 
													OSObject *                  target, 
													void *                      reference)
{
	
    if (selector < (uint32_t) kIOUSBLibInterfaceUserClientNumCommands)
    {
        dispatch = (IOExternalMethodDispatch *) &sMethods[selector];
        
        if (!target)
            target = this;
    }
	
	return super::externalMethod(selector, arguments, dispatch, target, reference);
}

#pragma mark Async Support

IOReturn 
IOUSBInterfaceUserClientV2::_SetAsyncPort(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_SetAsyncPort", target->getName(), target);
    return target->SetAsyncPort(arguments->asyncWakePort);
}


IOReturn IOUSBInterfaceUserClientV2::SetAsyncPort(mach_port_t port)
{
	USBLog(7,"+IOUSBInterfaceUserClientV2::SetAsyncPort");
    if (!fOwner)
        return kIOReturnNotAttached;
	
    fWakePort = port;
    return kIOReturnSuccess;
}

void
IOUSBInterfaceUserClientV2::ReqComplete(void *obj, void *param, IOReturn res, UInt32 remaining)
{
    io_user_reference_t						args[1];
    IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)param;
    IOUSBInterfaceUserClientV2 *me = OSDynamicCast(IOUSBInterfaceUserClientV2, (OSObject*)obj);
	
    if (!me)
		return;
	
    USBLog(7, "%s[%p]::ReqComplete, res = 0x%x, req = %08x, remaining = %08x", me->getName(), me, res, (int)pb->fMax, (int)remaining);
	
    if(res == kIOReturnSuccess) 
    {
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


void
IOUSBInterfaceUserClientV2::IsoReqComplete(void *obj, void *param, IOReturn res, IOUSBIsocFrame *pFrames)
{
    io_user_reference_t								args[1];
    IOUSBInterfaceUserClientISOAsyncParamBlock *	pb = (IOUSBInterfaceUserClientISOAsyncParamBlock *)param;
    IOUSBInterfaceUserClientV2 *					me = OSDynamicCast(IOUSBInterfaceUserClientV2, (OSObject*)obj);
	UInt32			i;
	
    if (!me)
		return;
	
    USBLog(7, "%s[%p]::IsoReqComplete, result = 0x%x, dataMem: %p", me->getName(), me, res, pb->dataMem);
	
	args[0] = (io_user_reference_t) pb->frameBase;
    pb->countMem->writeBytes(0, pb->frames, pb->frameLen);
    pb->dataMem->complete();
    pb->dataMem->release();
    pb->dataMem = NULL;
    
    pb->countMem->complete();
    pb->countMem->release();
    pb->countMem = NULL;
    
	if (!me->fDead)
		sendAsyncResult64(pb->fAsyncRef, res, args, 1);
	
    IOFree(pb, sizeof(*pb)+pb->frameLen);
    me->DecrementOutstandingIO();
	me->release();
}




void
IOUSBInterfaceUserClientV2::LowLatencyIsoReqComplete(void *obj, void *param, IOReturn res, IOUSBLowLatencyIsocFrame *pFrames)
{
    io_user_reference_t			args[1];
    IOUSBLowLatencyCommand *	command = (IOUSBLowLatencyCommand *) param;
    IOMemoryDescriptor *		dataBufferDescriptor;
    OSAsyncReference64			asyncRef;
    
    IOUSBInterfaceUserClientV2 *	me = OSDynamicCast(IOUSBInterfaceUserClientV2, (OSObject*)obj);
	
    if (!me)
		return;
	
    args[0] = (io_user_reference_t) command->GetFrameBase(); 
    
    command->GetAsyncReference(&asyncRef);
    
    if (!me->fDead)
		sendAsyncResult64( asyncRef, res, args, 1);
	
    // Complete the memory descriptor
    //
    dataBufferDescriptor = command->GetDataBuffer();
    dataBufferDescriptor->complete();
    dataBufferDescriptor->release();
    dataBufferDescriptor = NULL;
    
    // Free/give back the command 
    me->fFreeUSBLowLatencyCommandPool->returnCommand(command);
	
    me->DecrementOutstandingIO();
	me->release();
}


#pragma mark User Client Start/Close

// Don't add any USBLogs to this routine.   You will panic if you use getName().
bool
IOUSBInterfaceUserClientV2::initWithTask(task_t owningTask,void *security_id , UInt32 type, OSDictionary * properties )
{
	if ( properties != NULL )
	{
		properties->setObject( kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue);
	}
	
    if (!super::initWithTask(owningTask, security_id , type, properties))
        return false;
	
    if (!owningTask)
		return false;
	
	// Allocate our expansion data
    //
    if (!fIOUSBInterfaceUserClientExpansionData)
    {
        fIOUSBInterfaceUserClientExpansionData = (IOUSBInterfaceUserClientExpansionData *)IOMalloc(sizeof(IOUSBInterfaceUserClientExpansionData));
        if (!fIOUSBInterfaceUserClientExpansionData)
            return false;
		
        bzero(fIOUSBInterfaceUserClientExpansionData, sizeof(IOUSBInterfaceUserClientExpansionData));
    }
	
    fTask = owningTask;
    fDead = false;
	
#if DEBUG_LEVEL != 0
	char	nbuf[256];
	proc_t	p = proc_self();
	int		owningPID = proc_pid(p);
	
	proc_name(owningPID,nbuf,256);
	
	OSNumber* pidProp 	= OSNumber::withNumber( proc_pid(p), sizeof(proc_pid(p)) * 8 );
	if ( pidProp )
	{
		setProperty( "Owning PID", pidProp ) ;
		pidProp->release() ;						// property table takes a reference
	}
	
	OSSymbol* processNameProp	= (OSSymbol *)OSSymbol::withCString(nbuf);
	if ( processNameProp)
	{
		setProperty( "Process Name", processNameProp ) ;
		processNameProp->release() ;						// property table takes a reference
	}
	
	USBLog(5,"IOUSBInterfaceUserClientV2[%p]::initWithTask  Owning PID is %d, name is %s", this, owningPID, nbuf);

	proc_rele(p);
#endif
	
	// If bit 31 of type is set, then our client is running under Rosetta
	fClientRunningUnderRosetta = ( type & 0x80000000 );
	
    return true;
}



bool 
IOUSBInterfaceUserClientV2::start( IOService * provider )
{
    IOWorkLoop	*			workLoop = NULL;
    IOCommandGate *			commandGate = NULL;
	OSObject *				propertyObj = NULL;
    OSBoolean *				boolObj = NULL;

    USBLog(7, "+%s[%p]::start(%p)", getName(), this, provider);
    
    IncrementOutstandingIO();		// make sure we don't close until start is done
	
    fOwner = OSDynamicCast(IOUSBInterface, provider);
	
    if (!fOwner)
    {
        USBError(1, "%s[%p]::start - provider is NULL!", getName(), this);
        goto ErrorExit;
    }
    
    if(!super::start(provider))
    {
        USBError(1, "%s[%p]::start - super::start returned false!", getName(), this);
        goto ErrorExit;
    }
	
    commandGate = IOCommandGate::commandGate(this);
	
    if (!commandGate)
    {
        USBError(1, "%s[%p]::start - unable to create command gate", getName(), this);
        goto ErrorExit;
    }
	
    workLoop = getWorkLoop();
    if (!workLoop)
    {
        USBError(1, "%s[%p]::start - unable to find my workloop", getName(), this);
        goto ErrorExit;
    }
    workLoop->retain();
    
    if (workLoop->addEventSource(commandGate) != kIOReturnSuccess)
    {
        USBError(1, "%s[%p]::start - unable to add gate to work loop", getName(), this);
        goto ErrorExit;
    }
	
    fFreeUSBLowLatencyCommandPool = IOCommandPool::withWorkLoop(workLoop);
    if (!fFreeUSBLowLatencyCommandPool)
    {
        USBError(1,"%s[%p]::start - unable to create free command pool", getName(), this);
        
        // Remove the event source we added above
        //
        workLoop->removeEventSource(commandGate);
        
        goto ErrorExit;
    }
	
	// If our IOUSBDevice has a "Need contiguous memory for isoch" property, set a flag indicating so
	//
	propertyObj = fOwner->GetDevice()->copyProperty(kUSBControllerNeedsContiguousMemoryForIsoch);
    boolObj = OSDynamicCast( OSBoolean, propertyObj);
    if ( boolObj )
	{
		if ( boolObj->isTrue() )
			fNeedContiguousMemoryForLowLatencyIsoch = true;
		else
			fNeedContiguousMemoryForLowLatencyIsoch = false;
	}
	if (propertyObj)
		propertyObj->release();
	
    // Now that we have succesfully added our gate to the workloop, set our member variables
    //
    fGate = commandGate;
    fWorkLoop = workLoop;
	
    
    USBLog(7, "-%s[%p]::start", getName(), this);
    
    DecrementOutstandingIO();
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
	
    DecrementOutstandingIO();
    return false;
}


IOReturn IOUSBInterfaceUserClientV2::_open(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	USBLog(7, "+%s[%p]::_open", target->getName(), target);
   return target->open((bool)arguments->scalarInput[0]);
}

IOReturn 
IOUSBInterfaceUserClientV2::open(bool seize)
{
    IOOptionBits	options = seize ? (IOOptionBits)kIOServiceSeize : 0;
	
    USBLog(7, "+%s[%p]::open", getName(), this);
    
    if (!fOwner)
        return kIOReturnNotAttached;
	
    if (!fOwner->open(this, options))
    {
        USBLog(3, "+%s[%p]::open failed", getName(), this);
        return kIOReturnExclusiveAccess;
    }
    
	// If we are running under Rosetta, add a property to the interface nub:
	if ( fClientRunningUnderRosetta )
	{
		USBLog(5, "%s[%p]::open  setting kIOUserClientCrossEndianCompatibleKey TRUE on %p", getName(), this, fOwner);
		fOwner->setProperty(kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue);
	}
	else
	{
		USBLog(5, "%s[%p]::open  setting kIOUserClientCrossEndianCompatibleKey FALSE on %p", getName(), this, fOwner);
		fOwner->setProperty(kIOUserClientCrossEndianCompatibleKey, kOSBooleanFalse);
	}
	
    fNeedToClose = false;
    
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceUserClientV2::_close(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_close", target->getName(), target);
    return target->close();
}

// This is NOT the normal IOService::close(IOService*) method.
// We are treating this is a proxy that we should close our parent, but
// maintain the connection with the task
IOReturn
IOUSBInterfaceUserClientV2::close()
{
    IOReturn 	ret = kIOReturnSuccess;
    
    USBLog(7, "+%s[%p]::close", getName(), this);
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		if (fOwner->isOpen(this))
		{
			fNeedToClose = true;				// the last outstanding IO will close this
			if (GetOutstandingIO() > 1)				// 1 for the one at the top of this routine
			{
				int		i;
				
				USBLog(6, "%s[%p]::close - outstanding IO, aborting pipes", getName(), this);
				for (i=1; i <= kUSBMaxPipes; i++)
					AbortPipe(i);
			}
		}
		else
			ret = kIOReturnNotOpen;
    }
    else
		ret = kIOReturnNotAttached;
	
    USBLog(7, "-%s[%p]::close - returning %x", getName(), this, ret);
    DecrementOutstandingIO();
    return ret;
}


#pragma mark Miscellaneous InterfaceUserClient

IOReturn IOUSBInterfaceUserClientV2::_SetAlternateInterface(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_SetAlternateInterface", target->getName(), target);
    return target->SetAlternateInterface((UInt8)arguments->scalarInput[0]);
}


IOReturn 
IOUSBInterfaceUserClientV2::SetAlternateInterface(UInt8 altSetting)
{
    IOReturn	ret;
    
    USBLog(7, "+%s[%p]::SetAlternateInterface to %d", getName(), this, altSetting);
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
		ret = fOwner->SetAlternateInterface(this, altSetting);
    else
		ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "%s[%p]::SetAlternateInterface - returning err %x", getName(), this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_GetDevice(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn ret;
	
    USBLog(7, "+%s[%p]::_GetDevice", target->getName(), target);
    return target->GetDevice(&(arguments->scalarOutput[0]));
}

IOReturn 
IOUSBInterfaceUserClientV2::GetDevice(uint64_t *device)
{
    IOReturn		ret;
    io_object_t  	service;
	
    USBLog(7, "+%s[%p]::GetDevice (%p, 0x%qx)", getName(), this, device, *device);
    IncrementOutstandingIO();
	
	*device = NULL;
	
    if (fOwner && !isInactive())
    {
		// Although not documented, Simon says that exportObjectToClient consumes a reference,
		// so we have to put an extra retain on the device.  The user client side of the USB stack (USBLib)
		// always calls this routine in order to cache the USB device.  Radar #2586534
		fOwner->GetDevice()->retain();
		ret = exportObjectToClient(fTask, fOwner->GetDevice(), &service);
 		USBLog(7, "+%s[%p]::GetDevice exportObjectToClient of (%p) returning err 0x%x", getName(), this, fOwner->GetDevice(), ret);
		*device = (uint64_t) service;
   }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "%s[%p]::GetDevice - returning  %x", getName(), this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_GetFrameNumber(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_GetFrameNumber", target->getName(), target);
	return target->GetFrameNumber((IOUSBGetFrameStruct *)arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
}  

IOReturn 
IOUSBInterfaceUserClientV2::GetFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size)
{
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(7, "+%s[%p]::GetFrameNumber", getName(), this);
	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		clock_get_uptime(&data->timeStamp);
		data->frame = fOwner->GetDevice()->GetBus()->GetFrameNumber();
		USBLog(7,"IOUSBInterfaceUserClientV2::GetFrameNumber frame: 0x%qx, timeStamp.hi: 0x%lx, timeStamp.lo: 0x%lx", data->frame, (data->timeStamp).hi, (data->timeStamp).lo);
		*size = sizeof(IOUSBGetFrameStruct);
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "%s[%p]::GetFrameNumber - returning err %x", getName(), this, ret);
	}
	
    DecrementOutstandingIO();
    USBLog(7, "-%s[%p]::GetFrameNumber  FrameNumber: %qd", getName(), this,data->frame);
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_GetMicroFrameNumber(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_GetMicroFrameNumber", target->getName(), target);
	return target->GetMicroFrameNumber((IOUSBGetFrameStruct *)arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
}  

IOReturn
IOUSBInterfaceUserClientV2::GetMicroFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size)
{
    // This method only available for v2 controllers
    //
    IOUSBControllerV2	*v2 = NULL;
    IOReturn		ret = kIOReturnSuccess;
	
	if (fOwner)
		v2 = OSDynamicCast(IOUSBControllerV2, fOwner->GetDevice()->GetBus());
	
    if (!v2)
    {
        USBLog(3, "%s[%p]::GetMicroFrameNumber - Not a USB 2.0 controller!  Returning 0x%x", getName(), this, kIOReturnNotAttached);
        return kIOReturnNotAttached;
    }
	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		UInt64	microFrame;
		
        clock_get_uptime(&data->timeStamp);
        microFrame = v2->GetMicroFrameNumber();
		if ( microFrame != 0)
		{
			data->frame = microFrame;
			USBLog(7,"IOUSBInterfaceUserClientV2::GetMicroFrameNumber frame: 0x%qx, timeStamp.hi: 0x%lx, timeStamp.lo: 0x%lx", data->frame, (data->timeStamp).hi, (data->timeStamp).lo);
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
        USBLog(3, "%s[%p]::GetMicroFrameNumber - no fOwner(%p) or isInactive", getName(), this, fOwner);
        ret = kIOReturnNotAttached;
    }
	
    if (ret)
	{
        USBLog(3, "%s[%p]::GetFrameNumber - returning err %x", getName(), this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_GetFrameNumberWithTime(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	USBLog(7, "+%s[%p]::_GetFrameNumberWithTime", target->getName(), target);
	return target->GetFrameNumberWithTime( (IOUSBGetFrameStruct*) arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
}  


IOReturn 
IOUSBInterfaceUserClientV2::GetFrameNumberWithTime(IOUSBGetFrameStruct *data, UInt32 *size)
{
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(7, "+%s[%p]::GetFrameNumberWithTime", getName(), this);
	
    if(*size != sizeof(IOUSBGetFrameStruct))
		return kIOReturnBadArgument;
    
    if (fOwner && !isInactive())
    {
		IOUSBControllerV2		*busV2 = OSDynamicCast(IOUSBControllerV2, fOwner->GetDevice()->GetBus());
		if (busV2)
		{
			ret = busV2->GetFrameNumberWithTime(&data->frame, &data->timeStamp);
			USBLog(7,"IOUSBInterfaceUserClientV2::GetFrameNumberWithTime frame: 0x%qx, timeStamp.hi: 0x%lx, timeStamp.lo: 0x%lx", data->frame, (data->timeStamp).hi, (data->timeStamp).lo);
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
		USBLog(3, "%s[%p]::GetFrameNumberWithTime - returning err %x", getName(), this, ret);
	}
	
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_GetBandwidthAvailable(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_GetBandwidthAvailable", target->getName(), target);
    return target->GetBandwidthAvailable(&(arguments->scalarOutput[0]));
}

IOReturn
IOUSBInterfaceUserClientV2::GetBandwidthAvailable(uint64_t *bandwidth)
{
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(7, "+%s[%p]::GetBandwidthAvailable", getName(), this);
	
    if (fOwner && !isInactive())
    {
        *bandwidth = fOwner->GetDevice()->GetBus()->GetBandwidthAvailable();
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
        USBLog(3, "%s[%p]::GetBandwidthAvailable - returning err %x", getName(), this, ret);
	}
	
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_GetFrameListTime(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_GetFrameListTime", target->getName(), target);
    return target->GetFrameListTime(&(arguments->scalarOutput[0]));
}

IOReturn
IOUSBInterfaceUserClientV2::GetFrameListTime(uint64_t *microsecondsInFrame)
{
    IOReturn		ret = kIOReturnSuccess;
	
    USBLog(7, "+%s[%p]::GetFrameListTime", getName(), this);
	
    if (fOwner && !isInactive())
    {
        UInt8	speed;
        // Find the speed of the device and return the appropriate microseconds
        // depending on the speed
        //
        speed  = fOwner->GetDevice()->GetSpeed();
		
        if ( speed == kUSBDeviceSpeedHigh )
        {
            // High Speed Device
            //
            *microsecondsInFrame = kUSBHighSpeedMicrosecondsInFrame;
        }
        else
        {
            // Low and Full Speed
            //
            *microsecondsInFrame = kUSBFullSpeedMicrosecondsInFrame;
        }
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
        USBLog(3, "%s[%p]::GetFrameListTime - returning err %x", getName(), this, ret);
	}
	
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_GetEndpointProperties(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_GetEndpointProperties", target->getName(), target);
    return target->GetEndpointProperties((UInt8)arguments->scalarInput[0], (UInt8)arguments->scalarInput[1], (UInt8)arguments->scalarInput[2], 
									&(arguments->scalarOutput[0]), &(arguments->scalarOutput[1]), &(arguments->scalarOutput[2]));
}

IOReturn 
IOUSBInterfaceUserClientV2::GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, uint64_t *transferType, uint64_t *maxPacketSize, uint64_t *interval)
{
    IOReturn		ret = kIOReturnSuccess;
    UInt8			myTT;
    UInt16			myMPS;
    UInt8			myIV;
	
    USBLog(7, "+%s[%p]::GetEndpointProperties for altSetting %d, endpoint: %d, direction %d", getName(), this, alternateSetting, endpointNumber, direction);
	
    if (fOwner && !isInactive())
		ret = fOwner->GetEndpointProperties(alternateSetting, endpointNumber, direction, &myTT, &myMPS, &myIV);
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "%s[%p]::GetEndpointProperties - returning err %x", getName(), this, ret);
	}
    else
    {
        *transferType = myTT;
        *maxPacketSize = myMPS;
        *interval = myIV;
    }
    return ret;
}

IOReturn 
IOUSBInterfaceUserClientV2::_GetConfigDescriptor(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	USBLog(7, "+%s[%p]::_GetConfigDescriptor", target->getName(), target);
    if ( arguments->structureOutputDescriptor ) 
        return target->GetConfigDescriptor((UInt8)arguments->scalarInput[0], arguments->structureOutputDescriptor, &(arguments->structureOutputDescriptorSize));
    else
        return target->GetConfigDescriptor((UInt8)arguments->scalarInput[0], (IOUSBConfigurationDescriptorPtr) arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
}  


IOReturn
IOUSBInterfaceUserClientV2::GetConfigDescriptor(UInt8 configIndex, IOUSBConfigurationDescriptorPtr desc, UInt32 *size)
{
    UInt16 				length;
    const IOUSBConfigurationDescriptor	*cached;
    IOReturn				ret;
    
    USBLog(7,"+%s[%p]::GetConfigDescriptor (Config %d), with size %ld, struct: %p", getName(), this, configIndex, *size, desc);
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		cached = fOwner->GetDevice()->GetFullConfigurationDescriptor(configIndex);
		if ( cached == NULL )
		{
			USBLog(5,"+%s[%p]::GetConfigDescriptor GetFullConfigurationDescriptor returned NULL", getName(), this);
			desc = NULL;
			ret = kIOReturnNotFound;
		}
		else
		{
			USBLog(7,"+%s[%p]::GetConfigDescriptor  got descriptor %p, length: %d", getName(), this, cached, USBToHostWord(cached->wTotalLength));
			length = USBToHostWord(cached->wTotalLength);
			if(length < *size)
			*size = length;
			bcopy(cached, desc, *size);
			ret = kIOReturnSuccess;
		}
    }
    else
        ret = kIOReturnNotAttached;
	
   	if (ret)
	{
		USBLog(5, "%s[%p]::GetConfigDescriptor - returning err %x", getName(), this, ret);
	}
	
    DecrementOutstandingIO();
	return ret;
}

IOReturn
IOUSBInterfaceUserClientV2::GetConfigDescriptor(UInt8 configIndex, IOMemoryDescriptor * mem, uint32_t *size)
{
    UInt16 				length;
    const IOUSBConfigurationDescriptor	*cached;
    IOReturn				ret;
    
    USBLog(7,"+%s[%p]::GetConfigDescriptor > 4K (Config %d), with size %d, mem: %p", getName(), this, configIndex, *size, mem);
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		cached = fOwner->GetDevice()->GetFullConfigurationDescriptor(configIndex);
		if ( cached == NULL )
		{
			USBLog(5,"+%s[%p]::GetConfigDescriptor > 4K GetFullConfigurationDescriptor returned NULL", getName(), this);
			ret = kIOReturnNotFound;
		}
		else
		{
			USBLog(7,"+%s[%p]::GetConfigDescriptor > 4K  got descriptor %p, length: %d", getName(), this, cached, USBToHostWord(cached->wTotalLength));
			length = USBToHostWord(cached->wTotalLength);
			if(length < *size)
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
		USBLog(5, "%s[%p]::GetConfigDescriptor - returning err %x", getName(), this, ret);
	}
	
    DecrementOutstandingIO();
	return ret;
}	



#pragma mark Pipe Methods

//================================================================================================
//
//   _ReadPipe
//
//   This method is called for both sync and async writes.  In the case of async, the parameters
//   will only be 5 scalars.  In the sync case, it will be 3 scalars and, depending on the size,
//   an inputStructure or an inputIOMD.

//================================================================================================
//
IOReturn IOUSBInterfaceUserClientV2::_ReadPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn								ret;
	
    USBLog(7, "+%s[%p]::_ReadPipe", target->getName(), target);
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
        tap.action = &IOUSBInterfaceUserClientV2::ReqComplete;
        tap.parameter = pb;
		
		ret = target->ReadPipe(	(UInt8) arguments->scalarInput[0],				// pipeRef
								(UInt32) arguments->scalarInput[1],				// noDataTimeout
								(UInt32) arguments->scalarInput[2],				// completionTimeout
								(mach_vm_address_t) arguments->scalarInput[3],	// buffer (in user task)
								(mach_vm_size_t) arguments->scalarInput[4],		// size of buffer
								&tap);											// completion
		
        if ( ret ) 
		{
            if ( pb )
				IOFree(pb, sizeof(*pb));
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	else if ( arguments->structureOutputDescriptor ) 
        ret = target->ReadPipe(	(UInt8)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2],
						arguments->structureOutputDescriptor, (UInt32 *)&(arguments->structureOutputDescriptorSize));
    else
        ret = target->ReadPipe((UInt8)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2],
								arguments->structureOutput, (UInt32 *)&(arguments->structureOutputSize));
	return ret;
}  

IOReturn
IOUSBInterfaceUserClientV2::ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion)
{
	IOReturn					ret = kIOReturnNotAttached;
    IOMemoryDescriptor *	mem = NULL;
    IOUSBPipe *				pipeObj = NULL;
	
    USBLog(7, "+%s(%p)::ReadPipe (Async) (pipeRef: %d, %ld, %ld, buffer: 0x%qx, size: %qd, completion: %p)", getName(), this, pipeRef, noDataTimeout, completionTimeout, buffer, size, completion);
    
	if (fOwner && !isInactive())
    {
		if (completion == NULL)
		{
			USBLog(1,"%s[%p]::ReadPipe (async) bad arguments (%qd, %qx, %p)", getName(), this, size, buffer, completion); 
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			// This is an Async request 
			IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
			
			USBLog(7,"%s[%p]::ReadPipe (async) creating IOMD", getName(), this); 
			mem = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionIn, fTask);
			if (!mem)
			{
				USBLog(1,"%s[%p]::ReadPipe (async ) IOMemoryDescriptor::withAddressRange returned NULL", getName(), this); 
				ret = kIOReturnNoMemory;
				goto Exit;
			}
			
			ret = mem->prepare();
			if ( ret != kIOReturnSuccess)
			{
				USBLog(3,"%s[%p]::ReadPipe (async) mem->prepare() returned 0x%x", getName(), this, ret); 
				mem->release();
				goto Exit;
			}
			
			pb->fMax = size;
			pb->fMem = mem;
			
			ret = pipeObj->Read(mem, noDataTimeout, completionTimeout, completion, NULL);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(5,"%s[%p]::ReadPipe (async) returned 0x%x", getName(), this, ret); 
				mem->complete();
				mem->release();
			}
		}
		else
		{
			USBLog(5,"%s[%p]::ReadPipe (async) can't find pipeRef, returning kIOUSBUnknownPipeErr", getName(), this); 
			ret = kIOUSBUnknownPipeErr;
		}
	}
	
Exit:
	
	if (pipeObj)
		pipeObj->release();
	
    if (ret)
	{
		USBLog(3, "%s[%p]::ReadPipe (async - returning err %x", getName(), this, ret);
	}
	
	return ret;
}	


IOReturn
IOUSBInterfaceUserClientV2::ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size)
{
    IOReturn				ret = kIOReturnSuccess;
    IOMemoryDescriptor *	mem = NULL;
    IOUSBPipe *				pipeObj;
	
    USBLog(7, "+%s(%p)::ReadPipe < 4K (pipeRef: %d, data timeout: %ld, completionTimeout: %ld, buf: %p, size: %ld)", getName(), this, pipeRef, noDataTimeout, completionTimeout, buf, *size);
    
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			mem = IOMemoryDescriptor::withAddress( (void *)buf, *size, kIODirectionIn);
			if(mem)
			{ 
				*size = 0;
				ret = mem->prepare();
				if ( ret == kIOReturnSuccess)
				{
					ret = pipeObj->Read(mem, noDataTimeout, completionTimeout, 0, size);

					mem->complete();
				}
				else
				{
					USBLog(3,"%s[%p]::ReadPipe (sync < 4K) mem->prepare() returned 0x%x", getName(), this, ret); 
				}
				mem->release();
				mem = NULL;
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
		USBLog(3, "%s[%p]::ReadPipe - returning err %x, size read: %ld", getName(), this, ret, *size);
	}
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn
IOUSBInterfaceUserClientV2::ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem, IOByteCount *bytesRead)
{
    IOReturn				ret = kIOReturnSuccess;
    IOUSBPipe 			*	pipeObj;
	
    USBLog(7, "+%s(%p)::ReadPipe > 4K (pipeRef: %d, %ld, %ld, IOMD: %p)", getName(), this, pipeRef, noDataTimeout, completionTimeout, mem);
	
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			if(mem)
			{
				ret = mem->prepare();
				if (ret == kIOReturnSuccess)
				{
					ret = pipeObj->Read(mem, noDataTimeout, completionTimeout, 0, bytesRead );
					mem->complete();
				}
				else
				{
					USBLog(3,"%s[%p]::ReadPipe (sync > 4K) mem->prepare() returned 0x%x", getName(), this, ret);
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
		USBLog(3, "%s[%p]::ReadPipe > 4K - returning err %x", getName(), this, ret);
	}
	
	DecrementOutstandingIO();
	return ret;
}



//================================================================================================
//
//   _WritePipe
//
//   This method is called for both sync and async writes.  In the case of async, the parameters
//   will only be 5 scalars.  In the sync case, it will be 3 scalars and, depending on the size,
//   an inputStructure or an inputIOMD.

//================================================================================================
//
IOReturn IOUSBInterfaceUserClientV2::_WritePipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn								ret;
	
    USBLog(7, "+%s[%p]::_WritePipe", target->getName(), target);
	
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
        tap.action = &IOUSBInterfaceUserClientV2::ReqComplete;
        tap.parameter = pb;
		
		ret = target->WritePipe(	(UInt8) arguments->scalarInput[0],				// pipeRef
									(UInt32) arguments->scalarInput[1],				// noDataTimeout
									(UInt32) arguments->scalarInput[2],				// completionTimeout
									(mach_vm_address_t) arguments->scalarInput[3],	// buffer (in user task)
									(mach_vm_size_t) arguments->scalarInput[4],		// size of buffer
									&tap);											// completion
		
        if ( ret ) 
		{
            if ( pb )
				IOFree(pb, sizeof(*pb));
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	else if ( arguments->structureInputDescriptor ) 
        ret = target->WritePipe(	(UInt8)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2],
					arguments->structureInputDescriptor);
    else
        ret = target->WritePipe((UInt16)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2],
					arguments->structureInput, arguments->structureInputSize);
	return ret;
}  


IOReturn
IOUSBInterfaceUserClientV2::WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion)
{
   IOReturn					ret = kIOReturnNotAttached;
    IOMemoryDescriptor *	mem = NULL;
    IOUSBPipe *				pipeObj = NULL;
	
    USBLog(7, "+%s(%p)::WritePipe (Async) (pipeRef: %d, %ld, %ld, buffer: 0x%qx, size: %qd, completion: %p)", getName(), this, pipeRef, noDataTimeout, completionTimeout, buffer, size, completion);
    
	if (fOwner && !isInactive())
    {
		if (completion == NULL )
		{
			USBLog(1,"%s[%p]::WritePipe (async) bad arguments (%qd, %qx, %p)", getName(), this, size, buffer, completion); 
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			// This is an Async request 
			IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
			
			USBLog(7,"%s[%p]::WritePipe (async) creating IOMD", getName(), this); 
			mem = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionOut, fTask);
			if (!mem)
			{
				USBLog(1,"%s[%p]::WritePipe (async ) IOMemoryDescriptor::withAddressRange returned NULL", getName(), this); 
				ret = kIOReturnNoMemory;
				goto Exit;
			}

			ret = mem->prepare();
			if ( ret != kIOReturnSuccess)
			{
				USBLog(3,"%s[%p]::WritePipe (async) mem->prepare() returned 0x%x", getName(), this, ret); 
				mem->release();
				goto Exit;
			}

			pb->fMax = size;
			pb->fMem = mem;
			
			ret = pipeObj->Write(mem, noDataTimeout, completionTimeout, completion);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(5,"%s[%p]::WritePipe (async) returned 0x%x", getName(), this, ret); 
				mem->complete();
				mem->release();
			}
		}
		else
		{
			USBLog(5,"%s[%p]::WritePipe (async) can't find pipeRef, returning kIOUSBUnknownPipeErr", getName(), this); 
			ret = kIOUSBUnknownPipeErr;
		}
	}
	
Exit:
	if (pipeObj)
		pipeObj->release();
	
    if (ret)
	{
		USBLog(3, "%s[%p]::WritePipe (async - returning err %x", getName(), this, ret);
	}
	
	return ret;
}


IOReturn
IOUSBInterfaceUserClientV2::WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, const void *buf, UInt32 size)
{
    IOReturn				ret = kIOReturnSuccess;
    IOMemoryDescriptor *	mem = NULL;
    IOUSBPipe *				pipeObj;
	
    USBLog(7, "+%s(%p)::WritePipe < 4K (pipeRef: %d, %ld, %ld, buf: %p, size: %ld)", getName(), this, pipeRef, noDataTimeout, completionTimeout, buf, size);
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
	
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			// Sync, < 4K
			mem = IOMemoryDescriptor::withAddress( (void *) buf, size, kIODirectionOut);
			if(mem) 
			{
				ret = mem->prepare();
				if ( ret == kIOReturnSuccess)
				{
					ret = pipeObj->Write(mem, noDataTimeout, completionTimeout);
					if ( ret != kIOReturnSuccess)
					{
						USBLog(5,"%s[%p]::WritePipe (sync < 4K) returned 0x%x", getName(), this, ret); 
					}
					mem->complete();
				}
				else
				{
					USBLog(3,"%s[%p]::WritePipe (sync < 4K) mem->prepare() returned 0x%x", getName(), this, ret); 
				}
				
				mem->release();
				mem = NULL;
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
		USBLog(3, "%s[%p]::WritePipe < 4K - returning err %x", getName(), this, ret);
	}

    DecrementOutstandingIO();
    return ret;
}


IOReturn
IOUSBInterfaceUserClientV2::WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem)
{
    IOReturn				ret;
    IOUSBPipe 			*	pipeObj;
	
    USBLog(7, "+%s(%p)::WritePipe > 4K (pipeRef: %d, %ld, %ld, IOMD: %p)", getName(), this, pipeRef, noDataTimeout, completionTimeout, mem);
	
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			if(mem)
			{
				ret = mem->prepare();
				if (ret == kIOReturnSuccess)
				{
					ret = pipeObj->Write(mem, noDataTimeout, completionTimeout );
					mem->complete();
				}
				else
				{
					USBLog(1,"%s[%p]::WritePipe > (sync > 4K) mem->prepare() returned 0x%x", getName(), this, ret); 
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
		USBLog(3, "%s[%p]::WritePipe > 4K - returning err %x", getName(), this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_GetPipeProperties(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_GetPipeProperties", target->getName(), target);
	
    return target->GetPipeProperties((UInt8)arguments->scalarInput[0], &(arguments->scalarOutput[0]),&(arguments->scalarOutput[1]), 
										&(arguments->scalarOutput[2]), &(arguments->scalarOutput[3]), &(arguments->scalarOutput[4]));
}

IOReturn
IOUSBInterfaceUserClientV2::GetPipeProperties(UInt8 pipeRef, uint64_t *direction, uint64_t *number, uint64_t *transferType, uint64_t *maxPacketSize, uint64_t *interval)
{
    IOUSBPipe 			*pipeObj;
    IOReturn			ret = kIOReturnSuccess;
	
    USBLog(7, "+%s[%p]::GetPipeProperties for pipe %d", getName(), this, pipeRef);
	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);    
		if(pipeObj)
		{
			if (direction)
			*direction = pipeObj->GetDirection();
			if (number)
			*number = pipeObj->GetEndpointNumber();
			if (transferType)
			*transferType = pipeObj->GetType();
			if (maxPacketSize)
			*maxPacketSize = pipeObj->GetMaxPacketSize();
			if (interval)
			*interval = pipeObj->GetInterval();
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "%s[%p]::GetPipeProperties - returning err %x", getName(), this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_GetPipeStatus(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_GetPipeStatus", target->getName(), target);
    return target->GetPipeStatus((UInt8)arguments->scalarInput[0]);
}


IOReturn 
IOUSBInterfaceUserClientV2::GetPipeStatus(UInt8 pipeRef)
{
    IOUSBPipe 		*pipeObj;
    IOReturn		ret;
    
    USBLog(7, "+%s[%p]::GetPipeStatus", getName(), this);
    
    IncrementOutstandingIO();
	
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			ret = pipeObj->GetPipeStatus();
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
    
    DecrementOutstandingIO();
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_AbortPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_AbortPipe", target->getName(), target);
    return target->AbortPipe((UInt8)arguments->scalarInput[0]);
}

IOReturn 
IOUSBInterfaceUserClientV2::AbortPipe(UInt8 pipeRef)
{
    IOUSBPipe 		*pipeObj;
    IOReturn		ret;
    
    USBLog(7, "+%s[%p]::AbortPipe", getName(), this);
    
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
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
        if ( ret == kIOUSBUnknownPipeErr )
		{
            USBLog(6, "%s[%p]::AbortPipe - returning err %x", getName(), this, ret);
		}
        else
		{
            USBLog(3, "%s[%p]::AbortPipe - returning err %x", getName(), this, ret);
		}
    }
    
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_ResetPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_ResetPipe", target->getName(), target);
    return target->ResetPipe((UInt8)arguments->scalarInput[0]);
}

IOReturn 
IOUSBInterfaceUserClientV2::ResetPipe(UInt8 pipeRef)
{
    IOUSBPipe 			*pipeObj;
    IOReturn			ret;
	
    USBLog(7, "+%s[%p]::ResetPipe", getName(), this);
	
    IncrementOutstandingIO();
	
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			ret = pipeObj->Reset();
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "%s[%p]::ResetPipe - returning err %x", getName(), this, ret);
    }
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_ClearPipeStall(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_ClearPipeStall", target->getName(), target);
    return target->ClearPipeStall((UInt8)arguments->scalarInput[0], (bool)arguments->scalarInput[1]);
}

IOReturn 
IOUSBInterfaceUserClientV2::ClearPipeStall(UInt8 pipeRef, bool bothEnds)
{
    IOUSBPipe 		*pipeObj;
    IOReturn		ret;
	
    USBLog(7, "+%s[%p]::ClearPipeStall (pipe: %d, bothEnds: %d)", getName(), this, pipeRef, bothEnds);
	
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			USBLog(6, "%s[%p]::ClearPipeStall = bothEnds = %d", getName(), this, bothEnds);
			ret = pipeObj->ClearPipeStall(bothEnds);
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "%s[%p]::ClearPipeStall - returning err %x", getName(), this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}



IOReturn IOUSBInterfaceUserClientV2::_SetPipePolicy(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_SetPipePolicy", target->getName(), target);
    return target->SetPipePolicy((UInt8)arguments->scalarInput[0], (UInt16)arguments->scalarInput[1], (UInt8)arguments->scalarInput[2]);
}

IOReturn 
IOUSBInterfaceUserClientV2::SetPipePolicy(UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval)
{
    IOUSBPipe 		*pipeObj;
    IOReturn		ret;
	
    USBLog(7, "+%s[%p]::SetPipePolicy", getName(), this);
	
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			USBLog(7, "%s[%p]::SetPipePolicy(%d, %d)", getName(), this, maxPacketSize, maxInterval);
			ret = pipeObj->SetPipePolicy(maxPacketSize, maxInterval);
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if (ret)
	{
		USBLog(3, "%s[%p]::SetPipePolicy - returning err %x", getName(), this, ret);
	}
	
	DecrementOutstandingIO();
	return ret;
}



#pragma mark Control Request Out

IOReturn 
IOUSBInterfaceUserClientV2::_ControlRequestOut(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn								ret;
	
    USBLog(7, "+%s[%p]::_ControlRequestOut", target->getName(), target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort ) 
	{
		IOUSBCompletion						tap;
		IOUSBUserClientAsyncParamBlock *	pb = (IOUSBUserClientAsyncParamBlock*) IOMalloc(sizeof(IOUSBUserClientAsyncParamBlock));
		
        if (!pb) 
            return kIOReturnNoMemory;
		
		target->retain();
        target->IncrementOutstandingIO();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV2::ReqComplete;
        tap.parameter = pb;
		
 		// ControlRequestIn, Async, buffer in  clients task
		ret = target->ControlRequestOut(	(UInt8)arguments->scalarInput[0],			// pipeRef
											(UInt8)arguments->scalarInput[1],			// bmRequestType,
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
		// ControlRequestOut, Sync, > 4K
        ret = target->ControlRequestOut(	(UInt8)arguments->scalarInput[0],			// pipeRef
											(UInt8)arguments->scalarInput[1],			// bmRequestType,
											(UInt8)arguments->scalarInput[2],			// bRequest,
											(UInt16)arguments->scalarInput[3],			// wValue,
											(UInt16)arguments->scalarInput[4],			// wIndex,
											(UInt32)arguments->scalarInput[7],			// noDataTimeout,
											(UInt32)arguments->scalarInput[8],			// completionTimeout,
											arguments->structureInputDescriptor);		// IOMD for request
	}
    else
	{
		// ControlRequestOut, Sync, < 4K
        ret = target->ControlRequestOut(	(UInt8)arguments->scalarInput[0],			// pipeRef
											(UInt8)arguments->scalarInput[1],			// bmRequestType,
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
IOUSBInterfaceUserClientV2::ControlRequestOut(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, mach_vm_size_t size, mach_vm_address_t buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion * completion)
{
	IOReturn				ret = kIOReturnSuccess;
	IOUSBDevRequest			req;
	IOMemoryDescriptor *	mem = NULL;
	IOUSBPipe *				pipeObj = NULL;;
	
	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestOut (Async) : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x, noDataTimeout = %ld completionTimeout = %ld",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   noDataTimeout,
		   completionTimeout);
	
	if (fOwner && !isInactive())
    {
		if (completion == NULL)
		{
			USBLog(1,"%s[%p]::ControlRequestOut (async) completion is NULL!", getName(), this); 
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			// This is an Async request 
			IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
			
			if (size != 0 )
			{
				USBLog(7,"%s[%p]::ControlRequestOut (async) creating IOMD", getName(), this); 
				mem = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionOut, fTask);
				if (!mem)
				{
					USBLog(1,"%s[%p]::ControlRequestOut (async ) IOMemoryDescriptor::withAddressRange returned NULL", getName(), this); 
					ret = kIOReturnNoMemory;
					goto Exit;
				}
				
				ret = mem->prepare();
				if ( ret != kIOReturnSuccess)
				{
					USBLog(3,"%s[%p]::ControlRequestOut (async) mem->prepare() returned 0x%x", getName(), this, ret); 
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
			
			ret = pipeObj->ControlRequest(&pb->req, noDataTimeout, completionTimeout, completion);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(5,"%s[%p]::ControlRequestOut (async) returned 0x%x", getName(), this, ret); 
				mem->complete();
				mem->release();
			}
		}
		else
		{
			USBLog(5,"%s[%p]::ControlRequestOut (async) can't find pipeRef, returning kIOUSBUnknownPipeErr", getName(), this); 
			ret = kIOUSBUnknownPipeErr;
		}
	}
	
Exit:
	if (pipeObj)
		pipeObj->release();
	
    if (ret)
	{
		USBLog(3, "%s[%p]::ControlRequestOut (async) - returning err %x", getName(), this, ret);
	}
	
	return ret;
}


// This is an async/sync with < 4K request
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestOut(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, const void *requestBuffer, uint32_t size)
{
	IOReturn				ret = kIOReturnSuccess;
	IOUSBDevRequest			req;
	IOMemoryDescriptor *	mem = NULL;
	IOUSBPipe		*		pipeObj = NULL;
	
	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestOut < 4K : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4x noDataTimeout = %ld completionTimeout = %ld",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   size,
		   noDataTimeout,
		   completionTimeout);
	
	IncrementOutstandingIO();
	
	if (fOwner && !isInactive())
	{
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// This is a sync request that is < 4K
			req.bmRequestType = bmRequestType;
			req.bRequest = bRequest;
			req.wValue = wValue;
			req.wIndex = wIndex;
			req.wLength = size;
			req.pData = (void *)requestBuffer;
			req.wLenDone = 0;
			
			ret = pipeObj->ControlRequest(&req, noDataTimeout, completionTimeout);
			
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
	}
	else
		ret = kIOReturnNotAttached;
	
	if (ret)
	{
		USBLog(3, "%s[%p]::ControlRequest - returning err %x", getName(), this, ret);
	}
	
	DecrementOutstandingIO();
	return ret;
}

//  This is a ControlRequestOut (sync) that is OOL, so it has an IOMD
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestOut(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem)
{
    IOUSBDevRequestDesc		req;
	IOReturn				ret;
    IOUSBPipe *				pipeObj = NULL;
	
	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestOut > 4K: bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4lx noDataTimeout = %ld completionTimeout = %ld",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   mem->getLength(),
		   noDataTimeout,
		   completionTimeout);
	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
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
				
				ret = pipeObj->ControlRequest(&req, noDataTimeout, completionTimeout);
				
				mem->complete();
			}
			else
			{
				USBLog(4,"%s[%p]::ControlRequestOut > 4K mem->prepare() returned 0x%x", getName(), this, ret); 
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
		USBLog(3, "%s[%p]::ControlRequestOut (sync OOL) - returning err %x", getName(), this, ret);
	}
	
	DecrementOutstandingIO();
	return ret;
}



#pragma mark Control Request In

IOReturn IOUSBInterfaceUserClientV2::_ControlRequestIn(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn								ret;
	
    USBLog(7, "+%s[%p]::_ControlRequestIn", target->getName(), target);
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
        tap.action = &IOUSBInterfaceUserClientV2::ReqComplete;
        tap.parameter = pb;
		
		// ControlRequestIn, Async, buffer in  clients task
		ret = target->ControlRequestIn(	(UInt8)arguments->scalarInput[0],			// pipeRef
										(UInt8)arguments->scalarInput[1],			// bmRequestType,
										(UInt8)arguments->scalarInput[2],			// bRequest,
										(UInt16)arguments->scalarInput[3],			// wValue,
										(UInt16)arguments->scalarInput[4],			// wIndex,
										(mach_vm_size_t)arguments->scalarInput[5],	// pData (buffer),
										(mach_vm_address_t)arguments->scalarInput[6],	// wLength (bufferSize),
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
		// ControlRequestIn, Sync, > 4K
        ret = target->ControlRequestIn(	(UInt8)arguments->scalarInput[0],			// pipeRef
										(UInt8)arguments->scalarInput[1],			// bmRequestType,
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
		// ControlRequestIn, Sync, < 4K
        ret = target->ControlRequestIn(	(UInt8)arguments->scalarInput[0],			// pipeRef
										(UInt8)arguments->scalarInput[1],			// bmRequestType,
										(UInt8)arguments->scalarInput[2],			// bRequest,
										(UInt16)arguments->scalarInput[3],			// wValue,
										(UInt16)arguments->scalarInput[4],			// wIndex,
										(UInt32)arguments->scalarInput[7],			// noDataTimeout,
										(UInt32)arguments->scalarInput[8],			// completionTimeout,
										arguments->structureOutput,					// bufferPtr
										&(arguments->structureOutputSize));			// buffer size
	}
	return ret;
}  



// This is an Async
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestIn(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, mach_vm_size_t size, mach_vm_address_t buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion * completion)
{
	IOReturn				ret = kIOReturnSuccess;
	IOUSBDevRequest			req;
	IOMemoryDescriptor *	mem = NULL;
	IOUSBPipe *				pipeObj = NULL;;

	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestIn (Async) : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x, noDataTimeout = %ld completionTimeout = %ld",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   noDataTimeout,
		   completionTimeout);
	
	if (fOwner && !isInactive())
    {
		if (completion == NULL)
		{
			USBLog(1,"%s[%p]::ControlRequestIn (async) bad arguments (%qd, %qx, %p)", getName(), this, size, buffer, completion); 
			ret = kIOReturnBadArgument;
			goto Exit;
		}
		
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			// This is an Async request 
			IOUSBUserClientAsyncParamBlock * pb = (IOUSBUserClientAsyncParamBlock *)completion->parameter;
			
			if ( size != 0)
			{
				USBLog(7,"%s[%p]::ControlRequestIn (async) creating IOMD", getName(), this); 
				mem = IOMemoryDescriptor::withAddressRange( buffer, size, kIODirectionIn, fTask);
				if (!mem)
				{
					USBLog(1,"%s[%p]::ControlRequestIn (async ) IOMemoryDescriptor::withAddressRange returned NULL", getName(), this); 
					ret = kIOReturnNoMemory;
					goto Exit;
				}
				
				ret = mem->prepare();
				if ( ret != kIOReturnSuccess)
				{
					USBLog(3,"%s[%p]::ControlRequestIn (async) mem->prepare() returned 0x%x", getName(), this, ret); 
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
			
			ret = pipeObj->ControlRequest(&pb->req, noDataTimeout, completionTimeout, completion);
			if ( ret != kIOReturnSuccess)
			{
				USBLog(5,"%s[%p]::ControlRequestIn (async) returned 0x%x", getName(), this, ret); 
				mem->complete();
				mem->release();
			}
		}
		else
		{
			USBLog(5,"%s[%p]::ControlRequestIn (async) can't find pipeRef, returning kIOUSBUnknownPipeErr", getName(), this); 
			ret = kIOUSBUnknownPipeErr;
		}
	}
	
Exit:
	
	if (pipeObj)
		pipeObj->release();
	
    if (ret)
	{
		USBLog(3, "%s[%p]::ControlRequestIn (async) - returning err %x", getName(), this, ret);
	}
	
	return ret;
}

	// This is an sync with < 4K request
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestIn(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, void *requestBuffer, uint32_t *size)
{
	IOReturn				ret = kIOReturnSuccess;
	IOUSBDevRequest			req;
	IOMemoryDescriptor *	mem = NULL;
	IOUSBPipe *				pipeObj = NULL;;
	
	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestIn < 4K : bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4x noDataTimeout = %ld completionTimeout = %ld",
		   bmRequestType,
		   bRequest,
		   wValue,
		   wIndex,
		   *size,
		   noDataTimeout,
		   completionTimeout);
	
	IncrementOutstandingIO();
	
	if (fOwner && !isInactive())
	{
		pipeObj = GetPipeObj(pipeRef);
		if (pipeObj)
		{
			// This is a sync request that is < 4K
			req.bmRequestType = bmRequestType;
			req.bRequest = bRequest;
			req.wValue = wValue;
			req.wIndex = wIndex;
			req.wLength = *size;
			req.pData = requestBuffer;
			req.wLenDone = 0;
			
			ret = pipeObj->ControlRequest(&req, noDataTimeout, completionTimeout);
			USBLog(7, "%s[%p]::ControlRequestIn (sync < 4k) err:0x%x, wLenDone = %ld", getName(), this, ret, req.wLenDone);
		
			if (ret == kIOReturnSuccess) 
			{
				*size = req.wLenDone;
			}
			else 
			{
				USBLog(3, "%s[%p]::ControlRequestIn (sync < 4k) err:0x%x", getName(), this, ret);
				*size = 0;
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
		USBLog(3, "%s[%p]::ControlRequestIn < 4K - returning err %x, size: %d", getName(), this, ret, *size);
	}
	
	DecrementOutstandingIO();
	return ret;
}

//  This is a ControlRequestIn, Sync that is OOL, so it has an IOMD
IOReturn
IOUSBInterfaceUserClientV2::ControlRequestIn(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem, uint32_t *pOutSize)
{
	IOUSBDevRequestTO		reqIn;
    IOUSBDevRequestDesc		req;
	IOReturn				ret;
    IOUSBPipe *				pipeObj = NULL;
	
    USBLog(7, "+%s[%p]::ControlRequestIn > 4K ", getName(), this);

	// Copy the parameters to the reqIn
	reqIn.bmRequestType = bmRequestType;
	reqIn.bRequest = bRequest;
	reqIn.wValue = wValue;
	reqIn.wIndex = wIndex;
	reqIn.wLength = *pOutSize;
	reqIn.noDataTimeout = noDataTimeout;
	reqIn.completionTimeout = completionTimeout;
	
	USBLog(7, "IOUSBInterfaceUserClientV2::ControlRequestIn > 4K: bmRequestType = 0x%2.2x bRequest = 0x%2.2x wValue = 0x%4.4x wIndex = 0x%4.4x wLength = 0x%4.4x noDataTimeout = %ld completionTimeout = %ld",
				reqIn.bmRequestType,
				reqIn.bRequest,
				reqIn.wValue,
				reqIn.wIndex,
				reqIn.wLength,
				reqIn.noDataTimeout,
				reqIn.completionTimeout);
	
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(pipeRef);
		if(pipeObj)
		{
			ret = mem->prepare();
			
			if (ret == kIOReturnSuccess)
			{
				req.bmRequestType = reqIn.bmRequestType;
				req.bRequest = reqIn.bRequest;
				req.wValue = reqIn.wValue;
				req.wIndex = reqIn.wIndex;
				req.wLength = reqIn.wLength;
				req.pData = mem;
				req.wLenDone = 0;
				
				ret = pipeObj->ControlRequest(&req, reqIn.noDataTimeout, reqIn.completionTimeout);
				
				if (ret == kIOReturnSuccess) 
					*pOutSize = req.wLenDone;
				else 
					*pOutSize = 0;
				
				mem->complete();
			}
			else
			{
				USBLog(4,"%s[%p]::ControlRequestIn > 4K mem->prepare() returned 0x%x", getName(), this, ret); 
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
		USBLog(3, "%s[%p]::ControlRequestIn > 4K - returning err %x, outSize: %d", getName(), this, ret, *pOutSize);
	}
	
	DecrementOutstandingIO();
	return ret;
}


#pragma mark Isoch Methods


IOReturn IOUSBInterfaceUserClientV2::_ReadIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn								ret = kIOReturnUnsupported;
	
	// This call will always be Async and < 4K (the size of the IOUSBIsocStruct.  The call itself will need to map the buffers
	
    USBLog(7, "+%s[%p]::_ReadIsochPipe", target->getName(), target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort ) 
	{
		target->retain();
        target->IncrementOutstandingIO();
        
        if ( arguments->structureInputDescriptor ) 
		{
			USBLog(3, "+%s[%p]::__ReadIsochPipe  We got an unexpected structureOutputDescriptor.  Returning bad argument ", target->getName(), target);
            ret = kIOReturnBadArgument;
		}
        else
			ret = target->DoIsochPipeAsync( (IOUSBIsocStruct *) arguments->structureInput, arguments->asyncReference, arguments->asyncReferenceCount, kIODirectionIn);
		
        if ( ret ) 
		{
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	
	return ret;
}  


IOReturn IOUSBInterfaceUserClientV2::_WriteIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn								ret = kIOReturnUnsupported;
	
	// This call will always be Async and < 4K (the size of the IOUSBIsocStruct.  The call itself will need to map the buffers
	
    USBLog(7, "+%s[%p]::_WriteIsochPipe", target->getName(), target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort ) 
	{
 		target->retain();
		target->IncrementOutstandingIO();
        		
        if ( arguments->structureInputDescriptor ) 
		{
			USBLog(3, "+%s[%p]::_WriteIsochPipe  We got an unexpected structureInputDescriptor.  Returning unsupported ", target->getName(), target);
            ret = kIOReturnUnsupported;
		}
        else
			ret = target->DoIsochPipeAsync( (IOUSBIsocStruct *) arguments->structureInput,  arguments->asyncReference, arguments->asyncReferenceCount, kIODirectionOut);
		
        if ( ret ) 
		{
			target->DecrementOutstandingIO();
			target->release();
		}
	}
	
	return ret;
}  



IOReturn 
IOUSBInterfaceUserClientV2::DoIsochPipeAsync(IOUSBIsocStruct *stuff, io_user_reference_t * asyncReference, uint32_t asyncCount, IODirection direction)
{
    IOReturn				ret;
    IOUSBPipe *				pipeObj = NULL;
    IOMemoryDescriptor *	dataMem = NULL;
    IOMemoryDescriptor *	countMem = NULL;
    uint32_t				frameLen = 0;	// In bytes
    bool					countMemPrepared = false;
    bool					dataMemPrepared = false;
	IOUSBIsocCompletion		tap;
	IOUSBInterfaceUserClientISOAsyncParamBlock *	pb = NULL;
		
    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(stuff->fPipe);
		if(pipeObj)
		{
			frameLen = stuff->fNumFrames * sizeof(IOUSBIsocFrame);
			do {
				dataMem = IOMemoryDescriptor::withAddress( (vm_address_t)stuff->fBuffer, stuff->fBufSize, direction, fTask);
				if(!dataMem) 
				{
                    USBLog(1, "%s[%p]::DoIsochPipeAsync could not create dataMem descriptor", getName(), this);
					ret = kIOReturnNoMemory;
					break;
				}
				ret = dataMem->prepare();
				if (ret != kIOReturnSuccess)
                {
                    USBLog(1, "%s[%p]::DoIsochPipeAsync could not prepare dataMem descriptor (0x%x)", getName(), this, ret);
					break;
                }
				
                dataMemPrepared = true;
                
				countMem = IOMemoryDescriptor::withAddress( (vm_address_t)stuff->fFrameCounts, frameLen, kIODirectionOutIn, fTask);
				if(!countMem) 
				{
                    USBLog(1, "%s[%p]::DoIsochPipeAsync could not create countMem descriptor", getName(), this);
					ret = kIOReturnNoMemory;
					break;
				}
				
                ret = countMem->prepare();
                if (ret != kIOReturnSuccess)
                {
                    USBLog(1, "%s[%p]::DoIsochPipeAsync could not prepare dataMem descriptor (0x%x)", getName(), this, ret);
                    break;
                }
                countMemPrepared = true;
				
				pb = (IOUSBInterfaceUserClientISOAsyncParamBlock*) IOMalloc(sizeof(IOUSBInterfaceUserClientISOAsyncParamBlock) + frameLen);
				if (!pb) 
				{
					ret = kIOReturnNoMemory;
					break;
				}
				
                // Copy in requested transfers, we'll copy out result in completion routine
                
				bcopy(asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
				pb->fAsyncCount = asyncCount;
				pb->frameLen = frameLen;
				pb->frameBase = stuff->fFrameCounts;
				pb->numFrames = stuff->fNumFrames;
				pb->dataMem = dataMem;
				pb->countMem = countMem;
				
				countMem->readBytes(0, pb->frames, frameLen);
				
				tap.target = this;
				tap.action = &IOUSBInterfaceUserClientV2::IsoReqComplete;
				tap.parameter = pb;
				
				USBLog(7,"+%s[%p]::DoIsochPipeAsync  fPipe: %ld, dataMem: %p, fBufSize = 0x%lx, fStartFrame: %qd", getName(), this,
					   stuff->fPipe,dataMem, stuff->fBufSize, stuff->fStartFrame);

				if(direction == kIODirectionOut)
					ret = pipeObj->Write(dataMem, stuff->fStartFrame, stuff->fNumFrames, pb->frames, &tap);
				else
					ret = pipeObj->Read(dataMem, stuff->fStartFrame, stuff->fNumFrames, pb->frames, &tap);
			} while (false);
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if(kIOReturnSuccess != ret) 
    {
		USBLog(3, "%s[%p]::DoIsochPipeAsync err 0x%x", getName(), this, ret);
		if(dataMem)
        {
            if ( dataMemPrepared )
                dataMem->complete();
			dataMem->release();
            dataMem = NULL;
        }
        
		if(countMem)
        {
            if ( countMemPrepared )
                countMem->complete();
			countMem->release();
            countMem = NULL;
        }
        
    }
	
	if ( ret != kIOReturnSuccess )
	{
		USBLog(3, "%s[%p]::-DoIsochPipeAsync err 0x%x", getName(), this, ret);
	}
	
    return ret;
}


#pragma mark Low Latency Isoch Methods

IOReturn IOUSBInterfaceUserClientV2::_LowLatencyReadIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn								ret = kIOReturnUnsupported;
	
	// This call will always be Async and < 4K (the size of the IOUSBIsocStruct.  The call itself will need to map the buffers
	
    USBLog(7, "+%s[%p]::_LowLatencyReadIsochPipe", target->getName(), target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort ) 
	{
		IOUSBLowLatencyIsocCompletion	tap;
		IOUSBLowLatencyCommand *		command = NULL;
		
		// First, attempt to get a command for our transfer
		//
		command = (IOUSBLowLatencyCommand *) target->fFreeUSBLowLatencyCommandPool->getCommand(false);
		
		// If we couldn't get a command, increase the allocation and try again
		//
		if ( command == NULL )
		{
			target->IncreaseCommandPool();
			
			command = (IOUSBLowLatencyCommand *) target->fFreeUSBLowLatencyCommandPool->getCommand(false);
			if ( command == NULL )
			{
				USBLog(3,"%s[%p]::DoLowLatencyIsochPipeAsync Could not get a IOUSBLowLatencyIsocCommand", target->getName(),target);
				ret = kIOReturnNoResources;
				goto ErrorExit;
			}
		}
		
		target->retain();
		target->IncrementOutstandingIO();
        
		command->SetAsyncReference( arguments->asyncReference );
		command->SetAsyncCount(arguments->asyncReferenceCount);
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV2::LowLatencyIsoReqComplete;
        tap.parameter = command;
		
        if ( arguments->structureInputDescriptor ) 
		{
			USBLog(3, "+%s[%p]::_LowLatencyReadIsochPipe  We got an unexpected structureOutputDescriptor.  Returning unsupported ", target->getName(), target);
            ret = kIOReturnUnsupported;
		}
        else
			ret = target->DoLowLatencyIsochPipeAsync( (IOUSBLowLatencyIsocStruct *) arguments->structureInput,  &tap, kIODirectionIn);
		
		if(kIOReturnSuccess != ret) 
		{
			USBLog(3, "%s[%p]::_LowLatencyReadIsochPipe err 0x%x", target->getName(), target, ret);

			// return command
			//
			if ( command )
				target->fFreeUSBLowLatencyCommandPool->returnCommand(command);
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}

	ErrorExit:
	return ret;
}  


IOReturn IOUSBInterfaceUserClientV2::_LowLatencyWriteIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn								ret = kIOReturnUnsupported;
	
	// This call will always be Async and < 4K (the size of the IOUSBIsocStruct.  The call itself will need to map the buffers
	
    USBLog(7, "+%s[%p]::_LowLatencyWriteIsochPipe", target->getName(), target);
	// target->PrintExternalMethodArgs(arguments, 5);
	
	if ( arguments->asyncWakePort ) 
	{
		IOUSBLowLatencyIsocCompletion	tap;
		IOUSBLowLatencyCommand *		command = NULL;
		
		// First, attempt to get a command for our transfer
		//
		command = (IOUSBLowLatencyCommand *) target->fFreeUSBLowLatencyCommandPool->getCommand(false);
		
		// If we couldn't get a command, increase the allocation and try again
		//
		if ( command == NULL )
		{
			target->IncreaseCommandPool();
			
			command = (IOUSBLowLatencyCommand *) target->fFreeUSBLowLatencyCommandPool->getCommand(false);
			if ( command == NULL )
			{
				USBLog(3,"%s[%p]::DoLowLatencyIsochPipeAsync Could not get a IOUSBLowLatencyIsocCommand",target->getName(),target);
				ret = kIOReturnNoResources;
				goto ErrorExit;
			}
		}
		
		target->retain();
		target->IncrementOutstandingIO();
        
		command->SetAsyncReference( arguments->asyncReference );
		command->SetAsyncCount(arguments->asyncReferenceCount);
        
		tap.target = target;
        tap.action = &IOUSBInterfaceUserClientV2::LowLatencyIsoReqComplete;
        tap.parameter = command;
		
        if ( arguments->structureInputDescriptor ) 
		{
			USBLog(3, "+%s[%p]::_LowLatencyWriteIsochPipe  We got an unexpected structureInputDescriptor.  Returning unsupported ", target->getName(), target);
            ret = kIOReturnUnsupported;
		}
        else
			ret = target->DoLowLatencyIsochPipeAsync( (IOUSBLowLatencyIsocStruct *) arguments->structureInput,  &tap, kIODirectionOut);
		
		if(kIOReturnSuccess != ret) 
		{
			USBLog(3, "%s[%p]::_LowLatencyReadIsochPipe err 0x%x", target->getName(), target, ret);
			
			// return command
			if ( command )
				target->fFreeUSBLowLatencyCommandPool->returnCommand(command);
			
			target->DecrementOutstandingIO();
			target->release();
		}
	}

ErrorExit:
	return ret;
}  

IOReturn 
IOUSBInterfaceUserClientV2::DoLowLatencyIsochPipeAsync(IOUSBLowLatencyIsocStruct *isocInfo, IOUSBLowLatencyIsocCompletion *completion, IODirection direction)
{
    IOReturn							ret;
    IOUSBPipe *							pipeObj = NULL;
    IOUSBLowLatencyIsocCompletion		tap;
    IOMemoryDescriptor *				aDescriptor		= NULL;
	IOUSBLowLatencyCommand *			command = NULL;
    IOUSBLowLatencyIsocFrame *			pFrameList 		= NULL;
    IOUSBLowLatencyUserClientBufferInfo *	dataBuffer		= NULL;
    IOUSBLowLatencyUserClientBufferInfo *	frameListDataBuffer	= NULL;
	
    USBLog(7, "+%s[%p]::DoLowLatencyIsochPipeAsync", getName(), this);

    if (fOwner && !isInactive())
    {
		pipeObj = GetPipeObj(isocInfo->fPipe);
		if(pipeObj)
		{
			do {
                USBLog(6,"%s[%p]::DoLowLatencyIsochPipeAsync: dataBuffer cookie: %ld, offset: %ld, frameList cookie: %ld, offset : %ld", getName(),this, isocInfo->fDataBufferCookie, isocInfo->fDataBufferOffset, isocInfo->fFrameListBufferCookie, isocInfo->fFrameListBufferOffset );
                
				IOUSBLowLatencyCommand * command = (IOUSBLowLatencyCommand *)completion->parameter;

                // Find the buffer corresponding to the data buffer cookie:
                //
                dataBuffer = FindBufferCookieInList(isocInfo->fDataBufferCookie);
                
                if ( dataBuffer == NULL )
				{
					USBLog(3,"%s[%p]::DoLowLatencyIsochPipeAsync: Could not find our buffer (cookie %ld) in the list", getName(),this, isocInfo->fDataBufferCookie );
					ret = kIOReturnNoMemory;
					break;
				}
                
                USBLog(6,"%s[%p]::DoLowLatencyIsochPipeAsync: Found data buffer for cookie: %ld, descriptor: %p, uhciDescriptor: %p, offset : %ld", getName(),this, isocInfo->fDataBufferCookie, 
				dataBuffer->bufferDescriptor, dataBuffer->writeDescritporForUHCI,isocInfo->fDataBufferOffset );
				
                // Create a new IOMD that is a subrange of our data buffer memory descriptor, and prepare it
                //
                aDescriptor = IOMemoryDescriptor::withSubRange( dataBuffer->bufferDescriptor == NULL ? dataBuffer->writeDescritporForUHCI : dataBuffer->bufferDescriptor, isocInfo->fDataBufferOffset, isocInfo->fBufSize, direction );
                if ( aDescriptor == NULL )
				{
					USBLog(3,"%s[%p]::DoLowLatencyIsochPipeAsync: Could not create an IOMD:withSubRange", getName(),this );
					ret = kIOReturnNoMemory;
					break;
				}
				
                // Prepare this descriptor
                //
                ret = aDescriptor->prepare();
                if (ret != kIOReturnSuccess)
                {
 					USBLog(3,"%s[%p]::DoLowLatencyIsochPipeAsync: Preparing the descriptor returned 0x%x", getName(),this, ret );
					break;
                }
                
                // Find the buffer corresponding to the frame list cookie:
                //
                frameListDataBuffer = FindBufferCookieInList(isocInfo->fFrameListBufferCookie);
                
                if ( frameListDataBuffer == NULL )
				{
					USBLog(3,"%s[%p]::DoLowLatencyIsochPipeAsync: Could not find our buffer (cookie %ld) in the list, returning kIOReturnNoMemory", getName(),this, isocInfo->fFrameListBufferCookie );
					ret = kIOReturnNoMemory;
					break;
				}
                
                USBLog(7,"%s[%p]::DoLowLatencyIsochPipeAsync: Found frameList buffer for cookie: %ld, descriptor: %p, offset : %ld", getName(),this, isocInfo->fFrameListBufferCookie, 
				(void *) frameListDataBuffer->frameListKernelAddress,isocInfo->fFrameListBufferOffset );
				
                // Get our virtual address by looking at the buffer data and adding in the offset that was passed in
                //
                pFrameList = (IOUSBLowLatencyIsocFrame *) ( (UInt32) frameListDataBuffer->frameListKernelAddress + isocInfo->fFrameListBufferOffset);
                
                // Copy the data into our command buffer
                //
                command->SetFrameBase( (void *) ((UInt32) frameListDataBuffer->bufferAddress + isocInfo->fFrameListBufferOffset));
                command->SetDataBuffer( aDescriptor );
                
                
				if ( direction == kIODirectionOut )
					ret = pipeObj->Write(aDescriptor, isocInfo->fStartFrame, isocInfo->fNumFrames, pFrameList, completion, isocInfo->fUpdateFrequency);
				else
					ret = pipeObj->Read(aDescriptor, isocInfo->fStartFrame, isocInfo->fNumFrames,pFrameList, completion, isocInfo->fUpdateFrequency);
				
            } while (false);
            
			pipeObj->release();
		}
		else
			ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    if(kIOReturnSuccess != ret) 
    {
		USBLog(3, "%s[%p]::DoLowLatencyIsochPipeAsync err 0x%x", getName(), this, ret);
		
        if ( aDescriptor )
        {
            aDescriptor->release();
            aDescriptor = NULL;
        }
    }
	
    USBLog(7, "-%s[%p]::DoLowLatencyIsochPipeAsync", getName(), this);
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_LowLatencyPrepareBuffer(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_LowLatencyPrepareBuffer", target->getName(), target);
    return target->LowLatencyPrepareBuffer( (LowLatencyUserBufferInfoV2 *) arguments->structureInput,  &(arguments->scalarOutput[0]));
}


IOReturn 
IOUSBInterfaceUserClientV2::LowLatencyPrepareBuffer(LowLatencyUserBufferInfoV2 *bufferData, uint64_t * addrOut)
{
    IOReturn								ret = kIOReturnSuccess;
    IOMemoryDescriptor *					aDescriptor = NULL;
    IOUSBLowLatencyUserClientBufferInfo *	kernelDataBuffer = NULL;
    IOMemoryMap *							frameListMap = NULL;
    IODirection								direction;
	IOBufferMemoryDescriptor *				uhciDescriptor = NULL;
	IOMemoryMap *							uhciMap = NULL;
	void *									uhciMappedData = NULL;
	bool									preparedUHCIDescriptor = false;
    
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
		USBLog(3, "%s[%p]::LowLatencyPrepareBuffer cookie: %ld, buffer: %p, size: %ld, type %ld, isPrepared: %d, next: %p", getName(), this,
			   bufferData->cookie,
			   bufferData->bufferAddress,
			   bufferData->bufferSize,
			   bufferData->bufferType,
			   bufferData->isPrepared,
			   bufferData->nextBuffer);
		
		*addrOut = 0;
		
		// Allocate a buffer and zero it
        //
        kernelDataBuffer = ( IOUSBLowLatencyUserClientBufferInfo *) IOMalloc( sizeof(IOUSBLowLatencyUserClientBufferInfo) );
        if (kernelDataBuffer == NULL )
        {
            USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not malloc buffer info (size = %ld)!", getName(), this, sizeof(IOUSBLowLatencyUserClientBufferInfo) );
            return kIOReturnNoMemory;
        }
        
        bzero(kernelDataBuffer, sizeof(IOUSBLowLatencyUserClientBufferInfo));
        
        // Set the known fields
        //
        kernelDataBuffer->cookie = bufferData->cookie;
        kernelDataBuffer->bufferType = bufferData->bufferType;
        
        // If we are on a UHCI controller and this is a low latency buffer, we need to allocate the data here and share it with user space, as UHCI requires
		// contiguous memory   
        //
        if ( fNeedContiguousMemoryForLowLatencyIsoch and ((bufferData->bufferType == kUSBLowLatencyWriteBuffer) or (bufferData->bufferType == kUSBLowLatencyReadBuffer)) )
		{
			USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  About to call to get a contiguous bufferMemoryDescriptor( %ld)!", getName(), this, bufferData->bufferSize );
            direction = ( bufferData->bufferType == kUSBLowLatencyWriteBuffer ? kIODirectionOut : kIODirectionIn );
			uhciDescriptor = IOBufferMemoryDescriptor::withOptions( direction | kIOMemoryPhysicallyContiguous | kIOMemoryKernelUserShared, bufferData->bufferSize, PAGE_SIZE);
			USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Returned %p", getName(), this, uhciDescriptor );
			
			if ( uhciDescriptor == NULL)
			{
				USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not create a physically contiguous IOBMD (size %ld)!", getName(), this, bufferData->bufferSize );
				ret = kIOReturnNoMemory;
				goto ErrorExit;
			}
			
			ret = uhciDescriptor->prepare();
            if (ret != kIOReturnSuccess)
            {
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not prepare the data buffer memory descriptor 0x%x", getName(), this, ret );
                goto ErrorExit;
            }
			
			preparedUHCIDescriptor = true;
			
			uhciMap = uhciDescriptor->map(fTask, NULL, kIOMapAnywhere, 0, 0	);
			if ( uhciMap == NULL )
			{
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not map the data buffer memory descriptor", getName(), this );
                goto ErrorExit;
			}
			
			uhciMappedData = (void *) uhciMap->getVirtualAddress();
			if ( uhciMappedData == NULL )
			{
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not get the virtual address of the map", getName(), this );
                goto ErrorExit;
			}
			
			USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  mapped virtual address = %p", getName(), this, uhciMappedData );
			
			UInt32 * theAddr = (UInt32 *) uhciMappedData;
			
			// At this point, we have the contiguous buffer used for the UHCI Low Latency Writes, so save it in our data structure so we can clean up later on
			//
            kernelDataBuffer->bufferSize = bufferData->bufferSize;
			kernelDataBuffer->writeDescritporForUHCI = uhciDescriptor;
			kernelDataBuffer->writeMapForUHCI= uhciMap;
			
			*addrOut = (UInt32)uhciMappedData;
		}
        else if ( (bufferData->bufferType == kUSBLowLatencyWriteBuffer) or ( bufferData->bufferType == kUSBLowLatencyReadBuffer) )
        {
            // We have a data buffer, so create a IOMD and prepare it
            //
            direction = ( bufferData->bufferType == kUSBLowLatencyWriteBuffer ? kIODirectionOut : kIODirectionIn );
            aDescriptor = IOMemoryDescriptor::withAddress((vm_address_t)bufferData->bufferAddress, bufferData->bufferSize, direction, fTask);
            if(!aDescriptor) 
            {
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not create a data buffer memory descriptor (addr: %p, size %ld)!", getName(), this, bufferData->bufferAddress, bufferData->bufferSize );
                ret = kIOReturnNoMemory;
                goto ErrorExit;
            }
			
            ret = aDescriptor->prepare();
            if (ret != kIOReturnSuccess)
            {
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not prepare the data buffer memory descriptor (0x%x)!", getName(), this, ret );
                goto ErrorExit;
            }
			
            
            // OK, now save this in our user client structure
            // 
            kernelDataBuffer->bufferAddress = bufferData->bufferAddress;
            kernelDataBuffer->bufferSize = bufferData->bufferSize;
            kernelDataBuffer->bufferDescriptor = aDescriptor;
            
            USBLog(3, "%s[%p]::LowLatencyPrepareBuffer  finished preparing data buffer: %p, size %ld, desc: %p, cookie: %ld", getName(), this,
				   kernelDataBuffer->bufferAddress, kernelDataBuffer->bufferSize, kernelDataBuffer->bufferDescriptor, kernelDataBuffer->cookie);
        }
        else if ( bufferData->bufferType == kUSBLowLatencyFrameListBuffer )
        {
            // We have a frame list that we need to map to the kernel's memory space
            //
            // Create a memory descriptor for our frame list and prepare it (pages it in if necesary and prepares it). 
            //
            aDescriptor = IOMemoryDescriptor::withAddress((vm_address_t)bufferData->bufferAddress, bufferData->bufferSize, kIODirectionOutIn, fTask);
            if(!aDescriptor) 
            {
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not create a frame list memory descriptor (addr: %p, size %ld)!", getName(), this, bufferData->bufferAddress, bufferData->bufferSize );
                ret = kIOReturnNoMemory;
                goto ErrorExit;
            }
            
            ret = aDescriptor->prepare();
            if (ret != kIOReturnSuccess)
            {
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not prepare the frame list memory descriptor (0x%x)", getName(), this, ret );
                goto ErrorExit;
            }
            
			
            // Map it into the kernel
            //
            frameListMap = aDescriptor->map();
            if (!frameListMap) 
            {
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not map the frame list memory descriptor!", getName(), this );
                ret = kIOReturnNoMemory;
                aDescriptor->complete();
                goto ErrorExit;
            }
			
            // Get the the mapped in virtual address and save it
            //
            kernelDataBuffer->frameListKernelAddress = frameListMap->getVirtualAddress();
            
            // Save the rest of the items
            // 
            kernelDataBuffer->bufferAddress = bufferData->bufferAddress;
            kernelDataBuffer->bufferSize = bufferData->bufferSize;
            kernelDataBuffer->frameListDescriptor = aDescriptor;
            kernelDataBuffer->frameListMap = frameListMap;
			
            USBLog(3, "%s[%p]::LowLatencyPrepareBuffer  finished preparing frame list buffer: %p, size %ld, desc: %p, map %p, kernel address: 0x%x, cookie: %ld", getName(), this,
				   kernelDataBuffer->bufferAddress, kernelDataBuffer->bufferSize, kernelDataBuffer->bufferDescriptor, kernelDataBuffer->frameListMap,
				   kernelDataBuffer->frameListKernelAddress,  kernelDataBuffer->cookie);
        }
		
        // Cool, we have a good buffer, add it to our list
        //
        AddDataBufferToList( kernelDataBuffer );
        
    }
    else
        ret = kIOReturnNotAttached;
	
ErrorExit:
		
		if (ret)
		{
			USBLog(3, "%s[%p]::LowLatencyPrepareBuffer - returning err %x", getName(), this, ret);
			
			if ( uhciDescriptor )
			{
				if (preparedUHCIDescriptor)
					uhciDescriptor->complete();
				
				uhciDescriptor->release();
			}
			
			if ( uhciMap != NULL )
				uhciMap->release();
			
		}
	
    DecrementOutstandingIO();    
    return ret;
}


IOReturn IOUSBInterfaceUserClientV2::_LowLatencyReleaseBuffer(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments)
{
    USBLog(7, "+%s[%p]::_LowLatencyReleaseBuffer", target->getName(), target);
    return target->LowLatencyReleaseBuffer( (LowLatencyUserBufferInfoV2 *) arguments->structureInput);
}

IOReturn 
IOUSBInterfaceUserClientV2::LowLatencyReleaseBuffer(LowLatencyUserBufferInfoV2 *dataBuffer)
{
    IOUSBLowLatencyUserClientBufferInfo *	kernelDataBuffer	= NULL;
    IOReturn				ret 			= kIOReturnSuccess;
    bool				found 			= false;
    
    IncrementOutstandingIO();
    
    USBLog(3, "+%s[%p]::LowLatencyReleaseBuffer for cookie: %ld", getName(), this, dataBuffer->cookie);
	
    if (fOwner && !isInactive())
    {
        // We need to find the LowLatencyUserBufferInfoV2 structure that contains
        // this buffer and then remove it from the list and free the structure
        // and the memory that was allocated for it
        //
        kernelDataBuffer = FindBufferCookieInList( dataBuffer->cookie );
        if ( kernelDataBuffer == NULL )
        {
            USBLog(3, "+%s[%p]::LowLatencyReleaseBuffer cookie: %ld, could not find buffer in list", getName(), this, dataBuffer->cookie);
            ret = kIOReturnBadArgument;
            goto ErrorExit;
        }
        
        // Now, remove this bufferData from the list
        //
        found = RemoveDataBufferFromList( kernelDataBuffer );
        if ( !found )
        {
            USBLog(3, "+%s[%p]::LowLatencyReleaseBuffer cookie: %ld, could not remove buffer (%p) from list", getName(), this, dataBuffer->cookie, kernelDataBuffer);
            ret = kIOReturnBadArgument;
            goto ErrorExit;
        }
		
        // Now, need to complete/release/free the objects we allocated in our prepare
        //
        if ( kernelDataBuffer->frameListMap )
        {
            USBLog(6, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseBuffer releasing frameListMap (0x%x)", this, kernelDataBuffer->frameListKernelAddress);
            kernelDataBuffer->frameListMap->release();
            kernelDataBuffer->frameListMap = NULL;
            kernelDataBuffer->frameListKernelAddress = NULL;
        }
		
        if ( kernelDataBuffer->frameListDescriptor )
        {
            kernelDataBuffer->frameListDescriptor->complete();
            kernelDataBuffer->frameListDescriptor->release();
            kernelDataBuffer->frameListDescriptor = NULL;
        }
        
        if ( kernelDataBuffer->bufferDescriptor )
        {
            kernelDataBuffer->bufferDescriptor->complete();
            kernelDataBuffer->bufferDescriptor->release();
            kernelDataBuffer->bufferDescriptor = NULL;
        }
		
        if ( kernelDataBuffer->writeDescritporForUHCI )
        {
            kernelDataBuffer->writeDescritporForUHCI->complete();
            kernelDataBuffer->writeDescritporForUHCI->release();
            kernelDataBuffer->writeDescritporForUHCI = NULL;
        }
		
        if ( kernelDataBuffer->writeMapForUHCI )
        {
            USBLog(3, "+IOUSBInterfaceUserClientV2[%p]::LowLatencyReleaseBuffer releasing uhciMap (%p)", this, kernelDataBuffer->writeMapForUHCI);
            kernelDataBuffer->writeMapForUHCI->release();
            kernelDataBuffer->writeMapForUHCI = NULL;
        }
        
        // Finally, deallocate our kernelDataBuffer
        //
        IOFree(kernelDataBuffer, sizeof(IOUSBLowLatencyUserClientBufferInfo));
		
    }
    else
        ret = kIOReturnNotAttached;
	
ErrorExit:
		
	if (ret)
	{
		USBLog(3, "%s[%p]::LowLatencyReleaseBuffer - returning err %x", getName(), this, ret);
	}
	
    DecrementOutstandingIO();
    return ret;
}

void
IOUSBInterfaceUserClientV2::AddDataBufferToList( IOUSBLowLatencyUserClientBufferInfo * insertBuffer )
{
    IOUSBLowLatencyUserClientBufferInfo *	buffer;
    
    // Traverse the list looking for last buffer and insert ours into it
    //
    if ( fUserClientBufferInfoListHead == NULL )
    {
        fUserClientBufferInfoListHead = insertBuffer;
        return;
    }
    
    buffer = fUserClientBufferInfoListHead;
    
    while ( buffer->nextBuffer != NULL )
    {
        buffer = buffer->nextBuffer;
    }
    
    // When we get here, nextBuffer is pointing to NULL.  Our insert buffer
    // already has nextBuffer = NULL, so we just insert it
    //
    buffer->nextBuffer = insertBuffer;
}

IOUSBLowLatencyUserClientBufferInfo *	
IOUSBInterfaceUserClientV2::FindBufferCookieInList( UInt32 cookie)
{
    IOUSBLowLatencyUserClientBufferInfo *	buffer;
    bool				foundIt = true;
    
    // Traverse the list looking for this buffer
    //
    if ( fUserClientBufferInfoListHead == NULL )
    {
        USBLog(3, "%s[%p]::FindBufferCookieInList - fUserClientBufferInfoListHead was NULL", getName(), this);
        return NULL;
    }
    
    buffer = fUserClientBufferInfoListHead;
    
    // Now, we need to see if our cookie is the same as one in the buffer list
    //
    while ( buffer->cookie != cookie )
    {
        buffer = buffer->nextBuffer;
        if ( buffer == NULL )
        {
            foundIt = false;
            break;
        }
    }
    
    if ( foundIt )
        return buffer;
    else
	{
        USBLog(3, "%s[%p]::FindBufferCookieInList - Could no find buffer for cookie (%ld), returning NULL", getName(), this, cookie);
        return NULL;
	}
}

bool			
IOUSBInterfaceUserClientV2::RemoveDataBufferFromList( IOUSBLowLatencyUserClientBufferInfo *removeBuffer)
{
    IOUSBLowLatencyUserClientBufferInfo *	buffer;
    IOUSBLowLatencyUserClientBufferInfo *	previousBuffer;
    
    // If our head is NULL, then this buffer does not exist in our list
    //
    if ( fUserClientBufferInfoListHead == NULL )
    {
        return false;
    }
    
    buffer = fUserClientBufferInfoListHead;
    
    // if our removeBuffer is the first one in the list, then just update the head and
    // exit
    //
    if ( buffer == removeBuffer )
    {
        fUserClientBufferInfoListHead = buffer->nextBuffer;
    }
    else
    {    
        // Need to start previousBuffer pointing to our initial buffer, in case we match
        // the first time
        //
        previousBuffer = buffer;
        
        while ( buffer->nextBuffer != removeBuffer )
        {
            previousBuffer = buffer;
            buffer = previousBuffer->nextBuffer;
        }
        
        // When we get here, buffer is pointing to the same buffer as removeBuffer
        // and previous buffer is pointing to the previous element in the link list,
        // so, update the link in previous to point to removeBuffer->nextBuffer;
        //
        buffer->nextBuffer = removeBuffer->nextBuffer;
    }
    
    return true;
}




#pragma mark Bookkeeping Methods

void
IOUSBInterfaceUserClientV2::DecrementOutstandingIO(void)
{
    if (!fGate)
    {
		if (!--fOutstandingIO && fNeedToClose)
		{
			USBLog(3, "%s[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - closing device", getName(), this, isInactive(), fOutstandingIO);
			if (fOwner && fOwner->isOpen(this) ) 
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
    fGate->runAction(ChangeOutstandingIO, (void*)-1);
}


void
IOUSBInterfaceUserClientV2::IncrementOutstandingIO(void)
{
    if (!fGate)
    {
		fOutstandingIO++;
		return;
    }
    fGate->runAction(ChangeOutstandingIO, (void*)1);
}


IOReturn
IOUSBInterfaceUserClientV2::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    IOUSBInterfaceUserClientV2 *me = OSDynamicCast(IOUSBInterfaceUserClientV2, target);
    UInt32	direction = (UInt32)param1;
    
    if (!me)
    {
		USBLog(1, "IOUSBInterfaceUserClientV2::ChangeOutstandingIO - invalid target");
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
                USBLog(6, "%s[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me->getName(), me, me->isInactive(), me->fOutstandingIO);
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
			USBLog(1, "%s[%p]::ChangeOutstandingIO - invalid direction", me->getName(), me);
    }
    return kIOReturnSuccess;
}


UInt32
IOUSBInterfaceUserClientV2::GetOutstandingIO()
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
IOUSBInterfaceUserClientV2::GetGatedOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    IOUSBInterfaceUserClientV2 *me = OSDynamicCast(IOUSBInterfaceUserClientV2, target);
	
    if (!me)
    {
		USBLog(1, "IOUSBInterfaceUserClientV2::GetGatedOutstandingIO - invalid target");
		return kIOReturnSuccess;
    }
	
    *(UInt32 *) param1 = me->fOutstandingIO;
	
    return kIOReturnSuccess;
}

void
IOUSBInterfaceUserClientV2::IncreaseCommandPool(void)
{
    int i;
    
    USBLog(3,"%s[%p] Adding (%d) to Command Pool", getName(), this, kSizeToIncrementLowLatencyCommandPool);
	
    for (i = 0; i < kSizeToIncrementLowLatencyCommandPool; i++)
    {
        IOUSBLowLatencyCommand *command = IOUSBLowLatencyCommand::NewCommand();
        if (command)
            fFreeUSBLowLatencyCommandPool->returnCommand(command);
    }
    
    fCurrentSizeOfCommandPool += kSizeToIncrementLowLatencyCommandPool;
	
}

void
IOUSBInterfaceUserClientV2::ReleasePreparedDescriptors(void)
{
    IOUSBLowLatencyUserClientBufferInfo *	kernelDataBuffer;
    IOUSBLowLatencyUserClientBufferInfo *	nextBuffer;
	
    if ( fOutstandingIO != 0 )
    {
        USBLog(7, "+IOUSBInterfaceUserClientV2[%p]::ReleasePreparedDescriptors: OutstandingIO is NOT 0 (%d) ", this, fOutstandingIO);
        return;
    }
    // If we have any kernelDataBuffer pointers, then release them now
    //
    if (fUserClientBufferInfoListHead != NULL)
    {
        //USBLog(7, "+%s[%p]::ReleasePreparedDescriptors: fUserClientBufferInfoListHead NOT NULL (%p) ", getName(), this, fUserClientBufferInfoListHead);
		
        nextBuffer = fUserClientBufferInfoListHead;
        kernelDataBuffer = fUserClientBufferInfoListHead;
        
        // Traverse the list and release memory
        //
        while ( nextBuffer != NULL )
        {
            nextBuffer = kernelDataBuffer->nextBuffer;
			
            // Now, need to complete/release/free the objects we allocated in our prepare
            //
            if ( kernelDataBuffer->frameListMap )
            {
                USBLog(3, "+IOUSBInterfaceUserClientV2[%p]::ReleasePreparedDescriptors releasing frameListMap (@ 0x%x)", this, kernelDataBuffer->frameListKernelAddress);
                kernelDataBuffer->frameListMap->release();
                kernelDataBuffer->frameListMap = NULL;
                kernelDataBuffer->frameListKernelAddress = NULL;
                
            }
			
            if ( kernelDataBuffer->frameListDescriptor )
            {
                kernelDataBuffer->frameListDescriptor->complete();
                kernelDataBuffer->frameListDescriptor->release();
                kernelDataBuffer->frameListDescriptor = NULL;
            }
			
            if ( kernelDataBuffer->bufferDescriptor )
            {
                // We call prepare on the bufferDescriptor, so we need to call complete on it 
                //
                kernelDataBuffer->bufferDescriptor->complete();
                kernelDataBuffer->bufferDescriptor->release();
                kernelDataBuffer->bufferDescriptor = NULL;
			}
            
			if ( kernelDataBuffer->writeDescritporForUHCI )
			{
				kernelDataBuffer->writeDescritporForUHCI->complete();
				kernelDataBuffer->writeDescritporForUHCI->release();
				kernelDataBuffer->writeDescritporForUHCI = NULL;
			}
			
			if ( kernelDataBuffer->writeMapForUHCI )
			{
				USBLog(3, "+IOUSBInterfaceUserClientV2[%p]::ReleasePreparedDescriptors releasing uhciMap (%p)", this, kernelDataBuffer->writeMapForUHCI);
				kernelDataBuffer->writeMapForUHCI->release();
				kernelDataBuffer->writeMapForUHCI = NULL;
			}
            // Finally, deallocate our kernelDataBuffer
            //
            IOFree(kernelDataBuffer, sizeof(IOUSBLowLatencyUserClientBufferInfo));
            
            kernelDataBuffer = nextBuffer;
        }
        
        fUserClientBufferInfoListHead = NULL;
    }
}

#pragma mark Helper Methods

IOUSBPipe*
IOUSBInterfaceUserClientV2::GetPipeObj(UInt8 pipeNo)
{
    IOUSBPipe *pipe = NULL;
    
    if (fOwner && !isInactive())
    {
        if (pipeNo == 0)
            pipe = fOwner->GetDevice()->GetPipeZero();
		
        if ((pipeNo > 0) && (pipeNo <= kUSBMaxPipes))
            pipe = fOwner->GetPipeObj(pipeNo-1);
    }
    // we need to retain the pipe object because it could actually get released by the device/interface
    // and we don't want it to go away. This retain should probably be done in IOUSBDevice or IOUSBInterface
    // but we would affect too many people if we did that right now.
    if (pipe)
        pipe->retain();
	
    return pipe;
}


#pragma mark IOKit Methods

//
// clientClose - my client on the user side has released the mach port, so I will no longer
// be talking to him
//
IOReturn  
IOUSBInterfaceUserClientV2::clientClose( void )
{
    USBLog(7, "+%s[%p]::clientClose(%p), IO: %d", getName(), this, fUserClientBufferInfoListHead, fOutstandingIO);
	
    // Sleep for 1 ms to allow other threads that are pending to run
    //
    IOSleep(1);
    
    // We need to destroy the pipes so that any bandwidth gets returned -- our client has died so keeping them around 
    // is not useful
    //
    if ( fDead && fOwner && !isInactive() && fOwner->isOpen(this) )
	{
		// If the interface is other than 0, set it to 0 before closing the pipes
		UInt8	altSetting = fOwner->GetAlternateSetting();
		
		if ( altSetting != 0 )
		{
			USBLog(6, "+%s[%p]::clientClose setting Alternate Interface to 0 before closing pipes", getName(), this);
			fOwner->SetAlternateInterface(this, 0);
		}
		
        fOwner->ClosePipes();
	}
    
	// If we are already inactive, it means that our IOUSBInterface is going/has gone away.  In that case
	// we really do not need to do anything as the IOKit termination will take care of cleaning things.
	if ( !isInactive() )
	{
		if ( fOutstandingIO == 0 )
		{
			USBLog(6, "+%s[%p]::clientClose closing provider", getName(), this);
			
			if ( fOwner && fOwner->isOpen(this)) 
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
			USBLog(5, "+%s[%p]::clientClose will close provider later", getName(), this);
			fNeedToClose = true;
		}
		
		fTask = NULL;
		
		terminate();
	}
	
    USBLog(7, "-%s[%p]::clientClose(%p)", getName(), this, fUserClientBufferInfoListHead);
	
    return kIOReturnSuccess;			// DONT call super::clientClose, which just returns notSupported
}


IOReturn 
IOUSBInterfaceUserClientV2::clientDied( void )
{
    IOReturn ret;
	
    USBLog(6, "+%s[%p]::clientDied() IO: %d", getName(), this, fOutstandingIO);
    
    retain();                       // We will release once any outstandingIO is finished
	
    fDead = true;				// don't send any mach messages in this case
    ret = super::clientDied();
	
    USBLog(6, "-%s[%p]::clientDied()", getName(), this);
	
    return ret;
}

//
// stop
// 
// This IOService method is called AFTER we have closed our provider, assuming that the provider was 
// ever opened. If we issue I/O to the provider, then we must have it open, and we will not close
// our provider until all of that I/O is completed.
void 
IOUSBInterfaceUserClientV2::stop(IOService * provider)
{
    
    USBLog(7, "+%s[%p]::stop(%p), IO: %d", getName(), this, provider, fOutstandingIO);
	
    // If we have any kernelDataBuffer pointers, then release them now
    //
    if (fUserClientBufferInfoListHead != NULL)
    {
        ReleasePreparedDescriptors();
    }
	
	if (fWorkLoop && fGate)
		fWorkLoop->removeEventSource(fGate);
	
    super::stop(provider);
	
    USBLog(7, "-%s[%p]::stop(%p)", getName(), this, provider);
	
}

void 
IOUSBInterfaceUserClientV2::free()
{
    IOReturn ret;
	
    USBLog(7, "IOUSBInterfaceUserClientV2[%p]::free", this);
    
    // If we have any kernelDataBuffer pointers, then release them now
    //
    if (fUserClientBufferInfoListHead != NULL)
    {
        ReleasePreparedDescriptors();
    }
    
    if ( fFreeUSBLowLatencyCommandPool )
    {
        fFreeUSBLowLatencyCommandPool->release();
        fFreeUSBLowLatencyCommandPool = NULL;
    }
	
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
    if (fIOUSBInterfaceUserClientExpansionData)
    {
        IOFree(fIOUSBInterfaceUserClientExpansionData, sizeof(IOUSBInterfaceUserClientExpansionData));
        fIOUSBInterfaceUserClientExpansionData = NULL;
    }
	
    super::free();
}


bool 
IOUSBInterfaceUserClientV2::finalize( IOOptionBits options )
{
    bool ret;
	
    USBLog(7, "+%s[%p]::finalize(%08x)", getName(), this, (int)options);
    
    ret = super::finalize(options);
    
    USBLog(7, "-%s[%p]::finalize(%08x) - returning %s", getName(), this, (int)options, ret ? "true" : "false");
    return ret;
}


bool
IOUSBInterfaceUserClientV2::willTerminate( IOService * provider, IOOptionBits options )
{
    IOUSBPipe 		*pipe = NULL;
    IOReturn		ret;
    UInt32		ioPending = 0;
	
    // this method is intended to be used to stop any pending I/O and to make sure that
    // we have begun getting our callbacks in order. by the time we get here, the
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
	
    USBLog(3, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());
	
    //  We have seen cases where our fOwner is not valid at this point.  This is strange
    //  but we'll code defensively and only execute if our provider (fOwner) is still around
    //
    if ( fOwner )
    {
        IncrementOutstandingIO();
		
        ioPending = GetOutstandingIO();
        
        if ( (ioPending > 1) && fOwner )
        {
            int		i;
			
            USBLog(7, "%s[%p]::willTerminate - outstanding IO(%ld), aborting pipes", getName(), this, ioPending);
            for (i=1; i <= kUSBMaxPipes; i++)
            {
                pipe = fOwner->GetPipeObj(i-1);
				
                if(pipe)
                {
                    pipe->retain();
                    ret =  pipe->Abort();
                    pipe->release();
                }
            }
			
        }
        
        DecrementOutstandingIO();
    }
	
    return super::willTerminate(provider, options);
}


bool
IOUSBInterfaceUserClientV2::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
	USBLog(3, "%s[%p]::didTerminate isInactive = %d, outstandingIO = %d", getName(), this, isInactive(), fOutstandingIO);
	
    if ( fOwner && fOwner->isOpen(this))
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
IOUSBInterfaceUserClientV2::message( UInt32 type, IOService * provider,  void * argument )
{
    IOReturn	err = kIOReturnSuccess;
    
    switch ( type )
    {
	case kIOUSBMessagePortHasBeenSuspended:
		USBLog(6, "%s[%p]::message - received kIOUSBMessagePortHasBeenSuspended", getName(), this);
		break;
		
	case kIOUSBMessagePortHasBeenReset:
		USBLog(6, "%s[%p]::message - received kIOUSBMessagePortHasBeenReset", getName(), this);
		break;
		
	case kIOUSBMessagePortHasBeenResumed:
		USBLog(6, "%s[%p]::message - received kIOUSBMessagePortHasBeenResumed", getName(), this);
		break;
		
	case kIOMessageServiceIsTerminated: 
		USBLog(6, "%s[%p]::message - received kIOMessageServiceIsTerminated", getName(), this);
		break;
		
	default:
		break;
    }
    
    return err;
}


#pragma mark Debugging
void
IOUSBInterfaceUserClientV2::PrintExternalMethodArgs( IOExternalMethodArguments * arguments, UInt32 level )
{
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

#pragma mark Padding Methods

OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2,  0);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2,  1);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2,  2);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2,  3);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2,  4);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2,  5);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2,  6);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2,  7);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2,  8);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2,  9);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 10);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 11);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 12);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 13);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 14);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 15);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 16);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 17);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 18);
OSMetaClassDefineReservedUnused(IOUSBInterfaceUserClientV2, 19);


#pragma mark Low Latency Commands

IOUSBLowLatencyCommand *
IOUSBLowLatencyCommand::NewCommand()
{
    IOUSBLowLatencyCommand *me = new IOUSBLowLatencyCommand;
    
    return me;
	
}

void  			
IOUSBLowLatencyCommand::SetAsyncReference(OSAsyncReference64  ref)
{
    bcopy(ref, fAsyncRef, sizeof(OSAsyncReference64));
}

void  			
IOUSBLowLatencyCommand::SetAsyncCount(uint32_t  count)
{
    fAsyncReferenceCount = count;
}

// padding methods
//
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  0);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  1);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  2);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  3);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  4);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  5);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  6);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  7);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  8);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand,  9);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 10);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 11);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 12);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 13);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 14);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 15);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 16);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 17);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 18);
OSMetaClassDefineReservedUnused(IOUSBLowLatencyCommand, 19);
