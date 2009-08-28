/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2006 Apple Computer, Inc.  All Rights Reserved.
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

#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>
#include <IOKit/usb/IOUSBControllerV3.h>

#define super	IOUSBHubDevice
#define self	this

/* Convert USBLog to use kprintf debugging */
#ifndef IOUSBROOTHUBDEVICE_USE_KPRINTF
	#define IOUSBROOTHUBDEVICE_USE_KPRINTF 0
#endif

#if IOUSBROOTHUBDEVICE_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBROOTHUBDEVICE_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#define SAFE_RELEASE_NULL(object)  do {		\
if (object) object->release();				\
(object) = NULL;							\
} while (0)

#define _IORESOURCESENTRY					_expansionData->_IOResourcesEntry

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors( IOUSBRootHubDevice, IOUSBHubDevice )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOUSBRootHubDevice*
IOUSBRootHubDevice::NewRootHubDevice()
{
	IOUSBRootHubDevice *me = new IOUSBRootHubDevice;
	
	if (!me)
		return NULL;
	
	if (!me->init())
	{
		me->release();
		me = NULL;
	}
	
	return me;
}


bool 
IOUSBRootHubDevice::init()
{
    if (!super::init())
        return false;
		
    // allocate our expansion data
    if (!_expansionData)
    {
		_expansionData = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
		if (!_expansionData)
			return false;
		
		bzero(_expansionData, sizeof(ExpansionData));
    }
	
    return true;
 }



bool
IOUSBRootHubDevice::InitializeCharacteristics()
{
	UInt32			characteristics = kIOUSBHubDeviceIsRootHub;
	
	USBLog(5, "%s[%p]::InitializeCharacteristics", getName(), this);
	
	// since i am the root hub, just check my speed and that will be the bus speed
	if (GetSpeed() == kUSBDeviceSpeedHigh)
		characteristics |= kIOUSBHubDeviceIsOnHighSpeedBus;
		
	SetHubCharacteristics(characteristics);
	return true;
}



bool
IOUSBRootHubDevice::start(IOService *provider)
{
	mach_timespec_t				timeSpec;
	bool						returnValue = false;
	
	USBLog(5, "%s[%p]::start", getName(), this);

 	timeSpec.tv_sec = 5;
	timeSpec.tv_nsec = 0;
	
	_IORESOURCESENTRY = waitForService(serviceMatching("IOResources"), &timeSpec);

	returnValue = super::start(provider);
	if ( !returnValue)
	{
		USBLog(5, "IOUSBRootHubDevice[%p]::start - super returned false", this);
		return false;
	}
	
	// Make a copy of our superclass' commandGate.  We could just use it, but we need
	// to keep it around for binary compatibility. Retain it for good measure.
	
	_commandGate = IOUSBDevice::_expansionData->_commandGate;
	_commandGate->retain();
	
	USBLog(5, "%s[%p]::start (_commandGate %p)", getName(), this, IOUSBDevice::_expansionData->_commandGate);
	return returnValue;
}



void
IOUSBRootHubDevice::stop( IOService *provider )
{
	// Since we retain()'d this eariler, we need to release() it.
	if ( _commandGate )
	{
		_commandGate->release();
		_commandGate = NULL;
	}
	
	super::stop(provider);
}



void
IOUSBRootHubDevice::free()
{
    if (_expansionData)
    {
        IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }
    super::free();
}



IOReturn
IOUSBRootHubDevice::GatedDeviceRequest (OSObject *owner,  void *arg0,  void *arg1,  void *arg2,  void *arg3 )
{
	IOUSBRootHubDevice *me = (IOUSBRootHubDevice*)owner;
	
	if (!me)
		return kIOReturnNotResponding;
	return me->DeviceRequestWorker((IOUSBDevRequest*)arg0, (uintptr_t)arg1, (uintptr_t)arg2, (IOUSBCompletion*)arg3);
}



