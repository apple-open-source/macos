/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */


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
	const OSData *data;

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

	// description - if not included in thermal profile, will be set in registerDriver()
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginThermalLocalizedDescKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Found Sensor %s\n", string->getCStringNoCopy());
		infoDict->setObject(gIOPPluginThermalLocalizedDescKey, string);
	}

	// flags - if not included in thermal profile, will be set in registerDriver()
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginSensorFlagsKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Sensor Flags %08lx\n", number->unsigned32BitValue());
		infoDict->setObject(gIOPPluginSensorFlagsKey, number);
	}

	// type
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginTypeKey))) != NULL)
	{
		infoDict->setObject( gIOPPluginTypeKey, string);
	}

	// version - if not included in thermal profile, will be set in registerDriver()
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginVersionKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginVersionKey, number);
	}

	// location - if not included in thermal profile, will be set in registerDriver()
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginLocationKey))) != NULL)
	{
		infoDict->setObject( gIOPPluginLocationKey, string);
	}

	// zone - if not included in thermal profile, will be set in registerDriver()
	if ((data = OSDynamicCast(OSData, dict->getObject(gIOPPluginZoneKey))) != NULL)
	{
		infoDict->setObject( gIOPPluginZoneKey, data);
	}

	// polling period - if not included in thermal profile, will be set in registerDriver()
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginPollingPeriodKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor got polling period override %lu\n", number->unsigned32BitValue());
		infoDict->setObject(gIOPPluginPollingPeriodKey, number);
	}

	// create the "registered" key and set it to false
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanFalse);

	// create the "current-value" key and set it to zero
	SensorValue zeroVal;
	zeroVal.sensValue = 0x0;
	setCurrentValue( zeroVal );

	return(kIOReturnSuccess);
}

IOReturn IOPlatformSensor::initPlatformSensor( IOService * unknownSensor, const OSDictionary * dict )
{
	const OSNumber *number;

	if ( !unknownSensor || !init() ) return(kIOReturnError);

	// id
	if ((number = OSDynamicCast(OSNumber, unknownSensor->getProperty(gIOPPluginSensorIDKey))) != NULL)
	{
		//SENSOR_DLOG("IOPlatformSensor::initPlatformSensor Sensor ID %08lX\n", number->unsigned32BitValue());
		infoDict->setObject(gIOPPluginSensorIDKey, number);
	}
	else
	{
		IOLog("IOPlatformSensor::initPlatformSensor Unlisted Registrant %s omits Sensor ID\n", unknownSensor->getName());
		return(kIOReturnBadArgument);
	}

	// polling period - if not included in driver property, will be set in registerDriver()
	if ((number = OSDynamicCast(OSNumber, unknownSensor->getProperty(gIOPPluginPollingPeriodKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginPollingPeriodKey, number);
	}

	// create the "registered" key and set it to false
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanFalse);

	// create the "current-value" key and set it to zero
	SensorValue zeroVal;
	zeroVal.sensValue = 0x0;
	setCurrentValue( zeroVal );

	return(kIOReturnSuccess);
}

void IOPlatformSensor::free( void )
{
	if (infoDict) { infoDict->release(); infoDict = NULL; }
	if (sensorDriver) { sensorDriver->release(); sensorDriver = NULL; }

	super::free();
}

SensorValue IOPlatformSensor::applyCurrentValueTransform( SensorValue hwReading ) const
{
	// Default IOPlatformSensor implementation does not alter the value -- i.e.
	// the transform implemented here is f(x) = x
	//
	// Subclasses should override this method to implement their transforms.
	// Example would be:
	//
	// SensorValue transformed;
	// transformed.sensValue = hwReading.sensValue * 2 + 10;
	// return transformed;
	//
	// This example implements the transform f(x) = 2x+10

	return hwReading;
}


OSBoolean *IOPlatformSensor::isRegistered( void )
{
	return(OSDynamicCast(OSBoolean, infoDict->getObject(gIOPPluginRegisteredKey)));
}

IOReturn IOPlatformSensor::registerDriver( IOService * driver, const OSDictionary * dict, bool notify )
{
	const OSString * string;
	SensorValue pluginValue, hwValue;
	const OSNumber * pollingPeriod, * hwReading;

	SENSOR_DLOG("IOPlatformSensor::registerDriver ID 0x%08lX\n", getSensorID()->unsigned32BitValue());

	if (isRegistered() == kOSBooleanTrue || driver == NULL)
		return( kIOReturnError );

	// store a pointer to the driver
	sensorDriver = driver;
	sensorDriver->retain();

	// If there's no localized description key, add a default
	if (getSensorDescKey() == NULL)
	{
		string = OSSymbol::withCString("UNKNOWN_SENSOR");
		infoDict->setObject(gIOPPluginThermalLocalizedDescKey, string);
		string->release();
	}

	// If there's no flags, set a default of zero (no bits set)
	if (getSensorFlags() == NULL)
	{
		infoDict->setObject(gIOPPluginSensorFlagsKey, gIOPPluginZero);
	}

	// The following properties might be over-ridden by the thermal profile, but are required to
	// be present in IOHWSensor:
	//
	//	type
	//	version
	//	location
	//	zone
	//
	// Check if they're already set (meaning they were over-ridden), and if not, pull the value
	// out of IOHWSensor

	if (getSensorType() == NULL)
		infoDict->setObject( gIOPPluginTypeKey, sensorDriver->getProperty( gIOPPluginTypeKey ) );

	if (getVersion() == NULL)
		infoDict->setObject( gIOPPluginVersionKey, sensorDriver->getProperty( gIOPPluginVersionKey ));

	if (getSensorLocation() == NULL)
		infoDict->setObject( gIOPPluginLocationKey, sensorDriver->getProperty( gIOPPluginLocationKey ));

	if (getSensorZone() == NULL)
		infoDict->setObject( gIOPPluginZoneKey, sensorDriver->getProperty( gIOPPluginZoneKey ));

	// flag the successful registration
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanTrue );

	// set the current value sent with the registration
	hwReading = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginCurrentValueKey));
	if (hwReading)
	{
		// Extract the sensor reading
		hwValue.sensValue = (SInt32)hwReading->unsigned32BitValue();

		// Apply the current value transform
		pluginValue = applyCurrentValueTransform( hwValue );

		// publish the value
		setCurrentValue( pluginValue );
	}

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

	if (sendPollingPeriod(pollingPeriod))
	{
		setPollingPeriod(pollingPeriod);
	}
	else
	{
		SENSOR_DLOG("IOPlatformSensor::registerDriver failed to send polling period to sensor\n");
	}
		
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
SensorValue IOPlatformSensor::getCurrentValue( void )
{
	SensorValue value;
	OSNumber * num;
	
	num = OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginCurrentValueKey));
	value.sensValue = num ? (SInt32) num->unsigned32BitValue() : 0 ;

	return value;
}

