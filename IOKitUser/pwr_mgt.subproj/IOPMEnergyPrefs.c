/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include "IOPMLib.h"
#include "IOPMLibPrivate.h"

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/network/IONetworkLib.h>
#include <IOKit/network/IOEthernetInterface.h>


#define kIOPMPrefsPath			    CFSTR("com.apple.PowerManagement.xml")
#define kIOPMAppName			    CFSTR("PowerManagement configd")

/* Default Energy Saver settings for IOPMCopyPMPreferences
 * 
 *      AC
 */
#define kACMinutesToDim	                5
#define kACMinutesToSpin                10
#define kACMinutesToSleep               10
#define kACWakeOnRing                   0
#define kACAutomaticRestart             0
#define kACWakeOnLAN                    1
#define kACReduceProcessorSpeed         0
#define kACDynamicPowerStep             1

/*
 *      Battery
 */
#define kBatteryMinutesToDim            5
#define kBatteryMinutesToSpin           5
#define kBatteryMinutesToSleep          5
#define kBatteryWakeOnRing              0
#define kBatteryAutomaticRestart        0
#define kBatteryWakeOnLAN               0
#define kBatteryReduceProcessorSpeed	0
#define kBatteryDynamicPowerStep        1

/* Keys for Cheetah Energy Settings shim
 */
#define kCheetahDimKey                          CFSTR("MinutesUntilDisplaySleeps")
#define kCheetahDiskKey                         CFSTR("MinutesUntilHardDiskSleeps")
#define kCheetahSleepKey                        CFSTR("MinutesUntilSystemSleeps")
#define kCheetahRestartOnPowerLossKey           CFSTR("RestartOnPowerLoss")
#define kCheetahWakeForNetworkAccessKey         CFSTR("WakeForNetworkAdministrativeAccess")
#define kCheetahWakeOnRingKey                   CFSTR("WakeOnRing")

#define kApplePMUUserClientMagicCookie 0x0101BEEF

/* IOPMAggressivenessFactors
 *
 * The form of data that the kernel understands.
 */
typedef struct {
    unsigned int 		fMinutesToDim;
    unsigned int 		fMinutesToSpin;
    unsigned int 		fMinutesToSleep;
    unsigned int		fWakeOnLAN;
    unsigned int        fWakeOnRing;
    unsigned int        fAutomaticRestart;
// FIXME: Got to add support for timed shutdown/restart
/*    
    unsigned int		SleepAtTime;
    unsigned int		WakeAtTime;
    unsigned int		ShutdownAtTime;
    unsigned int		PowerOnAtTime;
*/
} IOPMAggressivenessFactors;

static IOReturn IOPMFunctionIsAvailable(CFStringRef, CFStringRef);

