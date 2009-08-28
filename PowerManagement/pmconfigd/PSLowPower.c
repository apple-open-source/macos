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

#include <sys/syslog.h>
#include <syslog.h>

#include "PrivateLib.h"
#include "PSLowPower.h"
#include "PMSettings.h"

// Data structure to track UPS shutdown thresholds
#define     kHaltEnabled        0
#define     kHaltValue          1

#ifndef kIOPSCommandStartupDelayKey
#define kIOPSCommandStartupDelayKey           "Startup Delay"
#endif

typedef struct  {
    int     haltafter[2];
    int     haltremain[2];
    int     haltpercent[2];
} threshold_struct;

// Externally defined UPS SPI
#ifndef _IOKIT_PM_IOUPSPRIVATE_H_
Boolean IOUPSMIGServerIsRunning(mach_port_t * bootstrap_port_ref, mach_port_t * upsd_port_ref);
IOReturn IOUPSSendCommand(mach_port_t connect, int upsID, CFDictionaryRef command);
IOReturn IOUPSGetEvent(mach_port_t connect, int upsID, CFDictionaryRef *event);
IOReturn IOUPSGetCapabilities(mach_port_t connect, int upsID, CFSetRef *capabilities);
#endif

// Globals
static const int                _delayedRemovePowerMinutes = 3;
static const int                _delayBeforeStartupMinutes = 4;
static CFAbsoluteTime           _switchedToUPSPowerTime = 0.0;
static threshold_struct        *_thresh;
#if HAVE_CF_USER_NOTIFICATION
static CFUserNotificationRef    _UPSAlert = NULL;
#endif

// Local functions defined below
static  int         _upsSupports(CFNumberRef whichUPS, CFStringRef  command);
static  int         _upsCommand(CFNumberRef whichUPS, CFStringRef command, int arg);
static  int         _threshEnabled(CFDictionaryRef dynamo);
static  int         _threshValue(CFDictionaryRef dynamo);
static  int         _minutesSpentOnUPSPower(void);
static  int         _secondsSpentOnUPSPower(void);
static  bool        _weManageUPSPower(void);
static  void        _getUPSShutdownThresholdsFromDisk(threshold_struct *thresho);
static  void        _itIsLaterNow(CFRunLoopTimerRef tmr, void *info);
static  void        _reEvaluatePowerSourcesLater(int seconds);
static  void        _doPowerEmergencyShutdown(CFNumberRef ups_id);

enum {
    _kIOUPSInternalPowerBit,
    _kIOUPSExternalPowerBit
};

/* PSLowPowerPrime
 *
 * Init
 */
__private_extern__ void PSLowPower_prime(void)
{
    _thresh = (threshold_struct *)malloc(sizeof(threshold_struct));
    
    _getUPSShutdownThresholdsFromDisk(_thresh);
    return; 
}


/* PSLowPowerPrefsHaveChanged
 *
 * Update UPS shutdown thresholds when preferences change on disk.
 * Must be called after
 */
__private_extern__ void
PSLowPowerPrefsHaveChanged(void) 
{
    if(_thresh)
    {
        _getUPSShutdownThresholdsFromDisk(_thresh);
    }
}


/* PSLowPowerPSChange
 *
 * Is the handler that gets notified when power source (battery or UPS)
 * state changes. We might respond to this by posting a user notification
 * or performing emergency shutdown.
 */
