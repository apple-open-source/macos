/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
 */

#ifndef _ADT746x_H
#define _ADT746x_H

/*
 * ADT746x Device ID Register Constants
 */
enum {
	kDeviceIDADT7460	= 0x27,
	kDeviceIDADT7467	= 0x68
};

/*
 * ADT7460 Registers - I2C Sub-Addresses
 */

enum {
        k2_5VReading				= 0x20, // (  R  ) 2.5V Reading
        k2_5VccpReading				= 0x21, // (  R  ) 2.5V Reading - 7467 ONLY
        kVccReading					= 0x22, // (  R  ) 
        
	kRemote1Temp					= 0x25,	// (  R  ) Remote 1 Temperature
        kLocalTemperature			= 0x26,	// (  R  ) Local Temperature
        kRemote2Temp				= 0x27,	// (  R  ) Remote 2 Temperature
        kTACH1LowByte				= 0x28,	// (  R  ) TACH 1 Low Byte
        kTACH1HighByte				= 0x29,	// (  R  ) TACH 1 High Byte
        kTACH2LowByte				= 0x2A,	// (  R  ) TACH 2 Low Byte
        kTACH2HighByte				= 0x2B,	// (  R  ) TACH 2 High Byte
        kTACH3LowByte				= 0x2C,	// (  R  ) TACH 3 Low Byte
        kTACH3HighByte				= 0x2D,	// (  R  ) TACH 3 High Byte
        kTACH4LowByte				= 0x2E,	// (  R  ) TACH 4 Low Byte
        kTACH4HighByte				= 0x2F,	// (  R  ) TACH 4 High Byte
        kPWM1DutyCycle				= 0x30,	// ( R/W ) PWM1 Duty Cycle
        kPWM2DutyCycle				= 0x31,	// ( R/W ) PWM2 Duty Cycle
        kPWM3DutyCycle				= 0x32,	// ( R/W ) PWM3 Duty Cycle
        kRemote1OperatingPoint		= 0x33,	// ( R/W ) Remote 1 Operating Point Reg
        kLocakTempOperatingPoint	= 0x34,	// ( R/W ) Local Operating Point Reg
        kRemote2OperatingPoint		= 0x35,	// ( R/W ) Remote 2 Operating Point Reg
        kDynTminContReg1			= 0x36,	// ( R/W )
        kDynTminContReg2			= 0x37,	// ( R/W )

        kDeviceIDReg				= 0x3D,
        kCompanyIDNum				= 0x3E,
        kRevisionNum				= 0x3F,
        kConfigReg1					= 0x40,	// ( R/W ) Configuration Register 1
        kIntStatusReg1				= 0x41,	// (  R  ) Interrupt Status Register 1
        kIntStatusReg2				= 0x42,	// (  R  ) Interrupt Status Register 2
        kVIDReg						= 0x43,
        k2_5VLowLimit				= 0x44, // ( R/W ) 2.5V Low Limit
        k2_5VHighLimit				= 0x45, // ( R/W ) 2.5V High Limit

        kVccLowLimit				= 0x48, // ( R/W ) Vcc Low Limit
        kVccHighLimit				= 0x49, // ( R/W ) Vcc High Limit

