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
//		$Log: IOPlatformSlewClockControl.cpp,v $
//		Revision 1.3  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.2.2.2  2003/06/04 10:21:10  eem
//		Supports forced PID meta states.
//		
//		Revision 1.2.2.1  2003/05/26 10:07:15  eem
//		Fixed most of the bugs after the last cleanup/reorg.
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

#include "IOPlatformPluginSymbols.h"
#include "IOPlatformSlewClockControl.h"

#define super IOPlatformControl
OSDefineMetaClassAndStructors(IOPlatformSlewClockControl, IOPlatformControl)


const OSNumber *IOPlatformSlewClockControl::fetchCurrentValue( void )
{
	const OSNumber * currentValue, * raw;

	CONTROL_DLOG("IOPlatformSlewClockControl::fetchCurrentValue - entered\n");

	if (!(isRegistered() == kOSBooleanTrue) || !controlDriver)
	{
		CONTROL_DLOG("IOPlatformSlewClockControl::fetchCurrentValue not registered\n");
		return(NULL);
	}

	if ((raw = OSDynamicCast(OSNumber,	controlDriver->getProperty(gIOPPluginCurrentValueKey))) != NULL)
	{
		currentValue = applyCurrentValueTransform(raw);
		//raw->release();
		return(currentValue);
	}

	// if we get here, the current-value was not found in any known location
	CONTROL_DLOG("IOPlatformSlewClockControl::fetchCurrentValue can't find current value in controlDriver!!\n");
	return(NULL);
}
