/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOMemoryDescriptor.h>

#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/usb/IOUSBControllerV2.h>

#include "IOUSBInterfaceUserClient.h"

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
#define super IOUserClient
OSDefineMetaClassAndStructors(IOUSBInterfaceUserClient, IOUserClient)
OSDefineMetaClassAndStructors(IOUSBLowLatencyCommand, IOCommand)


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
enum {
        kMethodObjectThis = 0,
        kMethodObjectOwner
    };
    
const IOExternalMethod 
IOUSBInterfaceUserClient::sMethods[kNumUSBInterfaceMethods] = {
    { //    kUSBInterfaceUserClientOpen
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::open,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBInterfaceUserClientClose
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::close,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kUSBInterfaceUserClientGetDevice
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::GetDevice,
	kIOUCScalarIScalarO,
	0,
	1
    },
    { //    kUSBInterfaceUserClientSetAlternateInterface
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::SetAlternateInterface,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBInterfaceUserClientGetFrameNumber
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::GetFrameNumber,
	kIOUCScalarIStructO,
	0,
	0xffffffff
    },
    { //    kUSBInterfaceUserClientGetPipeProperties
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::GetPipeProperties,
	kIOUCScalarIScalarO,
	1,
	5
    },
    { //    kUSBInterfaceUserClientReadPipe
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::ReadPipe,
	kIOUCScalarIStructO,
	3,
	0xffffffff
    },
    { //    kUSBInterfaceUserClientReadPipeOOL
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::ReadPipeOOL,
	kIOUCStructIStructO,
	sizeof(IOUSBBulkPipeReq),
	sizeof(UInt32)
    },
    { //    kUSBInterfaceUserClientWritePipe
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::WritePipe,
	kIOUCScalarIStructI,
	3,
	0xffffffff
    },
    { //    kUSBInterfaceUserClientWritePipeOOL
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::WritePipeOOL,
	kIOUCStructIStructO,
	sizeof(IOUSBBulkPipeReq),
	0
    },
    { //    kUSBInterfaceUserClientGetPipeStatus
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::GetPipeStatus,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBInterfaceUserClientAbortPipe
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::AbortPipe,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBInterfaceUserClientResetPipe
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::ResetPipe,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBInterfaceUserClientSetPipeIdle
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::SetPipeIdle,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBInterfaceUserClientSetPipeActive
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::SetPipeActive,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBInterfaceUserClientClearPipeStall
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::ClearPipeStall,
	kIOUCScalarIScalarO,
	2,
	0
    },
    { //    kUSBInterfaceUserClientControlRequestOut
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::ControlRequestOut,
	kIOUCScalarIStructI,
	4,
	0xffffffff
    },
    { //    kUSBInterfaceUserClientControlRequestIn
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::ControlRequestIn,
	kIOUCScalarIStructO,
	4,
	0xffffffff
    },
    { //    kUSBInterfaceUserClientControlRequestOutOOL
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::ControlRequestOutOOL,
	kIOUCStructIStructO,
	sizeof(IOUSBDevReqOOLTO),
	0
    },
    { //    kUSBInterfaceUserClientControlRequestInOOL
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::ControlRequestInOOL,
	kIOUCStructIStructO,
	sizeof(IOUSBDevReqOOLTO),
	sizeof(UInt32)
    },
    { //    kUSBInterfaceuserClientSetPipePolicy
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::SetPipePolicy,
	kIOUCScalarIScalarO,
	3,
	0
    },
    { //    kUSBInterfaceuserClientGetBandwidthAvailable
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::GetBandwidthAvailable,
	kIOUCScalarIScalarO,
	0,
	1
    },
    { //    kUSBInterfaceuserClientGetEndpointProperties
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::GetEndpointProperties,
	kIOUCScalarIScalarO,
	3,
	3
    },
    { //    kUSBInterfaceUserClientLowLatencyPrepareBuffer
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::LowLatencyPrepareBuffer,
	kIOUCStructIStructO,
	sizeof(LowLatencyUserBufferInfo),
	0
    },
    { //    kUSBInterfaceUserClientLowLatencyReleaseBuffer
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBInterfaceUserClient::LowLatencyReleaseBuffer,
	kIOUCStructIStructO,
	sizeof(LowLatencyUserBufferInfo),
	0
    },
    { //    kUSBInterfaceUserClientGetMicroFrameNumber
        (IOService*)kMethodObjectThis,
        (IOMethod) &IOUSBInterfaceUserClient::GetMicroFrameNumber,
        kIOUCScalarIStructO,
        0,
        0xffffffff
    },
    { //    kUSBInterfaceUserClientGetFrameListTime
        (IOService*)kMethodObjectThis,
        (IOMethod) &IOUSBInterfaceUserClient::GetFrameListTime,
        kIOUCScalarIScalarO,
        0,
        1
    }
};



const IOExternalAsyncMethod 
IOUSBInterfaceUserClient::sAsyncMethods[kNumUSBInterfaceAsyncMethods] = {
    { //    kUSBDeviceUserClientSetAsyncPort
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBInterfaceUserClient::SetAsyncPort,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kUSBInterfaceUserClientControlAsyncRequestOut
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBInterfaceUserClient::ControlAsyncRequestOut,
	kIOUCStructIStructO,
	sizeof(IOUSBDevReqOOLTO),
	0
    },
    { //    kUSBInterfaceUserClientControlAsyncRequestIn
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBInterfaceUserClient::ControlAsyncRequestIn,
	kIOUCStructIStructO,
	sizeof(IOUSBDevReqOOLTO),
	0
    },
    { //    kUSBInterfaceUserClientAsyncReadPipe
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBInterfaceUserClient::AsyncReadPipe,
	kIOUCScalarIScalarO,
	5,
	0
    },
    { //    kUSBInterfaceUserClientAsyncWritePipe
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBInterfaceUserClient::AsyncWritePipe,
	kIOUCScalarIScalarO,
	5,
	0
    },
    { //    kUSBInterfaceUserClientReadIsochPipe
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBInterfaceUserClient::ReadIsochPipe,
	kIOUCStructIStructO,
	sizeof(IOUSBIsocStruct),
	0
    },
    { //    kUSBInterfaceUserClientWriteIsochPipe
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBInterfaceUserClient::WriteIsochPipe,
	kIOUCStructIStructO,
	sizeof(IOUSBIsocStruct),
	0
    },
    { //    kUSBInterfaceUserClientLowLatencyReadIsochPipe
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBInterfaceUserClient::LowLatencyReadIsochPipe,
	kIOUCStructIStructO,
	sizeof(IOUSBLowLatencyIsocStruct),
	0
    },
    { //    kUSBInterfaceUserClientLowLatencyWriteIsochPipe
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBInterfaceUserClient::LowLatencyWriteIsochPipe,
	kIOUCStructIStructO,
	sizeof(IOUSBLowLatencyIsocStruct),
	0
    }
};



void 
IOUSBInterfaceUserClient::SetExternalMethodVectors()
{
    fMethods = sMethods;
    fNumMethods = kNumUSBInterfaceMethods;
    fAsyncMethods = sAsyncMethods;
    fNumAsyncMethods = kNumUSBInterfaceAsyncMethods;
}



IOExternalMethod *
IOUSBInterfaceUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if (index < (UInt32)fNumMethods) 
    {
        if ((IOService*)kMethodObjectThis == fMethods[index].object)
            *target = this;
        else if ((IOService*)kMethodObjectOwner == fMethods[index].object)
            *target = fOwner;
        else
            return NULL;
	return (IOExternalMethod *) &fMethods[index];
    }
    else
	return NULL;
}



