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
 *  File: $Id: SMU_Neo2_SlotFanCtrlLoop.cpp,v 1.6 2005/07/15 22:01:12 mpontil Exp $
 *
 */



#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "SMU_Neo2_SlotFanCtrlLoop.h"

#define super IOPlatformPIDCtrlLoop

OSDefineMetaClassAndStructors( SMU_Neo2_SlotFanCtrlLoop, IOPlatformPIDCtrlLoop )

bool SMU_Neo2_SlotFanCtrlLoop::init( void )
{
	if (!super::init()) return(false);


	activeAGPControl = false;

	agpSensor = NULL;
	agpControl = NULL;

	pciFanMinWithAGP = 0xFFFF;
	pciFanMaxWithAGP = 0xFFFF;

	safeMetaStateIndex = (unsigned int) -1;
	agpSafeMetaStateIndex = (unsigned int) -1;
	pciFanSafe = 0xFFFF;

	return(true);
}

/*
 *
 * SMU_Neo2_SlotFanCtrlLoop Meta States:
 *
 *	0: Normal, traditional PCI fan control based on PCI power sensor
 *	1: Failsafe, traditional PCI fan control based on PCI power sensor
 *	2: AGP Card Control, AGP card fan control + PCI fan control based on AGP temperature sensor
 *	3: Failsafe for AGP Card Control environment
 *	4,5: Dynamically created "Safe" or "Door Ajar" mode
 *
 */

bool SMU_Neo2_SlotFanCtrlLoop::updateMetaState( void )
	{
	const OSArray*								metaStateArray;
	const OSDictionary*							metaStateDict;

	if ( ( metaStateArray = OSDynamicCast( OSArray, infoDict->getObject( gIOPPluginThermalMetaStatesKey ) ) ) == NULL )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::updateMetaState: No meta state array.\n" );

		return( false );
		}

	// Check for door ajar condition.

	if ( ( platformPlugin->getEnv( gIOPPluginEnvChassisSwitch ) == kOSBooleanFalse ) &&
		 ( ( activeAGPControl && ( agpSafeMetaStateIndex != (unsigned int) -1 ) ) ||
		   ( safeMetaStateIndex != (unsigned int) -1 ) ) )
		{

		int safeIndex = activeAGPControl ? agpSafeMetaStateIndex : safeMetaStateIndex;
	
		CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::updateMetaState: Entering Safe Mode. (Meta State %u)\n", safeIndex );

		if ( ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( safeIndex ) ) ) != NULL ) &&
		    ( cacheMetaState( metaStateDict ) ) )
			{
			const OSNumber * newState = OSNumber::withNumber( safeIndex, 32 );
			setMetaState( newState );
			newState->release();

			return( true );
			}
		}

	// Check for overtemp condition.

	if ( platformPlugin->envArrayCondIsTrue( gIOPPluginEnvExternalOvertemp ) )
		{

		int failsafeIndex = activeAGPControl ? 3 : 1;
		const OSNumber* failsafeIndexNum = activeAGPControl ? gIOPPluginThree : gIOPPluginOne;

		CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::updateMetaState: Entering Overtemp Mode (%d).\n", failsafeIndex );

		if ( ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( failsafeIndex ) ) ) != NULL ) &&
		    ( cacheMetaState( metaStateDict ) ) )
			{
			setMetaState( failsafeIndexNum );

			return( true );
			}
		}

	// Look for forced meta state.

	if ( ( metaStateDict = OSDynamicCast( OSDictionary, infoDict->getObject( gIOPPluginForceCtrlLoopMetaStateKey ) ) ) != NULL )
		{
		if ( cacheMetaState( metaStateDict ) )
			{
			const OSNumber*								newMetaState;

			newMetaState = OSNumber::withNumber( 0xFFFFFFFF, 32 );
			setMetaState( newMetaState );
			newMetaState->release();

			return(true);
			}
		else
			{
			infoDict->removeObject( gIOPPluginForceCtrlLoopMetaStateKey );
			}
		}

	// Use default "Normal" meta state.
	int normalIndex = activeAGPControl ? 2 : 0;
	const OSNumber* normalIndexNum = activeAGPControl ? gIOPPluginTwo : gIOPPluginZero;

	CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::updateMetaState: Entering Normal Mode (%d).\n", normalIndex );

	if ( ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( normalIndex ) ) ) != NULL ) &&
	    ( cacheMetaState( metaStateDict ) ) )
		{
		setMetaState( normalIndexNum );
		return( true );
		}
	else
		{
		// Cannot find a valid meta state.  Log an error and return.

		CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::updateMetaState no valid meta states.\n" );

		return( false );
		}
	}


