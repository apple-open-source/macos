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
 *  File: $Id: SMU_Neo2_CPUFanCtrlLoop.cpp,v 1.8 2004/06/25 02:44:18 dirty Exp $
 *
 */



#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "SMU_Neo2_CPUFanCtrlLoop.h"


OSDefineMetaClassAndStructors( SMU_Neo2_CPUFanCtrlLoop, IOPlatformPIDCtrlLoop )


bool SMU_Neo2_CPUFanCtrlLoop::init( void )
	{
	bool								result;

	// We have a requirement that the SlewCtrlLoop must exist before the CPUFanCtrlLoop.  We solve
	// this problem by sorting the CtrlLoopArray such that the SlewCtrlLoop appears earlier than
	// the CPUFanCtrlLoop.

	result = IOPlatformPIDCtrlLoop::init();

	tempHistory[ 0 ].sensValue = tempHistory[ 1 ].sensValue = 0;
	tempIndex = 0;

	return( result );
	}


void SMU_Neo2_CPUFanCtrlLoop::free( void )
	{
	if ( linkedControl != NULL )
		{
		linkedControl->release();
		linkedControl = NULL;
		}

	if ( powerSensor != NULL )
		{
		powerSensor->release();
		powerSensor = NULL;
		}

	IOPlatformPIDCtrlLoop::free();
	}


