/*
 * Copyright © 1998-2009 Apple Inc.  All rights reserved.
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
#define super IOUSBPipe

//  These are from our superclass, but we need to reference them here
#define	_DEVICE							_expansionData->_device
#define	_CORRECTSTATUS					_expansionData->_correctStatus
#define	_SPEED							_expansionData->_speed
#define	_INTERFACE						_expansionData->_interface
#define	_CROSSENDIANCOMPATIBLE			_expansionData->_crossEndianCompatible
#define	_LOCATIONID						_expansionData->_locationID
#define	_UASUSAGEID						_expansionData->_uasUsageID
#define	_USAGETYPE						_expansionData->_usageType
#define	_SYNCTYPE						_expansionData->_syncType
#define	_OUTOFSPECMPSOK					_status				// if non-zero, then we should ignore an out of spec MPS


#ifndef IOUSBPIPEV2_USE_KPRINTF
	#define IOUSBPIPEV2_USE_KPRINTF 0
#endif

#if IOUSBPIPEV2_USE_KPRINTF
	#undef USBLog
	#undef USBError
	void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
	#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBPIPEV2_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
	#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

//================================================================================================
//
//  IOUSBPipeV2 Methods
//
//================================================================================================
OSDefineMetaClassAndStructors(IOUSBPipeV2, IOUSBPipe)


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
    IOReturn				err;
    IOUSBControllerV3		*controllerV3;
	UInt16					maxStreamID = 0;
    
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
			USBError(1,"IOUSBPipeV2[%p]::InitToEndpoint (%s, EP: 0x%x) -- Requested a pipe with stream ID, but this IOUSBController does not support it", this, device->getName(), (uint32_t)ed->bEndpointAddress);
			return false;
		}
    }
	
    _descriptor = ed;
	_sscd = sscd;
    _endpoint.number = ed->bEndpointAddress & kUSBPipeIDMask;
    
	// some of the stuff done below could just leverage off of GetEndpointPropertiesV3. However, not doing that yet as it will not work for Pipe 0 since Pipe0 does not have an interface
    _endpoint.transferType = (ed->bmAttributes & kUSB_EPDesc_bmAttributes_TranType_Mask) >> kUSB_EPDesc_bmAttributes_TranType_Shift;
    
    if (_endpoint.transferType == kUSBControl)
        _endpoint.direction = kUSBAnyDirn;
    else
		_endpoint.direction = (ed->bEndpointAddress & 0x80) ? kUSBIn : kUSBOut;

	
	// a note about _endpoint.maxPacketSize. HS Isoc endpoints can have a multiplier for up to three MPS packets per interval
	// SS Isoc can have both a burst of up to 16 packets and a multiplier for up to 48 packets per interval
	// we will keep the _endpoint.maxPacketSize as the full interval MPS for these types of endpoints, but we will calculate that further down
	// For other types of endpoints, we will not multiply by the burst (and they don't have a multiplier)
	
	_endpoint.maxPacketSize = (ed->wMaxPacketSize & kUSB_EPDesc_wMaxPacketSize_MPS_Mask) >> kUSB_EPDesc_wMaxPacketSize_MPS_Shift;
	
	// This mult will only be >0 for HSHB endpoints, and not for SS, because the mult bits for SS moved
	if ((sscd == NULL) && ((_endpoint.transferType == kUSBIsoc) || (_endpoint.transferType == kUSBInterrupt)))
	{
		_mult    = (ed->wMaxPacketSize & kUSB_HSFSEPDesc_wMaxPacketSize_Mult_Mask) >> kUSB_HSFSEPDesc_wMaxPacketSize_Mult_Shift;
		if (_mult > 2)
		{
			_mult = 2;
			USBLog(1,"IOUSBPipeV2[%p]::InitToEndpoint (%s, EP: 0x%x) -- HS mult for wMaxPacketSize is illegal (3), setting to 2", this, device ? device->getName() : "Unnamed Device", (uint32_t)ed->bEndpointAddress);
		}
	}

	_endpoint.interval = ed->bInterval;

 	if ( _endpoint.transferType == kUSBInterrupt)
	{
		_USAGETYPE = (ed->bmAttributes & kUSB_EPDesc_bmAttributes_UsageType_Mask) >> kUSB_EPDesc_bmAttributes_UsageType_Shift;
		_SYNCTYPE = 0;
	}
	else if ( _endpoint.transferType == kUSBIsoc)
	{
		_USAGETYPE = (ed->bmAttributes & kUSB_EPDesc_bmAttributes_UsageType_Mask) >> kUSB_EPDesc_bmAttributes_UsageType_Shift;
		_SYNCTYPE = (ed->bmAttributes & kUSB_EPDesc_bmAttributes_SyncType_Mask) >> kUSB_EPDesc_bmAttributes_SyncType_Shift;
	}
	else
	{
		_USAGETYPE = 0;
		_SYNCTYPE = 0;
	}

	_maxBurst = 0;							// init to 0
	
	if (_sscd != NULL)
	{
        _maxBurst = _sscd->bMaxBurst;
		_bytesPerInterval = _sscd->wBytesPerInterval;
		
		if (_endpoint.transferType == kUSBBulk)
		{
			_maxStream = (_sscd->bmAttributes & kUSB_SSCompDesc_Bulk_MaxStreams_Mask) >> kUSB_SSCompDesc_Bulk_MaxStreams_Shift;
            _configuredStreams = 0;
			if(_maxStream > 0)
			{
				UInt32		maxSupported;
				
				maxStreamID = (1 << _maxStream) - 1;						// Note -1, valid stream IDs run to max streams -1, zero is invalid
				
				if(maxStreamID > kUSBMaxStream)
				{
					maxStreamID = kUSBMaxStream;
				}
				
				maxSupported = controllerV3->UIMMaxSupportedStream();
				
				if( (maxStreamID > 0) && (maxSupported == 0) )
				{
					USBLog(1,"IOUSBPipeV2[%p]::InitToEndpoint (%s, EP: 0x%x) -- Requested a pipe with maxStreamID %d, but this IOUSBController does not support streams", this, device ? device->getName() : "Unnamed Device", (uint32_t)ed->bEndpointAddress, (uint32_t)maxStreamID);
					_maxStream = 0;
					return false;
				}
				if(maxStreamID > maxSupported)
				{
					USBLog(1,"IOUSBPipeV2[%p]::InitToEndpoint (%s, EP: 0x%x) -- Requested a pipe with max stream of %d, but this IOUSBController only supports %d streams", this, device ? device->getName() : "Unnamed Device", (uint32_t)ed->bEndpointAddress, (uint32_t)maxStreamID, (uint32_t)maxSupported);
					maxStreamID = maxSupported;
				}
			}
			
			if (_endpoint.maxPacketSize != kUSB_EPDesc_MaxMPS)
			{
				USBLog(1,"IOUSBPipeV2[%p]::InitToEndpoint (%s, EP: 0x%x) -- SuperSpeed Bulk EP with illegal wMaxPacketSize of %d, setting to kUSB_EPDesc_MaxMPS", this, device ? device->getName() : "Unnamed Device", (uint32_t)ed->bEndpointAddress, (uint32_t)_endpoint.maxPacketSize);
				_endpoint.maxPacketSize = kUSB_EPDesc_MaxMPS;
			}
			
			USBLog(3, "IOUSBPipeV2[%p]::InitToEndpoint: SuperSpeed Bulk pipe maxStreams: %d", this, (uint32_t)maxStreamID);
		}
		
		if (_endpoint.transferType == kUSBControl)
        {
			if (_endpoint.maxPacketSize != 512)
			{
				USBLog(1,"IOUSBPipeV2[%p]::InitToEndpoint (%s, EP: 0x%x) -- SuperSpeed Control EP with illegal wMaxPacketSize of %d, setting to 512", this, device ? device->getName() : "Unnamed Device", (uint32_t)ed->bEndpointAddress, (uint32_t)_endpoint.maxPacketSize);
				_endpoint.maxPacketSize = 512;
			}
            _maxBurst = 0;
        }

		if (_endpoint.transferType == kUSBIsoc)
		{
			_mult = (sscd->bmAttributes & kUSB_SSCompDesc_Isoc_Mult_Mask) >> kUSB_SSCompDesc_Isoc_Mult_Shift;           // this is zero based
			if (_mult > 2)
			{
				_mult = 2;
				USBLog(1,"IOUSBPipeV2[%p]::InitToEndpoint (%s, EP: 0x%x) -- SS mult for wMaxPacketSize is illegal (3), setting to 2", this, device ? device->getName() : "Unnamed Device", (uint32_t)ed->bEndpointAddress);
			}
			if ( (_maxBurst > 0) || (_mult > 0))
			{
				if (_endpoint.maxPacketSize != kUSB_EPDesc_MaxMPS)
				{
					USBLog(1,"IOUSBPipeV2[%p]::InitToEndpoint (%s, EP: 0x%x) -- SuperSpeed Isoch EP (bMaxBurst: %d mult: %d) with illegal wMaxPacketSize of %d, setting to kUSB_EPDesc_MaxMPS", this, device ? device->getName() : "Unnamed Device", (uint32_t)ed->bEndpointAddress, (uint32_t)_maxBurst, (uint32_t)_mult, (uint32_t)_endpoint.maxPacketSize);
					_endpoint.maxPacketSize = kUSB_EPDesc_MaxMPS;
				}
			}
			USBLog(6, "IOUSBPipeV2[%p]::InitToEndpoint: SuperSpeed Isoch pipe  bytesPerInterval: %d, maxPacketSize: %d, maxBurst: %d, mult: %d", this, (uint32_t)_bytesPerInterval, (uint32_t)_endpoint.maxPacketSize,(uint32_t)_maxBurst,(uint32_t)_mult);
		}

		if (_endpoint.transferType == kUSBInterrupt)
		{
			if ( _maxBurst > 0)
			{
				if (_endpoint.maxPacketSize != kUSB_EPDesc_MaxMPS)
				{
					USBLog(1,"IOUSBPipeV2[%p]::InitToEndpoint (%s, EP: 0x%x) -- SuperSpeed Interrupt EP (bMaxBurst: %d) with illegal wMaxPacketSize of %d, setting to kUSB_EPDesc_MaxMPS", this, device ? device->getName() : "Unnamed Device", (uint32_t)ed->bEndpointAddress, (uint32_t)_maxBurst, (uint32_t)_endpoint.maxPacketSize);
					_endpoint.maxPacketSize = kUSB_EPDesc_MaxMPS;
				}
			}
			else
			{
				if ( _endpoint.maxPacketSize == 0)
				{
					USBLog(1,"IOUSBPipeV2[%p]::InitToEndpoint (%s, EP: 0x%x) -- SuperSpeed Interrupt EP (bMaxBurst of 0) with illegal wMaxPacketSize of 0, setting to 1", this, device ? device->getName() : "Unnamed Device", (uint32_t)ed->bEndpointAddress);
					_endpoint.maxPacketSize = 1;
				}
			}
		}
	}
    
	if ((_endpoint.transferType == kUSBIsoc) || (_endpoint.transferType == kUSBInterrupt))
	{
		// this will be the case for all Isoc and Interrupt endpoints. Now that we have verified all of the fields, we will change maxPacketSize to include the _mult and the _burst
		// we do this so that the _endpoint.maxPacketSize remains consistent for our classic UIMs and for the XHCI UIM
		_endpoint.maxPacketSize = _endpoint.maxPacketSize * (_mult + 1) * (_maxBurst + 1);
	}
    
	_address = address;
    _SPEED = speed;
	_DEVICE = device;
	_INTERFACE = interface;
	
	if (_INTERFACE && _INTERFACE->GetInterfaceClass() == kUSBMassStorageInterfaceClass && _INTERFACE->GetInterfaceSubClass() == kUSBMassStorageSCSISubClass && _INTERFACE->GetInterfaceProtocol() == kMSCProtocolUSBAttachedSCSI)
	{
   	 	const UASPipeDescriptor		*desc = NULL;						// start with the interface descriptor
		if (_sscd != NULL)
		{
			desc = (UASPipeDescriptor *)_INTERFACE->FindNextAssociatedDescriptor(sscd, kUSBAnyDesc);
		}
		else
		{
			desc = (UASPipeDescriptor *)_INTERFACE->FindNextAssociatedDescriptor(ed, kUSBAnyDesc);
		}
		
		if (desc && (desc->bDescriptorType == kUSBClassSpecificDescriptor))
		{
			_UASUSAGEID = desc->bPipeID;
		}
	}
	
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
	
    if( (maxStreamID == 0) && (_maxBurst == 0) )
    {
		err = _controller->OpenPipe(_address, speed, &_endpoint);
	}
	else
	{
		err = controllerV3->OpenPipe(_address, speed, &_endpoint, maxStreamID, _maxBurst | (_mult << 8));
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
    IOUSBControllerV3		*controllerV3;
	UInt32					maxStreamID = SupportsStreams();
	
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
		if(streamID > maxStreamID)
		{
			USBLog(5, "IOUSBPipeV2[%p]::Abort(streamID) - bad argument:  stream ID greater than max (%d > %d)", this, (uint32_t)streamID, (uint32_t)maxStreamID);
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
    IOReturn				err = kIOReturnSuccess;
	IOUSBControllerV3		*controllerV3;
	UInt32					maxStreamID = SupportsStreams();
    
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
		if(streamID > maxStreamID)
		{
			USBLog(5, "IOUSBPipeV2[%p]::Read - bad arguments:  stream ID greater than max (%d > %d)", this, (uint32_t)streamID, (uint32_t)maxStreamID);
			return kIOReturnBadArgument;
		}
		
	}
	
    USBLog(7, "IOUSBPipeV2[%p]::Read (addr %d:%d type %d) - streamID = %d, reqCount = %qd", this, _address, _endpoint.number , _endpoint.transferType, streamID, (uint64_t)reqCount);
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
    IOReturn				err = kIOReturnSuccess;
    IOUSBControllerV3		*controllerV3;
	UInt32					maxStreamID = SupportsStreams();
    
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
		if(streamID > maxStreamID)
		{
			USBLog(5, "IOUSBPipeV2[%p]::Write - bad arguments:  stream ID greater than max (%d > %d)", this, (uint32_t)streamID, (uint32_t)maxStreamID);
			return kIOReturnBadArgument;
		}
		
	}
    
	USBLog(7, "IOUSBPipeV2[%p]::Write (addr %d:%d type %d) - streamID: %d, reqCount = %qd", this, _address, _endpoint.number , _endpoint.transferType, streamID, (uint64_t)reqCount);
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
		
 		if(streamID != 0)
		{
			err = controllerV3->Write(streamID, buffer, _address, &_endpoint, &tap, noDataTimeout, completionTimeout, reqCount);
		}
		else
		{
			err = _controller->Write(buffer, _address, &_endpoint, &tap, noDataTimeout, completionTimeout, reqCount);
		}
        
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
	// _maxStream is stored in the "raw" format so we need to convert to the expanded format	
	return ((1 << _maxStream) - 1);
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



#pragma mark IOUSBPipe overrides
//================================================================================================
//
//   SetPipePolicy
//
//================================================================================================
//
IOReturn
IOUSBPipeV2::SetPipePolicy(UInt16 maxPacketSize, UInt8 maxInterval)
{
	UInt16					oldsize = _endpoint.maxPacketSize;
    IOReturn				err = kIOReturnSuccess;
	IOUSBControllerV3		*controllerV3 = OSDynamicCast(IOUSBControllerV3, _controller);
   
	USBLog(6, "IOUSBPipeV2[%p]::SetPipePolicy (addr %d:%d dir: %d, type: %d) - maxPacketSize: %d, maxInterval: %d", this, _address, _endpoint.number, _endpoint.direction, _endpoint.transferType, maxPacketSize, maxInterval);
	
	if ((_SPEED == kUSBDeviceSpeedSuper) && (maxPacketSize != 0) && _sscd && _INTERFACE)
	{
		UInt32			calculatedMax = _INTERFACE->CalculateFullMaxPacketSize((IOUSBEndpointDescriptor*)&_endpoint, _sscd);
		
		if (maxPacketSize != calculatedMax)
		{
			USBLog(1, "IOUSBPipeV2[%p]::SetPipePolicy - cannot change the MPS of a SS Int/Isoc pipe to anything other than 0 (%d vs %d)", this, maxPacketSize, _INTERFACE->CalculateFullMaxPacketSize((IOUSBEndpointDescriptor*)&_endpoint, _sscd));
			return kIOReturnBadArgument;
		}
	}
	
	
	if (_endpoint.transferType != kUSBIsoc)
	{
		return super::SetPipePolicy(maxPacketSize, maxInterval);
	}
	
	// Isoc only at this point
	if (maxPacketSize <= _INTERFACE->CalculateFullMaxPacketSize((IOUSBEndpointDescriptor*)_descriptor, _sscd))
	{
		USBLog(6, "IOUSBPipeV2[%p]::SetPipePolicy - trying to change isoch pipe from %d to %d bytes", this, oldsize, maxPacketSize);
		_endpoint.maxPacketSize = maxPacketSize;											// this is a full MPS (baseMPS * burst * mult)
		
		// OpenPipe with Isoch pipes which already exist will try to change the maxpacketSize in the pipe.
		// the speed param below is actually unused for Isoc pipes
		
		if (controllerV3 && _maxBurst)
			err = controllerV3->OpenPipe(_address, _SPEED, &_endpoint, 0, _maxBurst | (_mult << 8));
		else
			err = _controller->OpenPipe(_address, _SPEED, &_endpoint);
		
		if (err)
		{
			USBLog(2, "IOUSBPipeV2[%p]::SetPipePolicy - new OpenPipe failed with 0x%x (%s) - returning old settings", this, err, USBStringFromReturn(err));
			_endpoint.maxPacketSize = oldsize;
		}
	}
	else
	{
		USBLog(2, "IOUSBPipeV2[%p]::SetPipePolicy - requested size (%d) larger than maxPacketSize in descriptor (%d)", this, maxPacketSize, _endpoint.maxPacketSize);
		err = kIOReturnBadArgument;
	}
	
    return err;
}


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

