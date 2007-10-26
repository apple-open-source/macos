/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 30-Jan-03 ebold created
 *
 */

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <syslog.h>
#include "PrivateLib.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"

enum {
    kIOWakeTimer = 0,
    kIOPowerOnTimer = 1,
    kIOSleepTimer = 2,
    kIOShutdownTimer = 3
};

#if TARGET_OS_EMBEDDED
#define MIN_SCHEDULE_TIME   (5.0)
#else
#define MIN_SCHEDULE_TIME   (10.0)
#endif

typedef void (*powerEventCallout)(CFDictionaryRef);

/* 
 * We use one PowerEventBehavior struct per-type of schedule power event 
 * sleep/wake/power/shutdown/wakeORpower/restart.
 * The struct contains special behavior per-type.
 */
struct PowerEventBehavior {
    // These values change to reflect the state of current 
    // and upcoming power events
    CFMutableArrayRef       array;
    CFDictionaryRef         currentEvent;
    CFRunLoopTimerRef       timer;

    CFStringRef             title;

    // wake and poweron sharedEvents pointer points to wakeorpoweron struct
    struct PowerEventBehavior      *sharedEvents;

    // Callouts will be defined at startup time and not modified after that
    powerEventCallout       timerExpirationCallout;    
    powerEventCallout       scheduleNextCallout;    
};
typedef struct PowerEventBehavior PowerEventBehavior;

/*
 * Global structs tracking behaviors & current state
 */
PowerEventBehavior          sleepBehavior;
PowerEventBehavior          shutdownBehavior;
PowerEventBehavior          restartBehavior;
PowerEventBehavior          wakeBehavior;
PowerEventBehavior          poweronBehavior;
PowerEventBehavior          wakeorpoweronBehavior;

enum {
    kBehaviorsCount = 6
};

/*
 * Stick pointers to them in an array for safekeeping
 */
PowerEventBehavior *behaviors[] =
{
    &sleepBehavior,
    &shutdownBehavior,
    &restartBehavior,
    &wakeBehavior,
    &poweronBehavior,
    &wakeorpoweronBehavior
};

/*
 * forwards
 */
static bool             isEntryValidAndFuturistic(CFDictionaryRef, CFDateRef);
static void             schedulePowerEvent(PowerEventBehavior *);
static bool             purgePastEvents(PowerEventBehavior *);
static void             copyScheduledPowerChangeArrays(void);
static CFDictionaryRef  getEarliestUpcoming(PowerEventBehavior *);
static kern_return_t    openHIDService(io_connect_t *);
static void             wakeDozingMachine(void);
static bool             isRepeating(CFDictionaryRef);
static CFDateRef        _getScheduledEventDate(CFDictionaryRef);
static CFArrayRef       copyMergedEventArray(PowerEventBehavior *, 
                                             PowerEventBehavior *);
static CFComparisonResult compareEvDates(CFDictionaryRef, 
                                             CFDictionaryRef, void *);

void poweronScheduleCallout(CFDictionaryRef);
void wakeScheduleCallout(CFDictionaryRef);

void wakeTimerExpiredCallout(CFDictionaryRef);
void poweronTimerExpiredCallout(CFDictionaryRef);
void sleepTimerExpiredCallout(CFDictionaryRef);
void shutdownTimerExpiredCallout(CFDictionaryRef);
void restartTimerExpiredCallout(CFDictionaryRef);

/* AutoWakeScheduler overview
 * 
 * PURPOSE
 * Set wake and power on timers to automatically power on the machine at a 
 * user/app requested time. 
 * Requests come via IOKit/pwr_mgt/IOPMLib.h:IOPMSchedulePowerEvent()
 *
 * On pre-2004 machines this is the PMU's responsibility.
 * The SMU has taken over the AutoWake/AutoPower role on some post-2004 machines.
 * The IOPMrootDomain kernel entity routes all requests to the appropriate
 * controller.
 *
 * POWER ON
 * Every time we set a power on time in the PMU, start a CFTimer to fire at the same time.
 *    (in scheduleShutdownTime())
 * If the CFTimer fires, then the machine was not powered off and we should find the 
 *    next power on date and schedule that. (in handleTimerPowerOnReset())
 * If the machine is powered off, the CFTimer won't fire and the PMU will power the machine.
 *
 * WAKE
 * Wake is simpler than power on, since we get a notification on the way to sleep.
 * At going-to-sleep time we scan the wake_arr CFArray for the next upcoming
 * wakeup time (in AutoWakeSleepWakeNotification())
 *
 * LOADING NEW AUTOWAKEUP TIMES
 * Via SCPreferences notifications
 *
 * PURGING OLD TIMES
 * Done only at boot and wakeup times. We have to write the file back to disk (if it changed).
 * So to minimize disk access we only purge when we think the disk is "up" anyway.
 */


