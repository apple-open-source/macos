/*
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2005 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _POWERMAC11_2_CPUS_CTRLLOOP_H
#define _POWERMAC11_2_CPUS_CTRLLOOP_H

#include "IOPlatformPIDCtrlLoop.h"


// Uncomment the following define if you wish to public a power sensor (it is a lot of wasted memory
// for a multiplication, but it may be useful for debugging.
#define PUBLISH_POWER_SENSOR

// The following key identify a group of control loops (all control loops with the same key value)
// that share the same output. Because output can not be tecnically shared the GroupVirtualControl
// actually chooses one specific value of all the ones presented by the control loop and uses that
// to control all the outputs.
#define kControlGroupingID "ControlGroupingID"

// Keys for CPU Fan PID parameters
#define kIOPPIDCtrlLoopMaxPowerKey				"power-max"
#define kIOPPIDCtrlLoopMaxPowerAdjustmentKey	"power-max-adjustment"
#define kIOPPIDCtrlLoopTargetTempDelta			"input-target-delta"

// Keys for CPU and Core ID
#define	kIOControlLoopCPUID						"cpu-id"
#define	kIOControlLoopCoreID					"core-id"

// Keys for the overtemp:
#define kOverTempSlewAverageOffset				"OverTempSlewAverageOffset"
#define kOverTempSlewImmediateOffset			"OverTempSlewImmediateOffset"
#define kEndOfSlewOffset						"EndOfSlewOffset"
#define	kOverTempSlewAverageNumberOfSamples		"OverTempSlewAverageNumberOfSamples"

// Keys for emergency sleep
#define kOverTempSleepAverageOffset				"OverTempSleepAverageOffset"
#define kOverTempSleepImmediateOffset			"OverTempSleepImmediateOffset"
#define	kOverTempSleepAverageNumberOfSamples	"OverTempSleepAverageNumberOfSamples"

// Values for overtemp:
#define kPM11_2CPU_DEFAULT_slewAverageOffset							(unsigned long long)0
#define kPM11_2CPU_DEFAULT_slewOffset									(unsigned long long)10
#define kPM11_2CPU_DEFAULT_slewAverageNumberOfSamples					(unsigned long long)180
#define kPM11_2CPU_DEFAULT_endOfSlewOffset								(unsigned long long)-10

// Values for Sleep
#define kPM11_2CPU_DEFAULT_sleepAverageOffset							(unsigned long long)10
#define kPM11_2CPU_DEFAULT_sleepOffset									(unsigned long long)14
#define kPM11_2CPU_DEFAULT_sleepAverageNumberOfSamples					(unsigned long long)180

// Control for how fast we let the fan slow down
#define kIOGlobalContorlDecreaseFanLimit								"decrease-fan-limit"
#define kPM11_2CPU_GlobalContorlDecreaseFanLimit						(unsigned long long)20

// Control for how fast we let the fan slow down
#define kIOOverrideMinValueTargetValue									"OverrideMinValue"

// The minimal output value is inherited from the control minimum value
// however it is often too small so this is a reasonable alternative
#define kMinReasonableOutputFor2Way										515
#define kMinReasonableOutputFor4Way										1000

// Forward declaration for a GroupControl (see kControlGroupingID);
class IOGroupVirtualControl;

class PowerMac11_2_CPUsCtrlLoop : public IOPlatformPIDCtrlLoop
{

	OSDeclareDefaultStructors(PowerMac11_2_CPUsCtrlLoop)
	
private:
	// This control loop belongs to the following cupu/core
	UInt8	mCoreID;
	UInt8	mCpuID;
	
	// This is a dictionary shared between all the objects of this class
	// it is a dictionary that matches ids to IOGroupVirtualControls
	static OSDictionary *gIOGroupVirtualControlDictionary;
	
	typedef struct InputSet
	{
		IOService		 *referenceSAT;
		IOPlatformSensor *temperatureSensor;
		IOPlatformSensor *powerSensor;
	} InputSet;

	IOGroupVirtualControl	*mGroupControl;

#define kMaxNumberOfInputSets	4

	InputSet	mInputSetArray[kMaxNumberOfInputSets];
	int			mActualNumberOfInputSets;
	
	bool		mAllSensorsRegistred;
	bool		mAllControlsRegistred;
	
	// Max CPU temperature at diode
	SensorValue inputMax;

	// PowerMaxAdj = PowerMaxROM - AdjStaticFactor
	SensorValue powerMaxAdj;

	// dedicated temperature sample buffer -- sampleHistory will be used for power readings
	#define kTemperatureSensorBufferSize 2
	SensorValue tempHistory[ kTemperatureSensorBufferSize ];
	int tempIndex;
	
	// Override for the min OutputValue:
	ControlValue	mOverrideMinOutputValue;

	// Last targetValue:
	ControlValue	mLastTargetValue;
	
	// The last delta (Tmax - average * G_r)
	SensorValue		mLastCalculatedDelta;

	// Overtemp Offsets:
	SensorValue		mOverTempSlewAverageOffset;
	SensorValue		mOverTempSlewImmediateOffset;
	SensorValue		mEndOfSlewOffset;
	int				mOverTempSlewAverageNumberOfSamples;
	
	// Sleep Offsets:
	SensorValue		mOverTempSleepImmediateOffset;
	SensorValue		mOverTempSleepAverageOffset;
	int				mOverTempSleepAverageNumberOfSamples;
	
	// Temperature History for Slewing or Sleeping
	SensorValue		*mTemperatureHistory;
	int				mTemperatureHistorySize;
	int				mCurrentPointInTemperatureHistory;

protected:

	virtual	SensorValue getSensorValue( IOPlatformSensor *sensor);
	virtual bool allControlsAndSensorsRegistredCheck( void );
	virtual OSData* getRawDatasetFor( UInt8 cpuid, UInt8 coreid );
	virtual OSData *getSubData( OSData * source, UInt32 offset, UInt32 length);
	virtual OSDictionary* getPIDDataset( const OSDictionary* dict, UInt8 cpuID, UInt8 coreID);
	
	// Need to know the number of Cores in this machine so we know ahed how many couples of sensor (Power + Temp)
	// we can expect. This is needed in case the control loop is configured as single loop on a single CPU
	// (dual core) machine.
	virtual	UInt8 numberOfCores( );
	virtual void adjustControls( void );
// protected Overrides:
	virtual bool cacheMetaState( const OSDictionary* metaState );
	virtual ControlValue calculateNewTargetNonConst( void );
	virtual void sendNewTarget( ControlValue newTarget );
	virtual	SensorValue calculateIntegralTerm( void ) const;
	virtual	SensorValue calculateDerivativeTerm( void ) const;
	virtual	SensorValue calculateTargetTemp( void );
	virtual SensorValue temperatureAverageOnLastNSamples( int numberOfSamples );
public:
	bool init( void );
	void free( void );

/*! @function groupVirtualControlForID
	@abstract returns the virtual controls for the given key. */
	static IOGroupVirtualControl *groupVirtualControlForID(OSString *groupID);
	