void IOPlatformSensor::setCurrentValue( SensorValue newValue )
{
	OSNumber * num;

	if ((num = OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginCurrentValueKey))) != NULL)
	{
		num->setValue( (UInt32)newValue.sensValue );
	}
	else
	{
		num = OSNumber::withNumber( (UInt32)newValue.sensValue, 32 );
		infoDict->setObject(gIOPPluginCurrentValueKey, num );
		num->release();
	}
}

SensorValue IOPlatformSensor::forceAndFetchCurrentValue( void )
{
#if SENSOR_DEBUG
	if (kIOReturnSuccess != sendForceUpdate())
	{
		SENSOR_DLOG("IOPlatformSensor::forceAndFetchCurrentValue(0x%08lX) failed to force-update\n", getSensorID()->unsigned32BitValue());
	}
#else
	sendForceUpdate();
#endif
	
	return(fetchCurrentValue());
}

SensorValue IOPlatformSensor::fetchCurrentValue( void )
{
	SensorValue pluginValue;
	const OSNumber * hwReading;

	// return 0 on failure
	pluginValue.sensValue = 0x0;

	if (!(isRegistered() == kOSBooleanTrue) || !sensorDriver)
	{
		SENSOR_DLOG("IOPlatformSensor::fetchCurrentValue(0x%08lX) not registered\n", getSensorID()->unsigned32BitValue());
		return pluginValue;
	}

	// Fetch a reference to IOHWSensor's current-value property.  This uses the
	// synchronized IORegistryEntry::getProperty() accessor.
	hwReading = OSDynamicCast(OSNumber, sensorDriver->getProperty(gIOPPluginCurrentValueKey));
	if (!hwReading)
	{
		SENSOR_DLOG("IOPlatformSensor::fetchCurrentValue(0x%08lX) failed to fetch IOHWSensor's current-value property\n", getSensorID()->unsigned32BitValue());
		return pluginValue;
	}

	// Apply the current-value transform to IOHWSensor's published current-value
	pluginValue.sensValue = (SInt32)hwReading->unsigned32BitValue();
	pluginValue = applyCurrentValueTransform( pluginValue );

	return pluginValue;
}

// polling period
OSNumber *IOPlatformSensor::getPollingPeriod( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginPollingPeriodKey));
}

void IOPlatformSensor::setPollingPeriod( const OSNumber * period )
{
	OSNumber * num;

	if ((num = getPollingPeriod()) != NULL)
	{
		num->setValue( period->unsigned32BitValue() );
	}
	else
	{
		infoDict->setObject(gIOPPluginPollingPeriodKey, period);
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

IOReturn IOPlatformSensor::sendForceUpdate( void )
{
	IOReturn status;
	OSDictionary *forceUpdateDict;

	forceUpdateDict = OSDictionary::withCapacity(2);
	if (!forceUpdateDict) return(kIOReturnNoMemory);
	forceUpdateDict->setObject(gIOPPluginForceUpdateKey, gIOPPluginZero);
	forceUpdateDict->setObject(gIOPPluginSensorIDKey, getSensorID());

	// force an update
	status = sendMessage( forceUpdateDict );
	forceUpdateDict->release();

	return(status);
}
