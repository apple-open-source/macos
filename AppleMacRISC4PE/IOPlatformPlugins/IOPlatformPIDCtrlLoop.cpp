/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "IOPlatformSensor.h"
#include "IOPlatformPIDCtrlLoop.h"

#define super IOPlatformCtrlLoop
OSDefineMetaClassAndStructors(IOPlatformPIDCtrlLoop, IOPlatformCtrlLoop)

bool IOPlatformPIDCtrlLoop::init( void )
{
	if (!super::init()) return(false);

	inputSensor = NULL;
	outputControl = NULL;
	outputOverride = NULL;

	historyArray = NULL;

	return(true);
}

void IOPlatformPIDCtrlLoop::free( void )
{
	if (inputSensor) { inputSensor->release(); inputSensor = NULL; }
	if (outputControl) { outputControl->release(); outputControl = NULL; }
	if (outputOverride) { outputOverride->release(); outputOverride = NULL; }

	if (historyArray)
	{
		IOFree(historyArray, sizeof(samplePoint) * historyLen);
		historyArray = NULL;
	}

	super::free();
}

IOReturn IOPlatformPIDCtrlLoop::initPlatformCtrlLoop( const OSDictionary *dict)
{
	IOReturn status;
	const OSArray * array;

	status = super::initPlatformCtrlLoop(dict);

	// initialize the sample history
	historyLen = kIOPPIDCtrlLoopDefaultHistLen;
	historyArray = (samplePoint *) IOMalloc( sizeof(samplePoint) * historyLen );
	if (!historyArray)
	{
		CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::initPlatformCtrlLoop failed to allocate history array len %u\n", historyLen);
		goto failNoHistory;
	}

	bzero( historyArray, sizeof(samplePoint) * historyLen );
	latestSample = 0;

	// the first listed control is the primary control
	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL ||
		(outputControl = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(0)) )) == NULL)
	{
		CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::initPlatformCtrlLoop no control ID!!\n");
		goto failNoControl;
	}

	outputControl->retain();
	addControl( outputControl );

	// assume that the first listed sensor is the one we want
	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalSensorIDsKey))) == NULL ||
		(inputSensor = OSDynamicCast(IOPlatformSensor, platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(0)) ))) == NULL)
	{
		CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::initPlatformCtrlLoop no sensor ID!!\n");
		goto failNoSensor;
	}

	inputSensor->retain();
	addSensor( inputSensor );

	// initialize the meta-state
	if (!updateMetaState())
	{
		goto failBadMetaState;
	}

	return status;

failBadMetaState:
	removeSensor( inputSensor );
	inputSensor->release();

failNoSensor:
	removeControl( outputControl );
	outputControl->release();

failNoControl:
failNoHistory:

	return(kIOReturnError);
}

bool IOPlatformPIDCtrlLoop::acquireSample( void )
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

	// store the sample in the history
	latest->sample.sensValue = curValue.sensValue;

	// calculate the error term and store it
	latest->error.sensValue = latest->sample.sensValue - inputTarget.sensValue;

	//CTRLLOOP_DLOG("*** SAMPLE *** InT: 0x%08lX Cur: 0x%08lX Error: 0x%08lX\n",
	//		inputTarget.sensValue, latest->sample.sensValue, latest->error.sensValue);

	return(true);
}

IOPlatformPIDCtrlLoop::samplePoint *IOPlatformPIDCtrlLoop::sampleAtIndex( unsigned int index ) const
{
	unsigned int myIndex;

	myIndex = latestSample + index;

	if (myIndex >= historyLen)
		myIndex -= historyLen;

	return( &historyArray[myIndex] );
}

SensorValue IOPlatformPIDCtrlLoop::getAggregateSensorValue( void )
{
	SensorValue aggValue;

	aggValue = inputSensor->forceAndFetchCurrentValue();
	inputSensor->setCurrentValue( aggValue );

	return( aggValue );
}

bool IOPlatformPIDCtrlLoop::updateMetaState( void )
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
		CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::updateMetaState no meta state array\n");
		return(false);
	}

	// Check for overtemp condition
	if ((platformPlugin->envArrayCondIsTrue(gIOPPluginEnvInternalOvertemp)) ||
	    (platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp)))
	{
		CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::updateMetaState Entering Overtemp Mode\n");

		if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(kIOPCtrlLoopFailsafeMetaState))) != NULL &&
		    (cacheMetaState( metaStateDict ) == true))
		{
			// successfully entered overtemp mode
			setMetaState( gIOPPluginOne );
			return(true);
		}
		else
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::updateMetaState Overtemp Mode Failed!\n");
		}
	}

	// Look for forced meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, infoDict->getObject(gIOPPluginForceCtrlLoopMetaStateKey))) != NULL)
	{
		if (cacheMetaState( metaStateDict ) == true)
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::updateMetaState using forced meta state\n");
			newMetaState = OSNumber::withNumber( 0xFFFFFFFF, 32 );
			setMetaState( newMetaState );
			newMetaState->release();
			return(true);
		}
		else
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::updateMetaState forced meta state is invalid, removing...\n");
			infoDict->removeObject(gIOPPluginForceCtrlLoopMetaStateKey);
		}
	}

	// Use default "Normal" meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(kIOPCtrlLoopNormalMetaState))) != NULL &&
	    (cacheMetaState( metaStateDict ) == true))
	{
		CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::updateMetaState use meta state zero\n");
		setMetaState( gIOPPluginZero );
		return(true);
	}
	else
	{
		// can't find a valid meta state, nothing we can really do except log an error
		CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::updateMetaState no valid meta states!\n");
		return(false);
	}
}

