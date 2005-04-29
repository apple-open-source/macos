/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/IOMessage.h>
#include <syslog.h>

#include "BatteryTimeRemaining.h"

/**** PMUBattery configd plugin
  The functions in battery.c calculate the "meta-data" from the raw data available from the hardware. 
  We provide the following information in a convenient CFDictionary format:
    Name
    CurrentCapacity
    MaxCapacity
    Remaining Time To Empty
    Remaining Time To Full Charge
    IsCharging
    IsPresent
    Type    
****/

// For tracking which algorithm to use to calculate time remaining
enum {
    kUseCurrentAlgorithm = 1,
    kUseChargeAlgorithm
};

// Return values from calculateTRWithCurrent
enum {
    kNothingToSeeHere = 0,
    kNoTimeEstimate,
};
    

// static global variables for tracking battery state
    static int      _trAlgorithm;

    static int      _batCount;
    static int      *_batLevel;
    static int      *_flags;
    static int      *_current;
    static int      *_charge;
    static int      *_capacity;
    static CFStringRef  *_batName;

    static double   *_hoursRemaining;
    static int      _state;
    static int      _pmExists;
    static int      _pluggedIn, _lastpluggedIn;
    static int      _impendingSleep = 0;

    static CFStringRef *PMUBatteryDynamicStore;

// For time remaining via battery capacity algorithm
#define kMaxBatterySamples          30
#define kMINIMUM_TIME_SPAN          30.0
    typedef struct {
        CFAbsoluteTime          bTime;
        int                    *bLevel;
    } batterySample;    
    static CFAbsoluteTime       sample_time_span = 0.0;
    static int                  samples_taken = 0;
    static int                  battery_history_index = 0;
    static int                  battery_history_start = 0;
    static batterySample        *battery_history;

// For time remaining via current calculation

    
// forward declarations
static void     _initializeBatteryCalculations(void);
static int      _calculateTRWithCurrent(void);
static void     _packageBatteryInfo(int, CFDictionaryRef *);
static void     _stashBatteryGlobals(CFArrayRef);
static void     _stashBatteryHistory(CFArrayRef);
static bool     _calculateTRWithCharge(void);
static bool     _checkCalcsStillValid(int);


static int _isBatteryPresent(int i) {
    return ((_flags[i] & kIOBatteryInstalled)?1:0);
}

static int _isACAdapterConnected(int i) {
    return ((_flags[i] & kIOBatteryChargerConnect)?1:0);
}

static int _isChargingBit(int i) {
    return ((_flags[i] & kIOBatteryCharge)?1:0);
}

// _isCharging does some magic to decide if the battery is REALLY charging
static int _isCharging(int i) {
    return ( _isChargingBit(i) && _isBatteryPresent(i) && _isACAdapterConnected(i) );
}


__private_extern__ void
BatteryTimeRemaining_prime(void)
{
    // setup battery calculation global variables
    _initializeBatteryCalculations();

    return;
}
 
__private_extern__ void
BatteryTimeRemainingSleepWakeNotification(natural_t messageType)
{
    switch ( messageType ) {
    case kIOMessageSystemWillSleep:
        // System is going to sleep - reset time remaining calculations.
        // Battery drain during sleep will produce an unrealistic time remaining
        // expectation on wake from sleep unless we reset the average sample.
        _impendingSleep = 1;
        break;
        
    case kIOMessageSystemHasPoweredOn:
        _impendingSleep = 0;
        break;
    }
}