        kRemote1TempLowLimit		= 0x4E,	// ( R/W ) Remote 1 Temperature Low Limit
        kRemote1TempHighLimit		= 0x4F, // ( R/W ) Remote 1 Temperature High Limit
        kLocalTempLowLimit			= 0x50, // ( R/W ) Local Temperature Low Limit
        kLocalTempHighLimit			= 0x51, // ( R/W ) Local Temperature High Limit
        kRemote2TempLowLimit		= 0x52,	// ( R/W ) Remote 2 Temperature Low Limit
        kRemote2TempHighLimit		= 0x53, // ( R/W ) Remote 2 Temperature High Limit
        kTACH1MinLowByte			= 0x54,	// ( R/W ) Tach 1 Minimum Low Byte
        kTACH1MinHighByte			= 0x55, // ( R/W ) Tach 1 Maximum High Byte
        kTACH2MinLowByte			= 0x56,	// ( R/W ) Tach 2 Minimum Low Byte
        kTACH2MinHighByte			= 0x57,	// ( R/W ) Tach 2 Maximum High Byte
        kTACH3MinLowByte			= 0x58,	// ( R/W ) Tach 3 Minimum Low Byte
        kTACH3MinHighByte			= 0x59,	// ( R/W ) Tach 3 Maximum High Byte
        kTACH4MinLowByte			= 0x5A,	// ( R/W ) Tach 4 Maximum Low Byte
        kTACH5MinHighByte			= 0x5B,	// ( R/W ) Tach 4 Maximum High Byte
        kPWM1ConfigReg				= 0x5C,	// ( R/W ) PWM1 Configuration
        kPWM2ConfigReg				= 0x5D,	// ( R/W ) PWM2 Configuration
        kPWM3ConfigReg				= 0x5E,	// ( R/W ) PWM3 Configuration
        kRemote1Trange				= 0x5F,	// ( R/W ) Remote 1 TRange / PWM1 Frequency
        kLocalTrange				= 0x60,	// ( R/W ) Local TRange / PWM1 Frequency
        kRemote2Trange				= 0x61,	// ( R/W ) Remote 2 TRange / PWM1 Frequency
        kEnhanceAcousticsReg1		= 0x62,	// ( R/W ) EnhanceAcoustics Reg 1
        kEnhanceAcousticsReg2		= 0x63,	// ( R/W ) EnhanceAcoustics Reg 2
        kPWM1MinDutyCycle			= 0x64,	// ( R/W ) PWM 1 Min Duty Cycle
        kPWM2MinDutyCycle			= 0x65,	// ( R/W ) PWM 2 Min Duty Cycle
        kPWM3MinDutyCycle			= 0x66,	// ( R/W ) PWM 3 Min Duty Cycle
        kRemote1TempTmin			= 0x67,	// ( R/W ) Remote 1 Temperature Tmin
        kLocalTempTmin				= 0x68,	// ( R/W ) Local Temperature Tmin
        kRemote2TempTmin			= 0x69,	// ( R/W ) Remote 2 Temperature Tmin
        kRemote1THERMLimit			= 0x6A, // ( R/W ) Remote 1 THERM Limit
        kLocalTHERMLimit			= 0x6B, // ( R/W ) Local THERM Limit
        kRemote2THERMLimit			= 0x6C, // ( R/W ) Remote 2 THERM Limit
        kRemote1LocalHysteresis		= 0x6D, // ( R/W ) Remote 1, Local Temp Hysteresis
        kRemote2LocalHysteresis		= 0x6E, // ( R/W ) Remote 2 Temp Hysteresis
        kXORTreeTestEnable			= 0x6F, // ( R/W ) XOR Tree Test Enable Reg
        kRemote1TempOffset			= 0x70,	// ( R/W ) Remote 1 Temperature Offset
        kLocalTempOffset			= 0x71,	// ( R/W ) Local Temperature Offset
        kRemote2TempOffset			= 0x72,	// ( R/W ) Remote 2 Temperature Offset
        kConfigReg2					= 0x73,	// ( R/W ) Configuration Register 2
        kInterruptMask1Reg			= 0x74, // ( R/W ) Interrupt Mask Register 1
        kInterruptMask2Reg			= 0x75, // ( R/W ) Interrupt Mask Register 2
        kExtendedRes1				= 0x76, // (  R  ) Extended Temperature Resolution Register 1		
        kExtendedRes2				= 0x77, // Extended Temperature Resolution Register 2
        kConfigReg3					= 0x78,	// Configuration Register 3

        kTHERMTimerLimit			= 0x7A,	// THERM Timer Limit
        kFanPulsePerRev				= 0x7B,
        kConfigReg4					= 0x7D, // Configuration Register 4
        
        kTestRegister1				= 0x7E,	// DO NOT WRITE TO THESE REGISTERS
        kTestRegister2				= 0x7F	// DO NOT WRITE TO THESE REGISTERS
};

/*
 * Constants for the Extended Temperature Resolution Register
 */

enum {
	kLocalExtMask		= 0x30,
	kLocalExtShift		= 2,
	kRemote1ExtMask		= 0x0C,
	kRemote1ExtShift	= 4,
	kRemote2ExtMask		= 0xC0,
	kRemote2ExtShift	= 0,
        
    kVoltageShift		= 2,
    kExtVoltageMask		= 0x03,
    kExtVoltageShift	= 0
};

/*
 * Constants for Configuration Register 2
 */

enum {
        k2_5VAttenuationMask	= 0x20
};

/* ADT7460  Voltage levels  */

enum {
    // and a full-scale voltage of 2.25V with the attenuator off
	kIncrementPerVolt2_25Max			= 45511,			// ( 1024 / 2.25 = 455.1 ) * 100
    
    //the voltage sensor had a 3/4 value of 2.4975V with the input attenuator on
	kIncrementPerVolt3_3Max				= 31030				// ( 1024 /  3.3 = 310.3 ) * 100
};

/* ADT7467  Voltage constants  */

enum {
    // With attenuation
	kUnitsPerVoltWithAttenuation7467		= 44546,		// ( 1023 / 2.2965  = 445.46 ) * 100

    // With attenuation disabled
	kUnitsPerVoltWithoutAttenuation7467		= 34133			// ( 1024 / (2.25 * 4/3 ) = 341.33 ) * 100
};

/*
 * Macros to extract the extended temperature bits for each channel
 * from the value obtained from the extended temperature register
 */

#define LOCAL_FROM_EXT_TEMP(x) 			(((x) & kLocalExtMask) << kLocalExtShift)
#define REMOTE1_FROM_EXT_TEMP(x)		(((x) & kRemote1ExtMask) << kRemote1ExtShift)
#define REMOTE2_FROM_EXT_TEMP(x)		(((x) & kRemote2ExtMask) << kRemote2ExtShift)
        
#define	VOLTAGE_INDEX_FROM_BYTES(x, y)	(((x) << kVoltageShift) + ((y & kExtVoltageMask) << kExtVoltageShift))

/*
 * ADT746X parts report temperature in two chunks: each channel has
 * a dedicated 8-bit register for the MSB, and extended resolution is
 * provided via an other register.  This macros will take the upper
 * and lower bytes of a temperature reading and construct a 32-bit
 * 16.16 fixed point representation of the temperature.
 */

#define TEMP_FROM_BYTES(high, low)			((SInt32)(((high & 0xFF) << 16) | ((low & 0xFF) << 8)))
#define SIGNED_TEMP_FROM_BYTES(high, low)	((SInt32)((high << 16) | ((low & 0xFF) << 8)))

#endif	// _ADT746x_H