bool IOPlatformPIDCtrlLoop::cacheMetaState( const OSDictionary * metaState )
{
	const OSData * dataG_p, * dataG_d, * dataG_r;
	const OSNumber * numInterval, * numOverride, * numInputTarget;
	const OSNumber * numOutputMin, * numOutputMax, * numHistLen;
	//const OSNumber * tmpNumber;
	samplePoint * sample, * newHistoryArray;
	unsigned int i;
	UInt32 tempInterval, newHistoryLen;

	// cache the interval.  it is listed in seconds.
	if ((numInterval = OSDynamicCast(OSNumber, metaState->getObject("interval"))) != NULL)
	{
		tempInterval = numInterval->unsigned32BitValue();

		if ((tempInterval == 0) || (tempInterval > 300))
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::cacheMetaState meta state interval is out of bounds\n");
			goto failNoInterval;
		}
	}
	else
	{
		CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::cacheMetaState meta state interval is absent\n");
		goto failNoInterval;
	}

	// if there is an output-override key, flag it.  Otherwise, look for the full
	// set of coefficients, setpoints and output bounds
	if ((numOverride = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputOverrideKey))) != NULL)
	{
		overrideActive = true;
		outputOverride = numOverride;
		outputOverride->retain();

		//CTRLLOOP_DLOG("*** PID CACHE *** Override: 0x%08lX\n", outputOverride->unsigned32BitValue());
	}
	else
	{
		// look for G_p, G_d, G_r, input-target, output-max, output-min
		if ((dataG_p = OSDynamicCast(OSData, metaState->getObject(kIOPPIDCtrlLoopProportionalGainKey))) == NULL)
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::cacheMetaState meta state has no G_p\n");
			goto failFullSet;
		}

		if ((dataG_d = OSDynamicCast(OSData, metaState->getObject(kIOPPIDCtrlLoopDerivativeGainKey))) == NULL)
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::cacheMetaState meta state has no G_d\n");
			goto failFullSet;
		}

		if ((dataG_r = OSDynamicCast(OSData, metaState->getObject(kIOPPIDCtrlLoopResetGainKey))) == NULL)
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::cacheMetaState meta state has no G_r\n");
			goto failFullSet;
		}

		if ((numInputTarget = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopInputTargetKey))) == NULL)
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::cacheMetaState meta state has no intput-target\n");
			goto failFullSet;
		}

		if ((numOutputMin = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputMinKey))) == NULL)
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::cacheMetaState meta state has no output-min\n");
			goto failFullSet;
		}

		if ((numOutputMax = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputMaxKey))) == NULL)
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::cacheMetaState meta state has no output-max\n");
			goto failFullSet;
		}

		if ((numHistLen = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopHistoryLenKey))) == NULL)
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::cacheMetaState meta state has no history-length\n");
			goto failFullSet;
		}

		overrideActive = false;
		if (outputOverride) { outputOverride->release(); outputOverride = NULL; }

		G_p = *((SInt32 *) dataG_p->getBytesNoCopy());
		G_d = *((SInt32 *) dataG_d->getBytesNoCopy());
		G_r = *((SInt32 *) dataG_r->getBytesNoCopy());
		inputTarget.sensValue = (SInt32)numInputTarget->unsigned32BitValue();
		outputMin = numOutputMin->unsigned32BitValue();
		outputMax = numOutputMax->unsigned32BitValue();

		// resize the history array if necessary
		newHistoryLen = numHistLen->unsigned32BitValue();
		if (newHistoryLen == 0) newHistoryLen = 2;	// must be two or more in order to have a valid
													// derivative term

		if (newHistoryLen != historyLen)
		{
			newHistoryArray = (samplePoint *) IOMalloc( sizeof(samplePoint) * newHistoryLen );
			bzero( newHistoryArray, sizeof(samplePoint) * newHistoryLen );

			// copy samples from the old array into the new
			for (i=0; i<historyLen && i<newHistoryLen; i++)
			{
				sample = sampleAtIndex(i);

				(&(newHistoryArray[i]))->sample.sensValue = sample->sample.sensValue;
				(&(newHistoryArray[i]))->error.sensValue = sample->error.sensValue;
			}

			IOFree( historyArray, sizeof(samplePoint) * historyLen );

			historyArray = newHistoryArray;
			historyLen = newHistoryLen;
			latestSample = 0;
		}

