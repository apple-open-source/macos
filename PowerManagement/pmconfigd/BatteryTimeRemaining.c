/*
 * Copyright (c) 2012 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2012 Apple Computer, Inc.  All rights reserved.
 *
 */
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <notify.h>
#include <mach/mach.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>
//#include <mach/clock.h>

#include <CoreFoundation/CFXPCBridge.h>
#include <servers/bootstrap.h>
#include <asl.h>
#include <bsm/libbsm.h>
#include <sys/time.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>

#if TARGET_OS_IPHONE || POWERD_IOS_XCTEST
#include <MobileKeyBag/MobileKeyBag.h>
#include <CoreFoundation/CFPreferences_Private.h>
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
#define kBattLogMaxEntries      64
#define kBattLogUpdateFreq      (5*60)  // 5 mins

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
static CFDictionaryRef  adapterDetails = NULL;

#if TARGET_OS_IPHONE || POWERD_IOS_XCTEST
bool smcBasedDevice = false;
bool nccp_cc_filtering = true;  // Support for NCCP filtering using CycleCount
uint64_t batteryHealthP0Threshold = 0;
uint64_t batteryHealthUPOAware = 0;
uint32_t battReadTimeDelta = kMinTimeDeltaForBattRead; // Time delta between reading battery data for battery health evaluation

void removeKeyFromBatteryHealthDataPrefs(CFStringRef key);
void saveBatteryHealthDataToPrefs(CFDictionaryRef bhData);
CFDictionaryRef copyBatteryHealthDataFromPrefs(void);
CFDictionaryRef copyPowerlogBatteryHealthData(void);
#endif


// forward declarations
STATIC PSStruct         *iops_newps(int pid, int psid);
static void             checkTimeRemainingValid(IOPMBattery **batts);
static CFDictionaryRef packageKernelPowerSource(IOPMBattery *b, PSStruct *ps);

static void             _discontinuityOccurred(void);
static void             publish_IOPSBatteryGetWarningLevel(IOPMBattery *b,
                                                           int combinedTime,
                                                           int percent);
static bool             publish_IOPSGetTimeRemainingEstimate(int timeRemaining,
                                                             bool external,
                                                             bool rawExternal,
                                                             bool timeRemainingUnknown,
                                                             bool isCharging,
                                                             bool showChargingUI,
                                                             bool playChargingChime,
                                                             bool noPoll);
static void             publish_IOPSGetPercentRemaining(int percent,
                                                        bool external,
                                                        bool isCharging,
                                                        bool fullyCharged,
                                                        IOPMBattery *b);

static void             HandlePublishAllPowerSources(void);
static IOReturn         HandleAccessoryPowerSources(PSStruct *ps, CFDictionaryRef update);
static CFDictionaryRef  getPSByType(CFStringRef type);
void initBatteryHealthData(void);
void _setBatteryHealthData( CFMutableDictionaryRef  outDict, IOPMBattery  *b);



// Arguments For startBatteryPoll()
typedef enum {
    kPeriodicPoll           = 0,
    kImmediateFullPoll      = 1
} PollCommand;
static bool             startBatteryPoll(PollCommand x);

static void BatteryTimeRemaining_notify_post(const char *token)
{
    uint32_t rc = notify_post(token);

    if (rc == NOTIFY_STATUS_OK) {
        INFO_LOG("posted '%s'\n", token);
    } else {
        ERROR_LOG("failed to post '%s'. rc:%#x\n", token, rc);
    }
}



__private_extern__ void
BatteryTimeRemaining_prime(void)
{

    battery_log = os_log_create(PM_LOG_SYSTEM, BATTERY_LOG);
    bzero(gPSList, sizeof(gPSList));
    bzero(&control, sizeof(BatteryControl));

    notify_register_check(kIOPSTimeRemainingNotificationKey,
                          &control.psTimeRemainingNotifyToken);
    notify_register_check(kIOPSNotifyPercentChange,
                          &control.psPercentChangeNotifyToken);

     // Initialize tracing battery events to FDR
     recordFDREvent(kFDRInit, false, NULL);



    /* Do initial full poll and kick of the polling timer */
    startBatteryPoll(kImmediateFullPoll);
    return;
}

