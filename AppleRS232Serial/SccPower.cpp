/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include "AppleRS232Serial.h"

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
    
#define number_of_power_states 2

static IOPMPowerState ourPowerStates[number_of_power_states] = 
{
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};

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

    ELG(IOThreadSelf(), domainState, 'iPDS', "BCM5701Enet::initialPowerStateForDomainState");

    return 0;
    
}/* end initialPowerStateForDomainState */

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

    ELG(0, powerStateOrdinal, 'stPS', "AppleRS232Serial::setPowerState");

    if (powerStateOrdinal >= kNumOfPowerStates)
        return IOPMNoSuchState;					// Do nothing if state invalid
    
    if (powerStateOrdinal == currentPowerState)
        return IOPMAckImplied;					// No change required
        
    if (powerStateOrdinal == 1)
    {
        if (portOpened)
        {
            callPlatformFunction("PowerModem", false, (void *)true, 0, 0, 0);
            IOSleep(250);					// wait 250 milli-seconds 
            callPlatformFunction("ModemResetHigh", false, 0, 0, 0, 0);
            IOSleep(250);					// wait 250 milli-seconds 
            callPlatformFunction("ModemResetLow", false, 0, 0, 0, 0);
            IOSleep(250);					// wait 250 milli-seconds 
            callPlatformFunction("ModemResetHigh", false, 0, 0, 0, 0);
            IOSleep(250);					// wait 250 milli-seconds 
        }
    } else {
        if (!portOpened)
        {
            callPlatformFunction("ModemResetLow", false, 0, 0, 0, 0);
            IOSleep(250);					// wait 250 milli-seconds 
            callPlatformFunction("PowerModem", false, (void *)false, 0, 0, 0);
        }
    }

    return IOPMAckImplied;

}/* end setPowerState */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::initForPM
//
//		Inputs:		provider - the provider
//
//		Outputs:	Return code - true(initialized), false(it didn't)
//
//		Desc:		Initialize the power manager
//
/****************************************************************************************************/

bool AppleRS232Serial::initForPM(IOService *provider)
{

    ELG(0, 0, 'stPS', "AppleRS232Serial::initForPM");
    
    PMinit();                   				// Initialize superclass variables
    provider->joinPMtree(this); 				// Attach into the power management hierarchy

    if (pm_vars == NULL)					// Did it work
    {
        return false;
    }
    
        // Register ourselves with ourself as policy-maker
                
    registerPowerDriver(this, ourPowerStates, number_of_power_states);

    return true;
}/* end initForPM
