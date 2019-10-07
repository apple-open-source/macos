/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */


#ifndef _PMConnection_h_
#define _PMConnection_h_


#define LOG_SLEEPSERVICES 1
/*
 * Struct for gSleepService 
 */
typedef struct {
    int                         notifyToken;
    CFStringRef                 uuid;
    long                        capTime;
} SleepServiceStruct;

// Bits for gPowerState
#define kSleepState                     0x01
#define kDarkWakeState                  0x02
#define kDarkWakeForBTState             0x04
#define kDarkWakeForSSState             0x08
#define kDarkWakeForMntceState          0x10
#define kDarkWakeForServerState         0x20
#define kFullWakeState                  0x40
#define kNotificationDisplayWakeState   0x80
#define kPowerStateMask                 0xff


__private_extern__ void PMConnection_prime(void);

// PMAssertions.c calls into this when a PreventSystemSleep assertion is taken
__private_extern__ IOReturn _unclamp_silent_running(bool sendNewCapBits);
__private_extern__ bool isInSilentRunningMode(void);

__private_extern__ bool _can_revert_sleep(void);
__private_extern__ void _set_sleep_revert(bool state);

__private_extern__ io_connect_t getRootDomainConnect(void);
__private_extern__ bool isA_BTMtnceWake(void);
__private_extern__ bool isA_SleepSrvcWake(void);
__private_extern__ void set_SleepSrvcWake(void);
__private_extern__ bool isA_FullWake(void);
__private_extern__ void cancelPowerNapStates(void);

__private_extern__ bool isA_SleepState(void);
__private_extern__ bool isA_DarkWakeState(void);
__private_extern__ bool isA_NotificationDisplayWake(void);
__private_extern__ void set_NotificationDisplayWake(void);
__private_extern__ void cancel_NotificationDisplayWake(void);

__private_extern__ void InternalEvalConnections(void);
__private_extern__ kern_return_t getPlatformSleepType(uint32_t *sleepType, uint32_t *standbyTimer);
__private_extern__ void setDwlInterval(uint32_t newInterval);
__private_extern__ int getBTWakeInterval(void);
__private_extern__ uint64_t getCurrentWakeTime(void);
__private_extern__ void updateWakeTime(void);
__private_extern__ void updateCurrentWakeStart(uint64_t timestamp);
__private_extern__ void updateCurrentWakeEnd(uint64_t timestamp);

__private_extern__ int getCurrentSleepServiceCapTimeout(void);
/** Sets whether processes should get modified vm behavior for darkwake. */
__private_extern__ void setVMDarkwakeMode(bool darkwakeMode);
__private_extern__ void cancelDarkWakeCapabilitiesTimer(void);

#ifdef XCTEST
__private_extern__ void xctSetPowerState(uint32_t powerState);
#endif
#endif

