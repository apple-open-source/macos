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
#include <time.h>
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

#pragma mark - Battery Health Recalibration
/*
    Recalibration information is stored in the battery health dictionary under a
    key unique to each calibration performed
 
    These sub-dictionaries should only be cleared on a change of battery serial
 
    Structure
    bhDict {
        "Battery Serial" = "FOOBARBAZ"
        "Battery Service Flags" = 3
        "Battery Service State" = 0
        "Maximum Capacity Percent" = 103
        "CycleCount" = 44 // nccp_cc_filter only in top-level bhDict
 
        // New calibrations will be x+1
        "calibrationX" = {
            "baseline" = {
                // Any keys snapshotted at the start, e.g.
                "CycleCount" = 44
                ...
            }
            "snapshots" = {
                // Any keys snapshotted at a given interval, e.g. cycle count change
                "44": {
                    ...
                }
            }
            "calibrationFlags" = 0 // indicating information before/during recalibration
            ...
        }
    }
 */

typedef enum: unsigned long {
    kBHCalibrationFlagServiceBeforeCalibration =                (1UL << 0),
    kBHCalibrationFlagServiceDuringCalibration =                (1UL << 1),
    kBHCalibrationFlagNoServiceToServiceDuringCalibration =     (1UL << 2),
    kBHCalibrationFlagServiceAtEndOfCalibration =               (1UL << 3),
    kBHCalibrationFlagServiceToNoServiceDuringCalibration =     (1UL << 4),
    kBHCalibrationFlagCompleted =                               (1UL << 5),
    kBHCalibrationFlagThresholdLower =                          (1UL << 6),
    kBHCalibrationFlagSkipped =                                 (1UL << 7),
    kBHCalibrationFlagThresholdHigher =                         (1UL << 8), // Do not redefine or move, used by AST2
    kBHCalibrationFlagFailure =                                 (1UL << 9), // Telemetry only, all other systems should use kBHSvcStateCalibrationFailed
    kBHCalibrationFlagCalib1NotNeeded =                         (1UL << 10), // Mark if another round of calibration is not needed
    kBHCalibrationFlagCalib1Completed =                         (1UL << 11), // Mark if second round of calibration is completed
} kBHCalibrationFlags;
#define kBHCalibrationBaselineKey   "baseline"
#define kBHCalibrationSnapshotsKey  "snapshots"
#define kBHCalibrationExitTOTKey    "exitTotalOperatingTime"
#define kBHCalibrationSvcTOTKey     "svcTotalOperatingTime"
#pragma mark -

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

#if TARGET_OS_IOS || POWERD_IOS_XCTEST || TARGET_OS_WATCH
static void updateCalibration0Flags(CFMutableDictionaryRef bhData, CFDictionaryRef batteryProps,
                             IOPSBatteryHealthServiceState prevSvcState, IOPSBatteryHealthServiceFlags prevSvcFlags,
                             IOPSBatteryHealthServiceState currentSvcState, IOPSBatteryHealthServiceFlags currentSvcFlags);
#endif

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

static int64_t pfStatusOverrideValue = -1;

enum vactMode {
    vactModeDisabled = 0,
    vactModeEnabled,
    vactModesCount,
};

#define CAPACITY_NO_CHANGE  (0)
#define CAPACITY_FCC_CHANGE  (1 << 0)
#define CAPACITY_NCC_CHANGE  (1 << 1)
#define CAPACITY_SAMPLING_EPOCH_CHANGE  (1 << 2)
#define CAPACITY_SEED_CHANGE  (1 << 3)

#define FCC_SAMPLE_DROPPED_ZERO  (1 << 0)
#define FCC_SAMPLE_DROPPED_DUPLICATE  (1 << 1)
#define FCC_SAMPLE_DROPPED_TEMP  (1 << 2)
#define FCC_SAMPLE_DROPPED_DIS  (1 << 3)
#define FCC_SAMPLE_DROPPED_MASK (FCC_SAMPLE_DROPPED_ZERO | FCC_SAMPLE_DROPPED_DUPLICATE | FCC_SAMPLE_DROPPED_TEMP | FCC_SAMPLE_DROPPED_DIS)
#define NEGATIVE_TS (1 << 4)

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
#define NCC_GAMMA  (909)   // 11.5 day time constant, 24hr calendar time epoch

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
    unsigned int debug;
    int cycleCount;
    uint64_t ts;
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

