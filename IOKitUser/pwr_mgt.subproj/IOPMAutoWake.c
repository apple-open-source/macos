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

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include "IOPMKeys.h"
#include "IOPMLib.h"
#include "IOPMLibPrivate.h"

enum {
    kIOPMMaxScheduledEntries = 1000
};

static CFComparisonResult compare_dates(CFDictionaryRef a1, CFDictionaryRef a2, void *c)
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

static CFAbsoluteTime roundOffDate(CFAbsoluteTime time)
{
    // round time down to the closest multiple of 30 seconds
    // CFAbsoluteTimes are encoded as doubles
    return (CFAbsoluteTime)nearbyint((time - fmod(time, (double)30.0)));
}

static CFDictionaryRef _IOPMCreatePowerOnDictionary(CFAbsoluteTime the_time, CFStringRef the_id, CFStringRef type)
{
    CFMutableDictionaryRef          d;
    CFDateRef	                    the_date;

    // make sure my_id is valid or NULL
    the_id = isA_CFString(the_id);        
    // round wakeup time to last 30 second increment
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

static bool inputsValid(CFDateRef time_to_wake, CFStringRef my_id, CFStringRef type)
{
    // NULL is an acceptable input for my_id
    if(!isA_CFDate(time_to_wake)) return false;
    if(!isA_CFString(type)) return false;
    if(!(CFEqual(type, CFSTR(kIOPMAutoWake)) || 
        CFEqual(type, CFSTR(kIOPMAutoPowerOn)) ||
        CFEqual(type, CFSTR(kIOPMAutoWakeOrPowerOn)) ||
        CFEqual(type, CFSTR(kIOPMAutoSleep)) ||
        CFEqual(type, CFSTR(kIOPMAutoShutdown))))
    {
        return false;
    }

    return true;
}

static bool addEntryAndSetPrefs(SCPreferencesRef prefs, CFStringRef type, CFDictionaryRef package)
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
        CFArraySortValues(new_arr, CFRangeMake(0, CFArrayGetCount(new_arr)), (CFComparatorFunction)compare_dates, 0);
    } else
    {
        // There is not already an array in the prefs file. Create one with this entry.
        new_arr = (CFMutableArrayRef)CFArrayCreate(0, (const void **)&package, 1, &kCFTypeArrayCallBacks);
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
static bool removeEntryAndSetPrefs(SCPreferencesRef prefs, CFStringRef type, CFDictionaryRef package)
{
    CFArrayRef                  arr = 0;
    CFMutableArrayRef           mut_arr = 0;
    CFIndex                     i;
    CFDictionaryRef             cancelee = 0;
    bool                        ret = false;

    // Grab the specific array from which we want to remove the entry... wakeup or poweron?
    // or both?
    arr = isA_CFArray(SCPreferencesGetValue(prefs, type));
    if(!arr) 
    {
        ret = true;
        goto exit;
    }

    i = CFArrayBSearchValues(arr, CFRangeMake(0, CFArrayGetCount(arr)), package,
                            (CFComparatorFunction)compare_dates, 0);
    
    // did it return an index within the array?                        
    if(0 <= i <= CFArrayGetCount(arr))
    {
        cancelee = CFArrayGetValueAtIndex(arr, i);
        // is the date at that index equal to the date to cancel?
        if(kCFCompareEqualTo == compare_dates(package, cancelee, 0))
        {
            // We have confirmation on the dates and types being equal. Check the id.
            // BUG: May have trouble if multiple apps have scheduled a wakeup at the same time.
            if(kCFCompareEqualTo == CFStringCompare(CFDictionaryGetValue(package, CFSTR(kIOPMPowerEventAppNameKey)), 
                                                    CFDictionaryGetValue(cancelee, CFSTR(kIOPMPowerEventAppNameKey)), 0))
            {
                // This is the one to cancel
                mut_arr = CFArrayCreateMutableCopy(0, 0, arr);
                CFArrayRemoveValueAtIndex(mut_arr, i);
                if(!SCPreferencesSetValue(prefs, type, mut_arr)) 
                { 
                    ret = false;
                    goto exit;
                }

                // success!
                ret = true;
            }
        }
    }
    
    exit:
    if(mut_arr) CFRelease(mut_arr);
    return ret;
}


extern IOReturn IOPMSchedulePowerEvent(CFDateRef time_to_wake, CFStringRef my_id, CFStringRef type)
{
    CFDictionaryRef         package = 0;
    SCPreferencesRef        prefs = 0;
    IOReturn                ret = false;
    CFArrayRef              tmp_wakeup_arr;
    int                     total_count = 0;
    CFAbsoluteTime          abs_time_to_wake;

    //  verify inputs
    if(!inputsValid(time_to_wake, my_id, type))
    {        
        ret = kIOReturnBadArgument;
        goto exit;
    }

    abs_time_to_wake = CFDateGetAbsoluteTime(time_to_wake);
    if(abs_time_to_wake < (CFAbsoluteTimeGetCurrent() + 30.0))
    {
        ret = kIOReturnNotReady;
        goto exit;
    }
    
    //  package the event in a CFDictionary
    package = _IOPMCreatePowerOnDictionary(abs_time_to_wake, my_id, type);

    // Open the prefs file and grab the current array
    prefs = SCPreferencesCreate(0, CFSTR("IOKit-AutoWake"), CFSTR(kIOPMAutoWakePrefsPath));
    if(!prefs || !SCPreferencesLock(prefs, true))
    {
        if(kSCStatusAccessError == SCError())
            ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
    // Examine number of entries currently in disk and bail if too many
    total_count = 0;
    tmp_wakeup_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoPowerOn)));
    if(tmp_wakeup_arr) total_count += CFArrayGetCount(tmp_wakeup_arr);
    tmp_wakeup_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoWake)));
    if(tmp_wakeup_arr) total_count += CFArrayGetCount(tmp_wakeup_arr);
    tmp_wakeup_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoSleep)));
    if(tmp_wakeup_arr) total_count += CFArrayGetCount(tmp_wakeup_arr);
    tmp_wakeup_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoShutdown)));
    if(tmp_wakeup_arr) total_count += CFArrayGetCount(tmp_wakeup_arr);
    if(total_count >= kIOPMMaxScheduledEntries)
    {
        ret = kIOReturnNoSpace;
        goto exit;
    }

    // add the CFDictionary to the CFArray in SCPreferences file
    if(CFEqual(type, CFSTR(kIOPMAutoWakeOrPowerOn)))
    {
        // add to both lists    
        addEntryAndSetPrefs(prefs, CFSTR(kIOPMAutoWake), package);
        addEntryAndSetPrefs(prefs, CFSTR(kIOPMAutoPowerOn), package);
    } else {
        // just add the entry to the one (wake or power on)
        addEntryAndSetPrefs(prefs, type, package);
    }

    // Add a warning to the file
    SCPreferencesSetValue(prefs, CFSTR("WARNING"), CFSTR("Do not edit this file by hand - it must remain in sorted-by-date order."));

    //  commit the SCPreferences file out to disk
    if(!SCPreferencesCommitChanges(prefs))
    {
        ret = kIOReturnError;
        goto exit;
    }

    ret = kIOReturnSuccess;

    exit:
    if(package) CFRelease(package);
    if(prefs) SCPreferencesUnlock(prefs);
    if(prefs) CFRelease(prefs);
    return ret;
}

