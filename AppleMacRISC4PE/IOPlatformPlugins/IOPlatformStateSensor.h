/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: IOPlatformStateSensor.h,v $
//		Revision 1.4  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.3.2.2  2003/05/23 05:44:40  eem
//		Cleanup, ctrlloops not get notification for sensor and control registration.
//		
//		Revision 1.3.2.1  2003/05/22 01:31:04  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.3  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.2.4.3  2003/05/17 12:55:37  eem
//		Active fan control works on RPM channels!!!!!!
//		
//		Revision 1.2.4.2  2003/05/17 11:08:22  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
//		
//		Revision 1.2.4.1  2003/05/14 22:07:49  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//		Revision 1.2  2003/05/10 06:50:33  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.1.2.1  2003/05/01 09:28:41  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//		

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

	// analogous to applyValueTransform, when we send values back to the sensor we have
	// to put them in the sensor's own representation.  This will convert back to the
	// hw representation.
	virtual const OSNumber *	applyValueTransform( const OSNumber * hwReading ) const;
	virtual const OSNumber *	applyHWTransform( const OSNumber * value ) const;

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