OSDictionary* SMU_Neo2_CPUFanCtrlLoop::getPIDDataset( const OSDictionary* dict ) const
	{
	unsigned char						buffer[ 4 ];
	const OSData*						tmpData;
	const OSNumber*						tmpNumber;
	OSDictionary*						dataset;

	if ( ( dataset = OSDictionary::withCapacity( 14 ) ) == NULL )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::getPIDDataset: Cannot allocate PID dataset.\n" );

		return( NULL );
		}

	// Proportional gain (G_p) (12.20).

	if ( !gPlatformPlugin->getSDBPartitionData( kCPUDependentThermalPIDParamsPartID, 12, 4, buffer ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::getPIDDataset: Failed to fetch G_p.\n" );

		goto SMU_Neo2_CPUFanCtrlLoop_getPIDDataset_ReleaseDataset;
		}

	tmpData = OSData::withBytes( buffer, 4 );
	dataset->setObject( kIOPPIDCtrlLoopProportionalGainKey, tmpData );
	tmpData->release();

	// Reset gain (G_r) (12.20) -- Actually used as power integral gain.

	if ( !gPlatformPlugin->getSDBPartitionData( kCPUDependentThermalPIDParamsPartID, 16, 4, buffer ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::getPIDDataset: Failed to fetch G_r.\n" );

		goto SMU_Neo2_CPUFanCtrlLoop_getPIDDataset_ReleaseDataset;
		}

	tmpData = OSData::withBytes( buffer, 4 );
	dataset->setObject( kIOPPIDCtrlLoopResetGainKey, tmpData );
	tmpData->release();

	// Derivative gain (G_d) (12.20).

	if ( !gPlatformPlugin->getSDBPartitionData( kCPUDependentThermalPIDParamsPartID, 20, 4, buffer ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::getPIDDataset: Failed to fetch G_d.\n" );

		goto SMU_Neo2_CPUFanCtrlLoop_getPIDDataset_ReleaseDataset;
		}

	tmpData = OSData::withBytes( buffer, 4 );
	dataset->setObject( kIOPPIDCtrlLoopDerivativeGainKey, tmpData );
	tmpData->release();

	// History length (8-bit integer).

	if ( !gPlatformPlugin->getSDBPartitionData( kCPUDependentThermalPIDParamsPartID, 7, 1, buffer ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::getPIDDataset: Failed to fetch history length.\n" );

		goto SMU_Neo2_CPUFanCtrlLoop_getPIDDataset_ReleaseDataset;
		}

	tmpNumber = OSNumber::withNumber( buffer[ 0 ], 8 );
	dataset->setObject( kIOPPIDCtrlLoopHistoryLenKey, tmpNumber );
	tmpNumber->release();

	// Target temperature delta (16.16).  Comes from partition 0x17.

	if ( !gPlatformPlugin->getSDBPartitionData( kCPUDependentThermalPIDParamsPartID, 5, 1, buffer ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::getPIDDataset: Failed to fetch Tmax delta.\n" );

		goto SMU_Neo2_CPUFanCtrlLoop_getPIDDataset_ReleaseDataset;
		}

	tmpNumber = OSNumber::withNumber( buffer[ 0 ] << 16, 32 );
	dataset->setObject( "input-target-delta", tmpNumber );
	tmpNumber->release();

	// Max power (16.16).

	if ( !gPlatformPlugin->getSDBPartitionData( kCPUDependentThermalPIDParamsPartID, 10, 2, buffer ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::getPIDDataset: Failed to fetch max power.\n" );

		goto SMU_Neo2_CPUFanCtrlLoop_getPIDDataset_ReleaseDataset;
		}

	tmpNumber = OSNumber::withNumber( ( *( unsigned short * ) &buffer[ 0 ] ) << 16, 32 );
	dataset->setObject( kIOPPIDCtrlLoopMaxPowerKey, tmpNumber );
	tmpNumber->release();

	// Power adjustment (16.16).

	if ( !gPlatformPlugin->getSDBPartitionData( kCPUDependentThermalPIDParamsPartID, 8, 2, buffer ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::getPIDDataset: Failed to fetch max power adjustment.\n" );

		goto SMU_Neo2_CPUFanCtrlLoop_getPIDDataset_ReleaseDataset;
		}

	tmpNumber = OSNumber::withNumber( ( *( unsigned short * ) &buffer[ 0 ] ) << 16, 32 );
	dataset->setObject( kIOPPIDCtrlLoopMaxPowerAdjustmentKey, tmpNumber );
	tmpNumber->release();

	// The iteration interval is not stored in the SDB.  Set it to 1 second.

	dataset->setObject( kIOPPIDCtrlLoopIntervalKey, gIOPPluginOne );

	return( dataset );


SMU_Neo2_CPUFanCtrlLoop_getPIDDataset_ReleaseDataset:
	dataset->release();
	return( NULL );
	}


IOReturn SMU_Neo2_CPUFanCtrlLoop::initPlatformCtrlLoop( const OSDictionary* dict )
	{
	OSArray*							metaStateArray;
	OSDictionary*						dataset;
	IOReturn							result;

	// Initialize the safeMetaStateIndex to something that indicates it has bot been set

	safeMetaStateIndex = (unsigned int) -1;

	// Create the metastate array from information in the thermal profile and the SDB.

	if ( ( dataset = getPIDDataset( dict ) ) == NULL )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::initPlatformCtrlLoop: Cannot get PID dataset.\n" );

		return( kIOReturnError );
		}

	if ( ( metaStateArray = OSDynamicCast( OSArray, dict->getObject( gIOPPluginThermalMetaStatesKey ) ) ) == NULL )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::initPlatformCtrlLoop: Failed to find meta state array.\n" );

		return( kIOReturnError );
		}

	metaStateArray->replaceObject( 0, dataset );

	result = IOPlatformPIDCtrlLoop::initPlatformCtrlLoop( dict );

	OSArray*							sensorArray;

	if ( ( sensorArray = OSDynamicCast( OSArray, dict->getObject( kIOPPluginThermalSensorIDsKey ) ) ) == NULL )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::initPlatformCtrlLoop: sensor-id array is missing.\n" );

		return( kIOReturnError );
		}

	// The CPU temperature sensor is the first entry in the sensor-id array.
	// The power sensor is the second entry in the sensor-id array.

	// IOPlatformPluginFamily will automatically register the first entry in the sensor-id array, so it
	// is our responsibility to add the power sensor to our control loop.

	if ( ( powerSensor = OSDynamicCast( IOPlatformSensor, platformPlugin->lookupSensorByID( OSDynamicCast( OSNumber, sensorArray->getObject( 1 ) ) ) ) ) == NULL )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::initPlatformCtrlLoop: Power sensor is missing.\n" );

		return( kIOReturnError );
		}

	powerSensor->retain();
	addSensor( powerSensor );

	// If there is a second entry in control-id array, then we treat it as a control which should be driven
	// exactly the same as the first control.

	OSArray*							controlArray;

	if ( ( controlArray = OSDynamicCast( OSArray, dict->getObject( kIOPPluginThermalControlIDsKey ) ) ) == NULL )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::initPlatformCtrlLoop: control-id array is missing.\n" );

		return( kIOReturnError );
		}

	if ( ( linkedControl = OSDynamicCast( IOPlatformControl, platformPlugin->lookupControlByID( OSDynamicCast( OSNumber, controlArray->getObject( 1 ) ) ) ) ) != NULL )
		{
		linkedControl->retain();
		addControl( linkedControl );
		}

	return( result );
	}


bool SMU_Neo2_CPUFanCtrlLoop::updateMetaState( void )
	{
	const OSArray*								metaStateArray;
	const OSDictionary*							metaStateDict;

	if ( ( metaStateArray = OSDynamicCast( OSArray, infoDict->getObject( gIOPPluginThermalMetaStatesKey ) ) ) == NULL )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::updateMetaState: No meta state array.\n" );

		return( false );
		}

	// At this point, check the "cpu-tmax" key to get the current Tmax.  This is published by
	// Eric in the SlewCtrlLoop and is dependent on the operating point.  It is an OSNumber, and
	// in 16.16.

	OSNumber*						tMaxNumber;

	if ( ( tMaxNumber = OSDynamicCast( OSNumber, platformPlugin->getEnv( "cpu-tmax" ) ) ) != NULL )
		{
		inputMax.sensValue = tMaxNumber->unsigned32BitValue();
		}
	else
		{
		// Missing cpu-tmax.  Assume 94 degrees for now.

		IOLog( "SMU_Neo2_CPUFanCtrlLoop: Missing \"cpu-tmax\" key.\n" );
		inputMax.sensValue = 94 << 16;
		}

	// Check for door ajar condition.

	if ( ( safeMetaStateIndex != (unsigned int) -1 ) &&
	     ( platformPlugin->getEnv( gIOPPluginEnvChassisSwitch ) == kOSBooleanTrue ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::updateMetaState: Entering Safe Mode. (Meta State %u)\n", safeMetaStateIndex );

		if ( ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( safeMetaStateIndex ) ) ) != NULL ) &&
		    ( cacheMetaState( metaStateDict ) ) )
			{
			const OSNumber * newState = OSNumber::withNumber( safeMetaStateIndex, 32 );
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::updateMetaState: newState is %u\n", newState->unsigned32BitValue() );
			setMetaState( newState );
			newState->release();

			return( true );
			}
		}

	// Check for overtemp condition.

	if ( platformPlugin->envArrayCondIsTrue( gIOPPluginEnvInternalOvertemp ) ||
		platformPlugin->envArrayCondIsTrue( gIOPPluginEnvExternalOvertemp ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::updateMetaState: Entering overtemp mode.\n" );

		if ( ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 1 ) ) ) == NULL ) &&
			( cacheMetaState( metaStateDict ) ) )
			{
			setMetaState( gIOPPluginOne );

			return( true );
			}
		}

	// Look for forced meta state.

	if ( ( metaStateDict = OSDynamicCast( OSDictionary, infoDict->getObject( gIOPPluginForceCtrlLoopMetaStateKey ) ) ) != NULL )
		{
		if ( cacheMetaState( metaStateDict ) )
			{
			const OSNumber*						newMetaState;

			newMetaState = OSNumber::withNumber( 0xFFFFFFFF, 32 );
			setMetaState( newMetaState );
			newMetaState->release();

			return( true );
			}
		else
			{
			infoDict->removeObject( gIOPPluginForceCtrlLoopMetaStateKey );
			}
		}

	// Use default "Normal" meta state.

	if ( ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 0 ) ) ) != NULL ) &&
		( cacheMetaState( metaStateDict ) ) )
		{
		setMetaState( gIOPPluginZero );
		return( true );
		}
	else
		{
		// Cannot find a valid meta state.  Log an error and return.

		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::updateMetaState: No valid meta state.\n" );

		return( false );
		}
	}


