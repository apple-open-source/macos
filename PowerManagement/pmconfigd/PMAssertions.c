/*
 * Copyright (c) 2005-2013 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#include <unistd.h>
#include <stdlib.h>
#include <notify.h>
#include <asl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <dispatch/dispatch.h>
#include <bsm/libbsm.h>
#include <libproc.h>



#include "PMConnection.h"
#include "PrivateLib.h"
#include "PMSettings.h"
#include "PMAssertions.h"
#include "BatteryTimeRemaining.h"
#include "PMStore.h"
#include "powermanagementServer.h"
#include "SystemLoad.h"
#include "Platform.h"

//#include <IOKit/IOReportMacros.h>

#define kIOPMAppName                "Power Management configd plugin"
#define kIOPMPrefsPath              "com.apple.PowerManagement.xml"
#define FMMD_WIPE_BOOT_ARG          "fmm-wipe-system-status"


#define kMaxAssertions              10240

// CAST_PID_TO_KEY casts a mach_port_t into a void * for CF containers
#define CAST_PID_TO_KEY(x)          ((void *)(uintptr_t)(x))

#define LEVEL_FOR_BIT(idx)          (getAssertionLevel(idx))



// Selectors for AppleSmartBatteryManagerUserClient
enum {
    kSBUCInflowDisable              = 0,
    kSBUCChargeInhibit              = 1
};

typedef enum {
    kTimerTypeTimedOut              = 0,
    kTimerTypeReleased              = 1
} TimerType;

enum {
    kIOPMSystemActivityAssertionDisabled = 0,
    kIOPMSystemActivityAssertionEnabled  = 1
};


/*
 * Maximum delay allowed(in Mins) for turning off the display after the
 * PreventDisplayIdleSleep assertion is released
 */
#define kPMMaxDisplayTurnOffDelay  (5)


// Globals

uint32_t gSAAssertionBehaviorFlags = kIOPMSystemActivityAssertionEnabled;

CFArrayRef copyScheduledPowerEvents(void);
CFDictionaryRef copyRepeatPowerEvents(void);

// forward

static void                         sendSmartBatteryCommand(uint32_t which, uint32_t level);
static void                         sendUserAssertionsToKernel(uint32_t user_assertions);
static void                         evaluateAssertions(void);
static void                         HandleProcessExit(pid_t deadPID);

static bool                         callerIsEntitledToAssertion(audit_token_t token,
                                                                CFDictionaryRef newAssertionProperties);
#if !TARGET_OS_EMBEDDED
__private_extern__ void             logASLAssertionsAggregate( );

#else
#define                             logASLAssertionsAggregate()
#endif

static bool                         propertiesDictRequiresRoot(CFDictionaryRef   props);
static IOReturn                     doRetain(pid_t pid, IOPMAssertionID id);
static IOReturn                     doRelease(pid_t pid, IOPMAssertionID id);
static IOReturn                     doSetProperties(pid_t pid, 
                                                    IOPMAssertionID id, 
                                                    CFDictionaryRef props);

static CFArrayRef                   copyPIDAssertionDictionaryFlattened(void);
static CFDictionaryRef              copyAggregateValuesDictionary(void);

static IOReturn                     doCreate(pid_t pid, CFMutableDictionaryRef newProperties,
                                             IOPMAssertionID *assertion_id, ProcessInfo **pinfo);
static IOReturn                     copyAssertionForID(pid_t inPID, int inID,
                                                       CFMutableDictionaryRef  *outAssertion);

static ProcessInfo*                 processInfoCreate(pid_t p);
static ProcessInfo*                 processInfoRetain(pid_t p);
static void                         processInfoRelease(pid_t p);
static ProcessInfo*                 processInfoGet(pid_t p);
static void                         sendActivityTickle ();
static void                         setClamshellSleepState(int clamshellSleepState);
static int                          getAssertionTypeIndex(CFStringRef type);

static void                         handleAssertionTimeout(assertionType_t *assertType);
static void                         resetGlobalTimer(assertionType_t *assertType, uint64_t timer);
static IOReturn                     raiseAssertion(assertion_t *assertion);
static void                         allocStatsBuf(ProcessInfo *pinfo);

__private_extern__ bool             isDisplayAsleep( );
__private_extern__ void             logASLMessageSleepServiceTerminated(int forcedTimeoutCnt);

// globals
static uint32_t                     kerAssertionBits = 0;
static int                          aggregate_assertions;
static CFStringRef                  assertion_types_arr[kIOPMNumAssertionTypes];

static CFMutableDictionaryRef       gAssertionsArray = NULL;
static CFMutableDictionaryRef       gUserAssertionTypesDict = NULL;
CFMutableDictionaryRef              gProcessDict = NULL;
assertionType_t                     gAssertionTypes[kIOPMNumAssertionTypes];
assertionEffect_t                   gAssertionEffects[kMaxAssertionEffects];
uint32_t                            gDisplaySleepTimer = 0;      /* Display Sleep timer value in mins */
unsigned long                       gIdleSleepTimer = 0;         /* Idle Sleep timer value in mins */

extern uint32_t                     gDebugFlags;
static IOPMAssertionID              gDarkWakeNetworkAssertion = kIOPMNullAssertionID;

/* Number of procs interested in kIOPMAssertionsAnyChangedNotifyString notification */
static  uint32_t                    gAnyChange = 0;
/* Number of procs interested in kIOPMAssertionsChangedNotifyString notification */
static  uint32_t                    gAggChange = 0;
/* Number of procs interested in kIOPMAssertionTimedOutNotifyString notification */
static  uint32_t                    gTimeoutChange = 0;

uint32_t                            gActivityAggCnt = 0; // Number of requests received to enable activity aggregation

#pragma mark -
#pragma mark MIG

/******************************************************************************
 ******************************************************************************
 ******************************************************************************
 * MIG Handlers
 ******************************************************************************
 ******************************************************************************
 *****************************************************************************/
#if !TARGET_OS_EMBEDDED
void updateAppSleepStates(ProcessInfo *pinfo, int *disableAppSleep, int *enableAppSleep)
{
    if (!pinfo) return;

    if ((disableAppSleep) && (pinfo->disableAS_pend == true)) {
        *disableAppSleep = 1;
        pinfo->disableAS_pend = false;
    }
    if ((enableAppSleep) && (pinfo->enableAS_pend == true)) {
        *enableAppSleep = 1;
        pinfo->enableAS_pend = false;
    }
}
#endif

kern_return_t _io_pm_assertion_create (
                                       mach_port_t         server __unused,
                                       audit_token_t       token,
                                       vm_offset_t         props,
                                       mach_msg_type_number_t  propsCnt,
                                       int                 *assertion_id,
                                       int                 *disableAppSleep,
                                       int                 *return_code)
{
    CFMutableDictionaryRef     newAssertionProperties = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;
    ProcessInfo         *pinfo = NULL;

    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    

    *disableAppSleep = 0;
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        newAssertionProperties = (CFMutableDictionaryRef)
                                    CFPropertyListCreateWithData( 0, unfolder, 
                                                                  kCFPropertyListMutableContainersAndLeaves, 
                                                                  NULL, NULL);
        CFRelease(unfolder);
    }

    if (!newAssertionProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }


    if (!callerIsEntitledToAssertion(token, newAssertionProperties))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    // Check for privileges if the assertion requires it.
    if (propertiesDictRequiresRoot(newAssertionProperties)
        && ( !(callerIsRoot(callerUID) || callerIsAdmin(callerUID, callerGID))))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    *return_code = doCreate(callerPID, newAssertionProperties, (IOPMAssertionID *)assertion_id, &pinfo);

#if !TARGET_OS_EMBEDDED
    if ((*return_code == kIOReturnSuccess) && (pinfo != NULL) ) {
        updateAppSleepStates(pinfo, disableAppSleep, NULL);
    }
#endif

exit:
    if (newAssertionProperties) {
        CFRelease(newAssertionProperties);
    }

    vm_deallocate(mach_task_self(), props, propsCnt);

    return KERN_SUCCESS;
}


/*****************************************************************************/

kern_return_t _io_pm_assertion_set_properties (
                                               mach_port_t         server __unused,
                                               audit_token_t       token,
                                               int                 assertion_id,
                                               vm_offset_t         props,
                                               mach_msg_type_number_t propsCnt,
                                               int                 *disableAppSleep,
                                               int                 *enableAppSleep,
                                               int                 *return_code) 
{
    CFDictionaryRef     setProperties = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

    *disableAppSleep = 0;
    *enableAppSleep = 0;
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        setProperties = (CFDictionaryRef)CFPropertyListCreateWithData(0, unfolder, 0, NULL, NULL);
        CFRelease(unfolder);
    }

    if (!setProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    *return_code = doSetProperties(callerPID, assertion_id, setProperties);
#if !TARGET_OS_EMBEDDED
    if (*return_code == kIOReturnSuccess) {
        updateAppSleepStates(processInfoGet(callerPID), disableAppSleep, enableAppSleep);
    }
#endif

    CFRelease(setProperties);

exit:
    vm_deallocate(mach_task_self(), props, propsCnt);

    return KERN_SUCCESS;

}

/*****************************************************************************/
kern_return_t _io_pm_assertion_retain_release (
                                               mach_port_t         server __unused,
                                               audit_token_t       token,
                                               int                 assertion_id,
                                               int                 action,
                                               int                 *disableAppSleep,
                                               int                 *enableAppSleep,
                                               int                 *return_code) 
{
    pid_t               callerPID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

    *disableAppSleep = 0;
    *enableAppSleep = 0;
    if (kIOPMAssertionMIGDoRetain == action) {
        *return_code = doRetain(callerPID, assertion_id);
    } else {
        *return_code = doRelease(callerPID, assertion_id);
    }
#if !TARGET_OS_EMBEDDED
    if (*return_code == kIOReturnSuccess) {
        updateAppSleepStates(processInfoGet(callerPID), disableAppSleep, enableAppSleep);
    }
#endif
    return KERN_SUCCESS;
}

/*****************************************************************************/
kern_return_t _io_pm_assertion_copy_details (
                                             mach_port_t         server,
                                             audit_token_t       token,
                                             int                 assertion_id,
                                             int                 whichData,
                                             vm_offset_t         *assertions,
                                             mach_msg_type_number_t  *assertionsCnt,
                                             int                 *return_val) 
{
    CFTypeRef           theCollection = NULL;
    CFDataRef           serializedDetails = NULL;
    pid_t               callerPID = -1;


    *return_val = kIOReturnNotFound;

    if (kIOPMAssertionMIGCopyAll == whichData)
    {
        theCollection = copyPIDAssertionDictionaryFlattened();

    } else if (kIOPMAssertionMIGCopyOneAssertionProperties == whichData) 
    {
        audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

        *return_val = copyAssertionForID(callerPID, assertion_id,  
                                         (CFMutableDictionaryRef *)&theCollection);

    } else if (kIOPMAssertionMIGCopyStatus == whichData)
    {
        theCollection = copyAggregateValuesDictionary();

    } else if (kIOPMPowerEventsMIGCopyScheduledEvents == whichData)
    {
        theCollection = copyScheduledPowerEvents();
    }
    else if (kIOPMPowerEventsMIGCopyRepeatEvents == whichData)
    {
        theCollection = copyRepeatPowerEvents();
    }

    if (!theCollection) {
        *assertionsCnt = 0;
        *assertions = 0;
        *return_val = kIOReturnSuccess;
        return KERN_SUCCESS;
    }


    serializedDetails = CFPropertyListCreateData(0, theCollection, 
                                                 kCFPropertyListBinaryFormat_v1_0, 0, NULL);            

    CFRelease(theCollection);        

    if (serializedDetails) 
    {
        *assertionsCnt = (mach_msg_type_number_t)CFDataGetLength(serializedDetails);

        vm_allocate(mach_task_self(), (vm_address_t *)assertions, *assertionsCnt, TRUE);

        memcpy((void *)*assertions, CFDataGetBytePtr(serializedDetails), *assertionsCnt);

        CFRelease(serializedDetails);

        *return_val = kIOReturnSuccess;
    } else {
        *return_val = kIOReturnInternalError;
    }

    return KERN_SUCCESS;
}

kern_return_t _io_pm_ctl_assertion_type (
                                         mach_port_t         server,
                                         audit_token_t       token,
                                         string_t            type, 
                                         int                 op,
                                         int                 *return_code)
{
    CFStringRef typeRef = NULL;
    uid_t       callerEUID;
    int         idx;

    *return_code = kIOReturnError;
    audit_token_to_au32(token, NULL, &callerEUID, NULL, NULL, NULL, NULL, NULL, NULL);
    if (callerEUID != 0) {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    if (type && strlen(type)) {
        typeRef = CFStringCreateWithCString(0, type, kCFStringEncodingUTF8);
    }
    if (!isA_CFString(typeRef))
        goto exit;

    if ( (idx = getAssertionTypeIndex(typeRef)) < 0 ) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    *return_code = kIOReturnSuccess;    
    if (op == kIOPMDisableAssertionType) {
        disableAssertionType(idx);
    }
    else if (op == kIOPMEnableAssertionType) {
        enableAssertionType(idx);
    }
    else
        *return_code = kIOReturnBadArgument;
exit:
    if (typeRef) CFRelease(typeRef);

    return KERN_SUCCESS;

}

// Changes clamshell sleep state
// 1 - disabled, 0 - enabled
static void setClamshellSleepState(int clamshellSleepState)
{
    io_connect_t        connect = IO_OBJECT_NULL;
    const uint64_t      in = (uint64_t)clamshellSleepState;
    static int          prevState = -1;

    if ( prevState == clamshellSleepState) return;
    prevState = clamshellSleepState;

    if ( (connect = getRootDomainConnect()) == IO_OBJECT_NULL)
        return;


    IOConnectCallMethod(connect, kPMSetClamshellSleepState, 
                        &in, 1, 
                        NULL, 0, NULL, 
                        NULL, NULL, NULL);
    return;
}


static void sendActivityTickle ()
{
    io_connect_t                connect = IO_OBJECT_NULL;

    SystemLoadUserActiveAssertions(true);
    if (!isDisplayAsleep( ))
        return; // Nothing to do when display is on

    if ( (connect = getRootDomainConnect()) == IO_OBJECT_NULL)
        return;

    IOConnectCallMethod(connect, kPMActivityTickle, 
                        NULL, 0, 
                        NULL, 0, NULL, 
                        NULL, NULL, NULL);

    return;

}
static void sendUserAssertionsToKernel(uint32_t user_assertions)
{
    io_connect_t                connect = IO_OBJECT_NULL;
    const uint64_t              in = (uint64_t)user_assertions;

    if ( (connect = getRootDomainConnect()) == IO_OBJECT_NULL)
        return;

    IOConnectCallMethod(connect, kPMSetUserAssertionLevels, 
                        &in, 1, 
                        NULL, 0, NULL, 
                        NULL, NULL, NULL);

    return;
}

#pragma mark -
#pragma mark Act on assertions

__private_extern__ void PMAssertions_SettingsHaveChanged(void)
{
    static int lastDWBTSetting = -1;
    int newDWBT = GetPMSettingBool(CFSTR(kIOPMDarkWakeBackgroundTaskKey));

    if (newDWBT == lastDWBTSetting) {
        return;
    }
    lastDWBTSetting = newDWBT;

    configAssertionType(kBackgroundTaskType, false);
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

    if (MACH_PORT_NULL == sbmanager) {
        goto bail;
    }

    kret = IOServiceOpen( sbmanager, mach_task_self(), 0, &sbconnection);
    if (kIOReturnSuccess != kret) {
        goto bail;
    }

    IOConnectCallMethod(
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
                                              (which == kSBUCChargeInhibit) ? 
                                                    CFSTR(kIOPMPSIsChargingKey) : CFSTR(kIOPMPSExternalConnectedKey), 
                                              level ? kCFBooleanFalse : kCFBooleanTrue);
            IOObjectRelease(next);
        }
        IOObjectRelease(iter);
    }
    while (false);
    return;
}

#endif /* HAVE_SMART_BATTERY */


