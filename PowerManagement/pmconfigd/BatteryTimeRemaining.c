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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved. 
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
#define kIOPSFailureKey "Failure"
#endif

#ifndef kIOPSDynamicStorePowerAdapterKey
#define kIOPSDynamicStorePowerAdapterKey "/IOKit/PowerAdapter"
#endif

#define kBatteryPermFailureString "Permanent Battery Failure"

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

#ifndef kIOPSDynamicStoreLowBattPathKey
#define kIOPSDynamicStoreLowBattPathKey "/IOKit/LowBatteryWarning"
#endif

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


// Return values from calculateTRWithCurrent
enum {
    kNothingToSeeHere = 0,
    kNoTimeEstimate,
};

// Battery health calculation constants
#define kSmartBattReserve_mAh    200.0

#define kMaxBattMinutes     1200

// static global variables for tracking battery state
static CFAbsoluteTime   _estimatesInvalidUntil = 0.0;
static int              _systemBatteryWarningLevel = 0;
static bool             _warningsShouldResetForSleep = false;
static bool             _readACAdapterAgain = false;
static bool             _ignoringTimeRemainingEstimates = false;
static CFRunLoopTimerRef    _timeSettledTimer = NULL;
static bool             _batterySelectionHasSwitched = false;
static int              _psTimeRemainingNotifyToken = 0;

// forward declarations
static void             _initializeBatteryCalculations(void);
static int              _populateTimeRemaining(IOPMBattery **batts);
static void             _packageBatteryInfo(CFDictionaryRef *);
static void             _timeRemainingMaybeValid(CFRunLoopTimerRef timer, void *info);
static void             _discontinuityOccurred(void);
static IOReturn         _readAndPublishACAdapter(bool, CFDictionaryRef);

__private_extern__ void
BatteryTimeRemaining_prime(void)
{
    bzero(gPSList, sizeof(gPSList));
    
    // setup battery calculation global variables
    _initializeBatteryCalculations();
    
    notify_register_check(kIOPSTimeRemainingNotificationKey, &_psTimeRemainingNotifyToken);
    
    return;
}

