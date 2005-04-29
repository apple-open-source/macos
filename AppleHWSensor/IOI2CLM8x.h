/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * File: $Id: IOI2CLM8x.h,v 1.1 2004/09/18 00:55:36 jlehrer Exp $
 *
 *		$Log: IOI2CLM8x.h,v $
 *		Revision 1.1  2004/09/18 00:55:36  jlehrer
 *		Initial checkin.
 *		
 *
 */

#ifndef _IOI2CLM8x_H
#define _IOI2CLM8x_H

#include <IOKit/IOService.h>
#include <IOI2C/IOI2CDevice.h>

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

class IOI2CLM8x : public IOI2CDevice
{
	OSDeclareDefaultStructors(IOI2CLM8x)

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

	typedef struct savedRegisters_t
	{
		UInt8	ChannelMode;
		UInt8	Configuration1;
		UInt8	Configuration2;
	} savedRegisters_t; 

	savedRegisters_t					*fSavedRegisters;
	bool								fRegistersAreSaved;

	const OSSymbol						*sGetSensorValueSym;

	LogicalUnitNumberTable				LUNtable[15]; // 15 possible sensors
	UInt8 								LUNtableElement; // # of elements

	IOReturn publishChildren(IOService *);
	IOReturn buildEntryTable(IORegistryEntry *);
	IOReturn initHW(IOService *provider);	// method to program DS1775 hardware

	IOReturn saveRegisters(void);
	IOReturn restoreRegisters(void);

	virtual void processPowerEvent(UInt32 eventType);

public:

	virtual bool start(IOService *);
	virtual void free(void);

	using IOI2CDevice::callPlatformFunction;
	virtual IOReturn callPlatformFunction(const OSSymbol *, bool, void *, void *, void *, void *);
};

#endif // _IOI2CLM8x_H
