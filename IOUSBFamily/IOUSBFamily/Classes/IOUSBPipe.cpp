/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <libkern/OSByteOrder.h>

#include <IOKit/IOService.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/usb/IOUSBController.h>

#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBLog.h>

#define super OSObject


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOUSBPipe, OSObject)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool 
IOUSBPipe::InitToEndpoint(const IOUSBEndpointDescriptor *ed, UInt8 speed, USBDeviceAddress address, IOUSBController * controller)
{
    IOReturn	err;
    
    if( !super::init() || ed == 0)
        return (false);

    // allocate our expansion data
    if (!_expansionData)
    {
        _expansionData = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
        if (!_expansionData)
            return false;
        bzero(_expansionData, sizeof(ExpansionData));
    }

    _controller = controller;
    _descriptor = ed;
    _endpoint.number = ed->bEndpointAddress & kUSBPipeIDMask;
    _endpoint.transferType = ed->bmAttributes & 0x03;
    if(_endpoint.transferType == kUSBControl)
        _endpoint.direction = kUSBAnyDirn;
    else
	_endpoint.direction = (ed->bEndpointAddress & 0x80) ? kUSBIn : kUSBOut;
    _endpoint.maxPacketSize = USBToHostWord(ed->wMaxPacketSize);
    _endpoint.interval = ed->bInterval;
    _status = 0;
    _address = address;
    
    err = _controller->OpenPipe(_address, speed, &_endpoint);
    
    if ((err == kIOReturnNoBandwidth) && (_endpoint.transferType == kUSBIsoc))
    {
	_endpoint.maxPacketSize = 0;
	USBLog(2, "IOUSBPipe[%p]::InitToEndpoint - can't get bandwidth for full isoc pipe - creating 0 bandwidth pipe", this);
	err = _controller->OpenPipe(_address, speed, &_endpoint);
    }
    
    if( err != kIOReturnSuccess)
    {
        USBLog(3,"IOUSBPipe[%p]::InitToEndpoint Could not create pipe for endpoint (Addr: %d, numb: %d, type: %d).  Error 0x%x", this, _address, _endpoint.number, _endpoint.transferType, err);
        return false;
    }
    
    return true;
}



IOUSBPipe *
IOUSBPipe::ToEndpoint(const IOUSBEndpointDescriptor *ed, UInt8 speed, USBDeviceAddress address, IOUSBController *controller)
{
    IOUSBPipe *me = new IOUSBPipe;

    if ( me && !me->InitToEndpoint(ed, speed, address, controller) ) 
    {
        me->release();
        return NULL;
    }

    return me;
}



void 
IOUSBPipe::free()
{
    USBLog(7,"IOUSBPipe[%p] free",this);

    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_expansionData)
    {
        bzero(_expansionData, sizeof(ExpansionData));
        IOFree(_expansionData, sizeof(ExpansionData));
    }

    super::free();
}



// Controlling pipe state

IOReturn 
IOUSBPipe::Abort(void)
{
    USBLog(5,"IOUSBPipe[%p]::AbortPipe",this);
    if (_correctStatus != 0)
        USBLog(2, "IOUSBPipe[%p]::Abort setting status to 0");
    _correctStatus = 0;
    return _controller->AbortPipe(_address, &_endpoint);
}



IOReturn 
IOUSBPipe::Reset(void)
{
    USBLog(5,"+IOUSBPipe[%p]::ResetPipe",this);
    if (_correctStatus != 0)
        USBLog(2, "IOUSBPipe[%p]::ResetPipe setting status to 0");
    _correctStatus = 0;
    return _controller->ResetPipe(_address, &_endpoint);
}



IOReturn 
IOUSBPipe::ClearStall(void)
{
    return ClearPipeStall(false);
}



// Transferring Data
IOReturn 
IOUSBPipe::Read(IOMemoryDescriptor *buffer, IOUSBCompletion *completion, IOByteCount *bytesRead)
{
    USBLog(7, "IOUSBPipe[%p]::Read #1", this);
    return Read(buffer, 0, 0, completion, bytesRead);
}



IOReturn 
IOUSBPipe::Write(IOMemoryDescriptor *buffer, IOUSBCompletion *completion)
{
    USBLog(7, "IOUSBPipe[%p]::Write #1", this);
    return Write(buffer, 0, 0, completion);
}



