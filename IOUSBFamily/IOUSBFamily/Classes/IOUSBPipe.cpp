/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <libkern/OSByteOrder.h>

#include <IOKit/IOService.h>
#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/USBHub.h>

#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBLog.h>

#include "IOUSBInterfaceUserClient.h"

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super OSObject

#ifndef kIOUserClientCrossEndianCompatibleKey
#define kIOUserClientCrossEndianCompatibleKey "IOUserClientCrossEndianCompatible"
#endif

#define	_device							_expansionData->_device
#define	_correctStatus					_expansionData->_correctStatus
#define	_speed							_expansionData->_speed
#define	_interface						_expansionData->_interface
#define	_crossEndianCompatible			_expansionData->_crossEndianCompatible

// Note:  We are overloading the use of the _status iVar -- was obsoleted, but now use it to signify that
// we should accept an illegal MPS.  We did not create a new ivar in the expansion data because we need
// to check the property before the expansion data is allocated and we did not want to modify the params
// and create another method.  We just reused an unused ivar, but we don't change the name so as not break
// binary compatibility
#define	_OUTOFSPECMPSOK				_status				// if non-zero, then we should ignore an out of spec MPS

//================================================================================================
#ifndef IOUSBPIPE_USE_KPRINTF
	#define IOUSBPIPE_USE_KPRINTF 0
#endif

#if IOUSBPIPE_USE_KPRINTF
	#undef USBLog
	#undef USBError
	void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
	#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBPIPE_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
	#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

//================================================================================================
//
//  IOUSBPipe Methods
//
//================================================================================================
OSDefineMetaClassAndStructors(IOUSBPipe, OSObject)


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
    _endpoint.maxPacketSize = mungeMaxPacketSize(USBToHostWord(ed->wMaxPacketSize));
	
    _endpoint.interval = ed->bInterval;
    _address = address;
    _speed = speed;
	
	// Bring in workaround from the EHCI UIM for Bulk pipes that are high speed but report a MPS of 64:
    if ( (_speed == kUSBDeviceSpeedHigh) && (_endpoint.transferType == kUSBBulk) && (_endpoint.maxPacketSize != 512) ) 
    {	
		if ( _OUTOFSPECMPSOK == 0 )
		{
			// This shouldn't happen any more, this has been fixed.
			USBError(1, "IOUSBPipe[%p]::InitToEndpoint: USB 2.0 Spec (5.8.3) converting Bulk MPS from %d to 512", this, _endpoint.maxPacketSize);
			_endpoint.maxPacketSize = 512;
		}
		else
		{
			USBLog(5, "IOUSBPipe[%p]::InitToEndpoint: High Speed Bulk pipe with %d MPS, but property says we should still use it.", this, _endpoint.maxPacketSize);
		}
    }
	
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
	// Deprecated method
    USBLog(1, "IOUSBPipe::ToEndpoint, obsolete method 1 called");
    return NULL;
}



IOUSBPipe *
IOUSBPipe::ToEndpoint(const IOUSBEndpointDescriptor *ed, IOUSBDevice * device, IOUSBController *controller)
{
	// Deprecated method
    USBLog(1, "IOUSBPipe::ToEndpoint, obsolete method 2 called");
    return NULL;
}