__private_extern__ IOReturn  _IOPMSetActivePowerProfilesRequiresRoot (
                                                                      CFDictionaryRef which_profile, 
                                                                      int uid, 
                                                                      int gid)
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

    if ( (!callerIsRoot(uid) &&
          !callerIsAdmin(uid, gid) &&
          !callerIsConsole(uid, gid)) || 
         ( (-1 == uid) || (-1 == gid) ))
    {
        ret = kIOReturnNotPrivileged;
        goto exit;    
    }

    if (!which_profile) {
        // We leave most of the input argument vetting for IOPMSetActivePowerProfiles,
        // which checks the contents of the dictionary. At this point we assume it
        // is well formed (and that it exists).
        ret = kIOReturnBadArgument;
        goto exit;
    }

    energyPrefs = SCPreferencesCreate( kCFAllocatorDefault, CFSTR(kIOPMAppName), CFSTR(kIOPMPrefsPath) );
    if (!energyPrefs) {
        goto exit;
    }

    if (!SCPreferencesLock(energyPrefs, true))
    {  
        ret = kIOReturnInternalError;
        goto exit;
    }

    if (!SCPreferencesSetValue(energyPrefs, CFSTR("ActivePowerProfiles"), which_profile)) {
        goto exit;
    }

    if (!SCPreferencesCommitChanges(energyPrefs))
    {
        // handle error
        if (kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }

    if (!SCPreferencesApplyChanges(energyPrefs))
    {
        // handle error
        if (kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;        
        goto exit;
    }

    ret = kIOReturnSuccess;
exit:
    if (energyPrefs) {
        SCPreferencesUnlock(energyPrefs);
        CFRelease(energyPrefs);
    }
    return ret;
}

/*
 * This functions creates assertions required at boot time.
 */
void createOnBootAssertions( )
{
    CFMutableDictionaryRef assertionDescription = NULL;

    int value;
    IOReturn ret;
    /* 
     * For now only 'preventSystemSleep' assertion is created
     * for FindMyMacd if system is booting in wipe mode.
     */
    ret = getNvramArgInt(FMMD_WIPE_BOOT_ARG, &value);
    if (ret == kIOReturnSuccess && value > 0) {
        /* Create 'PreventSystemSleep' assertion */
        assertionDescription = _IOPMAssertionDescriptionCreate(
                                                               kIOPMAssertionTypePreventSystemSleep,
                                                               CFSTR("com.apple.powermanagement.fmmdwipe"),
                                                               NULL, CFSTR("Proxy Assertion during FMMD system wipe"), NULL,
                                                               120, kIOPMAssertionTimeoutActionRelease);

        if (assertionDescription)
        {
            /* This assertion should be applied even on battery power */
            CFDictionarySetValue(assertionDescription, 
                                 kIOPMAssertionAppliesToLimitedPowerKey, (CFBooleanRef)kCFBooleanTrue);
            InternalCreateAssertion(assertionDescription, NULL);

            CFRelease(assertionDescription);
        }
    }
}

/***********************************
 * Dynamic Assertions
 ***********************************/


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static CFDictionaryRef copyAggregateValuesDictionary(void)
{
    CFDictionaryRef                 assertions_info = NULL;
    CFNumberRef                     cf_agg_vals[kIOPMNumAssertionTypes];
    int                             i;

    // Massage int values into CFNumbers for CFDictionaryCreate
    for (i=0; i<kIOPMNumAssertionTypes; i++)
    {
        int tmp_bit = getAssertionLevel(i);

        cf_agg_vals[i] = CFNumberCreate(0, kCFNumberIntType, &tmp_bit);
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
    for (i=0; i<kIOPMNumAssertionTypes; i++)
    {
        CFRelease(cf_agg_vals[i]);
    }

    // TODO: strip unsupported assertions?

    return assertions_info;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

__private_extern__ void _PMAssertionsDriverAssertionsHaveChanged(uint32_t changedDriverAssertions)
{
    if (gAggChange)
        notify_post( kIOPMAssertionsChangedNotifyString );
}


#define     kDarkWakeNetworkHoldForSeconds          30
#define     kDeviceEnumerationHoldForSeconds        (45LL)

#pragma mark -
#pragma mark powerd-Internal Use Only


__private_extern__ CFMutableDictionaryRef _IOPMAssertionDescriptionCreate(
                                                                          CFStringRef AssertionType, 
                                                                          CFStringRef Name, 
                                                                          CFStringRef Details,
                                                                          CFStringRef HumanReadableReason,
                                                                          CFStringRef LocalizationBundlePath,
                                                                          CFTimeInterval Timeout,
                                                                          CFStringRef TimeoutBehavior)
{
    CFMutableDictionaryRef  descriptor = NULL;

    if (!AssertionType || !Name) {
        return NULL;
    }

    descriptor = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!descriptor) {
        return NULL;
    }

    CFDictionarySetValue(descriptor, kIOPMAssertionNameKey, Name);

    int _on = kIOPMAssertionLevelOn;
    CFNumberRef _on_num = CFNumberCreate(0, kCFNumberIntType, &_on);
    CFDictionarySetValue(descriptor, kIOPMAssertionLevelKey, _on_num);
    CFRelease(_on_num);

    CFDictionarySetValue(descriptor, kIOPMAssertionTypeKey, AssertionType);

    if (Details) {
        CFDictionarySetValue(descriptor, kIOPMAssertionDetailsKey, Details);
    }
    if (HumanReadableReason) {
        CFDictionarySetValue(descriptor, kIOPMAssertionHumanReadableReasonKey, HumanReadableReason);
    }
    if (LocalizationBundlePath) {
        CFDictionarySetValue(descriptor, kIOPMAssertionLocalizationBundlePathKey, LocalizationBundlePath);
    }
    if (Timeout) {
        CFNumberRef Timeout_num = CFNumberCreate(0, kCFNumberDoubleType, &Timeout);
        CFDictionarySetValue(descriptor, kIOPMAssertionTimeoutKey, Timeout_num);
        CFRelease(Timeout_num);
    }
    if (TimeoutBehavior)
    {
        CFDictionarySetValue(descriptor, kIOPMAssertionTimeoutActionKey, TimeoutBehavior);
    }

    return descriptor;
}



__private_extern__ IOReturn InternalCreateAssertion(
                                                    CFMutableDictionaryRef properties, 
                                                    IOPMAssertionID *outID)
{
    if (!properties) 
        return kIOReturnBadArgument;

    CFRetain(properties);

    CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{
                          if (outID == NULL) {
                          /* Some don't care for assertionId */
                          IOPMAssertionID   assertionID = kIOPMNullAssertionID;
                          doCreate(getpid(), properties, &assertionID, NULL);    
                          }
                          else if ( *outID == kIOPMNullAssertionID )
                          doCreate(getpid(), properties, outID, NULL);    

                          CFRelease(properties);
                          });
    CFRunLoopWakeUp(_getPMRunLoop());

    return kIOReturnSuccess;
}

__private_extern__ void InternalReleaseAssertion(
                                                 IOPMAssertionID *outID)
{
    CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{
                          if ( *outID != kIOPMNullAssertionID ) {
                          doRelease(getpid(), *outID);
                          }
                          *outID = kIOPMNullAssertionID;
                          });
    CFRunLoopWakeUp(_getPMRunLoop());
}

__private_extern__ void InternalEvaluateAssertions(void)
{
    CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{
                          evaluateAssertions();
                          });
    CFRunLoopWakeUp(_getPMRunLoop());

}

static IOReturn _localCreateAssertionWithTimer(
                                               CFStringRef type, CFStringRef name, int timerSecs, IOPMAssertionID *outID)
{
    CFMutableDictionaryRef          dict = NULL;

    if (!type || !name || !outID) {
        return kIOReturnBadArgument;
    }

    if ( (dict = _IOPMAssertionDescriptionCreate(type, name, NULL, NULL, NULL,
                                                 (CFTimeInterval)timerSecs, kIOPMAssertionTimeoutActionRelease)) )
    {
        doCreate(getpid(), dict, outID, NULL);
        CFRelease(dict);
    }

    return kIOReturnSuccess;

}
static IOReturn _localCreateAssertion(CFStringRef type, CFStringRef name, IOPMAssertionID *outID)
{
    return _localCreateAssertionWithTimer(type, name, 0, outID);
}

static IOReturn _enableAssertionForLimitedPower(pid_t pid, IOPMAssertionID id)
{
    CFMutableDictionaryRef          dict = NULL;
    IOReturn                        rc = kIOReturnError;

    if (!id)
        return kIOReturnBadArgument;

    if ((dict = CFDictionaryCreateMutable(0, 0, 
                                          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)))
    {
        CFDictionarySetValue(dict, kIOPMAssertionAppliesToLimitedPowerKey, kCFBooleanTrue);

        rc = doSetProperties(pid, id, dict);

        CFRelease(dict);
    }

    return rc;
}

/* _DarkWakeHandleNetworkWake runs when PM enters dark wake via network packet.
 * It creates a temporary assertion that keeps the system awake hopefully long enough
 * for a remote client to connect, and for the server to create an assertion
 * keeping the system awak.
 */
static void  _DarkWakeHandleNetworkWake(void)
{
    IOReturn rc;

    rc = _localCreateAssertionWithTimer(kIOPMAssertInternalPreventSleep, 
                                        CFSTR("Network wake delay proxy assertion"),
                                        30, &gDarkWakeNetworkAssertion);

    if (rc != kIOReturnSuccess)
        return;

    /* Enable this assertion on battery power also */
    _enableAssertionForLimitedPower(getpid(), gDarkWakeNetworkAssertion);

    return ;
}



typedef struct notifyRegInfo {
    IONotificationPortRef port;
    io_object_t handle;
} notifyRegInfo_st;

static void _DeregisterForNotification(
                                       notifyRegInfo_st *notifyInfo)
{
    if (notifyInfo->handle)
        IOObjectRelease(notifyInfo->handle);

    if (notifyInfo->port)
        IONotificationPortDestroy(notifyInfo->port);

    if (notifyInfo)
        free(notifyInfo);
}

/*
 * Register for specific interestType with service at specified 'path' */
static notifyRegInfo_st * _RegisterForNotification( 
                                                   const io_string_t	path, const io_name_t 	interestType,
                                                   IOServiceInterestCallback callback, void *refCon)
{

    io_service_t	obj = MACH_PORT_NULL;
    notifyRegInfo_st  *notifyInfo = NULL;
    kern_return_t kr;

    notifyInfo = calloc(sizeof(notifyRegInfo_st), 1);
    if ( !notifyInfo )
        goto exit;

    notifyInfo->port = IONotificationPortCreate( kIOMasterPortDefault );
    if ( !notifyInfo->port ) 
        goto exit;

    obj = IORegistryEntryFromPath( kIOMasterPortDefault, path);
    if ( !obj )
        goto exit;

    kr = IOServiceAddInterestNotification(
                                          notifyInfo->port,
                                          obj, interestType,
                                          callback, refCon,
                                          &notifyInfo->handle );

    if (kr !=  KERN_SUCCESS)
        goto exit;


    IOObjectRelease(obj);
    return  notifyInfo;

exit:
    if (obj)
        IOObjectRelease(obj);

    if (notifyInfo)
    {
        if (notifyInfo->handle)
            IOObjectRelease(notifyInfo->handle);
        if (notifyInfo->port)
            IONotificationPortDestroy(notifyInfo->port);
        free(notifyInfo);
    }

    return NULL;
}

typedef struct devEnumInfo {
    dispatch_source_t   dispSrc; /* Dispatched 5sec after IOKit is quiet */
    bool                suspended; /* true if 'dispSrc' is suspended */
    dispatch_source_t   dispSrc2; /* Dispatched 45sec after assertion is created */
    notifyRegInfo_st    *notifyInfo;
    IOPMAssertionID assertId;
}devEnumInfo_st;

/*
 * Function that releases the assertion and cleans up all dispatch queues */
static void devEnumerationDone( devEnumInfo_st *deInfo )
{

    /* First cancel the dispatch sources */
    if (deInfo->dispSrc && (dispatch_source_testcancel(deInfo->dispSrc) == 0)) {
        dispatch_source_cancel(deInfo->dispSrc);
    }

    if (deInfo->dispSrc2 && (dispatch_source_testcancel(deInfo->dispSrc2) == 0)) {
        dispatch_source_cancel(deInfo->dispSrc2);
    }
    /* De-register from IOkit busy state updates */
    if (deInfo->notifyInfo) {
        _DeregisterForNotification(deInfo->notifyInfo);
        deInfo->notifyInfo = 0;
    }

    /* Release the assertion */
    if (deInfo->assertId) {
        doRelease(getpid(), deInfo->assertId);
        deInfo->assertId = 0;
        free(deInfo);
    }
}

void ioKitStateCallback (
                         void *			refcon,
                         io_service_t		service,
                         uint32_t		messageType,
                         void *			messageArgument ) 
{
    devEnumInfo_st *deInfo = (devEnumInfo_st *)refcon;
    long state = (long)messageArgument;
    dispatch_source_t dispSrc;


    if (messageType != kIOMessageServiceBusyStateChange)
        return;

    if (state) {
        /* IOKit is busy. Suspend the timer until the Iokit is free */
        if (deInfo->dispSrc && !deInfo->suspended) {
            dispatch_suspend(deInfo->dispSrc);
            deInfo->suspended = true;
        }
    }
    else {
        /* 
         * IOkit is free. Create/extend a timer to dispatch a function 
         * that can release the device enumeration assertion.
         */
        if (deInfo->dispSrc == 0) {
            dispSrc  = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 
                                              0, dispatch_get_main_queue());

            dispatch_source_set_event_handler(dispSrc, ^{
                                              devEnumerationDone(deInfo);
                                              });

            dispatch_source_set_cancel_handler(dispSrc, ^{
                                               dispatch_release(dispSrc);
                                               });

            deInfo->dispSrc = dispSrc;
            deInfo->suspended = true;
        }
        if (deInfo->suspended) {
            dispatch_source_set_timer(deInfo->dispSrc, dispatch_time(DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC), 
                                      DISPATCH_TIME_FOREVER, 0);
            dispatch_resume(deInfo->dispSrc);
        }

        deInfo->suspended = false;
    }


}


static void _AssertForDeviceEnumeration(CFStringRef wakeType)
{
    IOPMAssertionID     deviceEnumerationAssertion = kIOPMNullAssertionID;
    notifyRegInfo_st    *notifyInfo = NULL;
    devEnumInfo_st *deInfo = NULL;
    IOReturn rc;
    dispatch_source_t dispSrc;
    
    if (isA_CFString(wakeType) && (
        CFEqual(wakeType, kIOPMRootDomainWakeTypeAlarm) ||
        CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) ||
        CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepTimer) ||
        CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepService) ||
        CFEqual(wakeType, kIOPMrootDomainWakeTypeLowBattery) ||
        CFEqual(wakeType, kIOPMRootDomainWakeTypeNotification)
       )) {

        /* No need to take proxy for device enumeration */
        return;
    }

    rc = _localCreateAssertion(kIOPMAssertInternalPreventSleep, 
                               CFSTR("PM configd - Wait for Device enumeration"), 
                               &deviceEnumerationAssertion);
    if (rc != kIOReturnSuccess)
        return;

    /* Enable this assertion on battery power also */
    _enableAssertionForLimitedPower(getpid(), deviceEnumerationAssertion);

    deInfo = calloc(sizeof(devEnumInfo_st), 1);
    if (!deInfo) {
        goto exit;
    }

    /* Register IOService Busy/free notifications */
    notifyInfo = _RegisterForNotification(kIOServicePlane ":/", 
                                          kIOBusyInterest, ioKitStateCallback, (void *)deInfo);
    if ( !notifyInfo ) {
        /* Failed to register for notification. Remove the assertion immediately */
        goto exit;
    }

    deInfo->notifyInfo = notifyInfo;
    deInfo->assertId = deviceEnumerationAssertion;

    /* 
     * Create a higher level timer dispatch, which guarantees that assertion is 
     * released irrespective of the IOkit busy/quiet state.
     */

    dispSrc = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 
                                     0, dispatch_get_main_queue());
    dispatch_source_set_timer(dispSrc, 
                              dispatch_time(DISPATCH_TIME_NOW, kDeviceEnumerationHoldForSeconds * NSEC_PER_SEC), 
                              DISPATCH_TIME_FOREVER, 0);

    dispatch_source_set_event_handler(dispSrc, ^{
                                      devEnumerationDone(deInfo);
                                      });

    dispatch_source_set_cancel_handler(dispSrc, ^{
                                       dispatch_release(dispSrc);
                                       });

    deInfo->dispSrc2 = dispSrc;
    dispatch_resume(deInfo->dispSrc2);


    IONotificationPortSetDispatchQueue(notifyInfo->port, dispatch_get_main_queue());

    return;

exit:
    if (deInfo) 
        free(deInfo);

    if (notifyInfo)
        _DeregisterForNotification(notifyInfo);

    doRelease(getpid(), deviceEnumerationAssertion); 
}

/*
 * Create assertions for a short period on behalf of other modules/processes. This is to
 * give a chance to those modules to create any required assertions on their own.
 * Otherwise, system may go back to sleep before those modules got a chance to
 * complete their processing.
 */
