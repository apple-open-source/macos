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
 *  DRI: Dave Radcliffe
 *
 */


#ifndef _IOPLATFORMSENSOR_H
#define _IOPLATFORMSENSOR_H

// IOService pulls in all the headers we need
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include "IOPlatformPluginDefs.h"

#ifdef SENSOR_DLOG
#undef SENSOR_DLOG
#endif

// Uncomment for debug info
// #define SENSOR_DEBUG 1

#ifdef SENSOR_DEBUG
#define SENSOR_DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define SENSOR_DLOG(fmt, args...)
#endif

enum {
	kIOPSensorTypeUnknown,
	kIOPSensorTypeTemp,
	kIOPSensorTypeVoltage,
	kIOPSensorTypeCurrent,
	kIOPSensorTypePower,
	kIOPSensorTypeADC
};

class IOPlatformCtrlLoop;

/*!
    @class IOPlatformSensor
    @abstract A class abstracting hardware sensors such as for power and thermal */
class IOPlatformSensor : public OSObject
{
    OSDeclareDefaultStructors(IOPlatformSensor)	

protected:
	/*! @var infoDict A dictionary containing up-to-date info for publishing in the registry.  At the least this should contain sensor-id, version, location, type, zone and current-value keys */
	OSDictionary *	infoDict;

	/*! @var sensorDriver A pointer to the device driver that implements this controller.  Should be an IOHWSensor object. */
	IOService *		sensorDriver;

	/*! @var ctrlLoops An array of IOPlatformCtrlLoops that this sensor acts as an input for.  */
	OSArray *		ctrlLoops;

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual bool initSymbols( void );
	virtual void free( void );

	// sometimes we need to apply a transform to convert a value read from the sensor into something meaningful.
	// Subclasses should implement this method to perform their transforms.
	//
	// This method operates on the OSNumber object that's passed into it by using the OSNumber::setValue()
	// method.  This should NOT be used on the object that belongs to the IOHWSensor instance, rather it
	// should be used on the platform plugin's object.
	virtual SensorValue applyCurrentValueTransform( SensorValue ) const;

	// notify the control loops that we've got a registered driver
	virtual void notifyCtrlLoops( void );

public:

	/*
	 * Notes on initPlatformSensor( const OSDictionary * ), initPlatformSensor( IOService * ),
	 * and registerDriver( IOSerivce *, const OSDictionary *, bool )
	 *
	 * These routines are cumulatively responsible for initializing an IOPlatformSensor object
	 * and making sure that all the correct data is supplied, parsed, and prioritized.  There
	 * are a number of properties inside a sensor.  Some can be declared by the thermal
	 * profile, some can be supplied by the registering driver, and some, if left unspecified,
	 * will be set to some default value.
	 *
	 * initPlatformSensor( const OSDictionary * ) is invoked when a sensor is being initialized
	 * from a thermal profile entry.  The OSDictionary parameter is the thermal profile entry.
	 *
	 * initPlatformSensor( IOService * ) is invoked when a sensor driver tries to register and
	 * its sensor-id is not found in the thermal profile.  The IOService parameter is the driver
	 * that is sending the registration.
	 *
	 * registerDriver( IOService *, const OSDictionary *, bool ) is invoked after one of the
	 * initPlatformSensor() routines has been invoked (and some of the parameters have already
	 * been set.
	 *
	 * The following properties are REQUIRED for successful completion of initPlatformSensor:
	 *	kIOPPluginSensorIDKey
	 *
	 * The following properties may be supplied by the thermal profile, but will be given
	 * default values if not specified (the sensor driver does not know about these
	 * properties):
	 *
	 *	kIOPPluginThermalLocalizedDescKey		"UNKNOWN_SENSOR"	<OSString>
	 *	kIOPPluginSensorFlagsKey				0x0					<OSNumber>
	 *
	 * The following properties are always supplied by the sensor driver (IOHWSensor) but, if
	 * specified, values in the thermal profile will take precedence and be used instead.
	 *
	 *	kIOPPluginTypeKey
	 *	kIOPPluginVersionKey
	 *	kIOPPluginLocationKey
	 *	kIOPPluginZoneKey
	 *
	 * The following parameters may not be supplied at all, by either the thermal profile
	 * or by the sensor driver.  The value will be searched for with the highest precedence
	 * on the thermal profile, then next on the sensor driver.  If not specified in either
	 * place, a default will be assigned.
	 *
	 *	kIOPPluginPollingPeriodKey				-1					<OSNumber>
	 *
	 */

	/*! @function initPlatformSensor
		@abstract Initialize an IOPlatformSensor object from a thermal profile entry
		@param dict An element from the SensorArray array of the thermal profile
	*/
	virtual IOReturn	initPlatformSensor( const OSDictionary * dict );

