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
 * HISTORY
 *
 * 29-Aug-02 ebold created
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

#include "powermanagementServer.h" // mig generated
#include "BatteryTimeRemaining.h"
#include "PMAssertions.h"
#include "PrivateLib.h"
#include "PMStore.h"

#ifndef kIOPSFailureKey
#define kIOPSFailureKey                         "Failure"
#endif

#define kBatteryPermFailureString               "Permanent Battery Failure"

#ifndef kIOPMBatteryPercentageFactors
#define kIOPMBatteryPercentageFactors           CFSTR("IOPMBatteryPercentageFactors")
#endif

#ifndef kIOPSDynamicStorePowerAdapterKey
#define kIOPSDynamicStorePowerAdapterKey        "/IOKit/PowerAdapter"
#endif

#ifndef kIOPSDynamicStoreLowBattPathKey
#define kIOPSDynamicStoreLowBattPathKey         "/IOKit/LowBatteryWarning"
#endif

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

extern CFMachPortRef            pmServerMachPort;
extern CFMutableSetRef          _publishedBatteryKeysSet;

// PMAssertions.c may modify gRespectRealPowerSources, to signal
// that the ignore-real-power-sources assertion is set.
int                             _showWhichBatteries = kBatteryShowReal;

/* OpaqueIOPSPowerSourceID
 *      == PSTracker (in this file)
 */
typedef struct  {
    mach_port_t     connection;
    CFStringRef     scdsKey;
} OpaqueIOPSPowerSourceID;

/* kPSMaxTrackedPowerSources
 * Allow for 1 battery, 1 UPS, 16 other
 */
#define kPSMaxTrackedPowerSources   20

static      OpaqueIOPSPowerSourceID      gPSList[kPSMaxTrackedPowerSources];
typedef     OpaqueIOPSPowerSourceID     *PSTracker;

// kBattNotCharging checks for (int16_t)-1 invalid current readings
#define kBattNotCharging        0xffff

#define kSlewStepMin            2
#define kSlewStepMax            10
#define kDiscontinuitySettle    60
static CFAbsoluteTime                  lastDiscontinuity;
typedef struct {
    int                 showingTime;
    bool                settled;
} SlewStruct;
SlewStruct *slew = NULL;

// Return values from calculateTRWithCurrent
enum {
    kNothingToSeeHere = 0,
    kNoTimeEstimate,
};

// Arguments For startBatteryPoll()
enum {
    kPeriodicPoll           = 0,
    kImmediateFullPoll      = 1
};

// Battery health calculation constants
#define kSmartBattReserve_mAh    200.0

#define kMaxBattMinutes     1200


// static global variables for tracking battery state
typedef struct {
    int              systemWarningLevel;
    bool             warningsShouldResetForSleep;
    bool             readACAdapterAgain;
    bool             selectionHasSwitched;
    int              psTimeRemainingNotifyToken;
    bool             noPoll;
} BatteryControl;

static BatteryControl   control;

// forward declarations
static void             _initializeBatteryCalculations(void);
static void             checkTimeRemainingValid(IOPMBattery **batts);
static void             _packageBatteryInfo(CFDictionaryRef *);
static void             _discontinuityOccurred(void);
static IOReturn         _readAndPublishACAdapter(bool, CFDictionaryRef);
static void             publish_IOPSBatteryGetWarningLevel(IOPMBattery *b, int combinedTime);
static void             publish_IOPSGetTimeRemainingEstimate(int timeRemaining, bool external, bool timeRemainingUnknown,
                                                              bool isCharging, bool noPoll);


static bool             startBatteryPoll();


__private_extern__ void
BatteryTimeRemaining_prime(void)
{
#if !TARGET_OS_EMBEDDED
#endif



    bzero(gPSList, sizeof(gPSList));
    bzero(&control, sizeof(BatteryControl));

    notify_register_check(kIOPSTimeRemainingNotificationKey, &control.psTimeRemainingNotifyToken);


#if !TARGET_OS_EMBEDDED
#endif
     // Initialize tracing battery events to FDR
     recordFDREvent(kFDRInit, false, NULL);

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
    lastDiscontinuity = CFAbsoluteTimeGetCurrent();
    
    // Kick off a battery poll now,
    // and schedule the next poll in exactly 60 seconds.
    startBatteryPoll(kImmediateFullPoll);
}

