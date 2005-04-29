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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *		
 */

#ifndef _APPLEMAXIM1989_H
#define _APPLEMAXIM1989_H

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
// #define MAX1989_DEBUG 1

#ifdef MAX1989_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// Symbols for I2C access
#define kWriteI2Cbus                    "writeI2CBus"
#define kReadI2Cbus                     "readI2CBus"
#define kOpenI2Cbus                     "openI2CBus"
#define kCloseI2Cbus                    "closeI2CBus"
#define kSetDumbMode                    "setDumbMode"
#define kSetStandardMode                "setStandardMode"
#define kSetStandardSubMode             "setStandardSubMode"
#define kSetCombinedMode              	"setCombinedMode"
#define kNumRetries			10

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

/*
 * If the polling period is 0xFFFFFFFF it is treated the same
 * as if it didn't ever exist
 */
enum {
	kHWSensorPollingPeriodNULL = 0xFFFFFFFF
};

class AppleMaxim1989 : public IOService
{
    OSDeclareDefaultStructors(AppleMaxim1989)

private:

enum {
	// Maxim 1989 Command Bytes (subaddresses)
	kReadLocalTemp			= 0x00,
	kReadRemoteDX1Temp		= 0x01,
	kReadRemoteDX2Temp		= 0x02,
	kReadRemoteDX3Temp		= 0x03,
	kReadRemoteDX4Temp		= 0x04,
	kReadStatusByte1		= 0x05,
	kReadStatusByte2		= 0x06,
	kReadConfigurationByte		= 0x07,
        kReadLocalHighLimit		= 0x08,
        kReadLocalLowLimit		= 0x09,
        kReadRemoteDX1HighLimit		= 0x0A,
        kReadRemoteDX1LowLimit		= 0x0B,
        kReadRemoteDX2HighLimit		= 0x0C,
        kReadRemoteDX2LowLimit		= 0x0D,
        kReadRemoteDX3HighLimit		= 0x0E,
        kReadRemoteDX3LowLimit		= 0x0F,
        kReadRemoteDX4HighLimit		= 0x10,
        kReadRemoteDX4LowLimit		= 0x11,
	kWriteConfigurationByte		= 0x12,
	kWriteLocalHighLimit		= 0x13,
	kWriteLocalLowLimit		= 0x14,
	kWriteRemoteDX1HighLimit	= 0x15,
	kWriteRemoteDX1LowLimit		= 0x16,
	kWriteRemoteDX2HighLimit	= 0x17,
	kWriteRemoteDX2LowLimit		= 0x18,
	kWriteRemoteDX3HighLimit	= 0x19,
	kWriteRemoteDX3LowLimit		= 0x1A,
	kWriteRemoteDX4HighLimit	= 0x1B,
	kWriteRemoteDX4LowLimit		= 0x1C,
        
	kReadManufactureID		= 0xFE,
	kReadDeviceID			= 0xFF
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

	const OSSymbol *	fGetSensorValueSym;

	// Platform functions at start, stop, sleep and wake time are supported
	// for the purpose of initializing the hardware.  I2C commands are supported.
	OSArray			*fPlatformFuncArray;
	bool 			performFunction(IOPlatformFunction *func, void *pfParam1, void *pfParam2,
                                    void *pfParam3, void *pfParam4); 

	// Power Management routines  --  we only do power management stuff if there
	// are one or more platform functions that activate at sleep or wake
	IOPMrootDomain		*pmRootDomain;
	void 			doSleep(void);
	void 			doWake(void);

	IOReturn		publishChildren(IOService *);
        
	// Ask the hardware what the temperature is
	IOReturn		getTemp( UInt32 Reg, SInt32 * temp );

	// helper methods for i2c stuff, uses fI2C_iface, fI2CBus and fI2CAddress
	bool			openI2C();
	void			closeI2C();
	bool			writeI2C( UInt8 subAddr, UInt8 * data, UInt16 size );
	bool			readI2C( UInt8 subAddr, UInt8 * data, UInt16 size );
	bool			setI2CDumbMode( void );
	bool			setI2CStandardMode( void );
	bool			setI2CStandardSubMode( void );
	bool			setI2CCombinedMode( void );

public:

        virtual bool		start( IOService * nub );
        virtual void		stop( IOService * nub );
	virtual bool		init( OSDictionary * dict );
	virtual void		free( void );

	virtual IOReturn 	powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*);

        virtual IOReturn 	callPlatformFunction(const OSSymbol *functionName,
                                             bool waitForFunction, 
                                             void *param1, void *param2,
                                             void *param3, void *param4);
};

#endif	// _APPLEMAXIM1989_H