__private_extern__ void _ProxyAssertions(const struct IOPMSystemCapabilityChangeParameters *capArgs)
{

    CFStringRef         wakeType = NULL;
    CFStringRef         wakeReason = NULL;
    IOPMAssertionID     pushSvcAssert = kIOPMNullAssertionID;

    if ( !(kIOPMSystemCapabilityDidChange & capArgs->changeFlags) )
        return;


    if ( IOPMIsADarkWake(capArgs->toCapabilities) &&
         IOPMIsASleep(capArgs->fromCapabilities) )
    {
        wakeType = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeTypeKey));
        wakeReason = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeReasonKey));

        if (isA_CFString(wakeReason) && CFEqual(wakeReason, kIORootDomainWakeReasonDarkPME))
        {
            if (isA_CFString(wakeType) && CFEqual(wakeType, kIOPMRootDomainWakeTypeNetwork))
                _DarkWakeHandleNetworkWake( );
            else
                _AssertForDeviceEnumeration(wakeType);
        }
        else if (isA_CFString(wakeType))
        {
            if (CFEqual(wakeType, kIOPMRootDomainWakeTypeNetwork))
                _DarkWakeHandleNetworkWake( );
            else if (CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) &&
                     CFEqual(wakeReason, kIOPMRootDomainWakeReasonRTC))
            {
                _localCreateAssertionWithTimer(kIOPMAssertionTypeBackgroundTask, 
                                               CFSTR("Powerd - Wait for client BackgroundTask assertions"), 10, &pushSvcAssert);
            }
            else {
                _AssertForDeviceEnumeration(wakeType);
            }
        }
        else 
        {
            /* 
             * Wake type is not set for some cases. Only wake reason is set.
             * Plugging a new USB device while sleeping is one such case
             */
            _AssertForDeviceEnumeration(wakeType);
        }

        if (wakeType) {
            CFRelease(wakeType);
        }
        if (wakeReason)
            CFRelease(wakeReason);
    }

}

/*
 * Takes an assertion to keep display on for up to
 * a max of 'kPMMaxDisplayTurnOffDelay' minutes
 */
void delayDisplayTurnOff( )
{

    CFNumberRef  levelNum = NULL;
    CFNumberRef  Timeout_num = NULL;
    CFTimeInterval   delay = 0;

    IOPMAssertionLevel        level = kIOPMAssertionLevelOn;
    static IOPMAssertionID    id = kIOPMNullAssertionID; 
    CFMutableDictionaryRef    dict = NULL;

    if (gDisplaySleepTimer)
        delay = gDisplaySleepTimer > kPMMaxDisplayTurnOffDelay ?
            kPMMaxDisplayTurnOffDelay : gDisplaySleepTimer;
    else 
        delay = kPMMaxDisplayTurnOffDelay;

    delay *= 60;

    if (id == kIOPMNullAssertionID) 
    {
        dict = _IOPMAssertionDescriptionCreate(
                                               kIOPMAssertInternalPreventDisplaySleep,
                                               CFSTR("com.apple.powermanagement.delayDisplayOff"),
                                               NULL, CFSTR("Proxy to delay display off"), NULL, 
                                               (CFTimeInterval)delay, kIOPMAssertionTimeoutActionTurnOff);

        if (dict) {
            doCreate(getpid(), dict, &id, NULL);    
            CFRelease(dict);
        }
    }
    else 
    {
        dict = CFDictionaryCreateMutable(0, 0, 
                                         &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        levelNum = CFNumberCreate(0, kCFNumberIntType, &level);
        Timeout_num = CFNumberCreate(0, kCFNumberDoubleType, &delay);

        if (dict && levelNum && Timeout_num)
        {
            CFDictionarySetValue(dict, kIOPMAssertionLevelKey, levelNum);
            CFDictionarySetValue(dict, kIOPMAssertionTimeoutKey, Timeout_num);
            doSetProperties(getpid(), id, dict);
        }
        if (dict) CFRelease(dict);
        if (levelNum) CFRelease(levelNum);
        if (Timeout_num) CFRelease(Timeout_num);
    }


}

static bool propertiesDictRequiresRoot(CFDictionaryRef props)
{
    if ( CFDictionaryGetValue(props, kIOPMInflowDisableAssertion)
         || CFDictionaryGetValue(props, kIOPMChargeInhibitAssertion) )
    {
        return true;
    } else {
        return false;
    }
}



static bool callerIsEntitledToAssertion(
                                        audit_token_t token,
                                        CFDictionaryRef newAssertionProperties)
{
    CFStringRef         assert_type = NULL;
    bool                caller_is_allowed = true;
    int                 idx = -1;

    assert_type = CFDictionaryGetValue(newAssertionProperties, kIOPMAssertionTypeKey);
    idx = getAssertionTypeIndex(assert_type);
    if ( (idx < 0) || (gAssertionTypes[idx].entitlement == NULL))
        return true;

    caller_is_allowed = auditTokenHasEntitlement(token, gAssertionTypes[idx].entitlement);

    return caller_is_allowed;
}


static ProcessInfo* processInfoCreate(pid_t p)
{
    ProcessInfo             *proc = NULL;
    char                    name[kProcNameBufLen];
    static  uint32_t        create_seq = 0;

    proc = calloc(1, sizeof(ProcessInfo));
    if (!proc) return NULL;


    proc->disp_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, p, 
                                            DISPATCH_PROC_EXIT, dispatch_get_main_queue());
    if (proc->disp_src == NULL) {
        free(proc);
        return NULL;
    }

    dispatch_source_set_event_handler(proc->disp_src, ^{
                                      HandleProcessExit(p);
                                      });

    dispatch_source_set_cancel_handler(proc->disp_src, ^{
                                       dispatch_release(proc->disp_src);
                                       });

    dispatch_resume(proc->disp_src);

    proc_name(p, name, sizeof(name));
    proc->name = CFStringCreateWithCString(0, name, kCFStringEncodingUTF8);
    proc->pid = p;
    proc->retain_cnt++;
    proc->create_seq = create_seq++;

    CFDictionarySetValue(gProcessDict, (uintptr_t)p, (const void *)proc);

    return proc;
}

/* Wrap a pointer to a non-CF object inside a CFData, so that 
 * we can place it an a CF container.
 */

static ProcessInfo* processInfoRetain(pid_t p)
{    
    ProcessInfo       *proc = NULL;
    proc = (ProcessInfo *)CFDictionaryGetValue(gProcessDict, (uintptr_t)p);

    if (proc) {
        if (proc->retain_cnt != UINT_MAX) proc->retain_cnt++;
        return proc;
    }

    return NULL;

}
static ProcessInfo* processInfoGet(pid_t p)
{    
    ProcessInfo       *proc = NULL;
    proc = (ProcessInfo *)CFDictionaryGetValue(gProcessDict, (uintptr_t)p);

    if (proc) {
        return proc;
    }

    return NULL;
}

CFStringRef processInfoGetName(pid_t p)
{
    ProcessInfo         *proc = NULL;
    CFStringRef         retString = NULL;
    proc = (ProcessInfo *)CFDictionaryGetValue(gProcessDict, (uintptr_t)p);

    if (proc) {
        retString = proc->name;
    }

    return retString;
}

static void processInfoRelease(pid_t p)
{
    ProcessInfo   *proc = NULL;

    proc = (ProcessInfo *)CFDictionaryGetValue(gProcessDict, (uintptr_t)p);

    if (!proc) return;



    if (proc->retain_cnt == 1) {

        dispatch_release(proc->disp_src);
        if (proc->name) CFRelease(proc->name);
        CFDictionaryRemoveValue(gProcessDict, (uintptr_t)p);
        free(proc);
    }
    else {
        proc->retain_cnt--;
    }
    return ;
}

#if !TARGET_OS_EMBEDDED
static void disableAppSleep(ProcessInfo *pinfo)
{
    char notify_str[128];

    if (pinfo->disableAS_pend == false)
        return;

    pinfo->disableAS_pend = false;
    snprintf(notify_str, sizeof(notify_str), "%s.%d", 
             kIOPMDisableAppSleepPrefix,pinfo->pid);
    notify_post(notify_str);

}

static void enableAppSleep(ProcessInfo *pinfo)
{
    char notify_str[128];

    if (pinfo->enableAS_pend == false)
        return;

    pinfo->enableAS_pend = false;
    snprintf(notify_str, sizeof(notify_str), "%s.%d", 
             kIOPMEnableAppSleepPrefix, pinfo->pid);
    notify_post(notify_str);
}
#endif


void schedDisableAppSleep(assertion_t *assertion)
{
#if !TARGET_OS_EMBEDDED
    assertionType_t     *assertType = NULL;
    ProcessInfo         *pinfo = NULL;
    uint32_t            agg;

    assertType = &gAssertionTypes[assertion->kassert]; 

    if ( !(assertType->flags & kAssertionTypePreventAppSleep)) return;

    pinfo = assertion->pinfo;
    if (pinfo->pid == getpid()) return;


    if (pinfo->assert_cnt[assertion->kassert] == UCHAR_MAX) return;
    pinfo->assert_cnt[assertion->kassert]++;

    agg = pinfo->aggTypes;
    pinfo->aggTypes |= ( 1 << assertion->kassert );
    if (agg == 0) {
        processInfoRetain(pinfo->pid);
        pinfo->disableAS_pend = true;
        CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, 
                              ^{ 
                              disableAppSleep(pinfo);
                              processInfoRelease(pinfo->pid);
                              });
        CFRunLoopWakeUp(_getPMRunLoop());
    }
#endif
}


void schedEnableAppSleep(assertion_t *assertion)
{
#if !TARGET_OS_EMBEDDED
    assertionType_t     *assertType = NULL;
    ProcessInfo         *pinfo = NULL;

    assertType = &gAssertionTypes[assertion->kassert]; 
    if ( !(assertType->flags & kAssertionTypePreventAppSleep)) return;

    pinfo = assertion->pinfo;
    if (pinfo->pid == getpid()) return;


    if (--pinfo->assert_cnt[assertion->kassert] == 0) 
    {
        pinfo->aggTypes &= ~( 1 << assertion->kassert );

        if (pinfo->aggTypes == 0) {
            processInfoRetain(pinfo->pid);
            pinfo->enableAS_pend = true;
            CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, 
                                  ^{ 
                                  enableAppSleep(pinfo); 
                                  processInfoRelease(pinfo->pid);
                                  });
            CFRunLoopWakeUp(_getPMRunLoop());
        }
    }
#endif
}


void updateAppStats(assertion_t *assertion, assertionOps op)
{
    assertionType_t     *assertType = NULL;
    ProcessInfo         *pinfo = NULL;
    effectStats_t       *stats = NULL;
    uint64_t            duration = 0;

    assertType = &gAssertionTypes[assertion->kassert]; 
    
    if (gActivityAggCnt && (assertType->effectIdx < kMaxEffectStats)) {

        if (assertion->causingPid) {
            // If kIOPMAssertionOnBehalfOfPID is set, stats are collected on that process structure
            if (assertion->causingPinfo == NULL) {
                if (!(assertion->causingPinfo = processInfoRetain(assertion->causingPid))) {
                    assertion->causingPinfo = processInfoCreate(assertion->causingPid);

                    // If allocation failed, use the process creating the assertion itself
                    if (!assertion->causingPinfo)
                        assertion->causingPinfo = assertion->pinfo;
                }
            }

            pinfo = assertion->causingPinfo;
        }
        else {
            pinfo = assertion->pinfo;
        }

        if (!pinfo->reportBuf) allocStatsBuf(pinfo);

        if (pinfo->reportBuf)
            stats = &pinfo->stats[assertType->effectIdx];
    }

    switch (op) {

    case kAssertionOpRaise:
        if ((assertType->flags & kAssertionTypeNotValidOnBatt) && 
                (!(assertion->state & kAssertionStateValidOnBatt)) && (_getPowerSource() == kBatteryPowered)) {
            /*
             * If this assertion type is not allowed on battery and this assertion doesn't have override flag,
             * then don't let this assertion add to stats
             */
            stats = NULL;
        }
        if (stats && !(assertion->state & kAssertionStateAddsToProcStats)) {
            if (stats->cnt++ == 0) {
                stats->startTime = getMonotonicTime();
            }
            assertion->state |= kAssertionStateAddsToProcStats;
        }
        break;

    case kAssertionOpRelease:
        if (stats && (stats->cnt) && (assertion->state & kAssertionStateAddsToProcStats)) {
            if (--stats->cnt == 0) {
                duration = (getMonotonicTime() - stats->startTime);
                SIMPLEARRAY_INCREMENTVALUE(pinfo->reportBuf, assertType->effectIdx, duration);
            }
            assertion->state &= ~kAssertionStateAddsToProcStats;
        }
        break;

    default:
        break;
    }

}

static IOReturn lookupAssertion(pid_t pid, IOPMAssertionID id, assertion_t **assertion)
{
    unsigned int idx = INDEX_FROM_ID(id);
    assertion_t  *tmp_a = NULL;

    if (idx >= kMaxAssertions)
        return kIOReturnBadArgument;

    if( CFDictionaryGetValueIfPresent(gAssertionsArray, 
                                      (uintptr_t)idx, (const void **)&tmp_a) == false)
        return kIOReturnBadArgument;

    if (tmp_a->pinfo->pid != pid)
        return kIOReturnNotPermitted;

    *assertion = tmp_a;
    return kIOReturnSuccess;

}

kern_return_t _io_pm_change_sa_assertion_behavior (
                                                   mach_port_t             server  __unused,
                                                   audit_token_t           token,
                                                   uint32_t                newFlags,
                                                   uint32_t                *oldFlags,
                                                   int                     *return_code)
{
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    
    if (!callerIsRoot(callerUID))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    if (oldFlags)
        *oldFlags = gSAAssertionBehaviorFlags;
    gSAAssertionBehaviorFlags = newFlags;

    *return_code = kIOReturnSuccess;

exit:
    return KERN_SUCCESS;
}
kern_return_t _io_pm_declare_system_active (
                                            mach_port_t             server  __unused,
                                            audit_token_t           token,
                                            int                     *system_state,
                                            vm_offset_t             props,
                                            mach_msg_type_number_t  propsCnt,
                                            int                     *assertion_id,
                                            int                     *return_code)
{
    pid_t                   callerPID = -1;
    CFDataRef               unfolder  = NULL;
    CFMutableDictionaryRef  assertionProperties = NULL;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        assertionProperties = (CFMutableDictionaryRef)CFPropertyListCreateWithData
                                    (0, unfolder, kCFPropertyListMutableContainersAndLeaves, NULL, NULL);
        CFRelease(unfolder);
    }

    if (!assertionProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }
    *system_state = kIOPMSystemSleepNotReverted;
    *return_code = kIOReturnSuccess;


    if(_can_revert_sleep()) {
        *system_state = kIOPMSystemSleepReverted;
    }
    else {
        CFStringRef sleepReason = _getSleepReason();
        if (CFStringCompare(sleepReason, CFSTR(kIOPMIdleSleepKey), 0) == kCFCompareEqualTo) {
            // Schedule an immediate wake if system is going for an idle sleep
            int d = 1;
            CFNumberRef sleepDuration = CFNumberCreate(kCFAllocatorDefault,
                                                       kCFNumberSInt32Type,
                                                       &d);
            if (sleepDuration) {
                _setRootDomainProperty( CFSTR(kIOPMSettingDebugWakeRelativeKey),
                                        sleepDuration);
                CFRelease(sleepDuration);
            }
        }
    }

    // If set via a backdoor in pmset, this makes
    // IOPMAssertionDeclareSystemActivity() behave identically to a
    // PreventUserIdleSystemSleep assertion. It negates the side-effect
    // behaviors associated with the call
    if(gSAAssertionBehaviorFlags != kIOPMSystemActivityAssertionEnabled)
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertionTypePreventUserIdleSystemSleep);
    else
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertionTypeSystemIsActive);

    if(kIOReturnSuccess != doCreate(callerPID, assertionProperties, (IOPMAssertionID *)assertion_id, NULL))
        *return_code = kIOReturnInternalError;

exit:

    if (assertionProperties)
        CFRelease(assertionProperties);

    vm_deallocate(mach_task_self(), props, propsCnt);

    return KERN_SUCCESS;
}

