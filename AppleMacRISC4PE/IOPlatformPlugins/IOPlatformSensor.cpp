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
 *  DRI: Dave Radcliffe
 *
 */
//		$Log: IOPlatformSensor.cpp,v $
//		Revision 1.6  2003/07/08 04:32:49  eem
//		3288891, 3279902, 3291553, 3154014
//		
//		Revision 1.5  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.4.2.8  2003/06/06 08:17:56  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.4.2.7  2003/06/04 10:21:10  eem
//		Supports forced PID meta states.
//		
//		Revision 1.4.2.6  2003/06/04 00:00:51  eem
//		More PID stuff, working towards support for forced meta states.
//		
//		Revision 1.4.2.5  2003/05/29 03:51:34  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.4.2.4  2003/05/26 10:19:00  eem
//		Fixed OSNumber leaks.
//		
//		Revision 1.4.2.3  2003/05/26 10:07:15  eem
//		Fixed most of the bugs after the last cleanup/reorg.
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
//		Revision 1.3.2.4  2003/05/17 12:55:37  eem
//		Active fan control works on RPM channels!!!!!!
//		
//		Revision 1.3.2.3  2003/05/17 11:08:22  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
//		
//		Revision 1.3.2.2  2003/05/16 07:08:45  eem
//		Table-lookup active fan control working with this checkin.
//		
//		Revision 1.3.2.1  2003/05/14 22:07:49  eem
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
//		Revision 1.1.1.1.2.3  2003/05/10 06:32:34  eem
//		Sensor changes, should be ready to merge to trunk as 1.0.1d12.
//		
//		Revision 1.1.1.1.2.2  2003/05/03 01:11:38  eem
//		*** empty log message ***
//		
//		Revision 1.1.1.1.2.1  2003/05/01 09:28:40  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

#include <IOKit/IOLib.h>
#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformCtrlLoop.h"
#include "IOPlatformSensor.h"

#define super OSObject
OSDefineMetaClassAndStructors(IOPlatformSensor, OSObject)

bool IOPlatformSensor::init( void )
{
	if (!super::init()) return(false);

	// allocate the info dictionary
	if ((infoDict = OSDictionary::withCapacity(7)) == NULL) return(false);

	sensorDriver = NULL;

	return( initSymbols() );
}

bool IOPlatformSensor::initSymbols( void )
{
	return( true );
}

IOReturn IOPlatformSensor::initPlatformSensor( const OSDictionary *dict )
{
	const OSNumber *number;
	const OSString *string;

	if ( !dict || !init() ) return(kIOReturnError);

	// id
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginSensorIDKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Sensor ID %08lX\n", number->unsigned32BitValue());
		infoDict->setObject(gIOPPluginSensorIDKey, number);
	}
	else
	{
		IOLog("IOPlatformSensor::initPlatformSensor Invalid Thermal Profile omits Sensor ID\n");
		return(kIOReturnBadArgument);
	}

	// description
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginThermalLocalizedDescKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Found Sensor %s\n", string->getCStringNoCopy());
		infoDict->setObject(gIOPPluginThermalLocalizedDescKey, string);
	}
	else
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Found Sensor without Description\n");
		string = OSSymbol::withCString("UNKNOWN_SENSOR");
		infoDict->setObject(gIOPPluginThermalLocalizedDescKey, string);
		string->release();
	}

	// flags
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginSensorFlagsKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Sensor Flags %08lx\n", number->unsigned32BitValue());
		infoDict->setObject(gIOPPluginSensorFlagsKey, number);
	}
	else
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Sensor Flags omitted in thermal profile, continuing...\n");
		infoDict->setObject(gIOPPluginSensorFlagsKey, gIOPPluginZero);
	}

	// version - optional override - if not here it should come from IOHWSensor when it registers
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginVersionKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor got version %u\n", number->unsigned32BitValue());
		infoDict->setObject(gIOPPluginVersionKey, number);
	}

	// type - optional override -if not here it should come from IOHWSensor when it registers
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginTypeKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor got type override %s\n", string->getCStringNoCopy());
		infoDict->setObject(gIOPPluginTypeKey, string);
	}

	// polling period - optional override - if not here it should come from IOHWSensor when it registers
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginPollingPeriodKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor got polling period override %lu\n", number->unsigned32BitValue());
		infoDict->setObject(gIOPPluginPollingPeriodKey, number);
	}

	// create the "registered" key and set it to false
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanFalse);

	// create the "current-value" key and set it to zero
	setCurrentValue( gIOPPluginZero );

	return(kIOReturnSuccess);
}

