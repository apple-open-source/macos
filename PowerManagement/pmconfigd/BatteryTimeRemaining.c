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

#define MAX_BATTERY_NUM		        2
#define kMaxBatterySamples          30
#define kMINIMUM_TIME_SPAN          30.0

// static global variables for tracking battery state
    static int      _batCount;
    static int		_batLevel[MAX_BATTERY_NUM];
    static int		_flags[MAX_BATTERY_NUM];
    static int		_charge[MAX_BATTERY_NUM];
    static int		_capacity[MAX_BATTERY_NUM];
    static CFStringRef	_batName[MAX_BATTERY_NUM];

    static double	_hoursRemaining[MAX_BATTERY_NUM];
    static int		_state;
    static int		_pmExists;
    static int		_pluggedIn, _lastpluggedIn;
    static int		_impendingSleep = 0;

    typedef struct {
        CFAbsoluteTime          bTime;
        long                    bLevel[MAX_BATTERY_NUM];
    } batterySample;
    
    static CFAbsoluteTime       sample_time_span = 0.0;
    static int                  samples_taken = 0;
    static int                  battery_history_index = 0;
    static int                  battery_history_start = 0;
    static batterySample        battery_history[kMaxBatterySamples];

    static CFStringRef PMUBatteryDynamicStore[MAX_BATTERY_NUM];

    
// forward declarations
    static void     _initializeBatteryCalculations(void);
    static void     _stashBatteryInfo(CFArrayRef);
    static bool     _calculateBatteryTimeRemaining(void);
    static bool     _checkCalcsStillValid(int);
    static void     _packageBatteryInfo(bool, CFDictionaryRef *);


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
    // Initialize SCDynamicStore battery key names
    PMUBatteryDynamicStore[0] = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@/%@"),
            kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath), CFSTR("InternalBattery-0"));
    PMUBatteryDynamicStore[1] = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@/%@"),
            kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath), CFSTR("InternalBattery-1"));

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


