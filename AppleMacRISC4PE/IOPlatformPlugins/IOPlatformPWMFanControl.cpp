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


#include "IOPlatformPWMFanControl.h"

#define super IOPlatformControl
OSDefineMetaClassAndStructors(IOPlatformPWMFanControl, IOPlatformControl)

IOReturn IOPlatformPWMFanControl::initPlatformControl( const OSDictionary *dict )
{
	IOReturn status;
	const OSNumber * ticks;

	if ((status = super::initPlatformControl(dict)) != kIOReturnSuccess)
		return status;

	if ((ticks = OSDynamicCast(OSNumber, dict->getObject("ticks-per-cycle"))) != NULL && ticks != 0)
	{
		ticksPerCycle = ticks->unsigned32BitValue();
	}
	else
	{
		ticksPerCycle = 255;
	}

	CONTROL_DLOG("IOPlatformPWMFanControl::initPlatformControl ticksPerCycle = %u\n", ticksPerCycle);

	return status;
}

ControlValue IOPlatformPWMFanControl::applyTargetValueTransform( ControlValue hwReading )
{
	ControlValue pluginReading;

	pluginReading = (hwReading * 100) / ticksPerCycle;

	return pluginReading;
}

ControlValue IOPlatformPWMFanControl::applyTargetValueInverseTransform( ControlValue pluginReading )
{
	ControlValue hwReading;

	hwReading = (pluginReading * ticksPerCycle) / 100;

	return hwReading;
}
