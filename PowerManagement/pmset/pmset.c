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
#include <SystemConfiguration/SystemConfiguration.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#include <string.h>
#include <ctype.h>

/* 
 * This is the command line interface to Energy Saver Preferences in
 * /var/db/SystemConfiguration/com.apple.PowerManagement.xml
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

// CLArgs
#define	ARG_DIM			"dim"
#define ARG_SLEEP		"sleep"
#define ARG_SPINDOWN		"spindown"
#define ARG_REDUCE		"slower"
#define ARG_WOMP		"womp"
#define ARG_BOOT        "boot"

/* unimplemented: */
#define ARG_WAKEAT		"wakeat"
#define ARG_SLEEPAT		"sleepat"
#define ARG_SHUTDOWNAT		"shutdownat"
#define ARG_POWERONAT		"poweronat"
    

enum ArgumentType {
    ApplyToBattery = 1,
    ApplyToCharger = 2
};

static void usage(void)
{
    printf("Usage:  pmset [-b | -c | -a] <action> <minutes> [[<opts>] <action> <minutes>...]\n");
    printf("                -c adjust settings used while connected to a charger\n");
    printf("                -b adjust settings used when running off a battery\n");
    printf("                -a (default) adjust settings for both\n");
    printf("                <action> is one of: dim, sleep, spindown, slower\n");
    printf("                eg. pmset -c dim 5 sleep 15 -b dim 3 spindown 5 sleep 8\n");
//    printf("         pmset <setting> <time> [<setting> <time>]\n");
//    printf("                <setting> is one of: wakeat, sleepat\n");
//    printf("                <time> is in 24 hour format\n");
    printf("         pmset womp [ on | off ]\n");
}

static IOReturn tell_boot_complete(void) {
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
    
    ret = IORegistryEntrySetCFProperty(root_domain, CFSTR("System Boot Complete"), kCFBooleanTrue);
    
    IOObjectRelease(root_domain);
    IOObjectRelease(it);
    IOObjectRelease(masterPort);
    return ret;
}

