/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _IOPLATFORMPLUGINSYMBOLS_H
#define _IOPLATFORMPLUGINSYMBOLS_H

/*
 * the platform plugin's symbol extern const'd for usage by helper classes
 */

class OSSymbol;
class OSNumber;
class IOPlatformPlugin;

extern const OSSymbol * gIOPPluginForceUpdateKey;
extern const OSSymbol * gIOPPluginForceUpdateAllKey;
extern const OSSymbol * gIOPPluginForceSensorCurValKey;
extern const OSSymbol * gIOPPluginReleaseForcedSensorKey;
extern const OSSymbol * gIOPPluginForceControlTargetValKey;
extern const OSSymbol * gIOPPluginReleaseForcedControlKey;
extern const OSSymbol * gIOPPluginForceCtrlLoopMetaStateKey;
extern const OSSymbol * gIOPPluginReleaseForcedCtrlLoopKey;
extern const OSSymbol * gIOPPluginVersionKey;
extern const OSSymbol * gIOPPluginTypeKey;
extern const OSSymbol * gIOPPluginLocationKey;
extern const OSSymbol * gIOPPluginZoneKey;
extern const OSSymbol * gIOPPluginCurrentValueKey;
extern const OSSymbol * gIOPPluginPollingPeriodKey;
extern const OSSymbol * gIOPPluginRegisteredKey;
extern const OSSymbol * gIOPPluginSensorDataKey;
extern const OSSymbol * gIOPPluginControlDataKey;
extern const OSSymbol * gIOPPluginCtrlLoopDataKey;
extern const OSSymbol * gIOPPluginSensorIDKey;
extern const OSSymbol * gIOPPluginSensorFlagsKey;
extern const OSSymbol * gIOPPluginCurrentStateKey;
extern const OSSymbol * gIOPPluginLowThresholdKey;
extern const OSSymbol * gIOPPluginHighThresholdKey;
extern const OSSymbol * gIOPPluginTypeTempSensor;
extern const OSSymbol * gIOPPluginTypePowerSensor;
extern const OSSymbol * gIOPPluginTypeVoltageSensor;
extern const OSSymbol * gIOPPluginTypeCurrentSensor;
extern const OSSymbol * gIOPPluginTypeADCSensor;
extern const OSSymbol * gIOPPluginControlIDKey;
extern const OSSymbol * gIOPPluginControlFlagsKey;
extern const OSSymbol * gIOPPluginTargetValueKey;
extern const OSSymbol * gIOPPluginControlMinValueKey;
extern const OSSymbol * gIOPPluginControlMaxValueKey;
extern const OSSymbol * gIOPPluginTypeSlewControl;
extern const OSSymbol * gIOPPluginTypeFanRPMControl;
extern const OSSymbol * gIOPPluginTypeFanPWMControl;
extern const OSSymbol * gIOPPluginControlClass;
extern const OSSymbol * gIOPPluginSensorClass;
extern const OSSymbol * gIOPPluginEnvInternalOvertemp;
extern const OSSymbol * gIOPPluginEnvExternalOvertemp;
extern const OSSymbol * gIOPPluginEnvDynamicPowerStep;
extern const OSSymbol * gIOPPluginEnvControlFailed;
extern const OSSymbol * gIOPPluginCtrlLoopIDKey;
extern const OSSymbol * gIOPPluginCtrlLoopMetaState;
extern const OSSymbol * gIOPPluginThermalLocalizedDescKey;
extern const OSSymbol * gIOPPluginThermalValidConfigsKey;
extern const OSSymbol * gIOPPluginThermalMetaStatesKey;
extern const OSSymbol * gIOPPluginPlatformID;

extern const OSNumber * gIOPPluginZero;
extern const OSNumber * gIOPPluginOne;

extern IOPlatformPlugin * platformPlugin;

#endif	// _IOPLATFORMPLUGINSYMBOLS_H
