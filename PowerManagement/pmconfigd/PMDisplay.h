/*
 * Copyright (c) 2019 Apple Computer, Inc. All rights reserved.
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

#ifndef PMDisplay_h
#define PMDisplay_h
#include <SkyLight/SLSLegacy.h>
#include <CoreGraphics/CGError.h>
#include <CoreGraphics/CGSDisplay.h>
#include <IOKit/pwr_mgt/powermanagement_mig.h>
#include <sys/queue.h>
#include "PrivateLib.h"

// delay in seconds for clamshell reevalaute after
// wake
#define kClamshellEvaluateDelay 5

__private_extern__ void dimDisplay(void);
__private_extern__ void unblankDisplay(void);
__private_extern__ void blankDisplay(void);
__private_extern__ bool canSustainFullWake(void);
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
__private_extern__ void updateDesktopMode(xpc_object_t connection, xpc_object_t msg);
__private_extern__ void skylightCheckIn(xpc_object_t connection, xpc_object_t msg);
#endif
__private_extern__ bool skylightDisplayOn(void);
__private_extern__ bool isDesktopMode(void);
__private_extern__ void evaluateClamshellSleepState(void);
__private_extern__ void updateClamshellState(void *message);
__private_extern__ uint64_t inFlightDimRequest(void);
__private_extern__ void resetDisplayState(void);

void requestDisplayState(uint64_t state, int timeout);
void requestClamshellState(SLSClamshellState state);
void displayStateDidChange(uint64_t state);
void getClamshellDisplay(void);
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
void handleSkylightCheckIn(void);
void handleDesktopMode(void);
#endif
uint32_t rootDomainClamshellState(void);

#if XCTEST
void xctSetDesktopMode(bool);
void xctSetClamshellState(uint32_t state);
uint32_t xctGetClamshellState(void);
#endif

#endif /* PMDisplay_h */
