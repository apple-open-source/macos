/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998-2001 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */

#ifndef _IOKIT_KEYLARGO_H
#define _IOKIT_KEYLARGO_H

#include <IOKit/IOLocks.h>

#include <IOKit/platform/AppleMacIO.h>
#include <IOKit/system_management/IOWatchDogTimer.h>

enum {
    kKeyLargoDeviceId22			= 0x22,				// KeyLargo
	kKeyLargoVersion2			= 2,

    kPangeaDeviceId25			= 0x25,				// Pangea
	kPangeaVersion0				= 0
};

enum {
  // KeyLargo Register Offsets
  kKeyLargoMediaBay                      = 0x00034,
  kKeyLargoFCRBase                       = 0x00038,
  kKeyLargoFCRCount						 = 5,
  kPangeaFCRCount						 = 6,
  kKeyLargoFCR0                          = 0x00038,
  kKeyLargoFCR1                          = 0x0003C,
  kKeyLargoFCR2                          = 0x00040,
  kKeyLargoFCR3                          = 0x00044,
  kKeyLargoFCR4                          = 0x00048,
  kKeyLargoFCR5							 = 0x0004C,			// Pangea only
  
  kKeyLargoExtIntGPIOBase                = 0x00050,
  kKeyLargoExtIntGPIOCount               = 18,
  kKeyLargoGPIOBase                      = 0x0006A,
  kKeyLargoGPIOCount                     = 17,
  kKeyLargoGPIOOutputEnable              = 0x04,
  kKeyLargoGPIOData                      = 0x01,
  
  kKeyLargoGTimerFreq                    = 18432000UL,
  kKeyLargoCounterLoOffset               = 0x15038,
  kKeyLargoCounterHiOffset               = 0x1503C,
  
  
  // MediaBay Register Definitions
  kKeyLargoMB0DevEnable                   = 1 << 12,
  kKeyLargoMB0DevPower                    = 1 << 10,
  kKeyLargoMB0DevReset                    = 1 <<  9,
  kKeyLargoMB0Enable                      = 1 <<  8,
  kKeyLargoMB1DevEnable                   = 1 << 28,
  kKeyLargoMB1DevPower                    = 1 << 26,
  kKeyLargoMB1DevReset                    = 1 << 25,
  kKeyLargoMB1Enable                      = 1 << 24,
  
  // Feature Control Register 0 Definitions
  kKeyLargoFCR0ChooseSCCB				 = 1 << 0,
  kKeyLargoFCR0ChooseSCCA				 = 1 << 1,
  kKeyLargoFCR0SlowSccPClk               = 1 << 2,
  kKeyLargoFCR0ResetSCC					 = 1 << 3,
  kKeyLargoFCR0SccAEnable                = 1 << 4,
  kKeyLargoFCR0SccBEnable                = 1 << 5,
  kKeyLargoFCR0SccCellEnable             = 1 << 6,
  kKeyLargoFCR0ChooseVIA				 = 1 << 7,
  kKeyLargoFCR0HighBandFor1MB			 = 1 << 8,
  kKeyLargoFCR0UseIRSource2				 = 1 << 9,
  kKeyLargoFCR0UseIRSource1				 = 1 << 10,
  kKeyLargoFCR0IRDASWReset				 = 1 << 11,
  kKeyLargoFCR0IRDADefault1				 = 1 << 12,
  kKeyLargoFCR0IRDADefault0				 = 1 << 13,
  kKeyLargoFCR0IRDAFastCon				 = 1 << 14,
  kKeyLargoFCR0IRDAEnable                = 1 << 15,
  kKeyLargoFCR0IRDAClk32Enable           = 1 << 16,
  kKeyLargoFCR0IRDAClk19Enable           = 1 << 17,
  kKeyLargoFCR0USB0PadSuspend0           = 1 << 18,
  kKeyLargoFCR0USB0PadSuspend1           = 1 << 19,
  kKeyLargoFCR0USB0CellEnable            = 1 << 20,
  kKeyLargoFCR0USB1PadSuspend0           = 1 << 22,
  kKeyLargoFCR0USB1PadSuspend1           = 1 << 23,
  kKeyLargoFCR0USB1CellEnable            = 1 << 24,
  kKeyLargoFCR0USBRefSuspend             = 1 << 28,
  
  // Feature Control Register 1 Definitions
  kKeyLargoFCR1AudioSel22MClk            = 1 << 1,
  kKeyLargoFCR1AudioClkEnable            = 1 << 3,
  kKeyLargoFCR1AudioClkOutEnable         = 1 << 5,
  kKeyLargoFCR1AudioCellEnable           = 1 << 6,
  kKeyLargoFCR1ChooseAudio               = 1 << 7,
  kKeyLargoFCR1ChooseI2S0                = 1 << 9,
  kKeyLargoFCR1I2S0CellEnable            = 1 << 10,
  kKeyLargoFCR1I2S0ClkEnable             = 1 << 12,
  kKeyLargoFCR1I2S0Enable                = 1 << 13,
  kKeyLargoFCR1I2S1CellEnable            = 1 << 17,
  kKeyLargoFCR1I2S1ClkEnable             = 1 << 19,
  kKeyLargoFCR1I2S1Enable                = 1 << 20,
  kKeyLargoFCR1EIDE0Enable               = 1 << 23,
  kKeyLargoFCR1EIDE0Reset                = 1 << 24,
  kKeyLargoFCR1EIDE1Enable               = 1 << 26,
  kKeyLargoFCR1EIDE1Reset                = 1 << 27,
  kKeyLargoFCR1UIDEEnable                = 1 << 29,
  kKeyLargoFCR1UIDEReset                 = 1 << 30,
  
