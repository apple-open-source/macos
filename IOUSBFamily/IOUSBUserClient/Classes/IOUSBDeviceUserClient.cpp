/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>

#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBLog.h>

#include "IOUSBDeviceUserClient.h"

#define super IOUserClient

struct AsyncPB {
    OSAsyncReference 		fAsyncRef;
    UInt32 			fMax;
    IOMemoryDescriptor 		*fMem;
    IOUSBDevRequestDesc		req;
};

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
OSDefineMetaClassAndStructors(IOUSBDeviceUserClient, IOUserClient)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
enum {
        kMethodObjectThis = 0,
        kMethodObjectOwner
    };
    
const IOExternalMethod 
IOUSBDeviceUserClient::sMethods[kNumUSBDeviceMethods] = {
    { //    kUSBDeviceUserClientOpen
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::open,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBDeviceUserClientClose
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::close,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kUSBDeviceUserClientSetConfig
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::SetConfiguration,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBDeviceUserClientGetConfig
	(IOService*)kMethodObjectOwner,
	(IOMethod) &IOUSBDevice::GetConfiguration,
	kIOUCScalarIScalarO,
	0,
	1
    },
    { //    kUSBDeviceUserClientGetConfigDescriptor
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::GetConfigDescriptor,
	kIOUCScalarIStructO,
	1,
	0xffffffff
    },
    { //    kUSBDeviceUserClientGetFrameNumber
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::GetFrameNumber,
	kIOUCScalarIStructO,
	0,
	0xffffffff
    },
    { //    kUSBDeviceUserClientDeviceRequestOut
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::DeviceReqOut,
	kIOUCScalarIStructI,
	4,
	0xffffffff
    },
    { //    kUSBDeviceUserClientDeviceRequestIn
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::DeviceReqIn,
	kIOUCScalarIStructO,
	4,
	0xffffffff
    },
    { //    kUSBDeviceUserClientDeviceRequestOutOOL
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::DeviceReqOutOOL,
	kIOUCStructIStructO,
	sizeof(IOUSBDevRequestTO),
	0
    },
    { //    kUSBDeviceUserClientDeviceRequestInOOL
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::DeviceReqInOOL,
	kIOUCStructIStructO,
	sizeof(IOUSBDevRequestTO),
	sizeof(UInt32)
    },
    { //    kUSBDeviceUserClientCreateInterfaceIterator
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::CreateInterfaceIterator,
	kIOUCStructIStructO,
	sizeof(IOUSBFindInterfaceRequest),
	sizeof(io_iterator_t)
    },
    { //    kUSBDeviceUserClientResetDevice
	(IOService*)kMethodObjectOwner,
	(IOMethod) &IOUSBDevice::ResetDevice,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kUSBDeviceUserClientSuspend
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::SuspendDevice,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBDeviceUserClientAbortPipeZero
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::AbortPipeZero,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kUSBDeviceUserClientReEnumerateDevice
	(IOService*)kMethodObjectThis,
	(IOMethod) &IOUSBDeviceUserClient::ReEnumerateDevice,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kUSBDeviceUserClientGetMicroFrameNumber
        (IOService*)kMethodObjectThis,
        (IOMethod) &IOUSBDeviceUserClient::GetMicroFrameNumber,
        kIOUCScalarIStructO,
        0,
        0xffffffff
    }
};



const IOExternalAsyncMethod 
IOUSBDeviceUserClient::sAsyncMethods[kNumUSBDeviceAsyncMethods] = {
    { //    kUSBDeviceUserClientSetAsyncPort
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBDeviceUserClient::SetAsyncPort,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kUSBDeviceUserClientDeviceAsyncRequestOut
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBDeviceUserClient::DeviceReqOutAsync,
	kIOUCStructIStructO,
	sizeof(IOUSBDevRequestTO),
	0
    },
    { //    kUSBDeviceUserClientDeviceAsyncRequestIn
	(IOService*)kMethodObjectThis,
	(IOAsyncMethod) &IOUSBDeviceUserClient::DeviceReqInAsync,
	kIOUCStructIStructO,
	sizeof(IOUSBDevRequestTO),
	0
    }
};



