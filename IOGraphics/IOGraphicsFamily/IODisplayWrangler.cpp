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
#include "IODisplayWranglerUserClients.hpp"

#include "IOGraphicsKTrace.h"
#include "GMetric.hpp"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

namespace {

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

GTraceBuffer::shared_type sIODWGTrace;
IODisplayWrangler *sIODisplayWrangler;

}; // namespace


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService

OSDefineMetaClassAndStructors(IODisplayConnect, IOService)

bool IODisplayConnect::initWithConnection(
    const uint64_t fbRegID, const IOIndex _connection)
{
    IODC_START(initWithConnection,0,0,0);
    char        name[ 12 ];

    if (!super::init())
    {
        IODC_END(initWithConnection,false,0,0);
        return (false);
    }

    fConnectIndex = _connection;
    fFBRegID = fbRegID;

    snprintf( name, sizeof(name), "display%d", (int) fConnectIndex);

    setName( name);

    IODC_END(initWithConnection,true,0,0);
    return (true);
}

void IODisplayConnect::recordGTraceToken(
        const uint16_t line,
        const uint16_t tag0, const uint64_t fbRegID,
        const uint16_t tag1, const uint64_t arg1,
        const uint16_t tag2, const uint64_t arg2,
        const uint16_t tag3, const uint64_t arg3)
{
    sIODWGTrace->recordToken(line, tag0, fbRegID, tag1, arg1,
                             tag2, arg2, tag3, arg3);
}

void IODisplayConnect::recordGTraceToken(
        const uint16_t line,
        const uint16_t fnID, const uint8_t  fnType,
        const uint16_t tag1, const uint64_t arg1,
        const uint16_t tag2, const uint64_t arg2,
        const uint16_t tag3, const uint64_t arg3)
{
    recordGTraceToken(line, GTFuncTag(fnID, fnType, 0), getFBRegistryID(),
                      tag1, arg1, tag2, arg2, tag3, arg3);
}

IOFramebuffer * IODisplayConnect::getFramebuffer( void )
{
    IODC_START(initWithConnection,0,0,0);
    IOFramebuffer * fb = (IOFramebuffer *) getProvider();
    IODC_END(initWithConnection,0,0,0);
    return (fb);
}

IOIndex IODisplayConnect::getConnection( void )
{
    IODC_START(initWithConnection,0,0,0);
    IODC_END(initWithConnection,0,0,0);
    return (fConnectIndex);
}

IOReturn IODisplayConnect::getAttributeForConnection( IOSelect selector, uintptr_t * value )
{
    IODC_START(initWithConnection,selector,0,0);
    if (!getProvider())
    {
        IODC_END(initWithConnection,kIOReturnNotReady,0,0);
        return (kIOReturnNotReady);
    }

    FB_START(getAttributeForConnection,selector,__LINE__,0);
    IOReturn err = ((IOFramebuffer *) getProvider())->getAttributeForConnection(fConnectIndex, selector, value);
    FB_END(getAttributeForConnection,err,__LINE__,0);
    IODC_END(initWithConnection,err,0,0);
    return (err);
}

IOReturn  IODisplayConnect::setAttributeForConnection( IOSelect selector, uintptr_t value )
{
    IODC_START(initWithConnection,selector,value,0);
    if (!getProvider())
    {
        IODC_END(initWithConnection,kIOReturnNotReady,0,0);
        return (kIOReturnNotReady);
    }
    FB_START(setAttributeForConnection,selector,__LINE__,value);
    IOReturn err = ((IOFramebuffer *) getProvider())->setAttributeForConnection(fConnectIndex,  selector, value);
    FB_END(setAttributeForConnection,err,__LINE__,0);
    IODC_END(initWithConnection,err,0,0);
    return (err);
}

// joinPMtree
//
// The policy-maker in the display driver calls here when initializing.
// We attach it into the power management hierarchy as a child of our
// frame buffer.

