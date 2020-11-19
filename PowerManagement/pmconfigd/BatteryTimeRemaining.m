/*
 * Copyright (c) 2012 - 2020 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2012 - 2020 Apple Computer, Inc.  All rights reserved.
 *
 */
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <notify.h>
#include <mach/mach.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>

#include <CoreFoundation/CFXPCBridge.h>
#include <servers/bootstrap.h>
#include <asl.h>
#include <bsm/libbsm.h>
#include <sys/time.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <Foundation/Foundation.h>
#import "AppleSmartBatteryKeys.h"


#if TARGET_OS_IPHONE || POWERD_IOS_XCTEST
#include <MobileKeyBag/MobileKeyBag.h>
#include <CoreFoundation/CFPreferences_Private.h>
#endif
#if TARGET_OS_IPHONE
#include <containermanager/containermanager.h>
#endif
#if !TARGET_OS_OSX
#include <battery/battery.h>
#endif

#include "pmconfigd.h"
#include "powermanagementServer.h" // mig generated
#include "BatteryTimeRemaining.h"
#include "PMSettings.h"
#include "UPSLowPower.h"
#include "PMAssertions.h"
#include "PMStore.h"
#include "IOUPSPrivate.h"
#include "BatteryData.h"

#define kMinNominalCapacityPercentage       1
#define kMaxNominalCapacityPercentage       150
#define kNominalCapacityPercentageThreshold  80
#define kInitialNominalCapacityPercentage   104

os_log_t    battery_log = NULL;
#undef   LOG_STREAM
#define  LOG_STREAM   battery_log

/**** PMBattery configd plugin
  We clean up, massage, and re-package the data from the batteries and publish
  it in the more palatable form described in IOKit/Headers/IOPowerSource.h

  All kernel batteries conform to the IOPMPowerSource base class.

  We provide the following information in a CFDictionary and publish it for
  all user processes to see:
    Name
    CurrentCapacity
    MaxCapacity
    Remaining Time To Empty
    Remaining Time To Full Charge
    IsCharging
    IsPresent
    Type
****/

#define kPSMaxCount   16

static PSStruct gPSList[kPSMaxCount];

// kBattNotCharging checks for (int16_t)-1 invalid current readings
#define kBattNotCharging        0xffff

#define kSlewStepMin            2
#define kSlewStepMax            10
#define kDiscontinuitySettle    60
typedef struct {
    int                 showingTime;
    bool                settled;
} SlewStruct;
SlewStruct *slew = NULL;

// Battery health calculation constants
#define kSmartBattReserve_mAh    200.0
#define kMaxBattMinutes     1200

#define kSpecialInternalBatteryID  99

// static global variables for tracking battery state
typedef struct {
    CFAbsoluteTime   lastDiscontinuity;
    bool             percentageDiscontinuity;
    int              systemWarningLevel;
    bool             warningsShouldResetForSleep;
    bool             readACAdapterAgain;
    bool             selectionHasSwitched;
    int              psTimeRemainingNotifyToken;
    int              psPercentChangeNotifyToken;
    bool             noPoll;
    bool             needsNotifyAC;
    PSStruct         *internal;
} BatteryControl;
static BatteryControl   control;
static CFDictionaryRef  customBatteryProps = NULL;
static CFDictionaryRef  adapterDetails = NULL;
static dispatch_queue_t batteryTimeRemainingQ;
static int currentPercentRemaining = 0;
static long physicalBatteriesCount = 0;
static CFStringRef gBatterySerialNumber = NULL;

#if TARGET_OS_IPHONE || POWERD_IOS_XCTEST || TARGET_OS_OSX
uint64_t batteryHealthUPOAware = 0;
uint32_t battReadTimeDelta = kMinTimeDeltaForBattRead; // Time delta between reading battery data for battery health evaluation
#endif
#if TARGET_OS_IPHONE || POWERD_IOS_XCTEST
bool smcBasedDevice = false;
bool nccp_cc_filtering = true;  // Support for NCCP filtering using CycleCount
uint64_t batteryHealthP0Threshold = 0;

void removeKeyFromBatteryHealthDataPrefs(CFStringRef key);
void saveBatteryHealthDataToPrefs(CFDictionaryRef bhData);
CFDictionaryRef copyBatteryHealthDataFromPrefs(void);
CFDictionaryRef copyPowerlogBatteryHealthData(void);
#endif

// forward declarations
STATIC PSStruct         *iops_newps(int pid, int psid);
static void             checkTimeRemainingValid(IOPMBattery **batts);
static CFDictionaryRef packageKernelPowerSource(IOPMBattery *b, PSStruct *ps);
static void             HandlePublishAllPowerSources(void);
static IOReturn         HandleAccessoryPowerSources(PSStruct *ps, CF_RELEASES_ARGUMENT CFDictionaryRef update);
static CFDictionaryRef  getPSByType(CFStringRef type);
static int getActivePSType_sync(void);
static CFDictionaryRef getActiveUPSDictionary_sync(void);
static int _batteryCountSync(void);
static PowerSources _getPowerSourceSync(void);
static void BatteryTimeRemaining_finishSync(void);
static void btr_recordFDREvent(int eventType, bool checkStandbyStatus);

#if TARGET_OS_OSX
#define NVRAM_BATTERY_HEALTH_VER_MAJOR  1
#define NVRAM_BATTERY_HEALTH_VER_MINOR  0
#define kUnMitigatedNominalCapacityPercentage "nccAlt"
#define kMitigatedNominalCapacityPercentage "ncc"
#define kUnMitigatedNominalCapacityAvg "nccAvgAlt"
#define kMitigatedNominalCapacityAvg "nccAvg"
#define kFccAvgHistoryCount "fccAvgHistoryCount"
#define kFccDaySampleCount "fccDaySampleCount"
#define kMitigatedFccDaySampleAvg "fccDaySampleAvg"
#define kUnMitigatedFccDaySampleAvg "fccDaySampleAvgAlt"
#define kWaitForFCState "waitFc"

enum vactMode {
    vactModeDisabled = 0,
    vactModeEnabled,
    vactModesCount,
};

#define CAPACITY_NO_CHANGE  (0)
#define CAPACITY_FCC_CHANGE  (1 << 0)
#define CAPACITY_NCC_CHANGE  (1 << 1)
#define CAPACITY_SAMPLING_EPOCH_CHANGE  (1 << 2)

#define SECONDS_IN_WEEK     (7 * 24 * 60 * 60)
#define NVRAM_DEVICE_PATH "IODeviceTree:/options"
#define NVRAM_BATTERY_HEALTH_KEY "battery-health"

#if TARGET_CPU_ARM64
// 40A0DDD2-77F8-4392-B4A3-1E7304206516
#define NVRAM_BATTERY_HEALTH_UUID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, \
        0x40, 0xA0, 0xDD, 0xD2, 0x77, 0xF8, 0x43, 0x92, 0xB4, 0xA3, 0x1E, 0x73, 0x04, 0x20, 0x65, 0x16)
#else
// 735B3B05-2634-4253-9DB8-5A048B418E3D
#define NVRAM_BATTERY_HEALTH_UUID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, \
        0x73, 0x5B, 0x3B, 0x05, 0x26, 0x34, 0x42, 0x53, 0x9D, 0xB8, 0x5A, 0x04, 0x8B, 0x41, 0x8E, 0x3D)
#endif

#define NCC_TEMP_THRESH  (2912)
#define NCC_CURRENT_THRESH  (40)
#define NCC_CURRENT_THRESH_SCALE  (100)
#define NCC_GAMMA_SCALE  (1000)
#define NCC_GAMMA  (909)   // 11.5 day time constant

struct capacitySample {
    // inputs
    int fcc;    // Scaled FCC as returned by SMC

    // persistent variable
    int fccDaySampleAvg;
    // persistent output
    int ncc;
    int nccpMonotonic;
};

struct nominalCapacityParams {
    // inputs
    int current;        // BISS or B0AC
    int temperature;    // TB0T
    int designCapacity;
    int fcc;            // FCC as returned by the gauge
    // substruct
    struct capacitySample sample[vactModesCount];
    // persistent variables
    unsigned int fccDaySampleCount;
    unsigned int fccAvgHistoryCount;
    // parameters
    int gamma;
    // output
    unsigned int significantChange;
    unsigned int error;
};

static const CFStringRef capacityKeys[vactModesCount][3] = {
    [vactModeEnabled] = {
        CFSTR(kMitigatedNominalCapacityPercentage),
        CFSTR(kMitigatedNominalCapacityAvg),
        CFSTR(kMitigatedFccDaySampleAvg),
    },
    [vactModeDisabled] = {
        CFSTR(kUnMitigatedNominalCapacityPercentage),
        CFSTR(kUnMitigatedNominalCapacityAvg),
        CFSTR(kUnMitigatedFccDaySampleAvg),
    },
};

static CFMutableDictionaryRef cachedBatteryHealthDataDict;
static void (^energyPrefsNotificationHandler)(void);
static bool getVactState(void);
static bool isVactSupported(void) __attribute__((unused));
static void updateVactState(void);
#endif

static void _internal_dispatch_assert_queue(dispatch_queue_t queue) {
#if !XCTEST
    dispatch_assert_queue(queue);
#else
    return;
#endif
}

static void _internal_dispatch_assert_queue_barrier(dispatch_queue_t queue) {
#if !XCTEST
    dispatch_assert_queue_barrier(queue);
#else
    return;
#endif
}

static void _internal_dispatch_assert_queue_not(dispatch_queue_t queue) {
#if !XCTEST
    dispatch_assert_queue_not(queue);
#else
    return;
#endif
}

#ifdef XCTEST

PowerSources xctPowerSource = kACPowered;
uint32_t xctCapacity = 80;

void xctSetPowerSource(PowerSources src) {
    xctPowerSource = src;
}

void xctSetCapacity(uint32_t capacity) {
    xctCapacity = capacity;
}

#endif

// Arguments For startBatteryPoll()
typedef enum {
    kPeriodicPoll           = 0,
    kImmediateFullPoll      = 1
} PollCommand;
static bool             startBatteryPoll(PollCommand x);

#if TARGET_OS_IOS || TARGET_OS_WATCH || TARGET_OS_OSX
static void initBatteryHealthData(void);
#endif // TARGET_OS_IOS

static void BatteryTimeRemaining_notify_post(const char *token)
{
    uint32_t rc = notify_post(token);

    if (rc == NOTIFY_STATUS_OK) {
        INFO_LOG("posted '%s'\n", token);
    } else {
        ERROR_LOG("failed to post '%s'. rc:%#x\n", token, rc);
    }
}

#if TARGET_OS_IOS || TARGET_OS_WATCH
static dispatch_source_t batteryDataUpdateTimer = NULL;
#endif // TARGET_OS_IOS || TARGET_OS_WATCH

#if TARGET_OS_IOS || TARGET_OS_OSX || TARGET_OS_WATCH
static void _setBatteryHealthData(CFMutableDictionaryRef outDict, IOPMBattery *b);

/*
 * Convert a raw 'val' to its nominal value in percentage, using nominal 'base'.
 */
static int rawToNominal(int val, int base)
{
    int result = 0;

    if (val && (base > 0)) {
        result = ceil((double)val/(double)base * 100);
    }
    return result;
}

// The NCC Not Determinable flag will be set if DesignCapacity ≤ 0, if NCC% ∉ [1,150], or
// if either NominalChargeCapacity or DesignCapacity keys are missing from battery properties.
#define IS_IN_NOMINAL_RANGE(x) ((x >= kMinNominalCapacityPercentage) && (x <= kMaxNominalCapacityPercentage))
#endif


__private_extern__ dispatch_queue_t BatteryTimeRemaining_getQ(void)
{
    return batteryTimeRemainingQ;
}

static void btr_recordFDREvent(int eventType, bool checkStandbyStatus)
{
    dispatch_async(_getPMMainQueue(), ^() {
        recordFDREvent(eventType, checkStandbyStatus);
    });
}

/* If the battery doesn't specify an alternative time, we wait 16 seconds
   of ignoring the battery's (or our own) time remaining estimate.
*/
enum {
    kInvalidWakeSecsDefault = 16
};

static IOReturn _getLowCapRatioTime(CFStringRef batterySerialNumber,
                             boolean_t *hasLowCapRatio,
                             time_t *since)
{
    IOReturn                    ret         = kIOReturnError;

#if !TARGET_OS_IPHONE
    CFNumberRef                 num         = NULL;
    CFDictionaryRef             dict        = NULL;

    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if (!hasLowCapRatio || !since || !isA_CFString(batterySerialNumber)) {
        return ret;
    }

    *hasLowCapRatio = false;
    *since = 0;

    if (cachedKeyPresence) {
        *hasLowCapRatio = cachedHasLowCap;
        *since = cachedLowCapRatioTime;
        ret = kIOReturnSuccess;
        goto exit;
    }

    dict = IOPMCopyFromPrefs(NULL, kIOPMBatteryWarnSettings);
    if (!isA_CFDictionary(dict)) {
        goto exit;
    }

    num = CFDictionaryGetValue(dict, batterySerialNumber);
    if (isA_CFNumber(num)) {
        if (!CFNumberGetValue(num, kCFNumberSInt64Type, since)) {
            goto exit;
        }
        *hasLowCapRatio = true;
        cachedHasLowCap = true;
        cachedLowCapRatioTime = *since;
        // set the flag to indicate the file was read once successfully
        cachedKeyPresence = true;
    }

    ret = kIOReturnSuccess;

exit:
    if (dict) {
        CFRelease(dict);
    }

#endif
    return ret;
}

static void _unpackBatteryState(IOPMBattery *b, CFDictionaryRef prop)
{
    CFBooleanRef    boo;
    CFNumberRef     n;
    CFDictionaryRef dict;
    uint64_t        tmp64;

    if (!isA_CFDictionary(prop)) return;

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSExternalConnectedKey));
    b->externalConnected = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSExternalChargeCapableKey));
    b->externalChargeCapable = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSBatteryInstalledKey));
    b->isPresent = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSIsChargingKey));
    b->isCharging = (kCFBooleanTrue == boo);

#if TARGET_OS_IPHONE
    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSRawExternalConnectedKey));
    b->rawExternalConnected = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSAtCriticalLevelKey));
    b->isCritical = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSRestrictedModeKey));
    b->isRestricted = (kCFBooleanTrue == boo);
#endif
#if TARGET_OS_IOS || TARGET_OS_WATCH
    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMFullyChargedKey));
    b->fullyCharged = (kCFBooleanTrue == boo);
#endif // TARGET_OS_IOS || TARGET_OS_WATCH

    b->failureDetected = (CFStringRef)CFDictionaryGetValue(prop, CFSTR(kIOPMPSErrorConditionKey));

    b->batterySerialNumber = (CFStringRef)CFDictionaryGetValue(prop, CFSTR(kIOPMPSSerialKey));

    b->chargeStatus = (CFStringRef)CFDictionaryGetValue(prop, CFSTR(kIOPMPSBatteryChargeStatusKey));

    _getLowCapRatioTime(b->batterySerialNumber,
                        &(b->hasLowCapRatio),
                        &(b->lowCapRatioSinceTime));

    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSVoltageKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->voltage);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSCurrentCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->currentCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSMaxCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->maxCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSDesignCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->designCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSTimeRemainingKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->hwAverageTR);
    }


    n = CFDictionaryGetValue(prop, CFSTR("InstantAmperage"));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->instantAmperage);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSAmperageKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->avgAmperage);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSMaxErrKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->maxerr);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSCycleCountKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->cycleCount);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSLocationKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->location);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSInvalidWakeSecondsKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->invalidWakeSecs);
    } else {
        b->invalidWakeSecs = kInvalidWakeSecsDefault;
    }
    n = CFDictionaryGetValue(prop, CFSTR("PermanentFailureStatus"));
    if (n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->pfStatus);
    } else {
        b->pfStatus = 0;
    }

    return;
}

static void initializeBatteryCalculations(void)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if ((_batteryCountSync() == 0) || (control.internal != NULL)) {
        return;
    }

    // Does this Mac have an internal battery
    // reported through IOPMPowerSource?
    // If so, we'll track it in the gPSList.

    // Any other processes that publish power sources (like upsd)
    // will get a powersource id > 5000
    control.internal = iops_newps(getpid(), kSpecialInternalBatteryID);
    control.internal->psType = kPSTypeIntBattery;

    control.lastDiscontinuity = CFAbsoluteTimeGetCurrent();
    BatteryTimeRemaining_notify_post(kIOPSNotifyAttach);

#if TARGET_OS_IOS || TARGET_OS_WATCH
    dispatch_async(batteryTimeRemainingQ, ^() {initBatteryHealthData();});
#endif // TARGET_OS_IOS
    return;
}

// _readAndPublicACAdapter
static void readAndPublishACAdapter(bool adapterExists, CFDictionaryRef newAdapter)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if (!adapterExists && !adapterDetails) {
        goto exit;
    }

    // Make sure we re-read the adapter on wake from sleep
    if (control.readACAdapterAgain) {
        control.readACAdapterAgain = false;
        if (adapterDetails) {
            CFRelease(adapterDetails);
            adapterDetails = NULL;
        }
    }

    if (adapterExists) {
        if (!newAdapter) {
            goto exit;
        }
        else {
            CFRetain(newAdapter);
        }
        if (isA_CFDictionary(adapterDetails) && CFEqual(newAdapter, adapterDetails)) {
            CFRelease(newAdapter);
            goto exit;
        }
    }
    else {
        newAdapter = NULL;
    }

    if (adapterDetails) {
        CFRelease(adapterDetails);
        adapterDetails = NULL;
    }

    if (newAdapter) {
        adapterDetails = newAdapter;
    }

    BatteryTimeRemaining_notify_post(kIOPSNotifyAdapterChange);
#if TARGET_OS_IPHONE && !TARGET_OS_BRIDGE
    IOPMBattery **b = _batteries();
    if (b && b[0] && isA_CFDictionary(b[0]->properties)) {
        updateBatteryData(b[0]->properties);
    }
#endif // TARGET_OS_IPHONE && !TARGET_OS_BRIDGE

exit:
    return ;
}

static void BatterySetNoPoll(bool noPoll)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if (control.noPoll == noPoll) {
        return;
    }

    control.noPoll = noPoll;
    if (!noPoll) {
        startBatteryPoll(kImmediateFullPoll);
    } else {
        kernelPowerSourcesDidChange(kInternalBattery);
    }

    ERROR_LOG("Battery polling is now %s\n", noPoll ? "disabled." : "enabled. Initiating a battery poll.");
}

__private_extern__ void batteryTimeRemaining_setCustomBatteryProps(CFDictionaryRef batteryProps)
{
    if (!batteryProps) {
        return;
    }

    dispatch_barrier_sync(batteryTimeRemainingQ, ^() {
        if (customBatteryProps) {
            CFRelease(customBatteryProps);
        }

        INFO_LOG("System updated to use custom battery properties\n");

        BatterySetNoPoll(true);
        customBatteryProps = CFRetain(batteryProps);
        BatteryTimeRemaining_finishSync();
    });
}

__private_extern__ void batteryTimeRemaining_resetCustomBatteryProps(void)
{
    if (!customBatteryProps) {
        return;
    }

    dispatch_async(batteryTimeRemainingQ, ^() {
        if (gBatterySerialNumber) {
            CFRelease(gBatterySerialNumber);
            gBatterySerialNumber = NULL;
        }
        CFRelease(customBatteryProps);
        customBatteryProps = NULL;

        BatterySetNoPoll(false);
        BatteryTimeRemaining_finishSync();
    });
}

__private_extern__ CFDictionaryRef batteryTimeRemaining_copyIOPMPowerSourceDictionary(void)
{
    __block CFDictionaryRef ret = NULL;

    _internal_dispatch_assert_queue_not(batteryTimeRemainingQ);

    dispatch_sync(batteryTimeRemainingQ, ^(void){
        IOPMBattery **b = _batteries();
        if (b && b[0] && isA_CFDictionary(b[0]->properties)) {
            ret = CFDictionaryCreateCopy(kCFAllocatorDefault, b[0]->properties);
        }
    });
    if (!ret) {
        ERROR_LOG("CFDictCreate returned nil");
    }
    return ret;
}

__private_extern__ CFStringRef batteryTimeRemaining_getBatterySerialNumber(void)
{
    return gBatterySerialNumber;
}

