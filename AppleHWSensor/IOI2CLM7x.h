/*	File: $Id: IOI2CLM7x.h,v 1.3 2004/12/15 04:44:51 jlehrer Exp $
 *
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Tom Sherman
 *
 *		$Log: IOI2CLM7x.h,v $
 *		Revision 1.3  2004/12/15 04:44:51  jlehrer
 *		[3867728] Support for failed hardware.
 *		
 *		Revision 1.2  2004/09/18 01:12:01  jlehrer
 *		Removed APSL header.
 *		
 *
 *
 */

#ifndef _IOI2CLM7x_H
#define _IOI2CLM7x_H

#include <IOI2C/IOI2CDevice.h>

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

class IOI2CLM7x : public IOI2CDevice
{
    OSDeclareDefaultStructors(IOI2CLM7x)

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

	bool	fRegistersAreSaved;
	bool	fInitHWFailed; // flag used to indicate hardware is not responding.

    // Varibles
    const OSSymbol 			*sGetSensorValueSym;

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

    IOReturn	 			initHW(void);	// method to program DS1775 hardware
    IOReturn				saveRegisters(void);
    IOReturn				restoreRegisters(void);

    IOReturn 				getTemperature(SInt32 *);

public:

    // Methods
    virtual bool 			start(IOService *);
	virtual void			free ( void );

	using IOService::callPlatformFunction;
    virtual IOReturn 		callPlatformFunction(const OSSymbol *, bool, void *, void *, void *, void *);
                                             
	// Power handling methods:
	virtual void processPowerEvent(UInt32 eventType);
};

#endif // _IOI2CLM7x_H