__private_extern__ void 
AutoWake_prime(void) 
{
    PowerEventBehavior      *this_behavior;
    int                     i;

    // clear out behavior structs for good measure
    for(i=0; i<kBehaviorsCount; i++) 
    {
        this_behavior = behaviors[i];
        bzero(this_behavior, sizeof(PowerEventBehavior));
    }

    wakeBehavior.title                      = CFSTR(kIOPMAutoWake);
    poweronBehavior.title                   = CFSTR(kIOPMAutoPowerOn);
    wakeorpoweronBehavior.title             = CFSTR(kIOPMAutoWakeOrPowerOn);
    sleepBehavior.title                     = CFSTR(kIOPMAutoSleep);
    shutdownBehavior.title                  = CFSTR(kIOPMAutoShutdown);
    restartBehavior.title                   = CFSTR(kIOPMAutoRestart);
    
    // Initialize powerevent callouts per-behavior    
    // note: wakeorpoweronBehavior does not have callouts of its own, by design
    wakeBehavior.timerExpirationCallout     = wakeTimerExpiredCallout;
    poweronBehavior.timerExpirationCallout  = poweronTimerExpiredCallout;
    sleepBehavior.timerExpirationCallout    = sleepTimerExpiredCallout;
    shutdownBehavior.timerExpirationCallout = shutdownTimerExpiredCallout;
    restartBehavior.timerExpirationCallout  = restartTimerExpiredCallout;
    
    // schedulePowerEvent callouts
    wakeBehavior.scheduleNextCallout        = wakeScheduleCallout;
    poweronBehavior.scheduleNextCallout     = poweronScheduleCallout;

    // Use this "sharedEvents" linkage to later merge wakeorpoweron events 
    // from wakeorpoweron into runtime wake and poweron queues. 
    wakeBehavior.sharedEvents = 
            poweronBehavior.sharedEvents = &wakeorpoweronBehavior;


    // system bootup; read prefs from disk
    copyScheduledPowerChangeArrays();
    
    for(i=0; i<kBehaviorsCount; i++) 
    {
        this_behavior = behaviors[i];
        if(!this_behavior) continue;

        // purge past wakeup and restart times
        purgePastEvents(this_behavior);

        // schedule next power changes
        schedulePowerEvent(this_behavior);
    }

    return;
}

/*
 * Sleep/wake
 *
 */

__private_extern__ void 
AutoWakeSleepWakeNotification(natural_t message_type) 
{
    PowerEventBehavior  *this_behavior = NULL;
    int i;

    switch (message_type) {
        case kIOMessageSystemWillSleep:
            // scheduleWakeupTime
            schedulePowerEvent(&wakeBehavior);
            break;

        case kIOMessageSystemHasPoweredOn:
            // scan for past-wakeup events, yank 'em from the queue
            for(i=0; i<kBehaviorsCount; i++) 
            {
                this_behavior = behaviors[i];
                if(!this_behavior) continue;
        
                purgePastEvents(this_behavior);
                
                schedulePowerEvent(this_behavior);
            }
            break;

        default:
            // don't care about CanSystemSleep
            break;
    }
}

/*
 * Prefs file changed on disk
 *
 */

__private_extern__ void 
AutoWakePrefsHaveChanged(void) 
{
    PowerEventBehavior      *this_behavior = NULL;
    int                     i;

    copyScheduledPowerChangeArrays();
    
    // set AutoWake=0 in case our scheduled wakeup was just cancelled
    IOPMSchedulePowerEvent(
            NULL,   NULL,
            CFSTR(kIOPMAutoWakeScheduleImmediate) );

    // set AutoPower=0 in case our scheduled wakeup was just cancelled
    IOPMSchedulePowerEvent(
            NULL,   NULL,
            CFSTR(kIOPMAutoPowerScheduleImmediate) );


    // Call schedulePowerEvent per-behavior
    for(i=0; i<kBehaviorsCount; i++) 
    {
        this_behavior = behaviors[i];
        if(!this_behavior) continue;

        schedulePowerEvent(this_behavior);
    }
}


