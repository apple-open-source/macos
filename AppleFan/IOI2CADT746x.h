/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CADT746x.h,v 1.3 2005/01/08 03:42:49 jlehrer Exp $
 *
 *		$Log: IOI2CADT746x.h,v $
 *		Revision 1.3  2005/01/08 03:42:49  jlehrer
 *		[3944335] Set flag to read SMB interrupt registers 0x41 and 0x42 on wake from sleep
 *		
 *		Revision 1.2  2004/12/15 04:15:55  jlehrer
 *		[3867728] Support for failed hardware.
 *		
 *		Revision 1.1  2004/11/04 21:11:28  jlehrer
 *		Initial checkin.
 *		
 *
 *
 */

#ifndef _IOI2CADT746x_H
#define _IOI2CADT746x_H

#include <IOI2C/IOI2CDevice.h>
#include "ADT746x.h"

// Uncomment to enable debug output
// #define IOI2CADT746x_DEBUG 1

#ifdef DLOG
#undef DLOG
#endif

#ifdef IOI2CADT746x_DEBUG
#define DLOG(fmt, args...) IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...) do {} while(0)
#endif

#define ERRLOG(fmt, args...) IOLog(fmt, ## args)

#define kNumRetries 10	// I2C transactions are retried before failing

/*
 * If we find a property key "playform-getTemp" in our provider's node,
 * we DO NOT want to load.  That is AppleFan territory.
 */
#define kGetTempSymbol "platform-getTemp"

#define kGetSensorValueSymbol "getSensorValue"

/*
 * Sensor device tree property keys
 *  - used when parsing the device tree
 */
#define kDTSensorParamsVersionKey	"hwsensor-params-version"
#define kDTSensorIDKey			"hwsensor-id"
#define kDTSensorZoneKey		"hwsensor-zone"
#define kDTSensorTypeKey		"hwsensor-type"
#define kDTSensorLocationKey		"hwsensor-location"
#define kDTSensorPollingPeriodKey	"hwsensor-polling-period"

#define	kDTTemperatureSensorType	"temperature"
#define	kDTVoltageSensorType		"voltage"
#define	kDTFanSpeedSensorType		"fanspeed"

/*
 * Sensor nub property keys
 *  - used when creating the sensor nub in the device tree
 */
#define kHWSensorTemperatureNubName	"temp-sensor"
#define kHWSensorVoltageNubName		"voltage-sensor"
#define kHWSensorFanSpeedNubName	"fanspeed-sensor"
#define kHWSensorParamsVersionKey	"version"
#define kHWSensorIDKey			"sensor-id"
#define kHWSensorZoneKey		"zone"
#define kHWSensorTypeKey		"type"
#define kHWSensorLocationKey		"location"
#define kHWSensorPollingPeriodKey	"polling-period"

#define	kFanTachOne			1
#define	kFanTachTwo			2

/*
 * If the polling period is 0xFFFFFFFF it is treated the same
 * as if it didn't ever exist
 */
enum {
	kHWSensorPollingPeriodNULL = 0xFFFFFFFF
};

class IOI2CADT746x : public IOI2CDevice
{

	OSDeclareDefaultStructors(IOI2CADT746x)

	private:
		UInt8	fDeviceID;	// Contents of the Device ID Register
					
		// On Q54 ( 12" G4 1Ghz miniPB) there is a chance we went to sleep due
		// to an overtemp situation and we need to read both status registers
		// to make sure we reset its status as approprate.
	
		bool	fClearSMBAlertStatus; 
	
		// We need a way to map a hwsensor-id onto a temperature
		// channel.  This is done by assuming that the sensors are
		// listed in the following orders:
		//		ADM1030: Local, Remote
		//		ADM1031: Local, Remote1, Remote2
		//
		// As such, the parseSensorParamsAndCreateNubs method is
		// responsible for populating this array with the sensor ids
		// from the device tree.
		UInt32 fHWSensorIDMap[6];
	
		const OSSymbol *getSensorValueSymbol;
	
		// This method parses the hwsensor parameters from the device
		// tree.  It supports version 1 of the hwsensor device tree
		// property encodings.  This driver's probe method verfies the
		// presence of sensor params of the proper version.
		//
		// While parsing the sensor parameters, this method creates
		// nubs, populated with the appropriate properties, for
		// IOHWSensor drivers to match against.  Each created nub
		// is registered with IOService::registerService() so that
		// matching is started.
		//
		// The return value of this method is the number of sensor
		// nubs created.
		OSArray *parseSensorParamsAndCreateNubs(IOService *provider);
	
		IOReturn getLocalTemp(SInt32 *temperature);
		IOReturn getRemote1Temp(SInt32 *temperature);
		IOReturn getRemote2Temp(SInt32 *temperature);
	
		IOReturn getVoltage(SInt32 *voltage);
	
		IOReturn getFanTach(SInt32 *fanSpeed, SInt16 whichFan);

		virtual void processPowerEvent(UInt32 eventType);
	
	public:
		virtual bool start(IOService *provider);
//		virtual void free(void);
	
		virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4);
};

#endif	// _APPLEADT746x_H