static void     _initializeBatteryCalculations(void)
{
    io_registry_entry_t pm_root_domain = MACH_PORT_NULL;
    CFArrayRef          info = NULL;
    CFDictionaryRef     supported = NULL;
    int                 i, j;  
    bool                allocationFailure = false;
    
    if (IOPMCopyBatteryInfo(0, &info) != 0 || info == NULL) {
        // No batteries detected
        _pmExists = 0;
        return;
    }
    
    // Find IOPMrootDomain
    pm_root_domain = IORegistryEntryFromPath( MACH_PORT_NULL, 
                        kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
    supported = IORegistryEntryCreateCFProperty(
                        pm_root_domain,
                        CFSTR("Supported Features"),
                        kCFAllocatorDefault,
                        0);
//    if( supported && 
//        CFDictionaryGetValue(supported, CFSTR("BatteryReportsCurrent") )
    if (1)
    {
        // If battery accurately repors current, use time remaining algorithm
        // for current.
        _trAlgorithm = kUseCurrentAlgorithm;
    } else {
        _trAlgorithm = kUseChargeAlgorithm;

        // Allocate ring buffer for storing battery history
        battery_history = (batterySample *)calloc(1, 
                                kMaxBatterySamples * sizeof(batterySample *));
        if(NULL == battery_history) 
        {
            allocationFailure = true;
        } else {
            // Successful allocation
            for(i=0; i < kMaxBatterySamples; i++)
            {
                battery_history[i].bTime = 0;
                battery_history[i].bLevel = (int *) calloc(1, _batCount * sizeof(int));
                if (NULL == battery_history[i].bLevel) {
                    allocationFailure = true;
                }
            }
        }
    }

    // Batteries detected, get their initial state
    _batCount = CFArrayGetCount(info);
    if (_batCount > 0) {
        _batLevel = (int *) calloc(1, _batCount * sizeof(int));
        _flags = (int *) calloc(1, _batCount * sizeof(int));
        _current = (int *) calloc(1, _batCount * sizeof(int));
        _charge = (int *) calloc(1, _batCount * sizeof(int));
        _capacity = (int *) calloc(1, _batCount * sizeof(int));
        _batName = (CFStringRef *) calloc(1, _batCount * sizeof(CFStringRef));
        _hoursRemaining = (double *) calloc(1, _batCount * sizeof(double));
        PMUBatteryDynamicStore = (CFStringRef *) calloc(1, _batCount * sizeof(CFStringRef));

        if (_batLevel == NULL || _flags == NULL || _charge == NULL 
            || _capacity == NULL || _batName == NULL || _hoursRemaining == NULL 
            || PMUBatteryDynamicStore == NULL || _current == NULL
            || allocationFailure) {
            // We can't do much here.  There's no way to return failure and not be
            // loaded, so we'll just declare that we have no batteries and avoid
            // crashing.
            allocationFailure = true;
            _batCount = 0;
        }
    }
    
    for(i = 0;i < _batCount;i++){
        _batName[i] = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("InternalBattery-%d"), i);

        // Initialize SCDynamicStore battery key name
        PMUBatteryDynamicStore[i] = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@/InternalBattery-%d"),
                kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath), i);

        if (_batName[i] == NULL || PMUBatteryDynamicStore[i] == NULL) {
            allocationFailure = true;
            break;
        }
        CFNumberGetValue(CFDictionaryGetValue(CFArrayGetValueAtIndex((CFArrayRef)info,i),
            CFSTR("Current")),kCFNumberSInt32Type,&_charge[i]);
        _batLevel[i] = _charge[i];
        CFNumberGetValue(CFDictionaryGetValue(CFArrayGetValueAtIndex((CFArrayRef)info,i),
            CFSTR("Flags")),kCFNumberSInt32Type,&_flags[i]);
        _flags[i] &= kIOBatteryInstalled;

        if(_flags[i] & kIOBatteryChargerConnect)
            _lastpluggedIn = _pluggedIn = TRUE;
        else _lastpluggedIn = _pluggedIn = FALSE; 
    }

    if (allocationFailure) 
    {
        // Clean up as best as we can.
        if (i < _batCount) 
        {
            for(j = 0; j < i; j++) 
            {
                if (_batName[i] != NULL) CFRelease(_batName[i]);
                if (PMUBatteryDynamicStore[i] != NULL) CFRelease(PMUBatteryDynamicStore[i]);
            }
            _batCount = 0;
        }
        free(_batLevel); _batLevel = NULL;
        free(_flags); _flags = NULL;
        free(_current); _current = NULL;
        free(_charge); _charge = NULL;
        free(_capacity); _capacity = NULL;
        free(_batName); _batName = NULL;
        free(_hoursRemaining); _hoursRemaining = NULL;
        free(PMUBatteryDynamicStore); PMUBatteryDynamicStore = NULL;
        for(i = 0; i < kMaxBatterySamples; i++) 
        {
            free(battery_history[i].bLevel); battery_history[i].bLevel = NULL;
        }
        syslog(LOG_INFO, "Power management: Allocation failure in %s; disabling power management\n", __func__);
    }

    _pmExists = 1;

    // make initial call to populate array and publish state
    BatteryTimeRemainingBatteriesHaveChanged(info);

    CFRelease(info);
    if(supported) CFRelease(supported);

    // _state tracks the battery/plug state. If it changes, we know to reset our calculations.
    //  _state = 0 means there are no batteries
    //  _state = 1 means we are hooked up to AC/charger power, but not necessarily charging
    //  _state = 2 means we aren't hooked up to AC power, and are d_isCharging
    if(_isACAdapterConnected(0)) {
        _state = 1;
    } else {
        _state = 0;
        for(i = 0; i < _batCount; i++) 
        {
            if (_isBatteryPresent(i)) 
            {
                _state = 2;
                break;
            }
        }
    }

    samples_taken=0;
    
    return;
}


