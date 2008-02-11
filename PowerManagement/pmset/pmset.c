/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2001 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 18-Dec-01 ebold created
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFDateFormatter.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>

#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMUPSPrivate.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <IOKit/IOCFSerialize.h>

// dynamically mig generated
#include "powermanagement.h"


#include <servers/bootstrap.h>
#include <mach/mach_port.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <notify.h>

/* 
 * This is the command line interface to Energy Saver Preferences in
 * /Library/Preferences/SystemConfiguration/com.apple.PowerManagement.plist
 *
 * Courtesy of Soren Spies:
 * usage as of Mon Oct 29, 2001  06:19:43 PM
 Usage: pmset [-b | -c | -a] <action> <minutes> [[<opts>] <action> <minutes> ...]
       -c adjust settings used while connected to a charger
       -b adjust settings used when running off a battery
       -a (default) adjust settings for both
       <action> is one of: dim, sleep, spindown, slower, womp* (* flag = 1/0)
       eg. pmset womp 1 -c dim 5 sleep 15 -b dim 3 spindown 5 sleep 8
 *
 *
 *
 * usage as of Tues Oct 30, 2001, 09:23:34 PM
 Usage:  pmset [-b | -c | -a] <action> <minutes> [[<opts>] <action> <minutes>...]
                -c adjust settings used while connected to a charger
                -b adjust settings used when running off a battery 
                -a (default) adjust settings for both
                <action> is one of: dim, sleep, spindown, slower
                eg. pmset -c dim 5 sleep 15 -b dim 3 spindown 5 sleep 8
         pmset <setting> <time> [<setting> <time>]
                <setting> is one of: wakeat, sleepat
                <time> is in 24 hour format
         pmset womp [ on | off ]
 *
 */

// Settings options
#define ARG_DIM             "dim"
#define ARG_DISPLAYSLEEP    "displaysleep"
#define ARG_SLEEP           "sleep"
#define ARG_SPINDOWN        "spindown"
#define ARG_DISKSLEEP       "disksleep"
#define ARG_REDUCE          "slower"
#define ARG_WOMP            "womp"
#define ARG_POWERBUTTON     "powerbutton"
#define ARG_LIDWAKE         "lidwake"


#define ARG_REDUCE2         "reduce"
#define ARG_DPS             "dps"
#define ARG_RING            "ring"
#define ARG_AUTORESTART     "autorestart"
#define ARG_WAKEONACCHANGE  "acwake"
#define ARG_REDUCEBRIGHT    "lessbright"
#define ARG_SLEEPUSESDIM    "halfdim"
#define ARG_MOTIONSENSOR    "sms"
#define ARG_MOTIONSENSOR2   "ams"
#define ARG_TTYKEEPAWAKE    "ttyskeepawake"

// Scheduling options
#define ARG_SCHEDULE        "schedule"
#define ARG_SCHED           "sched"
#define ARG_REPEAT          "repeat"
#define ARG_CANCEL          "cancel"
//#define ARG_SLEEP         "sleep"
#define ARG_SHUTDOWN        "shutdown"
#define ARG_RESTART         "restart"
#define ARG_WAKE            "wake"
#define ARG_POWERON         "poweron"
#define ARG_WAKEORPOWERON   "wakeorpoweron"

// UPS options
#define ARG_HALTLEVEL       "haltlevel"
#define ARG_HALTAFTER       "haltafter"
#define ARG_HALTREMAIN      "haltremain"

// get options
#define ARG_CAP             "cap"
#define ARG_DISK            "disk"
#define ARG_CUSTOM          "custom"
#define ARG_LIVE            "live"
#define ARG_SCHED           "sched"
#define ARG_UPS             "ups"
#define ARG_SYS_PROFILES    "profiles"
#define ARG_BATT            "batt"
#define ARG_PS              "ps"
#define ARG_PSLOG           "pslog"
#define ARG_PSRAW           "rawlog"
#define ARG_THERMLOG        "thermlog"

// special
#define ARG_BOOT            "boot"
#define ARG_UNBOOT          "unboot"
#define ARG_FORCE           "force"
#define ARG_TOUCH           "touch"
#define ARG_NOIDLE          "noidle"
#define ARG_SLEEPNOW        "sleepnow"

// special system
#define ARG_DISABLESLEEP    "disablesleep"

// return values for parseArgs
#define kParseSuccess                   0       // success
#define kParseBadArgs                   -1      // error
#define kParseInternalError             -2      // error

// bitfield for tracking what's been modified in parseArgs()
#define kModSettings                    (1<<0)
#define kModProfiles                    (1<<1)
#define kModUPSThresholds               (1<<2)
#define kModSched                       (1<<3)
#define kModRepeat                      (1<<4)
#define kModSystemSettings              (1<<5)

// return values for idleSettingsNotConsistent
#define kInconsistentDisplaySetting     1
#define kInconsistentDiskSetting        2
#define kConsistentSleepSettings        0

// day-of-week constants for repeating power events
#define daily_mask      ( kIOPMMonday | kIOPMTuesday | kIOPMWednesday \
                        | kIOPMThursday | kIOPMFriday | kIOPMSaturday \
                        | kIOPMSunday)
#define weekday_mask    ( kIOPMMonday | kIOPMTuesday | kIOPMWednesday \
                        | kIOPMThursday | kIOPMFriday )
#define weekend_mask    ( kIOPMSaturday | kIOPMSunday )

#define kDateAndTimeFormat      "MM/dd/yy HH:mm:ss"
#define kTimeFormat             "HH:mm:ss"

#define kNUM_PM_FEATURES    14
/* list of all features */
char    *all_features[kNUM_PM_FEATURES] =
{ 
    kIOPMDisplaySleepKey, 
    kIOPMDiskSleepKey, 
    kIOPMSystemSleepKey, 
    kIOPMReduceSpeedKey, 
    kIOPMDynamicPowerStepKey, 
    kIOPMWakeOnLANKey, 
    kIOPMWakeOnRingKey, 
    kIOPMWakeOnACChangeKey,
    kIOPMRestartOnPowerLossKey,
    kIOPMSleepOnPowerButtonKey,
    kIOPMWakeOnClamshellKey,
    kIOPMReduceBrightnessKey,
    kIOPMDisplaySleepUsesDimKey,
    kIOPMMobileMotionModuleKey
};

enum ArgumentType {
    ApplyToBattery = 1,
    ApplyToCharger = 2,
    ApplyToUPS     = 4
};

enum AssertionBitField {
    kAssertionCPU = 1,
    kAssertionInflow = 2,
    kAssertionCharge = 4,
    kAssertionIdle = 8
};

// ack port for sleep/wake callback
static io_connect_t gPMAckPort = MACH_PORT_NULL;

// thermal notification mach ports
static CFMachPortRef   gPowerConstraintMachPort = NULL;
static CFMachPortRef   gCPUPowerMachPort = NULL;    


enum SleepCallbackBehavior {
    kLogSleepEvents = (1<<0),
    kCancelSleepEvents = (1<<1)
};

/* pmset commands */
enum PMCommandType {
    kPMCommandSleepNow = 1,
    kPMCommandTouch,
    kPMCommandNoIdle
};

/* check and set int value multiplier */
enum {
    kNoMultiplier = 0,
    kMillisecondsMultiplier = 1000
};

typedef struct {
    CFStringRef         who;
    CFDateRef           when;
    CFStringRef         which;
} ScheduledEventReturnType;


// function declarations
static void usage(void);
static IOReturn setRootDomainProperty(CFStringRef key, CFTypeRef val);
static IOReturn _pm_connect(mach_port_t *newConnection);
static IOReturn _pm_disconnect(mach_port_t connection);

static void show_pm_settings_dict(
        CFDictionaryRef d, 
        int indent, 
        bool log_overrides,
        bool prune_unsupported);
static void show_system_power_settings(void);
static void show_supported_pm_features(void);
static void show_custom_pm_settings(void);
static void show_live_pm_settings(void);
static void show_ups_settings(void);
static void show_active_profiles(void);
static void show_system_profiles(void);
static void show_scheduled_events(void);
static void show_active_assertions(uint32_t which);
static void show_power_sources(int which);
static bool prevent_idle_sleep(void);

static void print_pretty_date(bool newline);
static void sleepWakeCallback(
        void *refcon, 
        io_service_t y __unused,
        natural_t messageType,
        void * messageArgument);

static void log_ps_change_handler(void *);
static int log_power_source_changes(int which);
static int log_raw_power_source_changes(void);
static void log_raw_battery_interest(
        void *refcon, 
        io_service_t batt, 
        natural_t messageType, 
        void *messageArgument);
static void log_raw_battery_match(
        void *refcon, 
        io_iterator_t b_iter);

static void log_thermal_events(void);        
static void log_thermal_callback(
        CFMachPortRef port, 
        void *msg,
        CFIndex size,
        void *info);
static void show_thermal_warning_level(void);
static void show_thermal_cpu_power_level(void);

static void print_raw_battery_state(io_registry_entry_t b_reg);
static void print_setting_value(CFTypeRef a, int divider);
static void print_override_pids(CFStringRef assertion_type);
static void print_time_of_day_to_buf(int m, char *buf);
static void print_days_to_buf(int d, char *buf);
static void print_repeating_report(CFDictionaryRef repeat);
static void print_scheduled_report(CFArrayRef events);

static CFDictionaryRef getPowerEvent(int type, CFDictionaryRef events);
static int getRepeatingDictionaryMinutes(CFDictionaryRef event);
static int getRepeatingDictionaryDayMask(CFDictionaryRef event);
static CFStringRef getRepeatingDictionaryType(CFDictionaryRef event);
static int arePowerSourceSettingsInconsistent(CFDictionaryRef set);
static void checkSettingConsistency(CFDictionaryRef profiles);
static ScheduledEventReturnType *scheduled_event_struct_create(void);
static void scheduled_event_struct_destroy(ScheduledEventReturnType *);
static int checkAndSetIntValue(
        char *valstr, 
        CFStringRef settingKey, 
        int apply,
        int isOnOffSetting, 
        int multiplier,
        CFMutableDictionaryRef ac, 
        CFMutableDictionaryRef batt, 
        CFMutableDictionaryRef ups);
static int setUPSValue(
        char *valstr, 
        CFStringRef    whichUPS, 
        CFStringRef settingKey, 
        int apply, 
        CFMutableDictionaryRef thresholds);
static int parseScheduledEvent(
        char                        **argv,
        int                         *num_args_parsed,
        ScheduledEventReturnType    *local_scheduled_event, 
        bool                        *cancel_scheduled_event);
static int parseRepeatingEvent(
        char                        **argv,
        int                         *num_args_parsed,
        CFMutableDictionaryRef      local_repeating_event, 
        bool                        *local_cancel_repeating);
static int parseArgs(
        int argc, 
        char* argv[], 
        CFDictionaryRef             *settings,
        int                         *modified_power_sources,
        bool                        *force_activate_settings,
        CFDictionaryRef             *active_profiles,
        CFDictionaryRef             *system_power_settings,
        CFDictionaryRef             *ups_thresholds,
        ScheduledEventReturnType    **scheduled_event,
        bool                        *cancel_scheduled_event,
        CFDictionaryRef             *repeating_event,
        bool                        *cancel_repeating_event,
        uint32_t                    *pmCmd);

//****************************
//****************************
//****************************

static void usage(void)
{
    printf("Usage:  pmset [-b | -c | -u | -a] <action> <minutes> [<action> <minutes>...]\n");
    printf("        pmset -g [disk | cap | live | sched | ups | batt]\n");
    printf("           -c adjust settings used while connected to a charger\n");
    printf("           -b adjust settings used when running off a battery\n");
    printf("           -u adjust settings used while running off a UPS\n");
    printf("           -a (default) adjust settings for both\n");
    printf("        <action> is one of: displaysleep, sleep, disksleep (minutes argument)\n");
    printf("           or: reduce, dps, womp, ring, autorestart, powerbutton, halfdim,\n");
    printf("                lidwake, acwake, lessbright (with a 1 or 0 argument)\n");
    printf("               or for UPS only: haltlevel (with a percentage argument)\n");
    printf("                    haltafter, haltremain (with a minutes argument)\n");
    printf("           eg. pmset -c dim 5 sleep 15 spindown 10 autorestart 1 womp 1\n");
    printf("        pmset schedule [cancel] <type> <date/time> [owner]\n");
    printf("        pmset repeat cancel\n");    
    printf("        pmset repeat <type> <days of week> <time> \n");
    printf("          <type> is one of: sleep, wake, poweron, shutdown, wakeorpoweron\n");
    printf("          <date/time> is in \"%s\" format\n", kDateAndTimeFormat);
    printf("          <time> is in \"%s\" format\n", kTimeFormat);
    printf("          <days of week> is a subset of MTWRFSU\n");
    printf("          [owner] optionally describes the event creator\n");
}