	/*! @function initPlatformSensor
		@abstract Initialize an IOPlatformSensor object from an unknown (i.e. not listed in thermal profile) sensor that is trying to register
		@param unknownSensor An IOService reference to the sender of the registration request
		@param dict The OSDictionary supplied with the registration message
	*/
	virtual IOReturn	initPlatformSensor( IOService * unknownSensor, const OSDictionary * dict );

	/*! @function isRegistered
		@abstract Tells whether there is an IOService * associated with this sensor.
	*/
	virtual OSBoolean *	isRegistered(void);

	/*!	@function registerDriver
		@abstract Used to associated an IOService with this sensor, responsible for extracting needed data from the sensor driver instance and sending the polling period to the sensor driver
		@param driver An IOService reference to the sender of the registration request
		@param dict The registration dictionary send by the driver
		@param notify A boolean that can be used by IOPlatformSensor subclasses that can be used to properly synchronize notification to IOPlatformCtrlLoops that this sensor has registered.  See IOPlatformStateSensor for example usage.
	*/
	virtual IOReturn	registerDriver(IOService *driver, const OSDictionary * dict, bool notify = true);

	/*! @function joinedCtrlLoop
		@abstract Called by IOPlatformCtrlLoop when a sensor is added.
		@param aCtrlLoop The IOPlatformCtrlLoop* referencing the control loop. */
	virtual bool		joinedCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop );

	/*! @function leftCtrlLoop
		@abstract Called by IOPlatformCtrlLoop when a sensor is removed from a loop.
		@param aCtrlLoop The control loop that this sensor is leaving. */
	virtual bool		leftCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop );

	/*!	@function memberOfCtrlLoops
		@abstract Accessor for ctrlLoops array, so that the plugin can get a list of control loops affected by a change in this sensor's value/state. */
	virtual OSArray *				memberOfCtrlLoops( void );

	/*!	@function sendMessage
		@abstract Sends a dictionary to the sensor.  The caller is responsible for setting up the dictionary to represent the desired command.  This call can block, as the sensor may use the calling thread to perform I/O. */
	virtual IOReturn				sendMessage( OSDictionary * msg );

	// get a reference to the info dictionary
	virtual OSDictionary *			getInfoDict( void );

	// accessors - this just grabs attributes from the info dict
	// version
	virtual OSNumber *				getVersion( void );

	// sensor-id
	virtual OSNumber *				getSensorID( void );

	// type
	virtual OSString *				getSensorType( void );
	virtual UInt32					getSensorTypeID( void );

	// zone
	virtual OSData *				getSensorZone( void );

	// location
	virtual OSString *				getSensorLocation( void );

	// Desc-Key
	virtual OSString *				getSensorDescKey( void );

	// sensor-flags
	virtual const OSNumber *		getSensorFlags( void );

	// current-value
	virtual SensorValue				getCurrentValue( void );
	virtual void					setCurrentValue( SensorValue newValue );

	/*! @function fetchCurrentValue
		@abstract Read the "current-value" currently stored in the IOHWSensor instance that corresponds to this sensor and return the transformed (if applicable) fresh value.  This does NOT force IOHWSensor to poll the hardware before utilizing the current-value it's published.
	*/
	virtual SensorValue				fetchCurrentValue( void );	// blocks for I/O

	/*!	@function sendForceUpdateMessage
		@abstract Formulates and sends a force-update message to the sensor.  This causes the IOHWSensor driver to poll the hardware and update it's current-value. */
	virtual IOReturn				sendForceUpdate( void );

	/*! @function forceAndFetchCurrentValue
		@abstract Same as fetchCurrentValue(), except calls sendForceUpdate() before fetching, transforming and storing the current-value.
	*/
	virtual SensorValue				forceAndFetchCurrentValue( void );	// blocks for I/O

	// polling period
	virtual UInt32					getPollingPeriodPrimitive( const OSSymbol * key );
	virtual void					setPollingPeriodPrimitive( const OSSymbol * key, UInt32 value );
	virtual UInt32					getPollingPeriod( void );
	virtual void					setPollingPeriod( UInt32 sec );
	virtual UInt32					getPollingPeriodNS( void );
	virtual void					setPollingPeriodNS( UInt32 nsec);

	// this sends the polling period to the sensor
	virtual bool					sendPollingPeriod( void );

	// accessor for sensorDriver member variable
	virtual IOService *				getSensorDriver(void);

/*	
public:

	// First variant takes a preallocated array of thresholds - useful when compiled in
	virtual IOReturn initPlatformSensor (OSDictionary *dict, IOService *sensor, UInt32 maxSensorStates, 
		ThresholdInfo *thresholds);
	// Second variant takes a dictionary - convenient when read from a property
	virtual IOReturn initPlatformSensor (OSDictionary *dict, IOService *sensor, UInt32 maxSensorStates, 
		OSDictionary *thresholdDict);
	// Handle messages from sensor
	virtual IOReturn handleSensorEvent (UInt32 eventType, IOService *fromService, OSDictionary *dict);
*/
};


#endif 	// _IOPLATFORMSENSOR_H