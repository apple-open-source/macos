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
 * Copyright (c) 1998-2001 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */

#ifndef _IOKIT_KEYLARGO_H
#define _IOKIT_KEYLARGO_H

#include <IOKit/IOLocks.h>

#include <IOKit/platform/AppleMacIO.h>

#include "USBKeyLargo.h"
#include "KeyLargoWatchDogTimer.h"

enum {
    kKeyLargoDeviceId22			= 0x22,				// KeyLargo
	kKeyLargoVersion2			= 2,

    kPangeaDeviceId25			= 0x25,				// Pangea
    kPangeaVersion0				= 0,
        
    kIntrepidDeviceId3e			= 0x3e,				// Intrepid
    kIntrepidVersion0			= 0    
};

enum {
	// KeyLargo Register Offsets
	kKeyLargoMediaBay						= 0x00034,			// KeyLargo only, reserved on Pangea
	kKeyLargoFCRBase						= 0x00038,
	kKeyLargoFCRCount						= 5,
	kPangeaFCRCount							= 6,
	kIntrepidFCRCount						= 6,
	kKeyLargoFCR0							= 0x00038,
	kKeyLargoFCR1							= 0x0003C,
	kKeyLargoFCR2							= 0x00040,
	kKeyLargoFCR3							= 0x00044,
	kKeyLargoFCR4							= 0x00048,
	kKeyLargoFCR5							= 0x0004C,			// Pangea only
  
	kKeyLargoGTimerFreq						= 18432000UL,
	kKeyLargoWatchDogLowOffset				= 0x15030,
	kKeyLargoWatchDogHighOffset				= 0x15034,
	kKeyLargoCounterLoOffset				= 0x15038,
	kKeyLargoCounterHiOffset				= 0x1503C,
	kKeyLargoWatchDogEnableOffset			= 0x15048,

    // I2S serial format register (for tracking selected clock)
    kKeyLargoI2S0SerialFormat				= 0x10010,
    kKeyLargoI2S1SerialFormat				= 0x11010,
    kKeylargoI2SClockSelect					= (1<<31) | (1<<30),
    kKeylargoI2SSelect45Mhz					= 1 << 30,
    kKeylargoI2SSelect49Mhz					= 1 << 31,

	// MediaBay Register Definitions (KeyLargo only)
	kKeyLargoMB0DevEnable					= 1 << 12,			// MB0_Dev1_Enable_h
	kKeyLargoMB0DevPower					= 1 << 10,			// MB0_Power_l
	kKeyLargoMB0DevReset					= 1 <<  9,			// MB0_Reset_l
	kKeyLargoMB0Enable						= 1 <<  8,			// MB0_Enable_h
	kKeyLargoMB1DevEnable					= 1 << 28,			// MB1_Dev1_Enable_h
	kKeyLargoMB1DevPower					= 1 << 26,			// MB1_Power_l
	kKeyLargoMB1DevReset					= 1 << 25,			// MB1_Reset_l
	kKeyLargoMB1Enable						= 1 << 24,			// MB1_Enable_h
  
	// Feature Control Register 0 Definitions
	kKeyLargoFCR0ChooseSCCB					= 1 << 0,			// ChooseSCCB (KeyLargo only - reserved on Pangea and Intrepid)
	kKeyLargoFCR0ChooseSCCA					= 1 << 1,			// ChooseSCCA
	kKeyLargoFCR0SlowSccPClk				= 1 << 2,			// SlowSccPClk_h
	kKeyLargoFCR0ResetSCC					= 1 << 3,			// ResetScc_h
	kKeyLargoFCR0SccAEnable					= 1 << 4,			// SccAEnable_h
	kKeyLargoFCR0SccBEnable					= 1 << 5,			// SccBEnable_h
	kKeyLargoFCR0SccCellEnable				= 1 << 6,			// SCC_Cell_EN_h
	kKeyLargoFCR0ChooseVIA					= 1 << 7,			// ChooseVIA
	kKeyLargoFCR0HighBandFor1MB				= 1 << 8,			// HighBandFor1MB (KeyLargo only - reserved on Pangea and Intrepid)
	kKeyLargoFCR0UseIRSource2				= 1 << 9,			// UseIRSource2 (KeyLargo only - reserved on Pangea and Intrepid)
	kKeyLargoFCR0UseIRSource1				= 1 << 10,			// UseIRSource1 (KeyLargo only)
	kPangeaFCR0USB0PMIEnable				= 1 << 10,			// USB0_PMI_En_h (Pangea and Intrepid only)
	kKeyLargoFCR0IRDASWReset				= 1 << 11,			// IRDA_SW_Reset_h (KeyLargo only)
	kPangeaFCR0USB0RefSuspendSel			= 1 << 11,			// USB0_RefSuspendSel_h (Pangea and Intrepid only)
	kKeyLargoFCR0IRDADefault1				= 1 << 12,			// IRDA_Default1 (KeyLargo only)
	kPangeaFCR0USB0RefSuspend				= 1 << 12,			// USB0_RefSuspend_h (Pangea and Intrepid only)
	kKeyLargoFCR0IRDADefault0				= 1 << 13,			// IRDA_Default0 (KeyLargo only)
	kPangeaFCR0USB0PadSuspendSel			= 1 << 13,			// USB0_PadSuspendSel_h (Pangea and Intrepid only)
	kKeyLargoFCR0IRDAFastCon				= 1 << 14,			// IRDA_FAST_CON (KeyLargo only)
	kPangeaFCR0USB1PMIEnable				= 1 << 14,			// USB0_PadSuspendSel_h (Pangea and Intrepid only)
	kKeyLargoFCR0IRDAEnable					= 1 << 15,			// IRDAEnable_h (KeyLargo only)
	kPangeaFCR0USB1RefSuspendSel			= 1 << 15,			// USB1_RefSuspendSel_h (Pangea and Intrepid only)
	kKeyLargoFCR0IRDAClk32Enable			= 1 << 16,			// IRDA_Clk32_EN_h (KeyLargo only)
	kPangeaFCR0USB1RefSuspend				= 1 << 16,			// USB1_RefSuspend_h (Pangea and Intrepid only)
	kKeyLargoFCR0IRDAClk19Enable			= 1 << 17,			// IRDA_Clk19_EN_h (KeyLargo only)
	kPangeaFCR0USB1PadSuspendSel			= 1 << 17,			// USB1_PadSuspendSel_h (Pangea and Intrepid only)
	kKeyLargoFCR0USB0PadSuspend0			= 1 << 18,			// USB0_PadSuspend0_h
	kKeyLargoFCR0USB0PadSuspend1			= 1 << 19,			// USB0_PadSuspend1_h
	kKeyLargoFCR0USB0CellEnable				= 1 << 20,			// USB0_Cell_EN_h
	kKeyLargoFCR0USB1PadSuspend0			= 1 << 22,			// USB1_PadSuspend0_h
	kKeyLargoFCR0USB1PadSuspend1			= 1 << 23,			// USB1_PadSuspend1_h
	kKeyLargoFCR0USB1CellEnable				= 1 << 24,			// USB1_Cell_EN_h
	kKeyLargoFCR0USBRefSuspend				= 1 << 28,			// USB_RefSuspend_h (KeyLargo only - reserved on Pangea and Intrepid)
  
