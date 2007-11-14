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


#include "PowerMac7_2_PCIPowerSensor.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(PowerMac7_2_PCIPowerSensor, IOPlatformSensor)

SensorValue PowerMac7_2_PCIPowerSensor::applyCurrentValueTransform( SensorValue hwReading ) const
{
	SensorValue pluginReading;

	pluginReading.sensValue = (((UInt32)hwReading.sensValue) << 12) / 40;

	return pluginReading;
}

/* XXX the inverse transform is not used, and has not been for a long time...  since PID started
   XXX working.  At that point, this class was changed so that instead of inheriting from
   XXX IOPlatformStateSensor, it inherits from IOPlatformSensor.  This method was never removed.
   XXX I've updated it in line with the change in the value transform functions, but I'm leaving
   XXX the code commented out because it's not used and shouldn't be here.

SensorValue PowerMac7_2_PCIPowerSensor::applyCurrentValueInverseTransform( SensorValue pluginReading ) const
{
	SensorValue hwReading;

	hwReading.sensValue = (SInt32)((((UInt32)pluginReading.sensValue) * 40) >> 12);

	return hwReading;
}

*/