IOUSBPipe *
IOUSBPipe::ToEndpoint(const IOUSBEndpointDescriptor *ed, IOUSBDevice * device, IOUSBController *controller, IOUSBInterface * interface)
{
	OSObject *	propertyObj = NULL;
	OSBoolean * boolObj = NULL;
    
	IOUSBPipe *	me = new IOUSBPipe;
   
	if ( me == NULL )
	{
		USBLog(1, "IOUSBPipe::ToEndpoint  could not allocate IOUSBPIPE object for device %p, interface %p", device, interface);
		return NULL;
	}
	
	USBLog(6, "IOUSBPipe[%p]::ToEndpoint device %p, interface %p", me, device, interface);
	
	if ( me && interface)
	{
		// If our interface has the CrossEndianCompatible property, set our boolean
		propertyObj = interface->copyProperty(kUSBOutOfSpecMPSOK);
		boolObj = OSDynamicCast( OSBoolean, propertyObj );
		if ( boolObj && boolObj->isTrue())
		{
			USBLog(6,"IOUSBPipe[%p]::ToEndpoint Device reports Out of spec MPS property and is TRUE", me);
			me->_OUTOFSPECMPSOK = 0xFF;
		}
		
		if (propertyObj)
			propertyObj->release();
	}

  if ( !me->InitToEndpoint(ed, device->GetSpeed(), device->GetAddress(), controller) ) 
    {
        me->release();
        return NULL;
    }

	me->_device = device;
	me->_interface = interface;

	if ( me->_interface )
    {
        // If our interface has the CrossEndianCompatible property, set our boolean
		OSObject * propertyObj = me->_interface->copyProperty(kIOUserClientCrossEndianCompatibleKey);
        OSBoolean * boolObj = OSDynamicCast( OSBoolean, propertyObj );
        if ( boolObj )
		{
			if (boolObj->isTrue() )
			{
				USBLog(6,"IOUSBPipe[%p]::ToEndpoint CrossEndianProperty exists and is TRUE", me);
				me->_crossEndianCompatible = true;
			}
			else
			{
				USBLog(6,"IOUSBPipe[%p]::ToEndpoint CrossEndianProperty exists and is FALSE", me);
				me->_crossEndianCompatible = false;
			}
		}
		if (propertyObj)
			propertyObj->release();
    }
	
    return me;
}



void 
IOUSBPipe::free()
{

    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_expansionData)
    {
        IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }

    super::free();
}



// Controlling pipe state

IOReturn 
IOUSBPipe::Abort(void)
{
    USBLog(5,"IOUSBPipe[%p]::AbortPipe",this);
    if (_correctStatus != 0)
	{
        USBLog(2, "IOUSBPipe[%p]::Abort setting status to 0", this);
	}
    _correctStatus = 0;
    return _controller->AbortPipe(_address, &_endpoint);
}



IOReturn 
IOUSBPipe::Reset(void)
{
    USBLog(5,"+IOUSBPipe[%p]::ResetPipe",this);
    if (_correctStatus != 0)
	{
        USBLog(2, "IOUSBPipe[%p]::ResetPipe setting status to 0", this);
	}
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

	// The following is a hack to tell the UIM that this request is coming from a Rosetta client.  We set the high bit of the endpoint transfer type here
	// and in IsocIO we will clear it and set a flag in the IOUSBCommand
	if ( _crossEndianCompatible )
		_endpoint.direction |= 0x80;

    if (completion == NULL)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBIsocCompletion	tap;

        // The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
        //
        tap.target = NULL;
        tap.action = &IOUSBSyncIsoCompletion;
        tap.parameter = NULL;

		USBLog(3, "IOUSBPipe[%p]::Read Sync (Isoc) completion: %p", this, tap.action);
		err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, &tap);

    }
    else
    {
		if (completion->action == NULL)
		{
			USBLog(1, "IOUSBPipe[%p]::Read - completion has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			return kIOReturnBadArgument;
		}
		err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, completion);
    }

    return err;
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

	// The following is a hack to tell the UIM that this request is coming from a Rosetta client.  We set the high bit of the endpoint transfer type here
	// and in IsocIO we will clear it and set a flag in the IOUSBCommand
	if ( _crossEndianCompatible )
		_endpoint.direction |= 0x80;

    if (completion == NULL)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBIsocCompletion	tap;

        // The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
        //
        tap.target = NULL;
        tap.action = &IOUSBSyncIsoCompletion;
        tap.parameter = NULL;

        USBLog(3, "IOUSBPipe[%p]::Write Sync (Isoc) completion: %p", this, tap.action);
        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, &tap);

    }
    else
    {
		if (completion->action == NULL)
		{
			USBLog(1, "IOUSBPipe[%p]::Write - completion has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			return kIOReturnBadArgument;
		}
        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, completion);
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
    {
        USBLog(5, "IOUSBPipe[%p]::Read - NULL buffer!", this);
		return kIOReturnBadArgument;
    }
    
    return Read(buffer, noDataTimeout, completionTimeout, buffer->getLength(), completion, bytesRead);
}
	


