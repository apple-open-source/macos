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
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMUPSPrivate.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>

#include <string.h>
#include <ctype.h>

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

// Scheduling options
#define ARG_SCHEDULE        "schedule"
#define ARG_SCHED           "sched"
#define ARG_REPEAT          "repeat"
#define ARG_CANCEL          "cancel"
//#define ARG_SLEEP         "sleep"
#define ARG_SHUTDOWN        "shutdown"
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
#define ARG_LIVE            "live"
#define ARG_SCHED           "sched"
#define ARG_UPS             "ups"
#define ARG_SYS_PROFILES    "profiles"
#define ARG_BATT            "batt"
#define ARG_PS              "ps"
#define ARG_PSLOG           "pslog"

// special
#define ARG_BOOT            "boot"
#define ARG_FORCE           "force"

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

// return values for idleSettingsNotConsistent
#define kInconsistentDisplaySetting     1
#define kInconsistentDiskSetting        2
#define kConsistentSleepSettings        0

// day-of-week constants for repeating power events
#define daily_mask              (kIOPMMonday|kIOPMTuesday|kIOPMWednesday|kIOPMThursday|kIOPMFriday|kIOPMSaturday|kIOPMSunday)
#define weekday_mask            (kIOPMMonday|kIOPMTuesday|kIOPMWednesday|kIOPMThursday|kIOPMFriday)
#define weekend_mask            (kIOPMSaturday|kIOPMSunday)

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

typedef struct {
    CFStringRef         who;
    CFDateRef           when;
    CFStringRef         which;
} ScheduledEventReturnType;


// function declarations
static void usage(void);
static IOReturn setRootDomainProperty(CFStringRef key, CFTypeRef val);
static void show_pm_settings_dict(CFDictionaryRef d, int indent, int log_overrides);
static void show_supported_pm_features(void);
static void show_disk_pm_settings(void);
static void show_live_pm_settings(void);
static void show_ups_settings(void);
static void show_active_profiles(void);
static void show_system_profiles(void);
static void show_scheduled_events(void);
static void show_power_sources(int which);
static void log_ps_change_handler(void *);
static int log_power_source_changes(int which);
static void print_setting_value(CFTypeRef a);
static void print_cpu_override_pids(void);
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
static int checkAndSetIntValue(char *valstr, CFStringRef settingKey, int apply,
         int isOnOffSetting, CFMutableDictionaryRef ac, 
         CFMutableDictionaryRef batt, CFMutableDictionaryRef ups);
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
        int                         *cancel_scheduled_event);
static int parseRepeatingEvent(
        char                        **argv,
        int                         *num_args_parsed,
        CFMutableDictionaryRef      local_repeating_event, 
        int                         *local_cancel_repeating);
static int parseArgs(
        int argc, 
        char* argv[], 
        CFDictionaryRef             *settings,
        int                         *modified_power_sources,
        int                         *force_activate_settings,
        CFDictionaryRef             *active_profiles,
        CFDictionaryRef             *ups_thresholds,
        ScheduledEventReturnType    **scheduled_event,
        int                         *cancel_scheduled_event,
        CFDictionaryRef             *repeating_event,
        int                         *cancel_repeating_event);

//****************************
//****************************
//****************************

