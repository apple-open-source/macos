/*
 * Copyright © 1997-2011 Apple Inc.  All rights reserved.
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

#include <libkern/version.h>
#include <libkern/OSDebug.h>

#include <IOKit/pwr_mgt/RootDomain.h>

#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOKitKeys.h>

#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>
#include <IOKit/usb/IOUSBLog.h>
#include "USBTracepoints.h"



//================================================================================================
//
//   Globals
//
//================================================================================================
//
uint32_t *				IOUSBControllerV3::_gHibernateState;	


//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUSBControllerV2
#define _controllerCanSleep				_expansionData->_controllerCanSleep
#define _rootHubPollingRate32			_v3ExpansionData->_rootHubPollingRate32
#define _rootHubTransactionWasAborted	_v3ExpansionData->_rootHubTransactionWasAborted
#define _externalUSBDeviceAssertionID	_v3ExpansionData->_externalUSBDeviceAssertionID
#define _externalDeviceCount			_v3ExpansionData->_externalDeviceCount
#define _inCheckPowerModeSleeping		_v3ExpansionData->_inCheckPowerModeSleeping
#define	_onThunderbolt					_v3ExpansionData->_onThunderbolt
#define	_thunderboltModelID				_v3ExpansionData->_thunderboltModelID
#define	_thunderboltVendorID			_v3ExpansionData->_thunderboltVendorID
#define	_rootHubNumPortsSS				_v3ExpansionData->_rootHubNumPortsSS
#define	_rootHubNumPortsHS				_v3ExpansionData->_rootHubNumPortsHS
#define	_outstandingSSRHTrans			_v3ExpansionData->_outstandingSSRHTrans
#define	_rootHubPortsSSStartRange		_v3ExpansionData->_rootHubPortsSSStartRange
#define	_rootHubPortsHSStartRange		_v3ExpansionData->_rootHubPortsHSStartRange

#ifndef kIOPMPCISleepResetKey
	#define kIOPMPCISleepResetKey           "IOPMPCISleepReset"
#endif

#ifndef CONTROLLERV3_USE_KPRINTF
#define CONTROLLERV3_USE_KPRINTF 0
#endif

#if CONTROLLERV3_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= CONTROLLERV3_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

//================================================================================================
//
//   IOKit Constructors and Destructors
//
//================================================================================================
//
OSDefineMetaClass( IOUSBControllerV3, IOUSBControllerV2 )
OSDefineAbstractStructors(IOUSBControllerV3, IOUSBControllerV2)

//================================================================================================
//
//   IOUSBControllerV3 Methods
//
//================================================================================================
//
#pragma mark ¥¥¥¥ IOKit methods ¥¥¥¥
bool 
IOUSBControllerV3::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  return false;

    // allocate our expansion data
    if (!_v3ExpansionData)
    {
		_v3ExpansionData = (V3ExpansionData *)IOMalloc(sizeof(V3ExpansionData));
		if (!_v3ExpansionData)
			return false;
		bzero(_v3ExpansionData, sizeof(V3ExpansionData));
    }
	
 	_powerStateChangingTo = kUSBPowerStateStable;
    
    if ( !_gHibernateState )
    {
        OSData * data = OSDynamicCast(OSData, (IOService::getPMRootDomain())->getProperty(kIOHibernateStateKey));
        if (data)
        {
            _gHibernateState = (uint32_t *) data->getBytesNoCopy();
        }
        
    }
    return true;
}


bool 
IOUSBControllerV3::start( IOService * provider )
{
	IOReturn	err;
	
	_device = OSDynamicCast(IOPCIDevice, provider);
	if (_device == NULL)
	{
		return false;
	}
	
	// the controller speed is set in the ::init methods of the controller subclasses
	if (_controllerSpeed == kUSBDeviceSpeedFull)
	{
		err = CheckForEHCIController(provider);
		if (err)
		{
			USBLog(1, "IOUSBControllerV3(%s)[%p]::start - CheckForEHCIController returned (%p)", getName(), this, (void*)err);
			USBTrace( kUSBTController, kTPControllerV3Start, (uintptr_t)this, err, 0, 1 );
			return false;
		}
	}
    	
	if ( !super::start(provider))
	{
		if (_ehciController)
		{
			_ehciController->release();
			_ehciController = NULL;
		}
        return  false;
	}
	
	
	if (_expansionData && _watchdogUSBTimer && _watchdogTimerActive)
	{
		_watchdogUSBTimer->cancelTimeout();									// cancel the timer
		_watchdogTimerActive = false;
	}
	
	// Initialize the root hub timer, which will be used when we create the root hub
	_rootHubTimer = IOTimerEventSource::timerEventSource(this, (IOTimerEventSource::Action) RootHubTimerFired);
	
	if ( _rootHubTimer == NULL )
	{
		USBLog(1, "IOUSBControllerV3[%p]::UIMInitialize - couldn't allocate timer event source", this);
		USBTrace( kUSBTController, kTPControllerV3Start, (uintptr_t)this, kIOReturnNoMemory, 0, 2 );
		return kIOReturnNoMemory;
	}
	
	if ( _workLoop->addEventSource( _rootHubTimer ) != kIOReturnSuccess )
	{
		USBLog(1, "IOUSBControllerV3[%p]::UIMInitialize - couldn't add timer event source", this);
		USBTrace( kUSBTController, kTPControllerV3Start, (uintptr_t)this, kIOReturnError, 0, 3 );
		return kIOReturnError;
	}
		
	// initialize PM
	err = InitForPM();
	if (err)
	{
		USBLog(1, "IOUSBControllerV3(%s)[%p]::start - InitForPM returned (%p)", getName(), this, (void*)err);
		USBTrace( kUSBTController, kTPControllerV3Start, (uintptr_t)this, err, 0, 4 );
		return false;
	}
	
	return true;
}



void
IOUSBControllerV3::stop( IOService * provider )
{
	IOPMrootDomain *	myRootDomain = getPMRootDomain();

    USBLog(5, "IOUSBControllerV3(%s)[%p]::stop isInactive = %d", getName(), this, isInactive());
    
	if (_ehciController)
	{
		// we retain this so that we have a valid copy in case of sleep/wake
		// once we stop we will no longer sleep/wake, so we can release it
		_ehciController->release();
		_ehciController = NULL;
	}

    if (_rootHubTimer)
    {
		_rootHubTimer->cancelTimeout();
		
        if ( _workLoop )
            _workLoop->removeEventSource(_rootHubTimer);
        
        _rootHubTimer->release();
        _rootHubTimer = NULL;
    }
	
	// 8051802 - release the deep sleep assertion stuff
	if (myRootDomain && (kIOPMUndefinedDriverAssertionID != _externalUSBDeviceAssertionID))
	{
		myRootDomain->releasePMAssertion(_externalUSBDeviceAssertionID);
	}
	
	super::stop(provider);
}

bool		
IOUSBControllerV3::willTerminate(IOService * provider, IOOptionBits options)
{
	USBLog(5, "IOUSBControllerV3(%s)[%p]::willTerminate - isInactive(%s)", getName(), this, isInactive() ? "true" : "false");
	return super::willTerminate(provider, options);
}



bool		
IOUSBControllerV3::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
	USBLog(5, "IOUSBControllerV3(%s)[%p]::didTerminate - isInactive(%s)", getName(), this, isInactive() ? "true" : "false");
    
    if (_onThunderbolt)
        requireMaxBusStall(0);
    
	return super::didTerminate(provider, options, defer);
}



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// Overrides superclass implementation, to account for a couple of scenarios
// ¥ when we are waking from hibernation, we need to force the state to kUSBPowerStateOff and then to kUSBPowerStateOn
// ¥ when we have received a systemWillShutdown call, we force the proper state based on that
// ¥ when our parent is going into Doze mode (which is different than our low power mode) then we switch to ON
//		that state change always happens from PCI SLEEP, which means we wake from sleep without having lost power
//
// NB: the domainState is actually the outputCharacter of our parent node or nodes in the power plane
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
unsigned long 
IOUSBControllerV3::maxCapabilityForDomainState ( IOPMPowerFlags domainState )
{
	unsigned long		ret;
	
	if (isInactive())
	{
		// note: sometimes IOPowerManager will try to send us to IDLE in the middle of termination
		// this override of normal behavior will make sure that we always tell the PM that we can be ON during termination
		USBLog(5, "IOUSBControllerV3(%s)[%p]::maxCapabilityForDomainState - INACTIVE - assuming full ON", getName(), this);
		ret = kUSBPowerStateOn;
	}
	else
	{
        ret = super::maxCapabilityForDomainState(domainState);

		// add in the kIOPMClockNormal flag if it is already part of our inputRequirement for the ON state (KeyLargo systems)
		IOPMPowerFlags		domainStateForDoze = kIOPMDoze | (_myPowerStates[kUSBPowerStateOn].inputPowerRequirement & kIOPMClockNormal);

		// if we are currently asleep, check to see if we are waking from hibernation
		if ( (_myPowerState == kUSBPowerStateSleep) && _gHibernateState && (*_gHibernateState == kIOHibernateStateWakingFromHibernate) && !_wakingFromHibernation && !_v3ExpansionData->_wakingFromStandby)
		{
			UInt16	configCommand = _device->configRead16(kIOPCIConfigCommand);
			
			// make sure that the PCI config space is set up to allow memory access. this appears to fix some OHCI controllers
			USBLog(5, "IOUSBControllerV3(%s)[%p]::maxCapabilityForDomainState - waking from hibernation - setting flag - kIOPCIConfigCommand(0x%04x)", getName(), this, (int)configCommand);
			_device->configWrite16(kIOPCIConfigCommand, configCommand | kIOPCICommandMemorySpace);
			USBLog(5, "IOUSBControllerV3(%s)[%p]::maxCapabilityForDomainState - new kIOPCIConfigCommand(%p)", getName(), this, (void*)_device->configRead16(kIOPCIConfigCommand));
			
			UInt8			pciPMCapOffset = 0;
			UInt16			pmControlStatus = 0;
			UInt16			pmcsr =    0;
			
			// 9760024 - if we think we are doing a wake from hibernation (which is normally from S5)
			// get the PCI Power Management CSR register (if possible) and if the PME_EN bit is set
			// then asssume that we are waking from S4 instead and that all of the USB controller bits are still intact
			_device->findPCICapability(kIOPCIPowerManagementCapability, &pciPMCapOffset);
			if (pciPMCapOffset > kIOPCIConfigMaximumLatency)					// kIOPCIConfigMaximumLatency (0x3f) is the end of the standard header
			{
				pmControlStatus = pciPMCapOffset + kPCIPMRegBlockPMCSR;
			}	
			
			if (pmControlStatus)
			{
				pmcsr = _device->configRead16(pmControlStatus);
				if (pmcsr & kPCIPMCSPMEEnable)
				{
					// if the enable bit is set, then this is a wake from S4 and USB did not lose power
                    _v3ExpansionData->_wakingFromStandby = true;
					USBLog(1, "IOUSBControllerV3(%s)[%p]::maxCapabilityForDomainState - PME_EN bit is set - will wake from S4", getName(), this);
				}
			}
			
			if (!_v3ExpansionData->_wakingFromStandby)
			{
				ResetControllerState();
				EnableAllEndpoints(true);
				_wakingFromHibernation = true;
				ret = kUSBPowerStateOff;
			}
		}
		else if (_restarting)
		{
			USBLog(5, "IOUSBControllerV3(%s)[%p]::maxCapabilityForDomainState - restarting", getName(), this);
			ret = kUSBPowerStateRestart;
		}
		else if (_poweringDown)
		{
			USBLog(5, "IOUSBControllerV3(%s)[%p]::maxCapabilityForDomainState - powering off", getName(), this);
			ret = kUSBPowerStateOff;
		}
		else if (domainState == domainStateForDoze)
		{
			USBLog(5, "IOUSBControllerV3(%s)[%p]::maxCapabilityForDomainState - going into the PCI Doze state - we can support full ON..", getName(), this);
			ret = kUSBPowerStateOn;
		}
	}
	USBLog(5, "IOUSBControllerV3(%s)[%p]::maxCapabilityForDomainState - domainState[%p], returning[%p]", getName(), this, (void*)domainState, (void*)ret);
	return ret;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initialPowerStateForDomainState
//
// Overrides superclass implementation, because the OHCI has multiple
// parents that start up at different times.
// Return that we are in the On state at startup time.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
unsigned long 
IOUSBControllerV3::initialPowerStateForDomainState ( IOPMPowerFlags domainState )
{
	unsigned long ret = super::initialPowerStateForDomainState(domainState);
	USBLog(5, "IOUSBControllerV3(%s)[%p]::initialPowerStateForDomainState - domainState[%p], returning[%p]", getName(), this, (void*)domainState, (void*)ret);
	return ret;
}



IOReturn
IOUSBControllerV3::powerStateWillChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice)
{
	USBLog(5, "IOUSBControllerV3(%s)[%p]::powerStateWillChangeTo - capabilities(%p) - stateNumber (%d) - whatDevice(%p): currrent _myPowerState(%d)", getName(), this, (void*)capabilities, (int)stateNumber, whatDevice, (int)_myPowerState);

	_powerStateChangingTo = stateNumber;
	return super::powerStateWillChangeTo(capabilities, stateNumber, whatDevice);
}



IOReturn
IOUSBControllerV3::setPowerState( unsigned long powerStateOrdinal, IOService* whatDevice )
{
	USBTrace_Start( kUSBTController, kTPControllersetPowerState, (uintptr_t)this, (int)powerStateOrdinal, (uintptr_t)whatDevice, (int)_myPowerState );
	USBLog(5, "IOUSBControllerV3(%s)[%p]::setPowerState - powerStateOrdinal(%d) - whatDevice(%p) current state(%d)", getName(), this, (int)powerStateOrdinal, whatDevice, (int)_myPowerState);
	if ( whatDevice != this )
	{
		USBLog(1,"IOUSBControllerV3(%s)[%p]::setPowerState - whatDevice != this", getName(), this);
		USBTrace( kUSBTController, kTPControllersetPowerState, (uintptr_t)this, 0, 0, 1 );
		return kIOPMAckImplied;
	}
	
	if ( isInactive() )
	{
		USBLog(1,"IOUSBControllerV3(%s)[%p]::setPowerState - isInactive - no op", getName(), this);
		USBTrace( kUSBTController, kTPControllersetPowerState, (uintptr_t)this, 0, 0, 2 );
		_myPowerState = powerStateOrdinal;
		return kIOPMAckImplied;
	}
	
	if (powerStateOrdinal > kUSBPowerStateOn)
	{
		USBLog(1,"IOUSBControllerV3(%s)[%p]::setPowerState - bad ordinal(%d)", getName(), this, (int)powerStateOrdinal);
		USBTrace( kUSBTController, kTPControllersetPowerState, (uintptr_t)this, 0, 0, 3 );
		return kIOPMNoSuchState;
	}
	
	if (_myPowerState == powerStateOrdinal)
	{
		USBLog(5,"IOUSBControllerV3(%s)[%p]::setPowerState - already in correct power state (%d) - no op", getName(), this, (int)_myPowerState);
		return kIOPMAckImplied;
	}		

	if ((_myPowerState < kUSBPowerStateLowPower) && (powerStateOrdinal >= kUSBPowerStateLowPower))
	{
		// we turn off bus mastering when we go to sleep, and it is off at INIT time by default, so as we bring the controller
		// into an active state, we enable it
		EnableBusMastering(true);
	}
	else if ((_myPowerState > kUSBPowerStateSleep) && (powerStateOrdinal <= kUSBPowerStateSleep))
	{
		// if we are currently running or dozing, and we are going to sleep or lower, then we need to cancel
		// the watchdog timer
		if (_expansionData && _watchdogUSBTimer && _watchdogTimerActive)
		{
			_watchdogUSBTimer->cancelTimeout();									// cancel the timer
			_watchdogTimerActive = false;
		}
	}
	
	if (_ehciController)
	{
		if (_wakingFromHibernation)
		{
			// in the case of waking from Hibernation, we just need to make sure that the EHCI controller gets to a state > Sleep before I proceed
			while (_ehciController->_myPowerState <= kUSBPowerStateSleep)
			{
				USBLog(5, "IOUSBControllerV3(%s)[%p]::setPowerState - _wakingFromHibernation - waiting for EHCI controller (%p) to get higher than SLEEP before I proceed - current state (%d)", getName(), this, _ehciController, (int)_myPowerState);
				IOSleep(10);
			}
			USBLog(5, "IOUSBControllerV3(%s)[%p]::setPowerState - done waiting for EHCI controller - current state (%d)", getName(), this, (int)_myPowerState);
		}
		else 
		{
			// if I am turning ON from a state < Doze, and I have an EHCI controller (which means I am a companion controller)
			// then I need to wait for the EHCI controller to get into a state > doze
			// i need to do this before i get into the workLoop gate
			if ((_myPowerState < kUSBPowerStateLowPower) && (powerStateOrdinal >= kUSBPowerStateLowPower))
			{
				while (_ehciController->_myPowerState < kUSBPowerStateLowPower)
				{
					USBLog(5, "IOUSBControllerV3(%s)[%p]::setPowerState - waiting for EHCI controller (%p) to power up before I proceed - current state (%d)", getName(), this, _ehciController, (int)_myPowerState);
					IOSleep(10);
				}
				USBLog(5, "IOUSBControllerV3(%s)[%p]::setPowerState - done waiting for EHCI controller - current state (%d)", getName(), this, (int)_myPowerState);
			}
			
			// if I am turning OFF (or Restarting) from a state > Restart, and I have an EHCI controller (which means I am a companion controller)
			// then I need to wait for the EHCI controller to get into a state < Sleep
			// i need to do this before i get into the workLoop gate
			if ((_myPowerState > kUSBPowerStateRestart) && (powerStateOrdinal <= kUSBPowerStateRestart))
			{
				while (_ehciController->_myPowerState > kUSBPowerStateRestart)
				{
					USBLog(5, "IOUSBControllerV3(%s)[%p]::setPowerState - waiting for EHCI controller (%p) to power down before I proceed - current state (%d)", getName(), this, _ehciController, (int)_myPowerState);
					IOSleep(10);
				}
				USBLog(5, "IOUSBControllerV3(%s)[%p]::setPowerState - done waiting for EHCI controller - current state (%d)", getName(), this, (int)_myPowerState);
			}
		}
	}
	
	// call the method to send the request throuh the workloop gate
	HandlePowerChange(powerStateOrdinal);
	
	USBLog(5, "IOUSBControllerV3(%s)[%p]::setPowerState - returning kIOPMAckImplied", getName(), this);
	USBTrace_End( kUSBTController, kTPControllersetPowerState, (uintptr_t)this, _myPowerState, 0, 0);
	
	return kIOPMAckImplied;
}



IOReturn
IOUSBControllerV3::powerStateDidChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice)
{
	
	USBLog(5, "IOUSBControllerV3(%s)[%p]::powerStateDidChangeTo - capabilities(%p) - stateNumber (%d) - whatDevice(%p): current state(%d)", getName(), this, (void*)capabilities, (int)stateNumber, whatDevice, (int)_myPowerState);

	return super::powerStateDidChangeTo(capabilities, stateNumber, whatDevice);
}



void					
IOUSBControllerV3::powerChangeDone ( unsigned long fromState)
{
	USBLog((fromState == _myPowerState) ? 7 : 5, "IOUSBControllerV3(%s)[%p]::powerChangeDone - fromState (%d) current state (%d) _wakingFromHibernation(%s) _needToAckPowerDown(%s)", getName(), this, (int)fromState, (int)_myPowerState, _wakingFromHibernation ? "true" : "false", _needToAckPowerDown ? "true" : "false");
	_powerStateChangingTo = kUSBPowerStateStable;
    if (_v3ExpansionData && (_v3ExpansionData->_wakingFromStandby))
    {
        USBLog(1, "IOUSBControllerV3(%s)[%p]::powerChangeDone - clearing _wakingFromStandby", getName(), this);
        _v3ExpansionData->_wakingFromStandby = false;
    }
    
	if (_wakingFromHibernation)
	{
		USBLog(5, "IOUSBControllerV3(%s)[%p]::powerChangeDone - OFF while _wakingFromHibernation - turning ON", getName(), this);		
		_wakingFromHibernation = false;
		_poweringDown = false;
		_restarting = false;
		
		// the following sequence may seem a little odd
		// we are completing a tranition to state 0 because we are waking from hibernation and we told the power manager (in maxCapabilityForDomainState)
		// that our maximum state was 0. so now we need to tell the power manager to go to the ON state to restart the controller. However, at the moment,
		// our changePowerStateToPriv is set to LowPower, since that is what we leave it on for the most part (see InitForPM) and let the root hub control the actual state
		// so first we need to change the state to ON, then make sure that the root hub is driving the state, then let off to the LowPower state as the root hub will allow.
		
		changePowerStateToPriv(kUSBPowerStateOn);				// go to ON first
		EnsureUsability();										// tell the root hub to start notcing things and to control the state
		changePowerStateToPriv(kUSBPowerStateLowPower);			// back off and let the root hub be in charge
	}
	else if (_needToAckPowerDown)
	{
		if (_myPowerState == kUSBPowerStateRestart)
		{
			_needToAckPowerDown = false;
			USBLog(5, "IOUSBControllerV3(%s)[%p]::powerChangeDone - acknowledging systemWillShutdown(kIOMessageSystemWillRestart)", getName(), this);
			IOService::systemWillShutdown(kIOMessageSystemWillRestart);
		}
		else if (_myPowerState == kUSBPowerStateOff)
		{
			_needToAckPowerDown = false;
			USBLog(5, "IOUSBControllerV3(%s)[%p]::powerChangeDone - acknowledging systemWillShutdown(kIOMessageSystemWillPowerOff)", getName(), this);
			IOService::systemWillShutdown(kIOMessageSystemWillPowerOff);
		}
		else
		{
			USBLog(5, "IOUSBControllerV3(%s)[%p]::powerChangeDone - NOT acknowledging systemWillShutdown yet", getName(), this);
		}
	}
	super::powerChangeDone(fromState);
}



void		
IOUSBControllerV3::systemWillShutdown( IOOptionBits specifier )
{
	bool		ackNow = true;
	
    USBLog(5, "IOUSBControllerV3(%s)[%p]::systemWillShutdown - specifier(%p)", getName(), this, (void*)specifier);
    switch (specifier)
    {
		case kIOMessageSystemWillRestart:
			ackNow = false;
			_needToAckPowerDown = true;
			_restarting = true;
			USBLog(5, "IOUSBControllerV3(%s)[%p]::systemWillShutdown - changing power state to restart", getName(), this);
			changePowerStateToPriv(kUSBPowerStateRestart);
			powerOverrideOnPriv();
			break;
			
		case kIOMessageSystemWillPowerOff:
			ackNow = false;
			_needToAckPowerDown = true;
			_poweringDown = true;
			USBLog(5, "IOUSBControllerV3(%s)[%p]::systemWillShutdown - changing power state to off", getName(), this);
			changePowerStateToPriv(kUSBPowerStateOff);
			powerOverrideOnPriv();
			break;
		
		default:
			break;
    }
	if (ackNow)
	{
		USBLog(5, "AppleUSBUHCI[%p]::systemWillShutdown - acknowledging", this);
		IOService::systemWillShutdown(specifier);
	}
}



void
IOUSBControllerV3::free()
{
 	//  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_v3ExpansionData)
    {
		IOFree(_v3ExpansionData, sizeof(V3ExpansionData));
		_v3ExpansionData = NULL;
    }

	super::free();
}



#pragma mark ¥¥¥¥ class methods ¥¥¥¥
IOReturn
IOUSBControllerV3::CheckForEHCIController(IOService *provider)
{
	OSIterator				*siblings = NULL;
	OSIterator				*ehciList = NULL;
    IOService				*service;
    IORegistryEntry			*entry;
    bool					ehciPresent = false;
	int						myDeviceNum = 0;
	int						ehciDeviceNum = 0;
	IOUSBControllerV3 *		testEHCI = NULL;
	int						checkListCount = 0;
    
	USBLog(6, "+%s[%p]::CheckForEHCIController", getName(), this);
	
	// Check my provide (_device) parent (a PCI bridge) children (sibling PCI functions)
    // to see if any of them is an EHCI controller - if so, wait for it..
    if ( _device )
	{
		myDeviceNum = _device->getDeviceNumber();
	}
	
	if (provider)
	{
		siblings = provider->getParentEntry(gIOServicePlane)->getChildIterator(gIOServicePlane);

	}
	else
	{
		USBLog(2, "%s[%p]::CheckForEHCIController - NULL provider", getName(), this);
	}
	
	if ( siblings ) 
	{
		while( (entry = OSDynamicCast(IORegistryEntry, siblings->getNextObject())))
		{
			UInt32			classCode;
			OSData			*obj = OSDynamicCast(OSData, entry->getProperty("class-code"));
			if (obj) 
			{
				classCode = *(UInt32 *)obj->getBytesNoCopy();
				if (classCode == 0x0c0320) 
				{
					ehciPresent = true;
					break;
				}
			}
		}
		siblings->release();
	}
	else
	{
		USBLog(2, "%s[%p]::CheckForEHCIController - NULL siblings", getName(), this);
	}
	
	// Look for our "companion" EHCI controller.  If it's not the first one we find, then keep looking
	// until it appears, timing out after 5 seconds (but loop every 10ms).
    if (ehciPresent) 
	{	
		OSDictionary *		matchingDictionary = serviceMatching("AppleUSBEHCI");				// 10397671: thedictionary is not consumed
		
        USBLog(7, "%s[%p]::CheckForEHCIController calling waitForMatchingService for AppleUSBEHCI", getName(), this);
		if (matchingDictionary)
		{
			service = waitForMatchingService( matchingDictionary, (uint64_t)(5 * NSEC_PER_SEC));	
			testEHCI = (IOUSBControllerV3*)service;
		}
		
		while (testEHCI)
		{
			IOPCIDevice *	testPCI = (IOPCIDevice*)testEHCI->getParentEntry(gIOServicePlane);
			ehciDeviceNum = testPCI->getDeviceNumber();
			
			if (myDeviceNum == ehciDeviceNum)
			{
				USBLog(5, "%s[%p]::CheckForEHCIController - ehciDeviceNum and myDeviceNum match (%d)", getName(), this, myDeviceNum);
				_ehciController = testEHCI;
				_ehciController->retain();
				testEHCI->release();										// 10397671: this needs to be released since we switch to waitForMatchingServices
				USBLog(7, "%s[%p]::CheckForEHCIController got EHCI service %p", getName(), this, service);
				setProperty("Companion", "yes");
				break;
			}
			else
			{
				// we found an instance of EHCI, but it doesn't appear to be ours, so now I need to see how many there are in the system
				// and see if any of them matches
				USBLog(5, "%s[%p]::CheckForEHCIController - ehciDeviceNum(%d) and myDeviceNum(%d) do NOT match", getName(), this, ehciDeviceNum, myDeviceNum);

				testEHCI->release();									// 10397671: release the old one

				if (ehciList)
				{
					testEHCI = (IOUSBControllerV3*)(ehciList->getNextObject());
					if (testEHCI)
					{
						USBLog(3, "%s[%p]::CheckForEHCIController - found AppleUSBEHCI after %d ms", getName(), this, checkListCount * 10);
						testEHCI->retain();
					}
				}
				else
				{
					testEHCI = NULL;
				}
				
				if (!testEHCI && (checkListCount++ < 500))
				{
					if (ehciList)
						ehciList->release();
										
					IOSleep(10);
					
					USBLog(5, "%s[%p]::CheckForEHCIController - getting an AppleUSBEHCI list", getName(), this);
					ehciList = getMatchingServices(matchingDictionary);
					if (ehciList)
					{
						testEHCI = (IOUSBControllerV3*)(ehciList->getNextObject());
						if (testEHCI)
						{
							USBLog(5, "%s[%p]::CheckForEHCIController - got AppleUSBEHCI[%p] from the list", getName(), this, testEHCI);
							testEHCI->retain();							// 10397671: retain this since I don't have a reference from getNextObject
						}
					}
				}
			}
		}
    }
	
	// We know there has to be a "companion" EHCI controller, so if there isn't, log it out
	if ( !ehciPresent || checkListCount == 500 )
	{
		USBError(1, "We could not find a corresponding USB EHCI controller for our OHCI controller at PCI device number%d", myDeviceNum);
	}
	
	if (ehciList)
		ehciList->release();

	USBLog(6, "-%s[%p]::CheckForEHCIController", getName(), this);

	return kIOReturnSuccess;
}



//
// AllocatePowerStateArray
// This is the default implementation, which is 5 states
// A subclass can either call up to this and change the states afterwards, or it can replace it completely
//
IOReturn
IOUSBControllerV3::AllocatePowerStateArray(void)
{
	unsigned long	i;
	
	_numPowerStates = kUSBNumberBusPowerStates;
	
	_myPowerStates = (IOPMPowerState*)IOMalloc(kUSBNumberBusPowerStates * sizeof(IOPMPowerState));
	if (!_myPowerStates)
	{
		USBLog(1, "IOUSBControllerV3(%s)[%p]::AllocatePowerStateArray - no memory", getName(), this);
		USBTrace( kUSBTController, kTPAllocatePowerStateArray, (uintptr_t)this, kIOReturnNoMemory, 0, 0 );
		return kIOReturnNoMemory;
	}
	bzero(_myPowerStates, kUSBNumberBusPowerStates * sizeof(IOPMPowerState));
	
	// initialize every state to version 1
	for (i=0; i < _numPowerStates; i++)
	{
		_myPowerStates[i].version = kIOPMPowerStateVersion1;
	}
	
	// Nothing else needed for state 0 (off) because all of the rest of the fields are 0
	
	// state 1 is restart - almost identical to OFF, but different in that the downstream devcies might care
	_myPowerStates[kUSBPowerStateRestart].capabilityFlags = kIOPMRestartCapability;
	_myPowerStates[kUSBPowerStateRestart].outputPowerCharacter = kIOPMRestart;
	
	// state 2 is sleep
	_myPowerStates[kUSBPowerStateSleep].capabilityFlags = kIOPMSleepCapability;
	_myPowerStates[kUSBPowerStateSleep].outputPowerCharacter = kIOPMSleep;
	// the following may appear backwards, but in fact is correct IOPCIFamily only outputs 0 for sleep state
	// so if we know we can sleep, we request 0. Otherwise, we request kIOPMSleep, which we will not get, so we
	// will be forced into state OFF instead
	_myPowerStates[kUSBPowerStateSleep].inputPowerRequirement = _controllerCanSleep ? 0 : kIOPMSleep;	

	// state 3 is doze (aka idle)
	_myPowerStates[kUSBPowerStateLowPower].capabilityFlags = kIOPMLowPower;
	_myPowerStates[kUSBPowerStateLowPower].outputPowerCharacter = kIOPMLowPower;
	_myPowerStates[kUSBPowerStateLowPower].inputPowerRequirement = IOPMPowerOn;					// PCI needs to be on for me to doze
	
	// state 4 is running
	_myPowerStates[kUSBPowerStateOn].capabilityFlags = IOPMDeviceUsable;
	_myPowerStates[kUSBPowerStateOn].outputPowerCharacter = IOPMPowerOn;
	_myPowerStates[kUSBPowerStateOn].inputPowerRequirement = IOPMPowerOn;
	
	return kIOReturnSuccess;
}



// InitForPM
// Called after AllocatePowerStateArray
// registers our power state array
IOReturn
IOUSBControllerV3::InitForPM(void)
{
	IOReturn			err;
	IOPMrootDomain *	myRootDomain = getPMRootDomain();

	// initialize PM
	// PMinit and joinPMtree are called in IOUSBController
		
	err = AllocatePowerStateArray();
	if (err)
	{
		USBLog(1, "IOUSBControllerV3(%s)[%p]::InitForPM - AllocatePowerStateArray returned (%p)", getName(), this, (void*)err);
		USBTrace( kUSBTController, kTPInitForPM, (uintptr_t)this, err, 0, 0 );
		return kIOReturnNoMemory;
	}
	
	// 8051802 - USB support for deep sleep
	// We need to create our deep sleep assertion even in the PMRootDomain
	if (myRootDomain)
	{
		_externalUSBDeviceAssertionID = myRootDomain->createPMAssertion(kIOPMDriverAssertionUSBExternalDeviceBit, kIOPMDriverAssertionLevelOff, this, _device->getName());
		if (kIOPMUndefinedDriverAssertionID == _externalUSBDeviceAssertionID)
		{
			USBLog(1, "IOUSBControllerV3(%s)[%p]::InitForPM - createPMAssertion failed", getName(), this);
		}
	}
	
	// start by calling makeUsablel, which is essentitally a changePowerStateToPriv(ON)
	// this will cause the root hub to be created and attached as a child, at which point the controller
	// will track the state of the child
	makeUsable();						// start in fully ON state
	
	USBLog(3, "IOUSBControllerV3(%s)[%p]::InitForPM - calling registerPowerDriver", getName(), this);
	registerPowerDriver(this, _myPowerStates, _numPowerStates);
	
	// now that we know the child will be in charge, lower the other two inputs
	changePowerStateTo(kUSBPowerStateLowPower);
	changePowerStateToPriv(kUSBPowerStateLowPower);
	
	return kIOReturnSuccess;
}



IOReturn
IOUSBControllerV3::CheckPowerModeBeforeGatedCall(char *fromStr)
{
#pragma unused (fromStr)
	
	IOReturn	kr = kIOReturnSuccess;
	SInt32		retries = 100;
	
	// This method will sleep the running thread if the power state is not > Sleep
	// An exception will be if we are already on the workLoop thread
	// USBTrace_Start( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this );
	
	if (!_workLoop)								// this should never happen - good luck if it does
		return kIOReturnNotPermitted;
	
	if (_myPowerState != kUSBPowerStateOn)
	{
		// we are not currently ON - see if we are perhaps transitioning to ON
		if ((_powerStateChangingTo != kUSBPowerStateStable) && (_powerStateChangingTo != kUSBPowerStateOn))
		{
			USBLog(2, "IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall from (%s) - we are already in the process of changing state to (%d) - returning kIOReturnNotResponding", getName(), this, fromStr, (int)_powerStateChangingTo);
			kr =  kIOReturnNotResponding;
		}
		else if (_workLoop->onThread())
		{
			// we are already running on the workLoop - if we are at a reduced power state, then we might be headed for trouble..
			if (!_controllerAvailable)
			{
				USBLog(1, "IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall - call (%s) - onThread is true while !_controllerAvailable", getName(), this, fromStr);
				kr = kIOReturnNotReady;
				USBTrace( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this, (int)_myPowerState, kr, 1 );
			}
		}
		else if (_workLoop->inGate())
		{
			// We are holding the workloop gate, but in order to complete the power change to ON we need the gate, so commandSleep() with timeout and wait for the wakeup
			AbsoluteTime	deadline;
			IOCommandGate	* commandGate = GetCommandGate();
			
			if ( !commandGate )
			{
				USBLog(1,"IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall commandGate is NULL, returning kIOReturnNoMemory", getName(), this);
				kr = kIOReturnNoMemory;
			}
			else
			{
				USBLog(5,"IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall  inGate() is true, so commandSleep()'ing and waiting for our powerChange", getName(), this);
				USBTrace( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this, _myPowerState, 0, 10 );

				_inCheckPowerModeSleeping = true;
				
				clock_interval_to_deadline(5, kSecondScale, &deadline);
				IOReturn err = commandGate->commandSleep(&_inCheckPowerModeSleeping, deadline, THREAD_ABORTSAFE);
				
				switch (err)
				{
					case THREAD_AWAKENED:
						USBLog(6,"IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall commandSleep woke up normally (THREAD_AWAKENED) _myPowerState: %d", getName(), this, (uint32_t)_myPowerState );
						USBTrace( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this, _myPowerState, err, 4 );
						kr = kIOReturnSuccess;
						break;
						
					case THREAD_TIMED_OUT:
						USBLog(3,"IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall commandSleep timeout out (THREAD_TIMED_OUT) _myPowerState: %d", getName(), this, (uint32_t)_myPowerState );
						USBTrace( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this, _myPowerState, err, 5 );
						_inCheckPowerModeSleeping = false;
						kr = kIOReturnTimeout;
						break;
						
					case THREAD_INTERRUPTED:
						USBLog(3,"IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall commandSleep interrupted (THREAD_INTERRUPTED) _myPowerState: %d", getName(), this, (uint32_t)_myPowerState );
						USBTrace( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this, _myPowerState, err, 6 );
						_inCheckPowerModeSleeping = false;
						kr = kIOReturnAborted;
						break;
						
					case THREAD_RESTART:
						USBLog(3,"IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall commandSleep restarted (THREAD_RESTART) _myPowerState: %d", getName(), this, (uint32_t)_myPowerState);
						USBTrace( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this, _myPowerState, err, 7 );
						_inCheckPowerModeSleeping = false;
						kr = kIOReturnInternalError;
						break;
						
					case kIOReturnNotPermitted:
						USBLog(3,"IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall woke up with status (kIOReturnNotPermitted) - we do not hold the WL!", getName(), this);
						USBTrace( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this, _myPowerState, err, 8 );
						_inCheckPowerModeSleeping = false;
						kr = kIOReturnNotPermitted;
						break;
						
					default:
						USBLog(3,"IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall woke up with unknown status %p, _myPowerState: %d",  getName(), this, (void*)kr, (uint32_t)_myPowerState);
						USBTrace( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this, _myPowerState, err, 9 );
						_inCheckPowerModeSleeping = false;
						kr = kIOReturnNotPermitted;
				}
			}
		}
		else
		{
			// we are not on the thread, but we are about to be. In that case, sleep the running thread until we wake up
			while ( (_myPowerState != kUSBPowerStateOn) and (retries-- > 0) )
			{
#if 0
				char*		bt[8];
				
				OSBacktrace((void**)bt, 8);
				
				USBLog(4, "IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall - call (%s) while _myPowerState(%d) _controllerAvailable(%s) _powerStateChangingTo(%d) - sleeping thread, bt:[%p][%p][%p][%p][%p][%p][%p]", getName(), this, fromStr, (int)_myPowerState, _controllerAvailable ? "true" : "false", (int)_powerStateChangingTo, bt[1], bt[2], bt[3], bt[4], bt[5], bt[6], bt[7]);
#endif
				IOSleep(10);
			}
		}
		
		// If we "timed out" waiting for the state change, then just return an error
		if ( retries < 1 )
		{
			USBLog(1, "IOUSBControllerV3(%s)[%p]::CheckPowerModeBeforeGatedCall - call (%s) while _myPowerState(%d) _controllerAvailable (%s) - we could not wake up the controller, returning kIOReturnNotResponding", getName(), this, fromStr, (int)_myPowerState, _controllerAvailable ? "true" : "false");
			kr = kIOReturnNotResponding;
			USBTrace( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this, (int)_myPowerState, kr, 2 );
		}
	}

	// USBTrace_End( kUSBTController, kTPControllerCheckPowerModeBeforeGatedCall, (uintptr_t)this, kr);

	return kr;
}



IOReturn
IOUSBControllerV3::DoEnableAddressEndpoints(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3 )
{
#pragma unused (arg2, arg3)
    IOUSBControllerV3 *me = (IOUSBControllerV3 *)owner;
	
    return me->UIMEnableAddressEndpoints((USBDeviceAddress)(uintptr_t)arg0, (bool)(uintptr_t) arg1);
}



IOReturn
IOUSBControllerV3::EnableAddressEndpoints(USBDeviceAddress address, bool enable)
{
	IOCommandGate * 	commandGate = GetCommandGate();
	
    return commandGate->runAction(DoEnableAddressEndpoints, (void*)(uintptr_t)address, (void*)enable);
}



IOReturn
IOUSBControllerV3::DoEnableAllEndpoints(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3 )
{
#pragma unused (arg1, arg2, arg3)
    IOUSBControllerV3 *me = (IOUSBControllerV3 *)owner;
	
    return me->UIMEnableAllEndpoints((bool)(uintptr_t)arg0);
}



IOReturn
IOUSBControllerV3::EnableAllEndpoints(bool enable)
{
	IOCommandGate * 	commandGate = GetCommandGate();
	
    return commandGate->runAction(DoEnableAllEndpoints, (void*)enable);
}


void					
IOUSBControllerV3::ControllerOff(void)
{
	USBLog(5, "IOUSBControllerV3(%s)[%p]::ControllerOff - calling ResetControllerState", getName(), this);
	ResetControllerState();
	if (_expansionData && _watchdogUSBTimer && _watchdogTimerActive)
	{
		_watchdogUSBTimer->cancelTimeout();									// cancel the timer
		_watchdogTimerActive = false;
	}
}



void					
IOUSBControllerV3::ControllerRestart(void)
{
	ControllerOff();
}



void					
IOUSBControllerV3::ControllerSleep(void)
{
	if (_myPowerState == kUSBPowerStateLowPower)
	{
		USBLog(5, "IOUSBControllerV3(%s)[%p]::ControllerSleep - dozing now - need to wake controller from doze first", getName(), this);
		WakeControllerFromDoze();
	}
	EnableInterruptsFromController(false);
	USBLog(5, "IOUSBControllerV3(%s)[%p]::ControllerSleep - calling SaveControllerStateForSleep", getName(), this);
	SaveControllerStateForSleep();
}



void					
IOUSBControllerV3::ControllerDoze(void)
{
	USBLog(5, "IOUSBControllerV3(%s)[%p]::ControllerDoze", getName(), this);
	switch(_myPowerState)
	{
		case kUSBPowerStateOff:
		case kUSBPowerStateRestart:
			USBLog(5, "IOUSBControllerV3(%s)[%p]::ControllerDoze - OFF now - need to restart before dozing", getName(), this);
			RestartControllerFromReset();
			break;
			
		case kUSBPowerStateSleep:
			USBLog(5, "IOUSBControllerV3(%s)[%p]::ControllerDoze - sleeping now - need to wake from sleep before dozing", getName(), this);
			RestoreControllerStateFromSleep();
			break;
	}
	USBLog(5, "IOUSBControllerV3(%s)[%p]::ControllerDoze - calling DozeController", getName(), this);
	DozeController();
}


void
IOUSBControllerV3::ControllerOn(void)
{
	USBLog(5, "IOUSBControllerV3(%s)[%p]::ControllerOn - current state (%d) _ehciController(%p)", getName(), this, (int)_myPowerState, _ehciController);
	    
    UInt16 statusRegister = 0;
    statusRegister = _device->configRead16(kIOPCIConfigStatus);
    if ( statusRegister & kIOPCIStatusMasterAbortActive )
    {
        
        if ( gUSBStackDebugFlags & kUSBMasterAbortPanicMask )
        {
            panic("IOUSBControllerV3(%s)[%p] PCI Master Abort status is active\n", getName(), this);
        }
        else if ( gUSBStackDebugFlags & kUSBMasterAbortLoggingMask )
        {
            IOLog("IOUSBControllerV3(%s)[%p] PCI Master Abort status is active\n", getName(), this); 
        }
        
    }
    
	// how we turn on depends on the current state
	switch(_myPowerState)
	{
		case kUSBPowerStateOff:
		case kUSBPowerStateRestart:
			RestartControllerFromReset();
			break;
		
		case kUSBPowerStateSleep:
			RestoreControllerStateFromSleep();
			break;
		
		case kUSBPowerStateLowPower:
			WakeControllerFromDoze();
			break;
	}
	
}



bool					
IOUSBControllerV3::IsControllerAvailable(void)
{
	return _controllerAvailable;
}



IOReturn
IOUSBControllerV3::GatedPowerChange(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3 )
{
#pragma unused (arg1, arg2, arg3)
    IOUSBControllerV3			*me = (IOUSBControllerV3 *)owner;
	unsigned long				powerStateOrdinal = (unsigned long)arg0;
	unsigned long				oldState = me->_myPowerState;

	USBTrace_Start( kUSBTController, kTPControllerGatedPowerChange, (uintptr_t)me, powerStateOrdinal, oldState, 0 );
	
	switch (powerStateOrdinal)
	{
		case	kUSBPowerStateOff:
			me->ControllerOff();
			break;
			
		case	kUSBPowerStateRestart:
			me->ControllerRestart();
			break;
			
		case	kUSBPowerStateSleep:
			me->ControllerSleep();
			break;
			
		case	kUSBPowerStateLowPower:
			me->ControllerDoze();
			break;
			
		case	kUSBPowerStateOn:
			me->ControllerOn();
			break;
	}
	
	// This stuff used to be done in powerStateDidChangeTo. However, the timer activation and cancellation stuff really
	// needs to be done inside the workLoop to prevent race conditions between the actually running of those timers and 
	// rearming them. So now I do this stuff in here, where we are protected by the WL

	// I update _myPowerState here, because IOKit doesn't actually update the value returned by getPowerState() until the entire
	// power domain is in the new state. This means that I may have already made the power change, but until my children actually
	// get updated, getPowerState will appear to be in the old state, and I may need to know
	me->_myPowerState = powerStateOrdinal;
	if (powerStateOrdinal < kUSBPowerStateLowPower)
	{
		// first mark _controllerAvailable as false so that if the RootHubTimerFired method does get called, it doesn't actually do anything 
		me->_controllerAvailable = false;									// interrupts should have been disabled already
		me->RootHubStopTimer();
		me->EnableBusMastering(false);										// turn off bus mastering
	}
	else
	{
		// the controller should be ready to go, but interrupts are still disabled
		me->_controllerAvailable = true;
		
		// if we are coming FROM a lower state, then reenable interrupts (_myPowerState is the old state)
		if (oldState < kUSBPowerStateLowPower)
		{
			me->EnableInterruptsFromController(true);
			if (me->_rootHubPollingRate32)						// only once we have done this once
				me->RootHubStartTimer32(me->_rootHubPollingRate32);		// restart the timer on wakeup
		}
		
		if ( me->_rootHubDevice == NULL )
		{
			IOReturn err;
			
			if( (me->_expansionData) && (me->_controllerSpeed == kUSBDeviceSpeedSuper) )
			{
				USBLog(5, "IOUSBControllerV3(%s)[%p]::GatedPowerChange - calling CreateRootHubDevice for SS Controller", me->getName(), me);

				err = me->CreateRootHubDevice( me->_device, &(me->_rootHubDeviceSS) );
				USBLog(5,"IOUSBControllerV3(%s)[%p]::GatedPowerChange - done with CreateRootHubDevice for SS - return (%p)", me->getName(), me, (void*)err);
				if ( err != kIOReturnSuccess )
				{
					USBLog(1,"AppleUSBEHCI[%p]::GatedPowerChange - Could not create root hub device for SS upon wakeup (%x)!", me, err);
					USBTrace( kUSBTController, kTPControllerGatedPowerChange, (uintptr_t)me, err, 0, 0 );
				}
				else
				{
					me->_rootHubDeviceSS->registerService(kIOServiceRequired | kIOServiceSynchronous);
				}
			}
			
			USBLog(5, "IOUSBControllerV3(%s)[%p]::GatedPowerChange - calling CreateRootHubDevice", me->getName(), me);
			err = me->CreateRootHubDevice( me->_device, &(me->_rootHubDevice) );
			USBLog(5,"IOUSBControllerV3(%s)[%p]::GatedPowerChange - done with CreateRootHubDevice - return (%p)", me->getName(), me, (void*)err);
			if ( err != kIOReturnSuccess )
			{
				USBLog(1,"AppleUSBEHCI[%p]::GatedPowerChange - Could not create root hub device upon wakeup (%x)!", me, err);
				USBTrace( kUSBTController, kTPControllerGatedPowerChange, (uintptr_t)me, err, 0, 0 );
			}
			else
			{
				me->_rootHubDevice->registerService(kIOServiceRequired | kIOServiceSynchronous);
			}
		}
		if (me->_expansionData && me->_watchdogUSBTimer && !me->_watchdogTimerActive)
		{
			me->_watchdogTimerActive = true;
			me->_watchdogUSBTimer->setTimeoutMS(kUSBWatchdogTimeoutMS);
		}
	}

	// We only need to wake up when we get back to on
	if ( powerStateOrdinal == kUSBPowerStateOn && me->_inCheckPowerModeSleeping )
	{
		IOCommandGate * 	commandGate = me->GetCommandGate();
		if ( !commandGate )
		{
			USBLog(1,"IOUSBController::GatedPowerChange commandGate is NULL");
			USBTrace( kUSBTController, kTPControllerGatedPowerChange, (uintptr_t)me, 0, 0, 1 );
		}
		else
		{
			USBLog(5, "IOUSBControllerV3(%s)[%p]::GatedPowerChange - _inCheckPowerModeSleeping was true, calling commandWakeup", me->getName(), me);
			commandGate->commandWakeup(&me->_inCheckPowerModeSleeping,  false);
			USBTrace( kUSBTController, kTPControllerGatedPowerChange, (uintptr_t)me, 0, 0, 2 );
			
			me->_inCheckPowerModeSleeping = false;
		}

	}
	
	USBTrace_End( kUSBTController, kTPControllerGatedPowerChange, (uintptr_t)me, me->_myPowerState, 0, 0);
	
	return kIOReturnSuccess;
}


// 8051802 - Keep track of our external device count (behind the WL gate)
IOReturn
IOUSBControllerV3::ChangeExternalDeviceCount(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3 )
{
#pragma unused (arg1, arg2, arg3)
    IOUSBControllerV3			*me = (IOUSBControllerV3 *)owner;
	bool						addDevice = (bool)arg0;
	IOPMrootDomain				*myRootDomain = me->getPMRootDomain();

	if (myRootDomain && (kIOPMUndefinedDriverAssertionID != me->_externalUSBDeviceAssertionID))
	{
		if (addDevice)
		{
			if (me->_externalDeviceCount++ == 0)
			{
				// if we were 0 before we incremented, then we need to change our deep sleep assertion
				if (myRootDomain)
				{
					USBLog(3, "IOUSBControllerV3(%s)[%p]::ChangeExternalDeviceCount - got first external device, changing assertion to ON", me->getName(), me);
					myRootDomain->setPMAssertionLevel(me->_externalUSBDeviceAssertionID, kIOPMDriverAssertionLevelOn);
				}
			}
		}
		else
		{
			if (--(me->_externalDeviceCount) == 0)
			{
				// if we are 0 after decrementing then we need to change our deep sleep assertion
				if (myRootDomain)
				{
					USBLog(3, "IOUSBControllerV3(%s)[%p]::ChangeExternalDeviceCount - removed final external device, changing assertion to OFF", me->getName(), me);
					myRootDomain->setPMAssertionLevel(me->_externalUSBDeviceAssertionID, kIOPMDriverAssertionLevelOff);
				}
			}
		}

	}

#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
	if (me->_externalDeviceCount < 0)
	{
		USBLog(1, "IOUSBControllerV3(%s)[%p]::ChangeExternalDeviceCount - count fell below 0!", me->getName(), me);
	}
	me->setProperty("External Device Count", me->_externalDeviceCount, 32);
#endif
	return kIOReturnSuccess;
}



// HandlePowerChange - send the power state change through the gate
IOReturn				
IOUSBControllerV3::HandlePowerChange(unsigned long powerStateOrdinal)
{
	IOCommandGate * 	commandGate = GetCommandGate();
	
	USBLog(5, "IOUSBControllerV3(%s)[%p]::HandlePowerChange - sending the state change request (%d) through the gate - configCommand(%p)", getName(), this, (int)powerStateOrdinal, (void*)_device->configRead16(kIOPCIConfigCommand));
    return commandGate->runAction(GatedPowerChange, (void*)powerStateOrdinal);
}



IOReturn				
IOUSBControllerV3::EnableBusMastering(bool enable)
{
	UInt16		config = _device->configRead16(kIOPCIConfigCommand);
	
	if (enable)
	{
		USBLog(5, "IOUSBControllerV3(%s)[%p]::EnableBusMastering(true) - currently (%p) - enabling", getName(), this, (void*)config);
		config |= kIOPCICommandBusMaster;
	}
	else
	{
		USBLog(5, "IOUSBControllerV3(%s)[%p]::EnableBusMastering(false) - currently (%p) - disabling", getName(), this, (void*)config);
		config &= ~kIOPCICommandBusMaster;
	}
	
	_device->configWrite16(kIOPCIConfigCommand, config);
	
	USBLog(5, "IOUSBControllerV3(%s)[%p]::EnableBusMastering - new value[%p]", getName(), this, (void*)_device->configRead16(kIOPCIConfigCommand));
	return kIOReturnSuccess;
}



IOReturn				
IOUSBControllerV3::EnsureUsability(void)
{
	// as a general rule, the controller's power state is completely tied to the power state of the root hub (the only child)
	// so if the root hub is usable, then the controller will be also
	if (_rootHubDevice && _rootHubDevice->GetPolicyMaker())
	{
		USBLog(7, "IOUSBControllerV3(%s)[%p]::EnsureUsability - passing on to root hub policy maker[%p]", getName(), this, _rootHubDevice->GetPolicyMaker());
		_rootHubDevice->GetPolicyMaker()->EnsureUsability();
	}

	if (_rootHubDeviceSS && _rootHubDeviceSS->GetPolicyMaker())
	{
		USBLog(7, "IOUSBControllerV3(%s)[%p]::EnsureUsability - passing on to root hub policy maker[%p]", getName(), this, _rootHubDeviceSS->GetPolicyMaker());
		_rootHubDeviceSS->GetPolicyMaker()->EnsureUsability();
	}
	
	return kIOReturnSuccess;
}



// static method
void 
IOUSBControllerV3::RootHubTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
#pragma unused (sender)
    IOUSBControllerV3		*me;
    
    me = OSDynamicCast(IOUSBControllerV3, owner);
	
    if (!me || me->isInactive() || !me->_controllerAvailable)
        return;
	
	if (me->_rootHubDevice && me->_rootHubDevice->GetPolicyMaker())
	{
		USBLog(7, "IOUSBControllerV3(%s)[%p]::RootHubTimerFired - PolicyMaker[%p] powerState[%d] _powerStateChangingTo[%d]", me->getName(), me, me->_rootHubDevice->GetPolicyMaker(), (int)me->_rootHubDevice->GetPolicyMaker()->getPowerState(),(int)me->_powerStateChangingTo);
		if ((me->_powerStateChangingTo == kUSBPowerStateSleep) and (me->_rootHubDevice->GetPolicyMaker()->getPowerState() == kIOUSBHubPowerStateSleep))
		{
			USBLog(2, "IOUSBControllerV3(%s)[%p]::RootHubTimerFired - abandoning ship because I am going to sleep and my root hub is already asleep", me->getName(), me);
			return;
		}
	}
	else
	{
		USBLog(7, "IOUSBControllerV3(%s)[%p]::RootHubTimerFired", me->getName(), me);
	}
	
	if (me->_rootHubDeviceSS && me->_rootHubDeviceSS->GetPolicyMaker())
	{
		USBLog(7, "IOUSBControllerV3(%s)[%p]::RootHubTimerFired - PolicyMaker[%p] powerState[%d] _powerStateChangingTo[%d]", me->getName(), me, me->_rootHubDeviceSS->GetPolicyMaker(), (int)me->_rootHubDeviceSS->GetPolicyMaker()->getPowerState(),(int)me->_powerStateChangingTo);
		if ((me->_powerStateChangingTo == kUSBPowerStateSleep) and (me->_rootHubDeviceSS->GetPolicyMaker()->getPowerState() == kIOUSBHubPowerStateSleep))
		{
			USBLog(2, "IOUSBControllerV3(%s)[%p]::RootHubTimerFired - abandoning ship because I am going to sleep and my root hub is already asleep", me->getName(), me);
			return;
		}
	}
	else
	{
		USBLog(7, "IOUSBControllerV3(%s)[%p]::RootHubTimerFired", me->getName(), me);
	}
		
	USBTrace( kUSBTController, kTPControllerRootHubTimer, (uintptr_t)me, (uintptr_t)me->_rootHubDevice->GetPolicyMaker(), (uintptr_t)me->_rootHubDevice->GetPolicyMaker()->getPowerState(), 4 );
	me->CheckForRootHubChanges();
	
	// fire it up again
    if (me->_rootHubPollingRate32 && !me->isInactive() && me->_controllerAvailable)
		me->_rootHubTimer->setTimeoutMS(me->_rootHubPollingRate32);
}

void 
IOUSBControllerV3::RHCompleteTransaction(IOUSBRootHubInterruptTransactionPtr outstandingRHTransPtr)
{
	IOUSBRootHubInterruptTransaction		xaction;
	UInt8									bBitmap;
	UInt16									wBitmap;
	UInt8									bytesToMove;
	UInt8*									pBytes;
	int										i;

	if (outstandingRHTransPtr[0].completion.action)
		{
			// Save our corrent transaction and move all other transactions down the queue, and make sure the last one has a NULL completion
		xaction = outstandingRHTransPtr[0];
			for (i = 0; i < (kIOUSBMaxRootHubTransactions-1); i++)
			{
			outstandingRHTransPtr[i] = outstandingRHTransPtr[i+1];
			}
		outstandingRHTransPtr[kIOUSBMaxRootHubTransactions-1].completion.action = NULL;
			
			// Create the bitmap
			if (_rootHubNumPorts < 8)
			{
				bytesToMove = 1;
				bBitmap = (UInt8)_rootHubStatusChangedBitmap;
				pBytes = &bBitmap;
			}
			else
			{
				bytesToMove = 2;
				wBitmap = HostToUSBWord(_rootHubStatusChangedBitmap);
				pBytes = (UInt8*)&wBitmap;
			}
			if (xaction.completion.action && xaction.buf)
			{
				USBTrace( kUSBTController, kTPControllerRootHubTimer, (uintptr_t)this, 0, 0, 5 );
			USBLog(6, "IOUSBControllerV3(%s)[%p]::RHCompleteTransaction - stopping timer and calling complete", getName(), this);
				RootHubStopTimer();
				_rootHubTransactionWasAborted = false;
				
				xaction.buf->writeBytes(0, pBytes, bytesToMove);
				Complete(xaction.completion, kIOReturnSuccess, xaction.bufLen - bytesToMove);
			}
			else
			{
			USBError(1, "IOUSBControllerV3(%s)[%p]::RHCompleteTransaction - NULL action(%p) or buf(%p)", getName(), this, xaction.completion.action, xaction.buf);
		}
		
	}
	else
	{
		USBLog(5, "IOUSBControllerV3(%s)[%p]::RHCompleteTransaction - no one is listening - i will try again later", getName(), this);
	}
}

IOReturn
IOUSBControllerV3::CheckForRootHubChanges(void)
{
	// first call into the UIM to get the latest version of the bitmap
	UIMRootHubStatusChange();
	
	// now I only have to do anything if there is something in the status changed bitmap
	if (_rootHubStatusChangedBitmap)
	{
		USBLog(5, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - got _rootHubStatusChangedBitmap(%p) isInactive(%s)", getName(), this, (void*)_rootHubStatusChangedBitmap, isInactive() ? "true" : "false");
 		if (_rootHubDevice)
		{
			if (_rootHubDevice->GetPolicyMaker())
			{
				USBLog(5, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - making sure root hub driver AppleUSBHub[%p] is usable", getName(), this, _rootHubDevice->GetPolicyMaker());
				_rootHubDevice->GetPolicyMaker()->EnsureUsability();            // this will cause the interrupt read timer to fire if it isn't already
			}
			else
			{
				USBLog(1, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - _rootHubStatusChangedBitmap(%p) for _rootHubDevice with no policy maker!!", getName(), this, (void*)_rootHubStatusChangedBitmap);
				USBTrace( kUSBTController, kTPControllerCheckForRootHubChanges, (uintptr_t)this, _rootHubStatusChangedBitmap, 0, 0);
			}
		}
		
		if (_rootHubDeviceSS)
		{
			if (_rootHubDeviceSS->GetPolicyMaker())
			{
				USBLog(5, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - making sure root hub driver SS AppleUSBHub[%p] is usable", getName(), this, _rootHubDeviceSS->GetPolicyMaker());
				_rootHubDeviceSS->GetPolicyMaker()->EnsureUsability();          // this will cause the interrupt read timer to fire if it isn't already
			}
			else
			{
				USBLog(1, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - _rootHubStatusChangedBitmap(%p)for _rootHubDeviceSS with no  policy maker!!", getName(), this, (void*)_rootHubStatusChangedBitmap);
				USBTrace( kUSBTController, kTPControllerCheckForRootHubChanges, (uintptr_t)this, _rootHubStatusChangedBitmap, 0, 1);
			}
		}
		
		// if SS Controller then process SS RH xactions
		if( _controllerSpeed == kUSBDeviceSpeedSuper )
		{
			UInt16 end		= _rootHubPortsHSStartRange + _rootHubNumPortsHS - 1;
			UInt16 start	= _rootHubPortsHSStartRange;
			
			UInt16 HSPortMap	=	( _rootHubStatusChangedBitmap & USBBitRange(start,end) ) >>  USBBitRangePhase(start,end);
			HSPortMap			<<=	1;
			HSPortMap			|=	( _rootHubStatusChangedBitmap & 0x1 );
			
			end		= _rootHubPortsSSStartRange + _rootHubNumPortsSS - 1;
			start	= _rootHubPortsSSStartRange;
			
			UInt16 SSPortMap	= ( _rootHubStatusChangedBitmap & USBBitRange(start,end) ) >>  USBBitRangePhase(start,end);
			SSPortMap			<<=	1;
			SSPortMap			|=	( _rootHubStatusChangedBitmap & 0x1 );
			
			USBLog(6, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - _rootHubStatusChangedBitmap(%p) HSPortMap(%p) SSPortMap(%p)", getName(), this, (void*)_rootHubStatusChangedBitmap, (void*)HSPortMap, (void*)SSPortMap);
			
			if( HSPortMap > 0 )
			{
				_rootHubStatusChangedBitmap = HSPortMap;
				
				RHCompleteTransaction(_outstandingRHTrans);
			}
			
			if( SSPortMap > 0 )
			{
				_rootHubStatusChangedBitmap = SSPortMap;
				
				RHCompleteTransaction(_outstandingSSRHTrans);
			}
		}
		else 
		{
			RHCompleteTransaction(_outstandingRHTrans);
		}
	}
	
	return kIOReturnSuccess;
}

IOReturn
IOUSBControllerV3::RHQueueTransaction(IOMemoryDescriptor *buf, UInt32 bufLen, IOUSBCompletion completion, IOUSBRootHubInterruptTransactionPtr outstandingRHXaction)
{
    int				i;

	USBLog(6, "IOUSBControllerV3(%s)[%p]::RHQueueTransaction controllerSpeed: %d", getName(), this, _controllerSpeed);
	
	
    for (i = 0; i < kIOUSBMaxRootHubTransactions; i++)
    {
        if (outstandingRHXaction[i].completion.action == NULL)
        {
            // found free trans
            outstandingRHXaction[i].buf = buf;
            outstandingRHXaction[i].bufLen = bufLen;
            outstandingRHXaction[i].completion = completion;

			if (i != 0)
			{
				USBLog(1, "IOUSBControllerV3(%s)[%p]::RHQueueTransaction - this is index(%d) - UNEXPECTED?", getName(), this, (int)i);
				USBTrace( kUSBTController, kTPControllerRootHubQueueInterruptRead, (uintptr_t)this, i, 0, 0 );
				break;
			}
			
			// If this is the first transaction after an abort interrupt read, complete it immediately
			if ( _rootHubTransactionWasAborted )
			{
				USBLog(6, "IOUSBControllerV3(%s)[%p]::RHQueueTransaction  _rootHubTransactionWasAborted was true, calling CheckForRootHubChanges()", getName(), this);
				CheckForRootHubChanges();
			}
			
            return kIOReturnSuccess;
        }
		else
		{
			USBLog(1, "IOUSBControllerV3(%s)[%p]::RHQueueTransaction  we already had a completion.action for trans: %d, returning an error", getName(), this, i);
			break;
		}
    }

	return kIOReturnNotPermitted;
}

/*
 * RootHubQueueInterruptRead
 * Queue up a read on the Root Hub Interrupt Pipe - Unless there is an error and it completes right away,
 * this will end up completing when the Root Hub timer fires.
*/
IOReturn
IOUSBControllerV3::RootHubQueueInterruptRead(IOMemoryDescriptor *buf, UInt32 bufLen, IOUSBCompletion completion)
{
	IOReturn		status	= kIOReturnNotPermitted;
	
	USBTrace_Start( kUSBTController, kTPControllerRootHubQueueInterruptRead, (uintptr_t)this, (uintptr_t)buf, bufLen, 0);
	
	USBLog(6, "IOUSBControllerV3(%s)[%p]::RootHubQueueInterruptRead, starting timer", getName(), this);
	
	// Start the RootHub timer
	RootHubStartTimer32(kUSBRootHubPollingRate);

	IOUSBRootHubInterruptTransactionPtr outstandingRHXaction = _outstandingRHTrans;
		
	if (_controllerSpeed == kUSBDeviceSpeedSuper) 
	{
		UInt8			speed = 0;
		UInt16			address = 0;
		IOOptionBits 	options = buf->getTag();
		
		speed		= ((options & kUSBSpeed_Mask) >> kUSBSpeed_Shift);
		address		= ((options & kUSBAddress_Mask) >> kUSBAddress_Shift);
		
		if (speed == kUSBDeviceSpeedSuper)
		{
			outstandingRHXaction = _outstandingSSRHTrans;
		}
	}
	
	status = RHQueueTransaction(buf, bufLen, completion, outstandingRHXaction);
	

	USBTrace_End( kUSBTController, kTPControllerRootHubQueueInterruptRead, (uintptr_t)this, status, bufLen, 0);
	return status;
}

