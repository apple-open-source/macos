/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: IOPlatformCtrlLoop.cpp,v $
//		Revision 1.6  2003/07/16 02:02:09  eem
//		3288772, 3321372, 3328661
//		
//		Revision 1.5  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.4.2.10  2003/06/06 11:00:42  eem
//		Drive Bay and U3 zones now also under PID control.
//		
//		Revision 1.4.2.9  2003/06/06 08:17:56  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.4.2.8  2003/06/04 10:21:10  eem
//		Supports forced PID meta states.
//		
//		Revision 1.4.2.7  2003/06/04 00:00:51  eem
//		More PID stuff, working towards support for forced meta states.
//		
//		Revision 1.4.2.6  2003/06/01 14:52:51  eem
//		Most of the PID algorithm is implemented.
//		
//		Revision 1.4.2.5  2003/05/31 08:11:34  eem
//		Initial pass at integrating deadline-based timer callbacks for PID loops.
//		
//		Revision 1.4.2.4  2003/05/29 03:51:34  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.4.2.3  2003/05/23 06:36:57  eem
//		More registration notification stuff.
//		
//		Revision 1.4.2.2  2003/05/23 05:44:40  eem
//		Cleanup, ctrlloops not get notification for sensor and control registration.
//		
//		Revision 1.4.2.1  2003/05/22 01:31:04  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.4  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.3.2.3  2003/05/17 12:55:37  eem
//		Active fan control works on RPM channels!!!!!!
//		
//		Revision 1.3.2.2  2003/05/16 07:08:45  eem
//		Table-lookup active fan control working with this checkin.
//		
//		Revision 1.3.2.1  2003/05/14 22:07:48  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//		Revision 1.3  2003/05/13 02:13:51  eem
//		PowerMac7_2 Dynamic Power Step support.
//		
//		Revision 1.2.2.1  2003/05/12 11:21:10  eem
//		Support for slewing.
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