// intercept regular hub requests since the controller simulates the root hub
IOReturn 
IOUSBRootHubDevice::DeviceRequest(IOUSBDevRequest *request, IOUSBCompletion *completion)
{
    return DeviceRequest(request, 0, 0, completion);
}



IOReturn 
IOUSBRootHubDevice::DeviceRequest(IOUSBDevRequest *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
	// We have to use IOUSBDevice::_expansionData->_commandGate, instead of the copy of it in _commandGate because this method will be called by 
	// our super::start() BEFORE we're able to set the _commandGate to IOUSBDevice::_expansionData->_commandGate.
	
	if (!IOUSBDevice::_expansionData->_commandGate)
	{
		USBLog(5, "IOUSBRootHubDevice[%p]::DeviceRequest - but no IOUSBDevice::_expansionData->_commandGate", this);
		return kIOReturnNotResponding;
	}
		
	if (_myPolicyMaker && (_myPolicyMaker->getPowerState() == kIOUSBHubPowerStateLowPower))
	{
		// this is not usually an issue, but i want to make sure it doesn't become one
		USBLog(5, "IOUSBRootHubDevice[%p]::DeviceRequest - doing a device request while in low power mode - should be OK", this);
	}
	return (IOUSBDevice::_expansionData->_commandGate)->runAction(GatedDeviceRequest, request, (void*)noDataTimeout, (void*)completionTimeout, completion);
}




