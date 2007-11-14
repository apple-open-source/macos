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
#include "PowerMac7_2_SlotFanCtrlLoop.h"

#define super IOPlatformPIDCtrlLoop
OSDefineMetaClassAndStructors(PowerMac7_2_SlotFanCtrlLoop, IOPlatformPIDCtrlLoop)

extern const OSSymbol * gPM72EnvSystemUncalibrated;

bool PowerMac7_2_SlotFanCtrlLoop::init( void )
{
	if (!super::init()) return(false);

	activeAGPControl = false;

	agpSensor = NULL;
	agpControl = NULL;

	return(true);
}

bool PowerMac7_2_SlotFanCtrlLoop::updateMetaState( void )
{
	const OSArray * metaStateArray;
	const OSDictionary * metaStateDict;
	const OSNumber * newMetaState;

	// else if there is an overtemp condition, use meta-state 1 (normal) or 3 (agp card control)
	// else if there is a forced meta state, use it
	// else, use meta-state 0 (normal) or 2 (agp card control)

	if ((metaStateArray = OSDynamicCast(OSArray, infoDict->getObject(gIOPPluginThermalMetaStatesKey))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::updateMetaState no meta state array\n");
		return(false);
	}

	if ((platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp)) ||
	    (platformPlugin->getEnv(gPM72EnvSystemUncalibrated)) != NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::updateMetaState Entering Overtemp Mode\n");

		// Choose the correct failsafe meta state
		int failsafeIndex = activeAGPControl ? 3 : 1;
		const OSNumber* failsafeIndexNum = activeAGPControl ? gIOPPluginThree : gIOPPluginOne;

		CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::updateMetaState Entering Overtemp Mode (%d)\n", failsafeIndex );

		if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject( failsafeIndex ))) != NULL &&
		    (cacheMetaState( metaStateDict ) == true))
		{
			// successfully entered overtemp mode
			setMetaState( failsafeIndexNum );
			return(true);
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::updateMetaState Overtemp Mode Failed!\n");
		}
	}

	// Look for forced meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, infoDict->getObject(gIOPPluginForceCtrlLoopMetaStateKey))) != NULL)
	{
		if (cacheMetaState( metaStateDict ) == true)
		{
			CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::updateMetaState using forced meta state\n");
			newMetaState = OSNumber::withNumber( 0xFFFFFFFF, 32 );
			setMetaState( newMetaState );
			newMetaState->release();
			return(true);
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::updateMetaState forced meta state is invalid, removing...\n");
			infoDict->removeObject(gIOPPluginForceCtrlLoopMetaStateKey);
		}
	}

	// Choose the correct failsafe meta state
	int normalIndex = activeAGPControl ? 2 : 0;
	const OSNumber* normalIndexNum = activeAGPControl ? gIOPPluginTwo : gIOPPluginZero;

	CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::updateMetaState Trying for Normal Meta State (%d)\n", normalIndex );

	// Use default "Normal" meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject( normalIndex ))) != NULL &&
	    (cacheMetaState( metaStateDict ) == true))
	{

		setMetaState( normalIndexNum );
		return(true);
	}
	else
	{
		// can't find a valid meta state, nothing we can really do except log an error
		CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::updateMetaState no valid meta states!\n");
		return(false);
	}
}

SensorValue PowerMac7_2_SlotFanCtrlLoop::getAggregateSensorValue( void )
{
	SensorValue powerValue, agpValue;

	powerValue = inputSensor->forceAndFetchCurrentValue();
	inputSensor->setCurrentValue( powerValue );

	if ( agpSensor )
	{
		agpValue = agpSensor->forceAndFetchCurrentValue();
		agpSensor->setCurrentValue( agpValue );
	}

	return( activeAGPControl ? agpValue : powerValue );
}

ControlValue PowerMac7_2_SlotFanCtrlLoop::calculateNewTarget( void ) const
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

			// if we're doing AGP Card Control, we use the PID result as a delta with respect to the
			// previous target. For traditional PCI power control, the result is calculated independent
			// of the previous result.
			if ( activeAGPControl )
			{
				result = (SInt32)accum + (SInt32)agpControl->getTargetValue();
			}
			else
			{
				result = (SInt32)accum;
			}
		}

		newTarget = (UInt32)(result > 0) ? result : 0;

		// apply the hard limits
		if (newTarget < outputMin)
			newTarget = outputMin;
		else if (newTarget > outputMax)
			newTarget = outputMax;