static void     _initializeBatteryCalculations(void)
{
    // Batteries detected, get their initial state
    if (_batteryCount() == 0) {
        return;
    }

    lastDiscontinuity = CFAbsoluteTimeGetCurrent();

    // make initial call to populate array and publish state
    BatteryTimeRemainingBatteriesHaveChanged(_batteries());

    return;
}

/* switchActiveBatterySet
 An argument of "true" indicates the system should respect fake, software controlled batteries only.
 An argument of "False" indicates the system should use only real, physical batteries.
 */
__private_extern__ void switchActiveBatterySet(int which)
{
    control.selectionHasSwitched = true;

    if (kBatteryShowReal == which) {
        _showWhichBatteries = kBatteryShowReal;
    } else if (kBatteryShowFake == which) {
        _showWhichBatteries = kBatteryShowFake;
    }

    BatteryTimeRemainingBatteriesHaveChanged(NULL);
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

#ifndef kBootPathKey
#define kBootPathKey             "BootPathUpdated"
#define kFullPathKey             "FullPathUpdated"
#define kUserVisPathKey          "UserVisiblePathUpdated"
#endif

static bool startBatteryPoll(int doCommand)
{
#if !TARGET_OS_EMBEDDED
    const static CFTimeInterval     kUserVisibleMinFrequency = 55.0;
    const static CFTimeInterval     kFullMinFrequency = 595.0;
    const static uint64_t           kPollIntervalNS = 60ULL * NSEC_PER_SEC;
    
    CFAbsoluteTime                  lastBootUpdate = 0.0;
    CFAbsoluteTime                  lastUserVisibleUpdate = 0.0;
    CFAbsoluteTime                  lastFullUpdate = 0.0;
    CFAbsoluteTime                  now = CFAbsoluteTimeGetCurrent();
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
        
        lastBootUpdate = getASBMPropertyCFAbsoluteTime(CFSTR(kBootPathKey));
        lastFullUpdate = getASBMPropertyCFAbsoluteTime(CFSTR(kFullPathKey));
        lastUserVisibleUpdate = getASBMPropertyCFAbsoluteTime(CFSTR(kUserVisPathKey));
        
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
            BatteryTimeRemainingBatteriesHaveChanged(NULL);
        }
    
        asl_log(0, 0, ASL_LEVEL_ERR, "Battery polling is now %s\n", noPoll ? "disabled." : "enabled. Initiating a battery poll.");
    }
}

static void switchBatteries(bool needsSwitched) {
    /* When our working battery set has changed (from real to fake or vice versa)
     * We will tear down all previously published battery state here.
     */
    if (!needsSwitched)
        return;
    
    CFStringRef     *publishedKeys = NULL;
    int             publishedKeysCount = 0;
    int             i;
    
    if (_publishedBatteryKeysSet)
    {
        publishedKeysCount = CFSetGetCount(_publishedBatteryKeysSet);
        if (0 < publishedKeysCount) {
            publishedKeys = (CFStringRef *)calloc(publishedKeysCount, sizeof(CFStringRef));
            CFSetGetValues(_publishedBatteryKeysSet, (const void **)publishedKeys);
            for (i=0; i<publishedKeysCount; i++) {
                PMStoreRemoveValue(publishedKeys[i]);
            }
            free(publishedKeys);
        }
    }
        
    control.selectionHasSwitched = false;
    return;
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
        }
        
        control.systemWarningLevel = newWarningLevel;
    }

    return;
}

static void publish_IOPSGetTimeRemainingEstimate(
    int timeRemaining,
    bool external,
    bool timeRemainingUnknown,
    bool isCharging,
    bool noPoll)
{
    uint64_t            powerSourcesBitsForNotify = (uint64_t)(timeRemaining & 0xFFFF);
    static uint64_t     lastPSBitsNotify = 0;
    
    // Presence of bit _kPSTimeRemainingNotifyValidBit means IOPSGetTimeRemainingEstimate
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
    
    if (lastPSBitsNotify != powerSourcesBitsForNotify)
    {
        lastPSBitsNotify = powerSourcesBitsForNotify;
        notify_set_state(control.psTimeRemainingNotifyToken, powerSourcesBitsForNotify);
        notify_post(kIOPSNotifyTimeRemaining);
    }
}

