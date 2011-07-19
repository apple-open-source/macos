/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

#include <syslog.h>
#include <syslog.h>
#include "RepeatingAutoWake.h"
#include "PrivateLib.h"

/*
 * These are the days of the week as provided by
 * CFAbslouteTimeGetDayOfWeek()
 *

enum {
    kCFMonday = 1,
    kCFTuesday = 2,
    kCFWednesday = 3,
    kCFThursday = 4,
    kCFFriday = 5,
    kCFSaturday = 6,
    kCFSunday = 7
};

 *
 * In the "days of the week" bitmask, this is the key...
 *

enum {
    kBitMaskMonday = 0,
    kBitMaskTuesday = 1,
    kBitMaskWednesday = 2,
    kBitMaskThursday = 3,
    kBitMaskFriday = 4,
    kBitMaskSaturday = 5,
    kBitMaskSunday = 6
};

*/

static CFDictionaryRef  currentRepeatingEvents = 0;
static CFDictionaryRef  newRepeatingEvents = 0;
static CFDictionaryRef  repeatingPowerOff = 0;
static CFDictionaryRef  repeatingPowerOn = 0;


//****************************
// CANCEL PRE-SCHEDULED EVENTS
static IOReturn 
cancelAllRepeatingEvents(void)
{

    CFArrayRef              list = 0;
    CFDictionaryRef         scheduled_event;
    CFDateRef               scheduled_time;
    CFStringRef             scheduled_scheduler;
    CFStringRef             scheduled_type;
    IOReturn                ret = kIOReturnSuccess;
    int i, count;    

    list = IOPMCopyScheduledPowerEvents();
    if(!list || (0==CFArrayGetCount(list))) {
        ret = kIOReturnSuccess;
        goto exit;
    }

    // scan the list of power events and remove each one scheduled by "Repeating"
    count = CFArrayGetCount(list);
    for(i=0; i<count; i++)
    {
        scheduled_event = CFArrayGetValueAtIndex(list, i);
        if(!isA_CFDictionary(scheduled_event)) continue;
        scheduled_scheduler = isA_CFString(CFDictionaryGetValue(scheduled_event,
                                CFSTR(kIOPMPowerEventAppNameKey)));
        if( scheduled_scheduler 
            && CFEqual(scheduled_scheduler, CFSTR("Repeating")) )
        {
            scheduled_time = CFDictionaryGetValue( scheduled_event,
                                            CFSTR(kIOPMPowerEventTimeKey));
            scheduled_type = CFDictionaryGetValue( scheduled_event,     
                                            CFSTR(kIOPMPowerEventTypeKey));
            ret = IOPMCancelScheduledPowerEvent( scheduled_time, 
                                    scheduled_scheduler, scheduled_type);
            if(kIOReturnSuccess!=ret)
            {
                //goto exit;
            }
        }
    }
    
    exit:
    if(list) CFRelease(list);
    return ret;
}

static bool 
is_valid_repeating_dictionary(CFDictionaryRef   event)
{
    CFNumberRef         tmp_num;
    CFStringRef         tmp_str;

    if(NULL == event) return true;

    if(!isA_CFDictionary(event)) return false;
    
    tmp_num = (CFNumberRef)CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTimeKey));
    if(!isA_CFNumber(tmp_num)) return false;

    tmp_num = (CFNumberRef)CFDictionaryGetValue(event, CFSTR(kIOPMDaysOfWeekKey));
    if(!isA_CFNumber(tmp_num)) return false;

    tmp_str = (CFStringRef)CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTypeKey));
    if(!isA_CFString(tmp_str)) return false;    

    if(    !CFEqual(tmp_str, CFSTR(kIOPMAutoSleep))
        && !CFEqual(tmp_str, CFSTR(kIOPMAutoShutdown))
        && !CFEqual(tmp_str, CFSTR(kIOPMAutoWakeOrPowerOn))
        && !CFEqual(tmp_str, CFSTR(kIOPMAutoPowerOn))
        && !CFEqual(tmp_str, CFSTR(kIOPMAutoWake))
        && !CFEqual(tmp_str, CFSTR(kIOPMAutoRestart)) )
    {
        return false;
    }
    
    return true;
}

static int
getRepeatingDictionaryMinutes(CFDictionaryRef event)
{
    int val;
    CFNumberRef tmp_num;    
    tmp_num = (CFNumberRef)CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTimeKey));
    CFNumberGetValue(tmp_num, kCFNumberIntType, &val);
    return val;    
}

static int
getRepeatingDictionaryDayMask(CFDictionaryRef event)
{
    int val;
    CFNumberRef tmp_num;
    tmp_num = (CFNumberRef)CFDictionaryGetValue(event, CFSTR(kIOPMDaysOfWeekKey));
    CFNumberGetValue(tmp_num, kCFNumberIntType, &val);
    return val;
}

