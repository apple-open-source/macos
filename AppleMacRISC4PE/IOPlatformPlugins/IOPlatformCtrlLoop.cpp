/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformCtrlLoop.h"

#define super OSObject
OSDefineMetaClassAndStructors( IOPlatformCtrlLoop, OSObject )

bool IOPlatformCtrlLoop::init( void )
{
	if (!super::init()) return(false);

	// allocate the info dictionary
	if ((infoDict = OSDictionary::withCapacity(1)) == NULL) return(false);

	controls = NULL;
	sensors = NULL;

	// zero the deadline
	AbsoluteTime_to_scalar(&deadline) = 0;

	return true;
}

void IOPlatformCtrlLoop::free( void )
{
	if (infoDict) { infoDict->release(); infoDict = NULL; }
	if (controls) { controls->release(); controls = NULL; }
	if (sensors) { sensors->release(); sensors = NULL; }

	super::free();
}

IOReturn IOPlatformCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
{
	const OSArray * array;
	const OSString * string;
	const OSNumber * number;

	if ( !dict || !init() ) return(kIOReturnBadArgument);

	// id
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginCtrlLoopIDKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Sensor ID %08lX\n", number->unsigned32BitValue());
		infoDict->setObject(gIOPPluginCtrlLoopIDKey, number);
	}
	else
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::initPlatformCtrlLoop Invalid Thermal Profile omits CtrlLoop ID\n");
		return(kIOReturnBadArgument);
	}

	// read the description
	if ((string = OSDynamicCast(OSString, dict->getObject(kIOPPluginThermalGenericDescKey))) != NULL)
	{
		//CTRLLOOP_DLOG("IOPlatformCtrlLoop::initPlatformCtrlLoop found %s\n", string->getCStringNoCopy());
		infoDict->setObject(kIOPPluginThermalGenericDescKey, string);
	}
	else
	{
		//CTRLLOOP_DLOG("IOPlatformCtrlLoop::initPlatformCtrlLoop has no description\n");
		string = OSSymbol::withCString("Unknown Control Loop");
		infoDict->setObject(kIOPPluginThermalGenericDescKey, string);
		string->release();
	}

	// Pull in the meta state format if specified
	if ((string = OSDynamicCast(OSString, dict->getObject("meta-state-format"))) != NULL)
	{
		infoDict->setObject("meta-state-format", string);
	}

	// grab the meta state array
	if ((array = OSDynamicCast(OSArray, dict->getObject(gIOPPluginThermalMetaStatesKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginThermalMetaStatesKey, array);
	}
	else
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::initPlatformCtrlLoop NO META STATES!!\n");
	}

	// set the initial meta-state to zero -- subclasses should change as needed
	setMetaState(gIOPPluginZero);

	ctrlloopState = kIOPCtrlLoopNotReady;

	return(kIOReturnSuccess);
}

void IOPlatformCtrlLoop::willSleep( void )
{
	ctrlloopState = kIOPCtrlLoopWillSleep;
}

void IOPlatformCtrlLoop::didWake( void )
{
	ctrlloopState = kIOPCtrlLoopDidWake;
}

const AbsoluteTime IOPlatformCtrlLoop::getDeadline( void )
{
	return(deadline);
}

void IOPlatformCtrlLoop::deadlinePassed( void )
{
	CTRLLOOP_DLOG("IOPlatformCtrlLoop::deadlinePassed - entered\n");
	// zero the deadline - this will ensure that this routine won't get called again unless some other method sets a deadline.  Subclasses should override this, especially if they need a periodic timer callback
	AbsoluteTime_to_scalar(&deadline) = 0;
}	

OSDictionary * IOPlatformCtrlLoop::getInfoDict( void )
{
	return infoDict;
}

OSNumber *IOPlatformCtrlLoop::getCtrlLoopID( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginCtrlLoopIDKey));
}

OSNumber * IOPlatformCtrlLoop::getMetaState( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginCtrlLoopMetaState));
}

void IOPlatformCtrlLoop::setMetaState( const OSNumber * state )
{
	if (state)
		infoDict->setObject(gIOPPluginCtrlLoopMetaState, state);
}

bool IOPlatformCtrlLoop::updateMetaState( void )
{
	// subclasses need to be taught how to determine metastate from the pluginEnvInfo dict.
	return(false);
}

void IOPlatformCtrlLoop::adjustControls( void )
{
	// subclasses need to override this
	return;
}