bool SMU_Neo2_SlotFanCtrlLoop::cacheMetaState( const OSDictionary * metaState )
	{
	const OSNumber*									numInterval;
	const OSNumber*									numOverride;
	UInt32											tempInterval;

	// We have to override cacheMetaState because the absence of output-min or output-max is
	// not a failure.  That information is gathered when the output control registers with
	// the platform plugin.

	// Cache the interval.  The units are in seconds.

	if ( ( numInterval = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopIntervalKey ) ) ) != NULL )
		{
		tempInterval = numInterval->unsigned32BitValue();

		if ( ( tempInterval == 0 ) || ( tempInterval > 300 ) )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::cacheMetaState: Meta state interval out of bounds.\n" );

			return( false );
			}
		}
	else
		{
		CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::cacheMetaState: Missing meta state interval.\n" );

		return( false );
		}

	// If there is an output-override key, flag it.  Otherwise, look for the full set of coefficients,
	// setpoints and output bounds.

	if ( ( numOverride = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopOutputOverrideKey ) ) ) != NULL )
		{
		overrideActive = true;
		outputOverride = numOverride;
		outputOverride->retain();
		}
	else
		{
		const OSData*								dataG_p;
		const OSData*								dataG_d;
		const OSData*								dataG_r;
		const OSNumber*								numInputTarget;
		const OSNumber*								numOutputMin;
		const OSNumber*								numOutputMax;
		const OSNumber*								numHistoryLen;
		UInt32										newHistoryLen;

		// Look for G_p, G_d, G_r, input-target, output-max and output-min.

		if ( ( dataG_p = OSDynamicCast( OSData, metaState->getObject( kIOPPIDCtrlLoopProportionalGainKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::cacheMetaState: Meta state is missing G_p.\n" );

			return( false );
			}

		if ( ( dataG_d = OSDynamicCast( OSData, metaState->getObject( kIOPPIDCtrlLoopDerivativeGainKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::cacheMetaState: Meta state is missing G_p.\n" );

			return( false );
			}

		if ( ( dataG_r = OSDynamicCast( OSData, metaState->getObject( kIOPPIDCtrlLoopResetGainKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::cacheMetaState: Meta state is missing G_r.\n" );

			return( false );
			}

		if ( ( numInputTarget = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopInputTargetKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::cacheMetaState: Meta state is missing input target.\n" );
			}

		// We get output-min and output-max from the output control when it registers with us.  Since we cannot
		// begin managing the PID loop until the output control registers, it is not a failure if there is no
		// output-min or output-max key right now.

		numOutputMin = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopOutputMinKey ) );
		numOutputMax = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopOutputMaxKey ) );

		if ( ( numHistoryLen = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopHistoryLenKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::cacheMetaState: Meta state is missing history-length.\n" );

			return( false );
			}

		overrideActive = false;

		if ( outputOverride != NULL )
			{
			outputOverride->release();
			outputOverride = NULL;
			}

		G_p = *( SInt32 * ) dataG_p->getBytesNoCopy();
		G_d = *( SInt32 * ) dataG_d->getBytesNoCopy();
		G_r = *( SInt32 * ) dataG_r->getBytesNoCopy();

		inputTarget.sensValue = ( SInt32 ) numInputTarget->unsigned32BitValue();

		if ( numOutputMin )
			outputMin = numOutputMin->unsigned32BitValue();

		if ( numOutputMax )
			outputMax = numOutputMax->unsigned32BitValue();

		// Resize history array if necessary.

		newHistoryLen = numHistoryLen->unsigned32BitValue();

		if ( newHistoryLen != historyLen )
			{
			samplePoint*							newHistoryArray;

			newHistoryArray = ( samplePoint * ) IOMalloc( sizeof( samplePoint ) * newHistoryLen );
			bzero( newHistoryArray, sizeof( samplePoint ) * newHistoryLen );

			for ( unsigned int i = 0; ( ( i < historyLen ) && ( i < newHistoryLen ) ); i++ )
				{
				samplePoint*						sample;

				sample = sampleAtIndex( i );

				newHistoryArray[ i ].sample.sensValue = sample->sample.sensValue;
				newHistoryArray[ i ].error.sensValue = sample->error.sensValue;
				}

			IOFree( historyArray, sizeof( samplePoint ) * historyLen );

			historyArray = newHistoryArray;
			historyLen = newHistoryLen;
			latestSample = 0;
			}
		}

	// Set the interval.

	intervalSec = tempInterval;
	clock_interval_to_absolutetime_interval( intervalSec, NSEC_PER_SEC, &interval );

	return( true );
	}

SensorValue	SMU_Neo2_SlotFanCtrlLoop::getAggregateSensorValue( void )
{
	IOPlatformSensor * curInput;
	SensorValue curReading;

	if ( activeAGPControl )
	{
		curInput = agpSensor;
	}
	else
	{
		curInput = inputSensor;
	}

	curReading = curInput->forceAndFetchCurrentValue();
	curInput->setCurrentValue( curReading );

	return ( curReading );
}

ControlValue SMU_Neo2_SlotFanCtrlLoop::calculateNewTarget( void ) const
	{
	SInt64										accum;
	SInt64										dProd;
	SInt64										rProd;
	SInt64										pProd;
	ControlValue								newTarget;
	SInt32										dRaw;
	SInt32										rRaw;
	SInt32										result;
	samplePoint*								latest;

	// If there is an output override, use it.

	if ( overrideActive )
	{
		CTRLLOOP_DLOG( "*** PID *** Override Active\n" );

		newTarget = outputOverride->unsigned32BitValue();

		return( newTarget );
	}

	// Apply the PID algorithm to choose a new control target value.

	latest = sampleAtIndex( 0 );

	if ( ctrlloopState == kIOPCtrlLoopFirstAdjustment )
	{
		result = 0;
	}
	else
	{
		// Calculate the derivative term.

		dRaw = calculateDerivativeTerm().sensValue;

		// Apply the derivative gain.
		// This is 12.20 * 16.16 => 28.36.

		accum = dProd = ( SInt64 ) G_d * ( SInt64 ) dRaw;

		// Calculate the reset term and apply the reset gain.
		// This is 12.20 * 16.16 => 28.36.

		rRaw = calculateIntegralTerm().sensValue;
		rProd = ( SInt64 ) G_r * ( SInt64 ) rRaw; 
		accum += rProd;

		// Calculate the proportional term and apply the proportional gain.
		// This is 12.20 * 16.16 => 28.36.

		pProd = ( SInt64 ) G_p * ( SInt64 ) latest->error.sensValue;
		accum += pProd;

		// Truncate the fractional part.

		accum >>= 36;

		// PCI Fan controls are direct type. AGP controls use a delta from the previous result.

		if ( activeAGPControl )
		{
			result = ( SInt32 ) accum + ( SInt32 ) agpControl->getTargetValue();
		}
		else
		{
			result = ( SInt32 ) accum;
		}
	}

	newTarget = ( UInt32 )( result > 0 ) ? result : 0;

	// Apply any hard limits.

	newTarget = min( newTarget, outputMax );
	newTarget = max( newTarget, outputMin );

#ifdef CTRLLOOP_DEBUG
	{
		const OSString*				tempDesc;

		CTRLLOOP_DLOG( "*** TARGET *** "
					"%s"
					" G_p=%08lX"
					" G_d=%08lX"
					" G_r=%08lX"
					" Ttgt=%08lX (%ld)"
					" T_cur=%08lX"
					" Res=%lu"
					" Out=%lu"
					" Omin=%lu"
					" Omax=%lu"
					" T_err=%08lX"
					" pProd=%016llX"
					" dRaw=%08lX"
					" dProd=%016llX"
					" rRaw=%08lX"
					" rProd=%016llX"
					" ctrlloopstate=%d\n",
					( tempDesc = OSDynamicCast( OSString, infoDict->getObject( kIOPPluginThermalGenericDescKey ) ) ) != NULL ? tempDesc->getCStringNoCopy() : "Unknown CtrlLoop",
					G_p,
					G_d,
					G_r,
					inputTarget.sensValue, inputTarget.sensValue >> 16,
					latest->sample.sensValue,
					result,
					newTarget,
					outputMin,
					outputMax,
					latest->error.sensValue,
					pProd,
					dRaw,
					dProd,
					rRaw,
					rProd,
					ctrlloopState );
	}
#endif // CTRLLOOP_DEBUG

	return( newTarget );
}

void SMU_Neo2_SlotFanCtrlLoop::sendNewTarget( ControlValue newTarget )
{
	if ( activeAGPControl )
	{
		if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
			ctrlloopState == kIOPCtrlLoopDidWake ||
			newTarget != agpControl->getTargetValue() )
		{
			if (agpControl->sendTargetValue( newTarget ))
			{
				agpControl->setTargetValue( newTarget );
			}
			else
			{
				CTRLLOOP_DLOG("SMU_Neo2_SlotFanCtrlLoop::sendNewTarget failed to send AGP target value\n");
			}
		}

		if ( outputControl && outputControl->isRegistered() == kOSBooleanTrue )
		{
			ControlValue pciFanTarget;

			// If the chassis door is open and there is a "safe" fan speed, set the PCI fan
			// to the "safe" speed
			if ( ( platformPlugin->getEnv( gIOPPluginEnvChassisSwitch ) == kOSBooleanFalse ) &&
			     pciFanSafe != 0xFFFF )
			{
				pciFanTarget = pciFanSafe;
			}
			else
			{
				// If we're in AGP Card Control mode, newTarget is an AGP fan speed. Map it
				// onto the PCI fan.
				//
				// the following equation makes the assumption that the min/max for both fans
				// is greater than or equal to zero:
				//
				// ( pci_target - pci_min ) / ( pci_max - pci_min) = ( agp_target - agp_min ) / ( agp_max - agp_min )
		
				pciFanTarget = ( ( pciFanMaxWithAGP - pciFanMinWithAGP ) * ( newTarget - outputMin ) ) / ( outputMax - outputMin ) + pciFanMinWithAGP;
			}
	
			CTRLLOOP_DLOG( "NV40 FAN TARGET: %lu PCI FAN TARGET: %lu\n", newTarget, pciFanTarget );
	
			if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
				ctrlloopState == kIOPCtrlLoopDidWake ||
				pciFanTarget != outputControl->getTargetValue() )
			{
				if (outputControl->sendTargetValue( pciFanTarget ))
				{
					outputControl->setTargetValue( pciFanTarget );
				}
				else
				{
					CTRLLOOP_DLOG("SMU_Neo2_SlotFanCtrlLoop::sendNewTarget failed to send PCI target value\n");
				}
			}
		}

		// Set the control loop state to indicate that we've processed any pending state transitions
		ctrlloopState = kIOPCtrlLoopAllRegistered;
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
				ctrlloopState = kIOPCtrlLoopAllRegistered;
			}
			else
			{
				CTRLLOOP_DLOG("SMU_Neo2_SlotFanCtrlLoop::sendNewTarget failed to send target value\n");
			}
		}
	}
}

void SMU_Neo2_SlotFanCtrlLoop::sensorRegistered( IOPlatformSensor * aSensor )
{
	const OSData * zone;

	// see if this is a dynamically-provided AGP sensor, such as a video card temperature sensor
	if ( aSensor != inputSensor &&
	     ( ( zone = aSensor->getSensorZone() ) != NULL ) &&
		 ( *(UInt32 *)zone->getBytesNoCopy() == kIOPPluginAGPThermalZone ) )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::sensorRegistered got AGP sensor 0x%08lX\n",
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
			CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::sensorRegistered AGP Sensor 0x%08lX has input-target 0x%08lX\n",
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
				CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::sensorRegistered Failed to fetch meta state 2\n" );
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
		CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::sensorRegistered AGP Card Control active\n" );
		activeAGPControl = true;
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
		CTRLLOOP_DLOG("SMU_Neo2_SlotFanCtrlLoop::sensorRegistered starting control algorithm, AGP Control = %s\n",
				activeAGPControl ? "TRUE" : "FALSE" );

		ctrlloopState = kIOPCtrlLoopFirstAdjustment;

		// Ensure that all the meta state parameters are cached.
		updateMetaState();

		// set the deadline
		deadlinePassed();
	}
}

/*!
	@function controlRegistered
	@abstract Extract fan min/max/safe values from the control's registration dictionary and
	start the control loop iteration.
	@discussion When a fan registers, we parse its registration dictionary in search of min,
	max and safe speeds. These speeds are inserted into the pre-defined meta states appropriately,
	and then the deadlinePassed() routine is invoked to start the control loop ticker.
*/

void SMU_Neo2_SlotFanCtrlLoop::controlRegistered( IOPlatformControl* aControl )
{
	const OSData*					zone;
	const OSNumber*					tmpNumber;
	OSArray*						metaStateArray;
	OSDictionary*					safeMetaStateDict;

	// We care about the PCI fan (outputControl) or any control marked as being in the AGP
	// thermal zone. Others are ignored.

	if ( ! ( ( aControl == outputControl ) ||
	     ( ( ( zone = aControl->getControlZone() ) != NULL ) &&
		 ( *(UInt32 *)zone->getBytesNoCopy() == kIOPPluginAGPThermalZone ) ) ) )
	{
		return;
	}

	// Get the Meta State array.

	metaStateArray = OSDynamicCast( OSArray, infoDict->getObject( gIOPPluginThermalMetaStatesKey ) );

	// Populate meta states with information extracted from the registering control.

	if ( aControl == outputControl )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::controlRegistered Handling PCI Fan Control Registration\n" );

		safeMetaStateDict = NULL;	// This is an in-out parameter in populateMetaStates. Set it to
									// NULL here so we can tell whether it's been populated by the
									// subsequent call to populateMetaStates.

		populateMetaStates( aControl,												// the control that's registering
				OSDynamicCast( OSDictionary, metaStateArray->getObject( 0 ) ),		// "Normal" meta state dict
				OSDynamicCast( OSDictionary, metaStateArray->getObject( 1 ) ),		// "Failsafe" meta state dict
				&safeMetaStateDict );

		if ( safeMetaStateDict )
		{
			// replace the Description string in the safe meta state dict
			const OSSymbol* safeDesc = OSSymbol::withCString( "Safe / Chassis Door Ajar" );
			safeMetaStateDict->setObject( kIOPPluginThermalGenericDescKey, safeDesc );
			safeDesc->release();

			// append the Safe meta state to the meta state array, and record its index
			safeMetaStateIndex = metaStateArray->getCount();
			CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::controlRegistered appending safe meta state at index %u\n", safeMetaStateIndex );
			metaStateArray->setObject( safeMetaStateDict );

			// clean up
			safeMetaStateDict->release();
		}
	}
	else	// Handle the AGP control
	{
		OSDictionary*		agpNormalMetaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 2 ) );

		// The PCI fan has a different Min/Max when in AGP Card Control mode. Fetch the min/max from
		// the PList.

		tmpNumber = OSDynamicCast( OSNumber, agpNormalMetaStateDict->getObject( "pci-fan-min" ) );
		pciFanMinWithAGP = tmpNumber->unsigned32BitValue();

		tmpNumber = OSDynamicCast( OSNumber, agpNormalMetaStateDict->getObject( "pci-fan-max" ) );
		pciFanMaxWithAGP = tmpNumber->unsigned32BitValue();

		CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::controlRegistered Handling AGP Fan Control Registration (PCI Min: %u Max: %u)\n",
				pciFanMinWithAGP, pciFanMaxWithAGP );

		safeMetaStateDict = NULL;	// This is an in-out parameter in populateMetaStates. Set it to
									// NULL here so we can tell whether it's been populated by the
									// subsequent call to populateMetaStates.

		populateMetaStates( aControl,												// the control that's registering
				agpNormalMetaStateDict,												// "Normal" meta state dict
				OSDynamicCast( OSDictionary, metaStateArray->getObject( 3 ) ),		// "Failsafe" meta state dict
				&safeMetaStateDict );

		if ( safeMetaStateDict )
		{
			// replace the Description string in the safe meta state dict
			const OSSymbol* safeDesc = OSSymbol::withCString( "Safe / Chassis Door Ajar (AGP Card Control)" );
			safeMetaStateDict->setObject( kIOPPluginThermalGenericDescKey, safeDesc );
			safeDesc->release();

			// append the Safe meta state to the meta state array, and record its index
			agpSafeMetaStateIndex = metaStateArray->getCount();
			CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::controlRegistered appending safe meta state at index %u (AGP)\n", agpSafeMetaStateIndex );
			metaStateArray->setObject( safeMetaStateDict );

			// clean up
			safeMetaStateDict->release();
		}

		// Remember this control

		agpControl = aControl;
	}

	// if we've found both an agp sensor and an agp control, then switch to AGP control mode
	if ( !activeAGPControl && agpSensor && agpControl )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlotFanCtrlLoop::controlRegistered AGP Card Control active\n" );
		activeAGPControl = true;
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
		CTRLLOOP_DLOG("SMU_Neo2_SlotFanCtrlLoop::controlRegistered starting control algorithm, AGP Control = %s\n",
				activeAGPControl ? "TRUE" : "FALSE" );

		ctrlloopState = kIOPCtrlLoopFirstAdjustment;

		// Make sure all the parameters are cached
		updateMetaState();

		// set the deadline
		deadlinePassed();
	}
}

