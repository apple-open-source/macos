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
#define	_TOTALEXTRASLEEPCURRENT		_expansionData->_totalSleepCurrent
#define	_CANREQUESTEXTRAPOWER		_expansionData->_canRequestExtraPower
#define	_EXTRAPOWERFORPORTS			_expansionData->_extraPowerForPorts
#define	_EXTRAPOWERALLOCATED		_expansionData->_extraPowerAllocated
#define	_REQUESTFROMPARENT			_expansionData->_requestFromParent
#define _USBPLANE_PARENT			super::_expansionData->_usbPlaneParent
#define _MAXPORTSLEEPCURRENT		_expansionData->_maxPortSleepCurrent
#define	_EXTRASLEEPPOWERALLOCATED	_expansionData->_extraSleepPowerAllocated
#define	_CANREQUESTEXTRASLEEPPOWER	_expansionData->_canRequestExtraSleepPower
#define _STANDARD_PORT_POWER_IN_SLEEP			_expansionData->_standardPortSleepCurrent


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
	USBLog(5, "IOUSBHubDevice[%p]::GetHubCharacteristics - returning (0x%x)", this, (int)_myCharacteristics);
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
	IOReturn	kr = kIOReturnSuccess;
	uint64_t	extraAllocated = 0;
	
	if ( IOUSBDevice::_expansionData && IOUSBDevice::_expansionData->_commandGate && IOUSBDevice::_expansionData->_workLoop)
	{
		IOCommandGate *	gate = IOUSBDevice::_expansionData->_commandGate;
		IOWorkLoop *	workLoop = IOUSBDevice::_expansionData->_workLoop;
		
		retain();
		workLoop->retain();
		gate->retain();
       
        USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower - requestedPower = %d", this, (uint32_t)requestedPower);
        gate->runAction(GatedRequestExtraPower, (void *)requestedPower, &extraAllocated);
        if ( kr != kIOReturnSuccess )
        {
            USBLog(2,"%s[%p]::RequestExtraPower GatedRequestExtraPower runAction() failed (0x%x)", getName(), this, kr);
        }
		
		gate->release();
		workLoop->release();
		release();
	}
	
	USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower - returning %d", this, (uint32_t)extraAllocated);
	return (UInt32) extraAllocated;
}

IOReturn
IOUSBHubDevice::GatedRequestExtraPower(OSObject *owner,  void *arg0,  void *arg1,  void *arg2,  void *arg3 )
{
#pragma unused (arg2, arg3)
	IOUSBHubDevice *me = (IOUSBHubDevice*)owner;
	
	if (!me)
	{
		USBLog(5, "IOUSBHubDevice::GatedRequestExtraPower - owner was NULL");
		return kIOReturnNoDevice;
	}
	
	USBLog(7, "IOUSBHubDevice[%p]::GatedRequestExtraPower - requestedPower = %qd", me, (uint64_t)arg0);
	return me->RequestExtraPower((uint64_t)arg0, (uint64_t *)arg1);
}

