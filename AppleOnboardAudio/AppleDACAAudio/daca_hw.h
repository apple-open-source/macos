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

/*
 * I2S registers:
 */

#define		kI2S0BaseOffset		0x10000
#define		kI2S1BaseOffset		0x11000

#define		kI2SClockOffset		0x0003C
#define		kI2S0ClockEnable 	(UInt32)(0x00000001<<12)
#define		kI2S1ClockEnable 	(UInt32)(0x00000001<<19)
#define		kI2S0InterfaceEnable 	(UInt32)(0x00000001<<13)
#define		kI2S1InterfaceEnable 	(UInt32)(0x00000001<<20)

#define		kI2SIntCtlOffset	0x0000
#define		kI2SSerialFormatOffset	0x0010
#define		kI2SCodecMsgOutOffset	0x0020
#define		kI2SCodecMsgInOffset	0x0030
#define		kI2SFrameCountOffset	0x0040
#define		kI2SFrameMatchOffset	0x0050
#define		kI2SDataWordSizesOffset	0x0060
#define		kI2SPeakLevelSelOffset	0x0070
#define		kI2SPeakLevelIn0Offset	0x0080
#define		kI2SPeakLevelIn1Offset	0x0090

/*
 * Status register:
 */
#define		kGPio12			0x00064
#define		kHeadphoneBit		0x02

/*
 * interrupt control register definitions
 */
enum {
    kFrameCountEnable		=	(1<<31),		 // enable frame count interrupt
    kFrameCountPending		=	(1<<30),	 // frame count interrupt pending
    kMsgFlagEnable		=	(1<<29),				 // enable message flag interrupt
    kMsgFlagPending		=	(1<<28),	   // message flag interrupt pending
    kNewPeakEnable		=	(1<<27),	    // enable new peak interrupt
    kNewPeakPending		=	(1<<26),	   // new peak interrupt pending
    kClocksStoppedEnable	=	(1<<25),	// enable clocks stopped interrupt
    kClocksStoppedPending	=	(1<<24),// clocks stopped interrupt pending
    kExtSyncErrorEnable		=	(1<<23),	// enable external sync error interrupt
    kExtSyncErrorPending	=	(1<<22),	// external sync error interrupt pending
    kExtSyncOKEnable		=	(1<<21),	   // enable external sync OK interrupt
    kExtSyncOKPending		=	(1<<20),	  // external sync OK interrupt pending
    kNewSampleRateEnable	=	(1<<19),	// enable new sample rate interrupt
    kNewSampleRatePending	=	(1<<18),// new sample rate interrupt pending
    kStatusFlagEnable		=	(1<<17),	  // enable status flag interrupt
    kStatusFlagPending		=	(1<<16)		 // status flag interrupt pending
};

// serial format register definitions
enum {
    kClockSourceMask		=	(3<<30),	  // mask off clock sources
    kClockSource18MHz		=	(0<<30),	 // select 18 MHz clock base
    kClockSource45MHz		=	(1<<30),	 // select 45 MHz clock base
    kClockSource49MHz		=	(2<<30),	 // select 49 MHz clock base
    kMClkDivisorShift		=	24,			    // shift to position value in MClk divisor field
    kMClkDivisorMask		=	(0x1F<<24),// mask MClk divisor field
    kMClkDivisor1			=	(0x14<<24),	 // MClk == clock source
    kMClkDivisor3			=	(0x13<<24),	 // MClk == clock source/3
    kMClkDivisor5			=	(0x12<<24),	 // MClk == clock source/5
    kSClkDivisorShift		=	20,			    // shift to position value in SClk divisor field
    kSClkDivisorMask		=	(0xF<<20),	// mask SClk divisor field
    kSClkDivisor1			=	(8<<20),	    // SClk == MClk
    kSClkDivisor3			=	(9<<20),	    // SClk == MClk/3
    kSClkMaster				=	(1<<19),	     // SClk in master mode
    kSClkSlave				=	(0<<19),	      // SClk in slave mode
    kSerialFormatShift		=	16,			   // shift to position value in I2S serial format field
    kSerialFormatMask		=	(7<<16),	 // mask serial format field
    kSerialFormatSony		=	(0<<16),	 // Sony mode
    kSerialFormat64x		=	(1<<16),	  // I2S 64x mode
    kSerialFormat32x		=	(2<<16),	  // I2S 32x mode
    kSerialFormatDAV		=	(4<<16),	  // DAV mode
    kSerialFormatSiliLabs	=	(5<<16),	  // Silicon Labs mode
    kExtSampleFreqIntShift	=	12,			    // shift to position for external sample frequency interrupt
    kExtSampleFreqIntMask	=	(0xF<<12),	// mask external sample frequency interrupt field
    kExtSampleFreqMask		=	0xFFF		      // mask for external sample frequency
};

