/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
    Remaining Time To Empty
    Remaining Time To Full Charge
    IsCharging
    BatteryIsPresent
    
 
 Future work:
 - ApplePMU.kext should send notifications when the battery info has changed. This plugin should only
   check the battery on each of those notifications. Right now the plugin checks the battery every second
   so that we can observe AC plug/unplug events in "real time" and present them to the UI (in Battery Monitor).
   
 - This plugin should also compare its new battery info to the most recently published battery info to avoid
   unnecessarily duplicated work of clients. Note that whenever we publish a new battery state, every 
   "interested client" of the PowerSource API receives a notification.
****/

#define MAX_BATTERY_NUM		2
#define kBATTERY_HISTORY	30

// static global variables for tracking battery state
    static int		_batLevel[MAX_BATTERY_NUM];
    static int		_flags[MAX_BATTERY_NUM];
    static int		_charge[MAX_BATTERY_NUM];
    static int		_capacity[MAX_BATTERY_NUM];
    static CFStringRef	_batName[MAX_BATTERY_NUM];

    static double	_batteryHistory[MAX_BATTERY_NUM][kBATTERY_HISTORY];
    static int		_historyCount,_historyLimit;
    static double	_hoursRemaining[MAX_BATTERY_NUM];
    static int		_state;
    static int		_pmExists;
    static unsigned int	_avgCount;
    static mach_port_t	_batteryPort;
    static int		_pluggedIn, _lastpluggedIn;
    static int		_charging;
    static int		_impendingSleep = 0;


static CFStringRef PMUBatteryDynamicStore[MAX_BATTERY_NUM];

// These are defined below
static void	    calculateTimeRemaining(void);
static bool     _IOPMCalculateBatteryInfo(CFArrayRef, CFDictionaryRef *);
static void     _IOPMCalculateBatterySetup(void);


__private_extern__ void
BatteryTimeRemaining_prime(void)
{
    // setup battery calculation global variables
    _IOPMCalculateBatterySetup();
    
    // Initialize SCDynamicStore battery key names
    PMUBatteryDynamicStore[0] = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@/%@"),
            kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath), CFSTR("InternalBattery-0"));
    PMUBatteryDynamicStore[1] = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@/%@"),
            kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath), CFSTR("InternalBattery-1"));

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

__private_extern__ void
BatteryTimeRemainingBatteryPollingTimer(CFArrayRef battery_info)
{
    CFDictionaryRef	result[MAX_BATTERY_NUM];
    int			i;
    static SCDynamicStoreRef	store = NULL;
    static CFDictionaryRef	old_battery[MAX_BATTERY_NUM] = {NULL, NULL};
    
    if(!battery_info) return;
    
    if(!store) 
        store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("PMUBattery configd plugin"), NULL, NULL);

    bzero(result, MAX_BATTERY_NUM*sizeof(CFDictionaryRef));
    _IOPMCalculateBatteryInfo(battery_info, result);
    
    // Publish the results of calculation in the SCDynamicStore
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