__private_extern__ void
BatteryTimeRemainingBatteriesHaveChanged(CFArrayRef battery_info)
{
    CFIndex                     batteryCount;
    CFDictionaryRef             *result = NULL;
    int                         i;
    int                         invalid_time_remaining = 0;
    int                         calculation_return = kNothingToSeeHere;
    int                         not_enough_samples;
    static SCDynamicStoreRef    store = NULL;
    static CFDictionaryRef      *old_battery;

    if(!battery_info) return;

    batteryCount = CFArrayGetCount(battery_info);
    if ( NULL == old_battery ) {
        old_battery = (CFDictionaryRef *) calloc(1, batteryCount * sizeof(CFDictionaryRef));
        if ( NULL == old_battery ) {
            // Uh-oh.
            syslog(LOG_INFO, "Power management: Failed to allocate old_battery in %s\n", __func__);
            return;
        }
    }

    result = (CFDictionaryRef *) calloc(1, batteryCount * sizeof(CFDictionaryRef));
    if ( NULL == result ) {
        // This isn't good.
        syslog(LOG_INFO, "Power management: Failed to allocate result in %s\n", __func__);
        return;
    }
    
    _stashBatteryGlobals(battery_info);
    
    if(kUseCurrentAlgorithm == _trAlgorithm)
    {
        // Calculate time remaining using current
        calculation_return = _calculateTRWithCurrent();
        
        if(kNoTimeEstimate == calculation_return)
        {
            invalid_time_remaining = 1;
        }        
    } else {
        // Calculate time remaining using charges
        _stashBatteryHistory(battery_info);
    
        not_enough_samples = !_calculateTRWithCharge();
    
        invalid_time_remaining = !_checkCalcsStillValid(not_enough_samples);
    }

    // At this point either algorithm above has populated the global variable
    // _hoursRemaining[]. We'll package that info into user-consumable dictionaries
    // below.

    _packageBatteryInfo(invalid_time_remaining, result);

    // Publish the results of calculation in the SCDynamicStore
    if(!store) store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("PMUBattery configd plugin"), NULL, NULL);
    for(i=0; i<batteryCount; i++) {
        if(result[i]) {   
            // Determine if CFDictionary is new or has changed...
            // Only do SCDynamicStoreSetValue if the dictionary is different
            if(!old_battery[i]) {
                SCDynamicStoreSetValue(store, PMUBatteryDynamicStore[i], result[i]);
            } else {
                if(!CFEqual(old_battery[i], result[i])) {
                    SCDynamicStoreSetValue(store, PMUBatteryDynamicStore[i], result[i]);
                }
                CFRelease(old_battery[i]);
            }
            old_battery[i] = result[i];
        }
    }
    if(result) free(result);
}


static void resetBatterySamples(void)
{
    int i;
    
    sample_time_span = 0.0;
    battery_history_index = 0;
    battery_history_start = 0;
    samples_taken = 0;

    battery_history[0].bTime = CFAbsoluteTimeGetCurrent();
    for(i = 0; i < _batCount; i++) {
        battery_history[0].bLevel[i] = _charge[i];
    }
}


