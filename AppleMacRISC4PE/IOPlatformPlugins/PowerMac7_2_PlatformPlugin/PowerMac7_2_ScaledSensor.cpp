/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: PowerMac7_2_ScaledSensor.cpp,v $
//		Revision 1.4  2003/06/07 01:30:58  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.3.2.3  2003/06/06 08:17:58  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.3.2.2  2003/06/04 10:21:12  eem
//		Supports forced PID meta states.
//		
//		Revision 1.3.2.1  2003/05/22 01:31:05  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.3  2003/05/21 21:58:55  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.2.4.3  2003/05/17 12:55:41  eem
//		Active fan control works on RPM channels!!!!!!
//		
//		Revision 1.2.4.2  2003/05/17 11:08:25  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
//		
//		Revision 1.2.4.1  2003/05/16 07:08:48  eem
//		Table-lookup active fan control working with this checkin.
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

#include "PowerMac7_2_ScaledSensor.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(PowerMac7_2_ScaledSensor, IOPlatformSensor)

IOReturn PowerMac7_2_ScaledSensor::initPlatformSensor( const OSDictionary * dict )
{
	OSData *tmp_osdata;
	
	IOReturn status = super::initPlatformSensor( dict );

	tmp_osdata = OSDynamicCast(OSData, dict->getObject(kPM72ScalingFactorKey));

	if (tmp_osdata)
	{
		scalingFactor = *((UInt32 *)tmp_osdata->getBytesNoCopy());
		SENSOR_DLOG("PowerMac7_2_ScaledSensor::initPlatformSensor id=0x%X scale=0x%08lX\n",
			getSensorID()->unsigned16BitValue(), scalingFactor);
	}
	else
	{
		SENSOR_DLOG("PowerMac7_2_ScaledSensor::initPlatformSensor no scaling factor!!\n");
		status = kIOReturnError;
	}

	return(status);
}

const OSNumber *PowerMac7_2_ScaledSensor::applyValueTransform( const OSNumber * hwReading ) const
{
	const OSNumber *scaled;

	scaled = OSNumber::withNumber( scalingFactor * hwReading->unsigned32BitValue(), 32 );
	//SENSOR_DLOG("PowerMac7_2_ScaledSensor::applyValueTransform raw = %08lX value = %08lX\n",
	//		hwReading->unsigned32BitValue(), scaled->unsigned32BitValue());

	return scaled;
}

/*
const OSNumber *PowerMac7_2_ScaledSensor::applyHWTransform( const OSNumber * value ) const
{
	const OSNumber * raw;

	raw = OSNumber::withNumber( value->unsigned32BitValue() / scalingFactor, 32 );
	SENSOR_DLOG("PowerMac7_2_ScaledSensor::applyHWTransform value = %08lX raw = %08lX\n",
			value->unsigned32BitValue(), raw->unsigned32BitValue());

	return raw;
}
*/