/*
SensorValue IOPlatformCtrlLoop::getCurrentSensorValue( void )
{
	IOPlatformSensor * tmpSensor;
	SensorValue maxVal, tmpVal;
	unsigned int index, count;

	tmpVal.sensValue = -1;
	if (!sensors) return(tmpVal);

	// return the max value of all sensors
	count = sensors->getCount();
	for (index = 0; index < count; index++)
	{
		tmpSensor = OSDynamicCast(IOPlatformSensor, sensors->getObject(index));
	
		if (index == 0)
		{
			maxVal = tmpSensor->fetchCurrentSensorValue();
		}
		else
		{
			tmpVal = tmpSensor->fetchCurrentSensorValue();
			if (tmpVal.sensValue > maxVal.sensValue)
				maxVal = tmpVal;
		}
	}

	return(maxVal);
}
*/

void IOPlatformCtrlLoop::sensorRegistered( IOPlatformSensor * aSensor )
{
	// subclasses should use this to figure out when all the dependent sensor/control drivers are present
}

void IOPlatformCtrlLoop::controlRegistered( IOPlatformControl * aControl )
{
	// subclasses should use this to figure out when all the dependent sensor/control drivers are present
}


bool IOPlatformCtrlLoop::addSensor( IOPlatformSensor * aSensor )
{
	if (!aSensor)
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::addSensor bad argument\n");
		return(false);
	}

	if (!sensors)
	{
		sensors = OSArray::withObjects((const OSObject **) &aSensor, 1);
	}
	else
	{
		// Make sure this sensor isn't already listed
		unsigned index, count;
		count = sensors->getCount();
		for (index = 0; index < count; index++)
		{
			if (sensors->getObject(index) == aSensor)
			{
				CTRLLOOP_DLOG("IOPlatformCtrlLoop::addSensor already an input\n");
				return(false);
			}
		}

		sensors->setObject(aSensor);
	}

	if (!aSensor->joinedCtrlLoop(this))
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::addSensor joinedCtrlLoop failed!\n");
	}

	return(true);
}

bool IOPlatformCtrlLoop::removeSensor( IOPlatformSensor * aSensor )
{
	if (!aSensor)
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::removeSensor bad argument\n");
		return(false);
	}

	if (!sensors)
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::removeSensor no sensors\n");
		return(false);
	}

	bool removed = false;
	int index, count;
	count = sensors->getCount();
	for (index = 0; index < count; index++)
	{
		if (sensors->getObject(index) == aSensor)
		{
			sensors->removeObject(index);
			removed = true;
		}
	}

	if (removed)
	{
		aSensor->leftCtrlLoop(this);
	}
	else
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::removeSensor not found\n");
	}

	return(removed);
}

bool IOPlatformCtrlLoop::addControl( IOPlatformControl * aControl )
{
	if (!aControl)
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::addControl bad argument\n");
		return(false);
	}

	if (!controls)
	{
		controls = OSArray::withObjects((const OSObject **) &aControl, 1);
	}
	else
	{
		// Make sure this control isn't already listed
		unsigned index, count;
		count = controls->getCount();
		for (index = 0; index < count; index++)
		{
			if (controls->getObject(index) == aControl)
			{
				CTRLLOOP_DLOG("IOPlatformCtrlLoop::addControl already an output\n");
				return(false);
			}
		}

		controls->setObject(aControl);
	}

	if (!aControl->joinedCtrlLoop(this))
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::addControl joinedCtrlLoop failed!\n");
	}

	return(true);
}

bool IOPlatformCtrlLoop::removeControl( IOPlatformControl * aControl )
{
	if (!aControl)
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::removeControl bad argument\n");
		return(false);
	}

	if (!controls)
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::removeControl no controls\n");
		return(false);
	}

	bool removed = false;
	int index, count;
	count = controls->getCount();
	for (index = 0; index < count; index++)
	{
		if (controls->getObject(index) == aControl)
		{
			controls->removeObject(index);
			removed = true;
		}
	}

	if (removed)
	{
		aControl->leftCtrlLoop(this);
	}
	else
	{
		CTRLLOOP_DLOG("IOPlatformCtrlLoop::removeControl not found\n");
	}

	return(removed);
}

void IOPlatformCtrlLoop::sensorCurrentValueWasSet( IOPlatformSensor * aSensor, SensorValue newValue )
{
	return;
}

void IOPlatformCtrlLoop::controlCurrentValueWasSet( IOPlatformControl * aControl, ControlValue newValue )
{
	return;
}

void IOPlatformCtrlLoop::controlTargetValueWasSet( IOPlatformControl * aControl, ControlValue newValue )
{
	return;
}