static void _batteryChanged(IOPMBattery *changed_battery)
{
    kern_return_t       kr;

    CFMutableDictionaryRef props = NULL;
    CFDictionaryRef adapter = NULL;
    CFBooleanRef externalConnected = kCFBooleanFalse;
    CFBooleanRef battInstalled = kCFBooleanFalse;
    bool newBattery = true;

    if (!changed_battery) {
        // This is unexpected; we're not tracking this battery
        return;
    }

    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    // Free the last set of properties
    if (changed_battery->properties) {
        CFRelease(changed_battery->properties);
        changed_battery->properties = NULL;
        newBattery = false;
    }
    if (isA_CFDictionary(customBatteryProps)) {
        props = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, customBatteryProps);
    }
    else {
        kr = IORegistryEntryCreateCFProperties(
                changed_battery->me,
                &props,
                kCFAllocatorDefault, 0);
        if (KERN_SUCCESS != kr) {
            goto exit;
        }
    }

    if (CFDictionaryGetValueIfPresent(props, CFSTR(kIOPMPSBatteryInstalledKey), (const void **)&battInstalled) &&
            (battInstalled == kCFBooleanTrue)) {

        // Skip battery if there isn't a certain set of minimum information.
        // We expect at least current- and max capacity to be reported from
        // every PS. Assume the PS isn't ready yet if those are missing.
        if (!CFDictionaryGetValue(props, CFSTR(kIOPMPSCurrentCapacityKey)) ||
            !CFDictionaryGetValue(props, CFSTR(kIOPMPSMaxCapacityKey))) {
            ERROR_LOG("Battery does not report SOC");
            CFRelease(props);
            props = NULL;
            goto exit;
        }

        _unpackBatteryState(changed_battery, props);

        if (!gBatterySerialNumber && isA_CFString(changed_battery->batterySerialNumber)) {
            gBatterySerialNumber = changed_battery->batterySerialNumber;
            CFRetain(gBatterySerialNumber);

        }

        // Override cached serial number to aid tests. Don't care duplicate operation for debug paths.
        if (customBatteryProps) {
            if (gBatterySerialNumber) {
                CFRelease(gBatterySerialNumber);
            }
            gBatterySerialNumber = changed_battery->batterySerialNumber;
            if (changed_battery->batterySerialNumber) {
                CFRetain(gBatterySerialNumber);
            }
        }

        if (newBattery) {
            initializeBatteryCalculations();
        }
    }
    else {
        // There is no battery here. May be just an adapeter
        INFO_LOG("Battery is not installed\n");
        CFDictionaryGetValueIfPresent(props, CFSTR(kIOPMPSExternalConnectedKey), (const void **)&externalConnected);
        CFDictionaryGetValueIfPresent(props, CFSTR(kIOPMPSAdapterDetailsKey), (const void **)&adapter);

        if (isA_CFDictionary(adapter)) {
            readAndPublishACAdapter((externalConnected == kCFBooleanTrue) ? true : false, adapter);
        }
        CFRelease(props);
        props = NULL;
    }

exit:
    changed_battery->properties = props;
    return;
}

static void ioregBatteryProcess(IOPMBattery *changed_batt, io_service_t batt)
{
    if (!changed_batt) {
        return;
    }

    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

#if !TARGET_OS_IPHONE
    PowerSources oldPS = _getPowerSourceSync();
#endif

    // Update the arbiter
    changed_batt->me = (io_registry_entry_t)batt;
    _batteryChanged(changed_batt);

    if (changed_batt->properties == NULL) {
        // Nothing to do
        return;
    }

    LogObjectRetainCount("PM:BatteryInterest(B0) msg_port", changed_batt->msg_port);
    LogObjectRetainCount("PM:BatteryInterest(B1) msg_port", changed_batt->me);

    kernelPowerSourcesDidChange(changed_batt);

    SystemLoadBatteriesHaveChanged(_batteryCountSync());
    InternalEvaluateAssertions();
    InternalEvalConnections();
#if !TARGET_OS_IPHONE
    PowerSources newPS = _getPowerSourceSync();
    if (newPS != oldPS) {
        evalTcpkaForPSChange(newPS);
#if !(TARGET_OS_OSX && TARGET_CPU_ARM64)
        evalProxForPSChange();
// only evaluate clamshell sleep state when power source changes
#else
        INFO_LOG("EvaluateClamshellSleepState on power source change");
        evaluateClamshellSleepState();
#endif
    }
#endif

    return;
}

static void ioregBatteryInterest(
    void *refcon,
    io_service_t batt,
    natural_t messageType,
    void *messageArgument)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if (kIOPMMessageBatteryStatusHasChanged != messageType) {
        return;
    }

    IOPMBattery *changed_batt = (IOPMBattery *)refcon;

    ioregBatteryProcess(changed_batt, batt);
}

__private_extern__ IOPMBattery **_batteries(void)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    return physicalBatteriesArray;
}

static IOPMBattery *_newBatteryFound(io_registry_entry_t where)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    IOPMBattery *new_battery = NULL;
    static int new_battery_index = 0;
    // Populate new battery in array
    new_battery = calloc(1, sizeof(IOPMBattery));
    new_battery->me = where;
    new_battery->name = CFStringCreateWithFormat(
                            kCFAllocatorDefault,
                            NULL,
                            CFSTR("InternalBattery-%d"),
                            new_battery_index);
    static CFMutableSetRef physicalBatteriesSet;

    new_battery_index++;

    /* Real, physical battery found */
    if (!physicalBatteriesSet) {
        physicalBatteriesSet = CFSetCreateMutable(0, 1, NULL);
    }
    CFSetAddValue(physicalBatteriesSet, new_battery);
    physicalBatteriesCount = CFSetGetCount(physicalBatteriesSet);
    if (physicalBatteriesArray) {
        free(physicalBatteriesArray);
        physicalBatteriesArray = NULL;
    }
    physicalBatteriesArray = (IOPMBattery **)calloc(physicalBatteriesCount, sizeof(IOPMBattery *));
    CFSetGetValues(physicalBatteriesSet, (const void **)physicalBatteriesArray);

    return new_battery;
}

static void ioregBatteryMatch(
    void *refcon,
    io_iterator_t b_iter)
{
    IOPMBattery                 *tracking;
    IONotificationPortRef       notify = (IONotificationPortRef)refcon;
    io_registry_entry_t         battery;
    io_object_t                 notification_ref;

    _internal_dispatch_assert_queue(batteryTimeRemainingQ);

    while ((battery = (io_registry_entry_t)IOIteratorNext(b_iter))) {
        // Add battery to our list of batteries
        tracking = _newBatteryFound(battery);

        LogObjectRetainCount("PM::BatteryMatch(M0) me", battery);

        // And install an interest notification on it
        IOServiceAddInterestNotification(notify, battery,
                            kIOGeneralInterest, ioregBatteryInterest,
                            (void *)tracking, &notification_ref);

        LogObjectRetainCount("PM::BatteryMatch(M1) me", battery);
        LogObjectRetainCount("PM::BatteryMatch(M1) msg_port", notification_ref);

        tracking->msg_port = notification_ref;
        IOObjectRelease(battery);
    }
    InternalEvaluateAssertions();
    InternalEvalConnections();
#if !TARGET_OS_IPHONE
    evalTcpkaForPSChange(_getPowerSourceSync());
#endif
}

static void initNotifictions(void)
{
    IONotificationPortRef notify = IONotificationPortCreate(0);
    if (!notify) {
        return;
    }

    IONotificationPortSetDispatchQueue(notify, batteryTimeRemainingQ);

    io_iterator_t battery_iter = 0;
    kern_return_t kr = IOServiceAddMatchingNotification(
                                notify,
                                kIOFirstMatchNotification,
                                IOServiceMatching("IOPMPowerSource"),
                                ioregBatteryMatch,
                                (void *)notify,
                                &battery_iter);

    if (kr == KERN_SUCCESS) {
        // Install notifications on existing instances.
        ioregBatteryMatch((void *)notify, battery_iter);
    }

}

__private_extern__ void BatteryTimeRemaining_prime(void)
{

    battery_log = os_log_create(PM_LOG_SYSTEM, BATTERY_LOG);
    bzero(gPSList, sizeof(gPSList));
    bzero(&control, sizeof(BatteryControl));

     batteryTimeRemainingQ = dispatch_queue_create("com.apple.private.powerd.batteryTimeRemainingQ", DISPATCH_QUEUE_SERIAL);
     if (!batteryTimeRemainingQ) {
         return;
     }

    notify_register_check(kIOPSTimeRemainingNotificationKey,
                          &control.psTimeRemainingNotifyToken);
    notify_register_check(kIOPSNotifyPercentChange,
                          &control.psPercentChangeNotifyToken);

     // Initialize tracing battery events to FDR
     btr_recordFDREvent(kFDRInit, false);

     dispatch_sync(batteryTimeRemainingQ, ^() {
         initNotifictions();

#if TARGET_OS_IOS || TARGET_OS_WATCH
        /* kick off timer to collect battery data */
        batteryDataUpdateTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, batteryTimeRemainingQ);
        dispatch_source_set_timer(batteryDataUpdateTimer, dispatch_walltime(NULL, 0), kBatteryDataSaveInterval * NSEC_PER_SEC, 0);
        dispatch_source_set_event_handler(batteryDataUpdateTimer, ^{
            IOPMBattery **b = _batteries();
            if (b && b[0] && isA_CFDictionary(b[0]->properties)) {
                updateBatteryData(b[0]->properties);
            }
        });
# if !XCTEST
        dispatch_resume(batteryDataUpdateTimer);
# endif
#endif // TARGET_OS_IOS || TARGET_OS_WATCH
     });

    /* Do initial full poll and kick of the polling timer */
    dispatch_async(batteryTimeRemainingQ, ^() {
            startBatteryPoll(kImmediateFullPoll);
    });

    return;
}

static void BatteryTimeRemaining_finishSync(void)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    /* don't wait for notification if we already have battery info */
    IOPMBattery **b = _batteries();
    io_iterator_t       iter = MACH_PORT_NULL;
    kern_return_t       kr;


    if (!b || (b[0] == NULL)) {
        // No batteries found yet.
        return;
    }

    kr = IOServiceGetMatchingServices(kIOMasterPortDefault,
            IOServiceMatching("IOPMPowerSource"), &iter);
    if ((kIOReturnSuccess != kr) || (MACH_PORT_NULL == iter)) {
        ERROR_LOG("Failed to find IOPMPowerSource object\n")
    }
    else {
        dispatch_async(batteryTimeRemainingQ, ^() {
            io_registry_entry_t next;

            if ((next = IOIteratorNext(iter))) {
                ioregBatteryProcess(b[0], next);
                IOObjectRelease(next);
            }
            IOObjectRelease(iter);
        });
    }
}

__private_extern__ void BatteryTimeRemaining_finish(void)
{
    dispatch_async(batteryTimeRemainingQ, ^() {
        BatteryTimeRemaining_finishSync();
    });
}

/*
 * A battery time remaining discontinuity has occurred
 * Make sure we don't publish a time remaining estimate at all
 * until a given period has elapsed.
 */
static void _discontinuityOccurred(bool isWake)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if (slew) {
        bzero(slew, sizeof(SlewStruct));
    }
    control.lastDiscontinuity = CFAbsoluteTimeGetCurrent();
    control.percentageDiscontinuity = true;

#if TARGET_OS_WATCH
    if (isWake) {
        // Delay work on wake due to interaction with display (rdar://63540905)
        usleep(40000);  // 40 ms
    }
#endif // TARGET_OS_WATCH

    // Kick off a battery poll now,
    // and schedule the next poll in exactly 60 seconds.
    startBatteryPoll(kImmediateFullPoll);
}

__private_extern__ void
BatteryTimeRemainingWakeNotification(void)
{
    dispatch_barrier_async(batteryTimeRemainingQ, ^() {
        control.warningsShouldResetForSleep = true;
        control.readACAdapterAgain = true;

        _discontinuityOccurred(true);
    });
}

static CFAbsoluteTime getASBMPropertyCFAbsoluteTime(CFStringRef key)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);

    CFNumberRef     secSince1970 = NULL;
    IOPMBattery     **b = _batteries();
    uint32_t        secs = 0;
    CFAbsoluteTime  return_val = 0.0;
    if (b && b[0] && b[0]->properties) {
        secSince1970 = CFDictionaryGetValue(b[0]->properties, key);
        // the numbers in the registry are secs since start of epoch which is 1st Jan 1970
        if (secSince1970) {
            CFNumberGetValue(secSince1970, kCFNumberIntType, &secs);
            // this is the seconds since 1st Jan 2001
            return_val = (CFAbsoluteTime)secs - kCFAbsoluteTimeIntervalSince1970;
        } else {
            return_val = -kCFAbsoluteTimeIntervalSince1970;
        }
    }

    return return_val;
}

static CFTimeInterval mostRecent(CFTimeInterval a, CFTimeInterval b, CFTimeInterval c)
{
    if ((a >= b) && (a >= c) && a!= 0.0) {
        return a;
    } else if ((b >= a) && (b>= c) && b!= 0.0) {
        return b;
    } else return c;
}

static dispatch_source_t batteryPollingTimer = NULL;

#ifndef kBootPathKey
#define kBootPathKey             "BootPathUpdated"
#define kFullPathKey             "FullPathUpdated"
#define kUserVisPathKey          "UserVisiblePathUpdated"
#endif

static bool startBatteryPoll(PollCommand doCommand)
{
    const static CFTimeInterval     kFullMinFrequency = 595.0;
#if TARGET_OS_IPHONE
    const static CFTimeInterval     kUserVisibleMinFrequency = 15.0;
    const static uint64_t           kPollIntervalNS = 20ULL * NSEC_PER_SEC;
#else
    const static CFTimeInterval     kUserVisibleMinFrequency = 55.0;
    const static uint64_t           kPollIntervalNS = 60ULL * NSEC_PER_SEC;
#endif

    CFAbsoluteTime                  lastBootUpdate = 0.0;
    CFAbsoluteTime                  lastUserVisibleUpdate = 0.0;
    CFAbsoluteTime                  lastFullUpdate = 0.0;
    CFAbsoluteTime                  now = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime                  lastUpdateTime;
    CFTimeInterval                  sinceUserVisible = 0.0;
    CFTimeInterval                  sinceFull = 0.0;
    bool                            doUserVisible = false;
    bool                            doFull = false;

    _internal_dispatch_assert_queue(batteryTimeRemainingQ);

    if (!_batteries()) {
        return false;
    }

    if (control.noPoll) {
        ERROR_LOG("Battery polling is disabled. powerd is skipping this battery udpate request.");
        return false;
    }

    if (!batteryPollingTimer) {
        batteryPollingTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, batteryTimeRemainingQ);
        dispatch_source_set_event_handler(batteryPollingTimer, ^() { startBatteryPoll(kPeriodicPoll); });
        dispatch_resume(batteryPollingTimer);
    }

    if (kImmediateFullPoll == doCommand) {
        doFull = true;
    } else {

        lastUpdateTime = getASBMPropertyCFAbsoluteTime(CFSTR(kBootPathKey));
        if (lastUpdateTime < now) {
            lastBootUpdate = lastUpdateTime;
        };

        lastUpdateTime = getASBMPropertyCFAbsoluteTime(CFSTR(kFullPathKey));
        if (lastUpdateTime < now) {
            lastFullUpdate = lastUpdateTime;
        }

        lastUpdateTime = getASBMPropertyCFAbsoluteTime(CFSTR(kUserVisPathKey));
        if (lastUpdateTime < now) {
            lastUserVisibleUpdate = lastUpdateTime;
        }

        sinceUserVisible = now - mostRecent(lastBootUpdate, lastFullUpdate, lastUserVisibleUpdate);
        if (sinceUserVisible > kUserVisibleMinFrequency) {
            doUserVisible = true;
        }

        sinceFull = now - mostRecent(lastBootUpdate, lastFullUpdate, -kCFAbsoluteTimeIntervalSince1970);
        if (sinceFull > kFullMinFrequency) {
            doFull = true;
        }
    }

    if (doFull) {
        IOPSRequestBatteryUpdate(kIOPSReadAll);
        dispatch_source_set_timer(batteryPollingTimer, dispatch_time(DISPATCH_TIME_NOW, kPollIntervalNS), kPollIntervalNS, 0);
    } else if (doUserVisible) {
        IOPSRequestBatteryUpdate(kIOPSReadUserVisible);
        dispatch_source_set_timer(batteryPollingTimer, dispatch_time(DISPATCH_TIME_NOW, kPollIntervalNS), kPollIntervalNS, 0);
    } else {
        uint64_t checkAgainNS = kPollIntervalNS - (sinceUserVisible*NSEC_PER_SEC);

        if (checkAgainNS > kPollIntervalNS) {
            checkAgainNS = kPollIntervalNS;
        }

        dispatch_source_set_timer(batteryPollingTimer, dispatch_time(DISPATCH_TIME_NOW, checkAgainNS), kPollIntervalNS, 0);
    }
    return true;
}

#define kTimeThresholdEarly          20
#define kTimeThresholdFinal          10
#define kPercentThresholdFinal       5

static void publish_IOPSBatteryGetWarningLevel(
    IOPMBattery *b,
    int combinedTime,
    int percent)
{
    /* Display a system low battery warning?
     *
     * No Warning == AC Power or >= 20 minutes battery remaining
     * Early Warning == On Battery with < 20 minutes
     * Final Warning == On Battery with < 10 Minutes || SOC < 5%
     */

    static CFStringRef lowBatteryKey = NULL;
    static int prevLoggedLevel = kIOPSLowBatteryWarningNone;
    int newWarningLevel = kIOPSLowBatteryWarningNone;

    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if (control.warningsShouldResetForSleep || b->externalConnected) {
        // We reset the warning level upon system sleep or when external
        // power source is connected

        control.warningsShouldResetForSleep = false;
        if (control.systemWarningLevel != kIOPSLowBatteryWarningNone) {
            control.systemWarningLevel = 0;
            newWarningLevel = kIOPSLowBatteryWarningNone;
        }
    } else if (!control.percentageDiscontinuity) {
#if !TARGET_OS_IPHONE
        if (percent <= kPercentThresholdFinal) {
            newWarningLevel = kIOPSLowBatteryWarningFinal;
        } else
#endif
        if (combinedTime > 0) {
            if (combinedTime < kTimeThresholdFinal) {
                newWarningLevel = kIOPSLowBatteryWarningFinal;
            } else if (combinedTime < kTimeThresholdEarly) {
                newWarningLevel = kIOPSLowBatteryWarningEarly;
            }
        }
    }

    if (newWarningLevel < control.systemWarningLevel) {
        // kIOPSLowBatteryWarningNone  = 1,
        // kIOPSLowBatteryWarningEarly = 2,
        // kIOPSLowBatteryWarningFinal = 3
        //
        // Warning level may only increase.
        // Once we enter a >1 warning level, we can only reset it by
        // (1) having AC power re-applied, or (2) hibernating
        // and waking with a new battery.
        //
        // This prevents fluctuations in battery capacity from causing
        // multiple battery warnings.

        newWarningLevel = control.systemWarningLevel;
    }

    if ( (newWarningLevel != control.systemWarningLevel)
        && (0 != newWarningLevel) ) {
        CFNumberRef newlevel = CFNumberCreate(0, kCFNumberIntType, &newWarningLevel);

        if (newlevel) {
            if (!lowBatteryKey) {
                lowBatteryKey = SCDynamicStoreKeyCreate(
                        kCFAllocatorDefault, CFSTR("%@%@"),
                        kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStoreLowBattPathKey));
            }

            PMStoreSetValue(lowBatteryKey, newlevel );
            CFRelease(newlevel);

            BatteryTimeRemaining_notify_post(kIOPSNotifyLowBattery);
            if (newWarningLevel != prevLoggedLevel) {
#if TARGET_OS_IPHONE
                INFO_LOG("Warning level: %d time: %d cap: %d\n", newWarningLevel, combinedTime, percent);
#endif
                logASLLowBatteryWarning(newWarningLevel, combinedTime, b->currentCap);
                prevLoggedLevel = newWarningLevel;
            }
        }
        control.systemWarningLevel = newWarningLevel;
    }

    return;
}

static CFDictionaryRef getActiveBatteryDictionary(void)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    for (int i=0; i<kPSMaxCount; i++) {
        if (!gPSList[i].description) {
            continue;
        }

        CFStringRef transport_type = NULL;
        transport_type = CFDictionaryGetValue(gPSList[i].description,
                                              CFSTR(kIOPSTransportTypeKey));
        if (isA_CFString(transport_type)
            && ( CFEqual(transport_type, CFSTR(kIOPSInternalType)))) {
            return gPSList[i].description;
        }
    }
    return NULL;
}

static bool publish_IOPSGetTimeRemainingEstimate(
    int timeRemaining,
    bool external,
    bool rawExternal,
    bool timeRemainingUnknown,
    bool isCharging,
    bool showChargingUI,
    bool playChargingChime,
    bool noPoll,
    bool adapterUnsupported)
{
    uint64_t            powerSourcesBitsForNotify = (uint64_t)(timeRemaining & 0xFFFF);
    static uint64_t     lastPSBitsNotify = 0;
    bool                posted = false;
    uint32_t            rc;

    _internal_dispatch_assert_queue(batteryTimeRemainingQ);

    // Presence of bit kPSTimeRemainingNotifyValidBit means IOPSGetTimeRemainingEstimate
    // should trust this as a valid chunk of battery data.
    powerSourcesBitsForNotify |= kPSTimeRemainingNotifyValidBit;

    if (external) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyExternalBit;
    }