  // Feature Control Register 2 Definitions
  kKeyLargoFCR2IOBusEnable               = 1 << 1,
  kKeyLargoFCR2SleepState                = 1 << 8,
  kKeyLargoFCR2AltDataOut                = 1 << 25,
  
  // Feature Control Register 3 Definitions
  kKeyLargoFCR3ShutdownPLLTotal          = 1 << 0,
  kKeyLargoFCR3ShutdownPLLKW6            = 1 << 1,
  kKeyLargoFCR3ShutdownPLLKW4            = 1 << 2,
  kKeyLargoFCR3ShutdownPLLKW35           = 1 << 3,
  kKeyLargoFCR3ShutdownPLLKW12           = 1 << 4,
  kKeyLargoFCR3PLLReset                  = 1 << 5,
  kKeyLargoFCR3ShutdownPLL2X             = 1 << 7,
  kKeyLargoFCR3Clk66Enable               = 1 << 8,
  kKeyLargoFCR3Clk49Enable               = 1 << 9,
  kKeyLargoFCR3Clk45Enable               = 1 << 10,
  kKeyLargoFCR3Clk31Enable               = 1 << 11,
  kKeyLargoFCR3TimerClk18Enable          = 1 << 12,
  kKeyLargoFCR3I2S1Clk18Enable           = 1 << 13,
  kKeyLargoFCR3I2S0Clk18Enable           = 1 << 14,
  kKeyLargoFCR3ViaClk16Enable            = 1 << 15,
  kKeyLargoFCR3Stopping33Enabled         = 1 << 19,
  
  // Feature Control Register 4 Definitions
  kKeyLargoFCR4Port1DisconnectSelect     = 1 << 0,
  kKeyLargoFCR4Port1ConnectSelect        = 1 << 1,
  kKeyLargoFCR4Port1ResumeSelect         = 1 << 2,
  kKeyLargoFCR4Port1Enable               = 1 << 3,
  kKeyLargoFCR4Port1Disconnect           = 1 << 4,
  kKeyLargoFCR4Port1Connect              = 1 << 5,
  kKeyLargoFCR4Port1Resume               = 1 << 6,
  
  kKeyLargoFCR4Port2DisconnectSelect     = 1 << 8,
  kKeyLargoFCR4Port2ConnectSelect        = 1 << 9,
  kKeyLargoFCR4Port2ResumeSelect         = 1 << 10,
  kKeyLargoFCR4Port2Enable               = 1 << 11,
  kKeyLargoFCR4Port2Disconnect           = 1 << 12,
  kKeyLargoFCR4Port2Connect              = 1 << 13,
  kKeyLargoFCR4Port2Resume               = 1 << 14,
  
  kKeyLargoFCR4Port3DisconnectSelect     = 1 << 16,
  kKeyLargoFCR4Port3ConnectSelect        = 1 << 17,
  kKeyLargoFCR4Port3ResumeSelect         = 1 << 18,
  kKeyLargoFCR4Port3Enable               = 1 << 19,
  kKeyLargoFCR4Port3Disconnect           = 1 << 20,
  kKeyLargoFCR4Port3Connect              = 1 << 21,
  kKeyLargoFCR4Port3Resume               = 1 << 22,
  
  kKeyLargoFCR4Port4DisconnectSelect     = 1 << 24,
  kKeyLargoFCR4Port4ConnectSelect        = 1 << 25,
  kKeyLargoFCR4Port4ResumeSelect         = 1 << 26,
  kKeyLargoFCR4Port4Enable               = 1 << 27,
  kKeyLargoFCR4Port4Disconnect           = 1 << 28,
  kKeyLargoFCR4Port4Connect              = 1 << 29,
  kKeyLargoFCR4Port4Resume               = 1 << 30
};

enum {
  // Feature Control Register 0 Sleep Settings
  kKeyLargoFCR0SleepBitsSet              = (kKeyLargoFCR0USBRefSuspend),
  
  kKeyLargoFCR0SleepBitsClear            = (kKeyLargoFCR0SccAEnable |
					    kKeyLargoFCR0SccBEnable |
					    kKeyLargoFCR0SccCellEnable |
					    kKeyLargoFCR0IRDAEnable |
					    kKeyLargoFCR0IRDAClk32Enable |
					    kKeyLargoFCR0IRDAClk19Enable),
  
  // Feature Control Register 1 Sleep Settings
  kKeyLargoFCR1SleepBitsSet              = 0,
  
  kKeyLargoFCR1SleepBitsClear            = (kKeyLargoFCR1AudioSel22MClk |
					    kKeyLargoFCR1AudioClkEnable |
					    kKeyLargoFCR1AudioClkOutEnable |
					    kKeyLargoFCR1AudioCellEnable |
					    kKeyLargoFCR1I2S0CellEnable |
					    kKeyLargoFCR1I2S0ClkEnable |
					    kKeyLargoFCR1I2S0Enable |
					    kKeyLargoFCR1I2S1CellEnable |
					    kKeyLargoFCR1I2S1ClkEnable |
					    kKeyLargoFCR1I2S1Enable |
					    kKeyLargoFCR1EIDE0Enable |
					    kKeyLargoFCR1EIDE1Enable |
					    kKeyLargoFCR1UIDEEnable |
					    kKeyLargoFCR1EIDE0Reset |
					    kKeyLargoFCR1EIDE1Reset),
  
  // Feature Control Register 2 Sleep Settings
  kKeyLargoFCR2SleepBitsSet              = kKeyLargoFCR2AltDataOut,
  
