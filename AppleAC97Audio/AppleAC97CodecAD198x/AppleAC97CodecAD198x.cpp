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

/*
 * Analog Devices AD1985 AC97 Audio Codec Driver.
 *
 * The purpose of this driver is to program the vendor specific
 * registers in order to:
 *
 * 1. Swap Front outputs with the Surround outputs. The D865PERL is an
 *    example of a board that is cross-wired.  Driver must intevene to
 *    get the described back panel audio jack assignments. Driver uses
 *    jack sensing to detect speaker connection at load time.
 *
 * 2. Enable 2-to-4 channel spreading for 2-channel output modes when
 *    jack sense did not reveal the type of output device connected.
 *
 * 3. Make sure some of the unused vendor defined features are indeed
 *    off following a hardware reset.
 *
 * 4. Serve as a sample AC97 audio codec driver.
 */

#include <IOKit/IOLib.h>
#include "AppleAC97CodecAD198x.h"
#include "IOAC97Debug.h"

#define CLASS AppleAC97CodecAD198x
#define super IOAC97AudioCodec

OSDefineMetaClassAndStructors( AppleAC97CodecAD198x, IOAC97AudioCodec )

//---------------------------------------------------------------------------

bool CLASS::probeHardwareFeatures( void )
{
    IOAC97CodecWord senseInfo;
    IOAC97CodecWord senseDetail;
    UInt32          senseResult;
    bool            speakerOnDAC1 = false;
    bool            speakerOnDAC2 = false;
    IOReturn        ret;

    if (!super::probeHardwareFeatures())
        return false;

    fSwapFrontSurroundChannels = false;
    fChannelSpreadingEnabled   = false;
    fChannelSpreadingActivated = false;

    // Sense Master output.

    ret = performJackSense(kSenseFunctionDAC1, kSenseFromTip, 
                           &senseInfo, &senseDetail, &senseResult);
    if (ret == kIOReturnSuccess)
    {
        IOAC97CodecWord device = (senseDetail & kSenseDetail_S_MASK);
        if ((device == kSenseDetail_S_SPEAKER_PWR) &&
            (senseResult < kSpeakerSenseResultThreshold))
            speakerOnDAC1 = true;

        DebugLog("%s: DAC1 sense %d, device %x value %u\n", getName(),
                 speakerOnDAC1, device, senseResult);
    }

    // Sense Surround output.

    ret = performJackSense(kSenseFunctionDAC2, kSenseFromTip, 
                           &senseInfo, &senseDetail, &senseResult);
    if (ret == kIOReturnSuccess)
    {
        IOAC97CodecWord device = (senseDetail & kSenseDetail_S_MASK);
        if ((device == kSenseDetail_S_SPEAKER_PWR) &&
            (senseResult < kSpeakerSenseResultThreshold))
            speakerOnDAC2 = true;

        DebugLog("%s: DAC2 sense %d, device %x value %u\n", getName(),
                 speakerOnDAC2, device, senseResult);
    }

    // Speaker connected to Surround output.

    if (!speakerOnDAC1 && speakerOnDAC2)
    {
        fSwapFrontSurroundChannels = true;
        IOLog("%s: Front/Surround channel swap enabled\n", getName());
    }
    
    // No speakers detected on either Master or Surround outputs.

    if (!speakerOnDAC1 && !speakerOnDAC2)
    {
        fChannelSpreadingEnabled = true;
        IOLog("%s: 2-to-4 channel spreading enabled\n", getName());
    }

    return true;
}

//---------------------------------------------------------------------------

bool CLASS::primeHardware( void )
{
    if (!super::primeHardware())
        return false;

    codecWrite( kReg72_JackSenseStatus, 0x0000 );

    // Enable Sample Rate Unlock, Mute Split, and channel swapping.

    fReg76 = kReg76_SRU | kReg76_MSPLT;
    if (fSwapFrontSurroundChannels)
        fReg76 |= (kReg76_HPSEL | kReg76_LOSEL);
    codecWrite( kReg76_MiscControlBits, fReg76 );

    DebugLog("AD1985 0x70 = %04x\n", codecRead(kReg70_JackSenseGeneral));
    DebugLog("AD1985 0x72 = %04x\n", codecRead(kReg72_JackSenseStatus));
    DebugLog("AD1985 0x74 = %04x\n", codecRead(kReg72_SerialConfiguration));
    DebugLog("AD1985 0x76 = %04x\n", codecRead(kReg76_MiscControlBits));
    DebugLog("AD1985 0x78 = %04x\n", codecRead(kReg78_AdvancedJackSense));

    return true;
}

//---------------------------------------------------------------------------

