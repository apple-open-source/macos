/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
/*
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 30-Jan-03 ebold created
 *
 */

#include <syslog.h>
#include <bsm/libbsm.h>
#include "PrivateLib.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"

enum {
    kIOWakeTimer = 0,
    kIOPowerOnTimer = 1,
    kIOSleepTimer = 2,
    kIOShutdownTimer = 3
};

enum {                                                                                                                                         
    kIOPMMaxScheduledEntries = 1000                                                                                                            
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
    powerEventCallout       noScheduledEventCallout;
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

static uint32_t     activeEventCnt = 0;
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
static CFDictionaryRef  copyEarliestUpcoming(PowerEventBehavior *);
static bool             isRepeating(CFDictionaryRef);
static CFDateRef        _getScheduledEventDate(CFDictionaryRef);
static CFArrayRef       copyMergedEventArray(PowerEventBehavior *, 
                                             PowerEventBehavior *);
static CFComparisonResult compareEvDates(CFDictionaryRef, 
                                             CFDictionaryRef, void *);

void poweronScheduleCallout(CFDictionaryRef);
void wakeScheduleCallout(CFDictionaryRef);

void wakeNoScheduledEventCallout(CFDictionaryRef);

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
 * We schedule requests to the responsible kernel driver by calling setProperties on IOPMrootDomain.
 * The IOPMrootDomain kernel entity routes all requests to the appropriate
 * controller.
 *
 * POWER ON
 * Every time we set a power on time, we also start a software timer to fire at the same time.
 *    (in scheduleShutdownTime())
 * If the software timer fires, then the machine was not powered off and we should find the 
 *    next power on date and schedule that. (in handleTimerPowerOnReset())
 * If the machine is powered off, the software timer won't fire and the timer hardware will power the machine.
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
 * In memory events with old timestamo gets purged at boot, at wakeup and when new event is added. 
 * But, we don't update on-disk contents unless a new event is being added or existing event is deleted
 * thru IOKit.
 * So to minimize disk access we only purge when we think the disk is "up" anyway.
 */
#pragma mark -
#pragma mark AutoWakeScheduler

/*
 * Deletes events with specific appName in the given behavior array.
 *
 * The event array pointer gets modified. 
 */
void
removeEventsByAppName(PowerEventBehavior *behave, CFStringRef appName)
{
    int                 count, j;
    CFDictionaryRef     cancelee = 0;


    if ( (behave->array == NULL) || 
            (count = CFArrayGetCount(behave->array)) == 0)
        return;
    for (j = count-1; j >= 0; j--)
    {
        cancelee = CFArrayGetValueAtIndex(behave->array, j);
        if( CFEqual(
            CFDictionaryGetValue(cancelee, CFSTR(kIOPMPowerEventAppNameKey)), appName
            ))
        {
            // This is the one to cancel
            if (behave->currentEvent && CFEqual(cancelee, behave->currentEvent)) {
                CFRelease(behave->currentEvent);
                behave->currentEvent = NULL;
            }

            CFArrayRemoveValueAtIndex(behave->array, j);
            activeEventCnt--;
        }
    }

}


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
    poweronBehavior.scheduleNextCallout     = poweronScheduleCallout;
    poweronBehavior.noScheduledEventCallout = poweronScheduleCallout;

    // Use this "sharedEvents" linkage to later merge wakeorpoweron events 
    // from wakeorpoweron into runtime wake and poweron queues. 
    wakeBehavior.sharedEvents = 
            poweronBehavior.sharedEvents = &wakeorpoweronBehavior;


    // system bootup; read prefs from disk
    copyScheduledPowerChangeArrays();
    
    RepeatingAutoWake_prime();

    for(i=0; i<kBehaviorsCount; i++) 
    {
        this_behavior = behaviors[i];
        if(!this_behavior) continue;

        // purge past wakeup and restart times
        purgePastEvents(this_behavior);

        // purge any repeat events in these arrays.
        // Repeat events were saved into these arrays on disk previously
        // We don't do it anymore
        removeEventsByAppName(this_behavior, CFSTR(kIOPMRepeatingAppName));

        // schedule next power changes
        if (!CFEqual(this_behavior->title, CFSTR(kIOPMAutoWakeOrPowerOn)))
           schedulePowerEvent(this_behavior);
    }

