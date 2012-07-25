/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright й 2007-2009 Apple Inc.  All rights reserved.
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

 

#include <IOKit/IOKitKeys.h>
#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>
#include <IOKit/usb/IOUSBLog.h>
#include "USBTracepoints.h"

//================================================================================================
//
//   IOKit Constructors and Destructors
//
//================================================================================================
//
OSDefineMetaClass( IOUSBHubPolicyMaker, IOService )
OSDefineAbstractStructors( IOUSBHubPolicyMaker, IOService )

#define	super	IOService

// Convert USBLog to use kprintf debugging
#ifndef IOUSBHUBPOLICYMAKER_USE_KPRINTF
#define IOUSBHUBPOLICYMAKER_USE_KPRINTF 0
#endif

#if IOUSBHUBPOLICYMAKER_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBHUBPOLICYMAKER_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

static IOPMPowerState ourPowerStates[kIOUSBHubNumberPowerStates] = {
	{	// kIOUSBHubPowerStateOFF - all ports should be off and all devices disconnected
		kIOPMPowerStateVersion1,	// version - version number of this struct
		0,							// capabilityFlags - bits that describe (to interested drivers) the capability of the device in this state
		0,							// outputPowerCharacter - description (to power domain children) of the power provided in this state
		0,							// inputPowerRequirement - description (to power domain parent) of input power required in this state
		0,							// staticPower - average consumption in milliwatts
		0,							// unbudgetedPower - additional consumption from separate power supply (mw)
		0,							// powerToAttain - additional power to attain this state from next lower state (in mw)
		0,							// timeToAttain - time required to enter this state from next lower state (in microseconds)
		0,							// settleUpTime - settle time required after entering this state from next lower state (microseconds)
		0,							// timeToLower - time required to enter next lower state from this one (in microseconds)
		0,							// settleDownTime - settle time required after entering next lower state from this state (microseconds)
		0							// powerDomainBudget - power in mw a domain in this state can deliver to its children
	},
	{	// kIOUSBHubPowerStateRestart - same as state 0, except we pass down the information to children
		kIOPMPowerStateVersion1,	// version - version number of this struct
		kIOPMRestartCapability,		// capabilityFlags - bits that describe (to interested drivers) the capability of the device in this state
		kIOPMRestart,				// outputPowerCharacter - description (to power domain children) of the power provided in this state
		kIOPMRestart,				// inputPowerRequirement - description (to power domain parent) of input power required in this state
		0,							// staticPower - average consumption in milliwatts
		0,							// unbudgetedPower - additional consumption from separate power supply (mw)
		0,							// powerToAttain - additional power to attain this state from next lower state (in mw)
		0,							// timeToAttain - time required to enter this state from next lower state (in microseconds)
		0,							// settleUpTime - settle time required after entering this state from next lower state (microseconds)
		0,							// timeToLower - time required to enter next lower state from this one (in microseconds)
		0,							// settleDownTime - settle time required after entering next lower state from this state (microseconds)
		0							// powerDomainBudget - power in mw a domain in this state can deliver to its children
	},
	{	// kIOUSBHubPowerStateSleep - all ports should be SUSPENDED and devices still connected
		kIOPMPowerStateVersion1,	// version - version number of this struct
		kIOPMSleepCapability,		// capabilityFlags - bits that describe (to interested drivers) the capability of the device in this state
		kIOPMSleep,					// outputPowerCharacter - description (to power domain children) of the power provided in this state
		kIOPMSleep,					// inputPowerRequirement - description (to power domain parent) of input power required in this state
		0,							// staticPower - average consumption in milliwatts
		0,							// unbudgetedPower - additional consumption from separate power supply (mw)
		0,							// powerToAttain - additional power to attain this state from next lower state (in mw)
		0,							// timeToAttain - time required to enter this state from next lower state (in microseconds)
		0,							// settleUpTime - settle time required after entering this state from next lower state (microseconds)
		0,							// timeToLower - time required to enter next lower state from this one (in microseconds)
		0,							// settleDownTime - settle time required after entering next lower state from this state (microseconds)
		0							// powerDomainBudget - power in mw a domain in this state can deliver to its children
	},
	{	// kIOUSBHubPowerStateLowPower - all ports should be SUSPENDED and devices still connected
		kIOPMPowerStateVersion1,	// version - version number of this struct
		kIOPMLowPower,					// capabilityFlags - bits that describe (to interested drivers) the capability of the device in this state
		kIOPMLowPower,					// outputPowerCharacter - description (to power domain children) of the power provided in this state
		kIOPMLowPower,					// inputPowerRequirement - description (to power domain parent) of input power required in this state
		0,							// staticPower - average consumption in milliwatts
		0,							// unbudgetedPower - additional consumption from separate power supply (mw)
		0,							// powerToAttain - additional power to attain this state from next lower state (in mw)
		0,							// timeToAttain - time required to enter this state from next lower state (in microseconds)
		0,							// settleUpTime - settle time required after entering this state from next lower state (microseconds)
		0,							// timeToLower - time required to enter next lower state from this one (in microseconds)
		0,							// settleDownTime - settle time required after entering next lower state from this state (microseconds)
		0							// powerDomainBudget - power in mw a domain in this state can deliver to its children
	},
	{	// kIOUSBHubPowerStateON - all ports should be going strong	
		kIOPMPowerStateVersion1,	// version - version number of this struct
		IOPMDeviceUsable,			// capabilityFlags - bits that describe (to interested drivers) the capability of the device in this state
		IOPMPowerOn,				// outputPowerCharacter - description (to power domain children) of the power provided in this state
		IOPMPowerOn,				// inputPowerRequirement - description (to power domain parent) of input power required in this state
		0,							// staticPower - average consumption in milliwatts
		0,							// unbudgetedPower - additional consumption from separate power supply (mw)
		0,							// powerToAttain - additional power to attain this state from next lower state (in mw)
		0,							// timeToAttain - time required to enter this state from next lower state (in microseconds)
		0,							// settleUpTime - settle time required after entering this state from next lower state (microseconds)
		0,							// timeToLower - time required to enter next lower state from this one (in microseconds)
		0,							// settleDownTime - settle time required after entering next lower state from this state (microseconds)
		0							// powerDomainBudget - power in mw a domain in this state can deliver to its children
	}
};



