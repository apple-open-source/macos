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
 * 22-Mar-02 ebold created
 * Taken entirely from ryanc and epeyton's work in Battery Monitor MenuExtra
 *
 */
#include <syslog.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFDictionary.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#include "battery.h"

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
    static io_connect_t     _pm_ack_port = 0;

// Defined in PowerManagement/pmconfigd/pmconfigd.c
extern void _pmcfgd_goingToSleep(void);
extern void _pmcfgd_wakeFromSleep(void);

void _pmcfgd_callback(void * port,io_service_t y,natural_t messageType,void * messageArgument)
{    
    switch ( messageType ) {
    case kIOMessageSystemWillSleep:
        // System is going to sleep - reset time remaining calculations.
        // Battery drain during sleep will produce an unrealistic time remaining
        // expectation on wake from sleep unless we reset the average sample.
        _impendingSleep = 1;
        _pmcfgd_goingToSleep();
        // fall through
    case kIOMessageCanSystemSleep:
        IOAllowPowerChange(_pm_ack_port, (long)messageArgument);
        break;
        
    case kIOMessageSystemHasPoweredOn:
        _pmcfgd_wakeFromSleep();
        _impendingSleep = 0;
        break;
    }
}

static void	calculateTimeRemaining(void);

void 	_IOPMCalculateBatterySetup(void)
{
    io_connect_t		pm_tmp;
    io_connect_t		root_port;
    int 			kr;
    CFArrayRef		    	info;
    int				count,i;
    IONotificationPortRef       notify;
    io_object_t                 anIterator;
    
    // Initialize battery history to 3?
    for(count = 0;count < kBATTERY_HISTORY;count++){
        _batteryHistory[0][count] = 3;
        _batteryHistory[1][count] = 3;
    }
     
    _batName[0] = CFSTR("InternalBattery-0");
    _batName[1] = CFSTR("InternalBattery-1");
    
    // Register for SystemPower notifications. We re-calculate time remaining when we detect a system sleep.
    _pm_ack_port = IORegisterForSystemPower (0, &notify, _pmcfgd_callback, &anIterator);
    if ( _pm_ack_port != NULL ) {
        if(notify) CFRunLoopAddSource(CFRunLoopGetCurrent(),
                            IONotificationPortGetRunLoopSource(notify),
                            kCFRunLoopDefaultMode);
    }
    
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
bool 	_IOPMCalculateBatteryInfo(CFArrayRef info, CFDictionaryRef *ret)
{
    CFNumberRef			n, n0, nneg1;
    CFMutableDictionaryRef 	mutDict = NULL;
    int 			i,count;
    int				temp;
    int				minutes;
    int				ehours = 0;
    int				eminutes = 0;
    int				stillCalc = 0;
        
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
    }

    // Determine if we have a good time remaining calculation yet. If so, set stillCalc = 0
    if(!_historyLimit && (_historyCount < 1)){
        stillCalc = 1;
    }
    
    // is the calculated time valid? (0 <= hours < 10) && (
    if (!((ehours < 10) && (ehours >= 0)) && ((eminutes < 61) && (eminutes >= 0))) {
        stillCalc = 1;
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
                        (_flags[i] & kIOBatteryInstalled) ? kCFBooleanTrue:kCFBooleanFalse);
            
            // Set isCharging and time remaining
            minutes = (int)(60.0*_hoursRemaining[i]);
            temp = 0;
            n0 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &temp);
            temp = -1;
            nneg1 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &temp);
            if(!(_flags[i] & kIOBatteryInstalled)) {
                // remaining time calculations only have meaning if the battery is present
                CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanFalse);
                CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
                CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n0);
            } else if(stillCalc) {
                // If we are still calculating then our time remaining
                // numbers aren't valid yet. Stuff with -1.
                CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), 
                        _charging ? kCFBooleanTrue : kCFBooleanFalse);
                CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), nneg1);
                CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), nneg1);
            } else {   
                // else there IS a battery installed, and remaining time calculation makes sense
                if(_charging) {
                    // Set isCharging to True
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanTrue);
                    n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &minutes);
                    if(n) {
                        CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n);
                        CFRelease(n);
                    }
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n0);
                } else {
                    // Set isCharging to False
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSIsChargingKey), kCFBooleanFalse);
                    n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &minutes);
                    if(n) {
                        CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToEmptyKey), n);
                        CFRelease(n);
                    }
                    CFDictionarySetValue(mutDict, CFSTR(kIOPSTimeToFullChargeKey), n0);
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





void	calculateTimeRemaining(void)
{
    int		cnt;
    float		deltaAvg[2];

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
    
    //If these are both zero, discount them!
    if (!_batteryHistory[0][_historyCount] && !_batteryHistory[1][_historyCount] ) {
        return;
    } 
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

    /*
    #warning DEBUG STUFF!!
    {
        //Reset the delta values
        deltaAvg[0] = deltaAvg[1] = 0;

        if (_historyLimit) {
            for(cnt = 0;cnt < kBATTERY_HISTORY;cnt++) {
                deltaAvg[0] += _batteryHistory[0][cnt];
                deltaAvg[1] += _batteryHistory[1][cnt];
                printf("bh[%d] = %f\n",cnt,_batteryHistory[0][cnt]);
            }
        
            deltaAvg[0]     = deltaAvg[0]/kBATTERY_HISTORY;
            deltaAvg[1]     = deltaAvg[1]/kBATTERY_HISTORY;
        } else {
            for(cnt = 0;cnt <= _historyCount;cnt++) {
                deltaAvg[0] += _batteryHistory[0][cnt];
                deltaAvg[1] += _batteryHistory[1][cnt];
                printf("bh[%d] = %f\n",cnt,_batteryHistory[0][cnt]);
            }
            
            deltaAvg[0]     = deltaAvg[0]/(_historyCount+1);
            deltaAvg[1]     = deltaAvg[1]/(_historyCount+1);
        }
        printf("Average = %f\n",deltaAvg[0]);
    }*/
  
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
    
        //_hoursRemaining[0] = -1*((((double)toCharge[0]+(double)toCharge[1]))/((double)deltaAvg[0]+(double)deltaAvg[1]))/360;
        _hoursRemaining[0] = -1*(((double)toCharge[0])/((double)deltaAvg[0]+(double)deltaAvg[1]))/360;
        _hoursRemaining[1] = -1*(((double)toCharge[1])/((double)deltaAvg[0]+(double)deltaAvg[1]))/360;        
    } else if (!(_flags[0] & kIOBatteryInstalled) && !(_flags[1] & kIOBatteryInstalled)) {
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
            
        //_hoursRemaining[0] = ((((double)_charge[0]+(double)_charge[1]))/((double)deltaAvg[0]+(double)deltaAvg[1]))/360;     
        _hoursRemaining[0] = (((double)_charge[0])/((double)deltaAvg[0]+(double)deltaAvg[1]))/360;
        _hoursRemaining[1] = (((double)_charge[1])/((double)deltaAvg[0]+(double)deltaAvg[1]))/360;
    }
    
    _batLevel[0]   = _charge[0];
    _batLevel[1]   = _charge[1];
            
}