// Isochronous read and write
IOReturn 
IOUSBPipe::Read(IOMemoryDescriptor * buffer, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *pFrames, IOUSBIsocCompletion *completion)
{
    IOReturn	err = kIOReturnSuccess;

    if (_correctStatus == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Read - invalid read on a stalled isoch pipe", this);
        return kIOUSBPipeStalled;
    }

    if (_endpoint.maxPacketSize == 0)
    {
        USBLog(2, "IOUSBPipe[%p]::Read - no bandwidth on an isoc pipe", this);
        return kIOReturnNoBandwidth;
    }

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBIsocCompletion	tap;
        IOSyncer *		syncer;

        syncer  = IOSyncer::create();

        tap.target = (void *)syncer;
        tap.action = IOUSBSyncIsoCompletion;
        tap.parameter = NULL;

        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames,
                _address, &_endpoint, &tap);

        if (err == kIOReturnSuccess) {
            err = syncer->wait();

            // any err coming back in the callback indicates a stalled pipe
            if (err && (err != kIOUSBTransactionTimeout))
            {
                USBLog(2, "IOUSBPipe[%p]::Read  - i/o err (0x%x) on sync call - stalling pipe", this, err);
                _correctStatus = kIOUSBPipeStalled;
            }
        }
        else {
            syncer->release(); syncer->release();
	}
    }
    else {
        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames,
                _address, &_endpoint, completion);
    }

    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Read (isoch) - controller returned stalled pipe, changing status");
        _correctStatus = kIOUSBPipeStalled;
    }

    return(err);
}



IOReturn 
IOUSBPipe::Write(IOMemoryDescriptor * buffer, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *pFrames, IOUSBIsocCompletion *completion)
{
    IOReturn	err = kIOReturnSuccess;

    if (_correctStatus == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Write - invalid write on a stalled isoch pipe", this);
        return kIOUSBPipeStalled;
    }

    if (_endpoint.maxPacketSize == 0)
    {
        USBLog(2, "IOUSBPipe[%p]::Write - no bandwidth on an isoc pipe", this);
        return kIOReturnNoBandwidth;
    }

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBIsocCompletion	tap;
        IOSyncer *		syncer;

        syncer  = IOSyncer::create();

        tap.target = syncer;
        tap.action = &IOUSBSyncIsoCompletion;
        tap.parameter = NULL;

        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, &tap);

        if (err == kIOReturnSuccess)
        {
            err = syncer->wait();
            // any err coming back in the callback indicates a stalled pipe
            if (err && (err != kIOUSBTransactionTimeout))
            {
                USBLog(2, "IOUSBPipe[%p]::Write  - i/o err (0x%x) on sync call - stalling pipe", this, err);
                _correctStatus = kIOUSBPipeStalled;
            }
        }
        else {
            syncer->release(); syncer->release();
        }
    }
    else
    {
        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, completion);

    }

    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Write (isoch) - controller returned stalled pipe, changing status");
        _correctStatus = kIOUSBPipeStalled;
    }

    return(err);
}



IOReturn 
IOUSBPipe::ControlRequest(IOUSBDevRequest *request, IOUSBCompletion *completion)
{
    return ControlRequest(request, _endpoint.number ? 0 : kUSBDefaultControlNoDataTimeoutMS, _endpoint.number ? 0 : kUSBDefaultControlCompletionTimeoutMS, completion);
}



IOReturn 
IOUSBPipe::ControlRequest(IOUSBDevRequestDesc *request, IOUSBCompletion	*completion)
{
    return ControlRequest(request, _endpoint.number ? 0 : kUSBDefaultControlNoDataTimeoutMS, _endpoint.number ? 0 : kUSBDefaultControlCompletionTimeoutMS, completion);
}