bool
IOUSBHubPolicyMaker::start(IOService * provider)
{
	const IORegistryPlane	*usbPlane = NULL;
	UInt32					deviceCharacteristics;
	IOReturn				err;
	IOUSBControllerV3		*v3Bus = NULL;
	OSBoolean				*boolObj = NULL;
	OSNumber				*numberObj = NULL;

	// remember my device
    _device		= OSDynamicCast(IOUSBHubDevice, provider);
	
	if (!_device || !super::start(provider))
		return false;
	
    _bus		= OSDynamicCast(IOUSBControllerV2, _device->GetBus());
	
	if (!_bus)
		return false;
	
	// Only allow lowPower mode if we have a V3 Controller
	v3Bus = OSDynamicCast(IOUSBControllerV3, _bus);
	if ( v3Bus )
	{
		boolObj = OSDynamicCast( OSBoolean, _device->getProperty(kUSBHubDontAllowLowPower) );
		if ( boolObj && boolObj->isTrue() )
		{
			// This hub will not go into low power mode
			_dontAllowLowPower = true;
		}
		else
		{
			// Do allow hub to go into low power
			if( (gUSBStackDebugFlags & kUSBDontAllowHubLowPowerMask) != 0)
			{
				USBLog(5, "IOUSBHubPolicyMaker[%p]::start - boot arg not allowing low power: %x", this, (uint32_t)gUSBStackDebugFlags);
				_dontAllowLowPower = true;
			}
			else
			{
				_dontAllowLowPower = false;
			}
		} 
	} 
	else
	{
		// This hub will not go into low power mode because its on a non-V3 controller
		_dontAllowLowPower = true;
	}
		
	boolObj = OSDynamicCast( OSBoolean, _device->getProperty("kUSBNoExtraSleepCurrent") );
	if ( boolObj && boolObj->isTrue() )
	{
		_dontAllowSleepPower = true;
	}
	else
	{
		_dontAllowSleepPower = false;
	}

	// Set our _hubResumeRecoveryTime, overriding with a property-based errata
	numberObj = OSDynamicCast( OSNumber, _device->getProperty(kUSBDeviceResumeRecoveryTime) );
	if ( numberObj )
	{
		_hubResumeRecoveryTime = numberObj->unsigned32BitValue();
		if ( _hubResumeRecoveryTime < 10 ) _hubResumeRecoveryTime = 10;
		
		USBLog(5, "IOUSBHubPolicyMaker[%p]::start - device %s, setting kUSBDeviceResumeRecoveryTime to %d", this, _device->getName(), (uint32_t)_hubResumeRecoveryTime ); 
	}
	else
	{
		_hubResumeRecoveryTime = kHubResumeRecoveryTime;
	}
	
	_powerStateChangingTo = kIOUSBHubPowerStateStable;							// not currently changing state..
	
	PMinit();								// since I am a policy maker, i need to 
	makeUsable();

	usbPlane = getPlane(kIOUSBPlane);
	if (usbPlane)
	{
		deviceCharacteristics = _device->GetHubCharacteristics();
		if (deviceCharacteristics & kIOUSBHubDeviceIsRootHub)
		{
			_isRootHub = true;
			
			// and since this is a root hub, we attach to the controller in the power tree
			USBLog(5, "IOUSBHubPolicyMaker[%p]::start - root policyMaker on bus[%p]", this, _bus);
			_bus->joinPMtree(this);
		}
		else
		{
			// our hub device is not a root hub. this means that we are already attached in the USB plane and so we need to copy
			// the HSBus characteristic from our device's parent
			IOUSBHubDevice			*grandParentHub = OSDynamicCast(IOUSBHubDevice, _device->getParentEntry(usbPlane));
			IOUSBHubPolicyMaker		*policyMaker = NULL;
			if (grandParentHub)
			{
				_parentHubDevice = grandParentHub;
				
				UInt32		grandCharacteristics = grandParentHub->GetHubCharacteristics();
				if (grandCharacteristics & kIOUSBHubDeviceIsOnHighSpeedBus)
				{
					USBLog(5, "IOUSBHubPolicyMaker[%p]::start - hub is on HS bus - setting characteristic", this);
					_device->SetHubCharacteristics(deviceCharacteristics | kIOUSBHubDeviceIsOnHighSpeedBus);
				}
				else
				{
					USBLog(5, "IOUSBHubPolicyMaker[%p]::start - hub is on classic speed bus - not changing characteristic", this);
				}
				if (grandCharacteristics & kIOUSBHubDeviceCanSleep)
				{
					USBLog(5, "IOUSBHubPolicyMaker[%p]::start - hub can sleep - setting characteristic", this);
					_device->SetHubCharacteristics(deviceCharacteristics | kIOUSBHubDeviceCanSleep);
				}
				else
				{
					USBLog(5, "IOUSBHubPolicyMaker[%p]::start - hub cannot sleep - not changing characteristic", this);
				}
				policyMaker = grandParentHub->GetPolicyMaker();
				if (policyMaker)
				{
					USBLog(5, "IOUSBHubPolicyMaker[%p]::start - parent policyMaker[%p]", this, policyMaker);
					policyMaker->joinPMtree(this);
				}
			}
		}
	}

	// Check to see if we have any extra power to give to any of our downstream ports (operating and sleep)
	if ( !_isRootHub)
		AllocateExtraPower();

	// call the old AppleUSBHub::start method, now renamed to ConfigureHubDriver
	if (!ConfigureHubDriver())
	{
		USBLog(1, "IOUSBHubPolicyMaker[%p]::start - ConfigureHubDriver returned false", this);
		PMstop();
		return false;
	}
	
	// After we have configure the hub, see if we have sleep-current -- the amount of power per port we can give during sleep
	numberObj = OSDynamicCast(OSNumber,  _device->getProperty(kAppleStandardPortCurrentInSleep));
	if ( numberObj )
	{
		UInt32	perPortPowerInSleep = 0;
		
		perPortPowerInSleep = numberObj->unsigned32BitValue();
		USBLog(5, "IOUSBHubPolicyMaker[%p]::start - setting per port power in sleep to %d", this, (uint32_t) perPortPowerInSleep);
		_device->SetSleepCurrent( perPortPowerInSleep);
	}
	
	// now register our controlling driver
	USBLog(7, "IOUSBHubPolicyMaker[%p]::start - calling registerPowerDriver", this);
	err = registerPowerDriver(this, ourPowerStates, kIOUSBHubNumberPowerStates);
	if (err)
	{
		USBError(1, "IOUSBHubPolicyMaker[%p]::start - err [%p] from registerPowerDriver", this, (void*)err);
		PMstop();
		return false;
	}

	// don't do this until we are all set up.
	_device->SetPolicyMaker(this);

	return true;
}