  kKeyLargoFCR2SleepBitsClear            = kKeyLargoFCR2IOBusEnable,
  
  // Feature Control Register 3 Sleep and Restart Settings
  kKeyLargoFCR3SleepBitsSet              = (kKeyLargoFCR3ShutdownPLLKW6 |
					    kKeyLargoFCR3ShutdownPLLKW4 |
					    kKeyLargoFCR3ShutdownPLLKW35 |
					    kKeyLargoFCR3ShutdownPLLKW12),
  
  kKeyLargoFCR3SleepBitsClear            = (kKeyLargoFCR3Clk66Enable |
					    kKeyLargoFCR3Clk49Enable |
					    kKeyLargoFCR3Clk45Enable |
					    kKeyLargoFCR3Clk31Enable |
					    kKeyLargoFCR3TimerClk18Enable |
					    kKeyLargoFCR3I2S1Clk18Enable |
					    kKeyLargoFCR3I2S0Clk18Enable |
					    kKeyLargoFCR3ViaClk16Enable),
  
  kKeyLargoFCR3RestartBitsSet            = (kKeyLargoFCR3ShutdownPLLKW6 |
					    kKeyLargoFCR3ShutdownPLLKW4 |
					    kKeyLargoFCR3ShutdownPLLKW35),
  
  kKeyLargoFCR3RestartBitsClear          = (kKeyLargoFCR3Clk66Enable |
					    kKeyLargoFCR3Clk49Enable |
					    kKeyLargoFCR3Clk45Enable |
					    kKeyLargoFCR3Clk31Enable |
					    kKeyLargoFCR3I2S1Clk18Enable |
					    kKeyLargoFCR3I2S0Clk18Enable),
  
  // Feature Control Register 4 Sleep Settings
  // Marco since we are going to have two different controllers for each usb, I am going
  // to spearate each bus bit set:
    kKeyLargoFCR4USB0SleepBitsSet              = (kKeyLargoFCR4Port1DisconnectSelect |
                                              kKeyLargoFCR4Port1ConnectSelect |
                                              kKeyLargoFCR4Port1ResumeSelect |
                                              kKeyLargoFCR4Port1Enable |
                                              kKeyLargoFCR4Port2DisconnectSelect |
                                              kKeyLargoFCR4Port2ConnectSelect |
                                              kKeyLargoFCR4Port2ResumeSelect |
                                              kKeyLargoFCR4Port2Enable),

    kKeyLargoFCR4USB1SleepBitsSet              = (kKeyLargoFCR4Port3DisconnectSelect |
                                              kKeyLargoFCR4Port3ConnectSelect |
                                              kKeyLargoFCR4Port3ResumeSelect |
                                              kKeyLargoFCR4Port3Enable |
                                              kKeyLargoFCR4Port4DisconnectSelect |
                                              kKeyLargoFCR4Port4ConnectSelect |
                                              kKeyLargoFCR4Port4ResumeSelect |
                                              kKeyLargoFCR4Port4Enable),

    kKeyLargoFCR4SleepBitsSet              = (kKeyLargoFCR4USB0SleepBitsSet |
                                              kKeyLargoFCR4USB1SleepBitsSet),

    kKeyLargoFCR4USB0SleepBitsClear        = 0,
    kKeyLargoFCR4USB1SleepBitsClear        = 0,

    kKeyLargoFCR4SleepBitsClear            = kKeyLargoFCR4USB0SleepBitsClear | kKeyLargoFCR4USB1SleepBitsClear
};

enum {
    kMediaBayRegOffset			= 0x34,
    kFCR0Offset				= 0x38,
    kFCR1Offset				= 0x3C,
    kFCR2Offset				= 0x40,
    kFCR3Offset				= 0x44,
    kFCR4Offset				= 0x48,
	
    kMB0_Dev1_Enable_h			= 1 << 12,
	
//kFCR0_USB_RefSuspend_h		= 1 << 28,
    kFCR0_USB1_Cell_EN_h		= 1 << 24,
    kFCR0_USB1_PadSuspend1_h		= 1 << 23,
    kFCR0_USB1_PadSuspend0_h		= 1 << 22,
    kFCR0_USB0_Cell_EN_h		= 1 << 20,
    kFCR0_USB0_PadSuspend1_h		= 1 << 19,
    kFCR0_USB0_PadSuspend0_h		= 1 << 18,
    kFCR0_USB1_PadSuspendSel_h		= 1 << 17,
    kFCR0_USB1_RefSuspend_h		= 1 << 16,
    kFCR0_USB1_RefSuspendSel_h		= 1 << 15,
    kFCR0_USB1_PMI_En_h			= 1 << 14,
    kFCR0_USB0_PadSuspendSel_h		= 1 << 13,
    kFCR0_USB0_RefSuspend_h		= 1 << 12,
    kFCR0_USB0_RefSuspendSel_h		= 1 << 11,
    kFCR0_USB0_PMI_En_h			= 1 << 10,
    kFCR0_SCC_Cell_EN_h			= 1 << 6,
    kFCR0_SccBEnable_h			= 1 << 5,
    kFCR0_SccAEnable_h			= 1 << 4,
    kFCR0_SlowSccPClk_h			= 1 << 2,

