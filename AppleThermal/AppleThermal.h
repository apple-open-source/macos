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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * Interface definition for the keylargo I2C interface
 *
 * HISTORY
 *
 */

#ifndef _APPLETHERMAL_H
#define _APPLETHERMAL_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOService.h>

// This is the interface we need to program the fan:
#include <IOKit/i2c/PPCI2CInterface.h>

// Uncomment the following line to log the driver activity
//#define DEBUGMODE

// If this is uncommented we increase the number of checkpoints
// for data consistency:
//#define BEPARANOID

// This allows the setting thermostat values low enough that the fan
// starts.
//#define DOTEST

// The pourpose of the following define is to chnage the behavior
// of this driver. If "SUPPORTS_POWER_MANAGER" is defined the driver
// will "stick around" and change the status of the thermostats when
// the machine sleeps. If the define is commented as soon as the
// thermostats are programmed the driver will unload freeing all the
// memory it uses. The decision to comment the define depends on
// the will to save memory against saving power (in both cases I do
// not believe we are talking about a great amount of memory or power.
#define SUPPORTS_POWER_MANAGER 

// The pourpose of this driver is to program the termostat that
// controls the fan in some powerbooks. Once this operation is
// completed the driver will cease to exist.

class AppleThermal : public IOService
{
    OSDeclareDefaultStructors(AppleThermal)

private:
    enum {
        kNumberOfOnOffTemps = 4     // the number of entries in the temperture array
    };

    enum {
       kThermostatAddress  = 0x49   // the address of the thermostat in the uni-n i2c bus 
    };

    // This is the description of the therma info data in
    // the of property:
    typedef struct ThermalInfo
    {
        UInt8  thermalDesign;	     // unique ID per implementation
        UInt8  numberFans;           // total number of software-controllable fans in system
        UInt8  numberThermostats;    // number of software-controllable thermostats/thermometers in system
        UInt8  reserved3;
        UInt32 reserved4_7;
        UInt16 onOffTemperatures[kNumberOfOnOffTemps]; // array of on/off temperature pairs in ¡C (+127 to -128 ¡C)
    } ThermalInfo;
    typedef ThermalInfo *ThermalInfoPtr;

    // pmThermalDesign values
    enum {
        kThermalDesign_Unknown = 0,
        kThermalDesign_Pismo   = 1,    // "two DS1775R1's hanging off Uni-N I2C busses 0 and 1"
        kThermalDesign_2000    = 2     // same as Pismo with added information on thermal thressholds
    };

    // This is instead used locally to have a simpler rapresentation of the terperatures:
    typedef struct TempRanges {
        SInt8 on;
        SInt8 off;
    } TempRanges;
    typedef TempRanges *TempRangesPtr;
    
    // I wish to hold here a copy of the meaningful informations, so
    // I can perform a sanity check (mostly confirming that I have the
    // data for the correcr machine.
    UInt8  thermalDesign;	 // unique ID per implementation
    UInt8  numberFans;           // total number of software-controllable fans in system
    UInt8  numberThermostats;    // number of software-controllable thermostats/thermometers in system
    TempRangesPtr tempRanges;    // array of on/off temperature pairs in ¡C (+127 to -128 ¡C)

    // This provides access to the thermostats registers:
    PPCI2CInterface *interface;

    // Power managment variables:
    bool wakingUpFromSleep;

    // Attaches to the i2c interface:
    bool findAndAttachI2C(IOService *provider);
    bool detachFromI2C(IOService *provider);

    // Opens and closes the i2c bus:
    virtual bool openI2C(UInt8 id);
    virtual void closeI2C();

    // Write and read on the i2c.
    virtual bool writeI2C(SInt8 *data, UInt32 size);
    virtual bool readI2C(SInt8 *data, UInt32 size);
    
    // checks if the data we are holding is for the machine we are
    // running on:
    virtual bool dataIsValid();

    // programs a set of on/off in the given thermostat:
    bool setThermostat(UInt32 id, SInt8 on, SInt8 off);

    // returns a set of on/off in the given thermostat:
    bool getThermostat(UInt32 id, SInt8 *on, SInt8 *off);

    // programs all the thermostats
    bool setThermostats();
        
    // puts the thermostat in sleep mode:
    bool setSleepMode();

    // retrives the thermal properties from the provider
    bool retriveThermalProperty(IOService*);
    
public:
    // the standard inherits from IOService:
    virtual void free();
    virtual bool start(IOService*);

    // read the temperature (C) of the thermostat of the id given
    UInt32 readThermostatTemperature (UInt8 inID);

    // read the settings of the given thermostat.
    bool readThermostatSettings (UInt8 id, SInt8 *lOn,SInt8 *lOff);

    // Power managment:
    IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice);
};

#endif _APPLETHERMAL_H