#if 0
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
					  " accum=%016llX"
					  " result=%ld"
		              " T_err=%08lX"
					  " pProd=%016llX"
		              " dRaw=%08lX"
					  " dProd=%016llX"
					  " rRaw=%08lX"
					  " rProd=%016llX\n",
						(tempDesc = OSDynamicCast( OSString, infoDict->getObject(kIOPPluginThermalGenericDescKey))) != NULL ?
								tempDesc->getCStringNoCopy() : "Unknown CtrlLoop",
					  G_p,
					  G_d,
					  G_r,
					  latest->sample.sensValue,
					  accum,
					  result,
					  (latest->error.sensValue),
					  (pProd),
					  (dRaw),
					  (dProd),
					  (rRaw),
					  (rProd) );
#ifdef CTRLLOOP_DEBUG
	}
#endif
#endif
	}

	return(newTarget);
}

void PowerMac7_2_SlotFanCtrlLoop::sendNewTarget( ControlValue newTarget )
{
	bool				updateCtrlLoopState = false;

	if ( activeAGPControl )
	{
		if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
			ctrlloopState == kIOPCtrlLoopDidWake ||
			newTarget != agpControl->getTargetValue() )
		{
			if (agpControl->sendTargetValue( newTarget ))
			{
				agpControl->setTargetValue(newTarget);
				updateCtrlLoopState |= true;
			}
			else
			{
				CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::sendNewTarget failed to send AGP target value\n");
			}
		}

		if ( outputControl && outputControl->isRegistered() == kOSBooleanTrue )
		{
			// If we're in AGP Card Control mode, newTarget is an AGP fan speed. Map it
			// onto the PCI fan.
			ControlValue pciFanTarget;
	
			// the following equation makes the assumption that the min/max for both fans
			// is greater than or equal to zero:
			//
			// ( pci_target - pci_min ) / ( pci_max - pci_min) = ( agp_target - agp_min ) / ( agp_max - agp_min )
	
			pciFanTarget = ( ( pciFanMax - pciFanMin) * ( newTarget - outputMin ) ) / ( outputMax - outputMin ) + pciFanMin;
	
			CTRLLOOP_DLOG( "NV40 FAN TARGET: %lu PCI FAN TARGET: %lu\n", newTarget, pciFanTarget );
	
			if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
				ctrlloopState == kIOPCtrlLoopDidWake ||
				pciFanTarget != outputControl->getTargetValue() )
			{
				if (outputControl->sendTargetValue( pciFanTarget ))
				{
					outputControl->setTargetValue( pciFanTarget );
					updateCtrlLoopState |= true;
				}
				else
				{
					CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::sendNewTarget failed to send PCI target value\n");
				}
			}
		}
	}
	else
	{	
		if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
			ctrlloopState == kIOPCtrlLoopDidWake ||
			newTarget != outputControl->getTargetValue() )
		{
			if (outputControl->sendTargetValue( newTarget ))
			{
				outputControl->setTargetValue(newTarget);
				updateCtrlLoopState |= true;
			}
			else
			{
				CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::sendNewTarget failed to send target value\n");
			}
		}
	}

	if ( updateCtrlLoopState )
		ctrlloopState = kIOPCtrlLoopAllRegistered;
}

