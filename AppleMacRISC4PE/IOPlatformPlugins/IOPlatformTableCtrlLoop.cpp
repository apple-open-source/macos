/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: IOPlatformTableCtrlLoop.cpp,v $
//		Revision 1.5  2003/07/16 02:02:09  eem
//		3288772, 3321372, 3328661
//		
//		Revision 1.4  2003/07/08 04:32:49  eem
//		3288891, 3279902, 3291553, 3154014
//		
//		Revision 1.3  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.2.2.6  2003/06/06 08:17:56  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.2.2.5  2003/06/01 14:52:51  eem
//		Most of the PID algorithm is implemented.
//		
//		Revision 1.2.2.4  2003/05/29 03:51:34  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.2.2.3  2003/05/26 10:07:15  eem
//		Fixed most of the bugs after the last cleanup/reorg.
//		
//		Revision 1.2.2.2  2003/05/23 06:36:57  eem
//		More registration notification stuff.
//		
//		Revision 1.2.2.1  2003/05/22 01:31:04  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.2  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.1.2.2  2003/05/17 12:55:37  eem
//		Active fan control works on RPM channels!!!!!!
//		
//		Revision 1.1.2.1  2003/05/16 07:08:45  eem
//		Table-lookup active fan control working with this checkin.
//		
//
//

#include "IOPlatformPlugin.h"
#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformStateSensor.h"
#include "IOPlatformControl.h"
#include "IOPlatformTableCtrlLoop.h"

#define super IOPlatformCtrlLoop
OSDefineMetaClassAndStructors( IOPlatformTableCtrlLoop, IOPlatformCtrlLoop )

bool IOPlatformTableCtrlLoop::init( void )
{
	if (!super::init()) return(false);

	inputSensor = NULL;
	outputControl = NULL;

	return(true);
}

void IOPlatformTableCtrlLoop::free( void )
{
	if (inputSensor) { inputSensor->release(); inputSensor = NULL; }
	if (outputControl) { outputControl->release(); outputControl = NULL; }

	super::free();
}

IOReturn IOPlatformTableCtrlLoop::initPlatformCtrlLoop( const OSDictionary *dict)
{
	IOReturn status;
	OSArray * array;

	status = super::initPlatformCtrlLoop(dict);

	// assume that the first listed control is the one we want
	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL ||
		(outputControl = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(0)) )) == NULL)
	{
		CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::initPlatformCtrlLoop no control ID!!\n");
		goto failNoControl;
	}

	outputControl->retain();
	addControl( outputControl );

	// assume that the first listed sensor is the one we want
	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalSensorIDsKey))) == NULL ||
		(inputSensor = OSDynamicCast(IOPlatformStateSensor, platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(0)) ))) == NULL)
	{
		CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::initPlatformCtrlLoop no sensor ID!!\n");
		goto failNoSensor;
	}

	inputSensor->retain();
	addSensor( inputSensor );

	if (!updateMetaState())
	{
		CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::initPlatformCtrlLoop NO VALID META STATES!!\n");
		status = kIOReturnBadArgument;
	}

	return status;

failNoSensor:
	removeControl( outputControl );
	outputControl->release();

failNoControl:

	return(kIOReturnError);
}

bool IOPlatformTableCtrlLoop::updateMetaState( void )
{
	const OSArray * metaStateArray;
	const OSNumber * newMetaState;
	OSDictionary * metaStateDict;
	
	// point to the lookup table for this meta-state
	if ((metaStateArray = OSDynamicCast(OSArray, infoDict->getObject(gIOPPluginThermalMetaStatesKey))) == NULL)
	{
		CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::updateMetaState NO META STATE ARRAY!!\n");
		return(false);
	}
	 
	// Check for overtemp condition
//	if ((platformPlugin->envArrayCondIsTrue(gIOPPluginEnvInternalOvertemp)) ||
//	    (platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp)))
	if (platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp))
	{
		CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::updateMetaState Entering Overtemp Mode\n");

		if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(1))) != NULL &&
		    (lookupTable = OSDynamicCast(OSArray, metaStateDict->getObject(kIOPTCLLookupTableKey))) != NULL)
		{
			// successfully entered overtemp mode
			setMetaState( gIOPPluginOne );
			return(true);
		}
		else
		{
			CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::updateMetaState Overtemp Mode Failed!\n");
		}
	}

	// Look for forced meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, infoDict->getObject(gIOPPluginForceCtrlLoopMetaStateKey))) != NULL)
	{
		if ((lookupTable = OSDynamicCast(OSArray, metaStateDict->getObject(kIOPTCLLookupTableKey))) != NULL)
		{
			CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::updateMetaState using forced meta state\n");
			newMetaState = OSNumber::withNumber( 0xFFFFFFFF, 32 );
			setMetaState( newMetaState );
			newMetaState->release();
			return(true);
		}
		else
		{
			CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::updateMetaState forced meta state is invalid, removing...\n");
			infoDict->removeObject(gIOPPluginForceCtrlLoopMetaStateKey);
		}
	}

	// Use default "Normal" meta state
	if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(0))) != NULL &&
		(lookupTable = OSDynamicCast(OSArray, metaStateDict->getObject(kIOPTCLLookupTableKey))) != NULL)
	{
		//CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::updateMetaState use meta state zero\n");
		setMetaState( gIOPPluginZero );
		return(true);
	}
	else
	{
		// can't find a valid meta state, nothing we can really do except log an error
		CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::updateMetaState no valid meta states!\n");
		return(false);
	}
}

void IOPlatformTableCtrlLoop::adjustControls( void )
{
	const OSNumber * state, * newTarget;

	if (ctrlloopState == kIOPCtrlLoopNotReady)
	{
		CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::adjustControls some entities not yet registered\n");
		return;
	}

	// Look up the sensor's state in the lookup table
	if ((state = inputSensor->getSensorState()) == NULL)
	{
		CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::adjustControls sensor current state unknown\n");
		return;
	}

	if ((newTarget = OSDynamicCast(OSNumber, lookupTable->getObject( state->unsigned32BitValue() ))) == NULL)
	{
		CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::adjustControls no target for state %u\n", state->unsigned32BitValue() );
		return;
	}

	// If the new target value is different, send it to the control
	if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
	    ctrlloopState == kIOPCtrlLoopDidWake ||
	    !newTarget->isEqualTo( outputControl->getTargetValue() ))
	{
		if (outputControl->sendTargetValue( newTarget ))
		{
			outputControl->setTargetValue(newTarget);
			ctrlloopState = kIOPCtrlLoopAllRegistered;
		}
		else
		{
			CTRLLOOP_DLOG("IOPlatformTableCtrlLoop::adjustControls failed to send target value\n");
		}
	}
}

void IOPlatformTableCtrlLoop::sensorRegistered( IOPlatformSensor * aSensor )
{
	if (aSensor == inputSensor &&
	    outputControl->isRegistered() == kOSBooleanTrue)
	{
		ctrlloopState = kIOPCtrlLoopFirstAdjustment;
		adjustControls();
	}
}

void IOPlatformTableCtrlLoop::controlRegistered( IOPlatformControl * aControl )
{
	if (aControl == outputControl &&
	    inputSensor->isRegistered() == kOSBooleanTrue)
	{
		ctrlloopState = kIOPCtrlLoopFirstAdjustment;
		adjustControls();
	}
}