void 
IOUSBControllerV3::RHAbortTransaction(IOUSBRootHubInterruptTransactionPtr outstandingRHXaction)
{
    int										i;
	IOUSBRootHubInterruptTransaction		xaction;

	xaction = outstandingRHXaction[0];
	if (xaction.completion.action)
	{
		// move all other transactions down the queue
		for (i = 0; i < (kIOUSBMaxRootHubTransactions-1); i++)
		{
			outstandingRHXaction[i] = outstandingRHXaction[i+1];
		}
		outstandingRHXaction[kIOUSBMaxRootHubTransactions-1].completion.action = NULL;
		Complete(xaction.completion, kIOReturnAborted, xaction.bufLen);
		
		_rootHubTransactionWasAborted = true;
	}
}

/*
 * RootHubAbortInterruptRead
 * Abort a queued up read. This should only be called in the workloop gate, so it should be synchronized
*/
IOReturn
IOUSBControllerV3::RootHubAbortInterruptRead()
{
	
	USBTrace( kUSBTController, kTPControllerRootHubTimer, (uintptr_t)this, 0, 0, 3 );
	USBLog(6, "IOUSBControllerV3(%s)[%p]::RootHubAbortInterruptRead, stopping timer", getName(), this);
	
	RootHubStopTimer();
	
	// if SS Controller then process SS RH xactions
	if( _controllerSpeed == kUSBDeviceSpeedSuper )
	{
		RHAbortTransaction(_outstandingSSRHTrans);
	}
	
	RHAbortTransaction(_outstandingRHTrans);
	
	return kIOReturnSuccess;
}



