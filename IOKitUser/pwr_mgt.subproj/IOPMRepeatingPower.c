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

#include "IOSystemConfiguration.h"
#include <CoreFoundation/CoreFoundation.h> 
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#define kIOPMRepeatingAppName               "Repeating"

/*
 * SCPreferences file format
 *     com.apple.AutoWake.xml
 *
 * - CFSTR(kIOPMRepeatingPowerOnKey)
 *      - CFSTR(kIOPMPowerEventTimeKey) = CFNumberRef (kCFNumberIntType)
 *      - CFSTR(kIOPMDaysOfWeekKey) = CFNumberRef (kCFNumberIntType)
 *      - CFSTR(kIOPMPowerEventTypeKey) = CFStringRef (kIOPMAutoSleep, kIOPMAutoShutdown, kIOPMAutoPowerOn, kIOPMAutoWake)
 * - CFSTR(kIOPMRepeatingPowerOffKey)
 *      - CFSTR(kIOPMPowerEventTimeKey) = CFNumberRef (kCFNumberIntType)
 *      - CFSTR(kIOPMDaysOfWeekKey) = CFNumberRef (kCFNumberIntType)
 *      - CFSTR(kIOPMPowerEventTypeKey) = CFStringRef (kIOPMAutoSleep, kIOPMAutoShutdown, kIOPMAutoPowerOn, kIOPMAutoWake)
 */
 
static bool is_valid_repeating_dictionary(CFDictionaryRef   event)
{
    CFNumberRef         tmp_num;
    CFStringRef         tmp_str;
    int                 val;

    if(NULL == event) return true;

    if(!isA_CFDictionary(event)) return false;
    
    tmp_num = (CFNumberRef)CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTimeKey));
    if(!isA_CFNumber(tmp_num)) return false;
    CFNumberGetValue(tmp_num, kCFNumberIntType, &val);
    if(val < 0 || val >= (24*60)) return false;

    tmp_num = (CFNumberRef)CFDictionaryGetValue(event, CFSTR(kIOPMDaysOfWeekKey));
    if(!isA_CFNumber(tmp_num)) return false;

    tmp_str = (CFStringRef)CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTypeKey));
    if(!isA_CFString(tmp_str)) return false;    

    if( (!CFEqual(tmp_str, CFSTR(kIOPMAutoSleep))) &&
        (!CFEqual(tmp_str, CFSTR(kIOPMAutoShutdown))) &&
        (!CFEqual(tmp_str, CFSTR(kIOPMAutoWakeOrPowerOn))) &&
        (!CFEqual(tmp_str, CFSTR(kIOPMAutoPowerOn))) &&
        (!CFEqual(tmp_str, CFSTR(kIOPMAutoWake))) &&
        (!CFEqual(tmp_str, CFSTR(kIOPMAutoRestart))) )
    {
        return false;
    }
    
    return true;
}

