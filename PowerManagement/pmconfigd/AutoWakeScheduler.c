/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include "PrivateLib.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"

static CFMutableArrayRef    wakeup_arr = 0;
static CFMutableArrayRef    poweron_arr = 0;
static CFMutableArrayRef    sleep_arr = 0;
static CFMutableArrayRef    shutdown_arr = 0;

static CFRunLoopTimerRef    wakeup_timer = 0;
static CFRunLoopTimerRef    poweron_timer = 0;
static CFRunLoopTimerRef    sleep_timer = 0;
static CFRunLoopTimerRef    shutdown_timer = 0;

#define kIOPMUAutoWakeKey   CFSTR("AutoWakePriv")
#define kIOPMUAutoPowerKey  CFSTR("AutoPowerPriv")

#define PMU_MAGIC_PASSWORD    0x0101BEEF

enum {
    kIOWakeTimer = 0,
    kIOPowerOnTimer = 1,
    kIOSleepTimer = 2,
    kIOShutdownTimer = 3
};

/* AutoWakeScheduler overview
 * 
 * PURPOSE
 * Set PMU wake and power on registers to automatically power on the machine at a 
 * user/app requested time. 
 * Requests come via IOKit/pwr_mgt/IOPMLib.h:IOPMSchedulePowerEvent()
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

// Find an io_registry_entry_t for either the PMU or Cuda
// PMU or Cuda will be responsible for doing the actual wake-from-sleep
// or restart from power-off
static io_registry_entry_t 
getCudaPMURef(void)
{
    io_iterator_t               tmp = NULL;
    io_registry_entry_t         cudaPMU = NULL;
        
    // Search for PMU
    IOServiceGetMatchingServices(0, IOServiceNameMatching("ApplePMU"), &tmp);
    if(tmp) {
        cudaPMU = IOIteratorNext(tmp);
        //if(cudaPMU) magicCookie = kAppleMagicPMUCookie;
        IOObjectRelease(tmp);
    }

    // No? Search for Cuda
    if(!cudaPMU) {
        IOServiceGetMatchingServices(0, IOServiceNameMatching("AppleCuda"), &tmp);
        if(tmp) {
            cudaPMU = IOIteratorNext(tmp);
            //if(cudaPMU) magicCookie = kAppleMagicCudaCookie;
            IOObjectRelease(tmp);
        }
    }
    return cudaPMU;
}

// Boring PMU data massaging
kern_return_t
writeDataProperty(io_object_t handle, CFStringRef name,
                  unsigned char * bytes, unsigned int size)
{
    kern_return_t               kr = kIOReturnNoMemory;
    CFDataRef                   data;
    CFMutableDictionaryRef      dict = 0;

    do {
        // stuff the unsigned int into a CFData
        data = CFDataCreate( kCFAllocatorDefault, bytes, size );
        if( !data) continue;

        // Stuff the CFData into a CFDictionary
        dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if( !dict) continue;
        CFDictionarySetValue( dict, name, data );

        // And send it to the PMU via IOConnectSetCFProperties
        kr = IOConnectSetCFProperties( handle, dict );
        if (kr != KERN_SUCCESS)
        {
            //syslog(LOG_INFO, "IOPMAutoWake: IOConnectSetCFProperties fails\n");
        }
    } while( false );

    if( data)
        CFRelease(data);
    if( dict)
        CFRelease(dict);

    return( kr );
}

static kern_return_t
writePMUProperty(io_object_t pmuReference, CFStringRef propertyName, void *data, size_t *dataSize)
{
    io_connect_t conObj;
    kern_return_t kr = IOServiceOpen(pmuReference, mach_task_self(), PMU_MAGIC_PASSWORD, &conObj);

    if (kr == kIOReturnSuccess) {
        kr = writeDataProperty(conObj, propertyName, (unsigned char*)data, *dataSize);
        IOServiceClose(conObj);
    }

    return kr;
}

// isEntryValidAndFuturistic
// Returns true if the CFDictionary is validly formed
//     AND if the date is in the future
// Returns false if anything about the dictionary is invalid
//     OR if the CFDate is prior to the current time
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
        wakeup_date = isA_CFDate(CFDictionaryGetValue(wakeup_dict, CFSTR(kIOPMPowerEventTimeKey)));
        if(!wakeup_date || (kCFCompareLessThan == CFDateCompare(wakeup_date, date_now, 0)))
        {
            // date is too early
            ret = false;
        }
        // otherwise date is after now, and ret = true
    }

    return ret;
}

// Purge past wakeup times
// Does not care whether its operating on wakeup or poweron array.
// Just purges all entries with a time < now
// returns true on success, false on any failure
static bool 
purgePastWakeupTimes(CFMutableArrayRef arr, CFStringRef which)
{
    int                 i;
    bool                array_has_changed = false;
    CFDateRef           date_now;
    bool                ret;
    SCPreferencesRef    prefs = 0;
    
    if(!arr || (0 == CFArrayGetCount(arr))) return true;
    
    // Note: the value of CFArrayGetcount(arr) can change (decrease)
    // over iterations of this loop. Don't try to optimize that part out
    // of the while()
    i = 0;
    date_now = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
    do {
            if(!isEntryValidAndFuturistic(CFArrayGetValueAtIndex(arr, i), date_now))
            {
                // Remove entry from the array - its time has past
                CFArrayRemoveValueAtIndex(arr, i);
                array_has_changed = true;
                // do not increment i - higher array entries will be shifted down into its spot
            } else
            {
                // valid entry with wakeup date in the future
                // assuming the array is in sorted order, the rest of the entries
                // should also be in the future.
                break;
            }  
    } while(i < CFArrayGetCount(arr));
    CFRelease(date_now);

    if(array_has_changed)
    {
        // write new, purged array to disk
        // We should soon-after get a prefs notification to re-read the values
        prefs = SCPreferencesCreate(0, CFSTR("IOKit-AutoWake"), CFSTR(kIOPMAutoWakePrefsPath));
        if(!prefs) return 0;
        if(!SCPreferencesLock(prefs, true)) {
            ret = false;
            goto exit;
        }
        SCPreferencesSetValue(prefs, which, arr);
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

static void
copyScheduledPowerChangeArrays(void)
{
    CFArrayRef              tmp;
    SCPreferencesRef        prefs;

    prefs = SCPreferencesCreate(0, CFSTR("PM-configd-AutoWake"), CFSTR(kIOPMAutoWakePrefsPath));
    if(!prefs) return;

    tmp = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoWake)));
    if(tmp) wakeup_arr = CFArrayCreateMutableCopy(0, 0, tmp);
    tmp = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoPowerOn)));
    if(tmp) poweron_arr = CFArrayCreateMutableCopy(0, 0, tmp);
    tmp = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoSleep)));
    if(tmp) sleep_arr = CFArrayCreateMutableCopy(0, 0, tmp);    
    tmp = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoShutdown)));
    if(tmp) shutdown_arr = CFArrayCreateMutableCopy(0, 0, tmp);
    
    CFRelease(prefs);
}

// Find earliest upcoming wakeup time
static CFDictionaryRef 
findEarliestUpcomingTime(CFMutableArrayRef arr)
{
    int i=0;
    int count;
    
    if(!arr) return NULL;
    
    count = CFArrayGetCount(arr);
    CFDateRef now = CFDateCreate(0, CFAbsoluteTimeGetCurrent() + 10.0);

    while(i<count && !isEntryValidAndFuturistic(CFArrayGetValueAtIndex(arr, i), now))
    {
        i++;
    }
    CFRelease(now);
    if(i < count)
        return CFArrayGetValueAtIndex(arr, i);
    else return NULL;
}

// Sets the PMU to wakeup or power on at the time given in the dictionary.
// If the CFDictionaryRef argument is NULL, clears the PMU autowakeup register.
static bool 
tellPMUScheduledTime(CFDictionaryRef   dat, CFStringRef command)
{
    CFAbsoluteTime          now, wake_time;
    long int                diff_secs;
    int                     ret;
    size_t                  dataSize;
    io_registry_entry_t     pmuReference;

    // Assume a well-formed entry since we've been doing thorough type-checking
    // in the find & purge functions
    now = CFAbsoluteTimeGetCurrent();
    if(dat)
        wake_time = CFDateGetAbsoluteTime(CFDictionaryGetValue(dat, CFSTR(kIOPMPowerEventTimeKey)));
    else wake_time = now;
    diff_secs = lround(wake_time - now);
    if(diff_secs < 0) return false;

    dataSize = sizeof(unsigned long);
    
    // Find PMU in IORegistry
    pmuReference = getCudaPMURef();
    if(!pmuReference) {
        return false;
    }

    // Send command to the PMU to set AutoPower or AutoWake timer
    // The real magic happens in the PMU kext and PMU hardware
    ret = writePMUProperty(pmuReference, command, (unsigned long *)&diff_secs, &dataSize);
    
    //if(ret != kIOReturnSuccess)
    //    syslog(LOG_INFO, "PMconfigd: writePMUProperty error 0x%08x\n", ret);

    // Release reference to PMU
    IOObjectRelease(pmuReference);
    
    return true;
}

// Finds HID service
static kern_return_t openHIDService(mach_port_t io_master_port, io_connect_t *connection)
{
    kern_return_t  kr;
    mach_port_t ev, service, iter;
    
    kr = IOServiceGetMatchingServices(io_master_port, IOServiceMatching(kIOHIDSystemClass), &iter);
    if (kr != KERN_SUCCESS) {
        return kr;
    }

    service = IOIteratorNext(iter);    
    kr = IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &ev);    
    // These objects need to be released whether or not there is an error from IOServiceOpen, so
    // just do it up front.
    IOObjectRelease(service);
    IOObjectRelease(iter);
    if (kr != KERN_SUCCESS) {
        return kr;
    }
    
    *connection = ev;
    return kr;
}

// Wakes a dozing machine by posting a NULL HID event
// Will thus also wake displays if this code goes off while the machine is running
static void wakeDozingMachine(void)
{
    IOGPoint loc;
    kern_return_t kr;
    NXEvent nullEvent = {NX_NULLEVENT, {0, 0}, 0, -1, 0};
    static io_connect_t io_connection = MACH_PORT_NULL;

    // If the HID service has never been opened, do it now
    if (io_connection == MACH_PORT_NULL) 
    {
            kr = openHIDService(0, &io_connection);
            if (kr != KERN_SUCCESS) 
            {
                io_connection = NULL;
                return;
            }
    }
    
    // Finally, post a NULL event
    IOHIDPostEvent(io_connection, NX_NULLEVENT, loc, &nullEvent.data, FALSE, 0, FALSE);

    return;
}

static bool isRepeating(CFDictionaryRef event)
{
    CFStringRef     whose = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventAppNameKey));
 
    if(kCFCompareEqualTo == CFStringCompare(whose, CFSTR("Repeating"), 0))
        return true;   
    else
        return false;
}

static void schedulePowerOnTime(void);
static void scheduleWakeTime(void);

// CFTimer expiration handler
static void
handleTimerPowerOnReset(CFRunLoopTimerRef blah, void *info)
{
    CFDictionaryRef event = (CFDictionaryRef)info;
    CFStringRef     type;
    
    type = isA_CFString(CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTypeKey)));
    if(!type) return;

    if(kCFCompareEqualTo == CFStringCompare(type, CFSTR(kIOPMAutoWake), 0))
    {
        wakeup_timer = 0;
        wakeDozingMachine();
        if(isRepeating(event))
        {
            RepeatingAutoWakeTimeForPowerOn();
        }
    } else if(kCFCompareEqualTo == CFStringCompare(type, CFSTR(kIOPMAutoPowerOn), 0))
    {
        poweron_timer = 0;
        schedulePowerOnTime();
        if(isRepeating(event))
        {
            RepeatingAutoWakeTimeForPowerOn();
        }
    } else if(kCFCompareEqualTo == CFStringCompare(type, CFSTR(kIOPMAutoSleep), 0))
    {
        sleep_timer = 0;
        
        _askNicelyThenSleepSystem();

        if(isRepeating(event))
        {
            RepeatingAutoWakeTimeForPowerOff();
        }
    } else if(kCFCompareEqualTo == CFStringCompare(type, CFSTR(kIOPMAutoShutdown), 0))
    {
        shutdown_timer = 0;
    
        _askNicelyThenShutdownSystem();
        
        if(isRepeating(event))
        {
            RepeatingAutoWakeTimeForPowerOff();
        }
    }
}


static void
schedulePowerOnTime(void)
{
    static CFRunLoopTimerContext    tmr_context = {0, 0, CFRetain, CFRelease, 0};
    CFAbsoluteTime                  fire_time;
    CFDictionaryRef                 upcoming;
    
    // If there's a current timer out there, remove it.       
    if(poweron_timer) 
    {
        CFRunLoopTimerInvalidate(poweron_timer);
        poweron_timer = 0;
    }

    // find upcoming time
    upcoming = findEarliestUpcomingTime(poweron_arr);
    if(!upcoming)
    {
        // No scheduled events
        return;
    }
    
    tmr_context.info = (void *)upcoming;

    fire_time = CFDateGetAbsoluteTime(CFDictionaryGetValue(upcoming, CFSTR(kIOPMPowerEventTimeKey)));
    // info == 0 indicates this is the power on timer
    poweron_timer = CFRunLoopTimerCreate(0, fire_time, 0.0, 0, 
                    0, handleTimerPowerOnReset, &tmr_context);
    if(poweron_timer)
    {
        //syslog(LOG_INFO, "PMCONFIGD: TOLD TO SCHEDULE IN %f\n", (fire_time-CFAbsoluteTimeGetCurrent()));
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), poweron_timer, kCFRunLoopDefaultMode);
        CFRelease(poweron_timer);
    }
    
    // Send scheduled power-on to the PMU
    tellPMUScheduledTime(upcoming, kIOPMUAutoPowerKey);
}

static void
scheduleWakeTime(void)
{
    static CFRunLoopTimerContext    tmr_context = {0, 0, CFRetain, CFRelease, 0};
    CFAbsoluteTime                  fire_time;
    CFDictionaryRef                 upcoming;

    if(wakeup_timer) 
    {
        CFRunLoopTimerInvalidate(wakeup_timer);
        wakeup_timer = 0;
    }
    
    // find upcoming time
    upcoming = findEarliestUpcomingTime(wakeup_arr);
    if(!upcoming)
    {
        // No scheduled events
        return;
    }
    
    tmr_context.info = (void *)upcoming;    
    
    // Send scheduled power-on to the PMU
    tellPMUScheduledTime(upcoming, kIOPMUAutoWakeKey);
 
    fire_time = CFDateGetAbsoluteTime(CFDictionaryGetValue(upcoming, CFSTR(kIOPMPowerEventTimeKey)));
    // info == 1 indicates this is the wake timer
    wakeup_timer = CFRunLoopTimerCreate(0, fire_time, 0.0, 0, 
                    0, handleTimerPowerOnReset, &tmr_context);
    if(wakeup_timer)
    {
        //syslog(LOG_INFO, "PMCONFIGD: TOLD TO SCHEDULE IN %f\n", (fire_time-CFAbsoluteTimeGetCurrent()));
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), wakeup_timer, kCFRunLoopDefaultMode);
        CFRelease(wakeup_timer);
    }

}
//*******************
// Sleep time
//*******************
static void
scheduleSleepTime(void)
{
    static CFRunLoopTimerContext    tmr_context = {0, 0, CFRetain, CFRelease, 0};
    CFAbsoluteTime                  fire_time;
    CFDictionaryRef                 upcoming;

    // If there's a current timer out there, remove it.       
    if(sleep_timer) 
    {
        CFRunLoopTimerInvalidate(sleep_timer);
        sleep_timer = 0;
    }
    
    // find upcoming time
    upcoming = findEarliestUpcomingTime(sleep_arr);
    if(!upcoming)
    {
        // No scheduled events
        return;
    }

    tmr_context.info = (void *)upcoming;

    fire_time = CFDateGetAbsoluteTime(CFDictionaryGetValue(upcoming, CFSTR(kIOPMPowerEventTimeKey)));
    sleep_timer = CFRunLoopTimerCreate(0, fire_time, 0.0, 0, 
                    0, handleTimerPowerOnReset, &tmr_context);
    if(sleep_timer)
    {
        //syslog(LOG_INFO, "PMCONFIGD: TOLD TO SCHEDULE IN %f\n", (fire_time-CFAbsoluteTimeGetCurrent()));
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), sleep_timer, kCFRunLoopDefaultMode);
        CFRelease(sleep_timer);
    }
}

//*******************
// Shutdown time
//*******************
static void
scheduleShutdownTime(void)
{
    static CFRunLoopTimerContext    tmr_context = {0, 0, CFRetain, CFRelease, 0};
    CFAbsoluteTime                  fire_time;
    CFDictionaryRef                 upcoming;

    // If there's a current timer out there, remove it.       
    if(shutdown_timer) 
    {
       CFRunLoopTimerInvalidate(shutdown_timer);
       shutdown_timer = 0;
    }
    
    // find upcoming time
    upcoming = findEarliestUpcomingTime(shutdown_arr);
    if(!upcoming)
    {
        // No scheduled events
        return;
    }

    tmr_context.info = (void *)upcoming;    
    
    fire_time = CFDateGetAbsoluteTime(CFDictionaryGetValue(upcoming, CFSTR(kIOPMPowerEventTimeKey)));
    // info == 0 indicates this is the power on timer
    shutdown_timer = CFRunLoopTimerCreate(0, fire_time, 0.0, 0, 
                    0, handleTimerPowerOnReset, &tmr_context);
    if(shutdown_timer)
    {
        //syslog(LOG_INFO, "PMCONFIGD: TOLD TO SCHEDULE IN %f\n", (fire_time-CFAbsoluteTimeGetCurrent()));
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), shutdown_timer, kCFRunLoopDefaultMode);
        CFRelease(shutdown_timer);
    }
    
}




__private_extern__ void 
AutoWake_prime(void) 
{    
    // system bootup
    copyScheduledPowerChangeArrays();
    
    // purge past wakeup and restart times
    purgePastWakeupTimes(wakeup_arr, CFSTR(kIOPMAutoWake));
    purgePastWakeupTimes(poweron_arr, CFSTR(kIOPMAutoPowerOn));
    purgePastWakeupTimes(sleep_arr, CFSTR(kIOPMAutoSleep));
    purgePastWakeupTimes(shutdown_arr, CFSTR(kIOPMAutoShutdown));
    
    // schedule next power changes
    scheduleWakeTime();
    schedulePowerOnTime();
    scheduleSleepTime();
    scheduleShutdownTime();
}
 
__private_extern__ void 
AutoWakeSleepWakeNotification(natural_t message_type) 
{    
    switch (message_type) {
        case kIOMessageSystemWillSleep:
            // scheduleWakeupTime
            scheduleWakeTime();
            break;

        case kIOMessageSystemHasPoweredOn:
            // scan for past-wakeup events, yank 'em from the queue
            purgePastWakeupTimes(wakeup_arr, CFSTR(kIOPMAutoWake));
            purgePastWakeupTimes(poweron_arr, CFSTR(kIOPMAutoPowerOn));
            purgePastWakeupTimes(sleep_arr, CFSTR(kIOPMAutoSleep));
            purgePastWakeupTimes(shutdown_arr, CFSTR(kIOPMAutoShutdown));
            break;

        default:
            // don't care about CanSystemSleep
            break;
    }
}

__private_extern__ void 
AutoWakePrefsHaveChanged(void) 
{
    if(wakeup_arr) 
    {
        CFRelease(wakeup_arr);
        wakeup_arr = 0;
    }
    if(poweron_arr) 
    {
        CFRelease(poweron_arr);
        poweron_arr = 0;
    }
    if(sleep_arr) 
    {
        CFRelease(sleep_arr);
        sleep_arr = 0;
    }
    if(shutdown_arr) 
    {
        CFRelease(shutdown_arr);
        shutdown_arr = 0;
    }
    copyScheduledPowerChangeArrays();
    
    // set AutoWake=0 in case our scheduled wakeup was just cancelled
    tellPMUScheduledTime(NULL, kIOPMUAutoWakeKey);    
    // set AutoPower=0 in case our scheduled wakeup was just cancelled
    tellPMUScheduledTime(NULL, kIOPMUAutoPowerKey);

    scheduleWakeTime();
    schedulePowerOnTime();
    scheduleSleepTime();
    scheduleShutdownTime();
}


/* Provide legacy support for pre-Panther clients of the private PMU setProperties API
 * In the OS X 10.2 era, several applications (both third party and internal) are directly using the
 * ApplePMU::setProperty("AutoWake", time) private API to schedule PMU AutoWakes.
 *
 * To continue to support these apps while also providing a more reliable queued AutoWake functionality,
 * we're re-directing those old requests made directly to the PMU through the IOPMSchedulePowerEvent()
 * queueing API.
 *
 * The ApplePMU kext provides the notification that we're acting on here.
 */