IOReturn				
IOUSBControllerV3::RootHubStartTimer(UInt8 pollingRate)
{
#pragma unused (pollingRate)
	
	USBLog(1,"IOUSBControllerV3(%s)[[%p]::RootHubStartTimer  Obsolete method called", getName(), this);
	return kIOReturnUnsupported;
	
}

IOReturn				
IOUSBControllerV3::RootHubStartTimer32(uint32_t pollingRate)
{
	USBTrace( kUSBTController, kTPControllerRootHubTimer, (uintptr_t)this, pollingRate, 0, 1 );

	if (pollingRate == 0)
	{
		USBLog(1, "IOUSBControllerV3(%s)[%p]::RootHubStartTimer32 - invalid polling rate (%d)", getName(), this, pollingRate);
		USBTrace( kUSBTController, kTPControllerRootHubTimer, (uintptr_t)this, pollingRate, kIOReturnBadArgument, 6 );
		return kIOReturnBadArgument;
	}
	_rootHubPollingRate32 = pollingRate;
	
	if (_rootHubTimer)
	{
		USBLog(6, "IOUSBControllerV3(%s)[%p]::RootHubStartTimer32", getName(), this);
		
		_rootHubTimer->setTimeoutMS(_rootHubPollingRate32);
	}
	else
	{
		USBLog(1, "IOUSBControllerV3(%s)[%p]::RootHubStartTimer32 - NO TIMER!!", getName(), this);
		USBTrace( kUSBTController, kTPControllerRootHubTimer, (uintptr_t)this, 0, kIOReturnSuccess, 7 );
	}
	
	return kIOReturnSuccess;
}



