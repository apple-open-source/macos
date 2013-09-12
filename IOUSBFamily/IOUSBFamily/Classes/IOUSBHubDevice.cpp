/*
 * Copyright й 2006-2013 Apple Inc.  All rights reserved.
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
#include <libkern/OSAtomic.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/usb/IOUSBLog.h>

#include <IOKit/usb/IOUSBHubDevice.h>

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

#define _MAXPORTCURRENT					_expansionData->_maxPortCurrent
#define _TOTALEXTRACURRENT				_expansionData->_totalExtraCurrent
#define	_TOTALEXTRASLEEPCURRENT			_expansionData->_totalSleepCurrent
#define	_CANREQUESTEXTRAPOWER			_expansionData->_canRequestExtraPower
#define	_EXTRAPOWERFORPORTS				_expansionData->_extraPowerForPorts
#define	_EXTRAPOWERALLOCATED			_expansionData->_extraPowerAllocated
#define	_REQUESTFROMPARENT				_expansionData->_requestFromParent
#define _MAXPORTSLEEPCURRENT			_expansionData->_maxPortSleepCurrent
#define	_EXTRASLEEPPOWERALLOCATED		_expansionData->_extraSleepPowerAllocated
#define	_CANREQUESTEXTRASLEEPPOWER		_expansionData->_canRequestExtraSleepPower
#define _STANDARD_PORT_POWER_IN_SLEEP	_expansionData->_standardPortSleepCurrent
#define _UNCONNECTEDEXTERNALPORTS		_expansionData->_unconnectedExternalPorts
#define _EXTERNAL_PORTS					_expansionData->_externalPorts
#define _REVOCABLECURRENT				_expansionData->_revocablePower

// From our superclass
#define _USBPLANE_PARENT			super::_expansionData->_usbPlaneParent
#define _STANDARD_PORT_POWER		super::_expansionData->_standardUSBPortPower


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
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
	setProperty("characteristics", _myCharacteristics, 32);
#endif
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


#pragma mark Extra Power

UInt32
IOUSBHubDevice::RequestExtraPower(UInt32 type, UInt32 requestedPower)
{
	UInt32	returnValue = 0;
		
	USBLog(5, "IOUSBHubDevice[%p]::RequestExtraPower type: %d, requested %d", this, (uint32_t)type, (uint32_t) requestedPower);

	if ( type == kUSBPowerDuringWake || type == kUSBPowerDuringWakeRevocable || type == kUSBPowerDuringWakeUSB3)
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
			
			kr = gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOUSBHubDevice::RequestExtraWakePowerGated), (void *)type, (void *)requestedPower, &extraAllocated);
			if ( kr != kIOReturnSuccess )
			{
				USBLog(2,"IOUSBHubDevice[%p]::RequestExtraPower GatedRequestExtraPower runAction() failed (0x%x)", this, kr);
			}
			
			gate->release();
			workLoop->release();
			release();
		}

		returnValue = (UInt32) extraAllocated;
	}
	else  if ( type == kUSBPowerDuringSleep )
	{
		returnValue = RequestSleepPower( requestedPower );
	}
	else if ( type == kUSBPowerRequestWakeReallocate || type == kUSBPowerRequestSleepReallocate)
	{
		SendExtraPowerMessage( type, requestedPower );
	}

	return returnValue;
}


#pragma mark Request Extra Power (Wake)
//================================================================================================
//
//   RequestExtraWakePowerGated
//
//   Will see if this hub has "extra" power for wake.  If it does, we will allocate it here (e.g. an
//   external hub like those in an Apple display).  If it does NOT, we will see if we have a property
//   that tells us that we can ask for the extra power from our parent in the USB plane (e.g.  an internal
//   hub)
//
//================================================================================================
//
IOReturn
IOUSBHubDevice::RequestExtraWakePowerGated(uint64_t wakeType, uint64_t requestedPower, uint64_t * powerAllocated)
{	
	uint64_t		extraAllocated = 0;
	
	USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - type: %d, requestedPower = %d, hub has %d available, (_REQUESTFROMPARENT: %d, _CANREQUESTEXTRAPOWER: %d, _EXTRAPOWERALLOCATED: %d, _EXTRAPOWERFORPORTS: %d)", 
				this, (uint32_t)wakeType, (uint32_t)requestedPower, (uint32_t) _TOTALEXTRACURRENT, (uint32_t)_REQUESTFROMPARENT, (uint32_t)_CANREQUESTEXTRAPOWER, (uint32_t)_EXTRAPOWERALLOCATED, (uint32_t)_EXTRAPOWERFORPORTS);
	
	if (requestedPower == 0)
	{
		*powerAllocated = 0;
		return kIOReturnSuccess;
	}
	
	// The power requested is a delta above the USB Spec for the port.  That's why we need to add the _STANDARD_PORT_POWER (500 or 900mA) to the requested value
	if ( (requestedPower + _STANDARD_PORT_POWER) > _MAXPORTCURRENT)		// limit requests to the maximum the HW can support
	{
		USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - requestedPower of %d, was greater than the maximum this port can provide (including the standard port power): %d, Requesting %d instead", this, (uint32_t)requestedPower, (uint32_t) (_MAXPORTCURRENT-500), (uint32_t) (_MAXPORTCURRENT-500));
		requestedPower = _MAXPORTCURRENT - _STANDARD_PORT_POWER;
	}
	
	// Check to see if we have enough current to allocate and if so, then allocate it.  This just involves doing the math.
	if (requestedPower <= _TOTALEXTRACURRENT)
	{		
		// honor the request if possible
		extraAllocated = requestedPower;
		_TOTALEXTRACURRENT -= extraAllocated;
		USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - Asked for %d,  _TOTALEXTRACURRENT is %d", this, (uint32_t)requestedPower, (uint32_t) _TOTALEXTRACURRENT );
	}
	
	// If this hub didn't have any extra current, see if if can get some from our parent
	if (*powerAllocated	== 0)
	{
		// At this point, we can have a hub that can request a set amount of power from its parent (e.g. a Keyboard Hub), or a hub that can pass thru the request to its parent (RMH).
		
		// if we don't have any power to allocate, let's see if we have "ExtraPower" that can be requested from our parent (and we haven't already done so).  This is the "internal" hub case
		if ( _REQUESTFROMPARENT)
		{
			USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - requesting %d from our parent", this, (uint32_t) requestedPower);
			UInt32	parentPowerRequest = super::RequestExtraPower(wakeType, requestedPower);
			
			USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - requested %d from our parent and got %d ", this, (uint32_t) parentPowerRequest, (uint32_t)parentPowerRequest);
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
					USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated -  found a  PortActualRequestExtraPower with value of %d", this, (uint32_t) currentValue);
					currentValue += extraAllocated;
				}
				else
					currentValue = extraAllocated;
				
				setProperty("PortActualRequestExtraPower", currentValue, 32);
			}
			else
			{
				// If we don't get the full request from our parent, just return.  The user should call back to return it if they don't want it
				USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - we didn't get the full amount we requested", this);
			}
		}
		else
		{
			if ( (_CANREQUESTEXTRAPOWER != 0) and  (_EXTRAPOWERALLOCATED < 2))
			{
				if ( requestedPower > _EXTRAPOWERFORPORTS )
				{
					USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - we can request extra power from our parent but only %d and we want %d ", this, (uint32_t) _EXTRAPOWERFORPORTS, (uint32_t)requestedPower);
					extraAllocated = 0;
				}
				else
				{
					// Even tho' we only want "requestedPower", we have to ask for _CANREQUESTEXTRAPOWER
					USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - requesting %d from our parent", this, (uint32_t) _CANREQUESTEXTRAPOWER);
					UInt32	parentPowerRequest = super::RequestExtraPower(wakeType, _CANREQUESTEXTRAPOWER);
					
					USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - requested %d from our parent and got %d ", this, (uint32_t) _CANREQUESTEXTRAPOWER, (uint32_t)parentPowerRequest);
					
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
						USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - returning power %d because we didnt get enough", this, (uint32_t)parentPowerRequest);
						super::ReturnExtraPower(wakeType, parentPowerRequest);
					}
				}
			}
		}
	}
	// this method may be overriden by the IOUSBRootHubDevice class to implement this
	USBLog(5, "IOUSBHubDevice[%p]::RequestExtraWakePowerGated - extraAllocated = %d", this, (uint32_t)extraAllocated);
	
	*powerAllocated  = extraAllocated;
	
	return kIOReturnSuccess;
}

#pragma mark Return Extra Power (Wake)


IOReturn
IOUSBHubDevice::ReturnExtraPower(UInt32 type, UInt32 returnedPower)
{
	IOReturn	kr = kIOReturnSuccess;
	
	USBLog(5, "IOUSBHubDevice[%p]::ReturnExtraPower type: %d, returnedPower %d", this, (uint32_t)type, (uint32_t) returnedPower);
	
	if ( type == kUSBPowerDuringWake || type == kUSBPowerDuringWakeRevocable || type == kUSBPowerDuringWakeUSB3)
	{
		IOReturn	kr = kIOReturnSuccess;
		
		if ( IOUSBDevice::_expansionData && IOUSBDevice::_expansionData->_commandGate && IOUSBDevice::_expansionData->_workLoop)
		{
			IOCommandGate *	gate = IOUSBDevice::_expansionData->_commandGate;
			IOWorkLoop *	workLoop = IOUSBDevice::_expansionData->_workLoop;
			
			retain();
			workLoop->retain();
			gate->retain();
			
			kr = gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOUSBHubDevice::ReturnExtraWakePowerGated), (void *)type, (void *)returnedPower);
			if ( kr != kIOReturnSuccess )
			{
				USBLog(2,"IOUSBHubDevice[%p]::ReturnExtraPower GatedRequestExtraPower runAction() failed (0x%x)", this, kr);
			}
			
			gate->release();
			workLoop->release();
			release();
		}
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


IOReturn
IOUSBHubDevice::ReturnExtraWakePowerGated(uint64_t wakeType, uint64_t returnedPower)
{
	USBLog(5, "IOUSBHubDevice[%p]::ReturnExtraPowerGated - returning = %d", this, (uint32_t)returnedPower);
	
	if ( returnedPower == 0 )
		return kIOReturnSuccess;
	
	// Check to see if we had the extraPower allocated
	// (Note:  There might be a bug here in that we are assuming that any power allocated by this hub will set _EXTRAPOWERALLOCATED, and when we return ANY amount
	//         of power, we will return _CANREQUESTEXTRAPOWER, regardless.  This works, but seems fragile)
	if ( _EXTRAPOWERALLOCATED > 0 )
	{
		USBLog(5, "IOUSBHubDevice[%p]::ReturnExtraPowerGated - we had _EXTRAPOWERALLOCATED calling our parent to return %d", this, (uint32_t)_CANREQUESTEXTRAPOWER);
		
		// We allocated power from our parent, so return it now
		super::ReturnExtraPower(wakeType, _CANREQUESTEXTRAPOWER);	
		_EXTRAPOWERALLOCATED--;
		setProperty("ExtraWakePowerAllocated", _EXTRAPOWERALLOCATED, 32);
	}
	else if ( _REQUESTFROMPARENT )
	{
		super::ReturnExtraPower(wakeType, returnedPower);	
	}
	else
	{
		if (returnedPower > 0)
			_TOTALEXTRACURRENT += returnedPower;
	}
	
	return kIOReturnSuccess;
}


#pragma mark Request Extra Power (Sleep)

//================================================================================================
//
// ExtraPowerRequest - how much we need to ask from our hub parent in order to get ExtraPowerForPorts mA
// ExtraPowerForPorts - how much extra power we provide for all ports when a request for ExtraPowerRequest comes thru
//
//================================================================================================
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
        gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOUSBHubDevice::RequestSleepPowerGated), (void *)requestedPower, &extraAllocated);
        
        if ( kr != kIOReturnSuccess )
        {
            USBLog(2,"IOUSBHubDevice[%p]::RequestSleepPower GatedRequestSleepPower runAction() failed (0x%x)", this, kr);
        }
		
		gate->release();
		workLoop->release();
		release();
	}
    
	USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPower - returning %d", this, (uint32_t)extraAllocated);
	return (UInt32) extraAllocated;
}

IOReturn
IOUSBHubDevice::RequestSleepPowerGated(uint64_t requestedPower, uint64_t *powerAllocated)
{
	UInt32	extraAllocated = 0;
	
	USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - requestedPower = %d, _STANDARD_PORT_POWER_IN_SLEEP: %d, _TOTALEXTRASLEEPCURRENT: %d (_REQUESTFROMPARENT = %d)", this, (uint32_t)requestedPower, (uint32_t)_STANDARD_PORT_POWER_IN_SLEEP, (uint32_t) _TOTALEXTRASLEEPCURRENT, (uint32_t)_REQUESTFROMPARENT);
	
	if (requestedPower == 0)
	{
		USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - asked for 0, returning 0", this );
		*powerAllocated = 0;
		return kIOReturnSuccess;
	}
	
	// If we don't have any _standardPortSleepCurrent, then it means that we can't allocate any 
	if ( _STANDARD_PORT_POWER_IN_SLEEP == 0)
	{
		USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - port does not have any _STANDARD_PORT_POWER_IN_SLEEP, returning 0", this );
		*powerAllocated = 0;
		return kIOReturnSuccess;
	}
	
	// If we are requesting < _STANDARD_PORT_POWER_IN_SLEEP (i.e. 500mA), then we're good and just give it
	if ( requestedPower <= _STANDARD_PORT_POWER_IN_SLEEP)	
	{
		USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - requested <= _STANDARD_PORT_POWER_IN_SLEEP, returning %d", this, (uint32_t)requestedPower );
		*powerAllocated = requestedPower;
		return kIOReturnSuccess;
	}
	
	// At this point, we can have a hub that can request a set amount of power from its parent (e.g. a Keyboard Hub), or a hub that can pass thru the request to its parent (RMH).
	
	// if we don't have any power to allocate, let's see if we have "ExtraPower" that can be requested from our parent (and we haven't already done so).
	if ( _REQUESTFROMPARENT)
	{
		// 
		USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - requesting %d from our parent", this, (uint32_t) requestedPower);
		UInt32	parentPowerRequest = super::RequestExtraPower(kUSBPowerDuringSleep, requestedPower);
		
		USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - requested %d from our parent and got %d ", this, (uint32_t) requestedPower, (uint32_t)parentPowerRequest);
		if ( parentPowerRequest == requestedPower )
		{
			// We only can return _EXTRAPOWERFORPORTS, not less
			extraAllocated = requestedPower- _STANDARD_PORT_POWER_IN_SLEEP;
			setProperty("PortActualRequestExtraSleepPower", extraAllocated, 32);
		}
		else
		{
			// If we don't get the full request from our parent, just return.  The user should call back to return it if they don't want it
			USBLog(6, "IOUSBHubDevice[%p]::RequestSleepPowerGated - we didn't get the full amount we requested, we got only %d", this, (uint32_t)parentPowerRequest);
			extraAllocated = parentPowerRequest- _STANDARD_PORT_POWER_IN_SLEEP;
			setProperty("PortActualRequestExtraSleepPower", extraAllocated, 32);
		}
	}
	else 
	{
		// OK, at this point we are requesting more than _STANDARD_PORT_POWER_IN_SLEEP, so we need to determine how much to give them.  Subtract the _STANDARD_PORT_POWER_IN_SLEEP from the requested:
		
		if (requestedPower > _MAXPORTSLEEPCURRENT)		// limit requests to the maximum the HW can support
		{
			USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - requestedPower of %d, was greater than %d, (the maximum allowed sleep power per port).  Requesting the max instead", this, (uint32_t)requestedPower, (uint32_t) (_MAXPORTSLEEPCURRENT));
			requestedPower = _MAXPORTSLEEPCURRENT;
		}
		
		if ( (requestedPower - _STANDARD_PORT_POWER_IN_SLEEP) <= _TOTALEXTRASLEEPCURRENT)
		{		
			// honor the request
			extraAllocated = requestedPower - _STANDARD_PORT_POWER_IN_SLEEP;
			_TOTALEXTRASLEEPCURRENT -= extraAllocated;
			USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - Asked for %d extra,  _TOTALEXTRASLEEPCURRENT is now %d", this, (uint32_t)(requestedPower - _STANDARD_PORT_POWER_IN_SLEEP), (uint32_t) _TOTALEXTRASLEEPCURRENT );
		}
		else 
		{
			USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - Asked for %d extra,  but only %d _TOTALEXTRASLEEPCURRENT available", this, (uint32_t)(requestedPower - _STANDARD_PORT_POWER_IN_SLEEP), (uint32_t) _TOTALEXTRASLEEPCURRENT );
		}
	}

#if 0  // for M84/89  (not supporting providing sleep current on those hubs, for now)
	{
		if ( (_CANREQUESTEXTRASLEEPPOWER != 0) (_EXTRASLEEPPOWERALLOCATED < 2) )
		{
			if ( requestedPower > _EXTRAPOWERFORPORTS )
			{
				USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - we can request extra sleep power from our parent but only %d and we want %d ", this, (uint32_t) _EXTRAPOWERFORPORTS, (uint32_t)requestedPower);
				extraAllocated = 0;
			}
			else
			{
				// Even tho' we only want "requestedPower", we have to ask for _CANREQUESTEXTRASLEEPPOWER
				USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - requesting %d from our parent", this, (uint32_t) _CANREQUESTEXTRASLEEPPOWER);
				UInt32	parentPowerRequest = super::RequestExtraPower(kUSBPowerDuringSleep, _CANREQUESTEXTRASLEEPPOWER);
				
				USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - requested %d from our parent and got %d ", this, (uint32_t) _CANREQUESTEXTRASLEEPPOWER, (uint32_t)parentPowerRequest);
				
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
					USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - returning power %d because we didnt get enough", this, (uint32_t)parentPowerRequest);
					super::ReturnExtraPower(kUSBPowerDuringSleep, parentPowerRequest);	
				}
			}
		}
	}
#endif
	
	// this method may be overriden by the IOUSBRootHubDevice class to implement this
	USBLog(5, "IOUSBHubDevice[%p]::RequestSleepPowerGated - extraAllocated = %d, returning %d", this, (uint32_t)extraAllocated, (uint32_t)(extraAllocated + _STANDARD_PORT_POWER_IN_SLEEP));
	
	*powerAllocated = (extraAllocated + _STANDARD_PORT_POWER_IN_SLEEP);
	return kIOReturnSuccess;
}


#pragma mark Return Extra Power (Sleep)

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
        gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOUSBHubDevice::ReturnSleepPowerGated), (void *)returnedPower);
        if ( kr != kIOReturnSuccess )
        {
            USBLog(2,"IOUSBHubDevice[%p]::RequestExtraPower GatedRequestExtraPower runAction() failed (0x%x)", this, kr);
        }
		
		gate->release();
		workLoop->release();
		release();
	}
}


IOReturn
IOUSBHubDevice::ReturnSleepPowerGated(uint64_t returnedPower)
{
	USBLog(5, "IOUSBHubDevice[%p]::ReturnSleepPowerGated - returning = %d", this, (uint32_t)returnedPower);
#if 0  // M84/89
	// Check to see if we had the extraPower allocated
	// (Note:  There might be a bug here in that we are assuming that any power allocated by this hub will set _EXTRAPOWERALLOCATED, and when we return ANY amount
	//         of power, we will return _CANREQUESTEXTRAPOWER, regardless.  This works, but seems fragile)
	if ( _EXTRASLEEPPOWERALLOCATED > 0)
	{
		USBLog(5, "IOUSBHubDevice[%p]::ReturnSleepPowerGated - we had _EXTRASLEEPPOWERALLOCATED calling our parent to return %d", this, (uint32_t)_CANREQUESTEXTRASLEEPPOWER);
		
		// We allocated power from our parent, so return it now
		super::ReturnExtraPower(kUSBPowerDuringSleep, _CANREQUESTEXTRASLEEPPOWER);	
		_EXTRAPOWERALLOCATED--;
		setProperty("ExtraWakePowerAllocated", _EXTRASLEEPPOWERALLOCATED, 32);
	}
#endif		

	// If we are returning < _STANDARD_PORT_POWER_IN_SLEEP (i.e. 500mA), then we're good and just retrun it
	if ( returnedPower <= _STANDARD_PORT_POWER_IN_SLEEP)	
	{
		USBLog(5, "IOUSBHubDevice[%p]::ReturnSleepPowerGated - returned <= _STANDARD_PORT_POWER_IN_SLEEP, returning %d", this, (uint32_t)returnedPower );
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
			USBLog(5, "IOUSBHubDevice[%p]::ReturnSleepPowerGated - adding back %d to our extra", this, (uint32_t)(returnedPower - _STANDARD_PORT_POWER_IN_SLEEP));
			_TOTALEXTRASLEEPCURRENT += (returnedPower - _STANDARD_PORT_POWER_IN_SLEEP);
		}
	}
	
	return kIOReturnSuccess;
}


#pragma mark Extra Power (Other)

void
IOUSBHubDevice::InitializeExtraPower(UInt32 maxPortCurrent, UInt32 totalExtraCurrent, UInt32 maxPortCurrentInSleep, UInt32 totalExtraCurrentInSleep)
{
	OSNumber *		extraPowerProp = NULL;
	OSObject *		propertyObject = NULL;
	UInt32			deviceInfo = 0;
	bool			useNewMethodToAddRequestFromParent = false;
	
	(void) GetDeviceInformation(&deviceInfo);
	
	USBLog(5, "IOUSBHubDevice[%p]::InitializeExtraPower - maxPortCurrent = %d, totalExtraCurrent: %d, maxPortCurrentInSleep: %d, totalExtraCurrentInSleep: %d, deviceInfo: 0x%x", this, (uint32_t)maxPortCurrent, (uint32_t) totalExtraCurrent, (uint32_t)maxPortCurrentInSleep, (uint32_t)totalExtraCurrentInSleep, (uint32_t)deviceInfo);
	
	_MAXPORTCURRENT = maxPortCurrent;
	_TOTALEXTRACURRENT = totalExtraCurrent;
	_MAXPORTSLEEPCURRENT = maxPortCurrentInSleep;
	_TOTALEXTRASLEEPCURRENT = totalExtraCurrentInSleep;
	
	if ( _controller )
	{
		useNewMethodToAddRequestFromParent = (_controller->getProperty("UpdatedSleepPropertiesExists") == kOSBooleanTrue);
		if (useNewMethodToAddRequestFromParent)
		{
			USBLog(6, "IOUSBHubDevice[%p]::InitializeExtraPower  found UpdatedSleepPropertiesExists with true value", this);
		}
	}
	// If we have a hub that is (1) internal, (2) attached to the RootHub, (3) captive, and (4) is not on thunderbolt, then we will tell it to request power from its parent
	// Limit this only to machines that have the internal hubs and later.  We check that by llooking to see if the controller has the property that indicates
	// that the new sleep extra current properties existed in EFI.  This is an indication that the machine has a verified ACPI table and we can trust the PortInfo
	if ( useNewMethodToAddRequestFromParent && (deviceInfo & kUSBInformationDeviceIsInternalMask) && (deviceInfo & kUSBInformationDeviceIsAttachedToRootHubMask) && (deviceInfo & kUSBInformationDeviceIsCaptiveMask) && !(deviceInfo & kUSBInformationDeviceIsOnThunderboltMask) )
	{
		USBLog(6, "IOUSBHubDevice[%p]::InitializeExtraPower  found hub (0x%x, 0x%x), that is internal, attached to root hub, captive, not on Thunderbolt, and on a 'recent' machine", this, GetVendorID(), GetProductID());
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
		
		_REQUESTFROMPARENT = ( (getProperty("RequestExtraCurrentFromParent") == kOSBooleanTrue) && DoLocationOverrideAndModelMatch() );
		if (_REQUESTFROMPARENT)
        {
			USBLog(6, "IOUSBHubDevice[%p]::InitializeExtraPower  found RequestExtraCurrentFromParent property, setting _REQUESTFROMPARENT to true", this);
			
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
	USBLog(7, "IOUSBHubDevice[%p]::SendExtraPowerMessage - type: %d, argument: %d (_EXTRAPOWERFORPORTS: %d, _REQUESTFROMPARENT: %d)", this, (uint32_t)type, (uint32_t) returnedPower, (uint32_t)_EXTRAPOWERFORPORTS, _REQUESTFROMPARENT);
	if (_EXTRAPOWERFORPORTS || _REQUESTFROMPARENT)
	{
		// 
		USBLog(6, "IOUSBHubDevice[%p]::SendExtraPowerMessage - sending to our parent", this);
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
		USBLog(5, "IOUSBHubDevice[%p]::SendExtraPowerMessage - sending message to all devices attached to this hub", this);

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
						USBLog(7, "IOUSBHubDevice[%p]::SendExtraPowerMessage - sending  kIOUSBMessageReleaseExtraCurrent to %s", this, aDevice->getName());
						aDevice->messageClients(kIOUSBMessageReleaseExtraCurrent, &returnedPower, sizeof(UInt32));
					}
					else if ( type == kUSBPowerRequestWakeReallocate )
					{
						USBLog(7, "IOUSBHubDevice[%p]::SendExtraPowerMessage - sending  kIOUSBMessageReallocateExtraCurrent to %s", this, aDevice->getName());
						aDevice->messageClients(kIOUSBMessageReallocateExtraCurrent, NULL, 0);
					}
				}
			}
			iterator->release();
		}
		else 
		{
			USBLog(3, "IOUSBHubDevice[%p]::SendExtraPowerMessage - could not getChildIterator", this);
		}
		
	}

}

SInt32					
IOUSBHubDevice::UpdateUnconnectedExternalPorts(SInt32 count)
{
	IOReturn 	kr = kIOReturnSuccess;
	SInt32		newCount = 0;
	
	USBLog(6, "IOUSBHubDevice[%p]::UpdateUnconnectedExternalPorts  %d", this, (int32_t)count);

	if ( IOUSBDevice::_expansionData && IOUSBDevice::_expansionData->_commandGate && IOUSBDevice::_expansionData->_workLoop)
	{
		IOCommandGate *	gate = IOUSBDevice::_expansionData->_commandGate;
		IOWorkLoop *	workLoop = IOUSBDevice::_expansionData->_workLoop;
		
		retain();
		workLoop->retain();
		gate->retain();
        
        gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOUSBHubDevice::UpdateUnconnectedExternalPortsGated), (void*) count, (void*)&newCount);
        if ( kr != kIOReturnSuccess )
        {
            USBLog(2,"IOUSBHubDevice[%p]::UpdateUnconnectedExternalPorts UpdateUnconnectedExternalPortsGated runAction() failed (0x%x)", this, kr);
        }
		
		gate->release();
		workLoop->release();
		release();
	}
	
	USBLog(6, "IOUSBHubDevice[%p]::UpdateUnconnectedExternalPorts  returning %d", this, (int32_t)newCount);
	return newCount;
}

//================================================================================================
//
//   UpdateUnconnectedExternalPortsGated
//
//   This should really be called "UpdatedUnconnectedSuperSpeedPortsGated", as we are using it to
//   keep track of how many SS ports do not have a SS device attached to it.
//
//   We call this method from the hub driver in 3 ways:
//   1.  If the count > 2000, it tells us to decrease the # of ports.
//   2.  If the count is > 1000, but < 2000, it tells us to increase the # of ports.
//   3.  If the count < 1000, it tells us to set that value as the # of ports.
//
//================================================================================================
//
IOReturn					
IOUSBHubDevice::UpdateUnconnectedExternalPortsGated(SInt32 count, SInt32 * newCount)
{
	OSNumber *		numberObject = NULL;
	UInt32			unconnectedPorts = 0;
	UInt32			externalPorts = 0;
	OSObject *		propertyObject = NULL;
	IOService *		resourceService = getResourceService();
	IOReturn		kr = kIOReturnSuccess;
	bool			decreasePorts = false;
	
	if (resourceService)
	{
		USBLog(6, "IOUSBHubDevice[%p]::UpdateUnconnectedExternalPortsGated  %d", this, (int32_t)count);
		
		if (count >= 2000)
		{
			// We need to decrease the # of ports.  We set a flag and then fall through to the >= 1000 case
			decreasePorts = true;
			count -= 1000;
		}
		
		if (count >= 1000)
		{
			// We will increase or decrease the # ports, depending on our flag (decreasePorts)
			count -= 1000;
			
			propertyObject = resourceService->copyProperty(kAppleExternalSuperSpeedPorts);
			numberObject = OSDynamicCast(OSNumber, propertyObject);
			if (numberObject)
			{
				externalPorts = numberObject->unsigned32BitValue();
				USBLog(5, "IOUSBHubDevice[%p]::UpdateUnconnectedExternalPortsGated - we have a AAPL,ExternalUSBPorts with %d", this, (uint32_t)externalPorts);
			}
			OSSafeReleaseNULL(propertyObject);
			
			if (!decreasePorts)
			{
				externalPorts += count;
				_EXTERNAL_PORTS += externalPorts;
			}
			else
			{
				externalPorts -= count;
				_EXTERNAL_PORTS -= externalPorts;
			}
			
			USBLog(5, "IOUSBHubDevice[%p]::UpdateUnconnectedExternalPortsGated  Total number of external SuperSpeed ports now %d", this, (uint32_t)externalPorts);
			
			if ( count > 0 )
			{
				resourceService->setProperty(kAppleExternalSuperSpeedPorts, externalPorts, 32);
				
				// Update the number of unconnected ports.  We should really check to see if the value of that property is 0 to begin with еее
				resourceService->setProperty(kAppleUnconnectedSuperSpeedPorts, externalPorts, 32);
			}
			
			*newCount = externalPorts;
		}
		else
		{
			propertyObject = resourceService->copyProperty(kAppleUnconnectedSuperSpeedPorts);
			numberObject = OSDynamicCast(OSNumber, propertyObject);
			if (numberObject)
			{
				unconnectedPorts = numberObject->unsigned32BitValue();
				USBLog(5, "IOUSBHubDevice[%p]::UpdateUnconnectedExternalPortsGated - we have a AAPL,UnconnectedUSBPorts with %d", this, (uint32_t)unconnectedPorts);
			}
			OSSafeReleaseNULL(propertyObject);
			
			unconnectedPorts += count;
			_UNCONNECTEDEXTERNALPORTS = unconnectedPorts;
			
			USBLog(5, "IOUSBHubDevice[%p]::UpdateUnconnectedExternalPortsGated  unconnected external SuperSpeed ports now %d", this, (uint32_t)unconnectedPorts);
			resourceService->setProperty(kAppleUnconnectedSuperSpeedPorts, unconnectedPorts, 32);
			
			*newCount = unconnectedPorts;
		}
	}
	else
		kr = kIOReturnNoMemory;
	
	return kr;
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

#pragma mark Extra Power (Obsolete)

void			
IOUSBHubDevice::InitializeExtraPower(UInt32 maxPortCurrent, UInt32 totalExtraCurrent)
{
#pragma unused (maxPortCurrent, totalExtraCurrent)
	USBLog(5, "IOUSBHubDevice[%p]::InitializeExtraPower - obsolete method called", this);
}

UInt32
IOUSBHubDevice::RequestExtraPower(UInt32 requestedPower)
{
#pragma unused (requestedPower)
	USBLog(1, "IOUSBHubDevice[%p]::RequestExtraPower - OBSOLETE METHOD CALLED", this);
	return 0;
}

void
IOUSBHubDevice::ReturnExtraPower(UInt32 returnedPower)
{
#pragma unused (returnedPower)
	USBLog(1, "IOUSBHubDevice[%p]::ReturnExtraPower - OBSOLETE METHOD CALLED", this);
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


