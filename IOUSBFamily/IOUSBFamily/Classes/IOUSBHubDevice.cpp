/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2006 Apple Computer, Inc.  All Rights Reserved.
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
#include <IOKit/IOKitKeys.h>
#include <IOKit/usb/IOUSBLog.h>

#include "IOUSBHubDevice.h"

/* Convert USBLog to use kprintf debugging */
#ifndef IOUSBHUBDEVICE_USE_KPRINTF
#define IOUSBHUBDEVICE_USE_KPRINTF 0
#endif

#if IOUSBHUBDEVICE_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBHUBDEVICE_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super	IOUSBDevice

#define _MAXPORTCURRENT				_expansionData->_maxPortCurrent
#define _TOTALEXTRACURRENT			_expansionData->_totalExtraCurrent
#define	_TOTALSLEEPCURRENT			_expansionData->_totalSleepCurrent
#define	_CANREQUESTEXTRAPOWER		_expansionData->_canRequestExtraPower
#define	_EXTRAPOWERFORPORTS			_expansionData->_extraPowerForPorts
#define	_EXTRAPOWERALLOCATED		_expansionData->_extraPowerAllocated
#define _USBPLANE_PARENT			super::_expansionData->_usbPlaneParent


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors( IOUSBHubDevice, IOUSBDevice )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOUSBHubDevice*
IOUSBHubDevice::NewHubDevice()
{
	IOUSBHubDevice *me = new IOUSBHubDevice;
	
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
IOUSBHubDevice::init()
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
IOUSBHubDevice::start(IOService *provider)
{
	USBLog(6, "IOUSBHubDevice[%p]::start", this );
	
	if (!super::start(provider))
		return false;
		
	if (!InitializeCharacteristics())
		return false;
			
	return true;
}



bool
IOUSBHubDevice::InitializeCharacteristics()
{
	UInt32			characteristics = 0;			// not a root hub, which will get overriden
	
	// for now, we are not attached to any bus and we are not the root hub, so we set all to 0
	SetHubCharacteristics(characteristics);
	return true;
}



void
IOUSBHubDevice::stop( IOService *provider )
{
	super::stop(provider);
}



void
IOUSBHubDevice::free()
{
	if (_expansionData)
    {
		IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }
    super::free();
}



void
IOUSBHubDevice::SetHubCharacteristics(UInt32 characteristics)
{
	_myCharacteristics = characteristics;
}



UInt32
IOUSBHubDevice::GetHubCharacteristics()
{
	return _myCharacteristics;
}



UInt32			
IOUSBHubDevice::GetMaxProvidedPower()
{
	return _MAXPORTCURRENT;
}



UInt32
IOUSBHubDevice::RequestProvidedPower(UInt32 requestedPower)
{
	// what if this is a bus powered hub?   Then we should be returning 100 mA?
	
	if (requestedPower <= 500)
		return requestedPower;
	else
		return 500;
}



void
IOUSBHubDevice::SetPolicyMaker(IOUSBHubPolicyMaker *policyMaker)
{
    USBLog(6, "IOUSBHubDevice[%p]::SetPolicyMaker  set to %p", this, policyMaker );
	_myPolicyMaker = policyMaker;
}



IOUSBHubPolicyMaker *
IOUSBHubDevice::GetPolicyMaker(void)
{
	return _myPolicyMaker;
}



UInt32
IOUSBHubDevice::RequestExtraPower(UInt32 requestedPower)
{
	UInt32	extraAllocated = 0;
	
	USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower - requestedPower = %d, available: %d", this, (uint32_t)requestedPower, (uint32_t) _TOTALEXTRACURRENT);
	
	if (requestedPower == 0)
		return 0;
	
	// The power requested is a delta above the USB Spec for the port.  That's why we need to subtract the 500mA from the maxPowerPerPort value
	// Note:  should we see if this is a high power port or not?  It assumes it is
	if (requestedPower > (_MAXPORTCURRENT-500))		// limit requests to the maximum the HW can support
	{
		USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower - requestedPower of %d, was greater than %d, the (maximum power per port - 500mA).  Requesting %d instead", this, (uint32_t)requestedPower, (uint32_t) (_MAXPORTCURRENT-500), (uint32_t) (_MAXPORTCURRENT-500));
		requestedPower = _MAXPORTCURRENT-500;
	}
	
	if (requestedPower <= _TOTALEXTRACURRENT)
	{		
		// honor the request if possible
		extraAllocated = requestedPower;
		_TOTALEXTRACURRENT -= extraAllocated;
		USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower - Asked for %d, not _TOTALEXTRACURRENT is %d", this, (uint32_t)requestedPower, (uint32_t) _TOTALEXTRACURRENT );
	}
	
	// if we don't have any power to allocate, let's see if we have "ExtraPower" that can be requested from our parent (and we haven't already done so).
	if ( (_CANREQUESTEXTRAPOWER != 0) and not _EXTRAPOWERALLOCATED)
	{
		if ( requestedPower > _EXTRAPOWERFORPORTS )
		{
			USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower - we can request extra power from our parent but only %d and we want %d ", this, (uint32_t) _EXTRAPOWERFORPORTS, (uint32_t)requestedPower);
			extraAllocated = 0;
		}
		else
		{
			// Even tho' we only want "requestedPower", we have to ask for _CANREQUESTEXTRAPOWER
			USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower - requesting %d from our parent", this, (uint32_t) _CANREQUESTEXTRAPOWER);
			UInt32	parentPowerRequest = super::RequestExtraPower(kUSBPowerDuringWake, _CANREQUESTEXTRAPOWER);
			
			USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower - requested %d from our parent and got %d ", this, (uint32_t) _CANREQUESTEXTRAPOWER, (uint32_t)parentPowerRequest);
			
			if ( parentPowerRequest == _CANREQUESTEXTRAPOWER )
			{
				// We only can return _EXTRAPOWERFORPORTS, not less
				extraAllocated = _EXTRAPOWERFORPORTS;
				_EXTRAPOWERALLOCATED = true;
				setProperty("PortActualRequestExtraPower", extraAllocated, 32);
			}
			else
			{
				USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower - returning power %d because we didnt get enough", this, (uint32_t)parentPowerRequest);
				super::ReturnExtraPower(kUSBPowerDuringWake, parentPowerRequest);	
			}
		}
	}
	
	// this method may be overriden by the IOUSBRootHubDevice class to implement this
	USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower - extraAllocated = %d", this, (uint32_t)extraAllocated);
	
	return extraAllocated;
}



void
IOUSBHubDevice::ReturnExtraPower(UInt32 returnedPower)
{
	USBLog(5, "IOUSBHubDevice[%p]::ReturnExtraPower - returning = %d", this, (uint32_t)returnedPower);
	
	if ( returnedPower == 0 )
		return;
	
	// Check to see if we had the extraPower allocated
	// (Note:  There might be a bug here in that we are assuming that any power allocated by this hub will set _EXTRAPOWERALLOCATED, and when we return ANY amount
	//         of power, we will return _CANREQUESTEXTRAPOWER, regardless.  This works, but seems fragile)
	if ( _EXTRAPOWERALLOCATED )
	{
		USBLog(5, "IOUSBHubDevice[%p]::ReturnExtraPower - we had _EXTRAPOWERALLOCATED calling our parent to return %d", this, (uint32_t)_CANREQUESTEXTRAPOWER);
		
		// We allocated power from our parent, so return it now
		super::ReturnExtraPower(kUSBPowerDuringWake, _CANREQUESTEXTRAPOWER);	
		_EXTRAPOWERALLOCATED = false;
	}
	else
	{
		if (returnedPower > 0)
			_TOTALEXTRACURRENT += returnedPower;
	}
}

void
IOUSBHubDevice::SetTotalSleepCurrent(UInt32 sleepCurrent)
{
	SetSleepCurrent(sleepCurrent);
}

UInt32
IOUSBHubDevice::GetTotalSleepCurrent()
{
	return GetSleepCurrent();
}


void
IOUSBHubDevice::SetSleepCurrent(UInt32 sleepCurrent)
{
	USBLog(5, "IOUSBHubDevice[%p]::SetSleepCurrent -  %d", this, (uint32_t)sleepCurrent);
	_TOTALSLEEPCURRENT = sleepCurrent;
}

UInt32
IOUSBHubDevice::GetSleepCurrent()
{
	return _TOTALSLEEPCURRENT;
}



void			
IOUSBHubDevice::InitializeExtraPower(UInt32 maxPortCurrent, UInt32 totalExtraCurrent)
{
	OSNumber *		extraPowerProp = NULL;
	OSObject *		propertyObject = NULL;
	
	USBLog(5, "IOUSBHubDevice[%p]::InitializeExtraPower - maxPortCurrent = %d, totalExtraCurrent: %d", this, (uint32_t)maxPortCurrent, (uint32_t) totalExtraCurrent);

	_MAXPORTCURRENT = maxPortCurrent;
	_TOTALEXTRACURRENT = totalExtraCurrent;
	
	propertyObject = copyProperty("ExtraPowerRequest");
	extraPowerProp = OSDynamicCast(OSNumber, propertyObject);
    if ( extraPowerProp )
	{
        _CANREQUESTEXTRAPOWER = extraPowerProp->unsigned32BitValue();
		USBLog(6, "IOUSBHubDevice[%p]::InitializeExtraPower  got ExtraPowerRequest of %d", this, (uint32_t) _CANREQUESTEXTRAPOWER );
	}
	if ( propertyObject)
		propertyObject->release();
	
	propertyObject = copyProperty("ExtraPowerForPorts");
	extraPowerProp = OSDynamicCast(OSNumber, propertyObject);
    if ( extraPowerProp )
	{
        _EXTRAPOWERFORPORTS = extraPowerProp->unsigned32BitValue();
		USBLog(6, "IOUSBHubDevice[%p]::InitializeExtraPower  got ExtraPowerForPorts of %d", this, (uint32_t) _EXTRAPOWERFORPORTS );
	}
	if ( propertyObject)
		propertyObject->release();
	
	USBLog(6, "IOUSBHubDevice[%p]::InitializeExtraPower  USB Plane Parent is %p (%s)", this, _USBPLANE_PARENT, _USBPLANE_PARENT == NULL ? "" : _USBPLANE_PARENT->getName());

}

UInt32
IOUSBHubDevice::RequestSleepPower(UInt32 requestedPower)
{
	UInt32	extraAllocated = 0;
	
	USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower - requestedPower = %d, available: %d", this, (uint32_t)requestedPower, (uint32_t) _TOTALSLEEPCURRENT);
	
	if (requestedPower <= _TOTALSLEEPCURRENT)
	{		
		// honor the request if possible
		extraAllocated = requestedPower;
		_TOTALSLEEPCURRENT -= extraAllocated;
	}
	
	USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower - extraAllocated = %d", this, (uint32_t)extraAllocated);
	
	return extraAllocated;
}

void
IOUSBHubDevice::ReturnSleepPower(UInt32 returnedPower)
{
	USBLog(5, "IOUSBHubDevice[%p]::ReturnSleepPower - returning = %d", this, (uint32_t)returnedPower);
	if (returnedPower > 0)
		_TOTALSLEEPCURRENT += returnedPower;
}


OSMetaClassDefineReservedUsed(IOUSBHubDevice,  0);
OSMetaClassDefineReservedUsed(IOUSBHubDevice,  1);
OSMetaClassDefineReservedUsed(IOUSBHubDevice,  2);
OSMetaClassDefineReservedUsed(IOUSBHubDevice,  3);
OSMetaClassDefineReservedUsed(IOUSBHubDevice,  4);

OSMetaClassDefineReservedUnused(IOUSBHubDevice,  5);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  6);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  7);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  8);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  9);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  10);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  11);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  12);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  13);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  14);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  15);