#pragma mark еееее Power Manager еееее
unsigned long 
IOUSBHubPolicyMaker::powerStateForDomainState ( IOPMPowerFlags domainState )
{
	unsigned long	ret = super::powerStateForDomainState( domainState );
	USBLog(5, "IOUSBHubPolicyMaker[%p]::powerStateForDomainState - domainState(%p) - returning (%p)", this, (void*)domainState, (void*)ret);
	return ret;
}



unsigned long 
IOUSBHubPolicyMaker::maxCapabilityForDomainState ( IOPMPowerFlags domainState )
{
	unsigned long	ret = super::maxCapabilityForDomainState( domainState );
	USBLog(5, "IOUSBHubPolicyMaker[%p]::maxCapabilityForDomainState - domainState(%p) - returning (%p)", this, (void*)domainState, (void*)ret);
	return ret;
}



IOReturn		
IOUSBHubPolicyMaker::powerStateWillChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice)
{
	USBLog(5, "IOUSBHubPolicyMaker[%p]::powerStateWillChangeTo - capabilities[%p] stateNumber[%d] whatDevice[%p]", this, (void*)capabilities, (int)stateNumber, whatDevice);

	_powerStateChangingTo = stateNumber;
	return super::powerStateWillChangeTo( capabilities, stateNumber, whatDevice);
}



