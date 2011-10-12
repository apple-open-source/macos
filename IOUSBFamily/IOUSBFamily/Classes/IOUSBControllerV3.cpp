/*
*
* @APPLE_LICENSE_HEADER_START@
* 
 * Copyright й 1997-2011 Apple Inc.  All rights reserved.
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
#pragma mark ееее IOKit methods ееее
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
	return super::didTerminate(provider, options, defer);
}



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// Overrides superclass implementation, to account for a couple of scenarios
// е when we are waking from hibernation, we need to force the state to kUSBPowerStateOff and then to kUSBPowerStateOn
// е when we have received a systemWillShutdown call, we force the proper state based on that
// е when our parent is going into Doze mode (which is different than our low power mode) then we switch to ON
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
		if ( (_myPowerState == kUSBPowerStateSleep) && _gHibernateState && (*_gHibernateState == kIOHibernateStateWakingFromHibernate) && !_wakingFromHibernation)
		{
			UInt16	configCommand = _device->configRead16(kIOPCIConfigCommand);
			
			// make sure that the PCI config space is set up to allow memory access. this appears to fix some OHCI controllers
			USBLog(5, "IOUSBControllerV3(%s)[%p]::maxCapabilityForDomainState - waking from hibernation - setting flag - kIOPCIConfigCommand(%p)", getName(), this, (void*)configCommand);
			_device->configWrite16(kIOPCIConfigCommand, configCommand | kIOPCICommandMemorySpace);
			USBLog(5, "IOUSBControllerV3(%s)[%p]::maxCapabilityForDomainState - new kIOPCIConfigCommand(%p)", getName(), this, (void*)_device->configRead16(kIOPCIConfigCommand));
			
			ResetControllerState();
			EnableAllEndpoints(true);
			_wakingFromHibernation = true;
			ret = kUSBPowerStateOff;
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



#pragma mark ееее class methods ееее
IOReturn
IOUSBControllerV3::CheckForEHCIController(IOService *provider)
{
	OSIterator				*siblings = NULL;
	OSIterator				*ehciList = NULL;
    mach_timespec_t			t;
    IOService				*service;
    IORegistryEntry			*entry;
    bool					ehciPresent = false;
	int						myDeviceNum = 0;
	int						ehciDeviceNum = 0;
	IOUSBControllerV3 *		testEHCI;
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
        t.tv_sec = 5;
        t.tv_nsec = 0;
        USBLog(7, "%s[%p]::CheckForEHCIController calling waitForService for AppleUSBEHCI", getName(), this);
        service = waitForService( serviceMatching("AppleUSBEHCI"), &t );
		testEHCI = (IOUSBControllerV3*)service;
		while (testEHCI)
		{
			IOPCIDevice *	testPCI = (IOPCIDevice*)testEHCI->getParentEntry(gIOServicePlane);
			ehciDeviceNum = testPCI->getDeviceNumber();
			
			if (myDeviceNum == ehciDeviceNum)
			{
				USBLog(5, "%s[%p]::CheckForEHCIController - ehciDeviceNum and myDeviceNum match (%d)", getName(), this, myDeviceNum);
				_ehciController = testEHCI;
				_ehciController->retain();
				USBLog(7, "%s[%p]::CheckForEHCIController got EHCI service %p", getName(), this, service);
				setProperty("Companion", "yes");
				break;
			}
			else
			{
				// we found an instance of EHCI, but it doesn't appear to be ours, so now I need to see how many there are in the system
				// and see if any of them matches
				USBLog(5, "%s[%p]::CheckForEHCIController - ehciDeviceNum(%d) and myDeviceNum(%d) do NOT match", getName(), this, ehciDeviceNum, myDeviceNum);
				if (ehciList)
				{
					testEHCI = (IOUSBControllerV3*)(ehciList->getNextObject());
					if (testEHCI)
					{
						USBLog(3, "%s[%p]::CheckForEHCIController - found AppleUSBEHCI after %d ms", getName(), this, checkListCount * 10);
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
					ehciList = getMatchingServices(serviceMatching("AppleUSBEHCI"));
					if (ehciList)
					{
						testEHCI = (IOUSBControllerV3*)(ehciList->getNextObject());
						if (testEHCI)
						{
							USBLog(5, "%s[%p]::CheckForEHCIController - got AppleUSBEHCI[%p] from the list", getName(), this, testEHCI);
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
	if ( !_gHibernateState )
	{
		OSData * data = OSDynamicCast(OSData, (IOService::getPMRootDomain())->getProperty(kIOHibernateStateKey));
		if (data)
		{
			_gHibernateState = (uint32_t *) data->getBytesNoCopy();
		}
		
	}
	
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
	return kIOReturnSuccess;
}



// static method
void 
IOUSBControllerV3::RootHubTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
#pragma unused (sender)
    IOUSBControllerV3		*me;
	IOReturn				ret;
    
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
	
	USBTrace( kUSBTController, kTPControllerRootHubTimer, (uintptr_t)me, (uintptr_t)me->_rootHubDevice->GetPolicyMaker(), (uintptr_t)me->_rootHubDevice->GetPolicyMaker()->getPowerState(), 4 );
    ret = me->CheckForRootHubChanges();
	
	// fire it up again
    if (me->_rootHubPollingRate32 && !me->isInactive() && me->_controllerAvailable)
		me->_rootHubTimer->setTimeoutMS(me->_rootHubPollingRate32);
}



IOReturn
IOUSBControllerV3::CheckForRootHubChanges(void)
{
	IOUSBRootHubInterruptTransaction		xaction;
	UInt8									bBitmap;
	UInt16									wBitmap;
	UInt8									bytesToMove;
	UInt8*									pBytes;
	int										i;

	// first call into the UIM to get the latest version of the bitmap
	UIMRootHubStatusChange();
	
	// now I only have to do anything if there is something in the status changed bitmap
	if (_rootHubStatusChangedBitmap)
	{
		USBLog(5, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - got _rootHubStatusChangedBitmap(%p) isInactive(%s)", getName(), this, (void*)_rootHubStatusChangedBitmap, isInactive() ? "true" : "false");
 		if (_rootHubDevice && _rootHubDevice->GetPolicyMaker())
		{
			USBLog(5, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - making sure root hub driver AppleUSBHub[%p] is usable", getName(), this, _rootHubDevice->GetPolicyMaker());
			_rootHubDevice->GetPolicyMaker()->EnsureUsability();	// this will cause the interrupt read timer to fire if it isn't already
		}
		else
		{
			USBLog(1, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - _rootHubStatusChangedBitmap(%p) with no _rootHubDevice or policy maker!!", getName(), this, (void*)_rootHubStatusChangedBitmap);
			USBTrace( kUSBTController, kTPControllerCheckForRootHubChanges, (uintptr_t)this, _rootHubStatusChangedBitmap, 0, 0);
		}
		
		if (_outstandingRHTrans[0].completion.action)
		{
			// Save our corrent transaction and move all other transactions down the queue, and make sure the last one has a NULL completion
			xaction = _outstandingRHTrans[0];
			for (i = 0; i < (kIOUSBMaxRootHubTransactions-1); i++)
			{
				_outstandingRHTrans[i] = _outstandingRHTrans[i+1];
			}
			_outstandingRHTrans[kIOUSBMaxRootHubTransactions-1].completion.action = NULL;
			
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
				USBLog(6, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - stopping timer and calling complete", getName(), this);
				RootHubStopTimer();
				_rootHubTransactionWasAborted = false;
				
				xaction.buf->writeBytes(0, pBytes, bytesToMove);
				Complete(xaction.completion, kIOReturnSuccess, xaction.bufLen - bytesToMove);
			}
			else
			{
				USBError(1, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - NULL action(%p) or buf(%p)", getName(), this, xaction.completion.action, xaction.buf);
			}

		}
		else
		{
			USBLog(5, "IOUSBControllerV3(%s)[%p]::CheckForRootHubChanges - no one is listening - i will try again later", getName(), this);
		}
	}
	
	return kIOReturnSuccess;
}



/*
 * RootHubQueueInterruptRead
 * Queue up a read on the Root Hub Interrupt Pipe - Unless there is an error and it completes right away,
 * this will end up completing when the Root Hub timer fires.
*/
IOReturn
IOUSBControllerV3::RootHubQueueInterruptRead(IOMemoryDescriptor *buf, UInt32 bufLen, IOUSBCompletion completion)
{
    int				i;

	USBTrace_Start( kUSBTController, kTPControllerRootHubQueueInterruptRead, (uintptr_t)this, (uintptr_t)buf, bufLen, 0);
	USBLog(6, "IOUSBControllerV3(%s)[%p]::RootHubQueueInterruptRead, starting timer", getName(), this);
	
	// Start the RootHub timer
	RootHubStartTimer32(kUSBRootHubPollingRate);
	
    for (i = 0; i < kIOUSBMaxRootHubTransactions; i++)
    {
        if (_outstandingRHTrans[i].completion.action == NULL)
        {
            // found free trans
            _outstandingRHTrans[i].buf = buf;
            _outstandingRHTrans[i].bufLen = bufLen;
            _outstandingRHTrans[i].completion = completion;

			if (i != 0)
			{
				USBLog(1, "IOUSBControllerV3(%s)[%p]::RootHubQueueInterruptRead - this is index(%d) - UNEXPECTED?", getName(), this, (int)i);
				USBTrace( kUSBTController, kTPControllerRootHubQueueInterruptRead, (uintptr_t)this, i, 0, 0 );
				break;
			}
			
			// If this is the first transaction after an abort interrupt read, complete it immediately
			if ( _rootHubTransactionWasAborted )
			{
				USBLog(6, "IOUSBControllerV3(%s)[%p]::RootHubQueueInterruptRead  _rootHubTransactionWasAborted was true, calling CheckForRootHubChanges()", getName(), this);
				CheckForRootHubChanges();
			}
			
            return kIOReturnSuccess;
        }
		else
		{
			USBLog(1, "IOUSBControllerV3(%s)[%p]::RootHubQueueInterruptRead  we already had a completion.action for trans: %d, returning an error", getName(), this, i);
			break;
		}
    }

	USBTrace_End( kUSBTController, kTPControllerRootHubQueueInterruptRead, (uintptr_t)this, kIOReturnInternalError, bufLen, 0);
	return kIOReturnNotPermitted;
}



