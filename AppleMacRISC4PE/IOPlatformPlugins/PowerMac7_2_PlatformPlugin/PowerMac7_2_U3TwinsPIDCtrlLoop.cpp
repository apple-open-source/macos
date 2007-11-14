/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "IOPlatformSensor.h"
#include "PowerMac7_2_U3TwinsPIDCtrlLoop.h"

#define super PowerMac7_2_PIDCtrlLoop
OSDefineMetaClassAndStructors(PowerMac7_2_U3TwinsPIDCtrlLoop, PowerMac7_2_PIDCtrlLoop)

IOReturn PowerMac7_2_U3TwinsPIDCtrlLoop::initPlatformCtrlLoop( const OSDictionary *dict)
{
	IOReturn status;
	if (kIOReturnSuccess != (status = super::initPlatformCtrlLoop(dict)))
		return status;

	tDiodeSamples = kPM72PID_DEFAULT_tDiodeSamples;

	return kIOReturnSuccess;
}

// Override deadlinePassed so our acquireSample method is called
// for some reason acquireSample is not virtual!
void PowerMac7_2_U3TwinsPIDCtrlLoop::deadlinePassed( void )
{
	samplePoint * latest;
	bool deadlineAbsolute;
	bool didSetEnv = false;

	deadlineAbsolute = (ctrlloopState == kIOPCtrlLoopFirstAdjustment);

	timerCallbackActive = true;

	// sample the input
	if (!acquireSample())
	{
		//IOLog("PowerMac7_2_U3TwinsPIDCtrlLoop::deadlinePassed FAILED TO ACQUIRE INPUT SAMPLE!!!\n");
	}

	latest = sampleAtIndex(0);
#ifdef kLOG_ENABLED
	IOLog("U3: tgt:%ld.%03lu\n",
		(UInt32)(outputTarget>>36), ((((UInt32)(outputTarget>>33))&7)*128));
#endif // kLOG_ENABLED

	// If we changed the environment, the platform plugin will invoke updateMetaState()
	// and adjustControls().  If not, then we just need to call adjustControls()
	if (!didSetEnv)
	{
		adjustControls();
	}

	// set the deadline
	if (deadlineAbsolute)
	{
		// this is the first time we're setting the deadline.  In order to better stagger
		// timer callbacks, offset the deadline by 100us * ctrlloopID.
		AbsoluteTime adjustedInterval;
		const OSNumber * id = getCtrlLoopID();

		// 100 * ctrlLoopID -> absolute time format
		clock_interval_to_absolutetime_interval(100 * id->unsigned32BitValue(), NSEC_PER_USEC, &adjustedInterval);

		// Add standard interval to produce adjusted interval
		ADD_ABSOLUTETIME( &adjustedInterval, &interval );

		clock_absolutetime_interval_to_deadline(adjustedInterval, &deadline);
	}
	else
	{
		ADD_ABSOLUTETIME(&deadline, &interval);
	}

	timerCallbackActive = false;
}

