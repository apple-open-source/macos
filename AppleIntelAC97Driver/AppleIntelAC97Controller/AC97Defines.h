/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __APPLE_INTEL_AC97_DEFINES_H
#define __APPLE_INTEL_AC97_DEFINES_H

#include <libkern/OSTypes.h>
#include <IOKit/IOTypes.h>

#define BITS(x, v)   ((v) << (x))
#define RO_BIT(x)    BITS(x, 1)  /* read-only  */
#define RW_BIT(x)    RO_BIT(x)   /* read-write */
#define RC_BIT(x)    RO_BIT(x)   /* read-write, write 1 to clear */
#define RS_BIT(x)    RO_BIT(x)   /* read-write, hardware self clear */

/*
 * AC97 codec registers.
 */
enum {
    kCodecAudioReset            = 0x00,
    kCodecMasterVolume          = 0x02,
    kCodecAuxOutVolume          = 0x04,
    kCodecMonoVolume            = 0x06,
    kCodecMasterTone            = 0x08,
    kCodecPCBeepVolume          = 0x0a,
    kCodecPhoneVolume           = 0x0c,
    kCodecMicVolume             = 0x0e,
    kCodecLineInVolume          = 0x10,
    kCodecCDVolume              = 0x12,
    kCodecVideoVolume           = 0x14,
    kCodecAuxInVolume           = 0x16,
    kCodecPCMOutVolume          = 0x18,
    kCodecRecordSelect          = 0x1a,
    kCodecRecordGain            = 0x1c,
    kCodecRecordGainMic         = 0x1e,
    kCodecGeneralPurpose        = 0x20,
    kCodec3DControl             = 0x22,
    kCodecPowerdown             = 0x26,
    kCodecExtAudioID            = 0x28,
    kCodecExtAudioStat          = 0x2a,
    kCodecPCMFrontDACRate       = 0x2c,
    kCodecPCMSurroundDACRate    = 0x2E,
    kCodecPCMLFEDACRate         = 0x30,
    kCodecPCMADCRate            = 0x32,
    kCodecMicADCRate            = 0x34,
    kCodecExtModemID            = 0x3a,
    kCodecVendorID1             = 0x7c,
    kCodecVendorID2             = 0x7e,
    kCodecRegisterCount         = 0x80
};

/*
 * Codec register masks.
 */
enum {
    // Reset register (0x00)
    kDedicatedPCMInChannel      = 0x0001,
    kBassTrebleControl          = 0x0004,
    kSimulatedStereo            = 0x0008,
    kHeadphoneOutSupport        = 0x0010,
    kLoudnessSupport            = 0x0020,
    k18BitDAC                   = 0x0040,
    k20BitDAC                   = 0x0080,
    k18BitADC                   = 0x0100,
    k20BitADC                   = 0x0200,

    // Powerdown register (0x26)
    kADCReady                   = 0x0001,
    kDACReady                   = 0x0002,
    kAnalogReady                = 0x0004,
    kVrefReady                  = 0x0008,

    // Volume control register (all)
    kVolumeMuteBit              = 0x8000,

    // Extended Audio ID (0x28)
    kVariableRatePCMAudio       = 0x0001,
    kDoubleRatePCMAudio         = 0x0002,
};

/*
 * Enumeration of available audio input sources.
 */
enum {
    kInputSourceMic1      = 0x1,
    kInputSourceCD        = 0x2,
    kInputSourceVideo     = 0x3,
    kInputSourceAUX       = 0x4,
    kInputSourceLine      = 0x5,
    kInputSourceStereoMix = 0x6,
    kInputSourceMonoMix   = 0x7,
    kInputSourcePhone     = 0x8,
    kInputSourceMic2      = 0x9
};

/*
 * Intel AC97 controller bus master register offsets
 * in NABMBAR I/O range (64 byte range).
 */
enum {
    kBMBufferDescBaseAddress    = 0x00,
    kBMCurrentIndex             = 0x04,
    kBMLastValidIndex           = 0x05,
    kBMStatus                   = 0x06,
    kBMPositionInBuffer         = 0x08,
    kBMPrefetchedIndex          = 0x0a,
    kBMControl                  = 0x0b,
    kGlobalControl              = 0x2c,
    kGlobalStatus               = 0x30,
    kCodecAccessSemaphore       = 0x34,
    kSDataInMap                 = 0x80,
};

/*
 * x_SR - Status Register.
 *
 * Default Value: 0x0001
 * Size:          16 bits (Word access only)
 */
