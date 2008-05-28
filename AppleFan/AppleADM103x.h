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

#ifndef _APPLEADM103x_H
#define _APPLEADM103x_H

#include <IOKit/IOService.h>
#include <IOKit/i2c/PPCI2CInterface.h>
#include "ADM103x.h"

// Uncomment to enable debug output
// #define APPLEADM103x_DEBUG 1

#ifdef DLOG
#undef DLOG
#endif

#ifdef APPLEADM103x_DEBUG
#define DLOG(fmt, args...) IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
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
	kADM103xProbeFailureScore = 0,
	kADM103xProbeSuccessScore = 11000
};

/*
 * Sensor device tree property keys
 *  - used when parsing the device tree
 */
#define kDTSensorParamsVersionKey	"hwsensor-params-version"
#define kDTSensorIDKey				"hwsensor-id"
#define kDTSensorZoneKey			"hwsensor-zone"
#define kDTSensorTypeKey			"hwsensor-type"
#define kDTSensorLocationKey		"hwsensor-location"
#define kDTSensorPollingPeriodKey	"hwsensor-polling-period"

/*
 * Sensor nub property keys
 *  - used when creating the sensor nub in the device tree
 */
#define kHWSensorNubName			"temp-sensor"
#define kHWSensorParamsVersionKey	"version"
#define kHWSensorIDKey				"sensor-id"
#define kHWSensorZoneKey			"zone"
#define kHWSensorTypeKey			"type"
#define kHWSensorLocationKey		"location"
#define kHWSensorPollingPeriodKey	"polling-period"

/*
 * If the polling period is 0xFFFFFFFF it is treated the same
 * as if it didn't ever exist
 */
enum {
	kHWSensorPollingPeriodNULL = 0xFFFFFFFF
};

/*
 * ADM103x device nodes will be identified by their compatible property
 */
#define kADM1030Compatible "adm1030"
#define kADM1031Compatible "adm1031"

class AppleADM103x : public IOService
{

	OSDeclareDefaultStructors(AppleADM103x)

	private:
		PPCI2CInterface	*fI2CInterface;	// cached reference to my I2C driver

		UInt8	fI2CBus;	// I2C bus (synonymous with port)
		UInt8	fI2CAddr;	// I2C address, 7-bit

		UInt8	 fDeviceID;	// Contents of the Device ID Register

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

	public:
		virtual IOService *probe(IOService *provider, SInt32 *score);
		virtual bool init(OSDictionary *dict);
		virtual void free(void);
		virtual bool start(IOService *provider);
		virtual void stop(IOService *provider);
		virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4);
};

#endif	// _APPLEADM103x_H