/*
 * RootHubAbortInterruptRead
 * Abort a queued up read. This should only be called in the workloop gate, so it should be synchronized
*/
IOReturn
IOUSBControllerV3::RootHubAbortInterruptRead()
{
    int										i;
	IOUSBRootHubInterruptTransaction		xaction;

	USBTrace( kUSBTController, kTPControllerRootHubTimer, (uintptr_t)this, 0, 0, 3 );
	USBLog(6, "IOUSBControllerV3(%s)[%p]::RootHubAbortInterruptRead, stopping timer", getName(), this);

	RootHubStopTimer();

	xaction = _outstandingRHTrans[0];
	if (xaction.completion.action)
	{
		// move all other transactions down the queue
		for (i = 0; i < (kIOUSBMaxRootHubTransactions-1); i++)
		{
			_outstandingRHTrans[i] = _outstandingRHTrans[i+1];
		}
		_outstandingRHTrans[kIOUSBMaxRootHubTransactions-1].completion.action = NULL;
		Complete(xaction.completion, kIOReturnAborted, xaction.bufLen);
		
		_rootHubTransactionWasAborted = true;
	}
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



#pragma mark еееее IOUSBController methods еееее
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
	return IOUSBController::AbortPipe(address, endpoint);
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
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "Read");
	if ( kr != kIOReturnSuccess )
		return kr;
	
	return IOUSBController::Read(buffer, address, endpoint, completion, noDataTimeout, completionTimeout, reqCount);
}



