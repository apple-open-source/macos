/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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

SensorValue IOPlatformStateSensor::applyCurrentValueInverseTransform( SensorValue pluginReading ) const
{
	// Default IOPlatformStateSensor implementation does not alter the value -- i.e.
	// the transform implemented here is f(x) = x
	//
	// Subclasses should override this method to implement their transforms.
	// Example would be:
	//
	// SensorValue transformed;
	// transformed.sensValue = (pluginReading.sensValue - 10) / 2;
	// return transformed;
	//
	// This example implements the transform f(x) = (x-10)/2

	return pluginReading;
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
	UInt32 tryState, nStates;
	SensorValue curValue, highThreshold, lowThreshold;

	SENSOR_DLOG("IOPlatformStateSensor::registerDriver ID 0x%08lX\n", getSensorID()->unsigned32BitValue());

	if ((status = super::registerDriver(driver, dict, false)) != kIOReturnSuccess)
		return (status);

	curValue = getCurrentValue();

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
			
		highThreshold.sensValue = (SInt32)tmpHigh->unsigned32BitValue();
		lowThreshold.sensValue = (SInt32)tmpLow->unsigned32BitValue();

		if (curValue.sensValue >= lowThreshold.sensValue && curValue.sensValue < highThreshold.sensValue)
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
	const OSNumber * high, * low;
	SensorValue reading;

	//SENSOR_DLOG("IOPlatformStateSensor::sendThresoldsToSensor - entered\n");

	if ((thisState = getThresholdsForState( getSensorStateUInt32() )) == NULL)
	{
		SENSOR_DLOG("IOPlatformStateSensor::sendThresoldsToSensor - no thresholds for current state!\n");
		return(kIOReturnError);
	}

	// convert to the hw representation of the threshold values
	low = OSDynamicCast(OSNumber, thisState->getObject(gIOPPluginLowThresholdKey));
	reading.sensValue = (SInt32)low->unsigned32BitValue();
	reading = applyCurrentValueInverseTransform( reading );
	low = OSNumber::withNumber( (UInt32)reading.sensValue, 32 );

	high = OSDynamicCast(OSNumber, thisState->getObject(gIOPPluginHighThresholdKey));
	reading.sensValue = (SInt32)high->unsigned32BitValue();
	reading = applyCurrentValueInverseTransform( reading );
	high = OSNumber::withNumber( (UInt32)reading.sensValue, 32 );
	
	cmdDict = OSDictionary::withCapacity(3);

	cmdDict->setObject(gIOPPluginSensorIDKey, getSensorID());
	cmdDict->setObject(gIOPPluginLowThresholdKey, low);
	cmdDict->setObject(gIOPPluginHighThresholdKey, high);

	status = sendMessage( cmdDict );

	low->release();
	high->release();
	cmdDict->release();

	return(status);
}
