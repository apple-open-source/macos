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
 * Copyright (c) 1998-1999 Apple Computer, Inc.  All rights reserved.
 *
 * Keylargo Audio Hardware Registers and DAC3550 Hardware Registers
 *
 */

#ifndef _DACA_HW_H
#define _DACA_HW_H

#define kDACASampleLatency			32

/*
 * Status register:
 */
#define		kGPio12			0x00064
#define		kHeadphoneBit		0x02

enum {
    // 12c bus address for the chip and sub-addresses for registers
    i2cBusAddrDAC3550A		= 0x4d,
    i2cBusSubAddrSR_REG		= 0x01,
    i2cBusSubAddrAVOL		= 0x02,
    i2cBusSubaddrGCFG		= 0x03,

    // Sample Rate Control, 8 bit register
    kPowerOnDefaultSR_REG	= 0x00,

    kLeftLRSelSR_REG		= 0x00,			// left channel default
    kRightLRSelSR_REG		= 0x10,		  	// right channel
    kLRSelSR_REGMask		= 0x10,

    kNoDelaySPSelSR_REG		= 0x00,			// default
    k1BitDelaySPSelSR_REG	= 0x08,
    kDelaySPSelSR_REGMask	= 0x08,

    kSRC_48SR_REG			= 0x00,		    // 32 - 48 KHz default
    kSRC_32SR_REG			= 0x01,		    // 26 - 32 KHz
    kSRC_24SR_REG			= 0x02,			// 20 - 26 KHz
    kSRC_16SR_REG			= 0x03,			// 14 - 20 KHz
    kSRC_12SR_REG			= 0x04,			// 10 - 14 KHz
    kSRC_8SR_REG			= 0x05,			// 8 - 10 KHz
    kSRC_Auto_REG			= 0x06,			// autoselect
    kSampleRateControlMask  = 0x07,

    // Analog Volume, 16 bit register
    kMuteVolumeLevel_VOL	= 0x00,	 		// Mute
    kMinVolumeLevel_VOL		= 0x01,	 		// -75 dB
    kMaxVolumeLevel_VOL		= 0x38,	 		// 18 dB
    kVolumeRangeLevel_VOL	= kMaxVolumeLevel_VOL - kMinVolumeLevel_VOL,

    kPowerOnDefaultAVOL		= 0x2C2C,	 	// 0 dB
    kLeftAVOLShift			= 8,
    kRightAVOLShift			= 0,
    kRightAVOLMask			= 0x003F,		// range -75 to +18 dB, default 0 dB
    kLeftAVOLMask			= 0x3F00,		// range -75 to +18 dB, default 0 dB

    // Global Configuration, 8 bit register
    kPowerOnDefaultGCFG		= 0x04,

    kInvertRightAmpGCFG		= 0x01,			// 0 -> right power amplifier not inverted (default)
    kMonoGCFG				= 0x02,			// 0 -> stereo (default), 1 -> mono
    kDACOnGCFG				= 0x04,			// 1 -> DAC on (default)
    kAuxOneGCFG				= 0x08,			// 0 -> AUX1 off (default)
    kAuxTwoGCFG				= 0x10,			// 0 -> AUX2 off (default)
    kLowPowerGCFG			= 0x20,			// 0 -> normal power (default), 1 -> low power
    kSelect5VoltGCFG		= 0x40,			// 0 -> 3 Volt (default), 1 -> 5 Volt

    kNoChangeMask			= 0x00
};

#endif // _DACA_HW_H

