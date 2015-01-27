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
#include <servers/bootstrap.h>
#include <asl.h>
#include <bsm/libbsm.h>
#include <sys/time.h>

#include "powermanagementServer.h" // mig generated
#include "BatteryTimeRemaining.h"
#include "PMSettings.h"
#include "UPSLowPower.h"
#include "PMAssertions.h"
#include "PrivateLib.h"
#include "PMStore.h"


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


/* PSStruct 
 * Contains all the details about each power source that the system describes.
 * This struct is the backbone of the IOPowerSources() IOKit API for
 * power source reporting.
 */
typedef struct {
    // powerd will assign a unique psid to all sources.
    long                psid;

    // Ensure that only the process that created
    // a ps may modify it or destroy it, by recording caller's pid.
    int                 pid;
    dispatch_source_t   procdeathsrc;

    // This is the most current recorded state of this power source.
    CFDictionaryRef     description;

    // log of previous battery updates, maintained as ring buffer
    CFMutableArrayRef       log;         
    CFIndex                 logIdx;         // Index for next record
    uint64_t                logUpdate_ts;   // Timestamp of last log
} PSStruct;

#define kBattLogMaxEntries      64
#define kBattLogUpdateFreq      (5*60)  // 5 mins

#define kPSMaxCount   7

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