kern_return_t  _io_pm_declare_user_active (   
                                           mach_port_t             server  __unused,
                                           audit_token_t           token,
                                           int                     user_type,
                                           vm_offset_t             props,
                                           mach_msg_type_number_t  propsCnt,
                                           int                     *assertion_id,
                                           int                     *disableAppSleep,
                                           int                     *return_code)
{

    CFMutableDictionaryRef      assertionProperties = NULL;
    assertion_t      *assertion = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;
    IOReturn            ret;
    bool                create_new = true;
    CFTimeInterval      displaySleepTimerSecs;
    CFNumberRef         CFdisplaySleepTimer = NULL;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);    

    *disableAppSleep = 0;
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        assertionProperties = (CFMutableDictionaryRef)
                                CFPropertyListCreateWithData(0, unfolder, 
                                                             kCFPropertyListMutableContainersAndLeaves, 
                                                             NULL, NULL);
        CFRelease(unfolder);
    }

    if (!assertionProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    /* Set the assertion timeout value to display sleep timer value, if it is not 0 */

    displaySleepTimerSecs = gDisplaySleepTimer * 60; /* Convert to secs */
    CFdisplaySleepTimer = CFNumberCreate(0, kCFNumberDoubleType, &displaySleepTimerSecs);
    CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutKey, CFdisplaySleepTimer);
    CFRelease(CFdisplaySleepTimer);

    CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutActionKey, kIOPMAssertionTimeoutActionRelease);

    /* Check if this is a repeat call on previously returned assertion id */
    do {
        if (assertion_id == NULL || *assertion_id == kIOPMNullAssertionID) 
            break;

        ret = lookupAssertion(callerPID, *assertion_id, &assertion);
        if ((kIOReturnSuccess != ret) || !assertion) 
            break;

        if (assertion->kassert != kDeclareUserActivityType)
            break;

        /* Extend the timeout timer of this assertion by display sleep timer value */

        /* First set the assertion level to ON */
        int k = kIOPMAssertionLevelOn;
        CFNumberRef useLevelOnNum = CFNumberCreate(0, kCFNumberIntType, &k);
        CFDictionarySetValue(assertion->props, kIOPMAssertionLevelKey, useLevelOnNum);
        CFRelease(useLevelOnNum);

        *return_code = doSetProperties(callerPID, *assertion_id, assertionProperties);
        create_new = false;
    } while (false);

    if (create_new) {
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertionUserIsActive);

        *return_code = doCreate(callerPID, assertionProperties, (IOPMAssertionID *)assertion_id, NULL);
        if ((*return_code == kIOReturnSuccess) && 
            (lookupAssertion(callerPID, *assertion_id, &assertion) == kIOReturnSuccess)) {
            assertion->state |= kAssertionTimeoutIsSystemTimer;
        }

    }

#if !TARGET_OS_EMBEDDED
    if (*return_code == kIOReturnSuccess)
        updateAppSleepStates(processInfoGet(callerPID), disableAppSleep, NULL);
#endif

exit:


    if (assertionProperties)
        CFRelease(assertionProperties);

    vm_deallocate(mach_task_self(), props, propsCnt);
    return KERN_SUCCESS;
}

kern_return_t  _io_pm_declare_network_client_active (   
                                                     mach_port_t             server  __unused,
                                                     audit_token_t           token,
                                                     vm_offset_t             props,
                                                     mach_msg_type_number_t  propsCnt,
                                                     int                     *assertion_id,
                                                     int                     *disableAppSleep,
                                                     int                     *return_code)
{

    bool                create_new = true;
    pid_t               callerPID = -1;
    CFTimeInterval      idleSleepTimerSecs = 0;
    IOReturn            ret;
    CFDataRef           unfolder = NULL;
    CFNumberRef         idleSleepTimerCF = NULL;
    assertion_t *       assertion = NULL;

    CFMutableDictionaryRef      assertionProperties = NULL;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);    

    *disableAppSleep = 0;
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        assertionProperties = (CFMutableDictionaryRef)
                                CFPropertyListCreateWithData(0, unfolder, 
                                                             kCFPropertyListMutableContainersAndLeaves, 
                                                             NULL, NULL);
        CFRelease(unfolder);
    }

    if (!assertionProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    /* Set the assertion timeout value to idle sleep timer value, if it is not 0 */

    idleSleepTimerSecs = gIdleSleepTimer * 60; /* Convert to secs */

    if (idleSleepTimerSecs) {
        idleSleepTimerCF = CFNumberCreate(0, kCFNumberDoubleType, &idleSleepTimerSecs);
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutKey, idleSleepTimerCF);
        CFRelease(idleSleepTimerCF);

        CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutActionKey, kIOPMAssertionTimeoutActionRelease);
    }

    /* Check if this is a repeat call on previously returned assertion id */
    do {
        if (assertion_id == NULL || *assertion_id == kIOPMNullAssertionID) 
            break;

        ret = lookupAssertion(callerPID, *assertion_id, &assertion);
        if ((kIOReturnSuccess != ret) || !assertion) 
            break;

        if (assertion->kassert != kNetworkAccessType)
            break;

        /* Extend the timeout timer of this assertion by idle sleep timer value */

        /* First set the assertion level to ON */
        int k = kIOPMAssertionLevelOn;
        CFNumberRef useLevelOnNum = CFNumberCreate(0, kCFNumberIntType, &k);
        CFDictionarySetValue(assertion->props, kIOPMAssertionLevelKey, useLevelOnNum);
        CFRelease(useLevelOnNum);

        *return_code = doSetProperties(callerPID, *assertion_id, assertionProperties);
        create_new = false;
    } while (false);

    if (create_new) {
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertNetworkClientActive);

        *return_code = doCreate(callerPID, assertionProperties, (IOPMAssertionID *)assertion_id, NULL);
        if ((*return_code == kIOReturnSuccess) && 
            (lookupAssertion(callerPID, *assertion_id, &assertion) == kIOReturnSuccess)) {
            assertion->state |= kAssertionTimeoutIsSystemTimer;
        }

    }

#if !TARGET_OS_EMBEDDED
    if (*return_code == kIOReturnSuccess)
        updateAppSleepStates(processInfoGet(callerPID), disableAppSleep, NULL);
#endif

exit:
    if (assertionProperties)
        CFRelease(assertionProperties);

    vm_deallocate(mach_task_self(), props, propsCnt);
    return KERN_SUCCESS;
}



int do_assertion_notify(pid_t callerPID, string_t name, int req_type)
{

    ProcessInfo         *pinfo = NULL;
    bool                mod = false;
    int                 return_code = kIOReturnSuccess;

    if (req_type == kIOPMNotifyRegister)
    {
        // Create a dispatch handler for process exit, if there isn't one
        if ( !(pinfo = processInfoRetain(callerPID)) ) {
            pinfo = processInfoCreate(callerPID);
            if (!pinfo) {
                return_code = kIOReturnNoMemory;
                goto exit;
            }
        }
    }
    else {
        if ( !(pinfo = processInfoGet(callerPID)) ) {
            return_code = kIOReturnBadArgument;
            goto exit;
        }
    }

    if (!strncmp(name, kIOPMAssertionsAnyChangedNotifyString, sizeof(kIOPMAssertionsAnyChangedNotifyString)))
    {
        if (req_type == kIOPMNotifyRegister && pinfo->anychange == false) {
            pinfo->anychange = true; 
            mod = true;
            gAnyChange++;
        }
        else if (req_type == kIOPMNotifyDeRegister && pinfo->anychange == true) {
            pinfo->anychange = false; 
            gAnyChange--;
            processInfoRelease(callerPID);
        }
    }
    else if (!strncmp(name, kIOPMAssertionsChangedNotifyString, sizeof(kIOPMAssertionsChangedNotifyString))) 
    {
        if (req_type == kIOPMNotifyRegister && pinfo->aggchange == false) {
            pinfo->aggchange = true; 
            mod = true;
            gAggChange++;
        }
        else if (req_type == kIOPMNotifyDeRegister && pinfo->aggchange == true) {
            pinfo->aggchange = false; 
            gAggChange--;
            processInfoRelease(callerPID);
        }
    }
    else if (!strncmp(name, kIOPMAssertionTimedOutNotifyString, sizeof(kIOPMAssertionTimedOutNotifyString)))
    {
        if (req_type == kIOPMNotifyRegister && pinfo->timeoutchange == false) {
            pinfo->timeoutchange = true; 
            mod = true;
            gTimeoutChange++;
        }
        else if (req_type == kIOPMNotifyDeRegister && pinfo->timeoutchange == true) {
            pinfo->timeoutchange = false; 
            gTimeoutChange--;
            processInfoRelease(callerPID);
        }
    }
    else {
        return_code = kIOReturnBadArgument;
    }

    if (!mod && (req_type == kIOPMNotifyRegister)) 
        processInfoRelease(callerPID);

exit:
    return return_code;

}

static void releaseStatsBuf(ProcessInfo *pinfo)
{

    if (pinfo->reportBuf == NULL) return;

    memset(pinfo->stats, 0, sizeof(pinfo->stats));
    free(pinfo->reportBuf);
    pinfo->reportBuf = NULL;
    processInfoRelease(pinfo->pid);

}


static void allocStatsBuf(ProcessInfo *pinfo)
{
    kerAssertionEffect i;

    if (gActivityAggCnt == 0) return;
    if (pinfo->reportBuf) return;

    size_t nbytes = SIMPLEARRAY_BUFSIZE(kMaxEffectStats);
    pinfo->reportBuf = malloc(nbytes);

    if (pinfo->reportBuf) {
        SIMPLEARRAY_INIT(kMaxEffectStats, pinfo->reportBuf, nbytes, getpid(),
                         pinfo->pid, /* Channel ID */
                         kIOReportCategoryPower);
        for (i=0; i < kMaxEffectStats; i++) {
            SIMPLEARRAY_SETVALUE(pinfo->reportBuf, i, 0);
        }
        memset(pinfo->stats, 0, sizeof(pinfo->stats));
        processInfoRetain(pinfo->pid);
    }
}

void setAssertionActivityAggregate(int value)
{
    kerAssertionType i;
    assertionType_t *assertType = NULL;
    CFIndex j, cnt;
    ProcessInfo **procs = NULL;

    if (value) {
        if (gActivityAggCnt++ == 0) {
            cnt = CFDictionaryGetCount(gProcessDict);
            procs = malloc(cnt*(sizeof(procs)));
            if (!procs) return;

            memset(procs, 0, cnt*(sizeof(procs)));
            CFDictionaryGetKeysAndValues(gProcessDict, NULL, (const void **)procs);
            for (j = 0; (j < cnt) && (procs[j] != NULL); j++) {
                allocStatsBuf(procs[j]);
            }
            free(procs);

            for (i=0; i < kIOPMNumAssertionTypes; i++)
            {
                assertType = &gAssertionTypes[i]; 

                if (assertType->effectIdx >= kMaxEffectStats)
                    continue;
                applyToAllAssertionsSync(assertType, false, ^(assertion_t *assertion)
                                         { 
                                             updateAppStats(assertion, kAssertionOpRaise);
                                         });

            }
        }
    }
    else if ((gActivityAggCnt) && (--gActivityAggCnt == 0)) {
        cnt = CFDictionaryGetCount(gProcessDict);
        procs = malloc(cnt*(sizeof(procs)));
        if (!procs) return;

        memset(procs, 0, cnt*(sizeof(procs)));
        CFDictionaryGetKeysAndValues(gProcessDict, NULL, (const void **)procs);
        for (j = 0; (j < cnt) && (procs[j] != NULL); j++) {
            releaseStatsBuf(procs[j]);
        }
        free(procs);

        for (i=0; i < kIOPMNumAssertionTypes; i++)
        {
            assertType = &gAssertionTypes[i]; 

            if (assertType->effectIdx >= kMaxEffectStats)
                continue;
            applyToAllAssertionsSync(assertType, true, ^(assertion_t *assertion)
                                     {
                                     assertion->state &= ~kAssertionStateAddsToProcStats;
                                     });
        }

    }

}


kern_return_t  _io_pm_assertion_notify (   
                                        mach_port_t             server  __unused,
                                        audit_token_t           token,
                                        string_t                name, 
                                        int                     req_type,
                                        int                     *return_code)
{
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;

    *return_code = kIOReturnSuccess;
    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    
    if ( !(callerIsRoot(callerUID) || callerIsAdmin(callerUID, callerGID)))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    if (req_type != kIOPMNotifyRegister && req_type != kIOPMNotifyDeRegister)
    {
        *return_code = kIOReturnNoMemory;
        goto exit;
    }

    *return_code = do_assertion_notify(callerPID, name, req_type);

exit:
    return KERN_SUCCESS;
}

uint8_t getAssertionLevel(kerAssertionType idx)
{
    return ((aggregate_assertions & (1 << idx)) ? 1:0);
}

void setAggregateLevel(kerAssertionType idx, uint8_t val)
{
    if (val)
        aggregate_assertions |= (1 << idx);
    else
        aggregate_assertions &= ~(1<<idx);
}

uint32_t getKerAssertionBits( )
{
    return kerAssertionBits;
}

void insertInactiveAssertion(assertion_t *assertion, assertionType_t *assertType) 
{
    LIST_INSERT_HEAD(&assertType->inactive, assertion, link);
    assertion->state &= ~kAssertionStateTimed;
    assertion->state |= kAssertionStateInactive;
}

void removeInactiveAssertion(assertion_t *assertion, assertionType_t *assertType)
{
    LIST_REMOVE(assertion, link);
    assertion->state &= ~kAssertionStateInactive;
}

void insertActiveAssertion(assertion_t *assertion, assertionType_t *assertType)
{
    LIST_INSERT_HEAD(&assertType->active, assertion, link);
    assertion->state &= ~(kAssertionStateTimed|kAssertionStateInactive);

    if ( (assertType->flags & kAssertionTypeNotValidOnBatt) &&
         (assertion->state & kAssertionStateValidOnBatt) )
        assertType->validOnBattCount++;

    if (assertion->state & kAssertionLidStateModifier)
        assertType->lidSleepCount++;

    updateAppStats(assertion, kAssertionOpRaise);
    schedDisableAppSleep(assertion);
}

void removeActiveAssertion(assertion_t *assertion, assertionType_t *assertType)
{
    LIST_REMOVE(assertion, link);

    if ( (assertion->state & kAssertionStateValidOnBatt) && assertType->validOnBattCount)
        assertType->validOnBattCount--;

    if ( (assertion->state & kAssertionLidStateModifier) && assertType->lidSleepCount)
        assertType->lidSleepCount--;

    updateAppStats(assertion, kAssertionOpRelease);
    schedEnableAppSleep(assertion);
}


void resetAssertionTimer(assertionType_t *assertType)
{
    uint64_t currTime ;
    assertion_t *nextAssertion = NULL;

    nextAssertion = LIST_FIRST(&assertType->activeTimed);
    if (!nextAssertion) return;

    currTime = getMonotonicTime();

    if (nextAssertion->timeout <= currTime) {
        CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{ handleAssertionTimeout(assertType); });
        CFRunLoopWakeUp(_getPMRunLoop());
    }
    else {
        dispatch_suspend(assertType->timer);

        dispatch_source_set_timer(assertType->timer, 
                                  dispatch_time(DISPATCH_TIME_NOW, (nextAssertion->timeout-currTime)*NSEC_PER_SEC), 
                                  DISPATCH_TIME_FOREVER, 0);
        dispatch_resume(assertType->timer);
    }

}


static void releaseAssertionMemory(assertion_t *assertion, assertLogAction logAction)
{
    int idx = INDEX_FROM_ID(assertion->assertionId);
    assertion_t *tmp_a = NULL;

    if ( (idx < 0) || (idx >= kMaxAssertions) || 
         (CFDictionaryGetValueIfPresent(gAssertionsArray, 
                                        (uintptr_t)idx, (const void **)&tmp_a) == false) || (tmp_a != assertion) ) {
#ifdef DEBUG
        abort();
#endif
        return; // This is an error
    }

    assertion->retainCnt = 0;
    logAssertionEvent(logAction, assertion);
    CFDictionaryRemoveValue(gAssertionsArray, (uintptr_t)idx);
    if (assertion->props) CFRelease(assertion->props);


    processInfoRelease(assertion->pinfo->pid);
    free(assertion);
}