IOReturn 
IOUSBPipe::ClosePipe(void)
{
    // This method should be called by the provider of the IOUSBPipe (IOUSBInterface) right before it
    // releases the pipe.  We do it here instead of in the free method (as was previosuly done) because
    // _controller->ClosePipe() will end up deleting the endpoint, which is necessary.  If we left this
    // call in the free method then this wouldn't get called if someone had an extra retain on the pipe object.
    // Hence the USBError.
    //
    if ( getRetainCount() > 1 )
    {
        USBError(1,"IOUSBPipe[%p]:ClosePipe for address %d, ep %d had a retain count > 1.  Leaking a pipe", this, _address, _endpoint.number);
    }
    
    return _controller->ClosePipe(_address, &_endpoint);
}

const IOUSBController::Endpoint *	 
IOUSBPipe::GetEndpoint() 
{ 
    return(&_endpoint); 
}

const IOUSBEndpointDescriptor *	 
IOUSBPipe::GetEndpointDescriptor() 
{ 
    return(_descriptor); 
}

UInt8  
IOUSBPipe::GetDirection() 
{ 
    return(_endpoint.direction); 
}

UInt8  
IOUSBPipe::GetType() 
{ 
    return(_endpoint.transferType); 
}

UInt8  
IOUSBPipe::GetEndpointNumber() 
{ 
    return(_endpoint.number); 
}

USBDeviceAddress  
IOUSBPipe::GetAddress() 
{ 
    return(_address); 
}

UInt16  
IOUSBPipe::GetMaxPacketSize() 
{ 
    return _endpoint.maxPacketSize; 
}

UInt8  
IOUSBPipe::GetInterval() 
{ 
    return (_endpoint.interval); 
}





OSMetaClassDefineReservedUsed(IOUSBPipe,  0);
IOReturn 
IOUSBPipe::Read(IOMemoryDescriptor *buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion, IOByteCount *bytesRead)
{
    USBLog(7, "IOUSBPipe[%p]::Read #2", this);
    // Validate that there is a buffer so that we can call getLength on it
    if (!buffer)
	return kIOReturnBadArgument;
    
    return Read(buffer, noDataTimeout, completionTimeout, buffer->getLength(), completion, bytesRead);
}
	


OSMetaClassDefineReservedUsed(IOUSBPipe,  1);
IOReturn 
IOUSBPipe::Write(IOMemoryDescriptor *buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
    USBLog(7, "IOUSBPipe[%p]::Write #2", this);
    // Validate that there is a buffer so that we can call getLength on it
    if (!buffer)
	return kIOReturnBadArgument;
    
    return Write(buffer, noDataTimeout, completionTimeout, buffer->getLength(), completion);
}


OSMetaClassDefineReservedUsed(IOUSBPipe,  2);
IOReturn 
IOUSBPipe::ControlRequest(IOUSBDevRequestDesc *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion	*completion)
{
    IOReturn	err = kIOReturnSuccess;


#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: deviceRequest([%x,%x],[%x,%x],[%x,%lx])\n", "IOUSBPipe",
             request->bmRequestType,
             request->bRequest,
             request->wValue,
             request->wIndex,
             request->wLength,
             (UInt32)request->pData);
#endif

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

        syncer  = IOSyncer::create();
        request->wLenDone = request->wLength;

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = &request->wLenDone;

        err = _controller->DeviceRequest(request, &tap, _address, _endpoint.number, noDataTimeout, completionTimeout);
        if (err == kIOReturnSuccess) 
	{
            err = syncer->wait();
        }
	else 
	{
            syncer->release(); syncer->release();
	}
    }
    else
    {
        err = _controller->DeviceRequest(request, completion, _address, _endpoint.number, noDataTimeout, completionTimeout);
    }

    return(err);
}


OSMetaClassDefineReservedUsed(IOUSBPipe,  3);
IOReturn 
IOUSBPipe::ControlRequest(IOUSBDevRequest *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
    IOReturn	err = kIOReturnSuccess;


#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: deviceRequest([%x,%x],[%x,%x],[%x,%lx])\n", "IOUSBPipe",
             request->bmRequestType,
             request->bRequest,
             request->wValue,
             request->wIndex,
             request->wLength,
             (UInt32)request->pData);
#endif

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

        syncer  = IOSyncer::create();
        request->wLenDone = request->wLength;

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = &request->wLenDone;

        err = _controller->DeviceRequest(request, &tap, _address, _endpoint.number, noDataTimeout, completionTimeout);
        if (err == kIOReturnSuccess) 
	{
            err = syncer->wait();
        }
	else 
	{
            syncer->release(); syncer->release();
        }
    }
    else
    {
        err = _controller->DeviceRequest(request, completion, _address, _endpoint.number, noDataTimeout, completionTimeout);
    }

    return(err);
}