void PowerMac7_2_SlotFanCtrlLoop::sensorRegistered( IOPlatformSensor * aSensor )
{
	const OSData * zone;

	// see if this is a dynamically-provided AGP sensor, such as a video card temperature sensor
	if ( aSensor != inputSensor &&
	     ( ( zone = aSensor->getSensorZone() ) != NULL ) &&
		 ( *(UInt32 *)zone->getBytesNoCopy() == kIOPPluginAGPThermalZone ) )
	{
		CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::sensorRegistered got AGP sensor 0x%08lX\n",
				aSensor->getSensorID()->unsigned32BitValue() );

		// This is an AGP sensor. First, disable polling.
		aSensor->setPollingPeriod( (UInt32) kIOPPluginNoPolling );
		aSensor->setPollingPeriodNS( (UInt32) kIOPPluginNoPolling );
		aSensor->sendPollingPeriod();

		// Now, look for the input-target property. If found, save it in the meta state dictionary
		// and use this sensor to drive the AGP/PCI control loop.
		IOService* agpSensorDriver;
		IOService* agpSensorNub;
		OSData* target;

		if ( ( ( agpSensorDriver = aSensor->getSensorDriver() ) != NULL ) &&
		     ( ( agpSensorNub = agpSensorDriver->getProvider() ) != NULL ) &&
			 ( ( target = OSDynamicCast( OSData, agpSensorNub->getProperty( "input-target" ) ) ) != NULL ) )
		{
			CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::sensorRegistered AGP Sensor 0x%08lX has input-target 0x%08lX\n",
					aSensor->getSensorID()->unsigned32BitValue(), *(UInt32 *)target->getBytesNoCopy() );

			// place the input-target value in the "AGP control" meta state dict

			OSNumber* numTarget;
			OSArray* metaStateArray;
			OSDictionary* metaStateDict;

			// grab the meta state array
			metaStateArray = OSDynamicCast( OSArray, infoDict->getObject( gIOPPluginThermalMetaStatesKey ) );

			// "AGP Control" meta state is at index 2
			if ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 2 ) ) ) == NULL )
			{
				CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::sensorRegistered Failed to fetch meta state 2\n" );
				goto AGP_Sensor_Init_Failed;
			}

			// encapsulate the input-target as an OSNumber and insert it
			numTarget = OSNumber::withNumber( *(UInt32 *)target->getBytesNoCopy(), 32 );
			metaStateDict->setObject( kIOPPIDCtrlLoopInputTargetKey, numTarget );
			numTarget->release();

			// designate this as the AGP sensor to use for input

			agpSensor = aSensor;
		}
	}

AGP_Sensor_Init_Failed:

	// if we've found both an agp sensor and an agp control, then switch to AGP control mode
	if ( !activeAGPControl && agpSensor && agpControl )
	{
		CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::sensorRegistered AGP Card Control active\n" );
		activeAGPControl = true;
		updateMetaState();
	}

	/*
	 * Decide whether or not to kick off the control algorithm by invoking deadlinePassed().
	 *
	 * - if the deadline is already non-zero, the ball is already rolling, so do nothing
	 *
	 * - if activeAGPControl is true, we have all the required components for AGP Card Control,
	 *   so begin the control algorithm.
	 *
	 * - if inputSensor and outputControl are registered, we can do tradition PCI fan control,
	 *   so kick it off.
	 */

	if ( activeAGPControl ||
	     ( inputSensor && ( inputSensor->isRegistered() == kOSBooleanTrue ) &&
	       outputControl && ( outputControl->isRegistered() == kOSBooleanTrue ) ) )
	{
		CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::sensorRegistered starting control algorithm, AGP Control = %s\n",
				activeAGPControl ? "TRUE" : "FALSE" );

		ctrlloopState = kIOPCtrlLoopFirstAdjustment;

		// set the deadline
		deadlinePassed();
	}
}