void 
IOUSBDeviceUserClient::SetExternalMethodVectors()
{
    fMethods = sMethods;
    fNumMethods = kNumUSBDeviceMethods;
    fAsyncMethods = sAsyncMethods;
    fNumAsyncMethods = kNumUSBDeviceAsyncMethods;
}



IOExternalMethod *
IOUSBDeviceUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
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
IOUSBDeviceUserClient::getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index)
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
IOUSBDeviceUserClient::initWithTask(task_t owningTask,void *security_id , UInt32 type )
{
    if (!super::initWithTask(owningTask, security_id , type))
        return false;
    
    if (!owningTask)
	return false;
	
    fTask = owningTask;

    fOwner = NULL;
    fGate = NULL;
    fDead = false;
    SetExternalMethodVectors();
    return true;
}



bool 
IOUSBDeviceUserClient::start( IOService * provider )
{
    IncrementOutstandingIO();		// make sure we don't close until start is done

    fOwner = OSDynamicCast(IOUSBDevice, provider);

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
    
    fGate = IOCommandGate::commandGate(this);

    if(!fGate)
    {
	USBError(1, "%s[%p]::start - unable to create command gate", getName(), this);
	goto ErrorExit;
    }

    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
    {
	USBError(1, "%s[%p]::start - unable to find my workloop", getName(), this);
	goto ErrorExit;
    }
    fWorkLoop->retain();
        
    if (fWorkLoop->addEventSource(fGate) != kIOReturnSuccess)
    {
	USBError(1, "%s[%p]::start - unable to add gate to work loop", getName(), this);
	goto ErrorExit;
    }

    DecrementOutstandingIO();
    return true;
    
ErrorExit:
    
    if ( fGate != NULL )
    {
        fGate->release();
        fGate = NULL;
    }
        
    DecrementOutstandingIO();
    return false;
}



IOReturn 
IOUSBDeviceUserClient::open(bool seize)
{
    IOOptionBits	options = (seize ? (IOOptionBits)kIOServiceSeize : 0);
    IOReturn		ret = kIOReturnSuccess;

    USBLog(3, "+%s[%p]::open", getName(), this);
    IncrementOutstandingIO();

    if (fOwner && !isInactive())
    {
	if (fOwner->open(this, options))
	    fNeedToClose = false;
	else
	    ret = kIOReturnExclusiveAccess;
    }
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    USBLog(3, "-%s[%p]::open - returning %x", getName(), this, ret);    
    return ret;
}



// This is NOT the normal IOService::close(IOService*) method. It is intended to handle a close coming 
// in from the user side. Since we do not have any IOKit objects as children, we will just use this to
// terminate ourselves.
IOReturn 
IOUSBDeviceUserClient::close()
{
    IOReturn 	ret = kIOReturnSuccess;
    
    USBLog(3, "+%s[%p]::close", getName(), this);
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
	if (fOwner->isOpen(this))
	{
	    fNeedToClose = true;			// this will cause the DecrementOutstandingIO to close us
	    if (GetOutstandingIO() > 1)
	    {
		// we need to abort any outstanding IO to allow us to close
		USBLog(3, "%s[%p]::close - outstanding IO - aborting pipe zero", getName(), this);
		AbortPipeZero();
	    }
	}
	else
	    ret = kIOReturnNotOpen;
    }
    else
        ret = kIOReturnNotAttached;
	
    DecrementOutstandingIO();
    USBLog(3, "-%s[%p]::close - returning %x", getName(), this, ret);
    return ret;
}



void
IOUSBDeviceUserClient::ReqComplete(void *obj, void *param, IOReturn res, UInt32 remaining)
{
    void *	args[1];
    AsyncPB * pb = (AsyncPB *)param;
    IOUSBDeviceUserClient *me = OSDynamicCast(IOUSBDeviceUserClient, (OSObject*)obj);

    if (!me)
	return;
	
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
    }
    if (!me->fDead)
	sendAsyncResult(pb->fAsyncRef, res, args, 1);

    IOFree(pb, sizeof(*pb));
    me->DecrementOutstandingIO();
    me->release();
}



