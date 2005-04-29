/*
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
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

cc -DMAIN -o DynamicPowerStep -framework CoreFoundation -framework IOKit -framework SystemConfiguration DynamicPowerStep.c

 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include <unistd.h>

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>

#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#define kTimeInterval		(0.1)
#define kNumSamples		(8)
#define kPinThreshold		(0.70)
#define kSwitchThreshold	(0.50)
#define kDynamicSwitch		(0x80000000UL)

static mach_port_t			gHostPort;
static mach_port_t			gMasterPort;
static io_connect_t			gPowerManager;
static SCDynamicStoreRef		gSCDynamicStore;
static CFRunLoopTimerRef		gTimerRef;
static CFStringRef			gIOPMDynamicStoreSettingsKey;
static Boolean				gDPSEnabled;
static long				gCurrentSpeed;
static long				gPinsInRow;
static double				gLoadAverages[kNumSamples];
static host_cpu_load_info_data_t	gSavedTicks;

void load(CFBundleRef bundle, Boolean bundleVerbose);
static void UpdateConfiguration(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info);
static void EnableDynamicPowerStep(Boolean enable);
static void UpdateDynamicLoad(CFRunLoopTimerRef timerRef, void *info);

#ifdef MAIN
int main(int argc, char **argv)
{
  load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
  CFRunLoopRun();
  return 0;
}
#endif

void load(CFBundleRef bundle, Boolean bundleVerbose)
{
    kern_return_t		result;
    CFRunLoopSourceRef  	runLoopSourceRef;
    CFMutableArrayRef		arrayRef;
    Boolean			ok;
    
    gIOPMDynamicStoreSettingsKey = CFSTR(kIOPMDynamicStoreSettingsKey);
    
    // Set up some defaults: DPS off and assume full speed.
    gDPSEnabled = FALSE;
    gCurrentSpeed = 0;
    
    // Get the host port
    gHostPort = mach_host_self();
    
    result = IOMasterPort(bootstrap_port, &gMasterPort);
    if (result != KERN_SUCCESS) return;
    
    // Open a connection to the Power Manager.
    gPowerManager = IOPMFindPowerManagement(gMasterPort);
    if (gPowerManager == 0) return;
    
    // Open a session to the SCDynamicStore.
    gSCDynamicStore = SCDynamicStoreCreate(NULL, CFSTR("Dynamic Power Step"),
                                           UpdateConfiguration, NULL);
    if (gSCDynamicStore == 0) return;
    
    // Set the dynamic store notifications for the IOPM settings.
    arrayRef = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(arrayRef, gIOPMDynamicStoreSettingsKey);
    ok = SCDynamicStoreSetNotificationKeys(gSCDynamicStore, arrayRef, NULL);
    CFRelease(arrayRef);
    if (!ok) return;
    
    // Create a run loop source for the dynamic store.
    runLoopSourceRef = SCDynamicStoreCreateRunLoopSource(NULL, gSCDynamicStore, 0);
    if (runLoopSourceRef == 0) return;
    
    // Add the dynamic store source to the run loop.
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSourceRef, kCFRunLoopDefaultMode);
    CFRelease(runLoopSourceRef);
    
    UpdateConfiguration(gSCDynamicStore, 0, 0);
}

static void UpdateConfiguration(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
    CFDictionaryRef	dictionaryRef;
    CFNumberRef		rpsRef, dpsRef;
    long		rps, dps;
    
    dictionaryRef = SCDynamicStoreCopyValue(store, gIOPMDynamicStoreSettingsKey);
    if (isA_CFDictionary(dictionaryRef) == 0) {
        if (dictionaryRef != 0) CFRelease(dictionaryRef);
        return;
    }
    
    rpsRef = isA_CFNumber(CFDictionaryGetValue(dictionaryRef, CFSTR(kIOPMReduceSpeedKey)));
    dpsRef = isA_CFNumber(CFDictionaryGetValue(dictionaryRef, CFSTR(kIOPMDynamicPowerStepKey)));
    
    // Set Reduce Processor Speed as needed.
    if (rpsRef != 0) {
        CFNumberGetValue((CFNumberRef)rpsRef, kCFNumberSInt32Type, &rps);
        
        gCurrentSpeed = 0;
        if (rps == 0) {
            // Enable or disable Dynamic Power Step as needed.
            if (dpsRef != 0) {
                CFNumberGetValue((CFNumberRef)dpsRef, kCFNumberSInt32Type, &dps);
                
                if (dps != gDPSEnabled) {
                    EnableDynamicPowerStep(!gDPSEnabled);
                }
            }
        } else {
            if (gDPSEnabled) EnableDynamicPowerStep(FALSE);
            
            gCurrentSpeed = 1;
        }
    }
    
    CFRelease(dictionaryRef);
    
    // Change Processor Speed to refect new settings.
    IOPMSetAggressiveness(gPowerManager, kPMSetProcessorSpeed,
			  (gDPSEnabled * kDynamicSwitch) | gCurrentSpeed);
}

static void EnableDynamicPowerStep(Boolean enable)
{
    long cnt;
    
    if (enable) {
        // Create a new CFRunLoopTimer and add it to the run loop
        gTimerRef = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent(),
                                        kTimeInterval, 0, 0,
                                        UpdateDynamicLoad, NULL);
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), gTimerRef, kCFRunLoopDefaultMode);
        
        // Clear Dynamic Power Step data.
        gPinsInRow = 0;
        for (cnt = 0; cnt < kNumSamples; cnt++) gLoadAverages[cnt] = 0.0;
        bzero(&gSavedTicks, sizeof(host_cpu_load_info_data_t));
        
        // Set processor speed slow.
        gCurrentSpeed = 1;
        
        // Mark Dynamic Power Step as enabled.
        gDPSEnabled = true;
    } else {
        // Mark Dynamic Power Step as disabled.
        gDPSEnabled = false;
        
        // Set processor speed fast.
        gCurrentSpeed = 0;
        
        // Invalidate the CFRunLoopTimer.
        CFRunLoopTimerInvalidate(gTimerRef);
        CFRelease(gTimerRef);
    }
}

static void UpdateDynamicLoad(CFRunLoopTimerRef timerRef, void *info)
{
    host_cpu_load_info_data_t		deltaTicks;
    host_cpu_load_info_data_t		newTicks;
    host_basic_info_data_t		hostInfo;
    mach_msg_type_number_t		infoCount;
    int					numProcessors;
    double				totalLoad, userLoad, systemLoad, niceLoad;
    double 				average;
    double				pinThreshold, switchThreshold;
    unsigned long			cnt, sum, newMode;
    
    infoCount = HOST_BASIC_INFO_COUNT;
    host_info(gHostPort, HOST_BASIC_INFO, (host_info_t)&hostInfo, &infoCount);
    numProcessors = hostInfo.avail_cpus;

    pinThreshold = kPinThreshold / numProcessors;
    switchThreshold = kSwitchThreshold / numProcessors;
    
    infoCount = HOST_CPU_LOAD_INFO_COUNT;
    host_statistics(gHostPort,HOST_CPU_LOAD_INFO, 
                    (host_info_t)&newTicks, &infoCount);

    sum = 0;
    for (cnt = 0; cnt < CPU_STATE_MAX; cnt++) {
      deltaTicks.cpu_ticks[cnt] =
        newTicks.cpu_ticks[cnt] - gSavedTicks.cpu_ticks[cnt];
      
      sum += deltaTicks.cpu_ticks[cnt];
    }
    gSavedTicks = newTicks;
    
    if (sum > 0) {
        totalLoad  = 1.0 - ((double)(deltaTicks.cpu_ticks[CPU_STATE_IDLE])   / (double)sum);
        userLoad   = ((double)(deltaTicks.cpu_ticks[CPU_STATE_USER])   / (double)sum);
        systemLoad = ((double)(deltaTicks.cpu_ticks[CPU_STATE_SYSTEM]) / (double)sum);
        niceLoad   = ((double)(deltaTicks.cpu_ticks[CPU_STATE_NICE])   / (double)sum);
    } else {
        totalLoad  = 0.0;
        userLoad   = 0.0;
        systemLoad = 0.0;
        niceLoad   = 0.0;
    }
    
    // Age the load samples.
    for (cnt = kNumSamples - 1; cnt != 0 ; cnt--) {
        gLoadAverages[cnt] = gLoadAverages[cnt - 1];
    }
    
    gLoadAverages[0] = userLoad + systemLoad;
    
    if (gLoadAverages[0] >= pinThreshold) gPinsInRow++;
    else gPinsInRow /= 2;
    
    average = 0;
    for (cnt = 0; cnt < kNumSamples; cnt++) {
        average += gLoadAverages[cnt];
    }
    average += gPinsInRow;
    average /= 1.0 * (kNumSamples + gPinsInRow);
    
    newMode = average >= switchThreshold ? 0 : 1;
    
    if (newMode != gCurrentSpeed) {
        IOPMSetAggressiveness(gPowerManager, kPMSetProcessorSpeed,
			      kDynamicSwitch | newMode);
        gCurrentSpeed = newMode;
    }
}
