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

#include <IOKit/IOLib.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/audio/IOAudioSelectorControl.h>
#include "IOAC97AudioCodec.h"
#include "IOAC97Controller.h"
#include "IOAC97Debug.h"

#define CLASS IOAC97AudioCodec
#define super IOService
OSDefineMetaClassAndStructors( IOAC97AudioCodec, IOService )

enum {
    kControlIDMunge = 0x61630000,
    kControlIDMask  = 0x0000FFFF,
    kGlobalMuteControlID = kControlIDMask
};

#define EncodeControlID(i)  ((i) | kControlIDMunge)
#define DecodeControlID(i)  ((i) & kControlIDMask)

//---------------------------------------------------------------------------

bool CLASS::start( IOService * provider )
{
    OSObject * name;
    bool opened = false;

    DebugLog("%s::%s(%p, %p)\n", getName(), __FUNCTION__, this, provider);

    if (!super::start(provider))
        return false;

    fCodecDevice = OSDynamicCast(IOAC97CodecDevice, provider);
    if (!fCodecDevice)
        goto fail;

    fCodecDevice->retain();
    if (fCodecDevice->open(this) != true)
        goto fail;

    opened = true;
    fCodecID = fCodecDevice->getCodecID();

    fCurrentPowerState = kPowerStateActive;

    // Default hardware name is our PNP Vendor ID.

    if ((name = fCodecDevice->getProperty(kIOAC97CodecPNPVendorIDKey)))
    {
        setProperty(kIOAC97HardwareNameKey, name);
    }

    // Reset hardware to default state.

    if (resetHardware() != true)
    {
        DebugLog("%s[ID %u] resetHardware failed\n", getName(), fCodecID);
        goto fail;
    }

    // Probe optional or vendor specific features.

    if (probeHardwareFeatures() != true)
    {
        DebugLog("%s[ID %u] probeHardwareFeatures failed\n",
                 getName(), fCodecID);
        goto fail;
    }

    // Prime hardware for operation. Also called on wake from sleep.

    if (primeHardware() == false)
    {
        DebugLog("%s[ID %u] primeHardware failed\n", getName(), fCodecID);
        goto fail;
    }

    if (opened) provider->close(this);

    registerService();

    return true;

fail:
    if (opened) provider->close(this);
    super::stop(provider);
    return false;
}

//---------------------------------------------------------------------------

void CLASS::free( void )
{
    if (fCodecDevice)
    {
        fCodecDevice->release();
        fCodecDevice = 0;
    }
    super::free();
}

//---------------------------------------------------------------------------

bool CLASS::handleOpen( IOService * client, IOOptionBits options, void * arg )
{
    bool success = false;

    DebugLog("%s[ID %u] handle open from %p\n", getName(), fCodecID, client);

    if ( handleIsOpen( 0 ) == false )
    {
        if ( fCodecDevice->open( this ) )
        {
            success = super::handleOpen( client, options, arg );
            if ( success != true )
                fCodecDevice->close( this );
        }
    }
    return success;
}

//---------------------------------------------------------------------------

void CLASS::handleClose( IOService * client, IOOptionBits options )
{
    DebugLog("%s[ID %u] handle close from %p\n", getName(), fCodecID, client);

    if ( handleIsOpen( client ) )
        fCodecDevice->close( this );

    super::handleClose( client, options );
}

#pragma mark -
#pragma mark еее Hardware Initialization еее
#pragma mark -

//---------------------------------------------------------------------------

bool CLASS::waitForBits( IOAC97CodecOffset offset, IOAC97CodecWord bits )
{
    for (int i = 0; i < 100; i++)
    {
        if ((codecRead(offset) & bits) == bits)
        {
            return true;
        }
        IOSleep(10);
    }

    DebugLog("%s[ID %u] wait for bits (%x, %x) timed out\n",
             getName(), fCodecID, offset, bits);

    return false;
}

//---------------------------------------------------------------------------

bool CLASS::resetHardware( void )
{
    DebugLog("%s[ID %u]::%s\n", getName(), fCodecID, __FUNCTION__);

    // Reset codec audio subsystem.

    codecWrite( kCodecAudioReset, 0 );
    IOSleep(50);

    // The master volume register must be non-zero for audio codecs.

    if (codecRead( kCodecMasterVolume ) == 0)
        return false;

    // Wait for subsection ready flags in the powerdown register.

    waitForBits( kCodecPowerdown, kPowerdown_ADC | kPowerdown_DAC |
                                  kPowerdown_ANL | kPowerdown_REF );

    // Default to extended register page 0.

    codecWrite(kCodecInterruptAndPage, kCodecRegisterPage0);

    return true;
}

//---------------------------------------------------------------------------

bool CLASS::probeHardwareFeatures( void )
{
    // Cache often used codec registers.

    fVendorID = codecRead(kCodecVendorID1);
    fVendorID = (fVendorID << 16) | codecRead(kCodecVendorID2);

    fExtAudioID     = codecRead(kCodecExtAudioID);
    fExtAudioStatus = codecRead(kCodecExtAudioStatus);

    fExtAudioStatusReadyMask = 0;

    // Primary codec?

    fIsPrimaryCodec = true;
    if (fCodecID || (fExtAudioID & kExtAudioID_ID_MASK))
    {
        fIsPrimaryCodec = false;
    }

    // Count the number of PCM out DACs available.

    fNumPCMOutDAC = 0;
    if (isConverterSupported(kConverterPCMFront))
    {
        fNumPCMOutDAC = 2;
        if (isConverterSupported(kConverterPCMSurround))
        {
            fNumPCMOutDAC = 4;
            if (isConverterSupported(kConverterPCMCenter) &&
                isConverterSupported(kConverterPCMLFE))
            {
                fNumPCMOutDAC = 6;
            }
        }
    }

    DebugLog("%s[ID %u] PCM out DACs = %lu\n", getName(), fCodecID,
             fNumPCMOutDAC);

    if (fNumPCMOutDAC == 0)
    {
        return false;
    }

    // Page 1 extended registers supported?

    if (isRegisterPageSupported( kCodecRegisterPage1 ))
    {
        fIsPage1Supported = true;
    }

    // Probe optional features.

    probeAuxiliaryOutSupport();
    probeAnalogOutputSupport();
    probeAnalogSourceSupport();

    // Test if master volume control has 5 or 6 bits of resolution.

    fMasterVolumeBitCount = 5;
    codecWrite( kCodecMasterVolume, 0xA020 );
    if ( codecRead( kCodecMasterVolume ) == 0xA020 )
    {
        fMasterVolumeBitCount = 6;
    }
    DebugLog("%s[ID %u] %lu bit master volume\n",
             getName(), fCodecID, fMasterVolumeBitCount);

    // Turn on VRA (PCM out and in) if supported.

    if (fExtAudioID & kExtAudioID_VRA)
    {
        UInt32 measuredRate;

        fExtAudioStatus |= kExtAudioStatus_VRA;
        codecWrite(kCodecExtAudioStatus, fExtAudioStatus);

        fMeasured48KRate = kIOAC97SampleRate48K;
        measuredRate = measure48KSampleRate(kConverterPCMFront);

        if (measuredRate < 48750 && measuredRate > 47250)
            fMeasured48KRate = kIOAC97SampleRate48K;
        else
            fMeasured48KRate = measuredRate;

        DebugLog("%s[ID %u] measured 48K sample rate = %lu\n",
                 getName(), fCodecID, measuredRate);
    }

    // All optional converters will be powered off until an audio
    // configuration with more than 2 channels is activated.

    fExtAudioStatus |= (kExtAudioStatus_PRI |
                        kExtAudioStatus_PRJ |
                        kExtAudioStatus_PRK |
                        kExtAudioStatus_PRL |
                        kExtAudioStatus_SPSA_10_11);

    return true;
}

//---------------------------------------------------------------------------

bool CLASS::primeHardware( void )
{
    // If the codec supports the DAC Slot Mapping Descriptors in Page 1,
    // disable it by clearing the mapping valid bit. We don't use it.

    if (fIsPage1Supported)
    {
        IOAC97CodecWord map;

        map = codecRead(kCodecADCSlotMapping, kCodecRegisterPage1);
        codecWrite(kCodecADCSlotMapping, map & ~0x0001, kCodecRegisterPage1);
        DebugLog("%s[ID %u] Slot Mapping Descriptor = %04x %04x\n",
                 getName(), fCodecID,
                 codecRead(kCodecDACSlotMapping, kCodecRegisterPage1),
                 codecRead(kCodecADCSlotMapping, kCodecRegisterPage1));
    }

    // Set DAC slot assignment (DSA) to [00].

    IOAC97CodecWord id = codecRead(kCodecExtAudioID);
    if (id & kExtAudioID_DSA_MASK)
    {
        id &= ~kExtAudioID_DSA_MASK;
        codecWrite(kCodecExtAudioID, id);
    }

    // Update the Extended Audio Status register.

    codecWrite(kCodecExtAudioStatus, fExtAudioStatus);

    if (!fIsPrimaryCodec)
    {
        // Power down and disable all converters.
        codecWrite(kCodecPowerdown, kPowerdown_AllFunctions);
    }
    else
    {
        // FIXME: Default analog mixer settings (passthrough support?)  
        codecWrite( kCodecPCMOutVolume, 0x0808 ); // 0 db
        codecWrite( kCodecPCBeepVolume, 0x0000 ); // 0 db
        codecWrite( kCodecMicVolume,    0x801f ); // mute, -34.5db
        codecWrite( kCodecCDVolume,     0x9f1f ); // mute, -34.5db
        codecWrite( kCodecLineInVolume, 0x9f1f ); // mute, -34.5db
    }

    return true;
}