IOReturn
IOUSBHubPolicyMaker::setPowerState( unsigned long powerStateOrdinal, IOService* whatDevice )
{
	IOReturn		ret;
	
	USBLog(5, "IOUSBHubPolicyMaker[%p]::setPowerState - powerStateOrdinal(%d) - whatDevice(%p) current state(%d) isInactive(%s)", this, (int)powerStateOrdinal, whatDevice, (int)_myPowerState, isInactive() ? "true" : "false");
	if ( whatDevice != this )
	{
		USBLog(1,"IOUSBHubPolicyMaker[%p]::setPowerState - whatDevice != this", this);
		USBTrace( kUSBTHubPolicyMaker, kTPSetPowerState , (uintptr_t)this, (int)powerStateOrdinal, (uintptr_t)whatDevice, (int)_myPowerState);
		return kIOPMAckImplied;
	}
	
	if ( isInactive() )
	{
		USBLog(3,"IOUSBHubPolicyMaker[%p]::setPowerState - I am inactive - bailing", this);
		return kIOPMAckImplied;
	}
	
	if (powerStateOrdinal > kIOUSBHubPowerStateOn)
	{
		USBLog(1,"IOUSBHubPolicyMaker[%p]::setPowerState - bad ordinal(%d)", this, (int)powerStateOrdinal);
		USBTrace( kUSBTHubPolicyMaker, kTPSetPowerState , (uintptr_t)this, (int)powerStateOrdinal, (int)_myPowerState, kIOPMNoSuchState);
		return kIOPMNoSuchState;
	}
	
	if (_myPowerState == powerStateOrdinal)
	{
		USBLog(5,"IOUSBHubPolicyMaker[%p]::setPowerState - already in correct power state (%d) - no op", this, (int)_myPowerState);
		return kIOPMAckImplied;
	}		
	
	ret = HubPowerChange(powerStateOrdinal);
	
	USBLog(5, "IOUSBHubPolicyMaker[%p]::setPowerState - returning (%p)", this, (void*)ret);
	return ret;
}