	// Feature Control Register 1 Definitions
	kIntrepidFCR1USB2PMIEnable				= 1 << 0,			// USB2_PMI_En_h (Intrepid only)
	kKeyLargoFCR1AudioSel22MClk				= 1 << 1,			// Audio_Sel22MClk (KeyLargo and Pangea)
	kIntrepidFCR1USB2RefSuspendSel			= 1 << 1,			// USB2_RefSuspendSel_h (Intrepid only)
	kIntrepidFCR1USB2RefSuspend				= 1 << 2,			// USB2_RefSuspend_h (Intrepid only)
	kKeyLargoFCR1AudioClkEnable				= 1 << 3,			// AudClkEnBit_h (KeyLargo and Pangea)
	kIntrepidFCR1USB2PadSuspendSel			= 1 << 3,			// USB2_PadSuspendSel_h (Intrepid only)
	kIntrepidFCR1USB2PadSuspend0			= 1 << 4,			// USB2_PadSuspend0_h (Intrepid only)
	kKeyLargoFCR1AudioClkOutEnable			= 1 << 5,			// AudioClkOut_EN_h (KeyLargo and Pangea)
	kIntrepidFCR1USB2PadSuspend1			= 1 << 5,			// USB2_PadSuspend1_h (Intrepid only)
	kKeyLargoFCR1AudioCellEnable			= 1 << 6,			// AUDIO_Cell_EN_h (KeyLargo and Pangea)
	kIntrepidFCR1USB2CellEnable				= 1 << 6,			// USB2_Cell_EN_h (Intrepid only)
	kKeyLargoFCR1ChooseAudio				= 1 << 7,			// ChooseAudio (KeyLargo and Pangea)
	
	kKeyLargoFCR1ChooseI2S0					= 1 << 9,			// ChooseI2S0 (KeyLargo only - reserved on Pangea and Intrepid)
	kKeyLargoFCR1I2S0CellEnable				= 1 << 10,			// I2S0_Cell_EN_h
	kKeyLargoFCR1I2S0ClkEnable				= 1 << 12,			// I2S0_ClkEnBit_h
	kKeyLargoFCR1I2S0Enable					= 1 << 13,			// I2S0Enable_h
	
	kKeyLargoFCR1I2S1CellEnable				= 1 << 17,			// I2S1_Cell_EN_h
	kKeyLargoFCR1I2S1ClkEnable				= 1 << 19,			// I2S1_ClkEnBit_h
	kKeyLargoFCR1I2S1Enable					= 1 << 20,			// I2S1Enable_h
	kKeyLargoFCR1EIDE0Enable				= 1 << 23,			// EIDE0_EN_h (KeyLargo and Intrepid - reserved on Pangea)
	
	kKeyLargoFCR1EIDE0Reset					= 1 << 24,			// EIDE0_Reset_l (KeyLargo and Intrepid only - reserved on Pangea)
	kKeyLargoFCR1EIDE1Enable				= 1 << 26,			// EIDE1_EN_h (KeyLargo only - reserved on Pangea and Intrepid)
	kKeyLargoFCR1EIDE1Reset					= 1 << 27,			// EIDE1_Reset_l (KeyLargo only - reserved on Pangea and Intrepid)
	kKeyLargoFCR1UIDEEnable					= 1 << 29,			// IDE_UD_EN_h (KeyLargo and Pangea only - reserved on Intrepid)
	kKeyLargoFCR1UIDEReset					= 1 << 30,			// UltraIDE_Reset_l (KeyLargo and Pangea only - reserved on Intrepid)
  
  
	// Feature Control Register 2 Definitions
	kKeyLargoFCR2IOBusEnable				= 1 << 1,			// IOBus_EN_h
	kKeyLargoFCR2SleepState					= 1 << 8,			// SleepStateBit_h (KeyLargo only)
	kPangeaFCR2StopAllKLClocks				= 1 << 8,			// StopAllKLClocks (Pangea only)
	kKeyLargoFCR2MPICEnable					= 1 << 17,			// MPIC_Enable_h
	kPangeaCardSlotReset					= 1 << 18,			// CardSlot_Reset_h (Pangea and Intrepid only - reserved on KeyLargo)
	kKeyLargoFCR2AltDataOut					= 1 << 25,			// AltDataOut - spare on Pangea
  