IOReturn 
IOUSBRootHubDevice::DeviceRequestWorker(IOUSBDevRequest *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
#pragma unused (noDataTimeout, completionTimeout, completion)
	IOReturn	err = 0;
    UInt16		theRequest;
    UInt8		dType, dIndex;

    
    if (!request)
        return(kIOReturnBadArgument);

    theRequest = (request->bRequest << 8) | request->bmRequestType;

    switch (theRequest)
    {
        // Standard Requests
        //
        case kClearDeviceFeature:
            if (request->wIndex == 0)
                err = _controller->ClearRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetDescriptor:
            dType = request->wValue >> 8;
            dIndex = request->wValue & 0x00FF;
            switch (dType) {
                case kUSBDeviceDesc:
                    err = _controller->GetRootHubDeviceDescriptor((IOUSBDeviceDescriptor*)request->pData);
                    request->wLenDone = sizeof(IOUSBDeviceDescriptor);
                    break;

                case kUSBConfDesc:
                {
                    OSData *fullDesc = OSData::withCapacity(1024); // FIXME
                    UInt16 newLength;
                    
                    err = _controller->GetRootHubConfDescriptor(fullDesc);
                    newLength = fullDesc->getLength();
                    if (newLength < request->wLength)
                        request->wLength = newLength;
                    bcopy(fullDesc->getBytesNoCopy(), (char *)request->pData, request->wLength);
                    request->wLenDone = request->wLength;
                    fullDesc->free();
                    break;
                }

                case kUSBStringDesc:
                {
                    OSData *fullDesc = OSData::withCapacity(1024); // FIXME
                    UInt16 newLength;
                    
                    err = _controller->GetRootHubStringDescriptor((request->wValue & 0x00ff), fullDesc);
                    newLength = fullDesc->getLength();
                    if (newLength < request->wLength)
                        request->wLength = newLength;
                    bcopy(fullDesc->getBytesNoCopy(), (char *)request->pData, request->wLength);
                    request->wLenDone = request->wLength;
                    fullDesc->free();
                    break;
                }
                
                default:
                    err = kIOReturnBadArgument;
            }
            break;

        case kGetDeviceStatus:
            if ((request->wValue == 0) && (request->wIndex == 0) && (request->pData != 0))
            {
                *(UInt16*)(request->pData) = HostToUSBWord(1); // self-powered
                request->wLenDone = 2;
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kSetAddress:
            if (request->wIndex == 0)
                err = _controller->SetHubAddress(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;
                
        case kSetConfiguration:
            if (request->wIndex == 0)
                configuration = request->wValue;
            else
                err = kIOReturnBadArgument;
            break;

        case kSetDeviceFeature:
            if (request->wIndex == 0)
                err = _controller->SetRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetConfiguration:
            if ((request->wIndex == 0) && (request->pData != 0))
            {
                *(UInt8*)(request->pData) = configuration;
                request->wLenDone = 1;
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kClearInterfaceFeature:
        case kClearEndpointFeature:
        case kGetInterface:
        case kGetInterfaceStatus:
        case kGetEndpointStatus:
        case kSetInterfaceFeature:
        case kSetEndpointFeature:
        case kSetDescriptor:
        case kSetInterface:
        case kSyncFrame:
            err = kIOReturnUnsupported;
            break;

        // Class Requests
        //
        case kClearHubFeature:
            if (request->wIndex == 0)
                err = _controller->ClearRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kClearPortFeature:
            err = _controller->ClearRootHubPortFeature(request->wValue, request->wIndex);
            break;

        case kGetPortState:
            if ((request->wValue == 0) && (request->pData != 0))
                err = _controller->GetRootHubPortState((UInt8 *)request->pData, request->wIndex);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetHubDescriptor:
            if ((request->wValue == ((kUSBHubDescriptorType << 8) + 0)) && (request->pData != 0))
            {
                err = _controller->GetRootHubDescriptor((IOUSBHubDescriptor *)request->pData);
                request->wLenDone = sizeof(IOUSBHubDescriptor);
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kGetHubStatus:
            if ((request->wValue == 0) && (request->wIndex == 0) && (request->pData != 0))
            {
                err = _controller->GetRootHubStatus((IOUSBHubStatus *)request->pData);
                request->wLenDone = sizeof(IOUSBHubStatus);
            }
            else
                err = kIOReturnBadArgument;
           break;

        case kGetPortStatus:
            if ((request->wValue == 0) && (request->pData != 0))
            {
                err = _controller->GetRootHubPortStatus((IOUSBHubPortStatus *)request->pData, request->wIndex);
                request->wLenDone = sizeof(IOUSBHubPortStatus);
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kSetHubDescriptor:
            if (request->pData != 0)
                err = _controller->SetRootHubDescriptor((OSData *)request->pData);
            else
                err = kIOReturnBadArgument;
            break;

        case kSetHubFeature:
            if (request->wIndex == 0)
                err = _controller->SetRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kSetPortFeature:
            err = _controller->SetRootHubPortFeature(request->wValue, request->wIndex);
            break;

        default:
            err = kIOReturnBadArgument;

    }
    return(err);
}


bool
IOUSBRootHubDevice::IsRootHub(void)
{
	return true;
}

UInt32
IOUSBRootHubDevice::RequestExtraPower(UInt32 requestedPower)
{
	OSNumber *		numberObject = NULL;
	UInt32			totalExtraCurrent = 0;
	UInt32			maxPowerPerPort = 0;
	UInt32			extraAllocated = 0;
	OSObject *		propertyObject = NULL;
	
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::RequestExtraPower - _expansionData or _IORESOURCESENTRY is NULL", getName(), this);
		return 0;
	}
	
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentExtra);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		totalExtraCurrent = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestExtraPower - we have a kAppleCurrentExtra with %d", getName(), this, (uint32_t) totalExtraCurrent);
	}
	SAFE_RELEASE_NULL(propertyObject);
	
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentAvailable);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		maxPowerPerPort = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestExtraPower - we have a kAppleCurrentAvailable with %d", getName(), this, (uint32_t) maxPowerPerPort);
	}
	SAFE_RELEASE_NULL(propertyObject);

	USBLog(5, "%s[%p]::RequestExtraPower - requestedPower = %d, available: %d", getName(), this, (uint32_t)requestedPower, (uint32_t) totalExtraCurrent);
	
	// The power requested is a delta above the USB Spec for the port.  That's why we need to subtract the 500mA from the maxPowerPerPort value
	if (requestedPower > (maxPowerPerPort-500))		// limit requests to the maximum the HW can support
	{
		USBLog(5, "%s[%p]::RequestExtraPower - requestedPower = %d was grater than the maximum per port of %d.  Using that value instead", getName(), this, (uint32_t)requestedPower, (uint32_t) (maxPowerPerPort-500));
		requestedPower = maxPowerPerPort-500;
	}
	
	if (requestedPower <= totalExtraCurrent)
	{		
		// honor the request if possible
		extraAllocated = requestedPower;
		totalExtraCurrent -= extraAllocated;

		USBLog(5, "%s[%p]::RequestExtraPower - setting kAppleCurrentExtra to %d", getName(), this, (uint32_t) totalExtraCurrent);
		(_IORESOURCESENTRY)->setProperty(kAppleCurrentExtra, totalExtraCurrent, 32);
	}
	
	// this method may be overriden by the IOUSBRootHubDevice class to implement this
	USBLog(5, "%s[%p]::RequestExtraPower - extraAllocated = %d", getName(), this, (uint32_t)extraAllocated);
	
	return extraAllocated;
}



