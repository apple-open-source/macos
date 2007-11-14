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


#ifndef _IOPLATFORMPIDCTRLLOOP_H
#define _IOPLATFORMPIDCTRLLOOP_H

#include "IOPlatformCtrlLoop.h"

// by default store 4 samples of history
#define kIOPPIDCtrlLoopDefaultHistLen	5

// keys for PID parameters
#define kIOPPIDCtrlLoopProportionalGainKey	"G_p"
#define kIOPPIDCtrlLoopDerivativeGainKey	"G_d"
#define kIOPPIDCtrlLoopResetGainKey			"G_r"
#define kIOPPIDCtrlLoopIntervalKey			"interval"
#define kIOPPIDCtrlLoopInputTargetKey		"input-target"
#define kIOPPIDCtrlLoopInputMaxKey			"input-max"
#define kIOPPIDCtrlLoopOutputMaxKey			"output-max"
#define kIOPPIDCtrlLoopOutputMinKey			"output-min"
#define kIOPPIDCtrlLoopOutputOverrideKey	"output-override"
#define kIOPPIDCtrlLoopHistoryLenKey		"history-length"

class IOPlatformPIDCtrlLoop : public IOPlatformCtrlLoop
{

	OSDeclareDefaultStructors(IOPlatformPIDCtrlLoop)

protected:

typedef struct _samplePoint
{
	SensorValue		sample;
	SensorValue		error;
} samplePoint;

	IOPlatformSensor * inputSensor;
	IOPlatformControl * outputControl;

	// data structures for storing sample history
	unsigned int historyLen;
	unsigned int latestSample;
	samplePoint *historyArray;

	// helpers for accessing sample history.  latest sample is index 0, previous is index 1, and so on...
	bool acquireSample( void );		// gets a sample (using clock_get_uptime() and getAggregateSensorValue()) and stores it at index 0
	samplePoint * sampleAtIndex( unsigned int index ) const;	// retrieve the sample for a given history point

	// cached PID variables - these have to be extracted from the meta
	// state every time it changes

	// coefficients
	SInt32 G_p, G_d, G_r;

	// iteration interval
	AbsoluteTime interval;
	UInt32 intervalSec;

	// controlled-variable setpoint
	SensorValue inputTarget;

	// bounds on output
	UInt32 outputMin, outputMax;

	// a meta-state can specify an override constant output
	bool overrideActive;
	const OSNumber * outputOverride;

	// only log PID values when we're in a timer callback
	bool timerCallbackActive;

	// PID algorithm helpers
	virtual ControlValue calculateNewTarget( void ) const;
	virtual SensorValue calculateDerivativeTerm( void ) const;
	virtual SensorValue calculateIntegralTerm( void ) const;
	//SInt32 secondsElapsed( samplePoint * moreRecent, samplePoint * lessRecent );

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual void free( void );

	virtual SensorValue getAggregateSensorValue( void );

	virtual bool cacheMetaState( const OSDictionary * metaState );

public:

	// By setting a deadline and handling the deadlinePassed() callback, we get a periodic timer
	// callback that is funnelled through the platform plugin's command gate.
	//virtual void deadlinePassed( void );

	virtual IOReturn initPlatformCtrlLoop(const OSDictionary *dict);
	virtual bool updateMetaState( void );
	virtual void adjustControls( void );
	virtual void deadlinePassed( void );
	virtual void sendNewTarget( ControlValue newTarget );

	virtual void sensorRegistered( IOPlatformSensor * aSensor );
	virtual void controlRegistered( IOPlatformControl * aControl );

	virtual void didWake( void );

};

#endif	// _IOPLATFORMPIDCTRLLOOP_H
