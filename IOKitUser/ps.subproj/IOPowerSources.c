/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 * HISTORY
 *
 */


#include <sys/cdefs.h>
#include <notify.h>

#include <CoreFoundation/CoreFoundation.h>
#include "IOSystemConfiguration.h"
#include "IOPSKeys.h"
#include "IOPowerSources.h"
#include "IOPowerSourcesPrivate.h"
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <notify.h>

#ifndef kIOPSDynamicStorePathKey
#define kIOPSDynamicStorePathKey kIOPSDynamicStorePath
#endif

#ifndef kIOPSDynamicStoreLowBattPathKey
#define kIOPSDynamicStoreLowBattPathKey "/IOKit/LowBatteryWarning"
#endif

#ifndef kIOPSDynamicStorePowerAdapterKey
#define kIOPSDynamicStorePowerAdapterKey "/IOKit/PowerAdapter"
#endif


IOPSLowBatteryWarningLevel IOPSGetBatteryWarningLevel(void)
{
    SCDynamicStoreRef   store = NULL;
    CFStringRef         key = NULL;
    CFNumberRef         scWarnValue = NULL;
    int                 return_level = kIOPSLowBatteryWarningNone;

    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                            CFSTR("IOKit Power Source Copy"), NULL, NULL);
    if (!store) 
        goto SAD_EXIT;

    key = SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault, 
                            CFSTR("%@%@"),
                            _io_kSCDynamicStoreDomainState, 
                            CFSTR(kIOPSDynamicStoreLowBattPathKey));
    if (!key)
        goto SAD_EXIT;
        
    scWarnValue = isA_CFNumber(SCDynamicStoreCopyValue(store, key));
    if (scWarnValue) {

        CFNumberGetValue(scWarnValue, kCFNumberIntType, &return_level);    
    
        CFRelease(scWarnValue);
        scWarnValue = NULL;
    }

SAD_EXIT:
    if (store) CFRelease(store);
    if (key) CFRelease(key);
    return return_level;
}


// powerd uses these same constants to define the bitfields in the
// kIOPSTimeRemainingNotificationKey key
#define     _kPSTimeRemainingNotifyExternalBit       (1 << 16)
#define     _kPSTimeRemainingNotifyChargingBit       (1 << 17)
#define     _kPSTimeRemainingNotifyUnknownBit        (1 << 18)
#define     _kPSTimeRemainingNotifyValidBit          (1 << 19)
#define     _kSecondsPerMinute                       ((CFTimeInterval)60.0)
CFTimeInterval IOPSGetTimeRemainingEstimate(void)
{
    int                 myNotifyToken = 0;
    uint64_t            packedBatteryData = 0;
    int                 myNotifyStatus = 0;

    myNotifyStatus = notify_register_check(kIOPSTimeRemainingNotificationKey, &myNotifyToken);

    if (NOTIFY_STATUS_OK != myNotifyStatus) {
        // FAILURE: We return an optimistic unlimited time remaining estimate 
        // if we don't know the truth.
        return kIOPSTimeRemainingUnlimited;
    }

    notify_get_state(myNotifyToken, &packedBatteryData);

    notify_cancel(myNotifyToken);

    if (!(packedBatteryData & _kPSTimeRemainingNotifyValidBit)
        || (packedBatteryData & _kPSTimeRemainingNotifyExternalBit)) {
        return kIOPSTimeRemainingUnlimited;
    }

    if (packedBatteryData & _kPSTimeRemainingNotifyUnknownBit) {
        return kIOPSTimeRemainingUnknown;
    }
    
    return (_kSecondsPerMinute * (CFTimeInterval)(packedBatteryData & 0xFFFF));
}


CFDictionaryRef IOPSCopyExternalPowerAdapterDetails(void)
{
    SCDynamicStoreRef   store = NULL;
    CFStringRef         key = NULL;
    CFDictionaryRef     ret_dict = NULL;

    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                            CFSTR("IOKit Power Source Copy"), NULL, NULL);
    if (!store) 
        goto SAD_EXIT;

    key = SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault, 
                            CFSTR("%@%@"),
                            _io_kSCDynamicStoreDomainState, 
                            CFSTR(kIOPSDynamicStorePowerAdapterKey));
    if (!key)
        goto SAD_EXIT;
        
    ret_dict = isA_CFDictionary(SCDynamicStoreCopyValue(store, key));

SAD_EXIT:
    if (store) CFRelease(store);
    if (key) CFRelease(key);
    return ret_dict;
}
/***
 Returns a blob of Power Source information in an opaque CFTypeRef. Clients should
 not actually look directly at data in the CFTypeRef - they should use the accessor
 functions IOPSCopyPowerSourcesList and IOPSGetPowerSourceDescription, instead.
 Returns NULL if errors were encountered.
 Return: Caller must CFRelease() the return value when done.
***/
CFTypeRef IOPSCopyPowerSourcesInfo(void) {
    SCDynamicStoreRef   store = NULL;
    CFStringRef         ps_match = NULL;
    CFMutableArrayRef   ps_arr = NULL;
    CFDictionaryRef     power_sources = NULL;
    
    // Open connection to SCDynamicStore
    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                CFSTR("IOKit Power Source Copy"), NULL, NULL);
    if(!store) { 
        goto exit;
     }
    // Create regular expression to match all Power Sources
    ps_match = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@/%@"),
                _io_kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePathKey), _io_kSCCompAnyRegex);
    if(!ps_match) {
        goto exit;
    }
    ps_arr = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if(!ps_arr) {
        goto exit;
    }
    CFArrayAppendValue(ps_arr, ps_match);
    
    // Copy multiple Power Sources into dictionary
    power_sources = SCDynamicStoreCopyMultiple(store, NULL, ps_arr);
    
