/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: AppleMaxim6690.h,v 1.5 2004/01/30 23:52:00 eem Exp $
 *
 *  DRI: Eric Muehlhausen
 *
 *		$Log: AppleMaxim6690.h,v $
 *		Revision 1.5  2004/01/30 23:52:00  eem
 *		[3542678] IOHWSensor/IOHWControl should use "reg" with version 2 thermal parameters
 *		Remove AppleSMUSensor/AppleSMUFan since that code will be in AppleSMUDevice class.
 *		Fix IOHWMonitor, AppleMaxim6690, AppleAD741x to use setPowerState() API instead of
 *		unsynchronized powerStateWIllChangeTo() API.
 *		
 *		Revision 1.4  2003/07/03 01:51:29  dirty
 *		Add CVS log and id keywords.
 *		
 */

#ifndef _APPLEMAXIM6690_H
#define _APPLEMAXIM6690_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include "IOPlatformFunction.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
// #define MAX6690_DEBUG 1

#ifdef MAX6690_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// Symbols for I2C access
#define kWriteI2Cbus                            "writeI2CBus"
#define kReadI2Cbus                             "readI2CBus"
#define kOpenI2Cbus                             "openI2CBus"
#define kCloseI2Cbus                            "closeI2CBus"
#define kSetDumbMode                            "setDumbMode"
#define kSetStandardMode                        "setStandardMode"
#define kSetStandardSubMode                     "setStandardSubMode"
#define kSetCombinedMode                        "setCombinedMode"
#define kNumRetries			10

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

class AppleMaxim6690 : public IOService
{
    OSDeclareDefaultStructors(AppleMaxim6690)

private:

enum {
	// Maxim 6690 Command Bytes (subaddresses)
	kReadInternalTemp			= 0x00,
	kReadExternalTemp			= 0x01,
	kReadStatusByte				= 0x02,
	kReadConfigurationByte		= 0x03,
	kReadConversionRateByte		= 0x04,
	kReadInternalHighLimit		= 0x05,
	kReadInternalLowLimit		= 0x06,
	kReadExternalHighLimit		= 0x07,
	kReadExternalLowLimit		= 0x08,
	kWriteConfigurationByte		= 0x09,
	kWriteConversionRateByte	= 0x0A,
	kWriteInternalHighLimit		= 0x0B,
	kWriteInternalLowLimit		= 0x0C,
	kWriteExternalHighLimit		= 0x0D,
	kWriteExternalLowLimit		= 0x0E,
	kOneShot					= 0x0F,
	kReadExternalExtTemp		= 0x10,
	kReadInternalExtTemp		= 0x11,
	kReadDeviceID				= 0xFE,
	kReadDeviceRevision			= 0xFF
};

enum {
	kI2CDumbMode		= 0x01,
	kI2CStandardMode	= 0x02,
	kI2CStandardSubMode	= 0x03,
	kI2CCombinedMode	= 0x04,
	kI2CUnspecifiedMode	= 0x05
};

	bool				fSleep;

	IOService *			fI2C_iface;
	UInt8				fI2CBus;
	UInt8				fI2CAddress;

	// Some I2C platform functions expect the data that was read in one
	// transaction to persist for a later modify-write operation.  This
	// buffer will hold the most recent I2C read data.
#define READ_BUFFER_LEN	16
	UInt8			fI2CReadBuffer[READ_BUFFER_LEN];
	UInt16			fI2CReadBufferSize;

	// We need a way to map a hwsensor-id onto a temperature
	// channel.  This is done by assuming that the sensors are
	// listed in the following orders:
	//		Internal, External
	//
	// As such, the parseSensorParamsAndCreateNubs method is
	// responsible for populating this array with the sensor ids
	// from the device tree.
	UInt32				fHWSensorIDMap[2];

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
	OSArray * parseSensorParamsAndCreateNubs(IOService *nub);

	const OSSymbol *	fGetSensorValueSym;

	// Platform functions at start, stop, sleep and wake time are supported
	// for the purpose of initializing the hardware.  I2C commands are supported.
	OSArray				*fPlatformFuncArray;
	bool performFunction(IOPlatformFunction *func, void *pfParam1, void *pfParam2,
			void *pfParam3, void *pfParam4); 

	// Power Management routines  --  we only do power management stuff if there
	// are one or more platform functions that activate at sleep or wake
	IOPMrootDomain		*pmRootDomain;
	void doSleep(void);
	void doWake(void);

	// Ask the hardware what the temperature is
	IOReturn			getInternalTemp( SInt32 * temp );
	IOReturn			getExternalTemp( SInt32 * temp );

	// helper methods for i2c stuff, uses fI2C_iface, fI2CBus and fI2CAddress
	bool				openI2C();
	void				closeI2C();
	bool				writeI2C( UInt8 subAddr, UInt8 * data, UInt16 size );
	bool				readI2C( UInt8 subAddr, UInt8 * data, UInt16 size );
	bool				setI2CDumbMode( void );
	bool				setI2CStandardMode( void );
	bool				setI2CStandardSubMode( void );
	bool				setI2CCombinedMode( void );

public:

    virtual bool		start( IOService * nub );
    virtual void		stop( IOService * nub );
	virtual bool		init( OSDictionary * dict );
	virtual void		free( void );

	virtual IOReturn powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*);

    virtual IOReturn 	callPlatformFunction(const OSSymbol *functionName,
                                             bool waitForFunction, 
                                             void *param1, void *param2,
                                             void *param3, void *param4);
};

#endif	// _APPLEMAXIM6690_H