IOExternalAsyncMethod * 
IOUSBInterfaceUserClient::getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if (index < (UInt32)fNumAsyncMethods) 
    {
        if ((IOService*)kMethodObjectThis == fAsyncMethods[index].object)
            *target = this;
        else if ((IOService*)kMethodObjectOwner == fAsyncMethods[index].object)
            *target = fOwner;
        else
            return NULL;
	return (IOExternalAsyncMethod *) &fAsyncMethods[index];
    }
    else
	return 0;
}



bool
IOUSBInterfaceUserClient::initWithTask(task_t owningTask, void *security_id , UInt32 type )
{
    if (!super::initWithTask(owningTask, security_id , type))
        return false;

    if (!owningTask)
	return false;

    fTask = owningTask;
    fDead = false;
    SetExternalMethodVectors();

    return true;
}



bool 
IOUSBInterfaceUserClient::start( IOService * provider )
{
    IOWorkLoop	*			workLoop = NULL;
    IOCommandGate *			commandGate = NULL;

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

    // Now that we have succesfully added our gate to the workloop, set our member variables
    //
    fGate = commandGate;
    fWorkLoop = workLoop;

    DecrementOutstandingIO();
    
    USBLog(7, "-%s[%p]::start(%p)", getName(), this, provider);
    
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


// This method is NOT the IOService open(IOService*) method.
IOReturn 
IOUSBInterfaceUserClient::open(bool seize)
{
    IOOptionBits	options = seize ? (IOOptionBits)kIOServiceSeize : 0;

    USBLog(7, "+%s[%p]::open(%p)", getName(), this);
    
    if (!fOwner)
        return kIOReturnNotAttached;
        
    if (!fOwner->open(this, options))
    {
        USBLog(3, "+%s[%p]::open(%p) failed", getName(), this);
        return kIOReturnExclusiveAccess;
    }
    
    fNeedToClose = false;
    
    return kIOReturnSuccess;
}



// This is NOT the normal IOService::close(IOService*) method.
// We are treating this is a proxy that we should close our parent, but
// maintain the connection with the task
IOReturn
IOUSBInterfaceUserClient::close()
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
    
		USBLog(5, "%s[%p]::close - outstanding IO, aborting pipes", getName(), this);
		for (i=1; i <= kUSBMaxPipes; i++)
		    AbortPipe(i);
	    }
	}
	else
	    ret = kIOReturnNotOpen;
    }
    else
	ret = kIOReturnNotAttached;
 
    DecrementOutstandingIO();
    USBLog(7, "-%s[%p]::close - returning %x", getName(), this, ret);
    return ret;
}



//
// clientClose - my client on the user side has released the mach port, so I will no longer
// be talking to him
//
IOReturn  
IOUSBInterfaceUserClient::clientClose( void )
{
    USBLog(7, "+%s[%p]::clientClose(%p)", getName(), this, fUserClientBufferInfoListHead);

    // Sleep for 1 ms to allow other threads that are pending to run
    //
    IOSleep(1);
    
    // If we have any kernelDataBuffer pointers, then release them now
    //
    if (fUserClientBufferInfoListHead != NULL)
    {
            ReleasePreparedDescriptors();
    }

    fTask = NULL;

    terminate();

    USBLog(7, "-%s[%p]::clientClose(%p)", getName(), this, fUserClientBufferInfoListHead);

    return kIOReturnSuccess;			// DONT call super::clientClose, which just returns notSupported
}


IOReturn 
IOUSBInterfaceUserClient::clientDied( void )
{
    IOReturn ret;

    USBLog(5, "+%s[%p]::clientDied()", getName(), this);

    fDead = true;				// don't send any mach messages in this case
    ret = super::clientDied();

    USBLog(5, "-%s[%p]::clientDied()", getName(), this);

    return ret;
}



void
IOUSBInterfaceUserClient::ReqComplete(void *obj, void *param, IOReturn res, UInt32 remaining)
{
    void *	args[1];
    AsyncPB * pb = (AsyncPB *)param;
    IOUSBInterfaceUserClient *me = OSDynamicCast(IOUSBInterfaceUserClient, (OSObject*)obj);

    if (!me)
	return;
	
    USBLog(7, "%s[%p]::ReqComplete, req = %08x, remaining = %08x", me->getName(), me, (int)pb->fMax, (int)remaining);

    if(res == kIOReturnSuccess) 
    {
        args[0] = (void *)(pb->fMax - remaining);
    }
    else 
    {
        args[0] = 0;
    }
    if (pb->fMem)
    {
	pb->fMem->complete();
	pb->fMem->release();
        pb->fMem = NULL;
    }
    if (!me->fDead)
	sendAsyncResult(pb->fAsyncRef, res, args, 1);

    IOFree(pb, sizeof(*pb));
    me->DecrementOutstandingIO();
    me->release();
}


void
IOUSBInterfaceUserClient::IsoReqComplete(void *obj, void *param, IOReturn res, IOUSBIsocFrame *pFrames)
{
    void *	args[1];
    IsoAsyncPB * pb = (IsoAsyncPB *)param;
    IOUSBInterfaceUserClient *me = OSDynamicCast(IOUSBInterfaceUserClient, (OSObject*)obj);

    if (!me)
	return;
	
    args[0] = pb->frameBase;
    pb->countMem->writeBytes(0, pb->frames, pb->frameLen);
    pb->dataMem->complete();
    pb->dataMem->release();
    pb->dataMem = NULL;
    
    pb->countMem->complete();
    pb->countMem->release();
    pb->countMem = NULL;
    
    if (!me->fDead)
	sendAsyncResult(pb->fAsyncRef, res, args, 1);

    IOFree(pb, sizeof(*pb)+pb->frameLen);
    me->DecrementOutstandingIO();
    me->release(); 
}