void SMU_Neo2_SlotFanCtrlLoop::populateMetaStates( IOPlatformControl* newControl, OSDictionary* normalMetaStateDict,
		OSDictionary* failsafeMetaStateDict, OSDictionary** safeMetaStateDict )
{
	const OSNumber* tmpNumber;
	ControlValue minSpeed, maxSpeed, safeSpeed;

	// Min output (RPM/PWM) (16-bit integer).  Get from the outputControl's dictionary.

	minSpeed = newControl->getControlMinValue();
	tmpNumber = OSNumber::withNumber( minSpeed, 16 );
	normalMetaStateDict->setObject( kIOPPIDCtrlLoopOutputMinKey, tmpNumber );
	tmpNumber->release();

	// Max output (RPM/PWM) (16-bit integer).  Get from the newControl's dictionary.

	maxSpeed = newControl->getControlMaxValue();
	tmpNumber = OSNumber::withNumber( maxSpeed, 16 );
	normalMetaStateDict->setObject( kIOPPIDCtrlLoopOutputMaxKey, tmpNumber );
	failsafeMetaStateDict->setObject( kIOPPIDCtrlLoopOutputOverrideKey, tmpNumber );
	tmpNumber->release();

	// Safe output (RPM/PWM) (16-bit integer).  Get from the newControl's dictionary.

	// The Safe output mode / meta state is only created / activated on systems that have
	// exposed or user accessible fans that must be slowed when the chassis door is opened.

	// Create a new meta state for Safe mode. If the Safe output speed is greater than the
	// outputMin, then the new meta state will simply be a copy of the "Normal" meta state
	// (index 0) where the output-max is replaced by the Safe output speed. If the Safe output
	// speed is less than or equal to the outputMin, then the new meta state will be a copy
	// of the "Failsafe" meta state where the output-override is replaced by the Safe output
	// speed.

	// A Safe value of 0xFFFF indicates that the safe-value is not populated / does not exist
	// for this control.
	if ( ( safeSpeed = newControl->getControlSafeValue() ) != 0xFFFF )
	{
		// If this is the PCI fan, remember its safe speed in out-of-band storage.
		if ( newControl == outputControl ) pciFanSafe = safeSpeed;

		// Encapsulate the Safe output speed in an OSNumber object (released below)
		tmpNumber = OSNumber::withNumber( safeSpeed, 16 );

		if ( safeSpeed > minSpeed )
		{
			// create a copy of the "Normal" meta state
			*safeMetaStateDict = OSDictionary::withDictionary( normalMetaStateDict );

			// insert the Safe output value in place of the Max output
			(*safeMetaStateDict)->setObject( kIOPPIDCtrlLoopOutputMaxKey, tmpNumber );
		}
		else
		{
			// create a copy of the "Failsafe" meta state
			*safeMetaStateDict = OSDictionary::withDictionary( failsafeMetaStateDict );

			// insert the Safe output value in place of the Override output
			(*safeMetaStateDict)->setObject( kIOPPIDCtrlLoopOutputOverrideKey, tmpNumber );
		}

		// Release the Safe output speed OSNumber object
		tmpNumber->release();
	}
}