static uint64_t getTimeInSecsSinceEpoch(void)
{
    struct timespec timeSpec = {0, 0};
    clock_gettime(CLOCK_REALTIME, &timeSpec);
    return timeSpec.tv_sec;
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

/*
 * Valid only for non Apple Silicon devices. A previous/legacy service recommendation doesn't mean anything for AS macs that start their
 * life with new algorithm present.
 */
static IOReturn _getLowCapRatioTime(CFStringRef batterySerialNumber,
                             boolean_t *hasLowCapRatio,
                             time_t *since)
{
    IOReturn                    ret         = kIOReturnError;

#if !TARGET_OS_IPHONE && !(TARGET_OS_OSX && TARGET_CPU_ARM64)
    CFNumberRef                 num         = NULL;
    CFDictionaryRef             dict        = NULL;

    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if (!hasLowCapRatio || !since) {
        return ret;
    }

    // return defaults if batterySerial appears to be missing/invalid
    *hasLowCapRatio = false;
    *since = 0;

    if (!isA_CFString(batterySerialNumber)) {
        return ret;
    }

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

#endif // !TARGET_OS_IPHONE && !(TARGET_OS_OSX && TARGET_CPU_ARM64)
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

    PowerSources oldPS = _getPowerSourceSync();

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
    evalTcpkaForPSChange(_getPowerSourceSync());
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
static void _discontinuityOccurred(bool isWake, bool percentageDiscontinuity)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    if (slew) {
        bzero(slew, sizeof(SlewStruct));
    }
    control.lastDiscontinuity = CFAbsoluteTimeGetCurrent();
    control.percentageDiscontinuity = percentageDiscontinuity;


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

        _discontinuityOccurred(true, true);
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
    const static CFTimeInterval     kUserVisibleMinFrequency = 55.0;
    const static uint64_t           kPollIntervalNS = 60ULL * NSEC_PER_SEC;

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
        if (percent <= kPercentThresholdFinal) {
            newWarningLevel = kIOPSLowBatteryWarningFinal;
        } else
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

            if (lowBatteryKey) {
                PMStoreSetValue(lowBatteryKey, newlevel );
            } else {
                ERROR_LOG("Failed to create lowBatteryKey\n");
            }

            CFRelease(newlevel);

            BatteryTimeRemaining_notify_post(kIOPSNotifyLowBattery);
            if (newWarningLevel != prevLoggedLevel) {
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

__private_extern__ bool batteryTimeRemaining_isOBCEngaged(void)
{
    __block bool ret = false;
    dispatch_sync(batteryTimeRemainingQ, ^() {
        IOPMBattery **b = _batteries();
        if (b && b[0] && b[0]->isPresent) {
            ret = !!b[0]->obcEngaged;
        }
    });
    return ret;
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

    changedStateBits = lastStateBits ^ currentStateBits;
    if (changedStateBits)
    {
        lastStateBits = currentStateBits;
        notify_set_state(control.psPercentChangeNotifyToken, currentStateBits);

        // Suppress notification for charging state changes
        ignoreBits = (kPSTimeRemainingNotifyChargingBit 
                                 |kPSTimeRemainingNotifyFullyChargedBit
                                 );
        if (changedStateBits & ~ignoreBits)
        {
            notify_post(kIOPSNotifyPercentChange);
            INFO_LOG("Battery capacity change posted(0x%llx). Capacity:%d Source:%{public}s\n",
                    currentStateBits, percentRemaining, isExternal ? "AC":"Batt");
        }
    }
}

__private_extern__ void kernelPowerSourcesDidChange(IOPMBattery *b)
{
    _internal_dispatch_assert_queue_barrier(batteryTimeRemainingQ);

    static int  _lastExternalConnected = -1;
    static int  _lastPercentRemaining = 100;
    static int  _lastIsCharging = -1;
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
        _discontinuityOccurred(false, true);
        control.needsNotifyAC = true;
    } else if (_lastIsCharging != b->isCharging) {
        _discontinuityOccurred(false, false);
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
    _lastIsCharging = b->isCharging;

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
    bool                        battcase_change = false;
    CFAbsoluteTime              bootUpdateTime = kCFAbsoluteTimeIntervalSince1970; // non-zero value

    ups = getActiveUPSDictionary_sync();

    bootUpdateTime = getASBMPropertyCFAbsoluteTime(CFSTR(kFullPathKey));
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

     // Notifiy PSLowPower of power sources change
    UPSLowPowerPSChange();
    PMSettingsPSChange();

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

    bool currentlyCalibrating = (svcFlags & kBHSvcFlagCurrentlyCalibrating);
    if (currentlyCalibrating) {
        INFO_LOG("calib: calculating svc option from 0x%x", svcFlags);
    }

    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey), prevSvcFlags);
    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey), prevSvcState);

    if (currentlyCalibrating) {
        // Don't carry over sticky bits if we're calibrating
        INFO_LOG("calib: clearing bits");
    } else {
        // Carry over the sticky bits of Service Flags
        svcFlags |= (prevSvcFlags & kBHSvcFlagStickyBits);
    }

    if (svcFlags & kBHSvcFlagNoSerial) {
        svcState = kBHSvcStateUnknown;
    }
    else if (svcFlags & kBHSvcFlagNonDetBits) {
        // If any service condition couldn't be determined then change service state as NotDeterminable
        svcState = kBHSvcStateNotDeterminable;
        INFO_LOG("Unable to determine Battery Health Service state. Service Flags:0x%x Service State:%d\n", svcFlags, svcState);
    } else if (svcFlags & kBHSvcFlagCalibrationFailure) {
        svcState = kBHSvcStateCalibrationFailed;
        INFO_LOG("Calibration failed. Service Flags:0x%x Service State:%d", svcFlags, svcState);
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

#if TARGET_OS_IOS || POWERD_IOS_XCTEST || TARGET_OS_WATCH
    // Update calibration flag state for AppleCare
    updateCalibration0Flags(bhData, battProps, prevSvcState, prevSvcFlags, svcState, svcFlags);
#endif
    if (svcState == kBHSvcStateUnknown || svcState == kBHSvcStateNotDeterminable) {
        // This takes precedence over any recalibration states
        INFO_LOG("calib: skipping due to precedence");
    }
    else if (currentlyCalibrating) {
        // Show the "recalibrating" UI
        svcState = kBHSvcStateRecalibrating;
        kBHCalibrationFlags calibrationFlags = 0;
        CFDictionaryRef calibrationData = CFDictionaryGetValue(bhData, CFSTR(kBHCalibration0Key));
        if (calibrationData) {
            CFDictionaryGetInt64Value(calibrationData, CFSTR(kBHCalibrationFlagsKey), calibrationFlags);
            INFO_LOG("calib: read current calibration flags 0x%lx", calibrationFlags);
        }
        bool serviceBeforeCalibration = (calibrationFlags & kBHCalibrationFlagServiceBeforeCalibration);
        if (serviceBeforeCalibration) {
            // ...unless we recommended service before calibration started
            svcState = kBHSvcStateRecalibratingServiceRecommended;
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

// D4x/N104 AzulE - "Calibration 0"
static bool calib0RelevantDevice(void)
{
    bool relevant = false;
#if HAS_MOBILE_GESTALT
    // Only run on D4x/N104
    relevant = MGIsDeviceOneOfType(MGPROD_D421,
                                   MGPROD_D431,
                                   MGPROD_N104,
                                   nil);
#endif /* HAS_MOBILE_GESTALT */
    return relevant;
}

static uint64_t calibration0wRdcP0Threshold(CFDictionaryRef batteryProps)
{
    NSDictionary *batteryProperties = (__bridge NSDictionary*)batteryProps;
    NSDictionary *batteryData = batteryProperties[@kAsbBatteryDataKey];
    // Get the AlgoChemID
    int algoChemId = -1;

    if (batteryData[@kAsbAlgoChemIDKey]) {
        algoChemId = [batteryData[@kAsbAlgoChemIDKey] intValue];
    } else {
        ERROR_LOG("calib0: failed to read algoChemId");
    }

    uint64_t p0Threshold = 660; // Highest catch all value, for the pathological case that will never happen
#if HAS_MOBILE_GESTALT
    if (MGIsDeviceOfType(MGPROD_D421)) {
        switch (algoChemId) {
            case 0x3BFF351C:
                p0Threshold = 609;
                break;
            case 0x3BFE986E:
                p0Threshold = 405;
                break;
            case 0x3BFE9F76:
                p0Threshold = 508;    // D42 - ATL
                break;
            case 0x3BFF3BC0:
                p0Threshold = 660;    // D42 - LGC
                break;
            default:
                p0Threshold = 660;
                break;
        }
    }
    if (MGIsDeviceOfType(MGPROD_D431)) {
        switch (algoChemId) {
            case 0x3C009448:
                p0Threshold = 483;
                break;
            case 0x3C006D2E:
                p0Threshold = 577;    // D43 - ATL
                break;
            case 0x3C009B50:
                p0Threshold = 521;    // D43 - LGC
                break;
            default:
                p0Threshold = 577;
                break;
        }
    }
    if (MGIsDeviceOfType(MGPROD_N104)) {
        switch (algoChemId) {
            case 0x3BF327EA:
                p0Threshold = 366;
                break;
            case 0x3BF3762A:        // N104 - MUR
                p0Threshold = 321;
                break;
            case 0x3BF34F10:
                p0Threshold = 390;
                break;
            case 0x3BF32DC6:
                p0Threshold = 538;    // N104 - ATL
                break;
            case 0x3BF354EA:
                p0Threshold = 533;    // N104 - SDI
                break;
            default:
                p0Threshold = 538;
                break;
        }
    }
#endif
    return p0Threshold;
}

static int readBatteryLifetimeUPOCount(void)
{
    NSDictionary * data;
    int count = -1;
    
    io_service_t service=MACH_PORT_NULL;
    CFMutableDictionaryRef matchingDict;
    matchingDict = IOServiceMatching(kIOServiceClass);
    if ( matchingDict!=NULL ) {
        UInt32 zero=0;
        CFNumberRef zeroRef=CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &zero);
        static CFStringRef tmp = CFSTR("IOPMUBootErrorClear");
        CFDictionaryRef propertyDict = CFDictionaryCreate( kCFAllocatorDefault, (const void **)&tmp, (const void **)&zeroRef, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
        if ( propertyDict != NULL ) {
            CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyDict);
            CFRelease(propertyDict);
            service= IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
            if ( service ) {
                CFMutableDictionaryRef cfProperties = NULL;
                IORegistryEntryCreateCFProperties(service, &cfProperties, 0, 0);
                data = CFBridgingRelease(cfProperties);
                if (data && data[@"IOPMUBootUPOCounter"]) {
                    count = [data[@"IOPMUBootUPOCounter"] intValue];
                }
                IOObjectRelease(service);
            }
        }
        else {
            CFRelease(matchingDict);
        }
     }
    return count;
}

static bool calibration0isShowingService(IOPSBatteryHealthServiceState svcState) {
    if (svcState == kBHSvcStateNominalChargeCapacity ||
        svcState == kBHSvcStatePeakPowerCapacity ||
        svcState == kBHSvcStateNominalChargeAndPeakPower ||
        svcState == kBHSvcStateRBATT ||
        svcState == kBHSvcStateBCDC) {
        // BHUI would have recommended service
        return true;
    }
    return false;
}

static void updateCalibration0State(CFDictionaryRef batteryProps, CFMutableDictionaryRef bhData,
                             IOPSBatteryHealthServiceFlags *svcFlags)
{
    NSDictionary *batteryProperties = (__bridge NSDictionary*)batteryProps;
    
    NSMutableDictionary *batteryHealthData = (__bridge NSMutableDictionary*)bhData;
    NSMutableDictionary *calibrationData = [batteryHealthData[@kBHCalibration0Key] mutableCopy];
    if (calibrationData == nil) {
        ERROR_LOG("calib0: error reading calibration data");
        return;
    }
    NSMutableDictionary *snapshotData = [calibrationData[@kBHCalibrationSnapshotsKey] mutableCopy];
    
    IOPSBatteryHealthServiceFlags prevSvcFlags = 0;
    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey), prevSvcFlags);
    IOPSBatteryHealthServiceState prevSvcState = kBHSvcStateUnknown;
    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey), prevSvcState);
    int prevMaximumCapacityPct = -1;
    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), prevMaximumCapacityPct);
    
    // Snapshotting
    // Index by cycle count
    int cycleCount = [batteryProperties[@kIOPMPSCycleCountKey] intValue];
    if (batteryProperties[@kIOPMPSCycleCountKey] == nil) {
        cycleCount = -1;
        ERROR_LOG("calib0: error reading current cycle count");
    }
    NSString *cycleCountKey = [NSString stringWithFormat:@"%d", cycleCount];
    if (snapshotData[cycleCountKey] != nil) {
        INFO_LOG("calib0: %d already snapshotted", cycleCount);
    } else {
        /*
         Snapshot the following on each new cycle count while calibrating
         powerd:
         - Maximum Capacity Percent
         - Service Flags
         - Service Option
         
         IOPM:
         - CycleCount
         - weightedRa
         - NCC
         - Total Operating Time
         
         PMU:
         - IOPMUBootUPOCounter (Lifetime UPO Count)
         */
        INFO_LOG("calib0: creating snapshot for %d", cycleCount);

        NSDictionary *batteryData = batteryProperties[@kAsbBatteryDataKey];
        NSDictionary *lifetimeData = batteryData[@kAsbLifetimeDataKey];
        NSMutableDictionary *cycleCountSnapshot = [NSMutableDictionary dictionary];
        // Max Cap
        cycleCountSnapshot[@kIOPSBatteryHealthMaxCapacityPercent] = @(prevMaximumCapacityPct);
        // Svc Flags
        cycleCountSnapshot[@kIOPSBatteryHealthServiceFlagsKey] = @(prevSvcFlags);
        // Svc Opt
        cycleCountSnapshot[@kIOPSBatteryHealthServiceStateKey] = @(prevSvcState);
        // Cycle Count
        cycleCountSnapshot[@kIOPMPSCycleCountKey] = @(cycleCount);
        // WeightedRa
        cycleCountSnapshot[@kAsbWRaKey] = batteryData[@kAsbWRaKey];
        // NCC
        cycleCountSnapshot[@kAsbNominalChargeCapacityKey] = batteryProperties[@kAsbNominalChargeCapacityKey];
        // TotalOperatingTime
        cycleCountSnapshot[@kAsbTotalOperatingTimeKey] = lifetimeData[@kAsbTotalOperatingTimeKey];
        // Lifetime UPO Count
        cycleCountSnapshot[@"LifetimeUPOCount"] = @(readBatteryLifetimeUPOCount());
        
        INFO_LOG("calib0: created snapshot for %d %@", cycleCount, cycleCountSnapshot);
        snapshotData[cycleCountKey] = cycleCountSnapshot;
    }

    calibrationData[@kBHCalibrationSnapshotsKey] = snapshotData;
    batteryHealthData[@kBHCalibration0Key] = calibrationData;
    if (snapshotData) CFRelease(snapshotData);
    if (calibrationData) CFRelease(calibrationData);
}