// static global variables for tracking battery state
typedef struct {
    CFAbsoluteTime   lastDiscontinuity;
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


// forward declarations
static PSStruct         *iops_newps(int pid, int psid);
static void             _initializeBatteryCalculations(void);
static void             checkTimeRemainingValid(IOPMBattery **batts);
static CFDictionaryRef packageKernelPowerSource(IOPMBattery *b);

static void             _discontinuityOccurred(void);
static void             _readAndPublishACAdapter(bool, CFDictionaryRef);
static void             publish_IOPSBatteryGetWarningLevel(IOPMBattery *b,
                                                           int combinedTime);
static bool             publish_IOPSGetTimeRemainingEstimate(int timeRemaining,
                                                             bool external,
                                                             bool timeRemainingUnknown,
                                                             bool isCharging,
                                                             bool noPoll);
static void             publish_IOPSGetPercentRemaining(int percent,
                                                        bool external,
                                                        bool isCharging,
                                                        bool fullyCharged,
                                                        IOPMBattery *b);

static void             HandlePublishAllPowerSources(void);



// Arguments For startBatteryPoll()
typedef enum {
    kPeriodicPoll           = 0,
    kImmediateFullPoll      = 1
} PollCommand;
static bool             startBatteryPoll(PollCommand x);


__private_extern__ void
BatteryTimeRemaining_prime(void)
{

    bzero(gPSList, sizeof(gPSList));
    bzero(&control, sizeof(BatteryControl));

    notify_register_check(kIOPSTimeRemainingNotificationKey,
                          &control.psTimeRemainingNotifyToken);
    notify_register_check(kIOPSNotifyPercentChange,
                          &control.psPercentChangeNotifyToken);

     // Initialize tracing battery events to FDR
     recordFDREvent(kFDRInit, false, NULL);

#if !TARGET_OS_EMBEDDED
#endif
    _initializeBatteryCalculations();

    /*
     *Initiate the next battery poll; or start a timer to poll
     * when the 60sec user visible polling timer expres.
     */
    startBatteryPoll(kPeriodicPoll);
    return;
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
    
    // Kick off a battery poll now,
    // and schedule the next poll in exactly 60 seconds.
    startBatteryPoll(kImmediateFullPoll);
}

static void     _initializeBatteryCalculations(void)
{
    if (_batteryCount() == 0) {
        return;
    }

    // Does this Mac have an internal battery
    // reported through IOPMPowerSource?
    // If so, we'll track it in the gPSList.

    // Any other processes that publish power sources (like upsd)
    // will get a powersource id > 5000
    const int kSpecialInternalBatteryID = 99;
    control.internal = iops_newps(getpid(), kSpecialInternalBatteryID);

    control.lastDiscontinuity = CFAbsoluteTimeGetCurrent();

    // make initial call to populate array and publish state
    kernelPowerSourcesDidChange(kInternalBattery);

    return;
}

#if !TARGET_OS_EMBEDDED
static CFAbsoluteTime getASBMPropertyCFAbsoluteTime(CFStringRef key)
{
    CFNumberRef     secSince1970 = NULL;
    IOPMBattery     **b = _batteries();
    uint32_t        secs = 0;
    CFAbsoluteTime  return_val = 0.0;
/*
    if (b && b[0] && b[0]->me)
    {
        secSince1970 = IORegistryEntryCreateCFProperty(b[0]->me, key, 0, 0);
        if (secSince1970) {
*/
    if (b && b[0] && b[0]->properties)
    {
        secSince1970 = CFDictionaryGetValue(b[0]->properties, key);
        if (secSince1970) {
            CFNumberGetValue(secSince1970, kCFNumberIntType, &secs);
//            CFRelease(secSince1970);
            return_val = (CFAbsoluteTime)secs - kCFAbsoluteTimeIntervalSince1970;
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
#endif

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
    if (!entry) return;

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
#if !TARGET_OS_EMBEDDED
    const static CFTimeInterval     kUserVisibleMinFrequency = 55.0;
    const static CFTimeInterval     kFullMinFrequency = 595.0;
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
    
    if (!_batteries())
        return false;
    
    if (control.noPoll)
    {
        asl_log(0, 0, ASL_LEVEL_ERR, "Battery polling is disabled. powerd is skipping this battery udpate request.");
        return false;
    }
    
    if (kImmediateFullPoll == doCommand) {
        doFull = true;
    } else {
        
        lastUpdateTime = getASBMPropertyCFAbsoluteTime(CFSTR(kBootPathKey));
        if (lastUpdateTime < now) lastBootUpdate = lastUpdateTime;
        lastUpdateTime = getASBMPropertyCFAbsoluteTime(CFSTR(kFullPathKey));
        if (lastUpdateTime < now) lastFullUpdate = lastUpdateTime;
        lastUpdateTime = getASBMPropertyCFAbsoluteTime(CFSTR(kUserVisPathKey));
        if (lastUpdateTime < now) lastUserVisibleUpdate = lastUpdateTime;
        
        sinceUserVisible = now - mostRecent(lastBootUpdate, lastFullUpdate, lastUserVisibleUpdate);
        if (sinceUserVisible > kUserVisibleMinFrequency) {
            doUserVisible = true;
        }

        sinceFull = now - mostRecent(lastBootUpdate, lastFullUpdate, 0);
        if (sinceFull > kFullMinFrequency) {
            doFull = true;
        }
    }
    
    if (doFull) {
        IOPSRequestBatteryUpdate(kIOPSReadAll);
    } else if (doUserVisible) {
        IOPSRequestBatteryUpdate(kIOPSReadUserVisible);
    } else {
        // We'll wait until kPollIntervalNS has elapsed since the last user visible poll.
        uint64_t checkAgainNS = kPollIntervalNS - (sinceUserVisible*NSEC_PER_SEC);

        if (!batteryPollingTimer) {
            batteryPollingTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
            dispatch_source_set_event_handler(batteryPollingTimer, ^() { startBatteryPoll(kPeriodicPoll); });
            dispatch_resume(batteryPollingTimer);
        }
        dispatch_source_set_timer(batteryPollingTimer, dispatch_time(DISPATCH_TIME_NOW, checkAgainNS), DISPATCH_TIME_FOREVER, 0);
    }
#endif
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
            // Announce the cessation of polling to the world
            // (by publishing 55% 5:55 to BatteryMonitor)
            kernelPowerSourcesDidChange(kInternalBattery);
        }
    
        asl_log(0, 0, ASL_LEVEL_ERR, "Battery polling is now %s\n", noPoll ? "disabled." : "enabled. Initiating a battery poll.");
    }
}


#define kTimeThresholdEarly          20
#define kTimeThresholdFinal          10

static void publish_IOPSBatteryGetWarningLevel(
    IOPMBattery *b,
    int combinedTime)
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
    
    if (control.warningsShouldResetForSleep)
    {
        // We reset the warning level upon system sleep.
        control.warningsShouldResetForSleep = false;
        control.systemWarningLevel = 0;
        newWarningLevel = kIOPSLowBatteryWarningNone;
        
    } else if (b->externalConnected)
    {
        // We reset the warning level whenever AC is attached.
        control.systemWarningLevel = 0;
        newWarningLevel = kIOPSLowBatteryWarningNone;
        
    } else if (combinedTime > 0)
    {
        if (combinedTime < kTimeThresholdFinal)
        {
            newWarningLevel = kIOPSLowBatteryWarningFinal;       
        } else if (combinedTime < kTimeThresholdEarly)
        {
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
        && (0 != newWarningLevel) )
    {
        CFNumberRef newlevel = CFNumberCreate(0, kCFNumberIntType, &newWarningLevel);
        
        if (newlevel)
        {
            if (!lowBatteryKey) {
                lowBatteryKey = SCDynamicStoreKeyCreate(
                        kCFAllocatorDefault, CFSTR("%@%@"),
                        kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStoreLowBattPathKey));
            }
            
            PMStoreSetValue(lowBatteryKey, newlevel );
            CFRelease(newlevel);
            
            notify_post(kIOPSNotifyLowBattery);
            if ((newWarningLevel != prevLoggedLevel) && (newWarningLevel != kIOPSLowBatteryWarningNone)) {
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
    bool timeRemainingUnknown,
    bool isCharging,
    bool noPoll)
{
    uint64_t            powerSourcesBitsForNotify = (uint64_t)(timeRemaining & 0xFFFF);
    static uint64_t     lastPSBitsNotify = 0;
    bool                posted = false;
    
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
        notify_post(kIOPSNotifyTimeRemaining);
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
#if TARGET_OS_EMBEDDED
    if (b && b->isCritical)
        currentStateBits |= kPSCriticalLevelBit;
    if (b && b->isRestricted)
        currentStateBits |= kPSRestrictedLevelBit;
#endif

    changedStateBits = lastStateBits ^ currentStateBits;
    if (changedStateBits)
    {
        lastStateBits = currentStateBits;
        notify_set_state(control.psPercentChangeNotifyToken, currentStateBits);

        // Suppress notification for charging state changes
        ignoreBits = (kPSTimeRemainingNotifyChargingBit 
                                 |kPSTimeRemainingNotifyFullyChargedBit
#if TARGET_OS_EMBEDDED
                                 |kPSCriticalLevelBit
                                 |kPSRestrictedLevelBit
#endif
                                 );
        if (changedStateBits & ~ignoreBits)
        {
            notify_post(kIOPSNotifyPercentChange);
        }
#if TARGET_OS_EMBEDDED
        if (changedStateBits & kPSCriticalLevelBit)
            notify_post(kIOPSNotifyCriticalLevel);
        if (changedStateBits & kPSRestrictedLevelBit)
            notify_post(kIOPSNotifyRestrictedMode);
#endif
        if ((changedStateBits & kPSTimeRemainingNotifyExternalBit) && control.internal)
            updateLogBuffer(control.internal, true);
    }
}


__private_extern__ void
kernelPowerSourcesDidChange(IOPMBattery *b)
{
    static int                  _lastExternalConnected = -1;
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

    _nowExternalConnected = (b->externalConnected ? 1 : 0);
    if (_lastExternalConnected != _nowExternalConnected) {
        // If AC has changed, we must invalidate time remaining.
        _discontinuityOccurred();
        control.needsNotifyAC = true;

        _lastExternalConnected = _nowExternalConnected;
    }
    _readAndPublishACAdapter(b->externalConnected,
                             CFDictionaryGetValue(b->properties, CFSTR(kIOPMPSAdapterDetailsKey)));



    checkTimeRemainingValid(_batts);

    if (b->maxCap) {
        double percent = (double)(b->currentCap * 100) / (double)b->maxCap;
        percentRemaining = (int) lround(percent);
        if (percentRemaining > 100)
            percentRemaining = 100;
    }
    // b->swCalculatedPR is used by packageKernelPowerSource()
    b->swCalculatedPR = percentRemaining;

    /************************************************************************
     *
     * PUBLISH: SCDynamicStoreSetValue / IOPSCopyPowerSourcesInfo()
     *
     ************************************************************************/
    if (control.internal) {
        if (control.internal->description) {
            CFRelease(control.internal->description);
        }
        control.internal->description = packageKernelPowerSource(b);
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
    bool                        ups_externalConnected = false;
    bool                        externalConnected, tr_unknown, is_charging, fully_charged;
    CFDictionaryRef             ups = NULL;
    int                         ups_tr = -1;

    if ((0 == _batteryCount()) && ((ups = getActiveUPSDictionary()) == NULL) ) {
        return;
    }

    if (_batteryCount())
        b = batteries[0];

    externalConnected = tr_unknown = is_charging = fully_charged = false;
    for(int i=0; i<_batteryCount(); i++)
    {
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
        if (src && (CFStringCompare(src, CFSTR(kIOPSACPowerValue), kNilOptions) == kCFCompareEqualTo))
            ups_externalConnected = true;
    }
    
    if (b) {
            tr_unknown = b->isTimeRemainingUnknown;
            is_charging = b->isCharging;
            percentRemaining = b->swCalculatedPR;;
            fully_charged = isFullyCharged(b);

            if (ups)
                externalConnected = b->externalConnected && ups_externalConnected;
            else
                externalConnected = b->externalConnected;
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
                                         tr_unknown,
                                         is_charging,
                                         control.noPoll);
    
    if (b) {
        publish_IOPSBatteryGetWarningLevel(b, combinedTime);
    }

    publish_IOPSGetPercentRemaining(percentRemaining, 
                                    externalConnected, 
                                    is_charging,
                                    fully_charged,
                                    b);
    
    if ((percentRemaining != prev_percentRemaining) && !tr_posted) {
        notify_post(kIOPSNotifyTimeRemaining);
    }

    prev_percentRemaining = percentRemaining;

    /************************************************************************
     *
     * TELL: powerd-internal code that responds to power changes
     ************************************************************************/

     
 #if !TARGET_OS_EMBEDDED
     // Notifiy PSLowPower of power sources change
    UPSLowPowerPSChange();
    PMSettingsPSChange();
 #endif


    /************************************************************************
     *
     * NOTIFY: Providing power source changed.
     *          via notify(3)
     ************************************************************************/
    if (control.needsNotifyAC) {
        control.needsNotifyAC = false;

        recordFDREvent(kFDRACChanged, false, batteries);

        notify_post(kIOPSNotifyPowerSource);
    }


    notify_post(kIOPSNotifyAnyPowerSource);

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

// Set health & confidence
void _setBatteryHealthConfidence(
    CFMutableDictionaryRef  outDict,
    IOPMBattery             *b)
{
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
    
    static char* batteryHealth = "";
    
    // Permanent failure -> Poor health
    if (_batteryHas(b, CFSTR(kIOPMPSErrorConditionKey)))
    {
        if (CFEqual(b->failureDetected, CFSTR(kBatteryPermFailureString)))
        {
            CFDictionarySetValue(outDict,
                    CFSTR(kIOPSBatteryHealthKey), CFSTR(kIOPSPoorValue));
            CFDictionarySetValue(outDict,
                    CFSTR(kIOPSHealthConfidenceKey), CFSTR(kIOPSGoodValue));
            // Specifically log that the battery condition is permanent failure
            CFDictionarySetValue(outDict,
                    CFSTR(kIOPSBatteryHealthConditionKey), CFSTR(kIOPSPermanentFailureValue));
            
            if (strncmp(batteryHealth, kIOPSPoorValue, sizeof(kIOPSPoorValue))) {
                logASLBatteryHealthChanged(kIOPSPoorValue,
                                           batteryHealth,
                                           kIOPSPermanentFailureValue);
                batteryHealth = kIOPSPoorValue;
            }
            
            return;
        }
    }

    double compareRatioTo = 0.80;
    double capRatio = 1.0;

    if (0 != b->designCap)
    {
        capRatio =  ((double)b->maxCap + kSmartBattReserve_mAh) / (double)b->designCap;
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

    struct timeval t;
    time_t currentTime = 0;
    bool canCompareTime = true;
    
    
#if !TARGET_OS_EMBEDDED
    // retrieve current time
    if (gettimeofday(&t, NULL) == -1) {
        canCompareTime = false; // do not use 7-day observation period.
    }
    else {
        currentTime = t.tv_sec;
    }
#else
        canCompareTime = false; // Don't use 7-day observation for this case
#endif
    
    if (capRatio > 1.2) {
        // Poor|Perm Failure = max-capacity is more than 1.2x of the design-capacity.
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey), CFSTR(kIOPSPoorValue));
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthConditionKey),
                             CFSTR(kIOPSPermanentFailureValue));
        
        if (strncmp(batteryHealth, kIOPSPoorValue, sizeof(kIOPSPoorValue))) {
            logASLBatteryHealthChanged(kIOPSPoorValue,
                                       batteryHealth,
                                       kIOPSPermanentFailureValue);
            batteryHealth = kIOPSPoorValue;
        }
        if (b->hasLowCapRatio == true) {
            b->hasLowCapRatio = false;
            _setLowCapRatioTime(b->batterySerialNumber,
                                false,
                                0);
        }
    } else if (capRatio >= compareRatioTo) {
        b->markedDeclining = 0;
        // Good = CapRatio > 80% (plus or minus the 3% hysteresis mentioned above)
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey), CFSTR(kIOPSGoodValue));
        
        if ((batteryHealth[0] != 0) && (strncmp(batteryHealth, kIOPSGoodValue, sizeof(kIOPSGoodValue)))) {
            logASLBatteryHealthChanged(kIOPSGoodValue,
                                       batteryHealth,
                                       "");
        }
        batteryHealth = kIOPSGoodValue;
        if (b->hasLowCapRatio == true) {
            b->hasLowCapRatio = false;
            _setLowCapRatioTime(b->batterySerialNumber,
                                false,
                                0);
        }
    } else {
        if (b->hasLowCapRatio == false) {
            b->hasLowCapRatio = true;
            b->lowCapRatioSinceTime = currentTime;
            _setLowCapRatioTime(b->batterySerialNumber,
                                true,
                                currentTime);
        }
        // mark as declining to use hysteresis.
        b->markedDeclining = 1;
        
        // battery health status must be confirmed over a 7-day observation
        // period [7*86400]
        if (canCompareTime && (currentTime - b->lowCapRatioSinceTime <= 604800)) {
            // 7-day observation period is not complete, set the battery to Good
            CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey),
                                 CFSTR(kIOPSGoodValue));
            if (strncmp(batteryHealth, kIOPSGoodValue, sizeof(kIOPSGoodValue))) {
                logASLBatteryHealthChanged(kIOPSGoodValue,
                                           batteryHealth,
                                           "");
                batteryHealth = kIOPSGoodValue;
            }
        }
        else {
            // the 7-day observation period is complete, or the timestamps cannot
            // be compared now; set the kIOPSBatteryHealthKey to Fair/Poor/Check
            
            if (cyclesExceedStandard) {
                if (capRatio >= 0.50) {
                    // Fair = ExceedingCycles && CapRatio >= 50% && CapRatio < 80%
                    CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey),
                                         CFSTR(kIOPSFairValue));
                    if (strncmp(batteryHealth, kIOPSFairValue, sizeof(kIOPSFairValue))) {
                        logASLBatteryHealthChanged(kIOPSFairValue,
                                                   batteryHealth,
                                                   kIOPSCheckBatteryValue);
                        batteryHealth = kIOPSFairValue;
                    }
                } else {
                    // Poor = ExceedingCycles && CapRatio < 50%
                    CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey),
                                         CFSTR(kIOPSPoorValue));
                    if (strncmp(batteryHealth, kIOPSPoorValue, sizeof(kIOPSPoorValue))) {
                        logASLBatteryHealthChanged(kIOPSPoorValue,
                                                   batteryHealth,
                                                   kIOPSCheckBatteryValue);
                        batteryHealth = kIOPSPoorValue;
                    }
                }
                // HealthCondition == CheckBattery to distinguish the Fair & Poor
                // cases from from permanent failure (above), where
                // HealthCondition == PermanentFailure
                CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthConditionKey),
                                     CFSTR(kIOPSCheckBatteryValue));
            } else {
                // Check battery = NOT ExceedingCycles && CapRatio < 80%
                CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey),
                                     CFSTR(kIOPSCheckBatteryValue));
                if (strncmp(batteryHealth, kIOPSCheckBatteryValue,
                            sizeof(kIOPSCheckBatteryValue))) {
                    logASLBatteryHealthChanged(kIOPSCheckBatteryValue,
                                               batteryHealth,
                                               "");
                    batteryHealth = kIOPSCheckBatteryValue;
                }
            }
        }
    }
    return;
}

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
CFDictionaryRef packageKernelPowerSource(IOPMBattery *b)
{
    CFNumberRef     n, n0;
    CFMutableDictionaryRef  mDict = NULL;
    int             temp;
    int             minutes;
    int             set_capacity, set_charge;

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

    // Battery provided serial number
    if (b->batterySerialNumber) {
        CFDictionarySetValue(mDict, CFSTR(kIOPSHardwareSerialNumberKey), b->batterySerialNumber);
    }

    // Type = "InternalBattery", and "Transport Type" = "Internal"
    CFDictionarySetValue(mDict, CFSTR(kIOPSTransportTypeKey), CFSTR(kIOPSInternalType));
    CFDictionarySetValue(mDict, CFSTR(kIOPSTypeKey), CFSTR(kIOPSInternalBatteryType));

    // Set Power Source State to AC/Battery
    CFDictionarySetValue(mDict, CFSTR(kIOPSPowerSourceStateKey),
                            (b->externalConnected ? CFSTR(kIOPSACPowerValue):CFSTR(kIOPSBatteryPowerValue)));

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
    
    if (control.noPoll) {
        // 55% & 5:55 remaining means that battery polling is stopped for performance testing.
        set_charge = 55;
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

    // Set Amperage
    n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &b->avgAmperage);
    if(n) {
        CFDictionarySetValue(mDict, CFSTR(kIOPSCurrentKey), n);
        CFRelease(n);
    }

    // Set isPresent flag
    CFDictionarySetValue(mDict, CFSTR(kIOPSIsPresentKey),
                b->isPresent ? kCFBooleanTrue:kCFBooleanFalse);

    if (control.noPoll) {
        // 55% & 5:55 remaining means that battery polling is stopped for performance testing.
        minutes = 355;
    } else {
        minutes = b->swCalculatedTR;
    }

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

    // Set health & confidence
    _setBatteryHealthConfidence(mDict, b);


    // Set name
    if(b->name) {
        CFDictionarySetValue(mDict, CFSTR(kIOPSNameKey), b->name);
    } else {
        CFDictionarySetValue(mDict, CFSTR(kIOPSNameKey), CFSTR("Unnamed"));
    }

    return mDict;
}