IOReturn 
IOUSBDeviceUserClient::GetFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size)
{
    IOReturn		ret = kIOReturnSuccess;
    
    if(*size != sizeof(IOUSBGetFrameStruct))
        return kIOReturnBadArgument;

    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
	clock_get_uptime(&data->timeStamp);
	data->frame = fOwner->GetBus()->GetFrameNumber();
    }
    else
        ret = kIOReturnNotAttached;
	
    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::GetFrameNumber - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn
IOUSBDeviceUserClient::GetMicroFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size)
{
    // This method only available for v2 controllers
    //
    IOUSBControllerV2	*v2 = OSDynamicCast(IOUSBControllerV2, fOwner->GetBus());
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
IOUSBDeviceUserClient::SetAsyncPort(OSAsyncReference asyncRef)
{
    if (!fOwner)
        return kIOReturnNotAttached;
        
    fWakePort = (mach_port_t) asyncRef[0];
    return kIOReturnSuccess;
}



IOReturn
IOUSBDeviceUserClient::ResetDevice()
{
    IOReturn	ret;
    
    IncrementOutstandingIO();

    if (fOwner && !isInactive())
	ret = fOwner->ResetDevice();
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::ResetDevice - returning err %x", getName(), this, ret);
    return ret;
}


IOReturn
IOUSBDeviceUserClient::SetConfiguration(UInt8 configIndex)
{
    IOReturn	ret;

    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
	ret = fOwner->SetConfiguration(this, configIndex);
    else
        ret = kIOReturnNotAttached;
        
    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::SetConfiguration - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn
IOUSBDeviceUserClient::GetConfigDescriptor(UInt8 configIndex, IOUSBConfigurationDescriptorPtr desc, UInt32 *size)
{
    UInt16 				length;
    const IOUSBConfigurationDescriptor	*cached;
    IOReturn				ret;
    

    USBLog(7,"%s[%p]::GetConfigDescriptor (%d)", getName(), this, configIndex);
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
    {
	cached = fOwner->GetFullConfigurationDescriptor(configIndex);
	if ( cached == NULL )
	{
	    desc = NULL;
	    ret = kIOReturnNotFound;
	}
	else
	{
	    length = USBToHostWord(cached->wTotalLength);
	    if(length < *size)
		*size = length;
	    bcopy(cached, desc, *size);
	    ret = kIOReturnSuccess;
	}
    }
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::GetConfigDescriptor - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn 
IOUSBDeviceUserClient::CreateInterfaceIterator(IOUSBFindInterfaceRequest *reqIn, io_object_t *iterOut, IOByteCount inCount, IOByteCount *outCount)
{
    OSIterator		*iter;
    IOReturn		ret = kIOReturnSuccess;

    IncrementOutstandingIO();

    if (fOwner && !isInactive())
    {
	iter = fOwner->CreateInterfaceIterator(reqIn);
    
	if(iter) 
	{
	    *outCount = sizeof(io_object_t);
	    ret = exportObjectToClient(fTask, iter, iterOut);
	}
	else
	    ret = kIOReturnNoMemory;
    }
    else
        ret = kIOReturnNotAttached;
    
    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::CreateInterfaceIterator - returning err %x", getName(), this, ret);
    return ret;
}




/*
 * There's a limit of max 6 arguments to user client methods, so the type, recipient and request
 * are packed into one 16 bit integer.
 */
IOReturn
IOUSBDeviceUserClient::DeviceReqIn(UInt16 param1, UInt32 param2, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size)
{
    IOReturn 			ret;
    IOUSBDevRequest		req;
    UInt8			bmRequestType = (param1 >> 8) & 0xFF;
    UInt8			bRequest = param1 & 0xFF;
    UInt16			wValue = (param2 >> 16) & 0xFFFF;
    UInt16			wIndex = param2 & 0xFFFF;
    
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
    {
	req.bmRequestType = bmRequestType;
	req.bRequest = bRequest;
	req.wValue = wValue;
	req.wIndex = wIndex;
	req.wLength = *size;
	req.pData = buf;
	req.wLenDone = 0;
	
	ret = fOwner->DeviceRequest(&req, noDataTimeout, completionTimeout);
    
	if(ret == kIOReturnSuccess) 
	{
	    *size = req.wLenDone;
	}
	else 
	{
	    USBLog(3, "%s[%p]::DeviceReqIn err:0x%x", getName(), this, ret);
	    *size = 0;
	}
    }
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::DeviceReqIn - returning err %x", getName(), this, ret);
    return ret;
}



/*
 * There's a limit of max 6 arguments to user client methods, so the type, recipient and request
 * are packed into one 16 bit integer.
 */
IOReturn
IOUSBDeviceUserClient::DeviceReqOut(UInt16 param1, UInt32 param2, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 size)
{
    IOReturn 			ret;
    IOUSBDevRequest		req;
    UInt8			bmRequestType = (param1 >> 8) & 0xFF;
    UInt8			bRequest = param1 & 0xFF;
    UInt16			wValue = (param2 >> 16) & 0xFFFF;
    UInt16			wIndex = param2 & 0xFFFF;
    
    IncrementOutstandingIO();

    if (fOwner && !isInactive())
    {
	req.bmRequestType = bmRequestType;
	req.bRequest = bRequest;
	req.wValue = wValue;
	req.wIndex = wIndex;
	req.wLength = size;
	req.pData = buf;
	ret = fOwner->DeviceRequest(&req, noDataTimeout, completionTimeout);
    
	if(kIOReturnSuccess != ret) 
	    USBLog(3, "%s[%p]::DeviceReqOut err:0x%x", getName(), this, ret);
    }
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::DeviceReqOut - returning err %x", getName(), this, ret);
    return ret;
}



//
// DeviceReqInOOL: A device request where the returned data is going to be larger than 4K
//
IOReturn
IOUSBDeviceUserClient::DeviceReqInOOL(IOUSBDevRequestTO *reqIn, IOByteCount inCount, UInt32 *sizeOut, IOByteCount *outCount)
{
    IOReturn 			ret;
    IOUSBDevRequestDesc		req;
    IOMemoryDescriptor *	mem;

    if((inCount != sizeof(IOUSBDevRequestTO)) || (*outCount != sizeof(UInt32)))
        return kIOReturnBadArgument;

    IncrementOutstandingIO();
    if (fOwner && !isInactive())
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
	    req.wLenDone = 0;
	
	    ret = mem->prepare();
	    if(ret == kIOReturnSuccess)
		ret = fOwner->DeviceRequest(&req, reqIn->noDataTimeout, reqIn->completionTimeout);

	    mem->complete();
	    mem->release();
	    if(ret == kIOReturnSuccess) 
		*sizeOut = req.wLenDone;
	    else 
	    {
		USBLog(3, "%s[%p]::DeviceReqInOOL err:0x%x", getName(), this, ret);
		*sizeOut = 0;
	    }
	    *outCount = sizeof(UInt32);
	}
	else
	    ret = kIOReturnNoMemory;
    }
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::DeviceReqInOOL - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn
IOUSBDeviceUserClient::DeviceReqInAsync(OSAsyncReference asyncRef, IOUSBDevRequestTO *reqIn, IOByteCount inCount)
{
    IOReturn 			ret = kIOReturnSuccess;
    IOUSBCompletion		tap;
    AsyncPB * 			pb = NULL;
    IOMemoryDescriptor *	mem = NULL;

    if(inCount != sizeof(IOUSBDevRequestTO))
        return kIOReturnBadArgument;

    retain();
    IncrementOutstandingIO();
    if (fOwner && !isInactive())
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
	    ret = fOwner->DeviceRequest(&pb->req, reqIn->noDataTimeout, reqIn->completionTimeout, &tap);
	}
    }
    else
        ret = kIOReturnNotAttached;

    if (ret != kIOReturnSuccess)
    {
	USBError(1, "%s[%p]::DeviceReqInAsync - could not send request -  err 0x%x", getName(), this, ret);
	if (mem)
	{
	    mem->complete();
	    mem->release();
	}
	if(pb)
	    IOFree(pb, sizeof(*pb));
	
        // since there was SOME error, we need to dec here. otherwise, it will be handled in ReqComplete
	DecrementOutstandingIO();
        release();
    }

    return ret;
}