	// Feature Control Register 3 Definitions
	kKeyLargoFCR3ShutdownPLLTotal			= 1 << 0,			// Shutdown_PLL_Total (Pangea and KeyLargo only - reserved on Intrepid)
	kKeyLargoFCR3ShutdownPLLKW6				= 1 << 1,			// Shutdown_PLLKW6 (Pangea and KeyLargo)
	kIntrepidFCR3ShutdownPLL3				= 1 << 1,			// Shutdown_PLL3 (Intrepid only)
	kKeyLargoFCR3ShutdownPLLKW4				= 1 << 2,			// Shutdown_PLLKW4 (Pangea and KeyLargo)
	kIntrepidFCR3ShutdownPLL2				= 1 << 2,			// Shutdown_PLL2 (Intrepid only)
	kKeyLargoFCR3ShutdownPLLKW35			= 1 << 3,			// Shutdown_PLLKW35 (Pangea and KeyLargo)
	kIntrepidFCR3ShutdownPLL1				= 1 << 3,			// Shutdown_PLL1 (Intrepid only)
	kKeyLargoFCR3ShutdownPLLKW12			= 1 << 4,			// Shutdown_PLLKW12 (KeyLargo only - reserved on Pangea)
	kIntrepidFCR3EnablePll3Shutdown			= 1 << 4,			// EnablePLL3Shutdown (Intrepid only)
	kKeyLargoFCR3PLLReset					= 1 << 5,			// PLL_Reset (Pangea and KeyLargo)
	kIntrepidFCR3EnablePLL2Shutdown			= 1 << 5,			// EnablePLL2Shutdown (Intrepid only)
	kIntrepidFCR3EnablePLL1Shutdown			= 1 << 6,			// EnablePLL1Shutdown (Intrepid only)
	kKeyLargoFCR3ShutdownPLL2X				= 1 << 7,			// Shutdown_PLL2X (KeyLargo only - reserved on Pangea and Intrepid)
	
	kKeyLargoFCR3Clk66Enable				= 1 << 8,			// Clk66_EN_h (KeyLargo only - reserved on Pangea and Intrepid)
	kKeyLargoFCR3Clk49Enable				= 1 << 9,			// Clk49_EN_h
	kKeyLargoFCR3Clk45Enable				= 1 << 10,			// Clk45_EN_h
	kKeyLargoFCR3Clk31Enable				= 1 << 11,			// Clk31_EN_h (KeyLargo and Pangea only - reserved on Intrepid)
	kKeyLargoFCR3TimerClk18Enable			= 1 << 12,			// TIMER_Clk18_EN_h
	kKeyLargoFCR3I2S1Clk18Enable			= 1 << 13,			// I2S1_Clk18_EN_h
	kKeyLargoFCR3I2S0Clk18Enable			= 1 << 14,			// I2S0_Clk18_EN_h
	kKeyLargoFCR3ViaClk16Enable				= 1 << 15,			// VIA_Clk16_EN_h (KeyLargo & Pangea only)
	kIntrepidFCR3ViaClk32Enable				= 1 << 15,			// VIA_Clk32_EN_h (Intrepid only)
	
	kKeyLargoFCR3Stopping33Enabled			= 1 << 19,			// Stopping33Enabled_h (KeyLargo only)
	kPangeaFCR3PLLEnableTest				= 1 << 19,			// PLL_EN_TST (Pangea only)
  
	kIntrepidFCR3Port5DisconnectSelect		= 1 << 16,			// Port5_DisconnectSelect (Intrepid only)
	kIntrepidFCR3Port5ConnectSelect			= 1 << 17,			// Port5_ConnectSelect (Intrepid only)
	kIntrepidFCR3Port5ResumeSelect			= 1 << 18,			// Port5_ResumeSelect (Intrepid only)
	kIntrepidFCR3Port5Enable				= 1 << 19, 			// Port5_Enable (Intrepid only)
	kIntrepidFCR3Port5Disconnect			= 1 << 20, 			// Port5_Disconnect (Intrepid only)
	kIntrepidFCR3Port5Connect				= 1 << 21, 			// Port5_Connect (Intrepid only)
	kIntrepidFCR3Port5Resume				= 1 << 22, 			// Port5_Resume (Intrepid only)
	
	kIntrepidFCR3Port6DisconnectSelect		= 1 << 24,			// Port6_DisconnectSelect (Intrepid only)
	kIntrepidFCR3Port6ConnectSelect			= 1 << 25,			// Port6_ConnectSelect (Intrepid only)
	kIntrepidFCR3Port6ResumeSelect			= 1 << 26,			// Port6_ResumeSelect (Intrepid only)
	kIntrepidFCR3Port6Enable				= 1 << 27, 			// Port6_Enable (Intrepid only)
	kIntrepidFCR3Port6Disconnect			= 1 << 28, 			// Port6_Disconnect (Intrepid only)
	kIntrepidFCR3Port6Connect				= 1 << 29, 			// Port6_Connect (Intrepid only)
	kIntrepidFCR3Port6Resume				= 1 << 30, 			// Port6_Resume (Intrepid only)