    return;
}

/*
 * Sleep/wake
 *
 */

__private_extern__ void AutoWakeCapabilitiesNotification(
    IOPMSystemPowerStateCapabilities old_cap, 
    IOPMSystemPowerStateCapabilities new_cap)
{
    int i;
    
    if (CAPABILITY_BIT_CHANGED(new_cap, old_cap, kIOPMSystemPowerStateCapabilityCPU)) 
    {
        if (BIT_IS_SET(new_cap, kIOPMSystemPowerStateCapabilityCPU)) 
        {
            // scan for past-wakeup events, yank 'em from the queue
            for(i=0; i<kBehaviorsCount; i++) 
            {
                if(behaviors[i]) {
                    purgePastEvents(behaviors[i]);
                    
                    if (!CFEqual(behaviors[i]->title, CFSTR(kIOPMAutoWakeOrPowerOn)))
                       schedulePowerEvent(behaviors[i]);
                }
            }
        } else {
            // Going to sleep
            schedulePowerEvent(&wakeBehavior);
        }
        
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
    
    if (behave->timer) {
        CFRelease(behave->timer);
        behave->timer = 0;
    }
        
    if( behave->timerExpirationCallout ) {
        (*behave->timerExpirationCallout)(behave->currentEvent);
    }
    
    if (behave->currentEvent)
       CFRelease(behave->currentEvent);
    behave->currentEvent = NULL;

    // Schedule the next event
    schedulePowerEvent(behave);
    
    return;
}    

/*
 * Required behaviors at event scheduling time:
 *
 * on wake and/or poweron:
 *   - transmit expected wake/on time to underlying hardware that will wake
 *     the system when appropriate.
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
       CFRelease(behave->timer);
       behave->timer = 0;
    }
    
    // find upcoming time
    upcoming = copyEarliestUpcoming(behave);
    if(!upcoming)
    {
        // No scheduled events
        if (behave->noScheduledEventCallout) {
            (*behave->noScheduledEventCallout)(NULL);
        }
        return;
    }
        
    /* 
     * Perform any necessary actions at schedulePowerEvent time 
     */
    if ( behave->scheduleNextCallout ) {
        (*behave->scheduleNextCallout)(upcoming);    
    }    

    if (behave->currentEvent) {
        CFRelease(behave->currentEvent);
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
    }

exit:
    return;
}

__private_extern__ void
schedulePowerEventType(CFStringRef type)
{
    int i;

    if (CFEqual(type, CFSTR(kIOPMAutoWakeOrPowerOn))) {
        /*
         * If this is a 'WakeOrPowerOn' event, schedule
         * this for both wakeBehavior & poweronBehavior
         */
        schedulePowerEvent(&wakeBehavior);
        schedulePowerEvent(&poweronBehavior);
        return;
    }

    for(i=0; i<kBehaviorsCount; i++) {
        if (CFEqual(type, behaviors[i]->title))
            break;
    }
    if (i >= kBehaviorsCount) {
        return;
    }

    schedulePowerEvent(behaviors[i]);
}

__private_extern__ CFTimeInterval getEarliestRequestAutoWake(void)
{
    CFDictionaryRef     one_event = NULL;
    CFDateRef           event_date = NULL;
    CFTimeInterval      absTime = 0.0;
    
    if (!(one_event = copyEarliestUpcoming(&wakeBehavior))) {
        return 0.0;
    }
    if (!(event_date = _getScheduledEventDate(one_event))) {
        return 0.0;
    }
    absTime = CFDateGetAbsoluteTime(event_date);
    CFRelease(one_event);
    return absTime;
}

/******************************************************************************
 ******************************************************************************
 * Event type-specific callouts
 ******************************************************************************
 ******************************************************************************/

