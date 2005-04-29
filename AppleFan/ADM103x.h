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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#ifndef _ADM103x_H
#define _ADM103x_H

/*
 * ADM1031 Registers - I2C Sub-Addresses
 *
 * ADM1030 uses a subset of the ADM1031 register set
 */

enum {
	kConfigReg1				= 0x00,	// Configuration Register 1
	kConfigReg2				= 0x01,	// Configuration Register 2
	kStatusReg1				= 0x02,	// Status Register 1
	kStatusReg2				= 0x03,	// Status Register 2
	kExtTempReg				= 0x06,	// Extended Temperature Resolution Register
	kFan1SpeedReg			= 0x08,	// Fan 1 Speed - Fan 1 Tach Measurement
	kFan2SpeedReg			= 0x09,	// Fan 2 Speed - Fan 2 Tach Measurement - ADM1031 only
	kLocalTempReg			= 0x0A,	// 8 MSBs of the Local Temperature Value
	kRemote1TempReg			= 0x0B,	// 8 MSBs of the Remote 1 Temperature Value
	kRemote2TempReg			= 0x0C,	// 8 MSBs of the Remote 2 Temperature Value - ADM1031 only
	kLocalOffsetReg			= 0x0D,	// Local Temperature Offset
	kRemote1OffsetReg		= 0x0E,	// Remote 1 Temperature Offset
	kRemote2OffsetReg		= 0x0F,	// Remote 2 Temperature Offset - ADM1031 only
	kFan1TachHighLimitReg	= 0x10,	// Fan 1 Tach High Limit
	kFan2TachHighLimitReg	= 0x11,	// Fan 2 Tach High Limit - ADM1031 only
	kLocalTempHighLimReg	= 0x14,	// Local Temperature High Limit
	kLocalTempLowLimReg		= 0x15,	// Local Temperature Low Limit
	kLocalTempThermLimReg	= 0x16,	// Local Temperature Therm Limit
	kRemote1TempHighLimReg	= 0x18,	// Remote 1 Temperature High Limit
	kRemote1TempLowLimReg	= 0x19,	// Remote 1 Temperature Low Limit
	kRemote1TempThermLimReg	= 0x1A, // Remote 1 Temperature Therm Limit
	kRemote2TempHighLimReg	= 0x1C,	// Remote 2 Temperature High Limit - ADM1031 only
	kRemote2TempLowLimReg	= 0x1D,	// Remote 2 Temperature Low Limit - ADM1031 only
	kRemote2TempThermLimReg	= 0x1E, // Remote 2 Temperature Therm Limit - ADM1031 only
	kFan1CharReg			= 0x20,	// Fan Characteristics Register 1
	kFan2CharReg			= 0x21,	// Fan Characteristics Register 2 - ADM1031 only
	kSpeedCfgReg			= 0x22,	// Fan Speed Configuration Register
	kFanFilterReg			= 0x23,	// Fan Filter Register
	kLocTminTrangeReg		= 0x24,	// Local Temperature T_min/T_range
	kRmt1TminTrangeReg		= 0x25, // Remote 1 Temperature T_min/T_range
	kRmt2TminTrangeReg		= 0x26,	// Remote 2 Temperature T_min/T_range - ADM1031 only
	kDeviceIDReg			= 0x3D	// Device ID Register
};

/*
 * Constants for the Extended Temperature Resolution Register
 */

enum {
	kLocalExtMask		= 0xC0,
	kLocalExtShift		= 0,
	kRemote1ExtMask		= 0x07,
	kRemote1ExtShift	= 5,
	kRemote2ExtMask		= 0x38,
	kRemote2ExtShift	= 2
};

/*
 * ADM103x Device ID Register Constants
 */
enum {
	kDeviceIDADM1030	= 0x30,
	kDeviceIDADM1031	= 0x31
};

/*
 * Macros to extract the extended temperature bits for each channel
 * from the value obtained from the extended temperature register
 */

#define LOCAL_FROM_EXT_TEMP(x) \
	(((x) & kLocalExtMask) << kLocalExtShift)
#define REMOTE1_FROM_EXT_TEMP(x) \
	(((x) & kRemote1ExtMask) << kRemote1ExtShift)
#define REMOTE2_FROM_EXT_TEMP(x) \
	(((x) & kRemote2ExtMask) << kRemote2ExtShift)

/*
 * ADM103x parts report temperature in two chunks: each channel has
 * a dedicated 8-bit register for the MSB, and extended resolution is
 * provided via an other register.  This macros will take the upper
 * and lower bytes of a temperature reading and construct a 32-bit
 * 16.16 fixed point representation of the temperature.
 */

#define TEMP_FROM_BYTES(high, low) \
	((SInt32)(((high & 0xFF) << 16) | ((low & 0xFF) << 8)))

#endif	// _ADM103x_H