IOReturn				
IOUSBControllerV3::RootHubStopTimer(void)
{
	
	USBTrace( kUSBTController, kTPControllerRootHubTimer, (uintptr_t)this, 0, 0, 2 );
	USBLog(6, "IOUSBControllerV3(%s)[%p]::RootHubStopTimer", getName(), this);
	
	// first of all, make sure that we are not currently inside of the RootHubTimerFired method before we cancel the timeout
	// note that if we are not in it now, we won't get into it because at this point _controllerAvailable is false
	// cancel the timer, which may or may not actually do anything
    if (_rootHubTimer)
    {
		_rootHubTimer->cancelTimeout();
	}
	
	return kIOReturnSuccess;
}



// DEPRECATED API
UInt32
IOUSBControllerV3::AllocateExtraRootHubPortPower(UInt32 extraPowerRequested)
{
#pragma unused (extraPowerRequested)
	USBLog(1, "IOUSBControllerV3(%s)[%p]::AllocateExtraRootHubPortPower - DEPRECATED API called", getName(), this);
	return 0;
}


// DEPRECATED API
void
IOUSBControllerV3::ReturnExtraRootHubPortPower(UInt32 extraPowerReturned)
{
#pragma unused (extraPowerReturned)
	USBLog(1, "IOUSBControllerV3(%s)[%p]::ReturnExtraRootHubPortPower - DEPRECATED API called", getName(), this);
	return;
}



