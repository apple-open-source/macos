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
/*
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: IOPlatformPluginDefs.h,v $
//		Revision 1.4  2003/07/08 04:32:49  eem
//		3288891, 3279902, 3291553, 3154014
//		
//		Revision 1.3  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.2.2.2  2003/06/04 00:00:51  eem
//		More PID stuff, working towards support for forced meta states.
//		
//		Revision 1.2.2.1  2003/06/01 14:52:51  eem
//		Most of the PID algorithm is implemented.
//		
//		Revision 1.2  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.1.2.1  2003/05/16 07:08:45  eem
//		Table-lookup active fan control working with this checkin.
//		
//		Revision 1.1.2.1  2003/05/14 22:07:37  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//
//

#ifndef _IOPLATFORMPLUGINDEFS_H
#define _IOPLATFORMPLUGINDEFS_H

#include <IOKit/IOTypes.h>

// generic keys - for sensors and controls
#define kIOPPluginForceUpdateKey			"force-update"
#define kIOPPluginForceUpdateAllKey			"force-update-all"
#define kIOPPluginForceSensorCurValKey		"force-sensor-current-value"
#define kIOPPluginReleaseForcedSensorKey	"release-forced-sensor"
#define kIOPPluginForceCtrlTargetValKey		"force-control-target-value"
#define kIOPPluginReleaseForcedControlKey	"release-forced-control"
#define kIOPPluginForceCtrlLoopMetaStateKey	"force-ctrlloop-meta-state"
#define kIOPPluginReleaseForcedCtrlLoopKey	"release-forced-ctrlloop"
#define kIOPPluginVersionKey				"version"
#define kIOPPluginTypeKey					"type"
#define kIOPPluginLocationKey				"location"
#define kIOPPluginZoneKey					"zone"
#define kIOPPluginCurrentValueKey			"current-value"
#define kIOPPluginPollingPeriodKey			"polling-period"
#define kIOPPluginRegisteredKey				"registered"

// keys for publishing info to the registry
#define kIOPPluginSensorDataKey				"IOHWSensors"
#define kIOPPluginControlDataKey			"IOHWControls"
#define kIOPPluginCtrlLoopDataKey			"IOHWCtrlLoops"

// Sensor-specific keys
#define kIOPPluginSensorIDKey				"sensor-id"
#define kIOPPluginSensorFlagsKey			"sensor-flags"
#define kIOPPluginCurrentStateKey			"current-state"
#define kIOPPluginLowThresholdKey			"low-threshold"
#define kIOPPluginHighThresholdKey			"high-threshold"

// for interaction with IOHWSensor
#define kIOPPluginNoThreshold				-1
#define kIOPPluginNoPolling					-1

// Sensor type strings - values for the "type" attribute
#define kIOPPluginTypeTempSensor			"temperature"
#define kIOPPluginTypePowerSensor			"power"
#define kIOPPluginTypeVoltageSensor			"voltage"
#define kIOPPluginTypeCurrentSensor			"current"
#define kIOPPluginTypeADCSensor				"adc"

// Control-specific keys
#define kIOPPluginControlIDKey				"control-id"
#define kIOPPluginControlFlagsKey			"control-flags"
#define kIOPPluginTargetValueKey			"target-value"
#define kIOPPluginControlMinValueKey		"min-value"
#define kIOPPluginControlMaxValueKey		"max-value"

// When a control registers, it sends it's type in the registration
// dictionary with the key "control-type".  This is a bit off from
// normal convention of just using "type" as the key for sensors and
// controls, but it is easy to minimize the impact since the only
// place this needs to be handled is in the registration handler.
#define kIOPPluginControlTypeKey			"control-type"

// Control type strings - values for the "type" attribute
#define kIOPPluginTypeSlewControl			"slew"
#define kIOPPluginTypeFanRPMControl			"fan-rpm"
#define kIOPPluginTypeFanPWMControl			"fan-pwm"

/*
 * Control loop keys
 */
#define kIOPPluginCtrlLoopIDKey				"ctrlloop-id"
#define kIOPPluginCtrlLoopMetaState			"current-meta-state"

/*
 * Environment dictionary keys
 */
#define kIOPPluginEnvInfoKey				"IOEnvironment"

// internally generated overtemp condition - if the fans are at full
// and the system is still not cooling, this condition is set.  The
// processor is forced slow.  Boolean value.
#define kIOPPluginEnvInternalOvertemp		"internal-overtemp"

// externally generated overtemp condition. Boolean value.
#define kIOPPluginEnvExternalOvertemp		"external-overtemp"

// the dynamic power step processor speed.  OSNumber value.
#define kIOPPluginEnvDynamicPowerStep		"dynamic-power-step"

// if a control failed it will be flagged here.  OSArray value.
#define kIOPPluginEnvControlFailed			"control-failed"

/*
 * Keys for parsing out the thermal profile
 */
#define kIOPPluginThermalProfileKey			"IOPlatformThermalProfile"

// Human-readable description
#define kIOPPluginThermalGenericDescKey		"Description"

// Index into a string table
#define kIOPPluginThermalLocalizedDescKey	"Desc-Key"		

#define kIOPPluginThermalCreationDateKey	"CreationDate"
#define kIOPPluginThermalValidConfigsKey	"ValidConfigs"
#define kIOPPluginThermalConfigsKey			"ConfigArray"
#define kIOPPluginThermalSensorsKey			"SensorArray"
#define kIOPPluginThermalControlsKey		"ControlArray"
#define kIOPPluginThermalCtrlLoopsKey		"CtrlLoopArray"
#define kIOPPluginThermalSensorIDsKey		"SensorIDArray"
#define kIOPPluginThermalControlIDsKey		"ControlIDArray"
#define kIOPPluginThermalMetaStatesKey		"MetaStateArray"
#define kIOPPluginThermalThresholdsKey		"ThresholdArray"

#define kIOPPluginControlClass				"IOPlatformControl"
#define kIOPPluginSensorClass				"IOPlatformSensor"

// Generic PlatformID string
#define kIOPPluginPlatformIDKey				"platform-id"
#define kIOPPluginPlatformIDValue			"MacRISC4"

// Thermal sensor values and thresholds are 16.16 fixed point format
typedef struct ThermalValue {
	SInt16	intValue;
	UInt16	fracValue;
};

// Macro to convert integer to sensor temperature format (16.16)
#define TEMP_SENSOR_FMT(x) ((x) << 16)

// Others sensors are just integers so combine the two formats
typedef union SensorValue {
	SInt32			sensValue;
	ThermalValue	thermValue;
};

#endif // _IOPLATFORMPLUGINDEFS_H
