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
//		$Log: IOPlatformPWMFanControl.cpp,v $
//		Revision 1.3  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.2.2.2  2003/05/26 10:07:14  eem
//		Fixed most of the bugs after the last cleanup/reorg.
//		
//		Revision 1.2.2.1  2003/05/23 05:44:40  eem
//		Cleanup, ctrlloops not get notification for sensor and control registration.
//		
//		Revision 1.2  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.1.2.1  2003/05/17 11:08:22  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
//		
//
//

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


const OSNumber *IOPlatformPWMFanControl::applyTargetValueTransform( const OSNumber * hwReading )
{
	const OSNumber * number;
	UInt32 hw;
	
	hw = hwReading->unsigned32BitValue();
	number = OSNumber::withNumber( (hw * 100) / ticksPerCycle, 32 );

	CONTROL_DLOG("IOPlatformPWMFanControl::applyTargetValueTransform %08lX => %08lX\n",
			hw, number->unsigned32BitValue());

	return number;
}

const OSNumber *IOPlatformPWMFanControl::applyTargetHWTransform( const OSNumber * value )
{
	const OSNumber * number;
	UInt32 val;

	val = value->unsigned32BitValue();
	number = OSNumber::withNumber( (val * ticksPerCycle) / 100, 32 );

	CONTROL_DLOG("IOPlatformPWMFanControl::applyTargetHWTransform %08lX => %08lX\n",
			val, number->unsigned32BitValue());

	return number;
}
