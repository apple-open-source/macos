/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1998-2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#include <ppc/proc_reg.h>

#include <IOKit/IOLib.h>

#include "USBKeyLargo.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService

OSDefineMetaClassAndStructors(USBKeyLargo, IOService);

// --------------------------------------------------------------------------
// Method: initForBus
//
// Purpose:
//   initialize the driver for a specific bus, so that the driver knows which bus
//   it is responsable for.
bool
USBKeyLargo::initForBus(UInt32 busNumber)
{
	setProperty("usb", (UInt64)busNumber, 32);
	setProperty("IOClass", "USBKeyLargo");

	// initialize for Power Management
	initForPM(getProvider());

	return true;
}

// --------------------------------------------------------------------------
// Method: initForPM
//
// Purpose:
//   initialize the driver for power managment and register ourselves with
//   superclass policy-maker
void USBKeyLargo::initForPM (IOService *provider)
{
	PMinit();                   // initialize superclass variables
	provider->joinPMtree(this); // attach into the power management hierarchy

	// KeyLargo has only 2 power states::
	// 0 OFF
	// 1 all ON
	// Pwer state fields:
	// unsigned long	version;		// version number of this struct
	// IOPMPowerFlags	capabilityFlags;	// bits that describe (to interested drivers) the capability of the device in this state
	// IOPMPowerFlags	outputPowerCharacter;	// description (to power domain children) of the power provided in this state
	// IOPMPowerFlags	inputPowerRequirement;	// description (to power domain parent) of input power required in this state
	// unsigned long	staticPower;		// average consumption in milliwatts
	// unsigned long	unbudgetedPower;	// additional consumption from separate power supply (mw)
	// unsigned long	powerToAttain;		// additional power to attain this state from next lower state (in mw)
	// unsigned long	timeToAttain;		// time required to enter this state from next lower state (in microseconds)
	// unsigned long	settleUpTime;		// settle time required after entering this state from next lower state (microseconds)
	// unsigned long	timeToLower;		// time required to enter next lower state from this one (in microseconds)
	// unsigned long	settleDownTime;		// settle time required after entering next lower state from this state (microseconds)
	// unsigned long	powerDomainBudget;	// power in mw a domain in this state can deliver to its children

	// NOTE: all these values are made up since now I do not have a real clue of what to put.
#define kNumberOfPowerStates 2

	static IOPMPowerState ourPowerStates[kNumberOfPowerStates] = {
		{1,0,0,0,0,0,0,0,0,0,0,0},
		{1,IOPMDeviceUsable,IOPMClockNormal | IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
	};


	// register ourselves with ourself as policy-maker
	if (pm_vars != NULL)
		registerPowerDriver(this, ourPowerStates, kNumberOfPowerStates);
	
	return;
}

// Method: setPowerState
//
// Purpose:
IOReturn USBKeyLargo::setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice)
{
	// Do not do anything if the state is invalid.
	if (powerStateOrdinal >= kNumberOfPowerStates)
		return IOPMAckImplied;

	// Which bus id going to change state ?
	OSNumber *propertyValue;

	propertyValue = OSDynamicCast( OSNumber, getProperty("usb"));
	if (propertyValue == NULL)
		return IOPMAckImplied;

	if ( powerStateOrdinal == 0 ) {
		kprintf("USBKeyLargo would be powered off here\n");
		turnOffUSB(propertyValue->unsigned32BitValue());
	}
	if ( powerStateOrdinal == 1 ) {
		kprintf("USBKeyLargo would be powered on here\n");
		turnOnUSB(propertyValue->unsigned32BitValue());
	}
	
	return IOPMAckImplied;
}