void handleAssertionTimeout(assertionType_t *assertType)
{
    assertion_t     *assertion;
    CFDateRef       dateNow = NULL;
    uint64_t        currtime = getMonotonicTime( );
    uint32_t        timedoutCnt = 0;
    CFStringRef     timeoutAction = NULL;
    bool            displayProxy = false;

    while( (assertion = LIST_FIRST(&assertType->activeTimed)) )
    {
        if (assertion->timeout > currtime) {
            assertion = NULL;
            break;
        }
        timedoutCnt++;

        LIST_REMOVE(assertion, link);
        assertion->state &= ~kAssertionStateTimed;

        if ( (assertion->state & kAssertionStateValidOnBatt) && assertType->validOnBattCount)
            assertType->validOnBattCount--;

        if ( (assertion->state & kAssertionLidStateModifier) && assertType->lidSleepCount)
            assertType->lidSleepCount--;

        updateAppStats(assertion, kAssertionOpRelease);
        schedEnableAppSleep( assertion );

        if ((dateNow = CFDateCreate(0, CFAbsoluteTimeGetCurrent()))) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionTimedOutDateKey, dateNow);            
            CFRelease(dateNow);
        }


        if ( (assertion->kassert == kPreventDisplaySleepType) && 
             (assertion->pinfo->pid != getpid()))
            displayProxy = true;

        timeoutAction = CFDictionaryGetValue(assertion->props, kIOPMAssertionTimeoutActionKey);
        if (isA_CFString(timeoutAction) && CFEqual(kIOPMAssertionTimeoutActionRelease, timeoutAction))
        { 
            releaseAssertionMemory(assertion, kATimeoutLog);
        }
        else /* Default timeout action is to turn off */
        {
            logAssertionEvent(kATimeoutLog, assertion);

            // Leave this in the inactive assertions list
            insertInactiveAssertion(assertion, assertType);
            if (isA_CFString(timeoutAction) && CFEqual(kIOPMAssertionTimeoutActionKillProcess, timeoutAction))
            {
                kill(assertion->pinfo->pid, SIGTERM);
            }

        }

    }

    if ( !timedoutCnt ) return;

    resetAssertionTimer(assertType);

    if (displayProxy) delayDisplayTurnOff( );

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);

    logASLAssertionsAggregate();
    if (gTimeoutChange) notify_post( kIOPMAssertionTimedOutNotifyString );
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );

}

void removeTimedAssertion(assertion_t *assertion, assertionType_t *assertType, bool updateTimer)
{
    bool isTheFirstOne = false;

    CFDictionaryRemoveValue(assertion->props, kIOPMAssertionTimeoutTimeLeftKey);
    if (LIST_FIRST(&assertType->activeTimed) == assertion) {
        isTheFirstOne = true;
    }
    LIST_REMOVE(assertion, link);
    assertion->state &= ~kAssertionStateTimed;

    if ( (assertion->state & kAssertionStateValidOnBatt) && assertType->validOnBattCount)
        assertType->validOnBattCount--;

    if ( (assertion->state & kAssertionLidStateModifier) && assertType->lidSleepCount)
        assertType->lidSleepCount--;

    updateAppStats(assertion, kAssertionOpRelease);
    schedEnableAppSleep(assertion);

    if (isTheFirstOne && updateTimer) resetAssertionTimer(assertType);

}

void updateAssertionTimer(assertionType_t *assertType)
{
    uint64_t    currTime;
    assertion_t *assertion = NULL;

    if ((assertion = LIST_FIRST(&assertType->activeTimed)) == NULL) return;

    /* Update/create the dispatch timer.  */
    if (assertType->timer == NULL) {
        assertType->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());

        dispatch_source_set_event_handler(assertType->timer, ^{
                                          handleAssertionTimeout(assertType);
                                          });

        dispatch_source_set_cancel_handler(assertType->timer, ^{
                                           dispatch_release(assertType->timer);
                                           });

    }
    else {
        dispatch_suspend(assertType->timer);
    }
    currTime = getMonotonicTime();


    if (assertion->timeout <= currTime) {
        /* This has already timed out. */
        dispatch_resume(assertType->timer);
        CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{ handleAssertionTimeout(assertType); });
        CFRunLoopWakeUp(_getPMRunLoop());
    }
    else {
        dispatch_source_set_timer(assertType->timer, 
                                  dispatch_time(DISPATCH_TIME_NOW, (assertion->timeout-currTime)*NSEC_PER_SEC), 
                                  DISPATCH_TIME_FOREVER, 0);
        dispatch_resume(assertType->timer);
    }


}

/* Inserts assertion into activeTimed list, sorted by timeout */
static void insertByTimeout(assertion_t *assertion, assertionType_t *assertType)
{
    assertion_t *a, *prev;
    CFNumberRef         timeLeftCF = NULL;
    uint64_t            currTime, timeLeft;
    CFDateRef           updateDate = NULL;

    currTime = getMonotonicTime();
    if (assertion->timeout > currTime) {
        /* Update timeout time left property */
        timeLeft = (assertion->timeout-currTime);
        int timeLeft32 = (int)timeLeft;
        timeLeftCF = CFNumberCreate(0, kCFNumberIntType, &timeLeft32);

        if (timeLeftCF) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionTimeoutTimeLeftKey, timeLeftCF);
            CFRelease(timeLeftCF);
        }

        updateDate = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
        if (updateDate) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionTimeoutUpdateTimeKey, updateDate);
            CFRelease(updateDate);
        }
    }

    if (LIST_EMPTY(&assertType->activeTimed) ) {
        LIST_INSERT_HEAD(&assertType->activeTimed, assertion, link);
    }
    else {
        LIST_FOREACH(a, &assertType->activeTimed, link) 
        {
            prev = a;
            if (a->timeout > assertion->timeout)
                break;
        }
        if (a)
            LIST_INSERT_BEFORE(a, assertion, link);
        else
            LIST_INSERT_AFTER(prev, assertion, link);
    }

}

void insertTimedAssertion(assertion_t *assertion, assertionType_t *assertType, bool updateTimer)
{
    insertByTimeout(assertion, assertType);

    assertion->state |= kAssertionStateTimed;
    if ( (assertType->flags & kAssertionTypeNotValidOnBatt) &&
         (assertion->state & kAssertionStateValidOnBatt) )
        assertType->validOnBattCount++;

    if (assertion->state & kAssertionLidStateModifier)
        assertType->lidSleepCount++;

    updateAppStats(assertion, kAssertionOpRaise);
    schedDisableAppSleep( assertion );
    /*  
     * If this assertion is not the one with earliest timeout,
     * there is nothing to do.
     */
    if (LIST_FIRST(&assertType->activeTimed) != assertion)
        return;

    if (updateTimer) updateAssertionTimer(assertType);

    return;
}


static void releaseAssertion(assertion_t *assertion, bool callHandler)
{
    assertionType_t     *assertType;

    assertType = &gAssertionTypes[assertion->kassert]; 

    if ( (assertion->kassert == kPreventDisplaySleepType) && 
         (assertion->pinfo->pid != getpid()))
        delayDisplayTurnOff( );
    if (assertion->state & kAssertionStateTimed)
        removeTimedAssertion(assertion, assertType, true);
    else if (assertion->state & kAssertionStateInactive)
        removeInactiveAssertion(assertion, assertType);
    else
        removeActiveAssertion(assertion, assertType);

    if (!callHandler) return;

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);


}

static IOReturn doRelease(pid_t pid, IOPMAssertionID id)
{
    IOReturn                    ret;

    assertion_t    *assertion = NULL;
    ret = lookupAssertion(pid, id, &assertion);
    if ((kIOReturnSuccess != ret)) {
        return ret;
    }

    if (assertion->retainCnt)
        assertion->retainCnt--;

    if (assertion->retainCnt) {
        return kIOReturnSuccess;
    }

    releaseAssertion(assertion, true);
    releaseAssertionMemory(assertion, kAReleaseLog);

    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );

    return kIOReturnSuccess;
}


__private_extern__ void applyToAllAssertionsSync(assertionType_t *assertType, 
                                                 bool applyToInactives,  void (^performOnAssertion)(assertion_t *))
{
    assertion_t *assertion, *nextAssertion;

    assertion = LIST_FIRST(&assertType->active);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        performOnAssertion(assertion);
        assertion = nextAssertion;
    }

    assertion = LIST_FIRST(&assertType->activeTimed);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        performOnAssertion(assertion);
        assertion = nextAssertion;
    }

    if ( !applyToInactives) return; 

    assertion = LIST_FIRST(&assertType->inactive);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        performOnAssertion(assertion);
        assertion = nextAssertion;
    }

}

__private_extern__ void HandleProcessExit(pid_t deadPID)
{
    int i;
    assertionType_t *assertType = NULL;
    assertion_t     *assertion = NULL;
    __block LIST_HEAD(, assertion) list  = LIST_HEAD_INITIALIZER(list);     /* list of assertions released */

    do_assertion_notify(deadPID, kIOPMAssertionsAnyChangedNotifyString, kIOPMNotifyDeRegister);
    do_assertion_notify(deadPID, kIOPMAssertionTimedOutNotifyString, kIOPMNotifyDeRegister);
    do_assertion_notify(deadPID, kIOPMAssertionsChangedNotifyString, kIOPMNotifyDeRegister);

    /* Go thru each assertion type and release all assertion from its lists */
    for (i=0; i < kIOPMNumAssertionTypes; i++)
    {
        assertType = &gAssertionTypes[i]; 

        applyToAllAssertionsSync(assertType, true, ^(assertion_t *assertion)
                                 {
                                 if (assertion->pinfo->pid == deadPID) {
                                     releaseAssertion(assertion, false);
                                     LIST_INSERT_HEAD(&list, assertion, link);
                                 }
                                 });

        if (assertType->handler)
            (*assertType->handler)(assertType, kAssertionOpRelease);

        /* Release memory after calling the handler to get proper aggregate_assertions value into log */
        while( (assertion = LIST_FIRST(&list)) )
        {
            LIST_REMOVE(assertion, link);
            releaseAssertionMemory(assertion, kAClientDeathLog);
        }

    }
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );


}


static int getAssertionTypeIndex(CFStringRef type)
{
    int idx = -1;
    CFNumberRef numRef = NULL;

    if (!isA_CFString(type))
        return -1;

    numRef = CFDictionaryGetValue(gUserAssertionTypesDict, type);
    if (isA_CFNumber(numRef))
        CFNumberGetValue(numRef, kCFNumberIntType, &idx);

    if (idx < 0 || idx >= kIOPMNumAssertionTypes)
        return -1;

    return idx;
}
static void forwardPropertiesToAssertion(const void *key, const void *value, void *context)
{
    assertion_t *assertion = (assertion_t *)context;
    assertionType_t *assertType = NULL;
    CFTimeInterval      timeout = 0;
    int level;

    if (!isA_CFString(key))
        return; /* Key has to be a string */


    assertType = &gAssertionTypes[assertion->kassert];
    if (CFEqual(key, kIOPMAssertionLevelKey)) {
        if (!isA_CFNumber(value)) return;
        CFNumberGetValue(value, kCFNumberIntType, &level);
        if ( (assertion->state & kAssertionStateInactive) && (level == kIOPMAssertionLevelOn) )
        {
            assertion->state &= ~kAssertionStateInactive;
            assertion->mods |= kAssertionModLevel;
        }
        else if ( !(assertion->state & kAssertionStateInactive) && (level == kIOPMAssertionLevelOff) )
        {
            assertion->state |= kAssertionStateInactive;
            assertion->mods |= kAssertionModLevel;
        }
    }
    else if (CFEqual(key, kIOPMAssertionTimeoutKey)) {
        if (!isA_CFNumber(value)) return;
        CFNumberGetValue(value, kCFNumberDoubleType, &timeout);

        if (assertType->flags & kAssertionTypeAutoTimed) {
            /* Restrict timeout to a max value of 'autoTimeout' */
            if (!timeout || (timeout > assertType->autoTimeout))
                timeout = assertType->autoTimeout;
        }

        if (timeout) {
            assertion->timeout = (uint64_t)timeout + getMonotonicTime(); // Absolute time at which assertion expires
        }
        else  {
            assertion->timeout = 0;
        }

        /* Setting a timeout makes an inactive assertion active again */
        if (assertion->state & kAssertionStateInactive) {
            assertion->state &= ~kAssertionStateInactive;
            assertion->mods |= kAssertionModLevel;
        }
        else {
            assertion->mods |= kAssertionModTimer;
        }
    }
    else if (CFEqual(key, kIOPMAssertionAppliesToLimitedPowerKey)) {
        if (!isA_CFBoolean(value)) return;
        if ((assertType->flags & kAssertionTypeNotValidOnBatt) == 0) return;
        if ((value == kCFBooleanTrue) && !(assertion->state & kAssertionStateValidOnBatt))
        {
            assertType->validOnBattCount++;
            assertion->state |= kAssertionStateValidOnBatt;
            assertion->mods |= kAssertionModPowerConstraint;
        }
        else if ((value == kCFBooleanFalse) && (assertion->state & kAssertionStateValidOnBatt) )
        {
            if (assertType->validOnBattCount) assertType->validOnBattCount--;
            assertion->state &= ~kAssertionStateValidOnBatt;
            assertion->mods |= kAssertionModPowerConstraint;
        }

    }
    else if (CFEqual(key, kIOPMAssertionAppliesOnLidClose)) {
        if (!isA_CFBoolean(value)) return;
        if ((value == kCFBooleanTrue) && !(assertion->state & kAssertionLidStateModifier)) {
            assertType->lidSleepCount++;
            assertion->state |= kAssertionLidStateModifier;
            assertion->mods |= kAssertionModLidState;
        }
        else if((value == kCFBooleanFalse) && (assertion->state & kAssertionLidStateModifier)) {
            if (assertType->lidSleepCount) assertType->lidSleepCount--;
            assertion->state &= ~kAssertionLidStateModifier;
            assertion->mods |= kAssertionModLidState;
        }
    }
    else if (CFEqual(key, kIOPMAssertionTypeKey)) {
        /* Assertion type can't be modified */
        return;
    }

    CFDictionarySetValue(assertion->props, key, value);


}

static IOReturn doSetProperties(pid_t pid, 
                                IOPMAssertionID id, 
                                CFDictionaryRef inProps)
{
    assertion_t                 *assertion = NULL;
    assertionType_t             *assertType;
    uint32_t                    oldState;

    IOReturn                    ret;

    // doSetProperties doesn't handle retain()/release() count. 
    // Callers should use IOPMAssertionRetain() or IOPMAssertionRelease().

    ret = lookupAssertion(pid, id, &assertion);

    if ((kIOReturnSuccess != ret)) {
        return ret;
    }

    assertion->mods = 0;
    assertType = &gAssertionTypes[assertion->kassert];
    oldState = assertion->state;
    CFDictionaryApplyFunction(inProps, forwardPropertiesToAssertion,
                              assertion);



    if (assertion->mods & kAssertionModLevel) 
    {
        if (assertion->state & kAssertionStateInactive) 
        {
            /* Remove from active or activeTimed list */
            if (oldState & kAssertionStateTimed) 
                removeTimedAssertion(assertion, assertType, true);
            else 
                removeActiveAssertion(assertion, assertType);

            /* Add to inActive list */
            insertInactiveAssertion(assertion, assertType);

            if ( (assertion->kassert == kPreventDisplaySleepType) && 
                 (assertion->pinfo->pid != getpid()))
                delayDisplayTurnOff( );
            if (assertType->handler)
                (*assertType->handler)(assertType, kAssertionOpRelease);

            logAssertionEvent(kATurnOffLog, assertion);
        }
        else 
        {
            /* An inactive assertion is made active now */
            removeInactiveAssertion(assertion, assertType);
            CFDictionaryRemoveValue(assertion->props, kIOPMAssertionTimedOutDateKey);            
            raiseAssertion(assertion);
            logAssertionEvent(kATurnOnLog, assertion);
        }
        if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );
        return kIOReturnSuccess;
    }

    if (assertion->state & kAssertionStateInactive)
        return kIOReturnSuccess;    

    if  (assertion->mods & kAssertionModTimer) 
    {
        /* Remove from active or activeTimed list */
        if (oldState & kAssertionStateTimed)
            removeTimedAssertion(assertion, assertType, true);
        else 
            removeActiveAssertion(assertion, assertType);


        assertion->createTime = getMonotonicTime();

        if (assertion->timeout != 0) {
            insertTimedAssertion(assertion, assertType, true);
        }
        else {
            insertActiveAssertion(assertion, assertType);
        }

        if (assertion->kassert == kDeclareUserActivityType)
            (*assertType->handler)(assertType, kAssertionOpRaise);

        if (assertType->handler) 
            (*assertType->handler)(assertType, kAssertionOpEval);
    }


    if ( (assertion->mods & kAssertionModPowerConstraint) &&
         (assertType->handler) )
    {
        if (assertion->state & kAssertionStateValidOnBatt) {
            (*assertType->handler)(assertType, kAssertionOpRaise);
            updateAppStats(assertion, kAssertionOpRaise);
        }
        else {
            (*assertType->handler)(assertType, kAssertionOpRelease);
            updateAppStats(assertion, kAssertionOpRelease);
        }

    }

    if ( (assertion->mods & kAssertionModLidState) &&
         (assertType->handler) )
    {
        if (assertion->state & kAssertionLidStateModifier)
            (*assertType->handler)(assertType, kAssertionOpRaise);
        else
            (*assertType->handler)(assertType, kAssertionOpRelease);

    }

    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );
    return kIOReturnSuccess;    
}

