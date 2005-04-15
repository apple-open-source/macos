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
 *  File: $Id: PowerMac8_1_SystemFansCtrlLoop.cpp,v 1.4 2005/02/18 23:05:20 dirty Exp $
 *
 */



#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "PowerMac8_1_SystemFansCtrlLoop.h"


OSDefineMetaClassAndStructors( PowerMac8_1_SystemFansCtrlLoop, SMU_Neo2_PIDCtrlLoop )

IOReturn PowerMac8_1_SystemFansCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
	{
	IOReturn							result;

	result = SMU_Neo2_PIDCtrlLoop::initPlatformCtrlLoop( dict );

	cpuFanCtrlLoop = NULL;

	OSArray*							linearFactors;
	OSDictionary*						factorDict;
	OSData*								factorData;

	linearFactors = OSDynamicCast( OSArray, dict->getObject( "linear-factors" ) );

	factorDict = OSDynamicCast( OSDictionary, linearFactors->getObject( 0 ) );

	factorData = OSDynamicCast( OSData, factorDict->getObject( "scaling" ) );
	scalingFactorA = *( SInt16 * ) factorData->getBytesNoCopy();

	factorData = OSDynamicCast( OSData, factorDict->getObject( "offset" ) );
	constantB = *( SInt16 * ) factorData->getBytesNoCopy();

	factorDict = OSDynamicCast( OSDictionary, linearFactors->getObject( 1 ) );

	factorData = OSDynamicCast( OSData, factorDict->getObject( "scaling" ) );
	scalingFactorE = *( SInt16 * ) factorData->getBytesNoCopy();

	factorData = OSDynamicCast( OSData, factorDict->getObject( "offset" ) );
	constantF = *( SInt16 * ) factorData->getBytesNoCopy();

	linkedCtrlLoopID = OSDynamicCast( OSNumber, dict->getObject( "linked-ctrlloop-id" ) );
	linkedCtrlLoopID->retain();

	return( result );
	}


ControlValue PowerMac8_1_SystemFansCtrlLoop::calculateNewTarget( void ) const
	{
	ControlValue								newTarget;

	newTarget = SMU_Neo2_PIDCtrlLoop::calculateNewTarget();

	if ( linkedControl )
		{
		// Apply any hard limits.

		newTarget = min( newTarget, linkedControlOutputMax );
		newTarget = max( newTarget, linkedControlOutputMin );
		}

	return( newTarget );
	}


void PowerMac8_1_SystemFansCtrlLoop::sendNewTarget( ControlValue newTarget )
	{
	bool							updateCtrlLoopState = false;

	targetValue = newTarget;

	if ( linkedControl &&
		( ( ctrlloopState == kIOPCtrlLoopFirstAdjustment ) || ( ctrlloopState == kIOPCtrlLoopDidWake ) ||
		( newTarget != linkedControl->getTargetValue() ) ) )
		{
		if ( linkedControl->sendTargetValue( newTarget ) )
			{
			linkedControl->setTargetValue( newTarget );
			updateCtrlLoopState |= true;
			}
		else
			{
			CTRLLOOP_DLOG( "PowerMac8_1_SystemFansCtrlLoop::sendNewTarget failed to send target value to linked control\n" );
			}
		}

	ControlValue						scaledNewTarget;
	ControlValue						pinnedScaledNewTarget;
	ControlValue						scaledCPUFanTarget = 0;
	ControlValue						cpuFanTarget = 0;

	// Try and find the PowerMac8_1_CPUFanCtrlLoop here...

	if ( !cpuFanCtrlLoop )
		{
		if ( ( cpuFanCtrlLoop = OSDynamicCast( PowerMac8_1_CPUFanCtrlLoop, platformPlugin->lookupCtrlLoopByID( linkedCtrlLoopID ) ) ) != NULL )
			{
			linkedCtrlLoopID->release();
			linkedCtrlLoopID = NULL;
			}
		}

	long long							mx;

	mx = ( newTarget * scalingFactorA );
	scaledNewTarget = ( mx >> 12 ) + constantB;

	if ( cpuFanCtrlLoop != NULL )
		{
		// Scale cpuFanTarget linearly by scalingFactor (4.12 signed) and constant (16.0 signed).

		cpuFanTarget = cpuFanCtrlLoop->getCtrlLoopTarget();

		mx = ( cpuFanTarget * scalingFactorE );
		scaledCPUFanTarget = ( mx >> 12 ) + constantF;
		}

	pinnedScaledNewTarget = max( ( SInt16 ) scaledNewTarget, ( SInt16 ) scaledCPUFanTarget );

	pinnedScaledNewTarget = min( ( SInt16 ) pinnedScaledNewTarget, ( SInt16 ) outputMax );
	pinnedScaledNewTarget = max( ( SInt16 ) pinnedScaledNewTarget, ( SInt16 ) outputMin );

	if ( ( ctrlloopState == kIOPCtrlLoopFirstAdjustment ) || ( ctrlloopState == kIOPCtrlLoopDidWake ) ||
		( pinnedScaledNewTarget != outputControl->getTargetValue() ) )
		{
		if ( outputControl->sendTargetValue( pinnedScaledNewTarget ) )
			{
			outputControl->setTargetValue( pinnedScaledNewTarget );
			updateCtrlLoopState |= true;
			}
		else
			{
			CTRLLOOP_DLOG( "PowerMac8_1_SystemFansCtrlLoop::sendNewTarget failed to send target value to first control\n" );
			}
		}

	if ( updateCtrlLoopState )
		ctrlloopState = kIOPCtrlLoopAllRegistered;
	}


ControlValue PowerMac8_1_SystemFansCtrlLoop::getCtrlLoopTarget( void ) const
	{
	return( targetValue );
	}
