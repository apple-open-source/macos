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
#include <SystemConfiguration/SCPreferencesPrivate.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
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
#define kACSleepOnPowerButton           1
#define kACWakeOnClamshell              1
#define kACWakeOnACChange               0

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
#define kBatterySleepOnPowerButton	0
#define kBatteryWakeOnClamshell         1
#define kBatteryWakeOnACChange          0

/*
 *      UPS
 */
#define kUPSMinutesToDim	             kACMinutesToDim
#define kUPSMinutesToSpin                kACMinutesToSpin
#define kUPSMinutesToSleep               kACMinutesToSleep
#define kUPSWakeOnRing                   kACWakeOnRing
#define kUPSAutomaticRestart             kACAutomaticRestart
#define kUPSWakeOnLAN                    kACWakeOnLAN
#define kUPSReduceProcessorSpeed         kACReduceProcessorSpeed
#define kUPSDynamicPowerStep             kACDynamicPowerStep
#define kUPSSleepOnPowerButton           kACSleepOnPowerButton
#define kUPSWakeOnClamshell              kACWakeOnClamshell
#define kUPSWakeOnACChange               kACWakeOnACChange


#define kIOPMNumPMFeatures		11

static char *energy_features_array[kIOPMNumPMFeatures] = {
    kIOPMDisplaySleepKey, 
    kIOPMDiskSleepKey,
    kIOPMSystemSleepKey,
    kIOPMWakeOnRingKey,
    kIOPMRestartOnPowerLossKey,
    kIOPMWakeOnLANKey,
    kIOPMReduceSpeedKey,
    kIOPMDynamicPowerStepKey,
    kIOPMSleepOnPowerButtonKey,
    kIOPMWakeOnClamshellKey,
    kIOPMWakeOnACChangeKey
};

static const unsigned int battery_defaults_array[] = {
    kBatteryMinutesToDim,
    kBatteryMinutesToSpin,
    kBatteryMinutesToSleep,
    kBatteryWakeOnRing,
    kBatteryAutomaticRestart,
    kBatteryWakeOnLAN,
    kBatteryReduceProcessorSpeed,
    kBatteryDynamicPowerStep,
    kBatterySleepOnPowerButton,
    kBatteryWakeOnClamshell,
    kBatteryWakeOnACChange
};

static const unsigned int ac_defaults_array[] = {
    kACMinutesToDim,
    kACMinutesToSpin,
    kACMinutesToSleep,
    kACWakeOnRing,
    kACAutomaticRestart,
    kACWakeOnLAN,
    kACReduceProcessorSpeed,
    kACDynamicPowerStep,
    kACSleepOnPowerButton,
    kACWakeOnClamshell,
    kACWakeOnACChange
};

static const unsigned int ups_defaults_array[] = {
    kUPSMinutesToDim,
    kUPSMinutesToSpin,
    kUPSMinutesToSleep,
    kUPSWakeOnRing,
    kUPSAutomaticRestart,
    kUPSWakeOnLAN,
    kUPSReduceProcessorSpeed,
    kUPSDynamicPowerStep,
    kUPSSleepOnPowerButton,
    kUPSWakeOnClamshell,
    kUPSWakeOnACChange
};


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
    unsigned int        fSleepOnPowerButton;
    unsigned int        fWakeOnClamshell;
    unsigned int        fWakeOnACChange;
} IOPMAggressivenessFactors;