void _stashBatteryGlobals(CFArrayRef info)
{
    int                     i;
    
    if(info) _batCount = CFArrayGetCount(info);
    else return;
    
    // Unload battery state into our maze of global variables
    _pluggedIn = FALSE;
    for (i=0;i<_batCount;i++) {        
        _charge[i] = 0;
        // Grab Flags, Charge, and Capacity from the battery
        CFNumberGetValue(CFDictionaryGetValue(CFArrayGetValueAtIndex((CFArrayRef)info,i),
                CFSTR("Flags")),kCFNumberSInt32Type,&_flags[i]);
        CFNumberGetValue(CFDictionaryGetValue(CFArrayGetValueAtIndex((CFArrayRef)info,i),
                CFSTR("Amperage")),kCFNumberSInt32Type,&_current[i]);
        CFNumberGetValue(CFDictionaryGetValue(CFArrayGetValueAtIndex((CFArrayRef)info,i),
                CFSTR("Current")),kCFNumberSInt32Type,&_charge[i]);
        CFNumberGetValue(CFDictionaryGetValue(CFArrayGetValueAtIndex((CFArrayRef)info,i),
                CFSTR("Capacity")),kCFNumberSInt32Type,&_capacity[i]);
        
        // Determine plugged state
        if (_flags[i]&kIOBatteryChargerConnect) {
            _pluggedIn = TRUE;
        }

        // Cap the battery charge to its maximum capacity
        if(_charge[i] > _capacity[i]) _charge[i] = _capacity[i];
        
        // Zero out charge and battery level for non-present batteries.
        if (!_isBatteryPresent(i)) {
            _charge[i] = 0;
            _batLevel[i] = 0;
        }
    }
}

void _stashBatteryHistory(CFArrayRef info)
{
    int                     i;
    
    if(info) _batCount = CFArrayGetCount(info);
    else return;
    
    // Stash state into battery_history array
    battery_history_index = (battery_history_index+1) % kMaxBatterySamples;
    battery_history[battery_history_index].bTime = CFAbsoluteTimeGetCurrent();
    for(i = 0; i < _batCount; i++) {
        battery_history[battery_history_index].bLevel[i] = _charge[i];
    }

    if(samples_taken < kMaxBatterySamples) 
    {
        // expect battery_history_start == 0
        samples_taken++;
    } else {
        // expect samples_taken == kMaxBatterySamples
        battery_history_start = (battery_history_index+1)%kMaxBatterySamples;
    }

    sample_time_span = battery_history[battery_history_index].bTime -
        battery_history[battery_history_start].bTime;
}

/* _calculateTRWithCurrent
 * Implicit inputs: global battery state variables
 * Implicit output: values placed in global hoursRemaining[] variable
 *   returns true if we reached a valid estimate
 *   returns false if we're still calculating
 */
int _calculateTRWithCurrent(void)
{
    int     ret_val = kNothingToSeeHere;
    int     i;
    
    for(i=0; i<_batCount; i++)
    {
        // If current is zero, finding a time remaining estimate is irrelevant
        // (in the case of being fully charged) or impossible (in the case
        // of having just plugged into AC and the PMU is thinking.
        // Allowing for some slop in either direction of zero.
        // While operating, current should be roughly in the 1,000mA-2,000mA
        // range. We allow for 5mA slop here.
        if( (_current[i] < 5) &&
            (_current[i] > -5) )
        {
            _hoursRemaining[i] = 0;
            ret_val = kNoTimeEstimate;
            continue;
        }
        
        if(_isChargingBit(i))
        {
            // h = -mAh/mA
            _hoursRemaining[i] = ((double)(_capacity[i] - _charge[i])
                                / (double)_current[i]);
        } else { // discharging
            // h = mAh/mA
            _hoursRemaining[i] = -((double)_charge[i]
                                / (double)_current[i]);
        }
    }
    return ret_val;
}


/* _calculateTRWithCharge
 * Implicit inputs: global battery state variables
 * Implicit output: values placed in global hoursRemaining[] variable
 *   returns true if we reached a valid estimate
 *   returns false if we're still calculating
 */
