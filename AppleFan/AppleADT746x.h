/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#ifndef _APPLEADT746x_H
#define _APPLEADT746x_H

#include <IOKit/IOService.h>
#include <IOKit/i2c/PPCI2CInterface.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include "ADT746x.h"

// Uncomment to enable debug output
//#define APPLEADT746x_DEBUG 1

#ifdef DLOG
#undef DLOG
#endif

#ifdef APPLEADT746x_DEBUG
#define DLOG(fmt, args...) IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...) do {} while(0)
#endif

#define kNumRetries 10	// I2C transactions are retried before failing

/*
 * If we find a property key "playform-getTemp" in our provider's node,
 * we DO NOT want to load.  That is AppleFan territory.
 */
#define kGetTempSymbol "platform-getTemp"

#define kGetSensorValueSymbol "getSensorValue"

/*
 * Probe scores
 */
enum {
	kADT746xProbeFailureScore = 0,
	kADT746xProbeSuccessScore = 11000
};

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

/*
 * ADT746x device nodes will be identified by their compatible property
 */
#define	kADT7460Compatible	"adt7460"
#define kADT7467Compatible	"adt7467"

class AppleADT746x : public IOService
{

	OSDeclareDefaultStructors(AppleADT746x)

	private:
		PPCI2CInterface	*fI2CInterface;	// cached reference to my I2C driver

		UInt8	fI2CBus;	// I2C bus (synonymous with port)
		UInt8	fI2CAddr;	// I2C address, 7-bit

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
		UInt32 fHWSensorIDMap[3];

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
		unsigned parseSensorParamsAndCreateNubs(IOService *provider);

		bool doI2COpen(void);
		void doI2CClose(void);
		bool doI2CRead(UInt8 sub, UInt8 *bytes, UInt16 len);
		bool doI2CWrite(UInt8 sub, UInt8 *bytes, UInt16 len);

		IOReturn getLocalTemp(SInt32 *temperature);
		IOReturn getRemote1Temp(SInt32 *temperature);
		IOReturn getRemote2Temp(SInt32 *temperature);

		IOReturn getVoltage(SInt32 *voltage);

		IOReturn getFanTach(SInt32 *fanSpeed, SInt16 whichFan);

	public:
		virtual IOService *probe(IOService *provider, SInt32 *score);
		virtual bool init(OSDictionary *dict);
		virtual void free(void);
		virtual bool start(IOService *provider);
		virtual void stop(IOService *provider);
		virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4);
                                
		// used to get power state change notifications
		virtual IOReturn powerStateWillChangeTo(IOPMPowerFlags, unsigned long, IOService*);
		virtual IOReturn powerStateDidChangeTo(IOPMPowerFlags, unsigned long, IOService*);

};

#endif	// _APPLEADT746x_H