static int getDefaultEnergySettings(CFMutableDictionaryRef sys)
{
    CFMutableDictionaryRef 	batt = NULL;
    CFMutableDictionaryRef 	ac = NULL;
    CFMutableDictionaryRef 	ups = NULL;
    int			i;
    CFNumberRef		val;
    CFStringRef		key;


    // Use pre-existing battery dictionary if possible
    if((batt=(CFMutableDictionaryRef)CFDictionaryGetValue(sys, CFSTR(kIOPMBatteryPowerKey))))
      {
        CFRetain(batt);   // So the CFRelease at the end of the function is OK
      } else {
        // If no pre-existing prefs dictionary, create one
        batt = CFDictionaryCreateMutable(kCFAllocatorDefault, 
					 0,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      };
    
    if((ac=(CFMutableDictionaryRef)CFDictionaryGetValue(sys, CFSTR(kIOPMACPowerKey))))
      {
        CFRetain(ac);
      } else {
        ac = CFDictionaryCreateMutable(kCFAllocatorDefault,
				       0,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      };

    // Use pre-existing battery dictionary if possible
    if((ups=(CFMutableDictionaryRef)CFDictionaryGetValue(sys, CFSTR(kIOPMUPSPowerKey))))
      {
        CFRetain(ups);   // So the CFRelease at the end of the function is OK
      } else {
        // If no pre-existing prefs dictionary, create one
        ups = CFDictionaryCreateMutable(kCFAllocatorDefault, 
					 0,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      };

    /*
     * Note that in the following "poplulation" loops, we're using CFDictionaryAddValue rather
     * than CFDictionarySetValue. If a value is already present AddValue will not replace it.
     */
    
    /* 
     * Populate default battery dictionary 
     */
    
    for(i=0; i<kIOPMNumPMFeatures; i++)
    {
        key = CFStringCreateWithCString(kCFAllocatorDefault, energy_features_array[i], kCFStringEncodingMacRoman);
        val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &battery_defaults_array[i]);
        CFDictionaryAddValue(batt, key, val);
        CFRelease(key);
        CFRelease(val);
    }

    /* 
     * Populate default AC dictionary 
     */

    for(i=0; i<kIOPMNumPMFeatures; i++)
    {
        key = CFStringCreateWithCString(kCFAllocatorDefault, energy_features_array[i], kCFStringEncodingMacRoman);
        val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &ac_defaults_array[i]);
        CFDictionaryAddValue(ac, key, val);
        CFRelease(key);
        CFRelease(val);
    }

    /* 
     * Populate default UPS dictionary 
     */

    for(i=0; i<kIOPMNumPMFeatures; i++)
    {
        key = CFStringCreateWithCString(kCFAllocatorDefault, energy_features_array[i], kCFStringEncodingMacRoman);
        val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &ups_defaults_array[i]);
        CFDictionaryAddValue(ups, key, val);
        CFRelease(key);
        CFRelease(val);
    }


    /*
     * Stuff the default values into the "system settings"
     */
    CFDictionarySetValue(sys, CFSTR(kIOPMBatteryPowerKey), batt);
    CFDictionarySetValue(sys, CFSTR(kIOPMACPowerKey), ac);
    CFDictionarySetValue(sys, CFSTR(kIOPMUPSPowerKey), ups);

    CFRelease(batt);
    CFRelease(ac);
    CFRelease(ups);

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

static io_registry_entry_t  getPMRootDomainRef(void)
{
    io_registry_entry_t		    registry_entry;
    io_iterator_t		        tmp;
    
    IOServiceGetMatchingServices(NULL, IOServiceNameMatching("IOPMrootDomain"), &tmp);
    registry_entry = IOIteratorNext(tmp);
    IOObjectRelease(tmp);
    
    return registry_entry;    
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
    io_registry_entry_t             PMRootDomain;

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

    if(true == IOPMFeatureIsAvailable(CFSTR(kIOPMWakeOnLANKey), NULL))
    {
        type = kPMEthernetWakeOnLANSettings;
        err = IOPMSetAggressiveness(PM_connection, type, p->fWakeOnLAN);
    }
    
    IOServiceClose(PM_connection);
    
    // Wake On Ring
    if(true == IOPMFeatureIsAvailable(CFSTR(kIOPMWakeOnRingKey), NULL))
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
    if(true == IOPMFeatureIsAvailable(CFSTR(kIOPMRestartOnPowerLossKey), NULL))
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
    
    // Wake on change of AC state -- battery to AC or vice versa
    if(true == IOPMFeatureIsAvailable(CFSTR(kIOPMWakeOnACChangeKey), NULL))
    {
        cudaPMU = getCudaPMURef();
        ret = IOServiceOpen((io_service_t)cudaPMU, mach_task_self(), kApplePMUUserClientMagicCookie, &connection);
    
        if(p->fWakeOnACChange) i = 0xFFFFFFFF;
        else i = 0x0;
        on = CFDataCreate(kCFAllocatorDefault, (void *)&i, 4);
        
	// Not a typo.  ApplePMU has ACchange, not ACChange
        ret = IOConnectSetCFProperty(connection, CFSTR("WakeOnACchange"), on);
        CFRelease(on);
        IOServiceClose(connection);
        IOObjectRelease(cudaPMU);
    }
    
    // Disable power button sleep on PowerMacs, Cubes, and iMacs
    // Default is false == power button causes sleep
    if(true == IOPMFeatureIsAvailable(CFSTR(kIOPMSleepOnPowerButtonKey), NULL))
    {
        PMRootDomain = getPMRootDomainRef();
    
        if(p->fSleepOnPowerButton) 
            ret = IORegistryEntrySetCFProperty(PMRootDomain, CFSTR("DisablePowerButtonSleep"), kCFBooleanFalse);
        else 
            ret = IORegistryEntrySetCFProperty(PMRootDomain, CFSTR("DisablePowerButtonSleep"), kCFBooleanTrue);
        
        IOObjectRelease(PMRootDomain);
    }    
    
    // Wakeup on clamshell open
    // Default is true == wakeup when the clamshell opens
    if(true == IOPMFeatureIsAvailable(CFSTR(kIOPMWakeOnClamshellKey), NULL))
    {
        cudaPMU = getCudaPMURef();
        ret = IOServiceOpen((io_service_t)cudaPMU, mach_task_self(), kApplePMUUserClientMagicCookie, &connection);
    
        if(p->fWakeOnClamshell) i = 0xFFFFFFFF;
        else i = 0x0;
        on = CFDataCreate(kCFAllocatorDefault, (void *)&i, 4);
        
        ret = IOConnectSetCFProperty(connection, CFSTR("WakeOnLid"), on);
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
    
    // Disable Power Button Sleep
    GetAggressivenessValue(CFDictionaryGetValue(p, CFSTR(kIOPMSleepOnPowerButtonKey)),
                           kCFNumberSInt32Type, agg->fSleepOnPowerButton);    

    // Disable Clamshell Wakeup
    GetAggressivenessValue(CFDictionaryGetValue(p, CFSTR(kIOPMWakeOnClamshellKey)),
                           kCFNumberSInt32Type, agg->fWakeOnClamshell);    

    // Wake on AC Change
    GetAggressivenessValue(CFDictionaryGetValue(p, CFSTR(kIOPMWakeOnACChangeKey)),
                           kCFNumberSInt32Type, agg->fWakeOnACChange);    


    return 0;
}

/*** IOPMFeatureIsAvailable
     Arguments-
        CFStringRef f - Name of a PM feature/Energy Saver checkbox feature (like "WakeOnRing" or "Reduce Processor Speed")
        CFStringRef power_source - The current power source (like "AC Power" or "Battery Power")
     Return value-
        kIOReturnSuccess if the given PM feature is supported on the given power source
        kIOReturnError if the feature is unsupported
 ***/
extern bool IOPMFeatureIsAvailable(CFStringRef f, CFStringRef power_source)
{
    CFDictionaryRef		        supportedFeatures = NULL;
    CFPropertyListRef           izzo;
    CFArrayRef                  tmp_array;
    io_registry_entry_t		    registry_entry;
    io_iterator_t		        tmp;
    mach_port_t                 masterPort;
    bool                        ret = false;

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
        ret = true;
        goto IOPMFeatureIsAvailable_exitpoint;
    }
    
    // reduce processor speed
    if(CFEqual(f, CFSTR(kIOPMReduceSpeedKey)))
    {
        if(!supportedFeatures) return false;
        if(CFDictionaryGetValue(supportedFeatures, f))
            ret = true;
        else ret = false;
        goto IOPMFeatureIsAvailable_exitpoint;
    }

    // dynamic powerstep
    if(CFEqual(f, CFSTR(kIOPMDynamicPowerStepKey)))
    {
        if(!supportedFeatures) return false;
        if(CFDictionaryGetValue(supportedFeatures, f))
            ret = true;
        else ret = false;
        goto IOPMFeatureIsAvailable_exitpoint;
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
            ret = true;
        } else {
            ret = false;
        }
        goto IOPMFeatureIsAvailable_exitpoint;       
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
            ret = true;
        } else {
            IOObjectRelease(registry_entry);    
            ret = false;
        }
        goto IOPMFeatureIsAvailable_exitpoint;        
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
            ret = true;
        } else {
            IOObjectRelease(registry_entry);       
            ret = false;
        }
        goto IOPMFeatureIsAvailable_exitpoint;
    }
    
    // Wake on AC change
    if(CFEqual(f, CFSTR(kIOPMWakeOnACChangeKey)))
    {
        if(!supportedFeatures) return false;
	// Not a typo, ApplePMU has "ACchange" not "ACChange" :
        if(CFDictionaryGetValue(supportedFeatures, CFSTR("WakeOnACchange")))
            ret = true;
        else ret = false;
        goto IOPMFeatureIsAvailable_exitpoint;
    }

    // Disable power button sleep
    if(CFEqual(f, CFSTR(kIOPMSleepOnPowerButtonKey)))
    {
        // Pressing the power button only causes sleep on desktop PowerMacs, cubes, and iMacs
        // Therefore this feature is not supported on portables, just on desktops.
        // We'll use the presence of a battery (or the capability for a battery, as interpreted by the PMU)
        // as evidence whether this is a portable or not.
        IOReturn r = IOPMCopyBatteryInfo(NULL, &tmp_array);
        if((r == kIOReturnSuccess) && tmp_array) 
        {
            CFRelease(tmp_array);
            ret = false;
        } else ret = true;        
        goto IOPMFeatureIsAvailable_exitpoint;
    }
    
    // Disable clamshell wakeup
    if(CFEqual(f, CFSTR(kIOPMWakeOnClamshellKey)))
    {
        if(!supportedFeatures) return false;
        if(CFDictionaryGetValue(supportedFeatures, CFSTR("WakeOnLid")))
            ret = true;
        else ret = false;
        goto IOPMFeatureIsAvailable_exitpoint;
    }
        
 IOPMFeatureIsAvailable_exitpoint:
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
    int			profile_count = 0;
    int         dict_count = 0;
    CFStringRef		*profile_keys = NULL;
    CFDictionaryRef	*profile_vals = NULL;
    CFStringRef		*dict_keys    = NULL;
    CFDictionaryRef	*dict_vals    = NULL;
    CFMutableDictionaryRef	this_profile;
    CFTypeRef               ps_snapshot;
    
    ps_snapshot = IOPSCopyPowerSourcesInfo();
    
    /*
     * Remove features when not supported - Wake On Administrative Access, Dynamic Speed Step, etc.
     */
    profile_count = CFDictionaryGetCount(energyPrefs);
    profile_keys = (CFStringRef *)malloc(sizeof(CFStringRef) * profile_count);
    profile_vals = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * profile_count);
    if(!profile_keys || !profile_vals) return;
    
    CFDictionaryGetKeysAndValues(energyPrefs, (const void **)profile_keys, (const void **)profile_vals);
    // For each CFDictionary at the top level (battery, AC)
    while(--profile_count >= 0)
    {
        if(kCFBooleanTrue != IOPSPowerSourceSupported(ps_snapshot, profile_keys[profile_count]))
        {
            // Remove dictionary if the whole power source isn't supported on this machine.
            CFDictionaryRemoveValue(energyPrefs, profile_keys[profile_count]);        
        } else {
            this_profile = (CFMutableDictionaryRef)CFDictionaryGetValue(energyPrefs, profile_keys[profile_count]);
            dict_count = CFDictionaryGetCount(this_profile);
            dict_keys = (CFStringRef *)malloc(sizeof(CFStringRef) * dict_count);
            dict_vals = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * dict_count);
        
            CFDictionaryGetKeysAndValues(this_profile, (const void **)dict_keys, (const void **)dict_vals);
            // For each specific property within each dictionary
            while(--dict_count >= 0)
                if(false == IOPMFeatureIsAvailable((CFStringRef)dict_keys[dict_count], (CFStringRef)profile_keys[profile_count]) )
                {
                    // If the property isn't supported, remove it
                    CFDictionaryRemoveValue(this_profile, (CFStringRef)dict_keys[dict_count]);    
                }
            free(dict_keys);
            free(dict_vals);
        }
    }
    free(profile_keys);
    free(profile_vals);
    if(ps_snapshot) CFRelease(ps_snapshot);
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
    SCPreferencesRef	            	energyPrefs = NULL;
    CFDictionaryRef	                batterySettings = NULL;
    CFDictionaryRef	                ACSettings = NULL;
    CFDictionaryRef                 UPSSettings = NULL;    


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
    UPSSettings = isA_CFDictionary(SCPreferencesGetValue(energyPrefs, CFSTR(kIOPMUPSPowerKey)));
    
    // If com.apple.PowerManagement.xml opened correctly, read data from it
    if( batterySettings || ACSettings || UPSSettings) 
    {
        if(batterySettings)
            CFDictionaryAddValue(energyDict, CFSTR(kIOPMBatteryPowerKey), batterySettings);
        if(ACSettings)
            CFDictionaryAddValue(energyDict, CFSTR(kIOPMACPowerKey), ACSettings);
        if(UPSSettings)
            CFDictionaryAddValue(energyDict, CFSTR(kIOPMUPSPowerKey), UPSSettings);
        
        // Fill in any missing key/value pairs from the defaults.  Fixing Radar 3104287.
        getDefaultEnergySettings(energyDict);

    } else {
	// If com.apple.PowerManagement.xml was not read, start with defaults
        getDefaultEnergySettings(energyDict);
        // If Cheetah settings exist, merge those
        getCheetahPumaEnergySettings(energyDict);
    }
    
    IOPMRemoveIrrelevantProperties(energyDict);
    CFRelease(energyPrefs);    
  
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
    
    CFDictionaryGetKeysAndValues(ESPrefs, (const void **)dict_keys, (const void **)dict_vals);
    
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

    CFRelease(energyPrefs);

    return kIOReturnSuccess;
}