bool _calculateTRWithCharge(void)
{
    int                             i;
    CFAbsoluteTime                  time_span = 0.0;
    int                             samples_counted;
    int                             early_index, late_index;
    double                          *discharge_rate;
    long int                        *batt_level_delta;
    bool                            acAdapterConnected, batteryPresent;

    if(samples_taken <= 1) {
        // Not enough data to go from. Return.
        return false; // return "not enough data"
    }
    
    // Zero-initialize here rather than doing it below.
    discharge_rate = (double *) calloc(1, _batCount * sizeof(double));
    if ( NULL == discharge_rate) {
        syslog(LOG_INFO, "Power management: Failed to allocate discharge_rate in %s\n", __func__);
        return false;
    }
    
    batt_level_delta = (long int *) malloc(_batCount * sizeof(long int));
    if ( NULL == batt_level_delta ) {
        syslog(LOG_INFO, "Power management: Failed to allocate batt_level_delta in %s\n", __func__);
        free(discharge_rate);
        return false;
    }

    samples_counted = 0;
    early_index = late_index = battery_history_index;

    time_span = 0.0;
    // Sum up the lats few samples to determine the battery discharge rate.
    while( (time_span < 300.0) && (early_index != battery_history_start) )
    {
        // decrement early end of our range
        early_index = (early_index - 1 + kMaxBatterySamples) % kMaxBatterySamples;
        
        time_span = battery_history[late_index].bTime - battery_history[early_index].bTime;

        samples_counted++;
    }
    
    if(time_span < kMINIMUM_TIME_SPAN)
    {
        // Not enough data to go from.
        return false; // return "not enough data"    
    }

    for(i = 0; i<_batCount; i++)
    {
        batt_level_delta[i] = battery_history[early_index].bLevel[i] - battery_history[late_index].bLevel[i];
        discharge_rate[i] = ((double)batt_level_delta[i])/(double)lround(time_span);
    }
    

    // Use the charge or discharge rate calculated above, combined with the current battery
    // level, to estimate the time remaining on this set of batteries.
    acAdapterConnected = false;
    for(i = 0; i < _batCount; i++) {
        if (_isACAdapterConnected(i)) {
            acAdapterConnected = true;
            break;
        }
    }
    batteryPresent = false;
    for(i = 0; i < _batCount; i++) {
        if (_isBatteryPresent(i)) {
            batteryPresent = true;
            break;
        }
    }
    if (acAdapterConnected) {
        double      discharge_rate_combined;
        int         *toCharge;
        
        toCharge = (int *) malloc(_batCount * sizeof(int));
        if ( NULL == toCharge ) {
            // Uh-oh.
            syslog(LOG_INFO, "Power management: Failed to allocate toCharge in %s\n", __func__);
            free(discharge_rate);
            free(batt_level_delta);
            return false;
        }
        for(i = 0; i < _batCount; i++) {
            toCharge[i] = (((_flags[i] & kIOPMBatteryInstalled) == kIOPMBatteryInstalled) ?
                            _capacity[i] : 0) - _charge[i];
        }
        
        if (_state != 1) {
            resetBatterySamples();
        }
        
        _state = 1;
        
        discharge_rate_combined = 0;
        for(i = 0; i < _batCount; i++) {
            if (discharge_rate[i] > 0) {    
                discharge_rate[i] = 0;
            }
            discharge_rate_combined += (double) discharge_rate[i];
        }
        
        if(discharge_rate_combined != 0.0)
        {
            for(i = 0; i < _batCount; i++) {
                _hoursRemaining[i] = -1*(((double)toCharge[i])/discharge_rate_combined) / 3600;
            }
        } else {
            for(i = 0; i < _batCount; i++) {
                _hoursRemaining[i] = -1.0;
            }
        }

    } else if (!batteryPresent) {
        //When there are no batteries installed
        //if there were batteries installed before hand, then reset the history (so we get
        //a fresh buffer when a battery is installed)
        if (_state) {
            resetBatterySamples();
        }
        //_state == 0 means that there are no batteries
        _state = 0;
    } else {
        double      discharge_rate_combined;
    
        for(i = 0; i < _batCount; i++) {
            if (discharge_rate[i] < 0) {    
                discharge_rate[i] = 0;
            }
        }
        
        //if We just switched states (plugged to not), then start the charge array from scratch
        if (_state != 2) {
            resetBatterySamples();
        }
        
        _state = 2;
        
        discharge_rate_combined = 0;
        for(i = 0; i < _batCount; i++) {
            discharge_rate_combined += (double)discharge_rate[i];
        }

        if(discharge_rate_combined != 0.0)
        {
            for(i = 0; i < _batCount; i++) {
                _hoursRemaining[i] = (((double)_charge[i])/discharge_rate_combined) / 3600;
            }
        } else {
            for(i = 0; i < _batCount; i++) {
                _hoursRemaining[i] = -1.0;
            }
        }
    }
        
    for(i = 0; i < _batCount; i++) {
        _batLevel[i]   = _charge[i];
    }
    
    return true;
}

/* _calculateBatteryTimeRemaining
 *   returns true if we reached a valid estimate
 *   returns false if we're still calculating
 */
