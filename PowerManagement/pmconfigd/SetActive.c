/*
 * Copyright (c) 2005-2006 Apple Computer, Inc. All rights reserved.
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
#include <unistd.h>
#include <stdlib.h>
#include <notify.h>
#include <asl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include "PrivateLib.h"
#include "PMSettings.h"
#include "SetActive.h"
#include "BatteryTimeRemaining.h"
#include "powermanagementServer.h"

#define kIOPMAppName        "Power Management configd plugin"
#define kIOPMPrefsPath        "com.apple.PowerManagement.xml"

#define kMaxTaskAssertions          64
#define kIOPMTaskPortKey            CFSTR("task")
#define kIOPMTaskPIDKey             CFSTR("pid")
#define kIOPMTaskAssertionsKey      CFSTR("assertions")

/* kIOPMAssertionTimerRefKey
 * For internal use only.
 * Key into Assertion dictionary.
 * Records the CFRunLoopTimerRef (if any) associated with a given assertion.
 */
#define kIOPMAssertionTimerRefKey                   CFSTR("AssertTimerRef")

/*
 * ASL Log constants
 */
#define kPMASLFacility  	            "com.apple.powermanagement"
#define kPMASLPIDKey                    "Process"
#define kPMASLAssertionNameKey          "AssertionName"
#define kPMASLNewCallerValueKey         "NewValue"
#define kPMASLPreviousSystemValueKey    "PreviousSystemValue"
#define kPMASLNewSystemValueKey         "SystemValue"
#define kPMASLActionKey                 "Action"
#define kPMASLActionAssert              "Assert"
#define kPMASLActionRelease             "Release"
#define kPMASLActionClientDied          "ClientDied"
#define kPMASLActionTimedOut            "TimedOut"

enum {
    // These must be consecutive integers beginning at 0
    kHighPerfIndex              = 0,
    kPreventIdleIndex           = 1,
    kDisableInflowIndex         = 2,
    kInhibitChargeIndex         = 3,
    kDisableWarningsIndex       = 4,
    kPreventDisplaySleepIndex   = 5,
    kEnableIdleIndex            = 6,
    
    // Make sure this is the last enum element, as it tells us the total
    // number of elements in the enum definition
    kIOPMNumAssertionTypes      
};

// Selectors for AppleSmartBatteryManagerUserClient
enum {
    kSBUCInflowDisable = 0,
    kSBUCChargeInhibit = 1
};

static const int kMaxCountTimedOut = 5;


extern CFMachPortRef            pmServerMachPort;

#define DEBUG_LOG(x...) do { \
        asl_log(NULL, NULL, ASL_LEVEL_ERR, x); \
    } while(false);


// forward
static void evaluateAssertions(void);
static void calculateAggregates(void);
static void publishAssertionStatus(void);
static void sendSmartBatteryCommand(uint32_t which, uint32_t level);
static int indexForAssertionName(CFStringRef assertionName);
static void logAssertionEvent(
                const char      *assertionActionStr,
                CFDictionaryRef taskDictionary,
                CFDictionaryRef assertionDictionary);
static void timeoutExpirationCallBack(CFRunLoopTimerRef timer, void *info);
static IOReturn copyAssertionForID(
                mach_port_t             inPort,
                int                     inID,
                CFDictionaryRef         *outTask,
                CFMutableArrayRef       *outTaskAssertions,
                CFMutableDictionaryRef  *outAssertion);

static CFArrayRef                   copyPIDAssertionDictionaryFlattened(void);
static CFDictionaryRef              copyAggregateValuesDictionary(void);
static CFArrayRef                   copyTimedOutAssertionsArray(void);

// static void debugLogAssertion(CFDictionaryRef log_me);

// globals
static CFMutableDictionaryRef       gAssertionsDict = NULL;
static CFMutableArrayRef            gTimedOutArray = NULL;
static bool                         gNotifyTimeOuts = false;
static CFRunLoopTimerRef	        gUpdateAssertionStatusTimer = NULL;

static int                          aggregate_assertions[kIOPMNumAssertionTypes];
static int                          last_aggregate_assertions[kIOPMNumAssertionTypes];
static CFStringRef                  assertion_types_arr[kIOPMNumAssertionTypes];
static bool                         idle_enable_assumed = true;


static void logAssertionEvent(
                const char      *assertionActionCStr,
                CFDictionaryRef taskDictionary,
                CFDictionaryRef assertionDictionary)
{
    const int       kLongStringLen      = 100;
    const int       kShortStringLen     = 10;

    aslmsg          m;
    int             aslLogLevel         = ASL_LEVEL_INFO;
    CFNumberRef     pidNum              = NULL;
    int             app_pid             = -1;
    int             index               = 0;
    CFNumberRef     levelNum            = NULL;
    CFStringRef     assertionTypeStr    = NULL;
    uint32_t        assertionLevel      = -1;
    char            assertionTypeCString[kLongStringLen];
    char            pid_buf[kShortStringLen];
    char            level_buf[kShortStringLen];
    char            prior_system_level_buf[kShortStringLen];
    char            new_system_level_buf[kShortStringLen];


    m = asl_new(ASL_TYPE_MSG);    

    /* Log Action */
    asl_set(m, kPMASLActionKey, assertionActionCStr);

    /* Facility type */
    asl_set(m, ASL_KEY_FACILITY, kPMASLFacility);
    
    /* Log PID */
    pid_buf[0] = 0;
    if (taskDictionary) {
        // Try to read pid from Task's dictionary
        pidNum = CFDictionaryGetValue(taskDictionary, kIOPMTaskPIDKey);
    } 
    if (!pidNum && assertionDictionary) {
        // Try to read pid from Assertion's dictionary
        pidNum = CFDictionaryGetValue(assertionDictionary, kIOPMAssertionPIDKey);
    }
    if (pidNum)
    {
        if (CFNumberGetValue(pidNum, kCFNumberIntType, &app_pid)) 
        {
            if (0 < snprintf(pid_buf, kShortStringLen, "%d", app_pid)) {
                asl_set(m, kPMASLPIDKey, pid_buf);
            }
        }
    }
    

    /* Assertion details are specified for creation & release logs
     * Not for client death logs
     * If available, log them
     */
    if (isA_CFDictionary(assertionDictionary)) {
        assertionTypeStr = CFDictionaryGetValue(assertionDictionary, kIOPMAssertionTypeKey);
        levelNum = CFDictionaryGetValue(assertionDictionary, kIOPMAssertionLevelKey);
        CFNumberGetValue(levelNum, kCFNumberIntType, &assertionLevel);
    }
    if (assertionTypeStr)
    {
        if (CFStringGetCString(assertionTypeStr, 
                                assertionTypeCString, 
                                kLongStringLen, 
                                kCFStringEncodingMacRoman))
        {
            asl_set(m, kPMASLAssertionNameKey, assertionTypeCString);
        }
        
        index = indexForAssertionName(assertionTypeStr);

        level_buf[0] = 0;
        if (0 < snprintf(level_buf, kShortStringLen, "%d", assertionLevel)) {
            asl_set(m, kPMASLNewCallerValueKey, level_buf);
        }
    
        prior_system_level_buf[0] = 0;
        if (0 < snprintf(prior_system_level_buf, kShortStringLen, "%d", last_aggregate_assertions[index])) {
            asl_set(m, kPMASLPreviousSystemValueKey, prior_system_level_buf); 
        }
    
        new_system_level_buf[0] = 0;
        if (0 < snprintf(new_system_level_buf, kShortStringLen, "%d", aggregate_assertions[index])) {
            asl_set(m, kPMASLNewSystemValueKey, new_system_level_buf);
        }

    }

    if (!strncmp(assertionActionCStr, kPMASLActionTimedOut, strlen(kPMASLActionTimedOut)))
    {
        // Set a high log level for timeouts
        aslLogLevel = ASL_LEVEL_ERR;    
    }

    /* And log the message.
     * By default, INFO level messages won't get sent to the server,
     * and won't get written to the disk.
     */
    asl_log(NULL, m, aslLogLevel, "PMAssertion(%s) %s %s", 
                pid_buf, 
                assertionActionCStr, 
                assertionTypeStr ? assertionTypeCString:"");
    
    asl_free(m);
}


