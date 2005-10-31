/*	File: $Id: IOI2CMaxim1631.h,v 1.1 2005/04/01 02:53:22 galcher Exp $
 *
 * Copyright (c) 2005 Apple Computer, Inc.  All rights reserved.
 *
 *		$Log: IOI2CMaxim1631.h,v $
 *		Revision 1.1  2005/04/01 02:53:22  galcher
 *		Items added to repository.
 *		
 */

#ifndef _IOI2CMaxim1631_H
#define _IOI2CMaxim1631_H

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
// #define MAX1631_DEBUG 1

#ifdef MAX1631_DEBUG
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

class IOI2CMaxim1631 : public IOI2CDevice
{
    OSDeclareDefaultStructors(IOI2CMaxim1631)

private:

enum {
	// Maxim 1631 Command Bytes (subaddresses)
	kReadCurrentTemp		= 0xAA,
        kReadLocalHighLimit		= 0xA1,
        kReadLocalLowLimit		= 0xA2,
	kAccessConfigurationByte	= 0xAC,

	kStartConvertT			= 0x51,
	kStopConvertT			= 0x52,
	kSoftwareReset			= 0x54
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

#endif	// _IOI2CMaxim1631_H
