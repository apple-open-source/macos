 /*
 *  AudioI2SHardwareConstants.h
 *  Apple02Audio
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  Contains a lot of constants used across the Apple02Audio project.
 *  There are three kind of constants : 
 *      - the string equivalent used mainly to parse the Open Firmware 
 *        device tree. Eventually they will move to the header file
 *        of a OF device tree parser objects.
 *      - A serie of enum representing the abstracted input, output numbers
 *        for any chip. This is enough to cover hardware Codec with 6 or 7
 *        input and outputs, which we will never have...
 *      - A enumeration for device kinds. We may extend it for Apple Speakers
 *        All devices are bit exclusives
 *      - An enumeration for all kind codec.
 */

#ifndef _AUDIO_I2S_HARDWARE_CONSTANTS_H
#define	_AUDIO_I2S_HARDWARE_CONSTANTS_H

/*
 *	Feature Control Registers
 */
#define	kFCR0Offset					0x00000038
#define	kFCR1Offset					0x0000003C
#define	kFCR2Offset					0x00000040
#define	kFCR3Offset					0x00000044
#define	kFCR4Offset					0x00000048

enum FCR1_Bit_Addresses {			//	bit addresses
	kI2S1Enable					=	20,			//	1 = normal, 0 = tristate
	kI2S1ClkEnBit				=	19,			//	1 = normal, 0 = stopped low
	kI2S1SwReset				=	18,			//	1 = reset, 0 = run
	kI2S1CellEn					=	17,			//	1 = clock running, 0 = clock stopped
	kI2S0Enable					=	13,			//	1 = normal, 0 = tristate
	kI2S0ClkEnBit				=	12,			//	1 = normal, 0 = stopped low
	kI2S0SwReset				=	11,			//	1 = reset, 0 = run
	kI2S0CellEn					=	10,			//	1 = clock running, 0 = clock stopped
	kChooseI2S0					=	 9,			//	1 = I2S0 drives clock out, 0 = SccB or IrDA drives clock out
	kChooseAudio				=	 7,			//	1 = DAV audio, 0 = I2S0
	kAUDIOCellEN				=	 6,			//	1 = DAV clock running, 0 = DAV clocks stopped
	kAudioClkOut_EN_h			=	 5,			//	1 = DAV AudioClkOut active, 0 = DAV AudioClkOut tristate
	kAudioSW_Reset_h			=	 4,			//	1 = DAV reset, 0 = run
	kAudioClkEnBit_h			=	 3,			//	1 = normal, 0 = stopped low
	kAudioClkDiv2_h				=	 2,			//	1 = divided by 4, 0 = divided by 2
	kAudio_Sel22MClk			=	 1,
	kAudioClkOut1X_h			=	 0
};

enum FCR1_Field_Width {
	kI2S1Enable_bitWidth		=	1,			//	
	kI2S1ClkEnBit_bitWidth		=	1,			//	
	kI2S1SwReset_bitWidth		=	1,			//	
	kI2S1CellEn_bitWidth		=	1,			//	
	kI2S0Enable_bitWidth		=	1,			//	
	kI2S0ClkEnBit_bitWidth		=	1,			//	
	kI2S0SwReset_bitWidth		=	1,			//	
	kI2S0CellEn_bitWidth		=	1,			//	
	kChooseI2S0_bitWidth		=	1,			//	
	kChooseAudio_bitWidth		=	1,			//	
	kAUDIOCellEN_bitWidth		=	1			//	
};

enum FCR3_Bit_Addresses {
	kClk18_EN_h					=	14,			//	1 = enable 18.4320 MHz clock to the 12S0 cell
	kI2S1_Clk18_EN_h			=	13,			//	1 = enable 18.4320 MHz clock to the 12S1 cell
	kClk45_EN_h					=	10,			//	1 = enable 45.1584 MHz clock to Audio, I2S0, I2S1 and SCC
	kClk49_EN_h					=	 9,			//	1 = enable 49.1520 MHz clock to Audio, I2S0 
	kShutdown_PLLKW4			=	 2,			//	1 = shutdown the 45.1584 MHz PLL
	kShutdown_PLLKW6			=	 1,			//	1 = shutdown the 49.1520 MHz PLL
	kShutdown_PLL_Total			=	 0			//	1 = shutdown all five PLL modules
};