static int getDefaultEnergySettings(CFMutableDictionaryRef sys)
{
    CFMutableDictionaryRef 	batt = NULL;
    CFMutableDictionaryRef 	ac = NULL;
    int			i;
    CFNumberRef		val;
    CFStringRef		key;
    
    batt = CFDictionaryCreateMutable(kCFAllocatorDefault, 
                0,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    ac = CFDictionaryCreateMutable(kCFAllocatorDefault,
                0,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    /* 
     * Populate default battery dictionary 
     */
    i = kBatteryMinutesToDim;
    key = CFSTR(kIOPMDisplaySleepKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(batt, key, val);
    CFRelease(val);

    i = kBatteryMinutesToSpin;
    key = CFSTR(kIOPMDiskSleepKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(batt, key, val);
    CFRelease(val);

    i = kBatteryMinutesToSleep;
    key = CFSTR(kIOPMSystemSleepKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(batt, key, val);
    CFRelease(val);

    i = kBatteryWakeOnRing;
    key = CFSTR(kIOPMWakeOnRingKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(batt, key, val);
    CFRelease(val);

    i = kBatteryAutomaticRestart;
    key = CFSTR(kIOPMRestartOnPowerLossKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(batt, key, val);
    CFRelease(val);

    i = kBatteryWakeOnLAN;
    key = CFSTR(kIOPMWakeOnLANKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(batt, key, val);
    CFRelease(val);

    i = kBatteryReduceProcessorSpeed;
    key = CFSTR(kIOPMReduceSpeedKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(batt, key, val);
    CFRelease(val);                

    i = kBatteryDynamicPowerStep;
    key = CFSTR(kIOPMDynamicPowerStepKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(batt, key, val);
    CFRelease(val);           
                
    /* 
     * Populate default AC dictionary 
     */
    i = kACMinutesToDim;
    key = CFSTR(kIOPMDisplaySleepKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(ac, key, val);
    CFRelease(val);

    i = kACMinutesToSpin;
    key = CFSTR(kIOPMDiskSleepKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(ac, key, val);
    CFRelease(val);

    i = kACMinutesToSleep;
    key = CFSTR(kIOPMSystemSleepKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(ac, key, val);
    CFRelease(val);

    i = kACWakeOnRing;
    key = CFSTR(kIOPMWakeOnRingKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(ac, key, val);
    CFRelease(val);

    i = kACAutomaticRestart;
    key = CFSTR(kIOPMRestartOnPowerLossKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(ac, key, val);
    CFRelease(val);

    i = kACWakeOnLAN;
    key = CFSTR(kIOPMWakeOnLANKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(ac, key, val);
    CFRelease(val);

    i = kACReduceProcessorSpeed;
    key = CFSTR(kIOPMReduceSpeedKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(ac, key, val);
    CFRelease(val);                

    i = kACDynamicPowerStep;
    key = CFSTR(kIOPMDynamicPowerStepKey);
    val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
    CFDictionaryAddValue(ac, key, val);
    CFRelease(val);        
    
    /*
     * Stuff the default values into the "system settings"
     */
    CFDictionaryAddValue(sys, CFSTR(kIOPMBatteryPowerKey), batt);
    CFDictionaryAddValue(sys, CFSTR(kIOPMACPowerKey), ac);

    CFRelease(batt);
    CFRelease(ac);

    return 0;
}

static io_registry_entry_t getCudaPMURef(void)
{
    io_iterator_t	            tmp = NULL;
    io_registry_entry_t	        cudaPMU = NULL;
    mach_port_t                 masterPort;
    
    IOMasterPort(bootstrap_port,&masterPort);
    
    // Search for PMU
    IOServiceGetMatchingServices(masterPort, IOServiceNameMatching("ApplePMU"), &tmp);
    if(tmp) {
        cudaPMU = IOIteratorNext(tmp);
        //if(cudaPMU) magicCookie = kAppleMagicPMUCookie;
        IOObjectRelease(tmp);
    }

    // No? Search for Cuda
    if(!cudaPMU) {
        IOServiceGetMatchingServices(masterPort, IOServiceNameMatching("AppleCuda"), &tmp);
        if(tmp) {
            cudaPMU = IOIteratorNext(tmp);
            //if(cudaPMU) magicCookie = kAppleMagicCudaCookie;
            IOObjectRelease(tmp);
        }
    }
    return cudaPMU;
}

static int sendEnergySettingsToKernel(IOPMAggressivenessFactors *p)
{
    mach_port_t 		        master_device_port;
    io_connect_t        		PM_connection = NULL;
    kern_return_t       	    	kr;
    IOReturn    		        err, ret;
    int				        type;
    CFDataRef                   	on;
    io_registry_entry_t      		cudaPMU;

    io_connect_t			connection;
    UInt32				i;
    

        kr = IOMasterPort(bootstrap_port,&master_device_port);
        if ( kr == kIOReturnSuccess ) 
        {
            PM_connection = IOPMFindPowerManagement(master_device_port);
            if ( !PM_connection ) 
            {
                printf("IOPMconfigd: Error connecting to Power Management\n"); fflush(stdout);
                return -1;
            }
        }

    type = kPMMinutesToDim;
    err = IOPMSetAggressiveness(PM_connection, type, p->fMinutesToDim);

    type = kPMMinutesToSpinDown;
    err = IOPMSetAggressiveness(PM_connection, type, p->fMinutesToSpin);

    type = kPMMinutesToSleep;
    err = IOPMSetAggressiveness(PM_connection, type, p->fMinutesToSleep);

    if(kIOReturnSuccess == IOPMFunctionIsAvailable(CFSTR(kIOPMWakeOnLANKey), NULL))
    {
        type = kPMEthernetWakeOnLANSettings;
        err = IOPMSetAggressiveness(PM_connection, type, p->fWakeOnLAN);
    }
    
    IOServiceClose(PM_connection);
    
    // Wake On Ring
    if(kIOReturnSuccess == IOPMFunctionIsAvailable(CFSTR(kIOPMWakeOnRingKey), NULL))
    {
        cudaPMU = getCudaPMURef();
        ret = IOServiceOpen((io_service_t)cudaPMU, mach_task_self(), kApplePMUUserClientMagicCookie, &connection);
    
        if(p->fWakeOnRing) i = 0xFFFFFFFF;
        else i = 0x0;
        on = CFDataCreate(kCFAllocatorDefault, (void *)&i, 4); 
               
        ret = IOConnectSetCFProperty(connection, CFSTR("WakeOnRing"), on);
        CFRelease(on);
        IOServiceClose(connection);
        IOObjectRelease(cudaPMU);
    }
    
    // Automatic Restart On Power Loss, aka FileServer mode
    if(kIOReturnSuccess == IOPMFunctionIsAvailable(CFSTR(kIOPMRestartOnPowerLossKey), NULL))
    {
        cudaPMU = getCudaPMURef();
        ret = IOServiceOpen((io_service_t)cudaPMU, mach_task_self(), kApplePMUUserClientMagicCookie, &connection);
    
        if(p->fAutomaticRestart) i = 0xFFFFFFFF;
        else i = 0x0;
        on = CFDataCreate(kCFAllocatorDefault, (void *)&i, 4);
        
        ret = IOConnectSetCFProperty(connection, CFSTR("FileServer"), on);
        CFRelease(on);
        IOServiceClose(connection);
        IOObjectRelease(cudaPMU);
    }
    
    /* PowerStep and Reduce Processor Speed are handled by a separate configd plugin that's
       watching the SCDynamicStore key State:/IOKit/PowerManagement/CurrentSettings. Changes
       to the settings notify the configd plugin, which then activates th processor speed settings.
       Note that IOPMActivatePMPreference updates that key in the SCDynamicStore when we activate
       new settings.
    */

    return 0;
}

/* RY: Added macro to make sure we are using
   the appropriate object */
#define GetAggressivenessValue(obj, type, ret)	\
do {						\
    if (isA_CFNumber(obj)){			\
        CFNumberGetValue(obj, type, &ret);	\
        break;					\
    }						\
    else if (isA_CFBoolean(obj)){		\
        ret = CFBooleanGetValue(obj);		\
        break;					\
    }						\
} while (false);

/* For internal use only */
static int getAggressivenessFactorsFromProfile(CFDictionaryRef System, CFStringRef prof, IOPMAggressivenessFactors *agg)
{
    CFDictionaryRef p = NULL;

    if( !(p = CFDictionaryGetValue(System, prof)) )
    {
        printf("IOPMconfigd: error getting agg factors from profile!\n");
        return -1;
    }

    if(!agg) return -1;
    
    /*
     * Extract battery settings into s->battery
     */
    
    // dim
    GetAggressivenessValue(CFDictionaryGetValue(p, CFSTR(kIOPMDisplaySleepKey)),
                           kCFNumberSInt32Type, agg->fMinutesToDim);
    
    // spin down
    GetAggressivenessValue(CFDictionaryGetValue(p, CFSTR(kIOPMDiskSleepKey)),
                           kCFNumberSInt32Type, agg->fMinutesToSpin);

    // sleep
    GetAggressivenessValue(CFDictionaryGetValue(p, CFSTR(kIOPMSystemSleepKey)),
                           kCFNumberSInt32Type, agg->fMinutesToSleep);

    // Wake On Magic Packet
    GetAggressivenessValue(CFDictionaryGetValue(p, CFSTR(kIOPMWakeOnLANKey)),
                           kCFNumberSInt32Type, agg->fWakeOnLAN);

    // Wake On Ring
    GetAggressivenessValue(CFDictionaryGetValue(p, CFSTR(kIOPMWakeOnRingKey)),
                           kCFNumberSInt32Type, agg->fWakeOnRing);

    // AutomaticRestartOnPowerLoss
    GetAggressivenessValue(CFDictionaryGetValue(p, CFSTR(kIOPMRestartOnPowerLossKey)),
                           kCFNumberSInt32Type, agg->fAutomaticRestart);
    
    return 0;
}

/*** IOPMFunctionIsAvailable
     Arguments-
        CFStringRef f - Name of a PM feature/Energy Saver checkbox feature (like "WakeOnRing" or "Reduce Processor Speed")
        CFStringRef power_source - The current power source (like "AC Power" or "Battery Power")
     Return value-
        kIOReturnSuccess if the given PM feature is supported on the given power source
        kIOReturnError if the feature is unsupported
 ***/
static IOReturn IOPMFunctionIsAvailable(CFStringRef f, CFStringRef power_source)
{
    CFDictionaryRef		        supportedFeatures = NULL;
    CFPropertyListRef           izzo;
    io_registry_entry_t		    registry_entry;
    io_iterator_t		        tmp;
    mach_port_t                   masterPort;
    IOReturn                    ret;

    IOMasterPort(bootstrap_port, &masterPort);    
    
    IOServiceGetMatchingServices(masterPort, IOServiceNameMatching("IOPMrootDomain"), &tmp);
    registry_entry = IOIteratorNext(tmp);
    IOObjectRelease(tmp);
    
    supportedFeatures = IORegistryEntryCreateCFProperty(registry_entry, CFSTR("Supported Features"),
                            kCFAllocatorDefault, NULL);
    IOObjectRelease(registry_entry);
    
    if(CFEqual(f, CFSTR(kIOPMDisplaySleepKey))
        || CFEqual(f, CFSTR(kIOPMSystemSleepKey))
        || CFEqual(f, CFSTR(kIOPMDiskSleepKey)))
    {
        ret = kIOReturnSuccess;
        goto IOPMFunctionIsAvailable_exitpoint;
    }
    
    // reduce processor speed
    if(CFEqual(f, CFSTR(kIOPMReduceSpeedKey)))
    {
        if(!supportedFeatures) return kIOReturnError;
        if(CFDictionaryGetValue(supportedFeatures, f))
            ret = kIOReturnSuccess;
        else ret = kIOReturnError;
        goto IOPMFunctionIsAvailable_exitpoint;
    }

    // dynamic powerstep
    if(CFEqual(f, CFSTR(kIOPMDynamicPowerStepKey)))
    {
        if(!supportedFeatures) return kIOReturnError;
        if(CFDictionaryGetValue(supportedFeatures, f))
            ret = kIOReturnSuccess;
        else ret = kIOReturnError;
        goto IOPMFunctionIsAvailable_exitpoint;
    }

    // wake on magic packet
    if(CFEqual(f, CFSTR(kIOPMWakeOnLANKey)))
    {
        // Check for WakeOnLAN property in supportedFeatures
        // Radar 2946434 WakeOnLAN is only supported when running on AC power. It's automatically disabled
        // on battery power, and thus shouldn't be offered as a checkbox option.
        if(CFDictionaryGetValue(supportedFeatures, CFSTR("WakeOnMagicPacket"))
                && (!power_source || !CFEqual(CFSTR(kIOPMBatteryPowerKey), power_source)))
        {
            ret = kIOReturnSuccess;
        } else {
            ret = kIOReturnError;
        }
        goto IOPMFunctionIsAvailable_exitpoint;       
    }

    if(CFEqual(f, CFSTR(kIOPMWakeOnRingKey)))
    {
        // Check for WakeOnRing property under PMU
        registry_entry = getCudaPMURef();
        if((izzo = IORegistryEntryCreateCFProperty(registry_entry, CFSTR("WakeOnRing"),
                            kCFAllocatorDefault, NULL)))
        {
            CFRelease(izzo);
            IOObjectRelease(registry_entry);
            ret = kIOReturnSuccess;
        } else {
            IOObjectRelease(registry_entry);    
            ret = kIOReturnError;
        }
        goto IOPMFunctionIsAvailable_exitpoint;        
    }

    // restart on power loss
    if(CFEqual(f, CFSTR(kIOPMRestartOnPowerLossKey)))
    {
        registry_entry = getCudaPMURef();
        // Check for fileserver property under PMU
        if((izzo = IORegistryEntryCreateCFProperty(registry_entry, CFSTR("FileServer"),
                            kCFAllocatorDefault, NULL)))
        { 
            CFRelease(izzo);
            IOObjectRelease(registry_entry);
            ret = kIOReturnSuccess;
        } else {
            IOObjectRelease(registry_entry);       
            ret = kIOReturnError;
        }
        goto IOPMFunctionIsAvailable_exitpoint;
    }
    
 IOPMFunctionIsAvailable_exitpoint:
    if(supportedFeatures) CFRelease(supportedFeatures);
    IOObjectRelease(masterPort);
    return ret;
}


/***
 * removeIrrelevantPMProperties
 *
 * Prunes unsupported properties from the energy dictionary.
 * e.g. If your machine doesn't have a modem, this removes the Wake On Ring property.
 ***/
static void IOPMRemoveIrrelevantProperties(CFMutableDictionaryRef energyPrefs)
{
    CFArrayRef    tmp = NULL;

    mach_port_t   masterPort;
    int			profile_count = 0;
    int         dict_count = 0;
    CFStringRef		*profile_keys = NULL;
    CFDictionaryRef	*profile_vals = NULL;
    CFStringRef		*dict_keys    = NULL;
    CFDictionaryRef	*dict_vals    = NULL;
    CFMutableDictionaryRef	this_profile;
    
    /* 
     * Remove battery dictionary on desktop machines 
     */
    IOMasterPort(bootstrap_port,&masterPort);
    IOPMCopyBatteryInfo(masterPort, &tmp);
    if(!tmp)
    {
        // no batteries
        CFDictionaryRemoveValue(energyPrefs, CFSTR(kIOPMBatteryPowerKey));
    } else {
        // Batteries are present, cleanup the dummy battery array
        CFRelease(tmp);
    }


    /*
     * Remove features when not supported - Wake On Administrative Access, Dynamic Speed Step, etc.
     */
    profile_count = CFDictionaryGetCount(energyPrefs);
    profile_keys = (CFStringRef *)malloc(sizeof(CFStringRef) * profile_count);
    profile_vals = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * profile_count);
    if(!profile_keys || !profile_vals) return;
    
    CFDictionaryGetKeysAndValues(energyPrefs, (void **)profile_keys, (void **)profile_vals);
    // For each CFDictionary at the top level (battery, AC)
    while(--profile_count >= 0)
    {
        this_profile = (CFMutableDictionaryRef)CFDictionaryGetValue(energyPrefs, profile_keys[profile_count]);
        dict_count = CFDictionaryGetCount(this_profile);
        dict_keys = (CFStringRef *)malloc(sizeof(CFStringRef) * dict_count);
        dict_vals = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * dict_count);
    
        CFDictionaryGetKeysAndValues(this_profile, (void **)dict_keys, (void **)dict_vals);
        // For each specific property within each dictionary
        while(--dict_count >= 0)
            if( kIOReturnError == IOPMFunctionIsAvailable((CFStringRef)dict_keys[dict_count], (CFStringRef)profile_keys[profile_count]) )
            {
                // If the property isn't supported, remove it
                CFDictionaryRemoveValue(this_profile, (CFStringRef)dict_keys[dict_count]);    
            }
        free(dict_keys);
        free(dict_vals);
    }
    free(profile_keys);
    free(profile_vals);
    IOObjectRelease(masterPort);
    return;
}

/***
 * getCheetahPumaEnergySettings
 *
 * Reads the old Energy Saver preferences file from /Library/Preferences/com.apple.PowerManagement.xml
 *
 ***/
static int getCheetahPumaEnergySettings(CFMutableDictionaryRef energyPrefs)
{
   SCPreferencesRef             CheetahPrefs = NULL;
   CFMutableDictionaryRef       s = NULL;
   CFNumberRef                  n;
   CFBooleanRef                 b;
   
   CheetahPrefs = SCPreferencesCreate (kCFAllocatorDefault, 
                                     CFSTR("I/O Kit PM Library"),
                                     CFSTR("/Library/Preferences/com.apple.PowerManagement.plist"));
    
    if(!CheetahPrefs) return 0;
    
    s = (CFMutableDictionaryRef)CFDictionaryGetValue(energyPrefs, CFSTR(kIOPMBatteryPowerKey));
      
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahDimKey);
    if(n) CFDictionarySetValue(s, CFSTR(kIOPMDisplaySleepKey), n);
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahDiskKey);
    if(n) CFDictionarySetValue(s, CFSTR(kIOPMDiskSleepKey), n);
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahSleepKey);
    if(n) CFDictionarySetValue(s, CFSTR(kIOPMSystemSleepKey), n);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahRestartOnPowerLossKey);
    if(b) CFDictionarySetValue(s, CFSTR(kIOPMRestartOnPowerLossKey), b);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahWakeForNetworkAccessKey);
    if(b) CFDictionarySetValue(s, CFSTR(kIOPMWakeOnLANKey), b);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahWakeOnRingKey);
    if(b) CFDictionarySetValue(s, CFSTR(kIOPMWakeOnRingKey), b);
                    
    CFDictionarySetValue(energyPrefs, CFSTR(kIOPMBatteryPowerKey), s);

    s = (CFMutableDictionaryRef)CFDictionaryGetValue(energyPrefs, CFSTR(kIOPMACPowerKey));
      
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahDimKey);
    if(n) CFDictionarySetValue(s, CFSTR(kIOPMDisplaySleepKey), n);
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahDiskKey);
    if(n) CFDictionarySetValue(s, CFSTR(kIOPMDiskSleepKey), n);
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahSleepKey);
    if(n) CFDictionarySetValue(s, CFSTR(kIOPMSystemSleepKey), n);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahRestartOnPowerLossKey);
    if(b) CFDictionarySetValue(s, CFSTR(kIOPMRestartOnPowerLossKey), b);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahWakeForNetworkAccessKey);
    if(b) CFDictionarySetValue(s, CFSTR(kIOPMWakeOnLANKey), b);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahWakeOnRingKey);
    if(b) CFDictionarySetValue(s, CFSTR(kIOPMWakeOnRingKey), b);

    CFDictionarySetValue(energyPrefs, CFSTR(kIOPMACPowerKey), s);

    CFRelease(CheetahPrefs);


     return 1; // success
    //return 0; // failure
}

extern CFMutableDictionaryRef IOPMCopyPMPreferences(void)
{
    CFMutableDictionaryRef	        energyDict = NULL;
    SCPreferencesRef	            energyPrefs = NULL;
    CFDictionaryRef	                batterySettings = NULL;
    CFDictionaryRef	                ACSettings = NULL;

    energyDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    energyPrefs = SCPreferencesCreate( kCFAllocatorDefault, kIOPMAppName, kIOPMPrefsPath );
    if(energyPrefs == 0) {
        if(energyDict != 0) CFRelease(energyDict);
        return NULL;
    }
    
    // Attempt to read battery & AC settings
    batterySettings = isA_CFDictionary(SCPreferencesGetValue(energyPrefs, CFSTR(kIOPMBatteryPowerKey)));
    ACSettings = isA_CFDictionary(SCPreferencesGetValue(energyPrefs, CFSTR(kIOPMACPowerKey)));
    
    // If com.apple.PowerManagement.xml opened correctly, read data from it
    if( batterySettings || ACSettings ) 
    {
        if(batterySettings)
            CFDictionaryAddValue(energyDict, CFSTR(kIOPMBatteryPowerKey), batterySettings);
        if(ACSettings)
            CFDictionaryAddValue(energyDict, CFSTR(kIOPMACPowerKey), ACSettings);

    } else {
        // Fill dictionaries with default settings
        getDefaultEnergySettings(energyDict);

        // If Cheetah settings exist, use those
        getCheetahPumaEnergySettings(energyDict);
    }

    CFRelease(energyPrefs);    
    IOPMRemoveIrrelevantProperties(energyDict);
    
    return energyDict;
}

extern IOReturn IOPMActivatePMPreference(CFDictionaryRef SystemProfiles, CFStringRef profile)
{
    IOPMAggressivenessFactors	*agg = NULL;
    CFDictionaryRef                 activePMPrefs = NULL;
    CFDictionaryRef                 newPMPrefs = NULL;
    SCDynamicStoreRef               dynamic_store = NULL;

    if(0 == isA_CFDictionary(SystemProfiles) || 0 == isA_CFString(profile)) {
        return kIOReturnBadArgument;
    }

    // Activate settings by sending them to the kernel
    agg = (IOPMAggressivenessFactors *)malloc(sizeof(IOPMAggressivenessFactors));
    getAggressivenessFactorsFromProfile(SystemProfiles, profile, agg);
    sendEnergySettingsToKernel(agg);
    free(agg);
    
    // Put the new settings in the SCDynamicStore for interested apps
    dynamic_store = SCDynamicStoreCreate(kCFAllocatorDefault,
                                         CFSTR("IOKit User Library"),
                                         NULL, NULL);
    if(dynamic_store == NULL) return kIOReturnError;

    activePMPrefs = isA_CFDictionary(SCDynamicStoreCopyValue(dynamic_store,  
                                         CFSTR(kIOPMDynamicStoreSettingsKey)));

    newPMPrefs = isA_CFDictionary(CFDictionaryGetValue(SystemProfiles, profile));

    // If there isn't currently a value for kIOPMDynamicStoreSettingsKey
    //    or the current value is different than the new value
    if( !activePMPrefs || (newPMPrefs && !CFEqual(activePMPrefs, newPMPrefs)) )
    {
        // Then set the kIOPMDynamicStoreSettingsKey to the new value
        SCDynamicStoreSetValue(dynamic_store,  
                               CFSTR(kIOPMDynamicStoreSettingsKey),
                               newPMPrefs);
    }
    
    if(activePMPrefs) CFRelease(activePMPrefs);
    CFRelease(dynamic_store);
    return kIOReturnSuccess;

}

extern IOReturn IOPMSetPMPreferences(CFDictionaryRef ESPrefs)
{
    SCPreferencesRef	        energyPrefs = NULL;
    int			                i;
    int			                dict_count = 0;
    CFStringRef		            *dict_keys;
    CFDictionaryRef	            *dict_vals;
    
    energyPrefs = SCPreferencesCreate( kCFAllocatorDefault, kIOPMAppName, kIOPMPrefsPath );
    if(!energyPrefs) return kIOReturnError;
    
    dict_count = CFDictionaryGetCount(ESPrefs);
    dict_keys = (CFStringRef *)malloc(sizeof(CFStringRef) * dict_count);
    dict_vals = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * dict_count);
    
    CFDictionaryGetKeysAndValues(ESPrefs, (void **)dict_keys, (void **)dict_vals);
    
    if(!dict_keys || !dict_vals) return kIOReturnError;

    for(i=0; i<dict_count; i++)
    {
        if(!SCPreferencesSetValue(energyPrefs, dict_keys[i], dict_vals[i]))
        {
            return kIOReturnError;
        }
    }

    if(!SCPreferencesCommitChanges(energyPrefs))
    {
        // handle error
        if(kSCStatusAccessError == SCError()) return kIOReturnNotPrivileged;
        return kIOReturnError;
    }
    
    if(!SCPreferencesApplyChanges(energyPrefs))
    {
        // handle error
        if(kSCStatusAccessError == SCError()) return kIOReturnNotPrivileged;
        return kIOReturnError;        
    }

    return kIOReturnSuccess;
}
