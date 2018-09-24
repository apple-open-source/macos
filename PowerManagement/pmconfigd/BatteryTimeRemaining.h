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