__private_extern__ void
PSLowPowerPSChange(CFTypeRef ps_blob) 
{
    CFTypeRef           ups = NULL;
    CFDictionaryRef     ups_info = 0;
    int                 t1, t2;
    CFNumberRef         n1, n2;
    int                 minutes_remaining;
    int                 percent_remaining;
    CFBooleanRef        isPresent;
    static int          last_ups_power_source = _kIOUPSExternalPowerBit;
    CFStringRef         power_source = 0;
    CFNumberRef         ups_id = 0;
    
    // Bail immediately if another application (like APC's PowerChute) 
    //   is managing emergency UPS shutdown
    if(!_weManageUPSPower()) {
        goto _exit_PowerSourcesHaveChanged_;
    }
    
    // *** Inspect UPS power levels
    // We assume we're only dealing with 1 UPS for simplicity.
    // The "more than one UPS attached " case is undefined.
    if((ups = IOPSGetActiveUPS(ps_blob)))
    {
        ups_info = isA_CFDictionary(IOPSGetPowerSourceDescription(ps_blob, ups));
        if(!ups_info) goto _exit_PowerSourcesHaveChanged_;

        ups_id = isA_CFNumber(CFDictionaryGetValue(ups_info, CFSTR(kIOPSPowerSourceIDKey)));
        if(!ups_id) goto _exit_PowerSourcesHaveChanged_;
        
        // Check UPS "Is Present" key
        isPresent = isA_CFBoolean(CFDictionaryGetValue(ups_info, CFSTR(kIOPSIsPresentKey)));
        if(!isPresent || !CFBooleanGetValue(isPresent))
        {
#if HAVE_CF_USER_NOTIFICATION
            if(_UPSAlert)
            {
                CFUserNotificationCancel(_UPSAlert);
                _UPSAlert = 0;
            }
#endif
            // If UPS isn't active or connected we shouldn't base policy decisions on it
            goto _exit_PowerSourcesHaveChanged_;
        }
        
        // Check Power Source
        power_source = isA_CFString(CFDictionaryGetValue(ups_info, CFSTR(kIOPSPowerSourceStateKey)));
        if(!power_source || !CFEqual(power_source, CFSTR(kIOPSBatteryPowerValue)))
        {
#if HAVE_CF_USER_NOTIFICATION
            // Running off of AC Power
            if(_UPSAlert)
            {
                CFUserNotificationCancel(_UPSAlert);
                _UPSAlert = 0;
            }
#endif                
            // we have to be draining the internal battery to do a shutdown, so we'll just exit from here.
            goto _exit_PowerSourcesHaveChanged_;
        }
        
        // UPS is running off of internal battery power. Show warning if we just switched from AC to battery.
        if(_kIOUPSExternalPowerBit == last_ups_power_source)
        {
            _switchedToUPSPowerTime = CFAbsoluteTimeGetCurrent();
            
            if( _thresh->haltafter[kHaltEnabled] ) {    
                // If there's a "shutdown after X minutes on UPS power" threshold, 
                // set a timer to remind us to check UPS state again after X minutes
                _reEvaluatePowerSourcesLater(5 + (60*_thresh->haltafter[kHaltValue]));
            }
            
#if HAVE_CF_USER_NOTIFICATION
            if(!_UPSAlert) _UPSAlert = _showUPSWarning();
#endif 

        }
        
        // TODO: switch this battery-present check for the IOKit 
        //       private API IOPMSystemSupportsUPSShutdown
        // Is an internal battery present?
        if(kCFBooleanTrue == IOPSPowerSourceSupported(ps_blob, CFSTR(kIOPMBatteryPowerKey)))
        {
            // Do not do UPS shutdown if internal battery is present.
            // Internal battery may still be providing power. 
            // Don't do any further UPS shutdown processing.
            // PMU will cause an emergency sleep when the battery runs out - we fall back on that
            // in the battery case.
            goto _exit_PowerSourcesHaveChanged_;                
        }
        
        // ******
        // ****** Perform emergency shutdown if any of the shutdown thresholds is true
        
        // Check to make sure that the UPS has been on battery power for a full 10 seconds before initiating a shutdown.
        // Certain UPS's have reported transient "on battery power with 0% capacity remaining" states for 3-5 seconds.
        // So we make sure not to heed this shutdown notice unless we've been on battery power for 10 seconds.
        if(_secondsSpentOnUPSPower() < 10) {
            _reEvaluatePowerSourcesLater(10);
            goto _exit_PowerSourcesHaveChanged_;
         }
        
        // Calculate battery percentage remaining
        n1 = isA_CFNumber(CFDictionaryGetValue(ups_info, CFSTR(kIOPSCurrentCapacityKey)));
        n2 = isA_CFNumber(CFDictionaryGetValue(ups_info, CFSTR(kIOPSMaxCapacityKey)));
        if(n1 && n2)
        {
            if( CFNumberGetValue(n1, kCFNumberIntType, &t1) &&
                CFNumberGetValue(n2, kCFNumberIntType, &t2)) {
                percent_remaining = (int)(100.0* ((double)t1) / ((double)t2) );
    
                if( _thresh->haltpercent[kHaltEnabled] ) {
                    if( percent_remaining <= _thresh->haltpercent[kHaltValue] ) {
                        _doPowerEmergencyShutdown(ups_id);
                    }
                }
            }
        }
        
        // Get UPS's estimated time remaining
        n1 = isA_CFNumber(CFDictionaryGetValue(ups_info, CFSTR(kIOPSTimeToEmptyKey)));
        if(n1)
        {
            if(CFNumberGetValue(n1, kCFNumberIntType, &minutes_remaining))
            {
                if( _thresh->haltremain[kHaltEnabled] ) {
                    if( minutes_remaining <= _thresh->haltremain[kHaltValue] ) {
                        _doPowerEmergencyShutdown(ups_id);
                    }
                }
            }
        }

        // Determine how long we've been running on UPS power
        if( _thresh->haltafter[kHaltEnabled] ) {
            if(_minutesSpentOnUPSPower() >= _thresh->haltafter[kHaltValue]) {
                _doPowerEmergencyShutdown(ups_id);
            }
        }
        
    } else {
        // No "active UPS" detected. 
        // One could have just disappeared - if we're showing an alert, clear it.
#if HAVE_CF_USER_NOTIFICATION
        if(_UPSAlert)
        {
            CFUserNotificationCancel(_UPSAlert);
            _UPSAlert = 0;
        }
#endif
    }
    
    // exit point
    _exit_PowerSourcesHaveChanged_:
    
    if(power_source && CFEqual(power_source, CFSTR(kIOPSBatteryPowerValue))) {
        last_ups_power_source = _kIOUPSInternalPowerBit;
    } else {
        last_ups_power_source = _kIOUPSExternalPowerBit;
    }
    
    return;
}
 
