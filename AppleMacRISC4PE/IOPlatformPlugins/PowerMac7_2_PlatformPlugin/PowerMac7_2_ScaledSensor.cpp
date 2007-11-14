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

SensorValue PowerMac7_2_ScaledSensor::applyCurrentValueTransform( SensorValue hwReading ) const
{
	SensorValue pluginReading;

	pluginReading.sensValue = (SInt32)( scalingFactor * (UInt32)hwReading.sensValue );

	return pluginReading;
}
