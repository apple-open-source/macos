/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: IOI2CMaxim6690.h,v 1.3 2005/05/19 22:15:32 tsherman Exp $
 *
 *		$Log: IOI2CMaxim6690.h,v $
 *		Revision 1.3  2005/05/19 22:15:32  tsherman
 *		4095546 - SMUSAT sensors - Update driver to read all SAT sensors with one read (raddog)
 *		
 *		Revision 1.2  2004/09/18 01:12:01  jlehrer
 *		Removed APSL header.
 *		
 *		Revision 1.1  2004/06/14 20:26:28  jlehrer
 *		Initial Checkin
 *		
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

#ifndef _IOI2CMaxim6690_H
#define _IOI2CMaxim6690_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOI2C/IOI2CDevice.h>
#include "IOPlatformFunction.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
//#define MAX6690_DEBUG 1

#ifdef MAX6690_DEBUG
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

class IOI2CMaxim6690 : public IOI2CDevice
{
    OSDeclareDefaultStructors(IOI2CMaxim6690)

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

	// Ask the hardware what the temperature is
	IOReturn			getInternalTemp( SInt32 * temp );
	IOReturn			getExternalTemp( SInt32 * temp );

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

#endif	// _IOI2CMaxim6690_H
