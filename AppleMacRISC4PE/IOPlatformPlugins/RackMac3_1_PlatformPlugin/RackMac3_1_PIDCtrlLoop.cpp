/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */

#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "RackMac3_1_PlatformPlugin.h"
#include "IOPlatformSensor.h"
#include "RackMac3_1_PIDCtrlLoop.h"

#define super IOPlatformPIDCtrlLoop
OSDefineMetaClassAndStructors(RackMac3_1_PIDCtrlLoop, IOPlatformPIDCtrlLoop)

extern RackMac3_1_PlatformPlugin * RM31Plugin;
extern const OSSymbol * gRM31DIMMFanCtrlLoopTarget;


ControlValue RackMac3_1_PIDCtrlLoop::calculateNewTarget( void ) const
{
	SInt32 dRaw, rRaw;
	SInt64 accum, dProd, rProd, pProd;
	//UInt32 result, prevResult, scratch;
	SInt32 result;
	UInt32 newOutputMin;
    ControlValue newTarget;
	samplePoint * latest;
	const OSNumber * newDIMMFanCtrlLoopTargetNum;

	// if there is an output override, use it
	if (overrideActive)
	{
		CTRLLOOP_DLOG("*** PID *** Override Active\n");
		newTarget = outputOverride->unsigned32BitValue();
	}

	// apply the PID algorithm to choose a new control target value
	else
	{
		if (ctrlloopState == kIOPCtrlLoopFirstAdjustment)
		{
			result = 0;
		}
		else
		{
			//result = (SInt32)outputControl->getTargetValue()->unsigned32BitValue();

			// calculate the derivative term
			// apply the derivative gain
			// this is 12.20 * 16.16 => 28.36
			dRaw = calculateDerivativeTerm().sensValue;
			accum = dProd = (SInt64)G_d * (SInt64)dRaw;
			//CTRLLOOP_DLOG("CPU%u dProd=0x%016llX G_d=0x%08lX dRaw=0x%08lX\n", cpuID, dProd, G_d, dRaw);
	
			// calculate the reset term
			// apply the reset gain
			// this is 12.20 * 16.16 => 28.36
			rRaw = calculateIntegralTerm().sensValue;
			rProd = (SInt64)G_r * (SInt64)rRaw; 
			accum += rProd;
	
			// calculate the proportional term
			// apply the proportional gain
			// this is 12.20 * 16.16 => 28.36
			latest = sampleAtIndex(0);
			pProd = (SInt64)G_p * (SInt64)latest->error.sensValue;
			accum += pProd;
			
			// truncate the fractional part
			accum >>= 36;
	
			//result = (UInt32)(accum < 0 ? 0 : (accum & 0xFFFFFFFF));
			//result += (SInt32)accum;
			result = (SInt32)accum;
		}

		newTarget = (UInt32)(result > 0) ? result : 0;

		// Determine min fan speed based on DIMM control loop
		newOutputMin = outputMin;
		newDIMMFanCtrlLoopTargetNum = OSDynamicCast(OSNumber, platformPlugin->getEnv(gRM31DIMMFanCtrlLoopTarget));
		if(newDIMMFanCtrlLoopTargetNum)
		{
			newOutputMin = newDIMMFanCtrlLoopTargetNum->unsigned32BitValue();
			newOutputMin = (newOutputMin*100/14000);

			if(newOutputMin < outputMin)
				newOutputMin = outputMin;
		}
		
        // apply the hard limits
        if (newTarget < newOutputMin)
            newTarget = newOutputMin;
        else if (newTarget > outputMax)
            newTarget = outputMax;
	}

	return(newTarget);
}