#if TARGET_OS_IPHONE
    if (rawExternal) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyRawExternalBit;
    }
    if (showChargingUI) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyShowChargingUIBit;
    }
    if (playChargingChime) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyPlayChargingChimeBit;
    }
#endif // TARGET_OS_IPHONE
    if (timeRemainingUnknown) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyUnknownBit;
    }
    if (isCharging) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyChargingBit;
    }
    if (control.noPoll) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyNoPollBit;
    }
    if (adapterUnsupported) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyAdapterUnsupported;
    }

    /* These bits feed the SPI IOKit:IOPSGetSupportedPowerSources()
     *      - battery supported, UPS supported, active power sourecs
     */
    if (getActiveBatteryDictionary()) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyBattSupportBit;
    }
    if (getActiveUPSDictionary_sync()) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyUPSSupportBit;
    }
    uint64_t activePS = getActivePSType_sync();
    powerSourcesBitsForNotify |=
    (activePS & 0xFF) << kPSTimeRemainingNotifyActivePS8BitsStarts;

    if (lastPSBitsNotify != powerSourcesBitsForNotify) {
        lastPSBitsNotify = powerSourcesBitsForNotify;
        notify_set_state(control.psTimeRemainingNotifyToken, powerSourcesBitsForNotify);
        rc = notify_post(kIOPSNotifyTimeRemaining);
        if (rc != NOTIFY_STATUS_OK) {
            ERROR_LOG("Failed to post notification for time remaining. rc:0x%x\n", rc);
        }
        else {
            INFO_LOG("Battery time remaining posted(0x%llx) Time:%d Source:%{public}s\n",
                    powerSourcesBitsForNotify, timeRemaining, external ? "AC":"Batt");
        }
        posted = true;
    }

    return posted;
}

static void batteryTimeRemaining_setPercentRemaining(int val)
{
    currentPercentRemaining = val;
}

__private_extern__ int batteryTimeRemaining_getPercentRemaining(void)
{
    return currentPercentRemaining;
}

static void publish_IOPSGetPercentRemaining(
    int         percentRemaining,
    bool        isExternal,
    bool        isCharging,
    bool        fullyCharged,
    IOPMBattery *b)
{
    uint64_t            currentStateBits, changedStateBits;
    static uint64_t     lastStateBits = 0;
    uint64_t            ignoreBits;

    _internal_dispatch_assert_queue(batteryTimeRemainingQ);

    // Presence of bit kPSTimeRemainingNotifyValidBit means IOPSGetPercentRemaining
    // should trust this as a valid chunk of battery data.
    currentStateBits = kPSTimeRemainingNotifyValidBit;

    if ((percentRemaining >= 0) && (percentRemaining <= 100))
        currentStateBits |= percentRemaining;
    if (isExternal)
        currentStateBits |= kPSTimeRemainingNotifyExternalBit;
    if (isCharging)
        currentStateBits |= kPSTimeRemainingNotifyChargingBit;
    if (fullyCharged)
        currentStateBits |= kPSTimeRemainingNotifyFullyChargedBit;
#if TARGET_OS_IPHONE
    if (b && b->isCritical)
        currentStateBits |= kPSCriticalLevelBit;
    if (b && b->isRestricted)
        currentStateBits |= kPSRestrictedLevelBit;
#endif // TARGET_OS_IPHONE

    changedStateBits = lastStateBits ^ currentStateBits;
    if (changedStateBits)
    {
        lastStateBits = currentStateBits;
        notify_set_state(control.psPercentChangeNotifyToken, currentStateBits);

        // Suppress notification for charging state changes
        ignoreBits = (kPSTimeRemainingNotifyChargingBit 
                                 |kPSTimeRemainingNotifyFullyChargedBit
#if TARGET_OS_IPHONE
                                 |kPSCriticalLevelBit
                                 |kPSRestrictedLevelBit
#endif // TARGET_OS_IPHONE
                                 );
        if (changedStateBits & ~ignoreBits)
        {
            notify_post(kIOPSNotifyPercentChange);
            INFO_LOG("Battery capacity change posted(0x%llx). Capacity:%d Source:%{public}s\n",
                    currentStateBits, percentRemaining, isExternal ? "AC":"Batt");
        }
#if TARGET_OS_IPHONE
        if (changedStateBits & kPSCriticalLevelBit)
            BatteryTimeRemaining_notify_post(kIOPSNotifyCriticalLevel);
        if (changedStateBits & kPSRestrictedLevelBit)
            BatteryTimeRemaining_notify_post(kIOPSNotifyRestrictedMode);
#endif // TARGET_OS_IPHONE
    }
}

__private_extern__ void kernelPowerSourcesDidChange(IOPMBattery *b)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    static int  _lastExternalConnected = -1;
    static int  _lastPercentRemaining = 100;
    int         _nowExternalConnected = 0;
    int         percentRemaining = 0;
    IOPMBattery **_batts = _batteries();

    /*
     * Initiate the next battery poll; or start a timer to poll
     * when the 60sec user visible polling timer expres.
     */
    startBatteryPoll(kPeriodicPoll);

    if (0 == _batteryCountSync()) {
        return;
    }

    if (!b) {
        b = _batts[0];
    }
    if ( !b || (b->properties == NULL)) {
        INFO_LOG("No batteries found yet..\n");
        return;
    }

    _nowExternalConnected = (b->externalConnected ? 1 : 0) | (b->rawExternalConnected ? 1 : 0);
    if (_lastExternalConnected != _nowExternalConnected) {
        // If AC has changed, we must invalidate time remaining.
        _discontinuityOccurred(false);
        control.needsNotifyAC = true;
    }

    readAndPublishACAdapter(b->externalConnected,
                             CFDictionaryGetValue(b->properties, CFSTR(kIOPMPSAdapterDetailsKey)));


    checkTimeRemainingValid(_batts);

    if (b->maxCap) {
        double percent = (double)(b->currentCap * 100) / (double)b->maxCap;
        percentRemaining = (int) lround(percent);
        if (percentRemaining > 100)
            percentRemaining = 100;
    }

    // never show 0 to the user
    if (percentRemaining < 1) {
        percentRemaining = 1;
    }

    // prevent percentage to increase when discharging
    if (!control.percentageDiscontinuity && !_lastExternalConnected &&
        !_nowExternalConnected && (percentRemaining > _lastPercentRemaining)) {
        percentRemaining = _lastPercentRemaining;
    } else if (CFAbsoluteTimeGetCurrent() >= (control.lastDiscontinuity + kDiscontinuitySettle)) {
        control.percentageDiscontinuity = false;
    }

    // b->swCalculatedPR is used by packageKernelPowerSource()
    b->swCalculatedPR = percentRemaining;
    _lastPercentRemaining = percentRemaining;
    _lastExternalConnected = _nowExternalConnected;

    /************************************************************************
     *
     * PUBLISH: SCDynamicStoreSetValue / IOPSCopyPowerSourcesInfo()
     *
     ************************************************************************/
    if (control.internal) {
        CFDictionaryRef update = packageKernelPowerSource(b, control.internal);
        if (control.internal->description) {
            CFRelease(control.internal->description);
        }
        control.internal->description = update;
    }

    HandlePublishAllPowerSources();
}

static void HandlePublishAllPowerSources(void)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    IOPMBattery               **batteries = _batteries();
    IOPMBattery                *b = NULL;
    int                         combinedTime = 0;
    int                         percentRemaining = 0;
    int                         prev_percentRemaining = batteryTimeRemaining_getPercentRemaining();
    bool                        tr_posted;
    int                         ups_externalConnected = 0;
    static int                  ups_prevExternalConnected = -1;
    bool                        externalConnected, tr_unknown, is_charging, fully_charged;
    bool                        rawExternalConnected = false;
    bool                        showChargingUI = false;
    bool                        playChargingChime = false;
    bool                        adapterUnsupported = false;
    CFDictionaryRef             ups = NULL;
    int                         ups_tr = -1;
#if TARGET_OS_IPHONE
    CFDictionaryRef             battery_case = NULL;
    int                         battcase_percentRemaining = 0;
    static int                  battcase_prevCcap;
    static int                  battcase_prevMcap;
    int                         battcase_chargeState = -1;
    static int                  battcase_prevChargeState = -1;
#endif // TARGET_OS_IPHONE
    bool                        battcase_change = false;
    CFAbsoluteTime              bootUpdateTime = kCFAbsoluteTimeIntervalSince1970; // non-zero value

    ups = getActiveUPSDictionary_sync();

#if !TARGET_OS_IPHONE
    bootUpdateTime = getASBMPropertyCFAbsoluteTime(CFSTR(kFullPathKey));
#endif
    if (_batteryCountSync() && (batteries[0]->isPresent) && (bootUpdateTime != 0)) {
        b = batteries[0];
    }

    if ((b == NULL) && (ups == NULL)) {
        return;
    }
    is_charging = fully_charged = false;
    for (int i = 0; i < _batteryCountSync(); i++) {
        if (batteries[i]->isPresent) {
            combinedTime += batteries[i]->swCalculatedTR;
        }
    }

#if TARGET_OS_IPHONE
    battery_case = getPSByType(CFSTR(kIOPSPrivateBatteryCaseType));
    if (battery_case) {
        int battcase_ccap = 0, battcase_mcap = 0;
        CFNumberRef battcase_ccap_cf = CFDictionaryGetValue(battery_case, CFSTR(kIOPSCurrentCapacityKey));
        CFNumberRef battcase_mcap_cf = CFDictionaryGetValue(battery_case, CFSTR(kIOPSMaxCapacityKey));

        if (battcase_ccap_cf && battcase_mcap_cf) {
            CFNumberGetValue(battcase_ccap_cf, kCFNumberIntType, &battcase_ccap);
            CFNumberGetValue(battcase_mcap_cf, kCFNumberIntType, &battcase_mcap);
            if (battcase_mcap) {
                battcase_percentRemaining = (battcase_ccap * 100) / battcase_mcap;
            }
        }

        CFDictionaryGetValueIfPresent(battery_case, CFSTR(kIOPSIsChargingKey), (const void **)&battcase_chargeState);

        int battcase_prevPercentRemaining = 0;
        if (battcase_prevMcap) {
            battcase_prevPercentRemaining = 100 * battcase_prevCcap / battcase_prevMcap;
        }
        if ((battcase_chargeState != battcase_prevChargeState) ||
            (battcase_percentRemaining != battcase_prevPercentRemaining) ||
            (battcase_mcap && !battcase_prevMcap)) {
            battcase_change = true;
        }
        battcase_prevChargeState = battcase_chargeState;
        battcase_prevMcap = battcase_mcap;
        battcase_prevCcap = battcase_ccap;
    } else {
        battcase_prevChargeState = -1;
        battcase_prevMcap = 0;
        battcase_prevCcap = 0;
    }
#endif // TARGET_OS_IPHONE

    if (ups) {
        CFNumberRef num_cf = CFDictionaryGetValue(ups, CFSTR(kIOPSTimeToEmptyKey));
        if (num_cf) {
            CFNumberGetValue(num_cf, kCFNumberIntType, &ups_tr);
            if (ups_tr != -1) combinedTime += ups_tr;
        }

        CFStringRef src = CFDictionaryGetValue(ups, CFSTR(kIOPSPowerSourceStateKey));
        if (src && (CFStringCompare(src, CFSTR(kIOPSACPowerValue), kNilOptions) == kCFCompareEqualTo)) {
            ups_externalConnected = 1;
        }
        if (ups_prevExternalConnected != ups_externalConnected) {
            control.needsNotifyAC = true;
            ups_prevExternalConnected = ups_externalConnected;
        }
    }

    if (b) {
            tr_unknown = b->isTimeRemainingUnknown;
            is_charging = b->isCharging;
            percentRemaining = b->swCalculatedPR;
            fully_charged = isFullyCharged(b);

            if (ups) {
                externalConnected = b->externalConnected && ups_externalConnected;
            }
            else {
                externalConnected = b->externalConnected;
            }
            rawExternalConnected = b->rawExternalConnected;
            showChargingUI = b->showChargingUI;
            playChargingChime = b->playChargingChime;
            adapterUnsupported = b->adapterUnsupported;
    }
    else {
        int mcap = 0, ccap = 0;
        CFNumberRef mcap_cf = NULL, ccap_cf = NULL;

        /* ups must be non-NULL */
        externalConnected = ups_externalConnected;

        if (!externalConnected && (ups_tr == -1)) {
            tr_unknown = false;
        }
        else {
            tr_unknown = true;
        }

        if (CFDictionaryGetValue(ups, CFSTR(kIOPSIsChargingKey)) == kCFBooleanTrue)
            is_charging = true;

        ccap_cf = CFDictionaryGetValue(ups, CFSTR(kIOPSCurrentCapacityKey));
        if (ccap_cf)
            CFNumberGetValue(ccap_cf, kCFNumberIntType, &ccap);

        mcap_cf = CFDictionaryGetValue(ups, CFSTR(kIOPSMaxCapacityKey));
        if (mcap_cf)
            CFNumberGetValue(mcap_cf, kCFNumberIntType, &mcap);

        if (ccap && mcap)
            percentRemaining = (ccap*100)/mcap;

        if ((percentRemaining >= 95) && externalConnected && (!is_charging))
            fully_charged = true;
    }

    tr_posted = publish_IOPSGetTimeRemainingEstimate(combinedTime,
                                         externalConnected,
                                         rawExternalConnected,
                                         tr_unknown,
                                         is_charging,
                                         showChargingUI,
                                         playChargingChime,
                                         control.noPoll,
                                         adapterUnsupported);

    if (b) {
        publish_IOPSBatteryGetWarningLevel(b, combinedTime, percentRemaining);
    }

    publish_IOPSGetPercentRemaining(percentRemaining,
                                    externalConnected,
                                    is_charging,
                                    fully_charged,
                                    b);

    if (((percentRemaining != prev_percentRemaining) || battcase_change) && !tr_posted) {
        BatteryTimeRemaining_notify_post(kIOPSNotifyTimeRemaining);
    }

    batteryTimeRemaining_setPercentRemaining(percentRemaining);

    /************************************************************************
     *
     * TELL: powerd-internal code that responds to power changes
     ************************************************************************/

#if !TARGET_OS_IPHONE
     // Notifiy PSLowPower of power sources change
    UPSLowPowerPSChange();
    PMSettingsPSChange();
#endif // !TARGET_OS_IPHONE

    /************************************************************************
     *
     * NOTIFY: Providing power source changed.
     *          via notify(3)
     ************************************************************************/
    if (control.needsNotifyAC) {
        control.needsNotifyAC = false;

        btr_recordFDREvent(kFDRACChanged, false);

        INFO_LOG("Power Source change. Source:%{public}s", externalConnected ? "AC" : "Batt");
        notify_post(kIOPSNotifyPowerSource);
    }

    notify_post(kIOPSNotifyAnyPowerSource);

    /************************************************************************
     *
     * PUBLISH: Flight Data Recorder trace
     *
     ************************************************************************/
    btr_recordFDREvent(kFDRBattEventPeriodic, false);

    return;
}


/* checkTimeRemainingValid
 * Implicit inputs: battery state; battery's own time remaining estimate
 * Implicit output: estimated time remaining placed in b->swCalculatedTR; or -1 if indeterminate
 *   returns 1 if we reached a valid estimate
 *   returns 0 if we're still calculating
 */
static void checkTimeRemainingValid(IOPMBattery **batts)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    int             i;
    IOPMBattery     *b;
    int             batCount = _batteryCountSync();

    for (i = 0; i < batCount; i++) {
        b = batts[i];
        // Did our calculation come out negative?
        // The average current must still be out of whack!
        if ((b->swCalculatedTR < 0) || (false == b->isPresent)) {
            b->swCalculatedTR = -1;
        }

        // Cap all times remaining to 10 hours. We don't ship any
        // 44 hour batteries just yet.
        if (kMaxBattMinutes < b->swCalculatedTR) {
            b->swCalculatedTR = kMaxBattMinutes;
        }
    }

    if (-1 == batts[0]->swCalculatedTR) {
        batts[0]->isTimeRemainingUnknown = true;
    } else {
        batts[0]->isTimeRemainingUnknown = false;
    }

}

#if TARGET_OS_IOS || POWERD_IOS_XCTEST || TARGET_OS_WATCH || TARGET_OS_OSX
//
// updateBatteryServiceState - Updates Battery Health service state data in 'bhData' with new state based on
// Service Flags(svcFlags) passed to the function
//
void updateBatteryServiceState(CFDictionaryRef battProps, CFMutableDictionaryRef bhData, IOPSBatteryHealthServiceFlags svcFlags)
{
    IOPSBatteryHealthServiceFlags prevSvcFlags = 0;
    IOPSBatteryHealthServiceState svcState, prevSvcState;

    prevSvcState = kBHSvcStateUnknown;
    prevSvcFlags = 0;
    svcState = kBHSvcStateNone;


    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey), prevSvcFlags);
    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey), prevSvcState);

    // Carry over the sticky bits of Service Flags
    svcFlags |= (prevSvcFlags & kBHSvcFlagStickyBits);

    if (svcFlags & kBHSvcFlagNoSerial) {
        svcState = kBHSvcStateUnknown;
    }
    else if (svcFlags & kBHSvcFlagNonDetBits) {
        // If any service condition couldn't be determined then change service state as NotDeterminable
        svcState = kBHSvcStateNotDeterminable;
        INFO_LOG("Unable to determine Battery Health Service state. Service Flags:0x%x Service State:%d\n", svcFlags, svcState);
    }
    else {
        // Set the service state based on the service flags
        bool ppc = false;
        if (batteryHealthUPOAware == kBatteryHealthUsesUPO) {
            ppc = ((svcFlags & (kBHSvcFlagUPOPrime|kBHSvcFlagWRa)) == (kBHSvcFlagUPOPrime|kBHSvcFlagWRa));
        }
        else {
            ppc = (svcFlags & kBHSvcFlagWRa) ? true : false;
        }

        // A permanent fault reported by the gauge should take precedence over anything else
        if (svcFlags & kBHSvcFlagPermanentFault) {
            svcState = kBHSvcStatePermanentFault;
        } else if (svcFlags & kBHSvcFlagBCDC) {
            svcState = kBHSvcStateBCDC;
        }
        else if ((svcFlags & (kBHSvcFlagNCC)) && ppc) {
            svcState = kBHSvcStateNominalChargeAndPeakPower;
        }
        else if (ppc) {
            svcState = kBHSvcStatePeakPowerCapacity;
        }
        else if (svcFlags & (kBHSvcFlagNCC)) {
            svcState = kBHSvcStateNominalChargeCapacity;
        }
        else if (svcFlags & kBHSvcFlagRBATT) {
            svcState = kBHSvcStateRBATT;
        }
        else if ((prevSvcState == kBHSvcStateUnknown) || (prevSvcState == kBHSvcStateNotDeterminable)) {
            svcState = kBHSvcStateNone;
        }
    }

    CFDictionarySetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey), svcFlags);
    CFDictionarySetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey), svcState);
}

#if !POWERD_IOS_XCTEST
__private_extern__ void setBHUpdateTimeDelta(xpc_object_t remoteConnection, xpc_object_t msg)
{
    int64_t timeDelta = 0;

    if (!msg) {
        ERROR_LOG("Invalid message\n");
        return;
    }
    xpc_object_t respMsg = xpc_dictionary_create_reply(msg);
    if (respMsg == NULL) {
        ERROR_LOG("Failed to create response message\n");
        return;
    }

    if (!isSenderEntitled(remoteConnection, CFSTR("com.apple.private.iokit.batteryTester"), true)) {
        ERROR_LOG("Ignoring custom battery properties message from unprivileged sender\n");
        xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnNotPrivileged);
        goto exit;
    }

    timeDelta = xpc_dictionary_get_int64(msg, kSetBHUpdateTimeDelta);
    if ((timeDelta <= 0) || (timeDelta > UINT32_MAX)) {
        ERROR_LOG("Received invalid time delta %lld\n", timeDelta);
        xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnBadArgument);
        goto exit;
    }
    battReadTimeDelta = (uint32_t)timeDelta;

    INFO_LOG("Changed NCC update time delta to %u\n", battReadTimeDelta);
    xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnSuccess);

exit:
    xpc_connection_send_message(remoteConnection, respMsg);
    xpc_release(respMsg);
}
#endif // !POWERD_IOS_XCTEST
#endif // TARGET_OS_IOS || POWERD_IOS_XCTEST || TARGET_OS_WATCH || TARGET_OS_OSX

#if TARGET_OS_IOS || POWERD_IOS_XCTEST || TARGET_OS_WATCH
#if POWERD_IOS_XCTEST
// XCTest helper functions
void setBatteryHealthP0Threshold(uint64_t p0)
{
    batteryHealthP0Threshold = p0;
}
void setSmcBasedDevice(bool enable)
{
    smcBasedDevice = enable;
}
void setBatteryUPOAwareness(bool awareness)
{
    batteryHealthUPOAware = awareness;
}
void setNCCFilteringState(bool enable)
{
    nccp_cc_filtering = enable;
}
#endif // POWERD_IOS_XCTEST