enum {
    kFIFOError                      = RC_BIT(4),
    kBufferCompletionInterrupt      = RC_BIT(3),
    kLastValidBufferInterrupt       = RC_BIT(2),
    kCurrentEqualsLastValid         = RO_BIT(1),
    kDMAControllerHalted            = RO_BIT(0)
};

/*
 * x_CR - Control Register.
 *
 * Default Value: 0x00
 * Size:          8 bits
 */
enum {
    kInterruptOnCompletionEnable    = RW_BIT(4),
    kFIFOErrorInterruptEnable       = RW_BIT(3),
    kLastValidBufferInterruptEnable = RW_BIT(2),
    kResetRegisters                 = RS_BIT(1),
    kRunBusMaster                   = RW_BIT(0)
};

/* GLOB_CNT - Global Control Register.
 *
 * Default Value: 0x00000000
 * Size:          32 bits (DWord access only)
 */
enum {
    k2ChannelMode                  = BITS(20, 0),
    k4ChannelMode                  = BITS(20, 1),
    k6ChannelMode                  = BITS(20, 2),
    kSecResumeInterruptEnable      = RW_BIT(5),
    kPriResumeInterruptEnable      = RW_BIT(4),
    kACLinkShutOff                 = RW_BIT(3),
    kGlobalWarmReset               = RS_BIT(2),
    kGlobalColdResetDisable        = RW_BIT(1),
    kGPIInterruptEnable            = RW_BIT(0)
};

/* GLOB_STA - Global Status Register.
 *
 * Default Value: 0x00300000
 * Size:          32 bits (DWord access only)
 */
enum {
    k3rdCodecReady                 = RO_BIT(28),
    k6ChannelCapable               = RO_BIT(21),
    k4ChannelCapable               = RO_BIT(20),
    kModemPowerDownFlag            = RW_BIT(17),
    kAudioPowerDownFlag            = RW_BIT(16),
    kCodecReadTimeout              = RC_BIT(15),
    kSlot12Bit3                    = RO_BIT(14),
    kSlot12Bit2                    = RO_BIT(13),
    kSlot12Bit1                    = RO_BIT(12),
    kSecResumeInterrupt            = RC_BIT(11),
    kPriResumeInterrupt            = RC_BIT(10),
    kSecCodecReady                 = RO_BIT(9),
    kPriCodecReady                 = RO_BIT(8),
    kMicInInterrupt                = RO_BIT(7),
    kPCMOutInterrupt               = RO_BIT(6),
    kPCMInInterrupt                = RO_BIT(5),
    kModemOutInterrupt             = RO_BIT(2),
    kModemInInterrupt              = RO_BIT(1),
    kGPIInterrupt                  = RC_BIT(0)
};

/* CAS - Codec Access Semaphore Register.
 *
 * Default Value: 0x00
 * Size:          8 bits
 */
enum {
    kCodecAccessInProgress         = RS_BIT(0)
};

/* SDM - SDATA_IN Map Register (ICH4).
 *
 * Default Value: 0x00
 * Size:          8 bits
 */
enum {
    kSteerEnable                   = RW_BIT(3)
};

/*
 * AC97BD - Intel AC97 Controller Buffer Descriptor.
 */
typedef struct {
    IOPhysicalAddress pointer;  /* DWORD aligned physical address */
    UInt16            length;   /* buffer length in samples */
    UInt16            command;  /* command bits */
} AC97BD;

/*
 * Buffer descriptor commands.
 */
enum {
    kInterruptOnCompletion = 0x8000,
    kBufferUnderrunPolicy  = 0x4000,
};

/*
 * Enumeration of audio DMA channels.
 */
enum {
    kChannelPCMIn  = 0,
    kChannelPCMOut = 1,
    kChannelMICIn  = 2
};

/*
 * Functions for devices attached to the AC-Link.
 */
#define kAudioFunctionKey       "Audio"
#define kModemFunctionKey       "Modem"
#define kControllerFunctionKey  "Controller Function"

extern const OSSymbol * gAC97AudioFunction;
extern const OSSymbol * gAC97ModemFunction;

enum {
    kMaxCodecCount = 4
};

#ifndef RELEASE
#define RELEASE(x) do { if (x) { (x)->release(); (x) = 0; } } while (0)
#endif

typedef UInt32 DMAChannel;
typedef UInt8  CodecID;

#endif /* !__APPLE_INTEL_AC97_DEFINES_H */