    kFCR1_UltraIDE_Reset_l		= 1 << 30,
    kFCR1_IDE_UD_EN_h			= 1 << 29,
//kFCR1_EIDE1_Reset_l			= 1 << 27,
//kFCR1_EIDE1_EN_h			= 1 << 26,
//kFCR1_EIDE0_Reset_l			= 1 << 24,
//kFCR1_EIDE0_EN_h			= 1 << 23,
    kFCR1_I2S1Enable_h			= 1 << 20,
    kFCR1_I2S1_ClkEnBit_h		= 1 << 19,
    kFCR1_I2S1_Cell_EN_h		= 1 << 17,
    kFCR1_I2S0Enable_h			= 1 << 13,
    kFCR1_I2S0_ClkEnBit_h		= 1 << 12,
    kFCR1_I2S0_Cell_EN_h		= 1 << 10,	
    kFCR1_ChooseAudio			= 1 << 7,	// used by Bogart prototype boards only
    kFCR1_AUDIO_Cell_EN_h		= 1 << 6,
    kFCR1_AudioClkOut_EN_h		= 1 << 5,	// used by Burgundy sound chip only
    kFCR1_AudClkEnBit_h			= 1 << 3,
    kFCR1_Audio_Sel22MClk		= 1 << 1,
	
// include kFCR1_UltraIDE_Reset_l separately due to a problem with Quantum drive
//kFCR1_IDEResetBits			= kFCR1_EIDE1_Reset_l + kFCR1_EIDE0_Reset_l,
	
    kFCR2_AltDataOut			= 1 << 25,	// powers off modem when set	
    kFCR2_SleepStateBit_h		= 1 << 8,
    kFCR2_IOBus_EN_h			= 1 << 1,
	
//kFCR3_Stopping33Enabled_h		= 1 << 19,
    kFCR3_VIA_Clk16_EN_h		= 1 << 15,
    kFCR3_I2S0_Clk18_EN_h		= 1 << 14,
    kFCR3_I2S1_Clk18_EN_h		= 1 << 13,
    kFCR3_TIMER_Clk18_EN_h		= 1 << 12,
    kFCR3_Clk31_EN_h			= 1 << 11,
    kFCR3_Clk45_EN_h			= 1 << 10,
    kFCR3_Clk49_EN_h			= 1 << 9,
//kFCR3_Clk66_EN_h			= 1 << 8,
//kFCR3_Shutdown_PLL2X			= 1 << 7,
    kFCR3_PLL_Reset			= 1 << 5,
//kFCR3_Shutdown_PLLKW12		= 1 << 4,
    kFCR3_Shutdown_PLLKW35		= 1 << 3,
    kFCR3_Shutdown_PLLKW4		= 1 << 2,
    kFCR3_Shutdown_PLLKW6		= 1 << 1,
    kFCR3_Shutdown_PLL_Total		= 1 << 0,

    kFCR4_Port4_Resume			= 0x40000000,	// RO - resume detected on port 4 (bus 1, port 1)
    kFCR4_Port4_Connect			= 0x20000000,	// RO - device connect detected on port 4
    kFCR4_Port4_Disconnect		= 0x10000000,	// RO - device disconnect detected on port 4
    kFCR4_Port4_Enable			= 0x08000000,	// RW - enable port 4 events
    kFCR4_Port4_ResumeSelect		= 0x04000000,	// RW - enable resume on port 4
    kFCR4_Port4_ConnectSelect		= 0x02000000,	// RW - enable connect on port 4
    kFCR4_Port4_DisconnectSelect	= 0x01000000,	// RW - enable disconnect on port 4
	
    kFCR4_Port3_Resume			= 0x00400000,	// RO - resume detected on port 3 (bus 1, port 0)
    kFCR4_Port3_Connect			= 0x00200000,	// RO - device connect detected on port 3
    kFCR4_Port3_Disconnect		= 0x00100000,	// RO - device disconnect detected on port 3
    kFCR4_Port3_Enable			= 0x00080000,	// RW - enable port 3 events
    kFCR4_Port3_ResumeSelect		= 0x00040000,	// RW - enable resume on port 3
    kFCR4_Port3_ConnectSelect		= 0x00020000,	// RW - enable connect on port 3
    kFCR4_Port3_DisconnectSelect	= 0x00010000,	// RW - enable disconnect on port 3
	
    kFCR4_Port2_Resume			= 0x00004000,	// RO - resume detected on port 2 (bus 0, port 1)
    kFCR4_Port2_Connect			= 0x00002000,	// RO - device connect detected on port 2
    kFCR4_Port2_Disconnect		= 0x00001000,	// RO - device disconnect detected on port 2
    kFCR4_Port2_Enable			= 0x00000800,	// RW - enable port 2 events
    kFCR4_Port2_ResumeSelect		= 0x00000400,	// RW - enable resume on port 2
    kFCR4_Port2_ConnectSelect		= 0x00000200,	// RW - enable connect on port 2
    kFCR4_Port2_DisconnectSelect	= 0x00000100,	// RW - enable disconnect on port 2
	
    kFCR4_Port1_Resume			= 0x00000040,	// RO - resume detected on port 1 (bus 0, port 0)
    kFCR4_Port1_Connect			= 0x00000020,	// RO - device connect detected on port 1
    kFCR4_Port1_Disconnect		= 0x00000010,	// RO - device disconnect detected on port 1
    kFCR4_Port1_Enable			= 0x00000008,	// RW - enable port 1 events
    kFCR4_Port1_ResumeSelect		= 0x00000004,	// RW - enable resume on port 1
    kFCR4_Port1_ConnectSelect		= 0x00000002,	// RW - enable connect on port 1
    kFCR4_Port1_DisconnectSelect	= 0x00000001,	// RW - enable disconnect on port 1

