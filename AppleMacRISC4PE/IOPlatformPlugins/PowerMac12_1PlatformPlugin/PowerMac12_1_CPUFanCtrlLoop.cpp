/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: PowerMac12_1_CPUFanCtrlLoop.cpp,v 1.4 2005/07/22 22:27:13 mpontil Exp $
 *
 */



#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "PowerMac12_1_CPUFanCtrlLoop.h"
#include "PowerMac12_1VirtualControl.h"

#define super	SMU_Neo2_CPUFanCtrlLoop
OSDefineMetaClassAndStructors( PowerMac12_1_CPUFanCtrlLoop, SMU_Neo2_CPUFanCtrlLoop )

// The follwowing lets me override the SMU settings with the ones in the plist.
// I need it in EVT bringup. Also it may be useful later. Once SMU stores values
// that we feel are "right" we can remove the metastate 0 from the plist
IOReturn PowerMac12_1_CPUFanCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
{
	OSArray			*array = OSDynamicCast(OSArray, dict->getObject(gIOPPluginThermalMetaStatesKey) );
	OSDictionary	*originalMetastate = OSDynamicCast(OSDictionary, array->getObject( 0 ) );
	
	if ( originalMetastate )
	{
		// Makes sure this does not go away:
		originalMetastate->retain();
	}
	
	IOReturn		result = super::initPlatformCtrlLoop(dict);
	
	// Now gets the new metstate:
	OSDictionary	*smuMetastate = OSDynamicCast(OSDictionary, array->getObject( 0 ) );
	
	// And corrects it with the numbers in the plist:
	if ( originalMetastate )
	{
		if ( result == kIOReturnSuccess )
		{
			// Now overrides the smu metaastate with the content of the plist metastate:
			OSDictionary	*smuMetastate = OSDynamicCast(OSDictionary, array->getObject( 0 ) );
			OSCollectionIterator * iter = OSCollectionIterator::withCollection( originalMetastate );
			
			if ( iter != NULL )
			{
				OSSymbol * sym;

				while ( ( sym = (OSSymbol *)iter->getNextObject() ) )
				{
					// Gets the new value
					OSObject *newValue = originalMetastate->getObject( sym );
				
					// And  replaces the SMU one with this:
					smuMetastate->setObject( sym, newValue );
					
					//IOLog("PowerMac12_1_CPUFanCtrlLoop::initPlatformCtrlLoop Set Object %s to %s\n", sym->getCStringNoCopy(), newValue->getMetaClass()->getClassName());
				}
			}
			else
				IOLog("PowerMac12_1_CPUFanCtrlLoop::initPlatformCtrlLoop could not allocate iterator\n");
		}
		else

			IOLog("PowerMac12_1_CPUFanCtrlLoop::initPlatformCtrlLoop result != kIOReturnSuccess\n");

		// Do not need it anymore
		originalMetastate->release();
	}
	else
		IOLog(" PowerMac12_1_CPUFanCtrlLoop::initPlatformCtrlLoop Missing original metastate\n");

	// And finally add the "weighted average" parameter:
	OSNumber *tmpNumber = OSNumber::withNumber( kWeightedAverageDefault, 16 );
	if ( tmpNumber )
	{
		smuMetastate->setObject( kWeightedAverageKey , tmpNumber );
		tmpNumber->release();
	}

	// set the initial meta-state to zero -- subclasses should change as needed
	setMetaState(gIOPPluginZero);
	
	return result;
}

// I need  this method to aallow changes on the "weighted average" parameter:
bool PowerMac12_1_CPUFanCtrlLoop::cacheMetaState( const OSDictionary* metaState )
{
	bool superSuccedes = super::cacheMetaState( metaState );

	if ( superSuccedes )
		{
		const OSNumber*								numWeightedAverage;

		if ( ( numWeightedAverage = OSDynamicCast( OSNumber, metaState->getObject( kWeightedAverageKey ) ) ) == NULL )
			{
			mWeightedAverage = kWeightedAverageDefault;
			}
		else
			{
			mWeightedAverage = numWeightedAverage->unsigned16BitValue();
			}
		}
	
	return superSuccedes;
}

/*
	IMPORTANT NOTE:
	the following PowerMac12_1_CPUFanCtrlLoop::calculateNewTarget is a cut & paste
	of the SMU_Neo2_CPUFanCtrlLoop::calculateNewTarget(), no code changed except for:
	
	result = ( SInt32 ) targetValue;
	
	which became 
	
	result = ( SInt32 ) outputControl->getTargetValue();
*/

