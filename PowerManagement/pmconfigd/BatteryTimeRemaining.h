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

#if TARGET_OS_IPHONE || POWERD_IOS_XCTEST

#define kBHSvcFlagsVerison0     0
#define kBHSvcFlagsVersion1     1
#define kBHSvcFlagsVersion2     2
#define kBHSvcFlagsVersion3     3
#define kBatteryHealthCurrentVersion        kBHSvcFlagsVersion3

#define kMinNominalCapacityPercentage       1
#define kMaxNominalCapacityPercentage       150
#define kNominalCapacityPercetageThreshold  80
#define kInitialNominalCapacityPercentage   104

// kTrueNCCCycleCountThreshold - If previously calculated NCCP is not available, NCCP is set to h/w specified value only if
// battery cycle count is above kTrueNCCCycleCountThreshold. Otherwise, NCCP is set to kInitialNominalCapacityPercentage
#define kTrueNCCCycleCountThreshold         20  // CycleCount above which NCCP is set to true value(in case past data is not available)

// kNCCMinCycleCountChange - Change in battery cycle count required before triggering change in NCCP.
#define kNCCMinCycleCountChange             5

// kNCCChangeLimit - Percentage by which NCCP is reduced after kNCCMinCycleCountChange change in cycle count
#define kNCCChangeLimit                     1

// kMinTimeDeltaForBattRead - Minimum time(in seconds) between reading battery data for battery health evaluation
#define kMinTimeDeltaForBattRead           (24*60*60)  // 24hrs

#define kBatteryHealthUsesUPO        0x594553 // YES
#define kBatteryHealthWithoutUPO     0x4e4f   // NO

#define kMitigatedUPOCountThreshold         0
#define kBatteryCellDisconnectThreshold     10

#define kCFPrefsUPOMetricsDomain            "com.apple.thermalmonitor.upostepper.metrics"
#define kCFPrefsMitigatedUPOCountKey        "mitigatedUPOCount"

#define kSmcKeyBatteryFeatureSet                'BFS0'
#define BATTERY_FEATURE_HEATMAP_VOLTAGE_TEMP    (1 << 0)
#define BATTERY_FEATURE_HEATMAP_SOC_TEMP        (1 << 1)
#define BATTERY_FEATURE_NCC_FILTERING           (1 << 2)

#endif

__private_extern__ void BatteryTimeRemaining_prime(void);
__private_extern__ void BatteryTimeRemaining_finish(void);
__private_extern__ void BatteryTimeRemainingSleepWakeNotification(natural_t messageType);

__private_extern__ void BatteryTimeRemainingRTCDidResync(void);

__private_extern__ void readAndPublishACAdapter(bool adapterExists, CFDictionaryRef batteryACDict);
__private_extern__ void  initializeBatteryCalculations(void);
/*!
 * Pass kInternalBattery to kernelPowerSourcesDidChange when you need 
 * PM to re-evaluate the single internal battery (modeled as an IOPMPowerSource)
 * inside your device.
 */
#define kInternalBattery      NULL
__private_extern__ void kernelPowerSourcesDidChange(IOPMBattery *battery_info);

__private_extern__ bool BatteryHandleDeadName(mach_port_t deadName);

__private_extern__ void BatterySetNoPoll(bool noPoll);

__private_extern__ bool isFullyCharged(IOPMBattery *b);

__private_extern__ void sendAdapterDetails(xpc_object_t remoteConnection, xpc_object_t msg);

/* getActivePSType
 * returns one of AC, Internal Battery, or External Battery
 */
__private_extern__ int getActivePSType(void);
__private_extern__ CFDictionaryRef getActiveBatteryDictionary(void);
__private_extern__ CFDictionaryRef getActiveUPSDictionary(void);


#ifndef kIOPSFailureKey
#define kIOPSFailureKey                         "Failure"
#endif

#define kBatteryPermFailureString               "Permanent Battery Failure"

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
    long                psid;
    
    psTypes_t           psType;
    
    // Ensure that only the process that created
    // a ps may modify it or destroy it, by recording caller's pid.
    int                 pid;
    XCT_UNSAFE_UNRETAINED dispatch_source_t   procdeathsrc;
    
    // This is the most current recorded state of this power source.
    CFDictionaryRef     description;
    
    // log of previous battery updates, maintained as ring buffer
    CFMutableArrayRef       log;
    CFIndex                 logIdx;         // Index for next record
    uint64_t                logUpdate_ts;   // Timestamp of last log
} PSStruct;

#endif //_BatteryTimeRemaining_h_