bool SMU_Neo2_CPUFanCtrlLoop::cacheMetaState( const OSDictionary* metaState )
	{
	const OSNumber*									numInterval;
	const OSNumber*									numOverride;
	UInt32											tempInterval;

	// Cache the interval.  The units are in seconds.

	if ( ( numInterval = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopIntervalKey ) ) ) != NULL )
		{
		tempInterval = numInterval->unsigned32BitValue();

		if ( ( tempInterval == 0 ) || ( tempInterval > 300 ) )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::cacheMetaState: Meta state interval out of bounds.\n" );

			return( false );
			}
		}
	else
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::cacheMetaState: Missing meta state interval.\n" );

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
		// Look for G_p, G_d, G_r, input-target, output-max and output-min.

		const OSData*								dataG_p;
		const OSData*								dataG_d;
		const OSData*								dataG_r;
		const OSNumber*								numInputTargetDelta;
		const OSNumber*								numOutputMin;
		const OSNumber*								numOutputMax;
		const OSNumber*								numPowerMax;
		const OSNumber*								numPowerAdj;
		const OSNumber*								numHistoryLen;
		UInt32										newHistoryLen;

		if ( ( dataG_p = OSDynamicCast( OSData, metaState->getObject( kIOPPIDCtrlLoopProportionalGainKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::cacheMetaState: Meta state is missing G_p.\n" );

			return( false );
			}

		if ( ( dataG_d = OSDynamicCast( OSData, metaState->getObject( kIOPPIDCtrlLoopDerivativeGainKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::cacheMetaState: Meta state is missing G_p.\n" );

			return( false );
			}

		if ( ( dataG_r = OSDynamicCast( OSData, metaState->getObject( kIOPPIDCtrlLoopResetGainKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::cacheMetaState: Meta state is missing G_r.\n" );

			return( false );
			}

		if ( ( numInputTargetDelta = OSDynamicCast( OSNumber, metaState->getObject( "input-target-delta" ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::cacheMetaState: Meta state is missing input-target-delta.\n" );

			return( false );
			}

		if ( ( numPowerMax = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopMaxPowerKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::cacheMetaState: Meta state is missing max-power.\n" );

			return( false );
			}

		if ( ( numPowerAdj = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopMaxPowerAdjustmentKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::cacheMetaState: Meta state is missing max-power-adjustment.\n" );

			return( false );
			}

		// We get output-min and output-max from the output control when it registers with us.  Since we cannot
		// begin managing the PID loop until the output control registers, it is not a failure if there is no
		// output-min or output-max key right now.

		numOutputMin = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopOutputMinKey ) );
		numOutputMax = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopOutputMaxKey ) );

		if ( ( numHistoryLen = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopHistoryLenKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::cacheMetaState: Meta state is missing history-length.\n" );

			return( false );
			}

		overrideActive = false;

		if ( outputOverride != NULL )
			{
			outputOverride->release();
			outputOverride = NULL;
			}

		newHistoryLen = numHistoryLen->unsigned8BitValue();

		G_p = *( SInt32 * ) dataG_p->getBytesNoCopy();
		G_d = *( SInt32 * ) dataG_d->getBytesNoCopy();
		G_r = *( SInt32 * ) dataG_r->getBytesNoCopy();

		// The G_r coefficient is the power integral gain.  Since we need to average the power across newHistoryLen samples,
		// we need to reduce G_r by the sample size.  On Q37, we handled this reduction by changing G_r in the MPU EEPROM.
		// On SMU-based systems, it is more appropriate to do the math here.

		G_r = G_r / newHistoryLen;

		// The inputTarget is a constant delta from the inputMax.

		inputTargetDelta.sensValue = ( SInt32 ) numInputTargetDelta->unsigned32BitValue();
		inputTarget.sensValue = inputMax.sensValue - inputTargetDelta.sensValue;

		powerMaxAdj.sensValue = ( ( SInt32 ) numPowerMax->unsigned32BitValue() ) - ( ( SInt32 ) numPowerAdj->unsigned32BitValue() );

		if ( numOutputMin )
			outputMin = numOutputMin->unsigned16BitValue();

		if ( numOutputMax )
			outputMax = numOutputMax->unsigned16BitValue();

		// Resize history array if necessary.

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


void SMU_Neo2_CPUFanCtrlLoop::deadlinePassed( void )
	{
	bool											deadlineAbsolute;
	bool											didSetEnv = false;

	deadlineAbsolute = ( ctrlloopState == kIOPCtrlLoopFirstAdjustment );

	timerCallbackActive = true;

	if ( !acquireSample() )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::deadlinePassed FAILED TO ACQUIRE INPUT SAMPLE!!!\n" );
		}

	// If we changed the environment, the platform plugin will invoke updateMetaState() and
	// adjustControls().  If not, then we just need to call adjustControls().

	if ( !didSetEnv )
		{
		adjustControls();
		}

	// Set the deadline.

	if ( deadlineAbsolute )
		{
		// This is the first time we are setting the deadline.  In order to better stagger
		// the timer callbacks, offset the deadline by 100us * ctrlloopID.

		AbsoluteTime						adjustedInterval;
		const OSNumber*						id;

		id = getCtrlLoopID();

		// 100 * ctrlLoopID -> absolute time format.

		clock_interval_to_absolutetime_interval( 100 * id->unsigned32BitValue(), NSEC_PER_USEC, &adjustedInterval );

		// Add standard interval to produce adjusted interval.

		ADD_ABSOLUTETIME( &adjustedInterval, &interval );
		clock_absolutetime_interval_to_deadline( adjustedInterval, &deadline );
		}
	else
		{
		ADD_ABSOLUTETIME( &deadline, &interval );
		}

	timerCallbackActive = false;
	}


bool SMU_Neo2_CPUFanCtrlLoop::acquireSample( void )
	{
	SensorValue										curTempValue;
	SensorValue										curPowerValue;
	samplePoint*									latest;

	// Set up the temperature history.

	tempIndex = ( tempIndex + 1 ) % 2;

	// Fetch the temperature reading.

	curTempValue = getAggregateSensorValue();

	tempHistory[ tempIndex ].sensValue = curTempValue.sensValue;

	// Move the top of the power array to the next entry.

	if ( latestSample == 0 )
		latestSample = historyLen - 1;
	else
		latestSample -= 1;

	// Get a pointer to the array element where we will store this sample point.

	latest = &historyArray[ latestSample ];

	curPowerValue = powerSensor->fetchCurrentValue();
	powerSensor->setCurrentValue( curPowerValue );

	// Store the sample in the history.

	latest->sample.sensValue = curPowerValue.sensValue;

	// Calculate the error term.

	latest->error.sensValue = powerMaxAdj.sensValue - latest->sample.sensValue;

#if 0
	CTRLLOOP_DLOG( "*** SAMPLE *** InT: 0x%08X CurT: 0x%08X CurP: 0x%08X Error: 0x%08lX\n",
		( unsigned int ) inputTarget.sensValue, ( unsigned int ) curTempValue.sensValue,
		( unsigned int ) curPowerValue.sensValue, latest->error.sensValue );
#endif

	return( true );
	}


ControlValue SMU_Neo2_CPUFanCtrlLoop::calculateNewTarget( void ) const
	{
	SInt64											accum;
	SInt64											dProd;
	SInt64											rProd;
	SInt64											pProd;
	SInt32											pRaw;
	SInt32											dRaw;
	SInt32											rRaw;
	SInt32											result;
	SensorValue										adjInputTarget;
	ControlValue									newTarget;

	// If there is an output override, use it.

	if ( overrideActive )
		{
		CTRLLOOP_DLOG( "*** PID *** Override Active\n" );
		newTarget = outputOverride->unsigned32BitValue();

		return( newTarget );
		}

	// Apply the PID algorithm to choose a new control target value.

	if ( ctrlloopState == kIOPCtrlLoopFirstAdjustment )
		{
		result = 0;
		}
	else
		{
		SensorValue										sVal;

		// Calculate the adjusted target.

		rRaw = calculateIntegralTerm().sensValue;

		// This is 12.20 * 16.16 => 28.36

		rProd = ( SInt64 ) G_r * ( SInt64 ) rRaw;
		sVal.sensValue = ( SInt32 ) ( ( rProd >> 20 ) & 0xFFFFFFFF );
		sVal.sensValue = inputMax.sensValue - sVal.sensValue;

		adjInputTarget.sensValue = inputTarget.sensValue < sVal.sensValue ? inputTarget.sensValue : sVal.sensValue;

		// Do the PID iteration.

		result = ( SInt32 ) outputControl->getTargetValue();

		// Calculate the derivative term and apply the derivative gain.
		// 12.20 * 16.16 => 28.36

		dRaw = calculateDerivativeTerm().sensValue;
		accum = dProd = ( SInt64 ) G_d * ( SInt64 ) dRaw;

		// Calculate the proportional term and apply the proportional gain.
		// 12.20 * 16.16 => 28.36

		pRaw = tempHistory[ tempIndex ].sensValue - adjInputTarget.sensValue;
		pProd = ( SInt64 ) G_p * ( SInt64 ) pRaw;
		accum += pProd;

		// Truncate the fractional part.

		accum >>= 36;

		result += ( SInt32 ) accum;
		}

	newTarget = ( UInt32 )( ( result > 0 ) ? result : 0 );

	// Apply any hard limits.

	if ( newTarget < outputMin )
		{
		newTarget = outputMin;
		}
	else
		{
		if ( newTarget > outputMax )
			{
			newTarget = outputMax;
			}
		}

	// Update "ctrlloop-output-at-max" key for Eric to know in the SlewCtrlLoop.

	( void ) platformPlugin->setEnvArray( gIOPPluginEnvCtrlLoopOutputAtMax, this, ( newTarget == outputMax ) );

#if 0
#ifdef CTRLLOOP_DEBUG
		{
		const OSString*				tempDesc;

		CTRLLOOP_DLOG( "*** TARGET *** "
					"%s"
					" powerSum=%08lX (%ld)"
					" G_power=%08lX"
					" powerProd=%016llX (%lld)"
					" Ttgt=%08lX (%ld)"
					" Tmax=%08lX (%ld)"
					" TtgtAdj=%08lX (%ld)"
					" T_cur=%08lX (%ld)"
					" T_err=%08lX (%ld)"
					" G_p=%08lX"
					" pProd=%016llX (%lld)"
					" dRaw=%08lX (%ld)"
					" G_d=%08lX"
					" dProd=%016llX (%lld)"
					" Accum=%016llX"
					" Res=%lu"
					" Out=%lu\n",
					( tempDesc = OSDynamicCast( OSString, infoDict->getObject( kIOPPluginThermalGenericDescKey ) ) ) != NULL ? tempDesc->getCStringNoCopy() : "Unknown CtrlLoop",
					rRaw, rRaw >> 16,
					G_r,
					rProd, rProd >> 36,
					inputTarget.sensValue, inputTarget.sensValue >> 16,
					inputMax.sensValue, inputMax.sensValue >> 16,
					adjInputTarget.sensValue, adjInputTarget.sensValue >> 16,
					tempHistory[ tempIndex ].sensValue, tempHistory[ tempIndex ].sensValue >> 16,
					pRaw, pRaw >> 16,
					G_p,
					pProd, pProd >> 36,
					dRaw, dRaw >> 16,
					G_d,
					dProd, dProd >> 36,
					accum,
					result,
					newTarget );
		}
#endif // CTRLLOOP_DEBUG
#endif // 0

	return( newTarget );
	}


SensorValue SMU_Neo2_CPUFanCtrlLoop::calculateDerivativeTerm( void ) const
	{
	int												latest;
	int												previous;
	SensorValue										result;

	latest = tempIndex;
	previous = ( tempIndex == 0 ? 1 : 0 );

	// Get the change in the error term over the latest interval.

	result.sensValue = tempHistory[ latest ].sensValue - tempHistory[ previous ].sensValue;

	// Divide by the elapsed time to get the slope.

	result.sensValue = result.sensValue / ( SInt32 ) intervalSec;

	return( result );
	}


void SMU_Neo2_CPUFanCtrlLoop::sendNewTarget( ControlValue newTarget )
	{
	// If the new target value is different, send it to the control

	if ( ctrlloopState == kIOPCtrlLoopFirstAdjustment || ctrlloopState == kIOPCtrlLoopDidWake ||
		newTarget != outputControl->getTargetValue() )
		{
		if ( outputControl->sendTargetValue( newTarget ) )
			{
			outputControl->setTargetValue(newTarget);
			ctrlloopState = kIOPCtrlLoopAllRegistered;
			}
		else
			{
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::sendNewTarget failed to send target value to first control\n" );
			}

		// If there is a linkedControl, then set it to have the same value as outputControl.

		if ( linkedControl )
			{
			if ( linkedControl->sendTargetValue( newTarget ) )
				{
				linkedControl->setTargetValue( newTarget );
				ctrlloopState = kIOPCtrlLoopAllRegistered;
				}
			else
				{
				CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::sendNewTarget failed to send target value to linked control\n" );
				}
			}
		}
	}


void SMU_Neo2_CPUFanCtrlLoop::sensorRegistered( IOPlatformSensor* aSensor )
	{
	if ( ( ( aSensor == powerSensor && inputSensor->isRegistered() == kOSBooleanTrue ) ||
		( aSensor == inputSensor && powerSensor->isRegistered() == kOSBooleanTrue ) ) &&
		( outputControl->isRegistered() == kOSBooleanTrue ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::sensorRegistered - all registered.\n" );

		ctrlloopState = kIOPCtrlLoopFirstAdjustment;

		// set the deadline

		deadlinePassed();
		}
	}


void SMU_Neo2_CPUFanCtrlLoop::controlRegistered( IOPlatformControl* aControl )
	{
	if ( aControl == outputControl )
		{
		// Now that the outputControl has registered, ask it for it's min-value and max-value.

		OSArray*						metaStateArray;
		OSDictionary*					metaStateDict;
		OSDictionary*					safeMetaStateDict;
		UInt32							outputSafe;
		OSNumber*						tmpNumber;

		metaStateArray = OSDynamicCast( OSArray, infoDict->getObject( gIOPPluginThermalMetaStatesKey ) );

		// Get "Normal" meta state dictionary.

		metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 0 ) );

		// Min output (RPM/PWM) (16-bit integer).  Get from the outputControl's dictionary.

		outputMin = outputControl->getControlMinValue();
		tmpNumber = OSNumber::withNumber( outputMin, 16 );
		metaStateDict->setObject( kIOPPIDCtrlLoopOutputMinKey, tmpNumber );
		tmpNumber->release();

		// Max output (RPM/PWM) (16-bit integer).  Get from the outputControl's dictionary.

		outputMax = outputControl->getControlMaxValue();
		tmpNumber = OSNumber::withNumber( outputMax, 16 );
		metaStateDict->setObject( kIOPPIDCtrlLoopOutputMaxKey, tmpNumber );
		tmpNumber->release();

		// Get "Failsafe" meta state dictionary.

		metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 1 ) );

		// Set "output-override" to be "output-max".

		tmpNumber = OSNumber::withNumber( outputMax, 16 );
		metaStateDict->setObject( kIOPPIDCtrlLoopOutputOverrideKey, tmpNumber );
		tmpNumber->release();

		// Safe output (RPM/PWM) (16-bit integer).  Get from the outputControl's dictionary.

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
		if ( platformPlugin->matchPlatformFlags( kIOPPluginFlagEnableSafeModeHack ) &&
		     ( ( outputSafe = outputControl->getControlSafeValue() ) != 0xFFFF ) )
		{
			// Encapsulate the Safe output speed in an OSNumber object (released below)
			tmpNumber = OSNumber::withNumber( outputSafe, 16 );

			if (outputSafe > outputMin)
			{
				// create a copy of the "Normal" meta state
				metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 0 ) );
				safeMetaStateDict = OSDictionary::withDictionary( metaStateDict );

				// insert the Safe output value in place of the Max output
				safeMetaStateDict->setObject( kIOPPIDCtrlLoopOutputMaxKey, tmpNumber );
			}
			else
			{
				// create a copy of the "Failsafe" meta state
				metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 1 ) );
				safeMetaStateDict = OSDictionary::withDictionary( metaStateDict );

				// insert the Safe output value in place of the Override output
				safeMetaStateDict->setObject( kIOPPIDCtrlLoopOutputOverrideKey, tmpNumber );
			}

			// Release the Safe output speed OSNumber object
			tmpNumber->release();

			// replace the Description string in the safe meta state dict
			const OSSymbol* safeDesc = OSSymbol::withCString( "Safe / Chassis Door Ajar" );
			safeMetaStateDict->setObject( kIOPPluginThermalGenericDescKey, safeDesc );
			safeDesc->release();

			// append the Safe meta state to the meta state array, and record it's index
			safeMetaStateIndex = metaStateArray->getCount();
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::controlRegistered appending safe meta state at index %u\n", safeMetaStateIndex );
			metaStateArray->setObject( safeMetaStateDict );

			// clean up
			safeMetaStateDict->release();
		}

		// If everyone's registered, kick off the timer deadline
		if ( ( inputSensor->isRegistered() == kOSBooleanTrue ) && ( powerSensor->isRegistered() == kOSBooleanTrue ) )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_CPUFanCtrlLoop::controlRegistered - all registered.\n" );

			ctrlloopState = kIOPCtrlLoopFirstAdjustment;

			// set the deadline

			deadlinePassed();
			}
		}
	}
