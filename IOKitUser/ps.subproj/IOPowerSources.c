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

#if TARGET_OS_EMBEDDED
#define kIOPSDynamicStoreFullPath "State:/IOKit/PowerSources/InternalBattery-0"
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

static CFArrayRef CreatePSKeysArray(void)
{
    CFStringRef                 ps_match = NULL;
    CFMutableArrayRef           ps_arr = NULL;
    
#if TARGET_OS_EMBEDDED
    // Doing a regex match on iOS is unnecessary as there is always only 1
    // power source, whose identity is known.
    // Optimization for <rdar://problem/11177160>
    ps_match = SCDynamicStoreKeyCreate(kCFAllocatorDefault,
                                       CFSTR(kIOPSDynamicStoreFullPath));
#else
    // Create regular expression to match all Power Sources
    ps_match = SCDynamicStoreKeyCreate(
                                       kCFAllocatorDefault,
                                       CFSTR("%@%@/%@"),
                                       _io_kSCDynamicStoreDomainState,
                                       CFSTR(kIOPSDynamicStorePath),
                                       _io_kSCCompAnyRegex);
#endif
    
    if(!ps_match) return NULL;
    ps_arr = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if(!ps_arr) return NULL;
    CFArrayAppendValue(ps_arr, ps_match);
    CFRelease(ps_match);
    
    return (CFArrayRef)ps_arr;
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
    CFArrayRef          ps_arr = NULL;
    CFDictionaryRef     power_sources = NULL;
    
    // Open connection to SCDynamicStore
    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                CFSTR("IOKit Power Source Copy"), NULL, NULL);
    if(!store) { 
        goto exit;
     }

    ps_arr = CreatePSKeysArray();

#if TARGET_OS_EMBEDDED
    // No need to pattern check on embedded. There is always only one power source
    // <rdar://problem/11177160>
    power_sources = SCDynamicStoreCopyMultiple(store, ps_arr, NULL);
#else
    // Copy multiple Power Sources into dictionary
    power_sources = SCDynamicStoreCopyMultiple(store, NULL, ps_arr);
#endif
    
exit:

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
    int                             token;
    CFMachPortRef                   mpRef;
} IOPSNotifyCallbackContext;

static void IOPSRLSMachPortCallback (CFMachPortRef port __unused, void *msg __unused, CFIndex size __unused, void *info)
{
    IOPSNotifyCallbackContext	*c = (IOPSNotifyCallbackContext *)info;
    IOPowerSourceCallbackType cb;
    
    if (c && (cb = c->callback)) {
        (*cb)(c->context);
    }
}

static void IOPSRLSMachPortRelease(const void *info)
{
    IOPSNotifyCallbackContext	*c = (IOPSNotifyCallbackContext *)info;
    
    if (c) {
        if (0 != c->token) {
            notify_cancel(c->token);
        }
        if (c->mpRef) {
            CFMachPortInvalidate(c->mpRef);
            CFRelease(c->mpRef);
        }
        free(c);
    }
}


static CFRunLoopSourceRef doCreatePSRLS(const char *notify_type, IOPowerSourceCallbackType callback, void *context)
{
    int                             status = 0;
    int                             token = 0;
    mach_port_t                     mp = MACH_PORT_NULL;
    CFMachPortRef                   mpRef = NULL;
    CFMachPortContext               mpContext;
    CFRunLoopSourceRef              mpRLS = NULL;
    IOPSNotifyCallbackContext       *ioContext;
    Boolean                         isReused = false;
    int                             giveUpRetryCount = 5;

    status = notify_register_mach_port(notify_type, &mp, 0, &token);
    if (NOTIFY_STATUS_OK != status) {
        return NULL;
    }
    
    ioContext = calloc(1, sizeof(IOPSNotifyCallbackContext));
    ioContext->callback = callback;
    ioContext->context = context;
    ioContext->token = token;

    bzero(&mpContext, sizeof(mpContext));
    mpContext.info = (void *)ioContext;
    mpContext.release = IOPSRLSMachPortRelease;
    
    do {
        if (mpRef) {
            // CFMachPorts may be reused. We don't want to get a reused mach port; so if we're unlucky enough
            // to get one, we'll pre-emptively invalidate it, throw them back in the pool, and retry.
            CFMachPortInvalidate(mpRef);
            CFRelease(mpRef);
        }
        
        mpRef = CFMachPortCreateWithPort(0, mp, IOPSRLSMachPortCallback, &mpContext, &isReused);
    } while (!mpRef && isReused && (--giveUpRetryCount > 0));

    if (mpRef) {
        if (!isReused) {
            // A reused mach port is a failure; it'll have an invalid callback pointer associated with it.
            ioContext->mpRef = mpRef;
            mpRLS = CFMachPortCreateRunLoopSource(0, mpRef, 0);
        }
        CFRelease(mpRef);
    }
    
    return mpRLS;
}

CFRunLoopSourceRef IOPSNotificationCreateRunLoopSource(IOPowerSourceCallbackType callback, void *context) {
    return doCreatePSRLS(kIOPSNotifyTimeRemaining, callback, context);
}

CFRunLoopSourceRef IOPSCreateLimitedPowerNotification(IOPowerSourceCallbackType callback, void *context) {
    return doCreatePSRLS(kIOPSNotifyPowerSource, callback, context);
}