int main(int argc, char *argv[]) {
    IOReturn                        ret, ret1, err;
    io_connect_t                    fb;
    CFDictionaryRef                 es_custom_settings = 0;
    int                             modified_power_sources = 0;
    bool                            force_it = 0;
    CFDictionaryRef                 ups_thresholds = 0;
    CFDictionaryRef                 system_power_settings = 0;
    CFDictionaryRef                 active_profiles = 0;
    ScheduledEventReturnType        *scheduled_event_return = 0;
    bool                            cancel_scheduled_event = 0;
    CFDictionaryRef                 repeating_event_return = 0;
    bool                            cancel_repeating_event = 0;
    uint32_t                        pmCommand = 0;
        
    ret = parseArgs(argc, argv, 
        &es_custom_settings, &modified_power_sources, &force_it,
        &active_profiles,
        &system_power_settings,
        &ups_thresholds,
        &scheduled_event_return, &cancel_scheduled_event,
        &repeating_event_return, &cancel_repeating_event,
        &pmCommand);

    if(ret == kParseBadArgs)
    {
        //printf("pmset: error in parseArgs!\n");
        usage();
        exit(1);
    }
    
    if(ret == kParseInternalError)
    {
        fprintf(stderr, "%s: internal error!\n", argv[0]); fflush(stdout);    
        exit(1);
    }

    switch ( pmCommand )    
    {
        /*************** Sleep now *************************************/
        case kPMCommandSleepNow:            
            fb = IOPMFindPowerManagement(MACH_PORT_NULL);
            if ( MACH_PORT_NULL != fb ) {
                err = IOPMSleepSystem ( fb );
            }
            
            if( kIOReturnNotPrivileged == err )
            {
                printf("Sleep error 0x%08x; You must run this as root.\n", err);
            } else if ( (MACH_PORT_NULL == fb) || (kIOReturnSuccess != err) )
            {
                printf("Unable to sleep system: error 0x%08x\n", err);
            } else {
                printf("Sleeping now...\n");
            }
            
            return 0;

        /*************** Touch settings *************************************/
        case kPMCommandTouch:
            printf("touching prefs file on disk...\n");

            ret = IOPMSetPMPreferences(NULL);
            if(kIOReturnSuccess != ret)
            {
                printf("\'%s\' must be run as root...\n", argv[0]);
            }

            return 0;

        /*************** Prevent idle sleep **********************************/
        case kPMCommandNoIdle:
            if(!prevent_idle_sleep())
            {
                printf("Error preventing idle sleep\n");
            }
            return 1;

        default:
            // If no command is specified, execution continues with processing
            // other command-line arguments
            break;
    }
    
    if(force_it && es_custom_settings)
    {

        mach_port_t             pm_server = MACH_PORT_NULL;
        CFDataRef               settings_data;
        int32_t                 return_code;
        kern_return_t           kern_result;
        
        settings_data = IOCFSerialize(es_custom_settings, 0);

        if(!settings_data) return 1;

        err = _pm_connect(&pm_server);
        if(kIOReturnSuccess != err) {
            return 7;
        }
    
        // mig forced settings over to configd
        kern_result = io_pm_force_active_settings(
                pm_server, 
                (vm_offset_t)CFDataGetBytePtr(settings_data),
                CFDataGetLength(settings_data),
                &return_code);
    
        if( KERN_SUCCESS != kern_result )
        {
            printf("exit kern_result = 0x%08x\n", kern_result);
            return 7;        
        }
        if( kIOReturnSuccess != return_code )
        {
            printf("exit return_code = 0x%08x\n", return_code);
            return 7;        
        }

        _pm_disconnect(pm_server);

        // explicitly return here. If this is a 'force' call we do _not_ want
        // to attempt to write any settings to the disk, or to try anything 
        // else at all, as our environment may be unwritable / diskless
        // (booted from install CD)
        return 0;
    }

    if(es_custom_settings)
    {
        CFNumberRef             neg1 = NULL;
        int                     tmp_int = -1;
        CFDictionaryRef         tmp_dict = NULL;
        CFMutableDictionaryRef  customize_active_profiles = NULL;
    
        // Send pmset changes out to disk
        if(kIOReturnSuccess != (ret1 = IOPMSetPMPreferences(es_custom_settings)))
        {
            if(ret1 == kIOReturnNotPrivileged) 
            {
                printf("\'%s\' must be run as root...\n", argv[0]);
            } else {
                printf("Error 0x%08x writing Energy Saver preferences to disk\n", ret1);
            }
            exit(1);
        }
        
        // Also need to change the active profile to -1 (Custom)
        // We assume the user intended to activate these new custom settings.
        neg1 = CFNumberCreate(0, kCFNumberIntType, &tmp_int);
        tmp_dict = IOPMCopyActivePowerProfiles();
        if(!tmp_dict) {
            printf("Custom profile set; unable to update active profile to -1.\n");
            exit(1);
        }
        customize_active_profiles = CFDictionaryCreateMutableCopy(0, 0, tmp_dict);        
        if(!customize_active_profiles) {
            printf("Internal error\n");
            exit(1);
        }
        // For each power source we modified settings for, flip that 
        // source's profile to -1
        if(modified_power_sources & ApplyToCharger) {
            CFDictionarySetValue( customize_active_profiles, 
                                    CFSTR(kIOPMACPowerKey), neg1);
        }
        if(modified_power_sources & ApplyToBattery) {
            CFDictionarySetValue( customize_active_profiles, 
                                    CFSTR(kIOPMBatteryPowerKey), neg1);
        }
        if(modified_power_sources & ApplyToUPS) {
            CFDictionarySetValue( customize_active_profiles, 
                                    CFSTR(kIOPMUPSPowerKey), neg1);
        }
            
        ret = IOPMSetActivePowerProfiles(customize_active_profiles);
        if(kIOReturnSuccess != ret) {
            printf("Error 0x%08x writing customized power profiles to disk\n", ret);
            exit(1);
        }
        CFRelease(neg1);
        CFRelease(tmp_dict);
        CFRelease(customize_active_profiles);
        
                
        // Print a warning to stderr if idle sleep settings won't 
        // produce expected result; i.e. sleep < (display | dim)
        checkSettingConsistency(es_custom_settings);
        CFRelease(es_custom_settings);
    }
    
    if(active_profiles)
    {
        ret = IOPMSetActivePowerProfiles(active_profiles);
        if(kIOReturnSuccess != ret) {
            printf("Error 0x%08x writing active power profiles to disk\n", ret);
            exit(1);
        }
    
        CFRelease(active_profiles);    
    }
    
    if(system_power_settings)
    {
        int             iii;
        int             sys_setting_count = 0;
        
        if( isA_CFDictionary(system_power_settings)) {
            sys_setting_count = CFDictionaryGetCount(system_power_settings);
        }
 
        if (0 != sys_setting_count)
        {
            CFStringRef     *keys = malloc(sizeof(CFStringRef) * sys_setting_count);
            CFTypeRef       *vals = malloc(sizeof(CFTypeRef) * sys_setting_count);
        
            if(keys && vals)
            {
                CFDictionaryGetKeysAndValues( system_power_settings, 
                                          (const void **)keys, (const void **)vals);
            
                for(iii=0; iii<sys_setting_count; iii++)
                {
                    // We write the settings to disk here; PM configd picks them up and
                    // sends them to xnu from configd space.
                    IOPMSetSystemPowerSetting(keys[iii], vals[iii]);            
                }        
            }
        
            if (keys) free (keys);
            if (vals) free (vals);
        }
        
        CFRelease(system_power_settings);
    }

    // Did the user change UPS settings too?
    if(ups_thresholds)
    {
        // Only write out UPS settings if thresholds were changed. 
        //      * UPS sleep timers & energy settings have already been 
        //        written with IOPMSetPMPreferences() regardless.
        ret1 = IOPMSetUPSShutdownLevels(
                            CFSTR(kIOPMDefaultUPSThresholds), 
                            ups_thresholds);
        if(kIOReturnSuccess != ret1)
        {
            if(ret1 == kIOReturnNotPrivileged)
                printf("\'%s\' must be run as root...\n", argv[0]);
            if(ret1 == kIOReturnError
                || ret1 == kIOReturnBadArgument)
                printf("Error writing UPS preferences to disk\n");
            exit(1);
        }
        CFRelease(ups_thresholds);
    }
    
    
    if(scheduled_event_return)
    {
        if(cancel_scheduled_event)
        {
            // cancel the event described by scheduled_event_return
            ret = IOPMCancelScheduledPowerEvent(
                    scheduled_event_return->when,
                    scheduled_event_return->who,
                    scheduled_event_return->which);
        } else {
            ret = IOPMSchedulePowerEvent(
                    scheduled_event_return->when,
                    scheduled_event_return->who,
                    scheduled_event_return->which);
        }

        if(kIOReturnNotPrivileged == ret) {
            fprintf(stderr, "%s: This operation must be run as root\n", argv[0]);
            fflush(stderr);
            exit(1);
        }
        if(kIOReturnSuccess != ret) {
            fprintf(stderr, "%s: Error in scheduling operation\n", argv[0]);
            fflush(stderr);
            exit(1);
        }
        
        // free individual members
        scheduled_event_struct_destroy(scheduled_event_return);
    }
    
    if(cancel_repeating_event) 
    {
        ret = IOPMCancelAllRepeatingPowerEvents();
        if(kIOReturnSuccess != ret) {
            if(kIOReturnNotPrivileged == ret) {
                fprintf(stderr, "pmset: Must be run as root to modify settings\n");
            } else {
                fprintf(stderr, "pmset: Error 0x%08x cancelling repeating events\n", ret);
            }
            fflush(stderr);
            exit(1);
        }
    }
    
    if(repeating_event_return)
    {
        ret = IOPMScheduleRepeatingPowerEvent(repeating_event_return);
        if(kIOReturnSuccess != ret) {
            if(kIOReturnNotPrivileged == ret) {
                fprintf(stderr, "pmset: Must be run as root to modify settings\n");
            } else {
                fprintf(stderr, "pmset: Error 0x%08x scheduling repeating events\n", ret);
            }
            fflush(stderr);
            exit(1);
        }
        CFRelease(repeating_event_return);
    }
    
    
    return 0;
}

//****************************
//****************************
//****************************


static IOReturn setRootDomainProperty(CFStringRef key, CFTypeRef val) 
{
    io_registry_entry_t         root_domain;
    IOReturn                    ret;
    
    root_domain = (io_registry_entry_t)IOServiceGetMatchingService(
                    MACH_PORT_NULL, IOServiceNameMatching("IOPMrootDomain"));
    if(!root_domain) return kIOReturnError;
    
    ret = IORegistryEntrySetCFProperty(root_domain, key, val);
    
    IOObjectRelease(root_domain);
    return ret;
}



static void print_setting_value(CFTypeRef a, int divider)
{
    int n;
    
    if(isA_CFNumber(a))
    {
        CFNumberGetValue(a, kCFNumberIntType, (void *)&n);

        if( 0 != divider ) n/=divider;

        printf("%d", n);    
    } else if(isA_CFBoolean(a))
    {
        printf("%d", CFBooleanGetValue(a));
    } else if(isA_CFString(a))
    {
        char buf[100];
        if(CFStringGetCString(a, buf, 100, kCFStringEncodingUTF8))
        {
            printf("%s", buf);
        }    
    } else printf("oops - print_setting_value unknown data type\n");
}

static void show_pm_settings_dict(
    CFDictionaryRef d, 
    int indent, 
    bool show_overrides,
    bool prune_unsupported)
{
    int                     count;
    int                     i;
    int                     j;
    int                     divider = 0;
    char                    *ps;
    CFStringRef             *keys;
    CFTypeRef               *vals;
    CFTypeRef               ps_blob;
    CFStringRef             activeps = NULL;
    CFStringRef             show_override_type = NULL;
    
    ps_blob = IOPSCopyPowerSourcesInfo();
    if(ps_blob) {
        activeps = IOPSGetProvidingPowerSourceType(ps_blob);
    }
    if(!activeps) activeps = CFSTR(kIOPMACPowerKey);
    if(activeps) CFRetain(activeps);

    count = CFDictionaryGetCount(d);
    keys = (CFStringRef *)malloc(count * sizeof(void *));
    vals = (CFTypeRef *)malloc(count * sizeof(void *));
    if(!keys || !vals) return;
    CFDictionaryGetKeysAndValues(d, (const void **)keys, (const void **)vals);

    for(i=0; i<count; i++)
    {
        show_override_type = NULL;
        ps = (char *)CFStringGetCStringPtr(keys[i], 0);
        if(!ps) continue; // with for loop
                
        if( prune_unsupported 
            && !IOPMFeatureIsAvailable(keys[i], CFSTR(kIOPMACPowerKey)) )
        {
            // unsupported setting for the current power source!
            // do not show it!
            continue;
        }
        
        for(j=0; j<indent;j++) printf(" ");

        divider = 0;

        if (strcmp(ps, kIOPMDisplaySleepKey) == 0) {
                printf(" displaysleep\t");  
                if(show_overrides) 
                    show_override_type = kIOPMAssertionTypeNoDisplaySleep;
        } else if (strcmp(ps, kIOPMDiskSleepKey) == 0)
                printf(" disksleep\t");  
        else if (strcmp(ps, kIOPMSystemSleepKey) == 0) {
                printf(" sleep\t\t");
                if(show_overrides) 
                    show_override_type = kIOPMAssertionTypeNoIdleSleep;
        } else if (strcmp(ps, kIOPMWakeOnLANKey) == 0)
                printf(" womp\t\t");  
        else if (strcmp(ps, kIOPMWakeOnRingKey) == 0)
                printf(" ring\t\t");  
        else if (strcmp(ps, kIOPMRestartOnPowerLossKey) == 0)
                printf(" autorestart\t");  
        else if (strcmp(ps, kIOPMReduceSpeedKey) == 0) {
                printf(" reduce\t\t");
                if(show_overrides) 
                    show_override_type = kIOPMCPUBoundAssertion;
        } else if (strcmp(ps, kIOPMDynamicPowerStepKey) == 0) {
                printf(" dps\t\t");  
                if(show_overrides) 
                    show_override_type = kIOPMCPUBoundAssertion;
        } else if (strcmp(ps, kIOPMSleepOnPowerButtonKey) == 0)
                printf(" powerbutton\t");
        else if (strcmp(ps, kIOPMWakeOnClamshellKey) == 0)
                printf(" lidwake\t");
        else if (strcmp(ps, kIOPMWakeOnACChangeKey) == 0)
                printf(" acwake\t\t");
        else if (strcmp(ps, kIOPMReduceBrightnessKey) == 0)
                printf(" %s\t", ARG_REDUCEBRIGHT);
        else if (strcmp(ps, kIOPMDisplaySleepUsesDimKey) == 0)
                printf(" %s\t", ARG_SLEEPUSESDIM);
        else if (strcmp(ps, kIOPMMobileMotionModuleKey) == 0)
                printf(" %s\t\t", ARG_MOTIONSENSOR);
        else if (strcmp(ps, kIOPMTTYSPreventSleepKey) == 0)
                printf(" %s\t", ARG_TTYKEEPAWAKE);
        else {
                // unknown setting
                printf("%s\t", ps);
        }
  
        print_setting_value(vals[i], divider);  

        if(show_override_type) print_override_pids(show_override_type);

        printf("\n");
    }
    
    if (ps_blob) CFRelease(ps_blob);
    if (activeps) CFRelease(activeps);
    free(keys);
    free(vals);
}