	// Feature Control Register 5 Definitions (Pangea only, FCR5 does not exist on KeyLargo)
	kPangeaFCR5ViaUseClk31					= 1 << 0,				// ViaUseClk31
	kPangeaFCR5SCCUseClk31					= 1 << 1,				// SCCUseClk31
	kPangeaFCR5PwmClk32Enable				= 1 << 2,				// PwmClk32_EN_h
	kPangeaFCR5Clk3_68Enable				= 1 << 4,				// Clk3_68_EN_h
	kPangeaFCR5Clk32Enable					= 1 << 5,				// Clk32_EN_h
};

// desired state of Pangea FCR registers when sleeping
enum {
    // Feature Control Register 0 Sleep Settings
    kPangeaFCR0SleepBitsSet		=	0,
	
    kPangeaFCR0SleepBitsClear		=	kFCR0_USB1_Cell_EN_h |
                                                kFCR0_USB0_Cell_EN_h |
                                                kFCR0_SCC_Cell_EN_h |
                                                kFCR0_SccBEnable_h |
                                                kFCR0_SccAEnable_h,

    // Feature Control Register 1 Sleep Settings
    kPangeaFCR1SleepBitsSet		=	0,
	
    kPangeaFCR1SleepBitsClear		=	kFCR1_IDE_UD_EN_h |
                                                kFCR1_I2S1Enable_h |
						kFCR1_I2S1_ClkEnBit_h |
						kFCR1_I2S1_Cell_EN_h |
						kFCR1_I2S0Enable_h |
						kFCR1_I2S0_ClkEnBit_h |
						kFCR1_I2S0_Cell_EN_h |
						kFCR1_AUDIO_Cell_EN_h |
						kFCR1_AudioClkOut_EN_h |
						kFCR1_AudClkEnBit_h |
						kFCR1_Audio_Sel22MClk,
                                                
    // Feature Control Register 2 Sleep Settings
    kPangeaFCR2SleepBitsSet		=	kFCR2_AltDataOut,
	
    kPangeaFCR2SleepBitsClear		=	0,

    // Feature Control Register 3 Sleep and Restart Settings
    kPangeaFCR3SleepBitsSet		=	kFCR3_Shutdown_PLLKW35 |
						kFCR3_Shutdown_PLLKW4 |
						kFCR3_Shutdown_PLLKW6,
										
    kPangeaFCR3SleepBitsClear		=	kFCR3_VIA_Clk16_EN_h |
                                                kFCR3_I2S0_Clk18_EN_h |
						kFCR3_I2S1_Clk18_EN_h |
						kFCR3_TIMER_Clk18_EN_h |
                                                kFCR3_Clk31_EN_h |
						kFCR3_Clk45_EN_h |
						kFCR3_Clk49_EN_h,	

    // do not turn off SPI interface when restarting
	
    kPangeaFCR3RestartBitsSet		=	kFCR3_Shutdown_PLLKW35 |
                                                kFCR3_Shutdown_PLLKW4 |
						kFCR3_Shutdown_PLLKW6,
										
    kPangeaFCR3RestartBitsClear		=	kFCR3_I2S0_Clk18_EN_h |
						kFCR3_I2S0_Clk18_EN_h |
						kFCR3_I2S1_Clk18_EN_h |
						kFCR3_Clk31_EN_h |
						kFCR3_Clk45_EN_h |
						kFCR3_Clk49_EN_h,
                                                
    // Feature Control Register 4 Sleep Settings    
    kPangeaFCR4SleepBitsSet		=	0,
	
    kPangeaFCR4SleepBitsClear		=	0
};

// Much needed forward delcalration:
class USBKeyLargo;
class KeyLargoWatchDogTimer;

class KeyLargo : public AppleMacIO
{
  OSDeclareDefaultStructors(KeyLargo);
  
private:
  IOLogicalAddress	keyLargoBaseAddress;
  UInt32		keyLargoVersion;
  UInt32		keyLargoDeviceId;
  UInt32		keyLargoMediaBay;
  UInt32		keyLargoFCR[kKeyLargoFCRCount];
  UInt32		keyLargoGPIOLevels[2];
  UInt8			keyLargoExtIntGPIO[kKeyLargoExtIntGPIOCount];
  UInt8			keyLargoGPIO[kKeyLargoGPIOCount];
  
  // Remember if the media bay needs to be turnedOn:
  bool		   	mediaIsOn;
  void			EnableSCC(bool state, UInt8 device, bool type);
  void			PowerModem(bool state);
  void 			ModemResetLow();
  void 			ModemResetHigh();
  void			PowerI2S (bool powerOn, UInt32 cellNum);
  void			AdjustBusSpeeds ( void );
  
  KeyLargoWatchDogTimer	*watchDogTimer;
  
  // ***Added for outputting the FCR values to the IORegistry
  IOService 		*keyLargoService;
  const OSSymbol	*keyLargo_FCRNode;
  const OSObject	*fcrs[kPangeaFCRCount];  
  const OSArray	  	*fcrArray;
  
  // callPlatformFunction symbols
  const OSSymbol 	*keyLargo_resetUniNEthernetPhy;
  const OSSymbol 	*keyLargo_restoreRegisterState;
  const OSSymbol 	*keyLargo_syncTimeBase;
  const OSSymbol 	*keyLargo_saveRegisterState;
  const OSSymbol 	*keyLargo_turnOffIO;
  const OSSymbol 	*keyLargo_writeRegUInt8;
  const OSSymbol 	*keyLargo_safeWriteRegUInt8;
  const OSSymbol 	*keyLargo_safeReadRegUInt8;
  const OSSymbol 	*keyLargo_safeWriteRegUInt32;
  const OSSymbol 	*keyLargo_safeReadRegUInt32;
  const OSSymbol 	*keyLargo_powerMediaBay;
  const OSSymbol 	*keyLargo_enableSCC;
  const OSSymbol 	*keyLargo_powerModem;
  const OSSymbol 	*keyLargo_modemResetLow;
  const OSSymbol 	*keyLargo_modemResetHigh;
  const OSSymbol 	*keyLargo_getHostKeyLargo;
  const OSSymbol 	*keyLargo_powerI2S;
  