#pragma mark -
#pragma mark еее Codec Access еее
#pragma mark -

//---------------------------------------------------------------------------

IOAC97CodecWord CLASS::codecRead( IOAC97CodecOffset offset,
                                  UInt32            page )
{
    IOAC97CodecWord word = 0;

    IOAC97Assert(offset < kCodecRegisterCount);
    IOAC97Assert((offset & 1) == 0);

    if (page == kCodecRegisterPage0)
    {
        fCodecDevice->codecRead(offset, &word);
    }
    else
    {
        fCodecDevice->codecWrite(kCodecInterruptAndPage, page & 0x0F);
        fCodecDevice->codecRead(offset, &word);
        fCodecDevice->codecWrite(kCodecInterruptAndPage, 0);
    }

    return word;
}

//---------------------------------------------------------------------------

void CLASS::codecWrite( IOAC97CodecOffset offset, IOAC97CodecWord word,
                        UInt32 page )
{
    IOAC97Assert(offset < kCodecRegisterCount);
    IOAC97Assert((offset & 1) == 0);

    if (page == kCodecRegisterPage0)
    {
        fCodecDevice->codecWrite(offset, word);
    }
    else
    {
        fCodecDevice->codecWrite(kCodecInterruptAndPage, page & 0x0F);
        fCodecDevice->codecWrite(offset, word);
        fCodecDevice->codecWrite(kCodecInterruptAndPage, 0);
    }
}

//---------------------------------------------------------------------------

bool CLASS::isRegisterPageSupported( UInt32 page )
{
    bool support = false;

    if (page == kCodecRegisterPage0)
        return true;

    if (page <= 0xF)
    {
        IOAC97CodecWord word = (page & 0xF);
        IOAC97CodecWord back;
        fCodecDevice->codecWrite(kCodecInterruptAndPage, word);
        fCodecDevice->codecRead(kCodecInterruptAndPage, &back);
        if ((back & 0xF) == word)
            support = true;
    }

    return support;
}

//---------------------------------------------------------------------------

IOAC97CodecID CLASS::getCodecID( void ) const
{
    return fCodecDevice->getCodecID();
}

void * CLASS::getCodecParameter( void ) const
{
    return fCodecDevice->getCodecParameter();
}

IOAC97Controller * CLASS::getAudioController( void ) const
{
    return fCodecDevice->getController();
}

#pragma mark -
#pragma mark еее ADC and DAC Control еее
#pragma mark -

//---------------------------------------------------------------------------

bool CLASS::isConverterSupported( IOAC97CodecConverter converter )
{
    bool support = false;

    switch ( converter )
    {
        case kConverterPCMFront:
        case kConverterPCMIn:
            support = true;
            break;

        case kConverterPCMCenter:
            if (fExtAudioID & kExtAudioID_CDAC)
                support = true;
            break;

        case kConverterPCMLFE:
            if (fExtAudioID & kExtAudioID_LDAC)
                support = true;
            break;

        case kConverterPCMSurround:
            if (fExtAudioID & kExtAudioID_SDAC)
                support = true;
            break;
    }

    DebugLog("%s[ID %u] converter (%lu) support: %s \n",
             getName(), fCodecID, converter,
             support ? "Yes" : "No");

    return support;
}

//---------------------------------------------------------------------------

static const UInt8 SampleRateOffsetTable[ CLASS::kConverterLast ] =
{
    kCodecPCMFrontDACRate,      /* kConverterPCMFront */
    kCodecPCMSurroundDACRate,   /* kConverterPCMSurround */
    kCodecPCMFrontDACRate,      /* kConverterPCMCenter */
    kCodecPCMLFEDACRate,        /* kConverterPCMLFE */
    kCodecPCMADCRate,           /* kConverterPCMIn */
    kCodecMicADCRate            /* kConverterMIC */
};

bool CLASS::isConverterSampleRateSupported( IOAC97CodecConverter converter,
                                            UInt32               sampleRate )
{
    // Only way to really find out if the sample rate is supported is
    // by writing to the rate register then read it back. This should
    // be performed before the DMA engine is running.

    if (setConverterSampleRate(converter, sampleRate) == kIOReturnSuccess)
        return true;
    else
        return false;
}

IOReturn CLASS::setConverterSampleRate( IOAC97CodecConverter converter,
                                        UInt32               sampleRate )
{
    IOAC97CodecWord   feature;
    IOAC97CodecOffset regOffset;
    IOAC97CodecWord   savedRate;
    UInt32            scaledRate;

    if (converter >= kConverterLast)
        return kIOReturnBadArgument;

    // Sample rate limited to (8000 <= rate <= 48000).
    // DRA not currently supported.

    if ((sampleRate > kIOAC97SampleRate48K) ||
        (sampleRate < kIOAC97SampleRate8K))
        return false;

    // Is variable rate supported and enabled?
    // If not enabled, then support restricted to 48K.

    if (converter == kConverterMIC)
        feature = kExtAudioStatus_VRM;
    else
        feature = kExtAudioStatus_VRA;

    if ((fExtAudioStatus & feature) == 0)
    {
        DebugLog("%s[ID %u]::%s(%lu, %lu) no variable rate audio\n",
                 getName(), fCodecID, __FUNCTION__, converter, sampleRate);

        return (kIOAC97SampleRate48K == sampleRate) ?
                kIOReturnSuccess : kIOReturnUnsupported;
    }

    IOAC97Assert(fMeasured48KRate != 0);
    scaledRate = (sampleRate * kIOAC97SampleRate48K) / fMeasured48KRate;

    if (scaledRate > 0xFFFF)
    {
        DebugLog("%s[ID %u]::%s(%lu, %lu) scaled sample rate %lu overflow\n",
                 getName(), fCodecID, __FUNCTION__, converter, sampleRate,
                 scaledRate);
        return kIOReturnUnsupported;
    }

    regOffset = SampleRateOffsetTable[converter];
    savedRate = codecRead(regOffset);

    codecWrite( regOffset, scaledRate );
    if (codecRead(regOffset) == scaledRate)
    {
        DebugLog("%s[ID %u]::%s(%lu, %lu) success, real rate = %lu\n",
                 getName(), fCodecID, __FUNCTION__, converter, sampleRate,
                 scaledRate);
        return kIOReturnSuccess;
    }
    else
    {
        DebugLog("%s[ID %u]::%s(%lu, %lu) rate %u != desired rate %lu\n",
                 getName(), fCodecID, __FUNCTION__, converter, sampleRate,
                 codecRead(regOffset), scaledRate);

        codecWrite( regOffset, savedRate );
        return kIOReturnUnsupported;
    }
}

IOReturn CLASS::getConverterSampleRate( IOAC97CodecConverter converter,
                                        UInt32 *             sampleRate )
{
    IOAC97CodecWord  feature;
    IOAC97CodecWord  codecRate;
    UInt32           scaledRate;

    if (converter >= kConverterLast)
        return kIOReturnBadArgument;

    if (converter == kConverterMIC)
        feature = kExtAudioStatus_VRM;
    else
        feature = kExtAudioStatus_VRA;

    if ((fExtAudioStatus & feature) == 0)
    {
        *sampleRate = kIOAC97SampleRate48K;
    }
    else
    {
        codecRate  = codecRead( SampleRateOffsetTable[converter] );
        scaledRate = (codecRate * fMeasured48KRate) / kIOAC97SampleRate48K;
        *sampleRate = scaledRate;
    }

    DebugLog("%s[ID %u]::%s converter %u rate = %lu\n",
             getName(), fCodecID, __FUNCTION__, converter, *sampleRate);
    
    return kIOReturnSuccess;
}

#pragma mark -
#pragma mark еее Analog Output еее
#pragma mark -

static const struct {
    IOAC97CodecOffset offset;
    UInt8             volumeLeftShift;
    UInt8             volumeRightShift;
} AnalogOutputVolumeTable[] =
{
    { kCodecMasterVolume,    8, 0 },  /* kAnalogOutputLineOut */
    { kCodecAuxOutVolume,    8, 0 },  /* kAnalogOutputAuxOut */
    { kCodecSurroundVolume,  8, 0 },  /* kAnalogOutputSurround */
    { kCodecMonoVolume,      0, 0 },  /* kAnalogOutputMonoOut */
    { kCodecCenterLFEVolume, 0, 0 },  /* kAnalogOutputCenter */
    { kCodecCenterLFEVolume, 8, 8 }   /* kAnalogOutputLFE */
};

//---------------------------------------------------------------------------

void CLASS::probeAuxiliaryOutSupport( void )
{
    IOAC97CodecWord  auxVolume;

    fAuxOutVolumeSupport = false;
    fAuxOutFunction = kAuxOutFunctionLineLevelOut;

    auxVolume = codecRead( kCodecAuxOutVolume );

    if (auxVolume == 0x8000)
        fAuxOutVolumeSupport = true;

    if ((codecRead( kCodecAudioReset ) & kAudioReset_HPOUT) &&
        (auxVolume == 0x8000))
    {
        fAuxOutFunction = kAuxOutFunctionHeadphoneOut;
    }
    else if ((fExtAudioID & kExtAudioID_SDAC) &&
             (codecRead( kCodecSurroundVolume ) == 0x8080))
    {
        fAuxOutFunction = kAuxOutFunction4ChannelOut;
        fAuxOutVolumeSupport = false;
    }

    DebugLog("%s[ID %u]::%s function is %lu\n", getName(), fCodecID,
             __FUNCTION__, fAuxOutFunction);
}

//---------------------------------------------------------------------------