__private_extern__ void
BatteryTimeRemainingBatteriesHaveChanged(IOPMBattery **batteries)
{
    CFDictionaryRef             *result = NULL;
    int                         i;
    int                         batCount = _batteryCount();
    IOPMBattery                 *b = NULL;
    static int                 _lastExternalConnected = -1;
    int                         _nowExternalConnected = 0;
    bool                        needsNotifyAC = false;
    int                         combinedTime = 0;
    
    /*
     * Initiate the next battery poll; or start a timer to poll
     * when the 60sec user visible polling timer expres.
     */
    startBatteryPoll(kPeriodicPoll);
    
    switchBatteries(control.selectionHasSwitched);

    if (0 == batCount) {
        return;
    }
    
    if (!batteries) {
        batteries = _batteries();
    }
    b = batteries[0];
    
    _nowExternalConnected = (b->externalConnected ? 1 : 0);
    if (_lastExternalConnected != _nowExternalConnected) {
        // If AC has changed, we must invalidate time remaining.
        _discontinuityOccurred();
        needsNotifyAC = true;

        // Record AC change with FDR
        recordFDREvent(kFDRACChanged, false, batteries);

        _lastExternalConnected = _nowExternalConnected;
    }
    _readAndPublishACAdapter(b->externalConnected,
                             CFDictionaryGetValue(b->properties, CFSTR(kIOPMPSAdapterDetailsKey)));



    checkTimeRemainingValid(batteries);

    for(i=0; i<batCount; i++)
    {
        b = batteries[i];
        if (b->isPresent) {
            combinedTime += b->swCalculatedTR;
        }
    }
    
    publish_IOPSGetTimeRemainingEstimate(combinedTime,
                                         b->externalConnected,
                                         b->isTimeRemainingUnknown,
                                         b->isCharging,
                                         control.noPoll);
    
    publish_IOPSBatteryGetWarningLevel(b, combinedTime);

    /************************************************************************
     *
     * NOTIFY: Providing power source changed.
     *          via notify(3)
     ************************************************************************/
    if (needsNotifyAC) {
        notify_post(kIOPSNotifyPowerSource);
    }

    /************************************************************************
     *
     * PUBLISH: SCDynamicStoreSetValue / IOPSCopyPowerSourcesInfo()
     *
     ************************************************************************/
    result = (CFDictionaryRef *) calloc(1, batCount * sizeof(CFDictionaryRef));
    if (result)
    {
        _packageBatteryInfo(result);

        for (i=0; i<batCount; i++)
        {
            if (result[i])
            {
                PMStoreSetValue(batteries[i]->dynamicStoreKey, result[i]);
                CFRelease(result[i]);
            }
        }
        free(result);
    }

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

    if (capRatio > 1.2) {
        // Poor|Perm Failure = max-capacity is more than 1.2x of the design-capacity.
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey), CFSTR(kIOPSPoorValue));
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthConditionKey), CFSTR(kIOPSPermanentFailureValue));
    } else if (capRatio >= compareRatioTo) {
        b->markedDeclining = 0;
        // Good = CapRatio > 80% (plus or minus the 3% hysteresis mentioned above)
        CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey), CFSTR(kIOPSGoodValue));
    } else {
        b->markedDeclining = 1;
        if (cyclesExceedStandard) {
            if (capRatio >= 0.50)
            {
                // Fair = ExceedingCycles && CapRatio >= 50% && CapRatio < 80%
                CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey), CFSTR(kIOPSFairValue));
            } else {
                // Poor = ExceedingCycles && CapRatio < 50%
                CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey), CFSTR(kIOPSPoorValue));
            }
            // HealthCondition == CheckBattery to distinguish the Fair & Poor cases from from permanent
            // failure (above), where HealthCondition == PermanentFailure
            CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthConditionKey), CFSTR(kIOPSCheckBatteryValue));
        } else {
            // Check battery = NOT ExceedingCycles && CapRatio < 80%
            CFDictionarySetValue(outDict, CFSTR(kIOPSBatteryHealthKey), CFSTR(kIOPSCheckBatteryValue));
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
void _packageBatteryInfo(CFDictionaryRef *ret)
{
    CFNumberRef     n, n0;
    CFMutableDictionaryRef  mutDict = NULL;
    int             i;
    int             temp;
    int             minutes;
    int             set_capacity, set_charge;
    IOPMBattery     *b;
    IOPMBattery     **batts = _batteries();
    int             batCount = _batteryCount();

    // Stuff battery info into CFDictionaries
    for(i=0; i<batCount; i++)
    {
        b = batts[i];

        // Create the battery info dictionary
        mutDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if(!mutDict)
            return;

        // Does the battery provide its own time remaining estimate?
        CFDictionarySetValue(mutDict, CFSTR("Battery Provides Time Remaining"), kCFBooleanTrue);

        // Was there an error/failure? Set that.
        if (b->failureDetected) {
            CFDictionarySetValue(mutDict, CFSTR(kIOPSFailureKey), b->failureDetected);
        }

        // Is there a charging problem?
        if (b->chargeStatus) {
            CFDictionarySetValue(mutDict, CFSTR(kIOPMPSBatteryChargeStatusKey), b->chargeStatus);
        }

        // Battery provided serial number
        if (b->batterySerialNumber) {
            CFDictionarySetValue(mutDict, CFSTR(kIOPSHardwareSerialNumberKey), b->batterySerialNumber);
        }

        // Type = "InternalBattery", and "Transport Type" = "Internal"
        CFDictionarySetValue(mutDict, CFSTR(kIOPSTransportTypeKey), CFSTR(kIOPSInternalType));
        CFDictionarySetValue(mutDict, CFSTR(kIOPSTypeKey), CFSTR(kIOPSInternalBatteryType));

        // Set Power Source State to AC/Battery
        CFDictionarySetValue(mutDict, CFSTR(kIOPSPowerSourceStateKey),
                                (b->externalConnected ? CFSTR(kIOPSACPowerValue):CFSTR(kIOPSBatteryPowerValue)));

        // round charge and capacity down to a % scale
        if(0 != b->maxCap)
        {
            set_capacity = 100;
            set_charge = (int)lround((double)b->currentCap*100.0/(double)b->maxCap);

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
            CFDictionarySetValue(mutDict, CFSTR(kIOPSMaxCapacityKey), n);
            CFRelease(n);
        }

        // Set current charge
        n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &set_charge);
        if(n) {
            CFDictionarySetValue(mutDict, CFSTR(kIOPSCurrentCapacityKey), n);
            CFRelease(n);
        }

        // Set isPresent flag
        CFDictionarySetValue(mutDict, CFSTR(kIOPSIsPresentKey),
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
            CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanFalse);
            CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
            CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n0);
        } else {
            // There IS a battery installed.
            if(b->isCharging) {
                // Set _isCharging to True
                CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanTrue);
                // Set IsFinishingCharge
                CFDictionarySetValue(mutDict, CFSTR(kIOPSIsFinishingChargeKey),
                        (b->maxCap && (99 <= (100*b->currentCap/b->maxCap))) ? kCFBooleanTrue:kCFBooleanFalse);
                n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &minutes);
                if(n) {
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n);
                    CFRelease(n);
                }
                CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n0);
            } else {
                // Not Charging
                // Set _isCharging to False
                CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanFalse);
                // But are we plugged in?
                if(b->externalConnected)
                {
                    // plugged in but not charging == fully charged
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n0);

                    CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargedKey),
                        isFullyCharged(b) ? kCFBooleanTrue:kCFBooleanFalse);
                } else {
                    // not charging, not plugged in == d_isCharging
                    n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &minutes);
                    if(n) {
                        CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n);
                        CFRelease(n);
                    }
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
                }
            }

        }
        CFRelease(n0);

        // Set health & confidence
        _setBatteryHealthConfidence(mutDict, b);


        // Set name
        if(b->name) {
            CFDictionarySetValue(mutDict, CFSTR(kIOPSNameKey), b->name);
        } else {
            CFDictionarySetValue(mutDict, CFSTR(kIOPSNameKey), CFSTR("Unnamed"));
        }
        ret[i] = mutDict;
    }

    return;
}