IOReturn		
IOUSBHubPolicyMaker::powerStateDidChangeTo(IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice)
{
	USBLog(5, "IOUSBHubPolicyMaker[%p]::powerStateDidChangeTo - capabilities[%p] stateNumber[%d] whatDevice[%p]", this, (void*)capabilities, (int)stateNumber, whatDevice);
	// I update _myPowerState here, because IOKit doesn't actually update the value returned by getPowerState() until the entire
	// power domain is in the new state. This means that I may have already made the power change, but until my children actually
	// get updated, getPowerState will appear to be in the old state, and I may need to know
	_myPowerState = stateNumber;

	return super::powerStateDidChangeTo(capabilities, stateNumber, whatDevice);
}



void
IOUSBHubPolicyMaker::powerChangeDone(unsigned long fromState)
{
    if (isInactive())
    {
        // 10762286 - if we are inactive, then it is possible that powerStateDidChangeTo did not get called
        // and so we need to recalculate our current state
        unsigned long curState = getPowerState();
        if (_myPowerState != curState)
        {
            USBLog(2, "IOUSBHubPolicyMaker[%p]::powerChangeDone - we must have skipped powerStateDidChangeTo _myPowerState(%d) curState(%d)", this, (int)_myPowerState, (int)curState);
            _myPowerState = curState;
        }
        
    }
	_powerStateChangingTo = kIOUSBHubPowerStateStable;
	if (_isRootHub && (_myPowerState < kIOUSBHubPowerStateSleep))
	{
		// 5315453 if I am in OFF or in RESTART, then either I am a PCI card root hub and we are sleeping, or 
		// we are actually going to go away for good - make sure that if we DO come back, it will be to ON
		// use changePowerStateTo instead of changePowerStateToPriv because the override used for restarting uses Priv
		// this will get reduced when LowerPowerState is called after the ports are re-started
		// this call will have no immediate affect on the actual power state because we are clamped either by our parent's 
		// power state or by an override
		changePowerStateTo(kIOUSBHubPowerStateOn);
	}
	super::powerChangeDone(fromState);
}



IOReturn				
IOUSBHubPolicyMaker::EnsureUsability(void)
{
	// this will ensure that both the controller and the root hub are in their usable state
	// we could be in the ON state currently, but we may have issued a recent changePowerState to a lower state

	USBLog(7, "IOUSBHubPolicyMaker[%p]::EnsureUsability - _myPowerState(%d) _dozeEnabled(%s)", this, (int)_myPowerState, _dozeEnabled ? "true" : "false");
	
	if ((_myPowerState != kUSBPowerStateOn) || _dozeEnabled)
	{
		USBLog(5, "IOUSBHubPolicyMaker[%p]::EnsureUsability - _myPowerState(%d) _dozeEnabled(%s) - powering ON", this, (int)_myPowerState, _dozeEnabled ? "true" : "false");
		_dozeEnabled = false;
		changePowerStateToPriv(kIOUSBHubPowerStateOn);
	}
	return kIOReturnSuccess;
}


