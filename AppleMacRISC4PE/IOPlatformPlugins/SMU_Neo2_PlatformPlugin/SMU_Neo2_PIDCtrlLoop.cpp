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
 *  File: $Id: SMU_Neo2_PIDCtrlLoop.cpp,v 1.18 2005/07/15 22:01:12 mpontil Exp $
 *
 */



#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "SMU_Neo2_PIDCtrlLoop.h"


OSDefineMetaClassAndStructors( SMU_Neo2_PIDCtrlLoop, IOPlatformPIDCtrlLoop )

void SMU_Neo2_PIDCtrlLoop::free( void )
	{
	if ( linkedControl != NULL )
		{
		linkedControl->release();
		linkedControl = NULL;
		}

	IOPlatformPIDCtrlLoop::free();
	}


IOReturn SMU_Neo2_PIDCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
	{
	IOReturn							result;

	safeMetaStateIndex = (unsigned int) -1;

	outputMin = linkedControlOutputMin = minMinimum = 0x7FFFFFFF;
	outputMax = linkedControlOutputMax = maxMaximum = 0;

	result = IOPlatformPIDCtrlLoop::initPlatformCtrlLoop( dict );

	targetValue = 0;

	// If there is a second entry in control-id array, then we treat it as a control which should be driven
	// exactly the same as the first control.

	OSArray*							controlArray;

	if ( ( controlArray = OSDynamicCast( OSArray, dict->getObject( kIOPPluginThermalControlIDsKey ) ) ) == NULL )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::initPlatformCtrlLoop: control-id array is missing.\n" );

		return( kIOReturnError );
		}

	if ( ( linkedControl = OSDynamicCast( IOPlatformControl, platformPlugin->lookupControlByID( OSDynamicCast( OSNumber, controlArray->getObject( 1 ) ) ) ) ) != NULL )
		{
		linkedControl->retain();
		addControl( linkedControl );
		}

	return( result );
	}


bool SMU_Neo2_PIDCtrlLoop::updateMetaState( void )
	{
	const OSArray*								metaStateArray;
	const OSDictionary*							metaStateDict;

	if ( ( metaStateArray = OSDynamicCast( OSArray, infoDict->getObject( gIOPPluginThermalMetaStatesKey ) ) ) == NULL )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::updateMetaState: No meta state array.\n" );

		return( false );
		}

	// Check for door ajar condition.

	if ( ( safeMetaStateIndex != (unsigned int) -1 ) &&
	     ( platformPlugin->getEnv( gIOPPluginEnvChassisSwitch ) == kOSBooleanFalse ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::updateMetaState: Entering Safe Mode. (Meta State %u)\n", safeMetaStateIndex );

		if ( ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( safeMetaStateIndex ) ) ) != NULL ) &&
		    ( cacheMetaState( metaStateDict ) ) )
			{
			const OSNumber * newState = OSNumber::withNumber( safeMetaStateIndex, 32 );
			CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::updateMetaState: newState is %u\n", newState->unsigned32BitValue() );
			setMetaState( newState );
			newState->release();

			return( true );
			}
		}

	// Check for overtemp condition.

	if ( platformPlugin->envArrayCondIsTrue( gIOPPluginEnvExternalOvertemp ) )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::updateMetaState: Entering Overtemp Mode.\n" );

		if ( ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 1 ) ) ) != NULL ) &&
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

	if ( ( ( metaStateDict = OSDynamicCast( OSDictionary, metaStateArray->getObject( 0 ) ) ) != NULL ) &&
	    ( cacheMetaState( metaStateDict ) ) )
		{
		setMetaState( gIOPPluginZero );
		return( true );
		}
	else
		{
		// Cannot find a valid meta state.  Log an error and return.

		CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::updateMetaState no valid meta states.\n" );

		return( false );
		}
	}