// 8051802 - check for PM assertions based on devices being added
IOReturn
IOUSBControllerV3::CheckPMAssertions(IOUSBDevice *forDevice, bool deviceBeingAdded)
{
	UInt32			devInfo;
	IOCommandGate	* commandGate = GetCommandGate();
	
	IOReturn		ret = kIOReturnInternalError;			// default in case we don't actually have an assertion ID
	
	ret = forDevice->GetDeviceInformation(&devInfo);
	if (!ret)
	{
		if ((devInfo & kUSBInformationDeviceIsInternalMask) == 0)
		{
			// we have an external device
			ret = commandGate->runAction(ChangeExternalDeviceCount, (void*)deviceBeingAdded);
		}
	}
	
	
	return ret;
}


IOReturn
IOUSBControllerV3::GetRootHubBOSDescriptor(OSData *data)
{
#pragma unused (data)
	return kIOReturnUnsupported;
}


IOReturn
IOUSBControllerV3::GetRootHub3Descriptor(IOUSB3HubDescriptor *desc)
{
#pragma unused (desc)
	return kIOReturnUnsupported;
}

IOReturn
IOUSBControllerV3::GetRootHubPortErrorCount(UInt16 port, UInt16 *count)
{
#pragma unused (port, count)
	return kIOReturnUnsupported;
}

