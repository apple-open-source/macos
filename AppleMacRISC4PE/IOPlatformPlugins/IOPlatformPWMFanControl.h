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
//		$Log: IOPlatformPWMFanControl.h,v $
//		Revision 1.3  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.2.2.1  2003/05/26 10:07:14  eem
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

#ifndef _IOPLATFORMPWMFANCONTROL_H
#define _IOPLATFORMPWMFANCONTROL_H

#include "IOPlatformControl.h"

class IOPlatformPWMFanControl : public IOPlatformControl
{

	OSDeclareDefaultStructors(IOPlatformPWMFanControl)

protected:

	UInt32 ticksPerCycle;

	// the FCU represents PWM as a value in the range [0..255] for ticks per cycle
	// we need to convert that to a value in the range [0..100] for duty cycle %
	virtual const OSNumber * applyTargetValueTransform( const OSNumber * hwReading );
	virtual const OSNumber * applyTargetHWTransform( const OSNumber * value );

public:

	virtual IOReturn initPlatformControl( const OSDictionary *dict );

};

#endif // _IOPLATFORMPWMFANCONTROL_H