/*! @function initPlatformCtrlLoop
	@abstract Initialize a platform control loop with a dictionary from an IOPlatformThermalProfile and a pointer to the platform plugin's envInfo environmental info dictionary.  The control loop object will retain the envInfo dict. */
	virtual IOReturn initPlatformCtrlLoop(const OSDictionary *dict);

/*!
	@function updateMetaState
	@abstract Causes the control loop to scan the environmental conditions and update its meta state. */
	virtual bool updateMetaState( void );

/*!
	@function deadlinePassed
	@abstract This is called when the ctrl loop's deadline has passed.  At the very least it should clear the deadline, or it should reset it to the next desired timer interval. */
	virtual void deadlinePassed( void );

/*!
	@function sensorRegistered
	@abstract called by IOPlatformSensor::registerDriver when a sensor driver registers
	@param aSensor an IOPlatformSensor reference for the sensor that registered. */
	virtual void sensorRegistered( IOPlatformSensor * aSensor );

/*!
	@function controlRegistered
	@abstract called by IOPlatformControl::registerDriver when a control driver registers
	@param aControl an IOPlatformControl reference for the control that registered. */
	virtual void controlRegistered( IOPlatformControl * aControl );
};


#endif // _POWERMAC11_2_CPUS_CTRLLOOP_H