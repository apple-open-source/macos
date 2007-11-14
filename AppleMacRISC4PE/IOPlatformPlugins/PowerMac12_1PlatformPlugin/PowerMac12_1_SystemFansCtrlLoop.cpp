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
 *  File: $Id: PowerMac12_1_SystemFansCtrlLoop.cpp,v 1.3 2005/09/09 18:35:56 mpontil Exp $
 *
 */



#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "PowerMac12_1_SystemFansCtrlLoop.h"
#include "PowerMac12_1VirtualControl.h"

#define super SMU_Neo2_PIDCtrlLoop
OSDefineMetaClassAndStructors( PowerMac12_1_SystemFansCtrlLoop, SMU_Neo2_PIDCtrlLoop )

SensorValue PowerMac12_1_SystemFansCtrlLoop::getAggregateSensorValue( void )
{
	SensorValue aggValue;

	aggValue = inputSensor->forceAndFetchCurrentValue();
	inputSensor->setCurrentValue( aggValue );

	// If the value is not acceptable use the old value:
	if ( aggValue.sensValue < 0 )
	{
		unsigned int previousIndex;

		if (latestSample == historyLen - 1)
			previousIndex = 0;
		else
			previousIndex = latestSample + 1;

		IOLog("PowerMac12_1_SystemFansCtrlLoop::getAggregateSensorValue using previous value (ci=%d pi=%d cv=%ld pv=%ld\n",
			latestSample, previousIndex, aggValue.sensValue>>16 , historyArray[ previousIndex ].sample.sensValue>>16);

		aggValue = historyArray[ previousIndex ].sample;
	}

	return( aggValue );
}

/*
	IMPORTANT NOTE:
	the following PowerMac12_1_SystemFansCtrlLoop::calculateNewTarget is a cut & paste
	of the SMU_Neo2_PIDCtrlLoop::calculateNewTarget(), no code changed except for:
	
	result = ( SInt32 ) accum + ( SInt32 ) targetValue;
	
	which became 
	
	result = ( SInt32 ) accum + outputControl->getTargetValue();
*/

ControlValue PowerMac12_1_SystemFansCtrlLoop::calculateNewTarget( void ) const
	{
	SInt64										accum;
	SInt64										dProd;
	SInt64										rProd;
	SInt64										pProd;
	ControlValue								newTarget;
	SInt32										dRaw;
	SInt32										rRaw;
	SInt32										result;
	samplePoint*								latest = NULL;

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
			//result = ( SInt32 ) accum + ( SInt32 ) targetValue; // BEFORE
			result = ( SInt32 ) accum +  ( SInt32 ) outputControl->getTargetValue(); // AFTER
			}
		}

	newTarget = ( ControlValue )( ( result > 0 ) ? result : 0 );

	newTarget = min( newTarget, maxMaximum );
	newTarget = max( newTarget, minMinimum );

#if 0
#if 1 // #ifdef CTRLLOOP_DEBUG
	{
		const OSString*				tempDesc = OSDynamicCast( OSString, infoDict->getObject( kIOPPluginThermalGenericDescKey ) );

		if ( tempDesc && latest )
		{
		IOLog( "*** TARGET *** "
					"%s"
					" G_p=%08lX"
					" G_d=%08lX"
					" G_r=%08lX"
					" Ttgt=%08lX (%ld)"
					" T_cur=%08lX (%ld)"
					" Pres=%lu Res=%lu"
					" Out=%lu"
					" Omin=%lu"
					" Omax=%lu"
					" T_err=%08lX (%ld)"
					" pProd=%016llX"
					" dRaw=%08lX"
					" dProd=%016llX"
					" rRaw=%08lX"
					" rProd=%016llX"
					" ctrlloopstate=%d\n",
					tempDesc->getCStringNoCopy(),
					G_p,
					G_d,
					G_r,
					inputTarget.sensValue, inputTarget.sensValue >> 16,
					latest->sample.sensValue, latest->sample.sensValue >> 16,
					outputControl->getTargetValue(), result,
					newTarget,
					outputMin,
					outputMax,
					latest->error.sensValue, latest->error.sensValue >> 16,
					pProd,
					dRaw,
					dProd,
					rRaw,
					rProd,
					ctrlloopState );
			}
	}
#endif // CTRLLOOP_DEBUG
#endif

	return( newTarget );
	}


void PowerMac12_1_SystemFansCtrlLoop::sendNewTarget( ControlValue newTarget )
	{
	UInt32						mainNewTarget = newTarget;
	PowerMac12_1VirtualControl *tmpControl = OSDynamicCast( PowerMac12_1VirtualControl, outputControl);
	
	targetValue = newTarget;

	// const OSString*tempDesc; IOLog("%s sendNewTarget( %ld ) - %ld \n", ( tempDesc = OSDynamicCast( OSString, infoDict->getObject( kIOPPluginThermalGenericDescKey ) ) ) != NULL ? tempDesc->getCStringNoCopy() : "Unknown CtrlLoop", newTarget , outputControl->getTargetValue() );
	
	// Apply any hard limits.

	mainNewTarget = min( ( SInt16 ) mainNewTarget, ( SInt16 ) outputMax );
	mainNewTarget = max( ( SInt16 ) mainNewTarget, ( SInt16 ) outputMin );

	tmpControl->setTargetValue( mainNewTarget , this , mainNewTarget);
	ctrlloopState = kIOPCtrlLoopAllRegistered;
	}
	
void PowerMac12_1_SystemFansCtrlLoop::controlRegistered( IOPlatformControl* aControl )
	{
	PowerMac12_1VirtualControl *tmpControl = OSDynamicCast( PowerMac12_1VirtualControl, outputControl);
	
	if  ( ( tmpControl != NULL ) && ( aControl == tmpControl->targetControl() ) )
		super::controlRegistered( outputControl );
	else
		super::controlRegistered( aControl );
	}
