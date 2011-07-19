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

#include <TargetConditionals.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>
#include "IOSystemConfiguration.h"
#include "IOPMKeys.h"
#include "IOPMLib.h"
#include "IOPMLibPrivate.h"
#include <notify.h>

enum {
    kIOPMMaxScheduledEntries = 1000
};

#ifndef kIOPMMaintenanceScheduleImmediate
#define kIOPMMaintenanceScheduleImmediate   "MaintenanceImmediate"
#endif

#ifndef kPMSetMaintenanceWakeCalendar
#define kPMSetMaintenanceWakeCalendar 8
#endif

// Forward decls

static CFComparisonResult compare_dates(
    CFDictionaryRef a1, 
    CFDictionaryRef a2, 
    void *c);
static CFAbsoluteTime roundOffDate( 
    CFAbsoluteTime time);
static CFDictionaryRef _IOPMCreatePowerOnDictionary(
    CFAbsoluteTime the_time, 
    CFStringRef the_id, 
    CFStringRef type);
static bool inputsValid(
    CFDateRef time_to_wake, 
    CFStringRef my_id, 
    CFStringRef type);
static bool addEntryAndSetPrefs(
    SCPreferencesRef prefs, 
    CFStringRef type, 
    CFDictionaryRef package);
static bool removeEntryAndSetPrefs(
    SCPreferencesRef prefs, 
    CFStringRef type, 
    CFDictionaryRef package);
static IOReturn _setRootDomainProperty(
    CFStringRef key,
    CFTypeRef val);
static void tellClockController(
    CFStringRef command,
    CFDateRef power_date);
IOReturn IOPMSchedulePowerEvent(
    CFDateRef time_to_wake, 
    CFStringRef my_id, 
    CFStringRef type);
IOReturn IOPMCancelScheduledPowerEvent(
    CFDateRef time_to_wake, 
    CFStringRef my_id, 
    CFStringRef wake_or_restart);    
CFArrayRef IOPMCopyScheduledPowerEvents( void );
static IOReturn doAMaintenanceWake(
    CFDateRef earliestRequest);