exit:

    if (ps_match)
        CFRelease(ps_match);
    if (ps_arr)
        CFRelease(ps_arr);
    if (store)
        CFRelease(store);

    if(!power_sources) {
        // On failure, we return an empty dictionary instead of NULL
        power_sources = CFDictionaryCreate( kCFAllocatorDefault, 
                                  NULL, NULL, 0, 
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);
    }

    // Return CFDictionary as opaque CFTypeRef
    return (CFTypeRef)power_sources;
}

/***
 Arguments - Takes the CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 Returns a CFArray of Power Source handles, each of type CFTypeRef.
 The caller shouldn't look directly at the CFTypeRefs, but should use
 IOPSGetPowerSourceDescription on each member of the CFArray.
 Returns NULL if errors were encountered.
 Return: Caller must CFRelease() the returned CFArray.
***/
CFArrayRef IOPSCopyPowerSourcesList(CFTypeRef blob) {
    int                 count;
    void                **keys;
    CFArrayRef          arr;
    bool                failure = false;

    // Check that the argument is actually a CFDictionary
    if( !blob 
        || (CFGetTypeID(blob) != CFDictionaryGetTypeID()) ) 
    {
        failure = true;
        goto exit;
    }
    
    // allocate buffers for keys and values
    count = CFDictionaryGetCount((CFDictionaryRef)blob);    
    keys = (void **)malloc(count * sizeof(void *));
    if(!keys) {
        failure = true;
        goto exit;
    }

    // Get keys and values from CFDictionary
    CFDictionaryGetKeysAndValues((CFDictionaryRef)blob, (const void **)keys, NULL);
    
    // Create CFArray from keys
    arr = CFArrayCreate(kCFAllocatorDefault, (const void **)keys, count, &kCFTypeArrayCallBacks);
    
    // free keys and values
    free(keys);
exit:
    if(failure) {
        // On failure, we return an empty array instead of NULL
        arr = CFArrayCreate( 0, NULL, 0, &kCFTypeArrayCallBacks);
    }
    // Return CFArray
    return arr;
}

/***
 Arguments - Takes one of the CFTypeRefs in the CFArray returned by
 IOPSCopyPowerSourcesList
 Returns a CFDictionary with specific information about the power source.
 See IOKit.framework/Headers/ups/IOUPSKeys.h for specific fields.
 Return: Caller should not CFRelease the returned CFArray
***/
CFDictionaryRef IOPSGetPowerSourceDescription(CFTypeRef blob, CFTypeRef ps) {
    // Check that the info is a CFDictionary
    if( !(blob && (CFGetTypeID(blob)==CFDictionaryGetTypeID())) ) 
        return NULL;
    
    // Check that the Power Source is a CFString
    if( !(ps && (CFGetTypeID(ps)==CFStringGetTypeID())) ) 
        return NULL;
    
    // Extract the CFDictionary of Battery Info
    // and return
    return CFDictionaryGetValue(blob, ps);
}

static CFStringRef getPowerSourceState(CFTypeRef blob, CFTypeRef id)
{
    CFDictionaryRef the_dict = IOPSGetPowerSourceDescription(blob, id);
    return CFDictionaryGetValue(the_dict, CFSTR(kIOPSPowerSourceStateKey));
}

/* IOPSGetProvidingPowerSourceType
 * Argument: 
 *  ps_blob: as returned from IOPSCopyPowerSourcesInfo()
 * Returns: 
 *  The current system power source.
 *  CFSTR("AC Power"), CFSTR("Battery Power"), CFSTR("UPS Power")
 */