IOReturn IOPlatformSensor::initPlatformSensor( IOService * unknownSensor )
{
	const OSNumber *number;
	const OSString *string;

	if ( !unknownSensor || !init() ) return(kIOReturnError);

	// id
	if ((number = OSDynamicCast(OSNumber, unknownSensor->getProperty(gIOPPluginSensorIDKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Sensor ID %08lX\n", number->unsigned32BitValue());
		infoDict->setObject(gIOPPluginSensorIDKey, number);
	}
	else
	{
		IOLog("IOPlatformSensor::initPlatformSensor Unknown Registrant omits Sensor ID\n");
		return(kIOReturnBadArgument);
	}

	// description
	//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Found Sensor without Description\n");
	string = OSSymbol::withCString("UNKNOWN_SENSOR");
	infoDict->setObject(gIOPPluginThermalLocalizedDescKey, string);
	string->release();

	// flags
	//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Sensor Flags omitted in thermal profile, continuing...\n");
	infoDict->setObject(gIOPPluginSensorFlagsKey, gIOPPluginZero);

	// create the "registered" key and set it to false
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanFalse);

	// create the "current-value" key and set it to zero
	setCurrentValue( gIOPPluginZero );

	return(kIOReturnSuccess);
}

void IOPlatformSensor::free( void )
{
	if (infoDict) { infoDict->release(); infoDict = NULL; }
	if (sensorDriver) { sensorDriver->release(); sensorDriver = NULL; }

	super::free();
}

const OSNumber *IOPlatformSensor::applyValueTransform( const OSNumber * hwReading ) const
{
	//SENSOR_DLOG("IOPlatformSensor::applyValueTransform - entered\n");
	hwReading->retain();
	return hwReading;
}


OSBoolean *IOPlatformSensor::isRegistered( void )
{
	return(OSDynamicCast(OSBoolean, infoDict->getObject(gIOPPluginRegisteredKey)));
}

IOReturn IOPlatformSensor::registerDriver( IOService * driver, const OSDictionary * dict, bool notify )
{
	const OSString * type;
	const OSNumber * pollingPeriod, *value;

	SENSOR_DLOG("IOPlatformSensor::registerDriver ID 0x%08lX\n", getSensorID()->unsigned32BitValue());

	if (isRegistered() == kOSBooleanTrue || driver == NULL)
		return( kIOReturnError );

	// store a pointer to the driver
	sensorDriver = driver;
	sensorDriver->retain();

	// grab some properties out of the IOHWSensor node
	if (getVersion() == NULL)
		infoDict->setObject( gIOPPluginVersionKey, driver->getProperty( gIOPPluginVersionKey ));
	if (getSensorLocation() == NULL)
		infoDict->setObject( gIOPPluginLocationKey, driver->getProperty( gIOPPluginLocationKey ));
	if (getSensorZone() == NULL)
		infoDict->setObject( gIOPPluginZoneKey, driver->getProperty( gIOPPluginZoneKey ));

	// if there's no type override, get the type from the sensor driver
	if ((type = getSensorType()) == NULL)
		infoDict->setObject( gIOPPluginTypeKey, driver->getProperty( gIOPPluginTypeKey ));

	// flag the successful registration
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanTrue );

	// set the current value sent with the registration
	value = applyValueTransform( OSDynamicCast(OSNumber, dict->getObject(gIOPPluginCurrentValueKey)) );
	setCurrentValue( value );
	value->release();

	// initialize the polling period
	if ((pollingPeriod = getPollingPeriod()) != NULL)
	{
		pollingPeriod->retain();
	}
	else
	{
		// nothing in infoDict, default to no polling
		pollingPeriod = OSNumber::withNumber( (unsigned long long) kIOPPluginNoPolling, 32 );
	}

	if (sendPollingPeriod(pollingPeriod)) setPollingPeriod(pollingPeriod);
	pollingPeriod->release();

	// conditionally notify control loops that a driver registered
	if (notify) notifyCtrlLoops();

	return( kIOReturnSuccess );
}

void IOPlatformSensor::notifyCtrlLoops( void )
{
	IOPlatformCtrlLoop * loop;
	int i, count;

	// notify my ctrl loops that the driver registered
	if (ctrlLoops)
	{
		count = ctrlLoops->getCount();
		for ( i=0; i<count; i++ )
		{
			loop = OSDynamicCast(IOPlatformCtrlLoop, ctrlLoops->getObject(i));
			if (loop) loop->sensorRegistered( this );
		}
	}
}

bool IOPlatformSensor::joinedCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop )
{
	if (!aCtrlLoop)
	{
		SENSOR_DLOG("IOPlatformSensor::joinedCtrlLoop bad argument\n");
		return(false);
	}

	if (!ctrlLoops)
	{
		ctrlLoops = OSArray::withObjects((const OSObject **) &aCtrlLoop, 1);
		return(true);
	}
	else
	{
		// Make sure this control loop isn't already listed
		unsigned index, count;
		count = ctrlLoops->getCount();
		for (index = 0; index < count; index++)
		{
			if (ctrlLoops->getObject(index) == aCtrlLoop)
			{
				SENSOR_DLOG("IOPlatformSensor::joinCtrlLoop already a member\n");
				return(false);
			}
		}

		ctrlLoops->setObject(aCtrlLoop);
		return(true);
	}
}

