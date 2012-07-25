/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright � 1998-2009 Apple Inc.  All rights reserved.
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
#include <libkern/OSByteOrder.h>

#include <IOKit/IOService.h>
#include <IOKit/IOKitKeys.h>


#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/usb/USBHub.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBPipeV2.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBLog.h>

#include "IOUSBInterfaceUserClient.h"

#include "USBTracepoints.h"

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUSBPipeV2

//  These are from our superclass, but we need to reference them here
#define	_DEVICE							_expansionData->_device
#define	_CORRECTSTATUS					_expansionData->_correctStatus
#define	_SPEED							_expansionData->_speed
#define	_INTERFACE						_expansionData->_interface
#define	_CROSSENDIANCOMPATIBLE			_expansionData->_crossEndianCompatible
#define	_LOCATIONID						_expansionData->_locationID
#define	_OUTOFSPECMPSOK					_status				// if non-zero, then we should ignore an out of spec MPS


#ifndef IOUSBPipe_USE_KPRINTF
	#define IOUSBPIPE_USE_KPRINTF 0
#endif

#if IOUSBPipe_USE_KPRINTF
	#undef USBLog
	#undef USBError
	void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
	#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBPipe_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
	#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

//================================================================================================
//
//  IOUSBPipeV2 Methods
//
//================================================================================================
OSDefineMetaClassAndStructors(IOUSBPipeV2, IOUSBPipe)

#ifdef SUPPORTS_SS_USB

#pragma mark Intializers