void IODisplayConnect::joinPMtree ( IOService * driver )
{
    IODC_START(initWithConnection,0,0,0);
    getProvider()->addPowerChild(driver);
    IODC_END(initWithConnection,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService
OSDefineMetaClassAndStructors(IODisplayWrangler, IOService);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IODisplayWrangler::serverStart(void)
{
    IODW_START(serverStart,0,0,0);
    mach_timespec_t timeout = { 120, 0 };

    if (!sIODisplayWrangler)
        waitForService(serviceMatching("IODisplayWrangler"), &timeout);

    if (!sIODisplayWrangler) IOLog("IODisplayWrangler not started, IOResources hung\n");
    else
    {
        sIODisplayWrangler->fOpen = true;
        if (sIODisplayWrangler->fMinutesToDim)
            sIODisplayWrangler->setIdleTimerPeriod(60);
        sIODisplayWrangler->activityTickle(0, 0);
    }

    IODW_END(serverStart,sIODisplayWrangler != 0,0,0);
    return (sIODisplayWrangler != 0);
}

bool IODisplayWrangler::start( IOService * provider )
{
    assert(!static_cast<bool>(sIODWGTrace));
    assert( sIODisplayWrangler == 0 );
    IODW_START(start,0,0,0);

    if (!super::start(provider))
    {
        IODW_END(start,false,0,0);
        return (false);
    }

    // Mostly used by the AppleBacklight and IODisplay instrumentation
    if (!static_cast<bool>(sIODWGTrace))
        sIODWGTrace = GTraceBuffer::make(
                "abldecoder", "IOGraphicsFamily", kGTraceMinimumLineCount,
                /* breadcrumb func */ NULL, /* breadcrumb ctxt */ NULL);

    clock_interval_to_absolutetime_interval(kDimInterval, kSecondScale, &fDimInterval);

    sIODisplayWrangler = this;

    // initialize power managment
    sIODisplayWrangler->initForPM();
    getPMRootDomain()->publishFeature("AdaptiveDimming");

    IODW_END(start,true,0,0);
    return (true);
}


bool IODisplayWrangler::makeDisplayConnects( IOFramebuffer * fb )
{
    IODW_START(makeDisplayConnects,0,0,0);
    IODisplayConnect *  connect;
    IOItemCount         i;

    const uint64_t fbRegID = fb->getRegistryEntryID();
    for (i = 0; i < 1 /*fb->getConnectionCount()*/; i++)
    {
        connect = new IODisplayConnect;
        if (!static_cast<bool>(connect))
            continue;

        if (connect->initWithConnection(fbRegID, i)
        && connect->attach(fb))
            connect->registerService(kIOServiceSynchronous);
        connect->release();
    }

    IODW_END(makeDisplayConnects,true,0,0);
    return (true);
}

void IODisplayWrangler::destroyDisplayConnects( IOFramebuffer * fb )
{
    IODW_START(destroyDisplayConnects,0,0,0);
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
                    display->PMstop();
                }
                connect->terminate( kIOServiceSynchronous );
            }
        }
        iter->release();
    }
    IODW_END(destroyDisplayConnects,0,0,0);
}

void IODisplayWrangler::activityChange( IOFramebuffer * fb )
{
    IODW_START(activityChange,0,0,0);
    DEBG1("W", " activityChange\n");
    sIODisplayWrangler->activityTickle(0,0);
    IODW_END(activityChange,0,0,0);
}

IODisplayConnect * IODisplayWrangler::getDisplayConnect(
    IOFramebuffer * fb, IOIndex connectIndex )
{
    IODW_START(getDisplayConnect,connectIndex,0,0);
    OSIterator  *       iter;
    OSObject    *       next;
    IODisplayConnect *  connect = 0;

    iter = fb->getClientIterator();
    if (iter)
    {
        while ((next = iter->getNextObject()))
        {
            connect = OSDynamicCast( IODisplayConnect, next);
            if (connect)
            {
                if (connect->isInactive())
                    continue;
                if (0 == (connectIndex--))
                    break;
            }
        }
        iter->release();
    }
    IODW_END(getDisplayConnect,0,0,0);
    return (connect);
}