IOReturn
IOUSBHubDevice::RequestExtraPower(uint64_t requestedPower, uint64_t * powerAllocated)
{	
	uint64_t		extraAllocated = 0;
	
	USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower(thru gate) - requestedPower = %d, hub has %d available (_REQUESTFROMPARENT: %d, _CANREQUESTEXTRAPOWER: %d, _EXTRAPOWERALLOCATED: %d, _EXTRAPOWERFORPORTS: %d)", 
				this, (uint32_t)requestedPower, (uint32_t) _TOTALEXTRACURRENT, (uint32_t)_REQUESTFROMPARENT, (uint32_t)_CANREQUESTEXTRAPOWER, (uint32_t)_EXTRAPOWERALLOCATED, (uint32_t)_EXTRAPOWERFORPORTS);
	
	if (requestedPower == 0)
	{
		*powerAllocated = 0;
		return kIOReturnSuccess;
	}
	
	// The power requested is a delta above the USB Spec for the port.  That's why we need to subtract the 500mA from the maxPowerPerPort value
	// Note:  should we see if this is a high power port or not?  It assumes it is
	if (requestedPower > (_MAXPORTCURRENT-500))		// limit requests to the maximum the HW can support
	{
		USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) - requestedPower of %d, was greater than %d, (the maximum power per port - 500mA).  Requesting %d instead", this, (uint32_t)requestedPower, (uint32_t) (_MAXPORTCURRENT-500), (uint32_t) (_MAXPORTCURRENT-500));
		requestedPower = _MAXPORTCURRENT-500;
	}
	
	if (requestedPower <= _TOTALEXTRACURRENT)
	{		
		// honor the request if possible
		extraAllocated = requestedPower;
		_TOTALEXTRACURRENT -= extraAllocated;
		USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) - Asked for %d,  _TOTALEXTRACURRENT is %d", this, (uint32_t)requestedPower, (uint32_t) _TOTALEXTRACURRENT );
	}
	
	// At this point, we can have a hub that can request a set amount of power from its parent (e.g. a Keyboard Hub), or a hub that can pass thru the request to its parent (RMH).
	
	// if we don't have any power to allocate, let's see if we have "ExtraPower" that can be requested from our parent (and we haven't already done so).
	if ( _REQUESTFROMPARENT)
	{
		// 
		USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) - requesting %d from our parent", this, (uint32_t) requestedPower);
		UInt32	parentPowerRequest = super::RequestExtraPower(kUSBPowerDuringWake, requestedPower);
		
		USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) - requested %d from our parent and got %d ", this, (uint32_t) parentPowerRequest, (uint32_t)parentPowerRequest);
		if ( parentPowerRequest == requestedPower )
		{
			OSNumber * currentValueProperty = NULL;
			UInt32		currentValue;
			
			// We only can return _EXTRAPOWERFORPORTS, not less
			extraAllocated = requestedPower;
			
			currentValueProperty = (OSNumber *) getProperty("PortActualRequestExtraPower");
			if ( currentValueProperty )
			{
				currentValue = currentValueProperty->unsigned32BitValue();
				USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) -  found a  PortActualRequestExtraPower with value of %d", this, (uint32_t) currentValue);
				currentValue += extraAllocated;
			}
			else
				currentValue = extraAllocated;

			setProperty("PortActualRequestExtraPower", currentValue, 32);
		}
		else
		{
			// If we don't get the full request from our parent, just return.  The user should call back to return it if they don't want it
			USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) - we didn't get the full amount we requested", this);
		}
	}
	else
	{
		if ( (_CANREQUESTEXTRAPOWER != 0) and  (_EXTRAPOWERALLOCATED < 2))
		{
			if ( requestedPower > _EXTRAPOWERFORPORTS )
			{
				USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) - we can request extra power from our parent but only %d and we want %d ", this, (uint32_t) _EXTRAPOWERFORPORTS, (uint32_t)requestedPower);
				extraAllocated = 0;
			}
			else
			{
				// Even tho' we only want "requestedPower", we have to ask for _CANREQUESTEXTRAPOWER
				USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) - requesting %d from our parent", this, (uint32_t) _CANREQUESTEXTRAPOWER);
				UInt32	parentPowerRequest = super::RequestExtraPower(kUSBPowerDuringWake, _CANREQUESTEXTRAPOWER);
				
				USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) - requested %d from our parent and got %d ", this, (uint32_t) _CANREQUESTEXTRAPOWER, (uint32_t)parentPowerRequest);
				
				if ( parentPowerRequest == _CANREQUESTEXTRAPOWER )
				{
					// We only can return _EXTRAPOWERFORPORTS, not less
					extraAllocated = _EXTRAPOWERFORPORTS;
					_EXTRAPOWERALLOCATED++;
					setProperty("PortActualRequestExtraPower", extraAllocated, 32);
					setProperty("ExtraWakePowerAllocated", _EXTRAPOWERALLOCATED, 32);
				}
				else
				{
					USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) - returning power %d because we didnt get enough", this, (uint32_t)parentPowerRequest);
					super::ReturnExtraPower(kUSBPowerDuringWake, parentPowerRequest);	
				}
			}
		}
	}
	
	// this method may be overriden by the IOUSBRootHubDevice class to implement this
	USBLog(5, "IOUSBHubDevice[%p]:::RequestExtraPower(thru gate) - extraAllocated = %d", this, (uint32_t)extraAllocated);
	
	*powerAllocated  = extraAllocated;
	
	return kIOReturnSuccess;
}