void
IOUSBRootHubDevice::ReturnExtraPower(UInt32 returnedPower)
{
	OSNumber *		numberObject = NULL;
	OSObject *		propertyObject = NULL;
	UInt32			powerAvailable = 0;
	
	USBLog(5, "%s[%p]::ReturnExtraPower - returning = %d", getName(), this, (uint32_t)returnedPower);
	
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::ReturnExtraPower - _expansionData or _IORESOURCESENTRY is NULL", getName(), this);
		return;
	}
	
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentExtra);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		powerAvailable = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::ReturnExtraPower - we have a kAppleCurrentExtra with %d", getName(), this, (uint32_t) powerAvailable);
	}
	SAFE_RELEASE_NULL(propertyObject);
		
	if (returnedPower > 0)
	{
		powerAvailable += returnedPower;
		USBLog(5, "%s[%p]::ReturnExtraPower - setting kAppleCurrentExtra to %d", getName(), this, (uint32_t) powerAvailable);
		(_IORESOURCESENTRY)->setProperty(kAppleCurrentExtra, powerAvailable, 32);
	}
	
	return;
	
}

void			
IOUSBRootHubDevice::InitializeExtraPower(UInt32 maxPortCurrent, UInt32 totalExtraCurrent)
{
	OSNumber *		numberObject = NULL;
	OSObject *		propertyObject = NULL;
	UInt32			propertyValue = 0;
	
	USBLog(5, "%s[%p]::InitializeExtraPower - maxPortCurrent = %d, totalExtraCurrent: %d", getName(), this, (uint32_t)maxPortCurrent, (uint32_t) totalExtraCurrent);
	
	// Check to see if we have a property already in IOResources.  If we do then check to see if our value is different.  If we don't, then set it 

	if ( _expansionData == NULL )
	{
		USBLog(5, "%s[%p]::InitializeExtraPower - _expansionData is NULL", getName(), this);
		return;
	}

	if ( _IORESOURCESENTRY == NULL)
	{
		USBLog(5, "%s[%p]::InitializeExtraPower - no _IORESOURCESENTRY available", getName(), this);
	}
	else
	{
		propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentExtra);
		numberObject = OSDynamicCast(OSNumber, propertyObject);
		if (numberObject)
		{
			propertyValue = numberObject->unsigned32BitValue();
			USBLog(5, "%s[%p]::InitializeExtraPower - we have a kAppleCurrentExtra with %d", getName(), this, (uint32_t) propertyValue);
			if ( propertyValue < totalExtraCurrent)
			{
				USBLog(5, "%s[%p]::InitializeExtraPower - kAppleCurrentExtra, setting it to %d", getName(), this, (uint32_t) totalExtraCurrent);
				(_IORESOURCESENTRY)->setProperty(kAppleCurrentExtra, totalExtraCurrent, 32);
			}
		}
		else
		{
			USBLog(5, "%s[%p]::InitializeExtraPower - we did NOT have a kAppleCurrentExtra, setting it to %d", getName(), this, (uint32_t) totalExtraCurrent);
			(_IORESOURCESENTRY)->setProperty(kAppleCurrentExtra, totalExtraCurrent, 32);
		}
		SAFE_RELEASE_NULL(propertyObject);
	
		propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentAvailable);
		numberObject = OSDynamicCast(OSNumber, propertyObject);
		if (numberObject)
		{
			propertyValue = numberObject->unsigned32BitValue();
			USBLog(5, "%s[%p]::InitializeExtraPower - we have a kAppleCurrentAvailable with %d", getName(), this, (uint32_t) propertyValue);
			if (propertyValue < maxPortCurrent )
			{
				USBLog(1, "%s[%p]::InitializeExtraPower - we have a kAppleCurrentAvailable with %d, but maxPortCurrent is %d, setting it to that", getName(), this, (uint32_t) propertyValue, (uint32_t) maxPortCurrent);
				(_IORESOURCESENTRY)->setProperty(kAppleCurrentAvailable, maxPortCurrent, 32);
			}
		}
		else
		{
			USBLog(5, "%s[%p]::InitializeExtraPower - we did NOT have a kAppleCurrentAvailable in IOResources, setting it to %d", getName(), this, (uint32_t) maxPortCurrent);
			(_IORESOURCESENTRY)->setProperty(kAppleCurrentAvailable, maxPortCurrent, 32);
			
		}
		SAFE_RELEASE_NULL(propertyObject);
	}
	
	super::InitializeExtraPower(maxPortCurrent, totalExtraCurrent);
}