static void initializeCalibration0(CFDictionaryRef batteryProps, CFMutableDictionaryRef bhData,
                            IOPSBatteryHealthServiceFlags *svcFlags)
{
    // Create top-level calibration dict and baseline/snapshots sub-dicts
    NSMutableDictionary *calibrationData = [NSMutableDictionary dictionary];
    NSMutableDictionary *baselineData = [NSMutableDictionary dictionary];
    NSMutableDictionary *snapshotData = [NSMutableDictionary dictionary];

    NSDictionary *batteryProperties = (__bridge NSDictionary*)batteryProps;
    NSMutableDictionary *batteryHealthData = (__bridge NSMutableDictionary*)bhData;
    NSDictionary *batteryData = batteryProperties[@kAsbBatteryDataKey];
    NSDictionary *lifetimeData = batteryData[@kAsbLifetimeDataKey];
    /*
     Snapshot the following metrics for baseline
     powerd:
        - Maximum Capacity Percent
        - Service Flags
        - Service Option

     IOPM:
        - Battery Serial
        - Cycle Count
        - Gauge FW Version
        - AlgoChemID
        - WeightedRa
        - Ra Table
        - Total Operating Time
        - NCC + Lifetime min/max
        - FCC + Lifetime min/max
        - QMax + Lifetime min/max
        - TimeAtHighSoc
     
     PMU:
        - IOPMUBootUPOCounter (Lifetime UPO Count)
     
     System:
        - epoch time
     */
    IOPSBatteryHealthServiceFlags prevSvcFlags = 0;
    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey), prevSvcFlags);
    IOPSBatteryHealthServiceState prevSvcState = kBHSvcStateUnknown;
    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey), prevSvcState);
    int prevMaximumCapacityPct = -1;
    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), prevMaximumCapacityPct);

    // Max Cap
    baselineData[@kIOPSBatteryHealthMaxCapacityPercent] = @(prevMaximumCapacityPct);
    // Svc Flags
    baselineData[@kIOPSBatteryHealthServiceFlagsKey] = @(prevSvcFlags);
    // Svc Opt
    baselineData[@kIOPSBatteryHealthServiceStateKey] = @(prevSvcState);
    // Battery Serial
    baselineData[@kIOPSBatterySerialNumberKey] = batteryProperties[@"Serial"];
    // Cycle Count
    baselineData[@kIOPMPSCycleCountKey] = batteryProperties[@kIOPMPSCycleCountKey];
    // GG FW Version
    baselineData[@"GasGaugeFirmwareVersion"] = batteryProperties[@"GasGaugeFirmwareVersion"];
    // AlgoChemID
    baselineData[@kAsbAlgoChemIDKey] = batteryData[@kAsbAlgoChemIDKey];
    // WeightedRa
    baselineData[@kAsbWRaKey] = batteryData[@kAsbWRaKey];
    // Ra Table
    NSArray<NSString*> *raTableKeys = @[@kAsbRa00Key, @kAsbRa01Key, @kAsbRa02Key, @kAsbRa03Key, @kAsbRa04Key,
                                        @kAsbRa05Key, @kAsbRa06Key, @kAsbRa07Key, @kAsbRa08Key, @kAsbRa09Key,
                                        @kAsbRa10Key, @kAsbRa11Key, @kAsbRa12Key, @kAsbRa13Key, @kAsbRa14Key];
    [raTableKeys enumerateObjectsUsingBlock:^(NSString * _Nonnull key, NSUInteger idx, BOOL * _Nonnull stop) {
        baselineData[key] = batteryData[key];
    }];
    // Total Operating Time
    baselineData[@kAsbTotalOperatingTimeKey] = lifetimeData[@kAsbTotalOperatingTimeKey];
    // NCC
    baselineData[@kAsbNominalChargeCapacityKey] = batteryProperties[@kAsbNominalChargeCapacityKey];
    // Lifetime NCC Min
    baselineData[@kAsbNCCMinKey] = lifetimeData[@kAsbNCCMinKey];
    // Lifetime NCC Max
    baselineData[@kAsbNCCMaxKey] = lifetimeData[@kAsbNCCMaxKey];
    // FCC
    baselineData[@kAsbRawMaxCapacityKey] = batteryProperties[@kAsbRawMaxCapacityKey];
    // Lifetime FCC Min
    baselineData[@kAsbMinFCCKey] = lifetimeData[@kAsbMinFCCKey];
    // Lifetime FCC Max
    baselineData[@kAsbMaxFCCKey] = lifetimeData[@kAsbMaxFCCKey];
    // Qmax -- note Qmax in IOPM is an array, assume the first element is the one we're interested in
    NSArray* qmaxPerCell = batteryData[@kAsbQmaxCellKey];
    baselineData[@kAsbQmaxCellKey] = [qmaxPerCell firstObject];
    // Lifetime QMax Max
    baselineData[@kAsbMaxQmaxKey] = lifetimeData[@kAsbMaxQmaxKey];
    // Lifetime QMax Min
    baselineData[@kAsbMinQmaxKey] = lifetimeData[@kAsbMinQmaxKey];
    // TimeAtHighSoc -- note store as a hex string to allow AppleCare to read (NSData causes JSON to choke)
    NSData *timeAtHighSoc = lifetimeData[@kAsbTimeAtHighSocKey];
    if (timeAtHighSoc) {
        NSMutableString *hexString = [NSMutableString stringWithCapacity:(2*timeAtHighSoc.length)];
        const uint8_t* bytes = timeAtHighSoc.bytes;
        size_t i = 0;
        while(i < timeAtHighSoc.length) {
            [hexString appendFormat:@"%02x", bytes[i]];
            i++;
        }
        INFO_LOG("calib0: baseline TAHSoC %@ -> %@", timeAtHighSoc, hexString);
        baselineData[@kAsbTimeAtHighSocKey] = hexString;
    }
    // Lifetime UPO Count
    baselineData[@"LifetimeUPOCount"] = @(readBatteryLifetimeUPOCount());
    // epoch time
    baselineData[@"epoch"] = @(getTimeInSecsSinceEpoch());
    INFO_LOG("calib0: baseline data %@", baselineData);

    kBHCalibrationFlags calibrationFlags = 0;
    if (calibration0isShowingService(prevSvcState)) {
        calibrationFlags |= kBHCalibrationFlagServiceBeforeCalibration;
    }
    bool isNewBattery = (*svcFlags & kBHSvcFlagNewBattery);
    if (isNewBattery) {
        // If a new battery is detected, skip calibration
        calibrationFlags |= kBHCalibrationFlagCompleted;
        calibrationFlags |= kBHCalibrationFlagSkipped;
    }
    calibrationFlags |= kBHCalibrationFlagCalib1NotNeeded;
    calibrationData[@kBHCalibrationFlagsKey] = @(calibrationFlags);
    INFO_LOG("calib0: baseline calibration flags 0x%lx", calibrationFlags);
    
    calibrationData[@kBHCalibrationSnapshotsKey] = snapshotData;
    calibrationData[@kBHCalibrationBaselineKey] = baselineData;
    batteryHealthData[@kBHCalibration0Key] = calibrationData;
}