static int parseArgs(int argc, char* argv[], CFMutableDictionaryRef settings)
{
    int i = 1;
    int j;
    int val;
    int apply = 0;
    IOReturn kr;
    CFNumberRef		cfnum = NULL;
    CFMutableDictionaryRef	battery = NULL;
    CFMutableDictionaryRef	ac = NULL;

    if(argc == 1)
        return -1;
    
    if(!settings) {
        return -1;
    }
    
    // Either battery or AC settings may not exist if the system doesn't support it.
    battery = (CFMutableDictionaryRef)CFDictionaryGetValue(settings, CFSTR(kIOPMBatteryPowerKey));
    ac = (CFMutableDictionaryRef)CFDictionaryGetValue(settings, CFSTR(kIOPMACPowerKey));
    
    // Unless specified, apply changes to both battery and AC
    if(battery) apply |= ApplyToBattery;
    if(ac) apply |= ApplyToCharger;
    
    while(i < argc)
    {
        // I only speak lower case.
        for(j=0; j<strlen(argv[i]); j++)
        {
            tolower(argv[i][j]);
        }
    
        if(argv[i][0] == '-')
        {
        // Process -a/-b/-c arguments
            apply = 0;
            switch (argv[i][1])
            {
                case 'a':
                    if(battery) apply |= ApplyToBattery;
                    if(ac) apply |= ApplyToCharger;
                    break;
                case 'b':
                    if(battery) apply = ApplyToBattery;
                    break;
                case 'c':
                    if(ac) apply = ApplyToCharger;
                    break;
                default:
                    // bad!
                    return -1;
                    break;
            }
            
            i++;
        } else
        {
        // Process the settings
            if(0 == strcmp(argv[i], ARG_BOOT))
            {
                // Tell kernel power management that bootup is complete
                kr = tell_boot_complete();
                if(kr != kIOReturnSuccess) {
                    printf("pmset: Error 0x%x setting boot property\n", kr);
                }

                i++;
            } else if(0 == strcmp(argv[i], ARG_DIM))
            {
                val = atoi(argv[i+1]);
                cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &val);
                if(apply & ApplyToBattery)
                    CFDictionarySetValue(battery, CFSTR(kIOPMDisplaySleepKey), cfnum);
                if(apply & ApplyToCharger)
                    CFDictionarySetValue(ac, CFSTR(kIOPMDisplaySleepKey), cfnum);
                CFRelease(cfnum);                    
                i+=2;

            } else if(0 == strcmp(argv[i], ARG_SPINDOWN))
            {
                val = atoi(argv[i+1]);
                cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &val);
                if(apply & ApplyToBattery)
                    CFDictionarySetValue(battery, CFSTR(kIOPMDiskSleepKey), cfnum);
                if(apply & ApplyToCharger)
                    CFDictionarySetValue(ac, CFSTR(kIOPMDiskSleepKey), cfnum);
                CFRelease(cfnum);
                i+=2;
                                    
            } else if(0 == strcmp(argv[i], ARG_SLEEP))
            {
                val = atoi(argv[i+1]);
                cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &val);
                if(apply & ApplyToBattery)
                    CFDictionarySetValue(battery, CFSTR(kIOPMSystemSleepKey), cfnum);
                if(apply & ApplyToCharger)
                    CFDictionarySetValue(ac, CFSTR(kIOPMSystemSleepKey), cfnum);
                CFRelease(cfnum);
                i+=2;
                    
            } else if(0 == strcmp(argv[i], ARG_REDUCE))
            {
                val = atoi(argv[i+1]);
                cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &val);
                if(apply & ApplyToBattery)
                    CFDictionarySetValue(battery, CFSTR(kIOPMReduceSpeedKey), cfnum);
                if(apply & ApplyToCharger)
                    CFDictionarySetValue(ac, CFSTR(kIOPMReduceSpeedKey), cfnum);
                CFRelease(cfnum);
                i+=2;

            } else if(0 == strcmp(argv[i], ARG_WOMP))
            {
                val = atoi(argv[i+1]);
                cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &val);
                if(apply & ApplyToBattery)
                    CFDictionarySetValue(battery, CFSTR(kIOPMWakeOnLANKey), cfnum);
                if(apply & ApplyToCharger)
                    CFDictionarySetValue(ac, CFSTR(kIOPMWakeOnLANKey), cfnum);
                CFRelease(cfnum);
                i+=2;

            } else if(0 == strcmp(argv[i], ARG_SLEEPAT))
            {
                printf("pmset: %s is not implemented!!!\n", argv[i]);
                return -1;
            
            } else if(0 == strcmp(argv[i], ARG_WAKEAT))
            {
                printf("pmset: %s is not implemented!!!\n", argv[i]);
                return -1;
                
            } else if(0 == strcmp(argv[i], ARG_SHUTDOWNAT))
            {
                printf("pmset %s is not implemented!!!\n", argv[i]);
                return -1;
    
            } else if(0 == strcmp(argv[i], ARG_POWERONAT))
            {
                printf("pmset %s is not implemented!!!\n", argv[i]);
                return -1;
    
            } else return -1;
        } // if
    } // while
    
    if(battery) CFDictionarySetValue(settings, CFSTR(kIOPMBatteryPowerKey), battery);
    if(ac) CFDictionarySetValue(settings, CFSTR(kIOPMACPowerKey), ac);
    
    return 0;
}

int main(int argc, char *argv[]) {
    IOReturn			ret;
    CFMutableDictionaryRef	es;
                
    if(!(es=IOPMCopyPMPreferences()))
    {
        printf("pmset: error getting profiles\n");
    }
    
    if(-1 == parseArgs(argc, argv, es))
    {
        //printf("pmset: error in parseArgs!\n");
        usage();
        return 0;
    }

    if(kIOReturnSuccess != (ret = IOPMSetPMPreferences(es)))
    {
        if(ret == kIOReturnNotPrivileged)
            printf("\'%s\' must be run as root...\n", argv[0]);
        if(ret == kIOReturnError)
            printf("Error writing preferences to disk\n");
        exit(1);
    }

    CFRelease(es);
    return 0;
}