// Do not implement this if the UIM is happy with the way resets are currently handled

IOReturn
IOUSBControllerV3::UIMDeviceToBeReset(short functionAddress)
{
#pragma unused (functionAddress)
	return kIOReturnUnsupported;
}

// Only implement this if UIM supports streams (ie XHCI).
IOReturn
IOUSBControllerV3::UIMAbortStream(UInt32		streamID,
										 short		functionNumber,
										 short		endpointNumber,
										 short		direction)
{
	if(streamID != kUSBAllStreams)
	{
		USBError(1, "IOUSBControllerV3(%s)[%p]::UIMAbortEndpoint - Stream version called for UIM which doesn't support it", getName(), this);
		return kIOReturnUnsupported;
	}
	return UIMAbortEndpoint(functionNumber, endpointNumber, direction);
}

// Only impliment this if UIM supports streams (ie XHCI).
UInt32
IOUSBControllerV3::UIMMaxSupportedStream(void)
{
	return(0);
}


IOReturn
IOUSBControllerV3::GetActualDeviceAddress(USBDeviceAddress currentAddress, USBDeviceAddress *newAddress)
{
	IOCommandGate * 	commandGate = GetCommandGate();
	
    return commandGate->runAction(DoGetActualDeviceAddress, (void*)(uintptr_t)currentAddress, (void*)newAddress);
}

IOReturn
IOUSBControllerV3::DoGetActualDeviceAddress(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3 )
{
#pragma unused (arg2, arg3)
    IOUSBControllerV3 *	me = (IOUSBControllerV3 *)owner;
	IOReturn			kr = kIOReturnSuccess;
    USBDeviceAddress *	uimCurrentAddress = (USBDeviceAddress *)arg1;
	
    *uimCurrentAddress = me->UIMGetActualDeviceAddress((USBDeviceAddress)(uintptr_t)arg0);
	if ( *uimCurrentAddress == 0 )
	{
		kr = kIOReturnNotFound;
	}
	
	USBLog(6, "IOUSBControllerV3(%s)[%p]::DoGetActualDeviceAddress - uimCurrentAddress: %d, original: %d, returned 0x%x", me->getName(), me, *uimCurrentAddress, (USBDeviceAddress)(uintptr_t)arg0, (uint32_t)kr);
	
	return kr;
}

IOReturn
IOUSBControllerV3::CreateStreams(UInt8 functionNumber, UInt8 endpointNumber, UInt8 direction,  UInt32 maxStream)
{
	IOCommandGate * 	commandGate = GetCommandGate();
	
    return commandGate->runAction(DoCreateStreams, (void*)(uintptr_t)functionNumber, (void*)(uintptr_t)endpointNumber, (void*)(uintptr_t)direction, (void*)(uintptr_t)maxStream);
}

IOReturn
IOUSBControllerV3::DoCreateStreams(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3 )
{
    IOUSBControllerV3 *	me = (IOUSBControllerV3 *)owner;
	UInt8				functionNumber = (UInt8)(uintptr_t)arg0;
	UInt8				endpointNumber = (UInt8)(uintptr_t)arg1;
	UInt8				direction = (UInt8)(uintptr_t)arg2;
	UInt32              maxStream = (UInt32)(uintptr_t)arg3;
	
	USBLog(6, "IOUSBControllerV3(%s)[%p]::DoCreateStreams -  functionNumber: %d, endpointNumber: %d, direction: %d, maxStream: %d", me->getName(), me, functionNumber, endpointNumber, direction, (uint32_t)maxStream);

	return me->UIMCreateStreams(functionNumber, endpointNumber, direction, maxStream);
}

#pragma mark ¥¥¥¥¥ IOUSBController methods ¥¥¥¥¥
//
// These methods are implemented in IOUSBController, and they all call runAction to synchronize them
// We used to close the workLoop gate when we are sleeping, and we needed to stop doing that. So we run
// these methods through here now to check the power state and hold the thread until we are awake
//
IOReturn
IOUSBControllerV3::AcquireDeviceZero( void )
{
	IOReturn	kr;
	
	// we don't call changePowerStateTo here, because this call should only be made be the hub driver, which better be on
	// before it makes this call, meaning that our state is our childDesire
	
	kr = CheckPowerModeBeforeGatedCall( (char *) "AcquireDeviceZero");
	if ( kr != kIOReturnSuccess )
	{
		USBError(1, "IOUSBControllerV3(%s)[%p]::AcquireDeviceZero - CheckPowerModeBeforeGatedCall returned 0x%x", getName(), this, kr);
		return kr;
	}

	return IOUSBController::AcquireDeviceZero();
}