/*
 * Check for active assertions of the specified type and also for active
 * assertions of linked types.
 * Returns true if active assertions exist either in the specified type or
 * in the linked  types to the specified type.
 */
bool checkForActives(assertionType_t *assertType, bool *existsInThisType )
{
    bool                typeActive = false;
    bool                effectActive = false;
    assertionType_t     *type = NULL;
    assertionEffect_t   *effect = NULL;

    if (existsInThisType) 
        *existsInThisType = false;
    if (assertType->effectIdx == kNoEffect)
        return false;

    effect = &gAssertionEffects[assertType->effectIdx];

    LIST_FOREACH(type, &effect->assertTypes, link) {

        /* 
         * Check for active assertions in this assertionType's 'active' & 'activeTimed' lists 
         */
        if ( (type->flags & kAssertionTypeNotValidOnBatt) && ( _getPowerSource() == kBatteryPowered) )
            typeActive = (type->validOnBattCount > 0);
        else
            typeActive = (LIST_FIRST(&type->active) || LIST_FIRST(&type->activeTimed));

        effectActive |= typeActive; 
        if (existsInThisType && (type == assertType) && typeActive) { 
            *existsInThisType = true;
            return true;
        }
        if (typeActive && !existsInThisType) 
            return true;

    }

    return effectActive;
}

/*
 * Checks if there are assertions preventing system going from S0dark to S3.
 * Returns true if S3 sleep is prevented.
 */
__private_extern__ bool systemBlockedInS0Dark( )
{

    /* PreventSystemSleep assertion and its linked types are the only ones
     * that can keep the system in S0dark.
     */
    return checkForActives(&gAssertionTypes[kPreventSleepType], NULL);
}

/*
 * Check for active assertions of the specified type.
 * Returns true if specified type assertions are active
 */
__private_extern__ bool checkForActivesByType(kerAssertionType type)
{
    bool activesForTheType = false;

    checkForActives(&gAssertionTypes[type], &activesForTheType);
    return activesForTheType;
}

/*
 * Check for assertions of the specified type, even if assertion type is disabled.
 * Returns true if there any assertions of specified type raised
 */
__private_extern__ bool checkForEntriesByType(kerAssertionType type)
{
    assertionType_t *assertType = &gAssertionTypes[type];

    if (LIST_FIRST(&assertType->active) || LIST_FIRST(&assertType->activeTimed))
        return true;

    return false;
}

/* Disable the specified assertion type */
__private_extern__ void disableAssertionType(kerAssertionType type)
{
    gAssertionTypes[type].disableCnt++;
    configAssertionType(type, false);

}

/* Enablee the specified assertion type */
__private_extern__ void enableAssertionType(kerAssertionType type)
{
    if (gAssertionTypes[type].disableCnt)
        gAssertionTypes[type].disableCnt--;

    configAssertionType(type, false);

}

__private_extern__ kern_return_t setReservePwrMode(int enable)
{
#if TARGET_OS_EMBEDDED
    static bool inReservePwrMode = false;

    if (enable) {
        if (inReservePwrMode) return kIOReturnExclusiveAccess;

        disableAssertionType(kPreventIdleType);
        disableAssertionType(kDeclareSystemActivityType);
        disableAssertionType(kBackgroundTaskType);
        disableAssertionType(kPushServiceTaskType);
        inReservePwrMode = true;
    }
    else {
        if (!inReservePwrMode) return kIOReturnExclusiveAccess;

        enableAssertionType(kPreventIdleType);
        enableAssertionType(kDeclareSystemActivityType);
        enableAssertionType(kBackgroundTaskType);
        enableAssertionType(kPushServiceTaskType);
        inReservePwrMode = false;
    }
    return kIOReturnSuccess;
#else
    return kIOReturnError;
#endif
}

static inline void updateAggregates(assertionType_t *assertType, bool activesForTheType)
{
    if (activesForTheType) {
        setAggregateLevel(assertType->kassert,  1 );
    }
    else  {
        if (getAssertionLevel(assertType->kassert) && assertType->globalTimeout) {
            resetGlobalTimer(assertType, 0);
        }
        setAggregateLevel(assertType->kassert,  0 );
    }

    userActiveHandlePowerAssertionsChanged();
}

static void modifySettings(assertionType_t *assertType, assertionOps op)
{
    bool    activeExists;
    bool    activesForTheType = false;
    int     opVal;
    bool    assertionLevel = 0;

    if (assertType->effectIdx == kNoEffect)
        return;

    assertionLevel = getAssertionLevel(assertType->kassert);

    activeExists = checkForActives(assertType, &activesForTheType);
    updateAggregates(assertType, activesForTheType);


    if (op == kAssertionOpRaise) {
        /* 
         * if already raised with kernel or if there are no active ones,
         * nothing to do
         */
        if ( assertionLevel || !activeExists )
            return;
        opVal = 1;
    }
    else if (op == kAssertionOpRelease) {
        /*
         * If this assertionType is not raised with kernel
         * or if there are active ones, nothing to do
         */
        if ( !assertionLevel || activeExists)
            return;
        opVal = 0;

    }
    else { // op == kAssertionOpEval
        if (activeExists) {
            if (assertionLevel)
                return;
            opVal = 1;
        }
        else {
            if (!assertionLevel)
                return;
            opVal = 0;
        }
    }


    switch(assertType->kassert) {
    case kHighPerfType:
        overrideSetting( kPMForceHighSpeed, opVal);
        break;

    case kDeclareSystemActivityType:
        // Behaves identical to a PreventIdleSleep
        // assertion, except it also backs out of
        // idle if possible.
    case kBackgroundTaskType:
    case kPreventIdleType:
    case kNetworkAccessType:
    case kInteractivePushServiceType:
    case kReservePwrPreventIdleType:
        overrideSetting(kPMPreventIdleSleep, opVal);
        break;

    case kPreventDiskSleepType:
        overrideSetting(kPMPreventDiskSleep, opVal);
        break;

    default:
        return;
    }

    activateSettingOverrides();
    if (gAggChange) notify_post( kIOPMAssertionsChangedNotifyString );
    return;
}

void handleBatteryAssertions(assertionType_t *assertType, assertionOps op)
{
    bool    activeExists;

    if (op == kAssertionOpEval)
        return; // Nothing to evaluate

    activeExists = checkForActives(assertType, NULL);

    if (op == kAssertionOpRaise) {
        /* 
         * if already raised with kernel or if there are no active ones,
         * nothing to do
         */
        if ( getAssertionLevel(assertType->kassert) || !activeExists )
            return;
        setAggregateLevel(assertType->kassert, 1);
    }
    else {
        /*
         * If this assertionType is not raised with kernel
         * or if there are active ones, nothing to do
         */
        if ( !getAssertionLevel(assertType->kassert) || activeExists)
            return;

        setAggregateLevel(assertType->kassert, 0);
    }

    switch(assertType->kassert) {
    case kDisableInflowType:
        sendSmartBatteryCommand( kSBUCInflowDisable, 
                                 op == kAssertionOpRaise ? 1 : 0);
        break;

    case kInhibitChargeType:
        sendSmartBatteryCommand( kSBUCChargeInhibit, 
                                 op == kAssertionOpRaise ? 1 : 0);
        break;

    case kDisableWarningsType:
        _setRootDomainProperty( CFSTR("BatteryWarningsDisabled"), kCFBooleanTrue);
        break;

    default:
        break;
    }

    if (gAggChange) notify_post( kIOPMAssertionsChangedNotifyString );
    return;
}

void setKernelAssertions(assertionType_t *assertType, assertionOps op)
{
    uint32_t    assertBit = 0;
    bool    activeExists, activesForTheType;

    if (assertType->effectIdx == kNoEffect)
        return;

    /* 
     * active assertions exists if one of the active list is not empty and
     * the assertion is not globally disabled.
     */
    activeExists = checkForActives(assertType, &activesForTheType);

    switch(assertType->kassert) {

    case kPushServiceTaskType:
#if LOG_SLEEPSERVICES
        if ( !activesForTheType && getAssertionLevel(assertType->kassert) )
            logASLMessageSleepServiceTerminated(assertType->forceTimedoutCnt);
#endif
        assertBit = kIOPMDriverAssertionCPUBit;
        break;

    case kInteractivePushServiceType:
    case kPreventSleepType:
    case kBackgroundTaskType:
    case kSRPreventSleepType:
    case kNetworkAccessType:
        assertBit = kIOPMDriverAssertionCPUBit;
        break;

    case kDeclareUserActivityType:
    case kPreventDisplaySleepType:
    case kIntPreventDisplaySleepType:
        assertBit = kIOPMDriverAssertionPreventDisplaySleepBit;
        break;

    case kExternalMediaType:
        assertBit = kIOPMDriverAssertionExternalMediaMountedBit;
        break;

    default:
        return;
    }

    updateAggregates(assertType, activesForTheType);

    if (op == kAssertionOpRaise)  {

        if ((assertType->kassert == kDeclareUserActivityType) && activesForTheType) {
            if (assertType->lidSleepCount) setClamshellSleepState(1);
            sendActivityTickle();
            _unclamp_silent_running(true);
        }

        /*
         * If server-mode power assertion is being raised anew, then we need
         * to unclamp SilentRunning through AppleSMC
         */
#if !TARGET_OS_EMBEDDED
        if ( ((assertType->kassert == kPreventSleepType) || (assertType->kassert == kNetworkAccessType))
             && activesForTheType)
            _unclamp_silent_running(true);
#endif
        /*
         * if already raised with kernel or if there are no active ones,
         * nothing to do
         */
        if ( (kerAssertionBits & assertBit) || !activeExists )
            return;

    }
    else if (op == kAssertionOpRelease)  {
        if ((assertType->kassert == kDeclareUserActivityType) && (assertType->lidSleepCount == 0)) 
            setClamshellSleepState(0);

        /*
         * If this assertionType is not raised with kernel
         * or if there are active ones, nothing to do
         */
        if ( !(kerAssertionBits & assertBit) || activeExists)
            return;

    }
    else { // op == kAssertionOpEval

#if !TARGET_OS_EMBEDDED
        if ( (assertType->kassert == kPreventSleepType) && activesForTheType)
            _unclamp_silent_running(true);

#endif
        if (activeExists && (kerAssertionBits & assertBit))
            return;
        else if ( !activeExists && !(kerAssertionBits & assertBit) )
            return;
    }




    if (activeExists) {
        kerAssertionBits |= assertBit;
        sendUserAssertionsToKernel(kerAssertionBits);
    }
    else {
        kerAssertionBits &= ~assertBit;
        sendUserAssertionsToKernel(kerAssertionBits);
    }
    if (gAggChange) notify_post( kIOPMAssertionsChangedNotifyString );
}

static void enableIdleHandler(assertionType_t *assertType, assertionOps op)
{
    static IOPMAssertionID    enableIdleId = kIOPMNullAssertionID;
    CFMutableDictionaryRef    assertionDescription = NULL;
    bool                      activeExists;

#if !TARGET_OS_EMBEDDED
    return; // Supported for embedded only
#endif

    if (op == kAssertionOpEval)
        return; // Nothing to evaluate

    if (assertType->kassert != kEnableIdleType)
        return;

    activeExists = checkForActives(assertType, NULL);

    if (op == kAssertionOpRaise) {
        /* Check if this assertType is already raised */
        if (  !activeExists )
            return;
        setAggregateLevel(assertType->kassert, 1);

        InternalReleaseAssertion(&enableIdleId);
    }
    else {

        /* If not the last one or if this assertType is not raised, nothing to do */
        if ( activeExists )
            return;
        setAggregateLevel(assertType->kassert, 0);

        assertionDescription = _IOPMAssertionDescriptionCreate(
                                                               kIOPMAssertionTypePreventUserIdleSystemSleep,
                                                               CFSTR("com.apple.powermanagement.enableIdle"),
                                                               NULL, CFSTR("Waiting for Idle Sleep to enabled"), NULL, 0, 0);

        InternalCreateAssertion(assertionDescription, &enableIdleId);

        CFRelease(assertionDescription);



    }

    if (gAggChange) notify_post( kIOPMAssertionsChangedNotifyString );
}

#if TCPKEEPALIVE
static void displayWakeHandler(assertionType_t *assertType, assertionOps op)
{
    bool            activesForTheType = false;
    bool            activeExists;
    uint64_t        level = 0;
    bool            assertionLevel = 0;
    io_connect_t    connect = IO_OBJECT_NULL;

    if (assertType->kassert != kTicklessDisplayWakeType)
        return;

    assertionLevel = getAssertionLevel(assertType->kassert);
    activeExists = checkForActives(assertType, &activesForTheType);

    if (op == kAssertionOpRaise) {
        if (assertType->lidSleepCount) setClamshellSleepState(1);
        if ( !activeExists ) return;
        level = 1;
    }
    else if (op == kAssertionOpRelease) {
        if (assertType->lidSleepCount == 0) setClamshellSleepState(0);
        if ( activeExists ) return;
        level = 0;
    }
    else { // op == kAssertionOpEval
        if (activeExists) {
            if (assertionLevel)
                return;
            level = 1;
        }
        else {
            if (!assertionLevel)
                return;
            level = 0;
        }
    }

    if ( (connect = getRootDomainConnect()) == IO_OBJECT_NULL)
        return;

    if (level) { 
        if ((isA_DarkWakeState() || isA_SleepState())) {
            set_NotificationDisplayWake( );
        }
    }
    else {
        cancel_NotificationDisplayWake( );
    }

    // updateAggregates after set_NotificationDisplayWake() to
    // avoid 'kIOPMUserPresentPassive' level when display is waking
    // to display notification
    updateAggregates(assertType, activesForTheType);
    IOConnectCallMethod(connect, kPMSetDisplayPowerOn, 
                        &level, 1, 
                        NULL, 0, NULL, 
                        NULL, NULL, NULL);

    if (level && (gDarkWakeNetworkAssertion != kIOPMNullAssertionID)) {
        // Release n/w proxy, if it exists.
        // XXX: This is not a right place to release this, but leaving it here
        // for now, as this is the only case of releasing an assertion when another 
        // is created
        dispatch_async(dispatch_get_main_queue(), ^{
                       doRelease(getpid(), gDarkWakeNetworkAssertion);
                       gDarkWakeNetworkAssertion = kIOPMNullAssertionID;
                       });
    }
    if (gAggChange) notify_post( kIOPMAssertionsChangedNotifyString );
}
#endif

void enforceAssertionTypeTimeCap(assertionType_t *assertType)
{
    assertion_t *assertion = NULL;

    if ((assertType->flags & kAssertionTypeGloballyTimed) == 0)
        return;
    assertType->forceTimedoutCnt = 0;
    /* Move all active assertions to inactive mode */
    while( (assertion = LIST_FIRST(&assertType->active)) )
    {
        removeActiveAssertion(assertion, assertType);
        insertInactiveAssertion(assertion, assertType);
        assertType->forceTimedoutCnt++;
        logAssertionEvent(kACapExpiryLog, assertion);
        mt2RecordAssertionEvent(kAssertionOpGlobalTimeout, assertion);
    }

    /* Timeout all timed assertions */
    while( (assertion = LIST_FIRST(&assertType->activeTimed)) )
    {
        LIST_REMOVE(assertion, link);
        assertion->state &= ~kAssertionStateTimed;

        updateAppStats(assertion, kAssertionOpRelease);
        schedEnableAppSleep( assertion );

        insertInactiveAssertion(assertion, assertType);
        assertType->forceTimedoutCnt++;
        logAssertionEvent(kACapExpiryLog, assertion);
        mt2RecordAssertionEvent(kAssertionOpGlobalTimeout, assertion);
    }

    resetAssertionTimer(assertType);

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);

    if (assertType->forceTimedoutCnt == 0)
        logASLMessageSleepServiceTerminated(0);

    logASLAssertionsAggregate();
}

void resetGlobalTimer(assertionType_t *assertType, uint64_t timeout)
{

    if ((assertType->flags & kAssertionTypeGloballyTimed) == 0)
        return;
    assertType->globalTimeout = timeout;
    if (assertType->globalTimeout == 0) {
        if ( assertType->globalTimer)
            dispatch_source_cancel(assertType->globalTimer);
        return;
    }

    if (assertType->globalTimer == NULL) {
        assertType->globalTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());

        dispatch_source_set_event_handler(assertType->globalTimer, ^{
                                          enforceAssertionTypeTimeCap(assertType);
                                          });

        dispatch_source_set_cancel_handler(assertType->globalTimer, ^{
                                           dispatch_release(assertType->globalTimer);
                                           assertType->globalTimer = NULL;
                                           });

    }
    else {
        dispatch_suspend(assertType->globalTimer);
    }

    dispatch_source_set_timer(assertType->globalTimer, 
                              dispatch_time(DISPATCH_TIME_NOW, assertType->globalTimeout * NSEC_PER_SEC), 
                              DISPATCH_TIME_FOREVER, 0);
    dispatch_resume(assertType->globalTimer);

}