// _readAndPublicACAdapter
// These keys describe the bit-layout of the 64-bit AC info structure.

/* Legacy format */

#define kACCRCBit               56  // size 8
#define kACIDBit                44  // size 12
#define kACPowerBit             36  // size 8
#define kACRevisionBit          32  // size 4
#define kACSerialBit            8   // size 24
#define kACFamilyBit            0   // size 8

/* New format (intro'd in Jan 2012) */

//#define kACCRCBit             56   // 8 bits, same as in legacy
#define kACCurrentIdBit         48   // 8 bits
//#define kACCommEnableBit      45   // 1 bit; doesn't contain meaningful information
#define kACSourceIdBit          44   // 3 bits
//#define kACPowerBit           36   // 8 bits, same as in legacy
#define kACVoltageIDBit         33   // 3 bits
//#define kACSerialBit          8    // 25 bits
//#define kACFamilyBit          0    // 8 bits, same as in legacy

#define k3BitMask       0x7


typedef struct {
    uint32_t        valCommEn;
    uint32_t        valVoltageID;
    uint32_t        valID;
    uint32_t        valPower;
    uint32_t        valRevision;
    uint32_t        valSerial;
    uint32_t        valFamily;
    uint32_t        valCurrent;
    uint32_t        valSource;
} AdapterAttributes;