__private_extern__ void
BatteryTimeRemainingSleepWakeNotification(natural_t messageType)
{
    if (kIOMessageSystemWillPowerOn == messageType)
    {
        _warningsShouldResetForSleep = true;
        _readACAdapterAgain = true;

        BatteryTimeRemainingBatteriesHaveChanged(NULL);
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
    IOPMBattery             **b = _batteries();

    // Pick a time X seconds into the future. Until then, all TimeRemaining
    // estimates shall be considered invalid.
    // We will check the current timestamp against:
    // _lastWake + b[i]->invalidWakeSecs
    // and ignore time remaining until that moment has past.
    
    if (!b || !b[0])
        return;
    
    _estimatesInvalidUntil = CFAbsoluteTimeGetCurrent() + (double)b[0]->invalidWakeSecs;
    
    _ignoringTimeRemainingEstimates = true;
    
    // After the timeout has elapsed, re-read battery state & the now-valid
    // time remaining.
    if (_timeSettledTimer) {
        CFRunLoopTimerInvalidate(_timeSettledTimer);
        _timeSettledTimer = NULL;
    }
    _timeSettledTimer = CFRunLoopTimerCreate(
                    NULL, _estimatesInvalidUntil, 0.0, 
                    0, 0, _timeRemainingMaybeValid, NULL);
    CFRunLoopAddTimer( CFRunLoopGetCurrent(), _timeSettledTimer, 
                    kCFRunLoopDefaultMode);
    CFRelease(_timeSettledTimer);
    
    // TODO: re-publish battery state
}

static void _timeRemainingMaybeValid(CFRunLoopTimerRef timer, void *info)
{
    // The timer has fired. Settings are probably valid.
    _timeSettledTimer = NULL;
    _ignoringTimeRemainingEstimates = false;

    // Trigger battery time remaining re-calculation 
    // now that current reading is valid.
    BatteryTimeRemainingBatteriesHaveChanged(NULL);
}

static void     _initializeBatteryCalculations(void)
{
    // Batteries detected, get their initial state
    if (_batteryCount() == 0) {
        return;
    }

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
    _batterySelectionHasSwitched = true;

    if (kBatteryShowReal == which) {
        _showWhichBatteries = kBatteryShowReal;
    } else if (kBatteryShowFake == which) {
        _showWhichBatteries = kBatteryShowFake;
    }

    BatteryTimeRemainingBatteriesHaveChanged(NULL);
}


__private_extern__ void
BatteryTimeRemainingBatteriesHaveChanged(IOPMBattery **batteries)
{
    static CFStringRef          lowBatteryKey = NULL;
    static CFDictionaryRef      *old_battery = NULL;
    CFDictionaryRef             *result = NULL;
    int                         i;
    int                         batCount = _batteryCount();
    IOPMBattery                 *b = NULL;
    bool                        external = false;
    static bool                 _lastExternal = false;
    
    
    /* When our working battery set has changed (from real to fake or vice versa)
     * We will tear down all previously published battery state here.
     */
    if (_batterySelectionHasSwitched) 
    {
        CFStringRef     *publishedKeys = NULL;
        int             publishedKeysCount = 0;
        int             i;
        
        if (_publishedBatteryKeysSet)
        {
            publishedKeysCount = CFSetGetCount(_publishedBatteryKeysSet);
            if (0 < publishedKeysCount) {
                publishedKeys = calloc(publishedKeysCount, sizeof(void *));
                CFSetGetValues(_publishedBatteryKeysSet, (const void **)publishedKeys);
                for (i=0; i<publishedKeysCount; i++) {
                    PMStoreRemoveValue(publishedKeys[i]);
                }
                free(publishedKeys);
            }
        }
                
        if (old_battery) {
            for (i=0; i<batCount; i++) {
                CFRelease(old_battery[i]);
            }
            free(old_battery);
            old_battery = NULL;
        }

        _batterySelectionHasSwitched = false;
    }

    if (0 == _batteryCount()) {
        return;
    }
    
    if (!batteries) {
        batteries = _batteries();
    }

    result = (CFDictionaryRef *) calloc(1, batCount * sizeof(CFDictionaryRef));
    
    if ( NULL == old_battery ) {
        old_battery = (CFDictionaryRef *) calloc(1, batCount * sizeof(CFDictionaryRef));
    }
    
    if ( NULL == old_battery 
      || NULL == result ) {
        return;
    }
    
    
    
    /* First, we have to determine if AC has changed since our last reading,
     * since this effects our time remaining estimate.
     */
    b = batteries[0];
    if (b->externalConnected) {
        external = true;
    }
    if (_lastExternal != external) {
        // If AC has changed, we must invalidate time remaining.
        _discontinuityOccurred();
    }
    _lastExternal = external;

    /*
     * AC attached or detached
     * Code on new AC attach goes here.
     */
    _readAndPublishACAdapter(external, CFDictionaryGetValue(b->properties, CFSTR(kIOPMPSAdapterDetailsKey)));
    
    /*
     * Estimate N minutes until battery empty/full
     */
    b->isTimeRemainingUnknown = !_populateTimeRemaining(batteries);


    /* Display a system low battery warning?
     * 
     * No Warning == AC Power or >= 22% on battery
     * Early Warning == On Battery with < 22%
     * Final Warning == On Battery with < 10 Minutes
     *
     * Once we enter a "warning" state, we can only leave "warning"
     * state by (1) having AC power re-applied, or (2) hibernating
     * and waking with a new battery.
     *     
     * This prevents fluctuations in battery capacity from causing
     * multiple battery warnings.
     *
     */
    int             combinedTime    = 0;
    int             newWarningLevel = 0;
    int             combinedLevel   = 0;

    for(i=0; i<batCount; i++) 
    {
        b = batteries[i];
        if (b->isPresent) {
            if (0 != b->maxCap) {
                combinedLevel += (100 * b->currentCap)/b->maxCap;
            }
            combinedTime += b->swCalculatedTR;
        }
    }
    
    /*
     * Battery Inserted - new battery detected code here here
     */ 
    
    if (_warningsShouldResetForSleep)
    {
        _warningsShouldResetForSleep = false;
        _systemBatteryWarningLevel = 0;
        newWarningLevel = kIOPSLowBatteryWarningNone;
        
    } else if (b->externalConnected || !b->isPresent)
    {
        // If we have AC power, then the warnings come down.
        newWarningLevel = kIOPSLowBatteryWarningNone;

    } else if ((combinedLevel > 0) && (combinedTime > 0))
    {
        // It's invalid to go from showing any warning
        // to then showing no warning, without application of AC power,
        // sleeping and waking, or switching to a new battery.
        if ( (combinedLevel >= 22) 
            && (_systemBatteryWarningLevel != kIOPSLowBatteryWarningEarly)
            && (_systemBatteryWarningLevel != kIOPSLowBatteryWarningFinal))
        {
            newWarningLevel = kIOPSLowBatteryWarningNone;
        } else if (combinedTime < 10) {
            newWarningLevel = kIOPSLowBatteryWarningFinal;
        } else if (_systemBatteryWarningLevel != kIOPSLowBatteryWarningFinal)
        {
            // Early warning level if combinedLevel < 22 && combinedTime >= 10
            // Also we disallow the warning level to popup from Final
            // into early. The only ways out of Final are to (1) attach AC
            // or (2) wake from sleep/hibernation with a battery.
            newWarningLevel = kIOPSLowBatteryWarningEarly;
        }
    }

    // At this point our algorithm above has populated the time remaining estimate
    // We'll package that info into user-consumable dictionaries below.

    _packageBatteryInfo(result);

    /************************************************************************
     *
     * PUBLISH: IOPSBatteryGetWarningLevel
     *
     ************************************************************************/

    // And publish the new warning level.        
    if ( (newWarningLevel != _systemBatteryWarningLevel)
        && (0 != newWarningLevel) ) 
    {
        CFNumberRef newWarningLevelNumber = 
            CFNumberCreate(0, kCFNumberIntType, &newWarningLevel);
        
        if (newWarningLevelNumber) 
        {
            if (!lowBatteryKey) {
                lowBatteryKey = SCDynamicStoreKeyCreate(
                                    kCFAllocatorDefault, CFSTR("%@%@"),
                                    kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStoreLowBattPathKey));
            }
            
            PMStoreSetValue(lowBatteryKey, newWarningLevelNumber );
            CFRelease(newWarningLevelNumber); 
            
            notify_post( kIOPSNotifyLowBattery );
        }
        _systemBatteryWarningLevel = newWarningLevel;
    }

    /************************************************************************
     *
     * PUBLISH: IOPSGetTimeRemainingEstimate
     *          via notify(3) 
     *
     ************************************************************************/
    
    #define     _kPSTimeRemainingNotifyExternalBit       (1 << 16)
    #define     _kPSTimeRemainingNotifyChargingBit       (1 << 17)
    #define     _kPSTimeRemainingNotifyUnknownBit        (1 << 18)
    #define     _kPSTimeRemainingNotifyValidBit          (1 << 19)
    
    uint64_t            powerSourcesBitsForNotify = (uint64_t)(combinedTime & 0xFFFF);
    static uint64_t     lastPSBitsNotify = 0;

    // Presence of bit _kPSTimeRemainingNotifyValidBit means IOPSGetTimeRemainingEstimate
    // should trust this as a valid chunk of battery data.
    powerSourcesBitsForNotify |= _kPSTimeRemainingNotifyValidBit;

    if (external) {
        powerSourcesBitsForNotify |= _kPSTimeRemainingNotifyExternalBit;
    }
    if (batteries[0]->isTimeRemainingUnknown) {
        powerSourcesBitsForNotify |= _kPSTimeRemainingNotifyUnknownBit;
    }
    if (batteries[0]->isCharging) {
        powerSourcesBitsForNotify |= _kPSTimeRemainingNotifyChargingBit;
    }

    if (lastPSBitsNotify != powerSourcesBitsForNotify)
    {
        lastPSBitsNotify = powerSourcesBitsForNotify;
        notify_set_state(_psTimeRemainingNotifyToken, powerSourcesBitsForNotify);
        notify_post(kIOPSTimeRemainingNotificationKey);
    }

    /************************************************************************
     *
     * PUBLISH: SCDynamicStoreSetValue
     *
     ************************************************************************/
    for(i=0; i<batCount; i++)
    {
        if(result[i]) 
        {   
            // Determine if CFDictionary is new or has changed...
            // Only do SCDynamicStoreSetValue if the dictionary is different
            if( !old_battery[i] || !CFEqual(old_battery[i], result[i]))
            {
                PMStoreSetValue(batteries[i]->dynamicStoreKey, result[i]);
            }
            
            if (old_battery[i]) {
                CFRelease(old_battery[i]);
            }

            old_battery[i] = result[i];
        }
    }
    
    if(result) {
        free(result);
    }
}


