/*
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: IOI2CAD741x.h,v 1.2 2004/09/18 01:12:01 jlehrer Exp $
 *
 *		$Log: IOI2CAD741x.h,v $
 *		Revision 1.2  2004/09/18 01:12:01  jlehrer
 *		Removed APSL header.
 *		
 *		Revision 1.1  2004/06/14 20:26:28  jlehrer
 *		Initial Checkin
 *		
 *		Revision 1.6  2004/01/30 23:52:00  eem
 *		[3542678] IOHWSensor/IOHWControl should use "reg" with version 2 thermal parameters
 *		Remove AppleSMUSensor/AppleSMUFan since that code will be in AppleSMUDevice class.
 *		Fix IOHWMonitor, AppleMaxim6690, AppleAD741x to use setPowerState() API instead of
 *		unsynchronized powerStateWIllChangeTo() API.
 *		
 *		Revision 1.5  2003/07/02 22:56:58  dirty
 *		Add CVS log and id keywords.
 *		
 */

#ifndef _IOI2CAD741x_H
#define _IOI2CAD741x_H

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
//#define AD741X_DEBUG 1

#ifdef AD741X_DEBUG
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
#define kHWTempSensorNubName		"temp-sensor"
#define kHWADCSensorNubName			"adc-sensor"
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

class IOI2CAD741x : public IOI2CDevice
{
    OSDeclareDefaultStructors(IOI2CAD741x)

private:

enum {
	// Flag to tell which model device we have
	// Only AD7417 is currently supported
	kDeviceAD7416		= 0x00,
	kDeviceAD7417		= 0x01,
	kDeviceAD7418		= 0x02,

	// AD741x Register SubAddresses
	kTempValueReg		= 0x00,
	kConfig1Reg			= 0x01,
	kTHystSetpointReg	= 0x02,
	kTOTISetpointReg	= 0x03,
	kADCReg				= 0x04,
	kConfig2Reg			= 0x05,

	// Config1 Reg
	kTempChannel		= 0x00,
	kAD1Channel			= 0x01,
	kAD2Channel			= 0x02,
	kAD3Channel			= 0x03,
	kAD4Channel			= 0x04,

	kCfg1ChannelShift	= 5,
	kCfg1ChannelMask	= 0xE0
};

	// We need a way to map a hwsensor-id onto a temperature
	// channel.  This is done by assuming that the sensors are
	// listed in the following orders:
	//		Internal, External
	//
	// As such, the parseSensorParamsAndCreateNubs method is
	// responsible for populating this array with the sensor ids
	// from the device tree.
	UInt32				fHWSensorIDMap[5];

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

	// Power Management routines  --  we only do power management stuff if there
	// are one or more platform functions that activate at sleep or wake
	IOPMrootDomain		*pmRootDomain;
	void doSleep(void);
	void doWake(void);

	// Perform a software reset of the part
	IOReturn			softReset( void );

	// Read the device's sensor channels
	IOReturn			getTemperature( SInt32 * temp );
	IOReturn			getADC( UInt8 channel, UInt32 * sample );

public:

    virtual bool		start( IOService * nub );
    virtual void		stop( IOService * nub );
//	virtual bool		init( OSDictionary * dict );
	virtual void		free( void );
//	virtual void processPowerEvent(UInt32 eventType);

//	virtual IOReturn powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*);

	using IOService::callPlatformFunction;
    virtual IOReturn 	callPlatformFunction(const OSSymbol *functionName,
                                             bool waitForFunction, 
                                             void *param1, void *param2,
                                             void *param3, void *param4);
};

#endif	// _IOI2CAD741x_H