OSMetaClassDefineReservedUsed(IOUSBPipe,  1);
IOReturn 
IOUSBPipe::Write(IOMemoryDescriptor *buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
    USBLog(7, "IOUSBPipe[%p]::Write #2", this);
    // Validate that there is a buffer so that we can call getLength on it
    if (!buffer)
    {
        USBLog(5, "IOUSBPipe[%p]::Write - NULL buffer!", this);
        return kIOReturnBadArgument;
    }
    
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

    if (completion == NULL)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;

        request->wLenDone = request->wLength;

		// The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
		//
        tap.target = NULL;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = &request->wLenDone;

        err = _controller->DeviceRequest(request, &tap, _address, _endpoint.number, noDataTimeout, completionTimeout);

 }
    else
    {
		if (completion->action == NULL)
		{
			USBLog(1, "IOUSBPipe[%p]::ControlRequest - completion has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			return kIOReturnBadArgument;
		}
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

    if (completion == NULL)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;

        request->wLenDone = request->wLength;

		// The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
		//
        tap.target = NULL;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = &request->wLenDone;

        err = _controller->DeviceRequest(request, &tap, _address, _endpoint.number, noDataTimeout, completionTimeout);

    }
    else
    {
		if (completion->action == NULL)
		{
			USBLog(1, "IOUSBPipe[%p]::ControlRequest - completion has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			return kIOReturnBadArgument;
		}
        err = _controller->DeviceRequest(request, completion, _address, _endpoint.number, noDataTimeout, completionTimeout);
    }

    return(err);
}


OSMetaClassDefineReservedUsed(IOUSBPipe,  4);
IOReturn 
IOUSBPipe::Read(IOMemoryDescriptor *buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount, IOUSBCompletion *completion, IOByteCount *bytesRead)
{
    IOReturn	err = kIOReturnSuccess;

    USBLog(7, "IOUSBPipe[%p]::Read #3 (addr %d:%d type %d) - reqCount = %qd", this, _address, _endpoint.number , _endpoint.transferType, (uint64_t)reqCount);
    if ((_endpoint.transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
    {
        USBLog(5, "IOUSBPipe[%p]::Read - bad arguments:  (EP type: %d != kUSBBulk(%d)) && ( dataTimeout: %d || completionTimeout: %d)", this, _endpoint.transferType, kUSBBulk, (uint32_t)noDataTimeout, (uint32_t)completionTimeout);
		return kIOReturnBadArgument;
    }

    if (!buffer || (buffer->getLength() < reqCount))
    {
        USBLog(5, "IOUSBPipe[%p]::Write - bad buffer: (buffer %p) || ( length %qd < reqCount %qd)", this, buffer, buffer ? (uint64_t)buffer->getLength() : 0, (uint64_t)reqCount);
        return kIOReturnBadArgument;
    }

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

        if (bytesRead)
            *bytesRead = reqCount;

		// The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
		//
        tap.target = NULL;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = bytesRead;

        err = _controller->Read(buffer, _address, &_endpoint, &tap, noDataTimeout, completionTimeout, reqCount);
        if (err != kIOReturnSuccess)
        {
            // any err coming back in the callback indicates a stalled pipe
            if (err && (err != kIOUSBTransactionTimeout))
            {
                USBLog(2, "IOUSBPipe[%p]::Read  - i/o err (0x%x) on sync call - stalling pipe", this, err);
                _correctStatus = kIOUSBPipeStalled;
            }
        }
    }
    else
    {
		if (completion->action == NULL)
		{
			USBLog(1, "IOUSBPipe[%p]::Read - completion has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			return kIOReturnBadArgument;
		}
        err = _controller->Read(buffer, _address, &_endpoint, completion, noDataTimeout, completionTimeout, reqCount);
    }

    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Read  - controller returned stalled pipe, changing status", this);
        _correctStatus = kIOUSBPipeStalled;
    }

    return(err);
}



OSMetaClassDefineReservedUsed(IOUSBPipe,  5);
IOReturn 
IOUSBPipe::Write(IOMemoryDescriptor *buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount, IOUSBCompletion *completion)
{
    IOReturn	err = kIOReturnSuccess;

    USBLog(7, "IOUSBPipe[%p]::Write #3 (addr %d:%d type %d) - reqCount = %qd", this, _address, _endpoint.number , _endpoint.transferType, (uint64_t)reqCount);
    if ((_endpoint.transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
    {
        USBLog(5, "IOUSBPipe[%p]::Write - bad arguments:  (EP type: %d != kUSBBulk(%d)) && ( dataTimeout: %d || completionTimeout: %d)", this, _endpoint.transferType, kUSBBulk, (uint32_t)noDataTimeout, (uint32_t)completionTimeout);
		return kIOReturnBadArgument;
    }

    if (!buffer || (buffer->getLength() < reqCount))
    {
        USBLog(5, "IOUSBPipe[%p]::Write - bad buffer: (buffer %p) || ( length %qd < reqCount %qd)", this, buffer, buffer ? (uint64_t)buffer->getLength() : 0, (uint64_t)reqCount);
		return kIOReturnBadArgument;
    }

    if (_correctStatus == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Write - invalid write on a stalled pipe", this);
        return kIOUSBPipeStalled;
    }

    if (completion == NULL)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;

        // The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
        //
        tap.target = NULL;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = NULL;

        err = _controller->Write(buffer, _address, &_endpoint, &tap, noDataTimeout, completionTimeout, reqCount);
        if (err != kIOReturnSuccess)
        {
            // any err coming back in the callback indicates a stalled pipe
            if (err && (err != kIOUSBTransactionTimeout))
            {
                USBLog(2, "IOUSBPipe[%p]::Write  - i/o err (0x%x) on sync call - stalling pipe", this, err);
                _correctStatus = kIOUSBPipeStalled;
            }
        }
    }
    else
    {
		if (completion->action == NULL)
		{
			USBLog(1, "IOUSBPipe[%p]::Write - completion has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			return kIOReturnBadArgument;
		}
        err = _controller->Write(buffer, _address, &_endpoint, completion, noDataTimeout, completionTimeout, reqCount);
    }

    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Write - controller returned stalled pipe, changing status", this);
        _correctStatus = kIOUSBPipeStalled;
    }

    return err;
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
	{
        USBLog(2, "IOUSBPipe[%p]::ClearPipeStall setting status to 0", this);
	}

    _correctStatus = 0;
    
    if ( _endpoint.transferType == kUSBIsoc )
    {
        USBLog(2, "IOUSBPipe[%p]::ClearPipeStall Isoch pipes never stall.  Returning success", this);
        return kIOReturnSuccess;
    }
    
    err = _controller->ClearPipeStall(_address, &_endpoint);

    if (_device->GetSpeed() == kUSBDeviceSpeedHigh)
    {
		USBLog(5,"IOUSBPipe[%p]::ClearPipeStall - High Speed Device, no clear TT needed",this);
    }
    else if(!err && ( (_endpoint.transferType == kUSBBulk) || (_endpoint.transferType == kUSBControl) ) )
    {
		USBLog(5,"IOUSBPipe[%p]::ClearPipeStall Bulk or Control endpoint, clear TT",this);
		// Now, we need to tell our parent to issue a port reset to our port.  Our parent is an IOUSBDevice
		// that has a hub driver attached to it.  However, we don't know what kind of driver that is, so we
		// just send a message to all the clients of our parent.  The hub driver will be the only one that
		// should do anything with that message.
		//
		if( (_device != NULL) && (_device->_expansionData->_usbPlaneParent ) )
		{
			IOUSBHubPortClearTTParam	params;
			UInt8						deviceAddress;		//<<0
			UInt8						endpointNum;		//<<8
			UInt8						endpointType;		//<<16 // As split transaction. 00 Control, 10 Bulk
			UInt8						in;					//<<24 // Direction, 1 = IN, 0 = OUT};
		
			params.portNumber = _device->_expansionData->_portNumber;
			deviceAddress = _device->GetAddress();
			endpointNum = _endpoint.number;
			if(_endpoint.transferType == kUSBControl)
			{
				endpointType = 0;	// As split transaction. 00 Control, 10 Bulk
				in = 0;				// Direction, 1 = IN, 0 = OUT, not used for control
			}
			else
			{
				endpointType = 2;		// As split transaction. 00 Control, 10 Bulk
				if(_endpoint.direction == kUSBIn)
				{
					in = 1;			// Direction, 1 = IN, 0 = OUT, not used for control
				}
				else
				{
					in = 0;
				}
			}
			params.options = deviceAddress + (endpointNum <<8) + (endpointType << 16) + (in << 24);
			
			USBLog(6, "IOUSBPipe[%p]::ClearPipeStall  calling device messageClients (kIOUSBMessageHubPortClearTT) with options: 0x%x", this, (uint32_t)params.options);
			_device->_expansionData->_usbPlaneParent->retain();
			(void) _device->_expansionData->_usbPlaneParent->messageClients(kIOUSBMessageHubPortClearTT, &params, sizeof(params));
			_device->_expansionData->_usbPlaneParent->release();
		}
    }
    else
    {
		USBLog(5,"IOUSBPipe[%p]::ClearPipeStall Int or Isoc endpoint, don't clear TT (or err: 0x%x)",this, err);
    }
    
    if (!err && withDeviceRequest)
    {
		IOUSBDevRequest	request;
        IOUSBCompletion	tap;

		// The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
		//
        tap.target = NULL;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = &request.wLenDone;

        USBLog(7,"IOUSBPipe[%p]::ClearPipeStall - sending request to the device", this);
		request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBEndpoint);
		request.bRequest = kUSBRqClearFeature;
		request.wValue = kUSBFeatureEndpointStall;
		request.wIndex = _endpoint.number | ((_endpoint.direction == kUSBIn) ? 0x80 : 0);
		request.wLenDone = request.wLength = 0;
		request.pData = NULL;

		// send the request to pipe zero
		err = _controller->DeviceRequest(&request, &tap, _address, 0, kUSBDefaultControlNoDataTimeoutMS, kUSBDefaultControlCompletionTimeoutMS);

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
			if (maxPacketSize <= mungeMaxPacketSize(USBToHostWord(_descriptor->wMaxPacketSize)))
			{
				UInt16 oldsize = _endpoint.maxPacketSize;
				USBLog(6, "IOUSBPipe[%p]::SetPipePolicy - trying to change isoch pipe from %d to %d bytes", this, oldsize, maxPacketSize);
				_endpoint.maxPacketSize = maxPacketSize;
				// OpenPipe with Isoch pipes which already exist will try to change the maxpacketSize in the pipe.
				// the speed param below is actually unused for Isoc pipes
				err = _controller->OpenPipe(_address, _speed, &_endpoint);
				if (err)
				{
					USBLog(2, "IOUSBPipe[%p]::SetPipePolicy - new OpenPipe failed - size remaining the same [err = %x]", this, err);
					_endpoint.maxPacketSize = oldsize;
				}
			}
			else
			{
				USBLog(2, "IOUSBPipe[%p]::SetPipePolicy - requested size (%d) larger than maxPacketSize in descriptor (%d)", this, maxPacketSize, mungeMaxPacketSize(USBToHostWord(_descriptor->wMaxPacketSize)));
				err = kIOReturnBadArgument;
			}
			break;
			
		case kUSBInterrupt:
            if ( maxInterval != _endpoint.interval )
            {    
                UInt8   oldInterval = _endpoint.interval;
                USBLog(2, "IOUSBPipe[%p]::SetPipePolicy - trying to change interrupt interval from %d to %d", this, oldInterval, maxInterval);
                _endpoint.interval = maxInterval;
				err = _controller->OpenPipe(_address, _speed, &_endpoint);
				if (err)
				{
					USBLog(2, "IOUSBPipe[%p]::SetPipePolicy - changing maxInterval failed. [err = %x]", this, err);
					_endpoint.interval = oldInterval;
				}
            }
			else
			{
				USBLog(2, "IOUSBPipe[%p]::SetPipePolicy - requested maxInterval size is the same as before", this);
			}
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
IOUSBPipe::Read(IOMemoryDescriptor *	buffer,
				UInt64 frameStart, UInt32 numFrames, IOUSBLowLatencyIsocFrame *pFrames,
				IOUSBLowLatencyIsocCompletion *	completion, UInt32 updateFrequency)
{
    IOReturn	err = kIOReturnSuccess;

    USBLog(7, "IOUSBPipe[%p]::Read (Low Latency Isoc) buffer: %p, completion: %p, numFrames: %d, update: %d", this, buffer, completion, (uint32_t)numFrames, (uint32_t)updateFrequency);
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

	// The following is a hack to tell the UIM that this request is coming from a Rosetta client.  We set the high bit of the endpoint transfer type here
	// and in IsocIO we will clear it and set a flag in the IOUSBCommand
	if ( _crossEndianCompatible )
		_endpoint.direction |= 0x80;
	
    if (completion == NULL)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        //
        IOUSBLowLatencyIsocCompletion	tap;

        // The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
        //
        tap.target = NULL;
        tap.action = (IOUSBLowLatencyIsocCompletionAction) &IOUSBSyncIsoCompletion;
        tap.parameter = NULL;

        USBLog(3, "IOUSBPipe[%p]::Read Sync (Low Latency Isoc) completion: %p", this, tap.action);
        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, &tap, updateFrequency);
    }
    else
    {
		if (completion->action == NULL)
		{
			USBLog(1, "IOUSBPipe[%p]::Read - completion has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			return kIOReturnBadArgument;
		}
        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, completion, updateFrequency);
    }

    return err;
}                          


OSMetaClassDefineReservedUsed(IOUSBPipe,  10);
IOReturn 
IOUSBPipe::Write(IOMemoryDescriptor * buffer, UInt64 frameStart, UInt32 numFrames, IOUSBLowLatencyIsocFrame *pFrames, IOUSBLowLatencyIsocCompletion *completion, UInt32 updateFrequency)
{
    IOReturn	err = kIOReturnSuccess;

    USBLog(7, "IOUSBPipe[%p]::Write (Low Latency Isoc) buffer: %p, completion: %p, numFrames: %d, update: %d", this, buffer, completion, (uint32_t)numFrames, (uint32_t)updateFrequency);
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

	// The following is a hack to tell the UIM that this request is coming from a Rosetta client.  We set the high bit of the endpoint transfer type here
	// and in IsocIO we will clear it and set a flag in the IOUSBCommand
	if ( _crossEndianCompatible )
		_endpoint.direction |= 0x80;

    if (completion == NULL)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBLowLatencyIsocCompletion	tap;

		// The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
		//
        tap.target = NULL;
        tap.action = (IOUSBLowLatencyIsocCompletionAction)&IOUSBSyncIsoCompletion;
        tap.parameter = NULL;

        USBLog(3, "IOUSBPipe[%p]::Write Sync (Low Latency Isoc) completion: %p", this, tap.action);
        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, &tap, updateFrequency);
    }
    else
    {
		if (completion->action == NULL)
		{
			USBLog(1, "IOUSBPipe[%p]::Write - completion has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			return kIOReturnBadArgument;
		}
        err = _controller->IsocIO(buffer, frameStart, numFrames, pFrames, _address, &_endpoint, completion, updateFrequency);
    }

    return err;
}



OSMetaClassDefineReservedUsed(IOUSBPipe,  11);
IOReturn
IOUSBPipe::Read(IOMemoryDescriptor *buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount, IOUSBCompletionWithTimeStamp *completionWithTimeStamp, IOByteCount *bytesRead)
{
    IOReturn                err = kIOReturnSuccess;
    IOUSBControllerV2  *    controllerV2;
    
    controllerV2 = OSDynamicCast(IOUSBControllerV2, _controller);
    if ( controllerV2 == NULL )
    {
        USBLog(2,"IOUSBPipe[%p]:Read #4 -- Requested a Read with time stamp, but this IOUSBController does not support it", this);
        return kIOReturnUnsupported;
    }
    
    USBLog(7, "IOUSBPipe[%p]::Read #4 (addr %d:%d type %d) - reqCount = %qd", this, _address, _endpoint.number , _endpoint.transferType, (uint64_t)reqCount);
    if ((_endpoint.transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
    {
        USBLog(5, "IOUSBPipe[%p]::Read #4 - bad arguments:  (EP type: %d != kUSBBulk(%d)) && ( dataTimeout: %d || completionTimeout: %d)", this, _endpoint.transferType, kUSBBulk, (uint32_t)noDataTimeout, (uint32_t)completionTimeout);
        return kIOReturnBadArgument;
    }
    
    if (!buffer || (buffer->getLength() < reqCount))
    {
        USBLog(5, "IOUSBPipe[%p]::Read #4- bad buffer: (buffer %p) || ( length %qd < reqCount %qd)", this, buffer, buffer ? (uint64_t)buffer->getLength() : 0, (uint64_t)reqCount);
        return kIOReturnBadArgument;
    }

    if (_correctStatus == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Read #4 - invalid read on a stalled pipe", this);
        return kIOUSBPipeStalled;
    }

    if (completionWithTimeStamp == NULL)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;

        if (bytesRead)
            *bytesRead = reqCount;

        // The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
        //
        tap.target = NULL;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = bytesRead;

        err = controllerV2->ReadV2(buffer, _address, &_endpoint, (IOUSBCompletionWithTimeStamp *) &tap, noDataTimeout, completionTimeout, reqCount);
        if (err != kIOReturnSuccess)
        {
            // any err coming back in the callback indicates a stalled pipe
            if (err && (err != kIOUSBTransactionTimeout))
            {
                USBLog(2, "IOUSBPipe[%p]::Read  - i/o err (0x%x) on sync call - stalling pipe", this, err);
                _correctStatus = kIOUSBPipeStalled;
            }
        }
    }
    else
    {
		if (completionWithTimeStamp->action == NULL)
		{
			USBLog(1, "IOUSBPipe[%p]::Read - completionWithTimeStamp has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			return kIOReturnBadArgument;
		}
        err = controllerV2->ReadV2(buffer, _address, &_endpoint, completionWithTimeStamp, noDataTimeout, completionTimeout, reqCount);
    }

    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipe[%p]::Read  - controller returned stalled pipe, changing status", this);
        _correctStatus = kIOUSBPipeStalled;
    }

    return(err);
}



OSMetaClassDefineReservedUnused(IOUSBPipe,  12);
OSMetaClassDefineReservedUnused(IOUSBPipe,  13);
OSMetaClassDefineReservedUnused(IOUSBPipe,  14);
OSMetaClassDefineReservedUnused(IOUSBPipe,  15);
OSMetaClassDefineReservedUnused(IOUSBPipe,  16);
OSMetaClassDefineReservedUnused(IOUSBPipe,  17);
OSMetaClassDefineReservedUnused(IOUSBPipe,  18);
OSMetaClassDefineReservedUnused(IOUSBPipe,  19);