/*
 * poweron
 */
#pragma mark -
#pragma mark PowerOn

void poweronScheduleCallout(CFDictionaryRef event)
{    
    IOPMSchedulePowerEvent(event ?  _getScheduledEventDate(event) : NULL,
                NULL, CFSTR(kIOPMAutoPowerScheduleImmediate) );
    return;
}

void poweronTimerExpiredCallout(CFDictionaryRef event __unused)
{
    // Do nothing
    return;
}


/*
 * wake
 */
#pragma mark -
#pragma mark Wake

void wakeTimerExpiredCallout(CFDictionaryRef event __unused)
{
    wakeDozingMachine();
}

/*
 * sleep
 */
#pragma mark -
#pragma mark Sleep

void sleepTimerExpiredCallout(CFDictionaryRef event __unused)
{
    _askNicelyThenSleepSystem();
}

/*
 * shutdown
 */
#pragma mark -
#pragma mark Shutdown

void shutdownTimerExpiredCallout(CFDictionaryRef event __unused)
{
    _askNicelyThenShutdownSystem();
}

/*
 * restart
 */
#pragma mark -
#pragma mark Restart

void restartTimerExpiredCallout(CFDictionaryRef event __unused)
{
    _askNicelyThenRestartSystem();
}


#pragma mark -
#pragma mark Utility

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
    CFDateRef           date_now;
    CFDictionaryRef     event;
    bool                ret;
    int                 i;

    
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
    while(0 < CFArrayGetCount(behave->array))
    {
        event = CFArrayGetValueAtIndex(behave->array, 0);
        if (isEntryValidAndFuturistic(event, date_now) )
                break;

        // Remove entry from the array - its time has past
        // The rest of the array will shift down to fill index 0
        CFArrayRemoveValueAtIndex(behave->array, 0);
        activeEventCnt--;
    }

    CFRelease(date_now);

    ret = true;

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

    activeEventCnt = 0;
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
            activeEventCnt += CFArrayGetCount(tmp);
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
 * For non-repeat events, a reference to event dictionary is provided.
 * For repeat events, a new event dictionary is created. Caller has to take
 * care to release that dictionary eventually.
 */
static CFDictionaryRef 
copyEarliestUpcoming(PowerEventBehavior *b)
{
    CFArrayRef              arr = NULL;
    CFDateRef               now = NULL;
    CFDictionaryRef         the_result = NULL;
    CFDictionaryRef         repeatEvent = NULL;
    int                     i, count;
    CFComparisonResult      eq;

    if(!b) return NULL;

    // wake and poweron types get merged with wakeorpoweron array
    if(b->sharedEvents) {

        // musst release arr later
        arr = copyMergedEventArray(b, b->sharedEvents);

    } else {
        arr = b->array;
    }

    // If the array is NULL, we have no work to do.
    if (arr && (count = CFArrayGetCount(arr)) != 0) {

        
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
            CFRetain(the_result);
        }
    }

    // Compare against the repeat event, if there is any
    repeatEvent = copyNextRepeatingEvent(b->title);
    if (repeatEvent)
    {
        eq = compareEvDates(repeatEvent, the_result, 0);
        if((kCFCompareLessThan == eq) || (kCFCompareEqualTo == eq))
        {
            //   repeatEvent <= the_result
            if (the_result) CFRelease(the_result);
            the_result = repeatEvent;
            // In this case, repeatEvent is released in the
            // event expiration handler
        }
        else
        {
            CFRelease(repeatEvent);
        }
    }
    
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




static bool 
isRepeating(CFDictionaryRef event)
{
    CFStringRef     whose = NULL;

    if (!isA_CFDictionary(event))
        return false;
    whose = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventAppNameKey));
 
    if( whose && CFEqual(whose, CFSTR(kIOPMRepeatingAppName)) )
        return true;   
    else
        return false;
}

static CFDateRef
_getScheduledEventDate(CFDictionaryRef event)
{
    return isA_CFDate(CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTimeKey)));
}