static void show_system_power_settings(void)
{
    CFDictionaryRef     system_power_settings = NULL;
    CFBooleanRef        b;
    
    system_power_settings = IOPMCopySystemPowerSettings();
    if(!isA_CFDictionary(system_power_settings)) {
        return;
    }

    printf("System-wide power settings:\n");

    if((b = CFDictionaryGetValue(system_power_settings, kIOPMSleepDisabledKey)))
    {
        printf(" SleepDisabled\t\t%d\n", (b==kCFBooleanTrue) ? 1:0);
    }
    CFRelease(system_power_settings);
}

static void print_override_pids(CFStringRef assertion_type)
{
    CFDictionaryRef         assertions_state = NULL;
    CFDictionaryRef         assertions_pids = NULL;
    CFNumberRef             assertion_value;
    int                     cpus_forced = 0;
    IOReturn                ret;
    CFNumberRef             *pids = NULL;
    CFArrayRef              *assertions = NULL;
    int                     process_count;
    int                     i;
    char                    display_string[255] = "\0";
    char                    pid_buf[10] = "\0";
    int                     length = 0;
    int                     this_is_the_first = 1;
    
    // Determine if the CPU bound assertion is asserted at all.
    ret = IOPMCopyAssertionsStatus(&assertions_state);
    if( (kIOReturnSuccess != ret) || !isA_CFDictionary(assertions_state)) {
        goto bail;
    }
    assertion_value = CFDictionaryGetValue(assertions_state, assertion_type);
    if(assertion_value) CFNumberGetValue(assertion_value, kCFNumberIntType, &cpus_forced);
    // And exit if the assertion isn't forced:
    if(!cpus_forced) goto bail;

    // Find out which pids have asserted this and print 'em out
    // We conclude that at least one pid is forcing it if the assertion is enabled.
    ret = IOPMCopyAssertionsByProcess(&assertions_pids);
    if( (kIOReturnSuccess != ret) || !isA_CFDictionary(assertions_pids) ) {
        goto bail;    
    }
    
    sprintf(display_string, " (imposed by ");
    length = strlen(display_string);

    process_count = CFDictionaryGetCount(assertions_pids);
    pids = malloc(sizeof(CFNumberRef)*process_count);
    assertions = malloc(sizeof(CFArrayRef *)*process_count);
    CFDictionaryGetKeysAndValues(assertions_pids, 
                        (const void **)pids, 
                        (const void **)assertions);
    for(i=0; i<process_count; i++)
    {
        int         the_pid, j;        
        CFNumberGetValue(pids[i], kCFNumberIntType, &the_pid);
        
        for(j=0; j<CFArrayGetCount(assertions[i]); j++)
        {
            CFDictionaryRef     tmp_dict;
            CFStringRef         tmp_type;
            CFNumberRef         tmp_val;
            int                 val = 0;
            
            tmp_dict = CFArrayGetValueAtIndex(assertions[i], j);
            if(!tmp_dict || (kCFBooleanFalse == (CFBooleanRef)tmp_dict)) {
                continue;
            }
            tmp_type = CFDictionaryGetValue(tmp_dict, kIOPMAssertionTypeKey);
            tmp_val = CFDictionaryGetValue(tmp_dict, kIOPMAssertionLevelKey);
            if(!tmp_type || !tmp_val) {
                continue;
            }
            CFNumberGetValue(tmp_val, kCFNumberIntType, &val);
            if( (kCFCompareEqualTo == 
                 CFStringCompare(tmp_type, assertion_type, 0)) &&
                (kIOPMAssertionLevelOn == val) )
            {
                if(this_is_the_first) {
                    this_is_the_first = 0;
                } else {
                    strncat(display_string, ", ", 255-length-1);
                    length = strlen(display_string);
                }            
                snprintf(pid_buf, 9, "%d", the_pid);
                strncat(display_string, pid_buf, 255-length-1);
                length = strlen(display_string);
            }            
        }        
    }

    strncat(display_string, ")", 255-length-1);
    
    printf("%s", display_string);
    
    free(pids);
    free(assertions);

bail:
    if(assertions_state) CFRelease(assertions_state);
    if(assertions_pids) CFRelease(assertions_pids);
    return;
}

static void show_supported_pm_features(void) 
{
    int i;
    CFStringRef feature;
    CFTypeRef               ps_info = IOPSCopyPowerSourcesInfo();    
    CFStringRef source;
    char            ps_buf[40];

    if(!ps_info) {
        source = CFSTR(kIOPMACPowerKey);
    } else {
        source = IOPSGetProvidingPowerSourceType(ps_info);
    }
    if(!isA_CFString(source) ||
       !CFStringGetCString(source, ps_buf, 40, kCFStringEncodingMacRoman)) {
        printf("internal supported features string error!\n");
    }

    printf("Capabilities for %s:\n", ps_buf);
    // iterate the list of all features
    for(i=0; i<kNUM_PM_FEATURES; i++)
    {
        feature = CFStringCreateWithCStringNoCopy(NULL, all_features[i], 
                                kCFStringEncodingMacRoman, kCFAllocatorNull);
        if(!isA_CFString(feature)) continue;
        if( IOPMFeatureIsAvailable(feature, source) )
        {
          if (strcmp(all_features[i], kIOPMSystemSleepKey) == 0)
            printf(" sleep\n");  
          else if (strcmp(all_features[i], kIOPMRestartOnPowerLossKey) == 0)
            printf(" autorestart\n");  
          else if (strcmp(all_features[i], kIOPMDiskSleepKey) == 0)
            printf(" disksleep\n");  
          else if (strcmp(all_features[i], kIOPMWakeOnLANKey) == 0)
            printf(" womp\n");  
          else if (strcmp(all_features[i], kIOPMWakeOnRingKey) == 0)
            printf(" ring\n");  
          else if (strcmp(all_features[i], kIOPMDisplaySleepKey) == 0)
            printf(" displaysleep\n");  
          else if (strcmp(all_features[i], kIOPMReduceSpeedKey) == 0)
            printf(" reduce\n");  
          else if (strcmp(all_features[i], kIOPMDynamicPowerStepKey) == 0)
            printf(" dps\n");  
          else if (strcmp(all_features[i], kIOPMSleepOnPowerButtonKey) == 0)
            printf(" powerbutton\n");
          else if (strcmp(all_features[i], kIOPMWakeOnClamshellKey) == 0)
            printf(" lidwake\n");
          else if (strcmp(all_features[i], kIOPMWakeOnACChangeKey) == 0)
            printf(" acwake\n");
          else if (strcmp(all_features[i], kIOPMReduceBrightnessKey) == 0)
            printf(" %s\n", ARG_REDUCEBRIGHT);
          else if (strcmp(all_features[i], kIOPMDisplaySleepUsesDimKey) == 0)
            printf(" %s\n", ARG_SLEEPUSESDIM);
          else if (strcmp(all_features[i], kIOPMMobileMotionModuleKey) == 0)
            printf(" %s\n", ARG_MOTIONSENSOR);
          else if (strcmp(all_features[i], kIOPMTTYSPreventSleepKey) == 0)
            printf(" %s\n", ARG_TTYKEEPAWAKE);
          else
            printf("%s\n", all_features[i]);
        }
        CFRelease(feature);
    }
    if(ps_info) CFRelease(ps_info);
}

static void show_power_profile(
    CFDictionaryRef     es,
    int                 indent)
{
    int                 num_profiles;
    int                 i, j;
    char                *ps;
    CFStringRef         *keys;
    CFDictionaryRef     *values;

    if(indent<0 || indent>30) indent=0;

    num_profiles = CFDictionaryGetCount(es);
    keys = (CFStringRef *)malloc(num_profiles * sizeof(void *));
    values = (CFDictionaryRef *)malloc(num_profiles * sizeof(void *));
    if(!keys || !values) return;
    CFDictionaryGetKeysAndValues(es, (const void **)keys, (const void **)values);
    
    for(i=0; i<num_profiles; i++)
    {
        if(!isA_CFDictionary(values[i])) continue;
        ps = (char *)CFStringGetCStringPtr(keys[i], 0);
        if(!ps) continue; // with for loop
        for(j=0; j<indent; j++) {
            printf(" ");
        }
        printf("%s:\n", ps);
        show_pm_settings_dict(values[i], indent, 0, false);
    }

    free(keys);
    free(values);
}

static void show_custom_pm_settings(void)
{
    CFDictionaryRef     es = NULL;

    // read settings file from /Library/Preferences/SystemConfiguration/com.apple.PowerManagement.plist
    es = IOPMCopyPMPreferences();
    if(!isA_CFDictionary(es)) return;
    show_power_profile(es, 0);
    CFRelease(es);
}

static void show_live_pm_settings(void)
{
    SCDynamicStoreRef        ds;
    CFDictionaryRef        live;

    ds = SCDynamicStoreCreate(NULL, CFSTR("pmset"), NULL, NULL);

    // read current settings from SCDynamicStore key
    live = SCDynamicStoreCopyValue(ds, CFSTR(kIOPMDynamicStoreSettingsKey));
    if(!isA_CFDictionary(live)) return;
    printf("Currently in use:\n");
    show_pm_settings_dict(live, 0, true, true);

    CFRelease(live);
    CFRelease(ds);
}

static void show_ups_settings(void)
{
    CFDictionaryRef     thresholds;
    CFDictionaryRef     d;
    CFNumberRef         n_val;
    int                 val;
    CFBooleanRef        b;

    thresholds = IOPMCopyUPSShutdownLevels(CFSTR(kIOPMDefaultUPSThresholds));    
    if(!isA_CFDictionary(thresholds)) return;

    printf("UPS settings:\n");
    
    if((d = CFDictionaryGetValue(thresholds, CFSTR(kIOUPSShutdownAtLevelKey))))
    {
        b = CFDictionaryGetValue(d, CFSTR(kIOUPSShutdownLevelEnabledKey));
        n_val = CFDictionaryGetValue(d, CFSTR(kIOUPSShutdownLevelValueKey));
        CFNumberGetValue(n_val, kCFNumberIntType, &val);
        printf("  %s\t%s\t%d\n", ARG_HALTLEVEL, (kCFBooleanTrue==b)?"on":"off", val);        
    }
    if((d = CFDictionaryGetValue(thresholds, CFSTR(kIOUPSShutdownAfterMinutesOn))))
    {
        b = CFDictionaryGetValue(d, CFSTR(kIOUPSShutdownLevelEnabledKey));
        n_val = CFDictionaryGetValue(d, CFSTR(kIOUPSShutdownLevelValueKey));
        CFNumberGetValue(n_val, kCFNumberIntType, &val);
        printf("  %s\t%s\t%d\n", ARG_HALTAFTER, (kCFBooleanTrue==b)?"on":"off", val);        
    }
    if((d = CFDictionaryGetValue(thresholds, CFSTR(kIOUPSShutdownAtMinutesLeft))))
    {
        b = CFDictionaryGetValue(d, CFSTR(kIOUPSShutdownLevelEnabledKey));
        n_val = CFDictionaryGetValue(d, CFSTR(kIOUPSShutdownLevelValueKey));
        CFNumberGetValue(n_val, kCFNumberIntType, &val);
        printf("  %s\t%s\t%d\n", ARG_HALTREMAIN, (kCFBooleanTrue==b)?"on":"off", val);        
    }
    CFRelease(thresholds);
}

static void
show_active_profiles(void)
{
    CFDictionaryRef             active_prof = 0;
    int                         i;
    int                         count;
    int                         val;
    CFNumberRef                 *prof_val = 0;
    CFStringRef                 *ps = 0;
    char                        ps_str[40];
    
    CFTypeRef                   ps_info = 0;
    CFStringRef                 current_ps = 0;

    ps_info = IOPSCopyPowerSourcesInfo();
    if(ps_info) current_ps = IOPSGetProvidingPowerSourceType(ps_info);
    if(!ps_info || !current_ps) current_ps = CFSTR(kIOPMACPowerKey);

    active_prof = IOPMCopyActivePowerProfiles();
    if(!active_prof) {
        printf("PM system error - no active profiles found\n");
        goto exit;
    }
    
    count = CFDictionaryGetCount(active_prof);
    prof_val = (CFNumberRef *)malloc(sizeof(CFNumberRef)*count);
    ps = (CFStringRef *)malloc(sizeof(CFStringRef)*count);
    if(!prof_val || !ps) goto exit;
    
    printf("Active Profiles:\n");

    CFDictionaryGetKeysAndValues(active_prof, (const void **)ps, (const void **)prof_val);
    for(i=0; i<count; i++)
    {
        if( CFStringGetCString(ps[i], ps_str, 40, kCFStringEncodingMacRoman)
          && CFNumberGetValue(prof_val[i], kCFNumberIntType, &val)) {
            printf("%s\t\t%d", ps_str, val);

            // Put a * next to the currently active power supply
            if( current_ps && (kCFCompareEqualTo == CFStringCompare(ps[i], current_ps, 0))) {
                printf("*");
            }

            printf("\n");
        }
    }

exit:
    if(active_prof) CFRelease(active_prof);
    if(ps_info) CFRelease(ps_info);
    if(prof_val) free(prof_val);
    if(ps) free(ps);
}

static void
show_system_profiles(void)
{
    CFArrayRef                  sys_prof;
    int                         prof_count;
    int                         i;

    sys_prof = IOPMCopyPowerProfiles();
    if(!sys_prof) {
        printf("No system profiles found\n");
        return;
    }

    prof_count = CFArrayGetCount(sys_prof);
    for(i=0; i<prof_count;i++)
    {
        printf("=== Profile %d ===\n", i);
        show_power_profile( CFArrayGetValueAtIndex(sys_prof, i), 0 );
        if(i!=(prof_count-1)) printf("\n");
    }

    CFRelease(sys_prof);
}