static IOReturn raiseAssertion(assertion_t *assertion)
{
    int                 idx = -1;
    int                 level;
    uint64_t            currTime = getMonotonicTime();
    uint32_t            levelInt = 0;
    CFDateRef           start_date = NULL;
    CFStringRef         assertionTypeRef;
    CFNumberRef         numRef = NULL;
    CFNumberRef         levelNum = NULL;
    CFTimeInterval      timeout = 0;
    assertionType_t     *assertType;
    uint64_t            assertion_id_64;
    CFBooleanRef        val = NULL;


    assertionTypeRef = CFDictionaryGetValue(assertion->props, kIOPMAssertionTypeKey);

    /* Find index for this assertion type */
    idx = getAssertionTypeIndex(assertionTypeRef);
    if (idx < 0 )
        return kIOReturnBadArgument;
    assertType = &gAssertionTypes[idx];
    assertion->kassert = idx;

    assertion_id_64 = MAKE_UNIQAID(currTime, idx, assertion->assertionId);
    CFNumberRef uniqueAID = CFNumberCreate(0, kCFNumberSInt64Type, &assertion_id_64);
    if (uniqueAID) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionGlobalUniqueIDKey, uniqueAID);
        CFRelease(uniqueAID);
    }

    /* Attach the Create Time */
    start_date = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
    if (start_date) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionCreateDateKey, start_date);
        CFRelease(start_date);
    }
    assertion->createTime = currTime;


    /* Is level set to 0 */
    numRef = CFDictionaryGetValue(assertion->props, kIOPMAssertionLevelKey);
    if (isA_CFNumber(numRef)) {
        CFNumberGetValue(numRef, kCFNumberIntType, &level);
        if (level == kIOPMAssertionLevelOff) {
            /* Dump this assertion in inactive list */
            insertInactiveAssertion(assertion, assertType);
            goto exit;
        }
    }
    else {
        /* If level is not set, set it to On */
        levelInt = kIOPMAssertionLevelOn;
        levelNum = CFNumberCreate(0, kCFNumberIntType, &levelInt);
        CFDictionarySetValue(assertion->props, kIOPMAssertionLevelKey, levelNum);
        CFRelease(levelNum);
    }

    /* Check if this is appplicable on battery power also */
    if (assertType->flags & kAssertionTypeNotValidOnBatt) {
        val = CFDictionaryGetValue(assertion->props, kIOPMAssertionAppliesToLimitedPowerKey);
        if (isA_CFBoolean(val) && (val == kCFBooleanTrue)) {
            assertion->state |= kAssertionStateValidOnBatt;
        }
    }

    numRef = CFDictionaryGetValue(assertion->props, kIOPMAssertionOnBehalfOfPID);
    if (isA_CFNumber(numRef)) {
        CFNumberGetValue(numRef, kCFNumberIntType, &assertion->causingPid);
    }

    val = CFDictionaryGetValue(assertion->props, kIOPMAssertionAppliesOnLidClose);
    if (isA_CFBoolean(val) && (val == kCFBooleanTrue)) {
        assertion->state |= kAssertionLidStateModifier;
    }

    /* Is this timed */
    numRef = CFDictionaryGetValue(assertion->props, kIOPMAssertionTimeoutKey);
    if (isA_CFNumber(numRef)) 
        CFNumberGetValue(numRef, kCFNumberDoubleType, &timeout);

    if (assertType->flags & kAssertionTypeAutoTimed) {
        /* Restrict timeout to a max value of 'autoTimeout' */
        if (!timeout || (timeout > assertType->autoTimeout))
            timeout = assertType->autoTimeout;
    }
    if (timeout) {
        assertion->timeout = (uint64_t)timeout+currTime; // Absolute time at which assertion expires
        insertTimedAssertion(assertion, assertType, true);
    }
    else {
        /* Insert into active assertion list */
        insertActiveAssertion(assertion, assertType);
    }


    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRaise);

    mt2RecordAssertionEvent(kAssertionOpRaise, assertion);

exit:
    return kIOReturnSuccess;

}


IOReturn doCreate(
                  pid_t                   pid,
                  CFMutableDictionaryRef  newProperties,
                  IOPMAssertionID         *assertion_id,
                  ProcessInfo             **procInfo
                 ) 
{
    int                     i;
    assertion_t             *assertion = NULL;
    assertion_t             *tmp_a = NULL;
    IOReturn                result = kIOReturnSuccess;
    ProcessInfo             *pinfo = NULL;
    assertionType_t         *assertType = NULL;
    static uint32_t         gNextAssertionIdx = 0;

    // assertion_id will be set to kIOPMNullAssertionID on failure.
    *assertion_id = kIOPMNullAssertionID;

    // Create a dispatch handler for process exit, if there isn't one
    if ( !(pinfo = processInfoRetain(pid)) ) {
        pinfo = processInfoCreate(pid);
        if (!pinfo) return kIOReturnNoMemory;

        if (procInfo) *procInfo = pinfo;
    }

    // Generate an id
    for (i=gNextAssertionIdx; CFDictionaryGetValueIfPresent(gAssertionsArray, 
                                                            (uintptr_t)i, (const void **)&tmp_a) == true; ) {
        i = (i+1) % kMaxAssertions;
        if (i == gNextAssertionIdx) break;
    }
    if (CFDictionaryGetValueIfPresent(gAssertionsArray, (uintptr_t)i, (const void **)&tmp_a) == true) {
        processInfoRelease(pid);
        return kIOReturnNoMemory;
    }

    assertion = calloc(1, sizeof(assertion_t));
    if (assertion == NULL) {
        processInfoRelease(pid);
        return kIOReturnNoMemory;
    }
    assertion->props = newProperties;
    CFRetain(newProperties);
    assertion->retainCnt = 1;
    assertion->pinfo = pinfo;

    assertion->assertionId = ID_FROM_INDEX(i);
    CFDictionarySetValue(gAssertionsArray, (uintptr_t)i, (const void *)assertion);
    gNextAssertionIdx = (i+1) % kMaxAssertions;

    result = raiseAssertion(assertion);
    if (result != kIOReturnSuccess) {
        processInfoRelease(pid);
        CFDictionaryRemoveValue(gAssertionsArray, (uintptr_t)i);
        CFRelease(assertion->props);
        free(assertion);

        return result;
    }

    assertType = &gAssertionTypes[assertion->kassert];
    if (!(assertion->state & kAssertionStateInactive))
        logAssertionEvent(kACreateLog, assertion);
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );

    *assertion_id = assertion->assertionId;

    return result;
}

static void copyAssertion(assertion_t *assertion, CFMutableDictionaryRef assertionsDict)
{
    bool                    created = false;
    CFNumberRef             pidCF = NULL;
    CFMutableDictionaryRef  processDict = NULL;
    CFMutableArrayRef       pidAssertionsArr = NULL;
    CFStringRef             processName = NULL;

    pidCF = CFNumberCreate(0, kCFNumberIntType, &assertion->pinfo->pid);

    processDict = (CFMutableDictionaryRef)CFDictionaryGetValue(assertionsDict, pidCF);
    if (processDict == NULL) 
    {
        pidAssertionsArr = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);

        processDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 2,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(processDict,
                             CFSTR("PerTaskAssertions"),
                             pidAssertionsArr);
        CFDictionarySetValue(processDict,
                             kIOPMAssertionPIDKey,
                             pidCF);


        CFDictionarySetValue(assertionsDict, pidCF, processDict);
        created = true;
    }
    else {
        pidAssertionsArr = (CFMutableArrayRef)CFDictionaryGetValue(processDict, CFSTR("PerTaskAssertions"));
    }

    processName = assertion->pinfo->name;
    if (processName) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionProcessNameKey, processName);
    }
    if (assertion->kassert < kIOPMNumAssertionTypes) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionTrueTypeKey, assertion_types_arr[assertion->kassert]);
    }

    CFArrayAppendValue(pidAssertionsArr, assertion->props);
    CFRelease(pidCF);

    if (created) {
        CFRelease(pidAssertionsArr);
        CFRelease(processDict);
    }
}


static CFArrayRef copyPIDAssertionDictionaryFlattened(void)
{
    CFMutableDictionaryRef  assertionsDict = NULL;
    CFMutableArrayRef       returnArray = NULL;
    CFDictionaryRef         *assertionsDictArr;
    assertionType_t         *assertType = NULL;
    CFIndex                 i, count;


    returnArray = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);

    assertionsDict = CFDictionaryCreateMutable(
                                               kCFAllocatorDefault, 0, 
                                               &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);

    /* Go thru each assertion type and copy assertion props */
    for (i=0; i < kIOPMNumAssertionTypes; i++)
    {
        if (i == kEnableIdleType) continue;
        assertType = &gAssertionTypes[i]; 
        applyToAllAssertionsSync(assertType, false, ^(assertion_t *assertion)
                                 {
                                 copyAssertion(assertion, assertionsDict);                    
                                 });

    }

    count = CFDictionaryGetCount(assertionsDict);
    assertionsDictArr = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef)*count); 
    CFDictionaryGetKeysAndValues(assertionsDict,
                                 NULL, (const void **)assertionsDictArr);

    for (i=0; i < count; i++) {
        CFArrayAppendValue(returnArray, assertionsDictArr[i]);
    }

    free(assertionsDictArr);
    CFRelease(assertionsDict);

    return returnArray;
}

static IOReturn copyAssertionForID(
                                   pid_t inPID, int inID,
                                   CFMutableDictionaryRef  *outAssertion)
{
    IOReturn        ret = kIOReturnBadArgument;
    assertion_t     *assertion = NULL;

    if (outAssertion) {
        *outAssertion = NULL;
    }
    else goto exit;

    ret = lookupAssertion(inPID, inID, &assertion);
    if ((kIOReturnSuccess != ret)) {
        ret = kIOReturnNotFound;
        goto exit;
    }

    CFRetain(assertion->props);
    *outAssertion = assertion->props;

exit:
    return ret;
}

static IOReturn doRetain(pid_t pid, IOPMAssertionID id)
{
    IOReturn        ret;
    assertion_t     *assertion = NULL;

    ret = lookupAssertion(pid, id, &assertion);

    if ((kIOReturnSuccess != ret)) {
        return ret;
    }

    assertion->retainCnt++;
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );

    return kIOReturnSuccess;
}



/* 
 * Called when display Sleep Timer setting is changed. The timeout value
 * for all assertions of type kIOPMAssertionUserIsActive is modified
 * to reflect the new display Sleep timer value.
 */
__private_extern__ void evalAllUserActivityAssertions(unsigned int dispSlpTimer)
{

    assertionType_t     *assertType;
    int                 changeInSecs;
    assertion_t         *assertion, *nextAssertion;
    uint64_t            currTime = getMonotonicTime();
    LIST_HEAD(, assertion) list  = LIST_HEAD_INITIALIZER(list);  // local list to hold assertions for which timeout is changed

    if (gDisplaySleepTimer == (int)dispSlpTimer)
        return;

    changeInSecs = ((int)dispSlpTimer - gDisplaySleepTimer) * 60;
    gDisplaySleepTimer = dispSlpTimer;

    assertType = &gAssertionTypes[kDeclareUserActivityType];

    assertion = LIST_FIRST(&assertType->activeTimed);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        if (!(assertion->state & kAssertionTimeoutIsSystemTimer)) {
            assertion = nextAssertion;
            continue;
        }

        if (gDisplaySleepTimer) {
            LIST_REMOVE(assertion, link); // Remove from timed list

            if (assertion->timeout + changeInSecs < currTime)
                assertion->timeout = currTime;
            else
                assertion->timeout += changeInSecs;

            LIST_INSERT_HEAD(&list, assertion, link); // add to local list
        }
        else {
            removeTimedAssertion(assertion, assertType, false);
            assertion->timeout = 0;
            insertActiveAssertion(assertion, assertType);
        }
        assertion = nextAssertion;
    }

    // Walk thru local list and add them back to activeTimed list
    while( (assertion = LIST_FIRST(&list)) )
    {
        LIST_REMOVE(assertion, link);
        insertByTimeout(assertion, assertType);
    }

    assertion = LIST_FIRST(&assertType->active);
    while(assertion && gDisplaySleepTimer)
    {
        nextAssertion = LIST_NEXT(assertion, link);
        if (!(assertion->state & kAssertionTimeoutIsSystemTimer)) {
            assertion = nextAssertion;
            continue;
        }

        assertion->timeout = currTime + (gDisplaySleepTimer * 60); 

        removeActiveAssertion(assertion, assertType);
        insertTimedAssertion(assertion, assertType, false);
        assertion = nextAssertion;
    }
    updateAssertionTimer(assertType);

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);


    if (gTimeoutChange) notify_post( kIOPMAssertionTimedOutNotifyString );
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );
}


/* 
 * Called when Idle Sleep Timer setting is changed. The timeout value
 * for assertions of type kIOPMAssertNetworkClientActive created by
 * API _io_pm_declare_network_client_active() are  modified
 * to reflect the new idle Sleep timer value.
 */
__private_extern__ void evalAllNetworkAccessAssertions()
{

    assertionType_t     *assertType;
    int                 changeInSecs;
    assertion_t         *assertion, *nextAssertion;
    uint64_t            currTime = getMonotonicTime();
    unsigned long       idleSleepTimer = gIdleSleepTimer;
    LIST_HEAD(, assertion) list  = LIST_HEAD_INITIALIZER(list);  // local list to hold assertions for which timeout is changed

    getIdleSleepTimer(&idleSleepTimer);
    changeInSecs = ((int)idleSleepTimer - gIdleSleepTimer) * 60;
    gIdleSleepTimer = idleSleepTimer;

    if (changeInSecs == 0) return;

    assertType = &gAssertionTypes[kNetworkAccessType];

    assertion = LIST_FIRST(&assertType->activeTimed);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        if (!(assertion->state & kAssertionTimeoutIsSystemTimer)) {
            assertion = nextAssertion;
            continue;
        }

        if (gIdleSleepTimer) {
            LIST_REMOVE(assertion, link); // Remove from timed list

            if (assertion->timeout + changeInSecs < currTime)
                assertion->timeout = currTime;
            else
                assertion->timeout += changeInSecs;

            LIST_INSERT_HEAD(&list, assertion, link); // add to local list
        }
        else {
            removeTimedAssertion(assertion, assertType, false);
            assertion->timeout = 0;
            insertActiveAssertion(assertion, assertType);
        }
        assertion = nextAssertion;
    }

    // Walk thru local list and add them back to activeTimed list
    while( (assertion = LIST_FIRST(&list)) )
    {
        LIST_REMOVE(assertion, link);
        insertByTimeout(assertion, assertType);
    }

    assertion = LIST_FIRST(&assertType->active);
    while( assertion && gIdleSleepTimer )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        if (!(assertion->state & kAssertionTimeoutIsSystemTimer)) {
            assertion = nextAssertion;
            continue;
        }

        assertion->timeout = currTime + (gIdleSleepTimer * 60); 

        removeActiveAssertion(assertion, assertType);
        insertTimedAssertion(assertion, assertType, false);
        assertion = nextAssertion;
    }
    updateAssertionTimer(assertType);

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);

    if (gTimeoutChange) notify_post( kIOPMAssertionTimedOutNotifyString );
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );
}

__private_extern__ void evalAllInteractivePushAssertions()
{
    assertionType_t     *assertType;
    uint64_t            newTimeout;

    assertType = &gAssertionTypes[kInteractivePushServiceType];
    newTimeout = assertType->autoTimeout + getMonotonicTime();

    applyToAllAssertionsSync(assertType, false, ^(assertion_t *assertion)
                             {
                                 CFNumberRef  timeLeftCF = NULL;
                                 CFDateRef    updateDate = NULL;

                                 if (assertion->timeout > newTimeout) {
                                     assertion->timeout = newTimeout;

                                     timeLeftCF = CFNumberCreate(0, kCFNumberIntType, &assertType->autoTimeout);
                                     if (timeLeftCF) {
                                         CFDictionarySetValue(assertion->props, kIOPMAssertionTimeoutTimeLeftKey, timeLeftCF);
                                         CFRelease(timeLeftCF);
                                     }

                                     updateDate = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
                                     if (updateDate) {
                                         CFDictionarySetValue(assertion->props, kIOPMAssertionTimeoutUpdateTimeKey, updateDate);
                                         CFRelease(updateDate);
                                     }
                                 }
                             });

    updateAssertionTimer(assertType);

    if (gTimeoutChange) notify_post( kIOPMAssertionTimedOutNotifyString );
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );
}



