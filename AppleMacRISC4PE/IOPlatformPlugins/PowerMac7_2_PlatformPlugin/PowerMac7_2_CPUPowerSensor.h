/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: PowerMac7_2_CPUPowerSensor.h,v $
//		Revision 1.5  2003/06/07 01:30:58  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.4.2.2  2003/05/23 05:44:42  eem
//		Cleanup, ctrlloops not get notification for sensor and control registration.
//		
//		Revision 1.4.2.1  2003/05/22 01:31:05  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.4  2003/05/21 21:58:55  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.3.2.1  2003/05/14 22:07:55  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//		Revision 1.3  2003/05/13 02:13:52  eem
//		PowerMac7_2 Dynamic Power Step support.
//		
//		Revision 1.2.2.1  2003/05/12 11:21:12  eem
//		Support for slewing.
//		
//		Revision 1.2  2003/05/10 06:50:36  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.1.2.1  2003/05/03 01:11:40  eem
//		*** empty log message ***
//		
//		
//

#ifndef _POWERMAC7_2_CPUPOWERSENSOR_H
#define _POWERMAC7_2_CPUPOWERSENSOR_H

#include "IOPlatformSensor.h"

class PowerMac7_2_CPUPowerSensor : public IOPlatformSensor
{

	OSDeclareDefaultStructors(PowerMac7_2_CPUPowerSensor)

protected:

	IOPlatformSensor *sourceSensors[2];

	/*!	@function registerDriver Used to associated an IOService with this sensor. */
	virtual IOReturn	registerDriver(IOService *driver, const OSDictionary * dict, bool notify = true);

	// initialize a sensor from it's SensorArray dict in the IOPlatformThermalProfile
	virtual IOReturn		initPlatformSensor( const OSDictionary * dict );

	// this sends a force-update message to the sensor and then reads its current-value property
	virtual OSNumber *		fetchCurrentValue( void );	// blocks for I/O

	// this sends the polling period to the sensor
	virtual bool			sendPollingPeriod( OSNumber * period );

};

#endif // _POWERMAC7_2_CPUPOWERSENSOR_H