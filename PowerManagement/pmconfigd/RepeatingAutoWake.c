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
#include <bsm/libbsm.h>
#include "RepeatingAutoWake.h"
#include "PrivateLib.h"
#include "AutoWakeScheduler.h"

/*
 * These are the days of the week as provided by
 * CFAbsoluteTimeGetDayOfWeek()
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

static CFDictionaryRef  repeatingPowerOff = 0;
static CFDictionaryRef  repeatingPowerOn = 0;



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
 * Copy Events from on-disk file. We should be doing this only
 * once, at start of the powerd.
 */
static void
copyScheduledRepeatPowerEvents(void)
{
    SCPreferencesRef        prefs;
    CFDictionaryRef         tmp;
   
    prefs = SCPreferencesCreate(0, 
                               CFSTR("PM-configd-AutoWake"),
                                CFSTR(kIOPMAutoWakePrefsPath));
    if(!prefs) return;

    if (repeatingPowerOff) CFRelease(repeatingPowerOff);
    if (repeatingPowerOn) CFRelease(repeatingPowerOn);

    tmp = (CFDictionaryRef)SCPreferencesGetValue(prefs, CFSTR(kIOPMRepeatingPowerOffKey));
    if (tmp && isA_CFDictionary(tmp))
        repeatingPowerOff = CFDictionaryCreateMutableCopy(0,0,tmp);

    tmp = (CFDictionaryRef)SCPreferencesGetValue(prefs, CFSTR(kIOPMRepeatingPowerOnKey));
    if (tmp && isA_CFDictionary(tmp))
        repeatingPowerOn = CFDictionaryCreateMutableCopy(0,0,tmp);

    CFRelease(prefs);
}

/*
 * Returns a copy of repeat event after changing the repeat date
 * into next event date.
 *
 * Caller is responsible for releasing the copy after use.
 */
__private_extern__ CFDictionaryRef
copyNextRepeatingEvent(CFStringRef type)
{
    CFDictionaryRef         repeatDict = NULL;
    CFStringRef             repeatDictType = NULL;
    CFMutableDictionaryRef  repeatDictCopy = NULL;
    CFGregorianDate         greg;
    CFTimeZoneRef           tizzy;
    CFAbsoluteTime          ev_time;
    CFDateRef               ev_date;
    int                     days;
    int                     minutes_scheduled;
    int                     cf_day_of_week;

    /*
     * 'WakeOrPowerOn' repeat events are returned when caller asks
     * for 'Wake' events or 'PowerOn' events.
     * Don't bother to return anything if caller is looking specifically for
     * WakeOrPowerOn type repeat events.
     */
    if( CFEqual(type, CFSTR(kIOPMAutoSleep))
        || CFEqual(type, CFSTR(kIOPMAutoShutdown)) ||
        CFEqual(type, CFSTR(kIOPMAutoRestart)) )
    {
        repeatDict = repeatingPowerOff;
    }
    else if (
        CFEqual(type, CFSTR(kIOPMAutoPowerOn)) ||
        CFEqual(type, CFSTR(kIOPMAutoWake)) )
    {
        repeatDict = repeatingPowerOn;
    }
    else
        return NULL;

    repeatDictType = getRepeatingDictionaryType(repeatDict);
    if (CFEqual(type, repeatDictType) ||
            ( (CFEqual(repeatDictType, CFSTR(kIOPMAutoWakeOrPowerOn))) &&
              (CFEqual(type, CFSTR(kIOPMAutoPowerOn)) || CFEqual(type, CFSTR(kIOPMAutoWake)))
            )
       )
    {
        repeatDictCopy = CFDictionaryCreateMutableCopy(0,0,repeatDict);

        tizzy = CFTimeZoneCopySystem();

        // Massage the scheduled time into today's date
        cf_day_of_week = CFAbsoluteTimeGetDayOfWeek(
                CFAbsoluteTimeGetCurrent(), tizzy);
        days = daysUntil(repeatDict, cf_day_of_week);

        greg = CFAbsoluteTimeGetGregorianDate(
                        CFAbsoluteTimeGetCurrent() + days*(60*60*24), 
                        tizzy);
                                
        minutes_scheduled = getRepeatingDictionaryMinutes(repeatDict);

        greg.hour = minutes_scheduled/60;
        greg.minute = minutes_scheduled%60;
        greg.second = 0.0;
        
        ev_time = CFGregorianDateGetAbsoluteTime(greg, tizzy);
        ev_date = CFDateCreate(kCFAllocatorDefault, ev_time);
 
        CFDictionarySetValue(repeatDictCopy, CFSTR(kIOPMPowerEventTimeKey),
                ev_date);

        /* Set 'AppNameKey' to 'Repeating' */
        CFDictionarySetValue(repeatDictCopy, CFSTR(kIOPMPowerEventAppNameKey),
                CFSTR(kIOPMRepeatingAppName));
        CFRelease(ev_date);
        CFRelease(tizzy);
    }

    return repeatDictCopy;
}