CFStringRef IOPSGetProvidingPowerSourceType(CFTypeRef ps_blob)
{
    CFTypeRef       the_ups = NULL;
    CFTypeRef       the_batt = NULL;
    CFStringRef     ps_state = NULL;
    
    
    if(kCFBooleanFalse == IOPSPowerSourceSupported(ps_blob, CFSTR(kIOPMBatteryPowerKey)))
    {
        if(kCFBooleanFalse == IOPSPowerSourceSupported(ps_blob, CFSTR(kIOPMUPSPowerKey))) {
            // no batteries, no UPS -> AC Power
            return CFSTR(kIOPMACPowerKey);
        } else {
            // optimization opportunity: needless loops inside IOPSGetActiveUPS
            the_ups = IOPSGetActiveUPS(ps_blob);
            if(!the_ups) return CFSTR(kIOPMACPowerKey);
            ps_state = getPowerSourceState(ps_blob, the_ups);
            if(ps_state && CFEqual(ps_state, CFSTR(kIOPSACPowerValue)))
            {
                // no batteries, yes UPS, UPS is running off of AC power -> AC Power
                return CFSTR(kIOPMACPowerKey);
            } else if(ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue)))
            {
                // no batteries, yes UPS, UPS is running off of Battery power -> UPS Power
                return CFSTR(kIOPMUPSPowerKey);
            }
            
        }
        // Error in the data we were passed
        return CFSTR(kIOPMACPowerKey);
    } else {
        // Optimization opportunity: needless loops inside IOPSGetActiveBattery
        the_batt = IOPSGetActiveBattery(ps_blob);
        if(!the_batt) return CFSTR(kIOPMACPowerKey);
        ps_state = getPowerSourceState(ps_blob, the_batt);
        if(ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue)))
        {
            // Yes batteries, yes running on battery power -> Battery power
            return CFSTR(kIOPMBatteryPowerKey);
        } else {
            // batteries are on AC power. let's check UPS.
            // optimize.
            if(kCFBooleanFalse == IOPSPowerSourceSupported(ps_blob, CFSTR(kIOPMUPSPowerKey)))
            {
                // yes batteries on AC power, no UPS -> AC Power
                return CFSTR(kIOPMACPowerKey);
            } else {
                the_ups = IOPSGetActiveUPS(ps_blob);
                if(!the_ups) return CFSTR(kIOPMACPowerKey);
                ps_state = getPowerSourceState(ps_blob, the_ups);
                if(ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue)))                
                {
                    // yes batteries on AC power, UPS is on battery power -> UPS Power
                    return CFSTR(kIOPMUPSPowerKey);
                } else if(ps_state && CFEqual(ps_state, CFSTR(kIOPSACPowerValue)))
                {
                    // yes batteries on AC Power, UPS is on AC Power -> AC Power
                    return CFSTR(kIOPMACPowerKey);
                }
            }
        }
    }
    
    // Should not reach this point. Return something safe.
    return CFSTR(kIOPMACPowerKey);
}


/***
 Support structures and functions for IOPSNotificationCreateRunLoopSource
***/
typedef struct {
    IOPowerSourceCallbackType       callback;
    void                            *context;
} user_callback_context;

void ioCallout(SCDynamicStoreRef store __unused, CFArrayRef keys __unused, void *ctxt) {
    user_callback_context	*c;
    IOPowerSourceCallbackType cb;

    c = (user_callback_context *)CFDataGetBytePtr((CFDataRef)ctxt);
    if(!c) return;
    cb = c->callback;
    if(!cb) return;    
    (*cb)(c->context);
}

/***
 Returns a CFRunLoopSourceRef that notifies the caller when power source
 information changes.
 Arguments:
    IOPowerSourceCallbackType callback - A function to be called whenever any power source is added, removed, or changes
    void *context - Any user-defined pointer, passed to the IOPowerSource callback.
 Returns NULL if there were any problems.
 Caller must CFRelease() the returned value.
***/
CFRunLoopSourceRef IOPSNotificationCreateRunLoopSource(IOPowerSourceCallbackType callback, void *context) {
    SCDynamicStoreRef           store = NULL;
    CFStringRef                 ps_match = NULL;
    CFMutableArrayRef           ps_arr = NULL;
    CFRunLoopSourceRef          SCDrls = NULL;
    user_callback_context       *ioContext = NULL;
    SCDynamicStoreContext       scContext = {0, NULL, CFRetain, CFRelease, NULL};

    if(!callback) return NULL;

    scContext.info = CFDataCreateMutable(NULL, sizeof(user_callback_context));
    CFDataSetLength(scContext.info, sizeof(user_callback_context));
    ioContext = (user_callback_context *)CFDataGetBytePtr(scContext.info); 
    ioContext->context = context;
    ioContext->callback = callback;
        
    // Open connection to SCDynamicStore. User's callback as context.
    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                CFSTR("IOKit Power Source Copy"), ioCallout, (void *)&scContext);
    if(!store) return NULL;
     
    // Create regular expression to match all Power Sources
    ps_match = SCDynamicStoreKeyCreate(
                    kCFAllocatorDefault, 
                    CFSTR("%@%@/%@"), 
                    _io_kSCDynamicStoreDomainState, 
                    CFSTR(kIOPSDynamicStorePath), 
                    _io_kSCCompAnyRegex);
    if(!ps_match) return NULL;
    ps_arr = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if(!ps_arr) return NULL;
    CFArrayAppendValue(ps_arr, ps_match);
    CFRelease(ps_match);
    
    // Set up regular expression notifications
    SCDynamicStoreSetNotificationKeys(store, NULL, ps_arr);
    CFRelease(ps_arr);

    // Obtain the CFRunLoopSourceRef from this SCDynamicStoreRef session
    SCDrls = SCDynamicStoreCreateRunLoopSource(kCFAllocatorDefault, store, 0);
    CFRelease(store);    

    return SCDrls;
}
