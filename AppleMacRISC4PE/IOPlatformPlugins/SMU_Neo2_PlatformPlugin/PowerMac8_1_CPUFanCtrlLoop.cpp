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
 *  File: $Id: PowerMac8_1_CPUFanCtrlLoop.cpp,v 1.3 2004/08/31 02:35:24 dirty Exp $
 *
 */



#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "PowerMac8_1_CPUFanCtrlLoop.h"


OSDefineMetaClassAndStructors( PowerMac8_1_CPUFanCtrlLoop, SMU_Neo2_CPUFanCtrlLoop )


IOReturn PowerMac8_1_CPUFanCtrlLoop::initPlatformCtrlLoop( const OSDictionary* dict )
	{
	IOReturn							result;

	result = SMU_Neo2_CPUFanCtrlLoop::initPlatformCtrlLoop( dict );

	OSArray*							linearFactors;
	OSDictionary*						factorDict;
	OSData*								factorData;

	linearFactors = OSDynamicCast( OSArray, dict->getObject( "linear-factors" ) );
	factorDict = OSDynamicCast( OSDictionary, linearFactors->getObject( 0 ) );

	factorData = OSDynamicCast( OSData, factorDict->getObject( "scaling" ) );
	scalingFactorC = *( SInt16 * ) factorData->getBytesNoCopy();

	factorData = OSDynamicCast( OSData, factorDict->getObject( "offset" ) );
	constantD = *( SInt16 * ) factorData->getBytesNoCopy();

	// The Q45 CPU fan can be used to cool down other parts of the system.  The CPU fan will run at the
	// maximum of the HD fan (scaled) or it's own control loop.  We need to find the HD fan control loop
	// to be able to query it and find it's speed.

	OSNumber*							linkedCtrlLoopID;

	linkedCtrlLoopID = OSDynamicCast( OSNumber, dict->getObject( "linked-ctrlloop-id" ) );

	if ( ( systemFansCtrlLoop = OSDynamicCast( PowerMac8_1_SystemFansCtrlLoop, platformPlugin->lookupCtrlLoopByID( linkedCtrlLoopID ) ) ) == NULL )
		{
		CTRLLOOP_DLOG( "PowerMac8_1_CPUFanCtrlLoop::initPlatformCtrlLoop: Linked control loop is missing.\n" );

		return( kIOReturnError );
		}

	return( result );
	}


void PowerMac8_1_CPUFanCtrlLoop::sendNewTarget( ControlValue newTarget )
	{
	targetValue = newTarget;

	// To allow the CPU fan to help cool other parts of the system, newTarget = MAX( newTarget, systemFansCtrlLoop->getCtrlLoopTarget() )
	// If the other fans are working hard, but the CPU isn't, the CPU fan will speed up to help the other fans.

	long long							mx;
	ControlValue						systemFansTarget;
	ControlValue						scaledSystemFansTarget;

	// Scale systemFansTarget linearly by scalingFactor (4.12 signed) and constant (16.0 signed).

	systemFansTarget = systemFansCtrlLoop->getCtrlLoopTarget();

	mx = ( systemFansTarget * scalingFactorC );
	scaledSystemFansTarget = ( mx >> 12 ) + constantD;

	newTarget = max( ( SInt16 ) newTarget, ( SInt16 ) scaledSystemFansTarget );

	newTarget = min( ( SInt16 ) newTarget, ( SInt16 ) outputMax );
	newTarget = max( ( SInt16 ) newTarget, ( SInt16 ) outputMin );

	if ( ( ctrlloopState == kIOPCtrlLoopFirstAdjustment ) || ( ctrlloopState == kIOPCtrlLoopDidWake ) ||
		( newTarget != outputControl->getTargetValue() ) )
		{
		if ( outputControl->sendTargetValue( newTarget ) )
			{
			outputControl->setTargetValue( newTarget );
			ctrlloopState = kIOPCtrlLoopAllRegistered;
			}
		else
			{
			CTRLLOOP_DLOG( "PowerMac8_1_CPUFanCtrlLoop::sendNewTarget failed to send target value to first control\n" );
			}

		// Update "ctrlloop-output-at-max" key for Eric to know in the SlewCtrlLoop.

		( void ) platformPlugin->setEnvArray( gIOPPluginEnvCtrlLoopOutputAtMax, this, ( newTarget == outputMax ) );
		}
	}


ControlValue PowerMac8_1_CPUFanCtrlLoop::getCtrlLoopTarget( void ) const
	{
	return( targetValue );
	}