void
IOUSBRootHubDevice::SetSleepCurrent(UInt32 sleepCurrent)
{
	OSNumber *		numberObject = NULL;
	OSObject *		propertyObject = NULL;
	UInt32			propertyValue = 0;
	
	USBLog(5, "%s[%p]::SetSleepCurrent -  %d", getName(), this, (uint32_t)sleepCurrent);
	
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::SetSleepCurrent - _expansionData or _IORESOURCESENTRY is NULL", getName(), this);
		return;
	}
	
	// Get the property from IOResources:  if that value is less than sleepCurrent, updated it to sleepCurrent
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentInSleep);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		propertyValue = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::SetSleepCurrent - we have a kAppleCurrentInSleep with %d", getName(), this, (uint32_t) propertyValue);
	}
	SAFE_RELEASE_NULL(propertyObject);
	
	if ( propertyValue < sleepCurrent )
	{
		USBLog(5, "%s[%p]::SetSleepCurrent - setting kAppleCurrentInSleep to %d", getName(), this, (uint32_t) sleepCurrent);
		(_IORESOURCESENTRY)->setProperty(kAppleCurrentInSleep, sleepCurrent, 32);
	}
	
	
}

UInt32
IOUSBRootHubDevice::GetSleepCurrent()
{
	OSNumber *		numberObject = NULL;
	OSObject *		propertyObject = NULL;
	UInt32			propertyValue = 0;
	
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::GetSleepCurrent - _expansionData or _IORESOURCESENTRY is NULL", getName(), this);
		return 0;
	}
	
	// Get the property from IOResources:  if that value is less than sleepCurrent, updated it to sleepCurrent
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentInSleep);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		propertyValue = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::SetSleepCurrent - we have a kAppleCurrentInSleep with %d", getName(), this, (uint32_t) propertyValue);
	}
	SAFE_RELEASE_NULL(propertyObject);
	
	USBLog(5, "%s[%p]::GetSleepCurrent -  returning %d", getName(), this, (uint32_t)propertyValue);
	return propertyValue;
}


