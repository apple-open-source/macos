/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _IOPLATFORMSTATESENSOR_H
#define _IOPLATFORMSTATESENSOR_H

#include "IOPlatformSensor.h"

class IOPlatformStateSensor : public IOPlatformSensor
{
	
    OSDeclareDefaultStructors(IOPlatformStateSensor)	

protected:
	// overrides from OSObject superclass
	virtual bool init( void );
	virtual void free( void );

	// when we send values back to the sensor (as high and low thresholds) we have
	// to put them in the sensor's own representation.  This will convert back to the
	// hw representation.
	virtual SensorValue applyCurrentValueInverseTransform( SensorValue ) const;

public:
	// need to grab the threshold dictionary out of the thermal profile
	virtual IOReturn		initPlatformSensor( const OSDictionary * dict );

	// have to do a little extra initialization -- calculate initial state and send down
	// thresholds to sensor
	virtual IOReturn		registerDriver( IOService * driver, const OSDictionary * dict, bool notify = true );

	// Accessors for current sensor state
	virtual UInt32					getNumSensorStates( void );
	virtual const OSNumber *		getSensorState( void );
	virtual UInt32					getSensorStateUInt32( void );
	virtual void					setSensorState( const OSNumber * state );

	// get the thresholds for a state
	virtual const OSNumber *		getLowThresholdForState( const OSDictionary * state );
	virtual const OSNumber *		getHighThresholdForState( const OSDictionary * state );
	virtual const OSDictionary *	getThresholdsForState( UInt32 state );

	// This is called when a threshold hit message is received from a sensor.  It causes the
	// relevant control loops to be notified as well.
	virtual IOReturn	 			thresholdHit( bool low, OSDictionary * msgDict );

	// Set sensors threshold for a particular state - useful for overriding default behavior
	//virtual void setThresholdForState (UInt32 state, ThresholdInfo *thresh);

	// Send thresholds for current state to sensor
	virtual IOReturn		sendThresholdsToSensor( void );
};

#endif // _IOPLATFORMSTATESENSOR_H
