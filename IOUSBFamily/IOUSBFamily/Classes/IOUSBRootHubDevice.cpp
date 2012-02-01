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

//================================================================================================
//
//   Globals
//
//================================================================================================
//
// Declare a statically-initialized instance of the class so that its constructor will be called on driver load 
// and its destructor will be called on unload.
static class IOUSBController_ExtraCurrentIOLockClass gExtraCurrentIOLockClass;

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
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
#define _STANDARD_PORT_POWER_IN_SLEEP		super::_expansionData->_standardPortSleepCurrent

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
	OSString *					cardTypeRef = NULL;
	
	USBLog(5, "%s[%p]::start", getName(), this);

	// Only do this for "Built-in" controllers. If we ever are to support PCI cards, then we need to convert
	// this to a dictionary with entries for the different possible controllers.
	cardTypeRef = OSDynamicCast(OSString, provider->getProperty("Card Type"));
	if ( cardTypeRef && cardTypeRef->isEqualTo("Built-in") )
	{
		timeSpec.tv_sec = 5;
		timeSpec.tv_nsec = 0;
		
		_IORESOURCESENTRY = waitForService(serviceMatching("IOResources"), &timeSpec);
	}
	else
	{
		USBLog(6, "IOUSBRootHubDevice[%p]::start - no 'Card Type' property or is NOT 'Built'-in' ", this);
	}
	
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
	USBLog(2, "IOUSBRootHubDevice[%p]::+free", this);
    if (_expansionData)
    {
        IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }
    super::free();
	USBLog(2, "IOUSBRootHubDevice[%p]::-free", this);
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
	IOReturn	kr = kIOReturnSuccess;
    
	// We have to use IOUSBDevice::_expansionData->_commandGate, instead of the copy of it in _commandGate because this method will be called by 
	// our super::start() BEFORE we're able to set the _commandGate to IOUSBDevice::_expansionData->_commandGate.
	
	if ( IOUSBDevice::_expansionData && IOUSBDevice::_expansionData->_commandGate && IOUSBDevice::_expansionData->_workLoop)
	{
		IOCommandGate *	gate = IOUSBDevice::_expansionData->_commandGate;
		IOWorkLoop *	workLoop = IOUSBDevice::_expansionData->_workLoop;
		
		retain();
		workLoop->retain();
		gate->retain();
        
        if (_myPolicyMaker && (_myPolicyMaker->getPowerState() == kIOUSBHubPowerStateLowPower))
        {
            // this is not usually an issue, but i want to make sure it doesn't become one
            USBLog(5, "IOUSBRootHubDevice[%p]::DeviceRequest - doing a device request while in low power mode - should be OK", this);
        }
        
        kr = gate->runAction(GatedDeviceRequest, request, (void*)noDataTimeout, (void*)completionTimeout, completion);       
        if ( kr != kIOReturnSuccess )
        {
            USBLog(2,"IOUSBRootHubDevice[%p]::DeviceRequest GatedDeviceRequest runAction() failed (0x%x)",this, kr);
        }
		
		gate->release();
		workLoop->release();
		release();
	}
    else
    {
        kr = kIOReturnNotResponding;
    }
    
    return kr;
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
	
	IOLockLock(gExtraCurrentIOLockClass.lock);
	
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::RequestExtraPower - _expansionData or _IORESOURCESENTRY is NULL after locking", getName(), this);
		IOLockUnlock(gExtraCurrentIOLockClass.lock);
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
	
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleMaxPortCurrent);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		maxPowerPerPort = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestExtraPower - we have a kAppleMaxPortCurrent with %d", getName(), this, (uint32_t) maxPowerPerPort);
	}
	SAFE_RELEASE_NULL(propertyObject);

	USBLog(5, "%s[%p]::RequestExtraPower - requestedPower = %d, available: %d", getName(), this, (uint32_t)requestedPower, (uint32_t) totalExtraCurrent);
	
	// The power requested is a delta above the USB Spec for the port.  That's why we need to subtract the kUSB2MaxPowerPerPortmA from the maxPowerPerPort value
	if (requestedPower > (maxPowerPerPort-kUSB2MaxPowerPerPort))		// limit requests to the maximum the HW can support
	{
		USBLog(5, "%s[%p]::RequestExtraPower - requestedPower = %d was greater than the maximum per port of %d.  Using that value instead", getName(), this, (uint32_t)requestedPower, (uint32_t) (maxPowerPerPort-kUSB2MaxPowerPerPort));
		requestedPower = maxPowerPerPort-kUSB2MaxPowerPerPort;
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
	
	IOLockUnlock(gExtraCurrentIOLockClass.lock);
	
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
	
	IOLockLock(gExtraCurrentIOLockClass.lock);
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::ReturnExtraPower - _expansionData or _IORESOURCESENTRY is NULL after locking", getName(), this);
		IOLockUnlock(gExtraCurrentIOLockClass.lock);
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

	IOLockUnlock(gExtraCurrentIOLockClass.lock);
}