  // Offsets for the registers we wish to save.
  // These come (almost) unchanged from the MacOS9 Power
  // Manager plug-in (p99powerplugin.h)

// MPIC offsets and registers
enum
      {
  MPICIPI0 = 0x10A0,
  MPICIPI1 = 0x10B0,
  MPICIPI2 = 0x10C0,
  MPICIPI3 = 0x10D0,

  MPICSpuriousVector =  0x10E0,
  MPICTimeFreq = 0x10F0,

  MPICTimerBase0 = 0x1110,
  MPICTimerBase1 = 0x1150,
  MPICTimerBase2 = 0x1190,
  MPICTimerBase3 = 0x11D0,

  MPICIntSrcSize = 0x20,
  MPICIntSrcVectPriBase = 0x10000,
  MPICIntSrcDestBase = 0x10010,

  MPICP0CurrTaskPriority = 0x20080,
  MPICP1CurrTaskPriority = 0x21080,
  MPICP2CurrTaskPriority = 0x22080,
  MPICP3CurrTaskPriority = 0x23080
};

// 6522 VIA1 (and VIA2) register offsets

enum
      {
      vBufB                           =               0,                                      // BUFFER B
      vBufAH                          =               0x200,                                  // buffer a (with handshake) [ Dont use! ]
      vDIRB                           =               0x400,                                  // DIRECTION B
      vDIRA                           =               0x600,                                  // DIRECTION A
      vT1C                            =               0x800,                                  // TIMER 1 COUNTER (L.O.)
      vT1CH                           =               0xA00,                                  // timer 1 counter (high order)
      vT1L                            =               0xC00,                                  // TIMER 1 LATCH (L.O.)
      vT1LH                           =               0xE00,                                  // timer 1 latch (high order)
      vT2C                            =               0x1000,                                 // TIMER 2 LATCH (L.O.)
      vT2CH                           =               0x1200,                                 // timer 2 counter (high order)
      vSR                             =               0x1400,                                 // SHIFT REGISTER
      vACR                            =               0x1600,                                 // AUX. CONTROL REG.
      vPCR                            =               0x1800,                                 // PERIPH. CONTROL REG.
      vIFR                            =               0x1A00,                                 // INT. FLAG REG.
      vIER                            =               0x1C00,                                 // INT. ENABLE REG.
      vBufA                           =               0x1E00,                                 // BUFFER A
      vBufD                           =               vBufA                                   // disk head select is buffer A
      };

// FCR register offsets and constants
enum
      {
      kMediaBayRegOffset				= 0x34,
      kFCR0Offset					= 0x38,
      kFCR1Offset					= 0x3C,
      kFCR2Offset					= 0x40,
      kFCR3Offset					= 0x44,
      kFCR4Offset					= 0x48,

      kMB0_Dev1_Enable_h				= 1 << 12,

      kFCR0_USB_RefSuspend_h			= 1 << 28,
      kFCR0_USB1_Cell_EN_h			= 1 << 24,
      kFCR0_USB1_PadSuspend1_h		= 1 << 23,
      kFCR0_USB1_PadSuspend0_h		= 1 << 22,
      kFCR0_USB0_Cell_EN_h			= 1 << 20,
      kFCR0_USB0_PadSuspend1_h		= 1 << 19,
      kFCR0_USB0_PadSuspend0_h		= 1 << 18,
      kFCR0_IRDA_Clk19_EN_h			= 1 << 17,
      kFCR0_IRDA_Clk32_EN_h			= 1 << 16,
      kFCR0_IRDAEnable_h				= 1 << 15,
      kFCR0_SCC_Cell_EN_h			= 1 << 6,
      kFCR0_SccBEnable_h				= 1 << 5,
      kFCR0_SccAEnable_h				= 1 << 4,
      kFCR0_SlowSccPClk_h			= 1 << 2,

      kFCR1_UltraIDE_Reset_l			= 1 << 30,
      kFCR1_IDE_UD_EN_h				= 1 << 29,
      kFCR1_EIDE1_Reset_l			= 1 << 27,
      kFCR1_EIDE1_EN_h				= 1 << 26,
      kFCR1_EIDE0_Reset_l			= 1 << 24,
      kFCR1_EIDE0_EN_h				= 1 << 23,
      kFCR1_I2S1Enable_h				= 1 << 20,
      kFCR1_I2S1_ClkEnBit_h			= 1 << 19,
      kFCR1_I2S1_Cell_EN_h			= 1 << 17,
      kFCR1_I2S0Enable_h				= 1 << 13,
      kFCR1_I2S0_ClkEnBit_h			= 1 << 12,
      kFCR1_I2S0_Cell_EN_h			= 1 << 10,
      kFCR1_ChooseAudio				= 1 << 7,				// used by Bogart prototype boards only
      kFCR1_AUDIO_Cell_EN_h			= 1 << 6,
      kFCR1_AudioClkOut_EN_h			= 1 << 5,				// used by Burgundy sound chip only
      kFCR1_AudClkEnBit_h			= 1 << 3,
      kFCR1_Audio_Sel22MClk			= 1 << 1,

