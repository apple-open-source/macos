/*
 *  PlatformInterfaceSupportCommon.h
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne on Mon Aug 30 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */

#include	<IOKit/IOService.h>
#include	<libkern/c++/OSString.h>
#include	<IOKit/IOInterruptEventSource.h>

#ifndef __PlatformInterfaceSupportCommon__
#define	__PlatformInterfaceSupportCommon__

typedef enum {
	kDMADeviceIndex		= 0,
	kDMAOutputIndex		= 1,
	kDMAInputIndex		= 2,
	kDMANumberOfIndexes	= 3
} PlatformDMAIndexes;

typedef enum {
	kI2C_StandardMode 			= 0,
	kI2C_StandardSubMode		= 1,
	kI2C_CombinedMode			= 2
} BusMode;

typedef enum {
	kI2S_18MHz 					= 0,
	kI2S_45MHz					= 1,
	kI2S_49MHz					= 2
} I2SClockFrequency;

//	If this enumeration changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef enum GpioAttributes {
	kGPIO_Disconnected			= 0,
	kGPIO_Connected,
	kGPIO_Unknown,
	kGPIO_Muted,
	kGPIO_Unmuted,
	kGPIO_Reset,
	kGPIO_Run,
	kGPIO_MuxSelectDefault,
	kGPIO_MuxSelectAlternate,
	kGPIO_CodecInterruptActive,
	kGPIO_CodecInterruptInactive,
	kGPIO_CodecIRQEnable,
	kGPIO_CodecIRQDisable,
	kGPIO_TypeIsAnalog,
	kGPIO_TypeIsDigital,
	kGPIO_IsDefault,
	kGPIO_IsAlternate
};

//	If this enumeration changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef enum GPIOSelector {
	kGPIO_Selector_AnalogCodecReset	= 0,
	kGPIO_Selector_ClockMux,
	kGPIO_Selector_CodecInterrupt,
	kGPIO_Selector_CodecErrorInterrupt,
	kGPIO_Selector_ComboInJackType,
	kGPIO_Selector_ComboOutJackType,
	kGPIO_Selector_DigitalCodecReset,
	kGPIO_Selector_DigitalInDetect,
	kGPIO_Selector_DigitalOutDetect,
	kGPIO_Selector_HeadphoneDetect,
	kGPIO_Selector_HeadphoneMute,
	kGPIO_Selector_InputDataMux,
	kGPIO_Selector_InternalSpeakerID,
	kGPIO_Selector_LineInDetect,
	kGPIO_Selector_LineOutDetect,
	kGPIO_Selector_LineOutMute,
	kGPIO_Selector_SpeakerDetect,
	kGPIO_Selector_SpeakerMute,
	kGPIO_Selector_ExternalMicDetect,
	kGPIO_Selector_NotAssociated
};

//	If this enumeration changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef enum GPIOType {
	kGPIO_Type_ConnectorType = 0,
	kGPIO_Type_Detect,
	kGPIO_Type_Irq,
	kGPIO_Type_MuteL,
	kGPIO_Type_MuteH,
	kGPIO_Type_Mux,
	kGPIO_Type_Reset,
};

//	If this enumeration changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef enum {
	kCODEC_RESET_Analog			= 0,
	kCODEC_RESET_Digital,
	kCODEC_RESET_NumberOfResets
} CODEC_RESET;

//	If this enumeration changes then please apply the same changes to the DiagnosticSupport/AOA Viewer sources.
typedef enum {
	kUnknownInterrupt			= 0,
	kCodecErrorInterrupt,
	kCodecInterrupt,
	kDigitalInDetectInterrupt,
	kDigitalOutDetectInterrupt,
	kHeadphoneDetectInterrupt,
	kLineInputDetectInterrupt,
	kLineOutputDetectInterrupt,
	kSpeakerDetectInterrupt,
	kComboInDetectInterrupt,
	kComboOutDetectInterrupt
} PlatformInterruptSource;

#if 0
#define	kIOPFInterruptRegister		"IPOFInterruptRegister"
#define	kIOPFInterruptUnRegister	"IPOFInterruptUnRegister"
#define	kIOPFInterruptEnable		"IPOFInterruptEnable"
#define	kIOPFInterruptDisable		"IPOFInterruptDisable"
#else
//	PlatformFunction ERS version 0.76 on p. 19 shows the spelling below
#define	kIOPFInterruptRegister		"IOPFInterruptRegister"
#define	kIOPFInterruptUnRegister	"IOPFInterruptUnRegister"
#define	kIOPFInterruptEnable		"IOPFInterruptEnable"
#define	kIOPFInterruptDisable		"IOPFInterruptDisable"
#endif

#define	kParentOfParentCompatible32bitSysIO		"Keylargo"
#define	kParentOfParentCompatible64bitSysIO		"K2-Keylargo"

#define	kTAS3004_i2cAddress_i2sA		0x6A

#define	kPCM3052_i2cAddress_i2sA		0x8C
#define	kPCM3052_i2cAddress_i2sB		0x8E
#define	kPCM3052_i2cAddress_i2sC		0x8E

#define	kCS8406_i2cAddress_i2sA			0x20
#define	kCS8406_i2cAddress_i2sB			0x22
#define	kCS8406_i2cAddress_i2sC			0x24
#define	kCS8406_i2cAddress_i2sD			0x26

#define	kCS8416_i2cAddress_i2sA			0x20
#define	kCS8416_i2cAddress_i2sB			0x22
#define	kCS8416_i2cAddress_i2sC			0x24
#define	kCS8416_i2cAddress_i2sD			0x26

#define	kCS8420_i2cAddress_i2sA			0x20
#define	kCS8420_i2cAddress_i2sB			0x22
#define	kCS8420_i2cAddress_i2sC			0x24
#define	kCS8420_i2cAddress_i2sD			0x26


#endif
