/*
 * Copyright (c) 2004-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include "Apple16X50UARTSync.h"


/****** From iokit/IOKit/pwr_mgt/IOPMpowerState.h for reference (see ourPowerStates below) ******

struct IOPMPowerState
{
    unsigned long	version;				// version number of this struct

    IOPMPowerFlags	capabilityFlags;			// bits that describe the capability
    IOPMPowerFlags	outputPowerCharacter;			// description (to power domain children)
    IOPMPowerFlags	inputPowerRequirement;			// description (to power domain parent)

    unsigned long	staticPower;				// average consumption in milliwatts
    unsigned long	unbudgetedPower;			// additional consumption from separate power supply (mw)
    unsigned long	powerToAttain;				// additional power to attain this state from next lower state (in mw)

    unsigned long	timeToAttain;				// (microseconds)
    unsigned long	settleUpTime;				// (microseconds)
    unsigned long	timeToLower;				// (microseconds)
    unsigned long	settleDownTime;				// (microseconds)

    unsigned long	powerDomainBudget;			// power in mw a domain in this state can deliver to its children
};

*************************************************************************************************/
enum {
    kACPI_Settle_time = 500	// guess 500 microseconds
};

enum {			// just two power states supported, on and off.
    kACPI_PowerStateOff = 0,
    kACPI_PowerStateOn,
    kACPI_PowerStateCount
};

static IOPMPowerState gOurPowerStates[kACPI_PowerStateCount] =
{
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, IOPMDeviceUsable | IOPMMaxPerformance, IOPMPowerOn, IOPMPowerOn, 50, 0, 0,
	kACPI_Settle_time, kACPI_Settle_time, kACPI_Settle_time, kACPI_Settle_time, 0}
};

#pragma mark -- Class overrides

/****************************************************************************************************/
//
//		Method:		Apple16X50UARTSync::registerWithPolicyMaker
//
//		Inputs:		policyMaker - the power policy maker
//
//		Outputs:	Return code - various
//
//		Desc:		Initialize the driver for power management and register
//				ourselves with policy-maker. Called by superclass - not by
//				Power Management
//
/****************************************************************************************************/

bool Apple16X50UARTSync::initForPM(IOService *provider)
{
    IOReturn	rc;

    PMinit();                   				// Initialize superclass variables
    provider->joinPMtree(this); 				// Attach into the power management hierarchy
    if (pm_vars == NULL) {					// Did it work
        return false;
    }

    // Initialize power management support state.
    
    // if async on/off might need two of these
    fPowerThreadCall = thread_call_allocate(handleSetPowerState,
					    (thread_call_param_t) this );

    if (fPowerThreadCall == NULL) {		// check early, check often
	return false;
    }

    // We've done the PMinit, so we're the policy mgr.  Register w/ourselves as the driver (yuck)
    rc = registerPowerDriver(this, gOurPowerStates, kACPI_PowerStateCount);
    if (rc) {
        return false;
    }

    return true;

}/* end registerWithPolicyMaker */

    

/****************************************************************************************************/
//
//		Method:		Apple16X50UARTSync::setPowerState
//
//		Inputs:		powerStateOrdinal - the state being set
//
//		Outputs:	Return code - IOPMAckImplied or IOPMNoSuchState
//
//		Desc:		Power state is being set
//
/****************************************************************************************************/

IOReturn Apple16X50UARTSync::setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice)
{
    bool ok;
    UInt32 counter = 0;

//	IOLog("Apple16X50UARTSync::setPowerState (%ld) - >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n",powerStateOrdinal);
    	
    retain();				// paranoia is your friend, make sure we're not freed
	
    fWaitForGatedCmd = true;		// could do this async, but let's be sync initially
    
    ok = thread_call_enter1(fPowerThreadCall, (void *)powerStateOrdinal);     // schedule work on workloop
	
    if (ok) {				// if thread was already pending ...
	release();			// don't need/want the retain, so undo it
    }

    while (fWaitForGatedCmd) {		// we're being sync for now, wait for it to finish
	IOSleep(2);			// it should be very fast
	counter++;
    }
    
//	IOLog("Apple16X50UARTSync::setPowerState - <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    return IOPMAckImplied;		// we're done
    
}/* end setPowerState */