      // include kFCR1_UltraIDE_Reset_l separately due to a problem with Quantum drive
      kFCR1_IDEResetBits				= kFCR1_EIDE1_Reset_l + kFCR1_EIDE0_Reset_l,

      kFCR2_AltDataOut				= 1 << 25, // powers off modem when set
      kFCR2_SleepStateBit_h			= 1 << 8,
      kFCR2_IOBus_EN_h				= 1 << 1,

      kFCR3_Stopping33Enabled_h		= 1 << 19,
      kFCR3_VIA_Clk16_EN_h			= 1 << 15,
      kFCR3_I2S0_Clk18_EN_h			= 1 << 14,
      kFCR3_I2S1_Clk18_EN_h			= 1 << 13,
      kFCR3_TIMER_Clk18_EN_h			= 1 << 12,
      kFCR3_Clk31_EN_h				= 1 << 11,
      kFCR3_Clk45_EN_h				= 1 << 10,
      kFCR3_Clk49_EN_h				= 1 << 9,
      kFCR3_Clk66_EN_h				= 1 << 8,
      kFCR3_Shutdown_PLL2X			= 1 << 7,
      kFCR3_PLL_Reset				= 1 << 5,
      kFCR3_Shutdown_PLLKW12			= 1 << 4,
      kFCR3_Shutdown_PLLKW35			= 1 << 3,
      kFCR3_Shutdown_PLLKW4			= 1 << 2,
      kFCR3_Shutdown_PLLKW6			= 1 << 1,
      kFCR3_Shutdown_PLL_Total		= 1 << 0,

      kFCR4_Port4_Resume				=	0x40000000, // RO - resume detected on port 4 (bus 1, port 1)
      kFCR4_Port4_Connect			=	0x20000000, // RO - device connect detected on port 4
      kFCR4_Port4_Disconnect			=	0x10000000, // RO - device disconnect detected on port 4
      kFCR4_Port4_Enable				=	0x08000000, // RW - enable port 4 events
      kFCR4_Port4_ResumeSelect		=	0x04000000, // RW - enable resume on port 4
      kFCR4_Port4_ConnectSelect		=	0x02000000, // RW - enable connect on port 4
      kFCR4_Port4_DisconnectSelect	=	0x01000000, // RW - enable disconnect on port 4

      kFCR4_Port3_Resume				=	0x00400000, // RO - resume detected on port 3 (bus 1, port 0)
      kFCR4_Port3_Connect			=	0x00200000, // RO - device connect detected on port 3
      kFCR4_Port3_Disconnect			=	0x00100000, // RO - device disconnect detected on port 3
      kFCR4_Port3_Enable				=	0x00080000, // RW - enable port 3 events
      kFCR4_Port3_ResumeSelect		=	0x00040000, // RW - enable resume on port 3
      kFCR4_Port3_ConnectSelect		=	0x00020000, // RW - enable connect on port 3
      kFCR4_Port3_DisconnectSelect	=	0x00010000, // RW - enable disconnect on port 3

      kFCR4_Port2_Resume				=	0x00004000, // RO - resume detected on port 2 (bus 0, port 1)
      kFCR4_Port2_Connect			=	0x00002000, // RO - device connect detected on port 2
      kFCR4_Port2_Disconnect			=	0x00001000, // RO - device disconnect detected on port 2
      kFCR4_Port2_Enable				=	0x00000800, // RW - enable port 2 events
      kFCR4_Port2_ResumeSelect		=	0x00000400, // RW - enable resume on port 2
      kFCR4_Port2_ConnectSelect		=	0x00000200, // RW - enable connect on port 2
      kFCR4_Port2_DisconnectSelect	=	0x00000100, // RW - enable disconnect on port 2

      kFCR4_Port1_Resume				=	0x00000040, // RO - resume detected on port 1 (bus 0, port 0)
      kFCR4_Port1_Connect			=	0x00000020, // RO - device connect detected on port 1
      kFCR4_Port1_Disconnect			=	0x00000010, // RO - device disconnect detected on port 1
      kFCR4_Port1_Enable				=	0x00000008, // RW - enable port 1 events
      kFCR4_Port1_ResumeSelect		=	0x00000004, // RW - enable resume on port 1
      kFCR4_Port1_ConnectSelect		=	0x00000002, // RW - enable connect on port 1
      kFCR4_Port1_DisconnectSelect	=	0x00000001 // RW - enable disconnect on port 1
      };


  // Power Managment support functions and data structures:
  // These come (almost) unchanged from the MacOS9 Power
  // Manager plug-in (p99powerplugin.c)

  struct MPICTimers {
      UInt32 currentCountRegister;
      UInt32 baseCountRegister;
      UInt32 vectorPriorityRegister;
      UInt32 destinationRegister;
  };
  typedef struct MPICTimers MPICTimers;
  typedef volatile MPICTimers *MPICTimersPtr;

  struct KeyLargoMPICState {
      UInt32 				mpicIPI[4];
      UInt32 				mpicSpuriousVector;
      UInt32 				mpicTimerFrequencyReporting;
      MPICTimers 			mpicTimers[4];
      UInt32 				mpicInterruptSourceVectorPriority[64];
      UInt32 				mpicInterruptSourceDestination[64];
      UInt32				mpicCurrentTaskPriorities[4];
  };
  typedef struct KeyLargoMPICState KeyLargoMPICState;
  typedef volatile KeyLargoMPICState *KeyLargoMPICStatePtr;