//================================================================================================
//
//   InitToEndpoint
//
//================================================================================================
//
bool 
IOUSBPipeV2::InitToEndpoint(const IOUSBEndpointDescriptor *ed, IOUSBSuperSpeedEndpointCompanionDescriptor *sscd, UInt8 speed, USBDeviceAddress address, IOUSBController * controller, IOUSBDevice * device, IOUSBInterface * interface)
{
    IOReturn	err;
    IOUSBControllerV3  *controllerV3;
    
	USBTrace_Start( kUSBTPipe, kTPPipeInitToEndpoint, (uintptr_t)this, (uintptr_t)ed, speed, address);
	
    if ( !super::init() || ed == 0)
        return (false);
	
    // Because this method does NOT call its superclass and it is where we allocate the expansion data, we need to allocate our superclass's expansion data here.
    if (!_expansionData)
    {
        _expansionData = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
        if (!_expansionData)
            return false;
        bzero(_expansionData, sizeof(ExpansionData));
    }
	
    _controller = controller;
    controllerV3 = OSDynamicCast(IOUSBControllerV3, _controller);
    if ( controllerV3 == NULL )
    {
		if(sscd != NULL)
		{
			USBError(1,"IOUSBPipeV2[%p]:InitToEndpoint -- Requested a pipe with stream ID, but this IOUSBController does not support it", this);
			return false;
		}
    }
	
    _descriptor = ed;
	_sscd = sscd;
    _endpoint.number = ed->bEndpointAddress & kUSBPipeIDMask;
    _endpoint.transferType = ed->bmAttributes & 0x03;
    if (_endpoint.transferType == kUSBControl)
        _endpoint.direction = kUSBAnyDirn;
    else
		_endpoint.direction = (ed->bEndpointAddress & 0x80) ? kUSBIn : kUSBOut;
    _endpoint.maxPacketSize = mungeMaxPacketSize(USBToHostWord(ed->wMaxPacketSize));
	
    _endpoint.interval = ed->bInterval;
	if (_sscd != NULL)
	{
        _maxBurst = _sscd->bMaxBurst;
		_bytesPerInterval = _sscd->wBytesPerInterval;
		
		if (_endpoint.transferType == kUSBBulk)
		{
			int max;
			max = (_sscd->bmAttributes & 0x1f);
            _configuredStreams = 0;
			if(max > 0)
			{
				UInt32 maxSupported;
				_maxStream = (1 << max)-1;	// Note -1, valid stream IDs run to max streams -1, zero is invalid
				if(_maxStream > kUSBMaxStream)
				{
					_maxStream = kUSBMaxStream;
				}
				maxSupported = controllerV3->UIMMaxSupportedStream();
				if( (_maxStream > 0) && (maxSupported == 0) )
				{
					USBLog(2,"IOUSBPipeV2[%p]:InitToEndpoint -- Requested a pipe with %d streams, but this IOUSBController does not support streams", this, (uint32_t)_maxStream);
					_maxStream = 0;
					return false;
				}
				if(_maxStream > maxSupported)
				{
					USBLog(2,"IOUSBPipeV2[%p]:InitToEndpoint -- Requested a pipe with %d streams, but this IOUSBController only supports %d streams", this, (uint32_t)_maxStream, (uint32_t)maxSupported);
					_maxStream = maxSupported;
				}
			}
			
			USBLog(3, "IOUSBPipeV2[%p]::InitToEndpoint: SuperSpeed Bulk pipe maxStreams: %d", this, (uint32_t)_maxStream);
		}
		
		if (_endpoint.transferType == kUSBControl)
        {
            _maxBurst = 0;
        }

		if (_endpoint.transferType == kUSBIsoc)
		{
			_mult = (_sscd->bmAttributes & 0x3);
		}
	}
	
    
	_address = address;
    _SPEED = speed;
	_DEVICE = device;
	_INTERFACE = interface;
	
	
	if ( _DEVICE )
	{
		OSNumber *	location = OSDynamicCast(OSNumber, _DEVICE->getProperty(kUSBDevicePropertyLocationID));
		if (location)
		{
			_LOCATIONID = location->unsigned32BitValue();
		}
	}

	USBTrace(kUSBTPipe,  kTPPipeInitToEndpoint, (uintptr_t)this, (uintptr_t)_controller, _endpoint.transferType, _endpoint.maxPacketSize);

	// Bring in workaround from the EHCI UIM for Bulk pipes that are high speed but report a MPS of 64:
    if ( (_SPEED == kUSBDeviceSpeedHigh) && (_endpoint.transferType == kUSBBulk) && (_endpoint.maxPacketSize != 512) ) 
    {	
		if ( _OUTOFSPECMPSOK == 0 )
		{
			// This shouldn't happen any more, this has been fixed.
			USBError(1, "Endpoint 0x%x of the USB device \"%s\" at location 0x%x:  converting Bulk MPS from %d to 512 (USB 2.0 Spec section 5.8.3)", ed->bEndpointAddress, _DEVICE ? _DEVICE->getName() : "Unnamed", (uint32_t)_LOCATIONID, _endpoint.maxPacketSize);
			_endpoint.maxPacketSize = 512;
		}
		else
		{
			USBLog(5, "IOUSBPipeV2[%p]::InitToEndpoint: High Speed Bulk pipe with %d MPS, but property says we should still use it.", this, _endpoint.maxPacketSize);
		}
    }
	
    if( (_maxStream == 0) && (_maxBurst == 0) )
    {
		err = _controller->OpenPipe(_address, speed, &_endpoint);
	}
	else
	{
		err = controllerV3->OpenPipe(_address, speed, &_endpoint, _maxStream, _maxBurst);
	}

    
    if ((err == kIOReturnNoBandwidth) && (_endpoint.transferType == kUSBIsoc))
    {
		USBError(1,"There is not enough USB isochronous bandwidth to allow the device \"%s\" at location 0x%x to function in its current configuration (requested %d bytes for endpoint 0x%x )", _DEVICE ? _DEVICE->getName() : "Unnamed", (uint32_t)_LOCATIONID, _endpoint.maxPacketSize, ed->bEndpointAddress);
		
		_endpoint.maxPacketSize = 0;
		USBLog(6, "IOUSBPipeV2[%p]::InitToEndpoint  can't get bandwidth for isoc pipe - creating 0 bandwidth pipe", this);
		err = _controller->OpenPipe(_address, speed, &_endpoint);
    }
    
    if ( err != kIOReturnSuccess)
    {
        USBLog(3,"IOUSBPipeV2[%p]::InitToEndpoint Could not create pipe for endpoint (Addr: %d, numb: %d, type: %d).  Error 0x%x", this, _address, _endpoint.number, _endpoint.transferType, err);
        return false;
    }
    
	USBTrace_End( kUSBTPipe, kTPPipeInitToEndpoint, (uintptr_t)this, 0, 0, 0);
    
    return true;
}