/*
 * Required behaviors at timer expiration:
 *
 * on wake:
 *   - system is obviously already awake if we're alive to receive this message.
 *     post a NULL HID event to wake display.
 * on poweron:
 *   - system is obviously already on if we're alive to receive this message.
 *     schedule the next poweron event in the queue.
 * on sleep:
 *   - ask loginwindow to put up UI warning user of impending sleep
 * on shutdown:
 *   - ask loginwindow to put up UI warning user of impending shutdown
 */

static void
handleTimerExpiration(CFRunLoopTimerRef blah, void *info)
{
    PowerEventBehavior  *behave = (PowerEventBehavior *)info;

    if(!behave) return;
    
    behave->timer = 0;
        
    if( behave->timerExpirationCallout ) {
        (*behave->timerExpirationCallout)(behave->currentEvent);
    }
    
    if(isRepeating(behave->currentEvent)) {
        RepeatingAutoWakeRepeatingEventOcurred(behave->currentEvent);
    }

    // free and NULL currentEvent????
    
    return;
}    

/*
 * Required behaviors at event scheduling time:
 *
 * on wake and/or poweron:
 *   - transmit expected wake/on time to underlying hardware that will wake
 *     the system when appropriate, be it PMU, SMU, SMC, or beyond.
 *
 */

static void
schedulePowerEvent(PowerEventBehavior *behave)
{
    static CFRunLoopTimerContext    tmr_context = {0,0,0,0,0};
    CFAbsoluteTime                  fire_time = 0.0;
    CFDictionaryRef                 upcoming = NULL;
    CFDateRef                       temp_date = NULL;

    if(behave->timer) 
    {
       CFRunLoopTimerInvalidate(behave->timer);
       behave->timer = 0;
    }
    
    // find upcoming time
    upcoming = getEarliestUpcoming(behave);
    if(!upcoming)
    {
        // No scheduled events
        return;
    }
        
    /* 
     * Perform any necessary actions at schedulePowerEvent time 
     */
    if ( behave->scheduleNextCallout ) {
        (*behave->scheduleNextCallout)(upcoming);    
    }    

    behave->currentEvent = (CFDictionaryRef)upcoming;
    tmr_context.info = (void *)behave;    
    
    temp_date = _getScheduledEventDate(upcoming);
    if(!temp_date) goto exit;

    fire_time = CFDateGetAbsoluteTime(temp_date);

    behave->timer = CFRunLoopTimerCreate(0, fire_time, 0.0, 0, 
                    0, handleTimerExpiration, &tmr_context);

    if(behave->timer)
    {
        CFRunLoopAddTimer( CFRunLoopGetCurrent(), 
                            behave->timer, 
                            kCFRunLoopDefaultMode);
        CFRelease(behave->timer);
    }

exit:
    return;
}


/******************************************************************************
 ******************************************************************************
 * Event type-specific callouts
 ******************************************************************************
 ******************************************************************************/

/*
 * poweron
 */
void poweronScheduleCallout(CFDictionaryRef event)
{    
    IOPMSchedulePowerEvent( _getScheduledEventDate(event),
                NULL, CFSTR(kIOPMAutoPowerScheduleImmediate) );
}

void poweronTimerExpiredCallout(CFDictionaryRef event __unused)
{
    schedulePowerEvent(&poweronBehavior);
}


/*
 * wake
 */
void wakeScheduleCallout(CFDictionaryRef event)
{
    IOPMSchedulePowerEvent( _getScheduledEventDate(event),
                NULL, CFSTR(kIOPMAutoWakeScheduleImmediate) );
}

void wakeTimerExpiredCallout(CFDictionaryRef event __unused)
{
    wakeDozingMachine();
}


/*
 * sleep
 */
void sleepTimerExpiredCallout(CFDictionaryRef event __unused)
{
    _askNicelyThenSleepSystem();
}