// migrateSvcFlags - This function migrates powerlog's version of
// service flags(version 0 & 1) to version 2, as managed by powerd.
static uint32_t migrateSvcFlags(IOPSBatteryHealthServiceState oldSvcState, IOPSBatteryHealthServiceFlags oldSvcFlags)
{
    IOPSBatteryHealthServiceFlags newFlags = kBatteryHealthCurrentVersion;

    if ((oldSvcFlags & kBHSvcVersionMask) == kBHSvcFlagsVerison0) {
        // Version 0 doesn't have any flags set. Flags are set based
        // on the Service state
        switch (oldSvcState) {
            case kBHSvcStateUnknown:
                // State was previously unknown. Ignore setting flags for now
                // and set them after re-calculating the state.
                newFlags |= 0;
                break;

            case kBHSvcStateNone:
                newFlags |= 0;
                break;

            case kBHSvcStateNominalChargeCapacity:
                newFlags |= kBHSvcFlagNCC;
                break;

            case kBHSvcStatePeakPowerCapacity:
                newFlags |= kBHSvcFlagUPOPrime|kBHSvcFlagWRa;
                break;

            case kBHSvcStateNominalChargeAndPeakPower:
                newFlags |= kBHSvcFlagNCC|kBHSvcFlagUPOPrime|kBHSvcFlagWRa;
                break;

            case kBHSvcStateRBATT:
                newFlags |= kBHSvcStateRBATT;
                break;

            case kBHSvcStateNotDeterminable:
                // State was previously non-determinable. Ignore setting flags for now
                // and set them after re-calculating the state.
                newFlags |= 0;
                break;

            case kBHSvcStateBCDC:
                newFlags |= kBHSvcFlagBCDC;
                break;

            default:
                ERROR_LOG("Unexpected Service state %d\n", oldSvcState);
                break;
        }
    }
    else if ((oldSvcFlags & kBHSvcVersionMask) == kBHSvcFlagsVersion1) {
        // Remove version number, un-used flags and flags indicating non-determinable condition
        newFlags |= oldSvcFlags & (kBHSvcFlagRBATT|kBHSvcFlagUPOPrime|kBHSvcFlagNCC|kBHSvcFlagWRa|kBHSvcFlagBCDC);
    }
    else {
        ERROR_LOG("Powerlog Service Flags 0x%x with version 0x%x is unexpected\n",
                  oldSvcFlags, oldSvcFlags & kBHSvcVersionMask);
        // Reset flags to 0
        newFlags = kBatteryHealthCurrentVersion;
    }
    return newFlags;
}

#if !POWERD_IOS_XCTEST
static void saveBatteryHealthKeyValueToPrefs(const void *key, const void *value, void *context __unused)
{
    _CFPreferencesSetValueWithContainer(key, value, CFSTR(kBatteryHealthPrefsAppName), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, CFSTR(kBatteryHealthPrefsContainer));
}

void saveBatteryHealthDataToPrefs(CFDictionaryRef bhData)
{
    CFDictionaryApplyFunction(bhData, saveBatteryHealthKeyValueToPrefs, NULL);
    _CFPreferencesSynchronizeWithContainer(CFSTR(kBatteryHealthPrefsAppName), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, CFSTR(kBatteryHealthPrefsContainer));
}

void removeKeyFromBatteryHealthDataPrefs(CFStringRef key)
{
    saveBatteryHealthKeyValueToPrefs(key, NULL, NULL);
    _CFPreferencesSynchronizeWithContainer(CFSTR(kBatteryHealthPrefsAppName), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, CFSTR(kBatteryHealthPrefsContainer));
}

CFDictionaryRef copyBatteryHealthDataFromPrefs( )
{
    CFDictionaryRef dict = NULL;
    CFIndex count;
    dict = _CFPreferencesCopyMultipleWithContainer(NULL, CFSTR(kBatteryHealthPrefsAppName), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, CFSTR(kBatteryHealthPrefsContainer));
    if ((dict == NULL) || (CFDictionaryGetCount(dict) == 0)) {
        INFO_LOG("Failed to read battery health data from custom container location\n");
        if (dict) {
            CFRelease(dict);
            dict = NULL;
        }

        // Check for health data in the previous location
        dict = CFPreferencesCopyMultiple(NULL, CFSTR(kBatteryHealthPrefsAppName), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        if ((dict != NULL) && ((count = CFDictionaryGetCount(dict)) != 0)) {
            INFO_LOG("Battery data read from default prefs\n");
            // Save the data to new location and delete it from old location
            saveBatteryHealthDataToPrefs(dict);

            CFTypeRef *keys = malloc(sizeof(CFTypeRef) * count);
            if (keys != NULL) {
                CFDictionaryGetKeysAndValues(dict, (const void **)keys, NULL);
                for (int i = 0; i < count; i++) {
                    CFPreferencesSetValue(keys[i], NULL, CFSTR(kBatteryHealthPrefsAppName), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
                    INFO_LOG("Deleting key %@ from old store\n", keys[i]);
                }
                free(keys);
            }
            else {
                ERROR_LOG("Failed to allocate memory to delete battery data from default prefs\n");
            }
        }
        else {
            INFO_LOG("Failed to read battery data from default prefs\n");
        }
    }
    return dict;
}

CFDictionaryRef copyPowerlogBatteryHealthData()
{
    CFDictionaryRef oldBHData = NULL;
    container_error_t localError = CONTAINER_ERROR_NONE;
    CFStringRef containerPath = NULL;

    // Read powerlog's battery health data from its CFPrefs container
    const char *containerCStringPath = container_system_group_path_for_identifier(NULL, "systemgroup.com.apple.powerlog", &localError);
    if (containerCStringPath) {
        containerPath = CFStringCreateWithCString(NULL, containerCStringPath, kCFStringEncodingUTF8);
        free((void *)containerCStringPath);
    } else {
        ERROR_LOG("Error fetching group container systemgroup.com.apple.powerlog : %llu", localError);
    }

    if (containerPath) {
        oldBHData = _CFPreferencesCopyMultipleWithContainer(NULL, CFSTR("com.apple.powerlogd"),
                kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, containerPath);
        CFRelease(containerPath);
    }
    return oldBHData;
}
#endif // !POWERD_IOS_XCTEST

/*
 * This function returns previous battery health data saved to disk.
 * This function will return NULL only when there is no previous battery health data in powerd's CFPrefs
 * and system is not unlocked even once to migrate data from powerlog.
 */
static CFMutableDictionaryRef copyBatteryHealthData(void)
{
    CFDictionaryRef dict;
    CFMutableDictionaryRef bhData;
    CFDictionaryRef oldBHData = NULL;
    CFStringRef oldSerialNo = NULL;
    IOPSBatteryHealthServiceFlags oldSvcFlags = 0;
    IOPSBatteryHealthServiceState oldSvcState = 0;
    CFNumberRef oldMaxCapacity = NULL;

    dict = copyBatteryHealthDataFromPrefs();
    if (dict && (CFDictionaryGetCount(dict) != 0)) {
        bhData = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, dict);
        CFRelease(dict);
        return bhData;
    }
    if (dict) {
        CFRelease(dict);
    }

    // There is no battery health data in powerd's CFPrefs.
    // First migrate the old Battery Health data maintained by powerlog. Migration can't be done until first unlock is done.
    if (MKBDeviceUnlockedSinceBoot() != 1) {
        ERROR_LOG("powerlog's battery health data can't be migrated until first unlock\n");
        return NULL;
    }

    bhData = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (bhData == NULL) {
        ERROR_LOG("Failed to create dictionary to hold battery data\n");
        return NULL;
    }

    oldBHData = copyPowerlogBatteryHealthData();

    if (oldBHData) {
        // Below key names are as used by powerlog
        CFDictionaryGetValueIfPresent(oldBHData, CFSTR("BatterySerial"), (const void **)&oldSerialNo);
        CFDictionaryGetValueIfPresent(oldBHData, CFSTR("MaximumCapacityPercent"), (const void **)&oldMaxCapacity);

        CFDictionaryGetIntValue(oldBHData, CFSTR("batteryServiceFlags"), oldSvcFlags);
        CFDictionaryGetIntValue(oldBHData, CFSTR("batteryServiceRecommended"), oldSvcState);
    }
    INFO_LOG("Battery Health data from powerlog's prefs. SvcFlags:0x%x SvcState:%d MaxCapacity:%{public}@\n",
              oldSvcFlags, oldSvcState, oldMaxCapacity);

    if (isA_CFString(oldSerialNo)) {
        CFDictionarySetValue(bhData, CFSTR(kIOPSBatterySerialNumberKey), oldSerialNo);
    }
    else {
        INFO_LOG("No battery serial number found powerlog's battery health state.\n");
    }

    // Migrate old Service Flags to newer version
    IOPSBatteryHealthServiceFlags newSVCFlags = migrateSvcFlags(oldSvcState, oldSvcFlags);
    CFDictionarySetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey), newSVCFlags);

    INFO_LOG("Battery service flags after migration: 0x%x\n", newSVCFlags);

    // OldSvcState and oldMaxCapacity are re-used as-is
    CFDictionarySetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey), oldSvcState);

    if (isA_CFNumber(oldMaxCapacity)) {
        CFDictionarySetValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), oldMaxCapacity);
    }
    else {
        INFO_LOG("No MaxBatteryCapacity found in powerlog's battery health state\n");
    }

    if (oldBHData) {
        CFRelease(oldBHData);
    }
    return bhData;
}

static void _initBatteryHealthData(void)
{
    int status;
    int token;
    CFDictionaryRef dict;

    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

#if TARGET_OS_WATCH
    // No-op WRa and UPO checks for watch as they are not applicable.
    batteryHealthP0Threshold = INT64_MAX;
    batteryHealthUPOAware = kBatteryHealthWithoutUPO;
#else
    status = notify_register_dispatch("com.apple.system.batteryHealth.p0Threshold", &token, batteryTimeRemainingQ,
            ^(int token) {
                notify_get_state(token, &batteryHealthP0Threshold);
                INFO_LOG("Received notification for batteryHealthP0Threshold. Value set to %lld\n", batteryHealthP0Threshold);
            });
    if (status != NOTIFY_STATUS_OK) {
        ERROR_LOG("Failed to register for P0 threshold notifications. rc=%d\n", status);
    }
    else {
        notify_get_state(token, &batteryHealthP0Threshold);
        INFO_LOG("batteryHealthP0Threshold set to %lld\n", batteryHealthP0Threshold);
    }
    status = notify_register_dispatch("com.apple.system.batteryHealth.UPOAware", &token, batteryTimeRemainingQ,
            ^(int token) {
                notify_get_state(token, &batteryHealthUPOAware);
                INFO_LOG("Received notification for batteryHealthUPOAware. Value set to %lld\n", batteryHealthUPOAware);
            });
    if (status != NOTIFY_STATUS_OK) {
        ERROR_LOG("Failed to register for battery health UPO Aware notifications. rc=%d\n", status);
    }
    else {
        notify_get_state(token, &batteryHealthUPOAware);
        INFO_LOG("batteryHealthUPOAware set to %lld\n", batteryHealthUPOAware);
    }
#endif // TARGET_OS_WATCH

    dict = copyBatteryHealthDataFromPrefs();
    if ((dict == NULL) || (CFDictionaryGetCount(dict) == 0)) {
        // There is no battery health data saved.
        // Register for first unlock notification and migrate battery health from powerlog after unlock
        status = notify_register_dispatch(kMobileKeyBagFirstUnlockNotificationID, &token, batteryTimeRemainingQ,
                ^(int token) {
                    CFMutableDictionaryRef  outDict;
                    IOPMBattery **_battArray = NULL, *batt = NULL;

                    outDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                    if (!outDict) {
                        ERROR_LOG("Failed to create dictionary to trigger battery health data migration\n");
                        return;
                    }

                    INFO_LOG("Triggering battery health data migration from powerlog\n");
                    if ((_battArray = _batteries()) && (batt = _battArray[0])) {
                        _setBatteryHealthData(outDict, batt);
                    }
                    else {
                        ERROR_LOG("Battery Data not found");
                    }

                    CFRelease(outDict);
                });
        if (status != NOTIFY_STATUS_OK) {
            ERROR_LOG("Failed to register for first unlock notification. rc=%d\n", status);
        }
    }
    if (dict) {
        CFRelease(dict);
    }
}

static void initBatteryHealthData(void)
{
    static dispatch_once_t onceToken;
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);
    // Ensure this is called only once
    dispatch_once(&onceToken, ^{
        _initBatteryHealthData();
    });
}

void checkNominalCapacity(CFDictionaryRef batteryProps, CFMutableDictionaryRef bhData,
        IOPSBatteryHealthServiceFlags *svcFlags)
{
    int ncc = 0;
    int designCap = 0;
    int nccp = 0;
    int prevNccp = -1;

    if (svcFlags == NULL) {
        return;
    }
    if (bhData == NULL) {
        *svcFlags |= kBHSvcFlagNCCNotDet;
    }

    CFDictionaryGetIntValue(batteryProps, CFSTR("NominalChargeCapacity"), ncc);
    CFDictionaryGetIntValue(batteryProps, CFSTR(kIOPMPSDesignCapacityKey), designCap);

    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), prevNccp);

    nccp = rawToNominal(ncc, designCap);

    if (!IS_IN_NOMINAL_RANGE(nccp)) {
        ERROR_LOG("Failed to calculate Nominal Capacity percentage. NominalCapacity:%d DesignCapacity:%d\n",
                ncc, designCap);
        *svcFlags |= kBHSvcFlagNCCNotDet;
        return;
    }

    // NOTE: nccp_cc_filtering is assumed to be true before we read SMC key. Assuming 'nccp_cc_filtering' is false can
    // lead to sudden drop on MaxCapacity on devices that support CycleCount based filtering.
    if (nccp_cc_filtering == true) {
        int cycleCount = -1;
        int prevCycleCount = -1;

        CFDictionaryGetIntValue(bhData, CFSTR(kIOPMPSCycleCountKey), prevCycleCount);
        CFDictionaryGetIntValue(batteryProps, CFSTR(kIOPMPSCycleCountKey), cycleCount);
        if (cycleCount == -1) {
            ERROR_LOG("Failed to calculate Nominal Capacity percentage. NominalCapacity:%d DesignCapacity:%d CycleCount:%d\n",
                      ncc, designCap, cycleCount);
            *svcFlags |= kBHSvcFlagNCCNotDet;

            // No need to update anything else
            return;
        }
        // If previous cycle count data used in BH calculation is not available, reset it to current battery cycle count.
        if (prevCycleCount == -1) {
            prevCycleCount = cycleCount;
            INFO_LOG("Previous cycle count data is not available. Reset to %d\n", cycleCount);
        }

        // If previous NCCP is not available and cycle count is <= kTrueNCCCycleCountThreshold, consider this
        // as new battery and set NCCP to kInitialNominalCapacityPercentage.
        if (prevNccp == -1) {
            if (cycleCount <= kTrueNCCCycleCountThreshold) {
                nccp = kInitialNominalCapacityPercentage;
            }
            prevNccp = nccp;
            INFO_LOG("Previous NCCP data not available. Reset to %d. Cycle Count: %d\n", nccp, cycleCount);
        }


        // NCCP can only decrease from previous value.
        // NCCP can be reduced by utmost kNCCChangeLimit after cycle count has gone up by kNCCMinCycleCountChange.
        if ((cycleCount - prevCycleCount >= kNCCMinCycleCountChange) && (prevNccp - nccp >= kNCCChangeLimit)) {
            nccp = prevNccp - kNCCChangeLimit;
            INFO_LOG("Changing NCCP from %d -> %d after cycle count change(%d->%d). NCC:%d DesignCap:%d\n",
                    prevNccp, nccp, prevCycleCount, cycleCount, ncc, designCap);
        }
        else {
            nccp = prevNccp;
            cycleCount = prevCycleCount;
        }

        CFDictionarySetIntValue(bhData, CFSTR(kIOPMPSCycleCountKey), cycleCount);
    }
    else {
        static uint64_t nccpUpdate_ts = 0;
        uint64_t currentTime = getMonotonicContinuousTime();
        uint64_t timeDelta = currentTime - nccpUpdate_ts;

        // Remove Cycle Count value from bhData. This avoids MaxCapacity change before nccp_cc_filtering is set
        // to appropriate value at boot. nccp_cc_filtering is assumed to be true by default.
        CFDictionaryRemoveValue(bhData, CFSTR(kIOPMPSCycleCountKey));
        removeKeyFromBatteryHealthDataPrefs(CFSTR(kIOPMPSCycleCountKey));

        if (prevNccp == -1) {
            prevNccp = nccp;
            nccpUpdate_ts = currentTime;
            INFO_LOG("Previous NCCP data not available. Reset to %d.\n", nccp);
        }

        // NCCP can be updated only once every 24hrs.
        // NCCP can only decrease from previous value
        if ((prevNccp <= nccp) || (timeDelta < battReadTimeDelta))  {
            DEBUG_LOG("Using previous NCCP value %d\n", prevNccp);
            nccp = prevNccp;
        }
        else {
            INFO_LOG("Changing NCCP from %d -> %d after %llu secs. NCC:%d DesignCap:%d\n", prevNccp, nccp, timeDelta, ncc, designCap);
            nccpUpdate_ts = currentTime;
        }
    }

    if (nccp < kNominalCapacityPercentageThreshold) {
        *svcFlags |= kBHSvcFlagNCC;
        INFO_LOG("Nominal Capacity percentage(%d) is less than the threshold(%d)\n", nccp, kNominalCapacityPercentageThreshold);
    }

    CFDictionarySetIntValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), nccp);

    DEBUG_LOG("Battery NominalCapacity:%d DesignCapacity:%d NCC:%d\n", ncc, designCap, nccp);
}

static void checkUPOCount(IOPSBatteryHealthServiceFlags *svcFlags)
{
    CFTypeRef n;
    static int mitigatedUPOCnt = 0;

    if (!batteryHealthUPOAware) {
        ERROR_LOG("Battery health UPO Aware value not set\n");
        *svcFlags |= kBHSvcFlagUPOPrimeNotDet;
        return;
    }
    if (batteryHealthUPOAware == kBatteryHealthWithoutUPO) {
        // Not all models' battery health need UPO check
        return;
    }

    // This value has to be read every time as publisher of this value may not have published
    // by the time powerd reads this value.
    // Also, this key is not published when mitigatedUPOCnt is zero.
    n = CFPreferencesCopyValue(CFSTR(kCFPrefsMitigatedUPOCountKey), CFSTR(kCFPrefsUPOMetricsDomain),
            kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);

    if (isA_CFNumber(n)) {
        CFNumberGetValue(n, kCFNumberIntType, &mitigatedUPOCnt);
        DEBUG_LOG("Mitigated UPO count:%d\n", mitigatedUPOCnt);
    }
    else if (n) {
        ERROR_LOG("Unexpected data type for mitigatedUPOCnt(type:%lu)\n", CFGetTypeID(n));
    }
    else {
        DEBUG_LOG("Unable to read mitigatedUPOCnt. Considering it as 0\n");
    }

    if (mitigatedUPOCnt > kMitigatedUPOCountThreshold) {
        *svcFlags |= kBHSvcFlagUPOPrime;
        INFO_LOG("Mitigated UPO count(%d) is greater than the threshold(%d)\n", mitigatedUPOCnt, kMitigatedUPOCountThreshold);
    }

    if (n) {
        CFRelease(n);
    }
}

static void checkWeightedRa(CFDictionaryRef batteryProps, IOPSBatteryHealthServiceFlags *svcFlags)
{
    CFDictionaryRef batteryData = NULL;
    static int weightedRa = -1;
    static uint64_t wraUpdate_ts = 0;
    uint64_t currentTime = getMonotonicContinuousTime();
    uint64_t timeDelta = currentTime - wraUpdate_ts;

    if (batteryHealthP0Threshold == 0) {
        ERROR_LOG("Battery P0 threshold is not set\n");
        *svcFlags |= kBHSvcFlagWRaNotDet;
        return;
    }
    if (batteryHealthP0Threshold == -1) {
        // batteryHealthP0Threshold will be set to -1 on models with chemID specific
        // P0 thresholds, but device has unknown chemId.
        ERROR_LOG("Failed to get Battery health P0 threshold value\n");
        *svcFlags |= kBHSvcFlagChemIDNotDet;
    }
    else {
        if ((weightedRa <= 0) || (timeDelta >= battReadTimeDelta)) {
            weightedRa = -1; // Reset to -1 to avoid re-using previous value
            batteryData = CFDictionaryGetValue(batteryProps, CFSTR("BatteryData"));
            if (batteryData) {
                CFDictionaryGetIntValue(batteryData, CFSTR("WeightedRa"), weightedRa);
            }
            DEBUG_LOG("Using updated wRA %d from battery properties after %llu secs\n", weightedRa, timeDelta);
            wraUpdate_ts = currentTime;
        }
        else {
            DEBUG_LOG("Using previous wRA %d\n", weightedRa);
        }

        if (weightedRa <= 0) {
            ERROR_LOG("Failed to read battery weightedRa\n");
            *svcFlags |= kBHSvcFlagWRaNotDet;
        }
        else if (weightedRa >= batteryHealthP0Threshold) {
            *svcFlags |= kBHSvcFlagWRa;
            INFO_LOG("WeightedRa(%d) is >= threshold(%llu)\n", weightedRa, batteryHealthP0Threshold);
        }
    }
}

