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
//		$Log: PowerMac7_2_PCIPowerSensor.cpp,v $
//		Revision 1.2  2003/07/18 00:22:24  eem
//		[3329244] PCI fan conrol algorithm should use integral of power consumed
//		[3254911] Q37 Platform Plugin must disable debugging accessors before GM
//		
//		Revision 1.1  2003/07/08 04:32:51  eem
//		3288891, 3279902, 3291553, 3154014
//		
//
//

#include "PowerMac7_2_PCIPowerSensor.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(PowerMac7_2_PCIPowerSensor, IOPlatformSensor)

const OSNumber *PowerMac7_2_PCIPowerSensor::applyValueTransform( const OSNumber * hwReading ) const
{
	const OSNumber *scaled;

	scaled = OSNumber::withNumber( (hwReading->unsigned32BitValue() << 12) / 40, 32 );
	//SENSOR_DLOG("PowerMac7_2_ScaledSensor::applyValueTransform raw = %08lX value = %08lX\n",
	//		hwReading->unsigned32BitValue(), scaled->unsigned32BitValue());

	return scaled;
}

const OSNumber *PowerMac7_2_PCIPowerSensor::applyHWTransform( const OSNumber * value ) const
{
	const OSNumber * raw;

	raw = OSNumber::withNumber( (value->unsigned32BitValue() * 40) >> 12, 32 );
	//SENSOR_DLOG("PowerMac7_2_ScaledSensor::applyHWTransform value = %08lX raw = %08lX\n",
	//		value->unsigned32BitValue(), raw->unsigned32BitValue());

	return raw;
}
