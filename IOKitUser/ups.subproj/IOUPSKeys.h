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
 *** SCPreferences file ***
 */
#define kIOUPSPreferencesName           "com.apple.UPSSettings.xml"

/*
 * UPS settings keys
 * These define the power level (units are %) to take action like warning the user
 * or shutting down.
 */
#define kIOUPSLowWarnLevelKey           "Low Warn Level"
#define kIOUPSDeadWarnLevelKey          "Shutdown Level"


/*
 *** SCDynamicStore keys ***
 */
#define kIOUPSDynamicStorePath          "/IOKit/UPS/Status"

/*
 * UPS data keys
 */ 
#define kIOUPSPowerSourceStateKey       "Power Source State"
#define kIOUPSCurrentCapacityKey        "Current Capacity"
#define kIOUPSMaxCapacityKey            "Max Capacity"
#define kIOUPSTimeRemainingKey          "Time Remaining"
#define kIOUPSTimeToFullChargeKey       "Time to Full Charge"
#define kIOUPSVoltageKey                "Voltage"
#define kIOUPSCurrentKey                "Current"

/*
 * UPS state 
 */
#define kIOUPSACPowerValue              "AC Power"
#define kIOUPSBatteryPowerValue         "Battery Power"

