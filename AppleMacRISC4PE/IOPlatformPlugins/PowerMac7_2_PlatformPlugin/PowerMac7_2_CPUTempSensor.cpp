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
//		$Log: PowerMac7_2_CPUTempSensor.cpp,v $
//		Revision 1.4  2003/06/07 01:30:58  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.3.2.3  2003/06/06 08:17:58  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.3.2.2  2003/06/01 14:52:55  eem
//		Most of the PID algorithm is implemented.
//		
//		Revision 1.3.2.1  2003/05/22 01:31:05  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.3  2003/05/21 21:58:55  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.2.4.4  2003/05/17 12:55:41  eem
//		Active fan control works on RPM channels!!!!!!
//		
//		Revision 1.2.4.3  2003/05/17 11:08:25  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
//		
//		Revision 1.2.4.2  2003/05/16 07:08:48  eem
//		Table-lookup active fan control working with this checkin.
//		
//		Revision 1.2.4.1  2003/05/14 22:07:55  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//		Revision 1.2  2003/05/10 06:50:36  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.1.2.2  2003/05/10 06:32:35  eem
//		Sensor changes, should be ready to merge to trunk as 1.0.1d12.
//		
//		Revision 1.1.2.1  2003/05/03 01:11:40  eem
//		*** empty log message ***
//		
//		
//

#include "PowerMac7_2_PlatformPlugin.h"
#include "PowerMac7_2_CPUTempSensor.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(PowerMac7_2_CPUTempSensor, IOPlatformSensor)

extern PowerMac7_2_PlatformPlugin * PM72Plugin;

IOReturn PowerMac7_2_CPUTempSensor::initPlatformSensor( const OSDictionary * dict )
{
	OSNumber *cpu_id_num;
	OSData *data;
	UInt8 bytes[2];

	//SENSOR_DLOG("PowerMac7_2_CPUTempSensor::initPlatformSensor - entered\n");

	IOReturn status = super::initPlatformSensor( dict );

	// the cpu-id property tells me which cpu I'm related to
	cpu_id_num = OSDynamicCast(OSNumber, dict->getObject("cpu-id"));
	if (!cpu_id_num)
	{
		SENSOR_DLOG("PowerMac7_2_CPUTempSensor::initPlatformSensor no cpu-id\n");
		return(kIOReturnError);
	}

	// read M_diode
	if (!PM72Plugin->readProcROM(cpu_id_num->unsigned32BitValue(), 0x44, 2, bytes))
	{
		SENSOR_DLOG("PowerMac7_2_CPUTempSensor::initPlatformSensor failed to read M_diode!\n");
		return(kIOReturnError);
	}

	if ((data = OSData::withBytes( (void *) bytes, 2 )) == NULL)
	{
		SENSOR_DLOG("PowerMac7_2_CPUTempSensor::initPlatformSensor failed to create data container!\n");
		return(kIOReturnError);
	}

	infoDict->setObject("m-diode", data);
	scalingFactor = (UInt32) *((UInt16 *)data->getBytesNoCopy());
	data->release();

	// read B_diode, and shift it up to 14.18 to avoid doing it later
	if (!PM72Plugin->readProcROM(cpu_id_num->unsigned32BitValue(), 0x46, 2, bytes))
	{
		SENSOR_DLOG("PowerMac7_2_CPUTempSensor::initPlatformSensor failed to read B_diode!\n");
		return(kIOReturnError);
	}

	if ((data = OSData::withBytes( (void *) bytes, 2 )) == NULL)
	{
		SENSOR_DLOG("PowerMac7_2_CPUTempSensor::initPlatformSensor failed to create data container!!\n");
		return(kIOReturnError);
	}

	infoDict->setObject("b-diode", data);
	constant = (SInt32) *((SInt16 *)data->getBytesNoCopy());
	constant <<= 12; // shift to 14.18
	data->release();

	return(status);
}

const OSNumber *PowerMac7_2_CPUTempSensor::applyValueTransform( const OSNumber * hwReading ) const
{
	OSNumber *scaled;
	SInt32 sbuf32;

	sbuf32 = ((SInt32)(hwReading->unsigned32BitValue() * scalingFactor)) + constant;
	sbuf32 >>= 2;	// shift back down to 16.16
	scaled = OSNumber::withNumber( sbuf32, 32 );

	//SENSOR_DLOG("PowerMac7_2_CPUTempSensor::applyValueTransform raw = %08lX value = %08lX\n",
	//		hwReading->unsigned32BitValue(), scaled->unsigned32BitValue());

	return scaled;
}

const OSNumber *PowerMac7_2_CPUTempSensor::applyHWTransform( const OSNumber * value ) const
{
	const OSNumber *raw;
	UInt32 ubuf32;

	ubuf32 = (UInt32)(((SInt32)(value->unsigned32BitValue() << 2)) - constant);
	ubuf32 /= scalingFactor;
	raw = OSNumber::withNumber( ubuf32, 32 );

	//SENSOR_DLOG("PowerMac7_2_CPUTempSensor::applyHWTransform value = %08lX raw = %08lX\n",
	//		value->unsigned32BitValue(), raw->unsigned32BitValue());

	return raw;
}