#pragma mark -- Support and private Methods

//---------------------------------------------------------------------------
// handleSetPowerState()
//
// param0 - the object
// param1 - unsigned long, new power state ordinal
//---------------------------------------------------------------------------
//static
void Apple16X50UARTSync::handleSetPowerState(thread_call_param_t param0, thread_call_param_t param1 )
{
    Apple16X50UARTSync *self = OSDynamicCast(Apple16X50UARTSync, (const OSMetaClassBase *)param0);
    UInt32 on_off = (UInt32)param1;		// new power state
    
    if (self && self->CommandGate) {
	self->CommandGate->runAction(&(self->setPowerStateGated), (void *)on_off, (void *)0, (void *)0, (void *)0);
	self->release();		// offset the retain in setPowerState()
    }
}

//---------------------------------------------------------------------------
// setPowerState()
//
// owner = the object
// arg0 = boolean, true=power on, false=power off
//---------------------------------------------------------------------------
// static
IOReturn Apple16X50UARTSync::setPowerStateGated(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
    Apple16X50UARTSync *self = OSDynamicCast(Apple16X50UARTSync, (const OSMetaClassBase *)owner);
    UInt32 newState = (UInt32)arg0;		// new power state to go to
    
    // sleep -> save UART settings
    // wake -> restore UART settings
    // assumptions: not powering off no need to fully reprogram the chip
    if (self && self->portOpened && self->fCurrentPowerState != newState) 
	{
		if (newState) 
		{
			self->restoreUART();
			self->setStateGated(PD_RS232_S_CAR, PD_RS232_S_CAR);
		}
		else 
		{
			self->saveUART();
			self->setStateGated(0, PD_RS232_S_CAR);
		}
    }
    
    self->fCurrentPowerState = newState;
    self->fWaitForGatedCmd = false;	    // release caller (power thread)
    return kIOReturnSuccess;
}


/****************************************************************************************************/
//
//		Method:		Apple16X50UARTSync::maxCapabilityForDomainState
//
//		Inputs:		domainState - current state
//
//		Outputs:	Return code - maximum state
//
//		Desc:		Returns the maximum state of card power, which would be
//		  		power on without any attempt to manager power
//
/****************************************************************************************************/

unsigned long Apple16X50UARTSync::maxCapabilityForDomainState(IOPMPowerFlags domainState)
{
    if (domainState & IOPMPowerOn ) {
        return kACPI_PowerStateCount - 1;
    }

    return 0;

}/* end maxCapabilityForDomainState */

/****************************************************************************************************/
//
//		Method:		Apple16X50UARTSync::initialPowerStateForDomainState
//
//		Inputs:		domainState - current state
//
//		Outputs:	Return code - initial state
//
//		Desc:		The power domain may be changing state.	If power is on in the new
//				state, that will not affect our state at all. If domain power is off,
//				we can attain only our lowest state, which is off
//
/****************************************************************************************************/

unsigned long Apple16X50UARTSync::initialPowerStateForDomainState(IOPMPowerFlags domainState)
{
    if (domainState & IOPMPowerOn) {
        return kACPI_PowerStateCount - 1;
    }
    return 0;

}/* end initialPowerStateForDomainState */

/****************************************************************************************************/
//
//		Method:		Apple16X50UARTSync::powerStateForDomainState
//
//		Inputs:		domainState - current state
//
//		Outputs:	Return code - power state
//
//		Desc:		The power domain may be changing state.	If power is on in the new
//				state, that will not affect our state at all. If domain power is off,
//				we can attain only our lowest state, which is off
//
/****************************************************************************************************/

unsigned long Apple16X50UARTSync::powerStateForDomainState(IOPMPowerFlags domainState )
{

    if (domainState & IOPMPowerOn) {
        return 1;						// This should answer What If?
    }
    return 0;

}/* end powerStateForDomainState */