ControlValue PowerMac12_1_CPUFanCtrlLoop::calculateNewTarget( void ) const
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
	SensorValue										lastIntegralTerm;

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

		lastIntegralTerm.sensValue = rRaw = calculateIntegralTerm().sensValue;
		
		// This is 12.20 * 16.16 => 28.36

		rProd = ( SInt64 ) G_r * ( SInt64 ) rRaw;

		sVal.sensValue = inputMax.sensValue - ( SInt32 ) ( ( rProd >> 20 ) & 0xFFFFFFFF );

		adjInputTarget.sensValue = min( inputTarget.sensValue, sVal.sensValue );

		// Do the PID iteration.

		//result = ( SInt32 ) targetValue; /// BEFORE
		result = ( SInt32 ) outputControl->getTargetValue(); // AFTER
		
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

	newTarget = ( ControlValue )( ( result > 0 ) ? result : 0 );

	newTarget = min( newTarget, maxMaximum );
	newTarget = max( newTarget, minMinimum );

#if 0
#if 1 // #ifdef CTRLLOOP_DEBUG
	{
		const OSString*				tempDesc;
		
		IOLog("OldValue %ld\n", outputControl->getTargetValue());

		IOLog( "*** TARGET *** "
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
					" Out=%lu (%ld %ld)\n",
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
					newTarget , minMinimum, maxMaximum);
		}
#endif // CTRLLOOP_DEBUG
#endif // 0

	return( newTarget );
	}


void PowerMac12_1_CPUFanCtrlLoop::sendNewTarget( ControlValue newTarget )
	{
	UInt32						mainNewTarget = newTarget;
	bool						outputAtMax = false;
	PowerMac12_1VirtualControl *tmpControl = OSDynamicCast( PowerMac12_1VirtualControl, outputControl);
	
	targetValue = newTarget;

	// const OSString*tempDesc; IOLog("%s sendNewTarget( %ld ) - %ld \n", ( tempDesc = OSDynamicCast( OSString, infoDict->getObject( kIOPPluginThermalGenericDescKey ) ) ) != NULL ? tempDesc->getCStringNoCopy() : "Unknown CtrlLoop", newTarget , outputControl->getTargetValue() );

	// Apply any hard limits.

	mainNewTarget = min( ( SInt16 ) mainNewTarget, ( SInt16 ) outputMax );
	mainNewTarget = max( ( SInt16 ) mainNewTarget, ( SInt16 ) outputMin );
	
	if ( mainNewTarget == outputMax )
		{
		outputAtMax |= true;
		}

	tmpControl->setTargetValue( mainNewTarget , this,  mainNewTarget);
	( void ) platformPlugin->setEnvArray( gIOPPluginEnvCtrlLoopOutputAtMax, this, outputAtMax );
	ctrlloopState = kIOPCtrlLoopAllRegistered;
	}
	
void PowerMac12_1_CPUFanCtrlLoop::controlRegistered( IOPlatformControl* aControl )
	{
	PowerMac12_1VirtualControl *tmpControl = OSDynamicCast( PowerMac12_1VirtualControl, outputControl);
	
	if  ( ( tmpControl != NULL ) && ( aControl == tmpControl->targetControl() ) )
		super::controlRegistered( outputControl );
	else
		super::controlRegistered( aControl );
	}
	

SensorValue PowerMac12_1_CPUFanCtrlLoop::averagePower()
{
	// NOTE: the specs need the average power of the CPU,the integal term however
	// uses the error so we need to redo our math here:
	SensorValue accum = { 0 };
	
	if ( ctrlloopState != kIOPCtrlLoopNotReady )
	{
		SensorValue lastPowerSensorReading = { historyArray[ latestSample ].sample.sensValue };

		// Bob's Magic Formula : new average = old average - (old average / 45) + (current power / 45)
		accum.sensValue = mPreviousWeightedAverage.sensValue - ( mPreviousWeightedAverage.sensValue / mWeightedAverage ) + ( lastPowerSensorReading.sensValue / mWeightedAverage );

		CTRLLOOP_DLOG("PowerReading %ld.%2ld (%ld) OldValue %ld.%2ld (%ld) NewValue %ld.%2ld (%ld) (Parameter %d)\n",
		lastPowerSensorReading.sensValue >> 16 , lastPowerSensorReading.sensValue & 0x0000FFFF , lastPowerSensorReading.sensValue,
		mPreviousWeightedAverage.sensValue >> 16 , mPreviousWeightedAverage.sensValue & 0x0000FFFF , mPreviousWeightedAverage.sensValue,
		accum.sensValue >> 16, accum.sensValue & 0x0000FFFF, accum.sensValue,
		mWeightedAverage);

		// Record the new value for the next iteration:
		mPreviousWeightedAverage.sensValue = accum.sensValue;
	}
	
	return(accum);
}