void
IOUSBControllerV3::ReleaseDeviceZero( void )
{
	IOReturn	kr;
	
	// we don't call changePowerStateTo here, because this call should only be made be the hub driver, which better be on
	// before it makes this call, meaning that our state is our childDesire
	
	kr = CheckPowerModeBeforeGatedCall((char *) "ReleaseDeviceZero");
	if ( kr != kIOReturnSuccess )
		return;

	return IOUSBController::ReleaseDeviceZero();
}



IOReturn
IOUSBControllerV3::DeviceRequest(IOUSBDevRequest *request,  IOUSBCompletion *completion, USBDeviceAddress address, UInt8 epNum, UInt32 noDataTimeout, UInt32 completionTimeout)
{
	IOReturn	kr;
	
	if ((_powerStateChangingTo == kUSBPowerStateStable) || (_powerStateChangingTo > kUSBPowerStateSleep))
	{
		// use changePowerStateTo (as opposed to changePowerStateToPriv) to maintain the ON state 
		// on behalf of some client from user space. If the hub is already active, then this is essentially a NOP
		USBLog(7, "IOUSBControllerV3(%s)[%p]::DeviceRequest - current state (%d) _powerStateChangingTo(%d) - calling changePowerStateTo(4)", getName(), this, (int)_myPowerState, (int)_powerStateChangingTo);
		changePowerStateTo(kUSBPowerStateOn);
	}

	kr = CheckPowerModeBeforeGatedCall((char *) "DeviceRequest");
	if ( kr != kIOReturnSuccess )
		return kr;

	kr = IOUSBController::DeviceRequest(request, completion, address, epNum, noDataTimeout, completionTimeout);
	
	if ((_powerStateChangingTo == kUSBPowerStateStable) || (_powerStateChangingTo > kUSBPowerStateSleep))
	{
		// now go back to the LowPower state if our child is still in LowPower state
		USBLog(7, "IOUSBControllerV3(%s)[%p]::DeviceRequest done - current state (%d) - calling changePowerStateTo(3)", getName(), this, (int)_myPowerState);
		changePowerStateTo(kUSBPowerStateLowPower);
	}
	return kr;
}



IOReturn
IOUSBControllerV3::DeviceRequest(IOUSBDevRequestDesc *request,  IOUSBCompletion *completion, USBDeviceAddress address, UInt8 epNum, UInt32 noDataTimeout, UInt32 completionTimeout)
{
	IOReturn	kr;
	
	// use changePowerStateTo (as opposed to changePowerStateToPriv) to maintain the ON state 
	// on behalf of some client from user space. If the hub is already active, then this is essentially a NOP
	// we should probably run this through a gate with an incrementer/decrementer
	USBLog(7, "IOUSBControllerV3(%s)[%p]::DeviceRequest - current state (%d) _powerStateChangingTo(%d) - calling changePowerStateTo(4)", getName(), this, (int)_myPowerState, (int)_powerStateChangingTo);
	changePowerStateTo(kUSBPowerStateOn);

	kr = CheckPowerModeBeforeGatedCall((char *) "DeviceRequest");
	if ( kr != kIOReturnSuccess )
		return kr;
	
	kr = IOUSBController::DeviceRequest(request, completion, address, epNum, noDataTimeout, completionTimeout);
	
	// now go back to the LowPower state if our child is still in LowPower state
	USBLog(7, "IOUSBControllerV3(%s)[%p]::DeviceRequest done - current state (%d) - calling changePowerStateTo(3)", getName(), this, (int)_myPowerState);
	changePowerStateTo(kUSBPowerStateLowPower);
	
	return kr;
}



IOReturn
IOUSBControllerV3::ClosePipe(USBDeviceAddress address, Endpoint *endpoint)
{
	IOReturn	kr;
	
	// if we are in the middle of termination, then we can go ahead and allow the call, since we won't be touching any hardware regs
	if (!isInactive())
	{
		// we can  close the pipe without being fully on, as long as we know what state we are in
		if (!(_controllerAvailable || _wakingFromHibernation || _restarting || _poweringDown))
		{
			kr = CheckPowerModeBeforeGatedCall((char *) "ClosePipe");
			if ( kr != kIOReturnSuccess )
				return kr;
		}
	}
	return IOUSBController::ClosePipe(address, endpoint);
}



IOReturn
IOUSBControllerV3::AbortPipe(USBDeviceAddress address, Endpoint *endpoint)
{
	USBLog(7, "IOUSBControllerV3(%s)[%p]::AbortPipe - Deprecated version called (no stream ID)", getName(), this);
	return AbortPipe(0, address, endpoint);
}

IOReturn
IOUSBControllerV3::AbortPipe(UInt32 streamID, USBDeviceAddress address, Endpoint *endpoint)
{
		IOReturn	kr;
	
	// if we are in the middle of termination, then we can go ahead and allow the call, since we won't be touching any hardware regs
	if (!isInactive())
	{
	// we can  abort the pipe without being fully on, as long as we know what state we are in
		if (!(_controllerAvailable || _wakingFromHibernation || _restarting || _poweringDown))
		{
			kr = CheckPowerModeBeforeGatedCall((char *) "AbortPipe");
			if ( kr != kIOReturnSuccess )
				return kr;
		}
	}

	return _commandGate->runAction(DoAbortStream, (void *)streamID, (void *)(UInt32) address,
								   (void *)(UInt32) endpoint->number, (void *)(UInt32) endpoint->direction);

}

IOReturn 
IOUSBControllerV3::DoAbortStream(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOUSBControllerV3 *me = (IOUSBControllerV3 *)owner;
	
	return me->UIMAbortStream((UInt32)(uintptr_t)arg0, (short)(uintptr_t) arg1, (short)(uintptr_t) arg2, (short)(uintptr_t) arg3);
}


IOReturn
IOUSBControllerV3::ResetPipe(USBDeviceAddress address, Endpoint *endpoint)
{
	IOReturn	kr;
	
	// if we are in the middle of termination, then we can go ahead and allow the call, since we won't be touching any hardware regs
	if (!isInactive())
	{
		// we can  reset the pipe without being fully on, as long as we know what state we are in
		if (!(_controllerAvailable || _wakingFromHibernation || _restarting || _poweringDown))
		{
			kr = CheckPowerModeBeforeGatedCall((char *) "ResetPipe");
			if ( kr != kIOReturnSuccess )
				return kr;
		}
	}	
	return IOUSBController::ResetPipe(address, endpoint);
}



IOReturn
IOUSBControllerV3::ClearPipeStall(USBDeviceAddress address, Endpoint *endpoint)
{
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "ClearPipeStall");
	if ( kr != kIOReturnSuccess )
		return kr;

	return IOUSBController::ClearPipeStall(address, endpoint);
}



IOReturn
IOUSBControllerV3::Read(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
    return Read(0, buffer, address, endpoint, completion, noDataTimeout, completionTimeout, reqCount);
}

IOReturn
IOUSBControllerV3::Read(UInt32 streamID, IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "Read");
	if ( kr != kIOReturnSuccess )
		return kr;
	
	return IOUSBControllerV2::ReadStream(streamID, buffer, address, endpoint, completion, noDataTimeout, completionTimeout, reqCount);
}

IOReturn
IOUSBControllerV3::Write(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
	USBLog(7, "IOUSBControllerV3(%s)[%p]::Write deprecated method called (no stream ID)", getName(), this);
	return Write(0, buffer, address, endpoint, completion, noDataTimeout, completionTimeout, reqCount);
}

IOReturn
IOUSBControllerV3::Write(UInt32 streamID, IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "Write");
	if ( kr != kIOReturnSuccess )
		return kr;

	return IOUSBControllerV2::WriteStream(streamID, buffer, address, endpoint, completion, noDataTimeout, completionTimeout, reqCount);
}


IOReturn
IOUSBControllerV3::IsocIO(IOMemoryDescriptor *buffer, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList, USBDeviceAddress address, Endpoint *endpoint, IOUSBIsocCompletion *completion )
{
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "IsocIO");
	if ( kr != kIOReturnSuccess )
		return kr;

	return IOUSBController::IsocIO(buffer, frameStart, numFrames, frameList, address, endpoint, completion);
}



IOReturn
IOUSBControllerV3::IsocIO(IOMemoryDescriptor *buffer, UInt64 frameStart, UInt32 numFrames, IOUSBLowLatencyIsocFrame *frameList, USBDeviceAddress address, Endpoint *endpoint, IOUSBLowLatencyIsocCompletion *completion, UInt32 updateFrequency )
{
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "IsocIO");
	if ( kr != kIOReturnSuccess )
		return kr;

	return IOUSBController::IsocIO(buffer, frameStart, numFrames, frameList, address, endpoint, completion, updateFrequency);
}



#pragma mark ¥¥¥¥¥ IOUSBControllerV2 methods ¥¥¥¥¥
//
// These methods are implemented in IOUSBController, and they all call runAction to synchronize them
// We used to close the workLoop gate when we are sleeping, and we needed to stop doing that. So we run
// these methods through here now to check the power state and hold the thread until we are awake
//
IOReturn
IOUSBControllerV3::OpenPipe(USBDeviceAddress address, UInt8 speed, Endpoint *endpoint)
{
	return OpenPipe(address, speed, endpoint, 0, 0);
}

IOReturn
IOUSBControllerV3::OpenPipe(USBDeviceAddress address, UInt8 speed, Endpoint *endpoint, UInt32 maxStreams, UInt32 maxBurst)
{	
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "OpenPipe");
	if ( kr != kIOReturnSuccess )
		return kr;

	if( (maxStreams == 0) && (maxBurst == 0) )
	{
		return IOUSBControllerV2::OpenPipe(address, speed, endpoint);
	}
	else
	{
		return IOUSBControllerV2::OpenSSPipe(address, speed, endpoint, maxStreams, maxBurst);
	}

}


IOReturn
IOUSBControllerV3::AddHSHub(USBDeviceAddress highSpeedHub, UInt32 flags)
{
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "AddHSHub");
	if ( kr != kIOReturnSuccess )
		return kr;

	return IOUSBControllerV2::AddHSHub(highSpeedHub, flags);
}



IOReturn
IOUSBControllerV3::RemoveHSHub(USBDeviceAddress highSpeedHub)
{
	IOReturn	kr;
	
	// if we are in the middle of termination, then we can go ahead and allow the call, since we won't be touching any hardware regs
	if (!isInactive())
	{
		// we can  remove a hub without being fully on, as long as we know what state we are in
		if (!(_controllerAvailable || _wakingFromHibernation || _restarting || _poweringDown))
		{
			kr = CheckPowerModeBeforeGatedCall((char *) "RemoveHSHub");
			if ( kr != kIOReturnSuccess )
				return kr;
		}
	}	
	return IOUSBControllerV2::RemoveHSHub(highSpeedHub);
}



IOReturn
IOUSBControllerV3::SetTestMode(UInt32 mode, UInt32 port)
{
	IOReturn	kr;
	
	changePowerStateTo(kUSBPowerStateOn);							// this will leave it ON indefinitely, which is OK for Test Mode
	kr = CheckPowerModeBeforeGatedCall((char *) "SetTestMode");
	if ( kr != kIOReturnSuccess )
		return kr;

	return IOUSBControllerV2::SetTestMode(mode, port);
}

IOReturn
IOUSBControllerV3::ReadV2(IOMemoryDescriptor *buffer, USBDeviceAddress	address, Endpoint *endpoint, IOUSBCompletionWithTimeStamp *completion, UInt32 noDataTimeout, UInt32	completionTimeout, IOByteCount reqCount)
{
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "ReadV2");
	if ( kr != kIOReturnSuccess )
		return kr;
    
	return IOUSBControllerV2::ReadV2(buffer, address, endpoint, completion, noDataTimeout, completionTimeout, reqCount);
}



