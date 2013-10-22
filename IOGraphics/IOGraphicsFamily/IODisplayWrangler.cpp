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


#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOHibernatePrivate.h>

#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>

#include "IODisplayWrangler.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum {
    kIODisplayWranglerNumPowerStates = kIODisplayNumPowerStates + 1,
    kIODisplayWranglerMaxPowerState = kIODisplayWranglerNumPowerStates - 1,
};

enum
{ 
	// seconds
	kDimInterval          = 15,
	kAnnoyanceWithin      = 20,
	kMaxAnnoyanceInterval = 10 * 60,
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService

OSDefineMetaClassAndStructors(IODisplayConnect, IOService)

bool IODisplayConnect::initWithConnection( IOIndex _connection )
{
    char        name[ 12 ];

    if (!super::init())
        return (false);

    connection = _connection;

    snprintf( name, sizeof(name), "display%d", (int)connection);

    setName( name);

    return (true);
}

IOFramebuffer * IODisplayConnect::getFramebuffer( void )
{
    return ((IOFramebuffer *) getProvider());
}

IOIndex IODisplayConnect::getConnection( void )
{
    return (connection);
}

IOReturn  IODisplayConnect::getAttributeForConnection( IOSelect selector, uintptr_t * value )
{
    if (!getProvider())
        return (kIOReturnNotReady);
    return ((IOFramebuffer *) getProvider())->getAttributeForConnection(connection, selector, value);
}

IOReturn  IODisplayConnect::setAttributeForConnection( IOSelect selector, uintptr_t value )
{
    if (!getProvider())
        return (kIOReturnNotReady);
    return ((IOFramebuffer *) getProvider())->setAttributeForConnection(connection,  selector, value);
}

// joinPMtree
//
// The policy-maker in the display driver calls here when initializing.
// We attach it into the power management hierarchy as a child of our
// frame buffer.

void IODisplayConnect::joinPMtree ( IOService * driver )
{
    getProvider()->addPowerChild(driver);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService
OSDefineMetaClassAndStructors(IODisplayWrangler, IOService);

IODisplayWrangler *     gIODisplayWrangler;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IODisplayWrangler::serverStart(void)
{
    mach_timespec_t timeout = { 120, 0 };

    if (!gIODisplayWrangler)
        waitForService(serviceMatching("IODisplayWrangler"), &timeout);

    if (gIODisplayWrangler)
    {
        gIODisplayWrangler->fOpen = true;
        if (gIODisplayWrangler->fMinutesToDim) gIODisplayWrangler->setIdleTimerPeriod(60);
        gIODisplayWrangler->activityTickle(0, 0);
    }

    return (gIODisplayWrangler != 0);
}

bool IODisplayWrangler::start( IOService * provider )
{
    OSObject *  notify;

    if (!super::start(provider))
        return (false);

    assert( gIODisplayWrangler == 0 );

    setProperty(kIOUserClientClassKey, "IOAccelerationUserClient");

    fMatchingLock = IOLockAlloc();
    fFramebuffers = OSSet::withCapacity( 1 );
    fDisplays = OSSet::withCapacity( 1 );

	clock_interval_to_absolutetime_interval(kDimInterval, kSecondScale, &fDimInterval);

    assert( fMatchingLock && fFramebuffers && fDisplays );

    notify = addMatchingNotification( gIOPublishNotification,
                                      serviceMatching("IODisplay"), _displayHandler,
                                      this, fDisplays );
    assert( notify );

    notify = addMatchingNotification( gIOPublishNotification,
                                      serviceMatching("IODisplayConnect"), _displayConnectHandler,
                                      this, 0, 50000 );
    assert( notify );

    gIODisplayWrangler = this;

    // initialize power managment
    gIODisplayWrangler->initForPM();
    getPMRootDomain()->publishFeature("AdaptiveDimming");

    return (true);
}

bool IODisplayWrangler::_displayHandler( void * target, void * ref,
        IOService * newService, IONotifier * notifier )
{
    return (((IODisplayWrangler *)target)->displayHandler((OSSet *) ref,
            (IODisplay *) newService));
}

bool IODisplayWrangler::_displayConnectHandler( void * target, void * ref,
        IOService * newService, IONotifier * notifier )
{
    return (((IODisplayWrangler *)target)->displayConnectHandler(ref,
            (IODisplayConnect *) newService));
}

bool IODisplayWrangler::displayHandler( OSSet * set,
                                            IODisplay * newDisplay )
{
    assert( OSDynamicCast( IODisplay, newDisplay ));

    IOTakeLock( fMatchingLock );

    set->setObject( newDisplay );

    IOUnlock( fMatchingLock );

    return (true);
}

bool IODisplayWrangler::displayConnectHandler( void * /* ref */,
        IODisplayConnect * connect )
{
    SInt32              score = 50000;
    OSIterator *        iter;
    IODisplay *         display;
    bool                found = false;

    assert( OSDynamicCast( IODisplayConnect, connect ));

    IOTakeLock( fMatchingLock );

    iter = OSCollectionIterator::withCollection( fDisplays );
    if (iter)
    {
        while (!found && (display = (IODisplay *) iter->getNextObject()))
        {
            if (display->getConnection())
                continue;

            do
            {
                if (!display->attach(connect))
                    continue;
                found = ((display->probe( connect, &score ))
                         && (display->start( connect )));
                if (!found)
                    display->detach( connect );
            }
            while (false);
        }
        iter->release();
    }

    IOUnlock( fMatchingLock );

    return (true);
}

bool IODisplayWrangler::makeDisplayConnects( IOFramebuffer * fb )
{
    IODisplayConnect *  connect;
    IOItemCount         i;

    for (i = 0; i < 1 /*fb->getConnectionCount()*/; i++)
    {
        connect = new IODisplayConnect;
        if (0 == connect)
            continue;

        if ((connect->initWithConnection(i))
                && (connect->attach(fb)))
        {
            connect->registerService( kIOServiceSynchronous );
        }
        connect->release();
    }

    return (true);
}

void IODisplayWrangler::destroyDisplayConnects( IOFramebuffer * fb )
{
    OSIterator *        iter;
    OSObject *          next;
    IODisplayConnect *  connect;
    IODisplay *         display;

    fb->removeProperty(kIOFBBuiltInKey);

    iter = fb->getClientIterator();
    if (iter)
    {
        while ((next = iter->getNextObject()))
        {
            if ((connect = OSDynamicCast(IODisplayConnect, next)))
            {
                if (connect->isInactive())
                    continue;
                display = OSDynamicCast( IODisplay, connect->getClient());
                if (display)
                {
                    gIODisplayWrangler->fDisplays->removeObject( display );
                    display->PMstop();
                }
                connect->terminate( kIOServiceSynchronous );
            }
        }
        iter->release();
    }
}

void IODisplayWrangler::activityChange( IOFramebuffer * fb )
{
    DEBG1("W", " activityChange\n");
    gIODisplayWrangler->activityTickle(0,0);
}

IODisplayConnect * IODisplayWrangler::getDisplayConnect(
    IOFramebuffer * fb, IOIndex connect )
{
    OSIterator  *       iter;
    OSObject    *       next;
    IODisplayConnect *  connection = 0;

    iter = fb->getClientIterator();
    if (iter)
    {
        while ((next = iter->getNextObject()))
        {
            connection = OSDynamicCast( IODisplayConnect, next);
            if (connection)
            {
                if (connection->isInactive())
                    continue;
                if (0 == (connect--))
                    break;
            }
        }
        iter->release();
    }
    return (connection);
}

IOReturn IODisplayWrangler::getConnectFlagsForDisplayMode(
    IODisplayConnect * connect,
    IODisplayModeID mode, UInt32 * flags )
{
    IOReturn            err = kIOReturnUnsupported;
    IODisplay *         display;

    display = OSDynamicCast( IODisplay, connect->getClient());
    if (display)
        err = display->getConnectFlagsForDisplayMode( mode, flags );
    else
    {
        err = connect->getFramebuffer()->connectFlags(
                  connect->getConnection(), mode, flags );
    }

    return (err);
}

IOReturn IODisplayWrangler::getFlagsForDisplayMode(
    IOFramebuffer * fb,
    IODisplayModeID mode, UInt32 * flags )
{
    IODisplayConnect *          connect;

    // should look at all connections
    connect = gIODisplayWrangler->getDisplayConnect( fb, 0 );
    if (!connect)
        return (fb->connectFlags(0, mode, flags));

    return (gIODisplayWrangler->
            getConnectFlagsForDisplayMode(connect, mode, flags));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOPMPowerState ourPowerStates[kIODisplayWranglerNumPowerStates] = {
            // version,
            //   capabilityFlags, outputPowerCharacter, inputPowerRequirement,
            { 1, 0,                                      0, 0,           0,0,0,0,0,0,0,0 },
            { 1, 0,                                      0, IOPMPowerOn, 0,0,0,0,0,0,0,0 },
            { 1, 0,                                      0, IOPMPowerOn, 0,0,0,0,0,0,0,0 },
            { 1, IOPMDeviceUsable | kIOPMPreventIdleSleep, 0, IOPMPowerOn, 0,0,0,0,0,0,0,0 },
            { 1, IOPMDeviceUsable | kIOPMPreventIdleSleep, 0, IOPMPowerOn, 0,0,0,0,0,0,0,0 }
            // staticPower, unbudgetedPower, powerToAttain, timeToAttain, settleUpTime,
            // timeToLower, settleDownTime, powerDomainBudget
        };


/*
    This is the Power Management policy-maker for the displays.  It senses when
    the display is idle and lowers power accordingly.  It raises power back up
    when the display becomes un-idle.
 
    It senses idleness with a combination of an idle timer and the "activityTickle"
    method call.  "activityTickle" is called by objects which sense keyboard activity,
    mouse activity, or other button activity (display contrast, display brightness,
    PCMCIA eject).  The method sets a "displayInUse" flag.  When the timer expires,
    this flag is checked.  If it is on, the display is judged "in use".  The flag is
    cleared and the timer is restarted.
    
    If the flag is off when the timer expires, then there has been no user activity
    since the last timer expiration, and the display is judged idle and its power is
    lowered.
    
    The period of the timer is a function of the current value of Power Management
    aggressiveness.  As that factor varies from 1 to 999, the timer period varies
    from 1004 seconds to 6 seconds.  Above 1000, the system is in a very aggressive
    power management condition, and the timer period is 5 seconds.  (In this case,
    the display dims between five and ten seconds after the last user activity).
    
    This driver calls the drivers for each display and has them move their display
    between various power states. When the display is idle, its power is dropped
    state by state until it is in the lowest state.  When it becomes un-idle it is
    powered back up to the state where it was last being used.
    
    In times of very high power management aggressiveness, the display will not be
    operated above the lowest power state which is marked "usable".
    
    When Power Management is turned off (aggressiveness = 0), the display is never
    judged idle and never dimmed.
    
    We register with Power Management only so that we can be informed of changes in
    the Power Management aggressiveness factor.  We don't really have a device with
    power states so we implement the absolute minimum. The display drivers themselves
    are part of the Power Management hierarchy under their respective frame buffers.
*/

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initForPM
//

void IODisplayWrangler::initForPM(void )
{
    // initialize superclass variables
    PMinit();

    // attach into the power management hierarchy
    joinPMtree( this );

    // register ourselves with policy-maker (us)
    registerPowerDriver( this, ourPowerStates, kIODisplayWranglerNumPowerStates );
    makeUsable();

    // HID system is waiting for this
    registerService();
}

unsigned long IODisplayWrangler::initialPowerStateForDomainState( IOPMPowerFlags domainState )
{
    if (domainState & IOPMPowerOn)
        return (kIODisplayWranglerMaxPowerState);
    else
        return (0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// setAggressiveness
//
// We are informed by our power domain parent of a new level of "power management
// aggressiveness" which we use as a factor in our judgement of when we are idle.
// This change implies a change in our idle timer period, so restart that timer.
// timer.

IOReturn IODisplayWrangler::setAggressiveness( unsigned long type, unsigned long newLevel )
{
    switch (type)
    {

      case kIOFBCaptureAggressiveness:

        if (fDimCaptured && !newLevel)
            activityTickle(0,0);

        fDimCaptured = (0 != newLevel);
        setProperty("DimCaptured", fDimCaptured);

        /* fall thru */

      case kPMMinutesToDim:
        // minutes to dim received
        if (kPMMinutesToDim == type)
        {
            // Display will always dim at least kMinDimTime seconds before
            // display sleep kicks in.
            fMinutesToDim = newLevel;
			clock_interval_to_absolutetime_interval(fMinutesToDim * 60, kSecondScale, &fOffInterval[0]);

			uint32_t annoyedInterval = fMinutesToDim * 60 * 2;
			if (annoyedInterval > kMaxAnnoyanceInterval) annoyedInterval = kMaxAnnoyanceInterval;
			if (annoyedInterval < (fMinutesToDim * 60))  annoyedInterval = (fMinutesToDim * 60);
			clock_interval_to_absolutetime_interval(annoyedInterval, kSecondScale, &fOffInterval[1]);
			AbsoluteTime_to_scalar(&fSettingsChanged) = mach_absolute_time();

			DEBG2("W", " fMinutesToDim %ld, annoyed %d\n", newLevel, annoyedInterval);
        }

        newLevel = fDimCaptured ? 0 : fMinutesToDim;
        if (newLevel == 0)
        {
            // pm turned off while idle?
            if (getPowerState() < kIODisplayWranglerMaxPowerState)
            {
                // yes, bring displays up again
//                activityTickle(0,0);
                changePowerStateToPriv( kIODisplayWranglerMaxPowerState );
            }
        }
        setIdleTimerPeriod(newLevel * 30);
        break;

      default:
        break;
    }
    super::setAggressiveness(type, newLevel);
    return (IOPMNoErr);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// setPowerState
//
// The vanilla policy-maker in the superclass is changing our power state.
// If it's down, inform the displays to lower one state, too.  If it's up,
// the idle displays are made usable.

IOReturn IODisplayWrangler::setPowerState( unsigned long powerStateOrdinal, IOService * whatDevice )
{
    fPendingPowerState = powerStateOrdinal;

    DEBG2("W", " (%ld), pwr %d open %d\n", 
                powerStateOrdinal, gIOGraphicsSystemPower, fOpen);

    if (powerStateOrdinal == 0)
    {
        // system is going to sleep
        // keep displays off on wake till UI brings them up
        changePowerStateToPriv(0);
        return (IOPMNoErr);
    }

    if (!gIOGraphicsSystemPower || !fOpen)
        return (IOPMNoErr);
    else if (powerStateOrdinal < getPowerState())
    {
        // HI is idle, drop power
        if (kIODisplayWranglerMaxPowerState == getPowerState())
        {
			clock_interval_to_deadline(kAnnoyanceWithin, kSecondScale, &fAnnoyanceUntil);
        }

		if (powerStateOrdinal < 3) fAnnoyed = false;

        IOFramebuffer::updateDisplaysPowerState();
    }
    else if (powerStateOrdinal == kIODisplayWranglerMaxPowerState)
    {
        // there is activity, raise power
        IOFramebuffer::updateDisplaysPowerState();

		start_PM_idle_timer();
    }
    return (IOPMNoErr);
}

unsigned long IODisplayWrangler::getDisplaysPowerState(void)
{
    unsigned long state = gIODisplayWrangler 
                                ? gIODisplayWrangler->fPendingPowerState
                                : kIODisplayWranglerMaxPowerState;
    return (state);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// nextIdleTimeout
//
// Virtual member function of IOService
// overridden here to provide custom power-down behavior for dimming displays.
// - Transition from 4->3 (to dim on built-in LCDs)
// - Transition from 3->2 (to full display sleep on all machines)
//   will occur at exactly N minutes from last user activity, where N
//   is the value chosen by the user and set via setAggressiveness().

SInt32 IODisplayWrangler::nextIdleTimeout(
    AbsoluteTime currentTime,
    AbsoluteTime lastActivity, 
    unsigned int powerState)
{
	AbsoluteTime deadline;
	uint64_t delayNS = 0;
	SInt32   delaySecs = 0;

    if (!fOpen)
    {
        enum { kWindowServerStartTime = 24 * 60 * 60 };
        return (kWindowServerStartTime);
    }

	if (CMP_ABSOLUTETIME(&fSettingsChanged, &lastActivity) > 0) lastActivity = fSettingsChanged;

    switch (fPendingPowerState) 
    {
        case 4:
            // The system is currently in its 'on' state
        case 3:
            // The system is currently in its 'dim' state
            // The transition into the next 'display sleep' state must occur
            // fMinutesToDim after last UI activity
			deadline = lastActivity;
			ADD_ABSOLUTETIME(&deadline, &fOffInterval[fAnnoyed]);
			if (4 == fPendingPowerState) SUB_ABSOLUTETIME(&deadline, &fDimInterval);
			if (CMP_ABSOLUTETIME(&deadline, &currentTime) > 0)
			{
				SUB_ABSOLUTETIME(&deadline, &currentTime);
				absolutetime_to_nanoseconds(deadline, &delayNS);
				delaySecs = delayNS / kSecondScale;
			}
            break;

        case 2:
        case 1:
            delaySecs = fMinutesToDim * 30;
            break;
        case 0:
        default:
            // error
            delaySecs = 60;
            break;
    }

	DEBG2("W", " %ld, %d, annoyed %d, now %lld, last %lld\n", 
				fPendingPowerState, (int) delaySecs, fAnnoyed,
				AbsoluteTime_to_scalar(&currentTime),
				AbsoluteTime_to_scalar(&lastActivity));

    if (!delaySecs) delaySecs = 1;
	
    return (delaySecs);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// activityTickle
//
// This is called by the HID system and calls the superclass in turn.

bool IODisplayWrangler::activityTickle( unsigned long x, unsigned long y )
{
    AbsoluteTime now;

    if (!fOpen)
        return (true);

    AbsoluteTime_to_scalar(&now) = mach_absolute_time();
    if (AbsoluteTime_to_scalar(&fIdleUntil))
    {
        if (CMP_ABSOLUTETIME(&now, &fIdleUntil) < 0)
            return (true);
        AbsoluteTime_to_scalar(&fIdleUntil) = 0;
    }

    // Record if this was an annoyance.
    if (AbsoluteTime_to_scalar(&fAnnoyanceUntil))
	{
		DEBG2("W", " now %lld, annoyed until %lld\n", 
				AbsoluteTime_to_scalar(&now),
				AbsoluteTime_to_scalar(&fAnnoyanceUntil));

        if (CMP_ABSOLUTETIME(&now, &fAnnoyanceUntil) < 0)
            fAnnoyed = true;
        AbsoluteTime_to_scalar(&fAnnoyanceUntil) = 0;
	}

    if (super::activityTickle(kIOPMSuperclassPolicy1,
                kIODisplayWranglerMaxPowerState) )
    {
        return (true);
    }

    getPMRootDomain()->wakeFromDoze();
    
    return (false);
}

OSObject * IODisplayWrangler::copyProperty( const char * aKey ) const
{
    if (!strcmp(aKey, kIOGraphicsPrefsKey))
        return (IOFramebuffer::copyPreferences());
    return (super::copyProperty(aKey));
}

IOReturn IODisplayWrangler::setProperties( OSObject * properties )
{
    OSDictionary * dict;
    OSDictionary * prefs;
    OSObject *     obj;
    OSNumber *     num;
    uint32_t       idleFor = 0;
    enum { kIODisplayRequestDefaultIdleFor = 1000,
            kIODisplayRequestMaxIdleFor    = 15000 };

    if (!(dict = OSDynamicCast(OSDictionary, properties)))
        return (kIOReturnBadArgument);

    if ((prefs = OSDynamicCast(OSDictionary,
                              dict->getObject(kIOGraphicsPrefsKey))))
    {
        return (IOFramebuffer::setPreferences(this, prefs));
    }

    obj = dict->getObject(kIORequestIdleKey);
    if (kOSBooleanTrue == obj)
    {
        idleFor = kIODisplayRequestDefaultIdleFor;
    }
    else if (kOSBooleanFalse == obj)
    {
        AbsoluteTime_to_scalar(&fIdleUntil) = 0;
        activityTickle(0, 0);
    }
    else if ((num = OSDynamicCast(OSNumber, obj)))
    {
        idleFor = num->unsigned32BitValue();
        if (idleFor > kIODisplayRequestMaxIdleFor)
            idleFor = kIODisplayRequestMaxIdleFor;
    }

    if (idleFor)
    {
		DEBG1("W", " idleFor(%d)\n", idleFor);

        clock_interval_to_deadline(idleFor, kMillisecondScale, &fIdleUntil);
        if (getPowerState() > 1)
            changePowerStateToPriv(1);
        return (kIOReturnSuccess);
    }

    return (kIOReturnSuccess);
}