__private_extern__ void BatteryTimeRemaining_finish(void)
{
    /* don't wait for notification if we already have battery info */
    IOPMBattery **b = _batteries();
    io_iterator_t       iter = MACH_PORT_NULL;
    kern_return_t       kr;
    io_registry_entry_t next;


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
        if ((next = IOIteratorNext(iter))) {
            ioregBatteryProcess(b[0], next);
            IOObjectRelease(next);
        }
        IOObjectRelease(iter);
    }
}

__private_extern__ void
BatteryTimeRemainingSleepWakeNotification(natural_t messageType)
{
    if (kIOMessageSystemWillPowerOn == messageType)
    {
        control.warningsShouldResetForSleep = true;
        control.readACAdapterAgain = true;

        _discontinuityOccurred();
    }
}

/*
 * When we wake from sleep, we call this function to make note of the
 * battery time remaining discontinuity after the RTC resyncs with the CPU.
 */
__private_extern__ void
BatteryTimeRemainingRTCDidResync(void)
{
    _discontinuityOccurred();
}

/*
 * A battery time remaining discontinuity has occurred
 * Make sure we don't publish a time remaining estimate at all
 * until a given period has elapsed.
 */
static void _discontinuityOccurred(void)
{
    if (slew) {
        bzero(slew, sizeof(SlewStruct));
    }
    control.lastDiscontinuity = CFAbsoluteTimeGetCurrent();
    control.percentageDiscontinuity = true;

    // Kick off a battery poll now,
    // and schedule the next poll in exactly 60 seconds.
    startBatteryPoll(kImmediateFullPoll);
}

