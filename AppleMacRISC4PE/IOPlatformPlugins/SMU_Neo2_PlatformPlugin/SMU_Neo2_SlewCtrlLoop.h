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
 *  File: $Id: SMU_Neo2_SlewCtrlLoop.h,v 1.7 2004/07/08 22:23:32 eem Exp $
 *
 */


#ifndef _SMU_NEO2_SLEWCTRLLOOP_H
#define _SMU_NEO2_SLEWCTRLLOOP_H


#include "IOPlatformSlewClockControl.h"
#include "IOPlatformCtrlLoop.h"

// Internal operating point record
typedef struct
{
	UInt32			freq;
	SensorValue		Tmax;
	UInt16			Vcore[3];
} fvt_operating_point_t;

class SMU_Neo2_SlewCtrlLoop : public IOPlatformCtrlLoop
	{
	OSDeclareDefaultStructors( SMU_Neo2_SlewCtrlLoop )

protected:

	IOPlatformControl*							_slewControl;
	IOPlatformSensor*							_cpu0TempSensor;

	IOService*									_appleSMU;

	unsigned int								_stepIndex;
	unsigned int								_stepIndexLimit;

	// flags to tell us if all the prerequisites have been met in order to perform
	// each type of operation
	bool										_canStepProcessor;
	bool										_canSlewProcessor;

	// A flag to tell if an unrecoverable error occurred. This will not prevent
	// the thermal failsafes from running, but will inhibit any attempts to slew
	// or step or change voltage. This flag is cleared on wake from sleep because
	// the system is in a known state on wake from sleep.
	bool										_fatalErrorOccurred;

	// Data from the F/V/T tables is stored in instance variables
	unsigned int								_numSlewPoints;
	fvt_operating_point_t*						_slewPoints;
	SensorValue									_tmax;

	// this will be used to count to 30 sec at max cooling
	AbsoluteTime								_criticalDeadline;


	/*
	 *	In case there's no other control loop polling the temperature sensor, use a
	 *	2 second deadline timer to ensure that we're responding to changes in CPU
	 *	temperature.
	 *
	 *	_deadlineInterval is set to a 2 second interval
	 *
	 *	_deadlineActive is usually set to false, but is set to true if/when the
	 *	deadline trips, i.e. while the slew control loop is acting due to this
	 *	internal deadline tripping.
	 *
	 */
	AbsoluteTime								_deadlineInterval;
	bool										_deadlineActive;

	virtual		bool			init( void );
	virtual		void			free( void );

				bool			setProcessorSlewIndex( unsigned int slewIndex );
				bool			setProcessorStepIndex( unsigned int stepIndex );
				bool			setProcessorVoltageIndex( unsigned int slewIndex, unsigned int stepIndex );

				bool			fetchFVTTable( void );
				bool			setupSlewing( void );

				bool			setInitialState( void );
				bool			adjustStepVoltControls( unsigned int dpsTarget, bool currentTempValid, SensorValue currentTemp );
				bool			adjustSlewVoltControls( unsigned int dpsTarget, bool currentTempValid, SensorValue currentTemp );
				bool			adjustSlewStepVoltControls( unsigned int dpsTarget, bool currentTempValid, SensorValue currentTemp );
				void			checkThermalFailsafes( bool currentTempValid, SensorValue currentTemp );

public:

	virtual		IOReturn		initPlatformCtrlLoop(const OSDictionary *dict);

	virtual		bool			updateMetaState( void );
	virtual		void			adjustControls( void );

	virtual		void			deadlinePassed( void );

	virtual		void			sensorRegistered( IOPlatformSensor * aSensor );
	virtual		void			controlRegistered( IOPlatformControl * aControl );

	virtual		void			sensorCurrentValueWasSet( IOPlatformSensor * aSensor, SensorValue newValue );

	virtual		void			willSleep( void );
	virtual		void			didWake( void );
	};


#endif	// _SMU_NEO2_SLEWCTRLLOOP_H