IOReturn IODisplayWrangler::getConnectFlagsForDisplayMode(
    IODisplayConnect * connect,
    IODisplayModeID mode, UInt32 * flags )
{
    IODW_START(getConnectFlagsForDisplayMode,mode,0,0);
    IOReturn            err = kIOReturnUnsupported;
    IODisplay *         display;

    display = OSDynamicCast( IODisplay, connect->getClient());
    if (display)
        err = display->getConnectFlagsForDisplayMode( mode, flags );
    else
    {
        FB_START(connectFlags,mode,__LINE__,0);
        err = connect->getFramebuffer()->connectFlags(
                  connect->getConnection(), mode, flags );
        FB_END(connectFlags,err,__LINE__,*flags);
    }

    IODW_END(getConnectFlagsForDisplayMode,err,0,0);
    return (err);
}

IOReturn IODisplayWrangler::getFlagsForDisplayMode(
    IOFramebuffer * fb,
    IODisplayModeID mode, UInt32 * flags )
{
    IODW_START(getFlagsForDisplayMode,mode,0,0);
    IOReturn            err;
    IODisplayConnect    * connect;

    do {
    // should look at all connections
    connect = sIODisplayWrangler->getDisplayConnect( fb, 0 );
    if (!connect)
    {
        FB_START(connectFlags,mode,__LINE__,0);
        err = (fb->connectFlags(0, mode, flags));
        FB_END(connectFlags,err,__LINE__,*flags);
        break;
    }

    err = (sIODisplayWrangler->
            getConnectFlagsForDisplayMode(connect, mode, flags));
    } while(0);

    IODW_END(getFlagsForDisplayMode,err,0,0);
    return (err);
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
    IODW_START(initForPM,0,0,0);

    // WAR for rdar://47370368: DW shouldn't participate in power events on
    // j129 HW pending a replacement.  It screws with HID (and maybe others)
    // during sleep/wake.
#if !TARGET_OS_ARROW
    // initialize superclass variables
    PMinit();

    // attach into the power management hierarchy
    joinPMtree( this );

    // register ourselves with policy-maker (us)
    registerPowerDriver( this, ourPowerStates, kIODisplayWranglerNumPowerStates );
    makeUsable();
#endif

    // HID system is waiting for this
    registerService();
    IODW_END(initForPM,0,0,0);
}

unsigned long IODisplayWrangler::initialPowerStateForDomainState( IOPMPowerFlags domainState )
{
    IODW_START(initialPowerStateForDomainState,domainState,0,0);
    unsigned long   ret = 0;

    if (domainState & IOPMPowerOn)
        ret = (kIODisplayWranglerMaxPowerState);

    IODW_END(initialPowerStateForDomainState,ret,0,0);
    return (ret);
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
    IODW_START(setAggressiveness,type,newLevel,0);
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
            fMinutesToDim = static_cast<UInt32>(newLevel);
			clock_interval_to_absolutetime_interval(fMinutesToDim * 60, kSecondScale, &fOffInterval[0]);

			uint32_t annoyedInterval = fMinutesToDim * 60 * 2;
			if (annoyedInterval > kMaxAnnoyanceInterval) annoyedInterval = kMaxAnnoyanceInterval;
			if (annoyedInterval < (fMinutesToDim * 60))  annoyedInterval = (fMinutesToDim * 60);
			clock_interval_to_absolutetime_interval(annoyedInterval, kSecondScale, &fOffInterval[1]);
			AbsoluteTime_to_scalar(&fSettingsChanged) = mach_absolute_time();

			DEBG2("W", " fMinutesToDim %ld, annoyed %d\n", newLevel, annoyedInterval);
        }

        newLevel = fDimCaptured ? 0 : fMinutesToDim;

        IOG_KTRACE_NT(DBG_IOG_SET_TIMER_PERIOD, DBG_FUNC_NONE,
                      DBG_IOG_SOURCE_SET_AGGRESSIVENESS, newLevel * 30, 0, 0);
        setIdleTimerPeriod(newLevel * 30);
        break;

      default:
        break;
    }
    super::setAggressiveness(type, newLevel);

    IODW_END(setAggressiveness,IOPMNoErr,0,0);
    return (IOPMNoErr);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// setPowerState