OSMetaClassDefineReservedUsed(IOUSBPipe,  4);
IOReturn 
IOUSBPipe::Read(IOMemoryDescriptor *buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount, IOUSBCompletion *completion, IOByteCount *bytesRead)
{
    IOReturn	err = kIOReturnSuccess;

    USBLog(7, "IOUSBPipe[%p]::Read #3 - reqCount = %d", this, reqCount);
    if ((_endpoint.transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
	return kIOReturnBadArgument;

    if (!buffer || (buffer->getLength() < reqCount))
	return kIOReturnBadArgument;

    if (_correctStatus == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Read - invalid read on a stalled pipe", this);
        return kIOUSBPipeStalled;
    }

    if (completion == NULL)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

        syncer  = IOSyncer::create();
	if (bytesRead)
	    *bytesRead = reqCount;

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = bytesRead;

        err = _controller->Read(buffer, _address, &_endpoint, &tap, noDataTimeout, completionTimeout);
        if (err == kIOReturnSuccess)
	{
            err = syncer->wait();
            // any err coming back in the callback indicates a stalled pipe
            if (err && (err != kIOUSBTransactionTimeout))
            {
                USBLog(2, "IOUSBPipe[%p]::Read  - i/o err (0x%x) on sync call - stalling pipe", this, err);
                _correctStatus = kIOUSBPipeStalled;
            }
        }
        else 
	{
            syncer->release(); syncer->release();
        }
    }
    else
    {
        err = _controller->Read(buffer, _address, &_endpoint, completion, noDataTimeout, completionTimeout, reqCount);
    }

    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Read  - controller returned stalled pipe, changing status");
        _correctStatus = kIOUSBPipeStalled;
    }

    return(err);
}



OSMetaClassDefineReservedUsed(IOUSBPipe,  5);
IOReturn 
IOUSBPipe::Write(IOMemoryDescriptor *buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount, IOUSBCompletion *completion)
{
    IOReturn	err = kIOReturnSuccess;

    USBLog(7, "IOUSBPipe[%p]::Write #3 - reqCount = %d", this, reqCount);
    if ((_endpoint.transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
	return kIOReturnBadArgument;

    if (!buffer || (buffer->getLength() < reqCount))
	return kIOReturnBadArgument;

    if (_correctStatus == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Write - invalid write on a stalled pipe", this);
        return kIOUSBPipeStalled;
    }

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

        syncer  = IOSyncer::create();

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = NULL;

        err = _controller->Write(buffer, _address, &_endpoint, &tap, noDataTimeout, completionTimeout);
        if (err == kIOReturnSuccess) 
	{
            err = syncer->wait();
            // any err coming back in the callback indicates a stalled pipe
            if (err && (err != kIOUSBTransactionTimeout))
            {
                USBLog(2, "IOUSBPipe[%p]::Write  - i/o err (0x%x) on sync call - stalling pipe", this, err);
                _correctStatus = kIOUSBPipeStalled;
            }
        }
        else 
	{
            syncer->release(); syncer->release();
        }
    }
    else
    {
        err = _controller->Write(buffer, _address, &_endpoint, completion, noDataTimeout, completionTimeout, reqCount);
    }

    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Write - controller returned stalled pipe, changing status");
        _correctStatus = kIOUSBPipeStalled;
    }

    return(err);
}

// 5-16-02 JRH
// This is the "original" GetStatus call, which returned the wrong sized return code, and which always returned zero
// anyway, making it rather stupid. I am leaving in the zero return for backwards compatibility
UInt8 
IOUSBPipe::GetStatus(void)
{ 
    return(0);
}


// 5-16-02 JRH
// This is the "new" GetPipeStatus call which will attempt to return the correct status, i.e. whether the pipe is
// currently stalled or is active
OSMetaClassDefineReservedUsed(IOUSBPipe,  6);
IOReturn
IOUSBPipe::GetPipeStatus(void)
{
    return _correctStatus;
}