bool PowerMac7_2_U3TwinsPIDCtrlLoop::acquireSample( void )
{
	samplePoint * latest;
	SensorValue curValue;

	// move the top of the array to the next spot -- it's circular
	if (latestSample == 0)
		latestSample = historyLen - 1;
	else
		latestSample -= 1;

	// get a pointer to the array element where we'll store this sample point
	latest = &historyArray[latestSample];

	// fetch the sensor reading
	curValue = getAggregateSensorValue();


	int i;
	if (tDiodeHistory) // Get another sample and calculate new minimum...
	{
		tDiodeHistory[tDiodeIndex] = curValue.sensValue;
		tDiodeMinimum = tDiodeHistory[0];

		for (i = 1; i < tDiodeSamples; i++)
		{
			if ((UInt32)tDiodeHistory[i] < (UInt32)tDiodeMinimum)
				tDiodeMinimum = tDiodeHistory[i];
		}

#ifdef kLOG_ENABLED
		IOLog("PM72_U3TwinsPIDCtrlLoop N:%ld min:%2ld.%03ld temp[%2ld]=%2ld.%03ld\n",
			tDiodeSamples,
			(UInt32)tDiodeMinimum>>16, (((UInt32)tDiodeMinimum>>13)&7)*125,
			tDiodeIndex,
			(UInt32)tDiodeHistory[tDiodeIndex]>>16,
			(((UInt32)tDiodeHistory[tDiodeIndex]>>13)&7)*125);
#endif // kLOG_ENABLED
		if (++tDiodeIndex >= tDiodeSamples)
			tDiodeIndex = 0;
	}
	else
	if (tDiodeSamples >= 2) // Re-Initialize the sample history.
	{
		tDiodeHistory = (SInt32 *)IOMalloc(tDiodeSamples * sizeof(SInt32));

		// Initialize all samples to the current value.
		for (i = 0; i < tDiodeSamples; i++)
			tDiodeHistory[i] = curValue.sensValue;
		tDiodeMinimum = curValue.sensValue;

		tDiodeIndex = 1;

#ifdef kLOG_ENABLED
		IOLog("PM72_U3TwinsPIDCtrlLoop Changing N samples:%ld min:%2ld.%03ld\n",
			tDiodeSamples, tDiodeMinimum>>16, ((tDiodeMinimum>>12)&7)*125);
#endif // kLOG_ENABLED
	}
	else // No sample history? and Number of samples is less than two?
	{
		// Disable filter...  minimum = current value.
		tDiodeMinimum = curValue.sensValue;
	}

	// store the sample in the history
	latest->sample.sensValue = tDiodeMinimum;

	// calculate the error term and store it
	latest->error.sensValue = latest->sample.sensValue - inputTarget.sensValue;

	//CTRLLOOP_DLOG("*** SAMPLE *** InT: 0x%08lX Cur: 0x%08lX Error: 0x%08lX\n",
	//		inputTarget.sensValue, latest->sample.sensValue, latest->error.sensValue);

	return(true);
}

void PowerMac7_2_U3TwinsPIDCtrlLoop::adjustControls( void )
{
	ControlValue newTarget;

	//CTRLLOOP_DLOG("PowerMac7_2_U3TwinsPIDCtrlLoop::adjustControls - entered\n");

	if (ctrlloopState == kIOPCtrlLoopNotReady)
	{
		//CTRLLOOP_DLOG("PowerMac7_2_U3TwinsPIDCtrlLoop::adjustControls some entities not yet registered\n");
		IOLog("PowerMac7_2_U3TwinsPIDCtrlLoop::adjustControls state == not ready\n");
		return;
	}

	// Apply the PID algorithm
	newTarget = calculateNewTarget();

	// set the target
	sendNewTarget( newTarget );
}


ControlValue PowerMac7_2_U3TwinsPIDCtrlLoop::calculateNewTarget( void )
{
	SInt32 dRaw, rRaw;
	SInt64 accum, dProd, rProd, pProd;
	ControlValue newTarget;
	samplePoint * latest;

	// if there is an output override, use it
	if (overrideActive)
	{
		CTRLLOOP_DLOG("*** PID *** Override Active\n");
		outputTarget = outputOverride->unsigned32BitValue();
		outputTarget <<= 36; // shift for 28.36 fp
	}
	else // apply the PID algorithm to choose a new control target value
	{
		if (ctrlloopState == kIOPCtrlLoopFirstAdjustment)
		{
			outputTarget = 0;
		}
		else
		{
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
		
			// Save the new target in 28.36 fix-point
			outputTarget += accum;
		}
	}

	// apply the hard limits
	SInt64 outputMin64 = ( (SInt64)outputMin << 36 );
	SInt64 outputMax64 = ( (SInt64)outputMax << 36 );

	// The outputTarget could be negative so use signed compares for the limits.
	if (outputTarget < outputMin64)			outputTarget = outputMin64;
	else if (outputTarget > outputMax64)	outputTarget = outputMax64;

	// Only send the integer value to the output controls.
	// truncate the fractional part of the newTarget..
	newTarget = (ControlValue)( outputTarget >> 36 );

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

	return(newTarget);
}