// _readAndPublicACAdapter
static void _readAndPublishACAdapter(bool adapterExists, CFDictionaryRef batteryACDict)
{
    static CFDictionaryRef          oldACDict = NULL;
    CFStringRef                     key = NULL;
    Boolean                         success = FALSE;
    CFDictionaryRef          acDict = NULL;

    // Make sure we re-read the adapter on wake from sleep
    if (control.readACAdapterAgain) {
        control.readACAdapterAgain = false;

        if (oldACDict) {
            CFRelease(oldACDict);
            oldACDict = NULL;
        }

    }


    if (adapterExists) {
        if (!batteryACDict) {
            acDict = _copyACAdapterInfo(oldACDict);
            if (acDict == NULL) {
                goto exit;
            }

            batteryACDict = acDict;
        }
        if (isA_CFDictionary(oldACDict) && CFEqual(batteryACDict, oldACDict)) {
            goto exit;
        }

    }

    // Write dictionary into dynamic store
    key = SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault,
                            CFSTR("%@%@"),
                            kSCDynamicStoreDomainState,
                            CFSTR(kIOPSDynamicStorePowerAdapterKey));
    if (oldACDict) {
        CFRelease(oldACDict);
        oldACDict = NULL;
    }

    if (!key) {
        goto exit;
    }

    if (!adapterExists) {
        success = PMStoreRemoveValue(key);
    } else {
        success = PMStoreSetValue(key, batteryACDict);
        oldACDict = batteryACDict;
        CFRetain(oldACDict);
    }

    if (success) {
        notify_post("com.apple.system.powermanagement.poweradapter");
    }