OSMetaClassDefineReservedUsed(IOUSBPipe,  7);
IOReturn 
IOUSBPipe::ClearPipeStall(bool withDeviceRequest)
{
    IOReturn	err;
    
    USBLog(5,"IOUSBPipe[%p]::ClearPipeStall",this);
    if (_correctStatus != 0)
        USBLog(2, "IOUSBPipe[%p]::ClearPipeStall setting status to 0");
    _correctStatus = 0;
    err = _controller->ClearPipeStall(_address, &_endpoint);
    if (!err && withDeviceRequest)
    {
	IOUSBDevRequest	request;
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

	USBLog(2,"IOUSBPipe[%p]::ClearPipeStall - sending request to the device", this);
	request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBEndpoint);
	request.bRequest = kUSBRqClearFeature;
	request.wValue = kUSBFeatureEndpointStall;
	request.wIndex = _endpoint.number | ((_endpoint.direction == kUSBIn) ? 0x80 : 0);
	request.wLenDone = request.wLength = 0;
	request.pData = NULL;

        syncer  = IOSyncer::create();

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = &request.wLenDone;

	// send the request to pipe zero
	err = _controller->DeviceRequest(&request, &tap, _address, 0, kUSBDefaultControlNoDataTimeoutMS, kUSBDefaultControlCompletionTimeoutMS);
        if (err == kIOReturnSuccess) 
	{
            err = syncer->wait();
        }
	else 
	{
            syncer->release(); syncer->release();
	}
    }
    return err;
}


OSMetaClassDefineReservedUsed(IOUSBPipe,  8);
IOReturn
IOUSBPipe::SetPipePolicy(UInt16 maxPacketSize, UInt8 maxInterval)
{
    IOReturn 	err = kIOReturnSuccess;
    
    switch (_endpoint.transferType)
    {
	case kUSBIsoc:
	    if (maxPacketSize <= USBToHostWord(_descriptor->wMaxPacketSize))
	    {
		UInt16 oldsize = _endpoint.maxPacketSize;
		USBLog(2, "IOUSBPipe[%p]::SetPipePolicy - trying to change isoch pipe from %d to %d bytes", this, oldsize, maxPacketSize);
		_endpoint.maxPacketSize = maxPacketSize;
		// OpenPipe with Isoch pipes which already exist will try to change the maxpacketSize in the pipe.
		// the speed param below is actually unused for Isoc pipes
		err = _controller->OpenPipe(_address, kUSBDeviceSpeedFull, &_endpoint);
		if (err)
		{
		    USBLog(2, "IOUSBPipe[%p]::SetPipePolicy - new OpenPipe failed - size remaining the same [err = %x]", this, err);
		    _endpoint.maxPacketSize = oldsize;
		}
	    }
	    else
	    {
		USBLog(2, "IOUSBPipe[%p]::SetPipePolicy - requested size (%d) larger than maxPacketSize in descriptor (%d)", this, maxPacketSize, USBToHostWord(_descriptor->wMaxPacketSize));
		err = kIOReturnBadArgument;
	    }
	    break;
	    
	case kUSBInterrupt:
	    USBLog(2, "IOUSBPipe[%p]::SetPipePolicy - interrupt pipe - gracefully ignoring", this);
	    break;
	    
	default:
	    USBLog(2, "IOUSBPipe[%p]::SetPipePolicy - wrong type of pipe - returning kIOReturnBadArgument", this);
	    err = kIOReturnBadArgument;
    }
    return err;
}


OSMetaClassDefineReservedUsed(IOUSBPipe,  9);

