/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef __IOAC97TYPES_H
#define __IOAC97TYPES_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOMessage.h>

#define kIOAC97HardwareNameKey    "Hardware Name"

typedef UInt32 IOAC97DMAEngineID;
typedef UInt32 IOAC97CodecID;
typedef UInt32 IOAC97CodecOffset;
typedef UInt16 IOAC97CodecWord;
typedef UInt32 IOAC97CodecConverter;
typedef UInt32 IOAC97AnalogOutput;
typedef UInt32 IOAC97AnalogSource;
typedef UInt32 IOAC97AnalogChannel;
typedef UInt32 IOAC97RecordSource;
typedef SInt32 IOAC97VolumeValue;
typedef SInt32 IOAC97GainValue;

#ifndef BIT(b)
#define BIT(b)  (1 << (b))
#endif

// For build on pre-Tiger systems
#ifndef iokit_vendor_specific_msg(message)
#define iokit_vendor_specific_msg(message) \
        (UInt32)(sys_iokit|err_sub(-2)|message)
#endif

#define kIOAC97MessagePrepareAudioConfiguration \
        iokit_vendor_specific_msg(1)

#define kIOAC97MessageActivateAudioConfiguration \
        iokit_vendor_specific_msg(2)

#define kIOAC97MessageDeactivateAudioConfiguration \
        iokit_vendor_specific_msg(3)

#define kIOAC97MessageCreateAudioControls \
        iokit_vendor_specific_msg(4)

struct IOAC97MessageArgument
{
    void * param[4];
};

enum {
    kIOAC97MaxCodecCount = 4
};

enum {
    kIOAC97SampleFormatPCM16 = 1,
    kIOAC97SampleFormatPCM18,
    kIOAC97SampleFormatPCM20,
    kIOAC97SampleFormatAC3
};

enum {
    kIOAC97DMAEngineTypeAudioPCM   = 1,
    kIOAC97DMAEngineTypeAudioSPDIF = 2
};

enum {
    kIOAC97DMADataDirectionOutput  = 1,  /* from AC97 controller to codec */
    kIOAC97DMADataDirectionInput   = 2   /* from codec to AC97 controller */
};

enum {
    kIOAC97Slot_0     = (1 << 0),
    kIOAC97Slot_1     = (1 << 1),
    kIOAC97Slot_2     = (1 << 2),
    kIOAC97Slot_3     = (1 << 3),
    kIOAC97Slot_4     = (1 << 4),
    kIOAC97Slot_5     = (1 << 5),
    kIOAC97Slot_6     = (1 << 6),
    kIOAC97Slot_7     = (1 << 7),
    kIOAC97Slot_8     = (1 << 8),
    kIOAC97Slot_9     = (1 << 9),
    kIOAC97Slot_10    = (1 << 10),
    kIOAC97Slot_11    = (1 << 11),
    kIOAC97Slot_12    = (1 << 12),
    kIOAC97Slot_3_4   = (kIOAC97Slot_3  | kIOAC97Slot_4),
    kIOAC97Slot_7_8   = (kIOAC97Slot_7  | kIOAC97Slot_8),
    kIOAC97Slot_6_9   = (kIOAC97Slot_6  | kIOAC97Slot_9),
    kIOAC97Slot_10_11 = (kIOAC97Slot_10 | kIOAC97Slot_11)
};

enum {
    kIOAC97SampleRate8K    =  8000,
    kIOAC97SampleRate32K   = 32000,
    kIOAC97SampleRate44_1K = 44100,
    kIOAC97SampleRate48K   = 48000
};

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
    kCodecInterruptAndPage      = 0x24,
    kCodecPowerdown             = 0x26,
    kCodecExtAudioID            = 0x28,
    kCodecExtAudioStatus        = 0x2a,
    kCodecPCMFrontDACRate       = 0x2c,
    kCodecPCMSurroundDACRate    = 0x2e,
    kCodecPCMLFEDACRate         = 0x30,
    kCodecPCMADCRate            = 0x32,
    kCodecMicADCRate            = 0x34,
    kCodecCenterLFEVolume       = 0x36,
    kCodecSurroundVolume        = 0x38,
    kCodecSPDIFControl          = 0x3a,
    kCodecVendorID1             = 0x7c,
    kCodecVendorID2             = 0x7e,
    kCodecRegisterCount         = 0x80,

    /* Page 1 Extended Registers */

    kCodecSenseFunctionSelect   = 0x66,
    kCodecSenseInformation      = 0x68,
    kCodecSenseDetails          = 0x6a,
    kCodecDACSlotMapping        = 0x6c,
    kCodecADCSlotMapping        = 0x6e
};

enum {
    kCodecRegisterPage0 = 0,  /* vendor specific */
    kCodecRegisterPage1 = 1   /* AC97 defined    */
};

/*
 * Codec Register Masks.
 */