	// Feature Control Register 4 Definitions
	kKeyLargoFCR4Port1DisconnectSelect		= 1 << 0,			// Port1_DisconnectSelect
	kKeyLargoFCR4Port1ConnectSelect			= 1 << 1,			// Port1_ConnectSelect
	kKeyLargoFCR4Port1ResumeSelect			= 1 << 2,			// Port1_ResumeSelect
	kKeyLargoFCR4Port1Enable				= 1 << 3,			// Port1_Enable
	kKeyLargoFCR4Port1Disconnect			= 1 << 4,			// Port1_Disconnect
	kKeyLargoFCR4Port1Connect				= 1 << 5,			// Port1_Connect
	kKeyLargoFCR4Port1Resume				= 1 << 6,			// Port1_Resume
  
	kKeyLargoFCR4Port2DisconnectSelect		= 1 << 8,			// Port2_DisconnectSelect
	kKeyLargoFCR4Port2ConnectSelect			= 1 << 9,			// Port2_ConnectSelect
	kKeyLargoFCR4Port2ResumeSelect			= 1 << 10,			// Port2_ResumeSelect
	kKeyLargoFCR4Port2Enable				= 1 << 11,			// Port2_Enable
	kKeyLargoFCR4Port2Disconnect			= 1 << 12,			// Port2_Disconnect
	kKeyLargoFCR4Port2Connect				= 1 << 13,			// Port2_Connect
	kKeyLargoFCR4Port2Resume				= 1 << 14,			// Port2_Resume
  
	kKeyLargoFCR4Port3DisconnectSelect		= 1 << 16,			// Port3_DisconnectSelect
	kKeyLargoFCR4Port3ConnectSelect			= 1 << 17,			// Port3_ConnectSelect
	kKeyLargoFCR4Port3ResumeSelect			= 1 << 18,			// Port3_ResumeSelect
	kKeyLargoFCR4Port3Enable				= 1 << 19,			// Port3_Enable
	kKeyLargoFCR4Port3Disconnect			= 1 << 20,			// Port3_Disconnect
	kKeyLargoFCR4Port3Connect				= 1 << 21,			// Port3_Connect
	kKeyLargoFCR4Port3Resume				= 1 << 22,			// Port3_Resume
  
	kKeyLargoFCR4Port4DisconnectSelect		= 1 << 24,			// Port4_DisconnectSelect
	kKeyLargoFCR4Port4ConnectSelect			= 1 << 25,			// Port4_ConnectSelect
	kKeyLargoFCR4Port4ResumeSelect			= 1 << 26,			// Port4_ResumeSelect
	kKeyLargoFCR4Port4Enable				= 1 << 27,			// Port4_Enable
	kKeyLargoFCR4Port4Disconnect			= 1 << 28,			// Port4_Disconnect
	kKeyLargoFCR4Port4Connect				= 1 << 29,			// Port4_Connect
	kKeyLargoFCR4Port4Resume				= 1 << 30,			// Port4_Resume
	
	// Feature Control Register 5 Definitions (Pangea and Intrepid only, FCR5 does not exist on KeyLargo)
	kPangeaFCR5ViaUseClk31					= 1 << 0,				// ViaUseClk31 (Pangea only, reserved on Intrepid)
	kPangeaFCR5SCCUseClk31					= 1 << 1,				// SCCUseClk31 (Pangea only, reserved on Intrepid)
	kPangeaFCR5PwmClk32Enable				= 1 << 2,				// PwmClk32_EN_h
	kPangeaFCR5Clk3_68Enable				= 1 << 4,				// Clk3_68_EN_h
	kPangeaFCR5Clk32Enable					= 1 << 5,				// Clk32_EN_h
};

enum {
  // Feature Control Register 0 Sleep Settings for KeyLargo
  kKeyLargoFCR0SleepBitsSet				= (kKeyLargoFCR0USBRefSuspend),
  
  kKeyLargoFCR0SleepBitsClear			= (kKeyLargoFCR0SccAEnable |
											kKeyLargoFCR0SccBEnable |
											kKeyLargoFCR0SccCellEnable |
											kKeyLargoFCR0IRDAEnable |
											kKeyLargoFCR0IRDAClk32Enable |
											kKeyLargoFCR0IRDAClk19Enable),
  
  // Feature Control Register 1 Sleep Settings
  kKeyLargoFCR1SleepBitsSet				= 0,
  
