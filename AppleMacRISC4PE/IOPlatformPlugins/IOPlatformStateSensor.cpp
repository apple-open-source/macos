/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: IOPlatformStateSensor.cpp,v $
//		Revision 1.4  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.3.2.4  2003/06/04 10:21:10  eem
//		Supports forced PID meta states.
//		
//		Revision 1.3.2.3  2003/06/04 00:00:51  eem
//		More PID stuff, working towards support for forced meta states.
//		
//		Revision 1.3.2.2  2003/05/23 05:44:40  eem
//		Cleanup, ctrlloops not get notification for sensor and control registration.
//		
//		Revision 1.3.2.1  2003/05/22 01:31:04  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.3  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.2.4.4  2003/05/17 12:55:37  eem
//		Active fan control works on RPM channels!!!!!!
//		
//		Revision 1.2.4.3  2003/05/17 11:08:22  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
//		
//		Revision 1.2.4.2  2003/05/16 07:08:45  eem
//		Table-lookup active fan control working with this checkin.
//		
//		Revision 1.2.4.1  2003/05/14 22:07:49  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//		Revision 1.2  2003/05/10 06:50:33  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.1.2.1  2003/05/01 09:28:40  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//		

#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformCtrlLoop.h"
#include "IOPlatformStateSensor.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(IOPlatformStateSensor, IOPlatformSensor)

bool IOPlatformStateSensor::init( void )
{
	if (!super::init())	return(false);

	return true;
}

void IOPlatformStateSensor::free( void )
{
	super::free();
}

const OSNumber *IOPlatformStateSensor::applyValueTransform( const OSNumber * value ) const
{
	return super::applyValueTransform(value);
}

const OSNumber *IOPlatformStateSensor::applyHWTransform( const OSNumber * value ) const
{
	//SENSOR_DLOG("IOPlatformStateSensor::applyHWTransform - entered\n");

	// default to no transformation -- just return the value that was passed in
	value->retain();
	return ( value );
}

IOReturn IOPlatformStateSensor::initPlatformSensor( const OSDictionary *dict )
{
	IOReturn status;
	OSArray * array;

	if ((status = super::initPlatformSensor(dict)) != kIOReturnSuccess)
		return status;

	// grab the threshold array from the thermal profile
	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalThresholdsKey))) != NULL)
	{
		infoDict->setObject(kIOPPluginThermalThresholdsKey, array);
	}
	else
	{
		SENSOR_DLOG("IOPlatformStateSensor::initPlatformSensor no thresholds found\n");
	}

	// initialize the current-state key
	setSensorState( gIOPPluginZero );

	return(status);
}

IOReturn IOPlatformStateSensor::registerDriver( IOService * driver, const OSDictionary * dict, bool notify )
{
	IOReturn status;
	const OSDictionary * thisState;
	const OSNumber * tmpHigh, * tmpLow, *tmpNumber;
	UInt32 tryState, nStates, curValue, highThreshold, lowThreshold;

	SENSOR_DLOG("IOPlatformStateSensor::registerDriver ID 0x%08lX\n", getSensorID()->unsigned32BitValue());

	if ((status = super::registerDriver(driver, dict, false)) != kIOReturnSuccess)
		return (status);

	curValue = getCurrentValue()->unsigned32BitValue();

	// determine initial state from current-value
	if ((nStates = getNumSensorStates()) == 0xFFFFFFFF)
	{
		SENSOR_DLOG("IOPlatformStateSensor::registerDriver bad threshold dictionary sensor id %08lX\n",
				getSensorID()->unsigned32BitValue());
		return(kIOReturnError);
	}

	status = kIOReturnError;	// assume we have a bad dictionary
	tryState = 0;
	while ( tryState < nStates )
	{
		if ((thisState = getThresholdsForState( tryState )) == NULL ||
		    (tmpHigh = getHighThresholdForState( thisState )) == NULL ||
			(tmpLow = getLowThresholdForState( thisState )) == NULL)
		{
			SENSOR_DLOG("IOPlatformStateSensor::registerDriver bad threshold dictionary sensor id %08lX!\n",
					getSensorID()->unsigned32BitValue());
			return(kIOReturnError);
		}
			
		highThreshold = tmpHigh->unsigned32BitValue();
		lowThreshold = tmpLow->unsigned32BitValue();

		if (curValue >= lowThreshold && curValue < highThreshold)
		{
			tmpNumber = OSNumber::withNumber( tryState, 32 );
			setSensorState( tmpNumber );
			//SENSOR_DLOG("IOPlatformStateSensor::registerDriver initial state is %lu\n", tmpNumber->unsigned32BitValue());
			tmpNumber->release();
			status = kIOReturnSuccess;
			break;
		}

		tryState++;
	}

	// send thresholds
	if (status == kIOReturnSuccess &&
	    (status = sendThresholdsToSensor()) == kIOReturnSuccess)
	{
		if (notify) notifyCtrlLoops();
	}

	return(status);
}

UInt32 IOPlatformStateSensor::getNumSensorStates( void )
{
	const OSArray * thresholds;

	if ((thresholds = OSDynamicCast(OSArray, infoDict->getObject(kIOPPluginThermalThresholdsKey))) != NULL)
	{
		return (UInt32)thresholds->getCount();
	}
	else
	{
		return(0xFFFFFFFF);
	}
}

