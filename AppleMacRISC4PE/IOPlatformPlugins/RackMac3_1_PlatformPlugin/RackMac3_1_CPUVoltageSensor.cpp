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
 *
 */


#include "RackMac3_1_PlatformPlugin.h"
#include "RackMac3_1_CPUVoltageSensor.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(RackMac3_1_CPUVoltageSensor, IOPlatformSensor)

extern RackMac3_1_PlatformPlugin * RM31Plugin;

IOReturn RackMac3_1_CPUVoltageSensor::initPlatformSensor( const OSDictionary * dict )
{
    OSData *tmp_osdata;
    OSDictionary *hwmondThresholdsDictionary;
    OSNumber *tempOSNumber;
    OSNumber *cpu_id_num;
    UInt8 bytes[2];
	UInt16 *operatingVoltage;
    unsigned long long value;
    OSString	*tmp_string;
    OSNumber	*tmp_number;

    IOReturn status = super::initPlatformSensor( dict );

    tmp_osdata = OSDynamicCast(OSData, dict->getObject(kRM31ScalingFactorKey));

    if (tmp_osdata)
    {
        scalingFactor = *((UInt32 *)tmp_osdata->getBytesNoCopy());
        SENSOR_DLOG("RackMac3_1_CPUVoltageSensor::initPlatformSensor id=0x%X scale=0x%08lX\n",
                getSensorID()->unsigned16BitValue(), scalingFactor);
    }
    else
    {
        SENSOR_DLOG("RackMac3_1_CPUVoltageSensor::initPlatformSensor no scaling factor!!\n");
        status = kIOReturnError;
    }

    // the cpu-id property tells me which cpu I'm related to
    cpu_id_num = OSDynamicCast(OSNumber, dict->getObject("cpu-id"));
    if (!cpu_id_num)
    {
        SENSOR_DLOG("RackMac3_1_CPUVoltageSensor::initPlatformSensor no cpu-id\n");
        return(kIOReturnError);
    }

    // create hwmondThreshold dictionary
    hwmondThresholdsDictionary = OSDictionary::withCapacity(5);

    // read CPU voltage
    if (RM31Plugin->readProcROM(cpu_id_num->unsigned32BitValue(), 0x38, 2, bytes))
    {    
        operatingVoltage = (UInt16 *)bytes;
		value = *operatingVoltage >> 7; // value is in 1.15, shift 7 bits to make 8.8
        value *= 282; // 1.1 in 8.8, result = 16.16
        tempOSNumber = OSNumber::withNumber( value , 32);
        hwmondThresholdsDictionary->setObject("maxFailureThreshold", tempOSNumber);
        tempOSNumber->release();

		value = *operatingVoltage >> 7; // value is in 1.15, shift 7 bits to make 8.8
        value *= 269; // 1.05 in 8.8, result = 16.16
        tempOSNumber = OSNumber::withNumber( value , 32);
        hwmondThresholdsDictionary->setObject("maxWarningThreshold", tempOSNumber);
        tempOSNumber->release();

		value = *operatingVoltage << 1; // value is in 1.15, shift 1 bit to make 0.16
        tempOSNumber = OSNumber::withNumber( value , 32);
		hwmondThresholdsDictionary->setObject("medianValue", tempOSNumber);
		tempOSNumber->release();

		value = *operatingVoltage >> 7; // value is in 1.15, shift 7 bits to make 8.8
        value *= 243; // 0.95 in 8.8, result = 16.16
        tempOSNumber = OSNumber::withNumber( value , 32);
		hwmondThresholdsDictionary->setObject("minWarningThreshold", tempOSNumber);
		tempOSNumber->release();

		value = *operatingVoltage >> 7; // value is in 1.15, shift 7 bits to make 8.8
        value *= 230; // 0.90 in 8.8, result = 16.16
        tempOSNumber = OSNumber::withNumber( value , 32);
		hwmondThresholdsDictionary->setObject("minFailureThreshold", tempOSNumber);
		tempOSNumber->release();

		infoDict->setObject("hwmondThresholds", hwmondThresholdsDictionary);
    }

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

SensorValue RackMac3_1_CPUVoltageSensor::applyCurrentValueTransform( SensorValue hwReading ) const
{
	SensorValue pluginReading;

	pluginReading.sensValue = (SInt32)( scalingFactor * (UInt32)hwReading.sensValue );

	return pluginReading;
}
