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
//		$Log: PowerMac7_2_CPUPowerSensor.cpp,v $
//		Revision 1.5  2003/06/07 01:30:58  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.4.2.7  2003/06/06 08:17:58  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.4.2.6  2003/06/04 10:21:12  eem
//		Supports forced PID meta states.
//		
//		Revision 1.4.2.5  2003/06/01 14:52:55  eem
//		Most of the PID algorithm is implemented.
//		
//		Revision 1.4.2.4  2003/05/29 03:51:36  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.4.2.3  2003/05/26 10:19:03  eem
//		Fixed OSNumber leaks.
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
//		Revision 1.3.2.3  2003/05/17 11:08:25  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
//		
//		Revision 1.3.2.2  2003/05/16 07:08:48  eem
//		Table-lookup active fan control working with this checkin.
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
//		Revision 1.1.2.3  2003/05/10 06:32:35  eem
//		Sensor changes, should be ready to merge to trunk as 1.0.1d12.
//		
//		Revision 1.1.2.2  2003/05/03 18:01:29  eem
//		*** empty log message ***
//		
//		Revision 1.1.2.1  2003/05/03 01:11:40  eem
//		*** empty log message ***
//		
//		
//

#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "PowerMac7_2_CPUPowerSensor.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(PowerMac7_2_CPUPowerSensor, IOPlatformSensor)

IOReturn PowerMac7_2_CPUPowerSensor::registerDriver( IOService * driver, const OSDictionary * dict, bool notify )
{
	// this is a fake (logical) sensor, there is no IOHWSensor instance
	// associated with it so no driver should ever register
	return(kIOReturnError);
}

IOReturn PowerMac7_2_CPUPowerSensor::initPlatformSensor( const OSDictionary * dict )
{
	OSString * type;
	const OSNumber *pollingPeriod;
	OSArray * inputs;

	IOReturn status = super::initPlatformSensor( dict );

	// look for the I_V_inputs array which gives us the IDs of the current
	// and voltage sensors fomr which we will derive the power reading
	inputs = OSDynamicCast(OSArray, dict->getObject("I_V_inputs"));
	if (!inputs)
	{
		SENSOR_DLOG("PowerMac7_2_CPUPowerSensor::initPlatformSensor no I_V_inputs!!\n");
		return(kIOReturnError);
	}

	sourceSensors[0] = platformPlugin->lookupSensorByID(OSDynamicCast(OSNumber, inputs->getObject(0)));
	sourceSensors[1] = platformPlugin->lookupSensorByID(OSDynamicCast(OSNumber, inputs->getObject(1)));

	if (!sourceSensors[0] || !sourceSensors[1])
	{
		SENSOR_DLOG("PowerMac7_2_CPUPowerSensor::initPlatformSensor bad inputs!!\n");
		return(kIOReturnError);
	}

	// this routine needs to take care of a lot of the stuff that is normally
	// done by registerDriver(), since registerDriver() will never be called
	// on this instance.

	// grab some properties out of the IOHWSensor node
	infoDict->setObject( kIOPPluginVersionKey, dict->getObject( kIOPPluginVersionKey ));
	infoDict->setObject( kIOPPluginLocationKey, dict->getObject( kIOPPluginLocationKey ));
	infoDict->setObject( kIOPPluginZoneKey, dict->getObject( kIOPPluginZoneKey ));

	// if there's no type override, get the type from the sensor driver
	if ((type = getSensorType()) == NULL)
	{
		infoDict->setObject( kIOPPluginTypeKey, dict->getObject( kIOPPluginTypeKey ));
	}

	// initialize the polling period
	if ((pollingPeriod = getPollingPeriod()) == NULL)
	{
		// nothing in infoDict, default to no polling
		pollingPeriod = OSNumber::withNumber( (unsigned long long) kIOPPluginNoPolling, 32 );
		setPollingPeriod(pollingPeriod);
		pollingPeriod->release();
	}

	// flag the successful registration
	infoDict->setObject(kIOPPluginRegisteredKey, kOSBooleanTrue );

	// create an empty current-value property
	setCurrentValue( gIOPPluginZero );

	return( status );
}

OSNumber *PowerMac7_2_CPUPowerSensor::fetchCurrentValue( void )
{
	UInt64 buf64;
	const OSNumber *num1, *num2;
	UInt32 val1, val2, result;

	num1 = sourceSensors[0]->getCurrentValue();
	num2 = sourceSensors[1]->getCurrentValue();

	if (!num1 || !num2) return(NULL);

	// accumulate into a 64 bit buffer
	val1 = num1->unsigned32BitValue();
	val2 = num2->unsigned32BitValue();
	buf64 = ((UInt64)val1) * ((UInt64)val2);
	result = (UInt32)((buf64 >> 16) & 0xFFFFFFFF);

	//SENSOR_DLOG("PowerMac7_2_CPUPowerSensor::fetchCurrentValue %08lX * %08lX = %08lX\n", val1, val2, result);
	
	// shift right by 16 to convert back to 16.16 fixed point
	return(OSNumber::withNumber( result, 32 ));
}

// this sends the polling period to the sensor
bool PowerMac7_2_CPUPowerSensor::sendPollingPeriod( OSNumber * period )
{
	// there's no driver, this is a fake (logical) sensor
	return(true);
}