IOReturn
IOUSBControllerV3::Write(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "Write");
	if ( kr != kIOReturnSuccess )
		return kr;

	return IOUSBController::Write(buffer, address, endpoint, completion, noDataTimeout, completionTimeout, reqCount);
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



#pragma mark еееее IOUSBControllerV2 methods еееее
//
// These methods are implemented in IOUSBController, and they all call runAction to synchronize them
// We used to close the workLoop gate when we are sleeping, and we needed to stop doing that. So we run
// these methods through here now to check the power state and hold the thread until we are awake
//
IOReturn
IOUSBControllerV3::OpenPipe(USBDeviceAddress address, UInt8 speed, Endpoint *endpoint)
{
	IOReturn	kr;
	
	kr = CheckPowerModeBeforeGatedCall((char *) "OpenPipe");
	if ( kr != kIOReturnSuccess )
		return kr;

	return IOUSBControllerV2::OpenPipe(address, speed, endpoint);
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


OSMetaClassDefineReservedUsed(IOUSBControllerV3,  0);
OSMetaClassDefineReservedUsed(IOUSBControllerV3,  1);

OSMetaClassDefineReservedUnused(IOUSBControllerV3,  2);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  3);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  4);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  5);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  6);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  7);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  8);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  9);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  10);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  11);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  12);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  13);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  14);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  15);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  16);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  17);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  18);
OSMetaClassDefineReservedUnused(IOUSBControllerV3,  19);
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