bool SMU_Neo2_PIDCtrlLoop::cacheMetaState( const OSDictionary * metaState )
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
			CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::cacheMetaState: Meta state interval out of bounds.\n" );

			return( false );
			}
		}
	else
		{
		CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::cacheMetaState: Missing meta state interval.\n" );

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
			CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::cacheMetaState: Meta state is missing G_p.\n" );

			return( false );
			}

		if ( ( dataG_d = OSDynamicCast( OSData, metaState->getObject( kIOPPIDCtrlLoopDerivativeGainKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::cacheMetaState: Meta state is missing G_p.\n" );

			return( false );
			}

		if ( ( dataG_r = OSDynamicCast( OSData, metaState->getObject( kIOPPIDCtrlLoopResetGainKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::cacheMetaState: Meta state is missing G_r.\n" );

			return( false );
			}

		if ( ( numInputTarget = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopInputTargetKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::cacheMetaState: Meta state is missing input target.\n" );
			}

		// We get output-min and output-max from the output control when it registers with us.  Since we cannot
		// begin managing the PID loop until the output control registers, it is not a failure if there is no
		// output-min or output-max key right now.

		numOutputMin = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopOutputMinKey ) );
		numOutputMax = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopOutputMaxKey ) );

		if ( ( numHistoryLen = OSDynamicCast( OSNumber, metaState->getObject( kIOPPIDCtrlLoopHistoryLenKey ) ) ) == NULL )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::cacheMetaState: Meta state is missing history-length.\n" );

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


ControlValue SMU_Neo2_PIDCtrlLoop::calculateNewTarget( void ) const
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

		latest = sampleAtIndex( 0 );
		pProd = ( SInt64 ) G_p * ( SInt64 ) latest->error.sensValue;
		accum += pProd;

		// Truncate the fractional part.

		accum >>= 36;

		// Direct controlled fans 

		if ( isDirectControlType )
			{
			result = ( SInt32 ) accum;
			}
		else
			{
			result = ( SInt32 ) accum + ( SInt32 ) targetValue;
			}
		}

	newTarget = ( ControlValue )( ( result > 0 ) ? result : 0 );

	newTarget = min( newTarget, maxMaximum );
	newTarget = max( newTarget, minMinimum );

#if 0
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
#endif

	return( newTarget );
	}


void SMU_Neo2_PIDCtrlLoop::sendNewTarget( ControlValue newTarget )
	{
	UInt32						mainNewTarget = newTarget;
	bool						updateCtrlLoopState = false;

	targetValue = newTarget;

	// Apply any hard limits.

	mainNewTarget = min( mainNewTarget, outputMax );
	mainNewTarget = max( mainNewTarget, outputMin );

	if ( ( ctrlloopState == kIOPCtrlLoopFirstAdjustment ) || ( ctrlloopState == kIOPCtrlLoopDidWake ) ||
		( mainNewTarget != outputControl->getTargetValue() ) )
		{
		if ( outputControl->sendTargetValue( mainNewTarget ) )
			{
			outputControl->setTargetValue( mainNewTarget );
			updateCtrlLoopState |= true;
			}
		else
			{
			CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::sendNewTarget failed to send target value to first control\n" );
			}
		}

	// If there is a linkedControl, then set it to have the same value as outputControl.

	if ( linkedControl )
		{
		UInt32						linkedNewTarget = newTarget;

		// Apply any hard limits.

		linkedNewTarget = min( linkedNewTarget, linkedControlOutputMax );
		linkedNewTarget = max( linkedNewTarget, linkedControlOutputMin );

		if ( ( ctrlloopState == kIOPCtrlLoopFirstAdjustment ) || ( ctrlloopState == kIOPCtrlLoopDidWake ) ||
			( linkedNewTarget != linkedControl->getTargetValue() ) )
			{
			if ( linkedControl->sendTargetValue( linkedNewTarget ) )
				{
				linkedControl->setTargetValue( linkedNewTarget );
				updateCtrlLoopState |= true;
				}
			else
				{
				CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::sendNewTarget failed to send target value to linked control\n" );
				}
			}
		}

	if ( updateCtrlLoopState )
		ctrlloopState = kIOPCtrlLoopAllRegistered;
	}


void SMU_Neo2_PIDCtrlLoop::controlRegistered( IOPlatformControl* aControl )
	{
	if ( aControl == outputControl )
		{
		// Now that the outputControl has registered, ask it for it's min-value and max-value.

		OSArray*						metaStateArray;
		OSDictionary*					metaStateDict;
		OSDictionary*					safeMetaStateDict;
		UInt32							outputSafe;
		OSNumber*						tmpNumber;

		// Record whether this control is a type that wants a delta from it's current value (RPM fans), or
		// requires a direct target value (PWM fans).
		
		isDirectControlType = (outputControl->getControlTypeID() == kIOPControlTypeFanPWM);

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

		// [3765953] Since the primary control is not always registered first, be careful when updating
		// minMinimum or maxMaximum.

		minMinimum = min( minMinimum, outputMin );
		maxMaximum = max( maxMaximum, outputMax );

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
		if ( ( outputSafe = outputControl->getControlSafeValue() ) != 0xFFFF )
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
			CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::controlRegistered appending safe meta state at index %u\n", safeMetaStateIndex );
			metaStateArray->setObject( safeMetaStateDict );

			// clean up
			safeMetaStateDict->release();
		}

		// If everyone's registered, kick off the timer deadline
		if ( inputSensor->isRegistered() == kOSBooleanTrue )
			{
			CTRLLOOP_DLOG( "SMU_Neo2_PIDCtrlLoop::controlRegistered: all registered.\n" );

			ctrlloopState = kIOPCtrlLoopFirstAdjustment;

			// set the deadline

			deadlinePassed();
			}
		}

	if ( ( linkedControl != NULL ) && ( aControl == linkedControl ) )
		{
		// Min output (RPM/PWM) (16-bit integer).  Get it from linkedControl.

		linkedControlOutputMin = linkedControl->getControlMinValue();

		// Max output (RPM/PWM) (16-bit integer).  Get it from linkedControl.

		linkedControlOutputMax = linkedControl->getControlMaxValue();

		minMinimum = min( outputMin, linkedControlOutputMin );
		maxMaximum = max( outputMax, linkedControlOutputMax );
		}
	}