static CFDictionaryRef
getPowerEvent(int type, CFDictionaryRef     events)
{
    if(type)
        return (CFDictionaryRef)isA_CFDictionary(CFDictionaryGetValue(events, CFSTR(kIOPMRepeatingPowerOnKey)));
    else
        return (CFDictionaryRef)isA_CFDictionary(CFDictionaryGetValue(events, CFSTR(kIOPMRepeatingPowerOffKey)));
}
static int
getRepeatingDictionaryMinutes(CFDictionaryRef event)
{
    int val;
    CFNumberRef tmp_num;    
    tmp_num = (CFNumberRef)CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTimeKey));
    CFNumberGetValue(tmp_num, kCFNumberIntType, &val);
    return val;    
}
static int
getRepeatingDictionaryDayMask(CFDictionaryRef event)
{
    int val;
    CFNumberRef tmp_num;
    tmp_num = (CFNumberRef)CFDictionaryGetValue(event, CFSTR(kIOPMDaysOfWeekKey));
    CFNumberGetValue(tmp_num, kCFNumberIntType, &val);
    return val;
}
static CFStringRef
getRepeatingDictionaryType(CFDictionaryRef event)
{
    return (CFStringRef)CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTypeKey));
}

static void
print_time_of_day_to_buf(int m, char *buf)
{
    int         hours, minutes, afternoon;
    
    hours = m/60;
    minutes = m%60;
    afternoon = 0;
    if(hours >= 12) afternoon = 1; 
    if(hours > 12) hours-=12;

    sprintf(buf, "%d:%d%d%cM", hours,
            minutes/10, minutes%10,
            (afternoon? 'P':'A'));    
}

static void
print_days_to_buf(int d, char *buf)
{
    switch(d) {
        case daily_mask:
            sprintf(buf, "every day");
            break;

        case weekday_mask:                        
            sprintf(buf, "weekdays only");
            break;
            
        case weekend_mask:
            sprintf(buf, "weekends only");
            break;

        case  0x01 :                        
            sprintf(buf, "Monday");
            break;
 
        case  0x02 :                        
            sprintf(buf, "Tuesday");
            break;

        case 0x04 :                       
            sprintf(buf, "Wednesday");
            break;

        case  0x08 :                       
            sprintf(buf, "Thursday");
            break;
 
        case 0x10 :
            sprintf(buf, "Friday");
            break;

        case 0x20 :                      
            sprintf(buf, "Saturday");
            break;
       
        case  0x40 :
            sprintf(buf, "Sunday");
            break;

        default:
            sprintf(buf, "Some days");
            break;       
    }
}

static void print_repeating_report(CFDictionaryRef repeat)
{
    CFDictionaryRef     on, off;
    char                time_buf[20];
    char                day_buf[20];

    // assumes validly formatted dictionary - doesn't do any error checking
    on = getPowerEvent(1, repeat);
    off = getPowerEvent(0, repeat);

    if(on || off)
    {
        printf("Repeating power events:\n");
        if(on)
        {
            print_time_of_day_to_buf(getRepeatingDictionaryMinutes(on), time_buf);
            print_days_to_buf(getRepeatingDictionaryDayMask(on), day_buf);
        
            printf("  %s at %s %s\n",
                CFStringGetCStringPtr(getRepeatingDictionaryType(on), kCFStringEncodingMacRoman),
                time_buf, day_buf);
        }
        
        if(off)
        {
            print_time_of_day_to_buf(getRepeatingDictionaryMinutes(off), time_buf);
            print_days_to_buf(getRepeatingDictionaryDayMask(off), day_buf);
        
            printf("  %s at %s %s\n",
                CFStringGetCStringPtr(getRepeatingDictionaryType(off), kCFStringEncodingMacRoman),
                time_buf, day_buf);
        }
        fflush(stdout);
    }
}

static void
print_scheduled_report(CFArrayRef events)
{
    CFDictionaryRef     ev;
    int                 count, i;
    char                date_buf[40];
    char                name_buf[255];
    char                type_buf[40];
    char                *type_ptr = type_buf;
    CFStringRef         type;
    CFStringRef         author;
    CFDateFormatterRef  formatter;
    CFStringRef         cf_str_date;
    
    if(!events || !(count = CFArrayGetCount(events))) return;
    
    printf("Scheduled power events:\n");
    for(i=0; i<count; i++)
    {
        ev = (CFDictionaryRef)CFArrayGetValueAtIndex(events, i);
        
        formatter = CFDateFormatterCreate(kCFAllocatorDefault, CFLocaleGetSystem(),
                kCFDateFormatterShortStyle, kCFDateFormatterMediumStyle);
        CFDateFormatterSetFormat(formatter, CFSTR(kDateAndTimeFormat));
        cf_str_date = CFDateFormatterCreateStringWithDate(kCFAllocatorDefault,
                formatter, CFDictionaryGetValue(ev, CFSTR(kIOPMPowerEventTimeKey)));
        date_buf[0] = 0;
        if(cf_str_date) 
            CFStringGetCString(cf_str_date, date_buf, 40, kCFStringEncodingMacRoman);
        
        author = CFDictionaryGetValue(ev, CFSTR(kIOPMPowerEventAppNameKey));
        name_buf[0] = 0;
        if(isA_CFString(author)) 
            CFStringGetCString(author, name_buf, 255, kCFStringEncodingMacRoman);

        type = CFDictionaryGetValue(ev, CFSTR(kIOPMPowerEventTypeKey));
        type_buf[0] = 0;
        if(isA_CFString(type)) 
            CFStringGetCString(type, type_buf, 40, kCFStringEncodingMacRoman);

        // rename "wakepoweron" to "wakeorpoweron" to make things consistent between
        // "pmset -g" and "pmset sched"
        if(!strcmp(type_buf, kIOPMAutoWakeOrPowerOn)) type_ptr = ARG_WAKEORPOWERON;
        else type_ptr = type_buf;
        
        printf(" [%d]  %s at %s", i, type_ptr, date_buf);
        if(name_buf[0]) printf(" by %s", name_buf);
        printf("\n");
    }
}
 
static void show_scheduled_events(void)
{
    CFDictionaryRef     repeatingEvents;
    CFArrayRef          scheduledEvents;
 
    repeatingEvents = IOPMCopyRepeatingPowerEvents();
    scheduledEvents = IOPMCopyScheduledPowerEvents();

    if(repeatingEvents) print_repeating_report(repeatingEvents);
    if(scheduledEvents) print_scheduled_report(scheduledEvents);
    
    if(!repeatingEvents && !scheduledEvents)
        printf("No scheduled events.\n"); fflush(stdout);
}

static bool matchingAssertion(CFDictionaryRef asst_dict, CFStringRef asst)
{
    if(!asst_dict || (kCFBooleanFalse == (CFTypeRef)asst_dict)) return false;

    return CFEqual(asst, 
                   CFDictionaryGetValue(asst_dict, kIOPMAssertionTypeKey));
}


static void show_active_assertions(uint32_t which)
{
    // Print active DisableInflow or ChargeInhibit assertions on the 
    // following line
    CFDictionaryRef         assertions_status = NULL;
    CFDictionaryRef         assertions_by_pid = NULL;
    CFStringRef             *assertionNames = NULL;
    CFNumberRef             *assertionValues = NULL;
    CFNumberRef             *pids = NULL;
    CFArrayRef              *pidAssertions = NULL;
    char                    name[50];
    int                     val;
    int                     total_assertion_count;
    int                     pid_count;
    IOReturn                ret;
    int                     i, j, k;
    
    if(0 == which) return;

    ret = IOPMCopyAssertionsStatus(&assertions_status);
    if(kIOReturnSuccess != ret || !assertions_status)
        return;
    
    ret = IOPMCopyAssertionsByProcess(&assertions_by_pid);
    if(kIOReturnSuccess != ret || !assertions_by_pid)
        return;

    // Grab out the total/aggregate sate of the assertions
    total_assertion_count = CFDictionaryGetCount(assertions_status);
    if(0 == total_assertion_count)
        return;

    assertionNames = (CFStringRef *)malloc(
                                sizeof(void *) * total_assertion_count);
    assertionValues = (CFNumberRef *)malloc(
                                sizeof(void *) * total_assertion_count);
    CFDictionaryGetKeysAndValues(assertions_status, 
                                (const void **)assertionNames, 
                                (const void **)assertionValues);

    // Grab the list of activated assertions per-process
    pid_count = CFDictionaryGetCount(assertions_by_pid);
    if(0 == pid_count)
        return;
        
    pids = malloc(sizeof(CFNumberRef) * pid_count);
    pidAssertions = malloc(sizeof(CFArrayRef *) * pid_count);
 
    CFDictionaryGetKeysAndValues(assertions_by_pid, 
                                (const void **)pids, 
                                (const void **)pidAssertions);

    if( !assertionNames || !assertionValues
      || !pidAssertions || !pids) 
    {        
        return;
    }

    for(i=0; i<total_assertion_count; i++)
    {
        CFStringGetCString(assertionNames[i], name, 50, kCFStringEncodingMacRoman);
        CFNumberGetValue(assertionValues[i], kCFNumberIntType, &val);    
    
        // Determine if we want to display this assertion 
        if( !( ( (which & kAssertionCPU) &&
                CFEqual(assertionNames[i], kIOPMCPUBoundAssertion))
            || ( (which & kAssertionInflow) &&
                CFEqual(assertionNames[i], kIOPMInflowDisableAssertion))
            || ( (which & kAssertionCharge) &&
                CFEqual(assertionNames[i], kIOPMChargeInhibitAssertion))
            || ( (which & kAssertionIdle) &&
                CFEqual(assertionNames[i], kIOPMAssertionTypeNoIdleSleep)) ) )
        {
            // If the caller wasn't interested is this assertion, we pass
            continue;
        }
    
        if(val)
        {
            printf("\t'%s':\t", name);
            for(j=0; j<pid_count; j++)
            {                
                for(k=0; k<CFArrayGetCount(pidAssertions[j]); k++)
                {
                    CFDictionaryRef     obj;
                    if( (obj = (CFDictionaryRef)CFArrayGetValueAtIndex(
                                        pidAssertions[j], k)))
                    {
                        if(matchingAssertion(obj, assertionNames[i]))
                        {
                            int pid_num = 0;
                            CFNumberGetValue(pids[j], kCFNumberIntType, &pid_num);
                            printf("%d ", pid_num);
                        }
                   }
                }
            }
            printf("\n"); fflush(stdout);
        }
    }
    free(assertionNames);
    free(assertionValues);
    free(pids);
    free(pidAssertions);
    CFRelease(assertions_status);
    CFRelease(assertions_by_pid);

    return;
}

/******************************************************************************/
/*                                                                            */
/*      BLOCK IDLE SLEEP                                                      */
/*                                                                            */
/******************************************************************************/

static bool prevent_idle_sleep(void)
{
    IOPMAssertionID         neverSleep;
    
    if (kIOReturnSuccess != 
        IOPMAssertionCreate(kIOPMAssertionTypeNoIdleSleep,
                            kIOPMAssertionLevelOn,
                            &neverSleep))
    {
        // Failure
        return false;
    }
    
    printf("Preventing idle sleep (^C to exit)...\n");

    // ctrl-c at command-line exits
    while(1) {
        sleep(100);
    }
    
    return true;
}


/* sleepWakeCallback
 * 
 * Receives notifications on system sleep and system wake.
 */
static void
sleepWakeCallback(
    void *refcon, 
    io_service_t y __unused,
    natural_t messageType,
    void * messageArgument)
{
    uint32_t behavior = (uint32_t)refcon;

    switch ( messageType ) {
    case kIOMessageSystemWillSleep:
        if ( behavior & kLogSleepEvents )
        {
            print_pretty_date(false); 
            printf("Sleeping...\n");
            fflush(stdout);
        }

        IOAllowPowerChange(gPMAckPort, (long)messageArgument);
        break;
        
    case kIOMessageCanSystemSleep:
        if( behavior & kCancelSleepEvents ) 
        {
            IOCancelPowerChange(gPMAckPort, (long)messageArgument);
            print_pretty_date(false); 
            printf("Cancelling idle sleep attempt...\n");
        } else {
            IOAllowPowerChange(gPMAckPort, (long)messageArgument);
        }
        break;
        
    case kIOMessageSystemHasPoweredOn:
        if ( behavior & kLogSleepEvents )
        {
            print_pretty_date(false);   // false == no newline        
            printf("Waking...\n");
            fflush(stdout);
        }
        break;
    }

    return;
}



/******************************************************************************/
/*                                                                            */
/*     PS LOGGING                                                             */
/*                                                                            */
/******************************************************************************/