static void checkCellDisconnectCount(CFDictionaryRef batteryProps, IOPSBatteryHealthServiceFlags *svcFlags)
{
    int bcdc = -1;

    if (!smcBasedDevice) {
        // Cell disconnect count property is set only on SMC based devices.
        return;
    }

    CFDictionaryGetIntValue(batteryProps, CFSTR("BatteryCellDisconnectCount"), bcdc);
    if (bcdc < 0) {
        ERROR_LOG("Failed to read battery cell disconnect count\n");
        *svcFlags |= kBHSvcFlagBCDCNotDet;
    }
    else if (bcdc >= kBatteryCellDisconnectThreshold) {
        *svcFlags |= kBHSvcFlagBCDC;
        INFO_LOG("BCDC(%d) is greater than the threshold(%d)\n", bcdc, kBatteryCellDisconnectThreshold);
    }
}

static void _setBatteryHealthData(
    CFMutableDictionaryRef  outDict,
    IOPMBattery  *b)
{
    IOPSBatteryHealthServiceFlags svcFlags = kBatteryHealthCurrentVersion;
    IOPSBatteryHealthServiceState svcState = 0;
    CFDictionaryRef  batteryProps = b->properties;

    // Initialize BH data as this function can run before initializeBatteryCalculations() gets a chance.
    initBatteryHealthData();

    CFMutableDictionaryRef bhData = copyBatteryHealthData();
    if (bhData == NULL) {
        // Unable to get previous health data. Can't provide any health info at this point.
        // Do not save this info to disk, as this is a transient state
        svcFlags |= kBHSvcFlagUnknownPrevState;
        svcState = kBHSvcStateDataNotMigrated;

        CFDictionarySetIntValue(outDict, CFSTR(kIOPSBatteryHealthServiceFlagsKey), svcFlags);
        CFDictionarySetIntValue(outDict, CFSTR(kIOPSBatteryHealthServiceStateKey), svcState);
        // Don't set Max Capacity

        ERROR_LOG("Unable to get previous battery health state. Service Flags:0x%x Service State:%d\n",
                svcFlags, svcState);

        return;
    }

    CFStringRef battPropsSerial = CFDictionaryGetValue(batteryProps, CFSTR("Serial"));
    CFStringRef bhDataSerial = CFDictionaryGetValue(bhData, CFSTR(kIOPSBatterySerialNumberKey));

    if ( ( ((bhDataSerial == NULL) || (battPropsSerial == NULL)) && (bhDataSerial != battPropsSerial)) ||
            ((bhDataSerial != NULL) && (battPropsSerial != NULL) && (CFStringCompare(bhDataSerial, battPropsSerial, 0) != kCFCompareEqualTo)) ) {
        // Reset sticky bits of service Flags, reset Service state to Unknown.
        // Save this new serial number from battery props in the bhData
        svcFlags &= ~(kBHSvcFlagStickyBits);

        // Remove BH data about previous battery to force re-evaluation for new battery
        CFDictionaryRemoveValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent));
        CFDictionaryRemoveValue(bhData, CFSTR(kIOPMPSCycleCountKey));
        CFDictionaryRemoveValue(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey));

        removeKeyFromBatteryHealthDataPrefs(CFSTR(kIOPSBatteryHealthMaxCapacityPercent));
        removeKeyFromBatteryHealthDataPrefs(CFSTR(kIOPMPSCycleCountKey));
        removeKeyFromBatteryHealthDataPrefs(CFSTR(kIOPSBatteryHealthServiceStateKey));

        if (battPropsSerial == NULL) {
            CFDictionaryRemoveValue(bhData, CFSTR(kIOPSBatterySerialNumberKey));
            removeKeyFromBatteryHealthDataPrefs(CFSTR(kIOPSBatterySerialNumberKey));
        }
        else {
            CFDictionarySetValue(bhData, CFSTR(kIOPSBatterySerialNumberKey), battPropsSerial);
        }

        CFDictionarySetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey), svcFlags);
        saveBatteryHealthDataToPrefs(bhData);
        INFO_LOG("Battery serial number changed.\n");
    }

    // Reset flags for battery serial errors and set them again if still applicable
    svcFlags &= ~(kBHSvcFlagNoSerial|kBHSvcFlagEmptySerial);
    if ((battPropsSerial == NULL) || (CFStringGetLength(battPropsSerial) == 0)) {
        ERROR_LOG("Invalid battery serial number(%{public}@) in battery properties\n", battPropsSerial);

        if (battPropsSerial == NULL) {
            svcFlags |= kBHSvcFlagNoSerial;
        }
        else if (CFStringGetLength(battPropsSerial) == 0) {
            svcFlags |= kBHSvcFlagEmptySerial;
        }

        ERROR_LOG("Unable to get serial number of the battery. Service Flags:0x%x Service State:%d\n",
                svcFlags, svcState);
    }

    CFTypeRef flagsRef, stateRef, capRef, cycleCountRef;
    flagsRef = stateRef = capRef = cycleCountRef = NULL;
    CFDictionaryGetValueIfPresent(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey), (const void **)&flagsRef);
    CFDictionaryGetValueIfPresent(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey), (const void **)&stateRef);
    CFDictionaryGetValueIfPresent(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), (const void **)&capRef);
    CFDictionaryGetValueIfPresent(bhData, CFSTR(kIOPMPSCycleCountKey), (const void **)&cycleCountRef);
    INFO_LOG("Previous Battery Health: Flags:%{public}@ State:%{public}@ MaxCapacity:%{public}@ CycleCount:%{public}@\n",
            flagsRef, stateRef, capRef, cycleCountRef);

    checkNominalCapacity(batteryProps, bhData, &svcFlags);
    checkUPOCount(&svcFlags);
    checkWeightedRa(batteryProps, &svcFlags);
    checkCellDisconnectCount(batteryProps, &svcFlags);

    updateBatteryServiceState(batteryProps, bhData, svcFlags);
    saveBatteryHealthDataToPrefs(bhData);

    flagsRef = stateRef = capRef = NULL;
    flagsRef = CFDictionaryGetValue(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey));
    if (isA_CFNumber(flagsRef)) {
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthServiceFlagsKey), flagsRef);
    }
    stateRef = CFDictionaryGetValue(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey));
    if (isA_CFNumber(stateRef)) {
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthServiceStateKey), stateRef);
    }
    capRef = CFDictionaryGetValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent));
    if (isA_CFNumber(capRef)) {
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), capRef);
    }
    CFDictionaryGetValueIfPresent(bhData, CFSTR(kIOPMPSCycleCountKey), (const void **)&cycleCountRef);

    INFO_LOG("Updated Battery Health: Flags:%{public}@ State:%{public}@ MaxCapacity:%{public}@ CycleCount:%{public}@\n",
            flagsRef, stateRef, capRef, cycleCountRef);
    CFRelease(bhData);

}

#elif TARGET_OS_OSX // TARGET_OS_IOS || POWERD_IOS_XCTEST || TARGET_OS_WATCH
static CFStringRef getBatteryHealthPath(void)
{
    static CFMutableStringRef nvramBatteryHealthPath = NULL;

    if (nvramBatteryHealthPath) {
        return nvramBatteryHealthPath;
    }

    nvramBatteryHealthPath = CFStringCreateMutable(NULL, 0);
    CFStringAppend(nvramBatteryHealthPath, CFUUIDCreateString(NULL, NVRAM_BATTERY_HEALTH_UUID));
    CFStringAppend(nvramBatteryHealthPath, CFSTR(":"));
    CFStringAppend(nvramBatteryHealthPath, CFSTR(NVRAM_BATTERY_HEALTH_KEY));
    return nvramBatteryHealthPath;
}

static CF_RETURNS_RETAINED CFMutableDictionaryRef readBatteryHealthPersistentData(void)
{
    io_registry_entry_t ioent = IO_OBJECT_NULL;
    CFTypeRef dictData = nil;
    id json = nil;
    NSMutableDictionary *jsonDictionaryMutable = nil;
    CFMutableDictionaryRef cfDict = nil;

    _internal_dispatch_assert_queue(batteryTimeRemainingQ);

    if (cachedBatteryHealthDataDict) {
        CFRetain(cachedBatteryHealthDataDict);
        return cachedBatteryHealthDataDict;
    }

    ioent = IORegistryEntryFromPath(kIOMasterPortDefault, NVRAM_DEVICE_PATH);
    if (ioent == IO_OBJECT_NULL) {
        ERROR_LOG(""NVRAM_DEVICE_PATH" missing!\n");
        goto out;
    }

    dictData = IORegistryEntryCreateCFProperty(ioent, getBatteryHealthPath(), kCFAllocatorDefault, 0);
    if (dictData == nil) {
        ERROR_LOG("Persistent storage missing!\n");
        goto out;
    }

    if (CFGetTypeID(dictData) != CFDataGetTypeID()) {
        ERROR_LOG("Persistent data is corrupted\n");
        goto out;
    }

    @autoreleasepool {
        json = [NSJSONSerialization JSONObjectWithData:(__bridge NSData *)(CFDataRef)dictData options:0 error:nil];
        if (!json) {
            ERROR_LOG("Failed to unpack data\n");
            goto out;
        }

        if (![json isKindOfClass:[NSDictionary class]]) {
            ERROR_LOG("Persistent data not a dictionary\n");
            goto out;
        }

        jsonDictionaryMutable = [(NSDictionary *)json mutableCopy];
        if (!jsonDictionaryMutable) {
            ERROR_LOG("Failed to generate battery-health dictionary\n");
            goto out;
        }
    }

    cfDict = (__bridge CFMutableDictionaryRef)jsonDictionaryMutable;

out:
    if (dictData != nil) {
        CFRelease(dictData);
    }
    if (ioent != IO_OBJECT_NULL) {
        IOObjectRelease(ioent);
    }
    return cfDict;
}

static bool writeBatteryHealthPersistentData(CFMutableDictionaryRef cfDict)
{
    bool status = false;
    io_registry_entry_t ioent = IO_OBJECT_NULL;
    NSData *dictData = nil;
    kern_return_t ret;

    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    NSDictionary *dict = (__bridge NSDictionary *)cfDict;

    ioent = IORegistryEntryFromPath(kIOMasterPortDefault, NVRAM_DEVICE_PATH);
    if (ioent == IO_OBJECT_NULL) {
        goto out;
    }

    @autoreleasepool {
        dictData = [NSJSONSerialization dataWithJSONObject:dict options:0 error:nil];
        if (dictData == nil) {
            ERROR_LOG("Failed to serialize dict\n");
            goto out;
        }

        ret = IORegistryEntrySetCFProperty(ioent, getBatteryHealthPath(), (__bridge CFTypeRef)dictData);
        if (ret != KERN_SUCCESS) {
            ERROR_LOG("Failed to write persistent data\n");
            goto out;
        }
    }

    status = true;

    if (!cachedBatteryHealthDataDict) {
        cachedBatteryHealthDataDict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, cfDict);
    }

out:
    if (ioent != IO_OBJECT_NULL) {
        IOObjectRelease(ioent);
    }
    return status;
}

static CFMutableDictionaryRef createPersistentStorage(IOPMBattery *b, IOPSBatteryHealthServiceFlags flags)
{
    int val = 0;
    int rawMaxCap = 0, designCap = 0, fccp;
    CFMutableDictionaryRef dict = nil;
    CFDictionaryRef batteryProps = nil;
    IOPSBatteryHealthServiceFlags svcFlags = kBatteryHealthCurrentVersion;
    IOPSBatteryHealthServiceFlags sticky = flags & kBHSvcFlagStickyBits;
    IOReturn ret;
    boolean_t hasLowCapRatio = false;
    time_t since;

    _internal_dispatch_assert_queue(batteryTimeRemainingQ);

    if (b->properties) {
        batteryProps = b->properties;
    } else {
        ERROR_LOG("Missing battery props\n");
        goto out;
    }

    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                       &kCFTypeDictionaryKeyCallBacks,
                                       &kCFTypeDictionaryValueCallBacks);
    if (dict == nil) {
        ERROR_LOG("Failed to create battery dictionary\n");
        goto out;
    }

    // Port over serial number from existing dictionary
    CFStringRef battPropsSerial = batteryTimeRemaining_getBatterySerialNumber();
    if (!isA_CFString(battPropsSerial)) {
        svcFlags |= kBHSvcFlagNoSerial;
    } else if (CFStringGetLength(battPropsSerial) == 0) {
        svcFlags |= kBHSvcFlagEmptySerial;
    } else {
        CFDictionarySetValue(dict, CFSTR(kIOPSBatterySerialNumberKey), battPropsSerial);
    }

    CFDictionaryGetIntValue(batteryProps, CFSTR("AppleRawMaxCapacity"), rawMaxCap);
    if (!rawMaxCap) {
       CFDictionaryGetIntValue(batteryProps, CFSTR(kIOPMPSMaxCapacityKey), rawMaxCap);
    }
    CFDictionaryGetIntValue(batteryProps, CFSTR(kIOPMPSDesignCapacityKey), designCap);
    if (rawMaxCap <= 0 || designCap <= 0) {
        ERROR_LOG("Zero capacity detected\n");
        goto out;
    }

    fccp = rawToNominal(rawMaxCap, designCap) + rawToNominal(kSmartBattReserve_mAh, designCap);
    CFDictionarySetIntValue(dict, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), fccp);
    CFDictionarySetIntValue(dict, CFSTR(kFccAvgHistoryCount), val);
    CFDictionarySetIntValue(dict, CFSTR(kFccDaySampleCount), val);
    CFDictionarySetIntValue(dict, CFSTR(kMitigatedFccDaySampleAvg), val);
    CFDictionarySetIntValue(dict, CFSTR(kMitigatedNominalCapacityPercentage), val);
    CFDictionarySetIntValue(dict, CFSTR(kUnMitigatedNominalCapacityPercentage), val);
    CFDictionarySetIntValue(dict, CFSTR(kMitigatedNominalCapacityAvg), val);
    CFDictionarySetIntValue(dict, CFSTR(kUnMitigatedNominalCapacityAvg), val);
    val = (NVRAM_BATTERY_HEALTH_VER_MAJOR << 16) | NVRAM_BATTERY_HEALTH_VER_MINOR;
    CFDictionarySetIntValue(dict, CFSTR("version"), val);

    if ((flags & kBHSvcFlagNewBattery)) {
        svcFlags |= kBHSvcFlagNewBattery;
        sticky = 0;
    } else {
        ret = _getLowCapRatioTime(battPropsSerial, &hasLowCapRatio, &since);
        if (ret == kIOReturnSuccess && hasLowCapRatio) {
            svcFlags |= (kBHSvcFlagNCC | kBHSvcFlagPrevService);
            INFO_LOG("Detected previous service:%d\n", svcFlags);
        }
    }
    svcFlags |= sticky;
    CFDictionarySetIntValue(dict, CFSTR(kIOPSBatteryHealthServiceFlagsKey), svcFlags);

    val = kBHSvcStateNone;
    CFDictionarySetIntValue(dict, CFSTR(kIOPSBatteryHealthServiceStateKey), val);

    if(!writeBatteryHealthPersistentData(dict)) {
        ERROR_LOG("Failed to initialize persistent data\n");
    }

    if (cachedBatteryHealthDataDict) {
        CFRelease(cachedBatteryHealthDataDict);
        cachedBatteryHealthDataDict = NULL;
    }

out:
    return dict;
}

static void initBatteryHealthData(void)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    CFMutableDictionaryRef dict = NULL;
    IOPMBattery **b = _batteries();
    bool resetStorage = true;
    IOPSBatteryHealthServiceFlags flags = 0;

    CFStringRef battPropsSerial = batteryTimeRemaining_getBatterySerialNumber();

    if (!b || !b[0] || !isA_CFDictionary(b[0]->properties) || !isA_CFString(battPropsSerial)) {
        ERROR_LOG("Invalid battery data\n");
        return;
    }

    dict = readBatteryHealthPersistentData();
    if (dict == NULL) {
        INFO_LOG("Missing battery-health data\n");
        goto out;
    }

    CFDictionaryGetIntValue(dict, CFSTR(kIOPSBatteryHealthServiceFlagsKey), flags);

    CFStringRef storedSerial = CFDictionaryGetValue(dict, CFSTR(kIOPSBatterySerialNumberKey));
    if (!isA_CFString(storedSerial)) {
        INFO_LOG("Invalid Serial\n");
        goto out;
    }

    if (CFStringCompare(battPropsSerial, storedSerial, 0) != kCFCompareEqualTo) {
        INFO_LOG("Battery change detected\n");
        flags |= kBHSvcFlagNewBattery;
        goto out;
    }

    if (!CFDictionaryContainsKey(dict, CFSTR("version"))) {
        INFO_LOG("Battery health data outdated, resetting\n");
        goto out;
    }

    if (!(CFDictionaryContainsKey(dict, CFSTR(kIOPSBatteryHealthMaxCapacityPercent))
                && CFDictionaryContainsKey(dict, CFSTR(kIOPSBatteryHealthServiceFlagsKey))
                && CFDictionaryContainsKey(dict, CFSTR(kIOPSBatteryHealthServiceStateKey)))) {
        ERROR_LOG("Missing keys in battery health dictionary\n");
        goto out;
    }

    resetStorage = false;

out:
    if (resetStorage == true) {
        CFMutableDictionaryRef newDict = createPersistentStorage(b[0], flags);
        if (newDict == NULL) {
            ERROR_LOG("Failed to create persistent storage\n");
        } else {
            CFRelease(newDict);
            INFO_LOG("Initialized persistent storage\n");
        }
    }

    if (dict) {
        CFRelease(dict);
    }

    return;
}

/*
 * Test if the sampling time expiration has crossed the pre defined limit.
 * If so, reset the base sampling time to arm for next expiry
 *
 * @return value: true if limit crossed, false otherwise
 */
static bool hasSamplingTimeExpired(void)
{
    static uint64_t baseTime = 0;
    uint64_t currentTime, timeDelta;
    bool expired = false;

    /*
     * Quirk to set the baseTime for the very first time (after every boot).
     */
    if (baseTime == 0) {
        baseTime = getMonotonicContinuousTime();
    }

    currentTime = getMonotonicContinuousTime();
    timeDelta = currentTime - baseTime;

    if(timeDelta > battReadTimeDelta) {
        baseTime = currentTime;
        expired = true;
    }
    return expired;
}

/*
 * 'Code drop' from battery algo, move to independent file if needed
 */
void calculateNominalCapacity(struct nominalCapacityParams *params) {
    unsigned int significantChange = CAPACITY_NO_CHANGE;
    unsigned int fccDaySampleCount = params->fccDaySampleCount;
    unsigned int fccAvgHistoryCount = params->fccAvgHistoryCount;
    int fccGG = params->fcc;
    static int fccLast;
    int temperature = params->temperature;
    int current = params->current;
    int designCapacity = params->designCapacity;
    int gamma = params->gamma == 0 ? NCC_GAMMA : params->gamma ;
    int currentThreshold, fccDaySampleAvg[vactModesCount], ncc[vactModesCount];

    // step 1: assign inputs
    for (int k = 0; k < vactModesCount; k++) {
        fccDaySampleAvg[k] = params->sample[k].fccDaySampleAvg;
        ncc[k] = params->sample[k].ncc;
    }

    // Step 2: calculate average of day's FCC values
    currentThreshold = (designCapacity * NCC_CURRENT_THRESH) / NCC_CURRENT_THRESH_SCALE;
    if ((fccDaySampleCount == 0) || ((fccGG != fccLast) && (temperature > NCC_TEMP_THRESH) && (abs(current) < currentThreshold))) {
        fccDaySampleCount++;
        for (int k = 0; k < vactModesCount; k++) {
            fccDaySampleAvg[k] = ((fccDaySampleCount - 1) * fccDaySampleAvg[k] + params->sample[k].fcc) / fccDaySampleCount;

            // first day, treat fccDaySampleAvg as ncc
            if (fccAvgHistoryCount == 0) {
                ncc[k] = fccDaySampleAvg[k];
                if (fccDaySampleCount == 1) {
                    significantChange |= CAPACITY_NCC_CHANGE;
                }
            }
        }
        fccLast = fccGG;
        significantChange |= CAPACITY_FCC_CHANGE;
    }

    // Step 3. Check if one day is over
    if (!hasSamplingTimeExpired()) {
        DEBUG_LOG("Skipping NCC calculation\n");
        goto out;
    }
    significantChange |= CAPACITY_SAMPLING_EPOCH_CHANGE;

    // Step 4: iir filter the fcc average for the day to get the ncc
    for (int k = 0; k < vactModesCount; k++) {
        int nccPrev = ncc[k];
        ncc[k] = (gamma * ncc[k] + (NCC_GAMMA_SCALE - gamma) * fccDaySampleAvg[k]) / NCC_GAMMA_SCALE;
        fccDaySampleAvg[k] = 0;
        if (ncc[k] < nccPrev) {
            significantChange |= CAPACITY_NCC_CHANGE;
        }
    }
    fccDaySampleCount = 0;
    fccAvgHistoryCount = fccAvgHistoryCount < UINT32_MAX ? (fccAvgHistoryCount + 1) : UINT32_MAX;

out:
    params->fccDaySampleCount = fccDaySampleCount;
    params->fccAvgHistoryCount = fccAvgHistoryCount;
    for (int k = 0; k < vactModesCount; k++) {
        params->sample[k].fccDaySampleAvg = fccDaySampleAvg[k];
        params->sample[k].ncc = ncc[k];
    }
    params->significantChange = significantChange;
    return;
}