static CFStringRef
getRepeatingDictionaryType(CFDictionaryRef event)
{
    CFStringRef return_string;
    
    if(!event) {
        return CFSTR("");
    }
    
    return_string = isA_CFString( CFDictionaryGetValue(
                                    event, CFSTR(kIOPMPowerEventTypeKey)) );

    // prevent unexpected crashes by returning an empty string rather than NULL
    if(!return_string) {
        return CFSTR("");
    }

    return return_string;
}

// returns false if event occurs at 8PM and now it's 10PM
// returns true if event occurs at 8PM, now it's 9AM
static bool
upcomingToday(CFDictionaryRef event, int today_cf)
{
    static const int        kAllowScheduleWindowSeconds = 5;
    CFGregorianDate         greg_now;
    CFTimeZoneRef           tizzy;
    uint32_t                secondsToday;
    int                     secondsScheduled;
	int						days_mask;
    if(!event) return false;
    
    // Determine if the scheduled event falls on today's day of week
    days_mask = getRepeatingDictionaryDayMask(event);
    if(!(days_mask & (1 << (today_cf-1)))) return false;
    
    // get gregorian date for right now
    tizzy = CFTimeZoneCopySystem();    
    greg_now = CFAbsoluteTimeGetGregorianDate(
                    CFAbsoluteTimeGetCurrent(), tizzy);
    CFRelease(tizzy);
    
    secondsToday = 60 * ((greg_now.hour*60) + greg_now.minute);
    secondsScheduled = 60 * getRepeatingDictionaryMinutes(event);

    // Initially, we required a 2 minute safety window before scheduling the next
    // power event. Now, we throw caution to the wind and try a 5 second window.
    // Lost events will simply be lost events.
    if(secondsScheduled >= (secondsToday + kAllowScheduleWindowSeconds))
        return true;
    else 
        return false;
}

// daysUntil
// returns 0 if the event is upcoming today
// otherwise returns days until next repeating event, in range 1-7
static int
daysUntil(CFDictionaryRef event, int today_cf_day_of_week)
{
    int days_mask = getRepeatingDictionaryDayMask(event);
    int check = today_cf_day_of_week % 7;

    if(0 == days_mask) return -1;

    if(upcomingToday(event, today_cf_day_of_week)) return 0;

    // Note: CF days start counting at 1, the bit mask starts counting at 0.
    // Therefore, since we're tossing the CF day of week into a variable (check)
    // that we're checking the bitmask with, "check" effectively refers
    // to tomorrow, whlie today_cf_day_of_week refers to today.
    while(!(days_mask & (1<<check)))
    {
        check = (check + 1) % 7;
    }
    check -= today_cf_day_of_week;
	check++;	// adjust for CF day of week (1-7) vs. bitmask day of week (0-6)
    
    //  If the target day is next week, but earlier in the week than today, 
	//  check will be negative.  Mod of negative is bogus, so we add an extra
	//  7 days to make all work.

    check += 7;
    check %= 7;
    if(check == 0) check = 7;

    return check;
}

/*
 * _eventAlreadyScheduled
 *
 * returns true if there's already an event of this (date, name, type) 
 * active scheduled
 */