/* _populateTimeRemaining
 * Implicit inputs: battery state; battery's own time remaining estimate
 * Implicit output: estimated time remaining placed in b->swCalculatedTR; or -1 if indeterminate
 *   returns 1 if we reached a valid estimate
 *   returns 0 if we're still calculating
 */
static int _populateTimeRemaining(IOPMBattery **batts)
{
    int             i;
    IOPMBattery     *b;
    int             batCount = _batteryCount();
    
    double          lowerAmperageBound;
    double          upperAmperageBound;
    double          absValAvgCurrent;
    double          absValInstantCurrent;

    
    for(i=0; i<batCount; i++)
    {
        b = batts[i];

        absValAvgCurrent = abs(b->avgAmperage);
        absValInstantCurrent = abs(b->instantAmperage);
        
        // If the battery's instantaneous amperage differs wildly from the battery's
        // average amperage over the past minute, we will not use it.
        
        if (_batteryHas(b, CFSTR("InstantAmperage")))
        {
            lowerAmperageBound = (double) absValInstantCurrent * 0.5;
            upperAmperageBound = (double) absValInstantCurrent * 2.0;
        } else {
            // If instant amperage isn't available to rea from this battery we'll just use
            // some loose bounds for this comparison to prevent divide-by-zero in our 
            // calculations below.
            lowerAmperageBound = 5;
            upperAmperageBound = 15000;
        }
    
    
        // The following conditions invalidate a time remaining estimate.
        // (1) If current is zero, finding a time remaining estimate is irrelevant
        //       (in the case of being fully charged) or impossible (in the case
        //       of having just plugged into AC).
        // (2) We also check that average current is within a reasonable range 
        //       of the instant current. We want to avoid 500 hour time remainings 
        //       on wake from sleep; so we make sure the average amperage readings 
        //       are sane.
        // (3) For X seconds after wake from sleep, we cannot trust the time 
        //       remaining estimate provided, whether we provide it ourselves
        //       in SW, or we receive it from the battery.
        //       The battery's kext may specify this time with the 
        //       kIOPMPSInvalidWakeSecondsKey key.
        if( (0 == b->avgAmperage) 
            || (absValAvgCurrent < lowerAmperageBound) 
            || (absValAvgCurrent > upperAmperageBound) 
            || _ignoringTimeRemainingEstimates)
        {
            b->swCalculatedTR = -1;
            continue;
        } 
                        
#if 0
/* Battery time remaining estimate is provided directly by the battery.
 */
        b->swCalculatedTR = b->hwAverageTR;
#endif

        /* Manually calculate battery time remaining.
         */

        if (0 == b->avgAmperage) {
            b->swCalculatedTR = -1;
        } else {
            if(b->isCharging) {
                // h = -mAh/mA
                b->swCalculatedTR = 60*((double)(b->maxCap - b->currentCap)
                                    / (double)b->avgAmperage);                                
            } else { // discharging
                // h = mAh/mA
                b->swCalculatedTR = -60*((double)b->currentCap
                                    / (double)b->avgAmperage);
            }
        }

        // Did our calculation come out negative? 
        // The average current must still be out of whack!
        if (b->swCalculatedTR < 0) {
            b->swCalculatedTR = -1;
        }

        // Cap all times remaining to 10 hours. We don't ship any
        // 44 hour batteries just yet.
        if (kMaxBattMinutes < b->swCalculatedTR) {
            b->swCalculatedTR = kMaxBattMinutes;
        }
    }
    
    return (-1 != batts[0]->swCalculatedTR);
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

    if (capRatio > 1.5) {
        // Poor|Perm Failure = max-capacity is more than 1.5x of the design-capacity.
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
    bool            is_charged;
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

        // Are we in a time remaining black-out period due to a recent discontinuity?
        if (_ignoringTimeRemainingEstimates) {
            CFDictionarySetValue(mutDict, CFSTR("Waiting For Time Remaining Estimates"), kCFBooleanTrue);
        }
        
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
        
        // Set _isCharging and time remaining
        minutes = b->swCalculatedTR;
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
                    
                    // Set IsCharged if capacity >= 95% and not charging and plugged in.
                    // - Some portables will not initiate a battery charge if AC is 
                    //   connected when copacity is >= 95%. 
                    // - We consider > 95% to be fully charged; the battery will not charge 
                    //   any higher until AC is unplugged and re-attached.
                    // - IsCharged should be true when the external power adapter LED is Green; 
                    //   should be false when the external power adapter LED is Orange.
                    if (0 != b->maxCap) {
                        is_charged = ((100*b->currentCap/b->maxCap) >= 95);
                    } else { 
                        is_charged = false;
                    }
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargedKey), 
                        is_charged ? kCFBooleanTrue:kCFBooleanFalse);
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