__private_extern__ void 
AutoWakePMUInterestNotification(natural_t messageType, UInt32 messageArgument)
{    
    // Handle old calls to schedule AutoWakes and AutoPowers by funneling them up through 
    // the queueing API.
    // UInt32 messageArgument is a difference relative to the current time.
    if(0 == messageArgument) return;
    
    if(kIOPMUMessageLegacyAutoWake == messageType)
    {
        CFDateRef           wakey_date;
        CFAbsoluteTime      wakey_time;
        wakey_time = CFAbsoluteTimeGetCurrent() + (CFAbsoluteTime)messageArgument;
        wakey_date = CFDateCreate(kCFAllocatorDefault, wakey_time);
        IOPMSchedulePowerEvent(wakey_date, CFSTR("Legacy PMU setProperties"), CFSTR(kIOPMAutoWake));
        CFRelease(wakey_date);
    } else if(kIOPMUMessageLegacyAutoPower == messageType)
    {
        CFDateRef           wakey_date;
        CFAbsoluteTime      wakey_time;
        wakey_time = CFAbsoluteTimeGetCurrent() + (CFAbsoluteTime)messageArgument;
        wakey_date = CFDateCreate(kCFAllocatorDefault, wakey_time);
        IOPMSchedulePowerEvent(wakey_date, CFSTR("Legacy PMU setProperties"), CFSTR(kIOPMAutoPowerOn));
        CFRelease(wakey_date);
    }
    
}