void  initializeBatteryCalculations(void)
{
    if ((_batteryCount() == 0) || (control.internal != NULL)) {
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
    notify_post(kIOPSNotifyAttach);

    return;
}

static CFAbsoluteTime getASBMPropertyCFAbsoluteTime(CFStringRef key)
{
    CFNumberRef     secSince1970 = NULL;
    IOPMBattery     **b = _batteries();
    uint32_t        secs = 0;
    CFAbsoluteTime  return_val = 0.0;
    if (b && b[0] && b[0]->properties)
    {
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

static void updateLogBuffer(PSStruct *ps, bool asyncEvent)
{
    uint64_t        curTime = getMonotonicTime();
    CFTypeRef       n;
    CFDateRef       date = NULL;
    CFTimeZoneRef   tz = NULL;
    CFTimeInterval  diff = 0;
    CFAbsoluteTime  absTime;

    CFMutableDictionaryRef  entry = NULL;

    if ((ps == NULL) || (isA_CFDictionary(ps->description) == NULL)) return;

    if ((!asyncEvent) && (curTime - ps->logUpdate_ts < kBattLogUpdateFreq))
        return;

    if (ps->log == NULL) {
        ps->log = CFArrayCreateMutable(NULL, kBattLogMaxEntries, &kCFTypeArrayCallBacks);

        if (ps->log == NULL) return;
    }

    entry = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
                                      &kCFTypeDictionaryValueCallBacks);
    if (!entry) {
        goto exit;
    }

    // Current time of this activity
    tz = CFTimeZoneCopySystem();
    if (tz == NULL) {
        goto exit;
    }
    absTime = CFAbsoluteTimeGetCurrent();
    date = CFDateCreate(0, absTime);
    if (date == NULL) {
        goto exit;
    }
    CFDictionarySetValue(entry, CFSTR(kIOPSBattLogEntryTime), date);

    diff = CFTimeZoneGetSecondsFromGMT(tz, absTime);
    n = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &diff);
    if (n) {
        CFDictionarySetValue(entry, CFSTR(kIOPSBattLogEntryTZ), n);
        CFRelease(n);
    }

    n = CFDictionaryGetValue(ps->description, CFSTR(kIOPSCurrentCapacityKey));
    if (n) CFDictionarySetValue(entry, CFSTR(kIOPSCurrentCapacityKey), n);

    n = CFDictionaryGetValue(ps->description, CFSTR(kIOPSMaxCapacityKey));
    if (n) CFDictionarySetValue(entry, CFSTR(kIOPSMaxCapacityKey), n);

    n = CFDictionaryGetValue(ps->description, CFSTR(kIOPSPowerSourceStateKey));
    if (n) CFDictionarySetValue(entry, CFSTR(kIOPSPowerSourceStateKey), n);

    n = CFDictionaryGetValue(ps->description, CFSTR(kIOPSIsChargingKey));
    if (n) CFDictionarySetValue(entry, CFSTR(kIOPSIsChargingKey), n);

    n = CFDictionaryGetValue(ps->description, CFSTR(kIOPSCurrentKey));
    if (n) CFDictionarySetValue(entry, CFSTR(kIOPSCurrentKey), n);

    n = CFDictionaryGetValue(ps->description, CFSTR(kIOPSIsChargedKey));
    if (n) 
        CFDictionarySetValue(entry, CFSTR(kIOPSIsChargedKey), n);
    else
        CFDictionarySetValue(entry, CFSTR(kIOPSIsChargedKey), kCFBooleanFalse);

    CFArraySetValueAtIndex(ps->log, ps->logIdx , entry);
    ps->logIdx = (++ps->logIdx) % kBattLogMaxEntries;


    ps->logUpdate_ts = curTime;

exit:
    if (entry) CFRelease(entry);
    if (tz) CFRelease(tz);
    if (date) CFRelease(date);
}

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

    if (!_batteries()) {
        return false;
    }
    
    if (control.noPoll)
    {
        ERROR_LOG("Battery polling is disabled. powerd is skipping this battery udpate request.");
        return false;
    }

    if (!batteryPollingTimer) {
        batteryPollingTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());
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

__private_extern__ void BatterySetNoPoll(bool noPoll)
{

    if (control.noPoll != noPoll)
    {
        control.noPoll = noPoll;
        if (!noPoll) {
            startBatteryPoll(kImmediateFullPoll);
        } else {
            kernelPowerSourcesDidChange(kInternalBattery);
        }
    
        ERROR_LOG("Battery polling is now %s\n", noPoll ? "disabled." : "enabled. Initiating a battery poll.");
    }
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
     * Final Warning == On Battery with < 10 Minutes
     *
     */

    static CFStringRef lowBatteryKey = NULL;
    static int prevLoggedLevel = kIOPSLowBatteryWarningNone;
    int newWarningLevel = kIOPSLowBatteryWarningNone;

    if (control.warningsShouldResetForSleep || b->externalConnected) {
        // We reset the warning level upon system sleep or when external
        // power source is connected

        control.warningsShouldResetForSleep = false;
        if (control.systemWarningLevel != kIOPSLowBatteryWarningNone) {
            control.systemWarningLevel = 0;
            newWarningLevel = kIOPSLowBatteryWarningNone;
        }
    }
    else if (percent <= kPercentThresholdFinal) {
        newWarningLevel = kIOPSLowBatteryWarningFinal;
    }
    else if (combinedTime > 0) {
        if (combinedTime < kTimeThresholdFinal) {
            newWarningLevel = kIOPSLowBatteryWarningFinal;
        } else if (combinedTime < kTimeThresholdEarly) {
            newWarningLevel = kIOPSLowBatteryWarningEarly;
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

            notify_post(kIOPSNotifyLowBattery);
            if (newWarningLevel != prevLoggedLevel) {
                logASLLowBatteryWarning(newWarningLevel, combinedTime, b->currentCap);
                prevLoggedLevel = newWarningLevel;
            }
        }
        control.systemWarningLevel = newWarningLevel;
    }

    return;
}

static bool publish_IOPSGetTimeRemainingEstimate(
    int timeRemaining,
    bool external,
    bool rawExternal,
    bool timeRemainingUnknown,
    bool isCharging,
    bool showChargingUI,
    bool playChargingChime,
    bool noPoll)
{
    uint64_t            powerSourcesBitsForNotify = (uint64_t)(timeRemaining & 0xFFFF);
    static uint64_t     lastPSBitsNotify = 0;
    bool                posted = false;
    uint32_t            rc;
    
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

    /* These bits feed the SPI IOKit:IOPSGetSupportedPowerSources()
     *      - battery supported, UPS supported, active power sourecs
     */
    if (getActiveBatteryDictionary()) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyBattSupportBit;
    }
    if (getActiveUPSDictionary()) {
        powerSourcesBitsForNotify |= kPSTimeRemainingNotifyUPSSupportBit;
    }
    uint64_t activePS = getActivePSType();
    powerSourcesBitsForNotify |=
    (activePS & 0xFF) << kPSTimeRemainingNotifyActivePS8BitsStarts;



    if (lastPSBitsNotify != powerSourcesBitsForNotify)
    {
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
        if ((changedStateBits & kPSTimeRemainingNotifyExternalBit) && control.internal)
            updateLogBuffer(control.internal, true);
    }
}


