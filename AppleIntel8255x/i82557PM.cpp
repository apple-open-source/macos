/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include "i82557.h"

/**************************************************************************
* POWER-MANAGEMENT CODE
***************************************************************************
* These definitions and functions allow the driver to handle power state
* changes to support sleep and wake.
**************************************************************************/

// Two power states are supported by the driver, On and Off.

enum {
    k8255xPowerStateOff = 0,
    k8255xPowerStateOn,
    k8255xPowerStateCount
};

// An IOPMPowerState structure is added to the array for each supported
// power state. This array is used by the power management policy-maker
// to determine our capabilities and requirements for each power state.

static IOPMPowerState i8255xPowerStateArray[ k8255xPowerStateCount ] =
{
    { 1,0,0,0,0,0,0,0,0,0,0,0 },
    { 1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0 }
};

enum {
    kFiveSeconds = 5000000
};

//---------------------------------------------------------------------------
// handleSetPowerStateOff()
//
// The policy-maker has told the driver to turn off the device, and the
// driver has started a new thread to do this. This C function is the start
// of that thread. This function then calls itself through the command gate
// to gain exclusive access to the device.
//---------------------------------------------------------------------------

void handleSetPowerStateOff( thread_call_param_t param0,
                             thread_call_param_t param1 )
{
    Intel82557 * self = ( Intel82557 * ) param0;

    assert( self );

    if ( param1 == 0 )
    {
        self->getCommandGate()->runAction( (IOCommandGate::Action)
                                           handleSetPowerStateOff,
                                           (void *) self,
                                           (void *) 1 );
    }
    else
    {
        self->setPowerStateOff();
        self->release();  // offset the retain in setPowerState()
    }
}

//---------------------------------------------------------------------------
// handleSetPowerStateOn()
//
// The policy-maker has told the driver to turn on the device, and the
// driver has started a new thread to do this. This C function is the start
// of that thread. This function then calls itself through the command gate
// to gain exclusive access to the device.
//---------------------------------------------------------------------------

void handleSetPowerStateOn( thread_call_param_t param0,
                            thread_call_param_t param1 )
{
    Intel82557 * self = ( Intel82557 * ) param0;

    assert( self );

    if ( param1 == 0 )
    {
        self->getCommandGate()->runAction( (IOCommandGate::Action)
                                           handleSetPowerStateOn,
                                           (void *) self,
                                           (void *) 1 );
    }
    else
    {
        self->setPowerStateOn();
        self->release();  // offset the retain in setPowerState()
    }
}

//---------------------------------------------------------------------------
// registerWithPolicyMaker()
//
// The superclass invokes this function when it is time for the driver to
// register with the power management policy-maker of the device.
//
// The driver registers by passing to the policy-maker an array which
// describes the power states supported by the hardware and the driver.
//
// Argument:
//
// policyMaker - A pointer to the power management policy-maker of the
//               device.
//---------------------------------------------------------------------------

IOReturn Intel82557::registerWithPolicyMaker( IOService * policyMaker )
{
    IOReturn ret;

    // Initialize power management support state.

    pmPowerState  = k8255xPowerStateOn;
    pmPolicyMaker = policyMaker;

	// No cheating here. Decouple the driver's handling of power change
    // requests from the power management work loop thread. Not strictly
    // necessary for the work that this driver is doing currently, but
    // useful as an example of how to do things in general.
    //
    // Allocate thread callouts for asynchronous power on and power off.
    // Allocation failure is not detected here, but before each use in
    // the setPowerState() method.

    powerOffThreadCall = thread_call_allocate( 
                           (thread_call_func_t)  handleSetPowerStateOff,
                           (thread_call_param_t) this );

    powerOnThreadCall  = thread_call_allocate(
                           (thread_call_func_t)  handleSetPowerStateOn,
                           (thread_call_param_t) this );

    ret = pmPolicyMaker->registerPowerDriver( this,
                                              i8255xPowerStateArray,
                                              k8255xPowerStateCount );
    
    return ret;
}

//---------------------------------------------------------------------------
// setPowerState()
//
// The power management policy-maker for this device invokes this function
// to switch the power state of the device.
//
// Arguments:
//
// powerStateOrdinal - an index into the power state array for the state
//                     to switch to.
//
// policyMaker       - a pointer to the policy-maker.
//---------------------------------------------------------------------------