static int indexForAssertionName(CFStringRef assertionName) 
{
    if (CFEqual(assertionName, kIOPMAssertionTypeNeedsCPU))
       return kHighPerfIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeNoIdleSleep))
        return kPreventIdleIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeEnableIdleSleep))
        return kEnableIdleIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeDisableInflow))
        return kDisableInflowIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeInhibitCharging))
        return kInhibitChargeIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeDisableLowBatteryWarnings))
        return kDisableWarningsIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeNoDisplaySleep))
        return kPreventDisplaySleepIndex;
    else
        return 0;
}

 
static void
calculateAggregates(void)
{
    CFDictionaryRef         *process_assertions                 = NULL;
    int                     process_count                       = 0;
    int                     i, j;
    
    // Clear out the aggregate assertion values. We are about to re-calculate
    // these values in the big nasty loop below.
    bzero( aggregate_assertions, sizeof(aggregate_assertions) );
    
    // Initialize kEnableIdleIndex to idle_enable_assumed
    aggregate_assertions[kEnableIdleIndex] = idle_enable_assumed;
    
    process_count = CFDictionaryGetCount(gAssertionsDict);
    process_assertions = malloc(sizeof(CFDictionaryRef) * process_count);
    CFDictionaryGetKeysAndValues(gAssertionsDict, NULL, (const void **)process_assertions);
    for(i=0; i<process_count; i++)
    {
        CFArrayRef          asst_arr = NULL;
        int                 asst_arr_count = 0;
        if(!isA_CFDictionary(process_assertions[i])) continue;

        asst_arr = isA_CFArray(
            CFDictionaryGetValue(process_assertions[i], kIOPMTaskAssertionsKey));
        if(!asst_arr) continue;
        
        asst_arr_count = CFArrayGetCount(asst_arr);
        for(j=0; j<asst_arr_count; j++)
        {
            CFDictionaryRef     this_assertion = NULL;
            CFStringRef         asst_type = NULL;
            CFNumberRef         asst_val = NULL;
            int                 val = -1;
            
            this_assertion = CFArrayGetValueAtIndex(asst_arr, j);
            if(isA_CFDictionary(this_assertion))
            {
                asst_type = isA_CFString(
                    CFDictionaryGetValue(this_assertion, kIOPMAssertionTypeKey));
                asst_val = isA_CFNumber(
                    CFDictionaryGetValue(this_assertion, kIOPMAssertionLevelKey));
                if(asst_type && asst_val) {
                    CFNumberGetValue(asst_val, kCFNumberIntType, &val);
                    if(kIOPMAssertionLevelOn == val)
                    {
                        if (CFEqual(asst_type, kIOPMAssertionTypeNeedsCPU))
                        {
                            aggregate_assertions[kHighPerfIndex] = 1;
                        } else if (CFEqual(asst_type, kIOPMAssertionTypeNoIdleSleep))
                        {
                            aggregate_assertions[kPreventIdleIndex] = 1;
                        } else if (CFEqual(asst_type, kIOPMAssertionTypeEnableIdleSleep))
                        {
                            aggregate_assertions[kEnableIdleIndex] = 1;
                            
                            // Once idle_enable_assumed != true, the system will not idle sleep
                            // unless kIOPMAssertionTypeEnableIdleSleep is asserted.
                            idle_enable_assumed = false;

                        } else if (CFEqual(asst_type, kIOPMAssertionTypeDisableInflow))
                        {
                            aggregate_assertions[kDisableInflowIndex] = 1;
                        } else if (CFEqual(asst_type, kIOPMAssertionTypeInhibitCharging))
                        {
                            aggregate_assertions[kInhibitChargeIndex] = 1;
                        } else if (CFEqual(asst_type, kIOPMAssertionTypeDisableLowBatteryWarnings))
                        {
                            aggregate_assertions[kDisableWarningsIndex] = 1;
                        } else if (CFEqual(asst_type, kIOPMAssertionTypeNoDisplaySleep))
                        {
                            aggregate_assertions[kPreventDisplaySleepIndex] = 1;
                        }
                    }
                }
           }
        }
    }
    free(process_assertions);
    
    // At this point we have iterated through the entire nested data structure
    // and have calculaed what the total settings are for each of the profiles
    // and have stored them in the global array aggregate_assertions
}