bool _checkCalcsStillValid(int not_enough_battery_samples)
{
    static int      old_num_present = -1;
    int             num_present = 0;
    int             ehours = 0;
    int             eminutes = 0;
    double          totalHoursRemaining = 0;
    int             i;

    // If power source has changed, reset history count and restart battery sampling
    // triggers stillCalc = 1
    if(_pluggedIn != _lastpluggedIn) {
        resetBatterySamples();
        _lastpluggedIn = _pluggedIn;
        return false;
    }
    
    // System will sleep soon. Re-calculate time remaining from scratch.
    // Keep counts wired to 0 until _impendingSleep is non-NULL
    // We set _impendingSleep=0 when the system is fully awake.
    if(_impendingSleep) {
        resetBatterySamples();
        _state = -1;
        return false;
    }

     // is the calculated time reasonable? (0 <= hours < 10) && (
    for(i = 0; i < _batCount; i++) {
        totalHoursRemaining += _hoursRemaining[i];
    }
    ehours = (int) totalHoursRemaining;
    eminutes = (int)(60.0*(totalHoursRemaining - (double)ehours));
    if (!((ehours < 10) && (ehours >= 0)) && ((eminutes < 61) && (eminutes >= 0))) {
        return false;
    }
    
    for(i = 0; i < _batCount; i++) {
        if (_isBatteryPresent(i)) {
            num_present++;
        }
    }
    
    // 2 batteries to 1? or 1 to 2? or 0 to 1?
    if(num_present != old_num_present) {
        resetBatterySamples();
        old_num_present = num_present;
        return false;
    }
    old_num_present = num_present;
    
    // If we're plugged-in but not charging, we're fully charged and there's nothing
    // to calculate. Set stillCalc to 0.
    // EJA: The comment here doesn't seem to match the code.  The code checks to see
    // if any one battery is an AC adapter that is not charging, but the comment
    // implies that all of the batteries should not be charging.
    for(i = 0; i < _batCount; i++) {
        if (_isACAdapterConnected(i) && !_isCharging(i)) {
            return true;
        }
    }
    
    return !not_enough_battery_samples;
}

/* 
 * Implicit argument: All the global variables that track battery state
 */
void _packageBatteryInfo(int stillCalc, CFDictionaryRef *ret)
{
    CFNumberRef         n, n0, nneg1;
    CFMutableDictionaryRef  mutDict = NULL;
    int             i;
    int             temp;
    int             minutes;
    int             set_capacity, set_charge;

    // Stuff battery info into CFDictionaries
    for(i=0; i<_batCount; i++) {
        // Create the battery info dictionary
        mutDict = NULL;
        mutDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if(!mutDict) return;
        
        // Set transport type to "Internal"
        CFDictionarySetValue(mutDict, CFSTR(kIOPSTransportTypeKey), CFSTR(kIOPSInternalType));

        // Set Power Source State to AC/Battery
        CFDictionarySetValue(mutDict, CFSTR(kIOPSPowerSourceStateKey), 
                    _pluggedIn ? CFSTR(kIOPSACPowerValue):CFSTR(kIOPSBatteryPowerValue));
                    
        // round charge and capacity down to a % scale
        if(0 != _capacity[i])
        {
            set_capacity = 100;
            set_charge = (int)((double)_charge[i]*100.0/(double)_capacity[i]);
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
                    (_isBatteryPresent(i)) ? kCFBooleanTrue:kCFBooleanFalse);
        
        // Set _isCharging and time remaining
        minutes = (int)(60.0*_hoursRemaining[i]);
        temp = 0;
        n0 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &temp);
        temp = -1;
        nneg1 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &temp);
        if( !_isBatteryPresent(i) ) {
            // remaining time calculations only have meaning if the battery is present
            CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanFalse);
            CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
            CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n0);
        } else {
            // A battery is installed
            if(stillCalc) {
                // If we are still calculating then our time remaining
                // numbers aren't valid yet. Stuff with -1.
                CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), 
                        _isCharging(i) ? kCFBooleanTrue : kCFBooleanFalse);
                CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), nneg1);
                CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), nneg1);
            } else {   
                // else there IS a battery installed, and remaining time calculation makes sense
                if(_isCharging(i)) {
                    // Set _isCharging to True
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanTrue);
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
                    if(_isACAdapterConnected(i))
                    {
                        // plugged in but not charging == fully charged
                        CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
                        CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n0);
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
        }
        CFRelease(n0);
        CFRelease(nneg1);

        // Set name
        CFDictionarySetValue(mutDict, CFSTR(kIOPSNameKey), _batName[i]);
        ret[i] = mutDict;
    }

    return;
}