static void stuffInt32(CFMutableDictionaryRef d, CFStringRef k, uint32_t n)
{
    CFNumberRef stuffNum = NULL;
    if ((stuffNum = CFNumberCreate(0, kCFNumberSInt32Type, &n)))
    {
        CFDictionarySetValue(d, k, stuffNum);
        CFRelease(stuffNum);
    }
}

static IOReturn _readAndPublishACAdapter(bool adapterExists, CFDictionaryRef batteryACDict)
{
    static bool                     adapterInfoPublished = false;
    static CFDictionaryRef          oldACDict = NULL;
    CFStringRef                     key = NULL;
    CFMutableDictionaryRef          acDict = NULL;
    IOReturn                        ret = kIOReturnSuccess;
    Boolean                         success = FALSE;
#if !TARGET_OS_EMBEDDED
    int                             j = 0;
#endif
    AdapterAttributes               info;

    bzero(&info, sizeof(info));

    // Make sure we re-read the adapter on wake from sleep
    if (control.readACAdapterAgain) {
        adapterInfoPublished = false;
        control.readACAdapterAgain = false;
    }

    // Always republish AC info if it comes from the battery
    if (adapterExists && batteryACDict && oldACDict && !CFEqual(oldACDict, batteryACDict))
    {
        adapterInfoPublished = false;
    }

    // don't re-publish AC info until the adapter changes
    if (adapterExists && adapterInfoPublished)
    {
        return kIOReturnSuccess;
    }

    if (adapterExists && !batteryACDict)
    {
        uint64_t acBits;

        ret = _getACAdapterInfo(&acBits);
        if (kIOReturnSuccess != ret) {
            return ret;
        }

        // Decode SMC key
        info.valID              = (acBits >> kACIDBit) & 0xFFF;
        info.valFamily          = (acBits >> kACFamilyBit) & 0xFF;
        info.valPower           = (acBits >> kACPowerBit) & 0xFF;
        if ( (info.valSource    = (acBits >> kACSourceIdBit) & k3BitMask))
        {
            // New format
            info.valSerial      = (acBits >> kACSerialBit) & 0x1FFFFFF;
            info.valCurrent     = ((acBits >> kACCurrentIdBit) & 0xFF) * 25;
            info.valVoltageID   = (acBits >> kACVoltageIDBit) & k3BitMask;
        } else {
            // Legacy format
            info.valSerial      = (acBits >> kACSerialBit) & 0xFFFFFF;
            info.valRevision    = (acBits >> kACRevisionBit) & 0xF;
        }

        // Publish values in dictionary
        acDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        if (!acDict) {
            ret = kIOReturnNoMemory;
            goto exit;
        }

        if (info.valSource) {
            // New format
            stuffInt32(acDict, CFSTR(kIOPSPowerAdapterCurrentKey), info.valCurrent);
            stuffInt32(acDict, CFSTR(kIOPSPowerAdapterSourceKey), info.valSource);

        }
        else {
            // Legacy format
            stuffInt32(acDict, CFSTR(kIOPSPowerAdapterRevisionKey), info.valRevision);
        }

        if (0 != info.valPower) {
            stuffInt32(acDict, CFSTR(kIOPSPowerAdapterWattsKey), info.valPower);
        }

        stuffInt32(acDict, CFSTR(kIOPSPowerAdapterIDKey), info.valID);
        stuffInt32(acDict, CFSTR(kIOPSPowerAdapterSerialNumberKey), info.valSerial);
        stuffInt32(acDict, CFSTR(kIOPSPowerAdapterFamilyKey), info.valFamily);

        batteryACDict = acDict;
    }

    // Write dictionary into dynamic store
    key = SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault,
                            CFSTR("%@%@"),
                            kSCDynamicStoreDomainState,
                            CFSTR(kIOPSDynamicStorePowerAdapterKey));
    if (!key) {
        ret = kIOReturnError;
        goto exit;
    }

    if (oldACDict) {
        CFRelease(oldACDict);
        oldACDict = NULL;
    }

    if (!adapterExists) {
        success = PMStoreRemoveValue(key);
        adapterInfoPublished = false;
    } else {
        success = PMStoreSetValue(key, batteryACDict);
        adapterInfoPublished = true;
        oldACDict = batteryACDict;
        CFRetain(oldACDict);
    }

    if (success)
        ret = kIOReturnSuccess;
    else
        ret = kIOReturnLockedRead;

    if (success)
    notify_post("com.apple.system.powermanagement.poweradapter");