UInt32			
IOUSBControllerV3::GetErrataBits(UInt16 vendorID, UInt16 deviceID, UInt16 revisionID )
{
	UInt32	errataBits = IOUSBController::GetErrataBits(vendorID, deviceID, revisionID);
	
	OSBoolean *				tunnelledProp = NULL;
	OSData *				thunderboltModelIDProp = NULL;
    OSData *                thunderboltVendorIDProp = NULL;
    IOService *				parent = _device;

	if (_device)
	{
		// We should figure out how to differentiate between the PCIe slot on a Dr. B and the 
		// motherboards chips on the same platform, but for now they will all fail
		tunnelledProp = OSDynamicCast(OSBoolean, _device->getProperty(kIOPCITunnelledKey));
		if (tunnelledProp && tunnelledProp->isTrue())
		{
			int		retries = 10;
			
			_onThunderbolt = true;
			
            // 10186556 - when running tunneled, go ahead and set rMBS for the lifetime of this controller
            requireMaxBusStall(25000);
			while (!isInactive() && retries--)
            {
                while  (parent != NULL)
                {
                    OSObject            *thunderboltModelIDObj;
                    OSObject            *thunderboltVendorIDObj;
                    IORegistryEntry     *nextParent;
                    
                    thunderboltModelIDObj = parent->copyProperty(kIOThunderboltTunnelEndpointDeviceMIDProp);
                    thunderboltVendorIDObj = parent->copyProperty(kIOThunderboltTunnelEndpointDeviceVIDProp);
                    
                    thunderboltModelIDProp = OSDynamicCast(OSData, thunderboltModelIDObj);
                    thunderboltVendorIDProp = OSDynamicCast(OSData, thunderboltVendorIDObj);
                    
                    if (thunderboltModelIDProp && thunderboltVendorIDProp)
                    {
                        // we found what we were looking for. We will break out of this while loop and will release the two objects
                        // after we discover what is in them (although we release them under their new casted types)
                        // first release the registry entry that we found them in, unless it is us (which it shouldn't be)
                        if (parent != _device)
                            parent->release();
                        break;
                    }
                    
                    // these releases would be for the cases where we found one object but not the other, or if one or 
                    // both of them existed but was not the correct type (OSData)
                    if (thunderboltModelIDObj)
                    {
                        thunderboltModelIDProp = NULL;
                        thunderboltModelIDObj->release();
                    }
                    
                    if (thunderboltVendorIDObj)
                    {
                        thunderboltVendorIDProp = NULL;
                        thunderboltVendorIDObj->release();
                    }
                    
                    // get the next registry entry up the tree. we use copyParent so that we have a reference, in case
                    // an entry goes away while we are looking - which could happen since TBolt can be unplugged
					nextParent = parent->copyParentEntry(gIOServicePlane);
                    
                    // we are done with the old registry entry at this point
                    if (parent != _device)
                        parent->release();
                    
                    // copy the new parent to the iVar (making sure it is the correct type, which it better be
                    parent = OSDynamicCast(IOService, nextParent);
                    if ((parent == NULL) && (nextParent != NULL))
                    {
                        // this would be exceedingly strange
                        nextParent->release();
                    }
                }
                
                // if they both were found, then we break out of the retry loop as well (we still own a reference to each of them)
                if (thunderboltModelIDProp && thunderboltVendorIDProp)
                    break;
                
                // we didn't find a node with both.. sleep for 1 second and try again. this happens often with the TBolt display
                // not that parent will be NULL at this point so we don't need to release it
                IOSleep(1000);                                                          // wait up to 10 seconds for this to occur
                parent = _device;                                                       // start over
            }
            
            if (!isInactive())
            {
                if (thunderboltModelIDProp && thunderboltVendorIDProp)
                {
                    _thunderboltModelID = *(UInt32*)thunderboltModelIDProp->getBytesNoCopy();
                    _thunderboltVendorID = *(UInt32*)thunderboltVendorIDProp->getBytesNoCopy();
                }
                else
                {
                    // if we didn't get the model ID, then assume it is the 2011 display
                    _thunderboltModelID = kAppleThunderboltDisplay2011MID;
                    _thunderboltVendorID = kAppleThunderboltVID;
                }

                // save the model ID in case someone else (i.e. Audio) wants to use it
                setProperty(kIOThunderboltTunnelEndpointDeviceMIDProp, _thunderboltModelID, 32);
                setProperty(kIOThunderboltTunnelEndpointDeviceVIDProp, _thunderboltVendorID, 32);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                setProperty("Tbolt Model ID retries", retries, 32);
#endif
            }
            
            // we need to do these two releases whether we are inactive or not
            if (thunderboltModelIDProp)
                thunderboltModelIDProp->release();
            
            if (thunderboltVendorIDProp)
                thunderboltVendorIDProp->release();
		}
		
		if (_onThunderbolt && (_thunderboltModelID == kAppleThunderboltDisplay2011MID) && (_thunderboltVendorID == kAppleThunderboltVID))
		{
			if (!(gUSBStackDebugFlags & kUSBForceCompanionControllersMask))
			{
				USBLog(5,"IOUSBControllerV3[%p]::GetErrataBits  found PCI-Thunderbolt property, adding the errata", this);
				errataBits |= kErrataDontUseCompanionController;
			}
			
			// mark the root hub port as having port 1 captive
			if (_device->getProperty(kAppleInternalUSBDevice) == NULL)
				_device->setProperty(kAppleInternalUSBDevice, (unsigned long long)2, 8);

			// make sure we are assumed to be a built in controller
			if (_device->getProperty("built-in") == NULL)
				_device->setProperty("built-in", (unsigned long long)0, 8);
			
			if (errataBits & kErrataDisablePCIeLinkOnSleep)
			{
				OSObject *				aProperty = NULL;
				IOService *				parent = _device;
				IOPCIDevice *			highestPCIDevice = NULL;
				IOPCIDevice *			tempPCIDevice = NULL;
                IORegistryEntry *       nextParent = NULL;
				
				while (  parent != NULL )
				{
					// We need to chase up the tree looking for the root complex, which will have the PCI-Thunderbolt node in it
					aProperty = parent->copyProperty("PCI-Thunderbolt");
					
					// if we found it and we have recorded a highest PCI bridge between us and there, then add the disable property
					if ( OSDynamicCast( OSNumber, aProperty) != NULL )
					{						
						// if we have an IOPCIDevice in our upstream which is the same vendorID as the controller, then we mark it
						if (highestPCIDevice)
                        {
							highestPCIDevice->setProperty(kIOPMPCISleepResetKey, kOSBooleanTrue);
                            highestPCIDevice->release();
                            highestPCIDevice = NULL;
                        }
						
                        // release the property since we are breaking out of the loop
                        aProperty->release();
                        
                        // release the parent iVar as long as it is not my actual parent (which does not have an extra retain at this point)
                        if (parent != _device)
                            parent->release();
                        
						break;
					}
					
                    if (aProperty)
                    {
                        // exceedingly rare - would only happen if the property existed but was not an OSNumber
                        aProperty->release();
                        aProperty = NULL;
                    }
                    
					// while going up the tree, look for any IOPCIDevice (really a PCIe bridge) with the same
					// vendorID as the USB Host Controller. As we find them, remember the highest one, which is the
					// Pericom upstream bridge
					nextParent = parent->copyParentEntry(gIOServicePlane);
                    
                    // now that I have the next one, I can release the old (if it isn't me)
                    if (parent != _device)
                        parent->release();
                    
                    parent = OSDynamicCast(IOService, nextParent);
                    
                    if ((parent == NULL) && (nextParent != NULL))
                    {
                        // this would be exceedingly strange
                        nextParent->release();
                    }
                    
					tempPCIDevice = OSDynamicCast(IOPCIDevice, parent);
                    
                    // it would not be unusual for the following check to fail
					if (tempPCIDevice)
					{
                        OSObject    *vendObj;
						OSData		*vendProp;
						UInt32		tempVend;
						
						// get this chips vendID, deviceID, revisionID
						vendObj     = tempPCIDevice->copyProperty( "vendor-id" );
						vendProp     = OSDynamicCast(OSData, vendObj);
						if (vendProp)
						{
							tempVend = *((UInt32 *) vendProp->getBytesNoCopy());
							if (tempVend == vendorID)
                            {
                                if (highestPCIDevice)
                                {
                                    // this is normal.. we will usually find 2 before we get where we are going
                                    highestPCIDevice->release();
                                }
								highestPCIDevice = tempPCIDevice;
                                highestPCIDevice->retain();
                            }
						}
                        if (vendObj)
                            vendObj->release();
					}
				}
			}
		}
	}
	return errataBits;
}


//	this method fixes up some config space registers in an NEC uPD720101 controller (both EHCI and OHCI)
//	specifically it enables or re-enables the PME generation from D3 cold register

enum  
{
	kNECuPD720101EXT1		= 0xE0,				// 32 bit register called EXT1 in the user manual
	kNECuPD720101EXT1ID_WE	= 0x80,				// write enable bit in the EXT1 register
	kNECuPD720101PMC		= 0x42				// standard power manager capabilities register (16 bit)
};


void	
IOUSBControllerV3::FixupNECControllerConfigRegisters(void)
{
	UInt32	ext1;
	UInt16	pmc;
	
	
	pmc = _device->configRead16(kNECuPD720101PMC);
	
	if ( !(pmc & kPCIPMCPMESupportFromD3Cold) )
	{
		USBLog(2, "IOUSBControllerV3[%s][%p]::FixupNECControllerConfigRegisters - D3cold not set in PMC - changing", getName(), this);
		ext1 = _device->configRead32(kNECuPD720101EXT1);
		
		// first enable the write to the D3cold register
		_device->configWrite32(kNECuPD720101EXT1, ext1 | kNECuPD720101EXT1ID_WE);
		
		// now turn on D3cold PME generation
		_device->configWrite16(kNECuPD720101PMC, pmc | kPCIPMCPMESupportFromD3Cold);
		
		// make it read only again
		_device->configWrite32(kNECuPD720101EXT1, ext1);
	}
}

USBDeviceAddress 
IOUSBControllerV3::UIMGetActualDeviceAddress(USBDeviceAddress current)
{
#pragma unused(current)
	return 0;
}

IOReturn
IOUSBControllerV3::UIMCreateSSBulkEndpoint(
                                                UInt8		functionNumber,
                                                UInt8		endpointNumber,
                                                UInt8		direction,
                                                UInt8		speed,
                                                UInt16		maxPacketSize,
                                                UInt32      maxStream,
                                                UInt32      maxBurst)
{
	// UIM should override this method if it supprts streams
	
#pragma unused (functionNumber, endpointNumber, direction, speed, maxPacketSize, maxStream, maxBurst)

    return kIOReturnUnsupported;			// not implemented
}

IOReturn
IOUSBControllerV3::UIMCreateSSInterruptEndpoint(
                                                short		functionAddress,
                                                short		endpointNumber,
                                                UInt8		direction,
                                                short		speed,
                                                UInt16		maxPacketSize,
                                                short		pollingRate,
                                                UInt32      maxBurst)
{
	// UIM should override this method if it supprts streams
	
#pragma unused (functionAddress, endpointNumber, direction, speed, maxPacketSize, pollingRate, maxBurst)
    
    return kIOReturnUnsupported;			// not implemented
}

IOReturn
IOUSBControllerV3::UIMCreateSSIsochEndpoint(
                                            short				functionAddress,
                                            short				endpointNumber,
                                            UInt32				maxPacketSize,
                                            UInt8				direction,
                                            UInt8				interval,
                                            UInt32              maxBurst)
{
	// UIM should override this method if it supprts streams
	
#pragma unused (functionAddress, endpointNumber, maxPacketSize, direction, interval, maxBurst)
    
    return kIOReturnUnsupported;			// not implemented
}

IOReturn
IOUSBControllerV3::UIMCreateStreams(UInt8				functionNumber,
                                    UInt8				endpointNumber,
                                    UInt8				direction,
                                    UInt32              maxStream)
{
	// UIM should override this method if it supprts streams
	
#pragma unused (functionNumber)
#pragma unused (endpointNumber)
#pragma unused (direction)
#pragma unused (maxStream)
    
    return kIOReturnUnsupported;			// not implemented
}

IOReturn        
IOUSBControllerV3::GetBandwidthAvailableForDevice(IOUSBDevice *forDevice,  UInt32 *pBandwidthAvailable)
{
#pragma unused(forDevice)
    
    UInt32      bandwidth;
    IOReturn    ret = kIOReturnSuccess;
    
	USBLog(7, "IOUSBControllerV3(%s)[%p]::GetBandwidthAvailableForDevice - calling old method", getName(), this);
    // the default implementation it to ignore the rootHubPort and just call into the old KPI
    bandwidth = GetBandwidthAvailable();
    *pBandwidthAvailable = bandwidth;
    return ret;
}

OSMetaClassDefineReservedUsed(IOUSBControllerV3,  0);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  1);

OSMetaClassDefineReservedUsed(IOUSBControllerV3,  2);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  3);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  4);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  5);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  6);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  7);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  8);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  9);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  10);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  11);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  12);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  13);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  14);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  15);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  16);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  17);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  18);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  19);

OSMetaClassDefineReservedUnused(IOUSBControllerV3,  20);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  21);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  22);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  23);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  24);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  25);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  26);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  27);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  28);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  29);