__private_extern__ IOReturn
createSCSession(SCPreferencesRef *prefs, uid_t euid, int lock)
{
    IOReturn ret = kIOReturnSuccess;

#if TARGET_OS_EMBEDDED
    *prefs = SCPreferencesCreate( 0, CFSTR("PM-configd-AutoWake"),
                                 CFSTR(kIOPMAutoWakePrefsPath));
#else
    if (euid == 0)
        *prefs = SCPreferencesCreate( 0, CFSTR("PM-configd-AutoWake"),
                                 CFSTR(kIOPMAutoWakePrefsPath));
    else
    {
        ret = kIOReturnNotPrivileged;
        goto exit;
    }

#endif 
    if(!(*prefs))
    {
        if(kSCStatusAccessError == SCError())
            ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }

    if (lock && !SCPreferencesLock(*prefs, true))
    {
        ret = kIOReturnError;
        goto exit;
    }


exit:
    return ret;
}

__private_extern__ void
destroySCSession(SCPreferencesRef prefs, int unlock)
{

    if (prefs) {
        if(unlock) SCPreferencesUnlock(prefs); 
        CFRelease(prefs);
    }
}

static void
addEvent(PowerEventBehavior  *behave, CFDictionaryRef event)
{
    if (isA_CFArray(behave->array)) {

        // First clear off any expired events
        purgePastEvents(behave);

        CFArrayAppendValue(behave->array, event);

        // XXX: Manual sorting is probably better than using CFArraySortValues()
        // Element is being added to already sorted array
        CFArraySortValues( 
                behave->array, CFRangeMake(0, CFArrayGetCount(behave->array)),
                (CFComparatorFunction)compareEvDates, 0);
    }
    else {
        behave->array = CFArrayCreateMutable(
                            0, 0, &kCFTypeArrayCallBacks); 
        CFArrayAppendValue(behave->array, event);
    }
    activeEventCnt++;

}


static IOReturn
updateToDisk(SCPreferencesRef prefs, PowerEventBehavior  *behavior, CFStringRef type)  
{
    IOReturn ret = kIOReturnSuccess;

    if(!SCPreferencesSetValue(prefs, type, behavior->array)) 
    {
        ret = kIOReturnError;
        goto exit;
    }

    // Add a warning to the file
    SCPreferencesSetValue(prefs, CFSTR("WARNING"), 
        CFSTR("Do not edit this file by hand. It must remain in sorted-by-date order."));
    //
    //  commit the SCPreferences file out to disk
    if(!SCPreferencesCommitChanges(prefs))
    {
        ret = kIOReturnError;
        goto exit;
    }
exit:
    return ret;
}


static bool
removeEvent(PowerEventBehavior  *behave, CFDictionaryRef event)   
{

    int                 count, i, j;
    CFComparisonResult  eq;
    CFDictionaryRef     cancelee = 0;

    if (!behave->array || !isA_CFArray(behave->array))
        return false;

    count = CFArrayGetCount(behave->array);
    for (i = 0; i < count; i++)
    {
        cancelee = CFArrayGetValueAtIndex(behave->array, i);
        eq = compareEvDates(event, cancelee, 0);
        if(kCFCompareLessThan == eq)
        {
            // fail, date to cancel < date at index
            break;
        }
        else if(kCFCompareEqualTo == eq)
        {
            // We have confirmation on the dates and types being equal. Check id.
            if( CFEqual(
                CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventAppNameKey)), 
                CFDictionaryGetValue(cancelee, CFSTR(kIOPMPowerEventAppNameKey))
                ))
            {
                // This is the one to cancel.
                // First check if cancelee is the current scheduled event
                // If so, delete currentEvent field. Caller will take care of 
                // re-scheduling the next event
                for (j = 0; j < kBehaviorsCount; j++)
                    if (behaviors[j]->currentEvent && CFEqual(cancelee, behaviors[j]->currentEvent)) {
                        CFRelease(behaviors[j]->currentEvent);
                        behaviors[j]->currentEvent = NULL;
                    }
                
                CFArrayRemoveValueAtIndex(behave->array, i);
                activeEventCnt--;
                return true;
            }
        }
    }
 
    return false;
}