//================================================================================================
//
//   ToEndpoint
//
//================================================================================================
//
IOUSBPipeV2 *
IOUSBPipeV2::ToEndpoint(const IOUSBEndpointDescriptor *ed, IOUSBSuperSpeedEndpointCompanionDescriptor *sscd, IOUSBDevice * device, IOUSBController *controller, IOUSBInterface * interface)
{
	OSObject *	propertyObj = NULL;
	OSBoolean * boolObj = NULL;
    
	IOUSBPipeV2 *	me = new IOUSBPipeV2;
   
	USBLog(6, "IOUSBPipeV2[%p]::ToEndpoint device %p(%s), interface %p, ep: 0x%x,0x%x,%d,%d", me, device, device->getName(), interface, ed->bEndpointAddress, ed->bmAttributes, ed->wMaxPacketSize, ed->bInterval);
	
	if ( me && interface)
	{
		// If our interface has the CrossEndianCompatible property, set our boolean
		propertyObj = interface->copyProperty(kUSBOutOfSpecMPSOK);
		boolObj = OSDynamicCast( OSBoolean, propertyObj );
		if ( boolObj && boolObj->isTrue())
		{
			USBLog(6,"IOUSBPipeV2[%p]::ToEndpoint Device reports Out of spec MPS property and is TRUE", me);
			me->_OUTOFSPECMPSOK = 0xFF;
		}
		
		if (propertyObj)
			propertyObj->release();
	}

	if ( !me->InitToEndpoint(ed, sscd, device->GetSpeed(), device->GetAddress(), controller, device, interface) ) 
    {
        me->release();
        return NULL;
    }

	if ( me->_INTERFACE )
    {
        // If our interface has the CrossEndianCompatible property, set our boolean
		OSObject * propertyObj = me->_INTERFACE->copyProperty(kIOUserClientCrossEndianCompatibleKey);
        OSBoolean * boolObj = OSDynamicCast( OSBoolean, propertyObj );
        if ( boolObj )
		{
			if (boolObj->isTrue() )
			{
				USBLog(6,"IOUSBPipeV2[%p]::ToEndpoint CrossEndianProperty exists and is TRUE", me);
				me->_CROSSENDIANCOMPATIBLE = true;
			}
			else
			{
				USBLog(6,"IOUSBPipeV2[%p]::ToEndpoint CrossEndianProperty exists and is FALSE", me);
				me->_CROSSENDIANCOMPATIBLE = false;
			}
		}
		if (propertyObj)
			propertyObj->release();
    }
	
    return me;
}



#pragma mark IOUSBPipeV2 State

//================================================================================================
//
//   Abort
//
//================================================================================================
//Z
IOReturn 
IOUSBPipeV2::Abort(UInt32 streamID)
{
    IOUSBControllerV3  *    controllerV3;
	USBLog(5,"IOUSBPipeV2[%p]::AbortPipe(0x%x)",this, (uint32_t)streamID);
    if (_CORRECTSTATUS != 0)
	{
        USBLog(2, "IOUSBPipeV2[%p]::Abort setting status to 0", this);
	}
    _CORRECTSTATUS = 0;
    
    controllerV3 = OSDynamicCast(IOUSBControllerV3, _controller);
    if ( controllerV3 == NULL )
    {
		if(streamID != 0)
		{
			USBLog(2,"IOUSBPipeV2[%p]:Abort -- Requested an abort with stream ID, but this IOUSBController does not support it", this);
			return kIOReturnUnsupported;
		}
    }
	if(streamID == 0)
	{
        USBLog(5, "IOUSBPipeV2[%p]::Abort(streamID) - bad argument:  stream ID zero (%d)", this, (uint32_t)streamID);
        return kIOReturnBadArgument;
    }
    if(streamID != kUSBAllStreams)
    {
		if(_endpoint.transferType != kUSBBulk)
		{
			USBLog(5, "IOUSBPipeV2[%p]::Abort(streamID) - bad argument:  abort stream (%d) on non bulk pipe", this, (uint32_t)streamID);
			return kIOReturnBadArgument;
		}
		if(streamID > _maxStream)
		{
			USBLog(5, "IOUSBPipeV2[%p]::Abort(streamID) - bad argument:  stream ID greater than max (%d > %d)", this, (uint32_t)streamID, (uint32_t)_maxStream);
			return kIOReturnBadArgument;
		}
		
	}
    if ( streamID != 0 )
    {
		return controllerV3->AbortPipe(streamID, _address, &_endpoint);
	}
	else
	{
		return _controller->AbortPipe(_address, &_endpoint);
	}

}