exit:
    if (acDict)
        CFRelease(acDict);
    if (key)
        CFRelease(key);
    return ret;
}

/**** User-space power source code lives below here ********************************/
/***********************************************************************************/
/***********************************************************************************/
/***********************************************************************************/


/***********************************************************************************/
/* copyNewKeyForType
 * Assigns a unique string as the key for the power source type.
 * The name should reflect the power source's type, and should have a unique
 * integer appended to unique it within the system.
 */
static CFStringRef _copyNewKeyForType(char *type)
{
    CFStringRef         scKey = NULL;
    CFStringRef         typeString = NULL;
    static const unsigned long long kCounterWrapAround = 100000;
    static unsigned long long psCounter = 1000;

    typeString = CFStringCreateWithCString(0, type, kCFStringEncodingMacRoman);
    if (!typeString)
        return NULL;

    /* Note: There is a small chance of issuing a name that is already in use on the system.
     * This is only if psCounter exceeds 100,000; which would imply that around 99,000 power sources
     * were created and destroyed. That's unlikely.
     */

    if (psCounter > kCounterWrapAround)
        psCounter = 1000;

    scKey = SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault,
                            CFSTR("%@%@/%@-%ld"),
                            kSCDynamicStoreDomainState,
                            CFSTR(kIOPSDynamicStorePath),
                            typeString,
                            psCounter++);

    CFRelease(typeString);
    return scKey;
}

/***********************************************************************************/
static IOReturn _new_psTracker(PSTracker *new_ps)
{
    int             i = 0;

    *new_ps = NULL;

    for (i=0; i<kPSMaxTrackedPowerSources; i++)
    {
        if (MACH_PORT_NULL == gPSList[i].connection)
            break;
    }

    if (i >= kPSMaxTrackedPowerSources) {
        return kIOReturnNoMemory;
    }

    *new_ps = &gPSList[i];

    return kIOReturnSuccess;
}