void checkNominalCapacity(CFDictionaryRef batteryProps, CFMutableDictionaryRef dict,
        IOPSBatteryHealthServiceFlags *svcFlags)
{
    int fccp, rawMaxCap = 0, designCap = 0;
    struct nominalCapacityParams params;
    CFDictionaryRef batteryData = NULL;
    int capacityUI, nccService;
    int waitForFC = 0;

    if (svcFlags == NULL) {
        ERROR_LOG("Invalid scv flags\n");
        goto out;
    }

    if (dict == nil || batteryProps == nil) {
        ERROR_LOG("Invalid battery params\n");
        *svcFlags |= kBHSvcFlagNoBatteryData;
        goto out;
    }

    CFStringRef battPropsSerial = batteryTimeRemaining_getBatterySerialNumber();
    if ((battPropsSerial == NULL) || (CFStringGetLength(battPropsSerial) == 0)) {
        ERROR_LOG("Invalid battery serial number(%{public}@) in battery properties\n", battPropsSerial);

        if (battPropsSerial == NULL) {
            *svcFlags |= kBHSvcFlagNoSerial;
        } else {
            *svcFlags |= kBHSvcFlagEmptySerial;
        }

        ERROR_LOG("Unable to get serial number of the battery. Service Flags:0x%x\n", *svcFlags);
        goto out;
    }

    CFStringRef storedSerial = CFDictionaryGetValue(dict, CFSTR(kIOPSBatterySerialNumberKey));
    if (!isA_CFString(storedSerial) && isA_CFString(battPropsSerial)) {
        CFDictionarySetValue(dict, CFSTR(kIOPSBatterySerialNumberKey), battPropsSerial);
        INFO_LOG("persistent store serial missing, updated\n");
    }

    CFDictionaryGetIntValue(dict, CFSTR(kWaitForFCState), waitForFC);
    if (waitForFC && kCFBooleanTrue == CFDictionaryGetValue(batteryProps, CFSTR(kIOPMFullyChargedKey))) {
        waitForFC = 0;
        CFDictionarySetIntValue(dict, CFSTR(kWaitForFCState), waitForFC);
    }

    CFDictionaryGetIntValue(batteryProps, CFSTR("AppleRawMaxCapacity"), rawMaxCap);
    if (!rawMaxCap) {
       CFDictionaryGetIntValue(batteryProps, CFSTR(kIOPMPSMaxCapacityKey), rawMaxCap);
    }
    CFDictionaryGetIntValue(batteryProps, CFSTR(kIOPMPSDesignCapacityKey), designCap);
    if (rawMaxCap <= 0 || designCap <= 0) {
        ERROR_LOG("Zero capacity detected\n");
        *svcFlags |= kBHSvcFlagNCCNotDet;
        goto out;
    }

    fccp = rawToNominal(rawMaxCap, designCap);
    if (!IS_IN_NOMINAL_RANGE(fccp)) {
        ERROR_LOG("MaxCapacity out of range: %d\n", fccp);
        *svcFlags |= kBHSvcFlagNCCNotDet;
        goto out;
    }

    memset(&params, 0, sizeof(params));
    CFDictionaryGetIntValue(dict, CFSTR(kFccDaySampleCount), params.fccDaySampleCount);
    CFDictionaryGetIntValue(dict, CFSTR(kFccAvgHistoryCount), params.fccAvgHistoryCount);
    for (int i = 0; i < vactModesCount; i++) {
        struct capacitySample *vactSample = &params.sample[i];
        for (int j =0; j < ARRAY_SIZE(capacityKeys[i]); ) {
            CFDictionaryGetIntValue(dict, capacityKeys[i][j++], vactSample->nccpMonotonic);
            CFDictionaryGetIntValue(dict, capacityKeys[i][j++], vactSample->ncc);
            CFDictionaryGetIntValue(dict, capacityKeys[i][j++], vactSample->fccDaySampleAvg);
        }
    }

    params.designCapacity = designCap;
    params.fcc = rawMaxCap;
    CFDictionaryGetIntValue(batteryProps, CFSTR(kIOPMPSBatteryTemperatureKey), params.temperature);
    CFDictionaryGetIntValue(batteryProps, CFSTR(kAsbInstantAmperageKey), params.current);

    batteryData = CFDictionaryGetValue(batteryProps, CFSTR(kAsbBatteryDataKey));
    if (batteryData) {
        CFDictionaryGetIntValue(batteryData, CFSTR(kAsbFccComp1Key), params.sample[vactModeDisabled].fcc);
        CFDictionaryGetIntValue(batteryData, CFSTR(kAsbFccComp2Key), params.sample[vactModeEnabled].fcc);
    }

    /*
     * Fallback to gauge reading if scaled FCC is missing. (pre-gibraltar)
     */
    if (!params.sample[vactModeDisabled].fcc || !params.sample[vactModeEnabled].fcc) {
        params.sample[vactModeEnabled].fcc = params.sample[vactModeDisabled].fcc = params.fcc;
    }

    DEBUG_LOG("fccGG:%d T:%d I:%d vact:%d fccU:%d fccM:%d\n", params.fcc, params.temperature, params.current, getVactState(), params.sample[vactModeDisabled].fcc,
        params.sample[vactModeEnabled].fcc);

    calculateNominalCapacity(&params);
    if (params.significantChange) {
        for (int i = 0; i < vactModesCount; i++) {
            if (!params.sample[i].nccpMonotonic) {
                params.sample[i].nccpMonotonic = rawToNominal(params.sample[i].ncc, designCap);
            } else {
                if (waitForFC && i == vactModeDisabled) {
                    params.sample[i].nccpMonotonic = rawToNominal(params.sample[i].ncc, designCap);
                } else {
                    params.sample[i].nccpMonotonic = MIN(params.sample[i].nccpMonotonic, rawToNominal(params.sample[i].ncc, designCap));
                }
            }
        }
    }
    
    if (!IS_IN_NOMINAL_RANGE(params.sample[vactModeEnabled].nccpMonotonic) && !IS_IN_NOMINAL_RANGE(params.sample[vactModeDisabled].nccpMonotonic)) {
        ERROR_LOG("Failed to calculate NCC\n");
        *svcFlags |= kBHSvcFlagNCCNotDet;
        goto out;
    }

    CFDictionarySetIntValue(dict, CFSTR(kFccDaySampleCount), params.fccDaySampleCount);
    CFDictionarySetIntValue(dict, CFSTR(kFccAvgHistoryCount), params.fccAvgHistoryCount);
    for (int i = 0; i < vactModesCount; i++) {
        struct capacitySample *vactSample = &params.sample[i];
        for (int j =0; j < ARRAY_SIZE(capacityKeys[i]); ) {
            CFDictionarySetIntValue(dict, capacityKeys[i][j++], vactSample->nccpMonotonic);
            CFDictionarySetIntValue(dict, capacityKeys[i][j++], vactSample->ncc);
            CFDictionarySetIntValue(dict, capacityKeys[i][j++], vactSample->fccDaySampleAvg);
        }
        DEBUG_LOG("[%d] fccAvgHistoryCount:%d fccDaySampleCount:%d fccDaySampleAvg:%d nccAvg:%d nccpMono:%d svcFlagsNew:0x%x\n",
                i, params.fccAvgHistoryCount, params.fccDaySampleCount, vactSample->fccDaySampleAvg, vactSample->ncc, vactSample->nccpMonotonic,
                *svcFlags);
    }

    nccService = params.sample[vactModeEnabled].nccpMonotonic + rawToNominal(kSmartBattReserve_mAh, designCap);
    capacityUI = vactModeEnabled == getVactState() ? params.sample[vactModeEnabled].nccpMonotonic : params.sample[vactModeDisabled].nccpMonotonic;
    capacityUI += rawToNominal(kSmartBattReserve_mAh, designCap);
    CFDictionarySetIntValue(dict, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), capacityUI);
    // A low capacity service flag depends only on VACT enabled NCC value (default)
    if (nccService < kNominalCapacityPercentageThreshold) {
        *svcFlags |= kBHSvcFlagNCC;
        INFO_LOG("Nominal Capacity percentage(%d) is less than the threshold(%d)\n", params.sample[vactModeEnabled].ncc, kNominalCapacityPercentageThreshold);
    }

    DEBUG_LOG("nccService: %d capacityUI:%d nccU:%d nccM:%d vactMode:%d waitForFC:%d\n", nccService, capacityUI, params.sample[vactModeDisabled].nccpMonotonic, params.sample[vactModeEnabled].nccpMonotonic, getVactState(), waitForFC);
out:
    return;
}

static  bool _batteryHas(IOPMBattery *b, CFStringRef property)
{
    if (!b || !property || !b->properties) return false;

    // If the battery's descriptior dictionary has an entry at all for the
    // given 'property' it is supported, i.e. the battery 'has' it.
    return CFDictionaryGetValue(b->properties, property) ? true : false;
}

static IOReturn _setLowCapRatioTime(CFStringRef batterySerialNumber,
                             boolean_t hasLowCapRatio,
                             time_t since)
{
    IOReturn                    ret         = kIOReturnError;
    CFMutableDictionaryRef      dict        = NULL; // must release
    CFNumberRef                 num         = NULL; // must release

    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if (!isA_CFString(batterySerialNumber))
        goto exit;

    // return early if the cached copy indicates the flag is already set
    if (cachedKeyPresence)  {
        if (hasLowCapRatio == cachedHasLowCap) {
            ret = kIOReturnSuccess;
            goto exit;
        }
    }

    if (hasLowCapRatio) {
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                         0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);

        num = CFNumberCreate(kCFAllocatorDefault,
                             kCFNumberSInt64Type,
                             &since);
        CFDictionarySetValue(dict, batterySerialNumber, num);
        CFRelease(num);

    }
    else {
        // This removes the dictionary from prefs
        dict = NULL;
    }
    ret = IOPMWriteToPrefs(kIOPMBatteryWarnSettings, dict, true, false);


exit:
    if (dict) CFRelease(dict);

    return ret;
}

static void updateBatteryHealth(CFMutableDictionaryRef outDict, IOPMBattery *b)
{
    CFMutableDictionaryRef dict = nil;
    IOPSBatteryHealthServiceFlags svcFlags = kBatteryHealthCurrentVersion;

    CFDictionaryRef  batteryProps = b->properties;
    if (!batteryProps) {
        ERROR_LOG("Missing battery props\n");
        return;
    }

    dict = readBatteryHealthPersistentData();
    if (dict == nil) {
        ERROR_LOG("Failed to read battery health data\n");
        goto out;
    }

    if (0 != b->pfStatus) {
        svcFlags |= kBHSvcFlagPermanentFault;
    }
    checkNominalCapacity(batteryProps, dict, &svcFlags);
    updateBatteryServiceState(batteryProps, dict, svcFlags);
    if(!writeBatteryHealthPersistentData(dict)) {
        ERROR_LOG("Failed to write persistent data\n");
    }
    CFRelease(dict);

out:
    return;
}

// Set health & confidence
static void _setBatteryHealthData(
    CFMutableDictionaryRef  outDict,
    IOPMBattery             *b)
{
    CFStringRef             bh, bhc;
    CFMutableArrayRef       permanentFailures = NULL;

    // no battery present? no health & confidence then!
    // If we return without setting the health and confidence values in
    // outDict, that is OK, it just means they were indeterminate.
    if(!outDict || !b || !b->isPresent)
        return;

    initBatteryHealthData();
    updateBatteryHealth(outDict, b);

    /** Report any failure status from the PFStatus register                          **/
    /***********************************************************************************/
    /***********************************************************************************/
    if ( 0!= b->pfStatus) {
        permanentFailures = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        if (!permanentFailures)
            return;
        if (kSmartBattPFExternalInput & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureExternalInput) );
        }
        if (kSmartBattPFSafetyOverVoltage & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureSafetyOverVoltage) );
        }
        if (kSmartBattPFChargeSafeOverTemp & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureChargeOverTemp) );
        }
        if (kSmartBattPFDischargeSafeOverTemp & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureDischargeOverTemp) );
        }
        if (kSmartBattPFCellImbalance & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureCellImbalance) );
        }
        if (kSmartBattPFChargeFETFailure & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureChargeFET) );
        }
        if (kSmartBattPFDischargeFETFailure & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureDischargeFET) );
        }
        if (kSmartBattPFDataFlushFault & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureDataFlushFault) );
        }
        if (kSmartBattPFPermanentAFECommFailure & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailurePermanentAFEComms) );
        }
        if (kSmartBattPFPeriodicAFECommFailure & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailurePeriodicAFEComms) );
        }
        if (kSmartBattPFChargeSafetyOverCurrent & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureChargeOverCurrent) );
        }
        if (kSmartBattPFDischargeSafetyOverCurrent & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureDischargeOverCurrent) );
        }
        if (kSmartBattPFOpenThermistor & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureOpenThermistor) );
        }
        if (kSmartBattPFFuseBlown & b->pfStatus) {
            CFArrayAppendValue( permanentFailures, CFSTR(kIOPSFailureFuseBlown) );
        }
        CFDictionarySetValue( outDict, CFSTR(kIOPSBatteryFailureModesKey), permanentFailures);
        CFRelease(permanentFailures);
    }

    // Battery health is maintained at the lowest level seen
    static const char *batteryHealth = kIOPSGoodValue;
    static const char *batteryHealthCond = "";

    // Permanent failure -> Poor health
    if (_batteryHas(b, CFSTR(kIOPMPSErrorConditionKey))) {
        if (CFEqual(b->failureDetected, CFSTR(kAsbPermanentFailureKey))) {
            CFDictionarySetValue(outDict, CFSTR(kIOPSHealthConfidenceKey), CFSTR(kIOPSGoodValue));
            // Specifically log that the battery condition is permanent failure
            batteryHealthCond = kIOPSPermanentFailureValue;

            if (strncmp(batteryHealth, kIOPSPoorValue, sizeof(kIOPSPoorValue))) {
                logASLBatteryHealthChanged(kIOPSPoorValue, batteryHealth, kIOPSPermanentFailureValue);
                batteryHealth = kIOPSPoorValue;
            }

            goto exit;
        }
    }

    double compareRatioTo = 0.80;
    double capRatio = 1.0;

    // This test was designed for MacOS only and false triggers on Watches due
    // to Dali reserve.
    // iOS/WatchOS use different ways to determine battery health.
    if (b->designCap) {
        capRatio = ((double)b->maxCap + kSmartBattReserve_mAh) / (double)b->designCap;
    }
    bool cyclesExceedStandard = false;

    if (b->markedDeclining) {
        // The battery status should not fluctuate as battery re-learns and adjusts
        // its FullChargeCapacity. This number may fluctuate in normal operation.
        // Hysteresis: a battery that has previously been marked as 'declining'
        // will continue to be marked as declining until capacity ratio exceeds 83%.
        compareRatioTo = 0.83;
    } else {
        compareRatioTo = 0.80;
    }

    time_t currentTime = 0;
    bool canCompareTime = true;

    struct timeval t;
    // retrieve current time
    if (gettimeofday(&t, NULL) == -1) {
        canCompareTime = false; // do not use 7-day observation period.
    }
    else {
        currentTime = t.tv_sec;
    }

    if (capRatio > 1.2) {
        // Poor|Perm Failure = max-capacity is more than 1.2x of the design-capacity.
        batteryHealthCond = kIOPSPermanentFailureValue;

        if (strncmp(batteryHealth, kIOPSPoorValue, sizeof(kIOPSPoorValue))) {
            logASLBatteryHealthChanged(kIOPSPoorValue, batteryHealth, kIOPSPermanentFailureValue);
            batteryHealth = kIOPSPoorValue;
        }
        if (b->hasLowCapRatio == false) {
            b->hasLowCapRatio = true;
            _setLowCapRatioTime(b->batterySerialNumber, true, 0);
        }
    } else if (capRatio >= compareRatioTo) {
        // normal cap ratio
        if (!b->lowCapRatioSinceTime ||
            (canCompareTime && (currentTime - b->lowCapRatioSinceTime) <= SECONDS_IN_WEEK)) {
            // normal within observation period
            if (b->hasLowCapRatio) {
                // battery reverted back to normal cap ratio within observation period
                b->markedDeclining = 0;
                b->hasLowCapRatio = false;
                _setLowCapRatioTime(b->batterySerialNumber, false, currentTime);
            }
        } else if (canCompareTime) {
            // normal cap ratio after observation period was satisfied before => keep at Fair
            if (!strncmp(batteryHealth, kIOPSGoodValue, sizeof(kIOPSGoodValue))) {
                logASLBatteryHealthChanged(kIOPSFairValue, batteryHealth, kIOPSCheckBatteryValue);
                batteryHealth = kIOPSFairValue;
            }
        }
    } else {
        // low cap ratio (capration < compareRatioTo)
        if (b->hasLowCapRatio == false) {
            b->hasLowCapRatio = true;
            b->lowCapRatioSinceTime = currentTime;
            _setLowCapRatioTime(b->batterySerialNumber, true, currentTime);
        }
        // mark as declining to use hysteresis.
        b->markedDeclining = 1;

        // battery health status must be confirmed over a 7-day observation period
        if (canCompareTime && (currentTime - b->lowCapRatioSinceTime <= SECONDS_IN_WEEK)) {
            // 7-day observation period is not complete, maintain current health level
        }
        else if (canCompareTime) {
            // the 7-day observation period is complete: set the kIOPSBatteryHealthKey to Fair/Poor/Check
            if (cyclesExceedStandard) {
                if (capRatio >= 0.50) {
                    // Fair = ExceedingCycles && CapRatio >= 50% && CapRatio < 80%
                    if (!strncmp(batteryHealth, kIOPSGoodValue, sizeof(kIOPSGoodValue))) {
                        logASLBatteryHealthChanged(kIOPSFairValue, batteryHealth, kIOPSCheckBatteryValue);
                        batteryHealth = kIOPSFairValue;
                    }
                } else {
                    // Poor = ExceedingCycles && CapRatio < 50%
                    if (strncmp(batteryHealth, kIOPSPoorValue, sizeof(kIOPSPoorValue))) {
                        logASLBatteryHealthChanged(kIOPSPoorValue, batteryHealth, kIOPSCheckBatteryValue);
                        batteryHealth = kIOPSPoorValue;
                    }
                }
                // HealthCondition == CheckBattery to distinguish the Fair & Poor
                // cases from permanent failure (above), where
                // HealthCondition == PermanentFailure
                batteryHealthCond = kIOPSCheckBatteryValue;
            } else {
                // Check battery = NOT ExceedingCycles && CapRatio < 80%
                if (strncmp(batteryHealth, kIOPSCheckBatteryValue, sizeof(kIOPSCheckBatteryValue))) {
                    logASLBatteryHealthChanged(kIOPSCheckBatteryValue, batteryHealth, "");
                    batteryHealth = kIOPSCheckBatteryValue;
                }
            }
        }
    }

exit:
    bh = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, batteryHealth, kCFStringEncodingUTF8, kCFAllocatorNull);
    if (bh) {
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey), bh);
        CFRelease(bh);
    }
    bhc = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, batteryHealthCond, kCFStringEncodingUTF8, kCFAllocatorNull);
    if (bhc) {
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthConditionKey), bhc);
        CFRelease(bhc);
    }

    return;
}

static bool vactEnabled = true;
static bool vactSupported = true;

static bool getVactState(void)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    return vactEnabled;
}

static bool isVactSupported(void)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    return vactSupported;
}