  kKeyLargoFCR1SleepBitsClear			= (kKeyLargoFCR1AudioSel22MClk |
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
  kKeyLargoFCR2SleepBitsSet				= 0,
  
  kKeyLargoFCR2SleepBitsClear			= kKeyLargoFCR2IOBusEnable,
  
  // Feature Control Register 3 Sleep and Restart Settings
  kKeyLargoFCR3SleepBitsSet				= (kKeyLargoFCR3ShutdownPLLKW6 |
											kKeyLargoFCR3ShutdownPLLKW4 |
											kKeyLargoFCR3ShutdownPLLKW35 |
											kKeyLargoFCR3ShutdownPLLKW12),
  
  kKeyLargoFCR3SleepBitsClear			= (kKeyLargoFCR3Clk66Enable |
											kKeyLargoFCR3Clk49Enable |
											kKeyLargoFCR3Clk45Enable |
											kKeyLargoFCR3Clk31Enable |
											kKeyLargoFCR3TimerClk18Enable |
											kKeyLargoFCR3I2S1Clk18Enable |
											kKeyLargoFCR3I2S0Clk18Enable |
											kKeyLargoFCR3ViaClk16Enable),
  
  kKeyLargoFCR3RestartBitsSet			= (kKeyLargoFCR3ShutdownPLLKW6 |
											kKeyLargoFCR3ShutdownPLLKW4 |
											kKeyLargoFCR3ShutdownPLLKW35),
  
  kKeyLargoFCR3RestartBitsClear			= (kKeyLargoFCR3Clk66Enable |
											kKeyLargoFCR3Clk49Enable |
											kKeyLargoFCR3Clk45Enable |
											kKeyLargoFCR3Clk31Enable |
											kKeyLargoFCR3I2S1Clk18Enable |
											kKeyLargoFCR3I2S0Clk18Enable),
  
	// Feature Control Register 4 Sleep Settings
	// Marco since we are going to have two different controllers for each usb, I am going
	// to separate each bus bit set:
    kKeyLargoFCR4USB0SleepBitsSet		= (kKeyLargoFCR4Port1DisconnectSelect |
											kKeyLargoFCR4Port1ConnectSelect |
											kKeyLargoFCR4Port1ResumeSelect |
											kKeyLargoFCR4Port1Enable |
											kKeyLargoFCR4Port2DisconnectSelect |
											kKeyLargoFCR4Port2ConnectSelect |
											kKeyLargoFCR4Port2ResumeSelect |
											kKeyLargoFCR4Port2Enable),

    kKeyLargoFCR4USB1SleepBitsSet		= (kKeyLargoFCR4Port3DisconnectSelect |
											kKeyLargoFCR4Port3ConnectSelect |
											kKeyLargoFCR4Port3ResumeSelect |
											kKeyLargoFCR4Port3Enable |
											kKeyLargoFCR4Port4DisconnectSelect |
											kKeyLargoFCR4Port4ConnectSelect |
											kKeyLargoFCR4Port4ResumeSelect |
											kKeyLargoFCR4Port4Enable),

    kKeyLargoFCR4SleepBitsSet			= (kKeyLargoFCR4USB0SleepBitsSet |
											kKeyLargoFCR4USB1SleepBitsSet),

    kKeyLargoFCR4USB0SleepBitsClear		= 0,
    kKeyLargoFCR4USB1SleepBitsClear		= 0,

    kKeyLargoFCR4SleepBitsClear			= kKeyLargoFCR4USB0SleepBitsClear | kKeyLargoFCR4USB1SleepBitsClear
};

// desired state of Pangea FCR registers when sleeping
enum {
	// Feature Control Register 0 Sleep Settings for Pangea
    kPangeaFCR0SleepBitsSet			=	0,
	
    kPangeaFCR0SleepBitsClear		=	kKeyLargoFCR0USB1CellEnable |
											kKeyLargoFCR0USB0CellEnable |
											kKeyLargoFCR0SccCellEnable |
											kKeyLargoFCR0SccBEnable |
											kKeyLargoFCR0SccAEnable,

    // Feature Control Register 1 Sleep Settings
    kPangeaFCR1SleepBitsSet			=	0,
	
    kPangeaFCR1SleepBitsClear		=	kKeyLargoFCR1UIDEEnable |
											kKeyLargoFCR1I2S1Enable |
											kKeyLargoFCR1I2S1ClkEnable |
											kKeyLargoFCR1I2S1CellEnable |
											kKeyLargoFCR1I2S0Enable |
											kKeyLargoFCR1I2S0ClkEnable |
											kKeyLargoFCR1I2S0CellEnable |
											kKeyLargoFCR1AudioCellEnable |
											kKeyLargoFCR1AudioClkOutEnable |
											kKeyLargoFCR1AudioClkEnable |
											kKeyLargoFCR1AudioSel22MClk,
                                                
    // Feature Control Register 2 Sleep Settings
    kPangeaFCR2SleepBitsSet			=	kKeyLargoFCR2AltDataOut,
	
    kPangeaFCR2SleepBitsClear		=	0,

    // Feature Control Register 3 Sleep and Restart Settings
    kPangeaFCR3SleepBitsSet			=	kKeyLargoFCR3ShutdownPLLKW35 |
											kKeyLargoFCR3ShutdownPLLKW4 |
											kKeyLargoFCR3ShutdownPLLKW6,
										
    kPangeaFCR3SleepBitsClear		=	kKeyLargoFCR3ViaClk16Enable |
											kKeyLargoFCR3I2S0Clk18Enable |
											kKeyLargoFCR3I2S1Clk18Enable |
											kKeyLargoFCR3TimerClk18Enable |
											kKeyLargoFCR3Clk31Enable |
											kKeyLargoFCR3Clk45Enable |
											kKeyLargoFCR3Clk49Enable,	

    // do not turn off SPI interface when restarting
	
    kPangeaFCR3RestartBitsSet		=	kKeyLargoFCR3ShutdownPLLKW35 |
											kKeyLargoFCR3ShutdownPLLKW4 |
											kKeyLargoFCR3ShutdownPLLKW6,
										