void USBKeyLargo::turnOffUSB(UInt32 busNumber)
{
    UInt32 regMask, regData;
    KeyLargo *provider = OSDynamicCast(KeyLargo, getProvider());

    if (provider == NULL) {
        IOLog("USBKeyLargo::turnOffUSB missing provider, can not proceed");
        return;
    }

    // well, USB requires special timing of the various FCRx register
    // changes, and should probably be broken out separately. However,
    // I will hack in the necessary USB changes here.

    // USB requires the FCR4 bits before any of the other registers are
    // touched. Since FCR4 is ALL USB, then I moved it to the top of
    // the programming list

    if (busNumber == 0) {
		regMask = kKeyLargoFCR4USB0SleepBitsSet | kKeyLargoFCR4USB0SleepBitsClear;
		regData = kKeyLargoFCR4USB0SleepBitsSet & ~kKeyLargoFCR4USB0SleepBitsClear;
    } else {
		regMask = kKeyLargoFCR4USB1SleepBitsSet | kKeyLargoFCR4USB1SleepBitsClear;
		regData = kKeyLargoFCR4USB1SleepBitsSet & ~kKeyLargoFCR4USB1SleepBitsClear;
    }
    
    provider->safeWriteRegUInt32(kKeyLargoFCR4, regMask, regData);

    // WE SHOULD HAVE A 1 MICROSECOND DELAY IN HERE
    IODelay(1); // Marco changed this from IOSleep because I do not want other threads run at this time

    // now some stuff in FCR0 which cannot be done all at once

    // clear the Cell Enable bits for USB (turns off the 48 MHz clocks)

	regData = 0;
	regMask = (busNumber == 0) ? kKeyLargoFCR0USB0CellEnable : kKeyLargoFCR0USB1CellEnable;
        
    provider->safeWriteRegUInt32(kKeyLargoFCR0, regMask, regData);
    //regTemp = provider->readRegUInt32(kKeyLargoFCR0);

    // NEED A 600 nanosecond delay in here
    IODelay(1); // Marco changed this from IOSleep because I do not want other threads run at this time

    // now set the pad suspend bits
	regData = regMask = (busNumber == 0) ? (kKeyLargoFCR0USB0PadSuspend0 | kKeyLargoFCR0USB0PadSuspend1 ) :
		(kKeyLargoFCR0USB1PadSuspend0 | kKeyLargoFCR0USB1PadSuspend1);
    
    provider->safeWriteRegUInt32(kKeyLargoFCR0, regMask, regData);

    // NEED A 600 nanosecond delay in here
    IODelay(1); // Marco changed this from IOSleep because I do not want other threads run at this time

	return;
}

void USBKeyLargo::turnOnUSB(UInt32 busNumber)
{
    UInt32 regMask, regData;
    KeyLargo *provider = OSDynamicCast(KeyLargo, getProvider());

    if (provider == NULL) {
        IOLog("USBKeyLargo::turnOnUSB missing provider, cannot proceed");
        return;
    }

    // now we clear the individual pad suspend bits
    // now set the pad suspend bits
	regData = 0;		// Clear the bits
	regMask = (busNumber == 0) ? (kKeyLargoFCR0USB0PadSuspend0 | kKeyLargoFCR0USB0PadSuspend1) :
    	(kKeyLargoFCR0USB1PadSuspend0 | kKeyLargoFCR0USB1PadSuspend1);

    provider->safeWriteRegUInt32(kKeyLargoFCR0, regMask, regData);

    IODelay(1000);

    // now we go ahead and turn on the USB cell clocks
    regData = regMask = (busNumber == 0) ? (kKeyLargoFCR0USB0CellEnable) : (kKeyLargoFCR0USB1CellEnable);
    
    provider->safeWriteRegUInt32(kKeyLargoFCR0, regMask, regData);

    // now turn off the remote wakeup bits
    if (busNumber == 0) {
		regMask = kKeyLargoFCR4USB0SleepBitsSet | kKeyLargoFCR4USB0SleepBitsClear;
        regData = ~kKeyLargoFCR4USB0SleepBitsSet & kKeyLargoFCR4USB0SleepBitsClear;        
    } else {
        regMask = kKeyLargoFCR4USB1SleepBitsSet | kKeyLargoFCR4USB1SleepBitsClear;
        regData = ~kKeyLargoFCR4USB1SleepBitsSet & kKeyLargoFCR4USB1SleepBitsClear;    
    }
    
    provider->safeWriteRegUInt32(kKeyLargoFCR4, regMask, regData);
    
	return;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// If the power domain is supplying power, the device
// can be on.  If there is no power it can only be off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

unsigned long USBKeyLargo::maxCapabilityForDomainState(
                                        IOPMPowerFlags domainState )
{
   if( (domainState & IOPMPowerOn) || (domainState & IOPMSoftSleep) )
       return( kNumberOfPowerStates - 1);
   else
       return( 0);
}