#if TARGET_CPU_ARM64
static void updateVactState(void)
{
    return;
}
#else
static void updateVactState(void)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    if (!IOPMFeatureIsAvailable(CFSTR(kIOPMVact), NULL)) {
        INFO_LOG("VAC-T unsupported\n");
        vactSupported = false;
        vactEnabled = false;
        goto out;
    }

    @autoreleasepool {
        NSDictionary * dict = [(NSDictionary *)IOPMCopySystemPowerSettings() autorelease];
        if (!dict[@kIOPMVact]) {
            // default case, no opt out
            goto out;
        }

        vactEnabled = ![dict[@kIOPMVact] boolValue];
    }

out:
    return;
}
#endif

static void (^energyPrefsNotificationHandler)(void) =  ^{
    int ncc = 0;
    int nccAlt = 0;
    int count = 0;
    int dayCount = 0;
    int capacityUI;
    int waitForFC = 0;
    int fccSim[vactModesCount] = { 0 };
    bool legacyBattData = false;
    CFDictionaryRef batteryData = NULL;

    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);
    int prevVactState = getVactState();
    updateVactState();

    IOPMBattery **b = _batteries();
    if (!b || !b[0] || !isA_CFDictionary(b[0]->properties) || !isA_CFString(batteryTimeRemaining_getBatterySerialNumber()) || !b[0]->designCap) {
        ERROR_LOG("Invalid battery data\n");
        return;
    }

    CFMutableDictionaryRef dict = readBatteryHealthPersistentData();
    if (!dict) {
        goto out;
    }

    CFDictionaryGetIntValue(dict, CFSTR(kMitigatedNominalCapacityPercentage), ncc);
    CFDictionaryGetIntValue(dict, CFSTR(kUnMitigatedNominalCapacityPercentage), nccAlt);
    CFDictionaryGetIntValue(dict, CFSTR(kFccAvgHistoryCount), count);
    CFDictionaryGetIntValue(dict, CFSTR(kFccDaySampleCount), dayCount);
    CFDictionaryGetIntValue(dict, CFSTR(kWaitForFCState), waitForFC);
    if (!count && !dayCount) {
        // very first run, don't interfere with original algorithm yet
        goto out;
    }

    if (!ncc || !nccAlt) {
        goto out;
    }

    batteryData = CFDictionaryGetValue(b[0]->properties, CFSTR(kAsbBatteryDataKey));
    if (batteryData) {
        CFDictionaryGetIntValue(batteryData, CFSTR(kAsbFccComp1Key), fccSim[vactModeDisabled]);
        CFDictionaryGetIntValue(batteryData, CFSTR(kAsbFccComp2Key), fccSim[vactModeEnabled]);
    }
    legacyBattData = !fccSim[vactModeDisabled] || !fccSim[vactModeEnabled];

    if ((vactModeEnabled == prevVactState) && (vactModeDisabled == getVactState()) && !waitForFC && legacyBattData && isVactSupported()) {
        waitForFC = (int)true;
    }

    capacityUI = vactModeEnabled == getVactState() ? ncc : nccAlt;
    capacityUI += rawToNominal(kSmartBattReserve_mAh, b[0]->designCap);
    CFDictionarySetIntValue(dict, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), capacityUI);
    CFDictionarySetIntValue(dict, CFSTR(kWaitForFCState), waitForFC);
    writeBatteryHealthPersistentData(dict);

out:
    return;
};
#endif // TARGET_OS_OSX

bool isFullyCharged(IOPMBattery *b)
{
    bool is_charged = false;

    if (!b) return false;

#if TARGET_OS_IOS || TARGET_OS_WATCH
    is_charged = !!b->fullyCharged;
#else // TARGET_OS_IOS || TARGET_OS_WATCH
    // Set IsCharged if capacity >= 95% 
    // - Some portables will not initiate a battery charge if AC is
    //   connected when copacity is >= 95%.
    // - We consider > 95% to be fully charged; the battery will not charge
    //   any higher until AC is unplugged and re-attached.
    // - IsCharged should be true when the external power adapter LED is Green;
    //   should be false when the external power adapter LED is Orange.

    if (b->isPresent && (0 != b->maxCap)) {
            is_charged = ((100*b->currentCap/b->maxCap) >= 95);
    }
#endif // TARGET_OS_IOS || TARGET_OS_WATCH

    return is_charged;
}

/*
 * Implicit argument: All the global variables that track battery state
 */
static CFDictionaryRef packageKernelPowerSource(IOPMBattery *b, PSStruct *ps)
{
    CFNumberRef     n, n0;
    CFMutableDictionaryRef  mDict = NULL;
    int             temp;
    int             minutes;
    int             set_capacity, set_charge;
    int             psID;

    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if (!b) {
        IOPMBattery **batts = _batteries();
        b = batts[0];
    }

    // Create the battery info dictionary
    mDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if(!mDict)
        return NULL;

    // Does the battery provide its own time remaining estimate?
    CFDictionarySetValue(mDict, CFSTR("Battery Provides Time Remaining"), kCFBooleanTrue);

    // Was there an error/failure? Set that.
    if (b->failureDetected) {
        CFDictionarySetValue(mDict, CFSTR(kIOPSFailureKey), b->failureDetected);
    }

    // Is there a charging problem?
    if (b->chargeStatus) {
        CFDictionarySetValue(mDict, CFSTR(kIOPMPSBatteryChargeStatusKey), b->chargeStatus);
    }

    // Type = "InternalBattery", and "Transport Type" = "Internal"
    CFDictionarySetValue(mDict, CFSTR(kIOPSTransportTypeKey), CFSTR(kIOPSInternalType));
    CFDictionarySetValue(mDict, CFSTR(kIOPSTypeKey), CFSTR(kIOPSInternalBatteryType));

    // Set Power Source State to AC/Battery
    CFDictionarySetValue(mDict, CFSTR(kIOPSPowerSourceStateKey),
                            (b->externalConnected ? CFSTR(kIOPSACPowerValue):CFSTR(kIOPSBatteryPowerValue)));

#if TARGET_OS_IPHONE
    CFDictionarySetValue(mDict, CFSTR(kIOPSRawExternalConnectivityKey),
                            (b->rawExternalConnected ? kCFBooleanTrue : kCFBooleanFalse));
   // Show Charging UI if there is a UPS with non-zero capacity remaining attached
    bool caseConnected = false;
    bool lightningBatteryPackConnected = false;
    CFDictionaryRef bcase = getPSByType(CFSTR(kIOPSPrivateBatteryCaseType));
    if (bcase != NULL) {
        int caseCapacity = 0;
        CFDictionaryGetIntValue(bcase, CFSTR(kIOPSCurrentCapacityKey), caseCapacity);

        // Check max capacity as well, to ensure remaining capacity is rational
        int maxCapacity = 0;
        CFDictionaryGetIntValue(bcase, CFSTR(kIOPSMaxCapacityKey), maxCapacity);

        if (maxCapacity && maxCapacity >= caseCapacity) {
            int caseSoc = 100 * caseCapacity / maxCapacity;

            if (caseSoc) {
                caseConnected = true;
            }
        }

        // Check VID/PID to determine if this is a lightning battery pack that shows up as a battery case
        // but still needs to play an audible chime
        int vid = 0, pid = 0;
        CFDictionaryGetIntValue(bcase, CFSTR(kIOPSVendorIDKey), vid);
        CFDictionaryGetIntValue(bcase, CFSTR(kIOPSProductIDKey), pid);

        lightningBatteryPackConnected = (vid == 0x291A && pid == 0x0195);
    }

    b->showChargingUI = b->rawExternalConnected || caseConnected;
    CFDictionarySetValue(mDict, CFSTR(kIOPSShowChargingUIKey),
                            (b->showChargingUI ? kCFBooleanTrue : kCFBooleanFalse));

    b->playChargingChime = b->externalConnected || lightningBatteryPackConnected;
    CFDictionarySetValue(mDict, CFSTR(kIOPSPlayChargingChimeKey),
                         (b->playChargingChime ? kCFBooleanTrue : kCFBooleanFalse));
#else // TARGET_OS_IPHONE
    // Battery provided serial number
    if (b->batterySerialNumber) {
        CFDictionarySetValue(mDict, CFSTR(kIOPSHardwareSerialNumberKey), b->batterySerialNumber);
    }
    // Set Amperage
    n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &b->avgAmperage);
    if(n) {
        CFDictionarySetValue(mDict, CFSTR(kIOPSCurrentKey), n);
        CFRelease(n);
    }
#endif // TARGET_OS_IPHONE

    // round charge and capacity down to a % scale
    if(0 != b->maxCap)
    {
        set_capacity = 100;
        set_charge = b->swCalculatedPR;

#if !TARGET_OS_IPHONE
        if( (100 == set_charge) && b->isCharging)
        {
            // We will artificially cap the percentage to 99% while charging
            // Batteries may take 10-20 min beyond 100% of charging to
            // relearn their absolute maximum capacity. Leave cap at 99%
            // to indicate we're not done charging. (4482296, 3285870)
            set_charge = 99;
        }
#endif // !TARGET_OS_IPHONE
    } else {
        // Bad battery or bad reading => 0 capacity
        set_capacity = set_charge = 0;
    }

    // Set maximum capacity
    n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &set_capacity);
    if(n) {
        CFDictionarySetValue(mDict, CFSTR(kIOPSMaxCapacityKey), n);
        CFRelease(n);
    }

    // Set current charge
    n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &set_charge);
    if(n) {
        CFDictionarySetValue(mDict, CFSTR(kIOPSCurrentCapacityKey), n);
        CFRelease(n);
    }

    // Set isPresent flag
    CFDictionarySetValue(mDict, CFSTR(kIOPSIsPresentKey),
                b->isPresent ? kCFBooleanTrue:kCFBooleanFalse);

    minutes = b->swCalculatedTR;

    temp = 0;
    n0 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &temp);

    if( !b->isPresent ) {
        // remaining time calculations only have meaning if the battery is present
        CFDictionarySetValue(mDict, CFSTR(kIOPSIsChargingKey), kCFBooleanFalse);
        CFDictionarySetValue(mDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
        CFDictionarySetValue(mDict, CFSTR(kIOPSTimeToEmptyKey), n0);
    } else {
        // There IS a battery installed.
        if(b->isCharging) {
            // Set _isCharging to True
            CFDictionarySetValue(mDict, CFSTR(kIOPSIsChargingKey), kCFBooleanTrue);
            // Set IsFinishingCharge
            CFDictionarySetValue(mDict, CFSTR(kIOPSIsFinishingChargeKey),
                    (b->maxCap && (99 <= (100*b->currentCap/b->maxCap))) ? kCFBooleanTrue:kCFBooleanFalse);
            n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &minutes);
            if(n) {
                CFDictionarySetValue(mDict, CFSTR(kIOPSTimeToFullChargeKey), n);
                CFRelease(n);
            }
            CFDictionarySetValue(mDict, CFSTR(kIOPSTimeToEmptyKey), n0);
        } else {
            // Not Charging
            // Set _isCharging to False
            CFDictionarySetValue(mDict, CFSTR(kIOPSIsChargingKey), kCFBooleanFalse);
            // But are we plugged in?
            if(b->externalConnected)
            {
                // plugged in but not charging == fully charged
                CFDictionarySetValue(mDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
                CFDictionarySetValue(mDict, CFSTR(kIOPSTimeToEmptyKey), n0);

                CFDictionarySetValue(mDict, CFSTR(kIOPSIsChargedKey),
                    isFullyCharged(b) ? kCFBooleanTrue:kCFBooleanFalse);
            } else {
                // not charging, not plugged in == d_isCharging
                n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &minutes);
                if(n) {
                    CFDictionarySetValue(mDict, CFSTR(kIOPSTimeToEmptyKey), n);
                    CFRelease(n);
                }
                CFDictionarySetValue(mDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
            }
        }
    }
    CFRelease(n0);

#if TARGET_OS_OSX
    // Set health & confidence
    _setBatteryHealthData(mDict, b);
#elif TARGET_OS_IOS || TARGET_OS_WATCH
    // Trigger battery health calculation on cycle count change
    static int prevCycleCount = -1;
    if (b->cycleCount != prevCycleCount) {
        CFMutableDictionaryRef  outDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (outDict) {
            _setBatteryHealthData(outDict, b);
            CFRelease(outDict);
            prevCycleCount = b->cycleCount;
        }
    }
#endif

    // Set name
    if(b->name) {
        CFDictionarySetValue(mDict, CFSTR(kIOPSNameKey), b->name);
    } else {
        CFDictionarySetValue(mDict, CFSTR(kIOPSNameKey), CFSTR("Unnamed"));
    }

    // Set ID (UPS psID gets set by upsd)
    if (ps->psType != kPSTypeUPS) {
        psID = MAKE_UNIQ_SOURCE_ID(ps->pid, ps->psid);

        n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &psID);
        if (n) {
            CFDictionarySetValue(mDict, CFSTR(kIOPSPowerSourceIDKey), n);
            CFRelease(n);
        }
    }

    return mDict;
}

__private_extern__ void sendAdapterDetails(xpc_object_t remoteConnection, xpc_object_t msg)
{
    if (!remoteConnection || !msg) {
        ERROR_LOG("Invalid parameters. remoteConnection:%@ msg:%@", remoteConnection, msg);
        return;
    }

    xpc_object_t respMsg = xpc_dictionary_create_reply(msg);
    if (respMsg == NULL) {
        ERROR_LOG("Failed to create xpc object to send response\n");
        return;
    }

    dispatch_sync(batteryTimeRemainingQ, ^() {
        xpc_object_t respData = NULL;
        respData = _CFXPCCreateXPCObjectFromCFObject(adapterDetails);

        xpc_dictionary_set_value(respMsg, kPSAdapterDetails, respData);
        xpc_connection_send_message(remoteConnection, respMsg);

        DEBUG_LOG("Returned adapter details dictionary %{public}@\n", adapterDetails);
        if (respData) {
            xpc_release(respData);
        }
        xpc_release(respMsg);
    });
}

#if TARGET_OS_OSX
__private_extern__ void getBatteryHealthPersistentData(xpc_object_t remoteConnection, xpc_object_t msg)
{
    if (!remoteConnection || !msg) {
        ERROR_LOG("Invalid parameters. remoteConnection:%@ msg:%@", remoteConnection, msg);
        return;
    }

    xpc_object_t respMsg = xpc_dictionary_create_reply(msg);
    if (respMsg == NULL) {
        ERROR_LOG("Failed to create xpc object to send response\n");
        return;
    }

    if (!isSenderEntitled(remoteConnection, CFSTR("com.apple.private.iokit.batteryTester"), true)) {
        ERROR_LOG("Ignoring request for battery health persistent data from unprivileged sender\n");
        xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnNotPrivileged);
        xpc_connection_send_message(remoteConnection, respMsg);
        xpc_release(respMsg);
        return;
    }

    dispatch_async(batteryTimeRemainingQ, ^() {
        if (isSenderEntitled(remoteConnection, CFSTR("com.apple.private.iokit.batteryTester"), true)) {
            xpc_object_t respData = NULL;
            CFMutableDictionaryRef dict = readBatteryHealthPersistentData();
            respData = _CFXPCCreateXPCObjectFromCFObject(dict);

            xpc_dictionary_set_value(respMsg, "readBatteryHealthPersistentData", respData);
            DEBUG_LOG("Returned battery health persistent data %{public}@\n", dict);
            if (respData) {
                xpc_release(respData);
            }
        }
        else {
            ERROR_LOG("Ignoring request for battery health persistent data from unprivileged sender\n");
            xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnNotPrivileged);
        }

        xpc_connection_send_message(remoteConnection, respMsg);
        xpc_release(respMsg);
    });
}
#endif // TARGET_OS_OSX

/**** User-space power source code lives below here ********************************/
/***********************************************************************************/
/***********************************************************************************/
/***********************************************************************************/

STATIC PSStruct *iops_newps(int pid, int psid)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    // Find the first empty slot in gPSList
    int i = kPSMaxCount;
    if (psid == kSpecialInternalBatteryID) {
        // Reserve 0 for internal battery
        i = 0;
    }
    else {
        for (i=1; i<kPSMaxCount; i++) {
            if (0 == gPSList[i].psid) {
                break;
            }
        }
    }
    if (i < kPSMaxCount) {
        bzero(&gPSList[i], sizeof(PSStruct));
        gPSList[i].pid = pid;
        gPSList[i].psid = psid;
        return &gPSList[i];
    }

    return NULL;
}

STATIC PSStruct *iopsFromPSID(int _pid, int _psid)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    for (int i=0; i<kPSMaxCount; i++) {
        if (gPSList[i].psid == _psid && gPSList[i].pid == _pid) {
            return &gPSList[i];
        }
    }

    return NULL;
}

static CFDictionaryRef getPSByType(CFStringRef type)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    for (int i=0; i<kPSMaxCount; i++) {
        if (!isA_CFDictionary(gPSList[i].description)) {
            continue;
        }

        CFStringRef ps_type = CFDictionaryGetValue(gPSList[i].description, CFSTR(kIOPSTypeKey));
        if (isA_CFString(ps_type) && CFEqual(ps_type, type)) {
            return gPSList[i].description;
        }
    }
    return NULL;
}

static CFDictionaryRef getActiveUPSDictionary_sync(void)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    return getPSByType(CFSTR(kIOPSUPSType));
}

// Returns active UPS dictionary or NULL.
// Caller must release the returned object.
__private_extern__ CFDictionaryRef getActiveUPSDictionary(void)
{
    __block CFDictionaryRef ups;
    dispatch_sync(batteryTimeRemainingQ, ^() {
        ups = getActiveUPSDictionary_sync();
        if (ups) {
            CFRetain(ups);
        }
    });

    return ups;
}

static int getActivePSType_sync(void)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    int rc = kIOPSProvidedByAC;
    CFDictionaryRef activeBattery = getActiveBatteryDictionary();
    CFDictionaryRef activeUPS = getActiveUPSDictionary_sync();
    CFStringRef     ps_state = NULL;

    /* if (!activeBattery) is testing for whether batteries are supported on
     * this system at all, e.g. mobile vs desktop. */
    if (!activeBattery) {
        if(!activeUPS) {
            // no batteries, no UPS -> AC Power
            rc = kIOPSProvidedByAC;
        } else {
            ps_state = CFDictionaryGetValue(activeUPS,
                                            CFSTR(kIOPSPowerSourceStateKey));
            if (ps_state && CFEqual(ps_state, CFSTR(kIOPSACPowerValue))) {
                // no batteries, yes UPS, UPS is running off of AC power -> AC Power
                rc = kIOPSProvidedByAC;
            } else if(ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue))) {
                // no batteries, yes UPS, UPS is running drawing its Battery power -> UPS Power
                rc = kIOPSProvidedByExternalBattery;
            }
        }
    } else {
        ps_state = CFDictionaryGetValue(activeBattery,
                                        CFSTR(kIOPSPowerSourceStateKey));
        if (ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue))) {
            // Yes batteries, yes running on battery power -> Battery power
            rc = kIOPSProvidedByBattery;
        } else {
            // batteries are on AC power. let's check if UPS is present.
            if (!activeUPS) {
                // yes batteries on AC power, no UPS present -> AC Power
                rc = kIOPSProvidedByAC;
            } else {
                ps_state = CFDictionaryGetValue(activeUPS, CFSTR(kIOPSPowerSourceStateKey));
                if (ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue))) {
                    // yes batteries on AC power, UPS is on its battery -> UPS Power
                    rc = kIOPSProvidedByExternalBattery;
                } else if (ps_state && CFEqual(ps_state, CFSTR(kIOPSACPowerValue))) {
                    // yes batteries on AC Power, UPS is drawing AC Power -> AC Power
                    rc = kIOPSProvidedByAC;
                }
            }
        }
    }

    return rc;
}

__private_extern__ int getActivePSType(void)
{
    __block int rc = kIOPSProvidedByAC;
#if !XCTEST
    dispatch_sync(batteryTimeRemainingQ, ^() {
        rc = getActivePSType_sync();
    });
#else
    rc = getActivePSType_sync();
#endif

    return rc;
}