static PSTracker _psTrackerForPort(mach_port_t target_port)
{
    int i;

    for (i=0; i<kPSMaxTrackedPowerSources; i++)
    {
        if (target_port == gPSList[i].connection)
            break;
    }

    if (i >= kPSMaxTrackedPowerSources) {
        return NULL;
    }

    return &gPSList[i];
}



/***********************************************************************************/
/*** Destroy an existing power source ***/
/***********************************************************************************/

__private_extern__ bool BatteryHandleDeadName(mach_port_t deadName)
{
    PSTracker                   reap_me = _psTrackerForPort(deadName);

    if (!reap_me) {
        // Nothing to be done. This is not a battery-tracked port.
        // Return false to indicate we didn't handle it.
        return false;
    }

    if (reap_me->scdsKey)
    {
        PMStoreRemoveValue(reap_me->scdsKey);
        CFRelease(reap_me->scdsKey);
    }

    if (reap_me->connection != MACH_PORT_NULL)
    {
        __MACH_PORT_DEBUG(true, "_io_pm_new_pspowersource deadname", reap_me->connection);
        mach_port_deallocate(mach_task_self(), reap_me->connection);
    }

    reap_me->scdsKey = NULL;
    reap_me->connection = MACH_PORT_NULL;

    return true;
}

/***********************************************************************************/
// MIG handler - back end for IOKit API IOPSCreatePowerSource
kern_return_t _io_pm_new_pspowersource(
    mach_port_t                 server,
    mach_port_t                 clientport,
    string_t                    clienttype,         // in
    string_t                    dskey,              // out
    int                         *result)
{
    PSTracker                   new_tracker = NULL;
    static const int            kDSKeyMIGBufferSize = 1024;
    mach_port_t                 oldNotify;

    if (MACH_PORT_NULL == clientport
        || NULL == clienttype
        || NULL == result
        || NULL == dskey)
    {
        if (result) *result = kIOReturnBadArgument;
        goto exit;
    }

    if (kIOReturnSuccess != _new_psTracker(&new_tracker))
    {
        *result = kIOReturnNoSpace;
        goto exit;
    }

    __MACH_PORT_DEBUG(true, "_io_pm_new_pspowersource client", clientport);
    new_tracker->connection = clientport;

    mach_port_request_notification(
                mach_task_self(),           // task
                clientport,                 // port that will die
                MACH_NOTIFY_DEAD_NAME,      // msgid
                1,                          // make-send count
                CFMachPortGetPort(pmServerMachPort),        // notify port
                MACH_MSG_TYPE_MAKE_SEND_ONCE,               // notifyPoly
                &oldNotify);                                // previous

    new_tracker->scdsKey = _copyNewKeyForType(clienttype);

    if (new_tracker->scdsKey)
    {
        // We copy the string directly into the reply mach mesage at the address provided at 'dskey'
        CFStringGetCString(new_tracker->scdsKey, (void *)dskey,
                                kDSKeyMIGBufferSize, kCFStringEncodingUTF8);
    }

    *result = kIOReturnSuccess;

exit:
    __MACH_PORT_DEBUG(true, "_io_pm_new_pspowersource client - exit", clientport);
    return KERN_SUCCESS;
}

/***********************************************************************************/
// MIG handler - back end for IOKit API IOPSSetPowerSourceDetails

kern_return_t _io_pm_update_pspowersource(
    mach_port_t         server __unused,
    string_t            dskey,
    vm_offset_t         details_ptr,
    mach_msg_type_number_t  details_len,
    int                 *return_code)
{
    CFStringRef         dskeyCFSTR = NULL;
    CFDictionaryRef     details = NULL;

    dskeyCFSTR = CFStringCreateWithCString(0, dskey, kCFStringEncodingUTF8);
    if (!dskeyCFSTR)
        goto exit;

    details = IOCFUnserialize((const char *)details_ptr, NULL, 0, NULL);
    if (!details)
        goto exit;

    if (isA_CFDictionary(details)) {
        PMStoreSetValue(dskeyCFSTR, details);
        *return_code = kIOReturnSuccess;
    }
exit:
    if (dskeyCFSTR)
        CFRelease(dskeyCFSTR);
    if (details)
        CFRelease(details);
    return 0;
}

