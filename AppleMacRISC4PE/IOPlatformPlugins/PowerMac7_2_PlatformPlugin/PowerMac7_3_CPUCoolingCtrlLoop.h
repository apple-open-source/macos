/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _POWERMAC7_3_CPUCOOLINGCTRLLOOP_H
#define _POWERMAC7_3_CPUCOOLINGCTRLLOOP_H

#include "IOPlatformPIDCtrlLoop.h"

#define kPM72CPUPIDDatasetsKey		"PM72-CPU-PID-datasets"
#define kPM72ProcessorBinKey		"processor-bin"
#define kPM72MaxPowerKey			"power-max"
#define kPM72MaxPowerAdjustmentKey	"power-max-adjustment"
#define kPM72PIDDatasetVersionKey	"pid-dataset-version"
#define kPM72PIDDatasetSourceKey	"pid-dataset-source"
#define kPM72PIDDatasetSourceIICROM	"MPU EEPROM"
#define kPM72PIDDatasetSourcePList  "Thermal Profile PList"

// after 30 seconds at max cooling if we're still above T_max, sleep the machine
#define kPM72CPUMaxCoolingLimit		30

// if at any time the temperature exceeds T_max + 8, sleep the machine immediately
// this is in 16.16 fixed point format
#define kPM72CPUTempCriticalOffset	(8 << 16)

/*!	@class PowerMac7_3_CPUCoolingCtrlLoop
	@abstract This class implements a PID-based fan control loop for use on PowerMac7,2 machines.  This control loop is designed to work for both uni- and dual-processor machines.  There are actually two (mostly) independent control loops on dual-processor machines, but control is implemented in one control loop object to ease implementation of tach-lock. */

class PowerMac7_3_CPUCoolingCtrlLoop : public IOPlatformPIDCtrlLoop
{

	OSDeclareDefaultStructors(PowerMac7_3_CPUCoolingCtrlLoop)

protected:

	IOPlatformControl * secOutputControl, *thiOutputControl, *fouOutputControl, *fifOutputControl, *sixOutputControl;
	IOPlatformSensor  *inputSensor2, *currentSensor1, * voltageSensor1, * powerSensor1, *currentSensor2, * voltageSensor2, * powerSensor2;

	// for debugging purposes
#ifdef CTRLLOOP_DEBUG
	IOPlatformControl * slewControl;
#endif

	// the index for this cpu
	UInt32 procID;

	// intake fan speed scaling factor
	UInt32 intakeScaling;

	// dedicated temperature sample buffer -- sampleHistory will be used for power readings
	SensorValue tempHistory[2];
	int tempIndex;


	// defaults...
	#define kPM73CPU_DEFAULT_slewAverageSampleCount		180
	#define kPM73CPU_DEFAULT_slewAverageOffset			10
	#define kPM73CPU_DEFAULT_slewOffset					8
	#define kPM73CPU_DEFAULT_sleepOffset				20

	// Tmax average sample history data...
	SInt32		*tMaxAverageHistory;
	SInt32		tMaxAverageIndex;
	SInt32		tMaxAverage;
	SInt32		tMaxAverageSampleCount;
	bool		fOvertempAverage;
	UInt32		delayToReleaseFanAfterSlew;

	// Max CPU temperature at diode
	SensorValue inputMax;

	// PowerMaxAdj = PowerMaxROM - AdjStaticFactor
	SensorValue powerMaxAdj;

	// [3751599] pump max & min RPMs
	ControlValue outputMaxForPump;
	ControlValue outputMinForPump;

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual void free( void );

	//virtual const OSNumber *calculateNewTarget( void ) const;

	virtual bool cacheMetaState( const OSDictionary * metaState );
	bool choosePIDDataset( const OSDictionary * ctrlLoopDict );
	OSDictionary * fetchPIDDatasetFromROM( void ) const;
	int comparePIDDatasetVersions( const OSData * v1, const OSData * v2 ) const;

public:

	// By setting a deadline and handling the deadlinePassed() callback, we get a periodic timer
	// callback that is funnelled through the platform plugin's command gate.
	//virtual void deadlinePassed( void );

	virtual IOReturn initPlatformCtrlLoop(const OSDictionary *dict);
	virtual bool updateMetaState( void );
	virtual void adjustControls( void ); // [4001211] Override to prevent calculateNewTarget() from being called from when the environment changes.
	virtual SensorValue getAggregateSensorValue( void );
	virtual void deadlinePassed( void );
	bool acquireSample( void );		// gets a sample (using clock_get_uptime() and getAggregateSensorValue()) and stores it at 
	virtual SensorValue calculateDerivativeTerm( void ) const;
	virtual ControlValue calculateNewTarget( void ) const;
	virtual void sendNewTarget( ControlValue newTarget );
	//virtual void deadlinePassed( void );
	virtual void sensorRegistered( IOPlatformSensor * aSensor );
	virtual void controlRegistered( IOPlatformControl * aControl );
};

#endif	// _POWERMAC7_3_CPUCOOLINGCTRLLOOP_H
