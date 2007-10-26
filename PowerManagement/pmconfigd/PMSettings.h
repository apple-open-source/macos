/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 29-Aug-02 ebold created
 *
 */
 
#ifndef _PMSettings_h_
#define _PMSettings_h_

/* Power Management profile bits */
enum {
    kPMForceLowSpeedProfile         = (1<<0),
    kPMForceHighSpeed               = (1<<1),
    kPMPreventIdleSleep             = (1<<2),
    kPMPreventDisplaySleep          = (1<<3)
};

__private_extern__ void PMSettings_prime(void);
 
__private_extern__ void PMSettingsSleepWakeNotification(natural_t);

__private_extern__ void PMSettingsSupportedPrefsListHasChanged(void);

__private_extern__ void PMSettingsPrefsHaveChanged(void);

__private_extern__ void PMSettingsBatteriesHaveChanged(CFArrayRef);

__private_extern__ void PMSettingsPSChange(CFTypeRef);

__private_extern__ void PMSettingsConsoleUserHasChanged(void);

// For UPS shutdown/restart code in PSLowPower.c
__private_extern__ CFDictionaryRef  PMSettings_CopyActivePMSettings(void);

__private_extern__ IOReturn _activateForcedSettings(CFDictionaryRef);

// For IOPMAssertions code in SetActive.c
__private_extern__ void overrideSetting(int, int);
__private_extern__ void activateSettingOverrides(void);


#endif _PMSettings_h_