void PowerMac7_2_SlotFanCtrlLoop::controlRegistered( IOPlatformControl * aControl )
{
	const OSData * zone;

	// see if this is a dynamically-provided AGP control, such as a video card fan
	if ( aControl != outputControl &&
	     ( ( zone = aControl->getControlZone() ) != NULL ) &&
		 ( *(UInt32 *)zone->getBytesNoCopy() == kIOPPluginAGPThermalZone ) )
	{
		CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered got AGP control 0x%08lX\n",
				aControl->getControlID()->unsigned32BitValue() );

		OSNumber* tmpNumber;
		OSArray* metaStateArray;
		OSDictionary* metaStateDict;

		// grab the meta state array
		metaStateArray = OSDynamicCast( OSArray, infoDict->getObject( gIOPPluginThermalMetaStatesKey ) );

		// "AGP Control" meta state is at index 2
		if ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 2 ) ) ) == NULL )
		{
			CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered Failed to fetch meta state 2\n" );
			goto AGP_Control_Init_Failed;
		}

		// When we're in AGP Card Control mode, we use the PID result to drive the AGP
		// fan, and then we map that fan speed onto the PCI fan. The PCI fan max and min
		// are specified by two properties in the meta state 2 entry, pci-fan-min and
		// pci-fan-max

		if ( ( tmpNumber = OSDynamicCast( OSNumber, metaStateDict->getObject( "pci-fan-min" ) ) ) == NULL )
		{
			CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered Failed to fetch pci-fan-min\n" );
			goto AGP_Control_Init_Failed;
		}
		
		pciFanMin = tmpNumber->unsigned32BitValue();

		if ( ( tmpNumber = OSDynamicCast( OSNumber, metaStateDict->getObject( "pci-fan-max" ) ) ) == NULL )
		{
			CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered Failed to fetch pci-fan-max\n" );
			goto AGP_Control_Init_Failed;
		}

		pciFanMax = tmpNumber->unsigned32BitValue();

		CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered PCI FAN Min: %lu Max: %lu\n",
				pciFanMin, pciFanMax );

		// Min output (RPM/PWM)

		if ( ( tmpNumber = OSNumber::withNumber( aControl->getControlMinValue(), 16 ) ) == NULL )
		{
			CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered Failed to fetch control's min-value\n" );
			goto AGP_Control_Init_Failed;
		}

		CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered output-min: %lu\n", tmpNumber->unsigned32BitValue() );

		metaStateDict->setObject( kIOPPIDCtrlLoopOutputMinKey, tmpNumber );
		tmpNumber->release();

		// Max output (RPM/PWM)

		if ( ( tmpNumber = OSNumber::withNumber( aControl->getControlMaxValue(), 16 ) ) == NULL )
		{
			CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered Failed to fetch control's max-value\n" );
			goto AGP_Control_Init_Failed;
		}

		CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered output-max: %lu\n", tmpNumber->unsigned32BitValue() );

		metaStateDict->setObject( kIOPPIDCtrlLoopOutputMaxKey, tmpNumber );

		// "AGP Control Failsafe" meta state is at index 3
		if ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 3 ) ) ) == NULL )
		{
			CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered Failed to fetch meta state 3\n" );
			tmpNumber->release();
			goto AGP_Control_Init_Failed;
		}

		// Set "output-override" to be "output-max".

		metaStateDict->setObject( kIOPPIDCtrlLoopOutputOverrideKey, tmpNumber );
		tmpNumber->release();

		// designate this as the AGP sensor to use for input

		agpControl = aControl;
	}

AGP_Control_Init_Failed:

	// if we've found both an agp sensor and an agp control, then switch to AGP control mode
	if ( !activeAGPControl && agpSensor && agpControl )
	{
		CTRLLOOP_DLOG( "PowerMac7_2_SlotFanCtrlLoop::controlRegistered AGP Card Control active\n" );
		activeAGPControl = true;
		updateMetaState();
	}

	/*
	 * Decide whether or not to kick off the control algorithm by invoking deadlinePassed().
	 *
	 * - if activeAGPControl is true, we have all the required components for AGP Card Control,
	 *   so begin the control algorithm.
	 *
	 * - if inputSensor and outputControl are registered, we can do tradition PCI fan control,
	 *   so kick it off.
	 */

	if ( activeAGPControl ||
	     ( inputSensor && ( inputSensor->isRegistered() == kOSBooleanTrue ) &&
	       outputControl && ( outputControl->isRegistered() == kOSBooleanTrue ) ) )
	{
		CTRLLOOP_DLOG("PowerMac7_2_SlotFanCtrlLoop::controlRegistered starting control algorithm, AGP Control = %s\n",
				activeAGPControl ? "TRUE" : "FALSE" );

		ctrlloopState = kIOPCtrlLoopFirstAdjustment;

		// set the deadline
		deadlinePassed();
	}
}