/*
 * shutdown
 */
void shutdownTimerExpiredCallout(CFDictionaryRef event __unused)
{
    _askNicelyThenShutdownSystem();
}

/*
 * restart
 */
void restartTimerExpiredCallout(CFDictionaryRef event __unused)
{
    _askNicelyThenRestartSystem();
}


/******************************************************************************
 ******************************************************************************
 * Utility functions from here on out
 ******************************************************************************
 ******************************************************************************/

/*
 *
 * isEntryValidAndFuturistic
 * Returns true if the CFDictionary is validly formed
 *     AND if the date is in the future
 * Returns false if anything about the dictionary is invalid
 *     OR if the CFDate is prior to the current time
 *
 */
static bool 
isEntryValidAndFuturistic(CFDictionaryRef wakeup_dict, CFDateRef date_now)
{
    CFDateRef           wakeup_date;
    bool                ret = true;

    wakeup_dict = isA_CFDictionary(wakeup_dict);
    if(!wakeup_dict) 
    {
        // bogus entry!
        ret = false;
    } else 
    {
        // valid entry    
        wakeup_date = isA_CFDate(CFDictionaryGetValue(wakeup_dict, 
                                        CFSTR(kIOPMPowerEventTimeKey)));
        if( !wakeup_date 
            || (kCFCompareLessThan == CFDateCompare(wakeup_date, date_now, 0)))
        {
            // date is too early
            ret = false;
        }
        // otherwise date is after now, and ret = true
    }

    return ret;
}

/*
 *
 * Purge past wakeup times
 * Does not care whether its operating on wakeup or poweron array.
 * Just purges all entries with a time < now
 * returns true on success, false on any failure
 *
 */
static bool 
purgePastEvents(PowerEventBehavior  *behave)
{
    bool                array_has_changed = false;
    CFDateRef           date_now;
    bool                ret;
    SCPreferencesRef    prefs = 0;
    
    if( !behave 
        || !behave->title
        || !behave->array
        || (0 == CFArrayGetCount(behave->array)))
    {
        return true;
    }
    
    date_now = CFDateCreate(0, CFAbsoluteTimeGetCurrent());

    // Loop over the array and remove any values that are in the past.
    // Since array is sorted by date already, we stop once we reach an event
    // scheduled in the future.
    // Do not try to optimize the CFArrayGetCount out of the while loop; this value may
    // change during loop execution.
    while( (0 < CFArrayGetCount(behave->array))
        && !isEntryValidAndFuturistic(CFArrayGetValueAtIndex(behave->array, 0), date_now))
    {
        // Remove entry from the array - its time has past
        // The rest of the array will shift down to fill index 0
        CFArrayRemoveValueAtIndex(behave->array, 0);
        array_has_changed = true;
    }

    CFRelease(date_now);


    if(array_has_changed)
    {
        // write new, purged array to disk
        // We should soon-after get a prefs notification to re-read the values
        prefs = SCPreferencesCreate(0, CFSTR("IOKit-AutoWake"), 
                                CFSTR(kIOPMAutoWakePrefsPath));
        if(!prefs) return 0;
        if(!SCPreferencesLock(prefs, true)) {
            ret = false;
            goto exit;
        }
        SCPreferencesSetValue(prefs, behave->title, behave->array);
        if(!SCPreferencesCommitChanges(prefs)) {
            ret = false;
            goto exit;
        }        
        SCPreferencesUnlock(prefs);
    }
    ret = true;
exit:
    if(prefs) CFRelease(prefs);
    return ret;
}

/*
 *
 * copySchedulePowerChangeArrays
 *
 */
static void
copyScheduledPowerChangeArrays(void)
{
    CFArrayRef              tmp;
    SCPreferencesRef        prefs;
    PowerEventBehavior      *this_behavior;
    int                     i;
   
    prefs = SCPreferencesCreate(0, 
                                CFSTR("PM-configd-AutoWake"),
                                CFSTR(kIOPMAutoWakePrefsPath));
    if(!prefs) return;

    // Loop through all sleep, wake, shutdown powerbehaviors
    for(i=0; i<kBehaviorsCount; i++) 
    {
        this_behavior = behaviors[i];

        if(this_behavior->array) {
            CFRelease(this_behavior->array);
            this_behavior->array = NULL;
        }

        tmp = isA_CFArray(SCPreferencesGetValue(prefs, this_behavior->title));
        if(tmp && (0 < CFArrayGetCount(tmp))) {
            this_behavior->array = CFArrayCreateMutableCopy(0, 0, tmp);
        } else {
            this_behavior->array = NULL;
        }
    }

    CFRelease(prefs);
}

