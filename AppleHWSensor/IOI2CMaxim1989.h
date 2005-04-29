/*	File: $Id: IOI2CMaxim1989.h,v 1.2 2004/09/18 01:12:01 jlehrer Exp $
 *
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *		$Log: IOI2CMaxim1989.h,v $
 *		Revision 1.2  2004/09/18 01:12:01  jlehrer
 *		Removed APSL header.
 *		
 *
 *
 */

#ifndef _IOI2CMaxim1989_H
#define _IOI2CMaxim1989_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include "IOPlatformFunction.h"
#include <IOI2C/IOI2CDevice.h>

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

class IOI2CMaxim1989 : public IOI2CDevice
{
    OSDeclareDefaultStructors(IOI2CMaxim1989)

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

	const OSSymbol *	fGetSensorValueSym;

	IOReturn		publishChildren(IOService *);
        
	// Ask the hardware what the temperature is
	IOReturn		getTemp( UInt32 Reg, SInt32 * temp );

public:

        virtual bool		start( IOService * nub );
        virtual void		stop( IOService * nub );
	virtual bool		init( OSDictionary * dict );
	virtual void		free( void );

	using IOService::callPlatformFunction;
        virtual IOReturn 	callPlatformFunction(const OSSymbol *functionName,
                                             bool waitForFunction, 
                                             void *param1, void *param2,
                                             void *param3, void *param4);
};

#endif	// _IOI2CMaxim1989_H
