/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _IOPLATFORMPLUGINDEFS_H
#define _IOPLATFORMPLUGINDEFS_H

#include <IOKit/IOTypes.h>

/*
 * IOPlatformPluginDefs
 */


/*
 * Property keys for publishing sensor, control and control-loop instance data
 * in the I/O registry.
 *
 * The IOPlatformSensor, IOPlatformControl and IOPlatformCtrlLoop instances store
 * their instance data in an OSDictionary (infoDict).  The Platform Plugin creates
 * properties in its I/O registry node, using the keys defined below, which contain
 * arrays of pointers to the infoDict for each IOPlatformCtrlLoop instance (under
 * IOHWCtrlLoops) and each *registered* IOPlatformSensor and IOPlatformControl (under
 * IOHWSensors and IOHWControls, respectively).
 */
#define kIOPPluginSensorDataKey				"IOHWSensors"
#define kIOPPluginControlDataKey			"IOHWControls"
#define kIOPPluginCtrlLoopDataKey			"IOHWCtrlLoops"

/*
 * Keys defined by the Thermal SW ERS
 *
 * (Most of) these keys are define in the Thermal SW ERS.  Each sensor and control
 * has a set of properties that goes along with it.  As such, the following keys
 * are used to key member data in the infoDict, and will show up in the I/O
 * registry.
 */

// Thermal SW parameter version
#define kIOPPluginVersionKey				"version"

// Sensor/Control type key, followed by possible values
#define kIOPPluginTypeKey					"type"

// Sensor type strings - values for the "type" attribute
#define kIOPPluginTypeTempSensor			"temperature"
#define kIOPPluginTypePowerSensor			"power"
#define kIOPPluginTypeVoltageSensor			"voltage"
#define kIOPPluginTypeCurrentSensor			"current"
#define kIOPPluginTypeADCSensor				"adc"

// Control type strings - values for the "type" attribute
#define kIOPPluginTypeSlewControl			"slew"
#define kIOPPluginTypeFanRPMControl			"fan-rpm"
#define kIOPPluginTypeFanPWMControl			"fan-pwm"

// Location key, contains a string to identify the physical location and/or human-
// readable description of the device
#define kIOPPluginLocationKey				"location"

// Thermal zone key.  Value is an OSData.
#define kIOPPluginZoneKey					"zone"

// Current value key, holds the sensor's current value or the control's measured
// value as an OSNumber.
#define kIOPPluginCurrentValueKey			"current-value"

// Sensor polling period key (optional)
#define kIOPPluginPollingPeriodKey			"polling-period"
#define kIOPPluginPollingPeriodNSKey		"polling-period-ns"

// Boolean to flag whether or not a driver has registered as this sensor/control
#define kIOPPluginRegisteredKey				"registered"

// Sensor ID key
#define kIOPPluginSensorIDKey				"sensor-id"

// Sensor flags key  -- currently unused / no bits defined
#define kIOPPluginSensorFlagsKey			"sensor-flags"

// Current state key -- used for threshold-driven state sensors (see IOPlatformStateSensor)
#define kIOPPluginCurrentStateKey			"current-state"

// Low threshold key -- holds the low threshold that corresponds to the current
// state (see IOPlatformStateSensor)
#define kIOPPluginLowThresholdKey			"low-threshold"

// High threshold -- holds the high threshold that corresponds to the current
// state (see IOPlatformStateSensor)
#define kIOPPluginHighThresholdKey			"high-threshold"

// Control ID key
#define kIOPPluginControlIDKey				"control-id"

// Control flags key -- currently unused / no bits defined
#define kIOPPluginControlFlagsKey			"control-flags"

// Control target value key -- the control is instructed to adjust itself to this value
#define kIOPPluginTargetValueKey			"target-value"

// Control initial target value key -- this can be included in a ControlArray entry
// in the thermal profile to force a control to be programmed to a certain target
// when it registers
#define kIOPPluginInitialTargetKey			"initial-target"

// Minimum value -- used in IOPlatformControl as a lower bound for target-value
// and current-value
#define kIOPPluginControlMinValueKey		"min-value"

// Maximum value -- used in IOPlatformControl as an upper bound for target-value
// and current-value
#define kIOPPluginControlMaxValueKey		"max-value"

// Safe value -- maximum safe target-value when the chassis is open (used in
// IOPlatformControl, IOPlatformPIDCtrlLoop, etc.)
#define kIOPPluginControlSafeValueKey		"safe-value"

// for interaction with IOHWSensor class
#define kIOPPluginNoThreshold				-1
#define kIOPPluginNoPolling					-1

// When a control registers, it sends it's type in the registration
// dictionary with the key "control-type".  This is a bit off from
// normal convention of just using "type" as the key for sensors and
// controls, but it is easy to minimize the impact since the only
// place this needs to be handled is in the registration handler.
#define kIOPPluginControlTypeKey			"control-type"

/*
 * Control loop keys
 */

// Control loop ID key
#define kIOPPluginCtrlLoopIDKey				"ctrlloop-id"

// Current control loop meta state key
#define kIOPPluginCtrlLoopMetaState			"current-meta-state"

/*
 * Environment dictionary keys
 */

// Key for publishing the environment dictionary in the I/O registry, alongside
// IOHWSensors, IOHWControls and IOHWCtrlLoops
#define kIOPPluginEnvInfoKey				"IOEnvironment"

// Global environmental variable keys

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

// if a control loop is driving its output control at max, it can set
// this environment array flag so other control loops know it's maxed
// out
#define kIOPPluginEnvCtrlLoopOutputAtMax	"ctrlloop-output-at-max"

// state of the chassis intrusion switch -- depending on the platform, this
// corresponds to the clamshell switch, or door ajar switch, etc.
#define kIOPPluginEnvChassisSwitch			"chassis-switch"

// platform flags bitfield defined in at the top level of the thermal
// profile:
// Thermal Profile property key
#define kIOPPluginPlatformFlagsKey			"PlatformFlags"
// Environment dictionary property key
#define kIOPPluginEnvPlatformFlags			"platform-flags"

// platform flags bit definitions -- 32-bit field
enum
{
	// [3696669] temporary flag to isolate door-ajar response to PowerMac9,1
	kIOPPluginFlagEnableSafeModeHack	= 0x00000001
};

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

// Control's current-value and target-value are UInt32 type
typedef UInt32 ControlValue;

#endif // _IOPLATFORMPLUGINDEFS_H