/***
 Support structures and functions for IOPMPrefsNotificationCreateRunLoopSource
***/
typedef struct {
    IOPMPrefsCallbackType	callback;
    void			*context;
} user_callback_context;

typedef struct {
    SCDynamicStoreRef		store;
    CFRunLoopSourceRef		SCDSrls;
    user_callback_context   *user_callback;
} my_cfrls_context;

/* SCDynamicStoreCallback */
static void my_dynamic_store_call(SCDynamicStoreRef store, CFArrayRef keys, void *ctxt) {
    user_callback_context	*c = (user_callback_context *)ctxt;
    IOPowerSourceCallbackType cb;

    // Check that the callback is a valid pointer
    if(!c) return;
    cb = c->callback;
    if(!cb) return;
    
    // Execute callback
    (*cb)(c->context);
}

static void
rlsSchedule(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	my_cfrls_context	*c = (my_cfrls_context *)info;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), c->SCDSrls, mode);
	return;
}


static void
rlsCancel(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	my_cfrls_context	*c = (my_cfrls_context *)info;
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), c->SCDSrls, mode);
	return;
}

static void
rlsRelease(void *info)
{
	my_cfrls_context	*c = (my_cfrls_context *)info;

    if(!c) return;
    if(c->SCDSrls) CFRelease(c->SCDSrls);
    if(c->store) CFRelease(c->store);
    if(c->user_callback) free(c->user_callback);
    free(c);
        
	return;
}