const OSNumber *IOPlatformStateSensor::getSensorState( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginCurrentStateKey));
}

UInt32 IOPlatformStateSensor::getSensorStateUInt32( void )
{
	const OSNumber * state;

	if ((state = getSensorState()) != NULL)
	{
		return state->unsigned32BitValue();
	}
	else
	{
		return(0xFFFFFFFF);
	}
}

void IOPlatformStateSensor::setSensorState( const OSNumber * state )
{
	if (state)
		infoDict->setObject(gIOPPluginCurrentStateKey, state);
}

const OSNumber *IOPlatformStateSensor::getHighThresholdForState( const OSDictionary * state )
{
	//SENSOR_DLOG("IOPlatformStateSensor::getHighThresholdForState - entered\n");

	if (!state) return(NULL);

	return OSDynamicCast(OSNumber, state->getObject(gIOPPluginHighThresholdKey));
}

const OSNumber *IOPlatformStateSensor::getLowThresholdForState( const OSDictionary * state )
{
	//SENSOR_DLOG("IOPlatformStateSensor::getLowThresholdForState - entered\n");

	if (!state) return(NULL);

	return OSDynamicCast(OSNumber, state->getObject(gIOPPluginLowThresholdKey));
}

const OSDictionary *IOPlatformStateSensor::getThresholdsForState( UInt32 state )
{
	const OSArray * thresholdArray;
	const OSDictionary * thisState;

	//SENSOR_DLOG("IOPlatformStateSensor::getThresholdsForState - entered\n");

	if ((thresholdArray = OSDynamicCast(OSArray, infoDict->getObject(kIOPPluginThermalThresholdsKey))) == NULL)
	{
		SENSOR_DLOG("IOPlatformStateSensor::getThresholdsForState - no threshold dictionary!\n");
		return(NULL);
	}

	if ((thisState = OSDynamicCast(OSDictionary, thresholdArray->getObject( state ))) == NULL)
	{
		SENSOR_DLOG("IOPlatformStateSensor::getThresholdsForState - no thresholds for current state!\n");
	}

	return thisState;
}

IOReturn IOPlatformStateSensor::thresholdHit( bool low, OSDictionary * msgDict )
{
	const OSNumber * newState;
	UInt32 oldState;
	int i, count;
	IOPlatformCtrlLoop * loop;

	SENSOR_DLOG("IOPlatformStateSensor::thresholdHit ID 0x%08lX\n", getSensorID()->unsigned32BitValue());

	oldState = getSensorStateUInt32();

	// update the sensor's current state
	if (low)
	{
		if (oldState != 0)
		{
			newState = OSNumber::withNumber( oldState - 1, 32 );
			setSensorState( newState );
			newState->release();
		}
	}
	else  // msg == kIOPPluginMessageHighThresholdHit
	{
		if ( (oldState + 1) < getNumSensorStates() )
		{
			newState = OSNumber::withNumber( oldState + 1, 32 );
			setSensorState( newState );
			newState->release();
		}
	}

	// send new thresholds
	if (sendThresholdsToSensor() != kIOReturnSuccess)
	{
		SENSOR_DLOG("IOPlatformStateSensor::thresholdHit failed to send thresholds\n");
	}

	// inform control loops of new state
	if (ctrlLoops)
	{
		count = ctrlLoops->getCount();
		for (i=0; i<count; i++)
		{
			if ((loop = OSDynamicCast(IOPlatformCtrlLoop, ctrlLoops->getObject(i))) != NULL)
			{
				//SENSOR_DLOG("IOPlatformStateSensor::thresholdHit notifying ctrlLoop %d\n", i);
				loop->adjustControls();
			}
		}
	}

	return(kIOReturnSuccess);
}

IOReturn IOPlatformStateSensor::sendThresholdsToSensor( void )
{
	IOReturn status;
	const OSDictionary * thisState;
	OSDictionary * cmdDict;
	const OSNumber * rawHigh, * rawLow;

	//SENSOR_DLOG("IOPlatformStateSensor::sendThresoldsToSensor - entered\n");

	if ((thisState = getThresholdsForState( getSensorStateUInt32() )) == NULL)
	{
		SENSOR_DLOG("IOPlatformStateSensor::sendThresoldsToSensor - no thresholds for current state!\n");
		return(kIOReturnError);
	}

	// convert to the hw representation of the threshold values
	rawLow = applyHWTransform( OSDynamicCast(OSNumber, thisState->getObject(gIOPPluginLowThresholdKey)) );
	rawHigh = applyHWTransform( OSDynamicCast(OSNumber, thisState->getObject(gIOPPluginHighThresholdKey)) );

	cmdDict = OSDictionary::withCapacity(3);

	cmdDict->setObject(gIOPPluginSensorIDKey, getSensorID());
	cmdDict->setObject(gIOPPluginLowThresholdKey, rawLow);
	cmdDict->setObject(gIOPPluginHighThresholdKey, rawHigh);

	status = sendMessage( cmdDict );

	rawLow->release();
	rawHigh->release();
	cmdDict->release();

	return(status);
}