void
IOUSBHubDevice::ReturnExtraPower(UInt32 returnedPower)
{
	IOReturn	kr = kIOReturnSuccess;
	
	if ( IOUSBDevice::_expansionData && IOUSBDevice::_expansionData->_commandGate && IOUSBDevice::_expansionData->_workLoop)
	{
		IOCommandGate *	gate = IOUSBDevice::_expansionData->_commandGate;
		IOWorkLoop *	workLoop = IOUSBDevice::_expansionData->_workLoop;
		
		retain();
		workLoop->retain();
		gate->retain();
        
        USBLog(5, "IOUSBHubDevice[%p]::ReturnExtraPower - returning = %d", this, (uint32_t)returnedPower);
        gate->runAction(GatedReturnExtraPower, (void *)returnedPower);
        if ( kr != kIOReturnSuccess )
        {
            USBLog(2,"%s[%p]::ReturnExtraPower GatedReturnExtraPower runAction() failed (0x%x)", getName(), this, kr);
        }
		
		gate->release();
		workLoop->release();
		release();
	}
}

IOReturn
IOUSBHubDevice::GatedReturnExtraPower(OSObject *owner,  void *arg0,  void *arg1,  void *arg2,  void *arg3 )
{
#pragma unused (arg1, arg2, arg3)
	IOUSBHubDevice *me = (IOUSBHubDevice*)owner;
	
	if (!me)
	{
		USBLog(5, "IOUSBHubDevice::GatedReturnExtraPower - owner was NULL");
		return kIOReturnNoDevice;
	}
	
	USBLog(7, "IOUSBHubDevice[%p]::GatedReturnExtraPower - returning = %qd", me, (uint64_t)arg0);
	return me->ReturnExtraPower((uint64_t)arg0);
}


