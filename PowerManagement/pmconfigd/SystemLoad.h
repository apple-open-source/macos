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
__private_extern__ bool getSessionUserActivity();
__private_extern__ uint32_t getSystemThermalState();

__private_extern__ CFAbsoluteTime get_SleepFromUserWakeTime(void);

#endif