#define kBHCalibration0wRDCFailureThreshold .7
#define kBHCalibration0wRDCCycleCountLookback 5
static bool didCalibration0Fail(CFDictionaryRef batteryProps, CFMutableDictionaryRef bhData)
{
    bool calibrationFailed = false;
    uint64_t p0Threshold = batteryHealthP0Threshold == 0 ? calibration0wRdcP0Threshold(batteryProps) : batteryHealthP0Threshold;
    double calibrationFailureThreshold = (p0Threshold * kBHCalibration0wRDCFailureThreshold);
    INFO_LOG("calib0: checking for calibration failure with threshold %f", calibrationFailureThreshold);

    NSDictionary *batteryProperties = (__bridge NSDictionary*)batteryProps;
    int cycleCount = [batteryProperties[@kIOPMPSCycleCountKey] intValue];
    if (batteryProperties[@kIOPMPSCycleCountKey] == nil) {
        cycleCount = -1;
        ERROR_LOG("calib0: error reading current cycle count");
    }

    NSDictionary *batteryHealthData = (__bridge NSDictionary*)bhData;
    NSDictionary *calibrationData = batteryHealthData[@kBHCalibration0Key];
    NSDictionary *snapshotDictionary = calibrationData[@kBHCalibrationSnapshotsKey];
    // The snapshot dictionary is a mapping from cycle count (unfortunately a string) to a dictionary with wRDC included
    // Since we're interested in the most recent N, we'll walk backwards until we have found enough or have run out of snapshots
    int totalwRDC = 0;
    int numValidSamples = 0;
    size_t totalSamples = snapshotDictionary.count;
    int samplesChecked = 0;
    int targetCycleCount = cycleCount;

    while (targetCycleCount > 0) {
        INFO_LOG("calib0: loop: checking for target cycle %d, found %d (%d / %zu)", targetCycleCount, numValidSamples, samplesChecked, totalSamples);
        NSString *targetKey = [NSString stringWithFormat:@"%d", targetCycleCount];
        NSDictionary *targetSnapshot = snapshotDictionary[targetKey];
        if (targetSnapshot) {
            INFO_LOG("calib0: loop: snapshot found for %d", targetCycleCount);
            int snapshotwRDC = [targetSnapshot[@kAsbWRaKey] intValue];
            if (snapshotwRDC > 0) {
                totalwRDC += snapshotwRDC;
                numValidSamples++;
                INFO_LOG("calib0: loop: adding sample #%d with wRDC %d", numValidSamples, snapshotwRDC);
            }
            samplesChecked++;
        } else {
            INFO_LOG("calib0: loop: no snapshot found for %d", targetCycleCount);
        }

        targetCycleCount--;

        if (numValidSamples == kBHCalibration0wRDCCycleCountLookback) {
            INFO_LOG("calib0: loop: exiting, found enough");
            break;
        } else if (samplesChecked == totalSamples) {
            INFO_LOG("calib0: loop: ran through all %zu samples", totalSamples);
            break;
        }
    }

    if (numValidSamples != kBHCalibration0wRDCCycleCountLookback) {
        ERROR_LOG("calib0: only found %d samples, need %d", numValidSamples, kBHCalibration0wRDCCycleCountLookback);
    }

    if (numValidSamples <= 0) {
        calibrationFailed = false;
    } else {
        double averagewRDC = ((double)totalwRDC)/numValidSamples;
        if (averagewRDC > calibrationFailureThreshold) {
            calibrationFailed = true;
        } else {
            calibrationFailed = false;
        }
    }

    return calibrationFailed;
}

#define kBHCalibration0wRdcThresholdNotFound -1
static int calibration0wRdcThreshold(CFDictionaryRef batteryProps)
{
    NSDictionary *batteryProperties = (__bridge NSDictionary*)batteryProps;
    NSDictionary *batteryData = batteryProperties[@kAsbBatteryDataKey];
    // Get the AlgoChemID
    int algoChemId = -1;
    if (batteryData[@kAsbAlgoChemIDKey]) {
        algoChemId = [batteryData[@kAsbAlgoChemIDKey] intValue];
    } else {
        ERROR_LOG("calib0: failed to read algoChemId");
    }

    // rdar://75873981 (D4x N104 wRdc Threshold values to check IC reset)
    switch(algoChemId) {
        case 0x3BF32DC6: return 190;    // N104 - ATL
        case 0x3BF354EA: return 205;    // N104 - SDI
        case 0x3BFE9F76: return 175;    // D42 - ATL
        case 0x3BFF3BC0: return 218;    // D42 - LGC
        case 0x3C006D2E: return 138;    // D43 - ATL
        case 0x3C009B50: return 160;    // D43 - LGC
    }
    return kBHCalibration0wRdcThresholdNotFound;
}

#define kBHCalibration0LowCycleCountDelta 10
#define kBHCalibration0HighCycleCountDelta 20
static bool shouldCalibration0End(CFDictionaryRef batteryProps, CFMutableDictionaryRef bhData,
                           IOPSBatteryHealthServiceFlags *svcFlags)
{
    NSDictionary *batteryProperties = (__bridge NSDictionary*)batteryProps;
    
    NSMutableDictionary *batteryHealthData = (__bridge NSMutableDictionary*)bhData;
    NSMutableDictionary *calibrationData = [batteryHealthData[@kBHCalibration0Key] mutableCopy];
    if (calibrationData == nil) {
        ERROR_LOG("calib0: error reading existing calibration data");
    }
    
    kBHCalibrationFlags calibrationFlags = [calibrationData[@kBHCalibrationFlagsKey] unsignedLongValue];
    INFO_LOG("calib0: checking flags 0x%lx", calibrationFlags);
    bool calibrationComplete = (calibrationFlags & kBHCalibrationFlagCompleted);
    if (calibrationComplete) {
        INFO_LOG("calib0: calibration complete");
        if (calibrationData) CFRelease(calibrationData);
        return true;
    }
    
    NSDictionary *baselineData = calibrationData[@kBHCalibrationBaselineKey];
    if (baselineData == nil) {
        ERROR_LOG("calib0: error reading baseline data");
    }

    int initialwRDC = [baselineData[@kAsbWRaKey] intValue];
    if (baselineData[@kAsbWRaKey] == nil) {
        initialwRDC = -1;
        ERROR_LOG("calib0: error reading initial wRDc");
    }

    int cycleCount = [batteryProperties[@kIOPMPSCycleCountKey] intValue];
    if (batteryProperties[@kIOPMPSCycleCountKey] == nil) {
        cycleCount = -1;
        ERROR_LOG("calib0: error reading current cycle count");
    }
    
    int initialCycleCount = [baselineData[@kIOPMPSCycleCountKey] intValue];
    if (baselineData[@kIOPMPSCycleCountKey] == nil) {
        initialCycleCount = -1;
        ERROR_LOG("calib0: error reading initial cycle count");
    }

    bool exitCriteriaMet = false;
    int cycleCountThreshold = -1;
    INFO_LOG("calib0: checking exit criteria");

    // Decide between a lower and higher cycle count threshold
    int wRDCThreshold = calibration0wRdcThreshold(batteryProps);
    INFO_LOG("calib0: initial wRDC:%d threshold:%d", initialwRDC, wRDCThreshold);
    bool useHighThresholds = false;

    if (wRDCThreshold == kBHCalibration0wRdcThresholdNotFound) {
        // No cellID-dependent threshold, use low thresholds
        useHighThresholds = false;
    }
    else if (initialwRDC == -1) {
        // failure to read starting wRDC value, use high thresholds
        useHighThresholds = true;
    }
    else if (initialwRDC > wRDCThreshold) {
        // If wRDC started high and was reset, use the high thresholds
        useHighThresholds = true;
    } else {
        // Otherwise, use the low ones
        useHighThresholds = false;
    }

    if (useHighThresholds) {
        cycleCountThreshold = kBHCalibration0HighCycleCountDelta;
        calibrationFlags |= kBHCalibrationFlagThresholdHigher;
    } else {
        cycleCountThreshold = kBHCalibration0LowCycleCountDelta;
        calibrationFlags |= kBHCalibrationFlagThresholdLower;
    }
    

    INFO_LOG("calib0: initial:%d current:%d threshold:%d", initialCycleCount, cycleCount, cycleCountThreshold);
    int cyclesSinceCalibrationStart = cycleCount - initialCycleCount;

    if (cyclesSinceCalibrationStart > cycleCountThreshold) {
        exitCriteriaMet = true;
    }

    if (exitCriteriaMet) {
        // Update calibration flags
        calibrationFlags |= kBHCalibrationFlagCompleted;
        calibrationFlags |= kBHCalibrationFlagCalib1NotNeeded;
        INFO_LOG("calib0: exit criteria met: 0x%lx", calibrationFlags);

        // Check for calibration failure
        if (didCalibration0Fail(batteryProps, bhData)) {
            INFO_LOG("calib0: calibration failed");
            *svcFlags |= kBHSvcFlagCalibrationFailure;
            calibrationFlags |= kBHCalibrationFlagFailure; // telemetry only, all other systems should use kBHSvcStateCalibrationFailed
        }
    }
    if (calibrationData) {
        calibrationData[@kBHCalibrationFlagsKey] = @(calibrationFlags);
        batteryHealthData[@kBHCalibration0Key] = calibrationData;
        CFRelease(calibrationData);
    }
    return exitCriteriaMet;
}