enum FCR3_FieldWidth {
	kClk18_EN_h_bitWidth			=	1,		//	
	kI2S1_Clk18_EN_h_bitWidth		=	1,		//	
	kClk45_EN_h_bitWidth			=	1,		//	
	kClk49_EN_h_bitWidth			=	1,		//	
	kShutdown_PLLKW4_bitWidth		=	1,		//	
	kShutdown_PLLKW6_bitWidth		=	1,		//	
	kShutdown_PLL_Total_bitWidth	=	1		//	
};

/*
 * I2S registers:
 */

#define		kAPPLE_IO_CONFIGURATION_SIZE	256
#define		kI2S_IO_CONFIGURATION_SIZE		256

#define		kI2S0BaseOffset			0x10000							/*	mapped by AudioI2SControl	*/
#define		kI2S1BaseOffset			0x11000							/*	mapped by AudioI2SControl	*/

#define		kI2SIntCtlOffset		0x0000
#define		kI2SSerialFormatOffset	0x0010
#define		kI2SCodecMsgOutOffset	0x0020
#define		kI2SCodecMsgInOffset	0x0030
#define		kI2SFrameCountOffset	0x0040
#define		kI2SFrameMatchOffset	0x0050
#define		kI2SDataWordSizesOffset	0x0060
#define		kI2SPeakLevelSelOffset	0x0070
#define		kI2SPeakLevelIn0Offset	0x0080
#define		kI2SPeakLevelIn1Offset	0x0090

#define		kI2SClockOffset			0x0003C							/*	FCR1 offset (not mapped by AudioI2SControl)	*/
#define		kI2S0ClockEnable 		(UInt32)( 1 << kI2S0ClkEnBit )
#define		kI2S1ClockEnable 		(UInt32)( 1 << kI2S1ClkEnBit )
#define		kI2S0InterfaceEnable 	(UInt32)( 1 << kI2S0Enable )
#define		kI2S1InterfaceEnable 	(UInt32)( 1 << kI2S1Enable )
#define		kI2S0CellEnable 		(UInt32)( 1 << kI2S0CellEn )
#define		kI2S1CellEnable 		(UInt32)( 1 << kI2S1CellEn )


enum i2sReference {
	kUseI2SCell0			=	0,
	kUseI2SCell1			=	1,
	kNoI2SCell				=	0xFFFFFFFF
};

/*
 * interrupt control register definitions
 */
enum {
    kFrameCountEnable		=	(1<<31),	// enable frame count interrupt
    kFrameCountPending		=	(1<<30),	// frame count interrupt pending
    kMsgFlagEnable			=	(1<<29),	// enable message flag interrupt
    kMsgFlagPending			=	(1<<28),	// message flag interrupt pending
    kNewPeakEnable			=	(1<<27),	// enable new peak interrupt
    kNewPeakPending			=	(1<<26),	// new peak interrupt pending
    kClocksStoppedEnable	=	(1<<25),	// enable clocks stopped interrupt
    kClocksStoppedPending	=	(1<<24),	// clocks stopped interrupt pending
    kExtSyncErrorEnable		=	(1<<23),	// enable external sync error interrupt
    kExtSyncErrorPending	=	(1<<22),	// external sync error interrupt pending
    kExtSyncOKEnable		=	(1<<21),	// enable external sync OK interrupt
    kExtSyncOKPending		=	(1<<20),	// external sync OK interrupt pending
    kNewSampleRateEnable	=	(1<<19),	// enable new sample rate interrupt
    kNewSampleRatePending	=	(1<<18),	// new sample rate interrupt pending
    kStatusFlagEnable		=	(1<<17),	// enable status flag interrupt
    kStatusFlagPending		=	(1<<16)		// status flag interrupt pending
};