/* MIG entry point to schedule a power event */
kern_return_t
_io_pm_schedule_power_event
( 
    mach_port_t             server __unused,
    audit_token_t           token,
    vm_offset_t             flatPackage,
    mach_msg_type_number_t  packageLen,
    int                     action,
    int                     *return_code
)
{

    CFDictionaryRef     event = NULL;
    CFDataRef           dataRef = NULL;
    CFStringRef         type = NULL;
    SCPreferencesRef    prefs = 0;
    uid_t               callerEUID;
    int                 i;

    *return_code = kIOReturnSuccess;

    audit_token_to_au32(token, NULL, &callerEUID, NULL, NULL, NULL, NULL, NULL, NULL);

    if (activeEventCnt >= kIOPMMaxScheduledEntries) {
        *return_code = kIOReturnNoSpace;
        goto exit;
    }

    dataRef = CFDataCreate(0, (const UInt8 *)flatPackage, packageLen);
    if (dataRef) {
        event = (CFDictionaryRef)CFPropertyListCreateWithData(0, dataRef, 0, NULL, NULL); 
    }

    if (!event) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    type = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTypeKey) );
    if (!type) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    for(i=0; i<kBehaviorsCount; i++) {
        if (CFEqual(type, behaviors[i]->title))
            break;
    }
    if (i >= kBehaviorsCount) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    //who = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventAppNameKey));

    //asl_log(0, 0, ASL_LEVEL_ERR, "Sched event type: %s by  %s\n", CFStringGetCStringPtr(type,kCFStringEncodingMacRoman ),
    //       CFStringGetCStringPtr( who, kCFStringEncodingMacRoman));

    if((*return_code = createSCSession(&prefs, callerEUID, 1)) != kIOReturnSuccess)
        goto exit;

    if (action == 1) {

        /* Add event to in-memory array */
        addEvent(behaviors[i], event);
        
        /* Commit changes to disk */
        if ((*return_code = updateToDisk(prefs, behaviors[i], type)) != kIOReturnSuccess) {
            removeEvent(behaviors[i], event);
            goto exit;
        }
    }
    else {
        /* Remove event from in-memory array */
        if (!removeEvent(behaviors[i], event)) {
            *return_code = kIOReturnNotFound;
            goto exit;
        }

        /* Update to disk. Ignore the failure; */
        updateToDisk(prefs, behaviors[i], type);
    }
    /* Schedule the power event */
    if (CFEqual(type, CFSTR(kIOPMAutoWakeOrPowerOn))) {
        /*
         * If this is a 'WakeOrPowerOn' event, schedule
         * this for both wakeBehavior & poweronBehavior
         */
        schedulePowerEvent(&wakeBehavior);
        schedulePowerEvent(&poweronBehavior);
    }
    else {
        schedulePowerEvent(behaviors[i]);
    }


exit:
    destroySCSession(prefs, 1);
    if (dataRef)
        CFRelease(dataRef);

    if (event)
        CFRelease(event);

    vm_deallocate(mach_task_self(), flatPackage, packageLen);

    return KERN_SUCCESS;
}

__private_extern__ CFArrayRef copyScheduledPowerEvents(void)
{

    CFMutableArrayRef       powerEvents = NULL;
    PowerEventBehavior      *this_behavior;
    int                     i, bcount;
    CFRange                 rng;

    powerEvents = CFArrayCreateMutable( 0, 0, &kCFTypeArrayCallBacks); 
    for(i=0; i<kBehaviorsCount; i++) {
        this_behavior = behaviors[i];

        if(this_behavior->array && isA_CFArray(this_behavior->array)) {
            bcount = CFArrayGetCount(this_behavior->array);
            rng =  CFRangeMake(0, bcount);
            CFArrayAppendArray(powerEvents, this_behavior->array, rng);
        }
    }

    return powerEvents;
}



