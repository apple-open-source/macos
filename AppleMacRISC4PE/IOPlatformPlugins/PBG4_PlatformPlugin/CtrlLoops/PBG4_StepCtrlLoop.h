/*
 * Copyright (c) 2004-2005 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: PBG4_StepCtrlLoop.h,v 1.10 2005/09/17 00:11:43 raddog Exp $
 *
 */


#ifndef _PBG4_STEPCTRLLOOP_H
#define _PBG4_STEPCTRLLOOP_H

#include <IOKit/pwr_mgt/RootDomain.h>

#include "IOPlatformCtrlLoop.h"
#include "IOPlatformStateSensor.h"

#include <ppc/pms.h>

#ifdef STEPENGINE_DLOG
#undef STEPENGINE_DLOG
#endif

// Uncomment for debug info regarding low level stepper engine behavior (produces mucho output)
//#define STEPENGINE_DEBUG 1

#ifdef STEPENGINE_DEBUG
#define STEPENGINE_DLOG(fmt, args...) if (gDebugLevel & kStepDebugEngine) kprintf(fmt, ## args)
#else
#define STEPENGINE_DLOG(fmt, args...)
#endif


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
#define       kGPUSensorDesc                                  "GPU"

#define kCpuVCoreSelect					"platform-cpu-vcore-select"

#define kAAPLPHandle					"AAPL,phandle"

#define kCtrlLoopPowerAdapterBaseline	"power-adapter-baseline"

// GPU State Count 1 - Max value
#define fGPUStateCount1Max				10
enum {
	// VCore control types
	kVCoreType0					= 0,
	kVCoreType1					= 1,
	kVCoreType10				= 10,		// xxx temp
	
	kStepEnable					= 1,
	kStepEnableMask				= 1,
	kStepNorm					= 0,
	kStepOverTemp				= 1,
	kStepLow					= 2,
	kStepHigh					= 3,
	kStepSleep					= 4,
	kStepControlMask			= 0x30,
	kStepControlShift			= 4,
	kStepDebugMask				= 0xC,
	kStepDebugShift				= 2,
	kStepDebugEngine			= 1,
	kStepDebugLog				= 2,
	kStepTableOverride			= 0x40,
	
	// Battery overcurrent timeout interval (seconds)
	kOvercurrentInterval		= 5,
	// Idle timer interval (10 minutes in seconds)
	kIdleInterval				= (10 * 60)
};

#define kMaxStepTableEntries			2

typedef struct StepTableEntry {
	pmsDef		*address;
	UInt32		length;
	OSData		*dataObj;
};

class PBG4_StepCtrlLoop : public IOPlatformCtrlLoop {
	OSDeclareDefaultStructors( PBG4_StepCtrlLoop )

private:
				bool					fBatteryIsOvercurrent,
										fUsePowerPlay,
										fIdleTimerActive,
										fSetDelayAACK;

				UInt32					fBaselineWatts,
										*fVCoreControl,
										fVCoreControlLength;
									
				OSNumber				*fSleepState;
				
				// Utility routine to locate and add a sensor reference
				inline IOPlatformStateSensor
										*lookupAndAddSensorByKey (const char *key) { 
											IOPlatformStateSensor *sens; 
											addSensor (sens = OSDynamicCast(IOPlatformStateSensor, gPlatformPlugin->lookupSensorByKey( key ))); 
											return sens; }

				// Utility routine to return state iff sensor is valid and registered, otherwise returns 0
				inline UInt32			safeGetState (IOPlatformStateSensor *sens) { return (sens && (sens->isRegistered() == kOSBooleanTrue)) ? sens->getSensorStateUInt32() : 0; }

				// Utility routine to return state iff sensor is valid and registered, otherwise returns 0
				inline void			safeSetState (IOPlatformStateSensor *sens, const OSNumber * state) {  sens->setSensorState(state); sens->sendThresholdsToSensor();}

				IOMemoryDescriptor	*fGpioDesc,
									*fAACKCntlDesc;
				IOMemoryMap			*fGpioMap,
									*fAACKCntlMap;
	
	static		void					setGPUAgressivenessThread( void *self );

protected:
				// meta states
				const OSNumber			*fPreviousMetaState,
										*fSavedMetaState,
										*fPreviousGPUMetaState,
										*fSavedGPUMetaState,
										*fStepControlState;
				
				bool					fNewDesiredGPUState;
				
				// sensors we might care about
				IOPlatformStateSensor	*fCpuTempSensor,
										*fBattTempSensor,
										*fTPadTempSensor,
										*fUniNTempSensor,
										*fPwrSupTempSensor,
										*fGPUIdleSensor;
				// Keeps track of GPU State 1
				UInt32					fGPUState1Count;

				// stepper table data
				StepTableEntry			fStepperTableEntry;

				// Stepper function table - kernel expects table of size pmsSetFuncMax
				pmsSetFunc_t			fPmsFuncTab[pmsSetFuncMax];

				IOPMrootDomain			*fPMRootDomain;

				bool					setGraphicsProcessorStepValue( const OSNumber *newGPUMetaState );
				void					logStepTransition ( UInt32 oldState, UInt32 newState );
				bool					setStepperProgram ( const OSNumber *newMetaState );
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

				bool					setProcessorVoltage( UInt32 stepValue );
				void					setDelayAACK( UInt32 delayValue );


};

#endif // _PBG4_STEPCTRLLOOP_H