/*
 *
 * Find earliest upcoming wakeup time
 *
 */
static CFDictionaryRef 
getEarliestUpcoming(PowerEventBehavior *b)
{
    CFArrayRef              arr = NULL;
    CFDateRef               now = NULL;
    CFDictionaryRef         the_result = NULL;
    int                     i, count;

    if(!b) return NULL;

    // wake and poweron types get merged with wakeorpoweron array
    if(b->sharedEvents) {

        // musst release arr later
        arr = copyMergedEventArray(b, b->sharedEvents);

    } else {
        arr = b->array;
    }

    // If the array is NULL, we have no work to do.
    if(!arr || (0 == CFArrayGetCount(arr))) goto exit;
    
    count = CFArrayGetCount(arr);
    if(0 == count) goto exit;
    
    now = CFDateCreate(0, CFAbsoluteTimeGetCurrent() + MIN_SCHEDULE_TIME);

    // iterate through all past entries, stopping at one occurring  
    // >MIN_SCHEDULE_TIME seconds in the future, or at the end of the array
    i = 0;
    while( (i < count)
        && !isEntryValidAndFuturistic(CFArrayGetValueAtIndex(arr, i), now) )
    {
        i++;
    }
    CFRelease(now);

    if(i < count) 
    {
        the_result = CFArrayGetValueAtIndex(arr, i);
    }
    
exit:
    if(arr && b->sharedEvents) CFRelease(arr);
    
    return the_result;
}

/*
 *
 * comapareEvDates() - internal sorting helper for copyMergedEventArray()
 *
 */
 static CFComparisonResult 
compareEvDates(
    CFDictionaryRef a1, 
    CFDictionaryRef a2,
    void *c __unused)
{
    CFDateRef   d1, d2;
    a1 = isA_CFDictionary(a1);
    a2 = isA_CFDictionary(a2);
    if(!a1) return kCFCompareGreaterThan;
    else if(!a2) return kCFCompareLessThan;
    
    d1 = isA_CFDate(CFDictionaryGetValue(a1, CFSTR(kIOPMPowerEventTimeKey)));
    d2 = isA_CFDate(CFDictionaryGetValue(a2, CFSTR(kIOPMPowerEventTimeKey)));
    if(!d1) return kCFCompareGreaterThan;
    else if(!d2) return kCFCompareLessThan;
    
    return CFDateCompare(d1, d2, 0);
}

/*
 *
 * copyMergedEventArray
 *
 * Takes two PowerEventBehavior*, merges their CFArray array members into one 
 * mutable array, sorted by date.
 */
static CFArrayRef       
copyMergedEventArray(
    PowerEventBehavior *a, 
    PowerEventBehavior *b)
{
    CFMutableArrayRef       merged;
    int                     bcount;
    CFRange                 rng;

    if(!a || !b) return NULL;
    if(!a->array && !b->array) return NULL;
    if(!a->array) return CFRetain(b->array);
    if(!b->array) return CFRetain(a->array);

    // merge!
    merged = CFArrayCreateMutableCopy(0, 0, a->array);
    
    bcount = CFArrayGetCount(b->array);
    rng =  CFRangeMake(0, bcount);
    CFArrayAppendArray(merged, b->array, rng);

    // sort!
    // We sort using the same compare_dates function used in IOKitUser
    // pwr_mgt/IOPMAutoWake.c. Arrays must be sorted identically to how
    // they would be there.
    bcount = CFArrayGetCount(merged);
    rng =  CFRangeMake(0, bcount);
    CFArraySortValues(merged, rng, (CFComparatorFunction)compareEvDates, 0);

    // caller must release
    return merged;
}


/*
 *
 * Find HID service. Only used by wakeDozingMachine
 *
 */