static void usage(void)
{
    printf("Usage:  pmset [-b | -c | -u | -a] <action> <minutes> [<action> <minutes>...]\n");
    printf("        pmset -g [disk | cap | live | sched | ups | batt\n");
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

static IOReturn setRootDomainProperty(CFStringRef key, CFTypeRef val) {
    mach_port_t                 masterPort;
    io_iterator_t               it;
    io_registry_entry_t         root_domain;
    IOReturn                    ret;
    
    IOMasterPort(bootstrap_port, &masterPort);
    if(!masterPort) return kIOReturnError;
    IOServiceGetMatchingServices(masterPort, IOServiceNameMatching("IOPMrootDomain"), &it);
    if(!it) return kIOReturnError;
    root_domain = (io_registry_entry_t)IOIteratorNext(it);
    if(!root_domain) return kIOReturnError;
    
    ret = IORegistryEntrySetCFProperty(root_domain, key, val);
    
    IOObjectRelease(root_domain);
    IOObjectRelease(it);
    IOObjectRelease(masterPort);
    return ret;
}

static void print_setting_value(CFTypeRef a)
{
    int n;
    
    if(isA_CFNumber(a))
    {
        CFNumberGetValue(a, kCFNumberIntType, (void *)&n);
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

static void show_pm_settings_dict(CFDictionaryRef d, int indent, int show_overrides)
{
    int                     count;
    int                     i;
    int                     j;
    int                     cpu_overrides;
    char                    *ps;
    CFStringRef             *keys;
    CFTypeRef               *vals;

    count = CFDictionaryGetCount(d);
    keys = (CFStringRef *)malloc(count * sizeof(void *));
    vals = (CFTypeRef *)malloc(count * sizeof(void *));
    if(!keys || !vals) return;
    CFDictionaryGetKeysAndValues(d, (const void **)keys, (const void **)vals);

    for(i=0; i<count; i++)
    {
        cpu_overrides = 0;
        ps = (char *)CFStringGetCStringPtr(keys[i], 0);
        if(!ps) continue; // with for loop
        
        for(j=0; j<indent;j++) printf(" ");

        if (strcmp(ps, kIOPMDisplaySleepKey) == 0)
                printf(" displaysleep\t");  
        else if (strcmp(ps, kIOPMDiskSleepKey) == 0)
                printf(" disksleep\t");  
        else if (strcmp(ps, kIOPMSystemSleepKey) == 0)
                printf(" sleep\t\t");  
        else if (strcmp(ps, kIOPMWakeOnLANKey) == 0)
                printf(" womp\t\t");  
        else if (strcmp(ps, kIOPMWakeOnRingKey) == 0)
                printf(" ring\t\t");  
        else if (strcmp(ps, kIOPMRestartOnPowerLossKey) == 0)
                printf(" autorestart\t");  
        else if (strcmp(ps, kIOPMReduceSpeedKey) == 0) {
                printf(" reduce\t\t");
                if(show_overrides) cpu_overrides = 1;
        } else if (strcmp(ps, kIOPMDynamicPowerStepKey) == 0) {
                printf(" dps\t\t");  
                if(show_overrides) cpu_overrides = 1;
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
        else {
                // unknown setting
                printf("%s\t", ps);
        }
  
        print_setting_value(vals[i]);  

        if(cpu_overrides) print_cpu_override_pids();

        printf("\n");
    }
    free(keys);
    free(vals);
}

static void print_cpu_override_pids(void)
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
    assertion_value = CFDictionaryGetValue(assertions_state, kIOPMCPUBoundAssertion);
    if(assertion_value) CFNumberGetValue(assertion_value, kCFNumberIntType, &cpus_forced);
    // And exit if the assertion isn't forced:
    if(!cpus_forced) goto bail;

    // Find out which pids have asserted this CPU bound assertion and print 'em out
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
            if(!tmp_dict) {
                continue;
            }
            tmp_type = CFDictionaryGetValue(tmp_dict, kIOPMAssertionTypeKey);
            tmp_val = CFDictionaryGetValue(tmp_dict, kIOPMAssertionValueKey);
            if(!tmp_type || !tmp_val) {
                continue;
            }
            CFNumberGetValue(tmp_val, kCFNumberIntType, &val);
            if( (kCFCompareEqualTo == 
                 CFStringCompare(tmp_type, kIOPMCPUBoundAssertion, 0)) &&
                (kIOPMAssertionEnable == val) )
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
        show_pm_settings_dict(values[i], indent, 0);
    }

    free(keys);
    free(values);
}

static void show_disk_pm_settings(void)
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
    show_pm_settings_dict(live, 0, 1);

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
    
    if(d = CFDictionaryGetValue(thresholds, CFSTR(kIOUPSShutdownAtLevelKey)))
    {
        b = CFDictionaryGetValue(d, CFSTR(kIOUPSShutdownLevelEnabledKey));
        n_val = CFDictionaryGetValue(d, CFSTR(kIOUPSShutdownLevelValueKey));
        CFNumberGetValue(n_val, kCFNumberIntType, &val);
        printf("  %s\t%s\t%d\n", ARG_HALTLEVEL, (kCFBooleanTrue==b)?"on":"off", val);        
    }
    if(d = CFDictionaryGetValue(thresholds, CFSTR(kIOUPSShutdownAfterMinutesOn)))
    {
        b = CFDictionaryGetValue(d, CFSTR(kIOUPSShutdownLevelEnabledKey));
        n_val = CFDictionaryGetValue(d, CFSTR(kIOUPSShutdownLevelValueKey));
        CFNumberGetValue(n_val, kCFNumberIntType, &val);
        printf("  %s\t%s\t%d\n", ARG_HALTAFTER, (kCFBooleanTrue==b)?"on":"off", val);        
    }
    if(d = CFDictionaryGetValue(thresholds, CFSTR(kIOUPSShutdownAtMinutesLeft)))
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
    char                _name[60];
    int                 _charge, _capacity;
    int                 _hours, _minutes;
    int                 _charging;
    
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
        printf("Currenty drawing from '%s'\n", strbuf);
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
        
        if(name) CFStringGetCString(name, _name, 60, kCFStringEncodingMacRoman);
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
            if(show_time_estimate && remaining)
                if(-1 != _minutes)
                    printf("; %d:%d%d remaining ", _hours, _minutes/10, _minutes%10);
                else printf("; (no estimate) ");
            printf("\n"); fflush(stdout);
        } else {
            printf("(removed)\n");        
        }
    }

