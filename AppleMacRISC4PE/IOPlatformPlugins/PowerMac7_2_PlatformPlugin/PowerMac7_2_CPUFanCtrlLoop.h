/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: PowerMac7_2_CPUFanCtrlLoop.h,v $
//		Revision 1.9  2003/07/24 21:47:18  eem
//		[3338565] Q37 Final Fan data
//		
//		Revision 1.8  2003/07/17 06:57:39  eem
//		3329222 and other sleep stability issues fixed.
//		
//		Revision 1.7  2003/07/16 02:02:10  eem
//		3288772, 3321372, 3328661
//		
//		Revision 1.6  2003/07/08 04:32:51  eem
//		3288891, 3279902, 3291553, 3154014
//		
//		Revision 1.5  2003/06/25 02:16:25  eem
//		Merged 101.0.21 to TOT, fixed PM72 lproj, included new fan settings, bumped
//		version to 101.0.22.
//		
//		Revision 1.4.4.5  2003/06/21 01:42:08  eem
//		Final Fan Tweaks.
//		
//		Revision 1.4.4.4  2003/06/20 09:07:37  eem
//		Added rising/falling slew limiters, integral clipping, etc.
//		
//		Revision 1.4.4.3  2003/06/20 01:40:00  eem
//		Although commented out in this submision, there is support here to nap
//		the processors if the fans are at min, with the intent of keeping the
//		heat sinks up to temperature.
//		
//		Revision 1.4.4.2  2003/06/19 10:24:20  eem
//		Pulled common PID code into IOPlatformPIDCtrlLoop and subclassed it with
//		PowerMac7_2_CPUFanCtrlLoop and PowerMac7_2_PIDCtrlLoop.  Added history
//		length to meta-state.  No longer adjust T_err when the setpoint changes.
//		Don't crank the CPU fans for overtemp, just slew slow.
//		
//		Revision 1.4.4.1  2003/06/18 20:40:12  eem
//		Added variable sample history length, removed cpu fan response to
//		internal overtemp, made proportional gain only apply to proportional
//		term.
//		
//		Revision 1.4  2003/06/07 01:30:58  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.3.2.6  2003/06/06 08:17:58  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.3.2.5  2003/06/01 14:52:55  eem
//		Most of the PID algorithm is implemented.
//		
//		Revision 1.3.2.4  2003/05/31 08:11:38  eem
//		Initial pass at integrating deadline-based timer callbacks for PID loops.
//		
//		Revision 1.3.2.3  2003/05/29 03:51:36  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.3.2.2  2003/05/23 06:36:59  eem
//		More registration notification stuff.
//		
//		Revision 1.3.2.1  2003/05/22 01:31:05  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.3  2003/05/21 21:58:55  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.2.4.2  2003/05/17 11:08:25  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
//		
//		Revision 1.2.4.1  2003/05/17 02:55:05  eem
//		Intermediate commit with cpu fan loop states/outputs included.  This
//		probably won't compile right now.
//		
//		Revision 1.2  2003/05/10 06:50:36  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.1.2.1  2003/05/01 09:28:47  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//

#ifndef _POWERMAC7_2_CPUFANCTRLLOOP_H
#define _POWERMAC7_2_CPUFANCTRLLOOP_H

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

/*!	@class PowerMac7_2_CPUFanCtrlLoop
	@abstract This class implements a PID-based fan control loop for use on PowerMac7,2 machines.  This control loop is designed to work for both uni- and dual-processor machines.  There are actually two (mostly) independent control loops on dual-processor machines, but control is implemented in one control loop object to ease implementation of tach-lock. */

class PowerMac7_2_CPUFanCtrlLoop : public IOPlatformPIDCtrlLoop
{

	OSDeclareDefaultStructors(PowerMac7_2_CPUFanCtrlLoop)

protected:

	IOPlatformControl * secOutputControl;
	IOPlatformSensor * currentSensor, * voltageSensor, * powerSensor;

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

	// Max CPU temperature at diode
	SensorValue inputMax;

	// PowerMaxAdj = PowerMaxROM - AdjStaticFactor
	SensorValue powerMaxAdj;

	// If we've been at max cooling for 30 seconds and are still making no progress,
	// we have to put the machine to sleep.  This counter is used to determine how
	// many seconds we've been at max cooling.
	unsigned int secondsAtMaxCooling;

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
	virtual void deadlinePassed( void );
	bool acquireSample( void );		// gets a sample (using clock_get_uptime() and getAggregateSensorValue()) and stores it at 
	virtual SensorValue calculateDerivativeTerm( void ) const;
	virtual const OSNumber *calculateNewTarget( void ) const;
	virtual void sendNewTarget( const OSNumber * newTarget );
	//virtual void deadlinePassed( void );
	virtual void sensorRegistered( IOPlatformSensor * aSensor );
	virtual void controlRegistered( IOPlatformControl * aControl );
};

#endif	// _POWERMAC7_2_CPUFANCTRLLOOP_H