UInt32
IOUSBRootHubDevice::RequestSleepPower(UInt32 requestedPower)
{
	OSNumber *		numberObject = NULL;
	OSObject *		propertyObject = NULL;
	UInt32			totaSleepCurrent = 0;
	UInt32			maxPowerPerPort = 0;
	UInt32			extraAllocated = 0;
	
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::RequestSleepPower - _expansionData or _IORESOURCESENTRY is NULL", getName(), this);
		return 0;
	}
	
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentInSleep);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		totaSleepCurrent = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestSleepPower - we have a kAppleCurrentInSleep with %d", getName(), this, (uint32_t) totaSleepCurrent);
	}
	
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentAvailable);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		maxPowerPerPort = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestSleepPower - we have a kAppleCurrentAvailable with %d", getName(), this, (uint32_t) maxPowerPerPort);
	}
	SAFE_RELEASE_NULL(propertyObject);
	
	USBLog(5, "%s[%p]::RequestSleepPower - requestedPower = %d, available: %d", getName(), this, (uint32_t)requestedPower, (uint32_t) totaSleepCurrent);
	
	if (requestedPower > maxPowerPerPort)		// limit requests to the maximum the HW can support
	{
		USBLog(5, "%s[%p]::RequestSleepPower - requestedPower = %d was more than the maximum per port of: %d, limiting request to %d", getName(), this, (uint32_t)requestedPower, (uint32_t) maxPowerPerPort, (uint32_t) maxPowerPerPort);
		requestedPower = maxPowerPerPort;
	}
	
	if (requestedPower <= totaSleepCurrent)
	{		
		// honor the request if possible
		extraAllocated = requestedPower;
		totaSleepCurrent -= extraAllocated;

		USBLog(5, "%s[%p]::RequestSleepPower - setting kAppleCurrentInSleep to %d", getName(), this, (uint32_t) totaSleepCurrent);
		(_IORESOURCESENTRY)->setProperty(kAppleCurrentInSleep, totaSleepCurrent, 32);
}
	
	USBLog(5, "%s[%p]::RequestSleepPower - extraAllocated = %d", getName(), this, (uint32_t)extraAllocated);
	
	return extraAllocated;
}

void
IOUSBRootHubDevice::ReturnSleepPower(UInt32 returnedPower)
{
	OSNumber *		numberObject = NULL;
	OSObject *		propertyObject = NULL;
	UInt32			powerAvailable = 0;
	
	USBLog(5, "%s[%p]::ReturnSleepPower - returning = %d", getName(), this, (uint32_t)returnedPower);
	
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentInSleep);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		powerAvailable = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::ReturnSleepPower - we have a kAppleCurrentInSleep with %d", getName(), this, (uint32_t) powerAvailable);
	}
	SAFE_RELEASE_NULL(propertyObject);
	
	if (returnedPower > 0)
	{
		powerAvailable += returnedPower;
		USBLog(5, "%s[%p]::ReturnSleepPower - setting kAppleCurrentInSleep to %d", getName(), this, (uint32_t) powerAvailable);
		(_IORESOURCESENTRY)->setProperty(kAppleCurrentInSleep, powerAvailable, 32);
	}
	
	return;
	
}


IOReturn
IOUSBRootHubDevice::GetDeviceInformation(UInt32 *info)
{
	OSObject *		propertyObject = NULL;
    OSString *		theString = NULL;
    bool			isBuiltIn = false;
	
	*info = 0;
	
	*info =(( 1 << kUSBInformationDeviceIsCaptiveBit ) |
			( 1 << kUSBInformationDeviceIsInternalBit ) |
			( 1 << kUSBInformationDeviceIsConnectedBit ) |
			( 1 << kUSBInformationDeviceIsEnabledBit ) |
			( 1 << kUSBInformationDeviceIsRootHub )
			);
	
	// Need to determine if this is a built-in or external root hub
	propertyObject = _controller->copyProperty("Card Type");
    theString = OSDynamicCast(OSString, propertyObject);
	if( theString)
        isBuiltIn = theString->isEqualTo("Built-in");
    
 	SAFE_RELEASE_NULL(propertyObject);

	if (isBuiltIn)
		*info |=  (1 << kUSBInformationRootHubisBuiltIn);
	
	
	USBLog(2, "IOUSBRootHubDevice[%p]::GetDeviceInformation returning 0x%x", this, (uint32_t)*info);
	
	return kIOReturnSuccess;
}

OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  0);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  1);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  2);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  3);
OSMetaClassDefineReservedUnused(IOUSBRootHubDevice,  4);


