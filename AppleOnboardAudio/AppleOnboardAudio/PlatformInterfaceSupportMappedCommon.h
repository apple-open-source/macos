/*
 *  PlatformInterfaceSupporotMappedCommon.h
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 22 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include	<IOKit/IOService.h>
#include	<libkern/c++/OSString.h>
#include	"AppleOnboardAudio.h"

#ifndef __PlatformInterfaceSupporotMappedCommon
#define	__PlatformInterfaceSupporotMappedCommon

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

#define kPlatformInterfaceSupportMappedCommon_FCR0Offset					0x00000038;
#define kPlatformInterfaceSupportMappedCommon_FCR1Offset					0x0000003C;
#define kPlatformInterfaceSupportMappedCommon_FCR2Offset					0x00000040;
#define kPlatformInterfaceSupportMappedCommon_FCR3Offset					0x00000044;
#define kPlatformInterfaceSupportMappedCommon_FCR4Offset					0x00000048;

#define kPlatformInterfaceSupportMappedCommon_APPLE_IO_CONFIGURATION_SIZE	256;
#define kPlatformInterfaceSupportMappedCommon_I2S_IO_CONFIGURATION_SIZE		256;

#define kPlatformInterfaceSupportMappedCommon_I2S0BaseOffset				0x10000;							/*	mapped by AudioI2SControl	*/
#define kPlatformInterfaceSupportMappedCommon_I2S1BaseOffset				0x11000;							/*	mapped by AudioI2SControl	*/

#define kPlatformInterfaceSupportMappedCommon_I2SIntCtlOffset				0x0000;
#define kPlatformInterfaceSupportMappedCommon_I2SSerialFormatOffset			0x0010;
#define kPlatformInterfaceSupportMappedCommon_I2SCodecMsgOutOffset			0x0020;
#define kPlatformInterfaceSupportMappedCommon_I2SCodecMsgInOffset			0x0030;
#define kPlatformInterfaceSupportMappedCommon_I2SFrameCountOffset			0x0040;
#define kPlatformInterfaceSupportMappedCommon_I2SFrameMatchOffset			0x0050;
#define kPlatformInterfaceSupportMappedCommon_I2SDataWordSizesOffset		0x0060;
#define kPlatformInterfaceSupportMappedCommon_I2SPeakLevelSelOffset			0x0070;
#define kPlatformInterfaceSupportMappedCommon_I2SPeakLevelIn0Offset			0x0080;
#define kPlatformInterfaceSupportMappedCommon_I2SPeakLevelIn1Offset			0x0090;

#define kPlatformInterfaceSupportMappedCommon_I2SClockOffset				0x0003C;							/*	FCR1 offset (not mapped by AudioI2SControl)	*/
#define kPlatformInterfaceSupportMappedCommon_I2S0ClockEnable				( 1 << kI2S0ClkEnBit );
#define kPlatformInterfaceSupportMappedCommon_I2S1ClockEnable				( 1 << kI2S1ClkEnBit );
#define kPlatformInterfaceSupportMappedCommon_I2S0CellEnable				( 1 << kI2S0CellEn );
#define kPlatformInterfaceSupportMappedCommon_I2S1CellEnable	 			( 1 << kI2S1CellEn );
#define kPlatformInterfaceSupportMappedCommon_I2S0InterfaceEnable			( 1 << kI2S0Enable );
#define kPlatformInterfaceSupportMappedCommon_I2S1InterfaceEnable			( 1 << kI2S1Enable );
#define kPlatformInterfaceSupportMappedCommon_I2S0SwReset					( 1 << kI2S0SwReset );
#define kPlatformInterfaceSupportMappedCommon_I2S1SwReset					( 1 << kI2S1SwReset );

#define kPlatformInterfaceSupportMappedCommon_AmpMuteEntry					"amp-mute";
#define kPlatformInterfaceSupportMappedCommon_AnalogHWResetEntry			"audio-hw-reset";
#define kPlatformInterfaceSupportMappedCommon_ClockMuxEntry					"codec-clock-mux";
#define kPlatformInterfaceSupportMappedCommon_CodecErrorIrqTypeEntry		"codec-error-irq";
#define kPlatformInterfaceSupportMappedCommon_CodecIrqTypeEntry				"codec-irq";
#define kPlatformInterfaceSupportMappedCommon_ComboInJackTypeEntry			"combo-input-type";
#define kPlatformInterfaceSupportMappedCommon_ComboOutJackTypeEntry			"combo-output-type";
#define kPlatformInterfaceSupportMappedCommon_DigitalHWResetEntry			"audio-dig-hw-reset";
#define kPlatformInterfaceSupportMappedCommon_DigitalInDetectEntry			"digital-input-detect";
#define kPlatformInterfaceSupportMappedCommon_DigitalOutDetectEntry			"digital-output-detect";
#define kPlatformInterfaceSupportMappedCommon_HeadphoneDetectInt			"headphone-detect";
#define kPlatformInterfaceSupportMappedCommon_HeadphoneMuteEntry 			"headphone-mute";
#define kPlatformInterfaceSupportMappedCommon_InternalSpeakerIDEntry		"internal-speaker-id";
#define kPlatformInterfaceSupportMappedCommon_LineInDetectInt				"line-input-detect";
#define kPlatformInterfaceSupportMappedCommon_LineOutDetectInt				"line-output-detect";
#define kPlatformInterfaceSupportMappedCommon_LineOutMuteEntry				"line-output-mute";
#define kPlatformInterfaceSupportMappedCommon_SpeakerDetectEntry			"speaker-detect";

#define kPlatformInterfaceSupportMappedCommon_NumInputs						"#-inputs";
#define kPlatformInterfaceSupportMappedCommon_I2CAddress					"i2c-address";
#define kPlatformInterfaceSupportMappedCommon_AudioGPIO						"audio-gpio";
#define kPlatformInterfaceSupportMappedCommon_AudioGPIOActiveState			"audio-gpio-active-state";
#define kPlatformInterfaceSupportMappedCommon_IOInterruptControllers		"IOInterruptControllers";



#endif
