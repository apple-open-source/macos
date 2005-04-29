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

#ifndef __APPLEAC97CODECAD198X
#define __APPLEAC97CODECAD198X

#include "IOAC97AudioCodec.h"

/*
 * Vendor Specific Registers.
 */
enum {
    kReg70_JackSenseGeneral    = 0x70,
    kReg72_JackSenseStatus     = 0x72,
    kReg72_SerialConfiguration = 0x74,
    kReg76_MiscControlBits     = 0x76,
    kReg78_AdvancedJackSense   = 0x78
};

#define kReg70_JS2SEL     BIT(6)       /* select JS2 input behavior */
#define kReg70_MMDIS      BIT(7)       /* mono mute disable */

#define kReg72_JS0INT     BIT(0)
#define kReg72_JS1INT     BIT(1)
#define kReg72_JS0ST      BIT(2)
#define kReg72_JS1ST      BIT(3)
#define kReg72_JS0MD      BIT(4)
#define kReg72_JS1MD      BIT(5)
#define kReg72_JS0TMR     BIT(6)
#define kReg72_JS1TMR     BIT(7)
#define kReg72_JS0EQB     BIT(8)
#define kReg72_JS1EQB     BIT(9)
#define kReg72_JSMT_SHIFT 10
#define kReg72_JSMT_MASK  0x1C00
#define kReg72_JS0DMX     BIT(13)      /* JS0 down-mix enable */
#define kReg72_JS1DMX     BIT(14)      /* JS1 down-mix enable */
#define kReg72_JSSPRD     BIT(15)      /* JS spread control enable */ 

#define kReg74_SPLINK     BIT(0)       /* link SPDIF and front DACs */
#define kReg74_SPDZ       BIT(1)       /* SPDIF underrun behavior */
#define kReg74_SPAL       BIT(2)       /* SPDIF ADC loopback */
#define kReg74_CSWP       BIT(3)       /* swap Center/LFE channels */
#define kReg74_INTS       BIT(4)       /* interrupt path select */
#define kReg74_LBKS_MASK  BITS(5, 3)
#define kReg74_LBKS_FRONT BITS(5, 0)
#define kReg74_LBKS_SURR  BITS(5, 1)
#define kReg74_LBKS_CLFE  BITS(5, 3)
#define kReg74_SPOVR      BIT(7)       /* SPDIF override */
#define kReg74_CHEN       BIT(8)       /* slave codec chain enable */
#define kReg74_OMS        BIT(9)       /* output microphone select */
#define kReg74_DRF        BIT(10)      /* DAC request force */
#define kReg74_REGM3      BIT(11)
#define kReg74_REGM0      BIT(12)
#define kReg74_REGM1      BIT(13)
#define kReg74_REGM2      BIT(14)
#define kReg74_SLOT16     BIT(15)

#define kReg76_MBG_MASK   BITS(0, 3)   /* MIC boost gain select */
#define kReg76_MBG_20DB   BITS(0, 0)
#define kReg76_MBG_10DB   BITS(0, 1)
#define kReg76_MBG_30DB   BITS(0, 2)
#define kReg76_VREF_MASK  BITS(2, 3)
#define kReg76_VREF_2_25V BITS(2, 0)  
#define kReg76_VREF_HIZ   BITS(2, 1)
#define kReg76_VREF_3_7V  BITS(2, 2)
#define kReg76_VREF_0V    BITS(2, 3)
#define kReg76_SRU        BIT(4)       /* sample rate unlock */
#define kReg76_LOSEL      BIT(5)       /* LINE OUT amp input select */
#define kReg76_2CMIC      BIT(6)       /* 2-channel MIC select */
#define kReg76_SPRD       BIT(7)       /* 2-to-6 channel spread enable */
#define kReg76_DMIX_MASK  BITS(8, 3)
#define kReg76_DMIX_NONE  BITS(8, 0)
#define kReg76_DMIX_6_4   BITS(8, 2)   /* 6-to-4 channel down-mix */
#define kReg76_DMIX_6_2   BITS(8, 3)   /* 6-to-2 channel down-mix */
#define kReg76_HPSEL      BIT(10)      /* headphone amp input select */
#define kReg76_CLDIS      BIT(11)      /* Center/LFE disable */
#define kReg76_LODIS      BIT(12)      /* LINE OUT disable */
#define kReg76_MSPLT      BIT(13)      /* mute split */
#define kReg76_AC97NC     BIT(14)      /* AC97 no compatibility mode */
#define kReg76_DACZ       BIT(15)      /* DAC underrun behavior */

#define kReg78_JS2INT     BIT(0)
#define kReg78_JS3INT     BIT(1)
#define kReg78_JS2ST      BIT(2)
#define kReg78_JS3ST      BIT(3)
#define kReg78_JS2MD      BIT(4)
#define kReg78_JS3MD      BIT(5)
#define kReg78_JS2TMR     BIT(6)
#define kReg78_JS3TMR     BIT(7)

#define kSpeakerSenseResultThreshold   5000


class AppleAC97CodecAD198x : public IOAC97AudioCodec
{
    OSDeclareDefaultStructors( AppleAC97CodecAD198x )

protected:
    bool                    fSwapFrontSurroundChannels;
    bool                    fChannelSpreadingEnabled;
    bool                    fChannelSpreadingActivated;
    IOAC97CodecWord         fReg76;

    IOAC97CodecWord         getMuteMaskForChannel(
                                     IOAC97AnalogChannel channel );

    virtual bool            probeHardwareFeatures( void );

    virtual bool            primeHardware( void );

public:
    virtual IOReturn        activateAudioConfiguration(
                                     IOAC97AudioConfig * config );

    virtual void            deactivateAudioConfiguration(
                                     IOAC97AudioConfig * config );

    virtual IOReturn        setAnalogOutputVolume(
                                     IOAC97AnalogOutput  output,
                                     IOAC97AnalogChannel channel,
                                     IOAC97VolumeValue   volume );

    virtual IOReturn        setAnalogOutputMute(
                                     IOAC97AnalogOutput  output,
                                     IOAC97AnalogChannel channel,
                                     bool                isMute );

    virtual IOAC97CodecWord getAnalogOutputMuteBitMask(
                                     IOAC97AnalogOutput  output,
                                     IOAC97AnalogChannel channel );

    virtual IOAC97CodecWord getAnalogSourceMuteBitMask(
                                     IOAC97AnalogSource  source,
                                     IOAC97AnalogChannel channel );

    virtual IOAC97CodecWord getRecordGainMuteBitMask(
                                     IOAC97AnalogChannel analogChannel );
};

#endif /* !__APPLEAC97CODECAD198X */
