/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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


#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#include <Security/SecTask.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <grp.h>
#include <pwd.h>
#include <syslog.h>
#include <unistd.h>
#include <asl.h>
#include <membership.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <unistd.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include <notify.h>

#include "Platform.h"
#include "PrivateLib.h"
#include "BatteryTimeRemaining.h"
#include "PMAssertions.h"
#include "PMSettings.h"
#include "PMAssertions.h"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

__private_extern__
const char *stringForLWCode(uint8_t code)
{
    const char *string;
    switch (code)
    {
        default:
            string = "OK";
    }
    return string;
}

__private_extern__
const char *stringForPMCode(uint8_t code)
{
    const char *string = "";

    switch (code)
    {
        case kIOPMTracePointSystemUp:
            string = "On";
            break;
        case kIOPMTracePointSleepStarted:
            string = "SleepStarted";
            break;
        case kIOPMTracePointSleepApplications:
            string = "SleepApps";
            break;
        case kIOPMTracePointSleepPriorityClients:
            string = "SleepPriority";
            break;
        case kIOPMTracePointSleepWillChangeInterests:
            string = "SleepWillChangeInterests";
            break;
        case kIOPMTracePointSleepPowerPlaneDrivers:
            string = "SleepDrivers";
            break;
        case kIOPMTracePointSleepDidChangeInterests:
            string = "SleepDidChangeInterests";
            break;
        case kIOPMTracePointSleepCapabilityClients:
            string = "SleepCapabilityClients";
            break;
        case kIOPMTracePointSleepPlatformActions:
            string = "SleepPlatformActions";
            break;
        case kIOPMTracePointSleepCPUs:
            string = "SleepCPUs";
            break;
        case kIOPMTracePointSleepPlatformDriver:
            string = "SleepPlatformDriver";
            break;
        case kIOPMTracePointSystemSleep:
            string = "SleepPlatform";
            break;
        case kIOPMTracePointHibernate:
            string = "Hibernate";
            break;
        case kIOPMTracePointWakePlatformDriver:
            string = "WakePlatformDriver";
            break;
        case kIOPMTracePointWakePlatformActions:
            string = "WakePlatformActions";
            break;
        case kIOPMTracePointWakeCPUs:
            string = "WakeCPUs";
            break;
        case kIOPMTracePointWakeWillPowerOnClients:
            string = "WakeWillPowerOnClients";
            break;
        case kIOPMTracePointWakeWillChangeInterests:
            string = "WakeWillChangeInterests";
            break;
        case kIOPMTracePointWakeDidChangeInterests:
            string = "WakeDidChangeInterests";
            break;
        case kIOPMTracePointWakePowerPlaneDrivers:
            string = "WakeDrivers";
            break;
        case kIOPMTracePointWakeCapabilityClients:
            string = "WakeCapabilityClients";
            break;
        case kIOPMTracePointWakeApplications:
            string = "WakeApps";
            break;
        case kIOPMTracePointSystemLoginwindowPhase:
            string = "WakeLoginWindow";
            break;
        case kIOPMTracePointDarkWakeEntry:
            string = "DarkWakeEntry";
            break;
        case kIOPMTracePointDarkWakeExit:
            string = "DarkWakeExit";
            break;
    }
    return string;
}


__private_extern__ CFAbsoluteTime _CFAbsoluteTimeFromPMEventTimeStamp(uint64_t kernelPackedTime)
{
    uint32_t    cal_sec = (uint32_t)(kernelPackedTime >> 32);
    uint32_t    cal_micro = (uint32_t)(kernelPackedTime & 0xFFFFFFFF);
    CFAbsoluteTime timeKernelEpoch = (CFAbsoluteTime)(double)cal_sec + (double)cal_micro/1000.0;

    // Adjust from kernel 1970 epoch to CF 2001 epoch
    timeKernelEpoch -= kCFAbsoluteTimeIntervalSince1970;

    return timeKernelEpoch;
}



