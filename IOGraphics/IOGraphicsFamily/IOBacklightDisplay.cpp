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

#include <IOKit/IOLib.h>
#include <IOKit/graphics/IODisplay.h>
#include <IOKit/ndrvsupport/IOMacOSVideo.h>
#include <IOKit/pwr_mgt/RootDomain.h>


/*
    We further divide the actual display panel brightness levels into four
    IOKit power states which we export to our superclass.
    
    In the lowest state, the display is off.  This state consists of only one
    of the brightness levels, the lowest. In the next state it is in the dimmest
    usable state. The highest state consists of all the brightness levels.
    
    The display has no state or configuration or programming that would be
    saved/restored over power state changes, and the driver does not register
    with the superclass as an interested driver.
    
    This driver doesn't have much to do. It changes between the four power state
    brightnesses on command from the superclass, and it notices which brightness
    level the user has set.
    
    The only smart thing it does is keep track of which of the brightness levels
    the user has selected, and it never exceeds that on command from the display
    device object.
*/

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IODisplay

OSDefineMetaClassAndStructors(IOBacklightDisplay, IODisplay)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum {
    kIOBacklightDisplayMaxUsableState  = kIODisplayMaxPowerState - 1
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// probe
//

IOService * IOBacklightDisplay::probe( IOService * provider, SInt32 * score )
{
    IOFramebuffer *	framebuffer;
    IOService *		ret = 0;
    UInt32		displayType;
    UInt32		connectFlags;
    bool		haveBacklight = false;

    do {
        if( !super::probe( provider, score ))
            continue;

        framebuffer = (IOFramebuffer *) getConnection()->getFramebuffer();

        for( IOItemCount idx = 0; idx < framebuffer->getConnectionCount(); idx++) {

            if( kIOReturnSuccess != framebuffer->getAttributeForConnection( idx,
                                        kConnectionFlags, &connectFlags))
                continue;
            if( 0 == (kIOConnectionBuiltIn & connectFlags))
                continue;
            if( kIOReturnSuccess != framebuffer->getAppleSense( idx, NULL, NULL, NULL, &displayType))
                continue;
            if( (kPanelTFTConnect != displayType)
             && (kGenericLCD != displayType)
             && (kPanelFSTNConnect != displayType))
                continue;

            OSIterator * iter = getMatchingServices( nameMatching("backlight") );
            if( iter) {
                haveBacklight = (0 != iter->getNextObject()); 
                iter->release();
            }
            if( !haveBacklight)
                continue;
    
            ret = this;		// yes, we will control the panel
            break;
        }

    } while( false );
    
    return( ret );
}

void IOBacklightDisplay::stop( IOService * provider )
{
    getPMRootDomain()->receivePowerNotification( kIOPMDisableClamshell );

    return( super::stop( provider ));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initForPM
//
// This method overrides the one in IODisplay to do PowerBook-only
// power management of the display.

void IOBacklightDisplay::initPowerManagement( IOService * provider )
{
    static const IOPMPowerState ourPowerStates[kIODisplayNumPowerStates] = {
    // version,
    //   capabilityFlags,	   outputPowerCharacter, inputPowerRequirement,
    { 1, 0,                                     0, 0,           0,0,0,0,0,0,0,0 },
    { 1, 0,                      		0, IOPMPowerOn, 0,0,0,0,0,0,0,0 },
    { 1, IOPMDeviceUsable,                      0, IOPMPowerOn, 0,0,0,0,0,0,0,0 },
    { 1, IOPMDeviceUsable | IOPMMaxPerformance, 0, IOPMPowerOn, 0,0,0,0,0,0,0,0 }
    // staticPower, unbudgetedPower, powerToAttain, timeToAttain, settleUpTime, 
    // timeToLower, settleDownTime, powerDomainBudget
    };

    if( !fDisplayParams
     || !getIntegerRange( fDisplayParams, gIODisplayBrightnessKey,
                            &fCurrentBrightness, &fMinBrightness, &fMaxBrightness )) {

        fMinBrightness = 0;
        fMaxBrightness = 255;
        fCurrentBrightness = fMaxBrightness;
    }

    if( fCurrentBrightness < (fMinBrightness + 2))
        fCurrentBrightness = fMinBrightness + 2;

    fCurrentUserBrightness = fCurrentBrightness;

    fMaxBrightnessLevel[0] = 0;
    fMaxBrightnessLevel[1] = fMinBrightness;
    fMaxBrightnessLevel[2] = fMinBrightness + 1;
    fMaxBrightnessLevel[3] = fMaxBrightness;

    fDisplayPMVars->currentState = kIODisplayMaxPowerState;

    // initialize superclass variables
    PMinit();
    // attach into the power management hierarchy
    provider->joinPMtree(this);	

    // register ourselves with policy-maker (us)
    registerPowerDriver(this, (IOPMPowerState *) ourPowerStates, kIODisplayNumPowerStates);

    getPMRootDomain()->receivePowerNotification( kIOPMEnableClamshell );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// setPowerState
//

IOReturn IOBacklightDisplay::setPowerState( unsigned long powerState, IOService * whatDevice )
{
    IOReturn	ret = IOPMAckImplied;
    SInt32	value;

    if( powerState >= kIODisplayNumPowerStates)
        return( IOPMAckImplied );
        
    fCurrentPowerState = powerState;

    value = fMaxBrightnessLevel[fCurrentPowerState];
    if( value > fCurrentUserBrightness)
        value = fCurrentUserBrightness;

    setBrightness( value );

    powerState |= (powerState >= kIOBacklightDisplayMaxUsableState) ? kFBDisplayUsablePowerState : 0;
    if( fConnection)
        fConnection->setAttributeForConnection( kConnectionPower, powerState );

    return( ret );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// Alter the backlight brightness by user request.

bool IOBacklightDisplay::doIntegerSet( OSDictionary * params,
                                        const OSSymbol * paramName, UInt32 value )
{
    if( paramName != gIODisplayBrightnessKey)
        return( super::doIntegerSet( params, paramName, value));
    else {
        fCurrentUserBrightness = value;
        return( setBrightness( value ));
    }
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// setBrightness

bool IOBacklightDisplay::setBrightness( SInt32 value )
{
    bool	ret = true;

    if( !fDisplayParams)
        return( false );

#if 0
    UInt32	newState;

    // We make sure the brightness is not above the maximum
    // brightness level of our current power state.  If it
    // is too high, we ask the device to raise power.

    for( newState = 0; newState < kIODisplayNumPowerStates; newState++ ) {
        if( value <= fMaxBrightnessLevel[newState] )
            break;
    }
    if( newState >= kIODisplayNumPowerStates)
        return( false );

    if( newState != fCurrentPowerState) {
        // request new state
        if( IOPMNoErr != changePowerStateToPriv( newState ))
            value = fCurrentBrightness;
    }

    if( value != fCurrentBrightness)
#endif
    {
        fCurrentBrightness = value;
        if( value <= fMinBrightness)
            value = 0;
        ret = super::doIntegerSet( fDisplayParams, gIODisplayBrightnessKey, value);
    }

    return( ret );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// This simple device needs only power.  If the power domain is supplying
// power, the display can go to its highest state.  If there is no power
// it can only be in its lowest state, which is off.

unsigned long IOBacklightDisplay::maxCapabilityForDomainState( IOPMPowerFlags domainState )
{
    if( domainState & IOPMPowerOn )
        return( kIODisplayMaxPowerState );
    else
        return( 0 );
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initialPowerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.

unsigned long IOBacklightDisplay::initialPowerStateForDomainState( IOPMPowerFlags domainState )
{
    UInt32	newState;

    if( domainState & IOPMPowerOn ) {
        // domain has power,
        // find power state that has our current brightness level
        for( newState = 0; newState < kIODisplayNumPowerStates; newState++ ) {
           if( fCurrentBrightness <= fMaxBrightnessLevel[newState] )
                return( newState );
       }
    }
    // domain is down, so display is off
    return( 0 );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// powerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.

unsigned long IOBacklightDisplay::powerStateForDomainState( IOPMPowerFlags domainState )
{
    UInt32	newState;

    if( domainState & IOPMPowerOn ) {
        // domain has power,
        // find power state that has our current brightness level
        for( newState = 0; newState < kIODisplayNumPowerStates; newState++ ) {
            if( fCurrentBrightness <= fMaxBrightnessLevel[newState] )
                return( newState );
        }
    }
    // domain is down, so display is off
    return( 0 );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AppleBacklightDisplay : public IOBacklightDisplay
{
    OSDeclareDefaultStructors(AppleBacklightDisplay)
};

#undef super
#define super IODisplay

OSDefineMetaClassAndStructors(AppleBacklightDisplay, IOBacklightDisplay)

OSMetaClassDefineReservedUnused(IOBacklightDisplay, 0);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 1);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 2);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 3);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 4);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 5);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 6);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 7);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 8);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 9);