static void 
publishAssertionStatus(void)
{
    /*
     * Publish a few tracking dictionaries for retrieval by IOKit.framework.
     * We use the SCDynamicStore to provide this data to other processes.
     */
    SCDynamicStoreRef       sharedDSRef = NULL;
    CFArrayRef              publishPIDToAssertionsArray         = NULL;
    CFDictionaryRef         publishAggregateValuesDictionary    = NULL;
    CFArrayRef              publishTimedOutArray                = NULL;
    static CFStringRef      pidToAssertionsSCKey                = NULL;
    static CFStringRef      aggregateValuesSCKey                = NULL;
    static CFStringRef      timedOutArraySCKey                  = NULL;
    static CFMutableDictionaryRef  keysToSet			= NULL;
    static CFMutableArrayRef       keysToRemove			= NULL;
    
    sharedDSRef = _getSharedPMDynamicStore();
    if (!keysToSet)
    {
	keysToSet = CFDictionaryCreateMutable(0, 0, 
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);
    }
    if (!keysToRemove)
    {
	keysToRemove = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    }

    if (!sharedDSRef || !keysToSet || !keysToRemove)
        goto exit;

    /*
     * publishPIDToAssertions dictionary is accessible via IOKit.framework API
     *   ÊIOReturn IOPMCopyAssertionsByProcess(CFDictionaryRef *AssertionsByPid);
     */
    if (!pidToAssertionsSCKey)
    {
        pidToAssertionsSCKey = IOPMAssertionCreatePIDMappingKey();        
    }
    if (pidToAssertionsSCKey)
    {

        publishPIDToAssertionsArray = copyPIDAssertionDictionaryFlattened();

        if (publishPIDToAssertionsArray) 
        {
	    CFDictionarySetValue(keysToSet, pidToAssertionsSCKey, publishPIDToAssertionsArray);
            CFRelease(publishPIDToAssertionsArray);
        } else {
	    CFArrayAppendValue(keysToRemove, pidToAssertionsSCKey);
        }
    }
    
    
    /* 
     * publishAggregateValuesDictionary is accessible via IOKit.framework API
     *      IOReturn IOPMCopyAssertionsStatus(CFDictionaryRef *AssertionsStatus);
     */
    if (!aggregateValuesSCKey)
    {
        aggregateValuesSCKey = IOPMAssertionCreateAggregateAssertionKey();
    }
    
    if (aggregateValuesSCKey)
    {
        publishAggregateValuesDictionary = copyAggregateValuesDictionary();
        
        if (publishAggregateValuesDictionary)
        {
	    CFDictionarySetValue(keysToSet, aggregateValuesSCKey, publishAggregateValuesDictionary);
            CFRelease(publishAggregateValuesDictionary);
        } else {
	    CFArrayAppendValue(keysToRemove, aggregateValuesSCKey);
        }
    }


    
    /*
     * publishTimedOutArray is accessible via private IOKit SPI
     * IOReturn IOPMCopyTimedOutAssertions(CFArrayRef *timedOutAssertions);
     */
    if (!timedOutArraySCKey)
    {
        timedOutArraySCKey = IOPMAssertionCreateTimeOutKey();
    }

    if (timedOutArraySCKey)
    {
        publishTimedOutArray = copyTimedOutAssertionsArray();

        if (publishTimedOutArray)
        {
	    CFDictionarySetValue(keysToSet, timedOutArraySCKey, publishTimedOutArray);
            CFRelease(publishTimedOutArray);            
        }else {
	    CFArrayAppendValue(keysToRemove, timedOutArraySCKey);
        }
    }

    SCDynamicStoreSetMultiple(sharedDSRef, keysToSet, keysToRemove, NULL);
    CFDictionaryRemoveAllValues(keysToSet);
    CFArrayRemoveAllValues(keysToRemove);

exit:
    return;
}

static void
updateAssertionStatusCallback(CFRunLoopTimerRef timer, void *info)
{
    publishAssertionStatus();
    
    // Tell the world that an assertion has been created, released,
    // or an interested application has died.
    // The state of active assertions may not have changed as a result
    // of this assertion action - i.e. may be no changes to user.
    uint32_t status;
    status = notify_post( kIOPMAssertionsChangedNotifyString );
    if (gNotifyTimeOuts)
    {
	gNotifyTimeOuts = false;
	// After our data is in place, notify the world
	notify_post( kIOPMAssertionTimedOutNotifyString );
    }
}

static void
evaluateAssertions(void)
{
    calculateAggregates();

    // Override PM settings
    overrideSetting( kPMForceHighSpeed, 
                    aggregate_assertions[kHighPerfIndex]);
    overrideSetting( kPMPreventDisplaySleep, 
                    aggregate_assertions[kPreventDisplaySleepIndex]);
    if(  aggregate_assertions[kPreventIdleIndex]
     || !aggregate_assertions[kEnableIdleIndex]) {
        overrideSetting(kPMPreventIdleSleep, 1);
    } else {
        overrideSetting(kPMPreventIdleSleep, 0);
    }
    
    // Perform kDisableInflowIndex
    if( aggregate_assertions[kDisableInflowIndex] 
        != last_aggregate_assertions[kDisableInflowIndex]) 
    {
        sendSmartBatteryCommand( kSBUCInflowDisable,
                        aggregate_assertions[kDisableInflowIndex]);
    }
    
    // Perform kInhibitChargeIndex
    if( aggregate_assertions[kInhibitChargeIndex] 
        != last_aggregate_assertions[kInhibitChargeIndex]) 
    {
        sendSmartBatteryCommand( kSBUCChargeInhibit, 
                        aggregate_assertions[kInhibitChargeIndex]);
    }
    
    // Perform low battery warning
    if( aggregate_assertions[kDisableWarningsIndex] 
        != last_aggregate_assertions[kDisableWarningsIndex]) 
    {
        _setRootDomainProperty( CFSTR("BatteryWarningsDisabled"),
                                (aggregate_assertions[kDisableWarningsIndex]
                                    ? kCFBooleanTrue : kCFBooleanFalse));
    }
    
    bcopy(aggregate_assertions, last_aggregate_assertions, 
                            sizeof(aggregate_assertions));
    
    activateSettingOverrides();

    CFAbsoluteTime fireTime = (CFAbsoluteTimeGetCurrent() + 5.0);
    if (gUpdateAssertionStatusTimer)
    {
	CFRunLoopTimerSetNextFireDate(gUpdateAssertionStatusTimer, fireTime);
    }
    else
    {
	/* timer will repeat roughly once every 100 years, i.e. never. We need
	 a repeating timer so that it doesn't get removed from the runloop and
	 and we can set the next fire date to our next wakeup time. */
	static const CFAbsoluteTime intervalNever = 100. * 365. * 24. * 60. * 60.;
	gUpdateAssertionStatusTimer = CFRunLoopTimerCreate(0, fireTime, intervalNever, 0,
                                             0, updateAssertionStatusCallback, 0);
	if (gUpdateAssertionStatusTimer)
	{
	    CFRunLoopAddTimer(CFRunLoopGetCurrent(), gUpdateAssertionStatusTimer, kCFRunLoopDefaultMode);
	}
    }
}

#if HAVE_SMART_BATTERY
static void
sendSmartBatteryCommand(uint32_t which, uint32_t level)
{
    io_service_t    sbmanager = MACH_PORT_NULL;
    io_connect_t    sbconnection = MACH_PORT_NULL;
    kern_return_t   kret;
    uint32_t        output_count = 1;
    uint64_t        uc_return = kIOReturnError;
    uint64_t        level_64 = level;

    // Find SmartBattery manager
    sbmanager = IOServiceGetMatchingService(MACH_PORT_NULL,
                    IOServiceMatching("AppleSmartBatteryManager"));
                    
    if(MACH_PORT_NULL == sbmanager) {
        goto bail;
    }

    kret = IOServiceOpen( sbmanager, mach_task_self(), 0, &sbconnection);
    if(kIOReturnSuccess != kret) {
        goto bail;
    }

    kret = IOConnectCallMethod(
                    sbconnection, // connection
                    which,      // selector
                    &level_64,  // uint64_t *input
                    1,          // input Count
                    NULL,       // input struct count
                    0,          // input struct count
                    &uc_return, // output
                    &output_count,  // output count
                    NULL,       // output struct
                    0);         // output struct count

bail:

    if (MACH_PORT_NULL != sbconnection) {
        IOServiceClose(sbconnection);
    }

    if (MACH_PORT_NULL != sbmanager) {
        IOObjectRelease(sbmanager);
    }

    return;
}
#else /* HAVE_SMART_BATTERY */