#define kAudioReset_MICPCMIN        0x0001
#define kAudioReset_TONE            0x0004
#define kAudioReset_STEREO          0x0008
#define kAudioReset_HPOUT           0x0010
#define kAudioReset_BASSBOOST       0x0020
#define kAudioReset_DAC18B          0x0040
#define kAudioReset_DAC20B          0x0080
#define kAudioReset_ADC18B          0x0100
#define kAudioReset_ADC20B          0x0200

#define kPowerdown_ADC              0x0001
#define kPowerdown_DAC              0x0002
#define kPowerdown_ANL              0x0004
#define kPowerdown_REF              0x0008
#define kPowerdown_PR0              0x0100
#define kPowerdown_PR1              0x0200
#define kPowerdown_PR2              0x0400
#define kPowerdown_PR3              0x0800
#define kPowerdown_PR4              0x1000
#define kPowerdown_PR5              0x2000
#define kPowerdown_PR6              0x4000
#define kPowerdown_EAPD             0x8000
#define kPowerdown_AllFunctions     0xFF00

#define kExtAudioID_VRA             BIT(0)
#define kExtAudioID_DRA             BIT(1)
#define kExtAudioID_SPDIF           BIT(2)
#define kExtAudioID_VRM             BIT(3)
#define kExtAudioID_DSA_MASK        0x0030
#define kExtAudioID_CDAC            BIT(6)
#define kExtAudioID_SDAC            BIT(7)
#define kExtAudioID_LDAC            BIT(8)
#define kExtAudioID_AMAP            BIT(9)
#define kExtAudioID_REV_MASK        0x0C00
#define kExtAudioID_REV21           0x0000
#define kExtAudioID_REV22           0x0400
#define kExtAudioID_REV23           0x0800
#define kExtAudioID_ID_MASK         0xC000

#define kExtAudioStatus_VRA         BIT(0)
#define kExtAudioStatus_DRA         BIT(1)
#define kExtAudioStatus_SPDIF       BIT(2)
#define kExtAudioStatus_VRM         BIT(3)
#define kExtAudioStatus_SPSA_MASK   0x0030
#define kExtAudioStatus_SPSA_3_4    0x0000
#define kExtAudioStatus_SPSA_7_8    0x0010
#define kExtAudioStatus_SPSA_6_9    0x0020
#define kExtAudioStatus_SPSA_10_11  0x0030
#define kExtAudioStatus_CDAC        BIT(6)
#define kExtAudioStatus_SDAC        BIT(7)
#define kExtAudioStatus_LDAC        BIT(8)
#define kExtAudioStatus_MADC        BIT(9)
#define kExtAudioStatus_SPCV        BIT(10)
#define kExtAudioStatus_PRI         BIT(11)
#define kExtAudioStatus_PRJ         BIT(12)
#define kExtAudioStatus_PRK         BIT(13)
#define kExtAudioStatus_PRL         BIT(14)
#define kExtAudioStatus_VCFG        BIT(15)

#define kSPDIFControl_V             BIT(15)
#define kSPDIFControl_DRS           BIT(14)
#define kSPDIFControl_SPSR_MASK     0x3000
#define kSPDIFControl_SPSR_44K      0x0000
#define kSPDIFControl_SPSR_48K      0x2000
#define kSPDIFControl_SPSR_32K      0x3000
#define kSPDIFControl_L             BIT(11)
#define kSPDIFControl_CC_MASK       0x07F0
#define kSPDIFControl_CC_SHIFT      4
#define kSPDIFControl_PRE           BIT(3)
#define kSPDIFControl_COPY          BIT(3)
#define kSPDIFControl_NON_AUDIO     BIT(1)
#define kSPDIFControl_PRO           BIT(0)

#define kSenseInformation_IV        BIT(4)
#define kSenseInformation_FIP       BIT(0)

#define kSenseDetail_ST_MASK        0xE000
#define kSenseDetail_S_MASK         0x1F00
#define kSenseDetail_S_INVALID      0x0000
#define kSenseDetail_S_NO_DEVICE    0x0100
#define kSenseDetail_S_FINGERPRINT  0x0200
#define kSenseDetail_S_SPEAKER_8    0x0300
#define kSenseDetail_S_SPEAKER_4    0x0400
#define kSenseDetail_S_SPEAKER_PWR  0x0500
#define kSenseDetail_S_HEADPHONE    0x0600
#define kSenseDetail_S_SPDIF        0x0700
#define kSenseDetail_S_SPDIF_TOS    0x0800
#define kSenseDetail_S_HEADSET      0x0900
#define kSenseDetail_S_OTHER        0x0A00
#define kSenseDetail_OR_MASK        0x00C0
#define kSenseDetail_OR_SHIFT       6
#define kSenseDetail_SR_MASK        0x003F

#endif /* !__IOAC97TYPES_H */