// codec mesage in and out registers are not supported
// data word sizes
enum {
    kNumChannelsInShift		=	24,	     		// shift to get to num channels in
    kNumChannelsInMask		=	(0x1F<<24),	// mask num channels in field
    kDataInSizeShift		=	16,			        // shift to get to data in size
    kDataInSizeMask			=	(3<<16),	     // mask data in size
    kDataIn16				=	(0<<16),	          // 16 bit audio data in
    kDataIn24				=	(3<<16),	          // 24 bit audio data in
    kNumChannelsOutShift	=	8,			      // shift to get to num channels out
    kNumChannelsOutMask		=	(0x1F<<8),	// mask num channels out field
    kDataOutSizeShift		=	0,			        // shift to get to data out size
    kDataOutSizeMask		=	(3<<0),		     // mask data out size
    kDataOut16				=	(0<<0),		         // 16 bit audio data out
    kDataOut24				=	(3<<0)		          // 24 bit audio data out
};

// peak level subframe select register is not supported
// peak level in meter registers
enum {
    kNewPeakInShift			=	31,       			// shift to get to new peak in
    kNewPeakInMask			=	(1<<31),	     // mask new peak in bit
    kHoldPeakInShift		=	30,       			// shift to get to peak hold
    kHoldPeakInMask			=	(1<<30),    	// mask hold peak value
    kHoldPeakInEnable		=	(0<<30),	   // enable the hold peak register
    kHoldPeakInDisable		=	(1<<30),	  // disable the hold peak register (from updating)
    kPeakValueMask			=	0x00FFFFFF	   // mask to get peak value
};

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

    kNoDelaySPSelSR_REG		= 0x00,			 // default
    k1BitDelaySPSelSR_REG	= 0x08,
    kDelaySPSelSR_REGMask	= 0x08,

    kSRC_48SR_REG			= 0x00,		              // 32 - 48 KHz default
    kSRC_32SR_REG			= 0x01,		     	      // 26 - 32 KHz
    kSRC_24SR_REG			= 0x02,			      // 20 - 26 KHz
    kSRC_16SR_REG			= 0x03,			      // 14 - 20 KHz
    kSRC_12SR_REG			= 0x04,			      // 10 - 14 KHz
    kSRC_8SR_REG			= 0x05,			      // 8 - 10 KHz
    kSRC_Auto_REG			= 0x06,			      // autoselect
    kSampleRateControlMask  = 0x07,

    // Analog Volume, 16 bit register
    kMuteVolumeLevel_VOL	        = 0x00,	 	// Mute
    kMinVolumeLevel_VOL		        = 0x01,	 	// -75 dB
    kMaxVolumeLevel_VOL		        = 0x38,	 	// 18 dB
    kVolumeRangeLevel_VOL		= kMaxVolumeLevel_VOL - kMinVolumeLevel_VOL,

    kPowerOnDefaultAVOL		        = 0x2C2C,	 	// 0 dB
    kLeftAVOLShift			= 8,
    kRightAVOLShift			= 0,
    kRightAVOLMask			= 0x003F,		// range -75 to +18 dB, default 0 dB
    kLeftAVOLMask			= 0x3F00,		// range -75 to +18 dB, default 0 dB

    // Global Configuration, 8 bit register
    kPowerOnDefaultGCFG		= 0x04,

    kInvertRightAmpGCFG		= 0x01,			 // 0 -> right power amplifier not inverted (default)
    kMonoGCFG			= 0x02,			         // 0 -> stereo (default), 1 -> mono
    kDACOnGCFG			= 0x04,			        // 1 -> DAC on (default)
    kAuxOneGCFG			= 0x08,			       // 0 -> AUX1 off (default)
    kAuxTwoGCFG			= 0x10,			       // 0 -> AUX2 off (default)
    kLowPowerGCFG		= 0x20,			      // 0 -> normal power (default), 1 -> low power
    kSelect5VoltGCFG		= 0x40,			    // 0 -> 3 Volt (default), 1 -> 5 Volt

    kNoChangeMask			= 0x00
};

#endif // _DACA_HW_H