__private_extern__ void 
RepeatingAutoWake_prime(void)
{
    copyScheduledRepeatPowerEvents( );

}

static IOReturn
updateRepeatEventsOnDisk(SCPreferencesRef prefs)
{
    IOReturn ret = kIOReturnSuccess;

    if (repeatingPowerOn) {
        if(!SCPreferencesSetValue(prefs, CFSTR(kIOPMRepeatingPowerOnKey), repeatingPowerOn)) 
        {
            ret = kIOReturnError;
            goto exit;
        }
    }
    else SCPreferencesRemoveValue(prefs, CFSTR(kIOPMRepeatingPowerOnKey));


    if (repeatingPowerOff) {
        if(!SCPreferencesSetValue(prefs, CFSTR(kIOPMRepeatingPowerOffKey), repeatingPowerOff)) 
        {
            ret = kIOReturnError;
            goto exit;
        }
    }
    else SCPreferencesRemoveValue(prefs, CFSTR(kIOPMRepeatingPowerOffKey));

    if(!SCPreferencesCommitChanges(prefs))
    {
        ret = kIOReturnError;
        goto exit;
    }

exit:
    return ret;
}

kern_return_t
_io_pm_schedule_repeat_event
(
    mach_port_t             server __unused,
    audit_token_t           token,
    vm_offset_t             flatPackage,
    mach_msg_type_number_t  packageLen,
    int                     action,
    int                     *return_code
)
{
    CFDictionaryRef     events = NULL;
    CFDictionaryRef     offEvents = NULL;
    CFDictionaryRef     onEvents = NULL;
    CFDataRef           dataRef = NULL;
    uid_t               callerEUID;
    SCPreferencesRef    prefs = 0;
    CFStringRef         prevOffType = NULL;
    CFStringRef         prevOnType = NULL;
    CFStringRef         newOffType = NULL;
    CFStringRef         newOnType = NULL;


    *return_code = kIOReturnSuccess;

    audit_token_to_au32(token, NULL, &callerEUID, NULL, NULL, NULL, NULL, NULL, NULL);

    dataRef = CFDataCreate(0, (const UInt8 *)flatPackage, packageLen);
    if (dataRef) {
        events = (CFDictionaryRef)CFPropertyListCreateWithData(0, dataRef, 0, NULL, NULL); 
    }

    if (!events) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }
    offEvents = isA_CFDictionary(CFDictionaryGetValue(
                                events, 
                                CFSTR(kIOPMRepeatingPowerOffKey)));
    onEvents = isA_CFDictionary(CFDictionaryGetValue(
                                events, 
                                CFSTR(kIOPMRepeatingPowerOnKey)));

    if( !is_valid_repeating_dictionary(offEvents) 
     || !is_valid_repeating_dictionary(onEvents) )
    {
        syslog(LOG_INFO, "PMCFGD: Invalid formatted repeating power event dictionary\n");
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    if((*return_code = createSCSession(&prefs, callerEUID, 1)) != kIOReturnSuccess)
        goto exit;


    /* Need to take a retain on these strings as these dictionaries get released below */
    prevOffType =  getRepeatingDictionaryType(repeatingPowerOff); CFRetain(prevOffType);
    prevOnType = getRepeatingDictionaryType(repeatingPowerOn); CFRetain(prevOnType);

    /*
     * Remove both off & on events first. If off or on event is not set thru this request,
     * then it is assumed that user is requesting to delete it.
     */
    if (repeatingPowerOff && isA_CFDictionary(repeatingPowerOff))
        CFRelease(repeatingPowerOff); 
    if (repeatingPowerOn && isA_CFDictionary(repeatingPowerOn))
        CFRelease(repeatingPowerOn); 

    repeatingPowerOff = repeatingPowerOn = NULL;


    if (offEvents) {
        repeatingPowerOff = CFDictionaryCreateMutableCopy(0,0,offEvents);
    }
    if (onEvents) {
        repeatingPowerOn = CFDictionaryCreateMutableCopy(0,0,onEvents);
    }


    if ((*return_code = updateRepeatEventsOnDisk(prefs)) != kIOReturnSuccess)
        goto exit;

    newOffType = getRepeatingDictionaryType(repeatingPowerOff);
    newOnType = getRepeatingDictionaryType(repeatingPowerOn);


    /* 
     * Re-schedule the modified event types in case these new events are earlier
     * than previously scheduled ones
     */
    schedulePowerEventType(prevOffType);
    schedulePowerEventType(prevOnType);

    if (!CFEqual(prevOffType, newOffType))
        schedulePowerEventType(newOffType);

    if (!CFEqual(prevOnType, newOnType))
        schedulePowerEventType(newOnType);


exit:
    if (prevOffType)
        CFRelease(prevOffType);
    if (prevOnType)
        CFRelease(prevOnType);

    if (dataRef)
        CFRelease(dataRef);
    if (events)
        CFRelease(events);
    destroySCSession(prefs, 1);

    vm_deallocate(mach_task_self(), flatPackage, packageLen);

    return KERN_SUCCESS;
}