static void show_power_sources(int which)
{
    CFTypeRef           ps_info = IOPSCopyPowerSourcesInfo();
    CFArrayRef          list = NULL;
    CFStringRef         ps_name = NULL;
    static CFStringRef  last_ps = NULL;
    CFDictionaryRef     one_ps = NULL;
    char                strbuf[100];
    int                 count;
    int                 i;
    int                 show_time_estimate;
    CFNumberRef         remaining, charge, capacity;
    CFBooleanRef        charging;
    CFBooleanRef        present;
    CFStringRef         name;
    CFStringRef         state;
    CFStringRef         transport;
    CFStringRef         health;
    CFStringRef         confidence;
    CFStringRef         failure;
    char                _health[10];
    char                _confidence[10];
    char                _name[60];
    char                _failure[200];
    int                 _hours = 0;
    int                 _minutes = 0;
    int                 _charge = 0;
    int                 _capacity = 0;
    int                 _charging = 0;
    
    if(!ps_info) {
        printf("No power source info available\n");
        return;
    }
    ps_name = IOPSGetProvidingPowerSourceType(ps_info);
    if(!CFStringGetCString(ps_name, strbuf, 100, kCFStringEncodingUTF8))
    {
        goto exit;
    }
    if(!last_ps || kCFCompareEqualTo != CFStringCompare(last_ps, ps_name, 0))
    {
        printf("Currently drawing from '%s'\n", strbuf);
    }
    if(last_ps) CFRelease(last_ps);
    last_ps = CFStringCreateCopy(kCFAllocatorDefault, ps_name);
    
    list = IOPSCopyPowerSourcesList(ps_info);
    if(!list) goto exit;
    count = CFArrayGetCount(list);
    for(i=0; i<count; i++)
    {
        one_ps = IOPSGetPowerSourceDescription(ps_info, CFArrayGetValueAtIndex(list, i));
        if(!one_ps) break;
       
        // Only display settings for power sources we want to show
        transport = CFDictionaryGetValue(one_ps, CFSTR(kIOPSTransportTypeKey));
        if(!transport) continue;
        if(kCFCompareEqualTo != CFStringCompare(transport, CFSTR(kIOPSInternalType), 0))
        {
            // Internal transport means internal battery
            if(!(which & ApplyToBattery)) continue;
        } else {
            // Any specified non-Internal transport is a UPS
            if(!(which & ApplyToUPS)) continue;
        }
        
        charging = CFDictionaryGetValue(one_ps, CFSTR(kIOPSIsChargingKey));
        state = CFDictionaryGetValue(one_ps, CFSTR(kIOPSPowerSourceStateKey));
        if(kCFCompareEqualTo == CFStringCompare(state, CFSTR(kIOPSBatteryPowerValue), 0))
        {
            remaining = CFDictionaryGetValue(one_ps, CFSTR(kIOPSTimeToEmptyKey));
        } else {
            remaining = CFDictionaryGetValue(one_ps, CFSTR(kIOPSTimeToFullChargeKey));
        }            
        name = CFDictionaryGetValue(one_ps, CFSTR(kIOPSNameKey));
        charge = CFDictionaryGetValue(one_ps, CFSTR(kIOPSCurrentCapacityKey));
        capacity = CFDictionaryGetValue(one_ps, CFSTR(kIOPSMaxCapacityKey));
        present = CFDictionaryGetValue(one_ps, CFSTR(kIOPSIsPresentKey));
        health = CFDictionaryGetValue(one_ps, CFSTR(kIOPSBatteryHealthKey));
        confidence = CFDictionaryGetValue(one_ps, CFSTR(kIOPSHealthConfidenceKey));
        failure = CFDictionaryGetValue(one_ps, CFSTR("Failure"));
        
        if(name) CFStringGetCString(name, _name, 60, kCFStringEncodingMacRoman);
        if(health) CFStringGetCString(health, _health, 
                                        10, kCFStringEncodingMacRoman);
        if(confidence) CFStringGetCString(confidence, _confidence, 
                                        10, kCFStringEncodingMacRoman);
        if(failure) CFStringGetCString(failure, _failure,
                                        200, kCFStringEncodingMacRoman);
        if(charge) CFNumberGetValue(charge, kCFNumberIntType, &_charge);
        if(capacity) CFNumberGetValue(capacity, kCFNumberIntType, &_capacity);
        if(remaining) 
        {
            CFNumberGetValue(remaining, kCFNumberIntType, &_minutes);
            if(-1 != _minutes) {
                _hours = _minutes/60;
                _minutes = _minutes%60;
            }
        }
        if(charging) _charging = (kCFBooleanTrue == charging);
        
        show_time_estimate = 1;
        
        printf(" -");
        if(name) printf("%s\t", _name);
        if(present && (kCFBooleanTrue == present))
        {
            if(charge) printf("%d%%; ", _charge);
            if(charging) {
                if(_charging) {
                    printf("charging");
                } else {
                    if(kCFCompareEqualTo == CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0)) {
                        printf("AC attached; not charging");
                        show_time_estimate = 0;
                    } else {
                        printf("discharging");
                    }
                }
            }
            if(show_time_estimate && remaining) {
                if(-1 != _minutes) {
                    printf("; %d:%d%d remaining", _hours, _minutes/10, _minutes%10);
                } else {
                    printf("; (no estimate)");
                }
            }
            if(health && confidence) {
                printf(" (%s/%s)", _health, _confidence);
            }
            if(failure) {
                printf("\n\tfailure: \"%s\"", _failure);
            }
            printf("\n"); fflush(stdout);
        } else {
            printf(" (removed)\n");        
        }
    }
    
    // Display the battery-specific assertions that are affecting the system
    show_active_assertions( kAssertionInflow | kAssertionCharge );
    
exit:
    if(ps_info) CFRelease(ps_info);  
    if(list) CFRelease(list);
    return;
}

static void print_pretty_date(bool newline)
{
   CFDateFormatterRef  date_format;
    CFTimeZoneRef       tz;
    CFStringRef         time_date;
    CFLocaleRef         loc;
    char                _date[60];
 
   loc = CFLocaleCopyCurrent();
    date_format = CFDateFormatterCreate(kCFAllocatorDefault, loc,
        kCFDateFormatterShortStyle, kCFDateFormatterLongStyle);        
    CFRelease(loc);
    tz = CFTimeZoneCopySystem();
    CFDateFormatterSetProperty(date_format, kCFDateFormatterTimeZone, tz);
    CFRelease(tz);
    time_date = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault,
        date_format, CFAbsoluteTimeGetCurrent());
    CFRelease(date_format);
    if(time_date)
    {
        CFStringGetCString(time_date, _date, 60, kCFStringEncodingMacRoman);
        printf("%s ", _date); fflush(stdout);
        if(newline) printf("\n");
    }
    CFRelease(time_date); 
}

static void log_ps_change_handler(void *info)
{
    int                 which = (int)info;
    
    print_pretty_date(true);
    show_power_sources(which);
}

static int log_power_source_changes(int which)
{
    CFRunLoopSourceRef          rls = NULL;
    io_object_t                 root_notifier = MACH_PORT_NULL;
    IONotificationPortRef       notify = NULL;
    
    /* Log sleep/wake messages */
    gPMAckPort = IORegisterForSystemPower (
                        (void *)kLogSleepEvents, &notify, 
                        sleepWakeCallback, &root_notifier);

   if( notify && (MACH_PORT_NULL != gPMAckPort) )
   {
        CFRunLoopAddSource(CFRunLoopGetCurrent(),
                    IONotificationPortGetRunLoopSource(notify),
                    kCFRunLoopDefaultMode);
    }    
    
    /* Log changes to all attached power sources */
    
    rls = IOPSNotificationCreateRunLoopSource(log_ps_change_handler, (void *)which);
    if(!rls) return kParseInternalError;
    printf("pmset is in logging mode now. Hit ctrl-c to exit.\n");
    // and show initial power source state:
    log_ps_change_handler((void *)which);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
    CFRunLoopRun();
    
    return 0;
}




/******************************************************************************/
/*                                                                            */
/*     RAW PS LOGGING                                                         */
/*                                                                            */
/******************************************************************************/


static void print_raw_battery_state(io_registry_entry_t b_reg)    
{
    CFStringRef             failure;
    char                    _failure[200];
    CFBooleanRef            boo;
    CFNumberRef             n;
    int                     tmp;
    int                     cur_cap = -1;
    int                     max_cap = -1;
    CFMutableDictionaryRef  prop = NULL;
    IOReturn                ret;
    
    ret = IORegistryEntryCreateCFProperties(b_reg, &prop, 0, 0);
    if( (kIOReturnSuccess != ret) || (NULL == prop) )
    {
        printf("Couldn't read battery status; error = 0%08x\n", ret);
        return;
    }
    
    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSExternalConnectedKey));
    printf("  external connected = %s\n", 
                (kCFBooleanTrue == boo) ? "yes" : "no");

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSBatteryInstalledKey));
    printf("  battery present = %s\n", 
                (kCFBooleanTrue == boo) ? "yes" : "no");

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSIsChargingKey));
    printf("  battery charging = %s\n", 
                (kCFBooleanTrue == boo) ? "yes" : "no");

    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSCurrentCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &cur_cap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSMaxCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &max_cap);
    }
    
    if( (-1 != cur_cap) && (-1 != max_cap) )
    {
        printf("  cap = %d/%d\n", cur_cap, max_cap);
    }
    
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSTimeRemainingKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &tmp);
        printf("  time remaining = %d:%02d\n", tmp/60, tmp%60);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSAmperageKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &tmp);
        printf("  current = %d\n", tmp);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSCycleCountKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &tmp);
        printf("  cycle count = %d\n", tmp);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSLocationKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &tmp);
        printf("  location = %d\n", tmp);
    }
    
    failure = CFDictionaryGetValue(prop, CFSTR("ErrorCondition"));
    if(failure) {
        CFStringGetCString(failure, _failure, 200, kCFStringEncodingMacRoman);
        printf("  failure = \"%s\"\n", _failure);
    }
    
    printf("\n");

    CFRelease(prop);
    return;
}


static void log_raw_battery_match(
    void *refcon, 
    io_iterator_t b_iter) 
{
    IOReturn                    ret;
    IONotificationPortRef       notify = *((IONotificationPortRef *)refcon);
    io_registry_entry_t         battery;
    io_object_t                 notification_ref;
    int                         found = false;
    
    while ((battery = (io_registry_entry_t)IOIteratorNext(b_iter)))
    {
        found = true;        
        printf(" * Battery matched at registry = %d\n", (int32_t)battery);

        print_raw_battery_state(battery);
        
        // And install an interest notification on it
        ret = IOServiceAddInterestNotification(notify, battery, 
                            kIOGeneralInterest, log_raw_battery_interest,
                            NULL, &notification_ref);

        IOObjectRelease(battery);
    }
    
    if(!found) {
        printf("  (no batteries found; waiting)\n");
    }    
}


static void log_raw_battery_interest(
    void *refcon, 
    io_service_t batt, 
    natural_t messageType, 
    void *messageArgument)
{
    CFDateFormatterRef  date_format;
    CFTimeZoneRef       tz;
    CFStringRef         time_date;
    CFLocaleRef         loc;
    char                _date[60];


    if(kIOPMMessageBatteryStatusHasChanged == messageType)
    {
    
        loc = CFLocaleCopyCurrent();
        date_format = CFDateFormatterCreate(kCFAllocatorDefault, loc,
            kCFDateFormatterShortStyle, kCFDateFormatterLongStyle);        
        CFRelease(loc);
        tz = CFTimeZoneCopySystem();
        CFDateFormatterSetProperty(date_format, kCFDateFormatterTimeZone, tz);
        CFRelease(tz);
        time_date = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault,
            date_format, CFAbsoluteTimeGetCurrent());
        CFRelease(date_format);
        if(time_date)
        {
            CFStringGetCString(time_date, _date, 60, kCFStringEncodingMacRoman);
            printf("%s\n", _date); fflush(stdout);
        }
        CFRelease(time_date);
    

        print_raw_battery_state((io_registry_entry_t)batt);

    }

    return;
}


static int log_raw_power_source_changes(void)
{
    IONotificationPortRef       notify_port = 0;
    io_iterator_t               battery_iter = 0;
    CFRunLoopSourceRef          rlser = 0;
    IOReturn                    ret;

    printf("pmset is in RAW logging mode now. Hit ctrl-c to exit.\n");

    notify_port = IONotificationPortCreate(0);
    rlser = IONotificationPortGetRunLoopSource(notify_port);
    if(!rlser) return 0;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rlser, kCFRunLoopDefaultMode);


    ret = IOServiceAddMatchingNotification(
                              notify_port,
                              kIOFirstMatchNotification,  
                              IOServiceMatching("IOPMPowerSource"), 
                              log_raw_battery_match, 
                              (void *)&notify_port,  
                              &battery_iter);
    if(KERN_SUCCESS != ret){
         printf("!!Error prevented matching notifications; err = 0x%08x\n", ret);
    }

    // Install notifications on existing instances.
    log_raw_battery_match((void *)&notify_port, battery_iter);
        
    CFRunLoopRun();
    
    // should never return from CFRunLoopRun
    return 0;
}



static void log_thermal_events(void)
{
    /*
     * Open listening mode
     */

    mach_port_t     powerConstraint_port = MACH_PORT_NULL;
    mach_port_t     cpuPower_port = MACH_PORT_NULL;
    int             powerConstraintNotifyToken = 0;
    int             cpuPowerNotifyToken = 0;

    CFRunLoopSourceRef      rls1, rls2;

    uint32_t        status;
    

    status = notify_register_mach_port(kIOPMCPUPowerNotificationKey,
                    &cpuPower_port, 0, /* flags */
                    &cpuPowerNotifyToken);

    if (NOTIFY_STATUS_OK != status) {
        fprintf(stderr, "Registration failed for \"%s\" with (%u)\n",
                        kIOPMCPUPowerNotificationKey, status);
    }                        


    status = notify_register_mach_port(kIOPMThermalWarningNotificationKey,
                    &powerConstraint_port, 0, /* flags */
                    &powerConstraintNotifyToken);

    if (NOTIFY_STATUS_OK != status)
    {
        fprintf(stderr, "Registration failed for \"%s\" with (%u)\n",
                        kIOPMThermalWarningNotificationKey, status);
    }

    // CFMachPorts!
    gPowerConstraintMachPort = CFMachPortCreateWithPort(
                    kCFAllocatorDefault, powerConstraint_port,
                    log_thermal_callback, NULL, NULL);

    gCPUPowerMachPort = CFMachPortCreateWithPort(
                    kCFAllocatorDefault, cpuPower_port,
                    log_thermal_callback, NULL, NULL);

    rls1 = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, gPowerConstraintMachPort, 0);
    if (rls1) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), rls1, kCFRunLoopDefaultMode);
    }

    rls2 = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, gCPUPowerMachPort, 0);
    if (rls2) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), rls2, kCFRunLoopDefaultMode);
    }

    print_pretty_date(true);
    printf("Logging thermal event messages...\n");

    show_thermal_warning_level();
    show_thermal_cpu_power_level();
    
    CFRunLoopRun();    
    // should never return from CFRunLoopRun
    return;
}



static void log_thermal_callback(
        CFMachPortRef port, 
        void *msg,
        CFIndex size,
        void *info)
{
    if (gCPUPowerMachPort == port) {
        show_thermal_cpu_power_level();
        return;
    }

    if (gPowerConstraintMachPort == port) { 
        show_thermal_warning_level();
        return;
    }

}