static bool isCalibration0Running(CFDictionaryRef batteryProps, CFMutableDictionaryRef bhData,
                    IOPSBatteryHealthServiceFlags *svcFlags)
{
    bool calibration0DataAlreadyExists = CFDictionaryContainsKey(bhData, CFSTR(kBHCalibration0Key));
    if (!calibration0DataAlreadyExists) {
        INFO_LOG("calib0: starting");
        initializeCalibration0(batteryProps, bhData, svcFlags);
    }

    if (shouldCalibration0End(batteryProps, bhData, svcFlags)) {
        INFO_LOG("calib0: exiting");
        return false;
    }
    
    // At this point we know we're running, and need to update state.
    updateCalibration0State(batteryProps, bhData, svcFlags);
    return true;
}

static void updateCalibration0Flags(CFMutableDictionaryRef bhData, CFDictionaryRef batteryProps,
                             IOPSBatteryHealthServiceState prevSvcState, IOPSBatteryHealthServiceFlags prevSvcFlags,
                             IOPSBatteryHealthServiceState currentSvcState, IOPSBatteryHealthServiceFlags currentSvcFlags)
{
    // Bail if we're on an unsupported device
    if (!calib0RelevantDevice()) {
        return;
    }
    CFDictionaryRef calibrationDataDisk = CFDictionaryGetValue(bhData, CFSTR(kBHCalibration0Key));
    CFMutableDictionaryRef calibrationData = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, calibrationDataDisk);
    if (calibrationData) {
        INFO_LOG("calib0: updating calibraiton flags using psvc%d psvcflag0x%x -> svc%d svcflag0x%x", prevSvcState, prevSvcFlags, currentSvcState, currentSvcFlags);
        kBHCalibrationFlags calibrationFlags = 0;
        CFDictionaryGetInt64Value(calibrationData, CFSTR(kBHCalibrationFlagsKey), calibrationFlags);
        kBHCalibrationFlags oldCalibrationFlags = calibrationFlags;
        
        NSDictionary *batteryProperties = (__bridge NSDictionary*)batteryProps;
        NSDictionary *batteryData = batteryProperties[@kAsbBatteryDataKey];
        NSDictionary *lifetimeData = batteryData[@kAsbLifetimeDataKey];

        // Update flags/capture state on transitions
        
        // Note: current svc flags/state have not yet been "overriden" to show recalibration
        bool wouldRecommendService = calibration0isShowingService(currentSvcState);
        bool currentlyRecalibrating = (currentSvcFlags & kBHSvcFlagCurrentlyCalibrating);
        if (currentlyRecalibrating) {
            if (wouldRecommendService) {
                calibrationFlags |= kBHCalibrationFlagServiceDuringCalibration;
            }
            
            if (prevSvcState == kBHSvcStateRecalibrating &&
                wouldRecommendService) {
                // Capture ToT at time of first transition from NoSvc -> Svc during calibration
                if (!CFDictionaryContainsKey(calibrationData, CFSTR(kBHCalibrationSvcTOTKey))) {
                    int totalOperatingTime = [lifetimeData[@kAsbTotalOperatingTimeKey] intValue];
                    CFDictionarySetIntValue(calibrationData, CFSTR(kBHCalibrationSvcTOTKey), totalOperatingTime);
                    INFO_LOG("calib0: captured %s=%d", kBHCalibrationSvcTOTKey, totalOperatingTime);
                }
                calibrationFlags |= kBHCalibrationFlagNoServiceToServiceDuringCalibration;
            }
            
            if (prevSvcState == kBHSvcStateRecalibratingServiceRecommended &&
                !wouldRecommendService) {
                // If we entered recalibration recommending service, but are no longer doing so
                calibrationFlags |= kBHCalibrationFlagServiceToNoServiceDuringCalibration;
            }
            
        } else {
            bool previouslyRecalibrating = (prevSvcFlags & kBHSvcFlagCurrentlyCalibrating);
            if (previouslyRecalibrating) {
                // Capture ToT at time of exit, stash away for AppleCare
                int totalOperatingTime = [lifetimeData[@kAsbTotalOperatingTimeKey] intValue];
                CFDictionarySetIntValue(calibrationData, CFSTR(kBHCalibrationExitTOTKey), totalOperatingTime);
                INFO_LOG("calib0: just finished recalibrating, ToT:%d", totalOperatingTime);
                
                if (wouldRecommendService) {
                    calibrationFlags |= kBHCalibrationFlagServiceAtEndOfCalibration;
                }
            }
        }
        
        CFDictionarySetInt64Value(calibrationData, CFSTR(kBHCalibrationFlagsKey), calibrationFlags);
        CFDictionarySetValue(bhData, CFSTR(kBHCalibration0Key), (CFMutableDictionaryRef)calibrationData);
        INFO_LOG("calib0: updated calibration flags 0x%lx -> 0x%lx", oldCalibrationFlags, calibrationFlags);
    }
    if (calibrationData) CFRelease(calibrationData);
}

static bool isCalibration0FailureSet(CFDictionaryRef bhData)
{
    CFDictionaryRef calibrationData = CFDictionaryGetValue(bhData, CFSTR(kBHCalibration0Key));
    if (!calibrationData) {
        return false;
    }

    kBHCalibrationFlags calibrationFlags = 0;
    CFDictionaryGetInt64Value(calibrationData, CFSTR(kBHCalibrationFlagsKey), calibrationFlags);
    if (calibrationFlags & kBHCalibrationFlagFailure) {
        return true;
    } else {
        return false;
    }
}

static bool isCalibration1Complete(CFDictionaryRef bhData)
{
    CFDictionaryRef calibrationData = CFDictionaryGetValue(bhData, CFSTR(kBHCalibration0Key));
    if (!calibrationData) {
        return false;
    }

    kBHCalibrationFlags calibrationFlags = 0;
    CFDictionaryGetInt64Value(calibrationData, CFSTR(kBHCalibrationFlagsKey), calibrationFlags);
    if (calibrationFlags & kBHCalibrationFlagCalib1Completed) {
        return true;
    } else {
        return false;
    }
}

static bool isCalibration1Needed(CFDictionaryRef bhData)
{
    CFDictionaryRef calibrationData = CFDictionaryGetValue(bhData, CFSTR(kBHCalibration0Key));
    if (!calibrationData) {
        return false;
    }

    kBHCalibrationFlags calibrationFlags = 0;
    CFDictionaryGetInt64Value(calibrationData, CFSTR(kBHCalibrationFlagsKey), calibrationFlags);

    if (calibrationFlags & kBHCalibrationFlagCalib1NotNeeded) {
        return false;
    } else {
        return true;
    }
}