void CLASS::probeAnalogOutputSupport( void )
{
    fAnalogOutputSupportMask = 0;

    // Mandatory analog outputs.
 
    fAnalogOutputSupportMask |= (1 << kAnalogOutputLineOut);
    fAnalogOutputSupportMask |= (1 << kAnalogOutputAuxOut);
 
    // Probe surround (SDAC = 1, and default value for reg 0x38 = 0x8080).

    if (isConverterSupported(kConverterPCMSurround) &&
        codecRead(kCodecSurroundVolume) == 0x8080)
        fAnalogOutputSupportMask |= (1 << kAnalogOutputSurround);

    // Probe support for optional MONO_OUT.

    if (codecRead(kCodecMonoVolume) == 0x8000)
        fAnalogOutputSupportMask |= (1 << kAnalogOutputMonoOut);

    // Probe support for optional Center Channel output.

    if (isConverterSupported(kConverterPCMCenter) &&
        ((codecRead(kCodecCenterLFEVolume) & 0xFF) == 0x80))
         fAnalogOutputSupportMask |= (1 << kAnalogOutputCenter);

    // Probe support for optional LFE output.

    if (isConverterSupported(kConverterPCMLFE) &&
        ((codecRead(kCodecCenterLFEVolume) & 0xFF00) == 0x8000))
         fAnalogOutputSupportMask |= (1 << kAnalogOutputLFE);

    DebugLog("%s[ID %u]::%s analog output support mask %08lx\n",
             getName(), fCodecID, __FUNCTION__, fAnalogOutputSupportMask);
}

//---------------------------------------------------------------------------

bool CLASS::isAnalogOutputSupported( IOAC97AnalogOutput output )
{
    if ((output < kAnalogOutputLast) &&
        ((1 << output) & fAnalogOutputSupportMask))
        return true;
    else
        return false;
}

//---------------------------------------------------------------------------