#pragma mark Bulk Read

//================================================================================================
//
//   Read (Bulk)
//
//================================================================================================
//
IOReturn 
IOUSBPipeV2::Read(UInt32 streamID, IOMemoryDescriptor *buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount, IOUSBCompletion *completion, IOByteCount *bytesRead)
{
    IOReturn	err = kIOReturnSuccess;
	IOUSBControllerV3  *    controllerV3;
    
    controllerV3 = OSDynamicCast(IOUSBControllerV3, _controller);
    if ( controllerV3 == NULL )
    {
		if(streamID != 0)
		{
			USBLog(2,"IOUSBPipeV2[%p]:Read -- Requested a Read with stream ID, but this IOUSBController does not support it", this);
			return kIOReturnUnsupported;
		}
    }
	
	if(streamID != 0)
	{
		if(_endpoint.transferType != kUSBBulk)
		{
			USBLog(5, "IOUSBPipeV2[%p]::Read - bad arguments:  stream (%d) read on non bulk pipe", this, (uint32_t)streamID);
			return kIOReturnBadArgument;
		}
		if(streamID > _maxStream)
		{
			USBLog(5, "IOUSBPipeV2[%p]::Read - bad arguments:  stream ID greater than max (%d > %d)", this, (uint32_t)streamID, (uint32_t)_maxStream);
			return kIOReturnBadArgument;
		}
		
	}
	
    USBLog(7, "IOUSBPipeV2[%p]::Read (addr %d:%d type %d) - reqCount = %qd", this, _address, _endpoint.number , _endpoint.transferType, (uint64_t)reqCount);
	USBTrace_Start( kUSBTPipe, kTPBulkPipeRead, _address, _endpoint.number , _endpoint.transferType, reqCount );
	
    if ((_endpoint.transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
    {
        USBLog(5, "IOUSBPipeV2[%p]::Read - bad arguments:  (EP type: %d != kUSBBulk(%d)) && ( dataTimeout: %d || completionTimeout: %d)", this, _endpoint.transferType, kUSBBulk, (uint32_t)noDataTimeout, (uint32_t)completionTimeout);
		return kIOReturnBadArgument;
    }
	
    if (!buffer || (buffer->getLength() < reqCount))
    {
        USBLog(5, "IOUSBPipeV2[%p]::Write - bad buffer: (buffer %p) || ( length %qd < reqCount %qd)", this, buffer, buffer ? (uint64_t)buffer->getLength() : 0, (uint64_t)reqCount);
        return kIOReturnBadArgument;
    }
	
    if (_CORRECTSTATUS == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipeV2[%p]::Read - invalid read on a stalled pipe", this);
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
		
		if(streamID != 0)
		{
			err = controllerV3->Read(streamID, buffer, _address, &_endpoint, &tap, noDataTimeout, completionTimeout, reqCount);
		}
		else
		{
            err = _controller->Read(buffer, _address, &_endpoint, &tap, noDataTimeout, completionTimeout, reqCount);
		}
        
        if (err != kIOReturnSuccess)
        {
            // any err coming back in the callback indicates a stalled pipe
            if (err && (err != kIOUSBTransactionTimeout))
            {
				USBLog(2, "IOUSBPipe[%p]::Read(sync)  returned 0x%x (%s) - stalling pipe", this, err, USBStringFromReturn(err));
                _CORRECTSTATUS = kIOUSBPipeStalled;
            }
        }
    }
    else
    {
		if (completion->action == NULL)
		{
			USBLog(1, "IOUSBPipeV2[%p]::Read - completion has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			USBTrace(kUSBTPipe,  kTPBulkPipeRead, (uintptr_t)this, kIOReturnBadArgument, 0, 1 );
			return kIOReturnBadArgument;
		}
 		if(streamID != 0)
		{
			err = controllerV3->Read(streamID, buffer, _address, &_endpoint, completion, noDataTimeout, completionTimeout, reqCount);
		}
		else
		{
            err = _controller->Read(buffer, _address, &_endpoint, completion, noDataTimeout, completionTimeout, reqCount);
		}
    }
	
    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipeV2[%p]::Read  - controller returned stalled pipe, changing status", this);
        _CORRECTSTATUS = kIOUSBPipeStalled;
    }
	
    USBLog(7, "IOUSBPipeV2[%p]::Read (addr %d:%d type %d) - reqCount = %qd, returning: 0x%x", this, _address, _endpoint.number , _endpoint.transferType, (uint64_t)reqCount, err);
	USBTrace_End( kUSBTPipe, kTPBulkPipeRead, (uintptr_t)this, _address, _endpoint.number, err);
	
    return(err);
}


#pragma mark Bulk Write
//================================================================================================
//
//   Write (Bulk)
//
//================================================================================================
//
IOReturn 
IOUSBPipeV2::Write(UInt32 streamID, IOMemoryDescriptor *buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount, IOUSBCompletion *completion)
{
    IOReturn	err = kIOReturnSuccess;
    IOUSBControllerV3  *    controllerV3;
    
    controllerV3 = OSDynamicCast(IOUSBControllerV3, _controller);
    if ( controllerV3 == NULL )
    {
		if(streamID != 0)
		{
			USBLog(2,"IOUSBPipeV2[%p]:write -- Requested a stream write, but this IOUSBController does not support it", this);
			return kIOReturnUnsupported;
		}
    }
	
	if(streamID != 0)
	{
		if(_endpoint.transferType != kUSBBulk)
		{
			USBLog(5, "IOUSBPipeV2[%p]::Write - bad arguments:  stream (%d) read on non bulk pipe", this, (uint32_t)streamID);
			return kIOReturnBadArgument;
		}
		if(streamID > _maxStream)
		{
			USBLog(5, "IOUSBPipeV2[%p]::Write - bad arguments:  stream ID greater than max (%d > %d)", this, (uint32_t)streamID, (uint32_t)_maxStream);
			return kIOReturnBadArgument;
		}
		
	}
    USBLog(7, "IOUSBPipeV2[%p]::Write (addr %d:%d type %d) - reqCount = %qd", this, _address, _endpoint.number , _endpoint.transferType, (uint64_t)reqCount);
	USBTrace_Start( kUSBTPipe, kTPBulkPipeWrite, _address, _endpoint.number , _endpoint.transferType, reqCount );
	
    if ((_endpoint.transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
    {
        USBLog(5, "IOUSBPipeV2[%p]::Write - bad arguments:  (EP type: %d != kUSBBulk(%d)) && ( dataTimeout: %d || completionTimeout: %d)", this, _endpoint.transferType, kUSBBulk, (uint32_t)noDataTimeout, (uint32_t)completionTimeout);
		return kIOReturnBadArgument;
    }
	
    if (!buffer || (buffer->getLength() < reqCount))
    {
        USBLog(5, "IOUSBPipeV2[%p]::Write - bad buffer: (buffer %p) || ( length %qd < reqCount %qd)", this, buffer, buffer ? (uint64_t)buffer->getLength() : 0, (uint64_t)reqCount);
		return kIOReturnBadArgument;
    }
	
    if (_CORRECTSTATUS == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipeV2[%p]::Write - invalid write on a stalled pipe", this);
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
				USBLog(2, "IOUSBPipe[%p]::Write(sync)  returned 0x%x (%s) - stalling pipe", this, err, USBStringFromReturn(err));
               _CORRECTSTATUS = kIOUSBPipeStalled;
            }
        }
    }
    else
    {
		if (completion->action == NULL)
		{
			USBLog(1, "IOUSBPipeV2[%p]::Write - completion has NULL action - returning kIOReturnBadArgument(%p)", this, (void*)kIOReturnBadArgument);
			USBTrace(kUSBTPipe,  kTPBulkPipeWrite, (uintptr_t)this, kIOReturnBadArgument, 0, 0 );
			return kIOReturnBadArgument;
		}
		if ( streamID != 0 )
		{
			err = controllerV3->Write(streamID, buffer, _address, &_endpoint, completion, noDataTimeout, completionTimeout, reqCount);
		}
		else
		{
			err = _controller->Write(buffer, _address, &_endpoint, completion, noDataTimeout, completionTimeout, reqCount);
		}

    }
	
    if (err == kIOUSBPipeStalled)
    {
        USBLog(2, "IOUSBPipeV2[%p]::Write - controller returned stalled pipe, changing status", this);
        _CORRECTSTATUS = kIOUSBPipeStalled;
    }
	
	USBTrace_End( kUSBTPipe, kTPBulkPipeWrite, (uintptr_t)this, _address, _endpoint.number, err);
	
    USBLog(7, "IOUSBPipeV2[%p]::Write (addr %d:%d type %d) - reqCount = %qd, returning 0x%x", this, _address, _endpoint.number , _endpoint.transferType, (uint64_t)reqCount, err);
	
    return err;
}

IOReturn 
IOUSBPipeV2::CreateStreams(UInt32 maxStreams)
{
    IOUSBControllerV3  *    controllerV3;
    IOReturn ret;
    
    controllerV3 = OSDynamicCast(IOUSBControllerV3, _controller);
    if ( controllerV3 == NULL )
    {
		USBLog(2,"IOUSBPipeV2[%p]:CreateStreams -- Requested stream creation, but this IOUSBController does not support it", this);
        return kIOReturnUnsupported;
    }
    ret = controllerV3->CreateStreams(_address, _endpoint.number, _endpoint.direction, maxStreams);
    if(ret == kIOReturnSuccess)
    {
        _configuredStreams = maxStreams;
    }
    return(ret);
}


#pragma mark Accessors

//================================================================================================
//
//   Accessors 
//
//================================================================================================
//
UInt32
IOUSBPipeV2::SupportsStreams(void)
{
	return(_maxStream);
}

UInt32
IOUSBPipeV2::GetConfiguredStreams(void)
{
    if(_configuredStreams != 0)
    {
        USBLog(2,"IOUSBPipeV2[%p]:GetConfiguredStreams -- fn:%d, %d, %d - Configured streams: %d", this, _address, (uint32_t)_endpoint.number, _endpoint.direction, (uint32_t)_configuredStreams);
    }
	return(_configuredStreams);
}

UInt8
IOUSBPipeV2::GetMaxBurst(void)
{
	return _maxBurst;
}

UInt8
IOUSBPipeV2::GetMult(void)
{
	return _mult;
}

UInt16
IOUSBPipeV2::GetBytesPerInterval(void)
{
	return _bytesPerInterval;
}

const IOUSBSuperSpeedEndpointCompanionDescriptor *
IOUSBPipeV2::GetSuperSpeedEndpointCompanionDescriptor()
{
	return _sscd;
}

#endif

#pragma mark Padding Slots

OSMetaClassDefineReservedUnused(IOUSBPipeV2,  0);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  1);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  2);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  3);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  4);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  5);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  6);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  7);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  8);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  9);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  10);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  11);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  12);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  13);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  14);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  15);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  16);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  17);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  18);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  19);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  20);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  21);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  22);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  23);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  24);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  25);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  26);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  27);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  28);
OSMetaClassDefineReservedUnused(IOUSBPipeV2,  29);