//================================================================================================
//
// AllocateExtraPower
//
// Set's up the properties that will be used to determine if a hub (root hub or otherwise) can provide
// more than the standard amount of power
//
//================================================================================================
void
IOUSBHubPolicyMaker::AllocateExtraPower()
{
    OSNumber				*powerProp = NULL;
	UInt32					maxPortCurrent = 0;
	UInt32					totalExtraCurrent = 0;
	UInt32					maxPortCurrentInSleep = 0;
	UInt32					totalExtraCurrentInSleep = 0;
	
	USBLog(2, "IOUSBHubPolicyMaker[%p]::AllocateExtraPower - _parentHubDevice(%p - %s) _device(%p - %s)", this, _parentHubDevice, _parentHubDevice != NULL ? _parentHubDevice->getName() : "", _device, _device != NULL ? _device->getName() : "");
	
	if (!_device)
		return;

	//  First let's check if this hub has properties that indicates it has extra current available.
	//	Initially, this was just root hubs, but now there are monitor hubs that can provide 
	//	extra power as well.
	
#ifdef SUPPORTS_SS_USB
	maxPortCurrent = _device->GetSpeed() == kUSBDeviceSpeedSuper ? kUSB3MaxPowerPerPort: kUSB2MaxPowerPerPort;;	// for extra-power purposes assume all hubs have the USB spec (500/900) ma per port available;
#else
	maxPortCurrent = kUSB2MaxPowerPerPort;	// for extra-power purposes assume all hubs have 500ma per port available;
#endif
	totalExtraCurrent = 0;					// with no extra
	
	powerProp = OSDynamicCast(OSNumber,  _device->getProperty(kAppleMaxPortCurrent));
	if ( powerProp )
	{
		maxPortCurrent = powerProp->unsigned32BitValue();
		USBLog(6, "IOUSBHubPolicyMaker[%p]::AllocateExtraPower  Setting Maximum Port Current = %d", this, (uint32_t)maxPortCurrent);
	}
	else
	{
		USBLog(6, "IOUSBHubPolicyMaker[%p]::AllocateExtraPower  no kAppleMaxPortCurrent property", this);
	}
	
	powerProp = OSDynamicCast(OSNumber,  _device->getProperty(kAppleCurrentExtra));
	if ( powerProp )
	{
		totalExtraCurrent = powerProp->unsigned32BitValue();
		USBLog(6, "IOUSBHubPolicyMaker[%p]::AllocateExtraPower  Setting Total Extra Current = %d", this, (uint32_t)totalExtraCurrent);
	}
	else
	{
		USBLog(6, "IOUSBHubPolicyMaker[%p]::AllocateExtraPower  no kAppleCurrentExtra property", this);
	}
	
	// Add the properties for sleep
	powerProp = OSDynamicCast(OSNumber,  _device->getProperty(kAppleMaxPortCurrentInSleep));
	if ( powerProp )
	{
		maxPortCurrentInSleep = powerProp->unsigned32BitValue();
		USBLog(6, "IOUSBHubPolicyMaker[%p]::AllocateExtraPower  Setting Maximum Port Current in sleep = %d", this, (uint32_t)maxPortCurrentInSleep);
	}
	else
	{
		USBLog(6, "IOUSBHubPolicyMaker[%p]::AllocateExtraPower  no kAppleMaxPortCurrentInSleep property", this);
	}
	

	powerProp = OSDynamicCast(OSNumber,  _device->getProperty(kAppleCurrentExtraInSleep));
	if ( powerProp )
	{
		totalExtraCurrentInSleep = powerProp->unsigned32BitValue();
		USBLog(6, "IOUSBHubPolicyMaker[%p]::AllocateExtraPower  Setting Total Extra Current in Sleep = %d", this, (uint32_t)totalExtraCurrentInSleep);
	}
	else
	{
		USBLog(6, "IOUSBHubPolicyMaker[%p]::AllocateExtraPower  no kAppleCurrentExtraInSleep property", this);
	}
	
	
	// Go ahead and set the device properties that will allow us to parcel out extra power
	_device->InitializeExtraPower(maxPortCurrent, totalExtraCurrent, maxPortCurrentInSleep, totalExtraCurrentInSleep);
	
}



IOReturn
IOUSBHubPolicyMaker::GetExtraPortPower(UInt32 portNum, UInt32 *pExtraPower)
{
#pragma unused (portNum, pExtraPower)
	USBLog(1, "IOUSBHubPolicyMaker[%p]::GetExtraPortPower UNSUPPORTED", this);
	return kIOReturnUnsupported;
}



IOReturn
IOUSBHubPolicyMaker::ReturnExtraPortPower(UInt32 portNum, UInt32 extraPower)
{
#pragma unused (portNum, extraPower)
	USBLog(1, "IOUSBHubPolicyMaker[%p]::ReturnExtraPortPower UNSUPPORTED", this);
	
	return kIOReturnUnsupported;
}