    kPangeaFCR3RestartBitsClear		=	kKeyLargoFCR3I2S0Clk18Enable |
											kKeyLargoFCR3I2S1Clk18Enable |
											kKeyLargoFCR3Clk31Enable |
											kKeyLargoFCR3Clk45Enable |
											kKeyLargoFCR3Clk49Enable,
                                                
    // Feature Control Register 4 Sleep Settings    
    kPangeaFCR4SleepBitsSet			=	0,
    kPangeaFCR4SleepBitsClear		=	0
};

// desired state of Intrepid FCR registers when sleeping
enum {
	// Feature Control Register 0 Sleep Settings for Intrepid
    kIntrepidFCR0SleepBitsSet		=	0,
	
    kIntrepidFCR0SleepBitsClear		=	kKeyLargoFCR0SccCellEnable |
										kKeyLargoFCR0SccBEnable |
										kKeyLargoFCR0SccAEnable,

    // Feature Control Register 1 Sleep Settings
    kIntrepidFCR1SleepBitsSet		=	0,
	
    kIntrepidFCR1SleepBitsClear		=	kKeyLargoFCR1I2S1Enable |
										kKeyLargoFCR1I2S1ClkEnable |
										kKeyLargoFCR1I2S1CellEnable |
										kKeyLargoFCR1I2S0Enable |
										kKeyLargoFCR1I2S0ClkEnable |
										kKeyLargoFCR1I2S0CellEnable |
										kKeyLargoFCR1EIDE0Enable,
                                                
    // Feature Control Register 2 Sleep Settings
    kIntrepidFCR2SleepBitsSet		=	0,
	
    kIntrepidFCR2SleepBitsClear		=	0,

    // Feature Control Register 3 Sleep and Restart Settings
	// Intrepid adds a third USB bus so we set up USB2 sleep bits like for USB0 and USB1 in KeyLargo FCR3
	kIntrepidFCR3USB2SleepBitsSet		= (kIntrepidFCR3Port5DisconnectSelect |
											kIntrepidFCR3Port5ConnectSelect |
											kIntrepidFCR3Port5ResumeSelect |
											kIntrepidFCR3Port5Enable |
											kIntrepidFCR3Port6DisconnectSelect |
											kIntrepidFCR3Port6ConnectSelect |
											kIntrepidFCR3Port6ResumeSelect |
											kIntrepidFCR3Port6Enable),

	kIntrepidFCR3USB2SleepBitsClear		= 0,

	// [3321432] Don't shut down PLLs.  Intrepid will do this automatically at sleep
    kIntrepidFCR3SleepBitsSet		=	0,
										
    kIntrepidFCR3SleepBitsClear		=	//kIntrepidFCR2SleepBitsClear |
										kKeyLargoFCR3ViaClk16Enable |
										kKeyLargoFCR3I2S0Clk18Enable |
										kKeyLargoFCR3I2S1Clk18Enable |
										kKeyLargoFCR3TimerClk18Enable |
										kKeyLargoFCR3Clk45Enable |
										kKeyLargoFCR3Clk49Enable,	

    // do not turn off SPI interface when restarting
	
    kIntrepidFCR3RestartBitsSet		=	kIntrepidFCR3ShutdownPLL3 |
										kIntrepidFCR3ShutdownPLL2 |
										kIntrepidFCR3ShutdownPLL1,
										
    kIntrepidFCR3RestartBitsClear	=	kKeyLargoFCR3I2S0Clk18Enable |
										kKeyLargoFCR3I2S1Clk18Enable |
										kKeyLargoFCR3Clk45Enable |
										kKeyLargoFCR3Clk49Enable,
                                                
    // Feature Control Register 4 Sleep Settings    
    kIntrepidFCR4SleepBitsSet		=	0,
	kIntrepidFCR4SleepBitsClear		=	0
};

// As far as I can tell the clock stop status registers are Intrepid only.  The clock
// stop registers live in the UniN address space and can be read through the UniN driver
// via readIntrepidClockStopStatus
enum {
	// ClockStopStatus 0 bits
	kIntrepidIsStopped49			= (1 << (31 - 31)),
	kIntrepidIsStopped45			= (1 << (31 - 30)),
	kIntrepidIsStopped32			= (1 << (31 - 29)),
	kIntrepidIsStoppedUSB2			= (1 << (31 - 28)),
	kIntrepidIsStoppedUSB1			= (1 << (31 - 27)),
	kIntrepidIsStoppedUSB0			= (1 << (31 - 26)),
	kIntrepidIsStoppedVEO1			= (1 << (31 - 25)),
	kIntrepidIsStoppedVEO0			= (1 << (31 - 24)),
	kIntrepidIsStoppedPCI_FB_CLK_OUT = (1 << (31 - 23)),
	kIntrepidIsStoppedSlot2			= (1 << (31 - 22)),
	kIntrepidIsStoppedSlot1			= (1 << (31 - 21)),
	kIntrepidIsStoppedSlot0			= (1 << (31 - 20)),
	kIntrepidIsStoppedVIA32			= (1 << (31 - 19)),
	kIntrepidIsStoppedSCC_RTClk32or45 = (1 << (31 - 18)),
	kIntrepidIsStoppedSCC_RTClk18	= (1 << (31 - 17)),
	kIntrepidIsStoppedTimer			= (1 << (31 - 16)),
	kIntrepidIsStoppedI2S1_18		= (1 << (31 - 15)),
	kIntrepidIsStoppedI2S1_45or49	= (1 << (31 - 14)),
	kIntrepidIsStoppedI2S0_18		= (1 << (31 - 13)),
	kIntrepidIsStoppedI2S0_45or49	= (1 << (31 - 12)),
	kIntrepidIsStoppedAGPDel		= (1 << (31 - 11)),
	kIntrepidIsStoppedExtAGP		= (1 << (31 - 10)),
	
