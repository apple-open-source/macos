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


#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>

#include "IODisplayWrangler.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum {
    kIODisplayWranglerNumPowerStates = kIODisplayNumPowerStates + 1,
    kIODisplayWranglerMaxPowerState = kIODisplayWranglerNumPowerStates - 1,
};


#define kIODisplayWrangler_AnnoyancePenalties "AnnoyancePenalties"
#define kIODisplayWrangler_AnnoyanceCaps "AnnoyanceCaps"
#define kIODisplayWrangler_IdleTimeoutMin "IdleTimeoutMin"
#define kIODisplayWrangler_IdleTimeoutMax "IdleTimeoutMax"

static int gCOMPRESS_TIME = 0;

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


enum { kStaticAnnoyanceEventArrayLength = 4 };
/* static */ UInt32 IODisplayWrangler::staticAnnoyanceEventArrayLength =  kStaticAnnoyanceEventArrayLength;
/* static */ IODisplayWrangler::annoyance_event_t IODisplayWrangler::staticAnnoyanceEventArray[ kStaticAnnoyanceEventArrayLength ];

/* static */ IODisplayWrangler::annoyance_cap_t IODisplayWrangler::staticAnnoyanceCapsArray[]
=
{
    { 120,  4 },
    { 300,  8 },
    { 600, 12 },
    { 900, 16 }
};
/* static */ UInt32 IODisplayWrangler::staticAnnoyanceCapsArrayLength = sizeof(IODisplayWrangler::staticAnnoyanceCapsArray) / sizeof(*IODisplayWrangler::staticAnnoyanceCapsArray);

/* static */ IODisplayWrangler::annoyance_penalty_t IODisplayWrangler::staticAnnoyancePenaltiesArray[]
=
{
    {  3, 8 },
    { 10, 4 },
    { 30, 2 },
    { 90, 1 }
};
/* static */ UInt32 IODisplayWrangler::staticAnnoyancePenaltiesArrayLength = sizeof(staticAnnoyancePenaltiesArray) / sizeof(*staticAnnoyancePenaltiesArray);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Invariant 1: fAnnoyanceEventArrayQHead points to the first free element in the array.
// Invariant 2: adding an element to the queue increments fAnnoyanceEventArrayQHead
// Invariant 3: 0 <= fAnnoyanceEventArrayQHead < fAnnoyanceEventArrayLength
void IODisplayWrangler::enqueueAnnoyance( UInt64 dim_time_secs, UInt64 wake_time_secs, UInt32 penalty )
{
    // Record this annoyance.
    
    annoyance_event_t * annoyance = & fAnnoyanceEventArray[ fAnnoyanceEventArrayQHead ];
    
    annoyance->dim_time_secs = fLastDimTime_secs;
    annoyance->wake_time_secs = fLastWakeTime_secs;
    annoyance->penalty = penalty;

    // Increment fAnnoyanceEventArrayQHead.

    fAnnoyanceEventArrayQHead ++;
    fAnnoyanceEventArrayQHead %= fAnnoyanceEventArrayLength;
    
}