OSMetaClassDefineReservedUsed(IOUSBHubPolicyMaker,  0);
IOReturn
IOUSBHubPolicyMaker::GetPortInformation(UInt32 portNum, UInt32 *info)
{
#pragma unused (portNum, info)
	USBLog(5, "IOUSBHubPolicyMaker[%p]::GetPortInformation  UNSUPPORTED", this);
	
	return kIOReturnUnsupported;
}

OSMetaClassDefineReservedUsed(IOUSBHubPolicyMaker,  1);
IOReturn
IOUSBHubPolicyMaker::ResetPort(UInt32 portNum)
{
#pragma unused (portNum)
	USBLog(5, "IOUSBHubPolicyMaker[%p]::ResetPort  UNSUPPORTED", this);
	
	return kIOReturnUnsupported;
}

OSMetaClassDefineReservedUsed(IOUSBHubPolicyMaker,  2);
IOReturn
IOUSBHubPolicyMaker::SuspendPort(UInt32 portNum, bool suspend )
{
#pragma unused (portNum, suspend)
	USBLog(5, "IOUSBHubPolicyMaker[%p]::SuspendPort  UNSUPPORTED", this);
	
	return kIOReturnUnsupported;
}

OSMetaClassDefineReservedUsed(IOUSBHubPolicyMaker,  3);
IOReturn
IOUSBHubPolicyMaker::ReEnumeratePort(UInt32 portNum, UInt32 options)
{
#pragma unused (portNum, options)
	USBLog(5, "IOUSBHubPolicyMaker[%p]::ReEnumeratePort  UNSUPPORTED", this);
	
	return kIOReturnUnsupported;
}

OSMetaClassDefineReservedUsed(IOUSBHubPolicyMaker,  4);
UInt32
IOUSBHubPolicyMaker::RequestExtraPower(UInt32 portNum, UInt32 type, UInt32 requestedPower)
{
#pragma unused (portNum)
	UInt32		returnValue = 0;
	
	// Need to check whether this request will exceed the maximum power for this port (as opposed to whether there is enough extra available)

	USBLog(5, "IOUSBHubPolicyMaker[%p]::RequestExtraPower  for port %d, type: %d, requested %d", this, (uint32_t)portNum,(uint32_t)type, (uint32_t) requestedPower);
	if ( type == kUSBPowerDuringWake )
	{
		returnValue = _device->RequestExtraPower( requestedPower );
	}
	else  if ( type == kUSBPowerDuringSleep )
	{
		returnValue = _device->RequestSleepPower( requestedPower );
	}
	else if ( type == kUSBPowerRequestWakeReallocate || type == kUSBPowerRequestSleepReallocate)
	{
		_device->SendExtraPowerMessage( type, requestedPower );
	}
	
	return returnValue;
}

OSMetaClassDefineReservedUsed(IOUSBHubPolicyMaker,  5);
IOReturn
IOUSBHubPolicyMaker::ReturnExtraPower(UInt32 portNum, UInt32 type, UInt32 returnedPower)
{
#pragma unused (portNum)
	IOReturn	kr = kIOReturnSuccess;
	
	USBLog(5, "IOUSBHubPolicyMaker[%p]::ReturnExtraPower  for port %d, type %d, returnedPower %d", this, (uint32_t)portNum, (uint32_t)type, (uint32_t) returnedPower);
	if ( type == kUSBPowerDuringWake )
	{
		_device->ReturnExtraPower( returnedPower );
	}
	else if ( type == kUSBPowerDuringSleep )
	{
		_device->ReturnSleepPower( returnedPower );
	}
	else if ( type == kUSBPowerRequestWakeRelease || type == kUSBPowerRequestSleepRelease )
	{
		_device->SendExtraPowerMessage( type, returnedPower );
	}
	else
		kr = kIOReturnBadArgument;
	
	return kr;
}

OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  6);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  7);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  8);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  9);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  10);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  11);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  12);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  13);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  14);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  15);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  16);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  17);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  18);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  19);