static void   evaluateAssertions(void)
{
    int         i, pwrSrc;
    static int  prevPwrSrc = -1;
    assertionType_t    *assertType;

    pwrSrc = _getPowerSource();
    if (pwrSrc == prevPwrSrc)
        return; // If power source hasn't changed, there is nothing to do

    prevPwrSrc = pwrSrc;

    for (i=0; i < kIOPMNumAssertionTypes; i++)
    {
        assertType = &gAssertionTypes[i];
        if (assertType->handler) {
            (*assertType->handler)(assertType, kAssertionOpEval);
        }
    }

    // re-configure assertions that change with power source 
    configAssertionType(kBackgroundTaskType, false);
    configAssertionType(kNetworkAccessType, false);
    configAssertionType(kInteractivePushServiceType, false);

    /* Timeout for Interactive push assertions changes with power source change */
    evalAllInteractivePushAssertions( );
    cancelPowerNapStates( );

    for (i=0; i < kIOPMNumAssertionTypes; i++)
    {
        assertType = &gAssertionTypes[i];
        if (assertType->flags & kAssertionTypeNotValidOnBatt) {
            applyToAllAssertionsSync(assertType, false, ^(assertion_t *assertion)
            {
                if ((pwrSrc == kBatteryPowered) && !(assertion->state & kAssertionStateValidOnBatt)) {
                    updateAppStats(assertion, kAssertionOpRelease);
                }
                else if (pwrSrc != kBatteryPowered) {
                    updateAppStats(assertion, kAssertionOpRaise);
                }
            });
        }
    }
    logASLAssertionsAggregate();

}

__private_extern__ void setSleepServicesTimeCap(uint32_t  timeoutInMS)
{
    assertionType_t *assertType;

    assertType = &gAssertionTypes[kPushServiceTaskType];

    // Avoid duplicate resets to 0
    if ( (timeoutInMS == 0) && (assertType->globalTimeout == 0) )
        return;

    resetGlobalTimer(assertType, timeoutInMS/1000);
    if (timeoutInMS == 0) {
        CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, 
                              ^{ enforceAssertionTypeTimeCap(assertType); });
        CFRunLoopWakeUp(_getPMRunLoop());
    }
}


static void configAssertionEffect(kerAssertionEffect idx)
{
    gAssertionEffects[idx].effectIdx = idx;
    LIST_INIT(&gAssertionEffects[idx].assertTypes);
}


__private_extern__ void configAssertionType(kerAssertionType idx, bool initialConfig)
{
    assertionHandler_f   oldHandler = NULL;
    CFNumberRef idxRef = NULL;
    uint32_t    oldFlags, flags;
    static bool prevBTdisable = false;
    kerAssertionType  altIdx;
    assertionType_t *assertType;
    kerAssertionEffect  prevEffect, newEffect;

    // This can get called before gUserAssertionTypesDict is initialized
    if ( !gUserAssertionTypesDict )
        return;

    assertType = &gAssertionTypes[idx];
    if (!initialConfig) {
        oldHandler = assertType->handler;
        oldFlags = assertType->flags;
        prevEffect = assertType->effectIdx;
    }

    assertType->kassert = idx;
    switch(idx) 
    {
    case kHighPerfType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeNeedsCPU, idxRef);
        assertType->handler = modifySettings;
        newEffect = kHighPerfEffect;
        break;

    case kPreventIdleType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypePreventUserIdleSystemSleep, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeNoIdleSleep, idxRef);
        assertType->handler = modifySettings;

        newEffect = kPrevIdleSlpEffect;
        assertType->flags |= kAssertionTypePreventAppSleep;
        break;

    case kDisableInflowType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeDisableInflow, idxRef);
        assertType->handler = handleBatteryAssertions;
        newEffect = kDisableInflowEffect;
        break;


    case kInhibitChargeType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeInhibitCharging, idxRef);
        assertType->handler = handleBatteryAssertions;
        newEffect = kInhibitChargeEffect;
        break;

    case kDisableWarningsType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeDisableLowBatteryWarnings, idxRef);
        assertType->handler = handleBatteryAssertions;
        newEffect = kDisableWarningsEffect;
        break;

    case kPreventDisplaySleepType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypePreventUserIdleDisplaySleep, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeNoDisplaySleep, idxRef);
        assertType->handler = setKernelAssertions;

        assertType->flags |= kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        newEffect = kPrevDisplaySlpEffect;
        break;

    case kEnableIdleType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeEnableIdleSleep, idxRef);
        assertType->handler = enableIdleHandler;
        newEffect = kEnableIdleEffect;
        break;

    case kPreventSleepType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypePreventSystemSleep, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeDenySystemSleep, idxRef);
        assertType->flags |= kAssertionTypeNotValidOnBatt | kAssertionTypePreventAppSleep 
            | kAssertionTypeLogOnCreate;
        assertType->handler = setKernelAssertions;

        newEffect = kPrevDemandSlpEffect;
        break;

    case kSRPreventSleepType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertInternalPreventSleep, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertMaintenanceActivity, idxRef);
        assertType->flags |= kAssertionTypeNotValidOnBatt | kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        assertType->handler = setKernelAssertions;

        newEffect = kPrevDemandSlpEffect;

        break;

    case kPreventDiskSleepType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertPreventDiskIdle, idxRef);
        assertType->handler = modifySettings;
        newEffect = kPreventDiskSleepEffect;
        break;

    case kExternalMediaType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, _kIOPMAssertionTypeExternalMedia, idxRef);
        assertType->handler = setKernelAssertions;
        newEffect = kExternalMediaEffect;
        break;

    case kDeclareUserActivityType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionUserIsActive, idxRef);
        assertType->handler = setKernelAssertions;

        assertType->flags |= kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        newEffect = kPrevDisplaySlpEffect;
        break;

    case kDeclareSystemActivityType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeSystemIsActive, idxRef);
        assertType->handler = modifySettings;

        newEffect = kPrevIdleSlpEffect;
        assertType->flags |= kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        break;

    case kPushServiceTaskType:
        if ( isA_SleepSrvcWake() && _SS_allowed() ) {
            idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
            CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeApplePushServiceTask, idxRef);
            newEffect = kPrevDemandSlpEffect;
        }
        else {
            /* Set this as an alias to BackgroundTask assertion for non-sleep srvc wakes */
            altIdx = kBackgroundTaskType;
            idxRef = CFNumberCreate(0, kCFNumberIntType, &altIdx);
            CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeApplePushServiceTask, idxRef);
            newEffect = kNoEffect;
        }
        assertType->flags |= kAssertionTypeGloballyTimed | kAssertionTypePreventAppSleep;
        assertType->handler = setKernelAssertions;


        break;

    case kBackgroundTaskType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeBackgroundTask, idxRef);
        assertType->flags |= kAssertionTypeNotValidOnBatt | kAssertionTypePreventAppSleep;
        if (_DWBT_enabled()) {
            assertType->handler = setKernelAssertions;

            if ( !isA_BTMtnceWake() ) {
                if (!prevBTdisable) {
                    assertType->disableCnt++;
                    prevBTdisable = true;
                }
            }
            else if (prevBTdisable)  {
                if (assertType->disableCnt) 
                    assertType->disableCnt--;
                prevBTdisable = false;
            }
            newEffect = kPrevDemandSlpEffect;
        }
        else  {
            assertType->handler = modifySettings;
            newEffect = kPrevIdleSlpEffect;
            if (prevBTdisable) {
                if (assertType->disableCnt) 
                    assertType->disableCnt--;
                prevBTdisable = false;
            }
        }

        break;

    case kTicklessDisplayWakeType:
#if TCPKEEPALIVE
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertDisplayWake, idxRef);
        assertType->handler = displayWakeHandler;
        assertType->flags |= kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        newEffect = kTicklessDisplayWakeEffect;
        assertType->entitlement = kIOPMDarkWakeControlEntitlement;
#else
        // TicklessDisplayWake is not a valid assertion type.
        // We are intentionally disabling it.
        altIdx = kPreventDisplaySleepType;
        idxRef = CFNumberCreate(0, kCFNumberIntType, &altIdx);
        newEffect = kNoEffect;
#endif
        break;

    case kIntPreventDisplaySleepType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertInternalPreventDisplaySleep, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertRequiresDisplayAudio, idxRef);
        assertType->handler = setKernelAssertions;
        assertType->flags |= kAssertionTypeLogOnCreate;

        newEffect = kPrevDisplaySlpEffect;
        break;

    case kNetworkAccessType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertNetworkClientActive, idxRef);
        assertType->flags |= kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;

        if (kACPowered == _getPowerSource()) {
            assertType->handler = setKernelAssertions;
            newEffect = kPrevDemandSlpEffect;
        }
        else {
            assertType->handler = modifySettings;
            newEffect = kPrevIdleSlpEffect;
        }
        break;

    case kInteractivePushServiceType:
        newEffect = kNoEffect;
#if TCPKEEPALIVE
        if (getTCPKeepAliveState(NULL, 0) == kActive) {
            /* If keep alives are allowed */
            idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);

            assertType->handler = setKernelAssertions;
            assertType->flags = kAssertionTypePreventAppSleep | kAssertionTypeAutoTimed;
            assertType->autoTimeout = getCurrentSleepServiceCapTimeout()/1000;

            newEffect = kPrevDemandSlpEffect;
        }
        else if ( isA_SleepSrvcWake() && _SS_allowed() ) {
            /* else if in a sleep service window, set this as an alias to ApplePushServiceTask  */

            altIdx = kPushServiceTaskType;
            idxRef = CFNumberCreate(0, kCFNumberIntType, &altIdx);

        }
        else {
            /* else make this behave same as BackgroundTask assertion when PowerNap is disabled */
            idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
            assertType->flags = kAssertionTypeNotValidOnBatt | kAssertionTypePreventAppSleep;
            assertType->flags |= kAssertionTypeAutoTimed;
            assertType->autoTimeout = getCurrentSleepServiceCapTimeout()/1000;
            assertType->handler = modifySettings;

            newEffect = kPrevIdleSlpEffect;

        }
#else
        if ( isA_SleepSrvcWake() && _SS_allowed() ) {
            altIdx = kPushServiceTaskType;
        }
        else {
            altIdx = kBackgroundTaskType;
        }

        idxRef = CFNumberCreate(0, kCFNumberIntType, &altIdx);
#endif
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertInteractivePushServiceTask, idxRef);
        assertType->entitlement = kIOPMInteractivePushEntitlement;

        break;

    case kReservePwrPreventIdleType:
#if TARGET_OS_EMBEDDED
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
#else
        altIdx = kPreventIdleType;
        idxRef = CFNumberCreate(0, kCFNumberIntType, &altIdx);
#endif
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertAwakeReservePower, idxRef);
        assertType->flags |= kAssertionTypePreventAppSleep ;
        assertType->handler = modifySettings;
        newEffect = kPrevIdleSlpEffect;
        assertType->entitlement = kIOPMReservePwrCtrlEntitlement;

        break;


    default:
        return;
    }
    if (idxRef)
        CFRelease(idxRef);


    if (assertType->disableCnt) {
        newEffect = kNoEffect;
    }

    if (initialConfig) {
        assertType->effectIdx = newEffect;
        LIST_INSERT_HEAD(&gAssertionEffects[newEffect].assertTypes, assertType, link);
    }
    else if ((oldHandler != assertType->handler)  || (prevEffect != newEffect)){
        // Temporarily disable the assertion type and call the old handler.
        flags = assertType->flags;
        LIST_REMOVE(assertType, link);

        oldHandler(assertType, kAssertionOpEval);
        assertType->flags = flags;
        if (gActivityAggCnt && (prevEffect != newEffect)) {
            applyToAllAssertionsSync(assertType, false, ^(assertion_t *assertion)
                                     {
                                     updateAppStats(assertion, kAssertionOpRelease);
                                     });
        }

        assertType->effectIdx = newEffect;

        LIST_INSERT_HEAD(&gAssertionEffects[newEffect].assertTypes, assertType, link);

        // Call the new handler
        if (newEffect != kNoEffect)
            assertType->handler(assertType, kAssertionOpEval);

        if (gActivityAggCnt && (prevEffect != newEffect)) {
            applyToAllAssertionsSync(assertType, false, ^(assertion_t *assertion)
                                     {
                                     updateAppStats(assertion, kAssertionOpRaise);
                                     });
        }

    }
    else if (oldFlags != assertType->flags) {
        if (assertType->handler)
            assertType->handler(assertType, kAssertionOpEval);
    }

}

__private_extern__ void PMAssertions_prime(void)
{

    kerAssertionType  idx = 0;
    kerAssertionEffect  effctIdx = 0;
    int token;

    gAssertionsArray = CFDictionaryCreateMutable(NULL, kMaxAssertions, NULL, NULL); 
    gProcessDict = CFDictionaryCreateMutable(0, 0, NULL, NULL);

    gUserAssertionTypesDict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);


    assertion_types_arr[kHighPerfType]             = kIOPMAssertionTypeNeedsCPU; 
    assertion_types_arr[kPreventIdleType]          = kIOPMAssertionTypePreventUserIdleSystemSleep;
    assertion_types_arr[kPreventSleepType]         = kIOPMAssertionTypePreventSystemSleep;
    assertion_types_arr[kDisableInflowType]        = kIOPMAssertionTypeDisableInflow; 
    assertion_types_arr[kInhibitChargeType]        = kIOPMAssertionTypeInhibitCharging;
    assertion_types_arr[kDisableWarningsType]      = kIOPMAssertionTypeDisableLowBatteryWarnings;
    assertion_types_arr[kPreventDisplaySleepType]  = kIOPMAssertionTypePreventUserIdleDisplaySleep;
    assertion_types_arr[kEnableIdleType]           = kIOPMAssertionTypeEnableIdleSleep;
    assertion_types_arr[kExternalMediaType]             = _kIOPMAssertionTypeExternalMedia;
    assertion_types_arr[kDeclareUserActivityType]       = kIOPMAssertionUserIsActive;
    assertion_types_arr[kPushServiceTaskType]      = kIOPMAssertionTypeApplePushServiceTask;
    assertion_types_arr[kBackgroundTaskType]       = kIOPMAssertionTypeBackgroundTask;
    assertion_types_arr[kDeclareSystemActivityType]     = kIOPMAssertionTypeSystemIsActive;
    assertion_types_arr[kSRPreventSleepType]       = kIOPMAssertInternalPreventSleep;
    assertion_types_arr[kTicklessDisplayWakeType]       = kIOPMAssertDisplayWake;
    assertion_types_arr[kPreventDiskSleepType]     = kIOPMAssertPreventDiskIdle;
    assertion_types_arr[kNetworkAccessType]        = kIOPMAssertNetworkClientActive;
    assertion_types_arr[kIntPreventDisplaySleepType] = kIOPMAssertInternalPreventDisplaySleep;
    assertion_types_arr[kInteractivePushServiceType] = kIOPMAssertInteractivePushServiceTask;
    assertion_types_arr[kReservePwrPreventIdleType]  = kIOPMAssertAwakeReservePower;

    for (effctIdx = 0; effctIdx < kMaxAssertionEffects; effctIdx++)
        configAssertionEffect(effctIdx);

    for (idx = 0; idx < kIOPMNumAssertionTypes; idx++)
        configAssertionType(idx, true);

    getDisplaySleepTimer(&gDisplaySleepTimer); 
    getIdleSleepTimer(&gIdleSleepTimer); 

    // Reset kernel assertions to clear out old values from prior to powerd's crash
    sendUserAssertionsToKernel(0);
#if TARGET_OS_EMBEDDED
    /* 
     * Disable Idle Sleep until some one comes and enables the idle sleep
     * by issuing 'kIOPMAssertionTypeEnableIdleSleep' assertion.
     */
    gAssertionTypes[kEnableIdleType].handler(
                                              &gAssertionTypes[kEnableIdleType], kAssertionOpRelease);

    /* BT assertions should have no effect on embedded */
    disableAssertionType(kBackgroundTaskType);
#else

    setAggregateLevel(kEnableIdleType, 1); /* Idle sleep is enabled by default */
    gDebugFlags = kIOPMDebugAssertionASLLog;

    notify_register_dispatch("com.apple.notificationcenter.pushdnd", &token,
                             dispatch_get_main_queue(),
                             ^(int t) { 
                             configAssertionType(kInteractivePushServiceType, false); });
#endif
    notify_register_dispatch(kIOPMAssertionsCollectBTString, &token,
                             dispatch_get_main_queue(),
                             ^(int t) { /*Dummy registration to keep the key/value valid with notifyd */ });

    return;
}



