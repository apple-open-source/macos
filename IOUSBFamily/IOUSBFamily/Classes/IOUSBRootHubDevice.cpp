/*
 * Copyright © 1998-2012 Apple Inc.  All rights reserved.
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

#define _MYCONTROLLERSPEED					_expansionData->_myControllerSpeed
#define _PROVIDESEXTRACURRENT				_expansionData->_builtInController
#define _HASBUILTINPROPERTY					_expansionData->_hasBuiltInProperty
#define _HASTUNNELLEDPROPERTY				_expansionData->_hasTunnelledProperty

// From our superclass
#define _STANDARD_PORT_POWER_IN_SLEEP		super::_expansionData->_standardPortSleepCurrent
#define _UNCONNECTEDEXTERNALPORTS			super::_expansionData->_unconnectedExternalPorts



//================================================================================================

OSDefineMetaClassAndStructors( IOUSBRootHubDevice, IOUSBHubDevice )

//================================================================================================

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
		
	if (GetSpeed() == kUSBDeviceSpeedSuper)
		characteristics |= kIOUSBHubDeviceIsOnSuperSpeedBus;
    
	SetHubCharacteristics(characteristics);

	return true;
}



bool
IOUSBRootHubDevice::start(IOService *provider)
{
	bool						returnValue = false;
	OSString *					cardTypeRef = NULL;
	
	USBLog(5, "%s[%p]::start", getName(), this);

	_controller = OSDynamicCast(IOUSBController, provider);

	if(_controller)
	{
		IOUSBControllerV3		*v3Bus = OSDynamicCast(IOUSBControllerV3, _controller);
		if(v3Bus && _expansionData)
		{
			_MYCONTROLLERSPEED = v3Bus->_controllerSpeed;
			USBLog(5, "%s[%p]::start - _MYCONTROLLERSPEED: %d", getName(), this, _MYCONTROLLERSPEED);
		}
		else 
		{
			USBLog(5, "%s[%p]::start - _MYCONTROLLERSPEED is zero!!", getName(), this);
		}
	}
	else 
	{
		USBLog(5, "%s[%p]::start - _controller is NULL", getName(), this);
	}

	
	// Only do this for "Built-in" controllers. If we ever are to support PCI cards, then we need to convert
	// this to a dictionary with entries for the different possible controllers.
	cardTypeRef = OSDynamicCast(OSString, provider->getProperty("Card Type"));
	if ( cardTypeRef && cardTypeRef->isEqualTo("Built-in") )
	{
		_PROVIDESEXTRACURRENT = true;
		_HASBUILTINPROPERTY = true;
	}
	else
	{
		USBLog(6, "IOUSBRootHubDevice[%p]::start - no 'Card Type' property or is NOT 'Built'-in' ", this);
		_PROVIDESEXTRACURRENT = false;
		_HASBUILTINPROPERTY = false;
	}
	
	// If on Thunderbolt, then we don't support extra current
	if ( _controller && _controller->getProvider() && ((_controller->getProvider())->getProperty(kIOPCITunnelledKey) == kOSBooleanTrue) )
	{
		_PROVIDESEXTRACURRENT = false;
		_HASTUNNELLEDPROPERTY = true;
	}
	else
	{
		_HASTUNNELLEDPROPERTY = false;
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
	
	if (!isInactive() && IOUSBDevice::_expansionData && IOUSBDevice::_expansionData->_commandGate && IOUSBDevice::_expansionData->_workLoop)
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
	IOReturn	err = kIOReturnSuccess;
    UInt16		theRequest;
    UInt8		dType, dIndex;

    
    if (!request)
        return(kIOReturnBadArgument);

    theRequest = (request->bRequest << 8) | request->bmRequestType;

	USBLog(7, "+IOUSBRootHubDevice[%p]::DeviceRequestWorker speed: %d _MYCONTROLLERSPEED: %d theRequest: %d", this, _speed, _MYCONTROLLERSPEED, theRequest);

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
				{
					if(_MYCONTROLLERSPEED == kUSBDeviceSpeedSuper)
					{
						RHCommandHeaderPtr command = (RHCommandHeaderPtr)request->pData;
						command->request = 0;
						command->request |=	(_speed << kUSBSpeed_Shift) & kUSBSpeed_Mask;
					}
                    err = _controller->GetRootHubDeviceDescriptor((IOUSBDeviceDescriptor*)request->pData);
                    request->wLenDone = sizeof(IOUSBDeviceDescriptor);
                    break;
				}

                case kUSBConfDesc:
                {
                    OSData *fullDesc = OSData::withCapacity(1024);
                    UInt16 newLength;
                    
                    err = _controller->GetRootHubConfDescriptor(fullDesc);
 					if ( (err == kIOReturnSuccess) && (fullDesc->getLength() > 0) )
					{
						newLength = fullDesc->getLength();
						if (newLength < request->wLength)
							request->wLength = newLength;
						bcopy(fullDesc->getBytesNoCopy(), (char *)request->pData, request->wLength);
						request->wLenDone = request->wLength;
					}
                    fullDesc->release();
                    break;
                }

				case kUSBBOSDescriptor:
                {
					IOUSBControllerV3		*v3Bus = OSDynamicCast(IOUSBControllerV3, _controller);
                    
					err = kIOReturnUnsupported;
					
					if ( v3Bus )
					{
                        OSData *fullDesc = OSData::withCapacity(1024);
                        UInt16 newLength;

						err = v3Bus->GetRootHubBOSDescriptor(fullDesc);
						if ( (err == kIOReturnSuccess) && (fullDesc->getLength() > 0) )
						{
							newLength = fullDesc->getLength();
							if (newLength < request->wLength)
								request->wLength = newLength;
							bcopy(fullDesc->getBytesNoCopy(), (char *)request->pData, request->wLength);
							request->wLenDone = request->wLength;
						}
						else 
						{
							request->wLenDone = 0;
						}
						
						fullDesc->release();
					}
                    break;
                }
                case kUSBStringDesc:
                {
                    OSData *fullDesc = OSData::withCapacity(1024);
                    UInt16 newLength;
					unsigned int offset = 0;
                    
					if(_MYCONTROLLERSPEED == kUSBDeviceSpeedSuper)
					{
						RHCommandHeader command;
						command.request = 0;
						command.request |=	(_speed << kUSBSpeed_Shift) & kUSBSpeed_Mask;
						offset = sizeof(UInt32);
						fullDesc->appendBytes(&command, offset);
					}
                    err = _controller->GetRootHubStringDescriptor((request->wValue & 0x00ff), fullDesc);
					if ( (err == kIOReturnSuccess) && (fullDesc->getLength() > 0) )
					{
						newLength = fullDesc->getLength();
						if (newLength < request->wLength)
							request->wLength = newLength;
						bcopy(fullDesc->getBytesNoCopy(offset,request->wLength), (char *)request->pData, request->wLength);
						request->wLenDone = request->wLength;
					}
                    fullDesc->release();
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
			if(_MYCONTROLLERSPEED == kUSBDeviceSpeedSuper)
			{
				USBLog(5, "IOUSBRootHubDevice[%p]::DeviceRequestWorker kSetDeviceFeature unimplemented for SS RH for speed: %d ", this, _speed);
				RHCommandHeader command;
				command.request = 0;
				command.request |=	(_speed << kUSBSpeed_Shift) & kUSBSpeed_Mask;
			}
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
			if(_MYCONTROLLERSPEED == kUSBDeviceSpeedSuper)
			{
				USBLog(5, "IOUSBRootHubDevice[%p]::DeviceRequestWorker kClearHubFeature unimplemented for SS RH for speed: %d ", this, _speed);
				RHCommandHeader command;
				command.request = 0;
				command.request |=	(_speed << kUSBSpeed_Shift) & kUSBSpeed_Mask;
			}
            if (request->wIndex == 0)
                err = _controller->ClearRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kClearPortFeature:
			if(_MYCONTROLLERSPEED == kUSBDeviceSpeedSuper)
			{
				request->wValue <<= 8;
				request->wValue	|=	(_speed << kUSBSpeed_Shift) & kUSBSpeed_Mask;
			}
            err = _controller->ClearRootHubPortFeature(request->wValue, request->wIndex);
            break;

        case kGetPortState:
            if ((request->wValue == 0) && (request->pData != 0))
                err = _controller->GetRootHubPortState((UInt8 *)request->pData, request->wIndex);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetHubDescriptor:
		{
			IOUSBControllerV3		*v3Bus = OSDynamicCast(IOUSBControllerV3, _controller);
			
            if ( (request->wValue == ((kUSBHubDescriptorType << 8) + 0)) && (request->pData != 0))
            {
                err = _controller->GetRootHubDescriptor((IOUSBHubDescriptor *)request->pData);
                request->wLenDone = sizeof(IOUSBHubDescriptor);
            }
            else if ( v3Bus && (request->wValue == ((kUSB3HubDescriptorType << 8) + 0)) && (request->pData != 0))
            {
                err = v3Bus->GetRootHub3Descriptor((IOUSB3HubDescriptor *)request->pData);
                request->wLenDone = sizeof(IOUSB3HubDescriptor);
            }
            else
                err = kIOReturnBadArgument;
		}
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
				if(_MYCONTROLLERSPEED == kUSBDeviceSpeedSuper)
				{
					RHCommandHeaderPtr command = (RHCommandHeaderPtr)request->pData;
					command->request = 0;
					command->request |=	(_speed << kUSBSpeed_Shift) & kUSBSpeed_Mask;
				}
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
			if(_MYCONTROLLERSPEED == kUSBDeviceSpeedSuper)
			{
				USBLog(5, "IOUSBRootHubDevice[%p]::DeviceRequestWorker kSetHubFeature unimplemented for SS RH for speed: %d ", this, _speed);
				RHCommandHeader command;
				command.request = 0;
				command.request |=	(_speed << kUSBSpeed_Shift) & kUSBSpeed_Mask;
			}
            if (request->wIndex == 0)
                err = _controller->SetRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kSetPortFeature:
			if(_MYCONTROLLERSPEED == kUSBDeviceSpeedSuper)
			{
				request->wValue <<= 8;
				request->wValue	|=	(_speed << kUSBSpeed_Shift) & kUSBSpeed_Mask;
			}
            err = _controller->SetRootHubPortFeature(request->wValue, request->wIndex);
            break;
			
        case kSetHubDepth:
			USBLog(1, "%s[%p]::DeviceRequestWorker  unimplemented kSetHubDepth", getName(), this);
			// err = _controller->SetHubDepth(request->wValue, request->wIndex);
            break;
	
		case kGetPortErrorCount:
		{
			IOUSBControllerV3		*v3Bus = OSDynamicCast(IOUSBControllerV3, _controller);
			if ( v3Bus)
			{
				if (request->wValue == 0 && request->wLength == 2)
					err = v3Bus->GetRootHubPortErrorCount(request->wIndex, (UInt16 *)request->pData);
				else
					err = kIOReturnBadArgument;
			}
			else
				err = kIOReturnBadArgument;
			break;
		}
        default:
 			USBLog(3, "%s[%p]::DeviceRequestWorker  unimplemented request 0x%x", getName(), this, theRequest);
           err = kIOReturnBadArgument;

    }
    return(err);
}


bool
IOUSBRootHubDevice::IsRootHub(void)
{
	return true;
}

#pragma mark Extra Power APIs


UInt32
IOUSBRootHubDevice::RequestExtraPower(UInt32 type, UInt32 requestedPower)
{
	UInt32	returnValue = 0;
	bool	retry = false;
	
	if ( (_expansionData == NULL) || (_PROVIDESEXTRACURRENT == false) )
	{
		USBLog(5, "%s[%p]::RequestExtraPower - _expansionData is NULL or _PROVIDESEXTRACURRENT is false", getName(), this);
		return 0;
	}

	USBLog(5, "%s[%p]::RequestExtraPower type: %d, requested %d, inGate: %d", getName(), this, (uint32_t)type, (uint32_t) requestedPower, IOUSBDevice::_expansionData->_workLoop->inGate());
	if ( type == kUSBPowerDuringWake || type == kUSBPowerDuringWakeRevocable || type == kUSBPowerDuringWakeUSB3)
	{
		returnValue = RequestExtraWakePower( type, requestedPower, &retry );
		if ( retry && (returnValue == 0) )
		{
 			AbsoluteTime		deadline;
            IOReturn        	err;
			UInt32				flag = 0;
			
			SendExtraPowerMessage( kUSBPowerRequestWakeRelease, requestedPower );
			
			if ( IOUSBDevice::_expansionData->_workLoop->inGate())
			{
				clock_interval_to_deadline(100, kMillisecondScale, &deadline);
				
				USBLog(5, "%s[%p]::RequestExtraPower  we didn't get our %d, but were told to retry.  calling commandSleep for 100ms, and then retrying", getName(), this, (uint32_t) requestedPower);
				err = _commandGate->commandSleep(&flag, deadline, THREAD_ABORTSAFE);
				if (err != THREAD_TIMED_OUT)
				{
					USBLog(3, "%s[%p]::RequestExtraPower  commandSleep woke up with a 0x%x", getName(), this, (uint32_t) err);
				}
			}
			else
			{
				USBLog(5, "%s[%p]::RequestExtraPower  we didn't get our %d, but were told to retry.  Sleeping 100ms and trying again", getName(), this, (uint32_t) requestedPower);
				IOSleep(100);
			}
			
			retry = false;
			returnValue = RequestExtraWakePower( type, requestedPower, &retry );
			SendExtraPowerMessage( kUSBPowerRequestWakeReallocate, requestedPower );
		}
	}
	else if ( type == kUSBPowerDuringSleep )
	{
		returnValue = RequestSleepPower( requestedPower );
	}
	else if ( type == kUSBPowerRequestWakeReallocate || type == kUSBPowerRequestSleepReallocate)
	{
		SendExtraPowerMessage( type, requestedPower );
	}

	return returnValue;
}


UInt32
IOUSBRootHubDevice::RequestExtraWakePower(UInt32 wakeType, UInt32 requestedPower, bool *retry)
{
	OSNumber *		numberObject = NULL;
	UInt32			currentRequiredForUSB3Devices = 0;
	UInt32			totalExtraCurrent = 0;
	UInt32			totalRevocableExtraCurrent = 0;
	UInt32			maxPowerPerPort = 0;
	UInt32			extraAllocated = 0;
	UInt32			unconnectedSSPorts = 0;
	SInt32			adjustedUnconnectedPorts = 0;
	OSObject *		propertyObject = NULL;
	IOService *		resourceService = getResourceService();
	
	if ( (_expansionData == NULL) || (_PROVIDESEXTRACURRENT == false) )
	{
		USBLog(5, "%s[%p]::RequestExtraWakePower - _expansionData is NULL or _PROVIDESEXTRACURRENT is false", getName(), this);
		return 0;
	}
	
	IOLockLock(gExtraCurrentIOLockClass.lock);
	
	if ( _expansionData == NULL)
	{
		USBLog(5, "%s[%p]::RequestExtraWakePower - _expansionData is NULL after locking", getName(), this);
		IOLockUnlock(gExtraCurrentIOLockClass.lock);
		return 0;
	}
	
	propertyObject = resourceService->copyProperty(kAppleCurrentExtra);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		totalExtraCurrent = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestExtraWakePower - we have a kAppleCurrentExtra with %d", getName(), this, (uint32_t) totalExtraCurrent);
	}
	OSSafeReleaseNULL(propertyObject);
	
	propertyObject = resourceService->copyProperty(kAppleMaxPortCurrent);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		maxPowerPerPort = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestExtraWakePower - we have a kAppleMaxPortCurrent with %d", getName(), this, (uint32_t) maxPowerPerPort);
	}
	OSSafeReleaseNULL(propertyObject);

	propertyObject = resourceService->copyProperty(kAppleRevocableExtraCurrent);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		totalRevocableExtraCurrent = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestExtraWakePower - we have a totalRevocableExtraCurrent with %d", getName(), this, (uint32_t) totalRevocableExtraCurrent);
	}
	OSSafeReleaseNULL(propertyObject);
	
	// Get the current number of unconnected XHCI ports.  We need to have 400mA per port available for those.  However, we have to adjust this by taking
	// into account the # of non-SS external ports that are using extra current.  The idea here is that if a non-SS external port is using extra current, it
	// is using at least 400 mA, so that if you were to unplug it and plug in a SS device, we will get at least the extra 400 mA from the unplug.  Thus we don't
	// have to reserve 400 mA for those ports. Note that if the assumption that devices request at least 400mA is broken, then things will break!  (We would need
	// to keep track of ports that have a total of 400 or more ONLY).
	unconnectedSSPorts = UpdateUnconnectedExternalPorts(0);
	adjustedUnconnectedPorts = unconnectedSSPorts - IOUSBController::gExternalNonSSPortsUsingExtraCurrent;
	
	// If we don't support USB3 ports, our adjustedUnconnectedSSPort will be negative.  In that case, set it to 0 so that we don't reserve any
	// current for USB3 devices.
	if (adjustedUnconnectedPorts < 0)
		adjustedUnconnectedPorts = 0;
	
	currentRequiredForUSB3Devices = (kUSB3MaxPowerPerPort - kUSB2MaxPowerPerPort) * adjustedUnconnectedPorts;
	
	USBLog(5, "%s[%p]::RequestExtraWakePower type: %s - requestedPower = %d, available: %d, unconnected external SS ports: %d, external NonSS ports using extra: %d, thus we need to have %d for possible SS ports", getName(), this, wakeType == kUSBPowerDuringWake ? "kUSBPowerDuringWake" : (wakeType == kUSBPowerDuringWakeRevocable ? "kUSBPowerDuringWakeRevocable" : "kUSBPowerDuringWakeUSB3"), (uint32_t)requestedPower, (uint32_t) totalExtraCurrent, (uint32_t)unconnectedSSPorts, (uint32_t)IOUSBController::gExternalNonSSPortsUsingExtraCurrent, (uint32_t)currentRequiredForUSB3Devices);
	
	// The power requested is a delta above the USB Spec for the port.  That's why we need to subtract the kUSB2MaxPowerPerPortmA from the maxPowerPerPort value
	if (requestedPower > (maxPowerPerPort-kUSB2MaxPowerPerPort))		// limit requests to the maximum the HW can support
	{
		USBLog(5, "%s[%p]::RequestExtraWakePower - requestedPower = %d was greater than the maximum per port of %d.  Using that value instead", getName(), this, (uint32_t)requestedPower, (uint32_t) (maxPowerPerPort-kUSB2MaxPowerPerPort));
		requestedPower = maxPowerPerPort-kUSB2MaxPowerPerPort;
	}
	
	if (requestedPower <= totalExtraCurrent)
	{		
		bool	allocate = true;
		
		// If this is NOT a revocable or a USB3 power request, we need to make sure that after giving it away, we still have enough power in the "extra" or in the "revocable" 
		// to give to a future USB3 device.  If we don't, then we need to deny this request.
		if (wakeType == kUSBPowerDuringWake)
		{
			if ( (totalExtraCurrent + totalRevocableExtraCurrent - requestedPower) < currentRequiredForUSB3Devices)
			{
				// If we gave this power away, we would not be able to give power to a USB3 device that could be plugged in!
				USBLog(5, "%s[%p]::RequestExtraWakePower - requestedPower = %d, we have %d extra, %d revocable, but need to have %d for USB3 devices, so we can't allocate it ", getName(), this, (uint32_t)requestedPower, (uint32_t)totalExtraCurrent, (uint32_t)totalRevocableExtraCurrent, (uint32_t)currentRequiredForUSB3Devices);
				allocate = false;
				*retry = false;
			}
		}
		
		if ( allocate )
		{
			// honor the request
			extraAllocated = requestedPower;
			totalExtraCurrent -= extraAllocated;
			
			USBLog(5, "%s[%p]::RequestExtraWakePower - setting kAppleCurrentExtra to %d", getName(), this, (uint32_t) totalExtraCurrent);
			resourceService->setProperty(kAppleCurrentExtra, totalExtraCurrent, 32);
			
			// If this was revocable extra current, then set the amount in our global
			if (wakeType == kUSBPowerDuringWakeRevocable)
			{
				totalRevocableExtraCurrent += requestedPower;
				USBLog(5, "%s[%p]::RequestExtraWakePower - setting kAppleRevocableExtraCurrent to %d", getName(), this, (uint32_t) totalRevocableExtraCurrent);
				resourceService->setProperty(kAppleRevocableExtraCurrent, totalRevocableExtraCurrent, 32);
			}
		}
	}
	else
	{
		if ( (wakeType != kUSBPowerDuringWakeRevocable) &&  (requestedPower <= (totalExtraCurrent + totalRevocableExtraCurrent)) )
		{
			// We have a request for non-revocable extra current that can be satisified by revoking some current. Set the retry to
			// true so that the caller can retry
			*retry = true;
		}
	}

	// this method may be overriden by the IOUSBRootHubDevice class to implement this
	USBLog(5, "%s[%p]::RequestExtraWakePower - extraAllocated = %d, retry = %d", getName(), this, (uint32_t)extraAllocated, *retry);
	
	IOLockUnlock(gExtraCurrentIOLockClass.lock);
	
	return extraAllocated;
}



IOReturn
IOUSBRootHubDevice::ReturnExtraPower(UInt32 type, UInt32 returnedPower)
{
	IOReturn	kr = kIOReturnSuccess;
	
	if ( (_expansionData == NULL) || (_PROVIDESEXTRACURRENT == false) )
	{
		USBLog(5, "%s[%p]::ReturnExtraPower - _expansionData is NULL or _PROVIDESEXTRACURRENT is false", getName(), this);
		return 0;
	}

	USBLog(5, "%s[%p]::ReturnExtraPower type: %d, returnedPower %d", getName(), this, (uint32_t)type, (uint32_t) returnedPower);
	

	if ( type == kUSBPowerDuringWake || type == kUSBPowerDuringWakeRevocable || type == kUSBPowerDuringWakeUSB3)
	{
		ReturnExtraWakePower(type, returnedPower);
	}
	else if ( type == kUSBPowerDuringSleep )
	{
		ReturnSleepPower( returnedPower );
	}
	else if ( type == kUSBPowerRequestWakeRelease || type == kUSBPowerRequestSleepRelease )
	{
		SendExtraPowerMessage( type, returnedPower );
	}
	else
		kr = kIOReturnBadArgument;
	
	return kr;
}

void
IOUSBRootHubDevice::ReturnExtraWakePower(UInt32 wakeType, UInt32 returnedPower)
{
	OSNumber *		numberObject = NULL;
	OSObject *		propertyObject = NULL;
	UInt32			totalExtraCurrent = 0;
	UInt32			totalRevocableExtraCurrent = 0;
	UInt32			unconnectedSSPorts = 0;
	IOService *		resourceService = getResourceService();
	
	USBLog(5, "%s[%p]::ReturnExtraPower - returning = %d", getName(), this, (uint32_t)returnedPower);
	
	if ( (_expansionData == NULL) || (_PROVIDESEXTRACURRENT == false) )
	{
		USBLog(5, "%s[%p]::ReturnExtraPower - _expansionData is NULL or _PROVIDESEXTRACURRENT is false", getName(), this);
		return;
	}
	
	IOLockLock(gExtraCurrentIOLockClass.lock);
	if (_expansionData == NULL)
	{
		USBLog(5, "%s[%p]::ReturnExtraPower - _expansionData is NULL after locking", getName(), this);
		IOLockUnlock(gExtraCurrentIOLockClass.lock);
		return;
	}
	
	// Get the current number of unconnected XHCI ports.  We need to have 400mA per port available for those
	unconnectedSSPorts = UpdateUnconnectedExternalPorts(0);
	
	propertyObject = resourceService->copyProperty(kAppleCurrentExtra);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		totalExtraCurrent = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::ReturnExtraPower - we have a kAppleCurrentExtra with %d", getName(), this, (uint32_t) totalExtraCurrent);
	}
	OSSafeReleaseNULL(propertyObject);
		
	propertyObject = resourceService->copyProperty(kAppleRevocableExtraCurrent);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		totalRevocableExtraCurrent = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::ReturnExtraPower - we have a totalRevocableExtraCurrent with %d", getName(), this, (uint32_t) totalRevocableExtraCurrent);
	}
	OSSafeReleaseNULL(propertyObject);

	if (returnedPower > 0)
	{
		totalExtraCurrent += returnedPower;
		USBLog(5, "%s[%p]::ReturnExtraPower - setting kAppleCurrentExtra to %d", getName(), this, (uint32_t) totalExtraCurrent);
		resourceService->setProperty(kAppleCurrentExtra, totalExtraCurrent, 32);

		// If this was revocable extra current, then set the amount in our global
		if (wakeType == kUSBPowerDuringWakeRevocable)
		{
			totalRevocableExtraCurrent -= returnedPower;
			USBLog(5, "%s[%p]::ReturnExtraPower - setting kAppleRevocableExtraCurrent to %d", getName(), this, (uint32_t) totalRevocableExtraCurrent);
			resourceService->setProperty(kAppleRevocableExtraCurrent, totalRevocableExtraCurrent, 32);
		}
}

	USBLog(5, "%s[%p]::ReturnExtraWakePower type: %s - returnedPower = %d, new available: %d, unconnected external SS ports: %d, thus we need to have %d for them", getName(), this, wakeType == kUSBPowerDuringWake ? "kUSBPowerDuringWake" : (wakeType == kUSBPowerDuringWakeRevocable ? "kUSBPowerDuringWakeRevocable" : "kUSBPowerDuringWakeUSB3"), (uint32_t)returnedPower, (uint32_t) totalExtraCurrent, (uint32_t)unconnectedSSPorts, (uint32_t)(400*unconnectedSSPorts));
	
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
	IOService *		resourceService = getResourceService();
	
	if ( (_expansionData == NULL) || (_PROVIDESEXTRACURRENT == false) )
	{
		USBLog(5, "%s[%p]::RequestSleepPower - _expansionData is NULL or _PROVIDESEXTRACURRENT is false", getName(), this);
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
	if (_expansionData == NULL)
	{
		USBLog(5, "%s[%p]::RequestSleepPower - _expansionData is NULL after locking", getName(), this);
		IOLockUnlock(gExtraCurrentIOLockClass.lock);
		return 0;
	}
	
	
	// OK, at this point, we have a request for sleep current that exceeds the "standard" USB load of 500mA, so we need to see if we can give it from our extra.  Note that the
	// request is total current, while the extra is "extra above 500mA"
	
	propertyObject = resourceService->copyProperty(kAppleCurrentExtraInSleep);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		totalExtraSleepCurrent = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestSleepPower - we have a kAppleCurrentExtraInSleep with %d", getName(), this, (uint32_t) totalExtraSleepCurrent);
	}
	OSSafeReleaseNULL(propertyObject);
	
	propertyObject = resourceService->copyProperty(kAppleMaxPortCurrentInSleep);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		maxSleepCurrentPerPort = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::RequestSleepPower - we have a kAppleMaxPortCurrentInSleep with %d", getName(), this, (uint32_t) maxSleepCurrentPerPort);
	}
	OSSafeReleaseNULL(propertyObject);
	
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
		resourceService->setProperty(kAppleCurrentExtraInSleep, totalExtraSleepCurrent, 32);
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
	IOService *		resourceService = getResourceService();
	
	if ( (_expansionData == NULL) || (_PROVIDESEXTRACURRENT == false) )
	{
		USBLog(5, "%s[%p]::ReturnSleepPower - _expansionData is NULL or _PROVIDESEXTRACURRENT is false", getName(), this);
		return;
	}
	
	USBLog(5, "%s[%p]::ReturnSleepPower - returning = %d", getName(), this, (uint32_t)returnedPower);
	
	IOLockLock(gExtraCurrentIOLockClass.lock);
	if (_expansionData == NULL)
	{
		USBLog(5, "%s[%p]::ReturnSleepPower - _expansionData is NULL after locking", getName(), this);
		IOLockUnlock(gExtraCurrentIOLockClass.lock);
		return;
	}
	
	propertyObject = resourceService->copyProperty(kAppleCurrentExtraInSleep);
	numberObject = OSDynamicCast(OSNumber, propertyObject);
	if (numberObject)
	{
		powerAvailable = numberObject->unsigned32BitValue();
		USBLog(5, "%s[%p]::ReturnSleepPower - we have a kAppleCurrentExtraInSleep with %d", getName(), this, (uint32_t) powerAvailable);
	}
	OSSafeReleaseNULL(propertyObject);
	
	if (returnedPower > _STANDARD_PORT_POWER_IN_SLEEP)
	{
		powerAvailable += (returnedPower-_STANDARD_PORT_POWER_IN_SLEEP);
		USBLog(5, "%s[%p]::ReturnSleepPower - setting kAppleCurrentExtraInSleep to %d", getName(), this, (uint32_t) powerAvailable);
		resourceService->setProperty(kAppleCurrentExtraInSleep, powerAvailable, 32);
	}
	
	IOLockUnlock(gExtraCurrentIOLockClass.lock);
}



IOReturn
IOUSBRootHubDevice::GetDeviceInformation(UInt32 *info)
{
	*info = 0;
	
	if ( _expansionData == NULL )
	{
		USBLog(5, "%s[%p]::GetDeviceInformation - _expansionData is NULL", getName(), this);
		return kIOReturnNoDevice;
	}
	
	*info =(( 1 << kUSBInformationDeviceIsCaptiveBit ) |
			( 1 << kUSBInformationDeviceIsInternalBit ) |
			( 1 << kUSBInformationDeviceIsConnectedBit ) |
			( 1 << kUSBInformationDeviceIsEnabledBit ) |
			( 1 << kUSBInformationDeviceIsRootHub )
			);
	
	if (_HASBUILTINPROPERTY)
	{
		*info |=  (1 << kUSBInformationRootHubisBuiltIn);
	}
	
	if (_HASTUNNELLEDPROPERTY)
	{
		*info |=  (1 << kUSBInformationDeviceIsOnThunderboltBit);
	}

	USBLog(6, "IOUSBRootHubDevice[%p]::GetDeviceInformation returning 0x%x", this, (uint32_t)*info);
	
	return kIOReturnSuccess;
}

void			
IOUSBRootHubDevice::SendExtraPowerMessage(UInt32 type, UInt32 returnedPower)
{
	// Tell all the EHCI Root Hub Simulations to attempt to give extra power
	OSIterator *		rootHubDeviceiterator	= NULL;
	OSIterator *		iterator				= NULL;
	OSObject *			obj						= NULL;
	
	USBLog(6, "IOUSBRootHubDevice[%p]::SendExtraPowerMessage - type: 0x%x, argument: %d", this, (uint32_t)type, (uint32_t) returnedPower);
	
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
							USBLog(7, "%s[%p]::SendExtraPowerMessage - sending  kIOUSBMessageReleaseExtraCurrent to %s", getName(), this, aDevice->getName());
							aDevice->messageClients(kIOUSBMessageReleaseExtraCurrent,  &returnedPower, sizeof(UInt32));
						}
						else if ( type == kUSBPowerRequestWakeReallocate )
						{
							USBLog(7, "%s[%p]::SendExtraPowerMessage - sending  kIOUSBMessageReallocateExtraCurrent to %s", getName(), this, aDevice->getName());
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
		USBLog(5, "%s[%p]::RequestExtraWakePower - Could not find any IOUSBRootHubDevice's", getName(), this);
	}

}

OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  0);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  1);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  2);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  3);
OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  4);