bool IOPlatformSensor::leftCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop )
{
	if (!aCtrlLoop)
	{
		SENSOR_DLOG("IOPlatformSensor::leftCtrlLoop bad argument\n");
		return(false);
	}

	if (!ctrlLoops)
	{
		SENSOR_DLOG("IOPlatformSensor::leftCtrlLoop no control loops\n");
		return(false);
	}

	bool removed = false;
	int index, count;
	count = ctrlLoops->getCount();
	for (index = 0; index < count; index++)
	{
		if (ctrlLoops->getObject(index) == aCtrlLoop)
		{
			ctrlLoops->removeObject(index);
			removed = true;
		}
	}

	if (!removed)
		SENSOR_DLOG("IOPlatformSensor::leftCtrlLoop not a member\n");

	return(removed);
}

OSArray *IOPlatformSensor::memberOfCtrlLoops( void )
{
	return ctrlLoops;
}

// get a reference to the info dictionary
OSDictionary *IOPlatformSensor::getInfoDict( void )
{
	return infoDict;
}

// version
OSNumber *IOPlatformSensor::getVersion( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginVersionKey));
}

// sensor-id
OSNumber *IOPlatformSensor::getSensorID( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginSensorIDKey));
}

// type
OSString *IOPlatformSensor::getSensorType( void )
{
	return OSDynamicCast(OSString, infoDict->getObject(gIOPPluginTypeKey));
}

UInt32 IOPlatformSensor::getSensorTypeID( void )
{
	OSString * typeString;

	typeString = getSensorType();

	if (!typeString)
		return kIOPSensorTypeUnknown;
	else if (typeString->isEqualTo(gIOPPluginTypeTempSensor))
		return kIOPSensorTypeTemp;
	else if (typeString->isEqualTo(gIOPPluginTypePowerSensor))
		return kIOPSensorTypePower;
	else if (typeString->isEqualTo(gIOPPluginTypeVoltageSensor))
		return kIOPSensorTypeVoltage;
	else if (typeString->isEqualTo(gIOPPluginTypeCurrentSensor))
		return kIOPSensorTypeCurrent;
	else if (typeString->isEqualTo(gIOPPluginTypeADCSensor))
		return kIOPSensorTypeADC;
	else
		return kIOPSensorTypeUnknown;
}

// zone
OSData *IOPlatformSensor::getSensorZone( void )
{
	return OSDynamicCast(OSData, infoDict->getObject(gIOPPluginZoneKey));
}