/* _doPowerEmergencyShutdown()
 *
 Performs a semi-complicated proecdure to get machines experiencing power failure
 to automatically power up when external power is restored. We do this to trigger
 the onboard "Restart Automatically after a power failure" feature; pulling the power
 from a mostly-shutdown machine simulates a "power failure."
 
 At the time we do the shutdown...
 The UPS has lost external power (presumably due to a power outage) and our backup
 power thresholds have been exceeded and we've decided to do a system shutdown.
 
 1. Request that the UPS remove power from the system in kDelayedRemovePowerMinutes
 2. Set the StallSystemAtHalt root domain property that will cause the machine to hang 
    at shutdown (after the OS has been shutdown).
 3. We run the emergency shutdown script (/usr/libexec/upsshutdown) and shutdown to
    the point of stalling.
 4. OS hangs there at the endpoint, waiting for UPS to remove power to its AC plugs.
 
 Later...
 1. External power is restored to the UPS
 2. The UPS restores power to its outlets, the CPU's "Restart Automatically after a
    power failure" switch is triggered. System boots up and resumes operation.
 *
 */
static void 
_doPowerEmergencyShutdown(CFNumberRef ups_id)
{
    static int      _alreadyShuttingDown = 0;
    CFDictionaryRef _ESSettings = NULL;
    char            *shutdown_argv[2];
    pid_t           shutdown_pid;
    CFNumberRef     auto_restart;
    IOReturn        error;
    bool            upsRestart = false;
    int             restart_setting;
    
    if(_alreadyShuttingDown) return;
    _alreadyShuttingDown = 1;
    
    syslog(LOG_INFO, "Performing emergency UPS low power shutdown now");

    _ESSettings = PMSettings_CopyActivePMSettings();
    if(!_ESSettings) goto shutdown;
    
    auto_restart = isA_CFNumber(CFDictionaryGetValue(_ESSettings, CFSTR(kIOPMRestartOnPowerLossKey)));
    if(auto_restart) {
        CFNumberGetValue(auto_restart, kCFNumberIntType, &restart_setting);
    } else { 
        restart_setting = 0;
    }
    upsRestart = restart_setting ? true:false;
        
    if(!upsRestart)
    {
        // Automatic Restart On Power Loss checkbox is disabled - 
        // just do a plain old shutdown
        // and don't attempt to auto-restart.
        goto shutdown;
    }

    // Does the attached UPS support RemovePowerDelayed?
    if(_upsSupports(ups_id, CFSTR(kIOPSCommandDelayedRemovePowerKey)))
    {
        syslog(LOG_INFO, "System will restart when external power is restored to UPS.");

        error = _upsCommand(ups_id, 
                    CFSTR(kIOPSCommandStartupDelayKey), 
                    _delayBeforeStartupMinutes);

        if(kIOReturnSuccess != error)
        {
            // Attempt to set "startup when power restored" delay failed
            syslog(LOG_INFO, "UPS Emergency shutdown: error 0x%08x requesting UPS startup delay of %d minutes\n", 
                                error, _delayBeforeStartupMinutes);
            goto shutdown;
        }

        error = _upsCommand(ups_id, CFSTR(kIOPSCommandDelayedRemovePowerKey), _delayedRemovePowerMinutes);
        if(kIOReturnSuccess != error)
        {
            // We tried telling the UPS to auto-restart us, but since that's 
            // failing we're just going to do an old school shutdown that 
            // requires human intervention to power on.
            syslog(LOG_INFO, "UPS Emergency shutdown: error 0x%08x communicating shutdown time to UPS\n", error);
            goto shutdown;
        }
    }
    
shutdown:

    if (_ESSettings) {
        CFRelease(_ESSettings);
    }
    
    shutdown_argv[0] = (char *)"/usr/libexec/upsshutdown";

    if(upsRestart) {
        shutdown_argv[1] = (char *)"WaitForUPS";
        shutdown_argv[2] = NULL;
    } else {
        shutdown_argv[1] = NULL; 
    }
    shutdown_pid = _SCDPluginExecCommand(0, 0, 0, 0, 
                        "/usr/libexec/upsshutdown", shutdown_argv);
}