// Array-style zero-based indexing.
// The 0'th element is the head of the queue.
// The (fAnnoyanceEventArrayLength - 1)'th element is the tail.
IODisplayWrangler::annoyance_event_t * IODisplayWrangler::getNthAnnoyance( int i )
{
    // I don't trust % of negative numbers, so I bias by fAnnoyanceEventArrayLength to begin with.
    int j = ( fAnnoyanceEventArrayLength + fAnnoyanceEventArrayQHead - i - 1 ) % fAnnoyanceEventArrayLength;
    return & fAnnoyanceEventArray[ j ];
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IODisplayWrangler::serverStart(void)
{
    mach_timespec_t timeout = { 120, 0 };

    if (!gIODisplayWrangler)
        waitForService(serviceMatching("IODisplayWrangler"), &timeout);

    if (gIODisplayWrangler)
    {
        gIODisplayWrangler->fOpen = true;
        if (gIODisplayWrangler->fMinutesToDim)
            gIODisplayWrangler->setIdleTimerPeriod(gIODisplayWrangler->fMinutesToDim*60 / 2);
        gIODisplayWrangler->activityTickle(0, 0);
    }

    return (gIODisplayWrangler != 0);
}

bool IODisplayWrangler::start( IOService * provider )
{
    AbsoluteTime        current_time;
    UInt64              current_time_ns;
    OSObject *  notify;

    if (!super::start(provider))
        return (false);

    assert( gIODisplayWrangler == 0 );

    setProperty(kIOUserClientClassKey, "IOAccelerationUserClient");

    fMatchingLock = IOLockAlloc();
    fFramebuffers = OSSet::withCapacity( 1 );
    fDisplays = OSSet::withCapacity( 1 );

        AbsoluteTime_to_scalar(&current_time) = mach_absolute_time();
    absolutetime_to_nanoseconds(current_time, &current_time_ns);
    fLastWakeTime_secs = current_time_ns / NSEC_PER_SEC;
    fLastDimTime_secs = 0;
    
    fAnnoyanceEventArrayLength = staticAnnoyanceEventArrayLength;
    fAnnoyanceEventArray = staticAnnoyanceEventArray;
    fAnnoyanceEventArrayQHead = 0;

    fAnnoyanceCapsArrayLength = staticAnnoyanceCapsArrayLength;
    fAnnoyanceCapsArray = staticAnnoyanceCapsArray;

    fAnnoyancePenaltiesArrayLength = staticAnnoyancePenaltiesArrayLength;
    fAnnoyancePenaltiesArray = staticAnnoyancePenaltiesArray;

        fIdleTimeoutMin = 30; // 30 seconds
        fIdleTimeoutMax = 600; // 10 minutes

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
            // Display will always dim at least 5 seconds before
            // display sleep kicks in.
            fIdleTimeoutMax = fMinutesToDim * 60 - 5;        
            fMinutesToDim = newLevel;
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

        // Set new timeout        
        setIdleTimerPeriod( newLevel*60 / 2);

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

    DEBG1("W", " (%ld), pwr %d open %d\n", 
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
            // Log time of initial dimming
            UInt64 current_time_ns;
            UInt64 current_time_secs;

            AbsoluteTime current_time_absolute;
            AbsoluteTime_to_scalar(&current_time_absolute) = mach_absolute_time();
            absolutetime_to_nanoseconds(current_time_absolute, &current_time_ns);
            current_time_secs = current_time_ns / NSEC_PER_SEC;
            fLastDimTime_secs = current_time_secs;
        }
        IOFramebuffer::updateDisplaysPowerState();
    }
    else if (powerStateOrdinal == kIODisplayWranglerMaxPowerState)
    {
        // there is activity, raise power
        IOFramebuffer::updateDisplaysPowerState();
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
//   is adaptive and adjusts, starting out fairly aggressively and backing 
//   off depending on how frequently the user cancels dimming.
// - Transition from 3->2 (to full display sleep on all machines)
//   will occur at exactly N minutes from last user activity, where N
//   is the value chosen by the user and set via setAggressiveness().

SInt32 IODisplayWrangler::nextIdleTimeout(
    AbsoluteTime currentTime,
    AbsoluteTime lastActivity, 
    unsigned int powerState)
{
    UInt64 lastActivity_ns;
    UInt64 lastActivity_secs;
    UInt64 current_time_ns;
    UInt64 current_time_secs;
    SInt32 delay_till_time_secs;    
    SInt32 timeout_used_for_dim;
    SInt32 delay_secs;

    absolutetime_to_nanoseconds(currentTime, &current_time_ns);
    current_time_secs = current_time_ns / NSEC_PER_SEC;

    absolutetime_to_nanoseconds(lastActivity, &lastActivity_ns);
    lastActivity_secs = lastActivity_ns / NSEC_PER_SEC;

    if (!lastActivity_secs)
    {
        enum { kWindowServerStartTime = 24 * 60 * 60 };
        return (kWindowServerStartTime);
    }

    switch( getPowerState() ) {
        case 4:
            // System displays are ON, not dimmed or asleep.
            // Calculate adaptive time-to-dim
            delay_till_time_secs = 
                calculate_earliest_time_idle_timeout_allowed(
                    current_time_secs, lastActivity_secs, powerState);
            if ( delay_till_time_secs > (SInt32)current_time_secs )
            {
                if(  (int)(delay_till_time_secs - (SInt32)lastActivity_secs) > 
                     (int)(fMinutesToDim*60 - 5) )
                {
                    // backoff pushed dim time too high, beyond user's selected
                    // display sleep timeout. So we cap it, with a 5 second
                    // threshold for good measure. i.e. there will always
                    // be at least 5 seconds of dim between full on and display
                    // sleep.
                    delay_till_time_secs = (SInt32)lastActivity_secs
                                         + (SInt32)(fMinutesToDim*60 - 5);
                }
                
                // Will time out in the future.
                delay_secs = (SInt32)(delay_till_time_secs - current_time_secs);
            } else {
                // There are no vetos in effect. Use standard
                // idle timeout period.
                SInt32 period = calculate_idle_timer_period(powerState);            
                delay_secs = (SInt32)(
                    (UInt64)lastActivity_secs 
                    + (UInt64)period
                    - (UInt64)current_time_secs);
            }
     
            break;
        case 3:
            // The system is currently in its 'dim' state
            // The transition into the next 'display sleep' state must occur
            // fMinutesToDim after last UI activity
            timeout_used_for_dim = (SInt32)((SInt64)fLastDimTime_secs -
                                        (SInt64)lastActivity_secs);
            //IOLog("Display Wrangler state(3) last dim took %d\n", 
            //                    timeout_used_for_dim);
            delay_secs = (SInt32)fMinutesToDim*60 
                        - timeout_used_for_dim;
            break;
        case 2:
        case 1:
            delay_secs = fMinutesToDim*30;
            break;
        case 0:
            delay_secs = 60;
            break;
        default:
            // error
            delay_secs = 60;
            break;
    }

//    IOLog("Display Wrangler state(%d) recommending idle sleep in %ld\n",
//           powerState,  delay_secs);
    return delay_secs;
}


//*****************************************************************************
// calculate_idle_timer_period
//
// Called from inside nextIdleTimeout().
// Return value is in seconds.
//*****************************************************************************
UInt32 IODisplayWrangler::calculate_idle_timer_period(int powerState)
{
    UInt32 idle_timer_period;

    // Bright to dim quickly, dim to dark less quickly.
    // We only return a value for state 4. The idle timeout
    // period of state 3 is automatically determined by the caller
    // as a consequence of how long state 4 took. Other state
    // timeouts do not affect user experience once the display is off
    // after transitioning state 3->2.
    if ( 4 == powerState )
    {
                if ( gCOMPRESS_TIME )
        {
            idle_timer_period = fMinutesToDim * 60 / 12;
        }
        else
        {
            idle_timer_period = fMinutesToDim * 60 / 5;
        }
        
        // Clip it into the range fIdleTimeoutMin seconds thru fIdleTimeoutMax
        if ( idle_timer_period < fIdleTimeoutMin )
        {
                idle_timer_period = fIdleTimeoutMin;
        }
        else
        if ( idle_timer_period > fIdleTimeoutMax )
        {
                idle_timer_period = fIdleTimeoutMax;
        }
    }
    else
    {
        // Should never be called with a powerState other than 4,
        // return a sane value anyway.
        idle_timer_period = fMinutesToDim * 30;
    }

    return idle_timer_period;
} // IODisplayWrangler::calculate_idle_timer_period()



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// activityTickle
//
// This is called by the HID system and calls the superclass in turn.

bool IODisplayWrangler::activityTickle( unsigned long x, unsigned long y )
{
    AbsoluteTime current_time_absolute;

    if (!fOpen)
        return (true);

        AbsoluteTime_to_scalar(&current_time_absolute) = mach_absolute_time();
    if (AbsoluteTime_to_scalar(&fIdleUntil))
    {
        if (CMP_ABSOLUTETIME(&current_time_absolute, &fIdleUntil) < 0)
            return (true);
        AbsoluteTime_to_scalar(&fIdleUntil) = 0;
    }

    if (super::activityTickle(kIOPMSuperclassPolicy1,
                kIODisplayWranglerMaxPowerState) )
    {
        return (true);
    }

    // Get uptime in nanoseconds
    UInt64 current_time_ns;
    absolutetime_to_nanoseconds(current_time_absolute, &current_time_ns);
    UInt64 current_time_secs = current_time_ns / NSEC_PER_SEC;

    fLastWakeTime_secs = current_time_secs;

    // Record this if it was an annoyance.
    record_if_annoyance();

    getPMRootDomain()->wakeFromDoze();
    
    return (false);
} // IODisplayWrangler::activityTickle()


void IODisplayWrangler::record_if_annoyance() // implicit parameters: fLastDimTime_secs, fLastWakeTime_secs
{
    // Determine if it is an annoyance, i.e., if it has a penalty.

    UInt64 delta_secs = fLastWakeTime_secs - fLastDimTime_secs;

    UInt32 penalty = calculate_penalty( delta_secs );

    if ( penalty > 0 )
    {
        enqueueAnnoyance( fLastDimTime_secs, fLastWakeTime_secs, penalty );
    }
} // IODisplayWrangler::record_if_annoyance()


UInt32 IODisplayWrangler::calculate_penalty( UInt32 time_between_dim_and_wake_secs )
{
    UInt32 penalty = 0;

    for (int i = 0; i < fAnnoyancePenaltiesArrayLength; i++)
    {
        if ( time_between_dim_and_wake_secs <= fAnnoyancePenaltiesArray[ i ].time_secs )
        {
            penalty = fAnnoyancePenaltiesArray[ i ].penalty_points;
            break;
        }
    }
    return penalty;
} // IODisplayWrangler::calculate_penalty()


// In: seconds.
// Returns: seconds.
UInt64 IODisplayWrangler::calculate_latest_veto_till_time( UInt64 current_time_secs )
{
    int total = 0;
    UInt64 latest_veto_till_time_secs = 0;
    UInt64 rolling_wake_time_secs = 0;
    int i = 0, j = 0;

    while ( j < fAnnoyanceCapsArrayLength )
    {
        while (         ( i < fAnnoyanceEventArrayLength )
                &&      ( 0 != getNthAnnoyance( i )->wake_time_secs ) 
                &&      ( (current_time_secs - getNthAnnoyance(i)->wake_time_secs) 
                    < (UInt64)fAnnoyanceCapsArray[ j ].cutoff_time_secs )
              )
        {
            rolling_wake_time_secs = getNthAnnoyance( i )->wake_time_secs;
            total += getNthAnnoyance( i )->penalty;
            i ++;
        }

        if ( total >= fAnnoyanceCapsArray[ j ].cutoff_points )
        {
            // Assert: if we get here, total>0 so we're guaranteed that 
            // rolling_wake_time_secs has been assigned a value,
            // otherwise total would still be zero.
            // (This also requires that we don't have an 
            // absurd cutoff_points==0)
        
            // Since the time at which the annoyance occurred is variable, 
            // we need to calculate the time in the future at which
            // this veto will expire before testing whether it's further
            // in the future than he previously

            UInt64 veto_till_time_secs = 
                (UInt64)fAnnoyanceCapsArray[ j ].cutoff_time_secs 
                + rolling_wake_time_secs;
                
            if ( veto_till_time_secs > latest_veto_till_time_secs )
            {
                latest_veto_till_time_secs = veto_till_time_secs;
            }
        }

        j ++;

    } // while()

    return latest_veto_till_time_secs;

} // IODisplayWrangler::calculate_latest_veto_till_time()

// In: seconds.
// Returns: seconds.
UInt64 IODisplayWrangler::calculate_earliest_time_idle_timeout_allowed( 
    UInt64 current_time_secs,
    UInt64 last_activity_secs,
    int    powerState)
{
    // Should only be called while DisplayWrangler is in state 4

    // Potentially overridden in a subclass to determine the correct 
    // idle timer period.
    UInt32 idle_timer_period_secs = calculate_idle_timer_period(powerState);

    // The moment at which we would want the idle timer to go off 
    // (might be in the past).
    UInt64 idle_timer_delay_till_time_secs = 
          last_activity_secs
        + (UInt64) idle_timer_period_secs;

    // The moment at which the longest veto expires (might be in the past)
    UInt64 latest_veto_till_time_secs = 
        calculate_latest_veto_till_time( current_time_secs );


    // Take the larger of the two (might be in the past).    
    UInt64 delay_till_time_secs;

    if ( idle_timer_delay_till_time_secs > latest_veto_till_time_secs )
    {
        delay_till_time_secs = idle_timer_delay_till_time_secs;
    }
    else
    {
        delay_till_time_secs = latest_veto_till_time_secs;
    }

    return delay_till_time_secs;

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

    OSObject * value;

    value = dict->getObject( "COMPRESS_TIME" );
    if ( value )
    { // COMPRESS_TIME
        OSNumber * number;
        number = OSDynamicCast( OSNumber, value );
        if ( number )
        {
            gCOMPRESS_TIME = number->unsigned32BitValue();

            this->setProperty( "COMPRESS_TIME", number );
        }
    } // COMPRESS_TIME

    value = dict->getObject( kIODisplayWrangler_AnnoyancePenalties );
    if ( value )
    { // PENALTIES
    
        int penaltiesArrayLength;
        OSArray * penaltiesArray;
    
        penaltiesArray = OSDynamicCast( OSArray, value );
        if ( ! penaltiesArray )
        {
            goto Return;
        }
    
        penaltiesArrayLength = penaltiesArray->getCount();
        
        if ( ! ( penaltiesArrayLength <= fAnnoyancePenaltiesArrayLength ) )
        {
            goto Return;
        }
        
        for (int i = 0; i < penaltiesArrayLength; i++)
        {
            value = penaltiesArray->getObject( i );
    
            OSArray * penalty_time_and_points_pair;
    
            penalty_time_and_points_pair = OSDynamicCast( OSArray, value );
            if ( ! penalty_time_and_points_pair )
            {
                goto Return;
            }
    
            if ( 2 != penalty_time_and_points_pair->getCount() )
            {
                goto Return;
            }
            
            OSObject * p0, * p1;
            
            p0 = penalty_time_and_points_pair->getObject( 0 );
            p1 = penalty_time_and_points_pair->getObject( 1 );
    
            OSNumber * n0, * n1;
            
            n0 = OSDynamicCast( OSNumber, p0 );
            if ( ! n0 )
            {
                goto Return;
            }
    
            n1 = OSDynamicCast( OSNumber, p1 );
            if ( ! n1 )
            {
                goto Return;
            }
            
            int time_secs = n0->unsigned32BitValue();
            
            int penalty_points = n1->unsigned32BitValue();
    
            fAnnoyancePenaltiesArray[ i ].time_secs = time_secs;
            fAnnoyancePenaltiesArray[ i ].penalty_points = penalty_points;
                    
        }
        
        
        this->setProperty( kIODisplayWrangler_AnnoyancePenalties, penaltiesArray );
        
    } // PENALTIES
    
    value = dict->getObject( kIODisplayWrangler_AnnoyanceCaps );
    if ( value )
    { // CAPS
    
        int capsArrayLength;
        OSArray * capsArray;
    
        capsArray = OSDynamicCast( OSArray, value );
        if ( ! capsArray )
        {
            goto Return;
        }
    
        capsArrayLength = capsArray->getCount();
        
        if ( ! ( capsArrayLength <= fAnnoyanceCapsArrayLength ) )
        {
            goto Return;
        }
        
        for (int i = 0; i < capsArrayLength; i++)
        {
            value = capsArray->getObject( i );
    
            OSArray * cap_time_and_points_pair;
    
            cap_time_and_points_pair = OSDynamicCast( OSArray, value );
            if ( ! cap_time_and_points_pair )
            {
                goto Return;
            }
    
            if ( 2 != cap_time_and_points_pair->getCount() )
            {
                goto Return;
            }
            
            OSObject * p0, * p1;
            
            p0 = cap_time_and_points_pair->getObject( 0 );
            p1 = cap_time_and_points_pair->getObject( 1 );
    
            OSNumber * n0, * n1;
            
            n0 = OSDynamicCast( OSNumber, p0 );
            if ( ! n0 )
            {
                goto Return;
            }
    
            n1 = OSDynamicCast( OSNumber, p1 );
            if ( ! n1 )
            {
                goto Return;
            }
            
            int cutoff_time_secs = n0->unsigned32BitValue();
            
            int cutoff_points = n1->unsigned32BitValue();
            
            fAnnoyanceCapsArray[ i ].cutoff_time_secs = cutoff_time_secs;
            fAnnoyanceCapsArray[ i ].cutoff_points = cutoff_points;
            
        }

        this->setProperty( kIODisplayWrangler_AnnoyanceCaps, capsArray );
        
    } // CAPS

    value = dict->getObject( kIODisplayWrangler_IdleTimeoutMin );
    if ( value )
    { // IdleTimeoutMin
        OSNumber * number;
        number = OSDynamicCast( OSNumber, value );
        if ( number )
        {
            fIdleTimeoutMin = number->unsigned32BitValue();
    
            this->setProperty( kIODisplayWrangler_IdleTimeoutMin, number );
        }
    } // IdleTimeoutMin

    value = dict->getObject( kIODisplayWrangler_IdleTimeoutMax );
    if ( value )
    { // IdleTimeoutMax
        OSNumber * number;
        number = OSDynamicCast( OSNumber, value );
        if ( number )
        {
            fIdleTimeoutMax = number->unsigned32BitValue();
    
            this->setProperty( kIODisplayWrangler_IdleTimeoutMax, number );
        }
    } // IdleTimeoutMax

    // The new values may change the timeout we calculate.
    
    start_PM_idle_timer();

    goto Return;

Return:
    return kIOReturnSuccess;
}
