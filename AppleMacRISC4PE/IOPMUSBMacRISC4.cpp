/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
#include "IOPMUSBMacRISC4.h"

#include <IOKit/IOLib.h>

#define number_of_power_states 3

static IOPMPowerState ourPowerStates[number_of_power_states] = {
{1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMPowerOn,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0},
    {1,IOPMPowerOn,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

#define super IOService
OSDefineMetaClassAndStructors(IOPMUSBMacRISC4,IOService)


// **********************************************************************************
// start
//
// **********************************************************************************
bool IOPMUSBMacRISC4::start ( IOService * nub )
{
    super::start(nub);
    PMinit();

    registerPowerDriver(this,ourPowerStates,number_of_power_states);

    changePowerStateTo( number_of_power_states-1);			// clamp power on

    setProperty ("IOClass", "IOPMUSBMacRISC4");

    return true;
}


// **********************************************************************************
// setPowerState
//
// **********************************************************************************
IOReturn IOPMUSBMacRISC4::setPowerState ( long powerStateOrdinal, IOService* whatDevice)
{
  return IOPMAckImplied;
}


// **********************************************************************************
// maxCapabilityForDomainState
//
// this is incomplete pending completed design of root domain states
// **********************************************************************************
unsigned long IOPMUSBMacRISC4::maxCapabilityForDomainState ( IOPMPowerFlags powerFlags)
{
    if ( powerFlags & IOPMPowerOn ) {
        return  number_of_power_states-1;
    }
    return  0;
}


// **********************************************************************************
// powerStateForDomainState
//
// this is incomplete pending completed design of root domain states
// **********************************************************************************
unsigned long IOPMUSBMacRISC4::powerStateForDomainState ( IOPMPowerFlags powerFlags)
{
    if ( powerFlags & IOPMPowerOn ) {
        return  number_of_power_states-1;
    }
    return  0;
}


// **********************************************************************************
// initialPowerStateForDomainState
//
// this is incomplete pending completed design of root domain states
// **********************************************************************************
unsigned long IOPMUSBMacRISC4::initialPowerStateForDomainState ( IOPMPowerFlags powerFlags)
{
    if ( powerFlags & IOPMPowerOn ) {
        return  number_of_power_states-1;
    }
    return  0;
}