//
// The vanilla policy-maker in the superclass is changing our power state.
// If it's down, inform the displays to lower one state, too.  If it's up,
// the idle displays are made usable.

#define RECORD_METRIC(func, domain) \
    GMETRICFUNC(func, DBG_FUNC_NONE, (domain)|kGMETRICS_DOMAIN_DISPLAYWRANGLER)
#define RECORD_METRIC_POWER(func, domain) \
    RECORD_METRIC(func, (domain) | kGMETRICS_DOMAIN_POWER);

IOReturn IODisplayWrangler::setPowerState( unsigned long powerStateOrdinal, IOService * whatDevice )
{
    // Single threaded by IOServicePM design
    IOG_KTRACE_LOG_SYNCH(DBG_IOG_LOG_SYNCH);

    RECORD_METRIC_POWER(DBG_IOG_SET_POWER_STATE,
                        powerStateOrdinal < getPowerState()
                            ? kGMETRICS_DOMAIN_SLEEP : kGMETRICS_DOMAIN_WAKE);
    IOG_KTRACE(DBG_IOG_SET_POWER_STATE, DBG_FUNC_NONE,
               0, powerStateOrdinal,
               0, DBG_IOG_SOURCE_IODISPLAYWRANGLER,
               0, 0,
               0, 0);
    IODW_START(setPowerState,powerStateOrdinal,0,0);

    fPendingPowerState = powerStateOrdinal;

	fPowerStateChangeTime = mach_absolute_time();

    DEBG2("W", " (%ld), pwr %d open %d\n", 
                powerStateOrdinal, gIOGraphicsSystemPower, fOpen);

    if (powerStateOrdinal == 0)
    {
        // system is going to sleep
        // keep displays off on wake till UI brings them up

        RECORD_METRIC_POWER(DBG_IOG_CHANGE_POWER_STATE_PRIV,
                            kGMETRICS_DOMAIN_SLEEP);
        IOG_KTRACE(DBG_IOG_CHANGE_POWER_STATE_PRIV, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_IODISPLAYWRANGLER,
                   0, 0,
                   0, 0,
                   0, 0);

        changePowerStateToPriv(0);
        IODW_END(setPowerState,IOPMNoErr,0,0);
        return (IOPMNoErr);
    }

    if (!gIOGraphicsSystemPower || !fOpen)
    {
        IODW_END(setPowerState,IOPMNoErr,0,0);
        return (IOPMNoErr);
    }
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

    IODW_END(setPowerState,IOPMNoErr,0,0);
    return (IOPMNoErr);
}

