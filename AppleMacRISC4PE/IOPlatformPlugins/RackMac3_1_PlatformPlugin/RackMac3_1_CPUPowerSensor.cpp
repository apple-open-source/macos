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

#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "RackMac3_1_CPUPowerSensor.h"
#include "RackMac3_1_PlatformPlugin.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(RackMac3_1_CPUPowerSensor, IOPlatformSensor)

extern RackMac3_1_PlatformPlugin * RM31Plugin;

IOReturn RackMac3_1_CPUPowerSensor::registerDriver( IOService * driver, const OSDictionary * dict, bool notify )
{
    // this is a fake (logical) sensor, there is no IOHWSensor instance
    // associated with it so no driver should ever register
    return(kIOReturnError);
}

IOReturn RackMac3_1_CPUPowerSensor::initPlatformSensor( const OSDictionary * dict )
{
    OSDictionary *hwmondThresholdsDictionary;
    OSNumber *tempOSNumber;
    OSNumber *cpu_id_num;
    OSString * type;
    const OSNumber *pollingPeriod;
    OSArray * inputs;
    UInt8 bytes[2];
    unsigned long long value;
    OSString	*tmp_string;
    OSNumber	*tmp_number;

    IOReturn status = super::initPlatformSensor( dict );

    // the cpu-id property tells me which cpu I'm related to
    cpu_id_num = OSDynamicCast(OSNumber, dict->getObject("cpu-id"));
    if (!cpu_id_num)
    {
        SENSOR_DLOG("RackMac3_1_CPUPowerSensor::initPlatformSensor no cpu-id\n");
        return(kIOReturnError);
    }

    // look for the I_V_inputs array which gives us the IDs of the current
    // and voltage sensors fomr which we will derive the power reading
    inputs = OSDynamicCast(OSArray, dict->getObject("I_V_inputs"));
    if (!inputs)
    {
        SENSOR_DLOG("RackMac3_1_CPUPowerSensor::initPlatformSensor no I_V_inputs!!\n");
        return(kIOReturnError);
    }

    sourceSensors[0] = platformPlugin->lookupSensorByID(OSDynamicCast(OSNumber, inputs->getObject(0)));
    sourceSensors[1] = platformPlugin->lookupSensorByID(OSDynamicCast(OSNumber, inputs->getObject(1)));

    if (!sourceSensors[0] || !sourceSensors[1])
    {
        SENSOR_DLOG("RackMac3_1_CPUPowerSensor::initPlatformSensor bad inputs!!\n");
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
    SensorValue zeroVal;
    zeroVal.sensValue = 0x0;
    setCurrentValue( zeroVal );

    // create hwmondThreshold dictionary
    hwmondThresholdsDictionary = OSDictionary::withCapacity(5);

    // read max power
    if (RM31Plugin->readProcROM(cpu_id_num->unsigned32BitValue(), 0x2A, 1, bytes))
    {
        value = bytes[0];
        value *= 72090; // 110% in 16.16
        tempOSNumber = OSNumber::withNumber( value , 32);
        hwmondThresholdsDictionary->setObject("maxWarningThreshold", tempOSNumber);
        tempOSNumber->release();
    
        value = bytes[0];
        value *= 78643; // 120% in 16.16
        tempOSNumber = OSNumber::withNumber( value , 32);
        hwmondThresholdsDictionary->setObject("maxFailureThreshold", tempOSNumber);
        tempOSNumber->release();
    }
    
    // no min thresholds, the lower the better!
    bytes[0] = 0;
    tempOSNumber = OSNumber::withNumber( (unsigned long long) bytes[0] , 32);
    hwmondThresholdsDictionary->setObject("medianValue", tempOSNumber);
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

    return( status );
}

SensorValue RackMac3_1_CPUPowerSensor::fetchCurrentValue( void )
{
	UInt64 buf64;
	SensorValue val1, val2, result;

	val1 = sourceSensors[0]->getCurrentValue();
	val2 = sourceSensors[1]->getCurrentValue();

	// accumulate into a 64 bit buffer
	buf64 = ((UInt64)val1.sensValue) * ((UInt64)val2.sensValue);

	// shift right by 16 to convert back to 16.16 fixed point
	result.sensValue = (SInt32)((buf64 >> 16) & 0xFFFFFFFF);

	return result;
}

// this sends the polling period to the sensor
bool RackMac3_1_CPUPowerSensor::sendPollingPeriod( OSNumber * period )
{
    // there's no driver, this is a fake (logical) sensor
    return(true);
}
