/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _privatelib_h_
#define _privatelib_h_

#include <TargetConditionals.h>

#if !TARGET_OS_EMBEDDED
  #define HAVE_CF_USER_NOTIFICATION     1
  #define HAVE_HID_SYSTEM               1
  #define HAVE_SMART_BATTERY            1
#endif

// TODO: Delete me once sync'd with OS (rdar://5608255)
#ifndef kIOPMPSBatteryChargeStatusKey
    #define kIOPMPSBatteryChargeStatusKey               "ChargeStatus"
#endif

struct IOPMBattery {
    io_registry_entry_t     me;
    io_object_t             msg_port;
    CFMutableDictionaryRef  properties;
    bool                    externalConnected:1;
    bool                    externalChargeCapable:1;
    bool                    isCharging:1;
    bool                    isPresent:1;
    bool                    markedNeedsReplacement:1;
    int                     currentCap;
    int                     maxCap;
    int                     designCap;
    int                     voltage;
    int                     avgAmperage;
    int                     instantAmperage;
    int                     maxerr;
    int                     cycleCount;
    int                     location;
    int                     hwAverageTR;
    int                     hwInstantTR;
    int                     swCalculatedTR;
    int                     invalidWakeSecs;
    CFStringRef             health;
    CFStringRef             failureDetected;
    CFStringRef             name;
    CFStringRef             dynamicStoreKey;
    CFStringRef             chargeStatus;
};
typedef struct IOPMBattery IOPMBattery;

__private_extern__ IOPMBattery **_batteries(void);
__private_extern__ IOPMBattery *_newBatteryFound(io_registry_entry_t);
__private_extern__ void _batteryChanged(IOPMBattery *);
__private_extern__ bool _batteryHas(IOPMBattery *, CFStringRef);
__private_extern__ int  _batteryCount(void);
__private_extern__ void  _removeBattery(io_registry_entry_t);

// Returns 10.0 - 10.4 style IOPMCopyBatteryInfo dictionary, when possible.
__private_extern__ CFArrayRef _copyLegacyBatteryInfo(void);

__private_extern__ void _askNicelyThenShutdownSystem(void);
__private_extern__ void _askNicelyThenSleepSystem(void);

// getSystemManagementKeyInt32 - only valid for i386 platforms
__private_extern__ IOReturn getSystemManagementKeyInt32(uint32_t key, uint32_t *val);

#if !TARGET_OS_EMBEDDED
__private_extern__ CFUserNotificationRef _showUPSWarning(void);
#endif

__private_extern__ IOReturn _setRootDomainProperty(
                                    CFStringRef     key,
                                    CFTypeRef       val);

__private_extern__ int callerIsRoot(int uid, int gid);
__private_extern__ int callerIsAdmin(int uid, int gid);
__private_extern__ int callerIsConsole(int uid, int gid);


__private_extern__ void _oneOffHacksSetup(void);

#endif