kern_return_t
_io_pm_cancel_repeat_events
(
    mach_port_t             server __unused,
    audit_token_t           token,
    int                     *return_code
)
{

    SCPreferencesRef    prefs = 0;
    uid_t               callerEUID;
    CFStringRef         offType = NULL;
    CFStringRef         onType = NULL;

    *return_code = kIOReturnSuccess;

    audit_token_to_au32(token, NULL, &callerEUID, NULL, NULL, NULL, NULL, NULL, NULL);

    if((*return_code = createSCSession(&prefs, callerEUID, 1)) != kIOReturnSuccess)
        goto exit;


    /* Need to take a retain on these strings as these dictionaries get release below */
    offType = getRepeatingDictionaryType(repeatingPowerOff); CFRetain(offType);
    onType = getRepeatingDictionaryType(repeatingPowerOn); CFRetain(onType);

    if (repeatingPowerOff && isA_CFDictionary(repeatingPowerOff))
        CFRelease(repeatingPowerOff); 
    if (repeatingPowerOn && isA_CFDictionary(repeatingPowerOn))
        CFRelease(repeatingPowerOn); 

    repeatingPowerOff = repeatingPowerOn = NULL;

    if ((*return_code = updateRepeatEventsOnDisk(prefs)) != kIOReturnSuccess)
        goto exit;

    schedulePowerEventType(offType);
    schedulePowerEventType(onType);

exit:

    if (offType)
        CFRelease(offType);
    if (onType)
        CFRelease(onType);
    destroySCSession(prefs, 1);

    return KERN_SUCCESS;
}

__private_extern__ CFDictionaryRef copyRepeatPowerEvents( )
{

    CFMutableDictionaryRef  return_dict = NULL;

    return_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, 
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks); 

    if (repeatingPowerOn && isA_CFDictionary(repeatingPowerOn))
        CFDictionaryAddValue(return_dict, CFSTR(kIOPMRepeatingPowerOnKey), repeatingPowerOn);     

    if (repeatingPowerOff && isA_CFDictionary(repeatingPowerOff))
        CFDictionaryAddValue(return_dict, CFSTR(kIOPMRepeatingPowerOffKey), repeatingPowerOff);     

    return return_dict;
}