IOReturn
IOUSBHubDevice::ReturnExtraPower(uint64_t returnedPower)
{
	USBLog(5, "IOUSBHubDevice[%p]::ReturnExtraPower(thru gate) - returning = %d", this, (uint32_t)returnedPower);
	
	if ( returnedPower == 0 )
		return kIOReturnSuccess;
	
	// Check to see if we had the extraPower allocated
	// (Note:  There might be a bug here in that we are assuming that any power allocated by this hub will set _EXTRAPOWERALLOCATED, and when we return ANY amount
	//         of power, we will return _CANREQUESTEXTRAPOWER, regardless.  This works, but seems fragile)
	if ( _EXTRAPOWERALLOCATED > 0 )
	{
		USBLog(5, "IOUSBHubDevice[%p]::ReturnExtraPower(thru gate) - we had _EXTRAPOWERALLOCATED calling our parent to return %d", this, (uint32_t)_CANREQUESTEXTRAPOWER);
		
		// We allocated power from our parent, so return it now
		super::ReturnExtraPower(kUSBPowerDuringWake, _CANREQUESTEXTRAPOWER);	
		_EXTRAPOWERALLOCATED--;
		setProperty("ExtraWakePowerAllocated", _EXTRAPOWERALLOCATED, 32);
	}
	else if ( _REQUESTFROMPARENT )
	{
		super::ReturnExtraPower(kUSBPowerDuringWake, returnedPower);	
	}
	else
	{
		if (returnedPower > 0)
			_TOTALEXTRACURRENT += returnedPower;
	}
	
	return kIOReturnSuccess;
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

// This method should really be called: SetStandardSleepCurrent
void
IOUSBHubDevice::SetSleepCurrent(UInt32 sleepCurrent)
{
	USBLog(5, "IOUSBHubDevice[%p]::SetSleepCurrent -  setting _STANDARD_PORT_POWER_IN_SLEEP to %d", this, (uint32_t)sleepCurrent);
	_STANDARD_PORT_POWER_IN_SLEEP = sleepCurrent;
}

UInt32
IOUSBHubDevice::GetSleepCurrent()
{
	return _STANDARD_PORT_POWER_IN_SLEEP;
}

//================================================================================================
//
// ExtraPowerRequest - how much we need to ask from our hub parent in order to get ExtraPowerForPorts mA
// ExtraPowerForPorts - how much extra power we provide for all ports when a request for ExtraPowerRequest comes thru
//
//================================================================================================
void			
IOUSBHubDevice::InitializeExtraPower(UInt32 maxPortCurrent, UInt32 totalExtraCurrent)
{
#pragma unused (maxPortCurrent, totalExtraCurrent)
	USBLog(5, "IOUSBHubDevice[%p]::InitializeExtraPower - obsolete method called", this);
}

// requestedPower is the total amount of current, NOT the extra over the standard USB load
UInt32
IOUSBHubDevice::RequestSleepPower(UInt32 requestedPower)
{
	IOReturn	kr = kIOReturnSuccess;
	uint64_t	extraAllocated = 0;
	
	if ( IOUSBDevice::_expansionData && IOUSBDevice::_expansionData->_commandGate && IOUSBDevice::_expansionData->_workLoop)
	{
		IOCommandGate *	gate = IOUSBDevice::_expansionData->_commandGate;
		IOWorkLoop *	workLoop = IOUSBDevice::_expansionData->_workLoop;
		
		retain();
		workLoop->retain();
		gate->retain();
        
        
        USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower - requestedPower = %d", this, (uint32_t)requestedPower);
        gate->runAction(GatedRequestSleepPower, (void *)requestedPower, &extraAllocated);
        
        if ( kr != kIOReturnSuccess )
        {
            USBLog(2,"%s[%p]::RequestSleepPower GatedRequestSleepPower runAction() failed (0x%x)", getName(), this, kr);
        }
		
		gate->release();
		workLoop->release();
		release();
	}
    
	USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower - returning %d", this, (uint32_t)extraAllocated);
	return (UInt32) extraAllocated;
}

IOReturn
IOUSBHubDevice::GatedRequestSleepPower(OSObject *owner,  void *arg0,  void *arg1,  void *arg2,  void *arg3 )
{
#pragma unused (arg2, arg3)
	IOUSBHubDevice *me = (IOUSBHubDevice*)owner;
	
	if (!me)
	{
		USBLog(5, "IOUSBHubDevice::GatedRequestSleepPower - owner was NULL");
		return kIOReturnNoDevice;
	}
	
	USBLog(7, "IOUSBHubDevice[%p]::GatedRequestSleepPower - requestedPower = %qd", me, (uint64_t)arg0);
	return me->RequestSleepPower((uint64_t)arg0, (uint64_t *)arg1);
}

IOReturn
IOUSBHubDevice::RequestSleepPower(uint64_t requestedPower, uint64_t *powerAllocated)
{
	UInt32	extraAllocated = 0;
	
	USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - requestedPower = %d, _STANDARD_PORT_POWER_IN_SLEEP: %d, _TOTALEXTRASLEEPCURRENT: %d (_REQUESTFROMPARENT = %d)", this, (uint32_t)requestedPower, (uint32_t)_STANDARD_PORT_POWER_IN_SLEEP, (uint32_t) _TOTALEXTRASLEEPCURRENT, (uint32_t)_REQUESTFROMPARENT);
	
	if (requestedPower == 0)
	{
		USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - asked for 0, returning 0", this );
		*powerAllocated = 0;
		return kIOReturnSuccess;
	}
	
	// If we don't have any _standardPortSleepCurrent, then it means that we can't allocate any 
	if ( _STANDARD_PORT_POWER_IN_SLEEP == 0)
	{
		USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - port does not have any _STANDARD_PORT_POWER_IN_SLEEP, returning 0", this );
		*powerAllocated = 0;
		return kIOReturnSuccess;
	}
	
	// If we are requesting < _STANDARD_PORT_POWER_IN_SLEEP (i.e. 500mA), then we're good and just give it
	if ( requestedPower <= _STANDARD_PORT_POWER_IN_SLEEP)	
	{
		USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - requested <= _STANDARD_PORT_POWER_IN_SLEEP, returning %d", this, (uint32_t)requestedPower );
		*powerAllocated = requestedPower;
		return kIOReturnSuccess;
	}
	
	// At this point, we can have a hub that can request a set amount of power from its parent (e.g. a Keyboard Hub), or a hub that can pass thru the request to its parent (RMH).
	
	// if we don't have any power to allocate, let's see if we have "ExtraPower" that can be requested from our parent (and we haven't already done so).
	if ( _REQUESTFROMPARENT)
	{
		// 
		USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - requesting %d from our parent", this, (uint32_t) requestedPower);
		UInt32	parentPowerRequest = super::RequestExtraPower(kUSBPowerDuringSleep, requestedPower);
		
		USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - requested %d from our parent and got %d ", this, (uint32_t) requestedPower, (uint32_t)parentPowerRequest);
		if ( parentPowerRequest == requestedPower )
		{
			// We only can return _EXTRAPOWERFORPORTS, not less
			extraAllocated = requestedPower- _STANDARD_PORT_POWER_IN_SLEEP;
			setProperty("PortActualRequestExtraSleepPower", extraAllocated, 32);
		}
		else
		{
			// If we don't get the full request from our parent, just return.  The user should call back to return it if they don't want it
			USBLog(6, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - we didn't get the full amount we requested, we got only %d", this, (uint32_t)parentPowerRequest);
			extraAllocated = parentPowerRequest- _STANDARD_PORT_POWER_IN_SLEEP;
			setProperty("PortActualRequestExtraSleepPower", extraAllocated, 32);
		}
	}
	else 
	{
		// OK, at this point we are requesting more than _STANDARD_PORT_POWER_IN_SLEEP, so we need to determine how much to give them.  Subtract the _STANDARD_PORT_POWER_IN_SLEEP from the requested:
		
		if (requestedPower > _MAXPORTSLEEPCURRENT)		// limit requests to the maximum the HW can support
		{
			USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - requestedPower of %d, was greater than %d, (the maximum allowed sleep power per port).  Requesting the max instead", this, (uint32_t)requestedPower, (uint32_t) (_MAXPORTSLEEPCURRENT));
			requestedPower = _MAXPORTSLEEPCURRENT;
		}
		
		if ( (requestedPower - _STANDARD_PORT_POWER_IN_SLEEP) <= _TOTALEXTRASLEEPCURRENT)
		{		
			// honor the request
			extraAllocated = requestedPower - _STANDARD_PORT_POWER_IN_SLEEP;
			_TOTALEXTRASLEEPCURRENT -= extraAllocated;
			USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - Asked for %d extra,  _TOTALEXTRASLEEPCURRENT is now %d", this, (uint32_t)(requestedPower - _STANDARD_PORT_POWER_IN_SLEEP), (uint32_t) _TOTALEXTRASLEEPCURRENT );
		}
		else 
		{
			USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - Asked for %d extra,  but only %d _TOTALEXTRASLEEPCURRENT available", this, (uint32_t)(requestedPower - _STANDARD_PORT_POWER_IN_SLEEP), (uint32_t) _TOTALEXTRASLEEPCURRENT );
		}
	}

#if 0  // for M84/89  (not supporting providing sleep current on those hubs, for now)
	{
		if ( (_CANREQUESTEXTRASLEEPPOWER != 0) (_EXTRASLEEPPOWERALLOCATED < 2) )
		{
			if ( requestedPower > _EXTRAPOWERFORPORTS )
			{
				USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - we can request extra sleep power from our parent but only %d and we want %d ", this, (uint32_t) _EXTRAPOWERFORPORTS, (uint32_t)requestedPower);
				extraAllocated = 0;
			}
			else
			{
				// Even tho' we only want "requestedPower", we have to ask for _CANREQUESTEXTRASLEEPPOWER
				USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - requesting %d from our parent", this, (uint32_t) _CANREQUESTEXTRASLEEPPOWER);
				UInt32	parentPowerRequest = super::RequestExtraPower(kUSBPowerDuringSleep, _CANREQUESTEXTRASLEEPPOWER);
				
				USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - requested %d from our parent and got %d ", this, (uint32_t) _CANREQUESTEXTRASLEEPPOWER, (uint32_t)parentPowerRequest);
				
				if ( parentPowerRequest == _CANREQUESTEXTRASLEEPPOWER )
				{
					// We only can return _EXTRAPOWERFORPORTS, not less
					extraAllocated = _EXTRAPOWERFORPORTS;
					_EXTRASLEEPPOWERALLOCATED++;
					setProperty("PortActualRequestExtraSleepPower", extraAllocated, 32);
					setProperty("ExtraSleepPowerAllocated", _EXTRASLEEPPOWERALLOCATED, 32);
				}
				else
				{
					USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - returning power %d because we didnt get enough", this, (uint32_t)parentPowerRequest);
					super::ReturnExtraPower(kUSBPowerDuringSleep, parentPowerRequest);	
				}
			}
		}
	}
#endif
	
	// this method may be overriden by the IOUSBRootHubDevice class to implement this
	USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower(thru gate) - extraAllocated = %d, returning %d", this, (uint32_t)extraAllocated, (uint32_t)(extraAllocated + _STANDARD_PORT_POWER_IN_SLEEP));
	
	*powerAllocated = (extraAllocated + _STANDARD_PORT_POWER_IN_SLEEP);
	return kIOReturnSuccess;
}



void
IOUSBHubDevice::ReturnSleepPower(UInt32 returnedPower)
{
	IOReturn	kr = kIOReturnSuccess;
	
	if ( IOUSBDevice::_expansionData && IOUSBDevice::_expansionData->_commandGate && IOUSBDevice::_expansionData->_workLoop)
	{
		IOCommandGate *	gate = IOUSBDevice::_expansionData->_commandGate;
		IOWorkLoop *	workLoop = IOUSBDevice::_expansionData->_workLoop;
		
		retain();
		workLoop->retain();
		gate->retain();
        
        USBLog(5, "IOUSBHubDevice[%p]::ReturnSleepPower - returning = %d", this, (uint32_t)returnedPower);
        gate->runAction(GatedReturnSleepPower, (void *)returnedPower);
        if ( kr != kIOReturnSuccess )
        {
            USBLog(2,"%s[%p]::RequestExtraPower GatedRequestExtraPower runAction() failed (0x%x)", getName(), this, kr);
        }
		
		gate->release();
		workLoop->release();
		release();
	}
}

IOReturn
IOUSBHubDevice::GatedReturnSleepPower(OSObject *owner,  void *arg0,  void *arg1,  void *arg2,  void *arg3 )
{
#pragma unused (arg1, arg2, arg3)
	IOUSBHubDevice *me = (IOUSBHubDevice*)owner;
	
	if (!me)
	{
		USBLog(5, "IOUSBHubDevice::GatedReturnSleepPower - owner was NULL");
		return kIOReturnNoDevice;
	}
	
	USBLog(7, "IOUSBHubDevice[%p]::GatedReturnSleepPower - returning = %qd", me, (uint64_t)arg0);
	return me->ReturnSleepPower((uint64_t)arg0);
}


IOReturn
IOUSBHubDevice::ReturnSleepPower(uint64_t returnedPower)
{
	USBLog(5, "IOUSBHubDevice[%p]::ReturnSleepPower(thru gate) - returning = %d", this, (uint32_t)returnedPower);
#if 0  // M84/89
	// Check to see if we had the extraPower allocated
	// (Note:  There might be a bug here in that we are assuming that any power allocated by this hub will set _EXTRAPOWERALLOCATED, and when we return ANY amount
	//         of power, we will return _CANREQUESTEXTRAPOWER, regardless.  This works, but seems fragile)
	if ( _EXTRASLEEPPOWERALLOCATED > 0)
	{
		USBLog(5, "IOUSBHubDevice[%p]::ReturnSleepPower(thru gate) - we had _EXTRASLEEPPOWERALLOCATED calling our parent to return %d", this, (uint32_t)_CANREQUESTEXTRASLEEPPOWER);
		
		// We allocated power from our parent, so return it now
		super::ReturnExtraPower(kUSBPowerDuringSleep, _CANREQUESTEXTRASLEEPPOWER);	
		_EXTRAPOWERALLOCATED--;
		setProperty("ExtraWakePowerAllocated", _EXTRASLEEPPOWERALLOCATED, 32);
	}
#endif		

	// If we are returning < _STANDARD_PORT_POWER_IN_SLEEP (i.e. 500mA), then we're good and just retrun it
	if ( returnedPower <= _STANDARD_PORT_POWER_IN_SLEEP)	
	{
		USBLog(5, "IOUSBHubDevice[%p]::ReturnSleepPower(thru gate) - returned <= _STANDARD_PORT_POWER_IN_SLEEP, returning %d", this, (uint32_t)returnedPower );
		return kIOReturnSuccess;
	}

	if ( _REQUESTFROMPARENT )
	{
		super::ReturnExtraPower(kUSBPowerDuringSleep, returnedPower);	
	}
	else
	{
		if (returnedPower > 0)
		{
			USBLog(5, "IOUSBHubDevice[%p]::ReturnSleepPower(thru gate) - adding back %d to our extra", this, (uint32_t)(returnedPower - _STANDARD_PORT_POWER_IN_SLEEP));
			_TOTALEXTRASLEEPCURRENT += (returnedPower - _STANDARD_PORT_POWER_IN_SLEEP);
		}
	}
	
	return kIOReturnSuccess;
}


void
IOUSBHubDevice::InitializeExtraPower(UInt32 maxPortCurrent, UInt32 totalExtraCurrent, UInt32 maxPortCurrentInSleep, UInt32 totalExtraCurrentInSleep)
{
	OSNumber *		extraPowerProp = NULL;
	OSObject *		propertyObject = NULL;
	OSBoolean *		booleanObj = NULL;
	UInt32			deviceInfo = 0;
	bool			useNewMethodToAddRequestFromParent = false;;
	
	(void) GetDeviceInformation(&deviceInfo);
	
	USBLog(5, "IOUSBHubDevice[%p]::InitializeExtraPower - maxPortCurrent = %d, totalExtraCurrent: %d, maxPortCurrentInSleep: %d, totalExtraCurrentInSleep: %d, deviceInfo: 0x%x", this, (uint32_t)maxPortCurrent, (uint32_t) totalExtraCurrent, (uint32_t)maxPortCurrentInSleep, (uint32_t)totalExtraCurrentInSleep, (uint32_t)deviceInfo);
	
	_MAXPORTCURRENT = maxPortCurrent;
	_TOTALEXTRACURRENT = totalExtraCurrent;
	_MAXPORTSLEEPCURRENT = maxPortCurrentInSleep;
	_TOTALEXTRASLEEPCURRENT = totalExtraCurrentInSleep;
	
	if ( _controller )
	{
		booleanObj = OSDynamicCast(OSBoolean, _controller->getProperty("UpdatedSleepPropertiesExists"));
		if (booleanObj && booleanObj->isTrue())
		{
			USBLog(6, "IOUSBHubDevice[%p]::InitializeExtraPower  found UpdatedSleepPropertiesExists with true value", this);
			useNewMethodToAddRequestFromParent = true;
		}
	}
	// If we have a hub that is (1) internal, (2) attached to the RootHub, then we will tell it to request power from its parent
	// Limit this only to machines that have the internal hubs and later.  We check that by llooking to see if the controller has the property that indicates
	// that the new sleep extra current properties existed in EFI.  This is an indication that the machine has a verified ACPI table and we can trust the PortInfo
	if ( useNewMethodToAddRequestFromParent && (deviceInfo & kUSBInformationDeviceIsInternalMask) && (deviceInfo & kUSBInformationDeviceIsAttachedToRootHubMask) && (deviceInfo & kUSBInformationDeviceIsCaptiveMask) )
	{
		USBLog(1, "IOUSBHubDevice[%p]::InitializeExtraPower  found hub (0x%x, 0x%x), that is internal, attached to root hub, captive, and on a recent machine", this, GetVendorID(), GetProductID());
		_REQUESTFROMPARENT = true;
		if (_USBPLANE_PARENT != NULL)
		{
			extraPowerProp = OSDynamicCast(OSNumber, _USBPLANE_PARENT->getProperty(kAppleMaxPortCurrent));
			if ( extraPowerProp )
			{
				_MAXPORTCURRENT = extraPowerProp->unsigned32BitValue();
				USBLog(6, "IOUSBHubDevice[%p]::InitializeExtraPower  setting _MAXPORTCURRENT to %d", this, (uint32_t) _MAXPORTCURRENT );
			}
		}
	}
	else 
	{
		// If we have an "RequestExtraCurrentFromParent" property, this means that the hub needs to ask its parent for the extra power.  This is used for internal hubs where the MLB provides
		// the extra current.  If a hub had a unique vid/pid (like a display hub), then it could just add the required properties and not set the "RequestExtraCurrentFromParent" one.
		
		propertyObject = getProperty("RequestExtraCurrentFromParent");
		booleanObj = OSDynamicCast(OSBoolean, propertyObject);
		if (booleanObj && booleanObj->isTrue() && DoLocationOverrideAndModelMatch())
		{
			USBLog(6, "IOUSBHubDevice[%p]::InitializeExtraPower  found RequestExtraCurrentFromParent property, setting _REQUESTFROMPARENT to true", this);
			_REQUESTFROMPARENT = true;
			
			if (_USBPLANE_PARENT != NULL)
			{
				extraPowerProp = OSDynamicCast(OSNumber, _USBPLANE_PARENT->getProperty(kAppleMaxPortCurrent));
				if ( extraPowerProp )
				{
					_MAXPORTCURRENT = extraPowerProp->unsigned32BitValue();
					USBLog(6, "IOUSBHubDevice[%p]::InitializeExtraPower  setting _MAXPORTCURRENT to %d", this, (uint32_t) _MAXPORTCURRENT );
				}
			}
		}
	}
	
	
	// Now set some parameters (for keyboards) that also request from the parent
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


void			
IOUSBHubDevice::SendExtraPowerMessage(UInt32 type, UInt32 returnedPower)
{
	USBLog(5, "IOUSBHubDevice[%p]::SendExtraPowerMessage - type: %d, argument: %d (_EXTRAPOWERFORPORTS: %d, _REQUESTFROMPARENT: %d)", this, (uint32_t)type, (uint32_t) returnedPower, (uint32_t)_EXTRAPOWERFORPORTS, _REQUESTFROMPARENT);
	if (_EXTRAPOWERFORPORTS || _REQUESTFROMPARENT)
	{
		// 
		USBLog(5, "IOUSBHubDevice[%p]:::SendExtraPowerMessage - sending to our parent", this);
		if ( type == kUSBPowerRequestWakeRelease )
		{
			super::ReturnExtraPower(type, returnedPower);
		}
		else 
		{
			super::RequestExtraPower(type, returnedPower);
		}
	}
	else 
	{
		OSIterator *		iterator				= NULL;

		// We are a hub that provides extra power.  Send a messages to all the downstream devices
		USBLog(5, "IOUSBHubDevice[%p]:::SendExtraPowerMessage - sending message to all devices attached to this hub", this);

		iterator = getChildIterator(getPlane(kIOUSBPlane));
		
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
						USBLog(5, "IOUSBHubDevice[%p]:::SendExtraPowerMessage - sending  kIOUSBMessageReleaseExtraCurrent to %s", this, aDevice->getName());
						aDevice->messageClients(kIOUSBMessageReleaseExtraCurrent, &returnedPower, sizeof(UInt32));
					}
					else if ( type == kUSBPowerRequestWakeReallocate )
					{
						USBLog(5, "IOUSBHubDevice[%p]:::SendExtraPowerMessage - sending  kIOUSBMessageReallocateExtraCurrent to %s", this, aDevice->getName());
						aDevice->messageClients(kIOUSBMessageReallocateExtraCurrent, NULL, 0);
					}
				}
			}
			iterator->release();
		}
		else 
		{
			USBLog(3, "IOUSBHubDevice[%p]:::SendExtraPowerMessage - could not getChildIterator", this);
		}
		
	}

}

OSMetaClassDefineReservedUsed(IOUSBHubDevice,  0);
OSMetaClassDefineReservedUsed(IOUSBHubDevice,  1);
OSMetaClassDefineReservedUsed(IOUSBHubDevice,  2);
OSMetaClassDefineReservedUsed(IOUSBHubDevice,  3);
OSMetaClassDefineReservedUsed(IOUSBHubDevice,  4);
OSMetaClassDefineReservedUsed(IOUSBHubDevice,  5);
OSMetaClassDefineReservedUsed(IOUSBHubDevice,  6);

OSMetaClassDefineReservedUnused(IOUSBHubDevice,  7);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  8);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  9);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  10);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  11);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  12);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  13);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  14);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  15);