IOReturn CLASS::getAnalogOutputVolumeRange( IOAC97AnalogOutput  output,
                                            IOAC97VolumeValue * minValue,
                                            IOAC97VolumeValue * maxValue,
                                            IOFixed *           minDB,
                                            IOFixed *           maxDB )
{
    if (!minValue || !maxValue || !minDB || !maxDB)
    {
        return kIOReturnBadArgument;
    }

    if (isAnalogOutputSupported(output) == false)
    {
        return kIOReturnUnsupported;
    }

    if (fMasterVolumeBitCount == 5)
        *minDB = (-46 << 16) | 0x8000;
    else
        *minDB = (-94 << 16) | 0x8000;

    *maxValue = (1 << fMasterVolumeBitCount) - 1;
    *minValue = 0;
    *maxDB    = 0;

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::setAnalogOutputVolume( IOAC97AnalogOutput  output,
                                       IOAC97AnalogChannel channel,
                                       IOAC97VolumeValue   volume )
{
    IOAC97CodecOffset offset;
    IOAC97CodecWord   word;
    IOAC97CodecWord   mask = (1 << fMasterVolumeBitCount) - 1;
    int               shift;

    if ((volume < 0) || (volume > mask) ||
        ((channel & kAnalogChannelStereo) == 0))
    {
        return kIOReturnBadArgument;
    }

    if (isAnalogOutputSupported(output) == false)
    {
        return kIOReturnUnsupported;
    }

    volume = mask - (volume & mask);

    offset = AnalogOutputVolumeTable[output].offset;
    word   = codecRead(offset);

    if (channel & kAnalogChannelLeft)
    {
        shift = AnalogOutputVolumeTable[output].volumeLeftShift;
        word &= ~(mask << shift);
        word |= (volume << shift);
    }

    if (channel & kAnalogChannelRight)
    {
        shift = AnalogOutputVolumeTable[output].volumeRightShift;
        word &= ~(mask << shift);
        word |= (volume << shift);
    }

    codecWrite(offset, word);

    DebugLog("%s[ID %u]::%s(%lu, %lu, %ld) offset 0x%lx word 0x%04x\n",
             getName(), fCodecID, __FUNCTION__, output, channel, volume,
             offset, word);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::setAnalogOutputMute( IOAC97AnalogOutput  output,
                                     IOAC97AnalogChannel channel,
                                     bool                mute )
{
    IOAC97CodecOffset offset;
    IOAC97CodecWord   volume;
    IOAC97CodecWord   muteMask;

    if ((channel & kAnalogChannelStereo) == 0)
    {
        return kIOReturnBadArgument;
    }

    if (isAnalogOutputSupported(output) == false)
    {
        return kIOReturnUnsupported;
    }

    offset = AnalogOutputVolumeTable[output].offset;
    volume = codecRead(offset);

    muteMask = getAnalogOutputMuteBitMask(output, channel);

    volume &= ~muteMask;
    if (mute) volume |= muteMask;

    codecWrite(offset, volume);

    DebugLog("%s[ID %u]::%s(%lu, %lu, %u) offset 0x%lx volume 0x%04x\n",
             getName(), fCodecID, __FUNCTION__, output, channel, mute,
             offset, volume);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOAC97CodecWord CLASS::getAnalogOutputMuteBitMask(
                       IOAC97AnalogOutput  output,
                       IOAC97AnalogChannel channel )
{
    IOOptionBits mask = 0;

    static const struct {
        UInt8  left;
        UInt8  right;
    } AnalogOutputMutePositionTable[ kAnalogOutputLast ] =
    {
        { 15, 15 },  /* kAnalogOutputLineOut */
        { 15, 15 },  /* kAnalogOutputAuxOut */
        { 15,  7 },  /* kAnalogOutputSurround */
        { 15, 15 },  /* kAnalogOutputMonoOut */
        {  7,  7 },  /* kAnalogOutputCenter */
        { 15, 15 }   /* kAnalogOutputLFE */
    };

    if (output >= kAnalogOutputLast)
        return 0;

    switch (channel)
    {
        case kAnalogChannelLeft:
            mask = (1 << AnalogOutputMutePositionTable[output].left);
            break;

        case kAnalogChannelRight:
            mask = (1 << AnalogOutputMutePositionTable[output].right);
            break;

        case kAnalogChannelStereo:
            mask = (1 << AnalogOutputMutePositionTable[output].right) |
                   (1 << AnalogOutputMutePositionTable[output].left);
            break;
    }

    return mask;
}

#pragma mark -
#pragma mark еее Analog Source еее
#pragma mark -

#define kAnalogSourceVolumeMask   ((1 << 5) - 1)

static const IOAC97CodecOffset AnalogSourceOffsetTable[] =
{
    kCodecPCBeepVolume,  /* FIXME: register format is unique */
    kCodecPhoneVolume,
    kCodecMicVolume,     /* MIC1 */
    kCodecMicVolume,     /* MIC2 */
    kCodecLineInVolume,
    kCodecCDVolume,
    kCodecVideoVolume,
    kCodecAuxInVolume,
    kCodecPCMOutVolume
};

//---------------------------------------------------------------------------

void CLASS::probeAnalogSourceSupport( void )
{
    fAnalogSourceSupportMask = 0;

    // Mandatory analog sources.

    fAnalogSourceSupportMask |= (1 << kAnalogSourceMIC1); 
    fAnalogSourceSupportMask |= (1 << kAnalogSourceLineIn);
    fAnalogSourceSupportMask |= (1 << kAnalogSourceCD);
    fAnalogSourceSupportMask |= (1 << kAnalogSourcePCMOut);

    DebugLog("%s[ID %u]::%s analog source support mask %08lx\n",
             getName(), fCodecID, __FUNCTION__, fAnalogSourceSupportMask);
}

//---------------------------------------------------------------------------

bool CLASS::isAnalogSourceSupported( IOAC97AnalogSource source )
{
    if ((source < kAnalogSourceLast) &&
        ((1 << source) & fAnalogSourceSupportMask))
        return true;
    else
        return false;
}

//---------------------------------------------------------------------------

IOReturn CLASS::getAnalogSourceVolumeRange( IOAC97AnalogOutput  source,
                                            IOAC97VolumeValue * minValue,
                                            IOAC97VolumeValue * maxValue,
                                            IOFixed *           minDB,
                                            IOFixed *           maxDB )
{
    if (!minValue || !maxValue || !minDB || !maxDB)
    {
        return kIOReturnBadArgument;
    }

    if (isAnalogSourceSupported(source) == false)
    {
        return kIOReturnUnsupported;
    }

    *minValue = 0;
    *maxValue = kAnalogSourceVolumeMask;
    *minDB    = (-34 << 16) | 0x8000;
    *maxDB    = ( 12 << 16);

    // FIXME: MIC has +20dB gain control

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::setAnalogSourceVolume( IOAC97AnalogSource  source,
                                       IOAC97AnalogChannel channel,
                                       IOAC97VolumeValue   volume )
{
    IOAC97CodecOffset offset;
    IOAC97CodecWord   word;

    if ((volume < 0) || (volume > kAnalogSourceVolumeMask) ||
        ((channel & kAnalogChannelStereo) == 0))
    {
        return kIOReturnBadArgument;
    }

    if (isAnalogSourceSupported(source) == false)
    {
        return kIOReturnUnsupported;
    }

    volume = kAnalogSourceVolumeMask - (volume & kAnalogSourceVolumeMask);

    offset = AnalogSourceOffsetTable[source];
    word   = codecRead(offset);

    if (channel & kAnalogChannelLeft)
    {
        word &= ~(kAnalogSourceVolumeMask << 8);
        word |= (volume << 8);
    }

    if (channel & kAnalogChannelRight)
    {
        word &= ~kAnalogSourceVolumeMask;
        word |= volume;
    }

    codecWrite(offset, word);

    DebugLog("%s[ID %u]::%s(%lu, %lu, %ld) offset 0x%lx word 0x%04x\n",
             getName(), fCodecID, __FUNCTION__, source, channel, volume,
             offset, word);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::setAnalogSourceMute( IOAC97AnalogSource  source,
                                     IOAC97AnalogChannel channel,
                                     bool                mute )
{
    IOAC97CodecOffset offset;
    IOAC97CodecWord   word;
    IOAC97CodecWord   mask;

    if ((channel & kAnalogChannelStereo) == 0)
    {
        return kIOReturnBadArgument;
    }

    if (isAnalogSourceSupported(source) == false)
    {
        return kIOReturnUnsupported;
    }

    offset = AnalogSourceOffsetTable[source];
    word   = codecRead(offset);
    mask   = getAnalogSourceMuteBitMask(source, channel);

    if (mute) word |= mask;
    else      word &= ~mask;

    codecWrite(offset, word);

    DebugLog("%s[ID %u]::%s(%lu, %lu, %u) offset 0x%lx word 0x%04x\n",
             getName(), fCodecID, __FUNCTION__, source, channel, mute,
             offset, word);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOAC97CodecWord CLASS::getAnalogSourceMuteBitMask(
                       IOAC97AnalogSource  source,
                       IOAC97AnalogChannel channel )
{
    return (1 << 15);
}

#pragma mark -
#pragma mark еее Record Source Selection еее
#pragma mark -

//---------------------------------------------------------------------------

bool CLASS::isRecordSourceSupported( IOAC97RecordSource source )
{
    // Mandatory record sources

    if (source == kRecordSourceMIC       ||
        source == kRecordSourceCD        ||
        source == kRecordSourceLineIn    ||
        source == kRecordSourceStereoMix ||
        source == kRecordSourceMonoMix)
        return true;
    else
        return false;
}

//---------------------------------------------------------------------------

IOReturn CLASS::setRecordSourceSelection( IOAC97RecordSource sourceLeft,
                                          IOAC97RecordSource sourceRight )
{
    IOAC97CodecWord  select;

    DebugLog("%s[ID %u]::%s source left %u right %u\n",
             getName(), fCodecID, __FUNCTION__, sourceLeft, sourceRight);

    if (isRecordSourceSupported(sourceLeft)  == false ||
        isRecordSourceSupported(sourceRight) == false)
    {
        return kIOReturnUnsupported;
    }

    select = ((sourceLeft & 7) << 8) | (sourceRight & 7);
    codecWrite(kCodecRecordSelect, select);

    return kIOReturnSuccess;
}

#pragma mark -
#pragma mark еее Record Gain Control еее
#pragma mark -

#define kRecordGainMask   ((1 << 4) - 1)

//---------------------------------------------------------------------------

IOReturn CLASS::getRecordGainRange( IOAC97GainValue * minValue,
                                    IOAC97GainValue * maxValue,
                                    IOFixed *         minDB,
                                    IOFixed *         maxDB )
{
    if (!minValue || !maxValue || !minDB || !maxDB)
    {
        return kIOReturnBadArgument;
    }

    *minValue = 0;
    *maxValue = kRecordGainMask;
    *minDB    = 0;
    *maxDB    = (22 << 16) | 0x8000;

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::setRecordGainValue( IOAC97AnalogChannel analogChannel,
                                    IOAC97GainValue     gainValue )
{
    IOAC97CodecWord  word;

    DebugLog("%s[ID %u]::%s channel %lu gain %ld\n",
             getName(), fCodecID, __FUNCTION__, analogChannel, gainValue);

    if ((gainValue < 0) || (gainValue > kRecordGainMask) ||
        ((analogChannel & kAnalogChannelStereo) == 0))
    {
        return kIOReturnBadArgument;
    }

    word = codecRead(kCodecRecordGain);

    if (analogChannel & kAnalogChannelLeft)
    {
        word &= ~(kRecordGainMask << 8);
        word |=  (gainValue << 8);
    }

    if (analogChannel & kAnalogChannelRight)
    {
        word &= ~kRecordGainMask;
        word |=  gainValue;
    }

    codecWrite(kCodecRecordGain, word);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::setRecordGainMute( IOAC97AnalogChannel analogChannel,
                                   bool                mute )
{
    IOAC97CodecWord  word;
    IOAC97CodecWord  mask;

    DebugLog("%s[ID %u]::%s channel %lu mute %u\n",
             getName(), fCodecID, __FUNCTION__, analogChannel, mute);

    if ((analogChannel & kAnalogChannelStereo) == 0)
    {
        return kIOReturnBadArgument;
    }

    word = codecRead(kCodecRecordGain);
    mask = getRecordGainMuteBitMask(analogChannel);

    if (mute) word |= mask;
    else      word &= ~mask;

    codecWrite(kCodecRecordGain, word);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOAC97CodecWord CLASS::getRecordGainMuteBitMask(
                       IOAC97AnalogChannel analogChannel )
{
    return (1 << 15);
}

#pragma mark -
#pragma mark еее Audio Configuration еее
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::prepareAudioConfiguration( IOAC97AudioConfig * config )
{
    UInt32   engineType;
    UInt32   direction;
    IOReturn ret;

    if (!config)
        return kIOReturnBadArgument;

    engineType = config->getDMAEngineType();
    direction  = config->getDMADataDirection();

    if ((engineType == kIOAC97DMAEngineTypeAudioPCM) &&
        (direction  == kIOAC97DMADataDirectionOutput))
    {
        ret = prepareAudioConfigurationPCMOut( config );
    }
    else if ((engineType == kIOAC97DMAEngineTypeAudioPCM) &&
             (direction  == kIOAC97DMADataDirectionInput))
    {
        ret = prepareAudioConfigurationPCMIn( config );
    }
    else if ((engineType == kIOAC97DMAEngineTypeAudioSPDIF) &&
             (direction  == kIOAC97DMADataDirectionOutput))
    {
        ret = prepareAudioConfigurationSPDIF( config );
    }
    else
        ret = kIOReturnUnsupported;

    return ret;
}

//---------------------------------------------------------------------------

IOReturn CLASS::activateAudioConfiguration( IOAC97AudioConfig * config )
{
    UInt32    engineType;
    UInt32    direction;
    UInt32    cachedPowerState;
    IOReturn  ret;

    if (!config)
        return kIOReturnBadArgument;

    // If the codec is not contributing to the configuration,
    // then return success immediately.

    if (config->getCodecSlotMap(fCodecID) == 0)
        return kIOReturnSuccess;

    engineType = config->getDMAEngineType();
    direction  = config->getDMADataDirection();

    cachedPowerState = fCurrentPowerState;
    IOAC97Assert(cachedPowerState != kPowerStateSleep);    

    if (cachedPowerState == kPowerStateIdle)
    {
        raisePowerState( kPowerStateActive );
    }

    if ((engineType == kIOAC97DMAEngineTypeAudioPCM) &&
        (direction  == kIOAC97DMADataDirectionOutput))
    {
        ret = activateAudioConfigurationPCMOut( config );    
    }
    else if ((engineType == kIOAC97DMAEngineTypeAudioPCM) &&
             (direction  == kIOAC97DMADataDirectionInput))
    {
        ret = activateAudioConfigurationPCMIn( config );    
    }
    else if ((engineType == kIOAC97DMAEngineTypeAudioSPDIF) &&
             (direction  == kIOAC97DMADataDirectionOutput))
    {
        ret = activateAudioConfigurationSPDIF( config );
    }
    else
        ret = kIOReturnUnsupported;

    if (cachedPowerState == kPowerStateIdle)
    {
        lowerPowerState( kPowerStateIdle );
    }

    return ret;
}

//---------------------------------------------------------------------------

void CLASS::deactivateAudioConfiguration( IOAC97AudioConfig * config )
{
    UInt32  engineType;
    UInt32  direction;

    if (!config) return;

    // If the codec is not contributing to the configuration,
    // then return success immediately.

    if (config->getCodecSlotMap(fCodecID) == 0)
        return;

    engineType = config->getDMAEngineType();
    direction  = config->getDMADataDirection();

    if ((engineType == kIOAC97DMAEngineTypeAudioPCM) &&
        (direction  == kIOAC97DMADataDirectionOutput))
    {
        deactivateAudioConfigurationPCMOut( config );    
    }
    else if ((engineType == kIOAC97DMAEngineTypeAudioPCM) &&
             (direction  == kIOAC97DMADataDirectionInput))
    {
        deactivateAudioConfigurationPCMIn( config );    
    }
    else if ((engineType == kIOAC97DMAEngineTypeAudioSPDIF) &&
             (direction  == kIOAC97DMADataDirectionOutput))
    {
        deactivateAudioConfigurationSPDIF( config );
    }
}

//---------------------------------------------------------------------------

IOReturn CLASS::createAudioControls( IOAC97AudioConfig * config,
                                     OSArray *           array )
{
    UInt32   engineType;
    UInt32   direction;
    IOReturn ret;

    if (!config || !array)
        return kIOReturnBadArgument;

    // If the codec is not contributing to the configuration,
    // then return success immediately.

    if (config->getCodecSlotMap(fCodecID) == 0)
        return kIOReturnSuccess;

    engineType = config->getDMAEngineType();
    direction  = config->getDMADataDirection();

    if ((engineType == kIOAC97DMAEngineTypeAudioPCM) &&
        (direction  == kIOAC97DMADataDirectionOutput))
    {
        ret = createAudioControlsPCMOut( config, array );
    }
    else if ((engineType == kIOAC97DMAEngineTypeAudioPCM) &&
             (direction  == kIOAC97DMADataDirectionInput))
    {
        ret = createAudioControlsPCMIn( config, array );
    }
    else if ((engineType == kIOAC97DMAEngineTypeAudioSPDIF) &&
             (direction  == kIOAC97DMADataDirectionOutput))
    {
        ret = createAudioControlsSPDIF( config, array );
    }
    else
        ret = kIOReturnUnsupported;

    return ret;
}

#pragma mark -
#pragma mark еее Audio Configuration (PCM Out) еее
#pragma mark -

//---------------------------------------------------------------------------

struct PCMChannelEntry {
    UInt32       analogOutput;
    UInt32       analogChannel;
    UInt32       converter;
    UInt32       channelID;
    const char * channelName;
};

static const PCMChannelEntry PCMChannelTable[] =
{
    {
        CLASS::kAnalogOutputLineOut,
        CLASS::kAnalogChannelLeft,
        CLASS::kConverterPCMFront,
        kIOAudioControlChannelIDDefaultLeft,
        kIOAudioControlChannelNameLeft
    },
    {
        CLASS::kAnalogOutputLineOut,
        CLASS::kAnalogChannelRight,
        CLASS::kConverterPCMFront,
        kIOAudioControlChannelIDDefaultRight,
        kIOAudioControlChannelNameRight
    },
    {
        CLASS::kAnalogOutputSurround,
        CLASS::kAnalogChannelLeft,
        CLASS::kConverterPCMSurround,
        kIOAudioControlChannelIDDefaultLeftRear,
        kIOAudioControlChannelNameLeftRear
    },
    {
        CLASS::kAnalogOutputSurround,
        CLASS::kAnalogChannelRight,
        CLASS::kConverterPCMSurround,
        kIOAudioControlChannelIDDefaultRightRear,
        kIOAudioControlChannelNameRightRear
    },
    {
        CLASS::kAnalogOutputCenter,
        CLASS::kAnalogChannelRight,
        CLASS::kConverterPCMCenter,
        kIOAudioControlChannelIDDefaultCenter,
        kIOAudioControlChannelNameCenter
    },
    {
        CLASS::kAnalogOutputLFE,
        CLASS::kAnalogChannelRight,
        CLASS::kConverterPCMLFE,
        kIOAudioControlChannelIDDefaultSub,
        kIOAudioControlChannelNameSub
    }
};

//---------------------------------------------------------------------------

IOReturn CLASS::prepareAudioConfigurationPCMOut( IOAC97AudioConfig * config )
{
    IOOptionBits slotMap;
    IOItemCount  numChannels;
    bool         ok;

    DebugLog("%s[ID %u]::%s(%p)\n",
             getName(), fCodecID, __FUNCTION__, config);

    // Secondary audio codecs are not used, but should not prevent
    // configurations from using the primary codec.

    if (!fIsPrimaryCodec)
        return kIOReturnSuccess;

    // Check the number of channels specified by the configuration.

    numChannels = config->getAudioChannelCount();
    if ((numChannels > fNumPCMOutDAC) || (numChannels & 1))
    {
        DebugLog("%s[ID %u]::%s bad number of audio channels %lu\n",
                 getName(), fCodecID, __FUNCTION__, numChannels);
        return kIOReturnUnsupported;
    }

    // Restricted to single codec configurations.

    if (config->getMergedCodecSlotMap())
    {
        DebugLog("%s[ID %u]::%s codec slot map not empty %lx\n",
                 getName(), fCodecID, __FUNCTION__,
                 config->getMergedCodecSlotMap());
        return kIOReturnUnsupported;
    }

    // Does the codec support the sample rate specified?

    for (unsigned int i = 0; i < numChannels; i++)
    {
        ok = isConverterSampleRateSupported(
             PCMChannelTable[i].converter, config->getSampleRate() );
        if (!ok)
        {
            DebugLog("%s[ID %u]::%s unsupported sample rate %lu\n",
                 getName(), fCodecID, __FUNCTION__,
                 config->getSampleRate());
            return kIOReturnUnsupported;
        }
    }

    // Install our proposed AC-link slot usage.

    switch (numChannels)
    {
        case 6:
            slotMap = kIOAC97Slot_6_9 | kIOAC97Slot_7_8 | kIOAC97Slot_3_4;
            break;
        case 4:
            slotMap = kIOAC97Slot_7_8 | kIOAC97Slot_3_4;
            break;
        case 2:
            slotMap = kIOAC97Slot_3_4;
            break;
        default:
            slotMap = 0;
    }

    ok = config->setCodecSlotMap( fCodecID, this, slotMap );
    if (ok != true)
    {
        DebugLog("%s[ID %u]::%s slot conflict\n",
                 getName(), fCodecID, __FUNCTION__);
        return kIOReturnNoChannels;
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::activateAudioConfigurationPCMOut( IOAC97AudioConfig * config )
{
    UInt32 numChannels = config->getAudioChannelCount();

    // Wait for optional DACs to become ready.

    if (numChannels > 2)
    {
        // Surround channels
        fExtAudioStatus &= ~kExtAudioStatus_PRJ;
        fExtAudioStatusReadyMask = kExtAudioStatus_SDAC;

        if (numChannels > 4)
        {
            // Center/LFE channels
            fExtAudioStatus &= ~(kExtAudioStatus_PRI | kExtAudioStatus_PRK);
            fExtAudioStatusReadyMask |= (kExtAudioStatus_CDAC |
                                         kExtAudioStatus_LDAC);
        }

        codecWrite( kCodecExtAudioStatus, fExtAudioStatus);
        waitForBits(kCodecExtAudioStatus, fExtAudioStatusReadyMask);

        DebugLog("%s[ID %u]::%s ExtAudioStatus = %04x\n",
                 getName(), fCodecID, __FUNCTION__, fExtAudioStatus);
    }

    // Program DAC sample rate.

    for (int dac = kConverterPCMFront; dac <= kConverterPCMLFE; dac++)
    {
        if (isConverterSupported(dac))
            setConverterSampleRate(dac, config->getSampleRate());
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void CLASS::deactivateAudioConfigurationPCMOut( IOAC97AudioConfig * config )
{
    // Turn off optional DACs.

    if (fExtAudioStatusReadyMask)
    {
        fExtAudioStatus |= (kExtAudioStatus_PRI |
                            kExtAudioStatus_PRJ | kExtAudioStatus_PRK);

        codecWrite(kCodecExtAudioStatus, fExtAudioStatus);
        fExtAudioStatusReadyMask = 0;

        DebugLog("%s[ID %u]::%s ExtAudioStatus = %04x\n",
                 getName(), fCodecID, __FUNCTION__, fExtAudioStatus);
    }

    setAnalogOutputMute(kAnalogOutputLineOut,  kAnalogChannelStereo, true);
    setAnalogOutputMute(kAnalogOutputSurround, kAnalogChannelStereo, true);
    setAnalogOutputMute(kAnalogOutputCenter,   kAnalogChannelStereo, true);
    setAnalogOutputMute(kAnalogOutputLFE,      kAnalogChannelStereo, true);
}

//---------------------------------------------------------------------------

IOReturn CLASS::createAudioControlsPCMOut( IOAC97AudioConfig * config,
                                           OSArray *           array )
{
    IOItemCount              numChannels;
    IOAC97VolumeValue        minValue;
    IOAC97VolumeValue        maxValue;
    IOFixed                  minDB;
    IOFixed                  maxDB;
    SInt32                   initialVolume;
    IOAudioControl *         audioControl;
    IOAudioSelectorControl * selectorControl;
    IOReturn                 ior = kIOReturnSuccess;

    // For PCM out, there is always a 1-to-1 mapping between the
    // audio channel and a DAC.

    numChannels = config->getAudioChannelCount();
    if ((numChannels > fNumPCMOutDAC) || (numChannels & 1))
    {
        DebugLog("%s[ID %u]::%s bad number of audio channels %lu\n",
                 getName(), fCodecID, __FUNCTION__, numChannels);
        return kIOReturnUnsupported;
    }

    for (unsigned int i = 0; i < numChannels; i++)
    {
        const PCMChannelEntry * entry = &PCMChannelTable[i];

        // Volume control

        ior = getAnalogOutputVolumeRange(
                      entry->analogOutput,
                      &minValue, &maxValue, &minDB, &maxDB);

        if (ior != kIOReturnSuccess)
            break;

        initialVolume = minValue + (maxValue - minValue) * 3 / 4;

        audioControl = IOAudioLevelControl::createVolumeControl(
                       /* initialValue */  initialVolume,
                       /* minValue     */  minValue,
                       /* maxValue     */  maxValue,
                       /* minDB        */  minDB,
                       /* maxDB        */  maxDB,
                       /* channelID    */  entry->channelID,
                       /* channelName  */  entry->channelName,
                       /* cntrlID      */  EncodeControlID(i),
                       /* usage        */  kIOAudioControlUsageOutput );

        if ( audioControl == 0 )
        {
            DebugLog("%s[ID %u]: createVolumeControl failed\n",
                     getName(), fCodecID);
            ior = kIOReturnNoMemory;
            break;
        }

        audioControl->setValueChangeHandler( volumeControlChanged, this );
        array->setObject( audioControl );
        audioControl->release();
        audioControl = 0;

        // Mute control

        audioControl = IOAudioToggleControl::createMuteControl(
                       /* initialValue */  false,
                       /* channelID    */  entry->channelID,
                       /* channelName  */  entry->channelName,
                       /* cntrlID      */  EncodeControlID(i),
                       /* usage        */  kIOAudioControlUsageOutput );

        if ( audioControl == 0 )
        {
            DebugLog("%s[ID %u]: createMuteControl failed\n",
                     getName(), fCodecID);
            ior = kIOReturnNoMemory;
            break;
        }

        audioControl->setValueChangeHandler( muteControlChanged, this );
        array->setObject( audioControl );
        audioControl->release();
        audioControl = 0;
    }
  
    selectorControl = IOAudioSelectorControl::createOutputSelector(
                      /* initialValue */ kIOAudioOutputPortSubTypeLine,
                      /* channelID    */ kIOAudioControlChannelIDAll);
    if (selectorControl)
    {
        selectorControl->addAvailableSelection(
                         kIOAudioOutputPortSubTypeLine, "Line Out");
        selectorControl->setValueChangeHandler(outputSelectorChanged, this);
        array->setObject( selectorControl );
        selectorControl->release();
        selectorControl = 0;
    }

    // Create global output mute control

    audioControl = IOAudioToggleControl::createMuteControl(
                   /* initialValue */  false,
                   /* channelID    */  kIOAudioControlChannelIDAll,
                   /* channelName  */  kIOAudioControlChannelNameAll,
                   /* cntrlID      */  kGlobalMuteControlID,
                   /* usage        */  kIOAudioControlUsageOutput );

    if (audioControl)
    {
        audioControl->setValueChangeHandler(muteControlChanged, this);
        array->setObject( audioControl );
        audioControl->release();
        audioControl = 0;
    }

    return ior;
}

//---------------------------------------------------------------------------

IOReturn CLASS::volumeControlChanged( OSObject *       target,
                                      IOAudioControl * control,
                                      SInt32           oldValue,
                                      SInt32           newValue )
{
    CLASS * codec = (CLASS *) target;
    int     ch;

    if (!codec || !control)
        return kIOReturnBadArgument;

    ch = DecodeControlID(control->getControlID());
    if (ch >= 6)
        return kIOReturnBadArgument;

    if ((PCMChannelTable[ch].analogOutput == kAnalogOutputLineOut) &&
         codec->fAuxOutVolumeSupport)
    {
        codec->setAnalogOutputVolume(kAnalogOutputAuxOut,
                                     PCMChannelTable[ch].analogChannel,
                                     newValue);
    }

    return codec->setAnalogOutputVolume( PCMChannelTable[ch].analogOutput,
                                         PCMChannelTable[ch].analogChannel,
                                         newValue );
}

static void
globalMuteApplier( IORegistryEntry * entry, void * context )
{
    IOAudioControl * muteControl;

    muteControl = OSDynamicCast(IOAudioToggleControl, entry);
    if (muteControl &&
        muteControl->getSubType() == kIOAudioToggleControlSubTypeMute &&
        muteControl->getControlID() != kGlobalMuteControlID)
    {
        SInt32 value = context ? 1 : 0;

        if (muteControl->getIntValue() != value)
            muteControl->setValue(value);
    }
}

IOReturn CLASS::muteControlChanged( OSObject *       target,
                                    IOAudioControl * control,
                                    SInt32           oldValue,
                                    SInt32           newValue )
{
    CLASS * codec = (CLASS *) target;
    int     ch;

    if (!codec || !control)
        return kIOReturnBadArgument;

    ch = DecodeControlID(control->getControlID());
    if (ch == kGlobalMuteControlID && (oldValue != newValue))
    {
        // Walk over all mute control siblings and update each one.

        IORegistryEntry * parent = control->copyParentEntry(gIOServicePlane);
        if (!parent) return kIOReturnNotAttached;
        parent->applyToChildren(globalMuteApplier, (void *)newValue,
                                gIOServicePlane);
        parent->release();
        return kIOReturnSuccess;
    }
    if (ch >= 6)
        return kIOReturnBadArgument;

    if ((PCMChannelTable[ch].analogOutput == kAnalogOutputLineOut) &&
         codec->fAuxOutVolumeSupport)
    {
        codec->setAnalogOutputMute(kAnalogOutputAuxOut,
                                   PCMChannelTable[ch].analogChannel,
                                   newValue);
    }

    return codec->setAnalogOutputMute( PCMChannelTable[ch].analogOutput,
                                       PCMChannelTable[ch].analogChannel,
                                       newValue );
}

IOReturn CLASS::outputSelectorChanged( OSObject *       target,
                                       IOAudioControl * control,
                                       SInt32           oldValue,
                                       SInt32           newValue )
{
    return kIOReturnSuccess;
}

#pragma mark -
#pragma mark еее Audio Configuration (PCM In) еее
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::prepareAudioConfigurationPCMIn( IOAC97AudioConfig * config )
{
    IOItemCount numChannels;
    bool        ok;

    DebugLog("%s[ID %u]::%s(%p)\n", getName(), fCodecID, __FUNCTION__,
             config);

    // Secondary audio codecs are not used, but should not prevent
    // configurations from using the primary codec.

    if (!fIsPrimaryCodec)
        return kIOReturnSuccess;

    // Check the number of channels specified by the configuration.

    numChannels = config->getAudioChannelCount();
    if (numChannels != 2)
    {
        DebugLog("%s[ID %u]::%s bad number of audio channels %lu\n",
                 getName(), fCodecID, __FUNCTION__, numChannels);
        return kIOReturnUnsupported;
    }

    // Restricted to single codec configurations.

    if (config->getMergedCodecSlotMap())
    {
        DebugLog("%s[ID %u]::%s codec slot map not empty %lx\n",
                 getName(), fCodecID, __FUNCTION__,
                 config->getMergedCodecSlotMap());
        return kIOReturnUnsupported;
    }

    // Is the sample rate supported?

    ok = isConverterSampleRateSupported(
         kConverterPCMIn, config->getSampleRate());
    if (!ok)
    {
        DebugLog("%s[ID %u]::%s unsupported sample rate %lu\n",
                 getName(), __FUNCTION__, fCodecID,
                 config->getSampleRate());
        return kIOReturnUnsupported;
    }

    // Report our proposed AC-link slot usage.

    ok = config->setCodecSlotMap( fCodecID, this, kIOAC97Slot_3_4 );
    if (ok != true)
    {
        DebugLog("%s[ID %u]::%s slot conflict\n",
                 getName(), fCodecID, __FUNCTION__);
        return kIOReturnNoChannels;
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::activateAudioConfigurationPCMIn( IOAC97AudioConfig * config )
{
    return setConverterSampleRate(kConverterPCMIn, config->getSampleRate());
}

void CLASS::deactivateAudioConfigurationPCMIn( IOAC97AudioConfig * config )
{
    return;
}

//---------------------------------------------------------------------------

IOReturn CLASS::createAudioControlsPCMIn( IOAC97AudioConfig * config,
                                          OSArray *           array )
{
    IOItemCount              numChannels;
    IOAC97VolumeValue        minValue;
    IOAC97VolumeValue        maxValue;
    IOFixed                  minDB;
    IOFixed                  maxDB;
    SInt32                   initialGain;
    UInt32                   sourceID;
    IOAudioControl *         gainControl;
    IOAudioControl *         muteControl;
    IOAudioSelectorControl * selectorControl;
    IOReturn                 ior = kIOReturnNoMemory;

    numChannels = config->getAudioChannelCount();
    if (numChannels != 2)
    {
        DebugLog("%s[ID %u]::%s bad number of audio channels %lu\n",
                 getName(), fCodecID, __FUNCTION__, numChannels);
        return kIOReturnUnsupported;
    }

    do {
        // Left channel record gain control.

        getRecordGainRange( &minValue, &maxValue, &minDB, &maxDB );

        initialGain = minValue + (maxValue - minValue) * 3 / 4;

        gainControl = IOAudioLevelControl::createVolumeControl(
                      /* initialValue */   initialGain,
                      /* minValue     */   minValue,
                      /* maxValue     */   maxValue,
                      /* minDB        */   minDB,
                      /* maxDB        */   maxDB,
                      /* channelID    */   kIOAudioControlChannelIDDefaultLeft,
                      /* channelName  */   kIOAudioControlChannelNameLeft,
                      /* cntrlID      */   EncodeControlID(kAnalogChannelLeft),
                      /* usage        */   kIOAudioControlUsageInput );

        if (!gainControl) break;
        gainControl->setValueChangeHandler( recordGainChanged, this );
        array->setObject( gainControl );
        gainControl->release();
        gainControl = 0;

        // Right channel record gain control.

        gainControl = IOAudioLevelControl::createVolumeControl(
                      /* initialValue */   initialGain,
                      /* minValue     */   minValue,
                      /* maxValue     */   maxValue,
                      /* minDB        */   minDB,
                      /* maxDB        */   maxDB,
                      /* channelID    */   kIOAudioControlChannelIDDefaultRight,
                      /* channelName  */   kIOAudioControlChannelNameRight,
                      /* cntrlID      */   EncodeControlID(kAnalogChannelRight),
                      /* usage        */   kIOAudioControlUsageInput );

        if (!gainControl) break;
        gainControl->setValueChangeHandler( recordGainChanged, this );
        array->setObject( gainControl );
        gainControl->release();
        gainControl = 0;

        // Record mute control.

        muteControl = IOAudioToggleControl::createMuteControl(
                      /* initialValue */  false,
                      /* channelID    */  kIOAudioControlChannelIDAll,
                      /* channelName  */  kIOAudioControlChannelNameAll,
                      /* cntrlID      */  EncodeControlID(0), 
                      /* usage        */  kIOAudioControlUsageInput );

        if (!muteControl) break;
        muteControl->setValueChangeHandler( recordMuteChanged, this );
        array->setObject( muteControl );
        muteControl->release();
        muteControl = 0;

        // Record source selector.

        selectorControl = IOAudioSelectorControl::createInputSelector(
                          EncodeControlID(kRecordSourceMIC),
                          kIOAudioControlChannelIDAll,
                          kIOAudioControlChannelNameAll,
                          EncodeControlID(0) );

        if (!selectorControl) break;

        // FIXME: localized strings?

        sourceID = kRecordSourceLineIn;
        if (isRecordSourceSupported(sourceID))
            selectorControl->addAvailableSelection(
            EncodeControlID(sourceID), "Line In");

        sourceID = kRecordSourceMIC;
        if (isRecordSourceSupported(sourceID))
            selectorControl->addAvailableSelection(
            EncodeControlID(sourceID), "MIC");

        sourceID = kRecordSourceCD;
        if (isRecordSourceSupported(sourceID))
            selectorControl->addAvailableSelection(
            EncodeControlID(sourceID), "CD");

        sourceID = kRecordSourceStereoMix;
        if (isRecordSourceSupported(sourceID))
            selectorControl->addAvailableSelection(
            EncodeControlID(sourceID), "Stereo Mix");

        sourceID = kRecordSourceMonoMix;
        if (isRecordSourceSupported(sourceID))
            selectorControl->addAvailableSelection(
            EncodeControlID(sourceID), "Mono Mix");

        selectorControl->setValueChangeHandler(recordSourceChanged, this);
        array->setObject( selectorControl );
        selectorControl->release();
        selectorControl = 0;

        ior = kIOReturnSuccess;
    }
    while (false);

    return ior;
}

//---------------------------------------------------------------------------

IOReturn CLASS::recordGainChanged( OSObject *       target,
                                   IOAudioControl * control,
                                   SInt32           oldValue,
                                   SInt32           newValue )
{
    CLASS * codec = (CLASS *) target;

    if (!codec || !control)
        return kIOReturnBadArgument;

    return codec->setRecordGainValue(
                     DecodeControlID(control->getControlID()),
                     newValue );
}

IOReturn CLASS::recordMuteChanged( OSObject *       target,
                                   IOAudioControl * control,
                                   SInt32           oldValue,
                                   SInt32           newValue )
{
    CLASS * codec = (CLASS *) target;

    if (!codec || !control)
        return kIOReturnBadArgument;

    return codec->setRecordGainMute( kAnalogChannelStereo, newValue );
}

IOReturn CLASS::recordSourceChanged( OSObject *       target,
                                     IOAudioControl * control,
                                     SInt32           oldValue,
                                     SInt32           newValue )
{
    CLASS * codec = (CLASS *) target;

    if (!codec || !control)
        return kIOReturnBadArgument;

    newValue = DecodeControlID(newValue);

    return codec->setRecordSourceSelection( newValue, newValue );
}

#pragma mark -
#pragma mark еее Audio Configuration (SPDIF) еее
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::prepareAudioConfigurationSPDIF( IOAC97AudioConfig * config )
{
    bool  ok;

    DebugLog("%s[ID %u]::%s(%p)\n",
             getName(), fCodecID, __FUNCTION__, config);

    // Secondary audio codecs are not used, but should not prevent
    // configurations from using the primary codec.

    if (!fIsPrimaryCodec)
        return kIOReturnSuccess;

    // Restricted to single codec configurations.

    if (config->getMergedCodecSlotMap())
    {
        DebugLog("%s[ID %u]::%s codec slot map not empty %lx\n",
                 getName(), fCodecID, __FUNCTION__,
                 config->getMergedCodecSlotMap());
        return kIOReturnUnsupported;
    }

    // Codec has SPDIF support?

    if ((fExtAudioID & kExtAudioID_SPDIF) == 0)
    {
        DebugLog("%s[ID %u]::%s SPDIF not supported\n",
                 getName(), fCodecID, __FUNCTION__);
        return kIOReturnUnsupported;
    }

    // 48K sample rate only.

    if (config->getSampleRate() != kIOAC97SampleRate48K)
    {
        DebugLog("%s[ID %u]::%s unsupported sample rate %lu\n",
                 getName(), fCodecID, __FUNCTION__,
                 config->getSampleRate());
        return kIOReturnUnsupported;
    }

    // Only supported mode is independent SPDIF on slots 10 & 11.

    ok = config->setCodecSlotMap( fCodecID, this, kIOAC97Slot_10_11 );
    if (ok != true)
    {
        DebugLog("%s[ID %u]::%s slot conflict\n",
                 getName(), fCodecID, __FUNCTION__);
        return kIOReturnNoChannels;
    }

    // Temporarily activate the configuration and see if the hardware
    // accepts the settings.

    if (activateAudioConfigurationSPDIF(config) != kIOReturnSuccess)
    {
        DebugLog("%s[ID %u]::%s codec rejected SPDIF configuration\n",
                 getName(), fCodecID, __FUNCTION__);
        return kIOReturnUnsupportedMode;
    }

    deactivateAudioConfigurationSPDIF(config);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::activateAudioConfigurationSPDIF( IOAC97AudioConfig * config )
{
    IOAC97CodecWord status;
    IOAC97CodecWord control;

    // Setup SPDIF on slots 10 & 11.

    fExtAudioStatus &= ~kExtAudioStatus_SPSA_MASK;
    fExtAudioStatus |= kExtAudioStatus_SPSA_10_11;
    codecWrite(kCodecExtAudioStatus, fExtAudioStatus);

    // Set up SPDIF control register.

    control = kSPDIFControl_SPSR_48K;

    if (config->getSampleFormat() == kIOAC97SampleFormatAC3)
    {
        control |= kSPDIFControl_NON_AUDIO;
    }
    
    codecWrite(kCodecSPDIFControl, control);
    
    // Now check if SPCV indicates if the SPDIF configuration is valid.
    
    status = codecRead(kCodecExtAudioStatus);
    DebugLog("%s[ID %u]::%s ExtAudioStatus = %04x SPDIFControl = %04x\n",
             getName(), fCodecID, __FUNCTION__, status, control);

    if ((status & kExtAudioStatus_SPCV) == 0)
    {
        DebugLog("%s[ID %u]::%s codec rejected SPDIF configuration\n",
                 getName(), fCodecID, __FUNCTION__);
        return kIOReturnUnsupportedMode;
    }

    // Enable SPDIF transmitter.

    fExtAudioStatus |= kExtAudioStatus_SPDIF;
    codecWrite(kCodecExtAudioStatus, fExtAudioStatus);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void CLASS::deactivateAudioConfigurationSPDIF( IOAC97AudioConfig * config )
{
    // Disable SPDIF transmitter

    if (fExtAudioStatus & kExtAudioStatus_SPDIF)
    {
        fExtAudioStatus &= ~kExtAudioStatus_SPDIF;
        codecWrite(kCodecExtAudioStatus, fExtAudioStatus);
    }
}

//---------------------------------------------------------------------------

IOReturn CLASS::createAudioControlsSPDIF( IOAC97AudioConfig * config,
                                          OSArray *           array )
{
    IOAudioSelectorControl * selectorControl;
 
    selectorControl = IOAudioSelectorControl::createOutputSelector(
                      /* initialValue */ kIOAudioOutputPortSubTypeSPDIF,
                      /* channelID    */ kIOAudioControlChannelIDAll);
    
    if (selectorControl)
    {
        selectorControl->addAvailableSelection(
                         kIOAudioOutputPortSubTypeSPDIF, "SPDIF Out");
        selectorControl->setValueChangeHandler(outputSelectorChanged, this);
        array->setObject( selectorControl );
        selectorControl->release();
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::message( UInt32 type, IOService * provider, void * argument )
{
    if (argument)
    {
        IOAC97MessageArgument * arg = (IOAC97MessageArgument *) argument;

        switch (type)
        {
            case kIOAC97MessagePrepareAudioConfiguration:
                return prepareAudioConfiguration(
                       (IOAC97AudioConfig *) arg->param[0]);
            
            case kIOAC97MessageActivateAudioConfiguration:
                return activateAudioConfiguration(
                       (IOAC97AudioConfig *) arg->param[0]);
            
            case kIOAC97MessageDeactivateAudioConfiguration:
                deactivateAudioConfiguration(
                       (IOAC97AudioConfig *) arg->param[0]);
                return kIOReturnSuccess;

            case kIOAC97MessageCreateAudioControls:
                return createAudioControls(
                       (IOAC97AudioConfig *) arg->param[0],
                       (OSArray *)           arg->param[1]);
        }
    }

    return super::message(type, provider, argument);
}

#pragma mark -
#pragma mark еее BIT_CLK Measurement еее
#pragma mark -

//---------------------------------------------------------------------------

UInt32 CLASS::measure48KSampleRate( IOAC97CodecConverter converter )
{
    IOAC97AudioConfig * config = 0;
    IOAC97Controller *  root;
    UInt32              startPosition;
    UInt32              finalPosition;
    AbsoluteTime        startTime;
    AbsoluteTime        finalTime;
    UInt64              elapsedTimeNS;
    UInt64              elapsedFrames;
    boolean_t           state;
    IOAC97DMAEngineID   engineID;
    const int           kNumChannels = 2;
    const int           kBytesPerSample = 2;
    UInt32              measuredRate = 0;

    // This is real messy but some machines appears to be mis-clocked,
    // and requires sample rate scaling based on what is measured here.

    IOAC97Assert(fCodecDevice != 0);
    root = fCodecDevice->getController();

    config = IOAC97AudioConfig::audioConfig(
                   kIOAC97DMAEngineTypeAudioPCM,
                   kIOAC97DMADataDirectionOutput,
                   kNumChannels,
                   kIOAC97SampleFormatPCM16,
                   kIOAC97SampleRate48K );

    if (!config ||
        root->prepareAudioConfiguration(config) != kIOReturnSuccess ||
        !config->isValid() ||
        root->activateAudioConfiguration(config) != kIOReturnSuccess)
    {    
        DebugLog("%s[ID %u]::%s audio config error\n",
                 getName(), fCodecID, __FUNCTION__);
        goto done;
    }

    // Start DMA engine

    engineID = config->getDMAEngineID();
    root->startDMAEngine( engineID );

    // [3224448]
    // Wait settles DMA hardware on some configurations,
    // this makes the measurement more reliable.

    IOSleep(20);

    // Get starting DMA engine position and time.

    state = ml_set_interrupts_enabled(FALSE);
    startPosition = root->getDMAEngineHardwarePointer( engineID );
    clock_get_uptime( &startTime );
    ml_set_interrupts_enabled(state);

    // Sampling interval. FIXME: adjust this based on buffer
    // size to avoid ring wrap around.

    IOSleep(50);

    // Get starting DMA engine position and time.

    state = ml_set_interrupts_enabled(FALSE);
    finalPosition = root->getDMAEngineHardwarePointer( engineID );
    clock_get_uptime( &finalTime );
    ml_set_interrupts_enabled(state);

    // Stop DMA and deactivate configuration.

    root->stopDMAEngine( engineID );
    root->deactivateAudioConfiguration(config);

    // Computed the measured sample rate when 48KHz rate was programmed.

    SUB_ABSOLUTETIME(&finalTime, &startTime);
    absolutetime_to_nanoseconds(finalTime, &elapsedTimeNS);
    elapsedFrames = ((finalPosition - startPosition) /
                     (kNumChannels * kBytesPerSample));

    if (elapsedTimeNS)
        measuredRate = (elapsedFrames * kSecondScale) / elapsedTimeNS;

    DebugLog("%s[ID %u]: measured 48K: %lu, pos (%lu-%lu), time %llu\n",
             getName(), fCodecID, measuredRate, startPosition, finalPosition,
             elapsedTimeNS);

done:
    if (config)
    {
        config->release();
        config = 0;
    }

    if (measuredRate == 0)
    {
        // failed, what shall we do?
        measuredRate = kIOAC97SampleRate48K;
    }

    return measuredRate;
}

#pragma mark -
#pragma mark еее Jack Sense еее
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::performJackSense( IOOptionBits      senseFunction,
                                  IOOptionBits      senseTipOrRing,
                                  IOAC97CodecWord * senseInfo,
                                  IOAC97CodecWord * senseDetail,
                                  UInt32 *          senseResult )
{
    static const int SenseOrder[4] = { 1, 10, 100, 1000 };

    IOAC97CodecWord word;
    IOAC97CodecWord reg66, reg68, reg6a;
    IOReturn        ret;

    DebugLog("%s[ID %u] %s func = %x, tip = %x\n", getName(),
             fCodecID, __FUNCTION__, senseFunction, senseTipOrRing);

    if (!senseInfo || !senseDetail)
        return kIOReturnBadArgument;

    if (!fIsPage1Supported)
        return kIOReturnUnsupported;

    *senseInfo = *senseDetail = 0;

    // Switch to page 1.

    codecWrite(kCodecInterruptAndPage, kCodecRegisterPage1);

    // Select the audio function.

    reg66 = ((senseFunction & 0xF) << 1) | (senseTipOrRing & 1);
    codecWrite(kCodecSenseFunctionSelect, reg66);

    // Read back the function select register to check support.

    if ((word = codecRead(kCodecSenseFunctionSelect)) != reg66)
    {
        DebugLog("%s[ID %u] unsupported sense function %04x (%04x)\n",
                 getName(), fCodecID, reg66, word);
        ret = kIOReturnUnsupported;
        goto done;
    }

    // Write 1 to IV bit to clear it.

    reg68 = codecRead(kCodecSenseInformation);
    codecWrite(kCodecSenseInformation, reg68 | kSenseInformation_IV);

    // Clear interrupt status and start sense cycle.

    codecWrite(kCodecInterruptAndPage,
               (1 << 15) | (1 << 12) | kCodecRegisterPage1);

    // Wait for completion.

    ret = kIOReturnTimeout;
    for (int i = 0; i < 10; i++)
    {
        IOSleep(10);
        if ((word = codecRead(kCodecInterruptAndPage)) & (1 << 15))
        {
            DebugLog("%s[ID %u] sense cycle completed in %u (%04x)\n",
                     getName(), fCodecID, i, word);
            ret = kIOReturnSuccess;
            break;
        }
    }

    if (ret == kIOReturnTimeout)
    {
        DebugLog("%s[ID %u] sense cycle timed out (%04x)\n",
                 getName(), fCodecID, codecRead(kCodecInterruptAndPage));
    }

    if (ret == kIOReturnSuccess)
    {
        reg66 = codecRead(kCodecSenseFunctionSelect);
        reg68 = codecRead(kCodecSenseInformation);
        reg6a = codecRead(kCodecSenseDetails);

        DebugLog("%s[ID %u] sense cycle done %04x %04x %04x\n",
                 getName(), fCodecID, reg66, reg68, reg6a);
    
        // Check Information Valid bit to make sure its set.

        if (reg68 & kSenseInformation_IV)
        {
            *senseInfo   = reg68;
            *senseDetail = reg6a;
            *senseResult = (reg6a & kSenseDetail_SR_MASK) *
                           SenseOrder[(reg6a & kSenseDetail_OR_MASK) >> 
                                       kSenseDetail_OR_SHIFT];
        }
        else
            ret = kIOReturnUnsupported;
    }

done:
    codecWrite(kCodecInterruptAndPage, kCodecRegisterPage0);
    return ret;
}

#pragma mark -
#pragma mark еее Power Management еее
#pragma mark -

//---------------------------------------------------------------------------

#define kIdleStatePowerdownBits \
    (kPowerdown_PR0 | kPowerdown_PR1 | kPowerdown_PR6 | kPowerdown_EAPD)

IOReturn CLASS::raisePowerState( UInt32 newPowerState )
{
    DebugLog("%s::%s state = %lu\n", getName(), __FUNCTION__, newPowerState);

    switch (newPowerState)
    {
        case kPowerStateIdle:  /* resume from system doze/sleep */

            // Assume a warm or cold reset has been issued on the AC-Link 
            // and this codec has asserted its ready flag.

            resetHardware();

            primeHardware();

            if (!fIsPrimaryCodec)
                break;

            // Back to idle state.
            // Turn off all DAC/ADC and analog amplifiers. Optional
            // DAC/ADC should be powered off since all audio configs
            // are deactivated before sleeping.
    
            codecWrite( kCodecPowerdown, kIdleStatePowerdownBits );
            break;

        case kPowerStateActive:  /* resume from audio driver idle */

            if (!fIsPrimaryCodec)
                break;

            // Make sure analog mixer is ready before writing to volume
            // control registers.

            waitForBits( kCodecPowerdown, kPowerdown_ANL );

            // Transition back to full operation and max power consumption.
            // Power up all everything except DAC/ADC.

            codecWrite(  kCodecPowerdown,
                         kPowerdown_PR0 | kPowerdown_PR1 | kPowerdown_EAPD);

            waitForBits( kCodecPowerdown, kPowerdown_ANL | kPowerdown_REF );

            // Power up all subsections.

            codecWrite(  kCodecPowerdown, 0 );

            waitForBits( kCodecPowerdown,
                         kPowerdown_ADC | kPowerdown_DAC |
                         kPowerdown_ANL | kPowerdown_REF );

            // Power up optional converters that were active before idle.

            if (fExtAudioStatusReadyMask)
            {
                codecWrite( kCodecExtAudioStatus, fExtAudioStatus);
                waitForBits(kCodecExtAudioStatus, fExtAudioStatusReadyMask);
                DebugLog("%s: waiting for opt DACs %04x %04x\n", getName(),
                         fExtAudioStatus, fExtAudioStatusReadyMask);
            }

            break;

        default:
            IOAC97Assert(false);
            break;
    }

    fCurrentPowerState = newPowerState; 

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::lowerPowerState( UInt32 newPowerState )
{
    IOAC97CodecWord  pd;

    DebugLog("%s::%s state = %lu\n", getName(), __FUNCTION__, newPowerState);

    switch (newPowerState)
    {
        case kPowerStateIdle:

            // Power off optional Center/LFE/Surround/MIC converters.

            if (fExtAudioStatusReadyMask)
            {
                IOAC97CodecWord idleStatus = fExtAudioStatus;

                idleStatus |= (kExtAudioStatus_PRI | kExtAudioStatus_PRJ |
                               kExtAudioStatus_PRK);

                codecWrite( kCodecExtAudioStatus, idleStatus);
                DebugLog("%s: powered off opt DACs %04x\n", getName(),
                         idleStatus);
            }

            // Turn off all DAC/ADC and analog amplifiers.

            codecWrite( kCodecPowerdown, kIdleStatePowerdownBits );
            break;

        case kPowerStateSleep:

            IOAC97Assert(fExtAudioStatusReadyMask == 0);

            pd = kIdleStatePowerdownBits;

            // Power down analog subsections.
            //
            // 3558627: Do not turn off the voltage reference on the analog
            // mixer by adding the kPowerdownMixerVrefOff flag.  Doing this
            // will reduce the power usage while dozing, but a loud pop can
            // be heard on wake for some systems (not all).

            pd |= (kPowerdown_PR2 /*| kPowerdown_PR3*/);
            codecWrite( kCodecPowerdown, pd );

            // Power down digital interface.

            pd |= kPowerdown_PR4;
            codecWrite( kCodecPowerdown, pd );

            // Power down internal clocks.

            pd |= kPowerdown_PR5;
            codecWrite( kCodecPowerdown, pd );

            // AC97 requires a minimum of 4 audio frame times before the
            // AC-link is brought back up (section 3.5.2 in 2.3 spec).
            // Add some time for the posted write to reach the codec.

            IOSleep( 20 );
            break;

        default:
            IOAC97Assert(false);
            break;
    }

    fCurrentPowerState = newPowerState; 

    return kIOReturnSuccess;
}
