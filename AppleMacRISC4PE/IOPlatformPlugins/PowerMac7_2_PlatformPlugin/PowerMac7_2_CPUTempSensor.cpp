/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


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

SensorValue PowerMac7_2_CPUTempSensor::applyCurrentValueTransform( SensorValue hwReading ) const
{
	SensorValue pluginReading;

	pluginReading.sensValue = (hwReading.sensValue * scalingFactor) + constant;
	pluginReading.sensValue >>= 2;	// shift back down to 16.16

	return pluginReading;
}

/* XXX the inverse transform is not used, and has not been for a long time...  since PID started
   XXX working.  At that point, this class was changed so that instead of inheriting from
   XXX IOPlatformStateSensor, it inherits from IOPlatformSensor.  This method was never removed.
   XXX I've updated it in line with the change in the value transform functions, but I'm leaving
   XXX the code commented out because it's not used and shouldn't be here.

SensorValue PowerMac7_2_CPUTempSensor::applyCurrentValueInverseTransform( SensorValue pluginReading ) const
{
	UInt32 ubuf32;
	SensorValue hwReading;

	ubuf32 = (UInt32)((pluginReading.sensValue << 2) - constant);
	hwReading.sensValue = (SInt32)(ubuf32 / scalingFactor);

	return hwReading;
}

*/