IOReturn IOPMScheduleRepeatingPowerEvent(CFDictionaryRef events)
{
    SCPreferencesRef            prefs = 0;
    IOReturn                    ret = kIOReturnError;
    CFDictionaryRef             repeating_on, repeating_off;
    
    // Validate our inputs
    if(!isA_CFDictionary(events)) return kIOReturnBadArgument;
    repeating_on = CFDictionaryGetValue(events, CFSTR(kIOPMRepeatingPowerOnKey));    
    repeating_off = CFDictionaryGetValue(events, CFSTR(kIOPMRepeatingPowerOffKey));
    if(!is_valid_repeating_dictionary(repeating_on) ||
        !is_valid_repeating_dictionary(repeating_off)) return kIOReturnBadArgument;
 
    // Toss 'em out to the disk and to PM configd. configd will do the heavy lifting.
#if TARGET_OS_EMBEDDED
    if (geteuid() != 0)
        prefs = SCPreferencesCreateWithAuthorization(0, CFSTR("IOKit-AutoWake"), 
                                CFSTR(kIOPMAutoWakePrefsPath), NULL);
    else
#endif /* TARGET_OS_EMBEDDED */
    prefs = SCPreferencesCreate(0, CFSTR("IOKit-AutoWake"), CFSTR(kIOPMAutoWakePrefsPath));

    if(!prefs || !SCPreferencesLock(prefs, true))
    {
        if(kSCStatusAccessError == SCError())
            ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
    if(repeating_on)
        SCPreferencesSetValue(prefs, CFSTR(kIOPMRepeatingPowerOnKey), repeating_on);
    else SCPreferencesRemoveValue(prefs, CFSTR(kIOPMRepeatingPowerOnKey));
    
    if(repeating_off)
        SCPreferencesSetValue(prefs, CFSTR(kIOPMRepeatingPowerOffKey), repeating_off);
    else SCPreferencesRemoveValue(prefs, CFSTR(kIOPMRepeatingPowerOffKey));

    if(!SCPreferencesCommitChanges(prefs))
    {
        ret = kIOReturnError;
        goto exit;
    }

    ret = kIOReturnSuccess;

    exit:
    if(prefs) SCPreferencesUnlock(prefs);
    if(prefs) CFRelease(prefs);
    return ret;
}


CFDictionaryRef IOPMCopyRepeatingPowerEvents(void)
{
    SCPreferencesRef            prefs;
    CFMutableDictionaryRef      return_dict = NULL;
    CFDictionaryRef             rep_power_on_dict;
    CFDictionaryRef             rep_power_off_dict;
    
    // Open SCPreferences
    // Open the prefs file and grab the current array
#if TARGET_OS_EMBEDDED
    if (geteuid() != 0)
        prefs = SCPreferencesCreateWithAuthorization(0, CFSTR("IOKit-AutoWake"), 
                                CFSTR(kIOPMAutoWakePrefsPath), NULL);
    else
#endif /* TARGET_OS_EMBEDDED */
    prefs = SCPreferencesCreate(0, CFSTR("IOKit-AutoWake"), CFSTR(kIOPMAutoWakePrefsPath));

    if(!prefs)
    {
        return_dict = NULL;
        goto exit;
    }
    
    rep_power_on_dict = isA_CFDictionary(SCPreferencesGetValue(prefs, CFSTR(kIOPMRepeatingPowerOnKey)));
    rep_power_off_dict = isA_CFDictionary(SCPreferencesGetValue(prefs, CFSTR(kIOPMRepeatingPowerOffKey)));

    if(rep_power_on_dict || rep_power_off_dict)
    {
        return_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 2,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
        // Toss them into a new dictionary
        if(rep_power_on_dict)
            CFDictionaryAddValue(return_dict, CFSTR(kIOPMRepeatingPowerOnKey), rep_power_on_dict);
        if(rep_power_off_dict)
            CFDictionaryAddValue(return_dict, CFSTR(kIOPMRepeatingPowerOffKey), rep_power_off_dict);
    } else {
        // No repeating events - just return NULL
        return_dict = NULL;
    }
    
    exit:
    if(prefs) CFRelease(prefs);

    return return_dict;
}

IOReturn IOPMCancelAllRepeatingPowerEvents(void)
{    
    SCPreferencesRef            prefs = 0;
    IOReturn                    ret = kIOReturnError;
    
#if TARGET_OS_EMBEDDED
    if (geteuid() != 0)
        prefs = SCPreferencesCreateWithAuthorization(0, CFSTR("IOKit-AutoWake"), 
                                CFSTR(kIOPMAutoWakePrefsPath), NULL);
    else
#endif /* TARGET_OS_EMBEDDED */
    prefs = SCPreferencesCreate(0, CFSTR("IOKit-AutoWake"), CFSTR(kIOPMAutoWakePrefsPath));

    if(!prefs || !SCPreferencesLock(prefs, true))
    {
        if(kSCStatusAccessError == SCError())
            ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
    SCPreferencesRemoveValue(prefs, CFSTR(kIOPMRepeatingPowerOnKey));
    SCPreferencesRemoveValue(prefs, CFSTR(kIOPMRepeatingPowerOffKey));

    if(!SCPreferencesCommitChanges(prefs))
    {
        ret = kIOReturnError;
        goto exit;
    }
    
    ret = kIOReturnSuccess;

    exit:
    if(prefs) SCPreferencesUnlock(prefs);
    if(prefs) CFRelease(prefs);
    return ret;
}