static void 	_IOPMCalculateBatterySetup(void)
{
    io_connect_t		pm_tmp;
    int 			kr;
    CFArrayRef		    	info;
    int				count,i;
    
    // Initialize battery history to 3?
    for(count = 0;count < kBATTERY_HISTORY;count++){
        _batteryHistory[0][count] = 3;
        _batteryHistory[1][count] = 3;
    }
     
    _batName[0] = CFSTR("InternalBattery-0");
    _batName[1] = CFSTR("InternalBattery-1");
    

    
    kr = IOMasterPort(bootstrap_port,&_batteryPort);
    if(kr == kIOReturnSuccess){
        if( (pm_tmp = IOPMFindPowerManagement(_batteryPort)) ){
            if((IOPMCopyBatteryInfo(_batteryPort,&info) != 0) || !info ){
                // no batteries detected
                _pmExists = 0;
                return;
            }

            // Batteries detected, get their initial state
            count = CFArrayGetCount(info);
            for(i = 0;i < count;i++){
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
            CFRelease(info);
            IOObjectRelease(pm_tmp);
        }else
            _pmExists = 0;            
    }
        
    return;
}


// Local helper functions
static int isBatteryPresent(int i)
{
    return ((_flags[i] & kIOBatteryInstalled) ? 1:0);
}

static int isACAdapterConnected(int i)
{
    return (_pluggedIn);
}

// Is Charging does some magic to decide if the battery is REALLY charging
// The PowerBook G3 lies to us - even when the battery is fully charged the
// "isCharging" bit is still true.
static int isCharging(int i)
{
    // Not charging if we're not plugged in
    if(!isACAdapterConnected(i))
        return false;

    // We are plugged in, but are we at full capacity?
    if(_capacity[i] == _charge[i])
        return false;
    
    // Plugged in, but not charging, so we'll fall back on the PMU's reported value
    return ((_flags[i] & kIOBatteryCharge) ? 1:0);
}


/**** _IOPMCalculateBatteryInfo(CFArrayRef info, CFDictionaryRef *ret)
 Calculates remaining battery time and stuffs lots of battery status into a CFDictionary.
 Arguments: 
    CFArrayRef info - The CFArrayRef of raw battery data returned by IOPMCopyBatteryInfo
    CFDictionaryRef ret - A caller allocated array that the callee "stuffs" with battery data.
        The array should be of size MAX_BATTERY_NUM = 2. 
        On return from this function:
            for a 2 battery system (PowerBook G3) both array entries are CFDictionaryRefs
            for a 1 battery system the first array entry is a CFDictionaryRef, the second is NULL
    The contents of each CFDictionaryRef are defined in IOKit.framework/ps/IOPSKeys.h
 Return: TRUE if no errors were encountered, FALSE otherwise.
****/
static bool 	
_IOPMCalculateBatteryInfo(CFArrayRef info, CFDictionaryRef *ret)
{
    CFNumberRef			n, n0, nneg1;
    CFMutableDictionaryRef 	mutDict = NULL;
    int 			i,count;
    int				temp;
    int				minutes;
    int				ehours = 0;
    int				eminutes = 0;
    int				stillCalc = 0;
    int			percentRemaining = 0;
        
    if(info) count = CFArrayGetCount(info);
    else return NULL;
    
    _pluggedIn = FALSE;
    _charging = FALSE;
    
    for (i=0;i<count;i++) {        
        // Grab Flags, Charge, and Capacity from the battery
        CFNumberGetValue(CFDictionaryGetValue(CFArrayGetValueAtIndex((CFArrayRef)info,i),
                CFSTR("Flags")),kCFNumberSInt32Type,&_flags[i]);
        CFNumberGetValue(CFDictionaryGetValue(CFArrayGetValueAtIndex((CFArrayRef)info,i),
                CFSTR("Current")),kCFNumberSInt32Type,&_charge[i]);
        CFNumberGetValue(CFDictionaryGetValue(CFArrayGetValueAtIndex((CFArrayRef)info,i),
                CFSTR("Capacity")),kCFNumberSInt32Type,&_capacity[i]);
        
        // Determine plugged state
        //CFDictionarySetValue(mutDict, CFSTR("Plugged"), (_flags[i]&0x1)?kCFBooleanTrue:kCFBooleanFalse);
        if (_flags[i]&kIOBatteryChargerConnect) {
            _pluggedIn = TRUE;
        }

        // Determine whether charging or not
        //CFDictionarySetValue(mutDict, CFSTR("Charging"), (_flags[i]&0x2)?kCFBooleanTrue:kCFBooleanFalse);
        if (_flags[i]&kIOBatteryCharge) {
            _charging = TRUE;
        }

        // Cap the battery charge to its maximum capacity
        if(_charge[i] > _capacity[i]) _charge[i] = _capacity[i];
    }
    
    // Calculate time remaining per battery
    if((_avgCount % 10) == 0){
        calculateTimeRemaining();
        ehours = (int)(_hoursRemaining[0] + _hoursRemaining[1]);
        eminutes = (int)(60.0*(_hoursRemaining[0] + _hoursRemaining[1] - (double)ehours));
    }
    _avgCount++;
 
    // If power source has changed, reset history count and restart battery sampling
    // triggers stillCalc = 1
    if(_pluggedIn != _lastpluggedIn) {
        _historyCount = 0;
        _historyLimit = 0;
        _lastpluggedIn = _pluggedIn;
    }
    
    // System will sleep soon. Re-calculate time remaining from scratch.
    // Keep counts wired to 0 until _impendingSleep is non-NULL
    // We set _impendingSleep=0 when the system is fully awake.
    if(_impendingSleep) {
        _historyCount = 0;
        _historyLimit = 0;
        _state = -1;
    }

    // Determine if we have a good time remaining calculation yet. If so, set stillCalc = 0
    if(!_historyLimit && (_historyCount < 1)){
        stillCalc = 1;
    }
    
    // is the calculated time valid? (0 <= hours < 10) && (
    if (!((ehours < 10) && (ehours >= 0)) && ((eminutes < 61) && (eminutes >= 0))) {
        stillCalc = 1;
    }
    
    // If we're plugged-in but not charging, we're fully charged and there's nothing
    // to calculate. Set stillCalc to 0.
    if(isACAdapterConnected(i) && !isCharging(i))
    {
        stillCalc = 0;
    }

    // Stuff battery info into CFDictionaries
    for(i=0; i<count; i++) {
        
            // Create the battery info dictionary
            mutDict = NULL;
            mutDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            if(!mutDict) return NULL;
            
            // Set transport type to "Internal"
            CFDictionarySetValue(mutDict, CFSTR(kIOPSTransportTypeKey), CFSTR(kIOPSInternalType));

            // Set Power Source State to AC/Battery
            CFDictionarySetValue(mutDict, CFSTR(kIOPSPowerSourceStateKey), 
                        _pluggedIn ? CFSTR(kIOPSACPowerValue):CFSTR(kIOPSBatteryPowerValue));
            
            // Set maximum capacity
            n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &(_capacity[i]));
            if(n) {
                CFDictionarySetValue(mutDict, CFSTR(kIOPSMaxCapacityKey), n);
                CFRelease(n);
            }
            
            // Set current charge
            n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &(_charge[i]));
            if(n) {
                CFDictionarySetValue(mutDict, CFSTR(kIOPSCurrentCapacityKey), n);
                CFRelease(n);
            }
            
            // Set isPresent flag
            CFDictionarySetValue(mutDict, CFSTR(kIOPSIsPresentKey), 
                        (isBatteryPresent(i)) ? kCFBooleanTrue:kCFBooleanFalse);
            
            // Set isCharging and time remaining
            minutes = (int)(60.0*_hoursRemaining[i]);
            temp = 0;
            n0 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &temp);
            temp = -1;
            nneg1 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &temp);
            if( !isBatteryPresent(i) ) {
                // remaining time calculations only have meaning if the battery is present
                CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanFalse);
                CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
                CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n0);
            } else {
                // A battery is installed
                // Fully charged?
                percentRemaining = (float)( ((float)_charge[i])/((float)_capacity[i]));
                if(stillCalc) {
                    // If we are still calculating then our time remaining
                    // numbers aren't valid yet. Stuff with -1.
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), 
                            _charging ? kCFBooleanTrue : kCFBooleanFalse);
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), nneg1);
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), nneg1);
                } else {   
                    // else there IS a battery installed, and remaining time calculation makes sense
                    if(isCharging(i)) {
                        // Set isCharging to True
                        CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanTrue);
                        n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &minutes);
                        if(n) {
                            CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n);
                            CFRelease(n);
                        }
                        CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n0);
                    } else {
                        // Not Charging
                        // Set isCharging to False
                        CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanFalse);
                        // But are we plugged in?
                        if(isACAdapterConnected(i))
                        {
                            // plugged in but not charging == fully charged
                            CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
                            CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n0);
                        } else {
                            // not charging, not plugged in == discharging
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

    return TRUE;
}