exit:
    if(ps_info) CFRelease(ps_info);  
    if(list) CFRelease(list);
    return;
}

static void log_ps_change_handler(void *info)
{
    CFDateFormatterRef  date_format;
    CFTimeZoneRef       tz;
    CFStringRef         time_date;
    CFLocaleRef         loc;
    char                _date[60];
    int                 which = (int)info;
    
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

    show_power_sources(which);
}

static int log_power_source_changes(int which)
{
    CFRunLoopSourceRef          rls = NULL;
    rls = IOPSNotificationCreateRunLoopSource(log_ps_change_handler, which);
    if(!rls) return kParseInternalError;
    printf("pmset is in logging mode now. Hit ctrl-c to exit.\n");
    // and show initial power source state:
    log_ps_change_handler(which);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
    CFRunLoopRun();
}


static int checkAndSetIntValue(char *valstr, CFStringRef settingKey, int apply,
                int isOnOffSetting, CFMutableDictionaryRef ac, CFMutableDictionaryRef batt, CFMutableDictionaryRef ups)
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
    if(isOnOffSetting) val = (val?1:0);

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
    int                         *local_cancel_repeating)
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

        *local_cancel_repeating = 1;
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
    int                         *cancel_scheduled_event)
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

        *cancel_scheduled_event = 1;
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
 *  active_profiles: Changes the 
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
    int                         *force_activate_settings,
    CFDictionaryRef             *active_profiles,
    CFDictionaryRef             *ups_thresholds,
    ScheduledEventReturnType    **scheduled_event,
    int                         *cancel_scheduled_event,
    CFDictionaryRef             *repeating_event,
    int                         *cancel_repeating_event)
{
    int                         i = 1;
    int                         j;
    int                         apply = 0;
    int                         length;
    int                         ret = kParseSuccess;
    int                         modified = 0;
    IOReturn                    kr;
    ScheduledEventReturnType    *local_scheduled_event = 0;
    int                         local_cancel_event = 0;
    CFDictionaryRef             tmp_repeating = 0;
    CFMutableDictionaryRef      local_repeating_event = 0;
    int                         local_cancel_repeating = 0;
    CFDictionaryRef             tmp_profiles = 0;
    CFMutableDictionaryRef      local_profiles = 0;
    CFDictionaryRef             tmp_ups_settings = 0;
    CFMutableDictionaryRef      local_ups_settings;
    CFDictionaryRef             tmp_settings = 0;
    CFMutableDictionaryRef      local_settings = 0;
    CFDictionaryRef             tmp_battery = 0;
    CFMutableDictionaryRef      battery = 0;
    CFDictionaryRef             tmp_ac = 0;
    CFMutableDictionaryRef      ac = 0;
    CFDictionaryRef             tmp_ups = 0;
    CFMutableDictionaryRef      ups = 0;

    if(argc == 1) {
        return kParseBadArgs;
    }


/***********
 * Setup mutable PM preferences
 ***********/
    tmp_settings = IOPMCopyPMPreferences();    
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
                        show_active_profiles();
                        show_live_pm_settings();
                    } else if(!strcmp(argv[i], ARG_DISK))        
                    {
                        show_disk_pm_settings();
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
                if(kr != kIOReturnSuccess) {
                    fprintf(stderr, "pmset: Error 0x%x setting boot property\n", kr);
                    fflush(stderr);
                }

                i++;
            } else if(0 == strcmp(argv[i], ARG_FORCE))
            {
                *force_activate_settings = 1;
                i++;
            } else if( (0 == strcmp(argv[i], ARG_DIM)) ||
                       (0 == strcmp(argv[i], ARG_DISPLAYSLEEP)) )
            {
                // either 'dim' or 'displaysleep' argument sets minutes until display dims
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMDisplaySleepKey), apply, 0, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if( (0 == strcmp(argv[i], ARG_SPINDOWN)) ||
                       (0 == strcmp(argv[i], ARG_DISKSLEEP)))
            {
                // either 'spindown' or 'disksleep' argument sets minutes until disk spindown
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMDiskSleepKey), apply, 0, ac, battery, ups))
                    return kParseBadArgs;                    
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_SLEEP))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMSystemSleepKey), apply, 0, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if( (0 == strcmp(argv[i], ARG_REDUCE))
                        || (0 == strcmp(argv[i], ARG_REDUCE2)) )
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMReduceSpeedKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_WOMP))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMWakeOnLANKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_DPS))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMDynamicPowerStepKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_RING))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMWakeOnRingKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_AUTORESTART))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMRestartOnPowerLossKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_WAKEONACCHANGE))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMWakeOnACChangeKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_POWERBUTTON))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMSleepOnPowerButtonKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_LIDWAKE))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMWakeOnClamshellKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_REDUCEBRIGHT))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMReduceBrightnessKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_SLEEPUSESDIM))
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMDisplaySleepUsesDimKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if((0 == strcmp(argv[i], ARG_MOTIONSENSOR))
                   || (0 == strcmp(argv[i], ARG_MOTIONSENSOR2)) )
            {
                if(-1 == checkAndSetIntValue(argv[i+1], CFSTR(kIOPMMobileMotionModuleKey), apply, 1, ac, battery, ups))
                    return kParseBadArgs;
                modified |= kModSettings;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_HALTLEVEL))
            {
                if(-1 == setUPSValue(argv[i+1], CFSTR(kIOPMDefaultUPSThresholds), CFSTR(kIOUPSShutdownAtLevelKey), apply, local_ups_settings))
                    return kParseBadArgs;
                modified |= kModUPSThresholds;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_HALTAFTER))
            {
                if(-1 == setUPSValue(argv[i+1], CFSTR(kIOPMDefaultUPSThresholds), CFSTR(kIOUPSShutdownAfterMinutesOn), apply, local_ups_settings))
                    return kParseBadArgs;
                modified |= kModUPSThresholds;
                i+=2;
            } else if(0 == strcmp(argv[i], ARG_HALTREMAIN))
            {
                if(-1 == setUPSValue(argv[i+1], CFSTR(kIOPMDefaultUPSThresholds), CFSTR(kIOUPSShutdownAtMinutesLeft), apply, local_ups_settings))
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


int main(int argc, char *argv[]) {
    IOReturn                        ret, ret1;
    CFDictionaryRef                 es_custom_settings = 0;
    int                             modified_power_sources = 0;
    int                             force_it = 0;
    CFDictionaryRef                 ups_thresholds = 0;
    CFDictionaryRef                 active_profiles = 0;
    ScheduledEventReturnType        *scheduled_event_return = 0;
    int                             cancel_scheduled_event = 0;
    CFDictionaryRef                 repeating_event_return = 0;
    int                             cancel_repeating_event = 0;
        
    ret = parseArgs(argc, argv, 
        &es_custom_settings, &modified_power_sources, &force_it,
        &active_profiles,
        &ups_thresholds,
        &scheduled_event_return, &cancel_scheduled_event,
        &repeating_event_return, &cancel_repeating_event);

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
    
    if(force_it && es_custom_settings)
    {
        ret = IOPMActivatePMPreference(es_custom_settings, CFSTR(kIOPMACPowerKey));

        if(kIOReturnSuccess != ret)
        {
            fprintf(stderr, "Error 0x%08x forcing Energy Saver preferences to activate\n", ret);
            fflush(stderr);
            exit(1);
        }        
        // explicitly return here. If this is a 'force' call we do _not_ want
        // to attempt to write any settings to the disk, or to try anything else1);
        // at all, as our environment may be sketchy (booted from install CD, etc.)
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
        // For each power source we modified settings for, flip that source's profile
        // to -1
        if(modified_power_sources & ApplyToCharger)
            CFDictionarySetValue(customize_active_profiles, CFSTR(kIOPMACPowerKey), neg1);
        if(modified_power_sources & ApplyToBattery)
            CFDictionarySetValue(customize_active_profiles, CFSTR(kIOPMBatteryPowerKey), neg1);
        if(modified_power_sources & ApplyToUPS)
            CFDictionarySetValue(customize_active_profiles, CFSTR(kIOPMUPSPowerKey), neg1);
            
        ret = IOPMSetActivePowerProfiles(customize_active_profiles);
        if(kIOReturnSuccess != ret) {
            printf("Error 0x%08x writing customized power profiles to disk\n", ret);
            exit(1);
        }
        CFRelease(neg1);
        CFRelease(tmp_dict);
        CFRelease(customize_active_profiles);
        
                
        // Print a warning to stderr if idle sleep settings won't produce expected result
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

    // Did the user change UPS settings too?
    if(ups_thresholds)
    {
        // Only write out UPS settings if thresholds were changed. 
        //      - UPS sleep timers & energy settings have already been written with IOPMSetPMPreferences() regardless.
        ret1 = IOPMSetUPSShutdownLevels(CFSTR(kIOPMDefaultUPSThresholds), ups_thresholds);
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