static int getCalibration1RefCycleCount(CFDictionaryRef calibrationDict)
{
    int cycleCountOffset = 0;
    NSDictionary *calibrationData = (__bridge NSDictionary *) calibrationDict;
    if (calibrationData == nil) {
        ERROR_LOG("calib0: error reading existing calibration data");
        return -1;
    }
    
    kBHCalibrationFlags calibrationFlags = [calibrationData[@kBHCalibrationFlagsKey] unsignedLongValue];
    if (calibrationFlags & kBHCalibrationFlagThresholdHigher) {
        cycleCountOffset = kBHCalibration0HighCycleCountDelta;
    } else if (calibrationFlags & kBHCalibrationFlagSkipped) {
        cycleCountOffset = 0;
    } else {
        cycleCountOffset = kBHCalibration0LowCycleCountDelta;
    }

    NSDictionary *baselineData = calibrationData[@kBHCalibrationBaselineKey];
    if (baselineData == nil || baselineData[@kIOPMPSCycleCountKey] == nil) {
        ERROR_LOG("calib0: error reading baseline data");
        return -1;
    }
    int initialCycleCount = [baselineData[@kIOPMPSCycleCountKey] intValue];
    return initialCycleCount + cycleCountOffset;
}

/*
 * Take calibrationData dictionary and run 2nd round of calibration on the same a.k.a 'calibration1'
 *
 * _In: @calibrationData, A nested dictionary of calibration0 snapshots
 * _Out: @calib1, Dictionary 'calib1' with calibration1 results
 *
 * Once successfully done, the routine will return calibrationData with 'calib1' dictionary bearing calibration1 results.
 * Callers are expected to make sure of device/calibration criteria etc. validity.
 * A dictionary of results will always be returned with below schema.
 *
 * calib1 {
 *  calib1MaxCapacity,  // calib1's decision about maxCapacity from snapshots, kInitialNominalCapacityPercentage if no snapshots found/calibration didn't run.
 *  calib1SvcFlags,     // decision about svcFlags from snapshots, '0' if no snapshots or calibration didn't run.
 *  refCycleCount,      // reference cycle count. (> refCycleCount) snapshots are parsed
 *  nSamples,           // number of samples parsed
 * }
 */
static CF_RETURNS_RETAINED CFDictionaryRef parseCalibration0Snapshots(CFDictionaryRef calibrationData)
{
    NSMutableDictionary *calib1 = [NSMutableDictionary dictionary];
    if (!calib1) {
        ERROR_LOG("Out of memory for calibration1");
        return NULL;
    }

    int calib1MaxCap = kInitialNominalCapacityPercentage; // start with a high reference value
    IOPSBatteryHealthServiceFlags calib1SvcFlags = 0; 
    int nSamples = 0;
    int refCycleCount = -1;

    NSDictionary *calib0 = (__bridge NSDictionary *)calibrationData;
    if (!calib0) {
        ERROR_LOG("No calibration data found");
        goto out;
    }

    NSDictionary *snapshots = calib0[@kBHCalibrationSnapshotsKey];
    if (!snapshots) {
        ERROR_LOG("No snapshots found");
        goto out;
    }

    refCycleCount = getCalibration1RefCycleCount(calibrationData);
    if (refCycleCount == -1) {
        goto out;
    }

    NSString *key;
    NSEnumerator *enumerator = [snapshots keyEnumerator];
    while (key = [enumerator nextObject]) {
        if (![snapshots[key] isKindOfClass:[NSDictionary class]]) {
            ERROR_LOG("Not a class");
            continue;
        }
        NSDictionary *snapshot = snapshots[key];
        int cycleCount = [snapshot[@kIOPMPSCycleCountKey] intValue];
        // skip this snapshot
        if (cycleCount <= refCycleCount) {
            continue;
        }

        int maxCap = [snapshot[@kIOPSBatteryHealthMaxCapacityPercent] intValue];
        if (maxCap > 0) {
            calib1MaxCap = MIN(calib1MaxCap, maxCap);
        }

        int svcFlags = [snapshot[@kIOPSBatteryHealthServiceFlagsKey] intValue];
        calib1SvcFlags |= (svcFlags & kBHSvcFlagStickyBits);
        nSamples++;
    }

out:
    calib1[@kIOPSBatteryHealthMaxCapacityPercent] = @(calib1MaxCap);
    calib1[@kIOPSBatteryHealthServiceFlagsKey] = @(calib1SvcFlags);
    calib1[@kIOPMPSCycleCountKey] = @(refCycleCount);
    calib1[@"nSamples"] = @(nSamples);
    CFRetain(calib1);
    return (__bridge CFDictionaryRef) calib1;
}

/*
 * Run calibration1 and decide final maxCapacity and service flags.
 *
 * _In: @bhData, Battery health data dictionary
 *
 * Once successfully done, the routine will update the following in battery health data dictionary, and set kBHCalibrationFlagCalib1Completed in calibration flags.
 * - maximum capacity as MIN(current maxCap, calib1 maxCap)
 * - service flags as OR(current service flags, calib1 service flags). calib1 service flags will only bear sticky bits.
 *
 * Finally, the routine will append the original calibration1 results to calibration0 dictionary.
 */
static void runCalibration1(CFMutableDictionaryRef bhData)
{
    int prevSvcFlags = 0, newSvcFlags = 0, prevNccp = 0, newNccp = 0;
    kBHCalibrationFlags calibrationFlags = 0;

    CFDictionaryRef calibrationData = CFDictionaryGetValue(bhData, CFSTR(kBHCalibration0Key));
    if (!calibrationData) {
        ERROR_LOG("Calibration0 not found");
        return;
    }

    CFDictionaryRef calib1 = parseCalibration0Snapshots(calibrationData);
    if (!calib1) {
        ERROR_LOG("Failed to initiate calibration1");
        return;
    }

    CFMutableDictionaryRef calibrationDataCopy = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, calibrationData);
    if (!calibrationDataCopy) {
        ERROR_LOG("Out of memory for calibration1");
        CFRelease(calib1);
        return;
    }

    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey), prevSvcFlags);
    CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), prevNccp);
    CFDictionaryGetIntValue(calib1, CFSTR(kIOPSBatteryHealthServiceFlagsKey), newSvcFlags);
    CFDictionaryGetIntValue(calib1, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), newNccp);

    INFO_LOG("calib1/current: ncc %d/%d svcFlags %d/%d", newNccp, prevNccp, newSvcFlags, prevSvcFlags);
    newNccp = MIN(newNccp, prevNccp);
    newSvcFlags |= prevSvcFlags;
    INFO_LOG("calib1: ncc %d svcFlags %d", newNccp, newSvcFlags);

    CFDictionarySetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceFlagsKey), newSvcFlags);
    CFDictionarySetIntValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), newNccp);
    CFDictionaryGetInt64Value(calibrationDataCopy, CFSTR(kBHCalibrationFlagsKey), calibrationFlags);
    calibrationFlags |= kBHCalibrationFlagCalib1Completed;

    CFDictionarySetInt64Value(calibrationDataCopy, CFSTR(kBHCalibrationFlagsKey), calibrationFlags);

    CFDictionarySetValue(calibrationDataCopy, CFSTR(kBHCalibration1Key), (CFMutableDictionaryRef) calib1);
    CFDictionarySetValue(bhData, CFSTR(kBHCalibration0Key), calibrationDataCopy);
    CFRelease(calibrationDataCopy);
    CFRelease(calib1);
    return;
}