extern IOReturn IOPMCancelScheduledPowerEvent(CFDateRef time_to_wake, CFStringRef my_id, CFStringRef wake_or_restart)
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

    package = _IOPMCreatePowerOnDictionary(CFDateGetAbsoluteTime(time_to_wake), my_id, wake_or_restart);
    if(!package) goto exit;

    // Open the prefs file and grab the current array
    prefs = SCPreferencesCreate(0, CFSTR("IOKit-AutoWake"), CFSTR(kIOPMAutoWakePrefsPath));
    if(!prefs || !SCPreferencesLock(prefs, true)) 
    {
        if(kSCStatusAccessError == SCError())
            ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
   // remove the CFDictionary from the CFArray in SCPreferences file
    if(CFEqual(wake_or_restart, CFSTR(kIOPMAutoWakeOrPowerOn)))
    {
        // add to both lists    
        changed = removeEntryAndSetPrefs(prefs, CFSTR(kIOPMAutoWake), package);
        changed |= removeEntryAndSetPrefs(prefs, CFSTR(kIOPMAutoPowerOn), package);
    } else {
        // just add the entry to the one (wake or power on or sleep or shutdown)
        changed = removeEntryAndSetPrefs(prefs, wake_or_restart, package);
    }
    
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

extern CFArrayRef IOPMCopyScheduledPowerEvents
(void)
{
    SCPreferencesRef            prefs;
    CFArrayRef                  wake_arr, poweron_arr, sleep_arr, shutdown_arr;
    CFMutableArrayRef           new_arr;
    
    // Copy wakeup and restart arrays from SCPreferences
    prefs = SCPreferencesCreate(0, CFSTR("IOKit-AutoWake"), CFSTR(kIOPMAutoWakePrefsPath));
    if(!prefs) return NULL;

    wake_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoWake)));
    poweron_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoPowerOn)));
    sleep_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoSleep)));
    shutdown_arr = isA_CFArray(SCPreferencesGetValue(prefs, CFSTR(kIOPMAutoShutdown)));
    
    new_arr = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if(wake_arr) CFArrayAppendArray(new_arr, wake_arr, CFRangeMake(0, CFArrayGetCount(wake_arr)));
    if(poweron_arr) CFArrayAppendArray(new_arr, poweron_arr, CFRangeMake(0, CFArrayGetCount(poweron_arr)));
    if(sleep_arr) CFArrayAppendArray(new_arr, sleep_arr, CFRangeMake(0, CFArrayGetCount(sleep_arr)));
    if(shutdown_arr) CFArrayAppendArray(new_arr, shutdown_arr, CFRangeMake(0, CFArrayGetCount(shutdown_arr)));

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