void			
IOUSBRootHubDevice::InitializeExtraPower(UInt32 maxPortCurrent, UInt32 totalExtraCurrent)
{
#pragma unused (maxPortCurrent, totalExtraCurrent)
	USBLog(1, "%s[%p]::InitializeExtraPower - Obsolete method called", getName(), this);
}


void
IOUSBRootHubDevice::SetSleepCurrent(UInt32 sleepCurrent)
{
	USBLog(5, "%s[%p]::SetSleepCurrent -  %d", getName(), this, (uint32_t)sleepCurrent);
	
	super::SetSleepCurrent(sleepCurrent);
}

UInt32
IOUSBRootHubDevice::GetSleepCurrent()
{
	return super::GetSleepCurrent();
	
}


// The request for sleep is total sleep power, NOT the delta above 500mA.
UInt32
IOUSBRootHubDevice::RequestSleepPower(UInt32 requestedPower)
{
	OSNumber *		numberObject = NULL;
	UInt32			totalExtraSleepCurrent = 0;
	UInt32			maxSleepCurrentPerPort = 0;
	UInt32			extraAllocated = 0;			// Above 500mA
	OSObject *		propertyObject = NULL;
	
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::RequestSleepPower - _expansionData or _IORESOURCESENTRY is NULL", getName(), this);
		return 0;
	}
	
	if (requestedPower == 0)
	{
		USBLog(5, "%s[%p]::RequestSleepPower - asked for 0, returning 0", getName(), this);
		return 0;
	}

	// If we don't have any _standardPortSleepCurrent, then it means that we can't allocate any 
	if ( _STANDARD_PORT_POWER_IN_SLEEP == 0)
	{
		USBLog(5, "%s[%p]::RequestSleepPower - port does not have any _STANDARD_PORT_POWER_IN_SLEEP, returning 0", getName(), this );
		return 0;
	}

	// If we are requesting < _STANDARD_PORT_POWER_IN_SLEEP (i.e. 500mA), then we're good and just give it
	if ( requestedPower <= _STANDARD_PORT_POWER_IN_SLEEP)	
	{
		USBLog(5, "%s[%p]::RequestSleepPower - requested <= _STANDARD_PORT_POWER_IN_SLEEP, returning %d", getName(), this, (uint32_t)requestedPower );
		return requestedPower;
	}

	IOLockLock(gExtraCurrentIOLockClass.lock);
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::RequestSleepPower - _expansionData or _IORESOURCESENTRY is NULL after locking", getName(), this);
		IOLockUnlock(gExtraCurrentIOLockClass.lock);
		return 0;
	}
	
	
	// OK, at this point, we have a request for sleep current that exceeds the "standard" USB load of 500mA, so we need to see if we can give it from our extra.  Note that the
	// request is total current, while the extra is "extra above 500mA"
	
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentExtraInSleep);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		totalExtraSleepCurrent = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestSleepPower - we have a kAppleCurrentExtraInSleep with %d", getName(), this, (uint32_t) totalExtraSleepCurrent);
	}
	SAFE_RELEASE_NULL(propertyObject);
	
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleMaxPortCurrentInSleep);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		maxSleepCurrentPerPort = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestSleepPower - we have a kAppleMaxPortCurrentInSleep with %d", getName(), this, (uint32_t) maxSleepCurrentPerPort);
	}
	SAFE_RELEASE_NULL(propertyObject);
	
	USBLog(5, "%s[%p]::RequestSleepPower - extra requestedPower = %d, available: %d", getName(), this, (uint32_t) (requestedPower-_STANDARD_PORT_POWER_IN_SLEEP), (uint32_t) totalExtraSleepCurrent);
	
	// Will this exceed the max per port during sleep?
	if (requestedPower > maxSleepCurrentPerPort)		// limit requests to the maximum the HW can support
	{
		USBLog(5, "%s[%p]::RequestSleepPower - requestedPower = %d was greater than the maximum per port of %d.  Using that value instead", getName(), this, (uint32_t)requestedPower, (uint32_t) maxSleepCurrentPerPort);
		requestedPower = maxSleepCurrentPerPort;
	}
	
	// Do we have enough extra for this request?
	if ((requestedPower-_STANDARD_PORT_POWER_IN_SLEEP) <= totalExtraSleepCurrent)
	{		
		// honor the request if possible
		extraAllocated = (requestedPower-_STANDARD_PORT_POWER_IN_SLEEP);
		totalExtraSleepCurrent -= extraAllocated;
		
		USBLog(5, "%s[%p]::RequestSleepPower - updating kAppleCurrentExtraInSleep to %d", getName(), this, (uint32_t) totalExtraSleepCurrent);
		(_IORESOURCESENTRY)->setProperty(kAppleCurrentExtraInSleep, totalExtraSleepCurrent, 32);
	}
	
	USBLog(5, "%s[%p]::RequestSleepPower - extraAllocated = %d", getName(), this, (uint32_t)extraAllocated);
	
	IOLockUnlock(gExtraCurrentIOLockClass.lock);

	return extraAllocated+_STANDARD_PORT_POWER_IN_SLEEP;
}


