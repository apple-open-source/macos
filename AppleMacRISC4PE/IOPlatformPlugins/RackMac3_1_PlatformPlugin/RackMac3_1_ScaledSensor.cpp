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
 *  File: $Id: RackMac3_1_ScaledSensor.cpp,v 1.4 2004/03/18 02:18:52 eem Exp $
 */


#include "RackMac3_1_ScaledSensor.h"

#define super RackMac3_1_MasterSensor
OSDefineMetaClassAndStructors(RackMac3_1_ScaledSensor, RackMac3_1_MasterSensor)

IOReturn RackMac3_1_ScaledSensor::initPlatformSensor( const OSDictionary * dict )
{
    OSData *tmp_osdata;

    IOReturn status = super::initPlatformSensor( dict );

    tmp_osdata = OSDynamicCast(OSData, dict->getObject(kRM31ScalingFactorKey));

    if (tmp_osdata)
    {
        scalingFactor = *((UInt32 *)tmp_osdata->getBytesNoCopy());
        SENSOR_DLOG("RackMac3_1_ScaledSensor::initPlatformSensor id=0x%X scale=0x%08lX\n",
                getSensorID()->unsigned16BitValue(), scalingFactor);
    }
    else
    {
        SENSOR_DLOG("RackMac3_1_ScaledSensor::initPlatformSensor no scaling factor!!\n");
        status = kIOReturnError;
    }

    return(status);
}

SensorValue RackMac3_1_ScaledSensor::applyCurrentValueTransform( SensorValue hwReading ) const
{
	SensorValue pluginReading;

	pluginReading.sensValue = (SInt32)( scalingFactor * (UInt32)hwReading.sensValue );

	return pluginReading;
}