// serial format register definitions
enum {
    kClockSourceMask		=	(3<<30),	// mask off clock sources
    kClockSource18MHz		=	(0<<30),	// select 18 MHz clock base
    kClockSource45MHz		=	(1<<30),	// select 45 MHz clock base
    kClockSource49MHz		=	(2<<30),	// select 49 MHz clock base
    kMClkDivisorShift		=	24,			// shift to position value in MClk divisor field
    kMClkDivisorMask		=	(0x1F<<24),	// mask MClk divisor field
    kMClkDivisor1			=	(0x14<<24),	// MClk == clock source
    kMClkDivisor3			=	(0x13<<24),	// MClk == clock source/3
    kMClkDivisor5			=	(0x12<<24),	// MClk == clock source/5
    kSClkDivisorShift		=	20,			// shift to position value in SClk divisor field
    kSClkDivisorMask		=	(0xF<<20),	// mask SClk divisor field
    kSClkDivisor1			=	(8<<20),	// SClk == MClk
    kSClkDivisor3			=	(9<<20),	// SClk == MClk/3
    kSClkMaster				=	(1<<19),	// SClk in master mode
    kSClkSlave				=	(0<<19),	// SClk in slave mode
    kSerialFormatShift		=	16,			// shift to position value in I2S serial format field
    kSerialFormatMask		=	(7<<16),	// mask serial format field
    kSerialFormatSony		=	(0<<16),	// Sony mode
    kSerialFormat64x		=	(1<<16),	// I2S 64x mode
    kSerialFormat32x		=	(2<<16),	// I2S 32x mode
    kSerialFormatDAV		=	(4<<16),	// DAV mode
    kSerialFormatSiliLabs	=	(5<<16),	// Silicon Labs mode
    kExtSampleFreqIntShift	=	12,			// shift to position for external sample frequency interrupt
    kExtSampleFreqIntMask	=	(0xF<<12),	// mask external sample frequency interrupt field
    kExtSampleFreqMask		=	0xFFF		// mask for external sample frequency
};

// codec mesage in and out registers are not supported
// data word sizes
enum {
    kNumChannelsInShift		=	24,	     	// shift to get to num channels in
    kNumChannelsInMask		=	(0x1F<<24),	// mask num channels in field
    kDataInSizeShift		=	16,			// shift to get to data in size
    kDataInSizeMask			=	(3<<16),	// mask data in size
    kDataIn16				=	(0<<16),	// 16 bit audio data in
    kDataIn24				=	(3<<16),	// 24 bit audio data in
    kNumChannelsOutShift	=	8,			// shift to get to num channels out
    kNumChannelsOutMask		=	(0x1F<<8),	// mask num channels out field
    kDataOutSizeShift		=	0,			// shift to get to data out size
    kDataOutSizeMask		=	(3<<0),		// mask data out size
    kDataOut16				=	(0<<0),		// 16 bit audio data out
    kDataOut24				=	(3<<0),		// 24 bit audio data out
	kI2sStereoChannels		=	2,			// USE: ( kI2sStereoChannels << kNumChannelsOutShift ) or ( kI2sStereoChannels << kNumChannelsInShift )
	kI2sMonoChannels		=	1			// USE: ( kI2sMonoChannels << kNumChannelsOutShift ) or ( kI2sMonoChannels << kNumChannelsInShift )
};

// peak level subframe select register is not supported
// peak level in meter registers
enum {
    kNewPeakInShift			=	31,       	// shift to get to new peak in
    kNewPeakInMask			=	(1<<31),	// mask new peak in bit
    kHoldPeakInShift		=	30,       	// shift to get to peak hold
    kHoldPeakInMask			=	(1<<30),    // mask hold peak value
    kHoldPeakInEnable		=	(0<<30),	// enable the hold peak register
    kHoldPeakInDisable		=	(1<<30),	// disable the hold peak register (from updating)
    kPeakValueMask			=	0x00FFFFFF	// mask to get peak value
};

#define ki2saEntry					"i2s-a"
#define ki2sbEntry					"i2s-b"

#endif
