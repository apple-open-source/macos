/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2007 Apple Inc.  All Rights Reserved.
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
			_dontAllowLowPower = false;
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
		
		USBLog(5, "IOUSBHubPolicyMaker[%p]::start - device %s, setting kUSBDeviceResumeRecoveryTime to %ld", this, _device->getName(), _hubResumeRecoveryTime ); 
	}
	else
	{
		_hubResumeRecoveryTime = kHubResumeRecoveryTime;
	}
	
	_powerStateChangingTo = kIOUSBHubPowerStateStable;							// not currently changing state..
	
	PMinit();								// since I am a policy maker, i need to 
	usbPlane = getPlane(kIOUSBPlane);
	if (usbPlane)
	{
		deviceCharacteristics = _device->GetHubCharacteristics();
		if (deviceCharacteristics & kIOUSBHubDeviceIsRootHub)
		{
			_isRootHub = true;
			// if my provider is an IOUSBRootHubDevice nub, then I should attach this hub device nub to the root.
			_device->attachToParent(getRegistryRoot(), usbPlane);
			
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

	// Check to see if we have any extra power to give to any of our downstream ports
	AllocateExtraPower();

	// call the old IOUSBHubPolicyMaker::start method, now renamed to ConfigureHubDriver
	if (!ConfigureHubDriver())
	{
		USBError(1, "IOUSBHubPolicyMaker[%p]::start - ConfigureHubDriver returned false", this);
		PMstop();
		return false;
	}
	
	makeUsable();
	// now register our controlling driver
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


//
// AllocateExtraPower
// Checks to see if the device node has an ExtraPowerRequest property, and if so, tries to make that request from the root hub
//
void
IOUSBHubPolicyMaker::AllocateExtraPower()
{
    OSNumber				*extraPowerProp;
	UInt32					extraPowerNeeded = 0;
	
	USBLog(2, "AppleUSBHub[%p]::AllocateExtraPower - _parentHubDevice(%p) _device(%p)", this, _parentHubDevice, _device);
	
	if (!_parentHubDevice || !_device)
		return;
	
    extraPowerProp = (OSNumber *)_device->getProperty("ExtraPowerRequest");
    if ( extraPowerProp )
        extraPowerNeeded = extraPowerProp->unsigned32BitValue();
    
	if (extraPowerNeeded)
	{
		_extraPower = _parentHubDevice->RequestExtraPower(extraPowerNeeded);											// request 600 ma extra per the HW team
		USBLog(2, "AppleUSBHub[%p]::AllocateExtraPower - asked for extra power (%d) and received (%d)", this, (int)extraPowerNeeded, (int)_extraPower);

		_extraPowerRemaining = _extraPower;
	}
}



IOReturn
IOUSBHubPolicyMaker::GetExtraPortPower(UInt32 portNum, UInt32 *pExtraPower)
{
	USBLog(2, "AppleUSBHub[%p]::GetExtraPortPower for port[%d] pExtraPower[%p] _extraPowerRemaining[%d]", this, (int)portNum, pExtraPower, (int)_extraPowerRemaining);
	
	if (!_extraPowerRemaining || !pExtraPower)
		return kIOReturnNoResources;
	
	*pExtraPower = _extraPowerRemaining;
	
	if (!_dontAllowSleepPower)
	{
		// now that someone has decided to use the extra power, we can go ahead and set the property for sleep as well
		_device->setProperty(kAppleExtraPowerInSleep, _extraPowerRemaining, 32);
	}
	
	_extraPowerRemaining = 0;
	
	return kIOReturnSuccess;
}



IOReturn
IOUSBHubPolicyMaker::ReturnExtraPortPower(UInt32 portNum, UInt32 extraPower)
{
	USBLog(2, "AppleUSBHub[%p]::ReturnExtraPortPower for port[%d] _extraPowerRemaining[%d] extraPower[%d]", this, (int)portNum, (int)_extraPowerRemaining, (int)extraPower);
	
	_extraPowerRemaining = extraPower;

	// since no one is getting the extra power, we remove the property
	_device->removeProperty(kAppleExtraPowerInSleep);
	
	return kIOReturnSuccess;
}



OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  0);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  1);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  2);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  3);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  4);
OSMetaClassDefineReservedUnused(IOUSBHubPolicyMaker,  5);
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