static void 	_initializeBatteryCalculations(void)
{
    io_connect_t		pm_tmp;
    CFArrayRef          info;
    int				    i;
    

    for(i=0; i < kMaxBatterySamples; i++)
    {
        battery_history[i].bTime = 0;
        battery_history[i].bLevel[0] = battery_history[i].bLevel[1] = 0;    
    }

    _batName[0] = CFSTR("InternalBattery-0");
    _batName[1] = CFSTR("InternalBattery-1");
    
    if( (pm_tmp = IOPMFindPowerManagement(0)) ){
        if((IOPMCopyBatteryInfo(0, &info) != 0) || !info ){
            // no batteries detected
            _pmExists = 0;
            return;
        }

        // Batteries detected, get their initial state
        _batCount = CFArrayGetCount(info);
        for(i = 0;i < _batCount;i++){
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
        _pmExists = 1;
    
        // make initial call to populate array and publish state
        BatteryTimeRemainingBatteriesHaveChanged(info);

        CFRelease(info);
        IOObjectRelease(pm_tmp);
    }

    // _state tracks the battery/plug state. If it changes, we know to reset our calculations.
    //  _state = 0 means there are no batteries
    //  _state = 1 means we are hooked up to AC/charger power, but not necessarily charging
    //  _state = 2 means we aren't hooked up to AC power, and are d_isCharging
    if(_isACAdapterConnected(0))
        _state = 1;
	else if(!_isBatteryPresent(0) && !_isBatteryPresent(1))
	    _state = 0;
	else _state = 2;

    samples_taken=0;

    return;
}


__private_extern__ void
BatteryTimeRemainingBatteriesHaveChanged(CFArrayRef battery_info)
{
    CFDictionaryRef             result[MAX_BATTERY_NUM] = {NULL, NULL};
    int                         i;
    int                         still_calculating;
    int                         not_enough_samples;
    static SCDynamicStoreRef    store = NULL;
    static CFDictionaryRef      old_battery[MAX_BATTERY_NUM] = {NULL, NULL};

    if(!battery_info) return;

#if DEBUGLEVEL>50
    printf("battery sample taken!\n"); fflush(stdout);
#endif
    
    _stashBatteryInfo(battery_info);

    not_enough_samples = !_calculateBatteryTimeRemaining();

    still_calculating = !_checkCalcsStillValid(not_enough_samples);

    _packageBatteryInfo(still_calculating, result);

    // Publish the results of calculation in the SCDynamicStore
    if(!store) store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("PMUBattery configd plugin"), NULL, NULL);
    for(i=0; i<MAX_BATTERY_NUM; i++) {
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
}


static void resetBatterySamples(void)
{
    sample_time_span = 0.0;
    battery_history_index = 0;
    battery_history_start = 0;
    samples_taken = 0;

    battery_history[0].bTime = CFAbsoluteTimeGetCurrent();
    battery_history[0].bLevel[0] = _charge[0];
    battery_history[0].bLevel[1] = _charge[1];
}


void _stashBatteryInfo(CFArrayRef info)
{
    int                     i;
    
    if(info) _batCount = CFArrayGetCount(info);
    else return;
    
    // Unload battery state into our maze of global variables
    _pluggedIn = FALSE;
    _charge[0] = _charge[1] = 0;
    for (i=0;i<_batCount;i++) {        
        // Grab Flags, Charge, and Capacity from the battery
        CFNumberGetValue(CFDictionaryGetValue(CFArrayGetValueAtIndex((CFArrayRef)info,i),
                CFSTR("Flags")),kCFNumberSInt32Type,&_flags[i]);
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
    }

    if (!_isBatteryPresent(0)) {
        _charge[0] = 0;
        _batLevel[0] = 0;
    }    
    if (!_isBatteryPresent(1)) {
        _charge[1] = 0;
        _batLevel[1] = 0;
    }
    
    // Stash state into battery_history array
    battery_history_index = (battery_history_index+1) % kMaxBatterySamples;
    battery_history[battery_history_index].bTime = CFAbsoluteTimeGetCurrent();
    battery_history[battery_history_index].bLevel[0] = _charge[0];
    battery_history[battery_history_index].bLevel[1] = _charge[1];

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

/* _calculateBatteryTimeRemaining
 *   returns true if we reached a valid estimate
 *   returns false if we're still calculating
 */
bool _calculateBatteryTimeRemaining(void)
{
    int                             i;
    CFAbsoluteTime                  time_span = 0.0;
    int                             samples_counted;
    int                             early_index, late_index;
    double                          discharge_rate[MAX_BATTERY_NUM];
    long int                        batt_level_delta[MAX_BATTERY_NUM];

    if(samples_taken <= 1) {
        // Not enough data to go from. Return.
        return false; // return "not enough data"
    }
    
#if DEBUGLEVEL>50
    syslog(LOG_INFO, "global_span=%d\n", lround(sample_time_span));
    syslog(LOG_INFO, "samples_taken=%d, battery_history_start=%d, battery_history_index=%d\n", 
            samples_taken, battery_history_start, battery_history_index);
#endif

    samples_counted = 0;
    discharge_rate[0] = discharge_rate[1] = 0.0;
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
    
#if DEBUGLEVEL>50
    syslog(LOG_INFO, "loop exit conditions (time_span=%d<300.0)=%d, samples_counted=%d, samples_taken=%d\n",
        lround(time_span), (time_span<300.0), samples_counted, samples_taken);
    syslog(LOG_INFO, "early_index = %d, late_index = %d\n", early_index, late_index);
#endif

    for(i = 0; i<MAX_BATTERY_NUM; i++)
    {
#if DEBUGLEVEL>50
    	syslog(LOG_INFO, "   (%d) starting level = %d, ending level = %d, over %d seconds\n",
    		i, battery_history[early_index].bLevel[i], battery_history[late_index].bLevel[i], lround(time_span));
#endif
        batt_level_delta[i] = battery_history[early_index].bLevel[i] - battery_history[late_index].bLevel[i];
        discharge_rate[i] = ((double)batt_level_delta[i])/(double)lround(time_span);
    }

#if DEBUGLEVEL>50
    syslog(LOG_INFO, "discharge_rate = %ld/%d = %f\n", batt_level_delta[0], 
        lround(time_span), discharge_rate[0]);
#endif    
    

    // Use the charge or discharge rate calculated above, combined with the current battery
    // level, to estimate the time remaining on this set of batteries.
    if ((_flags[0] & kIOBatteryChargerConnect) || (_flags[1] & kIOBatteryChargerConnect)) {
        double      discharge_rate_combined;
        int         toCharge[MAX_BATTERY_NUM];
                
        toCharge[0] = (((_flags[0] & 0x4) == 4)?_capacity[0]:0) - _charge[0];
        toCharge[1] = (((_flags[1] & 0x4) == 4)?_capacity[1]:0) - _charge[1];
        
        if (_state != 1) {
            resetBatterySamples();
        }
        
        _state = 1;
        
        if (discharge_rate[0] > 0) {	
            discharge_rate[0] = 0;
        }
        
        if (discharge_rate[1] > 0) {	
            discharge_rate[1] = 0;
        }
        
        discharge_rate_combined = ((double)discharge_rate[0]+(double)discharge_rate[1]);
        
        if(discharge_rate_combined != 0.0)
        {
            _hoursRemaining[0] = -1*(((double)toCharge[0])/discharge_rate_combined) / 3600;
            _hoursRemaining[1] = -1*(((double)toCharge[1])/discharge_rate_combined) / 3600;
        } else {
            _hoursRemaining[0] = _hoursRemaining[1] = -1.0;
        }

    } else if (!_isBatteryPresent(0) && !_isBatteryPresent(1)) {
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
    
        if (discharge_rate[0] < 0) {	
            discharge_rate[0] = 0;
        }
        if (discharge_rate[1] < 0) {	
            discharge_rate[1] = 0;
        }
        
        //if We just switched states (plugged to not), then start the charge array from scratch
        if (_state != 2) {
            resetBatterySamples();
        }
        
        _state = 2;
        
        discharge_rate_combined = ((double)discharge_rate[0]+(double)discharge_rate[1]);

        if(discharge_rate_combined != 0.0)
        {
            _hoursRemaining[0] = (((double)_charge[0])/discharge_rate_combined) / 3600;
            _hoursRemaining[1] = (((double)_charge[1])/discharge_rate_combined) / 3600;
        } else {
            _hoursRemaining[0] = _hoursRemaining[1] = -1.0;
        }
    }
    
#if DEBUGLEVEL>50
    syslog(LOG_INFO, "remaining [0] = %d:%2d, [1] = %d:%2d\n", 
        lround(_hoursRemaining[0]*60.0), lround(_hoursRemaining[0]*60.0)%60, lround(_hoursRemaining[1]*60.0));
#endif
    
    _batLevel[0]   = _charge[0];
    _batLevel[1]   = _charge[1];
    
    return true;

}

/* _calculateBatteryTimeRemaining
 *   returns true if we reached a valid estimate
 *   returns false if we're still calculating
 */
bool _checkCalcsStillValid(int not_enough_battery_samples)
{
	static int      old_num_present = -1;
    int             num_present = _isBatteryPresent(0) + _isBatteryPresent(1);
    bool            calcsValid = true;
    int				ehours = 0;
    int				eminutes = 0;
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
    ehours = (int)(_hoursRemaining[0] + _hoursRemaining[1]);
    eminutes = (int)(60.0*(_hoursRemaining[0] + _hoursRemaining[1] - (double)ehours));
    if (!((ehours < 10) && (ehours >= 0)) && ((eminutes < 61) && (eminutes >= 0))) {
        return false;
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
    if( (_isACAdapterConnected(0) && !_isCharging(0)) ||
        (_isACAdapterConnected(1) && !_isCharging(1)) );
    {
        return true;
    }
    
    return !not_enough_battery_samples;
}

void _packageBatteryInfo(bool calc_still_calc, CFDictionaryRef *ret)
{
    CFNumberRef			n, n0, nneg1;
    CFMutableDictionaryRef 	mutDict = NULL;
    int             i;
    int             temp;
    int	            minutes;
    int             set_capacity, set_charge;
    bool            stillCalc = calc_still_calc;

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