//		CTRLLOOP_DLOG("*** PID CACHE *** G_p: 0x%08lX G_d: 0x%08lX G_r: 0x%08lX\n"
//		              "***************** inT: 0x%08lX oMi: 0x%08lX oMa: 0x%08lX\n",
//					  G_p, G_d, G_r, inputTarget, outputMin, outputMax);
	}

	// set the interval
	intervalSec = tempInterval;
	clock_interval_to_absolutetime_interval(intervalSec, NSEC_PER_SEC, &interval);
//	CTRLLOOP_DLOG("***************** Interval: %u\n", intervalSec);

	return(true);

failFullSet:
failNoInterval:
	return(false);

}

void IOPlatformPIDCtrlLoop::deadlinePassed( void )
{
	samplePoint * latest;
	bool deadlineAbsolute;
	bool didSetEnv = false;

	deadlineAbsolute = (ctrlloopState == kIOPCtrlLoopFirstAdjustment);

	timerCallbackActive = true;

	// sample the input
	if (!acquireSample())
	{
		CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::deadlinePassed FAILED TO ACQUIRE INPUT SAMPLE!!!\n");
	}

	latest = sampleAtIndex(0);

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

void IOPlatformPIDCtrlLoop::didWake( void )
{
	AbsoluteTime adjustedInterval;
	const OSNumber * id = getCtrlLoopID();

	super::didWake();

	// a likely consequence of going to sleep is that our timer deadline is
	// now stale.  Set the deadline so that we'll get a callback after one more
	// interval passes.  Offset the timers by 100us * ctrlloop ID.

	// 100 * ctrlLoopID -> absolute time format
	clock_interval_to_absolutetime_interval(100 * id->unsigned32BitValue(), NSEC_PER_USEC, &adjustedInterval);

	// Add standard interval to produce adjusted interval
	ADD_ABSOLUTETIME( &adjustedInterval, &interval );

	clock_absolutetime_interval_to_deadline(adjustedInterval, &deadline);
}

void IOPlatformPIDCtrlLoop::adjustControls( void )
{
	ControlValue newTarget;

	//CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::adjustControls - entered\n");

	if (ctrlloopState == kIOPCtrlLoopNotReady || !timerCallbackActive)
	{
		//CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::adjustControls some entities not yet registered\n");
		return;
	}

	// Apply the PID algorithm
	newTarget = calculateNewTarget();

	// set the target
	sendNewTarget( newTarget );
}

void IOPlatformPIDCtrlLoop::sendNewTarget( ControlValue newTarget )
{
	// If the new target value is different, send it to the control
	if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
	    ctrlloopState == kIOPCtrlLoopDidWake ||
	    newTarget != outputControl->getTargetValue() )
	{
		if (outputControl->sendTargetValue( newTarget ))
		{
			outputControl->setTargetValue(newTarget);
			ctrlloopState = kIOPCtrlLoopAllRegistered;
		}
		else
		{
			CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::sendNewTarget failed to send target value\n");
		}
	}
}

ControlValue IOPlatformPIDCtrlLoop::calculateNewTarget( void ) const
{
	SInt32 dRaw, rRaw;
	SInt64 accum, dProd, rProd, pProd;
	//UInt32 result, prevResult, scratch;
	SInt32 result;
	UInt32 newTarget;
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

		// apply the hard limits

		newTarget = min( newTarget, outputMax );
		newTarget = max( newTarget, outputMin );

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

SensorValue IOPlatformPIDCtrlLoop::calculateDerivativeTerm( void ) const
{
	samplePoint * latest, * previous;
	SensorValue result;

	latest = sampleAtIndex(0);
	previous = sampleAtIndex(1);

	// get the change in the error term over the latest inteval
	result.sensValue = latest->error.sensValue - previous->error.sensValue;

	// divide by the elapsed time to get the slope
	result.sensValue /= (SInt32)intervalSec;

	return(result);
}

SensorValue IOPlatformPIDCtrlLoop::calculateIntegralTerm( void ) const
{
	samplePoint * sample;
	SensorValue accum;
	unsigned int i;

	// add up the error terms for the recorded intervals
	for (accum.sensValue = 0, i=0; i<historyLen; i++)
	{
		sample = sampleAtIndex(i);
		accum.sensValue += sample->error.sensValue;
	}

	// multiply by the interval length
	accum.sensValue *= (SInt32)intervalSec;

	return(accum);
}

void IOPlatformPIDCtrlLoop::sensorRegistered( IOPlatformSensor * aSensor )
{
	//CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::sensorRegistered - entered\n");

	if (aSensor == inputSensor &&
	    outputControl->isRegistered() == kOSBooleanTrue)
	{
		//CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::sensorRegistered allRegistered!\n");

		ctrlloopState = kIOPCtrlLoopFirstAdjustment;

		// set the deadline
		deadlinePassed();
	}
}

void IOPlatformPIDCtrlLoop::controlRegistered( IOPlatformControl * aControl )
{
	//CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::controlRegistered - entered\n");

	if (aControl == outputControl &&
		inputSensor->isRegistered() == kOSBooleanTrue)
	{
		//CTRLLOOP_DLOG("IOPlatformPIDCtrlLoop::controlRegistered allRegistered!\n");

		ctrlloopState = kIOPCtrlLoopFirstAdjustment;

		// set the deadline
		deadlinePassed();
	}
}