static void
sendSmartBatteryCommand(uint32_t which, uint32_t level)
{
    kern_return_t       kr;
    io_iterator_t       iter;
    io_registry_entry_t next;

    do
    {
    kr = IOServiceGetMatchingServices(kIOMasterPortDefault, 
        IOServiceMatching("IOPMPowerSource"), &iter);
    if (kIOReturnSuccess != kr)
        break;
    if (MACH_PORT_NULL == iter)
        break;
    while ((next = IOIteratorNext(iter)))
    {
        kr = IORegistryEntrySetCFProperty(next, 
                          (which == kSBUCChargeInhibit) ? CFSTR(kIOPMPSIsChargingKey) : CFSTR(kIOPMPSExternalConnectedKey), 
                          level ? kCFBooleanFalse : kCFBooleanTrue);
        IOObjectRelease(next);
    }
    IOObjectRelease(iter);
    }
    while (false);
    return;
}

#endif /* HAVE_SMART_BATTERY */


__private_extern__ IOReturn  
_IOPMSetActivePowerProfilesRequiresRoot
(
    CFDictionaryRef which_profile, 
    int uid, 
    int gid
)
{
    IOReturn                    ret = kIOReturnError;
    SCPreferencesRef            energyPrefs = NULL;
    
    /* Private call for Power Management use only.
       PM's configd plugin (our daemon-lite) is the only intended caller
       of _IOPMSetActivePowerProfilesRequiresRoot. configd will call this
       only when a running process (like BatteryMonitor) calls 
       IOPMSetActivePowerProfiles() as adimn, root, or console user.
       configd does the security check there, and then calls _RequiresRoot
       under configd's own root privileges. Writing out the preferences
       file using SCPreferences requires root privileges.       
      */

    if( (!callerIsRoot(uid, gid) &&
        !callerIsAdmin(uid, gid) &&
        !callerIsConsole(uid, gid)) || 
        ( (-1 == uid) || (-1 == gid) ))
    {
        ret = kIOReturnNotPrivileged;
        goto exit;    
    }

    if(!which_profile) {
        // We leave most of the input argument vetting for IOPMSetActivePowerProfiles,
        // which checks the contents of the dictionary. At this point we assume it
        // is well formed (and that it exists).
        ret = kIOReturnBadArgument;
        goto exit;
    }
    
    energyPrefs = SCPreferencesCreate( kCFAllocatorDefault, CFSTR(kIOPMAppName), CFSTR(kIOPMPrefsPath) );
    if(!energyPrefs) {
        goto exit;
    }

    if(!SCPreferencesLock(energyPrefs, true))
    {  
        ret = kIOReturnInternalError;
        goto exit;
    }
    
    if(!SCPreferencesSetValue(energyPrefs, CFSTR("ActivePowerProfiles"), which_profile)) {
        goto exit;
    }

    if(!SCPreferencesCommitChanges(energyPrefs))
    {
        // handle error
        if(kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
    if(!SCPreferencesApplyChanges(energyPrefs))
    {
        // handle error
        if(kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;        
        goto exit;
    }

    ret = kIOReturnSuccess;
exit:
    if(energyPrefs) {
        SCPreferencesUnlock(energyPrefs);
        CFRelease(energyPrefs);
    }
    return ret;
}

 
/***********************************
 * Dynamic Assertions
 ***********************************/
#define ID_FROM_INDEX(idx)  (idx + 300)
#define INDEX_FROM_ID(id)   (id - 300)



__private_extern__ void
PMAssertions_prime(void)
{
	
    assertion_types_arr[kHighPerfIndex]             = kIOPMAssertionTypeNeedsCPU; 
    assertion_types_arr[kPreventIdleIndex]          = kIOPMAssertionTypeNoIdleSleep;
    assertion_types_arr[kDisableInflowIndex]        = kIOPMAssertionTypeDisableInflow; 
    assertion_types_arr[kInhibitChargeIndex]        = kIOPMAssertionTypeInhibitCharging;
    assertion_types_arr[kDisableWarningsIndex]      = kIOPMAssertionTypeDisableLowBatteryWarnings;
    assertion_types_arr[kPreventDisplaySleepIndex]  = kIOPMAssertionTypeNoDisplaySleep;
    assertion_types_arr[kEnableIdleIndex]           = kIOPMAssertionTypeEnableIdleSleep;

    publishAssertionStatus();

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */



IOReturn _IOPMAssertionCreateRequiresRoot
(
    mach_port_t         task_port,
    char                *nameCStr,
    char                *assertionCStr,
    int                 level,
    int                 *assertion_id
)
{
    IOReturn                result = kIOReturnInternalError;
    CFMutableDictionaryRef  this_task = NULL;
    CFMutableArrayRef       assertions = NULL;
    CFStringRef             assertionString = NULL;
    CFStringRef             nameString = NULL;
    int                     task_pid_int = -1;
    CFNumberRef             task_pid_num = NULL;
    mach_port_t             oldNotify = MACH_PORT_NULL;
    kern_return_t           err = KERN_SUCCESS;
    
    __MACH_PORT_DEBUG(true, "assertion_create: expect send right", task_port);
    // Expect to have 1 send right on task_port if this is the first new assertion for this process
    // Expect to have 2 send rights on task_port if this isn't the first assertion

    // assertion_id will be set to kIOPMNullAssertionID on failure.
    *assertion_id = kIOPMNullAssertionID;

    if (KERN_SUCCESS == pid_for_task(task_port, &task_pid_int)) {
        task_pid_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &task_pid_int);
    }
    
    /* Get dictionary describing this tasks's open assertions */
    if (gAssertionsDict) {
       this_task = (CFMutableDictionaryRef)CFDictionaryGetValue(gAssertionsDict, MY_CAST_INT_POINTER(task_port));
    }
    if (this_task)
    {
        // task_port rights invariant: since we are tracking task_port in gAssertionsDict, we already
        // held a send right on task_port.
        // We just received a second send right in mig call io_pm_assertion_create;
        // so let's clear that send right.
        
        __MACH_PORT_DEBUG(true, "assertion_create: deallocate extra send right", task_port);
        mach_port_deallocate(mach_task_self(), task_port);        
    } else {



        __MACH_PORT_DEBUG(true, "assertion_create: did set invalid callback", task_port);
    
        // This is the first assertion for this process, so we'll create a new
        // assertion datastructure.
        this_task = CFDictionaryCreateMutable(0, 0, 
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
        if (!this_task){
            result = kIOReturnNoMemory;
            goto exit;
        }

        if (task_pid_num) {
            CFDictionarySetValue(this_task, kIOPMTaskPIDKey, task_pid_num);
        }

        // Register for a dead name notification on this task_t
        err = mach_port_request_notification(
                    mach_task_self(),       // task
                    task_port,                   // port that will die
                    MACH_NOTIFY_DEAD_NAME,   // msgid
                    1,                      // make-send count
                    CFMachPortGetPort(pmServerMachPort),       // notify port
                    MACH_MSG_TYPE_MAKE_SEND_ONCE, // notifyPoly
                    &oldNotify);            // previous

        if (KERN_SUCCESS != err)
        {
            syslog(LOG_ERR, "PM assertion mach port request notification error %s(0x%08x)\n",
                mach_error_string(err), err);
            mach_port_deallocate(mach_task_self(), task_port);
            result = err;
            goto exit;
        }
                    
        if (oldNotify != MACH_PORT_NULL) {
            mach_port_deallocate(mach_task_self(), oldNotify);
        }

        // gAssertionsDict is the global dictionary that maps a processes to their assertions.
        if (!gAssertionsDict) {
            gAssertionsDict = CFDictionaryCreateMutable(0, 0, NULL, &kCFTypeDictionaryValueCallBacks);
        }
        
        CFDictionarySetValue(gAssertionsDict, MY_CAST_INT_POINTER(task_port), this_task);
        CFRelease(this_task);
    }

    /* Get Array Listing all this task's open assertions */
    assertions = (CFMutableArrayRef)CFDictionaryGetValue(this_task, kIOPMTaskAssertionsKey);
    if(!assertions) 
    {
        assertions = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
        CFDictionarySetValue(this_task, kIOPMTaskAssertionsKey, assertions);
        CFRelease(assertions);
    }

    // find empty slot
    CFIndex     arrayIndex;
    int         asst_count;
    asst_count = CFArrayGetCount(assertions);
    for (arrayIndex=0; arrayIndex<asst_count; arrayIndex++) 
    {
        // find the first empty element in the array
        // empty elements are denoted by the value kCFBooleanFalse
        if (kCFBooleanFalse == CFArrayGetValueAtIndex(assertions, arrayIndex)) break;
    }
    if (arrayIndex >= kMaxTaskAssertions) 
    {
        // ERROR! out of space in array!
        result = kIOReturnNoMemory;
        goto exit;
    }

    /* 
     * Populate our new assertion dictionary 
     */
    CFMutableDictionaryRef          new_assertion_dict = NULL;
    CFNumberRef                     cf_assertion_val = NULL;
    CFDateRef                       start_date = NULL;

    *assertion_id = ID_FROM_INDEX(arrayIndex);
    new_assertion_dict = CFDictionaryCreateMutable(0, 0,
                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    /* Type */
    assertionString = CFStringCreateWithCString(0, assertionCStr, kCFStringEncodingMacRoman);
    if (assertionString) {
        CFDictionarySetValue(new_assertion_dict, kIOPMAssertionTypeKey, assertionString);
        CFRelease(assertionString);
    }
    /* Level */
    cf_assertion_val = CFNumberCreate(0, kCFNumberIntType, &level);
    if (cf_assertion_val) {
        CFDictionarySetValue(new_assertion_dict, kIOPMAssertionLevelKey, cf_assertion_val);
        CFRelease(cf_assertion_val);
    }
    /* Name */
    nameString = CFStringCreateWithCString(0, nameCStr, kCFStringEncodingMacRoman);
    if (nameString) {
        CFDictionarySetValue(new_assertion_dict, kIOPMAssertionNameKey, nameString);
        CFRelease(nameString);
    }
    /* Create Time */
    start_date = CFDateCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent());
    if (start_date) {
        CFDictionarySetValue(new_assertion_dict, kIOPMAssertionCreateDateKey, start_date);
        CFRelease(start_date);
    }
    /* Owner's PID */
    if (task_pid_num) {
        CFDictionarySetValue(new_assertion_dict, kIOPMAssertionPIDKey, task_pid_num);
        CFRelease(task_pid_num);
    }

    CFArraySetValueAtIndex(assertions, arrayIndex, new_assertion_dict);
    evaluateAssertions();

    // ASL Log assertion
    logAssertionEvent(kPMASLActionAssert, this_task, new_assertion_dict);

    CFRelease(new_assertion_dict);

    result = kIOReturnSuccess;
exit:

    return result;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Minimum timeout: 1 seconds */
static const int kIOMinimumTimeoutInterval = 1; 

kern_return_t _io_pm_assertion_settimeout
(
    mach_port_t server,
    mach_port_t task,
    int         assertion_id,
    int         interval,
    int         *return_code
)
{
    CFMutableDictionaryRef  timeoutAssertion = NULL;
    CFNumberRef             intervalNum = NULL;
    CFRunLoopTimerRef       timeOutTimer = NULL;

    if (interval < kIOMinimumTimeoutInterval) {
        return kIOReturnBadArgument;
    }

    __MACH_PORT_DEBUG(true, "set_timeout: expect send right", task);
    // Expect to have 2 send right on tasks if this assertion exists.
    // If this was called (invalidly) by a process without any assertions, 
    // we expect 1 send right on task.

    *return_code = copyAssertionForID(task, 
                                assertion_id,
                                NULL, /* task's dictionary */
                                NULL, /* task's array */
                                &timeoutAssertion);

    if (kIOReturnSuccess != *return_code) {
        goto exit;    
    }
    
    if (!timeoutAssertion) {
        *return_code = kIOReturnNotFound;
        goto exit;
    }
    
    // Set time interval property kIOPMAssertionTimeOutIntervalKey
    intervalNum = CFNumberCreate(0, kCFNumberIntType, &interval);
    if (intervalNum) {
        CFDictionarySetValue(timeoutAssertion, kIOPMAssertionTimeOutIntervalKey, intervalNum);
        CFRelease(intervalNum);
    }
    
    // And create CFRunLoopTimerRef property
    CFDateRef   assertionCreatedDate = CFDictionaryGetValue(timeoutAssertion, kIOPMAssertionCreateDateKey);
    if (assertionCreatedDate) 
    {
        CFRunLoopTimerContext  timerContext = 
                { 0, (void *)timeoutAssertion, NULL, NULL, NULL };
        CFAbsoluteTime fireDate = CFDateGetAbsoluteTime(assertionCreatedDate);
        fireDate += (CFTimeInterval)interval;
        timeOutTimer = CFRunLoopTimerCreate(0, fireDate, 0.0, 
                                            0, 0, 
                                            timeoutExpirationCallBack, &timerContext);
        if (timeOutTimer) 
        {
            CFRunLoopAddTimer(CFRunLoopGetCurrent(), timeOutTimer, kCFRunLoopDefaultMode);
            CFDictionarySetValue(timeoutAssertion, kIOPMAssertionTimerRefKey, timeOutTimer);
            CFRelease(timeOutTimer);
        }
    }

    // Do not call evaluateAssertions() - nothing to evaluate here.
    
    // If our timer instantiated correctly, we are now waiting for a timer to fire

    *return_code = kIOReturnSuccess;

exit:

    // Remove the send right on task that we received with this invocation.
    __MACH_PORT_DEBUG(true, "set_timeout: deallocate extra send right", task);
    mach_port_deallocate(mach_task_self(), task);

    if (timeoutAssertion) {
        CFRelease(timeoutAssertion);
    }

    return KERN_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

kern_return_t _io_pm_assertion_release
(
    mach_port_t server,
    mach_port_t task,
    int assertion_id,
    int *return_code
)
{
    CFDictionaryRef         callerTask          = NULL;
    CFMutableArrayRef       taskAssertions      = NULL;              
    CFMutableDictionaryRef  releaseAssertion    = NULL;
    CFRunLoopTimerRef       timeOutTimer        = NULL;
    int                     i;
    int                     n;
    bool                    releaseTask;

    // Expect to have 2 send right on task if this assertion exists.
    // If this was called (invalidly) by a process without any assertions, 
    // we expect 1 send right on task.
    __MACH_PORT_DEBUG(true, "assertion_release", task);

    *return_code = copyAssertionForID(task, 
                                assertion_id,
                                &callerTask,
                                &taskAssertions, 
                                &releaseAssertion);

    if (kIOReturnSuccess != *return_code) {
        goto exit;
    }
    
    // Cancel timeout at kIOPMAssertionTimerRefKey
    timeOutTimer = (CFRunLoopTimerRef)CFDictionaryGetValue(
                                        releaseAssertion, 
                                        kIOPMAssertionTimerRefKey);
    if (timeOutTimer)
    {
        CFRunLoopTimerInvalidate(timeOutTimer);
    }
    
    // Add a "Released" timestamp if this assertion has already timed out
    // If this assertion has timed-out, debugging humans will want to know if 
    // and when it was released.
    if (CFDictionaryGetValue(releaseAssertion, kIOPMAssertionTimedOutDateKey))
    {
        CFDateRef   dateNow = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
        CFDictionarySetValue(releaseAssertion, kIOPMAssertionReleaseDateKey, dateNow);
        CFRelease(dateNow);
    }

    // Figure out the index into the returned taskAssertions array
    CFIndex arrayIndex = INDEX_FROM_ID(assertion_id);

    // Release it from its task array
    // * Note that if this assertion has timed-out, it will continue to exist for
    // record-keeping purposes on the gTimedOutAssertions arrray.
    CFArraySetValueAtIndex(taskAssertions, arrayIndex, kCFBooleanFalse);
    
    // Check whether this is the last assertion in the task's array.
    // If no more assertions tied to the task, clean up the task's bookkeeping.
    releaseTask = TRUE;
    n = CFArrayGetCount(taskAssertions);
    for (i =0; i < n; i++) {
        CFTypeRef    assertion;
        assertion = CFArrayGetValueAtIndex(taskAssertions, i);
        if (!CFEqual(assertion, kCFBooleanFalse)) {
            releaseTask = FALSE;
            break;
        }
    }
    if (releaseTask) {
        CFDictionaryRemoveValue(gAssertionsDict, MY_CAST_INT_POINTER(task));

        // If we're dropping the task, then we need to drop our send right
        // that we received in _IOPMAssertionCreateRequiresRoot
        __MACH_PORT_DEBUG(true, "assertion_release: deallocating task's #1 send right", task);
        mach_port_deallocate(mach_task_self(), task);
    }
    
    // Re-evaluate
    evaluateAssertions();
    
    // ASL Log assertion release
    logAssertionEvent(kPMASLActionRelease, 
                    callerTask,
                    releaseAssertion);

    if (callerTask) {
        CFRelease(callerTask);
    }
    if (taskAssertions) {
        CFRelease(taskAssertions);
    }
    if (releaseAssertion) {
        CFRelease(releaseAssertion);
    }

    *return_code = kIOReturnSuccess;    


exit:
    __MACH_PORT_DEBUG(true, "assertion_release: EXIT deallocating send right", task);
    mach_port_deallocate(mach_task_self(), task);
    
    return KERN_SUCCESS;   
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Creates a dictionary mapping process id's to the assertions that they own.
 *
 * The corresponding IOKit API IOPMCopyAssertionsByProcess returns a dictionary 
 * whose keys are process ID's.
 * This is perfectly acceptable in CoreFoundation, EXCEPT that you cannot
 * serialize a dictionary with CFNumbers for keys using CF or IOKit
 * serialization.
 *
 * To serialize this dictionary and pass it from configd to the caller's process,
 * we re-formatted it as a "flattened" array of dictionaries in configd, 
 * and we will re-constitute with pid's for keys here.
 *
 * Next time around, I will simply not use CFNumberRefs for keys in API.
 */
static CFArrayRef copyPIDAssertionDictionaryFlattened(void)
{
    CFDictionaryRef         *taskDataArray = NULL;
    CFMutableArrayRef       returnArray = 0;
    int                     dict_count;
    int                     task_index;
    
    if(!gAssertionsDict) goto exit;

    dict_count = CFDictionaryGetCount(gAssertionsDict);
    if(0 == dict_count) goto exit;

    returnArray = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    
    taskDataArray = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef)*dict_count);

    if (!taskDataArray) 
        goto exit;

    CFDictionaryGetKeysAndValues(gAssertionsDict, 
                                (const void **)NULL,   
                                (const void **)taskDataArray);

    // Iterate our master list of processes and map them to pids & their assertions
    for (task_index=0; task_index<dict_count; task_index++)
    {
        CFNumberRef             processID = NULL;
        CFArrayRef              processAssertions = NULL;
        CFMutableDictionaryRef  perProcessDictionary = NULL;
        
        if (!taskDataArray[task_index]) 
            continue;
        
        processID = CFDictionaryGetValue(
                                taskDataArray[task_index], 
                                kIOPMTaskPIDKey);

        processAssertions = CFDictionaryGetValue(
                                taskDataArray[task_index], 
                                kIOPMTaskAssertionsKey);

        if (!processID || !processAssertions)
            continue;

        /* Create an array copy of the process's array of tracking assertions.
         * Remove empty entries & remove internal data from each assertion,
         * before passing it out to user space.
         */

        CFMutableArrayRef       newProcessAssertionsArray = CFArrayCreateMutable(
                                            kCFAllocatorDefault, 
                                            CFArrayGetCount(processAssertions), 
                                            &kCFTypeArrayCallBacks);

        int                     arrayCount = CFArrayGetCount(processAssertions);

        int assertion_index;
        for (assertion_index = 0; assertion_index<arrayCount; assertion_index++)
        {
            CFMutableDictionaryRef assertionAtIndex = 
                        (CFMutableDictionaryRef)CFArrayGetValueAtIndex(processAssertions, assertion_index);
            CFMutableDictionaryRef assertionCopy = NULL;
            
            if (isA_CFDictionary(assertionAtIndex))
            {
                assertionCopy = CFDictionaryCreateMutableCopy(
                                            kCFAllocatorDefault,
                                            CFDictionaryGetCount(assertionAtIndex),
                                            assertionAtIndex);

            /* Strip out CFTimerRef from dictionaries. CFTimerRef types are not serializable
             * and can't be stored in the SCDynamicStore; and API clients don't need to see them.
             */

                if (assertionCopy)
                {
                    CFDictionaryRemoveValue(assertionCopy, kIOPMAssertionTimerRefKey);
                
                    CFArrayAppendValue(newProcessAssertionsArray, assertionCopy);

                    CFRelease(assertionCopy);
                }   
            }
        }


        /* Flattening begins */            
        perProcessDictionary = CFDictionaryCreateMutable(
                                kCFAllocatorDefault,
                                2,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);

        CFDictionarySetValue(perProcessDictionary,
                                kIOPMAssertionPIDKey,
                                processID);

        CFDictionarySetValue(perProcessDictionary,
                                CFSTR("PerTaskAssertions"),
                                newProcessAssertionsArray);
                                
        CFRelease(newProcessAssertionsArray);

        /* Flattening complete - pid & assertions are encapsulated within a dictionary now. 
         * IOKitUser framework will re-build the PID (key) to assertions array (value) dictionary
         * mapping. 
         */
        CFArrayAppendValue(returnArray, perProcessDictionary);

        CFRelease(perProcessDictionary);
    }
    
    free(taskDataArray);

exit:
    return returnArray;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static CFDictionaryRef copyAggregateValuesDictionary(void)
{
    CFDictionaryRef                 assertions_info = NULL;
    CFNumberRef                     cf_agg_vals[kIOPMNumAssertionTypes];
    int                             i;
    
    // Massage int values into CFNumbers for CFDictionaryCreate
    for(i=0; i<kIOPMNumAssertionTypes; i++)
    {
        cf_agg_vals[i] = CFNumberCreate(
            0, 
            kCFNumberIntType,
            &aggregate_assertions[i]);
    }

    // We return the contents of aggregate_assertions packed into a CFDictionary.
    assertions_info = CFDictionaryCreate(
        0,
        (const void **)assertion_types_arr,     // type: CFStringRef
        (const void **)cf_agg_vals,   // value: CFNumberRef
        kIOPMNumAssertionTypes,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
        
    // Release CFNumbers
    for(i=0; i<kIOPMNumAssertionTypes; i++)
    {
        CFRelease(cf_agg_vals[i]);
    }

    // TODO: strip unsupported assertions?

    return assertions_info;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static CFArrayRef copyTimedOutAssertionsArray(void)
{
    /* Strip out non-serializable private portions of the timed-out assertions */

    if (gTimedOutArray)
    {    
        int                     arrayCount;
        int                     i;
        CFMutableArrayRef       newTimeOutArray;
        newTimeOutArray = CFArrayCreateMutableCopy(0, 0, gTimedOutArray);
        
        arrayCount = CFArrayGetCount(newTimeOutArray);
        for (i = 0; i<arrayCount; i++)
        {
            CFMutableDictionaryRef  assertionCopy = 
                    CFDictionaryCreateMutableCopy(0, 0, CFArrayGetValueAtIndex(newTimeOutArray, i));
    
            CFDictionaryRemoveValue(assertionCopy, kIOPMAssertionTimerRefKey);
    
            CFArraySetValueAtIndex(newTimeOutArray, i, assertionCopy);
            
            CFRelease(assertionCopy);
        }

        return newTimeOutArray;
    }
    return NULL;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

__private_extern__ bool
PMAssertionsHandleDeadName(
    mach_port_t dead_port
)
{
    CFDictionaryRef             deadTrackerDict = NULL;
    CFRunLoopTimerRef		timeOutTimer = NULL;

    if (!gAssertionsDict) {
        // We're not tracking any assertions, return false because we didn't act on this.
        return false;
    }

    deadTrackerDict = CFDictionaryGetValue(gAssertionsDict, MY_CAST_INT_POINTER(dead_port));
    if (!deadTrackerDict) {
        // We're not tracking any assertions for this process. Return false.
        return false;
    }
    
    __MACH_PORT_DEBUG(true, "cleanupAssertions: cleaning up", dead_port);
    
    // Clean up after this dead process
    CFRetain(deadTrackerDict);

    // Mark each assertion in the process as released
    // (useful debugging data for tracking down timed-out assertions)
    CFArrayRef  dead_task_assertions 
                    = CFDictionaryGetValue(deadTrackerDict, kIOPMTaskAssertionsKey);
    if (dead_task_assertions)
    {
        int assertions_count = CFArrayGetCount(dead_task_assertions);
        int i;
        
        for (i=0; i<assertions_count; i++)
        {
            CFMutableDictionaryRef  releaseAssertion = (CFMutableDictionaryRef) \
                                CFArrayGetValueAtIndex(dead_task_assertions, i);

            if (!releaseAssertion || !isA_CFDictionary(releaseAssertion)) {
                continue;
            }

	    // Cancel timeout at kIOPMAssertionTimerRefKey
	    timeOutTimer = (CFRunLoopTimerRef)CFDictionaryGetValue(releaseAssertion, 
								   kIOPMAssertionTimerRefKey);
	    if (timeOutTimer)
	    {
		CFRunLoopTimerInvalidate(timeOutTimer);
	    }
            
            // Add a "Released" timestamp if this assertion has already timed out
            // If this assertion has timed-out, debugging humans will want to know if 
            // and when it was released.

            if (CFDictionaryGetValue(releaseAssertion, kIOPMAssertionTimedOutDateKey))
            {
                CFDateRef   dateNow = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
                CFDictionarySetValue(releaseAssertion, kIOPMAssertionReleaseDateKey, dateNow);
                CFRelease(dateNow);
            }
        }
    }

    // Remove the process's tracking data
    CFDictionaryRemoveValue(gAssertionsDict, MY_CAST_INT_POINTER(dead_port));

    __MACH_PORT_DEBUG(true, "cleanupAssertions: deallocating", dead_port);
    mach_port_deallocate(mach_task_self(), dead_port);

    evaluateAssertions();

    // ASL Log client death
    logAssertionEvent(kPMASLActionClientDied,    
                        deadTrackerDict, /* task */
                        NULL /* assertion */);

    CFRelease(deadTrackerDict);
    __MACH_PORT_DEBUG(true, "cleanupAssertions: exiting", dead_port);

    // Return true if we destroyed the port.
    return true;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void appendTimedOutAssertion(CFDictionaryRef timedout)
{
    int timedoutcount = 0;

    if (!gTimedOutArray) {
        gTimedOutArray = CFArrayCreateMutable(0, kMaxCountTimedOut, &kCFTypeArrayCallBacks);    
    }

    if (!gTimedOutArray) 
        return;

    timedoutcount = CFArrayGetCount(gTimedOutArray);
    if (kMaxCountTimedOut == timedoutcount) {
        // Ensure that we store no more than the kMaxCountTimedOut latest timeouts
        // If we're over our tracking quota, pull the last entry off the end.
        CFArrayRemoveValueAtIndex(gTimedOutArray, kMaxCountTimedOut - 1);
    }

    // Always add new entries onto the front.
    // The array starts with newest assertions at index 0.
    CFArrayInsertValueAtIndex(gTimedOutArray, 0, timedout);
    
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* timeoutExpirationCallBack fires when an assertion specifies a timeout
 * and that timeout expires.
 */
static void timeoutExpirationCallBack(CFRunLoopTimerRef timer, void *info)
{
    CFMutableDictionaryRef assertion = (CFMutableDictionaryRef)isA_CFDictionary(info);

    if (!assertion) 
        return;

    /* We claim that the assertion in question has _not_ been released
     * by its caller and is still considered valid at the time of this firing.
     */

    // Let's remove the property tracking its now-invalid timer ref.
    CFDictionaryRemoveValue(assertion, kIOPMAssertionTimerRefKey);
    
    // This is a timed out assertion; mark it invalid with the timestamp.
    // Although marked invalid, we leave this assertion on the owning pid's assertion list
    // until it's actually released (if ever).

    CFDateRef   dateNow = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
    CFDictionarySetValue(assertion, kIOPMAssertionTimedOutDateKey, dateNow);
    CFRelease(dateNow);

    // Put a copy of this assertion into our "timeouts" array
    appendTimedOutAssertion(assertion);
    
    // Sync up changes to assertion with SCDynamicStore and user clients
    evaluateAssertions();
    
    gNotifyTimeOuts = true;
}





/* copyAssertionForID()
 * Identifies the assertion "inID" for task "inPort".
 * Returns copies of the task's dictionary (immutable),
 * the array of assertions associated with the task (mutable),
 * and the assertion identified by ID (mutable).
 *
 * Returns IOReturn code.
 */
static IOReturn copyAssertionForID(
    mach_port_t             inPort,
    int                     inID,
    CFDictionaryRef         *outTask,
    CFMutableArrayRef       *outTaskAssertions,
    CFMutableDictionaryRef  *outAssertion)
{
    CFDictionaryRef         localTask = NULL;
    CFMutableArrayRef       localTaskAssertions = NULL;              
    CFMutableDictionaryRef  localAssertion = NULL;
    IOReturn                ret;

    if (outTask) {
        *outTask = NULL;
    }
    if (outTaskAssertions) {
        *outTaskAssertions = NULL;
    }
    if (outAssertion) {
        *outAssertion = NULL;
    }
    if (gAssertionsDict) {
        localTask = CFDictionaryGetValue(gAssertionsDict, MY_CAST_INT_POINTER(inPort));
    }
    if (!localTask) {
        ret = kIOReturnNotFound;
        goto exit;
    }

    localTaskAssertions = (CFMutableArrayRef)CFDictionaryGetValue(localTask, kIOPMTaskAssertionsKey);
    if (!localTaskAssertions) {
        ret = kIOReturnInternalError;
        goto exit;
    }

    CFIndex arrayIndex = INDEX_FROM_ID(inID);
    if ((arrayIndex < 0) || (arrayIndex >= CFArrayGetCount(localTaskAssertions))) {
        ret = kIOReturnNotFound;
        goto exit;
    }

    localAssertion = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(localTaskAssertions, arrayIndex);
    if (!localAssertion 
     || !isA_CFDictionary(localAssertion)) {
        ret = kIOReturnNotFound;
        goto exit;
    }
    
    if (outTask) {
        CFRetain(localTask);
        *outTask = localTask;
    }
    if (outTaskAssertions) {
        CFRetain(localTaskAssertions);
        *outTaskAssertions = localTaskAssertions;
    }
    if (outAssertion) {
        CFRetain(localAssertion);
        *outAssertion = localAssertion;
    }

    ret = kIOReturnSuccess;

exit:
    return ret;
}


/*
static void debugLogAssertion(CFDictionaryRef log_me)
{
//#if 1
    CFStringRef     *keys;
    CFTypeRef       *values;
    int num_properties = 0;
    int i;

    if (!log_me)
    {
        asl_log(NULL,NULL,ASL_LEVEL_ERR, "No assertion to log!\n");    
    }
    
    num_properties = CFDictionaryGetCount(log_me);
    keys = (CFStringRef *)malloc(num_properties * sizeof(void *));
    values = (CFTypeRef *)malloc(num_properties * sizeof(void *));
    if(!keys || !values)
        return;

    CFDictionaryGetKeysAndValues(log_me, (const void **)keys, (const void **)values);

    asl_log(NULL, NULL, ASL_LEVEL_ERR, " * Logging assertion %p\n", (void *)log_me);

    for (i=0; i<num_properties; i++)
    {
        CFStringRef     theKey = NULL;
        CFStringRef     valString = NULL;
        CFNumberRef     valNum = NULL;
        CFBooleanRef    valBool;
    
        theKey = isA_CFString(keys[i]);
        valString = isA_CFString(values[i]);
        valNum = isA_CFNumber(values[i]);
        valBool = isA_CFBoolean(values[i]);

        if (theKey)
        {
            char    bufkey[100];
            CFStringGetCString(theKey, bufkey, 100, kCFStringEncodingMacRoman);

            if (valString)
            {
                char    buf[100];
                CFStringGetCString(valString, buf, 100, kCFStringEncodingMacRoman);
                asl_log(NULL, NULL, ASL_LEVEL_ERR, "\tkey = %s   string = %s\n", bufkey, buf);
            } else if (valNum)
            {
                int    num;
                CFNumberGetValue(valNum, kCFNumberIntType, &num);
                asl_log(NULL, NULL, ASL_LEVEL_ERR, "\tkey = %s   number = %d\n", bufkey, num);
            } else if (valBool)
            {
                asl_log(NULL, NULL, ASL_LEVEL_ERR, "\tkey = %s   bool = %s\n", bufkey,
                        valBool == kCFBooleanTrue? "true" : "false");                
            } else {
                asl_log(NULL, NULL, ASL_LEVEL_ERR, "\tkey = %s   unknown type!\n", bufkey);
            }
        }
    }
    asl_log(NULL, NULL, ASL_LEVEL_ERR, "\n");
    
    free(keys);
    free(values);
    
//#endif

return;    
}
*/