/***********************************************************************************/
// MIG handler - back end for IOKit API IOPSCreatePowerSource
#define PSID_MIN    5000
kern_return_t _io_ps_new_pspowersource(
    mach_port_t                 server __unused,
    audit_token_t               token,
    int                         *psid,              // out
    int                         *result)
{
    static int                  gPSID = PSID_MIN;
    int                         callerPID;
    __block PSStruct            *ps;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL,
                        &callerPID, NULL, NULL);

    *result = kIOReturnError;

    dispatch_barrier_sync(batteryTimeRemainingQ, ^() {
        if (gPSID == INT_MAX) {
            gPSID = PSID_MIN;
        }

        ps = iops_newps(callerPID, gPSID);
        if (!ps) {
            *result = kIOReturnNoSpace;
            return;
        }

        *psid = gPSID++;

        ps->procdeathsrc = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC,
                                                    callerPID,
                                                    DISPATCH_PROC_EXIT,
                                                    batteryTimeRemainingQ);

        /* Setup automatic cleanup if client process dies */
        dispatch_source_set_cancel_handler(ps->procdeathsrc, ^{
            /*
             * When the client process dies, remove
             * this power source and stop showing it to IOPS API clients.
             */

            if (ps->psType == kPSTypeAccessory) {
                BatteryTimeRemaining_notify_post(kIOPSAccNotifyTimeRemaining);
                BatteryTimeRemaining_notify_post(kIOPSAccNotifyAttach);
            }
            else {
                BatteryTimeRemaining_notify_post(kIOPSNotifyTimeRemaining);
                BatteryTimeRemaining_notify_post(kIOPSNotifyAttach);
            }
            INFO_LOG("Posted notifications for loss of power source id %d\n", ps->psid);
            if (ps->procdeathsrc) {
                dispatch_release(ps->procdeathsrc);
            }
            if (ps->description) {
                CFRelease(ps->description);
            }
            bzero(ps, sizeof(PSStruct));

#if TARGET_OS_IPHONE
            // Update the internal battery's state in light of
            // any other power soruces being removed (which may
            // impact ShowChargingUI, ExternalConnected, etc)
            // before we publish state. This also causes all
            // power sources to be published.
            kernelPowerSourcesDidChange(kInternalBattery);
#else // TARGET_OS_IPHONE
            HandlePublishAllPowerSources();
#endif // TARGET_OS_IPHONE
        });

        dispatch_source_set_event_handler(ps->procdeathsrc, ^{
            dispatch_source_cancel(ps->procdeathsrc);
        });

        dispatch_resume(ps->procdeathsrc);

        *result = kIOReturnSuccess;
        INFO_LOG("Created new power source id %d for pid %d\n", *psid, callerPID);
    });

    return KERN_SUCCESS;
}

/***********************************************************************************/
// MIG handler - back end for IOKit API IOPSSetPowerSourceDetails

kern_return_t _io_ps_update_pspowersource(
    mach_port_t         server __unused,
    audit_token_t       token,
    int                 psid,
    vm_offset_t         details_ptr,
    mach_msg_type_number_t  details_len,
    int                 *return_code)
{
    dispatch_sync(batteryTimeRemainingQ, ^() {
        CFMutableDictionaryRef     details = NULL;
        int                 callerPID;
        CFStringRef         psTypeStr = NULL;
        CFNumberRef         psIDKey = NULL;
        int                 psID = 0;

        audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL,
                            &callerPID, NULL, NULL);

        *return_code = kIOReturnError;

        details = (CFMutableDictionaryRef)IOCFUnserialize((const char *)details_ptr, NULL, 0, NULL);

        if (!isA_CFDictionary(details)) {
            *return_code = kIOReturnBadArgument;
            if (details) {
                CFRelease(details);
            }
            goto exit;
        }

        PSStruct *next = iopsFromPSID(callerPID, psid);
        if (!next) {
            ERROR_LOG("Failed to find the power source for psid 0x%x from pid %d\n", psid, callerPID);
            *return_code = kIOReturnNotFound;
            CFRelease(details);
            goto exit;
        }

        psIDKey = CFDictionaryGetValue(details, CFSTR(kIOPSPowerSourceIDKey));
        if (!isA_CFNumber(psIDKey)) {
            psID = MAKE_UNIQ_SOURCE_ID(next->pid, next->psid);

            psIDKey = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &psID);
            if (psIDKey) {
                CFDictionarySetValue(details, CFSTR(kIOPSPowerSourceIDKey), psIDKey);
                CFRelease(psIDKey);
            }
        }

        if (next->psType == kPSTypeUnknown) {
            psTypeStr = CFDictionaryGetValue(details, CFSTR(kIOPSTypeKey));
            if (isA_CFString(psTypeStr)) {
                if (CFStringCompare(psTypeStr, CFSTR(kIOPSAccessoryType), 0) == kCFCompareEqualTo)
                    next->psType = kPSTypeAccessory;
                else if ((CFStringCompare(psTypeStr, CFSTR(kIOPSUPSType), 0) == kCFCompareEqualTo)
#if TARGET_OS_IPHONE
                        ||  (CFStringCompare(psTypeStr, CFSTR(kIOPSPrivateBatteryCaseType), 0) == kCFCompareEqualTo)
#endif
                        )
                    next->psType = kPSTypeUPS;
                else if (CFStringCompare(psTypeStr, CFSTR(kIOPSInternalBatteryType), 0) == kCFCompareEqualTo)
                    next->psType = kPSTypeIntBattery;
            }
        }

        INFO_LOG("Received power source(psid:%d) update from pid %d: %@\n", psid, callerPID, details);
        if ((next->psType == kPSTypeIntBattery) || (next->psType == kPSTypeUPS)) {
            if (next->description) {
                CFRelease(next->description);
            }
            else {
                // This is the first update for this source
                BatteryTimeRemaining_notify_post(kIOPSNotifyAttach);
                INFO_LOG("Posted \"%s\" for new power source id %d\n", kIOPSNotifyAttach, psid);
            }
            next->description = details;
            dispatch_async(batteryTimeRemainingQ, ^() {
#if TARGET_OS_IPHONE
                // Update the internal battery's state in light of
                // any other power soruces changing (which may
                // impact ShowChargingUI, ExternalConnected, etc)
                // before we publish state. This also causes all
                // power sources to be published.
                kernelPowerSourcesDidChange(kInternalBattery);
#else // TARGET_OS_IPHONE
                HandlePublishAllPowerSources();
#endif // TARGET_OS_IPHONE
            });
            *return_code = kIOReturnSuccess;
        }
        else if (next->psType == kPSTypeAccessory) {
           *return_code = HandleAccessoryPowerSources(next, details);
        } else {
            CFRelease(details);
        }

exit:
        vm_deallocate(mach_task_self(), details_ptr, details_len);
    });

    return 0;
}

kern_return_t _io_ps_release_pspowersource(
    mach_port_t         server __unused,
    audit_token_t       token,
    int                 psid)
{
    int                         callerPID;
    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL,
                        &callerPID, NULL, NULL);

    dispatch_async(batteryTimeRemainingQ, ^() {
        INFO_LOG("Releasing power source id = %d\n", psid);

        PSStruct *toRelease = iopsFromPSID(callerPID, psid);
        if (toRelease) {
            dispatch_source_cancel(toRelease->procdeathsrc);
        }
    });

    return 0;
}

static CFDictionaryRef copyWithBatteryHealthData(audit_token_t token, CFDictionaryRef batteryData)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    CFMutableDictionaryRef updatedData = NULL;
    if (!auditTokenHasEntitlement(token, CFSTR("com.apple.private.iokit.batterydata"))) {
        // Just return copy of incoming battery data
        CFRetain(batteryData);
        return batteryData;
    }

#if TARGET_OS_IOS || TARGET_OS_WATCH
    IOPMBattery **_battArray = NULL, *batt = NULL;
    updatedData = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, batteryData);

    if ((_battArray = _batteries()) && (batt = _battArray[0])) {
        _setBatteryHealthData(updatedData, batt);
    }
    else {
        ERROR_LOG("Battery Data not found");
    }
#elif TARGET_OS_OSX
    struct keys {
        CFStringRef keyStr;
        int keyVal;
    } bhKeys[] = {
        {CFSTR(kIOPSBatteryHealthServiceFlagsKey), kBHSvcFlagNoBatteryData},
        {CFSTR(kIOPSBatteryHealthServiceStateKey), kBHSvcStateNotDeterminable},
        {CFSTR(kIOPSBatteryHealthMaxCapacityPercent), -1},
        {CFSTR(kMitigatedNominalCapacityPercentage), -1},
        {CFSTR(kUnMitigatedNominalCapacityPercentage), -1},
        {CFSTR("version"), 0},
        {CFSTR(kUnMitigatedNominalCapacityPercentage), -1},
        {CFSTR(kMitigatedNominalCapacityPercentage), -1},
        {CFSTR(kUnMitigatedNominalCapacityAvg), -1},
        {CFSTR(kMitigatedNominalCapacityAvg), -1},
        {CFSTR(kFccAvgHistoryCount), -1},
        {CFSTR(kFccDaySampleCount), -1},
        {CFSTR(kMitigatedFccDaySampleAvg), -1},
        {CFSTR(kUnMitigatedFccDaySampleAvg), -1},
        {CFSTR(kWaitForFCState), -1},
    };

    CFMutableDictionaryRef dict = readBatteryHealthPersistentData();
    if (dict != nil) {
        for (int i = 0; i < ARRAY_SIZE(bhKeys); i++) {
            CFDictionaryGetIntValue(dict, bhKeys[i].keyStr, bhKeys[i].keyVal);
        }
        CFRelease(dict);
    }

    updatedData = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, batteryData);
    if (updatedData) {
        for (int i = 0; i < ARRAY_SIZE(bhKeys); i++) {
            CFDictionarySetIntValue(updatedData, bhKeys[i].keyStr, bhKeys[i].keyVal);
        }
    }
#endif

    if (updatedData == nil) {
        CFRetain(batteryData);
        return batteryData;
    } else {
        return updatedData;
    }
}

kern_return_t _io_ps_copy_powersources_info(
    mach_port_t             server __unused,
    audit_token_t           token,
    int                     type,
    vm_offset_t             *ps_ptr,
    mach_msg_type_number_t  *ps_len,
    int                     *return_code)
{
    dispatch_sync(batteryTimeRemainingQ, ^() {
        CFMutableArrayRef   return_value = NULL;

        for (int i=0; i<kPSMaxCount; i++) {
            if (gPSList[i].description == NULL) {
                continue;
            }

            switch(type) {
            case kIOPSSourceInternal:
                if (gPSList[i].psType != kPSTypeIntBattery)
                    continue;
                break;

            case kIOPSSourceUPS:
                if (gPSList[i].psType != kPSTypeUPS)
                    continue;
                break;

            case kIOPSSourceInternalAndUPS:
                if ((gPSList[i].psType != kPSTypeIntBattery) && (gPSList[i].psType != kPSTypeUPS))
                    continue;
                break;

            case kIOPSSourceForAccessories:
                if (gPSList[i].psType != kPSTypeAccessory)
                    continue;
                break;

            case kIOPSSourceAll:
                break;

            default:
                continue;
            }

            if (!return_value) {
                return_value = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
            }
            if ((gPSList[i].psType == kPSTypeIntBattery) && gPSList[i].description) {
                CFDictionaryRef updatedData = copyWithBatteryHealthData(token, gPSList[i].description);
                CFArrayAppendValue(return_value, updatedData);
                CFRelease(updatedData);
            }
            else {
                CFArrayAppendValue(return_value, (const void *)gPSList[i].description);
            }

        }

        if (!return_value) {
            *ps_ptr = 0;
            *ps_len = 0;
        } else {
            CFDataRef   d = CFPropertyListCreateData(0, return_value,
                                                     kCFPropertyListBinaryFormat_v1_0,
                                                     0, NULL);
            CFRelease(return_value);

            if (d) {
                *ps_len = (mach_msg_type_number_t)CFDataGetLength(d);

                vm_allocate(mach_task_self(), (vm_address_t *)ps_ptr, *ps_len, TRUE);

                memcpy((void *)*ps_ptr, CFDataGetBytePtr(d), *ps_len);

                CFRelease(d);
            }
        }
        *return_code = kIOReturnSuccess;
    });

    return 0;
}

#if TARGET_OS_IPHONE
static bool CheckAccessoryLedChange(PSStruct *ps, CFDictionaryRef update)
{
        CFArrayRef old_leds = CFDictionaryGetValue(ps->description, CFSTR(kIOPSLEDsKey));
        CFArrayRef new_leds = CFDictionaryGetValue(update, CFSTR(kIOPSLEDsKey));

        if (!old_leds && !new_leds) {
            return false;
        }

        if (!!old_leds ^ !!new_leds) {
            return true;
        }

        size_t old_led_cnt = CFArrayGetCount(old_leds);
        size_t new_led_cnt = CFArrayGetCount(new_leds);

        // no entries
        if (!old_led_cnt && !new_led_cnt) {
            return false;
        }

        // notify if number of LEDs differs
        if (old_led_cnt != new_led_cnt) {
            return true;
        }

        // entries exist -> compare
        for (size_t i = 0; i < new_led_cnt; i++) {
            CFDictionaryRef old_led = CFArrayGetValueAtIndex(old_leds, i);
            CFDictionaryRef new_led = CFArrayGetValueAtIndex(new_leds, i);

            CFTypeRef old_value = CFDictionaryGetValue(old_led, CFSTR(kIOPSLedStateKey));
            CFTypeRef new_value = CFDictionaryGetValue(new_led, CFSTR(kIOPSLedStateKey));
            if (!!old_value ^ !!new_value) {
                return true;
            }
            if (old_value && new_value && !CFEqual(old_value, new_value)) {
                return true;
            }

            old_value = CFDictionaryGetValue(old_led, CFSTR(kIOPSLedColorKey));
            new_value = CFDictionaryGetValue(new_led, CFSTR(kIOPSLedColorKey));
            if (!!old_value ^ !!new_value) {
                return true;
            }
            if (old_value && new_value && !CFEqual(old_value, new_value)) {
                return true;
            }
        }

        return false;
}

static bool CheckAccessoryAdapterChange(PSStruct *ps, CFDictionaryRef update)
{
        CFDictionaryRef old_adapter = CFDictionaryGetValue(ps->description, CFSTR(kIOPMPSAdapterDetailsKey));
        CFDictionaryRef new_adapter = CFDictionaryGetValue(update, CFSTR(kIOPMPSAdapterDetailsKey));

        if (!old_adapter && !new_adapter) {
            return false;
        }

        if (!!old_adapter ^ !!new_adapter) {
            return true;
        }

        int old_fc = 0, new_fc = 0;
        CFDictionaryGetIntValue(old_adapter, CFSTR(kIOPSPowerAdapterFamilyKey), old_fc);
        CFDictionaryGetIntValue(new_adapter, CFSTR(kIOPSPowerAdapterFamilyKey), new_fc);
        if (old_fc != new_fc) {
            return true;
        }


        return false;
}
#endif // TARGET_OS_IPHONE

static IOReturn HandleAccessoryPowerSources(PSStruct *ps, CFDictionaryRef update)
{
    CFNumberRef     n = NULL;
    int  old_cap = 0, new_cap = 0;
    CFStringRef old_src = NULL, new_src = NULL;
#if TARGET_OS_IPHONE
    CFStringRef old_name, new_name;
    CFStringRef old_pname, new_pname;
    bool  old_exists, new_exists;
#endif

    /* update dictionary is validated by the caller */

    new_src = CFDictionaryGetValue(update, CFSTR(kIOPSPowerSourceStateKey));
    n = CFDictionaryGetValue(update, CFSTR(kIOPSCurrentCapacityKey));
    if (n) {
        CFNumberGetValue(n, kCFNumberIntType, &new_cap);
    }

    if (!new_src || !n) {
        ERROR_LOG("PS update is missing SourceState or Capacity\n");
        CFRelease(update);
        return kIOReturnBadArgument;
    }

    if (ps->description != NULL) {
        bool do_notify_ps = false;
        bool do_notify_tr = false;

        old_src = CFDictionaryGetValue(ps->description, CFSTR(kIOPSPowerSourceStateKey));
        if (old_src && CFStringCompare(new_src, old_src, 0) != kCFCompareEqualTo) {
            do_notify_ps = true;
        }

        n = CFDictionaryGetValue(ps->description, CFSTR(kIOPSCurrentCapacityKey));
        if (n) {
            CFNumberGetValue(n, kCFNumberIntType, &old_cap);
        }
        if (new_cap != old_cap) {
            do_notify_tr = true;
        }

        // notify if kIOPSIsChargingKey changes
        int old_is_charging = 0;
        int new_is_charging = 0;
        n = CFDictionaryGetValue(ps->description, CFSTR(kIOPSIsChargingKey));
        if (n) {
            CFNumberGetValue(n, kCFNumberIntType, &old_is_charging);
        }

        n = CFDictionaryGetValue(update, CFSTR(kIOPSIsChargingKey));
        if (n) {
            CFNumberGetValue(n, kCFNumberIntType, &new_is_charging);
        }

        if (old_is_charging != new_is_charging) {
            do_notify_ps = true;
        }

#if TARGET_OS_IPHONE
        old_name = new_name = NULL;
        old_exists = CFDictionaryGetValueIfPresent(ps->description, CFSTR(kIOPSNameKey), (const void **)&old_name);
        new_exists = CFDictionaryGetValueIfPresent(update, CFSTR(kIOPSNameKey), (const void **)&new_name);
        if ((old_exists != new_exists) ||
            (isA_CFString(old_name) && isA_CFString(new_name) &&
            !CFEqual(old_name, new_name))) {

            do_notify_ps = true; // Not the right notification
        }

        old_pname = new_pname = NULL;
        old_exists = CFDictionaryGetValueIfPresent(ps->description, CFSTR(kIOPSPartNameKey), (const void **)&old_pname);
        new_exists = CFDictionaryGetValueIfPresent(update, CFSTR(kIOPSPartNameKey), (const void **)&new_pname);
        if ((old_exists != new_exists) ||
            (isA_CFString(old_pname) && isA_CFString(new_pname) &&
            !CFEqual(old_pname, new_pname))) {

            do_notify_ps = true; // Not the right notification
        }

        // Notify for AirPod case LED changes (rdar://problem/37842910)
        if (CheckAccessoryLedChange(ps, update)) {
            do_notify_ps = true; // Not the right notification
        }

        if (CheckAccessoryAdapterChange(ps, update)) {
            do_notify_ps = true;
        }
#endif // TARGET_OS_IPHONE

        if (do_notify_ps) {
            BatteryTimeRemaining_notify_post(kIOPSAccNotifyPowerSource);
            INFO_LOG("Posted \"%s\" for power source id %d\n", kIOPSAccNotifyPowerSource, ps->psid);
        }

        if (do_notify_tr) {
            BatteryTimeRemaining_notify_post(kIOPSAccNotifyTimeRemaining);
            INFO_LOG("Posted \"%s\" for power source id %d\n", kIOPSAccNotifyTimeRemaining, ps->psid);
        }

        CFRelease(ps->description);
    }
    else {
        /* This is a new accessory with power source */
        BatteryTimeRemaining_notify_post(kIOPSAccNotifyAttach);
        BatteryTimeRemaining_notify_post(kIOPSAccNotifyTimeRemaining);
        INFO_LOG("Posted notifications for new power source id %d\n", ps->psid);
    }

    ps->description = update;
    return kIOReturnSuccess;
}

static int _batteryCountSync(void)
{
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    return ((int)physicalBatteriesCount);
}

__private_extern__ int _batteryCount(void)
{
    int __block ret;

    _internal_dispatch_assert_queue_not(batteryTimeRemainingQ);

#if !XCTEST
    dispatch_sync(batteryTimeRemainingQ, ^() {
            ret = _batteryCountSync();
    });
#else
    ret = _batteryCountSync();
#endif
    return ret;
}

static bool getPowerStateSync(PowerSources *source, uint32_t *percentage)
{
    IOPMBattery **batteries;
    int         batteryCount = 0;
    bool        ret = false;
    _internal_dispatch_assert_queue(batteryTimeRemainingQ);
    batteryCount = _batteryCountSync();
    if (0 < batteryCount) {
        int validBattCount = 0;
        uint32_t capPercent = 0;

        batteries = _batteries();
        for (int i = 0; i < batteryCount; i++) {
            if (batteries[i]->isPresent == false) {
                continue;
            }
            validBattCount++;
            if (0 != batteries[i]->maxCap) {
                capPercent += (batteries[i]->currentCap * 100) / batteries[i]->maxCap;
            }
        }
        if (validBattCount) {
            *source = batteries[0]->externalConnected ? kACPowered : kBatteryPowered;
            *percentage = capPercent;
            ret = true;
        }
    }

    return ret;
}

__private_extern__ bool getPowerState(PowerSources *source, uint32_t *percentage)
{
#ifdef XCTEST
    *source = xctPowerSource;
    *percentage = xctCapacity;
    return true;
#endif
    bool __block ret = false;

    // at powerd startup this may get called before prime()
    if (!batteryTimeRemainingQ) {
        return false;
    }

    _internal_dispatch_assert_queue_not(batteryTimeRemainingQ);

    *source = kACPowered;

    dispatch_sync(batteryTimeRemainingQ, ^() {
        ret = getPowerStateSync(source, percentage);
    });

    return ret;
}

static PowerSources _getPowerSourceSync(void)
{
   IOPMBattery      **batteries;
   _internal_dispatch_assert_queue(batteryTimeRemainingQ);
   if (_batteryCountSync() && (batteries = _batteries()) && (!batteries[0]->externalConnected) )
      return kBatteryPowered;
   else
      return kACPowered;
}

__private_extern__ PowerSources _getPowerSource(void)
{
   PowerSources __block ret;
#ifdef XCTEST
   return xctPowerSource;
#endif
   _internal_dispatch_assert_queue_not(batteryTimeRemainingQ);



   dispatch_sync(batteryTimeRemainingQ, ^() {
       ret = _getPowerSourceSync();
   });

   return ret;
}