static void show_thermal_warning_level(void)
{
    uint32_t                warn = -1;
    IOReturn                ret;
    
    ret = IOPMGetThermalWarningLevel(&warn);

    if (kIOReturnNotFound == ret) {
        printf("Note: No thermal warning level has been recorded\n");
        return;
    }

    if (kIOReturnSuccess != ret) 
    {
        printf("Error: No thermal warning level with error code 0x%08x\n", ret);
        return;
    }

    // successfully found warning level
    print_pretty_date(false);
    printf("Thermal Warning Level = %d\n", warn);
    return;
}


static void show_thermal_cpu_power_level(void)
{
    CFDictionaryRef         cpuStatus;
    CFStringRef             *keys = NULL;
    CFNumberRef             *vals = NULL;
    int                     count = 0;
    int                     i;
    IOReturn                ret;

    ret = IOPMCopyCPUPowerStatus(&cpuStatus);

    if (kIOReturnNotFound == ret) {
        printf("Note: No CPU power status has been recorded\n");
        return;
    }

    if (!cpuStatus || (kIOReturnSuccess != ret)) 
    {
        printf("Error: No CPU power status with error code 0x%08x\n", ret);
        return;
    }

    print_pretty_date(false);
    fprintf(stderr, "CPU Power notify\n"), fflush(stderr);        
    
    count = CFDictionaryGetCount(cpuStatus);
    keys = (CFStringRef *)malloc(count*sizeof(CFStringRef));
    vals = (CFNumberRef *)malloc(count*sizeof(CFNumberRef));
    if (!keys||!vals) 
        goto exit;

    CFDictionaryGetKeysAndValues(cpuStatus, 
                    (const void **)keys, (const void **)vals);

    for(i=0; i<count; i++) {
        char strbuf[125];
        int  valint;
        
        CFStringGetCString(keys[i], strbuf, 125, kCFStringEncodingUTF8);
        CFNumberGetValue(vals[i], kCFNumberIntType, &valint);
        printf("\t%s \t= %d\n", strbuf, valint);
    }


exit:    
    if (keys)
        free(keys);
    if (vals)
        free(vals);
    if (cpuStatus);
        CFRelease(cpuStatus);

}



/******************************************************************************/
/*                                                                            */
/*     BORING SETTINGS & PARSING                                              */
/*                                                                            */
/******************************************************************************/


static int checkAndSetIntValue(
    char *valstr, 
    CFStringRef settingKey, 
    int apply,
    int isOnOffSetting, 
    int multiplier,
    CFMutableDictionaryRef ac, 
    CFMutableDictionaryRef batt, 
    CFMutableDictionaryRef ups)
{
    CFNumberRef     cfnum;
    char            *endptr = NULL;
    long            val;

    if(!valstr) return -1;

    val = strtol(valstr, &endptr, 10);

    if(0 != *endptr)
    {
        // the string contained some non-numerical characters - bail
        return -1;
    }
    
    // for on/off settings, turn any non-zero number into a 1
    if(isOnOffSetting) {
        val = (val?1:0);
    } else {
        // Numerical values may have multipliers (i.e. x1000 for sec -> msec)
        if(0 != multiplier) val *= multiplier;
    }
    // negative number? reject it
    if(val < 0) return -1;

    cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &val);
    if(!cfnum) return -1;
    if(apply & ApplyToBattery)
        CFDictionarySetValue(batt, settingKey, cfnum);
    if(apply & ApplyToCharger)
        CFDictionarySetValue(ac, settingKey, cfnum);
    if(apply & ApplyToUPS)
        CFDictionarySetValue(ups, settingKey, cfnum);
    CFRelease(cfnum);
    return 0;
}

static int checkAndSetStrValue(char *valstr, CFStringRef settingKey, int apply,
                CFMutableDictionaryRef ac, CFMutableDictionaryRef batt, CFMutableDictionaryRef ups)
{
    CFStringRef     cfstr;

    if(!valstr) return -1;

    cfstr = CFStringCreateWithCString(kCFAllocatorDefault, 
                        valstr, kCFStringEncodingMacRoman);
    if(!cfstr) return -1;
    if(apply & ApplyToBattery)
        CFDictionarySetValue(batt, settingKey, cfstr);
    if(apply & ApplyToCharger)
        CFDictionarySetValue(ac, settingKey, cfstr);
    if(apply & ApplyToUPS)
        CFDictionarySetValue(ups, settingKey, cfstr);
    CFRelease(cfstr);
    return 0;
}


static int setUPSValue(char *valstr, 
    CFStringRef    whichUPS, 
    CFStringRef settingKey, 
    int apply, 
    CFMutableDictionaryRef thresholds)
{
    CFMutableDictionaryRef ups_setting = NULL;
    CFDictionaryRef     tmp_ups_setting = NULL;
    CFNumberRef     cfnum = NULL;
    CFBooleanRef    on_off = kCFBooleanTrue;
    char            *endptr = NULL;
    long            val;

    if(!valstr) return -1;

    val = strtol(valstr, &endptr, 10);

    if(0 != *endptr)
    {
        // the string contained some non-numerical characters - bail
        return -1;
    }

    if(-1 == val)
    {
        on_off = kCFBooleanFalse;
    }

    // negative number? reject it
    if(val < 0) val = 0;
    
    // if this should be a percentage, cap the value at 100%
    if(kCFCompareEqualTo == CFStringCompare(settingKey, CFSTR(kIOUPSShutdownAtLevelKey), 0))
    {
        if(val > 100) val = 100;
    };

    // bail if -u or -a hasn't been specified:
    if(!(apply & ApplyToUPS)) return -1;
    
    // Create the nested dictionaries of UPS settings
    tmp_ups_setting = CFDictionaryGetValue(thresholds, settingKey);
    ups_setting = CFDictionaryCreateMutableCopy(0, 0, tmp_ups_setting); 
    if(!ups_setting)
    {
        ups_setting = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &val);
    
    if(kCFBooleanFalse == on_off) {
        // If user is turning this setting off, then preserve the existing value in there.
        // via CFDictionaryAddValue
        CFDictionaryAddValue(ups_setting, CFSTR(kIOUPSShutdownLevelValueKey), cfnum);
    } else {
        // If user is providing a new value for this setting, overwrite the existing value.
        CFDictionarySetValue(ups_setting, CFSTR(kIOUPSShutdownLevelValueKey), cfnum);
    }
    CFRelease(cfnum);    
    CFDictionarySetValue(ups_setting, CFSTR(kIOUPSShutdownLevelEnabledKey), on_off);

    CFDictionarySetValue(thresholds, settingKey, ups_setting);
    CFRelease(ups_setting);
    return 0;
}