// Isochronous read with frame list updated at hardware interrupt time
//
IOReturn 
IOUSBPipe::Read( IOMemoryDescriptor *	buffer,
                 UInt64 frameStart, UInt32 numFrames, IOUSBLowLatencyIsocFrame *pFrames,
                          IOUSBLowLatencyIsocCompletion *	completion, UInt32 updateFrequency)
{
    IOReturn	err = kIOReturnSuccess;

    USBLog(7, "IOUSBPipe[%p]::Read (Low Latency Isoc) buffer: %p, completion: %p, numFrames: %ld, update: %ld", this, buffer, completion, numFrames, updateFrequency);
    if (_correctStatus == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Read (Low Latency Isoc) - invalid read on a stalled low latency isoch pipe", this);
        return kIOUSBPipeStalled;
    }

    if (_endpoint.maxPacketSize == 0)
    {
        USBLog(2, "IOUSBPipe[%p]::Read (Low Latency Isoc) - no bandwidth on an low latency isoc pipe", this);
        return kIOReturnNoBandwidth;
    }

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        //
        IOUSBLowLatencyIsocCompletion	tap;
        IOSyncer *		syncer;

        syncer  = IOSyncer::create();

        tap.target = (void *)syncer;
        tap.action = (IOUSBLowLatencyIsocCompletionAction) &IOUSBSyncIsoCompletion;
        tap.parameter = NULL;

        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, &tap, updateFrequency);

        if (err == kIOReturnSuccess) {
            err = syncer->wait();

            // any err coming back in the callback indicates a stalled pipe
            //
            if (err && (err != kIOUSBTransactionTimeout))
            {
                USBLog(2, "IOUSBPipe[%p]::Read (Low Latency Isoc)  - i/o err (0x%x) on sync call - stalling pipe", this, err);
                _correctStatus = kIOUSBPipeStalled;
            }
        }
        else {
            syncer->release(); syncer->release();
	}
    }
    else {
        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, completion, updateFrequency);
    }

    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Read (Low Latency Isoc) - controller returned stalled pipe, changing status");
        _correctStatus = kIOUSBPipeStalled;
    }

    return err;
}                          


OSMetaClassDefineReservedUsed(IOUSBPipe,  10);
IOReturn 
IOUSBPipe::Write(IOMemoryDescriptor * buffer, UInt64 frameStart, UInt32 numFrames, IOUSBLowLatencyIsocFrame *pFrames, IOUSBLowLatencyIsocCompletion *completion, UInt32 updateFrequency)
{
    IOReturn	err = kIOReturnSuccess;

    USBLog(7, "IOUSBPipe[%p]::Write (Low Latency Isoc) buffer: %p, completion: %p, numFrames: %ld, update: %ld", this, buffer, completion, numFrames, updateFrequency);
    if (_correctStatus == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Write (Low Latency Isoc) - invalid write on a stalled isoch pipe", this);
        return kIOUSBPipeStalled;
    }

    if (_endpoint.maxPacketSize == 0)
    {
        USBLog(2, "IOUSBPipe[%p]::Write (Low Latency Isoc) - no bandwidth on an isoc pipe", this);
        return kIOReturnNoBandwidth;
    }

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBLowLatencyIsocCompletion	tap;
        IOSyncer *		syncer;

        syncer  = IOSyncer::create();

        tap.target = syncer;
        tap.action = (IOUSBLowLatencyIsocCompletionAction)&IOUSBSyncIsoCompletion;
        tap.parameter = NULL;

        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, &tap, updateFrequency);

        if (err == kIOReturnSuccess)
        {
            err = syncer->wait();
            // any err coming back in the callback indicates a stalled pipe
            if (err && (err != kIOUSBTransactionTimeout))
            {
                USBLog(2, "IOUSBPipe[%p]::Write  (Low Latency Isoc) - i/o err (0x%x) on sync call - stalling pipe", this, err);
                _correctStatus = kIOUSBPipeStalled;
            }
        }
        else {
            syncer->release(); syncer->release();
        }
    }
    else
    {
        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, completion, updateFrequency);

    }

    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Write  (Low Latency Isoc) - controller returned stalled pipe, changing status");
        _correctStatus = kIOUSBPipeStalled;
    }

    return err;
}



OSMetaClassDefineReservedUnused(IOUSBPipe,  11);
OSMetaClassDefineReservedUnused(IOUSBPipe,  12);
OSMetaClassDefineReservedUnused(IOUSBPipe,  13);
OSMetaClassDefineReservedUnused(IOUSBPipe,  14);
OSMetaClassDefineReservedUnused(IOUSBPipe,  15);
OSMetaClassDefineReservedUnused(IOUSBPipe,  16);
OSMetaClassDefineReservedUnused(IOUSBPipe,  17);
OSMetaClassDefineReservedUnused(IOUSBPipe,  18);
OSMetaClassDefineReservedUnused(IOUSBPipe,  19);