static CFComparisonResult compare_dates(
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

#if TARGET_OS_EMBEDDED
#define ROUND_SCHEDULE_TIME	(5.0)
#define MIN_SCHEDULE_TIME	(5.0)
#else
#define ROUND_SCHEDULE_TIME	(5.0)
#define MIN_SCHEDULE_TIME	(5.0)
#endif  /* TARGET_OS_EMBEDDED */


static CFAbsoluteTime roundOffDate(CFAbsoluteTime time)
{
    // round time down to the closest multiple of ROUND_SCHEDULE_TIME seconds
    // CFAbsoluteTimes are encoded as doubles
    return (CFAbsoluteTime) (trunc(time / ((double) ROUND_SCHEDULE_TIME)) * ((double) ROUND_SCHEDULE_TIME)); 
}

static CFDictionaryRef 
_IOPMCreatePowerOnDictionary(
    CFAbsoluteTime the_time, 
    CFStringRef the_id, 
    CFStringRef type)
{
    CFMutableDictionaryRef          d;
    CFDateRef                       the_date;

    // make sure my_id is valid or NULL
    the_id = isA_CFString(the_id);        
    // round wakeup time to last ROUND_SCHEDULE_TIME second increment
    the_time = roundOffDate(the_time);
    // package AbsoluteTime as a date for CFType purposes
    the_date = CFDateCreate(0, the_time);
    d = CFDictionaryCreateMutable(0, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if(!d) return NULL;
    CFDictionaryAddValue(d, CFSTR(kIOPMPowerEventTimeKey), the_date);
    if(!the_id) the_id = CFSTR("");
    CFDictionaryAddValue(d, CFSTR(kIOPMPowerEventAppNameKey), the_id);
    CFDictionaryAddValue(d, CFSTR(kIOPMPowerEventTypeKey), type);
    CFRelease(the_date);
    return d;
}

static bool 
inputsValid(
    CFDateRef time_to_wake, 
    CFStringRef my_id __unused, 
    CFStringRef type)
{
    // NULL is an acceptable input for my_id
    
    // NULL is only an accetable input to IOPMSchedulePowerEvent
    // if the type of event being scheduled is Immediate; in which
    // case NULL means "zero out current settings" to the hardware.
    if (!isA_CFDate(time_to_wake)
        && !CFEqual(type, CFSTR(kIOPMAutoWakeScheduleImmediate))
        && !CFEqual(type, CFSTR(kIOPMAutoPowerScheduleImmediate)) )
    {
        return false;
    }
    
    if(!isA_CFString(type)) return false;
    if(!(CFEqual(type, CFSTR(kIOPMAutoWake)) || 
        CFEqual(type, CFSTR(kIOPMAutoPowerOn)) ||
        CFEqual(type, CFSTR(kIOPMAutoWakeOrPowerOn)) ||
        CFEqual(type, CFSTR(kIOPMAutoSleep)) ||
        CFEqual(type, CFSTR(kIOPMAutoShutdown)) ||
        CFEqual(type, CFSTR(kIOPMAutoRestart)) ||
        CFEqual(type, CFSTR(kIOPMAutoWakeScheduleImmediate)) ||
        CFEqual(type, CFSTR(kIOPMAutoPowerScheduleImmediate)) ||
        CFEqual(type, CFSTR(kIOPMAutoWakeRelativeSeconds)) ||
        CFEqual(type, CFSTR(kIOPMAutoPowerRelativeSeconds)) ||
        CFEqual(type, CFSTR(kIOPMMaintenanceScheduleImmediate)) ))
    {
        return false;
    }

    return true;
}

static bool 
addEntryAndSetPrefs(
    SCPreferencesRef prefs, 
    CFStringRef type, 
    CFDictionaryRef package)
{
    CFArrayRef              arr = 0;
    CFMutableArrayRef       new_arr = 0;
    bool                    ret = false;
    
    arr = isA_CFArray(SCPreferencesGetValue(prefs, type));
    if(arr)
    {
        // There is already an array - add this entry to the end of it
        new_arr = CFArrayCreateMutableCopy(0, 0, arr);
        CFArrayAppendValue(new_arr, package);
        
        // and sort it by wakeup time! Maintain the array in sorted order!
        CFArraySortValues(
                new_arr, CFRangeMake(0, CFArrayGetCount(new_arr)),
                (CFComparatorFunction)compare_dates, 0);
    } else
    {
        // There is not already an array in the prefs file. 
        // Create one with this entry.
        new_arr = (CFMutableArrayRef)CFArrayCreate(
                                        0, (const void **)&package, 
                                        1, &kCFTypeArrayCallBacks);
    }
    
    // Write it out
    if(!new_arr) 
    {
        ret = false;
        goto exit;
    }
    
    if(!SCPreferencesSetValue(prefs, type, new_arr)) 
    {
        ret = false;
        goto exit;
    }

    ret = true;
    
exit:
    if(new_arr) CFRelease(new_arr);
    return ret;
}

// returns true if an entry was successfully removed
// false otherwise (entry wasn't found, or couldn't be removed)
static bool removeEntryAndSetPrefs(
    SCPreferencesRef prefs, 
    CFStringRef type, 
    CFDictionaryRef package)
{
    CFArrayRef                  arr = 0;
    CFMutableArrayRef           mut_arr = 0;
    CFIndex                     i, count;
    CFDictionaryRef             cancelee = 0;
    CFComparisonResult          eq;
    bool                        ret = false;

    // Grab the specific array from which we want to remove the entry... 
    // wakeup or poweron?
    // or both?
    arr = isA_CFArray(SCPreferencesGetValue(prefs, type));
    if(!arr) 
    {
        ret = true;
        goto exit;
    }
    count = CFArrayGetCount(arr);
    for (i = 0; i < count; i++)
    {
        cancelee = CFArrayGetValueAtIndex(arr, i);
        eq = compare_dates(package, cancelee, 0);
        if(kCFCompareLessThan == eq)
        {
            // fail, date to cancel < date at index
            break;
        }
        else if(kCFCompareEqualTo == eq)
        {
            // We have confirmation on the dates and types being equal. Check id.
            if( CFEqual(
                CFDictionaryGetValue(package, CFSTR(kIOPMPowerEventAppNameKey)), 
                CFDictionaryGetValue(cancelee, CFSTR(kIOPMPowerEventAppNameKey))
                ))
            {
                // This is the one to cancel
                mut_arr = CFArrayCreateMutableCopy(0, 0, arr);
                CFArrayRemoveValueAtIndex(mut_arr, i);
                if(!SCPreferencesSetValue(prefs, type, mut_arr)) 
                { 
                    // fail
                    break;
                }
                // success!
                ret = true;
                break;
            }
        }
    }
    
    exit:
    if(mut_arr) CFRelease(mut_arr);
    return ret;
}

static IOReturn 
_setRootDomainProperty(
    CFStringRef                 key, 
    CFTypeRef                   val) 
{
    io_registry_entry_t         root_domain;
    IOReturn                    ret;

    root_domain = IORegistryEntryFromPath( kIOMasterPortDefault, 
                kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
    if(!root_domain) return kIOReturnError;
 
    ret = IORegistryEntrySetCFProperty(root_domain, key, val);

    IOObjectRelease(root_domain);
    return ret;
}

static IOReturn doAMaintenanceWake(
    CFDateRef earliestRequest)
 {
    CFGregorianDate         maintGregorian;
    IOPMCalendarStruct      pmMaintenanceDate;
    IOReturn                connectReturn = 0;
    size_t                  connectReturnSize = sizeof(IOReturn);
    io_connect_t            root_domain_connect = IO_OBJECT_NULL;
    io_registry_entry_t     root_domain = IO_OBJECT_NULL;
    IOReturn                ret = kIOReturnError;
    kern_return_t           kr = -1;

    // Package maintenance time as a PMCalendarType and pass it into IOPMrootDomain
    CFTimeZoneRef  myTimeZone = NULL;
    
    myTimeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(0, 0.0);
    if (!myTimeZone)
        goto exit;

    maintGregorian = CFAbsoluteTimeGetGregorianDate(
                        CFDateGetAbsoluteTime(earliestRequest), myTimeZone);

    CFRelease(myTimeZone);
    
    // Stuff into PM Calendar struct
    bzero(&pmMaintenanceDate, sizeof(pmMaintenanceDate));
    pmMaintenanceDate.year = maintGregorian.year;
    pmMaintenanceDate.month = maintGregorian.month;
    pmMaintenanceDate.day = maintGregorian.day;
    pmMaintenanceDate.hour = maintGregorian.hour;
    pmMaintenanceDate.minute = maintGregorian.minute;
    pmMaintenanceDate.second = maintGregorian.second;

    // Open up RootDomain
    
    root_domain = IORegistryEntryFromPath( kIOMasterPortDefault, 
                kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");

    if (root_domain != IO_OBJECT_NULL)
    {
        kr = IOServiceOpen(root_domain, mach_task_self(), 0, &root_domain_connect);
        
        if (KERN_SUCCESS == kr)
        {
            kr = IOConnectCallStructMethod(
                root_domain_connect, 
                kPMSetMaintenanceWakeCalendar,
                &pmMaintenanceDate, sizeof(pmMaintenanceDate),      // inputs struct
                &connectReturn, &connectReturnSize);                // outputs struct
    
            IOServiceClose(root_domain_connect);
        }
        
        IOObjectRelease(root_domain);
    }

    if ((KERN_SUCCESS != kr) || (kIOReturnSuccess != connectReturn))
    {
        // Cry, cry, cry.
        ret = kIOReturnError;
        goto exit;
    }

    ret = kIOReturnSuccess;
exit:
    return ret;
}

static void 
tellClockController(
    CFStringRef command, 
    CFDateRef power_date)
{
    CFAbsoluteTime          now, wake_time;
    CFGregorianDate         gmt_calendar;
    CFTimeZoneRef           gmt_tz = NULL;
    long int                diff_secs;
    IOReturn                ret;
    CFNumberRef             seconds_delta = NULL;
    IOPMCalendarStruct      *cal_date = NULL;
    CFMutableDataRef        date_data = NULL;

    if(!command) goto exit;
    
    // We broadcast the wakeup time both as calendar date struct and as seconds.
    //  * AppleRTC hardware needs the date in a structured calendar format
    //  * ApplePMU & AppleSMU just need the date in seconds relative to now

    // ******************** Calendar struct ************************************
    
    date_data = CFDataCreateMutable(NULL, sizeof(IOPMCalendarStruct));
    CFDataSetLength(date_data, sizeof(IOPMCalendarStruct));
    cal_date = (IOPMCalendarStruct *)CFDataGetBytePtr(date_data);
    bzero(cal_date, sizeof(IOPMCalendarStruct));

    if(!power_date) {
    
        // Zeroed out calendar means "clear wakeup timer"

    } else {

        // A calendar struct stuffed with meaningful date and time
        // schedules a wake or power event for then.

        wake_time = CFDateGetAbsoluteTime(power_date);

        gmt_tz = CFTimeZoneCreateWithTimeIntervalFromGMT(0, 0.0);
        gmt_calendar = CFAbsoluteTimeGetGregorianDate(wake_time, gmt_tz);
        CFRelease(gmt_tz);

        cal_date->second    = lround(gmt_calendar.second);
        if (60 == cal_date->second)
            cal_date->second = 59;
        cal_date->minute    = gmt_calendar.minute;
        cal_date->hour      = gmt_calendar.hour;
        cal_date->day       = gmt_calendar.day;
        cal_date->month     = gmt_calendar.month;
        cal_date->year      = gmt_calendar.year;
    }
    
    if(CFEqual(command, CFSTR(kIOPMAutoWake))) {

        // Set AutoWake calendar property
        ret = _setRootDomainProperty(
                        CFSTR(kIOPMSettingAutoWakeCalendarKey), 
                        date_data);

    } else {

        // Set AutoPower calendar property
        ret = _setRootDomainProperty(
                        CFSTR(kIOPMSettingAutoPowerCalendarKey), 
                        date_data);    
    }

    if(kIOReturnSuccess != ret) {
        goto exit;
    }


    // *************************** Seconds *************************************
    // ApplePMU/AppleSMU seconds path
    // Machine needs to be told alarm in seconds relative to current time.

    if(!power_date) {
        // NULL dictionary argument, clear wakeup timer
        diff_secs = 0;
    } else {
        // Assume a well-formed entry since we've been doing thorough 
        // type-checking in the find & purge functions
        now = CFAbsoluteTimeGetCurrent();
        wake_time = CFDateGetAbsoluteTime(power_date);
        
        diff_secs = lround(wake_time - now);
        if(diff_secs < 0) goto exit;
    }
    
    // Package diff_secs as a CFNumber
    seconds_delta = CFNumberCreate(0, kCFNumberLongType, &diff_secs);
    if(!seconds_delta) goto exit;
    
    if(CFEqual(command, CFSTR(kIOPMAutoWake))) {

        // Set AutoWake seconds property
        ret = _setRootDomainProperty(
                        CFSTR(kIOPMSettingAutoWakeSecondsKey), 
                        seconds_delta);

    } else {

        // Set AutoPower seconds property
        ret = _setRootDomainProperty(
                        CFSTR(kIOPMSettingAutoPowerSecondsKey), 
                        seconds_delta);
    }

    if(kIOReturnSuccess != ret) {
        goto exit;
    }

exit:
    if(date_data) CFRelease(date_data);
    if(seconds_delta) CFRelease(seconds_delta);
    return;
}


IOReturn IOPMSchedulePowerEvent(
    CFDateRef time_to_wake, 
    CFStringRef my_id,
    CFStringRef type)
{
    CFDictionaryRef         package = 0;
    SCPreferencesRef        prefs = 0;
    IOReturn                ret = kIOReturnError;
    CFArrayRef              tmp_wakeup_arr = NULL;
    int                     total_count = 0;
    CFAbsoluteTime          abs_time_to_wake;

    //  verify inputs
    if(!inputsValid(time_to_wake, my_id, type))
    {
        ret = kIOReturnBadArgument;
        goto exit;
    }

    if( CFEqual(type, CFSTR(kIOPMMaintenanceScheduleImmediate)) )
    {
        ret = doAMaintenanceWake(time_to_wake);
        goto exit;
    } else if( CFEqual(type, CFSTR(kIOPMAutoWakeScheduleImmediate)) )
    {

        // Just send down the wake event immediately
        tellClockController(CFSTR(kIOPMAutoWake), time_to_wake);
        ret = kIOReturnSuccess;
        goto exit;

    } else if( CFEqual(type, CFSTR(kIOPMAutoPowerScheduleImmediate)) )
    {

        // Just send down the power on event immediately
        tellClockController(CFSTR(kIOPMAutoPowerOn), time_to_wake);
        ret = kIOReturnSuccess;
        goto exit;

    } else if( CFEqual( type, CFSTR( kIOPMAutoWakeRelativeSeconds) ) 
            || CFEqual( type, CFSTR( kIOPMAutoPowerRelativeSeconds) ) )
    {

        // Immediately send down a relative seconds argument
        // Seconds are relative to "right now" in CFAbsoluteTime
        CFAbsoluteTime      now_secs;
        CFAbsoluteTime      event_secs;
        CFNumberRef         diff_secs = NULL;
        int                 diff;

        if(time_to_wake)
        {
            now_secs = CFAbsoluteTimeGetCurrent();
            event_secs = CFDateGetAbsoluteTime(time_to_wake);
            diff = (int)event_secs - (int)now_secs;
            if(diff <= 0)
            {
                // Only positive diffs are meaningful
                return kIOReturnIsoTooOld;
            }
        } else {
            diff = 0;
        }
                
        diff_secs = CFNumberCreate(0, kCFNumberIntType, &diff);
        if(!diff_secs) goto exit;
        
        _setRootDomainProperty( type, (CFTypeRef)diff_secs );

        CFRelease(diff_secs);

        ret = kIOReturnSuccess;
        goto exit;
    }

    /* From here onward, proceed with scheduling power events for the future
     * by:
     *   - Enqueueing them in com.apple.AutoWake.plist
     *   - Committing the file to disk, where PowerManagement configd plugin
     *         will immediately examine the file and setup upcoming events.
     */

    abs_time_to_wake = CFDateGetAbsoluteTime(time_to_wake);
    if(abs_time_to_wake < (CFAbsoluteTimeGetCurrent() + MIN_SCHEDULE_TIME))
    {
        ret = kIOReturnNotReady;
        goto exit;
    }
    
    // Package the event in a CFDictionary
    package = _IOPMCreatePowerOnDictionary(abs_time_to_wake, my_id, type);

    // Open the prefs file and grab the current array
#if TARGET_OS_EMBEDDED
    if (geteuid() != 0)
        prefs = SCPreferencesCreateWithAuthorization(0, CFSTR("IOKit-AutoWake"), 
                                CFSTR(kIOPMAutoWakePrefsPath), NULL);
    else
#endif /* TARGET_OS_EMBEDDED */
    prefs = SCPreferencesCreate( 0, CFSTR("IOKit-AutoWake"), 
                                 CFSTR(kIOPMAutoWakePrefsPath));
    if(!prefs || !SCPreferencesLock(prefs, true))
    {
        if(kSCStatusAccessError == SCError())
            ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }

    // Examine number of entries currently in disk and bail if too many
    total_count = 0;
    tmp_wakeup_arr = isA_CFArray(
                        SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoPowerOn)));
    if(tmp_wakeup_arr) total_count += CFArrayGetCount(tmp_wakeup_arr);
    tmp_wakeup_arr = isA_CFArray(
                        SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoWake)));
    if(tmp_wakeup_arr) total_count += CFArrayGetCount(tmp_wakeup_arr);
    tmp_wakeup_arr = isA_CFArray(
                        SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoWakeOrPowerOn)));
    if(tmp_wakeup_arr) total_count += CFArrayGetCount(tmp_wakeup_arr);
    tmp_wakeup_arr = isA_CFArray(
                        SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoSleep)));
    if(tmp_wakeup_arr) total_count += CFArrayGetCount(tmp_wakeup_arr);
    tmp_wakeup_arr = isA_CFArray(
                        SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoShutdown)));
    if(tmp_wakeup_arr) total_count += CFArrayGetCount(tmp_wakeup_arr);
    tmp_wakeup_arr = isA_CFArray(
                        SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoRestart)));
    if(tmp_wakeup_arr) total_count += CFArrayGetCount(tmp_wakeup_arr);
    if(total_count >= kIOPMMaxScheduledEntries)
    {
        ret = kIOReturnNoSpace;
        goto exit;
    }

    // just add the entry to the one (wake or power on)
    addEntryAndSetPrefs(prefs, type, package);

    // Add a warning to the file
    SCPreferencesSetValue(prefs, CFSTR("WARNING"), 
        CFSTR("Do not edit this file by hand. It must remain in sorted-by-date order."));

    //  commit the SCPreferences file out to disk
    if(!SCPreferencesCommitChanges(prefs))
    {
        ret = kIOReturnError;
        goto exit;
    }

    ret = kIOReturnSuccess;
    notify_post(kIOPMSchedulePowerEventNotification);