unsigned long IODisplayWrangler::getDisplaysPowerState(void)
{
    IODW_START(getDisplaysPowerState,0,0,0);
    unsigned long state = sIODisplayWrangler
                                ? sIODisplayWrangler->fPendingPowerState
                                : kIODisplayWranglerMaxPowerState;
    IODW_END(getDisplaysPowerState,state,0,0);
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
    IODW_START(nextIdleTimeout,currentTime,lastActivity,powerState);
	AbsoluteTime deadline;
	uint64_t delayNS = 0;
	SInt32   delaySecs = 0;

    if (!fOpen)
    {
        enum { kWindowServerStartTime = 24 * 60 * 60 };
        IODW_END(nextIdleTimeout,kWindowServerStartTime,0,0);
        return (kWindowServerStartTime);
    }

	if (CMP_ABSOLUTETIME(&fSettingsChanged, &lastActivity) > 0) lastActivity = fSettingsChanged;

    switch (fPendingPowerState) 
    {
        case 4:
            // The system is currently in its 'on' state
            // The transition into the next 'display sleep' state must occur
            // fMinutesToDim after last UI activity
			deadline = lastActivity;
			ADD_ABSOLUTETIME(&deadline, &fOffInterval[fAnnoyed]);
			//if (4 == fPendingPowerState) 
			SUB_ABSOLUTETIME(&deadline, &fDimInterval);
			if (CMP_ABSOLUTETIME(&deadline, &currentTime) > 0)
			{
				SUB_ABSOLUTETIME(&deadline, &currentTime);
				absolutetime_to_nanoseconds(deadline, &delayNS);
				delaySecs = static_cast<SInt32>(delayNS / kSecondScale);
			}
            break;

        case 3:
            // The system is currently in its 'dim' state
			deadline = fPowerStateChangeTime;
			ADD_ABSOLUTETIME(&deadline, &fDimInterval);
			if (CMP_ABSOLUTETIME(&deadline, &currentTime) > 0)
			{
				SUB_ABSOLUTETIME(&deadline, &currentTime);
				absolutetime_to_nanoseconds(deadline, &delayNS);
				delaySecs = static_cast<SInt32>(delayNS / kSecondScale);
            } else {
                RECORD_METRIC_POWER(DBG_IOG_CHANGE_POWER_STATE_PRIV,
                                    kGMETRICS_DOMAIN_WAKE);
                IOG_KTRACE(DBG_IOG_CHANGE_POWER_STATE_PRIV, DBG_FUNC_NONE,
                           0, DBG_IOG_SOURCE_IODISPLAYWRANGLER,
                           0, 2,
                           0, 0,
                           0, 0);
                changePowerStateToPriv(2);
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
	
    IODW_END(nextIdleTimeout,delaySecs,0,0);
    return (delaySecs);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// activityTickle
//
// This is called by the HID system and calls the superclass in turn.

bool IODisplayWrangler::activityTickle( unsigned long x, unsigned long y )
{
    IODW_START(activityTickle,x,y,0);
    AbsoluteTime now;

    if (!fOpen)
    {
        IODW_END(activityTickle,true,0,0);
        return (true);
    }


    AbsoluteTime_to_scalar(&now) = mach_absolute_time();
    if (AbsoluteTime_to_scalar(&fIdleUntil))
    {
        if (CMP_ABSOLUTETIME(&now, &fIdleUntil) < 0)
        {
            IODW_END(activityTickle,true,0,0);
            return (true);
        }
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
        IODW_END(activityTickle,true,0,0);
        return (true);
    }

    RECORD_METRIC(DBG_IOG_WAKE_FROM_DOZE, kGMETRICS_DOMAIN_DOZE);
    IOG_KTRACE(DBG_IOG_WAKE_FROM_DOZE, DBG_FUNC_NONE,
               0, x,
               0, y,
               0, 0,
               0, 0);

    getPMRootDomain()->wakeFromDoze();

    IODW_END(activityTickle,false,0,0);
    return (false);
}

OSObject * IODisplayWrangler::copyProperty( const char * aKey ) const
{
    IODW_START(copyProperty,0,0,0);

    OSObject * obj = NULL;
    const bool userPrefs = !strcmp(aKey, kIOGraphicsPrefsKey);

    if (userPrefs)
    {
        obj = IOFramebuffer::copyPreferences();
    }
    else
    {
        obj = super::copyProperty(aKey);
    }

    IODW_END(copyProperty,userPrefs,0,0);
    return (obj);
}

// Called from IOFramebuffer
/* static */ void IODisplayWrangler::forceIdle()
{
    if (sIODisplayWrangler)
        sIODisplayWrangler->forceIdleImpl();
}

void IODisplayWrangler::forceIdleImpl()
{
    // Force idle for idleFor time
    const auto currentPower = getPowerState();
    uint64_t procNameInt[4] = { 0 };
    auto * const procName = GPACKSTRINGCAST(procNameInt);
    proc_selfname(procName, sizeof(procNameInt));
    D(DIM, "W", " forceIdle by process '%s' from %d\n", procName, currentPower);
    IOLog("DW forceIdle by process '%s' from %d\n", procName, currentPower);

    if (currentPower > 1) {
        RECORD_METRIC_POWER(DBG_IOG_CHANGE_POWER_STATE_PRIV,
                            kGMETRICS_DOMAIN_SLEEP);
        IOG_KTRACE(DBG_IOG_CHANGE_POWER_STATE_PRIV, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_IODISPLAYWRANGLER,
                   0, 1,
                   kGTRACE_ARGUMENT_STRING, procNameInt[0],
                   kGTRACE_ARGUMENT_STRING, procNameInt[1]);

        changePowerStateToPriv(1);
    }
}

IOReturn IODisplayWrangler::setProperties( OSObject * properties )
{
    IODW_START(setProperties,0,0,0);
    IOReturn        err;
    OSDictionary * dict;
    OSDictionary * prefs;
    OSObject *     obj;
    OSNumber *     num;
    enum { kIODisplayRequestDefaultIdleFor =  1000,
           kIODisplayRequestMaxIdleFor     = 15000, };

    if (!(dict = OSDynamicCast(OSDictionary, properties)))
    {
        IODW_END(setProperties,kIOReturnBadArgument,0,0);
        return (kIOReturnBadArgument);
    }

    if ((prefs = OSDynamicCast(OSDictionary,
                              dict->getObject(kIOGraphicsPrefsKey))))
    {
        err = IOFramebuffer::setPreferences(this, prefs);
        IODW_END(setProperties,err,0,0);
        return (err);
    }


    uint32_t idleFor = 0;
    obj = dict->getObject(kIORequestIdleKey);
    if (kOSBooleanTrue == obj) {
        idleFor = kIODisplayRequestDefaultIdleFor;
    }
    else if (kOSBooleanFalse == obj) {
        // Clear idle and wake everything up
        AbsoluteTime_to_scalar(&fIdleUntil) = 0;
        activityTickle(0, 0);
    }
    else if ((num = OSDynamicCast(OSNumber, obj))) {
        idleFor = num->unsigned32BitValue();
        if (idleFor > kIODisplayRequestMaxIdleFor)
            idleFor = kIODisplayRequestMaxIdleFor;
    }

    if (idleFor) {
        clock_interval_to_deadline(idleFor, kMillisecondScale, &fIdleUntil);
        DEBG1("W", " requested idleFor %dms\n", idleFor);
        forceIdleImpl();
    } else {
        DEBG1("W", " Unknown or zero idle property value, no forceIdle\n");
        IOLog("DW Unknown or zero idle property value, no forceIdle\n");
    }

    IODW_END(setProperties,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

namespace {
static const OSSymbol *
sIODWBuiltinPanelPowerKey = OSSymbol::withCString("AAPL,builtin-panel-power");
}

/* static */ void
IODisplayWrangler::builtinPanelPowerNotify(bool state)
{
    IODisplayWrangler *w = sIODisplayWrangler;
    if (!sIODWBuiltinPanelPowerKey || !w) return;
    OSBoolean *b = OSDynamicCast(OSBoolean,
        w->getProperty(sIODWBuiltinPanelPowerKey));
    if (!b || (b->getValue() != state)) {
        w->setProperty(sIODWBuiltinPanelPowerKey, OSBoolean::withBoolean(state));
        IODWBuiltinPanelPower_t details = {0};
        details.version = IODW_BUILTIN_PANEL_POWER_VERSION,
        details.mct = mach_continuous_time();
        details.state = state;
        w->messageClients(kIOMessageServicePropertyChange,
            &details, sizeof(details));
    }
}