//
// DeviceReqOutOOL: The request is > 4K
//
IOReturn
IOUSBDeviceUserClient::DeviceReqOutOOL(IOUSBDevRequestTO *reqIn, IOByteCount inCount)
{
    IOReturn 			ret;
    IOUSBDevRequestDesc		req;
    IOMemoryDescriptor *	mem;

    if(inCount != sizeof(IOUSBDevRequestTO))
        return kIOReturnBadArgument;

    IncrementOutstandingIO();
    if (fOwner && !isInactive())
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
		ret = fOwner->DeviceRequest(&req, reqIn->noDataTimeout, reqIn->completionTimeout);
	    mem->complete();
	    mem->release();
	}
	else
	    ret =  kIOReturnNoMemory;
    }
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::DeviceReqOutOOL - returning err %x", getName(), this, ret);
    return ret;
}


IOReturn
IOUSBDeviceUserClient::DeviceReqOutAsync(OSAsyncReference asyncRef, IOUSBDevRequestTO *reqIn, IOByteCount inCount)
{
    IOReturn 			ret = kIOReturnSuccess;
    IOUSBCompletion		tap;
    AsyncPB * 			pb = NULL;
    IOMemoryDescriptor *	mem = NULL;

    if(inCount != sizeof(IOUSBDevRequestTO))
        return kIOReturnBadArgument;

    retain();
    IncrementOutstandingIO();		// to make sure ReqComplete is still around
    if (fOwner && !isInactive())
    {
	if (reqIn->wLength)
	{
	    if (!reqIn->pData)
		ret = kIOReturnBadArgument;
	    else
	    {
		mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->pData, reqIn->wLength, kIODirectionOut, fTask);
		if (!mem)
		    ret = kIOReturnNoMemory;
	    }
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
	    ret = fOwner->DeviceRequest(&pb->req, reqIn->noDataTimeout, reqIn->completionTimeout, &tap);
	}
    }
    else
        ret = kIOReturnNotAttached;

    if(ret != kIOReturnSuccess) 
    {
	USBError(1, "%s[%p]::DeviceReqOutAsync - could not send request -  err 0x%x", getName(), this, ret);
	if(mem) 
	{
	    mem->complete();
	    mem->release();
	}
	if(pb)
	    IOFree(pb, sizeof(*pb));
	DecrementOutstandingIO();
        release();
    }

    return ret;
}