/* For internal use only */
__private_extern__ io_registry_entry_t getRootDomain(void)
{
    static io_registry_entry_t gRoot = MACH_PORT_NULL;
    
    if (MACH_PORT_NULL == gRoot)
        gRoot = IORegistryEntryFromPath( kIOMasterPortDefault,
                                        kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
    
    return gRoot;
}

__private_extern__ io_registry_entry_t getIOPMPowerSource(void)
{
    static io_registry_entry_t ps = MACH_PORT_NULL;

    if (ps == MACH_PORT_NULL) {
        ps = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPMPowerSource"));
    }

    return ps;
}

/*****************************************************************************/
/*****************************************************************************/

__private_extern__ CFCalendarRef        _gregorian(void)
{
    static CFCalendarRef g = NULL;
    if (!g) {
        g = CFCalendarCreateWithIdentifier(NULL, kCFGregorianCalendar);
    }
    return g;
}

/***************************************************************************/
__private_extern__  asl_object_t open_pm_asl_store(char *store)
{
    asl_object_t        response = NULL;
    size_t              endMessageID;

    if (!store) {
        return NULL;
    }
    asl_object_t query = asl_new(ASL_TYPE_LIST);
    if (query != NULL)
    {
		asl_object_t cq = asl_new(ASL_TYPE_QUERY);
		if (cq != NULL)
		{
			asl_set_query(cq, ASL_KEY_FACILITY, kPMFacility, ASL_QUERY_OP_EQUAL);
			asl_append(query, cq);
			asl_release(cq);

			asl_object_t pmstore = asl_open_path(store, 0);
			if (pmstore != NULL) {
				response = asl_match(pmstore, query, &endMessageID, 0, 0, 0, ASL_MATCH_DIRECTION_FORWARD);
			}
			asl_release(pmstore);
		}
		asl_release(query);
    }

    return response;
}

uint64_t CFAbsoluteTimeToMachAbsoluteTime(CFAbsoluteTime absoluteTime)
{
    uint64_t absolute;
    struct timespec date;
    kern_return_t result = mach_get_times(&absolute, NULL, &date);
    if (result != KERN_SUCCESS) {
        ERROR_LOG("Unable to get times: %i", result);
        return 0;
    }

    // Convert the date to a CFAbsoluteTime
    CFAbsoluteTime now = (date.tv_sec - kCFAbsoluteTimeIntervalSince1970) + date.tv_nsec / (CFTimeInterval)NSEC_PER_SEC;

    // Figure out the difference between the date we got and the date we were passed in.
    CFTimeInterval secondsBetweenInputAndNow = now - absoluteTime;

    mach_timebase_info_data_t timebaseInfo;
    mach_timebase_info(&timebaseInfo);
    
    uint64_t absoluteTimeIntervalBetweenInputAndNow = secondsBetweenInputAndNow * (CFTimeInterval)NSEC_PER_SEC * (timebaseInfo.denom / timebaseInfo.numer);
    // back up the absolute time we got by this interval
    uint64_t inputInAbsoluteTime = absolute - absoluteTimeIntervalBetweenInputAndNow;

    return inputInAbsoluteTime;
}

uint64_t intervalInNanoseconds(uint64_t start, uint64_t end)
{
    // interval between two mach absolute timestamps
    mach_timebase_info_data_t timebaseInfo;
    mach_timebase_info(&timebaseInfo);
    uint64_t elapsedNano = (end - start) * timebaseInfo.numer / timebaseInfo.denom;
    return elapsedNano;

}

// smc shutdown cause
// we are only interested in normal shutdown, power button shutdown
// and shutdown due to thermal or battery reasons
SMCShutdownCause all_causes[] =
{
    { "Battery disconnected",               0},
    { "Normal warm reset",                  1},
    { "Power supply disconnected",          2},
    { "Power button pressed for > 4 sec",   3},
    { "Software initiated shutdown",        5},
    { "Normal shutdown by SOC",             7},
    { "Battery fully drained",              -60},
    { "Thermal shutdown for overtemp",      -81},
};


const char * smcShutdownCauseString(int shutdownCause) {
    int num_codes = sizeof(all_causes)/sizeof(SMCShutdownCause);
    for (int i = 0; i < num_codes; i++){
        if (all_causes[i].shutdownCause == shutdownCause)
            return all_causes[i].shutdownCauseString;
    }
    return "";
}

const char * descriptiveKernelAssertions(uint32_t val) {

    const char *string = "";
    if (val&kIOPMDriverAssertionCPUBit) {
        string = "CPU";
    }
    if (val&kIOPMDriverAssertionUSBExternalDeviceBit) {
        string = "USB";
    }
    if (val&kIOPMDriverAssertionBluetoothHIDDevicePairedBit) {
        string = "BT-HID";
    }
    if (val&kIOPMDriverAssertionExternalMediaMountedBit) {
        string = "MEDIA";
    }
    if (val&kIOPMDriverAssertionReservedBit5) {
        string = "THNDR";
    }
    if (val&kIOPMDriverAssertionPreventDisplaySleepBit) {
        string = "DSPLY";
    }
    if (val&kIOPMDriverAssertionReservedBit7) {
        string = "STORAGE";
    }
    if (val&kIOPMDriverAssertionMagicPacketWakeEnabledBit) {
        string = "MAGICWAKE";
    }
    return string;
}