	// ClockStopStatus 1 bits
	kIntrepidIsStopped18			= (1 << (31 - 31)),
	kIntrepidIsStoppedPCI0			= (1 << (31 - 30)),
	kIntrepidIsStoppedAGP			= (1 << (31 - 29)),
	kIntrepidIsStopped7PCI1			= (1 << (31 - 28)),
	kIntrepidIsStoppedUSB2PCI		= (1 << (31 - 26)),
	kIntrepidIsStoppedUSB1PCI		= (1 << (31 - 25)),
	kIntrepidIsStoppedUSB0PCI		= (1 << (31 - 24)),
	kIntrepidIsStoppedKLPCI			= (1 << (31 - 23)),
	kIntrepidIsStoppedPCI1			= (1 << (31 - 22)),
	kIntrepidIsStoppedMAX			= (1 << (31 - 21)),
	kIntrepidIsStoppedATA100		= (1 << (31 - 20)),
	kIntrepidIsStoppedATA66			= (1 << (31 - 19)),
	kIntrepidIsStoppedGB			= (1 << (31 - 18)),
	kIntrepidIsStoppedFW			= (1 << (31 - 17)),
	kIntrepidIsStoppedPCI2			= (1 << (31 - 16)),
	kIntrepidIsStoppedBUF_REF_CLK_OUT = (1 << (31 - 15)),
	kIntrepidIsStoppedCPU			= (1 << (31 - 14)),
	kIntrepidIsStoppedCPUDel		= (1 << (31 - 13)),
	kIntrepidIsStoppedPLL4Ref		= (1 << (31 - 12))
};

	// Offsets for the registers we wish to save.
	// These come (almost) unchanged from the MacOS9 Power
	// Manager plug-in (p99powerplugin.h)

	// MPIC offsets and registers
	enum {
		kKeyLargoMPICVectorsCount			= 64,
		kKeyLargoMPICIPICount				= 4,
		kKeyLargoMPICTaskPriorityCount		= 4,
		kKeyLargoMPICTimerCount				= 4,
		kKeyLargoMPICBaseOffset				= 0x40000,						// MPIC base offset from start of KeyLargo
	
        kKeyLargoMPICGlobal0				= 0x1020,						// MPIC global0 register
		kKeyLargoMPICIPI0					= 0x10A0,						// MPIC IPI0 vector/priority register
		kKeyLargoMPICIPI1					= 0x10B0,						// MPIC IPI1 vector/priority register
		kKeyLargoMPICIPI2					= 0x10C0,						// MPIC IPI2 vector/priority register
		kKeyLargoMPICIPI3					= 0x10D0,						// MPIC IPI3 vector/priority register
	
		kKeyLargoMPICSpuriousVector			= 0x10E0,						// MPIC spurious vector register
		kKeyLargoMPICTimeFreq				= 0x10F0,						// MPIC timer frequency reporting register
	
		kKeyLargoMPICTimerBase0				= 0x1110,						// MPIC timer 0 base count register
		kKeyLargoMPICTimerBase1				= 0x1150,						// MPIC timer 1 base count register
		kKeyLargoMPICTimerBase2				= 0x1190,						// MPIC timer 2 base count register
		kKeyLargoMPICTimerBase3				= 0x11D0,						// MPIC timer 3 base count register
	
		kKeyLargoMPICIntSrcSize				= 0x20,
		kKeyLargoMPICIntSrcVectPriBase		= 0x10000,						// MPIC interrupt source vector/priority base offset
		kKeyLargoMPICIntSrcDestBase			= 0x10010,						// MPIC interrupt source destination register base offset
	
		kKeyLargoMPICP0CurrTaskPriority		= 0x20080,						// MPIC CPU 0 current task priority register
		kKeyLargoMPICP1CurrTaskPriority		= 0x21080,						// MPIC CPU 1 current task priority register
		kKeyLargoMPICP2CurrTaskPriority		= 0x22080,						// MPIC CPU 2 current task priority register
		kKeyLargoMPICP3CurrTaskPriority		= 0x23080						// MPIC CPU 3 current task priority register
	};

	// 6522 VIA1 (and VIA2) register offsets
	enum {
		kKeyLargoVIABaseOffset	=	0x16000,								// VIA base offset from start of KeyLargo
		kKeyLargovBufB			=	0,                                      // BUFFER B
		kKeyLargovBufAH			=	0x200,                                  // buffer a (with handshake) [ Dont use! ]
		kKeyLargovDIRB			=	0x400,                                  // DIRECTION B
		kKeyLargovDIRA			=	0x600,                                  // DIRECTION A
		kKeyLargovT1C			=	0x800,                                  // TIMER 1 COUNTER (L.O.)
		kKeyLargovT1CH			=	0xA00,                                  // timer 1 counter (high order)
		kKeyLargovT1L			=	0xC00,                                  // TIMER 1 LATCH (L.O.)
		kKeyLargovT1LH			=	0xE00,                                  // timer 1 latch (high order)
		kKeyLargovT2C			=	0x1000,                                 // TIMER 2 LATCH (L.O.)
		kKeyLargovT2CH			=	0x1200,                                 // timer 2 counter (high order)
		kKeyLargovSR			=	0x1400,                                 // SHIFT REGISTER
		kKeyLargovACR			=	0x1600,                                 // AUX. CONTROL REG.
		kKeyLargovPCR			=	0x1800,                                 // PERIPH. CONTROL REG.
		kKeyLargovIFR			=	0x1A00,                                 // INT. FLAG REG.
		kKeyLargovIER			=	0x1C00,                                 // INT. ENABLE REG.
		kKeyLargovBufA			=	0x1E00,                                 // BUFFER A
		kKeyLargovBufD			=	kKeyLargovBufA                          // disk head select is buffer A
	};
	
