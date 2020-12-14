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
#ifndef _BatteryTimeRemaining_h_
#define _BatteryTimeRemaining_h_

#include "PrivateLib.h"
#include "XCTest_FunctionDefinitions.h"

// kMinTimeDeltaForBattRead - Minimum time(in seconds) between reading battery data for battery health evaluation
#if TARGET_OS_OSX
#define kMinTimeDeltaForBattRead           (4*60*60)  // 4hrs
#else
#define kMinTimeDeltaForBattRead           (24*60*60)  // 24hrs
#endif

#if TARGET_OS_IPHONE || POWERD_IOS_XCTEST || TARGET_OS_OSX

// kTrueNCCCycleCountThreshold - If previously calculated NCCP is not available, NCCP is set to h/w specified value only if
// battery cycle count is above kTrueNCCCycleCountThreshold. Otherwise, NCCP is set to kInitialNominalCapacityPercentage
#define kTrueNCCCycleCountThreshold         20  // CycleCount above which NCCP is set to true value(in case past data is not available)
#define kBatteryHealthUsesUPO        0x594553 // YES
#define kBatteryHealthWithoutUPO     0x4e4f   // NO

#define kBHSvcFlagsVerison0     0
#define kBHSvcFlagsVersion1     1
#define kBHSvcFlagsVersion2     2
#define kBHSvcFlagsVersion3     3
#define kBatteryHealthCurrentVersion        kBHSvcFlagsVersion3

#endif // TARGET_OS_IPHONE || POWERD_IOS_XCTEST || TARGET_OS_OSX

#if TARGET_OS_IPHONE || POWERD_IOS_XCTEST

// kNCCMinCycleCountChange - Change in battery cycle count required before triggering change in NCCP.
#define kNCCMinCycleCountChange             5

// kNCCChangeLimit - Percentage by which NCCP is reduced after kNCCMinCycleCountChange change in cycle count
#define kNCCChangeLimit                     1

#define kMitigatedUPOCountThreshold         0
#define kBatteryCellDisconnectThreshold     10

#define kCFPrefsUPOMetricsDomain            "com.apple.thermalmonitor.upostepper.metrics"
#define kCFPrefsMitigatedUPOCountKey        "mitigatedUPOCount"

#define kSmcKeyBatteryFeatureSet                'BFS0'

#endif // TARGET_OS_IPHONE || POWERD_IOS_XCTEST

__private_extern__ IOPMBattery **_batteries(void);
__private_extern__ int _batteryCount(void);
__private_extern__ void BatteryTimeRemaining_prime(void);
__private_extern__ void BatteryTimeRemaining_finish(void);
__private_extern__ void BatteryTimeRemainingWakeNotification(void);

/*!
 * Pass kInternalBattery to kernelPowerSourcesDidChange when you need 
 * PM to re-evaluate the single internal battery (modeled as an IOPMPowerSource)
 * inside your device.
 */
#define kInternalBattery      NULL
__private_extern__ void kernelPowerSourcesDidChange(IOPMBattery *battery_info);

__private_extern__ bool BatteryHandleDeadName(mach_port_t deadName);

__private_extern__ bool isFullyCharged(IOPMBattery *b);

__private_extern__ void sendAdapterDetails(xpc_object_t remoteConnection, xpc_object_t msg);

#if TARGET_OS_OSX
__private_extern__ void getBatteryHealthPersistentData(xpc_object_t remoteConnection, xpc_object_t msg);
#endif

/* getActivePSType
 * returns one of AC, Internal Battery, or External Battery
 */
__private_extern__ int getActivePSType(void);
__private_extern__ bool getPowerState(PowerSources *source, uint32_t *percentage);
__private_extern__ PowerSources _getPowerSource(void);
__private_extern__ CFDictionaryRef CF_RETURNS_RETAINED getActiveUPSDictionary(void);
__private_extern__ void batteryTimeRemaining_setCustomBatteryProps(CFDictionaryRef batteryProps);
__private_extern__ void batteryTimeRemaining_resetCustomBatteryProps(void);

// needed to untanble some cross calls, please don't expand usage of it
__private_extern__ dispatch_queue_t BatteryTimeRemaining_getQ(void);

#ifndef kIOPSFailureKey
#define kIOPSFailureKey                         "Failure"
#endif

#ifndef kIOPMBatteryPercentageFactors
#define kIOPMBatteryPercentageFactors           CFSTR("IOPMBatteryPercentageFactors")
#endif

#ifndef kIOPSDynamicStorePowerAdapterKey
#define kIOPSDynamicStorePowerAdapterKey        "/IOKit/PowerAdapter"
#endif

#ifndef kIOPSDynamicStoreLowBattPathKey
#define kIOPSDynamicStoreLowBattPathKey         "/IOKit/LowBatteryWarning"
#endif

typedef enum {
    kPSTypeUnknown          = 0,
    kPSTypeIntBattery       = 1,
    kPSTypeUPS              = 2,
    kPSTypeAccessory        = 3
} psTypes_t;


/* PSStruct
 * Contains all the details about each power source that the system describes.
 * This struct is the backbone of the IOPowerSources() IOKit API for
 * power source reporting.
 */
typedef struct {
    // powerd will assign a unique psid to all sources.
    int                 psid;
    psTypes_t           psType;

    // Ensure that only the process that created
    // a ps may modify it or destroy it, by recording caller's pid.
    int                 pid;
    XCT_UNSAFE_UNRETAINED dispatch_source_t   procdeathsrc;

    // This is the most current recorded state of this power source.
    CFDictionaryRef     description;
} PSStruct;

// Returns the current version of IOPMPowerSource dictionary in memory. Can return NULL.
__private_extern__ CFDictionaryRef batteryTimeRemaining_copyIOPMPowerSourceDictionary(void);
// Get current UI SOC (battery percent)
__private_extern__ int batteryTimeRemaining_getPercentRemaining(void);
__private_extern__ CFStringRef batteryTimeRemaining_getBatterySerialNumber(void);

#if TARGET_OS_IOS || TARGET_OS_WATCH || TARGET_OS_OSX
void setBHUpdateTimeDelta(xpc_object_t remoteConnection, xpc_object_t msg);
#endif

#if XCTEST
void xctSetPowerSource(PowerSources src);
void xctSetCapacity(uint32_t capacity);
#endif
#endif //_BatteryTimeRemaining_h_