IOReturn
IOUSBDeviceUserClient::SuspendDevice(bool suspend)
{
    IOReturn 	ret;
    
    IncrementOutstandingIO();

    if (fOwner && !isInactive())
	ret = fOwner->SuspendDevice(suspend);
    else
        ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::SuspendDevice - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn
IOUSBDeviceUserClient::AbortPipeZero(void)
{
    IOReturn 	ret;

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

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::AbortPipeZero - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn
IOUSBDeviceUserClient::ReEnumerateDevice(UInt32 options)
{
    IOReturn 	ret;
    
    IncrementOutstandingIO();
    
    if (fOwner && !isInactive())
	ret = fOwner->ReEnumerateDevice(options);
    else
	ret = kIOReturnNotAttached;

    DecrementOutstandingIO();
    if (ret)
	USBLog(3, "%s[%p]::ReEnumerateDevice - returning err %x", getName(), this, ret);
    return ret;
}



IOReturn 
IOUSBDeviceUserClient::clientClose( void )
{
    USBLog(7, "+%s[%p]::clientClose()", getName(), this);

    // Sleep for 1 ms to allow other threads that are pending to run
    //
    IOSleep(1);
    
    fTask = NULL;
    	
    terminate();			// this will call stop eventually
    
    USBLog(7, "-%s[%p]::clientClose()", getName(), this);

    return kIOReturnSuccess;		// DONT call super::clientClose, which just returns notSupported
}



IOReturn 
IOUSBDeviceUserClient::clientDied( void )
{
    IOReturn ret;
    
    USBLog(3, "+%s[%p]::clientDied", getName(), this);

    fDead = true;			// don't send mach messages in this case
    ret = super::clientDied();		// this just calls clientClose

    USBLog(3, "-%s[%p]::clientDied", getName(), this);

    return ret;
}


//
// stop
// 
// This IOService method is called AFTER we have closed our provider, assuming that the provider was 
// ever opened. If we issue I/O to the provider, then we must have it open, and we will not close
// our provider until all of that I/O is completed.
void 
IOUSBDeviceUserClient::stop(IOService * provider)
{
    
    USBLog(7, "+%s[%p]::stop(%p)", getName(), this, provider);

    super::stop(provider);

    USBLog(7, "-%s[%p]::stop(%p)", getName(), this, provider);

}

void 
IOUSBDeviceUserClient::free()
{
    
    if (fGate)
    {
	if (fWorkLoop)
	{
	    fWorkLoop->removeEventSource(fGate);
            fWorkLoop->release();
            fWorkLoop = NULL;
	}
	else
	{
	    USBError(1, "%s[%p]::free - have gate, but no valid workloop!", getName(), this);
	}
	fGate->release();
	fGate = NULL;
    }

    super::free();
}


// This is an IOKit method which is called AFTER we close our parent, but BEFORE stop.
bool 
IOUSBDeviceUserClient::finalize( IOOptionBits options )
{
    bool ret;

    USBLog(7, "+%s[%p]::finalize(%08x)", getName(), this, (int)options);
    
    ret = super::finalize(options);
    
    USBLog(7, "-%s[%p]::finalize(%08x) - returning %s", getName(), this, (int)options, ret ? "true" : "false");
    return ret;
}



// This is an IOKit method which lets us know that out PARENT is terminating, usually indicating that the
// USB device has been removed.
bool
IOUSBDeviceUserClient::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that 
    // we have begun getting our callbacks in order. by the time we get here, the 
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    USBLog(3, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());
    return super::willTerminate(provider, options);
}



bool
IOUSBDeviceUserClient::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
    USBLog(3, "%s[%p]::didTerminate isInactive = %d, outstandingIO = %d", getName(), this, isInactive(), fOutstandingIO);
    if ( fOutstandingIO == 0 )
	fOwner->close(this);
    else
	fNeedToClose = true;
    
    return super::didTerminate(provider, options, defer);
}


void
IOUSBDeviceUserClient::DecrementOutstandingIO(void)
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
IOUSBDeviceUserClient::IncrementOutstandingIO(void)
{
    if (!fGate)
    {
	fOutstandingIO++;
	return;
    }
    fGate->runAction(ChangeOutstandingIO, (void*)1);
}


IOReturn
IOUSBDeviceUserClient::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    IOUSBDeviceUserClient 	*me = OSDynamicCast(IOUSBDeviceUserClient, target);
    UInt32			direction = (UInt32)param1;
    
    if (!me)
    {
	USBLog(1, "IOUSBDeviceUserClient::ChangeOutstandingIO - invalid target");
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
IOUSBDeviceUserClient::GetOutstandingIO()
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
IOUSBDeviceUserClient::GetGatedOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    IOUSBDeviceUserClient *me = OSDynamicCast(IOUSBDeviceUserClient, target);
    
    if (!me)
    {
	USBLog(1, "IOUSBDeviceUserClient::GetGatedOutstandingIO - invalid target");
	return kIOReturnSuccess;
    }

    *(UInt32 *) param1 = me->fOutstandingIO;

    return kIOReturnSuccess;
}