/***
 Returns a CFRunLoopSourceRef that notifies the caller when power source
 information changes.
 Arguments:
    IOPowerSourceCallbackType callback - A function to be called whenever ES prefs file on disk changes
    void *context - Any user-defined pointer, passed to the IOPowerSource callback.
 Returns NULL if there were any problems.
 Caller must CFRelease() the returned value.
***/
CFRunLoopSourceRef IOPMPrefsNotificationCreateRunLoopSource(IOPMPrefsCallbackType callback, void *context) {
    SCDynamicStoreRef   store = NULL;
    CFStringRef         EnergyPrefsKey = NULL;
    CFRunLoopSourceRef  SCDrls = NULL;
    // For the source we're creating:
    CFRunLoopSourceRef	ourSource = NULL;
    CFRunLoopSourceContext  rlsContext;
    SCDynamicStoreContext	scdsctxt;

    user_callback_context		*callback_state = NULL;
    my_cfrls_context			*runloop_state = NULL;
    
    // Save the state of the user's callback
    callback_state = malloc(sizeof(user_callback_context));
    callback_state->context = context;
    callback_state->callback = callback;
    
    bzero(&scdsctxt, sizeof(SCDynamicStoreContext));
    scdsctxt.info = callback_state;
    
    // Open connection to SCDynamicStore. User's callback as context.
    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                CFSTR("IOKit Power Source Copy"), my_dynamic_store_call, (void *)&scdsctxt);
    if(!store) return NULL;
     
    // Setup notification for changes in Energy Saver prefences
    EnergyPrefsKey = SCDynamicStoreKeyCreatePreferences(NULL, kIOPMPrefsPath, kSCPreferencesKeyApply);
    if(EnergyPrefsKey) 
        SCDynamicStoreAddWatchedKey(store, EnergyPrefsKey, FALSE);

    // Obtain the CFRunLoopSourceRef from this SCDynamicStoreRef session
    SCDrls = SCDynamicStoreCreateRunLoopSource(kCFAllocatorDefault, store, 0);
    
    // Here's where it gets kind of hokey. The RLS granted us by the SCDynamicStore
    // is what we want, but we need to keep the SCDynamicStoreRef around as long as
    // the RLS is around, and free it when the RLS is released.
    // So we create our own RLS, and free all our bookkeeping data when it's released.
    runloop_state = malloc(sizeof(my_cfrls_context));
    runloop_state->store = store;
    runloop_state->SCDSrls = SCDrls;
    runloop_state->user_callback = callback_state;
        
    // Setup the CFRunLoopSource context for the return-value CFRLS
    // Install my hooks into schedule/cancel/perform here
    bzero(&rlsContext, sizeof(CFRunLoopSourceContext));
    rlsContext.version         = 0;
    rlsContext.info            = (void *)runloop_state;
    rlsContext.schedule        = rlsSchedule;
    rlsContext.cancel          = rlsCancel;
    rlsContext.release         = rlsRelease;
    
    // Create the RunLoopSource
    ourSource = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &rlsContext);    

    return ourSource;
}