__private_extern__ void
kernelPowerSourcesDidChange(IOPMBattery *b)
{
    static int                  _lastExternalConnected = -1;
    static int                  _lastPercentRemaining = 100;
    int                         _nowExternalConnected = 0;
    int                         percentRemaining = 0;
    IOPMBattery               **_batts = _batteries();

    /*
     * Initiate the next battery poll; or start a timer to poll
     * when the 60sec user visible polling timer expres.
     */
    startBatteryPoll(kPeriodicPoll);

    if (0 == _batteryCount()) {
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
        _discontinuityOccurred();
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
        updateLogBuffer(control.internal, false);
    }

    HandlePublishAllPowerSources();
}

static void HandlePublishAllPowerSources(void)
{
    IOPMBattery               **batteries = _batteries();
    IOPMBattery                *b = NULL;
    int                         combinedTime = 0;
    int                         percentRemaining = 0;
    static int                  prev_percentRemaining = 0;
    bool                        tr_posted;
    int                         ups_externalConnected = 0;
    static int                  ups_prevExternalConnected = -1;
    bool                        externalConnected, tr_unknown, is_charging, fully_charged;
    bool                        rawExternalConnected = false;
    bool                        showChargingUI = false;
    bool                        playChargingChime = false;
    CFDictionaryRef             ups = NULL;
    int                         ups_tr = -1;
    bool                        battcase_change = false;
    CFAbsoluteTime              bootUpdateTime = kCFAbsoluteTimeIntervalSince1970; // non-zero value

    ups = getActiveUPSDictionary();

    bootUpdateTime = getASBMPropertyCFAbsoluteTime(CFSTR(kFullPathKey));
    if (_batteryCount() && (batteries[0]->isPresent) && (bootUpdateTime != 0)) {
        b = batteries[0];
    }

    if ((b == NULL) && (ups == NULL)) {
        return;
    }
    is_charging = fully_charged = false;
    for (int i = 0; i < _batteryCount(); i++) {
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
                                         control.noPoll);

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

    prev_percentRemaining = percentRemaining;

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

        recordFDREvent(kFDRACChanged, false, batteries);

        INFO_LOG("Power Source change. Source:%{public}s", externalConnected ? "AC" : "Batt");
        BatteryTimeRemaining_notify_post(kIOPSNotifyPowerSource);
    }

    BatteryTimeRemaining_notify_post(kIOPSNotifyAnyPowerSource);

    /************************************************************************
     *
     * PUBLISH: Flight Data Recorder trace
     *
     ************************************************************************/
    recordFDREvent(kFDRBattEventPeriodic, false, batteries);

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

    int             i;
    IOPMBattery     *b;
    int             batCount = _batteryCount();

    for(i=0; i<batCount; i++)
    {
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

#if TARGET_OS_IPHONE || POWERD_IOS_XCTEST
//
// migrateSvcFlags - This function migrates powerlog's version of
// service flags(version 0 & 1) to version 2, as managed by powerd.
//
uint32_t migrateSvcFlags(IOPSBatteryHealthServiceState oldSvcState, IOPSBatteryHealthServiceFlags oldSvcFlags)
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
void saveBatteryHealthKeyValueToPrefs(const void *key, const void *value, void *context __unused)
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
#else
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
#endif


/*
 * This function returns previous battery health data saved to disk.
 * This function will return NULL only when there is no previous battery health data in powerd's CFPrefs
 * and system is not unlocked even once to migrate data from powerlog.
 */
CFMutableDictionaryRef copyBatteryHealthData( )
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

void initBatteryHealthData()
{
    int status;
    int token;
    CFDictionaryRef dict;

    status = notify_register_dispatch("com.apple.system.batteryHealth.p0Threshold", &token, _getPMMainQueue(),
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

    status = notify_register_dispatch("com.apple.system.batteryHealth.UPOAware", &token, _getPMMainQueue(),
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
        status = notify_register_dispatch(kMobileKeyBagFirstUnlockNotificationID, &token, _getPMMainQueue(),
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

    if (ncc && (designCap > 0)) {
        nccp = ceil((double)ncc/(double)designCap * 100);
    }
    else {
        nccp = 0;
    }

    // The NCC Not Determinable flag will be set if DesignCapacity ≤ 0, if NCC% ∉ [1,150], or
    // if either NominalChargeCapacity or DesignCapacity keys are missing from battery properties.
    if ((nccp < kMinNominalCapacityPercentage) || (nccp > kMaxNominalCapacityPercentage)) {
        ERROR_LOG("Failed to calculate Nominal Capacity percentage. NominalCapacity:%d DesignCapacity:%d\n",
                ncc, designCap);
        *svcFlags |= kBHSvcFlagNCCNotDet;

        // No need to update anything else
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

    if (nccp < kNominalCapacityPercetageThreshold) {
        *svcFlags |= kBHSvcFlagNCC;
        INFO_LOG("Nominal Capacity percentage(%d) is less than the threshold(%d)\n", nccp, kNominalCapacityPercetageThreshold);
    }

    CFDictionarySetIntValue(bhData, CFSTR(kIOPSBatteryHealthMaxCapacityPercent), nccp);

    DEBUG_LOG("Battery NominalCapacity:%d DesignCapacity:%d NCC:%d\n", ncc, designCap, nccp);
}

void checkUPOCount(IOPSBatteryHealthServiceFlags *svcFlags)
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

void checkWeightedRa(CFDictionaryRef batteryProps, IOPSBatteryHealthServiceFlags *svcFlags)
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

void checkCellDisconnectCount(CFDictionaryRef batteryProps, IOPSBatteryHealthServiceFlags *svcFlags)
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

        if (svcFlags & kBHSvcFlagBCDC) {
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

void _setBatteryHealthData(
    CFMutableDictionaryRef  outDict,
    IOPMBattery  *b)
{
    IOPSBatteryHealthServiceFlags svcFlags = kBatteryHealthCurrentVersion;
    IOPSBatteryHealthServiceState svcState = 0;
    CFDictionaryRef  batteryProps = b->properties;

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


__private_extern__ void setBHUpdteTimeDelta(xpc_object_t remoteConnection, xpc_object_t msg)
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

#else
#define SECONDS_IN_WEEK     (7 * 24 * 60 * 60)
// Set health & confidence
void _setBatteryHealthData(
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
        if (CFEqual(b->failureDetected, CFSTR(kBatteryPermFailureString))) {
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

#if TARGET_OS_OSX || TARGET_OS_BRIDGE
    // This test was designed for MacOS only and false triggers on Watches due
    // to Dali reserve.
    // iOS/WatchOS use different ways to determine battery health.
    if (b->designCap) {
        capRatio = ((double)b->maxCap + kSmartBattReserve_mAh) / (double)b->designCap;
    }
#endif
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
#endif

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
CFDictionaryRef packageKernelPowerSource(IOPMBattery *b, PSStruct *ps)
{
    CFNumberRef     n, n0;
    CFMutableDictionaryRef  mDict = NULL;
    int             temp;
    int             minutes;
    int             set_capacity, set_charge;
    int             psID;

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
    //
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

// _readAndPublicACAdapter
__private_extern__ void readAndPublishACAdapter(bool adapterExists, CFDictionaryRef newAdapter)
{
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
    xpc_object_t respData = NULL;
    respData = _CFXPCCreateXPCObjectFromCFObject(adapterDetails);

    xpc_dictionary_set_value(respMsg, kPSAdapterDetails, respData);
    xpc_connection_send_message(remoteConnection, respMsg);

    DEBUG_LOG("Returned adapter details dictionary %{public}@\n", adapterDetails);
    if (respData) {
        xpc_release(respData);
    }
    xpc_release(respMsg);
}


/**** User-space power source code lives below here ********************************/
/***********************************************************************************/
/***********************************************************************************/
/***********************************************************************************/



/***********************************************************************************/
STATIC PSStruct *iops_newps(int pid, int psid)
{
    // Find the first empty slot in gPSList
    int i = kPSMaxCount;
    if (psid == kSpecialInternalBatteryID) {
        // Reserve 0 for internal battery
        i = 0;
    }
    else {
        for (i=1; i<kPSMaxCount; i++)
        {
            if (0 == gPSList[i].psid)
            {
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
    for (int i=0; i<kPSMaxCount; i++)
    {
        if (gPSList[i].psid == _psid
            && gPSList[i].pid == _pid)
        {
            return &gPSList[i];
        }
    }

    return NULL;
}


__private_extern__ CFDictionaryRef getActiveBatteryDictionary(void)
{
    for (int i=0; i<kPSMaxCount; i++)
    {
        if (!gPSList[i].description) {
            continue;
        }

        CFStringRef transport_type = NULL;
        transport_type = CFDictionaryGetValue(gPSList[i].description,
                                              CFSTR(kIOPSTransportTypeKey));
        if (isA_CFString(transport_type)
            && ( CFEqual(transport_type, CFSTR(kIOPSInternalType))))
        {
            return gPSList[i].description;
        }
    }
    return NULL;
}

static CFDictionaryRef getPSByType(CFStringRef type)
{

    for (int i=0; i<kPSMaxCount; i++)
    {
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

__private_extern__ CFDictionaryRef getActiveUPSDictionary(void)
{
    return getPSByType(CFSTR(kIOPSUPSType));
}


__private_extern__ int getActivePSType(void)
{
    CFDictionaryRef activeBattery = getActiveBatteryDictionary();
    CFDictionaryRef activeUPS = getActiveUPSDictionary();
    CFStringRef     ps_state = NULL;

    /* if (!activeBattery) is testing for whether batteries are supported on
     * this system at all, e.g. mobile vs desktop. */
    if(!activeBattery)
    {
        if(!activeUPS) {
            // no batteries, no UPS -> AC Power
            return kIOPSProvidedByAC;
        } else {
            ps_state = CFDictionaryGetValue(activeUPS,
                                            CFSTR(kIOPSPowerSourceStateKey));
            if(ps_state && CFEqual(ps_state, CFSTR(kIOPSACPowerValue)))
            {
                // no batteries, yes UPS, UPS is running off of AC power -> AC Power
                return kIOPSProvidedByAC;
            } else if(ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue)))
            {
                // no batteries, yes UPS, UPS is running drawing its Battery power -> UPS Power
                return kIOPSProvidedByExternalBattery;
            }

        }
        // Error in the data we were passed
        return kIOPSProvidedByAC;
    } else {

        ps_state = CFDictionaryGetValue(activeBattery,
                                        CFSTR(kIOPSPowerSourceStateKey));
        if(ps_state && CFEqual(ps_state,
                               CFSTR(kIOPSBatteryPowerValue)))
        {
            // Yes batteries, yes running on battery power -> Battery power
            return kIOPSProvidedByBattery;
        }
        else
        {
            // batteries are on AC power. let's check if UPS is present.
            if (!activeUPS)
            {
                // yes batteries on AC power, no UPS present -> AC Power
                return kIOPSProvidedByAC;
            } else {
                ps_state = CFDictionaryGetValue(activeUPS,
                                                CFSTR(kIOPSPowerSourceStateKey));
                if(ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue)))
                {
                    // yes batteries on AC power, UPS is on its battery -> UPS Power
                    return kIOPSProvidedByExternalBattery;
                } else if(ps_state && CFEqual(ps_state, CFSTR(kIOPSACPowerValue)))
                {
                    // yes batteries on AC Power, UPS is drawing AC Power -> AC Power
                    return kIOPSProvidedByAC;
                }
            }
        }
    }

    // Should not reach this point. Return something safe.
    return kIOPSProvidedByAC;
}


/***********************************************************************************/
// MIG handler - back end for IOKit API IOPSCreatePowerSource
kern_return_t _io_ps_new_pspowersource(
    mach_port_t                 server __unused,
    audit_token_t               token,
    int                         *psid,              // out
    int                         *result)
{
    static unsigned int         gPSID = 5000;
    int                         callerPID;
    PSStruct                    *ps;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL,
                        &callerPID, NULL, NULL);

    *result = kIOReturnError;

    ps = iops_newps(callerPID, gPSID);
    if (!ps)
    {
        *result = kIOReturnNoSpace;
        goto exit;
    }

    ps->procdeathsrc= dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC,
                                                callerPID,
                                                DISPATCH_PROC_EXIT,
                                                _getPMMainQueue());

    /* Setup automatic cleanup if client process dies
     */
    dispatch_source_set_cancel_handler(ps->procdeathsrc, ^{
        /*
         * When the client process dies, remove
         * this power source and stop showing it to IOPS API clients.
         *
         */

        if (ps->psType == kPSTypeAccessory) {
            BatteryTimeRemaining_notify_post(kIOPSAccNotifyTimeRemaining);
            BatteryTimeRemaining_notify_post(kIOPSAccNotifyAttach);
        }
        else {
            BatteryTimeRemaining_notify_post(kIOPSNotifyTimeRemaining);
            BatteryTimeRemaining_notify_post(kIOPSNotifyAttach);
        }
        INFO_LOG("Posted notifications for loss of power source id %ld\n", ps->psid);
        if (ps->procdeathsrc) {
            dispatch_release(ps->procdeathsrc);
        }
        if (ps->description) {
            CFRelease(ps->description);
        }
        if (ps->log) {
            CFRelease(ps->log);
        }
        bzero(ps, sizeof(PSStruct));

        dispatch_async(_getPMMainQueue(), ^()
                       {
                           HandlePublishAllPowerSources();
                       });
    });

    dispatch_source_set_event_handler(ps->procdeathsrc, ^{
        dispatch_source_cancel(ps->procdeathsrc);
    });

    dispatch_resume(ps->procdeathsrc);


    *psid = gPSID++;
    if (*psid == 0)
        *psid = gPSID; // Avoid 0 as psid
    *result = kIOReturnSuccess;
    INFO_LOG("Created new power source id %d for pid %d\n", *psid, callerPID);

exit:
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
    CFMutableDictionaryRef     details = NULL;
    int                 callerPID;
    CFStringRef         psTypeStr = NULL;
    CFNumberRef         psIDKey = NULL;
    int                 psID = 0;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL,
                        &callerPID, NULL, NULL);

    *return_code = kIOReturnError;

    details = (CFMutableDictionaryRef)IOCFUnserialize((const char *)details_ptr, NULL, 0, NULL);

    if (!isA_CFDictionary(details))
    {
        *return_code = kIOReturnBadArgument;
    } else {
        PSStruct *next = iopsFromPSID(callerPID, psid);
        if (!next) {
            ERROR_LOG("Failed to find the power source for psid 0x%x from pid %d\n", psid, callerPID);
            *return_code = kIOReturnNotFound;
        } else {
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
                updateLogBuffer(next, false);
                dispatch_async(_getPMMainQueue(), ^()
                           {
                               HandlePublishAllPowerSources();
                           });
                *return_code = kIOReturnSuccess;
            }
            else if (next->psType == kPSTypeAccessory) {
               *return_code = HandleAccessoryPowerSources(next, details);
            }
        }
    }

    if (kIOReturnSuccess != *return_code) {
        if (details) {
            CFRelease(details);
        }
    }

    vm_deallocate(mach_task_self(), details_ptr, details_len);
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

    INFO_LOG("Releasing power source id = %d\n", psid);

    PSStruct *toRelease = iopsFromPSID(callerPID, psid);
    if (toRelease) {
        dispatch_source_cancel(toRelease->procdeathsrc);
    }
    return 0;
}

CFDictionaryRef copyWithBatteryHealthData(audit_token_t token, CFDictionaryRef batteryData)
{
    CFRetain(batteryData);
    return batteryData;
}

kern_return_t _io_ps_copy_powersources_info(
    mach_port_t             server __unused,
    audit_token_t           token,
    int                     type,
    vm_offset_t             *ps_ptr,
    mach_msg_type_number_t  *ps_len,
    int                     *return_code)
{
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
            INFO_LOG("Posted \"%s\" for power source id %ld\n", kIOPSAccNotifyPowerSource, ps->psid);
        }

        if (do_notify_tr) {
            BatteryTimeRemaining_notify_post(kIOPSAccNotifyTimeRemaining);
            INFO_LOG("Posted \"%s\" for power source id %ld\n", kIOPSAccNotifyTimeRemaining, ps->psid);
        }

        CFRelease(ps->description);
    }
    else {
        /* This is a new accessory with power source */
        BatteryTimeRemaining_notify_post(kIOPSAccNotifyAttach);
        BatteryTimeRemaining_notify_post(kIOPSAccNotifyTimeRemaining);
        INFO_LOG("Posted notifications for new power source id %ld\n", ps->psid);
    }

    ps->description = update;
    return kIOReturnSuccess;
}


CFArrayRef copyPowerSourceLog(PSStruct *ps, CFAbsoluteTime ts)
{
    CFIndex         i, arrCnt;
    CFDateRef       entry_ts = NULL;
    CFDateRef       input_ts = NULL;

    CFDictionaryRef         entry = NULL;
    CFMutableArrayRef       updates = NULL;


    arrCnt = CFArrayGetCount(ps->log);

    if (arrCnt == 0)
        goto exit;

    updates = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (updates == NULL) {
        goto exit;
    }

    input_ts = CFDateCreate(NULL, ts);
    if (input_ts == NULL) {
        goto exit;
    }

    if (arrCnt < kBattLogMaxEntries)
        ps->logIdx = 0;

    i = ps->logIdx; 
    do { 
        entry = NULL;
        if (i < arrCnt)
            entry = CFArrayGetValueAtIndex(ps->log, i);
        if (!isA_CFDictionary(entry)) {
            i = (i+1) % kBattLogMaxEntries;
            if (i >= arrCnt) i = 0;
            continue;
        }

        if (!entry_ts) {
            entry_ts = CFDictionaryGetValue(entry, CFSTR(kIOPSBattLogEntryTime));

            if ((entry_ts == NULL) || (CFDateCompare(entry_ts, input_ts, NULL) == kCFCompareLessThan)) {
                i = (i+1) % kBattLogMaxEntries;
                if (i >= arrCnt) i = 0;
                entry_ts = NULL;
                continue;
            }
        }

        CFArrayAppendValue(updates, entry);
        i = (i+1) % kBattLogMaxEntries;
        if (i >= arrCnt) i = 0;

    } while (i != ps->logIdx);

    CFArrayRemoveAllValues(ps->log);
    ps->logIdx = 0;

exit:

    if (input_ts)
        CFRelease(input_ts);

    return updates;
}

kern_return_t _io_ps_copy_chargelog(
    mach_port_t             server __unused,
    audit_token_t           token,
    double                  ts,
    vm_offset_t             *updates,
    mach_msg_type_number_t  *updates_len,
    int                     *rc)
{
    CFDataRef               serializedLog = NULL;
    CFArrayRef              psLog = NULL;
    CFStringRef             name = NULL;
    CFMutableDictionaryRef  logDict = NULL;
    CFErrorRef              err = NULL;

    *updates = 0; *updates_len = 0;
    *rc = kIOReturnNotFound;

    if (!auditTokenHasEntitlement(token, CFSTR("com.apple.private.iokit.powerlogging"))) 
    {
        *rc = kIOReturnNotPrivileged;
        goto exit;
    }

    logDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!logDict)
        goto exit;

    for (int i=0; i<kPSMaxCount; i++)
    {
        if (!gPSList[i].log) {
            continue;
        }

        name = CFDictionaryGetValue(gPSList[i].description, CFSTR(kIOPSNameKey));
        if (!isA_CFString(name)) {
            continue;
        }

        psLog = copyPowerSourceLog(&gPSList[i], ts);
        if (!psLog)
            continue;

        CFDictionarySetValue(logDict, name, psLog);
        CFRelease(psLog);

    }

    serializedLog = CFPropertyListCreateData(0, logDict,
                                             kCFPropertyListBinaryFormat_v1_0, 0, &err);            

    if (!serializedLog)
        goto exit;

    *updates_len = (mach_msg_type_number_t)CFDataGetLength(serializedLog);
    vm_allocate(mach_task_self(), (vm_address_t *)updates, *updates_len, TRUE);
    if (*updates == 0)
        goto exit;

    memcpy((void *)*updates, CFDataGetBytePtr(serializedLog), *updates_len);
    *rc = kIOReturnSuccess;
 

exit:
    if (logDict) CFRelease(logDict);
    if (serializedLog) CFRelease(serializedLog);

    return KERN_SUCCESS;

}


