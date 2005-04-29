/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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


#include "AppleRS232Serial.h"
// Globals

#if USE_ELG
extern com_apple_iokit_XTrace	*gXTrace;
extern UInt32			gTraceID;
#endif


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
    kSCC_Settle_time = 500	// guess 500 microseconds
};

enum {			// just two power states supported, on and off.
    kSCC_PowerStateOff = 0,
    kSCC_PowerStateOn,
    kSCC_PowerStateCount
};

static IOPMPowerState gOurPowerStates[kSCC_PowerStateCount] =
{
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, IOPMDeviceUsable | IOPMMaxPerformance, IOPMPowerOn, IOPMPowerOn, 50, 0, 0,
	kSCC_Settle_time, kSCC_Settle_time, kSCC_Settle_time, kSCC_Settle_time, 0}
};


#define super IOSerialDriverSync

#pragma mark -- Class overrides

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::registerWithPolicyMaker
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

bool AppleRS232Serial::initForPM(IOService *provider)
{
    IOReturn	rc;

    ELG(0, provider, "initForPM - entry.  provider=");

    PMinit();                   				// Initialize superclass variables
    provider->joinPMtree(this); 				// Attach into the power management hierarchy
    if (pm_vars == NULL) {					// Did it work
        ALERT(0, provider, "initForPM - joinPMtree failed");
        return false;
    }

    // Initialize power management support state.
    
    // if async on/off might need two of these
    fPowerThreadCall = thread_call_allocate(handleSetPowerState,
					    (thread_call_param_t) this );

    if (fPowerThreadCall == NULL) {		// check early, check often
	ALERT(0, 0, "initForPM - failed to allocate thread");
	return false;
    }

    // We've done the PMinit, so we're the policy mgr.  Register w/ourselves as the driver (yuck)
    rc = registerPowerDriver(this, gOurPowerStates, kSCC_PowerStateCount);
    if (rc) {
        ALERT(0, rc, "initForPM - failed to registerPowerDriver");
        return false;
    }

    ELG(0, rc, "initForPM - exit.  rc=");
    return true;

}/* end registerWithPolicyMaker */

    

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::setPowerState
//
//		Inputs:		powerStateOrdinal - the state being set
//
//		Outputs:	Return code - IOPMAckImplied or IOPMNoSuchState
//
//		Desc:		Power state is being set
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice)
{
    bool ok;
    UInt32 counter = 0;

    ELG(0, powerStateOrdinal, "setPowerState - powerStateOrdinal");
    	
    retain();				// paranoia is your friend, make sure we're not freed
	
    fWaitForGatedCmd = true;		// could do this async, but let's be sync initially
    
    ok = thread_call_enter1(fPowerThreadCall, (void *)powerStateOrdinal);     // schedule work on workloop
	
    if (ok) {				// if thread was already pending ...
	ALERT(0, 0, "setPowerState - thread already pending?"); // a 'never' in current flow
	release();			// don't need/want the retain, so undo it
    }

    while (fWaitForGatedCmd) {		// we're being sync for now, wait for it to finish
	IOSleep(2);			// it should be very fast
	counter++;
    }
    
    ELG(0, counter, "setPowerState - finished after N sleeps of 2ms each");
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
void AppleRS232Serial::handleSetPowerState(thread_call_param_t param0, thread_call_param_t param1 )
{
    AppleRS232Serial *self = OSDynamicCast(AppleRS232Serial, (const OSMetaClassBase *)param0);
    UInt32 on_off = (UInt32)param1;		// new power state
    
    ELG(self, on_off, "handleSetPowerState - entry, self, on_off");

    if (self && self->fCommandGate) {
	self->fCommandGate->runAction(&(self->setPowerStateGated), (void *)on_off, (void *)0, (void *)0, (void *)0);
	self->release();		// offset the retain in setPowerState()
    }

    ELG(param0, param1, "handleSetPowerState - exit");
}

//---------------------------------------------------------------------------
// setPowerState()
//
// owner = the object
// arg0 = boolean, true=power on, false=power off
//---------------------------------------------------------------------------
// static
IOReturn AppleRS232Serial::setPowerStateGated(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
    AppleRS232Serial *self = OSDynamicCast(AppleRS232Serial, (const OSMetaClassBase *)owner);
    UInt32 newState = (UInt32)arg0;		// new power state to go to
    
    ELG(self, newState, "setPowerStateGated - entry.  self, newState");

    // sleep -> stop dma
    // wake -> resume dma
    // assumptions: not powering off scc cell, no need to fully reprogram the chip
    if (self && self->portOpened && self->fCurrentPowerState != newState) {
	SccChannel *channel = &self->fPort;
	if (newState) {
	    ELG(self->fCurrentPowerState, newState, "setPowerStateGated - waking up, restarting dma");
	    // test - clear status change interrupt.  CTS bounced on us, but that's ok.  
	    //SccWriteReg(channel, 0, kResetExtStsInt);			// reset pending Ext/Sts interrupts
	    //SccWriteReg(channel, 0, kResetExtStsInt);			// and again in case it bounced on us (likely)
	    // end test.
	    SccEnableInterrupts(channel, kSccInterrupts);		// Enable interrupts
	    SccEnableInterrupts(channel, kRxInterrupts);		// and on rx
	    SccEnableInterrupts(channel, kTxInterrupts);		// and on tx
	    SccdbdmaStartReception(channel, channel->activeRxChannelIndex, true);	// start up the read, interrupt on 1st byte
	}
	else {
	    ELG(self->fCurrentPowerState, newState, "setPowerStateGated - sleeping, stopping dma");
	    SccDisableInterrupts(channel, kSccInterrupts);		// Disable scc interrupts before doing anything
	    SccDisableInterrupts(channel, kRxInterrupts);		// Disable the receiver
	    SccDisableInterrupts(channel, kTxInterrupts);		// Disable the transmitter
	    SccdbdmaEndTransmission(channel);				// stop any current tx
	    SccdbdmaEndReception(channel, channel->activeRxChannelIndex);	// stop the pending read
	}
    }
    
    self->fCurrentPowerState = newState;
    self->fWaitForGatedCmd = false;	    // release caller (power thread)
    ELG(0, 0, "setPowerStateGated - exit");
    return kIOReturnSuccess;
}


/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::maxCapabilityForDomainState
//
//		Inputs:		domainState - current state
//
//		Outputs:	Return code - maximum state
//
//		Desc:		Returns the maximum state of card power, which would be
//		  		power on without any attempt to manager power
//
/****************************************************************************************************/

unsigned long AppleRS232Serial::maxCapabilityForDomainState(IOPMPowerFlags domainState)
{
  ELG(0, domainState, "maxCapability called");

    if (domainState & IOPMPowerOn ) {
	ELG(domainState, kSCC_PowerStateCount - 1, "maxCapabilityForDomainState, domainState=, returning 1");
        return kSCC_PowerStateCount - 1;
    }

    ELG(domainState, 0, "maxCapabilityForDomainState, domainState=, returning 0");
    return 0;

}/* end maxCapabilityForDomainState */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::initialPowerStateForDomainState
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

unsigned long AppleRS232Serial::initialPowerStateForDomainState(IOPMPowerFlags domainState)
{
  ELG(0, domainState, "initialPowerState called, power flags");

    if (domainState & IOPMPowerOn) {
	ELG(domainState, kSCC_PowerStateCount-1, "initialPowerStateForDomainState, domainState=, rtn 1");
        return kSCC_PowerStateCount - 1;
    }
    ELG(domainState, 0, "initialPowerStateForDomainState, domainState=, rtn 0");
    return 0;

}/* end initialPowerStateForDomainState */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::powerStateForDomainState
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

unsigned long AppleRS232Serial::powerStateForDomainState(IOPMPowerFlags domainState )
{
  ELG(0, domainState, "power state from domain state called");

    if (domainState & IOPMPowerOn) {
	ELG(domainState, 1, "powerStateForDomainState, domain=, rtn 1");
        return 1;						// This should answer What If?
    }
    ELG(domainState, 0, "powerStateForDomainState, domain=, rtn 0");
    return 0;

}/* end powerStateForDomainState */



