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

#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "IOPlatformSensor.h"
#include "RackMac3_1_DIMMFanCtrlLoop.h"

#define super IOPlatformPIDCtrlLoop
OSDefineMetaClassAndStructors(RackMac3_1_DIMMFanCtrlLoop, IOPlatformPIDCtrlLoop)

extern const OSSymbol * gRM31DIMMFanCtrlLoopTarget;

void RackMac3_1_DIMMFanCtrlLoop::sendNewTarget( ControlValue newTarget )
{
	OSNumber * newTargetNum = OSNumber::withNumber( newTarget, 32 );

	// If the new target value is different, send it to the control
	if ( ctrlloopState == kIOPCtrlLoopFirstAdjustment || 
		 ctrlloopState == kIOPCtrlLoopDidWake || 
		 !newTargetNum->isEqualTo(platformPlugin->getEnv(gRM31DIMMFanCtrlLoopTarget)) )
	{
		if( platformPlugin->setEnv(gRM31DIMMFanCtrlLoopTarget, newTargetNum) )
		{
			//CTRLLOOP_DLOG("RackMac3_1_DIMMFanCtrlLoop::sendNewTarget setEnv 'DIMMFanCtrlLoopTarget' to %d RPMs\n", newTargetNum->unsigned32BitValue());
	
			ctrlloopState = kIOPCtrlLoopAllRegistered;
		}
		else
			CTRLLOOP_DLOG("RackMac3_1_DIMMFanCtrlLoop::sendNewTarget failed to set 'average-DIMM-temperature' value\n");
	}

	if (newTargetNum) newTargetNum->release();
}