#define kACCRCBit       56   // size 8
#define kACIDBit        44  // size 12
#define kACPowerBit     36  // size 8
#define kACRevisionBit  32  // size 4
#define kACSerialBit    8   // size 24
#define kACFamilyBit    0   // size 8

/* New format (intro'd in Jan 2012) */

//#define kACCRCBit     56   // 8 bits, same as in legacy
#define kACurrentIdBit  48   // 8 bits
#define kASourceIdBit   44   // 4 bits
//#define kAPowerBit    36   // 8 bits, same as in legacy
#define kACommEnableBit 35   // 1 bit
#define kASerialBit     8    // 25 bits
//#define kACFamilyBit  0    // 8 bits, same as in legacy

static IOReturn _readAndPublishACAdapter(bool adapterExists, CFDictionaryRef batteryACDict)
{
    CFStringRef                 key = NULL;
    CFMutableDictionaryRef      acDict = NULL;
    CFNumberRef     stuffNum = NULL;
//    uint32_t        valCRC = 0;
    uint32_t        valID = 0;
    uint32_t        valPower = 0;
    uint32_t        valRevision = 0;
    uint32_t        valSerial = 0;
    uint32_t        valFamily = 0;
    uint32_t        valCurrent = 0;
    uint32_t        valSource = 0;
    uint64_t        acBits = 0;
    IOReturn        ret = kIOReturnSuccess;
    Boolean         success = FALSE;
    static bool     adapterInfoPublished = false;
    static CFDictionaryRef  oldACDict = NULL;

    // Make sure we re-read the adapter on wake from sleep
    if (_readACAdapterAgain) {
        adapterInfoPublished = false;
        _readACAdapterAgain = false;
    }

    // Always republish AC info if it comes from the battery
    if (adapterExists && batteryACDict && oldACDict && !CFEqual(oldACDict, batteryACDict)) {
	adapterInfoPublished = false;
    }

    // don't re-publish AC info until the adapter changes
    if (adapterExists && adapterInfoPublished) {
        return kIOReturnSuccess;
    }

    if (adapterExists && !batteryACDict)
    {
        ret = _getACAdapterInfo(&acBits);
        if (kIOReturnSuccess != ret) {
            return ret;
        }
        
        // Decode SMC key
        valFamily = (acBits >> kACFamilyBit) & 0xFF;
        valPower = (acBits >> kACPowerBit) & 0xFF;
        if ( (valSource = (acBits >> kASourceIdBit) & 0xF))
        {
            // New format
            valSerial = (acBits >> kACSerialBit) & 0x1FFFFFF;
            valCurrent = ((acBits >> kACurrentIdBit) & 0xFF) * 25;
        }
        else 
        {
            // Legacy format
            valSerial = (acBits >> kACSerialBit) & 0xFFFFFF;
            valRevision = (acBits >> kACRevisionBit) & 0xF;
            valID = (acBits >> kACIDBit) & 0xFFF;
        }
//        valCRC = (acBits >> kACCRCBit) & 0xFF;
        
        // Publish values in dictionary
        acDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
        if (!acDict) {
            ret = kIOReturnNoMemory;
            goto exit;
        }
        
        stuffNum = CFNumberCreate(0, kCFNumberSInt32Type, &valSerial);
        if (stuffNum) {
            CFDictionarySetValue(acDict, CFSTR(kIOPSPowerAdapterSerialNumberKey), stuffNum);
            CFRelease(stuffNum);
        }
        stuffNum = CFNumberCreate(0, kCFNumberSInt32Type, &valFamily);
        if (stuffNum) {
            CFDictionarySetValue(acDict, CFSTR(kIOPSPowerAdapterFamilyKey), stuffNum);
            CFRelease(stuffNum);
        }
        if (valSource) {
            // New format
            if (valPower == 60 && valCurrent == 4250)
                valPower = 85;

            stuffNum = CFNumberCreate(0, kCFNumberSInt32Type, &valCurrent);
            if (stuffNum) {
                CFDictionarySetValue(acDict, CFSTR(kIOPSPowerAdapterCurrentKey), stuffNum);
                CFRelease(stuffNum);
            }

            stuffNum = CFNumberCreate(0, kCFNumberSInt32Type, &valSource);
            if (stuffNum) {
                CFDictionarySetValue(acDict, CFSTR(kIOPSPowerAdapterSourceKey), stuffNum);
                CFRelease(stuffNum);
            }
        }
        else {
            // Legacy format
            stuffNum = CFNumberCreate(0, kCFNumberSInt32Type, &valRevision);
            if (stuffNum) {
                CFDictionarySetValue(acDict, CFSTR(kIOPSPowerAdapterRevisionKey), stuffNum);
                CFRelease(stuffNum);
            }

            stuffNum = CFNumberCreate(0, kCFNumberSInt32Type, &valID);
            if (stuffNum) {
                CFDictionarySetValue(acDict, CFSTR(kIOPSPowerAdapterIDKey), stuffNum);
                CFRelease(stuffNum);
            }
        }
        stuffNum = CFNumberCreate(0, kCFNumberSInt32Type, &valPower);
        if (stuffNum) {
            CFDictionarySetValue(acDict, CFSTR(kIOPSPowerAdapterWattsKey), stuffNum);
            CFRelease(stuffNum);
        }       
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
/* newKeyForType
 * Assigns a unique string as the key for the power source type.
 * The name should reflect the power source's type, and should have a unique
 * integer appended to unique it within the system.
 */
static CFStringRef _newKeyForType(char *type)
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
  
    new_tracker->scdsKey = _newKeyForType(clienttype);
    
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

    details = isA_CFDictionary(IOCFUnserialize((const char *)details_ptr, NULL, 0, NULL));
    if (!details)
        goto exit;

    PMStoreSetValue(dskeyCFSTR, details);
    
    *return_code = kIOReturnSuccess;
exit:
    if (dskeyCFSTR)
        CFRelease(dskeyCFSTR);
    if (details)
        CFRelease(details);
    return 0;
}