static int
_upsSupports(CFNumberRef whichUPS, CFStringRef  command)
{
#ifdef STANDALONE
    return true;
#else
    mach_port_t                 bootstrap_port = MACH_PORT_NULL;
    mach_port_t                 connect = MACH_PORT_NULL;
    int                         prop_supported;
    CFSetRef                    cap_set;
    int                         _id;
    IOReturn                    ret;

    if (!IOUPSMIGServerIsRunning(&bootstrap_port, &connect))
    {
        return 0;
    }

    if(whichUPS) CFNumberGetValue(whichUPS, kCFNumberIntType, &_id);
    else _id = 0;

    ret = IOUPSGetCapabilities(connect, _id, &cap_set);
    if(kIOReturnSuccess != ret) return false;
    prop_supported = (int)CFSetContainsValue(cap_set, command);
    CFRelease(cap_set);
    return prop_supported;
#endif
}

static IOReturn
_upsCommand(CFNumberRef whichUPS, CFStringRef command, int arg)
{
#ifdef STANDALONE
    return kIOReturnSuccess;
#else
    CFMutableDictionaryRef      command_dict;
    IOReturn                    ret = kIOReturnSuccess;
    mach_port_t                 bootstrap_port = MACH_PORT_NULL;
    mach_port_t                 connect = MACH_PORT_NULL;
    CFNumberRef                 minutes;
    int                         _id;

    if (!IOUPSMIGServerIsRunning(&bootstrap_port, &connect))
    {
        return kIOReturnNoDevice;
    }
    
    if(whichUPS) CFNumberGetValue(whichUPS, kCFNumberIntType, &_id);
    else _id = 0;

    command_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if(!command_dict) return kIOReturnNoMemory;

    minutes = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &arg);
    if(!minutes) return kIOReturnNoMemory;
        
    CFDictionarySetValue(command_dict, command, minutes);
    CFRelease(minutes);
    
    ret = IOUPSSendCommand(connect, _id, command_dict);
    CFRelease(command_dict);

    return ret;
#endif
}

static int
_threshEnabled(CFDictionaryRef dynamo)
{
    return (kCFBooleanTrue == CFDictionaryGetValue(dynamo, CFSTR(kIOUPSShutdownLevelEnabledKey)));
}


static int
_threshValue(CFDictionaryRef dynamo)
{
    int val = 0;
    CFNumberRef     cfnum;
    cfnum = isA_CFNumber(CFDictionaryGetValue(dynamo, CFSTR(kIOUPSShutdownLevelValueKey)));
    if(cfnum)
        CFNumberGetValue(cfnum, kCFNumberIntType, &val);
    return val;
}