IOReturn Intel82557::setPowerState( unsigned long powerStateOrdinal,
                                    IOService *   policyMaker)
{
    // The default return value is for an implied acknowledgement.
    // If the appropriate thread wasn't allocated earlier in
    // registerWithPolicyMaker(), then this is what will be returned.

    IOReturn result = IOPMAckImplied;

    // There's nothing to do if our current power state is the one
    // we're being asked to change to.

    if ( pmPowerState == powerStateOrdinal )
    {
        return result;
    }

    switch ( powerStateOrdinal )
    {
        case k8255xPowerStateOff:

            // The driver is being told to turn off the device for some reason.
            // It saves whatever state and context it needs to and then shuts
            // off the device.
            //
            // It may take some time to turn off the HW, so the driver spawns
            // a thread to power down the device and returns immediately to
            // the policy-maker giving an upper bound on the time it will need
            // to complete the power state transition.

            if ( powerOffThreadCall )
            {
                // Prevent the object from being freed while a call is pending.
                // If thread_call_enter() returns TRUE, then a call is already
                // pending, and the extra retain is dropped.
                
                retain();
                if ( thread_call_enter( powerOffThreadCall ) == TRUE )
                {
                    release();
                }
                result = kFiveSeconds;
            }
            break;

        case k8255xPowerStateOn:

            // The driver is being told to turn on the device.  It does so
            // and then restores any state or context which it has previously
            // saved.
            //
            // It may take some time to turn on the HW, so the driver spawns
            // a thread to power up the device and returns immediately to
            // the policy-maker giving an upper bound on the time it will need
            // to complete the power state transition.

            if ( powerOnThreadCall )
            {
                // Prevent the object from being freed while a call is pending.
                // If thread_call_enter() returns TRUE, then a call is already
                // pending, and the extra retain is dropped.

                retain();
                if ( thread_call_enter( powerOnThreadCall ) == TRUE )
                {
                    release();
                }
                result = kFiveSeconds;
            }
            break;
        
        default:
            IOLog("%s: invalid power state (%ld)\n", getName(),
                  powerStateOrdinal);
            break;
    }

    return result;
}

//---------------------------------------------------------------------------
// setPowerStateOff()
//
// The policy-maker has told the driver to turn off the device, and this
// function is called by a new kernel thread, while holding the gate in
// the driver's work loop. Exclusive hardware access is assured.
//---------------------------------------------------------------------------

void Intel82557::setPowerStateOff(void)
{
    // At this point, all clients have been notified of the driver's
    // imminent transition to a power state that renders it "unusable".
    // And beacuse of the response to this notification by all clients,
    // the controller driver is guaranteed to be disabled when this
    // function is called.

    // If wake on Magic Packet support is active, then allow the device
    // to assert the PME# line on the PCI bus when a matching Magic Packet
    // is received, by setting the PME_En bit in the PMCSR register. The
    // assertion of the PME# signal should be sufficient to wake up the
    // machine from sleep.

    if ( pmPCICapPtr )
    {
        pciNub->configWrite16( kPCIPMCSR,
                               magicPacketEnabled ? 0x8103 : 0x8003 );
    }

    pmPowerState = k8255xPowerStateOff;

    // Since the driver returned a non-acknowledgement when called at
    // setPowerState(), it sends an ACK to the policy-maker here to
    // indicate that our power state transition is complete.

    pmPolicyMaker->acknowledgeSetPowerState();
}

//---------------------------------------------------------------------------
// setPowerStateOn()
//
// The policy-maker has told the driver to turn on the device, and this
// function is called by a new kernel thread, while holding the gate in
// the driver's work loop. Exclusive hardware access is assured.
//---------------------------------------------------------------------------

void Intel82557::setPowerStateOn()
{
    // Clear PME# assertion now that the device has regained full power.

    if ( pmPCICapPtr )
    {
        pciNub->configWrite16( kPCIPMCSR, 0x8000 );
    }

    pmPowerState = k8255xPowerStateOn;

    // Since the driver returned a non-acknowledgement when called at
    // setPowerState(), it sends an ACK to the policy-maker here to
    // indicate that our power state transition is complete.

    pmPolicyMaker->acknowledgeSetPowerState();

    // With power restored, all clients will be notified that the driver
    // has became "usable". If a client wishes to use the driver, then the
    // driver can expect a call to its enable() method to start things off.
}

//---------------------------------------------------------------------------
// setWakeOnMagicPacket()
//
// Just before the driver is disabled by the network interface when the
// system is going into sleep, this function is called to instruct the
// driver to enable Magic Packet wakeup.
//
// This function records that Magic Packet wakeup should be enabled,
// and let setPowerStateOff() take care of the rest.
//---------------------------------------------------------------------------

IOReturn Intel82557::setWakeOnMagicPacket( bool active )
{
    magicPacketEnabled = active;
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// getPacketFilters()
//
// The driver reports whether the hardware supports Magic Packets by
// implementing this function defined by the superclass.
//---------------------------------------------------------------------------

IOReturn Intel82557::getPacketFilters( const OSSymbol * group,
                                       UInt32 *         filters ) const
{
	if ( ( group == gIOEthernetWakeOnLANFilterGroup ) &&
         ( magicPacketSupported ) )
	{
		*filters = kIOEthernetWakeOnMagicPacket;
		return kIOReturnSuccess;
	}

    // For any other filter groups, return the default set of filters
    // reported by IOEthernetController.

	return IOEthernetController::getPacketFilters( group, filters );
}
