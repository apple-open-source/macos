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
//		$Log: IOPlatformPIDCtrlLoop.h,v $
//		Revision 1.5  2003/07/17 06:57:36  eem
//		3329222 and other sleep stability issues fixed.
//		
//		Revision 1.4  2003/07/16 02:02:09  eem
//		3288772, 3321372, 3328661
//		
//		Revision 1.3  2003/06/25 02:16:24  eem
//		Merged 101.0.21 to TOT, fixed PM72 lproj, included new fan settings, bumped
//		version to 101.0.22.
//		
//		Revision 1.2.4.3  2003/06/21 01:42:07  eem
//		Final Fan Tweaks.
//		
//		Revision 1.2.4.2  2003/06/20 09:07:33  eem
//		Added rising/falling slew limiters, integral clipping, etc.
//		
//		Revision 1.2.4.1  2003/06/19 10:24:16  eem
//		Pulled common PID code into IOPlatformPIDCtrlLoop and subclassed it with
//		PowerMac7_2_CPUFanCtrlLoop and PowerMac7_2_PIDCtrlLoop.  Added history
//		length to meta-state.  No longer adjust T_err when the setpoint changes.
//		Don't crank the CPU fans for overtemp, just slew slow.
//		
//
//

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
	virtual const OSNumber *calculateNewTarget( void ) const;
	virtual SensorValue calculateDerivativeTerm( void ) const;
	virtual SensorValue calculateIntegralTerm( void ) const;
	//SInt32 secondsElapsed( samplePoint * moreRecent, samplePoint * lessRecent );

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual void free( void );

	virtual const OSNumber * getAggregateSensorValue( void );

	virtual bool cacheMetaState( const OSDictionary * metaState );

public:

	// By setting a deadline and handling the deadlinePassed() callback, we get a periodic timer
	// callback that is funnelled through the platform plugin's command gate.
	//virtual void deadlinePassed( void );

	virtual IOReturn initPlatformCtrlLoop(const OSDictionary *dict);
	virtual bool updateMetaState( void );
	virtual void adjustControls( void );
	virtual void deadlinePassed( void );
	virtual void sendNewTarget( const OSNumber * newTarget );

	virtual void sensorRegistered( IOPlatformSensor * aSensor );
	virtual void controlRegistered( IOPlatformControl * aControl );

	virtual void didWake( void );

};

#endif	// _IOPLATFORMPIDCTRLLOOP_H