/* _weManageUPSPower
 *
 * Determines whether X Power Management should do the emergency shutdown when low on UPS power.
 * OS X should NOT manage low power situations if another third party application has already claimed
 * that emergency shutdown responsibility.
 *
 * Return value:
 * 	true == OS X should manage emergency shutdowns
 *  false == another installed & running application is managing shutdowns
 */
static bool
_weManageUPSPower(void)
{
    static CFStringRef                  ups_claimed = NULL;
    SCDynamicStoreRef                   ds_ref = NULL;
    CFTypeRef		                    temp;
    bool                                ret_val = true;

    if(!ups_claimed) {
        ups_claimed = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@"), kSCDynamicStoreDomainState, CFSTR(kIOPSUPSManagementClaimed));
    }
    
    ds_ref = _getSharedPMDynamicStore();

    // Check for existence of "UPS Management claimed" key in SCDynamicStore
    if( ups_claimed && ds_ref &&
        (temp = isA_CFBoolean(SCDynamicStoreCopyValue(ds_ref, ups_claimed))) ) 
    {
        if(kCFBooleanTrue == temp) ret_val = false;
        CFRelease(temp);
    }
    return ret_val;
}


static void
_getUPSShutdownThresholdsFromDisk(threshold_struct *thresho)
{
    CFDictionaryRef     happytown = IOPMCopyUPSShutdownLevels(CFSTR(kIOPMDefaultUPSThresholds));
    CFDictionaryRef     d;
    
    if(!thresho) goto exit;

    // by default, all 3 shutdown thresholds are "disabled"
    thresho->haltafter[kHaltEnabled] = 0;
    thresho->haltremain[kHaltEnabled] = 0;
    thresho->haltpercent[kHaltEnabled] = 0;

    if(!isA_CFDictionary(happytown)) goto exit;

    if((d = isA_CFDictionary(CFDictionaryGetValue(happytown, 
                            CFSTR(kIOUPSShutdownAtLevelKey)))))
    {
        thresho->haltpercent[kHaltEnabled] = _threshEnabled(d);
        thresho->haltpercent[kHaltValue] = _threshValue(d);    
    }
    if((d = isA_CFDictionary(CFDictionaryGetValue(happytown, 
                            CFSTR(kIOUPSShutdownAfterMinutesOn)))))
    {
        thresho->haltafter[kHaltEnabled] = _threshEnabled(d);
        thresho->haltafter[kHaltValue] = _threshValue(d);    
    }
    if((d = isA_CFDictionary(CFDictionaryGetValue(happytown, 
                            CFSTR(kIOUPSShutdownAtMinutesLeft)))))
    {
        thresho->haltremain[kHaltEnabled] = _threshEnabled(d);
        thresho->haltremain[kHaltValue] = _threshValue(d);    
    }


exit:
    if(happytown) CFRelease(happytown);
    return;    
}


static void
_itIsLaterNow(CFRunLoopTimerRef tmr, void *info)
{
    CFTypeRef snap = IOPSCopyPowerSourcesInfo();
    PSLowPowerPSChange(snap);
    CFRelease(snap);
    return;
}


static void
_reEvaluatePowerSourcesLater(int seconds)
{
    CFRunLoopTimerRef       later_tmr;
    CFAbsoluteTime          when;
    
    when = CFAbsoluteTimeGetCurrent() + (CFTimeInterval)seconds;
    
    later_tmr = CFRunLoopTimerCreate(kCFAllocatorDefault,
        when,   // fire date
        0.0,    // interval
        0,      // options
        0,      // order
        &_itIsLaterNow,     // callout
        0);     // callout
    
    if(later_tmr) {
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), later_tmr, kCFRunLoopDefaultMode);
        CFRelease(later_tmr);
    }
    return;
}


static int
_minutesSpentOnUPSPower(void)
{
    CFAbsoluteTime         now = CFAbsoluteTimeGetCurrent();
    return (int)((now - _switchedToUPSPowerTime)/60);
}


static int
_secondsSpentOnUPSPower(void)
{
    CFAbsoluteTime         now = CFAbsoluteTimeGetCurrent();
    return (int)((now - _switchedToUPSPowerTime));
}



