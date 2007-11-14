/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: PBG4_DPSCtrlLoop.h,v 1.10 2005/01/06 16:19:09 raddog Exp $
 *
 */


#ifndef _PBG4_DPSCTRLLOOP_H
#define _PBG4_DPSCTRLLOOP_H

#include <IOKit/pwr_mgt/RootDomain.h>

#include "IOPlatformCtrlLoop.h"
#include "IOPlatformStateSensor.h"

// For GPU power control
#ifndef sub_iokit_graphics
#	define sub_iokit_graphics           err_sub(5)
#endif
#ifndef kIOFBLowPowerAggressiveness
#	define kIOFBLowPowerAggressiveness iokit_family_err(sub_iokit_graphics,1)
#endif

// Sensor Desc keys
#define kCPUTempSensorDesc				"CPU_TEMP"
#define kUniNTempSensorDesc				"UNIN_TEMP"
#define kBatteryTempSensorDesc			"BATT_TEMP"
#define kPwrSupTempSensorDesc			"PWR_SUP_TEMP"
#define kTrackPadTempSensorDesc			"TPAD_TEMP"

#define kCpuVCoreSelect					"platform-cpu-vcore-select"

#define kAAPLPHandle					"AAPL,phandle"

#define kCtrlLoopPowerAdapterBaseline	"power-adapter-baseline"

enum {
	// Battery overcurrent timeout interval (seconds)
	kOvercurrentInterval		= 5,
	// Idle timer interval (10 minutes in seconds)
	kIdleInterval				= (10 * 60)
};

class PBG4_DPSCtrlLoop : public IOPlatformCtrlLoop {
	OSDeclareDefaultStructors( PBG4_DPSCtrlLoop )

private:
				bool					fBatteryIsOvercurrent,
										fUsePowerPlay,
										fIdleTimerActive;

				UInt32					fBaselineWatts;
				
				// Utility routine to locate and add a sensor reference
				inline IOPlatformStateSensor
										*lookupAndAddSensorByKey (const char *key) { 
											IOPlatformStateSensor *sens; 
											addSensor (sens = OSDynamicCast(IOPlatformStateSensor, gPlatformPlugin->lookupSensorByKey( key ))); 
											return sens; }

				// Utility routine to return state iff sensor is valid and registered, otherwise returns 0
				inline UInt32			safeGetState (IOPlatformStateSensor *sens) { return (sens && (sens->isRegistered() == kOSBooleanTrue)) ? sens->getSensorStateUInt32() : 0; }

protected:
				// meta states
				const OSNumber			*fPreviousMetaState,
										*fSavedMetaState,
										*fPreviousGPUMetaState,
										*fSavedGPUMetaState;
				
				// platform-function symbols for voltage control
				const OSSymbol			*fCpuVoltageControlSym;
				
				// platform-function services for voltage control
				IOService				*fCpuVoltageControlService;

				// sensors we might care about
				IOPlatformStateSensor	*fCpuTempSensor,
										*fBattTempSensor,
										*fTPadTempSensor,
										*fUniNTempSensor,
										*fPwrSupTempSensor;

				IOPMrootDomain			*fPMRootDomain;

				bool					setProcessorVoltage( UInt32 stepValue );
				bool					setProcessorVStepValue( const OSNumber *metaState );
				bool					setGraphicsProcessorStepValue( const OSNumber *newGPUMetaState );
				bool					setInitialState( void );
				bool					batteryIsOvercurrent( void );
				bool					lowPowerForcedDPS ( void );
				bool					idleForcedGPSLow ( void );

public:

	virtual		IOReturn				initPlatformCtrlLoop(const OSDictionary *dict);

	virtual		bool					updateMetaState( void );
	virtual		void					adjustControls( void );

	virtual		void					willSleep( void );
	virtual		void					didWake( void );
	virtual		void					deadlinePassed( void );

};

#endif // _PBG4_DPSCTRLLOOP_H