//  pmset repeat cancel
//  pmset repeat <type> <days of week> <time> [<type> <days of week> <time>]\n");
static int parseRepeatingEvent(
    char                        **argv,
    int                         *num_args_parsed,
    CFMutableDictionaryRef      local_repeating_event, 
    bool                        *local_cancel_repeating)
{
    CFDateFormatterRef          formatter = 0;
    CFTimeZoneRef               tz = 0;
    CFStringRef                 cf_str_date = 0;
    int                         i = 0;
    int                         j = 0;
    int                         str_len = 0;
    int                         days_mask = 0;
    int                         on_off = 0;
    IOReturn                    ret = kParseInternalError;

    CFStringRef                 the_type = 0;
    CFNumberRef                 the_days = 0;
    CFDateRef                   cf_date = 0;
    CFGregorianDate             cf_greg_date;
    int                         event_time = 0;
    CFNumberRef                 the_time = 0;       // in minutes from midnight
    CFMutableDictionaryRef      one_repeating_event = 0;

    formatter = CFDateFormatterCreate(kCFAllocatorDefault, CFLocaleGetSystem(),
        kCFDateFormatterShortStyle, kCFDateFormatterMediumStyle);
    if(!formatter) return kParseInternalError;
    tz = CFTimeZoneCopySystem();
    if(!tz) return kParseInternalError;

    CFDateFormatterSetFormat(formatter, CFSTR(kTimeFormat));
    if(!argv[i]) return kParseBadArgs;
    // cancel ALL repeating events
    if(0 == strcmp(argv[i], ARG_CANCEL) ) {

        *local_cancel_repeating = true;
        i++;
        ret = kParseSuccess;
        goto exit;
    }
    
    while(argv[i])
    {    
        // type
        if(0 == strcmp(argv[i], ARG_SLEEP))
        {
            on_off = 0;
            the_type = CFSTR(kIOPMAutoSleep);
            i++;
        } else if(0 == strcmp(argv[i], ARG_SHUTDOWN))
        {
            on_off = 0;
            the_type =  CFSTR(kIOPMAutoShutdown);
            i++;
        } else if(0 == strcmp(argv[i], ARG_RESTART))
        {
            on_off = 0;
            the_type =  CFSTR(kIOPMAutoRestart);
            i++;
        } else if(0 == strcmp(argv[i], ARG_WAKE))
        {
            on_off = 1;
            the_type =  CFSTR(kIOPMAutoWake);
            i++;
        } else if(0 == strcmp(argv[i], ARG_POWERON))
        {
            on_off = 1;
            the_type =  CFSTR(kIOPMAutoPowerOn);
            i++;
        } else if(0 == strcmp(argv[i], ARG_WAKEORPOWERON))
        {
            on_off = 1;
            the_type =  CFSTR(kIOPMAutoWakeOrPowerOn);
            i++;
        } else {
            printf("Error: Unspecified scheduled event type\n");
            ret = kParseBadArgs;
            goto bail;
        }
    
        // days of week
        // Expect argv[i] to be a NULL terminated string with a subset of: MTWRFSU
        // indicating the days of the week to schedule repeating wakeup
        // TODO: accept M-F style ranges
        if(!argv[i]) {
            ret = kParseBadArgs;
            goto bail;
        }
        
        str_len = strlen(argv[i]);
        for(j=0; j<str_len; j++)
        {
            if('m' == argv[i][j]) {
                days_mask |= kIOPMMonday;
            } else if('t' == argv[i][j]) {
                days_mask |= kIOPMTuesday;
            } else if('w' == argv[i][j]) {
                days_mask |= kIOPMWednesday;
            } else if('r' == argv[i][j]) {
                days_mask |= kIOPMThursday;
            } else if('f' == argv[i][j]) {
                days_mask |= kIOPMFriday;
            } else if('s' == argv[i][j]) {
                days_mask |= kIOPMSaturday;
            } else if('u' == argv[i][j]) {
                days_mask |= kIOPMSunday;
            }
        }
        if(0 == days_mask) {
            // something went awry; we expect a non-zero days mask.
            ret = kParseBadArgs;
            goto bail;
        } else {
            the_days = CFNumberCreate(0, kCFNumberIntType, &days_mask);
            i++;
        }
    
        // time
        if(!argv[i]) {
            ret = kParseBadArgs;
            goto bail;
        }
        cf_str_date = CFStringCreateWithCString(kCFAllocatorDefault, 
                        argv[i], kCFStringEncodingMacRoman);
        if(!cf_str_date) {
            ret = kParseInternalError;
            goto bail;
        }
        cf_date = CFDateFormatterCreateDateFromString(0, formatter, cf_str_date, 0);
        if(!cf_date) {
            ret = kParseBadArgs;
            goto bail;
        }
        cf_greg_date = CFAbsoluteTimeGetGregorianDate(CFDateGetAbsoluteTime(cf_date), tz);
        event_time = cf_greg_date.hour*60 + cf_greg_date.minute;
        the_time = CFNumberCreate(0, kCFNumberIntType, &event_time);
        
        i++;
            
        // check for existence of who, the_days, the_time, the_type
        // if this was a validly formatted dictionary, pack the repeating dict appropriately.
        if( isA_CFNumber(the_days) && isA_CFString(the_type) && isA_CFNumber(the_time) )
        {
            one_repeating_event = CFDictionaryCreateMutable(0, 0, 
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            if(!one_repeating_event) goto bail;
            CFDictionarySetValue(one_repeating_event, CFSTR(kIOPMPowerEventTypeKey), the_type);
            CFDictionarySetValue(one_repeating_event, CFSTR(kIOPMDaysOfWeekKey), the_days);
            CFDictionarySetValue(one_repeating_event, CFSTR(kIOPMPowerEventTimeKey), the_time);
            
            CFDictionarySetValue(local_repeating_event,
                 (on_off ? CFSTR(kIOPMRepeatingPowerOnKey):CFSTR(kIOPMRepeatingPowerOffKey)),
                                one_repeating_event);
            CFRelease(one_repeating_event);            
        }
    } // while loop
    
    ret = kParseSuccess;
    goto exit;

bail:
    fprintf(stderr, "Error: badly formatted repeating power event\n");
    fflush(stderr);

exit:
    if(num_args_parsed) *num_args_parsed = i;
    if(tz) CFRelease(tz);
    if(formatter) CFRelease(formatter);
    return ret;
}


// pmset sched wake "4/27/04 1:00:00 PM" "Ethan Bold"
// pmset sched cancel sleep "4/27/04 1:00:00 PM" "MyAlarmClock"
// pmset sched cancel shutdown "4/27/04 1:00:00 PM"
static int parseScheduledEvent(
    char                        **argv,
    int                         *num_args_parsed,
    ScheduledEventReturnType    *local_scheduled_event, 
    bool                        *cancel_scheduled_event)
{
    CFDateFormatterRef          formatter = 0;
    CFStringRef                 cf_str_date = 0;
    int                         i = 0;
    IOReturn                    ret = kParseSuccess;

    formatter = CFDateFormatterCreate(kCFAllocatorDefault, CFLocaleGetSystem(),
        kCFDateFormatterShortStyle, kCFDateFormatterMediumStyle);
    if(!formatter) return kParseInternalError;

    *num_args_parsed = 0;

    // We manually set the format (as recommended by header comments)
    // to ensure it doesn't vary from release to release or from locale
    // to locale.
    CFDateFormatterSetFormat(formatter, CFSTR(kDateAndTimeFormat));
    if(!argv[i]) return -1;

    // cancel
    if(0 == strcmp(argv[i], ARG_CANCEL) ) {
        char            *endptr = NULL;
        long            val;
        CFArrayRef      all_events = 0;
        CFDictionaryRef the_event = 0;

        *cancel_scheduled_event = true;
        i++;
        
        // See if the next field is an integer. If so, we cancel the event
        // indicated by the indices printed in "pmset -g sched"
        // If not, parse out the rest of the entry for a full-description
        // of the event to cancel.
        if(!argv[i]) return -1;
    
        val = strtol(argv[i], &endptr, 10);
    
        if(0 == *endptr)
        {
            all_events = IOPMCopyScheduledPowerEvents();
            if(!all_events) return kParseInternalError;
            
            if(val < 0 || val > CFArrayGetCount(all_events)) {
                ret = kParseBadArgs;            
                goto exit;
            }
            
            // the string was indeed a number
            the_event = isA_CFDictionary(CFArrayGetValueAtIndex(all_events, val));
            if(!the_event) {
                ret = kParseInternalError;
                goto exit;
            }
            
            local_scheduled_event->when = CFRetain(
                CFDictionaryGetValue(the_event, CFSTR(kIOPMPowerEventTimeKey)));
            local_scheduled_event->who = CFRetain(
                CFDictionaryGetValue(the_event, CFSTR(kIOPMPowerEventAppNameKey)));
            local_scheduled_event->which = CFRetain(
                CFDictionaryGetValue(the_event, CFSTR(kIOPMPowerEventTypeKey)));
            
            i++;
            
            CFRelease(all_events);
            ret = kParseSuccess;
            goto exit;
        }
    }

    // type
    if(0 == strcmp(argv[i], ARG_SLEEP))
    {
        local_scheduled_event->which =
            CFStringCreateWithCString(0, kIOPMAutoSleep, kCFStringEncodingMacRoman);
        i++;
    } else if(0 == strcmp(argv[i], ARG_SHUTDOWN))
    {
        local_scheduled_event->which =
            CFStringCreateWithCString(0, kIOPMAutoShutdown, kCFStringEncodingMacRoman);
        i++;
    } else if(0 == strcmp(argv[i], ARG_RESTART))
    {
        local_scheduled_event->which =
            CFStringCreateWithCString(0, kIOPMAutoRestart, kCFStringEncodingMacRoman);
        i++;
    } else if(0 == strcmp(argv[i], ARG_WAKE))
    {
        local_scheduled_event->which =
            CFStringCreateWithCString(0, kIOPMAutoWake, kCFStringEncodingMacRoman);
        i++;
    } else if(0 == strcmp(argv[i], ARG_POWERON))
    {
        local_scheduled_event->which =
            CFStringCreateWithCString(0, kIOPMAutoPowerOn, kCFStringEncodingMacRoman);
        i++;
    } else if(0 == strcmp(argv[i], ARG_WAKEORPOWERON))
    {
        local_scheduled_event->which =
            CFStringCreateWithCString(0, kIOPMAutoWakeOrPowerOn, kCFStringEncodingMacRoman);
        i++;
    } else {
        printf("Error: Unspecified scheduled event type\n");
        return kParseBadArgs;
    }
    
    if(0 == local_scheduled_event->which) {
        printf("Error: Unspecified scheduled event type (2)\n");
        return kParseBadArgs;
    }
    
    // date & time
    if(argv[i]) {
        cf_str_date = CFStringCreateWithCString(kCFAllocatorDefault, 
                        argv[i], kCFStringEncodingMacRoman);
        if(!cf_str_date) {
            local_scheduled_event->when = NULL;
            return kParseInternalError;
        }
        local_scheduled_event->when = 
            CFDateFormatterCreateDateFromString(
                0,
                formatter, 
                cf_str_date, 
                NULL);
        i++;
    } else {
        printf("Error: Badly formatted date\n");
        return kParseBadArgs;
    }

    if(0 == local_scheduled_event->when) {
        printf("Error: Badly formatted date (2)\n");
        return kParseBadArgs;
    }
    // author
    if(argv[i]) {
        local_scheduled_event->who =
            CFStringCreateWithCString(0, argv[i], kCFStringEncodingMacRoman);
        i++;
    } else {
        local_scheduled_event->who = 0;
    }

exit:
    if(num_args_parsed) *num_args_parsed = i;

    if(formatter) CFRelease(formatter);
    return ret;
}


/*
 * parseArgs - parse argv input stream into executable commands
 *      and returns executable commands.
 * INPUTS:
 *  int argc, 
 *  char* argv[], 
 * OUTPUTS:
 * If these pointers are specified on exit, that means parseArgs modified these settings
 * and they should be written out to persistent store by the caller.
 *  settings: Energy Saver settings
 *  modified_power_sources: which power sources the modified Energy Saver settings apply to
 *                          (only valid if settings is defined)
 *  active_profiles: Changes the active profile
 *  system_power_settings: system-wide settings, not tied to power source. like "disablesleep"
 *  ups_thresholds: UPS shutdown thresholds
 *  scheduled_event: Description of a one-time power event
 *  cancel_scheduled_event: true = cancel the scheduled event/false = schedule the event
 *                          (only valid if scheduled_event is defined)
 *  repeating_event: Description of a repeating power event
 *  cancel_repeating_event: true = cancel the repeating event/false = schedule the repeating event
 *                          (only valid if repeating_event is defined)
*/

static int parseArgs(int argc, 
    char* argv[], 
    CFDictionaryRef             *settings,
    int                         *modified_power_sources,
    bool                        *force_activate_settings,
    CFDictionaryRef             *active_profiles,
    CFDictionaryRef             *system_power_settings,
    CFDictionaryRef             *ups_thresholds,
    ScheduledEventReturnType    **scheduled_event,
    bool                        *cancel_scheduled_event,
    CFDictionaryRef             *repeating_event,
    bool                        *cancel_repeating_event,
    uint32_t                    *pmCmd)
{
    int                         i = 1;
    int                         j;
    int                         apply = 0;
    int                         length;
    int                         ret = kParseSuccess;
    int                         modified = 0;
    IOReturn                    kr;
    ScheduledEventReturnType    *local_scheduled_event = 0;
    bool                        local_cancel_event = false;
    CFDictionaryRef             tmp_repeating = 0;
    CFMutableDictionaryRef      local_repeating_event = 0;
    bool                        local_cancel_repeating = false;
    CFDictionaryRef             tmp_profiles = 0;
    CFMutableDictionaryRef      local_profiles = 0;
    CFDictionaryRef             tmp_ups_settings = 0;
    CFMutableDictionaryRef      local_ups_settings = 0;
    CFDictionaryRef             tmp_settings = 0;
    CFMutableDictionaryRef      local_settings = 0;
    CFMutableDictionaryRef      local_system_power_settings = 0;
    CFDictionaryRef             tmp_battery = 0;
    CFMutableDictionaryRef      battery = 0;
    CFDictionaryRef             tmp_ac = 0;
    CFMutableDictionaryRef      ac = 0;
    CFDictionaryRef             tmp_ups = 0;
    CFMutableDictionaryRef      ups = 0;

    if(argc == 1) {
        return kParseBadArgs;
    }


/*
 * Check for any commands
 * Commands may not be combined with any other flags
 *
 */
    if(0 == strcmp(argv[1], ARG_TOUCH)) 
    {
        *pmCmd = kPMCommandTouch;
        return kIOReturnSuccess;
    } else if(0 == strcmp(argv[1], ARG_NOIDLE)) 
    {
        *pmCmd = kPMCommandNoIdle;
        return kIOReturnSuccess;
    } else if(0 == strcmp(argv[1], ARG_SLEEPNOW)) 
    {
        *pmCmd = kPMCommandSleepNow;
        return kIOReturnSuccess;
    }


/***********
 * Setup mutable PM preferences
 ***********/
    tmp_settings = IOPMCopyActivePMPreferences();    
    if(!tmp_settings) {
        return kParseInternalError;
    }
    local_settings = CFDictionaryCreateMutableCopy(0, 0, tmp_settings);
    if(!local_settings) {
        return kParseInternalError;
    }
    CFRelease(tmp_settings);
    
    // Either battery or AC settings may not exist if the system doesn't support it.
    tmp_battery = isA_CFDictionary(CFDictionaryGetValue(local_settings, CFSTR(kIOPMBatteryPowerKey)));
    if(tmp_battery) {
        battery = CFDictionaryCreateMutableCopy(0, 0, tmp_battery);
        if(battery) {
            CFDictionarySetValue(local_settings, CFSTR(kIOPMBatteryPowerKey), battery);
            CFRelease(battery);
        }
    }
    tmp_ac = isA_CFDictionary(CFDictionaryGetValue(local_settings, CFSTR(kIOPMACPowerKey)));
    if(tmp_ac) {
        ac = CFDictionaryCreateMutableCopy(0, 0, tmp_ac);
        if(ac) {
            CFDictionarySetValue(local_settings, CFSTR(kIOPMACPowerKey), ac);
            CFRelease(ac);
        }
    }
    tmp_ups = isA_CFDictionary(CFDictionaryGetValue(local_settings, CFSTR(kIOPMUPSPowerKey)));
    if(tmp_ups) {
        ups = CFDictionaryCreateMutableCopy(0, 0, tmp_ups);
        if(ups) {
            CFDictionarySetValue(local_settings, CFSTR(kIOPMUPSPowerKey), ups);
            CFRelease(ups);
        }
    }
/***********
 * Setup mutable UPS thersholds
 ***********/
    tmp_ups_settings = IOPMCopyUPSShutdownLevels(CFSTR(kIOPMDefaultUPSThresholds));
    if(tmp_ups_settings) {
        local_ups_settings = CFDictionaryCreateMutableCopy(0, 0, tmp_ups_settings);
        CFRelease(tmp_ups_settings);
    }

/***********
 * Setup mutable Active profiles
 ***********/
    tmp_profiles = IOPMCopyActivePowerProfiles();
    if(tmp_profiles) {
        local_profiles = CFDictionaryCreateMutableCopy(0, 0, tmp_profiles);
        CFRelease(tmp_profiles);
    }
    
/************
 * Setup scheduled events
 ************/
    tmp_repeating = IOPMCopyRepeatingPowerEvents();
    if(tmp_repeating) {
        local_repeating_event = CFDictionaryCreateMutableCopy(0, 0, tmp_repeating);
        CFRelease(tmp_repeating);
    }
/************
 * Setup system power settings holder dictionary
 ************/
    local_system_power_settings = CFDictionaryCreateMutable(0, 0, 
                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    // Unless specified, apply changes to both battery and AC
    if(battery) apply |= ApplyToBattery;
    if(ac) apply |= ApplyToCharger;
    if(ups) apply |= ApplyToUPS;

    for(i=0; i<argc; i++)
    {
        // I only speak lower case.
        length=strlen(argv[i]);
        for(j=0; j<length; j++)
        {
            argv[i][j] = tolower(argv[i][j]);
        }
    }
    
    i=1;
    while(i < argc)
    {    
        if( (argv[i][0] == '-')
            && ('1' != argv[i][1]) ) // don't try to process it as a abcg argument if it's a -1
                                     // the profiles parsing code below is expecting the -1
        {
        // Process -a/-b/-c/-g arguments
            apply = 0;
            switch (argv[i][1])
            {
                case 'a':
                    if(battery) apply |= ApplyToBattery;
                    if(ac) apply |= ApplyToCharger;
                    if(ups) apply |= ApplyToUPS;
                    break;
                case 'b':
                    if(battery) apply = ApplyToBattery;
                    break;
                case 'c':
                    if(ac) apply = ApplyToCharger;
                    break;
                case 'u':
                    if(ups) apply = ApplyToUPS;
                    break;
                case 'g':
                    // One of the "gets"
                    if('\0' != argv[i][2]) {
                        return kParseBadArgs;
                    }
                    i++;
                    
                    // is the next word NULL? then it's a "-g" arg
                    if( (NULL == argv[i])
                        || !strcmp(argv[i], ARG_LIVE))
                    {
                        show_system_power_settings();
                        show_active_profiles();
                        show_live_pm_settings();
                    } else if( !strcmp(argv[i], ARG_DISK)
                            || !strcmp(argv[i], ARG_CUSTOM))        
                    {
                        show_custom_pm_settings();
                    } else if(!strcmp(argv[i], ARG_CAP))
                    {
                        // show capabilities
                        show_supported_pm_features();
                    } else if(!strcmp(argv[i], ARG_SCHED))
                    {
                        // show scheduled repeating and one-time sleep/wakeup events
                        show_scheduled_events();
                    } else if(!strcmp(argv[i], ARG_UPS))
                    {
                        // show UPS
                        show_ups_settings();
                    } else if(!strcmp(argv[i], ARG_SYS_PROFILES))
                    {
                        // show profiles
                        show_active_profiles();
                        show_system_profiles();
                    } else if(!strcmp(argv[i], ARG_PS)
                           || !strcmp(argv[i], ARG_BATT))
                    {
                        // show battery & UPS state
                        show_power_sources(ApplyToBattery | ApplyToUPS);
                    } else if(!strcmp(argv[i], ARG_PSLOG))
                    {
                        // continuously log PS changes until user ctrl-c exits
                        // or return kParseInternalError if something screwy happened
                        return log_power_source_changes(ApplyToBattery | ApplyToUPS);
                    } else if(!strcmp(argv[i], ARG_PSRAW))
                    {
                        // continuously log PS changes until user ctrl-c exits
                        // log via interest notes on the IOPMPowerSource nodes
                        return log_raw_power_source_changes();
                    } else if(!strcmp(argv[i], ARG_THERMLOG))
                    {
                        // continuously log thermal events until user ctrl-c
                        // exits
                        log_thermal_events();
                        return kParseSuccess;
                    } else {
                        return kParseBadArgs;
                    }

                    // return immediately - don't handle any more setting arguments
                    return kParseSuccess;
                    break;
                default:
                    // bad!
                    return kParseBadArgs;
                    break;
            }
            
            i++;
        } else if( (0 == strcmp(argv[i], ARG_SCHEDULE))
                || (0 == strcmp(argv[i], ARG_SCHED)) )
        {
            // Process rest of input as a cancel/schedule power event
            int args_parsed;
        
            local_scheduled_event = scheduled_event_struct_create();
            if(!local_scheduled_event) return kParseInternalError;

            i += 1;            
            ret = parseScheduledEvent(
                            &(argv[i]),
                            &args_parsed,
                            local_scheduled_event, 
                            &local_cancel_event);
            if(kParseSuccess != ret)
            {
                goto bail;
            }

            i += args_parsed;
            modified |= kModSched;
        } else if(0 == strcmp(argv[i], ARG_REPEAT))
        {
            int args_parsed;
            
            local_repeating_event = CFDictionaryCreateMutable(0, 0,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            if(!local_repeating_event) return kParseInternalError;
            i+=1;
            ret = parseRepeatingEvent(
                            &(argv[i]),
                            &args_parsed,
                            local_repeating_event, 
                            &local_cancel_repeating);
            if(kParseSuccess != ret)
            {
                goto bail;
            }

            i += args_parsed;
            modified |= kModRepeat;
        } else 
        {
        // Process the settings
            if(0 == strcmp(argv[i], ARG_BOOT))
            {
                // Tell kernel power management that bootup is complete
                kr = setRootDomainProperty(CFSTR("System Boot Complete"), kCFBooleanTrue);
                if(kr == kIOReturnSuccess) {
                    printf("Setting boot completed.\n");
                } else {
                    fprintf(stderr, "pmset: Error 0x%x setting boot property\n", kr);
                    fflush(stderr);
                }

                i++;
            } else if(0 == strcmp(argv[i], ARG_UNBOOT))
            {
                // Tell kernel power management that bootup is complete
                kr = setRootDomainProperty(CFSTR("System Shutdown"), kCFBooleanTrue);
                if(kr == kIOReturnSuccess) {
                    printf("Setting shutdown true.\n");
                } else {
                    fprintf(stderr, "pmset: Error 0x%x setting boot property\n", kr);
                    fflush(stderr);
                }

                i++;
            } else if(0 == strcmp(argv[i], ARG_FORCE))
            {
                *force_activate_settings = true;
                i++;
            } else if( (0 == strcmp(argv[i], ARG_DIM)) ||
                       (0 == strcmp(argv[i], ARG_DISPLAYSLEEP)) )
            {
                // either 'dim' or 'displaysleep' argument sets minutes until display dims
                if(-1 == checkAndSetIntValue( argv[i+1], 
                                                CFSTR(kIOPMDisplaySleepKey), 
                                                apply, false, kNoMultiplier,
                                                ac, battery, ups))
                {
                    return kParseBadArgs;
                }
                modified |= kModSettings;
                i+=2;
            } else if( (0 == strcmp(argv[i], ARG_SPINDOWN)) ||
                       (0 == strcmp(argv[i], ARG_DISKSLEEP)))
            {
                // either 'spindown' or 'disksleep' argument sets minutes until disk spindown
                if(-1 == checkAndSetIntValue( argv[i+1], 
                                                CFSTR(kIOPMDiskSleepKey), 
                                                apply, false, kNoMultiplier,
                                                ac, battery, ups))
                {
                    return kParseBadArgs;                    
                }
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_SLEEP))
            {
                if(-1 == checkAndSetIntValue( argv[i+1], 
                                                CFSTR(kIOPMSystemSleepKey), 
                                                apply, false, kNoMultiplier,
                                                ac, battery, ups))
                {
                    return kParseBadArgs;
                }
                modified |= kModSettings;
                i+=2;
            } else if( (0 == strcmp(argv[i], ARG_REDUCE))
                        || (0 == strcmp(argv[i], ARG_REDUCE2)) )
            {
                if(-1 == checkAndSetIntValue( argv[i+1], 
                                                CFSTR(kIOPMReduceSpeedKey), 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_WOMP))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMWakeOnLANKey), 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_DPS))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMDynamicPowerStepKey), 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_RING))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMWakeOnRingKey), 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_AUTORESTART))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMRestartOnPowerLossKey), 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_WAKEONACCHANGE))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMWakeOnACChangeKey), 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_POWERBUTTON))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMSleepOnPowerButtonKey),                                                 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))

                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_LIDWAKE))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMWakeOnClamshellKey), 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_REDUCEBRIGHT))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMReduceBrightnessKey), 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_SLEEPUSESDIM))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMDisplaySleepUsesDimKey),
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if((0 == strcmp(argv[i], ARG_MOTIONSENSOR))
                   || (0 == strcmp(argv[i], ARG_MOTIONSENSOR2)) )
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMMobileMotionModuleKey), 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_TTYKEEPAWAKE))
            {            
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMTTYSPreventSleepKey), 
                                                apply, true, kNoMultiplier, 
                                                ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_DISABLESLEEP))
            {
                char            *endptr = NULL;
                long            val;

                if( argv[i+1] ) 
                {
                    val = strtol(argv[i+1], &endptr, 10);
                    
                    if(0 != *endptr)
                    {
                        // the string contained some non-numerical characters - bail
                        return kParseBadArgs;
                    }
                    
                    // Any non-zero value of val (preferably 1) means DISABLE sleep.
                    // A zero value means ENABLE sleep.
    
                    CFDictionarySetValue( local_system_power_settings, 
                                          kIOPMSleepDisabledKey, 
                                          val ? kCFBooleanTrue : kCFBooleanFalse);
    
                    modified |= kModSystemSettings;
                }
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_HALTLEVEL))
            {
                if(-1 == setUPSValue(argv[i+1], CFSTR(kIOPMDefaultUPSThresholds), 
                                                CFSTR(kIOUPSShutdownAtLevelKey), 
                                                apply, local_ups_settings))
                    return kParseBadArgs;
                modified |= kModUPSThresholds;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_HALTAFTER))
            {
                if(-1 == setUPSValue(argv[i+1], CFSTR(kIOPMDefaultUPSThresholds), 
                                                CFSTR(kIOUPSShutdownAfterMinutesOn), 
                                                apply, local_ups_settings))
                    return kParseBadArgs;
                modified |= kModUPSThresholds;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_HALTREMAIN))
            {
                if(-1 == setUPSValue(argv[i+1], CFSTR(kIOPMDefaultUPSThresholds), 
                                                CFSTR(kIOUPSShutdownAtMinutesLeft), 
                                                apply, local_ups_settings))
                    return kParseBadArgs;
                modified |= kModUPSThresholds;
                i+=2;
            } else {
                // Determine if this is a number.
                // If so, the user is setting the active power profile
                // for the power sources specified in applyTo
                // If not, bail with bad input.
                char                    *endptr = 0;
                long                    val;
                CFNumberRef             prof_val = 0;
            
                val = strtol(argv[i], &endptr, 10);
            
                if(0 != *endptr || (val < -1) || (val > 4) )
                {
                    // the string contained some non-numerical characters
                    // or the profile number was out of the expected range
                    return kParseBadArgs;
                }
                prof_val = CFNumberCreate(0, kCFNumberIntType, &val);
                if(!prof_val) return kParseInternalError;
                if(!local_profiles) return kParseInternalError;
                // setting a profile
                if(ApplyToBattery & apply) {
                    CFDictionarySetValue(local_profiles, CFSTR(kIOPMBatteryPowerKey), prof_val);
                }
                if(ApplyToCharger & apply) {
                    CFDictionarySetValue(local_profiles, CFSTR(kIOPMACPowerKey), prof_val);                
                }
                if(ApplyToUPS & apply) {
                    CFDictionarySetValue(local_profiles, CFSTR(kIOPMUPSPowerKey), prof_val);
                }
                CFRelease(prof_val);
                i++;
                modified |= kModProfiles;
           }
        } // if
    } // while

