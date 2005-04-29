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

#ifndef _APPLELM8X_H
#define _APPLELM8X_H

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOMessage.h>

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
// #define LM8X_DEBUG 1

#ifdef LM8X_DEBUG
#define DLOG(fmt, args...)			IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// Method names for the callPlatformFunction:
#define kWriteI2Cbus				"writeI2CBus"
#define kReadI2Cbus					"readI2CBus"
#define kOpenI2Cbus					"openI2CBus"
#define kCloseI2Cbus				"closeI2CBus"
#define kSetPollingMode				"setPollingMode"
#define kSetStandardMode			"setStandardMode"
#define kSetStandardSubMode			"setStandardSubMode"
#define kSetCombinedMode			"setCombinedMode"

// DeviceTree Sensor Properties
#define kDTSensorDeviceTypeKey		"device_type"

// Compatible string for LM8x
#define kLM8xCompatibleString1		"lm87cimt"

// Number of I2C attempts before failing
#define kTriesToAttempt				5

enum
{
	kLM8xOffState = 0,
	kLM8xSleepState = 1,
	kLM8xOnState = 2,
	kLM8xNumStates = 3
};

enum
{
	kTypeTemperature = 0,
	kTypeADC = 1,
	kTypeRPM = 2,
	kTypeVoltage = 3
};

struct LogicalUnitNumberTable
{
//	UInt32	SensorID;
	UInt32	SubAddress;
	SInt32	ConversionMultiple;
	UInt8	type;
}; 

class AppleLM8x : public IOService
{
	OSDeclareDefaultStructors(AppleLM8x)

private:

	enum
	{
		kTestRegister					= 0x15,
		kChannelModeRegister			= 0x16,
		kInternalTempHighLimit			= 0x17,
		kExternalTempHighLimit			= 0x18,
		kDACDataRegister				= 0x19, // Start of Value RAM
		kAIN1LowLimit					= 0x1A,
		kAIN2LowLimit					= 0x1B,
		k25VExtTemp2Reading				= 0x20, // Register is optionally '2.5V' or 'Ext. Temp. 2'
		kVccp1Reading					= 0x21,
		kVccReading						= 0x22, // Register is optionally '3.3V' or '5V'
		k5VReading						= 0x23,
		k12VReading						= 0x24,
		kVccp2Reading					= 0x25,
		kExternelTemperature1Reading	= 0x26,
		kInternelTemperatureReading		= 0x27,
		kFan1AIN1Reading				= 0x28, // Register is optionally 'FAN1' or 'AIN1'
		kFan2AIN2Reading				= 0x29, // Register is optionally 'FAN2' or 'AIN2'
		k25VExtTemp2HighLimit			= 0x2B, // Register is optionally '2.V' or 'Ext. Temp. 2' high limit
		k25VExtTemp2LowLimit			= 0x2C, // Register is optionally '2.V' or 'Ext. Temp. 2' low limit
		kVccp1HighLimit					= 0x2D,
		kVccp1LowLimit					= 0x2E,
		k33VHighLimit					= 0x2F,
		k33VLowLimit					= 0x30,
		k5VHighLimit					= 0x31,
		k5VLowLimit						= 0x32,
		k12VHighLimit					= 0x33,
		k12VLowLimit					= 0x34,
		kVccp2HighLimit					= 0x35,
		kVccp2LowLimit					= 0x36,
		kExtTemp1HighLimit				= 0x37,
		kExtTemp1LowLimit				= 0x38,
		kIntTempHighLimit				= 0x39,
		kIntTempLowLimit				= 0x3A,
		kFan1AIN1HighLimit				= 0x3B, // Register is optionally 'FAN1' or 'AIN1' high limit
		kFan2AIN2HighLimit				= 0x3C, // Register is optionally 'FAN2' or 'AIN2' high limit
		kReserved						= 0x3D,
		kCompanyID						= 0x3E,
		kConfReg1						= 0x40,
		kIntStatReg1					= 0x41,
		kIntStatReg2					= 0x42,
		kIntMaskReg1					= 0x43,
		kIntMaskReg2					= 0x44,
		kCIClearReg						= 0x46,
		kVID03FanDivReg					= 0x47,
		kVID4Reg						= 0x49,
		kConfReg2						= 0x4A,
		kIntStatReg1Mirror				= 0x4C,
		kIntStatReg2Mirror				= 0x4D,
		kSMBEn							= 0x80
	};

    enum
    {
        k25VinMultiplier 				= 0x0354,
        kVccMultiplier 					= 0x0467,
        k5VinMultiplier 				= 0x06a8,
        k12VinMultiplier 				= 0x1000,
        Vccp1Multiplier 				= 0x039c,
        Vccp2Multiplier 				= 0x039c,
        AIN1Multiplier 					= 0x0282,
        AIN2Multiplier 					= 0x0282
    };

    struct savedRegisters_t
    {
        UInt8	ChannelMode;
        UInt8	Configuration1;
        UInt8	Configuration2;
    } savedRegisters; 
	
	// Varibles
	UInt8								kLM8xAddr, kLM8xBus;
	IOService							*interface;
	const OSSymbol						*callPlatformFunction_getTempForIOHWSensorSymbol,
										*sOpenI2Cbus,
										*sCloseI2Cbus,
										*sSetPollingMode,
										*sSetStandardSubMode,
										*sSetCombinedMode,
										*sWriteI2Cbus,
										*sReadI2Cbus,
										*sGetSensorValueSym;
	IOPMrootDomain						*pmRootDomain;
	
	IOReturn							publishChildren(IOService *);
	IOReturn							buildEntryTable(IORegistryEntry *);
	IOReturn							initHW(IOService *provider);	// method to program DS1775 hardware
	IOReturn							openI2C(UInt8 id);
	IOReturn							closeI2C();
	IOReturn							writeI2C(UInt8 subAddr, UInt8 *data, UInt16 size);
	IOReturn							readI2C(UInt8 subAddr, UInt8 *data, UInt16 size);
	IOReturn							saveRegisters();
	IOReturn							restoreRegisters();
	LogicalUnitNumberTable				LUNtable[15]; // 15 possible sensors
	UInt8 								LUNtableElement; // # of elements
	
public:
	
	// Variables
	static bool							systemIsRestarting; // flag reflecting restart state (global to class members)
	
	// Methods
	virtual IOService					*probe(IOService *provider, SInt32 *score);
	virtual bool						start(IOService *);
	virtual void						stop(IOService *);
	
	IOReturn 							getReading(UInt32, SInt32 *);
	virtual IOReturn					callPlatformFunction(const OSSymbol *, bool, void *, void *, void *, void *);
												
	// Power handling methods:
	virtual IOReturn 					setPowerState(unsigned long, IOService *);
	static IOReturn						sysPowerDownHandler(void *, void *, UInt32, IOService *, void *, vm_size_t);
};

#endif // _APPLELM8X_H
