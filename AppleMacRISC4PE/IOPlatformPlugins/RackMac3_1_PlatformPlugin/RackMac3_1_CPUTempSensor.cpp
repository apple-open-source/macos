/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: RackMac3_1_CPUTempSensor.cpp,v 1.5 2004/03/18 02:18:52 eem Exp $
 */


#include "RackMac3_1_PlatformPlugin.h"
#include "RackMac3_1_CPUTempSensor.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(RackMac3_1_CPUTempSensor, IOPlatformSensor)

extern RackMac3_1_PlatformPlugin * RM31Plugin;

IOReturn RackMac3_1_CPUTempSensor::initPlatformSensor( const OSDictionary * dict )
{
    OSDictionary *hwmondThresholdsDictionary;
    OSNumber *tempOSNumber;
    OSNumber *cpu_id_num;
    OSData *data;
    UInt8 bytes[2];
    unsigned long long value;
    OSString	*tmp_string;
    OSNumber	*tmp_number;
    
    //SENSOR_DLOG("RackMac3_1_CPUTempSensor::initPlatformSensor - entered\n");

    IOReturn status = super::initPlatformSensor( dict );

    // the cpu-id property tells me which cpu I'm related to
    cpu_id_num = OSDynamicCast(OSNumber, dict->getObject("cpu-id"));
    if (!cpu_id_num)
    {
        SENSOR_DLOG("RackMac3_1_CPUTempSensor::initPlatformSensor no cpu-id\n");
        return(kIOReturnError);
    }

    // read M_diode
    if (!RM31Plugin->readProcROM(cpu_id_num->unsigned32BitValue(), 0x44, 2, bytes))
    {
        SENSOR_DLOG("RackMac3_1_CPUTempSensor::initPlatformSensor failed to read M_diode!\n");
        return(kIOReturnError);
    }

    if ((data = OSData::withBytes( (void *) bytes, 2 )) == NULL)
    {
        SENSOR_DLOG("RackMac3_1_CPUTempSensor::initPlatformSensor failed to create data container!\n");
        return(kIOReturnError);
    }

    infoDict->setObject("m-diode", data);
    scalingFactor = (UInt32) *((UInt16 *)data->getBytesNoCopy());
    data->release();

    // read B_diode, and shift it up to 14.18 to avoid doing it later
    if (!RM31Plugin->readProcROM(cpu_id_num->unsigned32BitValue(), 0x46, 2, bytes))
    {
        SENSOR_DLOG("RackMac3_1_CPUTempSensor::initPlatformSensor failed to read B_diode!\n");
        return(kIOReturnError);
    }

    if ((data = OSData::withBytes( (void *) bytes, 2 )) == NULL)
    {
        SENSOR_DLOG("RackMac3_1_CPUTempSensor::initPlatformSensor failed to create data container!!\n");
        return(kIOReturnError);
    }

    infoDict->setObject("b-diode", data);
    constant = (SInt32) *((SInt16 *)data->getBytesNoCopy());
    constant <<= 12; // shift to 14.18
    data->release();

    // create hwmondThreshold dictionary
    hwmondThresholdsDictionary = OSDictionary::withCapacity(5);

    // read target temp
    if (RM31Plugin->readProcROM(cpu_id_num->unsigned32BitValue(), 0x28, 1, bytes))
    {
        value = (unsigned long long)bytes[0] << 16; // shift to 16.16
        tempOSNumber = OSNumber::withNumber( value , 32);
        hwmondThresholdsDictionary->setObject("medianValue", tempOSNumber);
        tempOSNumber->release();
    }

    // read max temp
    if (RM31Plugin->readProcROM(cpu_id_num->unsigned32BitValue(), 0x29, 1, bytes))
    {
        value = bytes[0] << 16; // shift to 16.16
        tempOSNumber = OSNumber::withNumber( value , 32);
        hwmondThresholdsDictionary->setObject("maxWarningThreshold", tempOSNumber);
        tempOSNumber->release();

        value += (4<<16); // add 4 degrees to max temp for failure
        tempOSNumber = OSNumber::withNumber( value , 32);
        hwmondThresholdsDictionary->setObject("maxFailureThreshold", tempOSNumber);
        tempOSNumber->release();
    }

    // no min thresholds, the colder the better!
    bytes[0] = 0;
    tempOSNumber = OSNumber::withNumber( (unsigned long long) bytes[0] , 32);
    hwmondThresholdsDictionary->setObject("minFailureThreshold", tempOSNumber);
    hwmondThresholdsDictionary->setObject("minWarningThreshold", tempOSNumber);
    tempOSNumber->release();

    infoDict->setObject("hwmondThresholds", hwmondThresholdsDictionary);
    hwmondThresholdsDictionary->release();

    tmp_string = OSDynamicCast(OSString, dict->getObject(kRM31GroupTag));
    if (tmp_string)
        infoDict->setObject(kRM31GroupTag, tmp_string);
#if SENSOR_DEBUG
    else
        SENSOR_DLOG("RackMac3_1_MasterSensor::initPlatformSensor no group tag for sensor id = 0x%x!!\n", getSensorID()->unsigned16BitValue());
#endif

    tmp_number = OSDynamicCast(OSNumber, dict->getObject(kRM31SortNumber));
    if (tmp_number)
        infoDict->setObject(kRM31SortNumber, tmp_number);
#if SENSOR_DEBUG
    else
        SENSOR_DLOG("RackMac3_1_MasterSensor::initPlatformSensor no sort number for sensor id = 0x%x!!\n", getSensorID()->unsigned16BitValue());
#endif

    return(status);
}

SensorValue RackMac3_1_CPUTempSensor::applyCurrentValueTransform( SensorValue hwReading ) const
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

SensorValue RackMac3_1_CPUTempSensor::applyCurrentValueInverseTransform( SensorValue pluginReading ) const
{
	UInt32 ubuf32;
	SensorValue hwReading;

	ubuf32 = (UInt32)((pluginReading.sensValue << 2) - constant);
	hwReading.sensValue = (SInt32)(ubuf32 / scalingFactor);

	return hwReading;
}

*/