static void checkCalibrationStatus(CFDictionaryRef batteryProps, CFMutableDictionaryRef bhData,
                            IOPSBatteryHealthServiceFlags *svcFlags)
{
    bool anyCalibrationRunning = false;
    bool isRelevantDevice = calib0RelevantDevice();
    if (!isRelevantDevice) {
        INFO_LOG("calib0: device not relevant");
        // On irrelevant devices, we'll never report the calibration is running.
        return;
    }
    
    anyCalibrationRunning |= isCalibration0Running(batteryProps, bhData, svcFlags);
    
    // ^Any future re-calibrations should be added here, return values ||'d into anyCalibrationRunning
    if (anyCalibrationRunning) {
        INFO_LOG("calibration running");
        *svcFlags |= kBHSvcFlagCurrentlyCalibrating;
    } else {
        *svcFlags &= ~(kBHSvcFlagCurrentlyCalibrating);

        // Re-test prior calibration failures. REF: rdar://78844328
        if (isCalibration0FailureSet(bhData)) {
            *svcFlags |= kBHSvcFlagCalibrationFailure;
        } else if (isCalibration1Needed(bhData) && !isCalibration1Complete(bhData)) {
            runCalibration1(bhData);
        } else {
            INFO_LOG("Calib0 success, calib1 not needed");
        }
    }
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
    
    bool currentlyCalibrating = (*svcFlags & kBHSvcFlagCurrentlyCalibrating);
    if (currentlyCalibrating) {
        // Allow NCC to increase if we're currently calibrating
        CFDictionaryRemoveValue(bhData, CFSTR(kIOPMPSCycleCountKey));
        CFDictionaryRemoveValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent));
        INFO_LOG("calib: floating NCC");
    } else {
        IOPSBatteryHealthServiceState prevSvcState = kBHSvcStateUnknown;
        // Reset NCC/maxCapacity to gauge values only if calibration1 was not needed.
        // If calibration1 was needed, it would set the right values in bhData in runCalibration1() routine (from snapshots), don't reset maxCapacity.
        // calibration1 would guarantee to complete, synchronously in calibration phase, hence checking for isCalibration1Complete() is a no-op here.
        // TODO: Find a new home for this existing/legacy code that lets NCC reset/float etc. checkNominalCapacity() should just 'check', resettting/floating
        // is burden of calibration logic.
        if (!isCalibration1Needed(bhData)) {
            CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey), prevSvcState);
            if (prevSvcState == kBHSvcStateRecalibrating ||
                prevSvcState == kBHSvcStateRecalibratingServiceRecommended) {
                // We just stopped calibrating, allow NCC to reset
                CFDictionaryRemoveValue(bhData, CFSTR(kIOPMPSCycleCountKey));
                CFDictionaryRemoveValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent));
                INFO_LOG("calib: resetting NCC");
            }
        }
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
        // as new battery and set NCCP to kInitialNominalCapacityPercentage, unless we're calibrating
        if (prevNccp == -1) {
            if (cycleCount <= kTrueNCCCycleCountThreshold && !currentlyCalibrating) {
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
        svcFlags |= (kBHSvcFlagNewBattery);

        // Remove BH data about previous battery to force re-evaluation for new battery
        CFDictionaryRemoveValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent));
        CFDictionaryRemoveValue(bhData, CFSTR(kIOPMPSCycleCountKey));
        CFDictionaryRemoveValue(bhData, CFSTR(kIOPSBatteryHealthServiceStateKey));
        CFDictionaryRemoveValue(bhData, CFSTR(kBHCalibration0Key));

        removeKeyFromBatteryHealthDataPrefs(CFSTR(kIOPSBatteryHealthMaxCapacityPercent));
        removeKeyFromBatteryHealthDataPrefs(CFSTR(kIOPMPSCycleCountKey));
        removeKeyFromBatteryHealthDataPrefs(CFSTR(kIOPSBatteryHealthServiceStateKey));
        removeKeyFromBatteryHealthDataPrefs(CFSTR(kBHCalibration0Key));

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

    INFO_LOG("calib: svcFlags pre: 0x%x", svcFlags);
    checkCalibrationStatus(batteryProps, bhData, &svcFlags);
    INFO_LOG("calib: svcFlags post: 0x%x", svcFlags);
    checkNominalCapacity(batteryProps, bhData, &svcFlags);
    checkUPOCount(&svcFlags);
    checkWeightedRa(batteryProps, &svcFlags);
    checkCellDisconnectCount(batteryProps, &svcFlags);

    updateBatteryServiceState(batteryProps, bhData, svcFlags);
    saveBatteryHealthDataToPrefs(bhData);

    bool currentlyCalibrating = (svcFlags & kBHSvcFlagCurrentlyCalibrating);
    if (currentlyCalibrating) {
        // While we're recalibrating, "freeze" NCC at the baseline value
        NSDictionary *calibrationDictionary = CFDictionaryGetValue(bhData, CFSTR(kBHCalibration0Key));
        NSDictionary *baselineDictionary = calibrationDictionary[@kBHCalibrationBaselineKey];
        int baselineMaximumCapacity = [baselineDictionary[@kIOPSBatteryHealthMaxCapacityPercent] intValue];

        int currentMaximumCapacity = -1;
        CFDictionaryGetIntValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), currentMaximumCapacity);

        // Report the baseline as max capacity to UI/AST2
        CFDictionarySetIntValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), baselineMaximumCapacity)
        // Surface actual current max capacity as well
        CFDictionarySetIntValue(outDict, CFSTR("currentMaxCap"), currentMaximumCapacity);
        INFO_LOG("reporting %s as %d (from %d)", kIOPSBatteryHealthMaxCapacityPercent, baselineMaximumCapacity, currentMaximumCapacity);
    }

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

    CFTypeRef calibration0Ref = CFDictionaryGetValue(bhData, CFSTR(kBHCalibration0Key));
    if (isA_CFDictionary(calibration0Ref)) {
        CFDictionarySetValue(outDict, CFSTR(kBHCalibration0Key), calibration0Ref);
    }

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
    uint64_t ts = getTimeInSecsSinceEpoch();
    CFDictionarySetInt64Value(dict, CFSTR("ts"), ts);

    if ((flags & kBHSvcFlagNewBattery)) {
        svcFlags |= kBHSvcFlagNewBattery;
        sticky = 0;
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
 *
 * @return value: current time if limit crossed, 0 otherwise.
 */
static uint64_t hasSamplingTimeExpired(uint64_t baseTime)
{
    uint64_t currentTime, timeDelta;

    currentTime = getTimeInSecsSinceEpoch();
    timeDelta = currentTime - baseTime;

    if ((int64_t) timeDelta < 0) {
        return -1;
    } else if (timeDelta > battReadTimeDelta) {
        return currentTime;
    } else {
        return 0;
    }
}

/*
 * 'Code drop' from battery algo, move to independent file if needed
 */
void calculateNominalCapacity(struct nominalCapacityParams *params) {
    unsigned int significantChange = CAPACITY_NO_CHANGE;
    unsigned int debug = 0;
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

    // Set 'seed'
    if (fccDaySampleCount == 0 && fccAvgHistoryCount == 0) {
        // seed ncc with kInitialNominalCapacityPercentage if cycleCount <= kTrueNCCCycleCountThreshold
        for (int k = 0; k < vactModesCount; k++) {
            if (params->cycleCount >= 0 && params->cycleCount <= kTrueNCCCycleCountThreshold) {
                ncc[k] = (kInitialNominalCapacityPercentage * designCapacity / 100) - kSmartBattReserve_mAh;
            } else {
                // if cycleCount exceeds && this is the very first sample in time, seed with first fcc sample. We may need a scaled version of FCC based on cycleCounts and temp (?)
                ncc[k] = params->sample[k].fcc + kSmartBattReserve_mAh;
            }
            INFO_LOG("Seed value for nominal capacity: %d", ncc[k]);
            significantChange |= CAPACITY_SEED_CHANGE;
        }
        significantChange |= CAPACITY_NCC_CHANGE;
    }

    // Step 2: calculate average of day's FCC values
    currentThreshold = (designCapacity * NCC_CURRENT_THRESH) / NCC_CURRENT_THRESH_SCALE;
    // last FCC reading != current reading, last FCC reading was non-zero == AP was ON
    if ((fccGG != fccLast) && (fccLast != 0) && (temperature > NCC_TEMP_THRESH) && (abs(current) < currentThreshold)) {
        fccDaySampleCount = fccDaySampleCount < UINT32_MAX ? (fccDaySampleCount + 1) : UINT32_MAX;
        DEBUG_LOG("Qualified sample fcc: %d fccLast: %d temp: %d current: %d\n", fccGG, fccLast, temperature, current);
        for (int k = 0; k < vactModesCount; k++) {
            fccDaySampleAvg[k] = ((fccDaySampleCount - 1) * fccDaySampleAvg[k] + params->sample[k].fcc) / fccDaySampleCount;
        }
        significantChange |= CAPACITY_FCC_CHANGE;
    } else {
        if (fccGG == fccLast)
            debug |= FCC_SAMPLE_DROPPED_DUPLICATE;
        if (fccLast == 0)
            debug |= FCC_SAMPLE_DROPPED_ZERO;
        if (temperature <= NCC_TEMP_THRESH)
            debug |= FCC_SAMPLE_DROPPED_TEMP;
        if (abs(current) >= currentThreshold)
            debug |= FCC_SAMPLE_DROPPED_DIS;
    }

    // If the BHUI algorithm samples FCC before the gas gauge resimulates FCC at the lower discharge rate, it will effectively use the high discharge rate FCC in the average,
    // hence cahce the last sampled FCC for duplicacy checks regardless of it was qualified or not.
    fccLast = fccGG;

    // Step 3. Check if one epoch is over
    uint64_t ts = hasSamplingTimeExpired(params->ts);
    if (ts > 0)
        significantChange |= CAPACITY_SAMPLING_EPOCH_CHANGE;

    if (ts == -1)
        debug |= NEGATIVE_TS;

    if (ts > 0 && fccDaySampleCount) {
        // Step 4: iir filter the fcc average for the day to get the ncc
        for (int k = 0; k < vactModesCount; k++) {
            ncc[k] = (gamma * ncc[k] + (NCC_GAMMA_SCALE - gamma) * fccDaySampleAvg[k]) / NCC_GAMMA_SCALE;
            fccDaySampleAvg[k] = 0;
            significantChange |= CAPACITY_NCC_CHANGE;
            INFO_LOG("Recalculated NCC: %d\n", ncc[k]);
        }
        fccDaySampleCount = 0;
        fccAvgHistoryCount = fccAvgHistoryCount < UINT32_MAX ? (fccAvgHistoryCount + 1) : UINT32_MAX;
        params->ts = ts;
    } else if (ts) {
        INFO_LOG("No samples for the day found");
    }

    params->fccDaySampleCount = fccDaySampleCount;
    params->fccAvgHistoryCount = fccAvgHistoryCount;
    for (int k = 0; k < vactModesCount; k++) {
        params->sample[k].fccDaySampleAvg = fccDaySampleAvg[k];
        params->sample[k].ncc = ncc[k];
    }
    params->significantChange = significantChange;
    params->debug = debug;
    return;
}

void checkNominalCapacity(CFDictionaryRef batteryProps, CFMutableDictionaryRef dict,
        IOPSBatteryHealthServiceFlags *svcFlags)
{
    int fccp, rawMaxCap = 0, designCap = 0, cycleCount = -1;
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

    CFDictionaryGetIntValue(batteryProps, CFSTR(kIOPMPSCycleCountKey), cycleCount);
    if (cycleCount == -1) {
        ERROR_LOG("Invalid cycle count received");
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

    DEBUG_LOG("fccGG:%d T:%d I:%d vact:%d fccU:%d fccM:%d CC:%d\n", params.fcc, params.temperature, params.current, getVactState(), params.sample[vactModeDisabled].fcc,
        params.sample[vactModeEnabled].fcc, cycleCount);

    params.cycleCount = cycleCount;
    uint64_t ts = 0;
    CFDictionaryGetInt64Value(dict, CFSTR("ts"), ts);
    if (ts == 0) {
        ts = getTimeInSecsSinceEpoch();
        CFDictionarySetInt64Value(dict, CFSTR("ts"), ts);
        INFO_LOG("init health ts with %llu\n", ts);
    } else {
        INFO_LOG("stored ts %llu, current ts %llu\n", ts, getTimeInSecsSinceEpoch());
    }
    params.ts = ts;

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

    if (params.significantChange & CAPACITY_SEED_CHANGE) {
        INFO_LOG("Seed change detected: %d %d\n", params.sample[0].ncc, params.sample[1].ncc);
        CFDictionarySetIntValue(dict, CFSTR("seed0"), params.sample[0].ncc);
        CFDictionarySetIntValue(dict, CFSTR("seed1"), params.sample[1].ncc);
        CFDictionarySetIntValue(dict, CFSTR("seedTs"), params.ts);
    }

    if (params.significantChange & CAPACITY_SAMPLING_EPOCH_CHANGE) {
        CFDictionarySetInt64Value(dict, CFSTR("tsPrev"), ts);
        ts = params.ts;
        CFDictionarySetInt64Value(dict, CFSTR("ts"), ts);
        INFO_LOG("new epoch start: %llu\n", ts);
    }

    if (params.debug & NEGATIVE_TS) {
        uint64_t ts;
        ts = getTimeInSecsSinceEpoch();
        ERROR_LOG("Negative time delta encountered (%llu), reset clock (%llu)\n", params.ts, ts);
        CFDictionarySetInt64Value(dict, CFSTR("ts"), ts);
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

    DEBUG_LOG("nccService: %d capacityUI:%d nccU:%d nccM:%d vactMode:%d waitForFC:%d sigChange:0x%x\n", nccService, capacityUI, params.sample[vactModeDisabled].nccpMonotonic, params.sample[vactModeEnabled].nccpMonotonic, getVactState(), waitForFC, params.significantChange);
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

// Battery health is maintained at the lowest level seen
static const char *batteryHealth = kIOPSGoodValue;
static const char *batteryHealthCond = "";

#if TARGET_CPU_ARM64
// Indicates a prior service recommendation triggered by legacy FCC based battey health algorithm
// No-op for AS portables
static bool isPrevServiceRecommended(void)
{
    DEBUG_LOG("Skipping prevService check (%s)", batteryHealth);
    return false;
}
#else
// A string of Poor, Fair, Check from the legacy algorithm should be treated as service state for the new algorithm to pick up.
// Conditional check on customBatteryProps to aid testing.
static bool isPrevServiceRecommended(void)
{
    return (!customBatteryProps) && (strncmp(batteryHealth, kIOPSPoorValue, sizeof(kIOPSPoorValue)) == 0 || strncmp(batteryHealth, kIOPSFairValue, sizeof(kIOPSFairValue)) == 0 || strncmp(batteryHealth, kIOPSCheckBatteryValue, sizeof(kIOPSCheckBatteryValue)) == 0);
}
#endif

static void updateBatteryHealth(CFMutableDictionaryRef outDict, IOPMBattery *b)
{
    int epochCount = -1;
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

    CFDictionaryGetIntValue(dict, CFSTR(kFccAvgHistoryCount), epochCount);
    if (epochCount == -1) {
        ERROR_LOG("Invalid epoch count in persistent storage\n");
    }

    // Consider service recommendation from legacy algorithm only for the first epoch of the new algorithm, else ignore.
    if (epochCount == 0 && isPrevServiceRecommended()) {
        svcFlags |= kBHSvcFlagNCC | kBHSvcFlagPrevService;
        INFO_LOG("Detected previous service:%s (%d)\n", batteryHealth, svcFlags);
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

    if(pfStatusOverrideValue != -1) {
        b->pfStatus = (uint32_t) pfStatusOverrideValue;
    }

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

    // round charge and capacity down to a % scale
    if(0 != b->maxCap)
    {
        set_capacity = 100;
        set_charge = b->swCalculatedPR;

        if( (100 == set_charge) && b->isCharging)
        {
            // We will artificially cap the percentage to 99% while charging
            // Batteries may take 10-20 min beyond 100% of charging to
            // relearn their absolute maximum capacity. Leave cap at 99%
            // to indicate we're not done charging. (4482296, 3285870)
            set_charge = 99;
        }
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

            xpc_dictionary_set_value(respMsg, kReadPersistentBHData, respData);
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

__private_extern__ void setPermFaultStatus(xpc_object_t remoteConnection, xpc_object_t msg)
{
    int64_t pfStatus = 0;

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

    pfStatus = xpc_dictionary_get_int64(msg, kSetPermFaultStatus);
    if ((pfStatus < -1) || (pfStatus > UINT32_MAX)) {
        ERROR_LOG("Received invalid permanent battery failure status %lld\n", pfStatus);
        xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnBadArgument);
        goto exit;
    }

    pfStatusOverrideValue = pfStatus;
    if (pfStatusOverrideValue != -1)
    {
        INFO_LOG("Set permanent battery failure status to %u\n", (uint32_t)pfStatusOverrideValue);
    }
    else
    {
        INFO_LOG("Disabled permanent battery failure status override\n");
    }
    xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnSuccess);

exit:
    xpc_connection_send_message(remoteConnection, respMsg);
    xpc_release(respMsg);
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

            HandlePublishAllPowerSources();
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

// Log lines are limited in size. Avoid truncation by removing the
// kIOPSDebugInformationKey subdictionary from the standard logging and log it
// separately in debug level logs.
static void log_ps_dictionary(int psid, int callerPID, CFDictionaryRef details)
{
    NSDictionary *dbgDict = CFDictionaryGetValue(details, CFSTR(kIOPSDebugInformationKey));
    NSMutableDictionary *logDict = (NSMutableDictionary *)CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, details);
    [logDict removeObjectForKey : @(kIOPSDebugInformationKey)];

    INFO_LOG("Received power source(psid:%d) update from pid %d: %@\n", psid, callerPID, logDict);
    if (dbgDict) {
        DEBUG_LOG("Received power source(psid:%d) update from pid %d: %@\n", psid, callerPID, dbgDict);
    }

    [logDict release];
}

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
                        )
                    next->psType = kPSTypeUPS;
                else if (CFStringCompare(psTypeStr, CFSTR(kIOPSInternalBatteryType), 0) == kCFCompareEqualTo)
                    next->psType = kPSTypeIntBattery;
            }
        }

        log_ps_dictionary(psid, callerPID, details);
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
                HandlePublishAllPowerSources();
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

#if   TARGET_OS_OSX
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


static IOReturn HandleAccessoryPowerSources(PSStruct *ps, CFDictionaryRef update)
{
    CFNumberRef     n = NULL;
    int  old_cap = 0, new_cap = 0;
    CFStringRef old_src = NULL, new_src = NULL;

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
