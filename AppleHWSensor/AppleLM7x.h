/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 *  DRI: Tom Sherman 
 *
 */

#ifndef _APPLELM7X_H
#define _APPLELM7X_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/IOMessage.h>

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
// #define LM7X_DEBUG 1

#ifdef LM7X_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// Method names for the callPlatformFunction:
#define kWriteI2Cbus			"writeI2CBus"
#define kReadI2Cbus			"readI2CBus"
#define kOpenI2Cbus			"openI2CBus"
#define kCloseI2Cbus			"closeI2CBus"
#define kSetPollingMode			"setPollingMode"
#define kSetStandardMode		"setStandardMode"
#define kSetStandardSubMode		"setStandardSubMode"
#define kSetCombinedMode		"setCombinedMode"

// Configuration Register bit definitions
#define kCfgRegSD			0x01
#define kCfgRegTM			0x02
#define kCfgRegPOL			0x04
#define kCfgRegF0			0x08
#define kCfgRegF1			0x10
#define kCfgRegR0			0x20
#define kCfgRegR1			0x40
// 0x80 bit is unused

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

/*
 * Sensor nub property keys
 *  - used when creating the sensor nub in the device tree
 */
#define kHWSensorNubName		"temp-sensor"
#define kHWSensorParamsVersionKey	"version"
#define kHWSensorIDKey			"sensor-id"
#define kHWSensorZoneKey		"zone"
#define kHWSensorTypeKey		"type"
#define kHWSensorLocationKey		"location"
#define kHWSensorPollingPeriodKey	"polling-period"

// Compatible string for LM7x
#define kLM7xCompatibleString1	"ds1775"
#define kLM7xCompatibleString2	"lm75"

#define kTriesToAttempt 5

enum
{
	kLM7xOffState = 0,
	kLM7xSleepState = 1,
	kLM7xOnState = 2,
	kLM7xNumStates = 3
};

class AppleLM7x : public IOService
{
    OSDeclareDefaultStructors(AppleLM7x)

private:

    enum // DS1995 register definitions
    {
        kTemperatureReg = 0x00,
        kConfigurationReg = 0x01,
        kT_hystReg = 0x02,
        kT_osReg = 0x03
    };

    struct savedRegisters_t
    {
        UInt16	Temperature;
        UInt8	Configuration;
        UInt16	Thyst;
        UInt16	Tos;
    } savedRegisters;   

    // Varibles
    UInt8 					kLM7xAddr, kLM7xBus;
    IOService	 			*interface;
    const OSSymbol 			*callPlatformFunction_getTempForIOHWSensorSymbol,
							*sOpenI2Cbus,
							*sCloseI2Cbus,
							*sSetPollingMode,
							*sSetStandardSubMode,
							*sSetCombinedMode,
							*sWriteI2Cbus,
							*sReadI2Cbus,
							*sGetSensorValueSym;
    IOPMrootDomain			*pmRootDomain;

    // We need a way to map a hwsensor-id onto a temperature
    // channel.  This is done by assuming that the sensors are
    // listed in the following orders:
    //		Internal, External
    //
    // As such, the parseSensorParamsAndCreateNubs method is
    // responsible for populating this array with the sensor ids
    // from the device tree.
    UInt32					fHWSensorIDMap[5];

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
    // The return value of this method is an array of IOService *
    // referring to the sensor nubs created
    OSArray 				*parseSensorParamsAndCreateNubs(IOService *nub);

    IOReturn	 			initHW(IOService *provider);	// method to program DS1775 hardware
    IOReturn	 			openI2C(UInt8 id);
    IOReturn	 			closeI2C();
    IOReturn	 			writeI2C(UInt8 subAddr, UInt8 *data, UInt16 size);
    IOReturn	 			readI2C(UInt8 subAddr, UInt8 *data, UInt16 size);
    IOReturn				saveRegisters();
    IOReturn				restoreRegisters();

public:

    // Variables
    static bool				systemIsRestarting; // flag reflecting restart state (global to class members)

    // Methods
    virtual bool 			start(IOService *);
    IOService				*processNewNub(IOService *provider);
    virtual void 			stop(IOService *);

    IOReturn 				getTemperature(SInt32 *);
    virtual IOReturn 		callPlatformFunction(const OSSymbol *, bool, void *, void *, void *, void *);
                                             
	// Power handling methods:
	virtual IOReturn		setPowerState(unsigned long, IOService *);
	static IOReturn			sysPowerDownHandler(void *, void *, UInt32, IOService *, void *, vm_size_t);
};

#endif _APPLELM7X_H