//exit:
    if(modified & kModSettings) {
        *settings = local_settings;    
        *modified_power_sources = apply;
    } else {
        if(local_settings) CFRelease(local_settings);
    }

    if(modified & kModSystemSettings) {
        *system_power_settings = local_system_power_settings;
    } else {
        if(local_system_power_settings) CFRelease(local_system_power_settings);
    }

    if(modified & kModProfiles) {
        *active_profiles = local_profiles;
    } else {
        if(local_profiles) CFRelease(local_profiles);
    }
    
    if(modified & kModUPSThresholds) {
        if(ups_thresholds) *ups_thresholds = local_ups_settings;    
    } else {
        if(local_ups_settings) CFRelease(local_ups_settings);
    }

    if(modified & kModSched) {
        *scheduled_event = local_scheduled_event;
        *cancel_scheduled_event = local_cancel_event;
    }
    
    if(modified & kModRepeat) {
        *repeating_event = local_repeating_event;
        *cancel_repeating_event = local_cancel_repeating;
    } else {
        if(local_repeating_event) CFRelease(local_repeating_event);
    }
    
bail:
    return ret;
}

// int arePowerSourceSettingsInconsistent(CFDictionaryRef)
// Function - determine if the settings will produce the "intended" idle sleep consequences
// Parameter - The CFDictionary contains energy saver settings of 
//             CFStringRef keys and CFNumber/CFBoolean values
// Return - non-zero bitfield, each bit indicating a setting inconsistency
//          0 if settings will produce expected result 
//         -1 in case of other error
static int arePowerSourceSettingsInconsistent(CFDictionaryRef set)
{
    int                 sleep_time, disk_time, dim_time;
    CFNumberRef         num;
    int                 ret = 0;

    num = isA_CFNumber(CFDictionaryGetValue(set, CFSTR(kIOPMSystemSleepKey)));
    if(!num || !CFNumberGetValue(num, kCFNumberIntType, &sleep_time)) return -1;
    
    num = isA_CFNumber(CFDictionaryGetValue(set, CFSTR(kIOPMDisplaySleepKey)));
    if(!num || !CFNumberGetValue(num, kCFNumberIntType, &dim_time)) return -1;

    num = isA_CFNumber(CFDictionaryGetValue(set, CFSTR(kIOPMDiskSleepKey)));
    if(!num || !CFNumberGetValue(num, kCFNumberIntType, &disk_time)) return -1;

    // For system sleep to occur around the time you set it to, the disk and display
    // sleep timers must conform to these rules:
    // 1. display sleep <= system sleep
    // 2. If system sleep != Never, then disk sleep can not be == Never
    //    2a. It is, however, OK for disk sleep > system sleep. A funky hack in
    //        the kernel IOPMrootDomain allows this.
    // and note: a time of zero means "never power down"
    
    if(sleep_time != 0)
    {
        if(dim_time > sleep_time || dim_time == 0) ret |= kInconsistentDisplaySetting;
    
        if(disk_time == 0) ret |= kInconsistentDiskSetting;
    }
    
    return ret;
}

// void checkSettingConsistency(CFDictionaryRef)
// Checks ES settings profile by profile
// Prints a user warning if any idle timer settings violate the kernel's assumptions
// about idle, disk, and display sleep timers.
// Parameter - a CFDictionary of energy settings "profiles", usually one per power supply
static void checkSettingConsistency(CFDictionaryRef profiles)
{
    int                 num_profiles;
    int                 i;
    int                 ret;
    char                buf[100];
    CFStringRef         *keys;
    CFDictionaryRef     *values;
    
    num_profiles = CFDictionaryGetCount(profiles);
    keys = (CFStringRef *)malloc(num_profiles * sizeof(void *));
    values = (CFDictionaryRef *)malloc(num_profiles * sizeof(void *));
    if(!keys || !values) return;
    CFDictionaryGetKeysAndValues(profiles, (const void **)keys, (const void **)values);
    
// TODO: Warn user if 1) they have just changed the custom settings
//       2) their active profile is _NOT_ custom
    
    for(i=0; i<num_profiles; i++)
    {
        // Check settings profile by profile
        if( isA_CFDictionary(values[i])
            && (ret = arePowerSourceSettingsInconsistent(values[i])) )
        {
            // get profile name
            if(!CFStringGetCString(keys[i], buf, 100, kCFStringEncodingMacRoman)) break;
            
            fprintf(stderr, "Warning: Idle sleep timings for \"%s\" may not behave as expected.\n", buf);
            if(ret & kInconsistentDisplaySetting)
            {
                fprintf(stderr, "- Display sleep should have a lower timeout than system sleep.\n");
            }
            if(ret & kInconsistentDiskSetting)
            {
                fprintf(stderr, "- Disk sleep should be non-zero whenever system sleep is non-zero.\n");
            }
            fflush(stderr);        
        }
    }

    free(keys);
    free(values);
}

static ScheduledEventReturnType *scheduled_event_struct_create(void)
{
    ScheduledEventReturnType *ret = malloc(sizeof(ScheduledEventReturnType));
    if(!ret) return NULL;
    ret->who = 0;
    ret->when = 0;
    ret->which = 0;
    return ret;
}

static void scheduled_event_struct_destroy(
    ScheduledEventReturnType *free_me)
{
    if( (0 == free_me) ) return;
    if(free_me->who) {
        CFRelease(free_me->who);
        free_me->who = 0;
    }
    if(free_me->when) {
        CFRelease(free_me->when);
        free_me->when = 0;
    }
    if((free_me)->which) {
        CFRelease(free_me->which);
        free_me->which = 0;
    }
    free(free_me);
}


static IOReturn _pm_connect(mach_port_t *newConnection)
{
    kern_return_t       kern_result = KERN_SUCCESS;
    
    if(!newConnection) return kIOReturnBadArgument;

    // open reference to PM configd
    kern_result = bootstrap_look_up(bootstrap_port, 
            kIOPMServerBootstrapName, newConnection);    
    if(KERN_SUCCESS != kern_result) {
        return kIOReturnError;
    }
    return kIOReturnSuccess;
}

static IOReturn _pm_disconnect(mach_port_t connection)
{
    if(!connection) return kIOReturnBadArgument;
    mach_port_destroy(mach_task_self(), connection);
    return kIOReturnSuccess;
}