IOReturn CLASS::activateAudioConfiguration( IOAC97AudioConfig * config )
{
    IOReturn ior;

    ior = super::activateAudioConfiguration(config);
    if (ior != kIOReturnSuccess)
        return ior;

    if (config->getCodecSlotMap(fCodecID) == 0)
        return kIOReturnSuccess;

    // Enable channel spreading for 2-channel PCM output.

    if (fChannelSpreadingEnabled &&
        (config->getAudioChannelCount() == 2) &&
        (config->getDMAEngineType()     == kIOAC97DMAEngineTypeAudioPCM) &&
        (config->getDMADataDirection()  == kIOAC97DMADataDirectionOutput))
    {
        // Enable 2-to-4 channel speading
        
        IOAC97CodecWord reg76 = fReg76;
        reg76 &= ~(kReg76_HPSEL | kReg76_LOSEL);
        reg76 |= kReg76_HPSEL;
        codecWrite(kReg76_MiscControlBits, reg76);
        fChannelSpreadingActivated = true;
        DebugLog("AD1985 2-to-4 spreading activated\n");
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void CLASS::deactivateAudioConfiguration( IOAC97AudioConfig * config )
{
    super::deactivateAudioConfiguration( config );

    if (fChannelSpreadingActivated &&
        (config->getAudioChannelCount() == 2) &&
        (config->getDMAEngineType()     == kIOAC97DMAEngineTypeAudioPCM) &&
        (config->getDMADataDirection()  == kIOAC97DMADataDirectionOutput))
    {
        // Disable 2-to-4 channel speading

        codecWrite(kReg76_MiscControlBits, fReg76);
        setAnalogOutputMute(kAnalogOutputLineOut, kAnalogChannelStereo, true);
        fChannelSpreadingActivated = false;
        DebugLog("AD1985 2-to-4 spreading deactivated\n");
    }
}

//---------------------------------------------------------------------------

IOReturn CLASS::setAnalogOutputVolume( IOAC97AnalogOutput  output,
                                       IOAC97AnalogChannel channel,
                                       IOAC97VolumeValue   volume )
{
    // Update both Front and Surround volume control when spreading.

    if (fChannelSpreadingActivated && (output == kAnalogOutputLineOut))
    {
        super::setAnalogOutputVolume(kAnalogOutputLineOut,  channel, volume);
        super::setAnalogOutputVolume(kAnalogOutputSurround, channel, volume);
        return kIOReturnSuccess;
    }

    // Re-map the volume control register if the front/surround
    // amplifier outputs are swapped.

    if (fSwapFrontSurroundChannels)
    {
        if (output == kAnalogOutputLineOut)
            output = kAnalogOutputSurround;
        else if (output == kAnalogOutputSurround)
            output = kAnalogOutputLineOut;        
    }

    return super::setAnalogOutputVolume(output, channel, volume);
}

IOReturn CLASS::setAnalogOutputMute( IOAC97AnalogOutput  output,
                                     IOAC97AnalogChannel channel,
                                     bool                isMute )
{
    // Update both Front and Surround mute control when spreading.

    if (fChannelSpreadingActivated && (output == kAnalogOutputLineOut))
    {
        super::setAnalogOutputMute(kAnalogOutputLineOut,  channel, isMute);
        super::setAnalogOutputMute(kAnalogOutputSurround, channel, isMute);
        return kIOReturnSuccess;
    }

    // Re-map the mute control register if the front/surround
    // amplifier outputs are swapped.

    if (fSwapFrontSurroundChannels)
    {
        if (output == kAnalogOutputLineOut)
            output = kAnalogOutputSurround;
        else if (output == kAnalogOutputSurround)
            output = kAnalogOutputLineOut;        
    }

    return super::setAnalogOutputMute(output, channel, isMute);
}

//---------------------------------------------------------------------------
// Support for independent left and right channel mute bit.

#define kMuteMaskRight  (1 << 7)
#define kMuteMaskLeft   (1 << 15)

IOAC97CodecWord CLASS::getMuteMaskForChannel( IOAC97AnalogChannel channel )
{
    IOAC97CodecWord mask = 0;

    switch (channel)
    {
        case kAnalogChannelLeft:
            mask = kMuteMaskLeft;
            break;
        case kAnalogChannelRight:
            mask = kMuteMaskRight;
            break;
        case kAnalogChannelStereo:
            mask = kMuteMaskLeft | kMuteMaskRight;
            break;
    }

    return mask;
}

IOAC97CodecWord CLASS::getAnalogOutputMuteBitMask(
                       IOAC97AnalogOutput  output,
                       IOAC97AnalogChannel channel )
{
    if (output == kAnalogOutputLineOut ||
        output == kAnalogOutputAuxOut)
    {
        return getMuteMaskForChannel(channel);
    }

    return super::getAnalogOutputMuteBitMask(output, channel);
}

IOAC97CodecWord CLASS::getAnalogSourceMuteBitMask(
                       IOAC97AnalogSource  source,
                       IOAC97AnalogChannel channel )
{
    if (source == kAnalogSourceLineIn ||
        source == kAnalogSourceCD     ||
        source == kAnalogSourcePCMOut)
    {
        return getMuteMaskForChannel(channel);
    }

    return super::getAnalogSourceMuteBitMask(source, channel);
}

IOAC97CodecWord CLASS::getRecordGainMuteBitMask(
                       IOAC97AnalogChannel analogChannel )
{
    return getMuteMaskForChannel(analogChannel);
}