	// DBDMA offsets
	enum {
		kKeyLargoDBDMAChannelCount		= 13,								// 13 DBDMA channels
		kKeyLargoDBDMABaseOffset		= 0x8000,							// Base address of DBDMA channel registers
		kKeyLargoDBDMAChannelStride		= 0x100								// Stride between successive DBDMA channels
	};

	// Audio offsets
	enum {
		kKeyLargoAudioRegisterCount		= 25,								// 25 audio registers
		kKeyLargoAudioBaseOffset		= 0x14000,							// Base address of audio channel registers
		kKeyLargoAudioRegisterStride	= 0x10								// Stride between successive audio register
	};

	// I2S offsets
	enum {
		kKeyLargoI2SRegisterCount		= 10,								// 10 I2S registers (per channel)
		kKeyLargoI2SChannelCount		= 2,								// 2 channels
		kKeyLargoI2S0BaseOffset			= 0x10000,							// Base address of I2S0 registers
		kKeyLargoI2S1BaseOffset			= 0x11000,							// Base address of I2S1 registers
		kKeyLargoI2SRegisterStride		= 0x10								// Stride between successive I2S register
	};
	
	// GPIO offsets
	enum {
		kKeyLargoGPIOLevels0			= 0x50,								// Offset to GPIO Levels 0 register
		kKeyLargoGPIOLevels1			= 0x54,								// Offset to GPIO Levels 1 register
		// There is an inconsistency here - see usage of kKeyLargoExtIntGPIOBase
		kKeyLargoExtIntGPIOBase			= 0x50,								// Ext-int GPIO base offset
		kKeyLargoExtIntGPIORegBase		= 0x58,								// Ext-int GPIO register base offset
		kKeyLargoExtIntGPIOCount		= 18,								// Ext-int GPIO count
		kKeyLargoGPIOBase				= 0x6A,								// GPIO base offset
		kKeyLargoGPIOCount				= 17,								// GPIO count
		kKeyLargoGPIOOutputEnable		= 0x04,								// DDIR
		kKeyLargoGPIOData				= 0x01,								// GPIO data
  
	};

// Much needed forward declaration:
class USBKeyLargo;
class KeyLargoWatchDogTimer;

class KeyLargo : public AppleMacIO
{
	OSDeclareDefaultStructors(KeyLargo);
  
protected:
	IOLogicalAddress	keyLargoBaseAddress;
	UInt32				keyLargoVersion;
	UInt32				keyLargoDeviceId;
	IOService *			keyLargoService;
 
	virtual	void		AdjustBusSpeeds ( void );
	virtual	void 		saveVIAState(UInt8* savedK2ViaState);
	virtual	void 		restoreVIAState(UInt8* savedK2ViaState);

	KeyLargoWatchDogTimer	*watchDogTimer;


	// this is to ensure mutual exclusive access to
	// the keylargo registers:
	IOSimpleLock *mutex;

	// Remember the bus speed:
	long long busSpeed;
  
	// true for PowerBooks/iBooks
	bool hostIsMobile;

    // Number of USB cells in the chip
	// the two USB busses - Intrepid has three:
	enum {
		kMaxNumUSB 			= 3,
	};
	
	UInt32 fNumUSB;
	USBKeyLargo *usbBus[kMaxNumUSB];
    UInt8 fBaseUSBID;
    
    bool fHasSoftModem;
    
public:
	virtual bool      start(IOService *provider);
					  
	virtual long long syncTimeBase(void);
	virtual void	  recalibrateBusSpeeds(void);

	virtual UInt8     readRegUInt8(unsigned long offset);
	virtual void      writeRegUInt8(unsigned long offset, UInt8 data);
	virtual UInt32    readRegUInt32(unsigned long offset);
	virtual void      writeRegUInt32(unsigned long offset, UInt32 data);
  
	// share register access:
	virtual void safeWriteRegUInt8(unsigned long offset, UInt8 mask, UInt8 data);
	virtual UInt8 safeReadRegUInt8(unsigned long offset);
    
    // Pure virtual so derived classes can save their FCRs
	virtual void safeWriteRegUInt32(unsigned long offset, UInt32 mask, UInt32 data) = 0;
    
	virtual UInt32 safeReadRegUInt32(unsigned long offset);
    
    virtual bool publishChildren(IOService * driver, IOService *(*createChildNub)(IORegistryEntry *) = 0);
    virtual bool publishChild(IOService * driver, IORegistryEntry * child,
                                                IOService *(*createChildNub)(IORegistryEntry *) = 0);

};

#endif /* ! _IOKIT_KEYLARGO_H */