// location
OSString *IOPlatformSensor::getSensorLocation( void )
{
	return OSDynamicCast(OSString, infoDict->getObject(gIOPPluginLocationKey));
}

// Desc-Key
OSString *IOPlatformSensor::getSensorDescKey( void )
{
	return OSDynamicCast(OSString, infoDict->getObject(gIOPPluginThermalLocalizedDescKey));
}

// sensor-flags
const OSNumber *IOPlatformSensor::getSensorFlags( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginSensorFlagsKey));
}

// current-value
const OSNumber *IOPlatformSensor::getCurrentValue( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginCurrentValueKey));
}

void IOPlatformSensor::setCurrentValue( const OSNumber * sensorValue )
{
	if (!sensorValue)
	{
		SENSOR_DLOG("IOPlatformSensor::setCurrentValue got null sensorValue\n");
		return;
	}

	infoDict->setObject(gIOPPluginCurrentValueKey, sensorValue);
}

const OSNumber *IOPlatformSensor::fetchCurrentValue( void )
{
	IOReturn status;
	OSDictionary *forceUpdateDict;
	const OSNumber * raw, * value;

	//SENSOR_DLOG("IOPlatformSensor::fetchCurrentValue - entered\n");

	if (!(isRegistered() == kOSBooleanTrue) || !sensorDriver)
	{
		SENSOR_DLOG("IOPlatformSensor::fetchCurrentValue not registered\n");
		return(NULL);
	}

	forceUpdateDict = OSDictionary::withCapacity(2);
	if (!forceUpdateDict) return(NULL);
	forceUpdateDict->setObject(gIOPPluginForceUpdateKey, gIOPPluginZero);
	forceUpdateDict->setObject(gIOPPluginSensorIDKey, getSensorID());

	// force an update
	status = sendMessage( forceUpdateDict );
	forceUpdateDict->release();

	if (status != kIOReturnSuccess )
	{
		SENSOR_DLOG("IOPlatformSensor::fetchCurrentValue sendMessage failed!!\n");
		return(NULL);
	}
	else
	{
		// read the updated value
		raw = OSDynamicCast(OSNumber, sensorDriver->getProperty(gIOPPluginCurrentValueKey));
		value = applyValueTransform( raw );
		//raw->release();
		return value;
	}
}

// polling period
const OSNumber *IOPlatformSensor::getPollingPeriod( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginPollingPeriodKey));
}

void IOPlatformSensor::setPollingPeriod( const OSNumber * period )
{
	infoDict->setObject(gIOPPluginPollingPeriodKey, period);
}

// this sends the polling period (as it is set in the infoDict) to the sensor
bool IOPlatformSensor::sendPollingPeriod( const OSNumber * period )
{
	OSDictionary * msgDict;
	IOReturn status;

	if (!period)
	{
		SENSOR_DLOG("IOPlatformSensor::sendPollingPeriod no polling period\n");
		return(kIOReturnBadArgument);
	}

	// set up the message dict
	msgDict = OSDictionary::withCapacity(2);
	msgDict->setObject(gIOPPluginSensorIDKey, getSensorID());
	msgDict->setObject(gIOPPluginPollingPeriodKey, period );

	// send the dict down to the sensor
	status = sendMessage( msgDict );
	msgDict->release();

	if (status != kIOReturnSuccess)
	{
		SENSOR_DLOG("IOPlatformSensor::sendPollingPeriod sendMessage() failed!!\n");
		return(false);
	}
	else
	{
		return(true);
	}
}

IOReturn IOPlatformSensor::sendMessage( OSDictionary * msg )
{
	//SENSOR_DLOG("IOPlatformSensor::sendMessage - entered\n");

	if (!(isRegistered() == kOSBooleanTrue) || !sensorDriver)
	{
		SENSOR_DLOG("IOPlatformSensor::sendMessage not registered\n");
		return(kIOReturnOffline);
	}

	if ( !msg )
	{
		SENSOR_DLOG("IOPlatformSensor::sendMessage no message\n");
		return(kIOReturnBadArgument);
	}

	return(sensorDriver->setProperties( msg ));
}