static void	
calculateTimeRemaining(void)
{
    int		cnt;
    float		deltaAvg[2];
    float       avg_sum;

    if (!((_flags[0] & 0x4) == 4)) {
        _charge[0] = 0;
        _batLevel[0] = 0;
    }
    
    if (!((_flags[1] & 0x4) == 4)) {
        _charge[1] = 0;
        _batLevel[1] = 0;
    }
    
    //Put new level into history (both batteries)
    _batteryHistory[0][_historyCount] = _batLevel[0] - _charge[0];
    _batteryHistory[1][_historyCount] = _batLevel[1] - _charge[1];
    
    //Reset the delta values
    deltaAvg[0] = deltaAvg[1] = 0;

    //See if we have hit the historyLimit...basically, we want to make sure
    //the whole array is filled, so we get an accurate avg
    if (_historyLimit) {
        for(cnt = 0;cnt < kBATTERY_HISTORY;cnt++) {
            deltaAvg[0] += _batteryHistory[0][cnt];
            deltaAvg[1] += _batteryHistory[1][cnt];
        }
        
        deltaAvg[0]     = deltaAvg[0]/kBATTERY_HISTORY;
        deltaAvg[1]     = deltaAvg[1]/kBATTERY_HISTORY;
    } else {
        for(cnt = 0;cnt <= _historyCount;cnt++) {
            deltaAvg[0] += _batteryHistory[0][cnt];
            deltaAvg[1] += _batteryHistory[1][cnt];
        }
        
        deltaAvg[0]     = deltaAvg[0]/(_historyCount+1);
        deltaAvg[1]     = deltaAvg[1]/(_historyCount+1);
    }

    //Reset the history counter (loop around in the buffer)
    if (_historyCount >= kBATTERY_HISTORY-1) {
        _historyCount = 0;
        _historyLimit++;
    } else {
        //or just increment
        _historyCount++;
    }
    

    if ((_flags[0] & kIOBatteryChargerConnect) || (_flags[1] & kIOBatteryChargerConnect)) {
        int toCharge[2];
                
        toCharge[0] = (((_flags[0] & 0x4) == 4)?_capacity[0]:0) - _charge[0];
        toCharge[1] = (((_flags[1] & 0x4) == 4)?_capacity[1]:0) - _charge[1];
                
        if (_state != 1) {
            _historyCount     = _historyLimit = 0;
        }
        
        _state = 1;
        
        if (deltaAvg[0] > 0) {	
            deltaAvg[0] = 0;
        }
        
        if (deltaAvg[1] > 0) {	
            deltaAvg[1] = 0;
        }
    
        avg_sum = ((double)deltaAvg[0]+(double)deltaAvg[1]);
        if(avg_sum != 0.0)
        {
            _hoursRemaining[0] = -1*((double)toCharge[0])/avg_sum/360;
            _hoursRemaining[1] = -1*((double)toCharge[1])/avg_sum/360;        
        } else {
            _hoursRemaining[0] = _hoursRemaining[1] = -1.0;        
        }
    } else if (!isBatteryPresent(0) && !isBatteryPresent(1)) {
        //When there are no batteries installed
        //if there were batteries installed before hand, then reset the history (so we get
        //a fresh buffer when a battery is installed)
        if (_state) {
            _historyCount = _historyLimit = 0;
        }
        //_state == 0 means that there are no batteries
        _state = 0;
    } else {
    
        if (deltaAvg[0] < 0) {	
            deltaAvg[0] = 0;
        }
        if (deltaAvg[1] < 0) {	
            deltaAvg[1] = 0;
        }
        
        //if We just switched states (plugged to not), then start the charge array from scratch
        if (_state != 2) {
            _historyCount     = _historyLimit = 0;
        }
        
        _state = 2;
            
        avg_sum = ((double)deltaAvg[0]+(double)deltaAvg[1]);
        if(avg_sum != 0.0)
        {
            _hoursRemaining[0] = ((double)_charge[0])/avg_sum/360;
            _hoursRemaining[1] = ((double)_charge[1])/avg_sum/360;
        } else {
            _hoursRemaining[0] = _hoursRemaining[1] = -1.0;        
        }
    }
    
    _batLevel[0]   = _charge[0];
    _batLevel[1]   = _charge[1];
            
}