static bool
_eventAlreadyScheduled(CFDateRef ev_date, CFStringRef app_name, CFStringRef event_type)
{
    CFArrayRef              all_events = NULL;
    CFDictionaryRef         this_event = NULL;
    int                     i;
    int                     count;
    bool                    ret = false;
    CFStringRef             keys[3];
    CFTypeRef               vals[3];
    CFDictionaryRef         new_event = NULL;

    keys[0] = CFSTR(kIOPMPowerEventTimeKey);
    vals[0] = ev_date;
    keys[1] = CFSTR(kIOPMPowerEventAppNameKey);
    vals[1] = app_name; //CFSTR("Repeating");
    keys[2] = CFSTR(kIOPMPowerEventTypeKey);
    vals[2] = event_type; //getRepeatingDictionaryType(event);    
    new_event = CFDictionaryCreate(0, (const void **)keys, (const void **)vals, 3,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if(!new_event) goto exit;

    all_events = IOPMCopyScheduledPowerEvents();
    if(!all_events) goto exit;

    count = CFArrayGetCount(all_events);
    for(i=0; i<count; i++)
    {
        this_event = CFArrayGetValueAtIndex(all_events, i);    
        if(CFEqual(this_event, new_event)) ret = true;
    }
exit:
    if(new_event) CFRelease(new_event);
    if(all_events) CFRelease(all_events);    
    return ret;
}


// scheduleNextRepeatingEvent
static void
scheduleNextRepeatingEvent(CFDictionaryRef   event)
{
    CFGregorianDate         greg;
    CFTimeZoneRef           tizzy;
    CFAbsoluteTime          ev_time;
    CFDateRef               ev_date;
    int                     days;
    int                     minutes_scheduled;
    int                     cf_day_of_week;

    if(!event) return;
    
    tizzy = CFTimeZoneCopySystem();

    // Massage the scheduled time into today's date
    cf_day_of_week = CFAbsoluteTimeGetDayOfWeek(
            CFAbsoluteTimeGetCurrent(), tizzy);
    days = daysUntil(event, cf_day_of_week);

    greg = CFAbsoluteTimeGetGregorianDate(
                    CFAbsoluteTimeGetCurrent() + days*(60*60*24), 
                    tizzy);
                            
    minutes_scheduled = getRepeatingDictionaryMinutes(event);

    greg.hour = minutes_scheduled/60;
    greg.minute = minutes_scheduled%60;
    greg.second = 0.0;
    
    ev_time = CFGregorianDateGetAbsoluteTime(greg, tizzy);
    ev_date = CFDateCreate(kCFAllocatorDefault, ev_time);
    
    // Make sure that we haven't already scheduled a repeating event
    // for exactly this purpose. Only schedule a new one-time event
    // if there isn't one already on the books.
    if( !_eventAlreadyScheduled(ev_date, CFSTR("Repeating"), 
            getRepeatingDictionaryType(event)) ) 
    { 
        IOPMSchedulePowerEvent(ev_date, CFSTR("Repeating"), 
                getRepeatingDictionaryType(event));
    }
    
    CFRelease(ev_date);
    CFRelease(tizzy);
}

//************************************
//*** CALLBACKS FROM AutoWakeScheduler.c
//************************************
__private_extern__ void 
RepeatingAutoWakeRepeatingEventOcurred(CFDictionaryRef event)
{
    CFStringRef         type = getRepeatingDictionaryType(event);

    if(!type) return;
    
    if( CFEqual(type, CFSTR(kIOPMAutoSleep))
        || CFEqual(type, CFSTR(kIOPMAutoShutdown)) )
    {
        // Schedule tomorrow's shutdown
        scheduleNextRepeatingEvent(repeatingPowerOff);
    } else {
        // Schedule tomorrow's wake
        scheduleNextRepeatingEvent(repeatingPowerOn);
    }

    return;
}

//************************************
//************************************

__private_extern__ void 
RepeatingAutoWakePrefsHaveChanged(void)
{
    // Grab the new prefs off disk
    newRepeatingEvents = IOPMCopyRepeatingPowerEvents();

    // Check whether the repeating prefs have changed, or if it's just
    // an otherwise scheduled automatic power-on event
    if(newRepeatingEvents && currentRepeatingEvents &&
        CFEqual(newRepeatingEvents, currentRepeatingEvents))
    {
        // Repeating prefs have not changed - bail immediately
        CFRelease(newRepeatingEvents);
        return;
    } else {
        // maybe the new ones are NULL, and were NULL before?
        if(!newRepeatingEvents && !currentRepeatingEvents) return;
    }

    // At this point, newRepeatingEvents exists and is valid, 
    // and currentRepeatingEvents exists and is valid,
    // And they're separate things

    // wipe the slate clean
    if(currentRepeatingEvents) 
    {
        CFRelease(currentRepeatingEvents);
        currentRepeatingEvents = 0;
    }
    currentRepeatingEvents = newRepeatingEvents;

    cancelAllRepeatingEvents();
    
    repeatingPowerOff = NULL;
    repeatingPowerOn = NULL;
    
    if(!currentRepeatingEvents) return;

    repeatingPowerOff = isA_CFDictionary(CFDictionaryGetValue(
                                currentRepeatingEvents, 
                                CFSTR(kIOPMRepeatingPowerOffKey)));
    repeatingPowerOn = isA_CFDictionary(CFDictionaryGetValue(
                                currentRepeatingEvents, 
                                CFSTR(kIOPMRepeatingPowerOnKey)));

    if( !is_valid_repeating_dictionary(repeatingPowerOff) 
     || !is_valid_repeating_dictionary(repeatingPowerOn) )
    {
        syslog(LOG_INFO, "PMCFGD: Invalid formatted repeating power event dictionary\n");
        return;
    }
    
    // Valid structured file... proceeding

    scheduleNextRepeatingEvent(repeatingPowerOn);
    scheduleNextRepeatingEvent(repeatingPowerOff);
}

__private_extern__ void
RepeatingAutoWakeSleepWakeNotification(natural_t messageType, int runState)
{
    if ((kIOMessageSystemHasPoweredOn == messageType)
        && (kRStateNormal == runState))
    {
        /* System sleep robs time from CFTimers!
         * On wake from sleep, reschedule all of our pending repeating
         * power requests.
         */
        scheduleNextRepeatingEvent(repeatingPowerOn);    
        scheduleNextRepeatingEvent(repeatingPowerOff);
    }
}

__private_extern__ void 
RepeatingAutoWake_prime(void)
{
    // We can get away just doing the same 
    // thing we do when the prefs file changes.
    RepeatingAutoWakePrefsHaveChanged();    
}