exit:
    
    if(package) CFRelease(package);
    if(prefs) SCPreferencesUnlock(prefs);
    if(prefs) CFRelease(prefs);
    return ret;
}


IOReturn IOPMCancelScheduledPowerEvent(
    CFDateRef time_to_wake, 
    CFStringRef my_id, 
    CFStringRef wake_or_restart)
{
    CFDictionaryRef         package = 0;
    SCPreferencesRef        prefs = 0;
    bool                    changed = false;
    IOReturn                ret = kIOReturnError;
        
    if(!inputsValid(time_to_wake, my_id, wake_or_restart)) 
    {
        ret = kIOReturnBadArgument;
        goto exit;
    }

    package = _IOPMCreatePowerOnDictionary(
                            CFDateGetAbsoluteTime(time_to_wake), 
                            my_id, wake_or_restart);
    if(!package) goto exit;

    // Open the prefs file and grab the current array
#if TARGET_OS_EMBEDDED
    if (geteuid() != 0)
        prefs = SCPreferencesCreateWithAuthorization(0, CFSTR("IOKit-AutoWake"), 
                                CFSTR(kIOPMAutoWakePrefsPath), NULL);
    else
#endif /* TARGET_OS_EMBEDDED */
    prefs = SCPreferencesCreate( 0, CFSTR("IOKit-AutoWake"), 
                                 CFSTR(kIOPMAutoWakePrefsPath));
    if(!prefs || !SCPreferencesLock(prefs, true)) 
    {
        if(kSCStatusAccessError == SCError())
            ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
    // just remove the entry
    changed = removeEntryAndSetPrefs(prefs, wake_or_restart, package);
    
    if(changed)
    {
        // commit changes
        if(!SCPreferencesCommitChanges(prefs)) 
        {
            ret = kIOReturnError;
            goto exit;
        } else ret = kIOReturnSuccess;
    } else ret = kIOReturnNotFound;

exit:
    
    // release the lock and exit
    if(package) CFRelease(package);
    if(prefs) SCPreferencesUnlock(prefs);
    if(prefs) CFRelease(prefs);
    return ret;
}

CFArrayRef IOPMCopyScheduledPowerEvents(void)
{
    SCPreferencesRef            prefs;
    CFArrayRef                  wake_arr; 
    CFArrayRef                  poweron_arr;
    CFArrayRef                  wakeorpoweron_arr;
    CFArrayRef                  sleep_arr; 
    CFArrayRef                  shutdown_arr;
    CFArrayRef                  restart_arr;
    CFMutableArrayRef           new_arr;
    
    // Copy wakeup and restart arrays from SCPreferences
#if TARGET_OS_EMBEDDED
    if (geteuid() != 0)
        prefs = SCPreferencesCreateWithAuthorization(0, CFSTR("IOKit-AutoWake"), 
                                CFSTR(kIOPMAutoWakePrefsPath), NULL);
    else
#endif /* TARGET_OS_EMBEDDED */
    prefs = SCPreferencesCreate(0, CFSTR("IOKit-AutoWake"), CFSTR(kIOPMAutoWakePrefsPath));

    if(!prefs) return NULL;

    wake_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoWake)));
    poweron_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoPowerOn)));
    wakeorpoweron_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoWakeOrPowerOn)));
    sleep_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoSleep)));
    shutdown_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoShutdown)));
    restart_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoRestart)));
    
    new_arr = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if(wake_arr) {
        CFArrayAppendArray(new_arr, wake_arr, 
                        CFRangeMake(0, CFArrayGetCount(wake_arr)));
    }
    if(poweron_arr) {   
        CFArrayAppendArray(new_arr, poweron_arr, 
                        CFRangeMake(0, CFArrayGetCount(poweron_arr)));
    }
    if(wakeorpoweron_arr) {   
        CFArrayAppendArray(new_arr, wakeorpoweron_arr, 
                        CFRangeMake(0, CFArrayGetCount(wakeorpoweron_arr)));
    }
    if(sleep_arr) {
        CFArrayAppendArray(new_arr, sleep_arr, 
                        CFRangeMake(0, CFArrayGetCount(sleep_arr)));
    }
    if(shutdown_arr) {
        CFArrayAppendArray(new_arr, shutdown_arr, 
                        CFRangeMake(0, CFArrayGetCount(shutdown_arr)));
    }
    if(restart_arr) {
        CFArrayAppendArray(new_arr, restart_arr, 
                        CFRangeMake(0, CFArrayGetCount(restart_arr)));
    }

    CFRelease(prefs);
    
    // Return NULL if there are 0 entries in the array
    if(!new_arr) 
    {
        return NULL;
    } else {
        if(0 == CFArrayGetCount(new_arr))
        {
            CFRelease(new_arr);
            return NULL;
        }
    }    
    
    // Return the combined arrays
    return (CFArrayRef)new_arr;
}
