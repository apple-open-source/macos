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


#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "IOPlatformSensor.h"
#include "PowerMac7_2_PIDCtrlLoop.h"

#define super IOPlatformPIDCtrlLoop
OSDefineMetaClassAndStructors(PowerMac7_2_PIDCtrlLoop, IOPlatformPIDCtrlLoop)

//extern const OSSymbol * gPM72EnvShroudRemoved;
extern const OSSymbol * gPM72EnvSystemUncalibrated;

bool PowerMac7_2_PIDCtrlLoop::updateMetaState( void )
{
	const OSArray * metaStateArray;
	const OSDictionary * metaStateDict;
	const OSNumber * newMetaState;

	// if the shroud is opened, use meta-state 2
	// else if there is an overtemp condition, use meta-state 1
	// else if there is a forced meta state, use it
	// else, use meta-state 0

	if ((metaStateArray = OSDynamicCast(OSArray, infoDict->getObject(gIOPPluginThermalMetaStatesKey))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_2_PIDCtrlLoop::updateMetaState no meta state array\n");
		return(false);
	}

	// Check for overtemp condition
//	if ((platformPlugin->envArrayCondIsTrue(gIOPPluginEnvInternalOvertemp)) ||
//	    (platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp)))
	if ((platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp)) ||
	    (platformPlugin->getEnv(gPM72EnvSystemUncalibrated)) != NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_2_PIDCtrlLoop::updateMetaState Entering Overtemp Mode\n");

		if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(1))) != NULL &&
		    (cacheMetaState( metaStateDict ) == true))
		{
			// successfully entered overtemp mode
			setMetaState( gIOPPluginOne );
			return(true);
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_2_PIDCtrlLoop::updateMetaState Overtemp Mode Failed!\n");
		}
	}

	// Look for forced meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, infoDict->getObject(gIOPPluginForceCtrlLoopMetaStateKey))) != NULL)
	{
		if (cacheMetaState( metaStateDict ) == true)
		{
			CTRLLOOP_DLOG("PowerMac7_2_PIDCtrlLoop::updateMetaState using forced meta state\n");
			newMetaState = OSNumber::withNumber( 0xFFFFFFFF, 32 );
			setMetaState( newMetaState );
			newMetaState->release();
			return(true);
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_2_PIDCtrlLoop::updateMetaState forced meta state is invalid, removing...\n");
			infoDict->removeObject(gIOPPluginForceCtrlLoopMetaStateKey);
		}
	}

	// Use default "Normal" meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(0))) != NULL &&
	    (cacheMetaState( metaStateDict ) == true))
	{
		//CTRLLOOP_DLOG("PowerMac7_2_PIDCtrlLoop::updateMetaState use meta state zero\n");
		setMetaState( gIOPPluginZero );
		return(true);
	}
	else
	{
		// can't find a valid meta state, nothing we can really do except log an error
		CTRLLOOP_DLOG("PowerMac7_2_PIDCtrlLoop::updateMetaState no valid meta states!\n");
		return(false);
	}
}

ControlValue PowerMac7_2_PIDCtrlLoop::calculateNewTarget( void ) const
{
	SInt32 dRaw, rRaw;
	SInt64 accum, dProd, rProd, pProd;
	SInt32 result;
	ControlValue newTarget;
	samplePoint * latest;

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
			result = (SInt32)outputControl->getTargetValue();

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
			result += (SInt32)accum;
		}

		newTarget = (UInt32)(result > 0) ? result : 0;

		// apply the hard limits
		if (newTarget < outputMin)
			newTarget = outputMin;
		else if (newTarget > outputMax)
			newTarget = outputMax;

/*
#ifdef CTRLLOOP_DEBUG
	if (timerCallbackActive)
	{
		const OSString * tempDesc;
#endif
		CTRLLOOP_DLOG("%s"
		              " G_p=%08lX"
					  " G_d=%08lX"
					  " G_r=%08lX"
					  " T_cur=%08lX"
					  " Res=%016llX"
					  " Out=%lu"
		              " T_err=%08lX"
					  " pProd=%016llX"
		              " dRaw=%08lX"
					  " dProd=%016llX"
					  " rRaw=%08lX"
					  " rProd=%016llX",
						(tempDesc = OSDynamicCast( OSString, infoDict->getObject(kIOPPluginThermalGenericDescKey))) != NULL ?
								tempDesc->getCStringNoCopy() : "Unknown CtrlLoop",
					  G_p,
					  G_d,
					  G_r,
					  latest->sample.sensValue,
					  accum,
					  uResult,
					  (latest->error.sensValue),
					  (pProd),
					  (dRaw),
					  (dProd),
					  (rRaw),
					  (rProd) );
#ifdef CTRLLOOP_DEBUG
	}
#endif
*/
	}

	return(newTarget);
}
