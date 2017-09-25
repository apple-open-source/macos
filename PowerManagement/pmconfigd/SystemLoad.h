/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
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

#ifndef _SystemLoad_h_
#define _SystemLoad_h_


#ifdef XCTEST
#define XCT_UNSAFE_UNRETAINED __unsafe_unretained
#else
#define XCT_UNSAFE_UNRETAINED
#endif

#include <xpc/xpc.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>

#include "BatteryTimeRemaining.h"

#define kMinIdleTimeout     10

typedef struct clientInfo {
    LIST_ENTRY(clientInfo) link;

    XCT_UNSAFE_UNRETAINED xpc_object_t    connection;
    uint32_t        idleTimeout;
    uint64_t        postedLevels;
} clientInfo_t;

/*! UserActiveStruct records the many data sources that affect
 *  our concept of user-is-active; and the user's activity level.
 *
 *  Track every aspect of "user is active on the system" in this struct
 *  right here.
 *  Anyplace in powerd that we consider "is the user active"; that
 *  data needs to go through UserActiveStruct and the functions that
 *  operate on it.
 */
typedef struct {
    int     token;

    /*! presentActive: user has been present and active within 5 minutes
     */
    bool    presentActive;

    /*! userActive is set to true if there is a HID activity or user-active
     *  assertion in last 5 mins.
     */
    bool    userActive;

    /*! loggedIn is true if there's a console user logged in to
     *  loginWindow. Also returns true for ScreenShared users.
     */
    bool    loggedIn;

    /*! rootDomain tracks the IOPMrootDomain's concept of user activity, as
     *  decided by root domain's policy surrounding kStimulusUserIsActive
     *  and kStimulusUserIsInactive.
     *  By definition, rootDomain is true when the system is in S0 notification
     *  wake, and set to false on asleep.
     */
    bool    rootDomain;

    /*! sessionUserActivity tracks if user was ever active since last full wake.
     * This is reset when system enters sleep state.
     */
    bool    sessionUserActivity;


    /*! sessionActivityLevels tracks all activity level bits set since last full wake.
     * This is reset when system enters sleep state.
     */
    uint64_t sessionActivityLevels;


    /*! sleepFromUserWakeTime is a timestamp tracking the last time the system
     *  was in full S0 user wake, and it went to sleep.
     *  We will not update this timestamp on maintenance wakes.
     */
    CFAbsoluteTime  sleepFromUserWakeTime;

    /*! postedLevels is the last set of user activity levels we've published.
     *  This corresponds to the currently available return value to
     *  processes calling IOPMGetUserActivityLevel().
     */
    uint64_t postedLevels;

    LIST_HEAD(, clientInfo) clientList;

    IOHIDEventSystemClientRef hidClient;

    /*!
     * Time stamp of last user activity assertion creation. This is updated for
     * every new user-activity tickle from PMAssertions.c
     */
    uint64_t assertionCreate_ts;

    /*! Idle timeout value(in seconds)  at which HID notification is requested.
     *  This can be different from the default of 5 mins if client specified
     *  different idle timeout value
     */
    uint32_t     idleTimeout;

    /*! Monotonic Time(in seconds) stamp of most recent HID event before kIOHIDEventSystemHIDActivity
     *  is received for system being idle. Valid only if hidActive is false
     */
    uint64_t    lastHid_ts;

    /*! Monotonic Timestamp(in seconds) when the most recent user activity assertion is created.
     *  Valid only when assertionActivityValid is true.
     */
    uint64_t    lastAssertion_ts;

    /*! Bool value tracking if 'lastHid_ts' can be used.
     * "lastHid_ts" is valid value only when this is set to false.
     */
    bool hidActive;

    /*! Bool value to track if there is a valid userActive assertion activity. Set to false on display sleep.
     * "lastAssertion_ts" is not a valid value when this is set to false.
     */
    bool assertionActivityValid;


} UserActiveStruct;


__private_extern__ void SystemLoad_prime(void);

__private_extern__ void SystemLoadBatteriesHaveChanged(IOPMBattery **batt_stats);

__private_extern__ void SystemLoadCPUPowerHasChanged(CFDictionaryRef newCPU);

__private_extern__ void SystemLoadUserStateHasChanged(void);

__private_extern__ void SystemLoadDisplayPowerStateHasChanged(bool displayIsOff);

__private_extern__ void SystemLoadPrefsHaveChanged(void);

__private_extern__ void SystemLoadSystemPowerStateHasChanged(void);

__private_extern__ void SystemLoadUserActiveAssertions(bool _userActiveAssertions);


/* These methods support userActivity tracking
 */

__private_extern__ bool userActiveRootDomain(void);
__private_extern__ void userActiveHandleRootDomainActivity(void);
__private_extern__ void userActiveHandleSleep(void);
__private_extern__ void userActiveHandlePowerAssertionsChanged(void);
__private_extern__ void resetSessionUserActivity();
__private_extern__ bool getSessionUserActivity(uint64_t *sessionLevels);
__private_extern__ uint32_t getSystemThermalState();

__private_extern__ CFAbsoluteTime get_SleepFromUserWakeTime(void);
__private_extern__ uint32_t getTimeSinceLastTickle( );

__private_extern__ void registerUserActivityClient(xpc_object_t peer, xpc_object_t msg);
__private_extern__ void updateUserActivityTimeout(xpc_object_t connection, xpc_object_t msg);
__private_extern__ void deRegisterUserActivityClient(xpc_object_t peer);

#endif