// The power here is overall, so we have to subtract the standard load
void
IOUSBRootHubDevice::ReturnSleepPower(UInt32 returnedPower)
{
	OSNumber *		numberObject = NULL;
	OSObject *		propertyObject = NULL;
	UInt32			powerAvailable = 0;
	
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::ReturnSleepPower - _expansionData or _IORESOURCESENTRY is NULL", getName(), this);
		return;
	}
	
	USBLog(5, "%s[%p]::ReturnSleepPower - returning = %d", getName(), this, (uint32_t)returnedPower);
	
	IOLockLock(gExtraCurrentIOLockClass.lock);
	if ( (_expansionData == NULL) || ( _IORESOURCESENTRY == NULL ))
	{
		USBLog(5, "%s[%p]::ReturnSleepPower - _expansionData or _IORESOURCESENTRY is NULL after locking", getName(), this);
		IOLockUnlock(gExtraCurrentIOLockClass.lock);
		return;
	}
	
	
	propertyObject = (_IORESOURCESENTRY)->copyProperty(kAppleCurrentExtraInSleep);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		powerAvailable = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::ReturnSleepPower - we have a kAppleCurrentExtraInSleep with %d", getName(), this, (uint32_t) powerAvailable);
	}
	SAFE_RELEASE_NULL(propertyObject);
	
	if (returnedPower > _STANDARD_PORT_POWER_IN_SLEEP)
	{
		powerAvailable += (returnedPower-_STANDARD_PORT_POWER_IN_SLEEP);
		USBLog(5, "%s[%p]::ReturnSleepPower - setting kAppleCurrentExtraInSleep to %d", getName(), this, (uint32_t) powerAvailable);
		(_IORESOURCESENTRY)->setProperty(kAppleCurrentExtraInSleep, powerAvailable, 32);
	}
	
	IOLockUnlock(gExtraCurrentIOLockClass.lock);
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
	if ( theString)
        isBuiltIn = theString->isEqualTo("Built-in");
    
 	SAFE_RELEASE_NULL(propertyObject);

	if (isBuiltIn)
		*info |=  (1 << kUSBInformationRootHubisBuiltIn);
	
	
	USBLog(2, "IOUSBRootHubDevice[%p]::GetDeviceInformation returning 0x%x", this, (uint32_t)*info);
	
	return kIOReturnSuccess;
}

void			
IOUSBRootHubDevice::SendExtraPowerMessage(UInt32 type, UInt32 returnedPower)
{
	// Tell all the EHCI Root Hub Simulations to attempt to give extra power
	OSIterator *		rootHubDeviceiterator	= NULL;
	OSIterator *		iterator				= NULL;
	OSObject *			obj						= NULL;
	
	USBLog(5, "IOUSBRootHubDevice[%p]::SendExtraPowerMessage - type: %d, argument: %d", this, (uint32_t)type, (uint32_t) returnedPower);
	
	rootHubDeviceiterator = IOService::getMatchingServices(serviceMatching("IOUSBRootHubDevice"));
	if ( rootHubDeviceiterator != NULL )
	{
		
		while ( (obj = rootHubDeviceiterator->getNextObject()) )
		{
			IOService *                service = ( IOService * ) obj;

			USBLog(7, "%s[%p]::SendExtraPowerMessage - found %s (%p)", getName(), this, service->getName(), service);
			iterator = service->getParentEntry(gIOServicePlane)->getChildIterator(gIOServicePlane);
			if ( !iterator )
			{
				USBLog(5, "%s[%p]::SendExtraPowerMessage - could not getChildIterator", getName(), this);
				continue;
			}
			
			if (iterator)
			{
				OSObject *next;
				
				while( (next = iterator->getNextObject()) )
				{
					IOUSBDevice *aDevice = OSDynamicCast(IOUSBDevice, next);
					if ( aDevice )
					{
						if ( type == kUSBPowerRequestWakeRelease )
						{
							USBLog(5, "%s[%p]::SendExtraPowerMessage - sending  kIOUSBMessageReleaseExtraCurrent to %s", getName(), this, aDevice->getName());
							aDevice->messageClients(kIOUSBMessageReleaseExtraCurrent,  &returnedPower, sizeof(UInt32));
						}
						else if ( type == kUSBPowerRequestWakeReallocate )
						{
							USBLog(5, "%s[%p]::SendExtraPowerMessage - sending  kIOUSBMessageReallocateExtraCurrent to %s", getName(), this, aDevice->getName());
							aDevice->messageClients(kIOUSBMessageReallocateExtraCurrent, NULL, 0);
						}

					}
					
				}
				iterator->release();
			}
		}
		rootHubDeviceiterator->release();
	}
	else 
	{
		USBLog(5, "%s[%p]::RequestExtraPower - Could not find any IOUSBRootHubDevice's", getName(), this);
	}

}


OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  0);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  1);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  2);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  3);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  4);