#if HAVE_HID_SYSTEM
static kern_return_t openHIDService(io_connect_t *connection)
{
    kern_return_t       kr;
    io_service_t        service;
    io_connect_t        hid_connect = MACH_PORT_NULL;
    
    service = IOServiceGetMatchingService(MACH_PORT_NULL, 
                                IOServiceMatching(kIOHIDSystemClass));
    if (MACH_PORT_NULL == service) {
        return kr;
    }

    kr = IOServiceOpen( service, mach_task_self(), 
                        kIOHIDParamConnectType, &hid_connect);    

    IOObjectRelease(service);

    if (kr != KERN_SUCCESS) {
        return kr;
    }
    
    *connection = hid_connect;
    return kr;
}
#endif /* HAVE_HID_SYSTEM */

/*
 *
 * Wakes a dozing machine by posting a NULL HID event
 * Will thus also wake displays on a running machine running
 *
 */
static void wakeDozingMachine(void)
{
#if HAVE_HID_SYSTEM
    IOGPoint loc;
    kern_return_t kr;
    NXEvent nullEvent = {NX_NULLEVENT, {0, 0}, 0, -1, 0};
    static io_connect_t io_connection = MACH_PORT_NULL;

    // If the HID service has never been opened, do it now
    if (io_connection == MACH_PORT_NULL) 
    {
        kr = openHIDService(&io_connection);
        if (kr != KERN_SUCCESS) 
        {
            io_connection = MACH_PORT_NULL;
            return;
        }
    }
    
    // Finally, post a NULL event
    IOHIDPostEvent( io_connection, NX_NULLEVENT, loc, 
                    &nullEvent.data, FALSE, 0, FALSE );
#endif /* HAVE_HID_SYSTEM */

    return;
}

static bool 
isRepeating(CFDictionaryRef event)
{
    CFStringRef     whose = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventAppNameKey));
 
    if( whose && CFEqual(whose, CFSTR("Repeating")) )
        return true;   
    else
        return false;
}

static CFDateRef
_getScheduledEventDate(CFDictionaryRef event)
{
    return isA_CFDate(CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTimeKey)));
}

#if __ppc__
/* Provide legacy support for pre-Panther clients of the private PMU 
 * setProperties API In the OS X 10.2 era, several applications 
 * (both third party and internal) are directly using the
 * ApplePMU::setProperty("AutoWake", time) private API to 
 * schedule PMU AutoWakes.
 *
 * To continue to support these apps while also providing a more reliable 
 * queued AutoWake functionality, we're re-directing those old requests 
 * made directly to the PMU through the IOPMSchedulePowerEvent()
 * queueing API.
 *
 * The ApplePMU kext provides the notification that we're acting on here.
 */
__private_extern__ void 
AutoWakePMUInterestNotification(natural_t messageType, UInt32 messageArgument)
{    
    // Handle old calls to schedule AutoWakes and AutoPowers by funneling them 
    // up through the queueing API.
    // UInt32 messageArgument is a difference relative to the current time.
    if(0 == messageArgument) return;
    
    if(kIOPMUMessageLegacyAutoWake == messageType)
    {
        CFDateRef           wakey_date;
        CFAbsoluteTime      wakey_time;
        wakey_time = CFAbsoluteTimeGetCurrent() 
                            + (CFAbsoluteTime)messageArgument;
        wakey_date = CFDateCreate(kCFAllocatorDefault, wakey_time);
        IOPMSchedulePowerEvent(
                        wakey_date, 
                        CFSTR("Legacy PMU setProperties"), 
                        CFSTR(kIOPMAutoWake));
        CFRelease(wakey_date);
    } else if(kIOPMUMessageLegacyAutoPower == messageType)
    {
        CFDateRef           wakey_date;
        CFAbsoluteTime      wakey_time;
        wakey_time = CFAbsoluteTimeGetCurrent() 
                            + (CFAbsoluteTime)messageArgument;
        wakey_date = CFDateCreate(kCFAllocatorDefault, wakey_time);
        IOPMSchedulePowerEvent(
                        wakey_date, 
                        CFSTR("Legacy PMU setProperties"), 
                        CFSTR(kIOPMAutoPowerOn));
        CFRelease(wakey_date);
    }
    
}
#endif /* __ppc__ */