void
IOUSBInterfaceUserClient::LowLatencyIsoReqComplete(void *obj, void *param, IOReturn res, IOUSBLowLatencyIsocFrame *pFrames)
{
    void *			args[1];
    IOUSBLowLatencyCommand *	command = (IOUSBLowLatencyCommand *) param;
    IOMemoryDescriptor *	dataBufferDescriptor;
    OSAsyncReference		asyncRef;
    
    IOUSBInterfaceUserClient *	me = OSDynamicCast(IOUSBInterfaceUserClient, (OSObject*)obj);

    if (!me)
	return;

    args[0] = command->GetFrameBase(); 
    
    command->GetAsyncReference(&asyncRef);
    
    if (!me->fDead)
	sendAsyncResult( asyncRef, res, args, 1);

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


IOReturn 
IOUSBInterfaceUserClient::SetAlternateInterface(UInt8 altSetting)
{
    IOReturn	ret;
    
    USBLog(7, "+%s[%p]::SetAlternateInterface(%d)", getName(), this, altSetting);
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
	ret = fOwner->SetAlternateInterface(this, altSetting);
    else
	ret = kIOReturnNotAttached;
	
    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::SetAlternateInterface - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn 
IOUSBInterfaceUserClient::GetDevice(io_service_t *device)
{
    IOReturn		ret;
    
    IncrementOutstandingIO();

    if (fOwner && !isInactive())
    {
	// Although not documented, Simon says that exportObjectToClient consumes a reference,
	// so we have to put an extra retain on the device.  The user client side of the USB stack (USBLib)
	// always calls this routine in order to cache the USB device.  Radar #2586534
	fOwner->GetDevice()->retain();
	ret = exportObjectToClient(fTask, fOwner->GetDevice(), device);
    }
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::GetDevice - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn 
IOUSBInterfaceUserClient::GetFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size)
{
    IOReturn		ret = kIOReturnSuccess;

    USBLog(7, "+%s[%p]::GetFrameNumber", getName(), this);

    if(*size != sizeof(IOUSBGetFrameStruct))
	return kIOReturnBadArgument;
    
    if (fOwner && !isInactive())
    {
	clock_get_uptime(&data->timeStamp);
	data->frame = fOwner->GetDevice()->GetBus()->GetFrameNumber();
    }
    else
        ret = kIOReturnNotAttached;

    if (ret)
	USBLog(3, "%s[%p]::GetFrameNumber - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn
IOUSBInterfaceUserClient::GetMicroFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size)
{
    // This method only available for v2 controllers
    //
    IOUSBControllerV2	*v2 = OSDynamicCast(IOUSBControllerV2, fOwner->GetDevice()->GetBus());
    IOReturn		ret = kIOReturnSuccess;

    if (!v2)
    {
        USBLog(3, "%s[%p]::GetMicroFrameNumber - Not a USB 2.0 controller!  Returning 0x%x", getName(), this, kIOReturnNotAttached);
        return kIOReturnNotAttached;
    }

    if (*size != sizeof(IOUSBGetFrameStruct))
    {
        USBLog(3, "%s[%p]::GetMicroFrameNumber - *size is not sizeof(IOUSBGetFrameStruct): %ld, %ld", getName(), this, *size, sizeof(IOUSBGetFrameStruct) );
        return kIOReturnBadArgument;
    }

    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
        clock_get_uptime(&data->timeStamp);
        data->frame = v2->GetMicroFrameNumber();
    }
    else
    {
        USBLog(3, "%s[%p]::GetMicroFrameNumber - no fOwner(%p) or isInactive", getName(), this, fOwner);
        ret = kIOReturnNotAttached;
    }

    DecrementOutstandingIO();
    if (ret)
        USBLog(3, "%s[%p]::GetFrameNumber - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn
IOUSBInterfaceUserClient::GetBandwidthAvailable(UInt32 *bandwidth)
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
        USBLog(3, "%s[%p]::GetBandwidthAvailable - returning err %x", getName(), this, ret);

    return ret;
}



IOReturn
IOUSBInterfaceUserClient::GetFrameListTime(UInt32 *microsecondsInFrame)
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
        USBLog(3, "%s[%p]::GetFrameListTime - returning err %x", getName(), this, ret);

    return ret;
}


IOReturn 
IOUSBInterfaceUserClient::GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, UInt32 *transferType, UInt32 *maxPacketSize, UInt32 *interval)
{
    IOReturn		ret = kIOReturnSuccess;
    UInt8		myTT;
    UInt16		myMPS;
    UInt8		myIV;

    USBLog(7, "+%s[%p]::GetEndpointProperties", getName(), this);

    if (fOwner && !isInactive())
	ret = fOwner->GetEndpointProperties(alternateSetting, endpointNumber, direction, &myTT, &myMPS, &myIV);
    else
        ret = kIOReturnNotAttached;

    if (ret)
	USBLog(3, "%s[%p]::GetEndpointProperties - returning err %x", getName(), this, ret);
    else
    {
        *transferType = myTT;
        *maxPacketSize = myMPS;
        *interval = myIV;
	USBLog(3, "%s[%p]::GetEndpointProperties - tt=%d, mps=%d, int=%d", getName(), this, *transferType, *maxPacketSize, *interval);
    }
    return ret;
}



IOUSBPipe*
IOUSBInterfaceUserClient::GetPipeObj(UInt8 pipeNo)
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



// because of the way User Client works, these params need to be treated as pointers to 32 bit ints instead
// of pointers to smaller values, because on the other side they are automatically dereferenced as such
IOReturn
IOUSBInterfaceUserClient::GetPipeProperties(UInt8 pipeRef, UInt32 *direction, UInt32 *number, UInt32 *transferType, UInt32 *maxPacketSize, UInt32 *interval)
{
    IOUSBPipe 			*pipeObj;
    IOReturn			ret = kIOReturnSuccess;

    USBLog(7, "+%s[%p]::GetPipeProperties", getName(), this);

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

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::GetPipeProperties - returning err %x", getName(), this, ret);
    return ret;
}


IOReturn
IOUSBInterfaceUserClient::ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;
    IOUSBPipe 			*pipeObj;

    USBLog(7, "+%s(%p)::ReadPipe(%d, %d, %d, %p, %p)", getName(), this, pipeRef, noDataTimeout, completionTimeout, buf, size);
    
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(pipeRef);
	if(pipeObj)
	{
	    mem = IOMemoryDescriptor::withAddress(buf, *size, kIODirectionIn);
	    if(mem)
	    { 
                *size = 0;
		ret = pipeObj->Read(mem, noDataTimeout, completionTimeout, 0, size);
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

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::ReadPipe - returning err %x", getName(), this, ret);

    return ret;
}



/*
 * Out of line version of read pipe - the buffer isn't mapped (yet) into the kernel task
 */
IOReturn
IOUSBInterfaceUserClient::ReadPipeOOL(IOUSBBulkPipeReq *reqIn, UInt32 *sizeOut, IOByteCount inCount, IOByteCount *outCount)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;
    UInt8			pipeRef = reqIn->pipeRef;
    IOUSBPipe 			*pipeObj;

    USBLog(7, "+%s(%p)::ReadPipeOOL(%p, %p, %d, %d)", getName(), this, reqIn, sizeOut, inCount, *outCount);
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete
    
    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(pipeRef);
	if(pipeObj)
	{
            *sizeOut = 0;
	    mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->buf, reqIn->size, kIODirectionIn, fTask);
	    if(mem)
	    {
		ret = mem->prepare();
		if(ret == kIOReturnSuccess)
		    ret = pipeObj->Read(mem, reqIn->noDataTimeout, reqIn->completionTimeout, 0, sizeOut);
		mem->complete();
		mem->release();
                mem = NULL;
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

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::ReadPipeOOL - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn
IOUSBInterfaceUserClient::WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 size)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;
    IOUSBPipe 			*pipeObj;

    USBLog(7, "+%s(%p)::WritePipe(%d, %d, %d, %p, %p)", getName(), this, pipeRef, noDataTimeout, completionTimeout, buf, size);
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete

    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(pipeRef);
	if(pipeObj)
	{
	    mem = IOMemoryDescriptor::withAddress(buf, size, kIODirectionOut);
	    if(mem) 
	    {
		ret = pipeObj->Write(mem, noDataTimeout, completionTimeout);
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

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::WritePipe - returning err %x", getName(), this, ret);
    return ret;
}



/*
 * Out of line version of write pipe - the buffer isn't mapped (yet) into the kernel task
 */
IOReturn
IOUSBInterfaceUserClient::WritePipeOOL(IOUSBBulkPipeReq *req, IOByteCount inCount)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;
    IOUSBPipe 			*pipeObj;

    USBLog(7, "+%s(%p)::WritePipeOOL(%d)", getName(), this, inCount);
    
    IncrementOutstandingIO();				// do this to "hold" ourselves until we complete

    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(req->pipeRef);
	if(pipeObj)
	{
	    mem = IOMemoryDescriptor::withAddress((vm_address_t)req->buf, req->size, kIODirectionOut, fTask);
	    if(mem) 
	    {
		ret = mem->prepare();
		if(ret == kIOReturnSuccess)
		    ret = pipeObj->Write(mem, req->noDataTimeout, req->completionTimeout);
		mem->complete();
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

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::WritePipeOOL - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn 
IOUSBInterfaceUserClient::GetPipeStatus(UInt8 pipeRef)
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



IOReturn 
IOUSBInterfaceUserClient::AbortPipe(UInt8 pipeRef)
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

    DecrementOutstandingIO();
    if (ret)
    {
        if ( ret == kIOUSBUnknownPipeErr )
            USBLog(5, "%s[%p]::AbortPipe - returning err %x", getName(), this, ret);
        else
            USBLog(3, "%s[%p]::AbortPipe - returning err %x", getName(), this, ret);
    }
    
    return ret;
}



IOReturn 
IOUSBInterfaceUserClient::ResetPipe(UInt8 pipeRef)
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

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::ResetPipe - returning err %x", getName(), this, ret);
    
    return ret;
}



IOReturn 
IOUSBInterfaceUserClient::SetPipeIdle(UInt8 pipeRef)
{
    if (!fOwner || isInactive())
        return kIOReturnNotAttached;
        
    return(0);
}



IOReturn 
IOUSBInterfaceUserClient::SetPipeActive(UInt8 pipeRef)
{
    if (!fOwner || isInactive())
        return kIOReturnNotAttached;
        
    return(0);
}



IOReturn 
IOUSBInterfaceUserClient::ClearPipeStall(UInt8 pipeRef, bool bothEnds)
{
    IOUSBPipe 		*pipeObj;
    IOReturn		ret;

    USBLog(7, "+%s[%p]::ClearPipeStall", getName(), this);

    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(pipeRef);
	if(pipeObj)
	{
	    USBLog(2, "%s[%p]::ClearPipeStall = bothEnds = %d", getName(), this, bothEnds);
	    ret = pipeObj->ClearPipeStall(bothEnds);
	    pipeObj->release();
	}
	else
	    ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::ClearPipeStall - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn 
IOUSBInterfaceUserClient::SetPipePolicy(UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval)
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
	    USBLog(2, "%s[%p]::SetPipePolicy(%d, %d)", getName(), this, maxPacketSize, maxInterval);
	    ret = pipeObj->SetPipePolicy(maxPacketSize, maxInterval);
	    pipeObj->release();
	}
	else
	    ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::SetPipePolicy - returning err %x", getName(), this, ret);
    return ret;
}



/*
 * There's a limit of max 6 arguments to user client methods, so the type, recipient and request
 * are packed into one 16 bit integer.
 */
IOReturn
IOUSBInterfaceUserClient::ControlRequestOut(UInt32 param1, UInt32 param2, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 size)
{
    IOReturn 		ret;
    IOUSBDevRequest	req;
    UInt8		pipeRef = (param1 >> 16) & 0xFF;
    UInt8		bmRequestType = (param1 >> 8) & 0xFF;
    UInt8		bRequest = param1 & 0xFF;
    UInt16		wValue = (param2 >> 16) & 0xFFFF;
    UInt16		wIndex = param2 & 0xFFFF;
    IOUSBPipe		*pipeObj;
    
    USBLog(7, "+%s[%p]::ControlRequestOut", getName(), this);

    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(pipeRef);
	if (pipeObj)
	{
	    req.bmRequestType = bmRequestType;
	    req.bRequest = bRequest;
	    req.wValue = wValue;
	    req.wIndex = wIndex;
	    req.wLength = size;
	    req.pData = buf;
	    ret = pipeObj->ControlRequest(&req, noDataTimeout, completionTimeout);
	
	    if(kIOReturnSuccess != ret) 
		USBLog(3, "%s[%p]::ControlRequestOut err:0x%x", getName(), this, ret);
	    pipeObj->release();
	}
	else
	    ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
	
    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::ControlRequestOut - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn
IOUSBInterfaceUserClient::ControlRequestIn(UInt32 param1, UInt32 param2, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size)
{
    IOReturn 		ret;
    IOUSBDevRequest	req;
    UInt8		pipeRef = (param1 >> 16) & 0xFF;
    UInt8		bmRequestType = (param1 >> 8) & 0xFF;
    UInt8		bRequest = param1 & 0xFF;
    UInt16		wValue = (param2 >> 16) & 0xFFFF;
    UInt16		wIndex = param2 & 0xFFFF;
    IOUSBPipe		*pipeObj;
    
    USBLog(7, "+%s[%p]::ControlRequestIn", getName(), this);

    IncrementOutstandingIO();

    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(pipeRef);
	if (pipeObj)
	{
	    req.bmRequestType = bmRequestType;
	    req.bRequest = bRequest;
	    req.wValue = wValue;
	    req.wIndex = wIndex;
	    req.wLength = *size;
	    req.pData = buf;
	    ret = pipeObj->ControlRequest(&req, noDataTimeout, completionTimeout);
	
	    if(ret == kIOReturnSuccess) 
		*size = req.wLenDone;
	    else 
	    {
		USBLog(3, "%s[%p]::ControlRequestIn err:0x%x", getName(), this, ret);
		*size = 0;
	    }
	    pipeObj->release();
	}
	else
	    ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::ControlRequestIn - returning err %x", getName(), this, ret);
    return ret;
}



//
// ControlRequestOutOOL: reqIn->wLength > 4K
//
IOReturn
IOUSBInterfaceUserClient::ControlRequestOutOOL(IOUSBDevReqOOLTO *reqIn, IOByteCount inCount)
{
    IOReturn 			ret;
    IOUSBDevRequestDesc		req;
    IOMemoryDescriptor *	mem;
    IOUSBPipe *			pipeObj;

    USBLog(7, "+%s[%p]::ControlRequestOutOOL", getName(), this);

    if(inCount != sizeof(IOUSBDevReqOOLTO))
        return kIOReturnBadArgument;

    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(reqIn->pipeRef);
	if(pipeObj)
	{
	    mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->pData, reqIn->wLength, kIODirectionOut, fTask);
	    if(mem) 
	    {
		req.bmRequestType = reqIn->bmRequestType;
		req.bRequest = reqIn->bRequest;
		req.wValue = reqIn->wValue;
		req.wIndex = reqIn->wIndex;
		req.wLength = reqIn->wLength;
		req.pData = mem;
	    
		ret = mem->prepare();
		if(ret == kIOReturnSuccess)
		    ret = pipeObj->ControlRequest(&req, reqIn->noDataTimeout, reqIn->completionTimeout);
		mem->complete();
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

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::ControlRequestOutOOL - returning err %x", getName(), this, ret);
    return ret;
}



//
// ControlRequestInOOL: reqIn->wLength > 4K
//
IOReturn
IOUSBInterfaceUserClient::ControlRequestInOOL(IOUSBDevReqOOLTO *reqIn, UInt32 *sizeOut, IOByteCount inCount, IOByteCount *outCount)
{
    IOReturn 			ret;
    IOUSBDevRequestDesc		req;
    IOMemoryDescriptor *	mem;
    IOUSBPipe *			pipeObj;

    USBLog(7, "+%s[%p]::ControlRequestInOOL", getName(), this);

    if((inCount != sizeof(IOUSBDevReqOOLTO)) || (*outCount != sizeof(UInt32)))
        return kIOReturnBadArgument;

    IncrementOutstandingIO();

    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(reqIn->pipeRef);
	if(pipeObj)
	{
	    mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->pData, reqIn->wLength, kIODirectionIn, fTask);
	    if(mem) 
	    {
		req.bmRequestType = reqIn->bmRequestType;
		req.bRequest = reqIn->bRequest;
		req.wValue = reqIn->wValue;
		req.wIndex = reqIn->wIndex;
		req.wLength = reqIn->wLength;
		req.pData = mem;
	    
		ret = mem->prepare();
		if(ret == kIOReturnSuccess)
		    ret = pipeObj->ControlRequest(&req, reqIn->noDataTimeout, reqIn->completionTimeout);
	
		mem->complete();
		mem->release();
                mem = NULL;
		if(ret == kIOReturnSuccess) 
		{
		    *sizeOut = req.wLenDone;
		}
		else 
		{
		    USBLog(3, "%s[%p]::ControlRequestInOOL err:0x%x", getName(), this, ret);
		    *sizeOut = 0;
		}
		*outCount = sizeof(UInt32);
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

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::ControlRequestInOOL - returning err %x", getName(), this, ret);
    return ret;
}


IOReturn 
IOUSBInterfaceUserClient::LowLatencyPrepareBuffer(LowLatencyUserBufferInfo *bufferData)
{
    IOReturn				ret 			= kIOReturnSuccess;
    IOMemoryDescriptor *		aDescriptor		= NULL;
    LowLatencyUserClientBufferInfo *	kernelDataBuffer	= NULL;
    IOMemoryMap *			frameListMap		= NULL;
    IODirection				direction;
    
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
         USBLog(3, "%s[%p]::LowLatencyPrepareBuffer cookie: %d, buffer: %p, size: %d, type %d, isPrepared: %d, next: %p", getName(), this,
           bufferData->cookie,
            bufferData->bufferAddress,
            bufferData->bufferSize,
            bufferData->bufferType,
            bufferData->isPrepared,
            bufferData->nextBuffer);
            
        // Allocate a buffer and zero it
        //
        kernelDataBuffer = ( LowLatencyUserClientBufferInfo *) IOMalloc( sizeof(LowLatencyUserClientBufferInfo) );
        if (kernelDataBuffer == NULL )
        {
            USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not malloc buffer info (size = %d)!", getName(), this, sizeof(LowLatencyUserClientBufferInfo) );
            return kIOReturnNoMemory;
        }
        
        bzero(kernelDataBuffer, sizeof(LowLatencyUserClientBufferInfo));
        
        // Set the known fields
        //
        kernelDataBuffer->cookie = bufferData->cookie;
        kernelDataBuffer->bufferType = bufferData->bufferType;
        
        // Create a memory descriptor for our data buffer and prepare it (pages it in if necesary and prepares it)
        //
        if ( (bufferData->bufferType == kUSBLowLatencyWriteBuffer) || ( bufferData->bufferType == kUSBLowLatencyReadBuffer) )
        {
            // We have a data buffer, so create a IOMD and prepare it
            //
            direction = ( bufferData->bufferType == kUSBLowLatencyWriteBuffer ? kIODirectionOut : kIODirectionIn );
            aDescriptor = IOMemoryDescriptor::withAddress((vm_address_t)bufferData->bufferAddress, bufferData->bufferSize, direction, fTask);
            if(!aDescriptor) 
            {
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not create a data buffer memory descriptor (addr: %p, size %d)!", getName(), this, bufferData->bufferAddress, bufferData->bufferSize );
                ret = kIOReturnNoMemory;
                goto ErrorExit;
            }

            ret = aDescriptor->prepare();
            if (ret != kIOReturnSuccess)
            {
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not prepare the data buffer memory descriptor!", getName(), this );
                goto ErrorExit;
            }

            
            // OK, now save this in our user client structure
            // 
            kernelDataBuffer->bufferAddress = bufferData->bufferAddress;
            kernelDataBuffer->bufferSize = bufferData->bufferSize;
            kernelDataBuffer->bufferDescriptor = aDescriptor;
            
            USBLog(3, "%s[%p]::LowLatencyPrepareBuffer  finished preparing data buffer: %p, size %d, desc: %p, cookie: %ld", getName(), this,
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
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not create a frame list memory descriptor (addr: %p, size %d)!", getName(), this, bufferData->bufferAddress, bufferData->bufferSize );
                ret = kIOReturnNoMemory;
                goto ErrorExit;
            }
            
            ret = aDescriptor->prepare();
            if (ret != kIOReturnSuccess)
            {
                USBLog(1,"%s[%p]::LowLatencyPrepareBuffer  Could not prepare the frame list memory descriptor!", getName(), this );
                ret = kIOReturnNoMemory;
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

            USBLog(3, "%s[%p]::LowLatencyPrepareBuffer  finished preparing frame list buffer: %p, size %d, desc: %p, map %p, kernel address: %p, cookie: %ld", getName(), this,
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
        USBLog(3, "%s[%p]::LowLatencyPrepareBuffer - returning err %x", getName(), this, ret);

    DecrementOutstandingIO();
    
    return ret;
}

IOReturn 
IOUSBInterfaceUserClient::LowLatencyReleaseBuffer(LowLatencyUserBufferInfo *dataBuffer)
{
    LowLatencyUserClientBufferInfo *	kernelDataBuffer	= NULL;
    IOReturn				ret 			= kIOReturnSuccess;
    bool				found 			= false;
    
    IncrementOutstandingIO();
    
    USBLog(3, "+%s[%p]::LowLatencyReleaseBuffer for cookie: %ld", getName(), this, dataBuffer->cookie);

    if (fOwner && !isInactive())
    {
        // We need to find the LowLatencyUserBufferInfo structure that contains
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
            USBLog(3, "+%s[%p]::LowLatencyReleaseBuffer cookie: %ld, could not remove buffer (%p) from list", getName(), this, dataBuffer->cookie);
            ret = kIOReturnBadArgument;
            goto ErrorExit;
        }

        // Now, need to complete/release/free the objects we allocated in our prepare
        //
        if ( kernelDataBuffer->frameListMap )
        {
            kernelDataBuffer->frameListMap->release();
            kernelDataBuffer->frameListMap = NULL;
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
                
        
        // Finally, deallocate our kernelDataBuffer
        //
        IOFree(kernelDataBuffer, sizeof(LowLatencyUserClientBufferInfo));

    }
    else
        ret = kIOReturnNotAttached;

ErrorExit:

    if (ret)
        USBLog(3, "%s[%p]::LowLatencyReleaseBuffer - returning err %x", getName(), this, ret);

    DecrementOutstandingIO();
    return ret;
}

void
IOUSBInterfaceUserClient::AddDataBufferToList( LowLatencyUserClientBufferInfo * insertBuffer )
{
    LowLatencyUserClientBufferInfo *	buffer;
    
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

LowLatencyUserClientBufferInfo *	
IOUSBInterfaceUserClient::FindBufferCookieInList( UInt32 cookie)
{
    LowLatencyUserClientBufferInfo *	buffer;
    bool				foundIt = true;
    
    // Traverse the list looking for this buffer
    //
    if ( fUserClientBufferInfoListHead == NULL )
    {
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
        return false;
}

 bool			
IOUSBInterfaceUserClient::RemoveDataBufferFromList( LowLatencyUserClientBufferInfo *removeBuffer)
{
    LowLatencyUserClientBufferInfo *	buffer;
    LowLatencyUserClientBufferInfo *	previousBuffer;
    
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


// ASYNC METHODS

IOReturn 
IOUSBInterfaceUserClient::SetAsyncPort(OSAsyncReference asyncRef)
{
    if (!fOwner || isInactive())
        return kIOReturnNotAttached;
        
    fWakePort = (mach_port_t) asyncRef[0];
    return kIOReturnSuccess;
}


IOReturn
IOUSBInterfaceUserClient::ControlAsyncRequestOut(OSAsyncReference asyncRef, IOUSBDevReqOOLTO *reqIn, IOByteCount inCount)
{
    IOReturn 			ret = kIOReturnSuccess;
    IOUSBPipe *			pipeObj;

    IOUSBCompletion		tap;
    AsyncPB * 			pb = NULL;
    IOMemoryDescriptor *	mem = NULL;

    USBLog(7, "+%s[%p]::ControlAsyncRequestOut", getName(), this);

    if(inCount != sizeof(IOUSBDevReqOOLTO))
        return kIOReturnBadArgument;

    retain();					// to make sure ReqComplete is still around
    IncrementOutstandingIO();			// to make sure ReqComplete is still around

    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(reqIn->pipeRef);
	if(pipeObj)
	{
	    if (reqIn->wLength)
	    {
		if (reqIn->pData)
		{
		    mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->pData, reqIn->wLength, kIODirectionOut, fTask);
		    if (!mem)
			ret = kIOReturnNoMemory;
		}
		else
		    ret = kIOReturnBadArgument;
	    }
	    if (ret == kIOReturnSuccess)
	    {
		pb = (AsyncPB *)IOMalloc(sizeof(AsyncPB));
		if(!pb) 
		    ret = kIOReturnNoMemory;
	    }
	    
	    if (mem && (ret == kIOReturnSuccess))
		ret = mem->prepare();
	
	    if (ret == kIOReturnSuccess)
	    {
		pb->req.bmRequestType = reqIn->bmRequestType;
		pb->req.bRequest = reqIn->bRequest;
		pb->req.wValue = reqIn->wValue;
		pb->req.wIndex = reqIn->wIndex;
		pb->req.wLength = reqIn->wLength;
		pb->req.pData = mem;
	    
		bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
		pb->fMax = reqIn->wLength;
		pb->fMem = mem;
		tap.target = this;
		tap.action = &ReqComplete;	// Want same number of reply args as for ControlReqIn
		tap.parameter = pb;
		ret = pipeObj->ControlRequest(&pb->req, reqIn->noDataTimeout, reqIn->completionTimeout, &tap);
	    }
	    pipeObj->release();
	}
	else
	    ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
    
    if(ret != kIOReturnSuccess) 
    {
	USBLog(3, "%s[%p]::ControlAsyncRequestOut err 0x%x", getName(), this, ret);
	if(mem) 
	{
	    mem->complete();
	    mem->release();
            mem = NULL;
	}
	if(pb)
	    IOFree(pb, sizeof(*pb));
	
        // only decrement if we are not going to be successful. otherwise, this will be done in the completion
	DecrementOutstandingIO();
        release();
    }
    return ret;
}



IOReturn
IOUSBInterfaceUserClient::ControlAsyncRequestIn(OSAsyncReference asyncRef, IOUSBDevReqOOLTO *reqIn, IOByteCount inCount)
{
    IOReturn 			ret = kIOReturnSuccess;
    IOUSBPipe *			pipeObj;

    IOUSBCompletion		tap;
    AsyncPB * 			pb = NULL;
    IOMemoryDescriptor *	mem = NULL;

    USBLog(7, "+%s[%p]::ControlAsyncRequestIn", getName(), this);

    if(inCount != sizeof(IOUSBDevReqOOLTO))
        return kIOReturnBadArgument;

    retain();
    IncrementOutstandingIO();		// to make sure ReqComplete is still around

    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(reqIn->pipeRef);
	if(pipeObj)
	{
	    if (reqIn->wLength)
	    {
		if (reqIn->pData)
		{
		    mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->pData, reqIn->wLength, kIODirectionIn, fTask);
		    if (!mem)
			ret = kIOReturnNoMemory;
		}
		else
		    ret = kIOReturnBadArgument;
	    }
	    if (ret == kIOReturnSuccess)
	    {
		pb = (AsyncPB *)IOMalloc(sizeof(AsyncPB));
		if(!pb) 
		    ret = kIOReturnNoMemory;
	    }
	    if (mem && (ret == kIOReturnSuccess))
		ret = mem->prepare();
	
	    if (ret == kIOReturnSuccess)
	    {
		pb->req.bmRequestType = reqIn->bmRequestType;
		pb->req.bRequest = reqIn->bRequest;
		pb->req.wValue = reqIn->wValue;
		pb->req.wIndex = reqIn->wIndex;
		pb->req.wLength = reqIn->wLength;
		pb->req.pData = mem;
	    
		bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
		pb->fMax = reqIn->wLength;
		pb->fMem = mem;
		tap.target = this;
		tap.action = &ReqComplete;
		tap.parameter = pb;
		ret = pipeObj->ControlRequest(&pb->req, reqIn->noDataTimeout, reqIn->completionTimeout, &tap);
	    }
	    pipeObj->release();
	}
	else
	    ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
        
    if(ret != kIOReturnSuccess) 
    {
	USBLog(3, "%s[%p]::ControlAsyncRequestIn err 0x%x", getName(), this, ret);
	if(mem) 
	{
	    mem->complete();
	    mem->release();
            mem = NULL;
	}
	if(pb)
	    IOFree(pb, sizeof(*pb));
	
        // only decrement if we are not going to be successful. otherwise, this will be done in the completion
	DecrementOutstandingIO();
        release();
    }

    return ret;
}



IOReturn 
IOUSBInterfaceUserClient::DoIsochPipeAsync(OSAsyncReference asyncRef, IOUSBIsocStruct *stuff, IODirection direction)
{
    IOReturn 			ret;
    IOUSBPipe *			pipeObj;
    IOUSBIsocCompletion		tap;
    IOMemoryDescriptor *	dataMem = NULL;
    IOMemoryDescriptor *	countMem = NULL;
    int				frameLen = 0;	// In bytes
    IsoAsyncPB * 		pb = NULL;
    bool			countMemPrepared = false;
    bool			dataMemPrepared = false;

    USBLog(7, "+%s[%p]::DoIsochPipeAsync", getName(), this);
    retain();
    IncrementOutstandingIO();		// to make sure IsoReqComplete is still around
    
    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(stuff->fPipe);
	if(pipeObj)
	{
	    frameLen = stuff->fNumFrames * sizeof(IOUSBIsocFrame);
	    do {
		dataMem = IOMemoryDescriptor::withAddress((vm_address_t)stuff->fBuffer, stuff->fBufSize, direction, fTask);
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
                
		countMem = IOMemoryDescriptor::withAddress((vm_address_t)stuff->fFrameCounts, frameLen, kIODirectionOutIn, fTask);
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

                // Copy in requested transfers, we'll copy out result in completion routine
		pb = (IsoAsyncPB *)IOMalloc(sizeof(IsoAsyncPB) + frameLen);
		if(!pb) 
		{
		    ret = kIOReturnNoMemory;
		    break;
		}
                
                bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
		pb->frameLen = frameLen;
		pb->frameBase = stuff->fFrameCounts;
		pb->dataMem = dataMem;
		pb->countMem = countMem;
		countMem->readBytes(0, pb->frames, frameLen);
		tap.target = this;
		tap.action = &IsoReqComplete;
		tap.parameter = pb;
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
        
	if(pb)
	    IOFree(pb, sizeof(*pb) + frameLen);
	DecrementOutstandingIO();
        release();
    }

    return ret;
}



IOReturn 
IOUSBInterfaceUserClient::DoLowLatencyIsochPipeAsync(OSAsyncReference asyncRef, IOUSBLowLatencyIsocStruct *isocInfo, IODirection direction)
{
    IOReturn 				ret;
    IOUSBPipe *				pipeObj;
    IOUSBLowLatencyIsocCompletion	tap;
    IOMemoryDescriptor *		aDescriptor		= NULL;
    IOUSBLowLatencyIsocFrame *		pFrameList 		= NULL;
    IOUSBLowLatencyCommand *		command 		= NULL;
    LowLatencyUserClientBufferInfo *	dataBuffer		= NULL;
    LowLatencyUserClientBufferInfo *	frameListDataBuffer	= NULL;
        
    USBLog(7, "+%s[%p]::DoLowLatencyIsochPipeAsync", getName(), this);
    retain();
    IncrementOutstandingIO();		// to make sure LowLatencyIsoReqComplete is still around
    
    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(isocInfo->fPipe);
	if(pipeObj)
	{
	    do {
                // First, attempt to get a command for our transfer
                //
                command = (IOUSBLowLatencyCommand *) fFreeUSBLowLatencyCommandPool->getCommand(false);
                
                // If we couldn't get a command, increase the allocation and try again
                //
                if ( command == NULL )
                {
                    IncreaseCommandPool();
                    
                    command = (IOUSBLowLatencyCommand *) fFreeUSBLowLatencyCommandPool->getCommand(false);
                    if ( command == NULL )
                    {
                        USBLog(3,"%s[%p]::DoLowLatencyIsochPipeAsync Could not get a IOUSBLowLatencyIsocCommand",getName(),this);
                        ret = kIOReturnNoResources;
                        break;
                    }
                }
                
                USBLog(7,"%s[%p]::DoLowLatencyIsochPipeAsync: dataBuffer cookie: %ld, offset: %ld, frameList cookie: %ld, offset : %ld", getName(),this, isocInfo->fDataBufferCookie, isocInfo->fDataBufferOffset, isocInfo->fFrameListBufferCookie, isocInfo->fFrameListBufferOffset );
                
                // Find the buffer corresponding to the data buffer cookie:
                //
                dataBuffer = FindBufferCookieInList(isocInfo->fDataBufferCookie);
                
                if ( dataBuffer == NULL )
		{
		    ret = kIOReturnNoMemory;
		    break;
		}
                
                // Create a new IOMD that is a subrange of our data buffer memory descriptor, and prepare it
                //
                aDescriptor = IOMemoryDescriptor::withSubRange( dataBuffer->bufferDescriptor, isocInfo->fDataBufferOffset, isocInfo->fBufSize, direction );
                if ( aDescriptor == NULL )
		{
		    ret = kIOReturnNoMemory;
		    break;
		}

                // Prepare this descriptor
                //
                ret = aDescriptor->prepare();
                if (ret != kIOReturnSuccess)
                {
                    break;
                }
                
                // Find the buffer corresponding to the frame list cookie:
                //
                frameListDataBuffer = FindBufferCookieInList(isocInfo->fFrameListBufferCookie);
                
                // Get our virtual address by looking at the buffer data and adding in the offset that was passed in
                //
                pFrameList = (IOUSBLowLatencyIsocFrame *) ( (UInt32) frameListDataBuffer->frameListKernelAddress + isocInfo->fFrameListBufferOffset);
                
                // Copy the data into our command buffer
                //
                command->SetAsyncReference( asyncRef );
                command->SetFrameBase( (void *) ((UInt32) frameListDataBuffer->bufferAddress + isocInfo->fFrameListBufferOffset));
                command->SetDataBuffer( aDescriptor );
                
                // Populate our completion routine
                //
                tap.target = this;
		tap.action = &LowLatencyIsoReqComplete;
		tap.parameter = command;
                
		if ( direction == kIODirectionOut )
		    ret = pipeObj->Write(aDescriptor, isocInfo->fStartFrame, isocInfo->fNumFrames, pFrameList, &tap, isocInfo->fUpdateFrequency);
		else
		    ret = pipeObj->Read(aDescriptor, isocInfo->fStartFrame, isocInfo->fNumFrames,pFrameList, &tap, isocInfo->fUpdateFrequency);
	    
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
            
        // return command
        //
        if ( command )
            fFreeUSBLowLatencyCommandPool->returnCommand(command);
        
	DecrementOutstandingIO();
        release();
    }

    USBLog(7, "-%s[%p]::DoLowLatencyIsochPipeAsync", getName(), this);
    return ret;
}



IOReturn 
IOUSBInterfaceUserClient::ReadIsochPipe(OSAsyncReference asyncRef, IOUSBIsocStruct *stuff, UInt32 sizeIn)
{
    return DoIsochPipeAsync(asyncRef, stuff, kIODirectionIn);
}



IOReturn 
IOUSBInterfaceUserClient::WriteIsochPipe(OSAsyncReference asyncRef, IOUSBIsocStruct *stuff, UInt32 sizeIn)
{
    return DoIsochPipeAsync(asyncRef, stuff, kIODirectionOut);
}

IOReturn 
IOUSBInterfaceUserClient::LowLatencyReadIsochPipe(OSAsyncReference asyncRef, IOUSBLowLatencyIsocStruct *stuff, UInt32 sizeIn)
{
    return DoLowLatencyIsochPipeAsync(asyncRef, stuff, kIODirectionIn);
}



IOReturn 
IOUSBInterfaceUserClient::LowLatencyWriteIsochPipe(OSAsyncReference asyncRef, IOUSBLowLatencyIsocStruct *stuff, UInt32 sizeIn)
{
    return DoLowLatencyIsochPipeAsync(asyncRef, stuff, kIODirectionOut);
}


/*
 * Async version of read pipe - the buffer isn't mapped (yet) into the kernel task
 */
IOReturn
IOUSBInterfaceUserClient::AsyncReadPipe(OSAsyncReference asyncRef, UInt32 pipe, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout)
{
    IOReturn 			ret;
    IOUSBPipe 			*pipeObj;
    IOUSBCompletion		tap;
    IOMemoryDescriptor *	mem = NULL;
    AsyncPB * 			pb = NULL;

    USBLog(7, "+%s[%p]::AsyncReadPipe", getName(), this);
    retain();
    IncrementOutstandingIO();		// to make sure ReqComplete is still around
    
    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(pipe);
	if(pipeObj)
	{
	    do {
		mem = IOMemoryDescriptor::withAddress((vm_address_t)buf, size, kIODirectionIn, fTask);
		if(!mem) 
		{
		    ret = kIOReturnNoMemory;
		    break;
		}
		ret = mem->prepare();
		if(ret != kIOReturnSuccess)
		    break;
	
		pb = (AsyncPB *)IOMalloc(sizeof(AsyncPB));
		if(!pb) 
		{
		    ret = kIOReturnNoMemory;
		    break;
		}
	
		bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
		pb->fMax = size;
		pb->fMem = mem;
		tap.target = this;
		tap.action = &ReqComplete;
		tap.parameter = pb;
		ret = pipeObj->Read(mem, noDataTimeout, completionTimeout, &tap, NULL);
	    } while (false);
	    pipeObj->release();
	}
	else
	    ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
    
    if(ret != kIOReturnSuccess) 
    {
	USBLog(3, "%s[%p]::AsyncReadPipe err 0x%x", getName(), this, ret);
	if(mem) 
	{
	    mem->complete();
	    mem->release();
            mem = NULL;
	}
	if(pb)
	    IOFree(pb, sizeof(*pb));
	
        DecrementOutstandingIO();
        release();
    }
    return ret;
}



/*
 * Async version of write pipe - the buffer isn't mapped (yet) into the kernel task
 */
IOReturn
IOUSBInterfaceUserClient::AsyncWritePipe(OSAsyncReference asyncRef, UInt32 pipe, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout)
{
    IOReturn 			ret;
    IOUSBPipe 			*pipeObj;
    IOUSBCompletion		tap;
    IOMemoryDescriptor *	mem = NULL;
    AsyncPB * 			pb = NULL;

    USBLog(7, "+%s[%p]::AsyncWritePipe", getName(), this);
    retain();
    IncrementOutstandingIO();		// to make sure ReqComplete is still around

    if (fOwner && !isInactive())
    {
	pipeObj = GetPipeObj(pipe);
	if(pipeObj)
	{
	    do {
		mem = IOMemoryDescriptor::withAddress((vm_address_t)buf, size, kIODirectionOut, fTask);
		if(!mem) 
		{
		    ret = kIOReturnNoMemory;
		    break;
		}
		ret = mem->prepare();
		if(ret != kIOReturnSuccess)
		    break;
	
		pb = (AsyncPB *)IOMalloc(sizeof(AsyncPB));
		if(!pb) 
		{
		    ret = kIOReturnNoMemory;
		    break;
		}
	
		bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
		pb->fMax = size;
		pb->fMem = mem;
		tap.target = this;
		tap.action = &ReqComplete;
		tap.parameter = pb;
		ret = pipeObj->Write(mem, noDataTimeout, completionTimeout, &tap);
	    } while (false);
	
	    pipeObj->release();
	}
	else
	    ret = kIOUSBUnknownPipeErr;
    }
    else
        ret = kIOReturnNotAttached;
    
    if(ret != kIOReturnSuccess) 
    {
	USBLog(3, "%s[%p]::AsyncWritePipe err 0x%x", getName(), this, ret);
	if(mem) 
	{
	    mem->complete();
	    mem->release();
            mem = NULL;
	}
	if(pb)
	    IOFree(pb, sizeof(*pb));
            
	DecrementOutstandingIO();
        release();
    }
    return ret;
}


//
// stop
// 
// This IOService method is called AFTER we have closed our provider, assuming that the provider was 
// ever opened. If we issue I/O to the provider, then we must have it open, and we will not close
// our provider until all of that I/O is completed.
void 
IOUSBInterfaceUserClient::stop(IOService * provider)
{
    
    USBLog(5, "+%s[%p]::stop(%p)", getName(), this, provider);

    // If we have any kernelDataBuffer pointers, then release them now
    //
    if (fUserClientBufferInfoListHead != NULL)
    {
        ReleasePreparedDescriptors();
    }

    super::stop(provider);

    USBLog(5, "-%s[%p]::stop(%p)", getName(), this, provider);

}

void 
IOUSBInterfaceUserClient::free()
{
    IOReturn ret;

    // USBLog(7, "+%s[%p]::free", getName(), this);
    
    if ( fFreeUSBLowLatencyCommandPool )
    {
        fFreeUSBLowLatencyCommandPool->release();
        fFreeUSBLowLatencyCommandPool = NULL;
    }

    if (fGate)
    {
        if (fWorkLoop)
        {
            ret = fWorkLoop->removeEventSource(fGate);
            fWorkLoop->release();
            fWorkLoop = NULL;
        }

        fGate->release();
        fGate = NULL;
    }
        
    super::free();
}


bool 
IOUSBInterfaceUserClient::finalize( IOOptionBits options )
{
    bool ret;

    USBLog(7, "+%s[%p]::finalize(%08x)", getName(), this, (int)options);
    
    ret = super::finalize(options);
    
    USBLog(7, "-%s[%p]::finalize(%08x) - returning %s", getName(), this, (int)options, ret ? "true" : "false");
    return ret;
}


bool
IOUSBInterfaceUserClient::willTerminate( IOService * provider, IOOptionBits options )
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

            USBLog(4, "%s[%p]::willTerminate - outstanding IO(%d), aborting pipes", getName(), this, ioPending);
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
IOUSBInterfaceUserClient::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
   USBLog(3, "%s[%p]::didTerminate isInactive = %d, outstandingIO = %d", getName(), this, isInactive(), fOutstandingIO);

    if ( fOwner )
    {
        if ( fOutstandingIO == 0 )
            fOwner->close(this);
        else
            fNeedToClose = true;
    }
    
    return super::didTerminate(provider, options, defer);
}


void
IOUSBInterfaceUserClient::DecrementOutstandingIO(void)
{
    if (!fGate)
    {
	if (!--fOutstandingIO && fNeedToClose)
	{
	    USBLog(3, "%s[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - closing device", getName(), this, isInactive(), fOutstandingIO);
	    fOwner->close(this);
	}
	return;
    }
    fGate->runAction(ChangeOutstandingIO, (void*)-1);
}


void
IOUSBInterfaceUserClient::IncrementOutstandingIO(void)
{
    if (!fGate)
    {
	fOutstandingIO++;
	return;
    }
    fGate->runAction(ChangeOutstandingIO, (void*)1);
}


IOReturn
IOUSBInterfaceUserClient::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    IOUSBInterfaceUserClient *me = OSDynamicCast(IOUSBInterfaceUserClient, target);
    UInt32	direction = (UInt32)param1;
    
    if (!me)
    {
	USBLog(1, "IOUSBInterfaceUserClient::ChangeOutstandingIO - invalid target");
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
                USBLog(3, "%s[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me->getName(), me, me->isInactive(), me->fOutstandingIO);
                me->fOwner->close(me);
                }
                break;
	    
	default:
	    USBLog(1, "%s[%p]::ChangeOutstandingIO - invalid direction", me->getName(), me);
    }
    return kIOReturnSuccess;
}


UInt32
IOUSBInterfaceUserClient::GetOutstandingIO()
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
IOUSBInterfaceUserClient::GetGatedOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    IOUSBInterfaceUserClient *me = OSDynamicCast(IOUSBInterfaceUserClient, target);

    if (!me)
    {
	USBLog(1, "IOUSBInterfaceUserClient::GetGatedOutstandingIO - invalid target");
	return kIOReturnSuccess;
    }

    *(UInt32 *) param1 = me->fOutstandingIO;

    return kIOReturnSuccess;
}

void
IOUSBInterfaceUserClient::IncreaseCommandPool(void)
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
IOUSBInterfaceUserClient::ReleasePreparedDescriptors(void)
{
    LowLatencyUserClientBufferInfo *	kernelDataBuffer;
    LowLatencyUserClientBufferInfo *	nextBuffer;

    // If we have any kernelDataBuffer pointers, then release them now
    //
    if (fUserClientBufferInfoListHead != NULL)
    {
        USBLog(5, "+%s[%p]::stop: fUserClientBufferInfoListHead NOT NULL (%p) ", getName(), this, fUserClientBufferInfoListHead);
    
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
                kernelDataBuffer->frameListMap->release();
                kernelDataBuffer->frameListMap = NULL;
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
            
            // Finally, deallocate our kernelDataBuffer
            //
            IOFree(kernelDataBuffer, sizeof(LowLatencyUserClientBufferInfo));
            
            kernelDataBuffer = nextBuffer;
        }
        
        fUserClientBufferInfoListHead = NULL;
    }
}

IOUSBLowLatencyCommand *
IOUSBLowLatencyCommand::NewCommand()
{
    IOUSBLowLatencyCommand *me = new IOUSBLowLatencyCommand;
    
    return me;

}

void  			
IOUSBLowLatencyCommand::SetAsyncReference(OSAsyncReference  ref)
{
    bcopy(ref, fAsyncRef, sizeof(OSAsyncReference));
}