exit:
    if (acDict)
        CFRelease(acDict);
    if (key)
        CFRelease(key);
    return ;
}

/**** User-space power source code lives below here ********************************/
/***********************************************************************************/
/***********************************************************************************/
/***********************************************************************************/



/***********************************************************************************/
static PSStruct *iops_newps(int pid, int psid)
{
    // Find the first empty slot in gPSList
    for (int i=0; i<kPSMaxCount; i++)
    {
        if (0 == gPSList[i].psid)
        {
            bzero(&gPSList[i], sizeof(PSStruct));
            gPSList[i].pid = pid;
            gPSList[i].psid = psid;
            return &gPSList[i];
        }
    }

    return NULL;
}

static PSStruct *iopsFromPSID(int _pid, int _psid)
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

__private_extern__ CFDictionaryRef getActiveUPSDictionary(void)
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
           && ( CFEqual(transport_type, CFSTR(kIOPSSerialTransportType))
               || CFEqual(transport_type, CFSTR(kIOPSUSBTransportType))
               || CFEqual(transport_type, CFSTR(kIOPSNetworkTransportType)) ))
        {
            return gPSList[i].description;
        }
    }
    return NULL;
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
                                                dispatch_get_main_queue());

    /* Setup automatic cleanup if client process dies
     */
    dispatch_source_set_cancel_handler(ps->procdeathsrc, ^{
        /*
         * When the client process dies, remove
         * this power source and stop showing it to IOPS API clients.
         *
         */

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

        dispatch_async(dispatch_get_main_queue(), ^()
                       { HandlePublishAllPowerSources(); });
    });

    dispatch_source_set_event_handler(ps->procdeathsrc, ^{
        dispatch_source_cancel(ps->procdeathsrc);
    });

    dispatch_resume(ps->procdeathsrc);


    *psid = gPSID++;
    *result = kIOReturnSuccess;

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
    CFDictionaryRef     details = NULL;
    int                 callerPID;
    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL,
                        &callerPID, NULL, NULL);

    syslog(0,0, ASL_LEVEL_ERR, "updating power source id = %d\n", psid);

    *return_code = kIOReturnError;

    details = IOCFUnserialize((const char *)details_ptr, NULL, 0, NULL);

    if (!isA_CFDictionary(details))
    {
        *return_code = kIOReturnBadArgument;
    } else {
        PSStruct *next = iopsFromPSID(callerPID, psid);
        if (!next) {
            *return_code = kIOReturnNotFound;
        } else {
            if (next->description) {
                CFRelease(next->description);
            }
            next->description = details;
            updateLogBuffer(next, false);
            *return_code = kIOReturnSuccess;
            dispatch_async(dispatch_get_main_queue(), ^()
                       { HandlePublishAllPowerSources(); });
        }
    }

    if (kIOReturnSuccess != *return_code) {
        CFRelease(details);
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

    syslog(0,0, ASL_LEVEL_ERR, "releasing power source id = %d\n", psid);

    PSStruct *toRelease = iopsFromPSID(callerPID, psid);
    if (toRelease) {
        dispatch_source_cancel(toRelease->procdeathsrc);
    }

    return 0;
}

kern_return_t _io_ps_copy_powersources_info(
    mach_port_t            server __unused,
    vm_offset_t             *ps_ptr,
    mach_msg_type_number_t  *ps_len,
    int                     *return_code)
{
    CFMutableArrayRef   return_value = NULL;

    for (int i=0; i<kPSMaxCount; i++) {
        if (gPSList[i].description) {
            if (!return_value) {
                return_value = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
            }
            CFArrayAppendValue(return_value,
                               (const void *)gPSList[i].description);
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
            *ps_len = CFDataGetLength(d);

            vm_allocate(mach_task_self(), (vm_address_t *)ps_ptr, *ps_len, TRUE);

            memcpy((void *)*ps_ptr, CFDataGetBytePtr(d), *ps_len);

            CFRelease(d);
        }
    }
    *return_code = kIOReturnSuccess;

    return 0;
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

    *updates = NULL; *updates_len = 0;
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