  struct KeyLargoGPIOState {
      UInt32 				gpioLevels[2];
      UInt8 				extIntGPIO[18];
      UInt8 				gpio[17];
  };
  typedef struct KeyLargoGPIOState KeyLargoGPIOState;
  typedef volatile KeyLargoGPIOState *KeyLargoGPIOStatePtr;


  struct KeyLargoConfigRegistersState {
      UInt32 				mediaBay;
      UInt32 				featureControl[5];
  };
  typedef struct KeyLargoConfigRegistersState KeyLargoConfigRegistersState;
  typedef volatile KeyLargoConfigRegistersState *KeyLargoConfigRegistersStatePtr;

  // This is a short version of the IODBDMAChannelRegisters which includes only
  // the registers we actually mean to save
  struct DBDMAChannelRegisters {
      UInt32 	commandPtrLo;
      UInt32 	interruptSelect;
      UInt32 	branchSelect;
      UInt32 	waitSelect;
  };
  typedef struct DBDMAChannelRegisters DBDMAChannelRegisters;
  typedef volatile DBDMAChannelRegisters *DBDMAChannelRegistersPtr;

  struct KeyLargoDBDMAState {
      DBDMAChannelRegisters 			dmaChannel[13];
  };
  typedef struct KeyLargoDBDMAState KeyLargoDBDMAState;
  typedef volatile KeyLargoDBDMAState *KeyLargoDBDMAStatePtr;


  struct KeyLargoAudioState {
      UInt32					audio[25];
  };
  typedef struct KeyLargoAudioState KeyLargoAudioState;
  typedef volatile KeyLargoAudioState *KeyLargoAudioStateStatePtr;


  struct KeyLargoI2SState {
      UInt32					i2s[20];
  };
  typedef struct KeyLargoI2SState KeyLargoI2SState;
  typedef volatile KeyLargoI2SState *KeyLargoI2SStateStatePtr;

  struct KeyLargoState {
      bool                     thisStateIsValid;
      KeyLargoMPICState		savedMPICState;
      KeyLargoGPIOState		savedGPIOState;
      KeyLargoConfigRegistersState	savedConfigRegistersState;
      KeyLargoDBDMAState		savedDBDMAState;
      KeyLargoAudioState		savedAudioState;
      KeyLargoI2SState		savedI2SState;
      UInt8					savedVIAState[9];
  };
  typedef struct KeyLargoState KeyLargoState;

  // These are actually the buffes where we save keylargo's state (above there
  // are only definitions).
  KeyLargoState savedKeyLargState;
  
  // This is instead only for the wireless slot:
  typedef struct WirelessPower {
	bool cardPower;				// Keeps track of the power in the wireless card.
	UInt8 wirelessCardReg[5];	// A backup of the registers we are overwriting.
	} WirelessPower;
  WirelessPower cardStatus;
	

  // the two USB busses:
  enum {
    kNumUSB = 2
  };
  USBKeyLargo *usbBus[kNumUSB];

  // Methods to save and restore the state:
  void saveKeyLargoState();
  void restoreKeyLargoState();
  void saveVIAState();
  void restoreVIAState();

  // this is to ensure mutual exclusive access to
  // the keylargo registers:
  IOSimpleLock *mutex;

  // Remember the bus speed:
  long long busSpeed;
  
  // Reference counts for shared hardware
  long clk31RefCount;			// 31.3 MHz clock - SCC & VIA
  long clk45RefCount;			// 45.1 MHz clock - Audio, I2S & SCC
  long clk49RefCount;			// 49.1 MHz clock - Audio & I2S
  long clk32RefCount;			// 32.0 MHz clock - SCC & VIA (Pangea only)
  
  void resetUniNEthernetPhy(void);
  
public:
  virtual bool      init(OSDictionary *);
  virtual bool      start(IOService *provider);
  virtual void      stop(IOService *provider);
  
  virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
                                        void *param1, void *param2,
					void *param3, void *param4);
  
  virtual long long syncTimeBase(void);

  virtual void      turnOffKeyLargoIO(bool restart);
  virtual void      turnOffPangeaIO(bool restart);

  virtual void      powerWireless(bool powerOn);

  virtual void		setReferenceCounts (void);
  virtual void      saveRegisterState(void);
  virtual void      restoreRegisterState(void);
  virtual void      enableCells();
  
  virtual UInt8     readRegUInt8(unsigned long offset);
  virtual void      writeRegUInt8(unsigned long offset, UInt8 data);
  virtual UInt32    readRegUInt32(unsigned long offset);
  virtual void      writeRegUInt32(unsigned long offset, UInt32 data);
  
  // Remember if the media bay needs to be turnedOn:
  virtual void      powerMediaBay(bool powerOn, UInt8 whichDevice);  

  // share register access:
  void safeWriteRegUInt8(unsigned long offset, UInt8 mask, UInt8 data);
  UInt8 safeReadRegUInt8(unsigned long offset);
  void safeWriteRegUInt32(unsigned long offset, UInt32 mask, UInt32 data);
  UInt32 safeReadRegUInt32(unsigned long offset);

  // Power handling methods:
  void initForPM (IOService *provider);
  IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice);
};

class KeyLargoWatchDogTimer : public IOWatchDogTimer
{
  OSDeclareDefaultStructors(KeyLargoWatchDogTimer);
  
private:
  KeyLargo *keyLargo;
  
public:
  static KeyLargoWatchDogTimer *withKeyLargo(KeyLargo *keyLargo);
  virtual bool start(IOService *provider);
  virtual void setWatchDogTimer(UInt32 timeOut);
};

#endif /* ! _IOKIT_KEYLARGO_H */
