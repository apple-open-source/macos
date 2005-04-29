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

#ifndef __IOAC97AUDIOCODEC_H
#define __IOAC97AUDIOCODEC_H

#include "IOAC97CodecDevice.h"
#include "IOAC97AudioConfig.h"

class IOAC97AudioCodec : public IOService
{
    OSDeclareDefaultStructors( IOAC97AudioCodec )

protected:
    IOAC97CodecDevice *  fCodecDevice;
    IOAC97CodecID        fCodecID;
    IOItemCount          fMasterVolumeBitCount;
    UInt32               fVendorID;

    enum {
        kAuxOutFunctionLineLevelOut,
        kAuxOutFunctionHeadphoneOut,
        kAuxOutFunction4ChannelOut
    };

    IOOptionBits         fAuxOutFunction;
    bool                 fAuxOutVolumeSupport;
    IOOptionBits         fAnalogOutputSupportMask;
    IOOptionBits         fAnalogSourceSupportMask;    
    IOItemCount          fNumPCMOutDAC;
    UInt32               fMeasured48KRate;
    bool                 fIsPrimaryCodec;
    bool                 fIsPage1Supported;
    IOAC97CodecWord      fExtAudioID;
    IOAC97CodecWord      fExtAudioStatus;
    IOAC97CodecWord      fExtAudioStatusReadyMask;
    UInt32               fCurrentPowerState;

    static IOReturn            volumeControlChanged(
                                       OSObject *       target,
                                       IOAudioControl * control,
                                       SInt32           oldValue,
                                       SInt32           newValue );

    static IOReturn            muteControlChanged(
                                       OSObject *       target,
                                       IOAudioControl * control,
                                       SInt32           oldValue,
                                       SInt32           newValue );

    static IOReturn            recordGainChanged(
                                       OSObject *       target,
                                       IOAudioControl * control,
                                       SInt32           oldValue,
                                       SInt32           newValue );

    static IOReturn            recordMuteChanged(
                                       OSObject *       target,
                                       IOAudioControl * control,
                                       SInt32           oldValue,
                                       SInt32           newValue );

    static IOReturn            recordSourceChanged(
                                       OSObject *       target,
                                       IOAudioControl * control,
                                       SInt32           oldValue,
                                       SInt32           newValue );

    static IOReturn            outputSelectorChanged(
                                       OSObject *       target,
                                       IOAudioControl * control,
                                       SInt32           oldValue,
                                       SInt32           newValue );

    virtual bool               resetHardware( void );

    virtual bool               probeHardwareFeatures( void );

    virtual bool               primeHardware( void );

    virtual bool               waitForBits(
                                       IOAC97CodecOffset offset,
                                       IOAC97CodecWord   bits );

    virtual void               probeAuxiliaryOutSupport( void );

    virtual void               probeAnalogOutputSupport( void );

    virtual void               probeAnalogSourceSupport( void );

    virtual IOReturn           prepareAudioConfigurationPCMOut(
                                       IOAC97AudioConfig * config );

    virtual IOReturn           activateAudioConfigurationPCMOut(
                                       IOAC97AudioConfig * config );

    virtual void               deactivateAudioConfigurationPCMOut(
                                       IOAC97AudioConfig * config );

    virtual IOReturn           createAudioControlsPCMOut(
                                       IOAC97AudioConfig * config,
                                       OSArray *           array );

    virtual IOReturn           prepareAudioConfigurationPCMIn(
                                       IOAC97AudioConfig * config );

    virtual IOReturn           activateAudioConfigurationPCMIn(
                                       IOAC97AudioConfig * config );

    virtual void               deactivateAudioConfigurationPCMIn(
                                       IOAC97AudioConfig * config );

    virtual IOReturn           createAudioControlsPCMIn(
                                       IOAC97AudioConfig * config,
                                       OSArray *           array );

    virtual IOReturn           prepareAudioConfigurationSPDIF(
                                       IOAC97AudioConfig * config );

    virtual IOReturn           activateAudioConfigurationSPDIF(
                                       IOAC97AudioConfig * config );

    virtual void               deactivateAudioConfigurationSPDIF(
                                       IOAC97AudioConfig * config );

    virtual IOReturn           createAudioControlsSPDIF(
                                       IOAC97AudioConfig * config,
                                       OSArray *           array );

    virtual UInt32             measure48KSampleRate(
                                       IOAC97CodecConverter converter );

public:
    virtual bool               start( IOService * provider );

    virtual void               free( void );

    virtual bool               handleOpen(
                                       IOService *  client,
                                       IOOptionBits options,
                                       void *       arg );

    virtual void               handleClose(
                                       IOService *  client,
                                       IOOptionBits options );

    // ADC and DAC Control (FIXME DigitalConverter?)

    enum {
        kConverterPCMFront = 0,
        kConverterPCMSurround,
        kConverterPCMCenter,
        kConverterPCMLFE,
        kConverterPCMIn,
        kConverterMIC,
        kConverterLast
    };

    virtual bool               isConverterSupported(
                                       IOAC97CodecConverter converter );

    virtual bool               isConverterSampleRateSupported(
                                       IOAC97CodecConverter converter,
                                       UInt32               sampleRate );

    virtual IOReturn           setConverterSampleRate(
                                       IOAC97CodecConverter converter,
                                       UInt32               sampleRate );

    virtual IOReturn           getConverterSampleRate(
                                       IOAC97CodecConverter converter,
                                       UInt32 *             sampleRate );

    // Analog Output

    enum {
        kAnalogOutputLineOut = 0,
        kAnalogOutputAuxOut,
        kAnalogOutputSurround,
        kAnalogOutputMonoOut,
        kAnalogOutputCenter,
        kAnalogOutputLFE,
        kAnalogOutputLast
    };

    enum {
        kAnalogChannelRight  = 0x1,
        kAnalogChannelLeft   = 0x2,
        kAnalogChannelStereo = 0x3
    };

    virtual bool               isAnalogOutputSupported(
                                       IOAC97AnalogOutput  output );

    virtual IOReturn           getAnalogOutputVolumeRange(
                                       IOAC97AnalogOutput  output,
                                       IOAC97VolumeValue * minValue,
                                       IOAC97VolumeValue * maxValue,
                                       IOFixed *           minDB,
                                       IOFixed *           maxDB );

    virtual IOReturn           setAnalogOutputVolume(
                                       IOAC97AnalogOutput  output,
                                       IOAC97AnalogChannel channel,
                                       IOAC97VolumeValue   volume );

    virtual IOReturn           setAnalogOutputMute(
                                       IOAC97AnalogOutput  output,
                                       IOAC97AnalogChannel channel,
                                       bool                isMute );

    virtual IOAC97CodecWord    getAnalogOutputMuteBitMask(
                                       IOAC97AnalogOutput  output,
                                       IOAC97AnalogChannel channel );

    // Analog Source

    enum {
        kAnalogSourcePCBeep = 0,
        kAnalogSourcePhone,
        kAnalogSourceMIC1,
        kAnalogSourceMIC2,
        kAnalogSourceLineIn,
        kAnalogSourceCD,
        kAnalogSourceVideo,
        kAnalogSourceAuxIn,
        kAnalogSourcePCMOut,
        kAnalogSourceLast
    };

    virtual bool               isAnalogSourceSupported(
                                       IOAC97AnalogSource  source );

    virtual IOReturn           getAnalogSourceVolumeRange(
                                       IOAC97AnalogSource  source,
                                       IOAC97VolumeValue * minValue,
                                       IOAC97VolumeValue * maxValue,
                                       IOFixed *           minDB,
                                       IOFixed *           maxDB );

    virtual IOReturn           setAnalogSourceVolume(
                                       IOAC97AnalogSource  source,
                                       IOAC97AnalogChannel channel,
                                       IOAC97VolumeValue   volume );

    virtual IOReturn           setAnalogSourceMute(
                                       IOAC97AnalogSource  source,
                                       IOAC97AnalogChannel channel,
                                       bool                isMute );

    virtual IOAC97CodecWord    getAnalogSourceMuteBitMask(
                                       IOAC97AnalogSource  source,
                                       IOAC97AnalogChannel channel );

    // Record Source Selection

    enum {
        kRecordSourceMIC = 0,
        kRecordSourceCD,
        kRecordSourceVideo,
        kRecordSourceAuxIn,
        kRecordSourceLineIn,
        kRecordSourceStereoMix,
        kRecordSourceMonoMix,
        kRecordSourcePhone,
    };

    virtual bool               isRecordSourceSupported(
                                       IOAC97RecordSource source );

    virtual IOReturn           setRecordSourceSelection(
                                       IOAC97RecordSource sourceLeft,
                                       IOAC97RecordSource sourceRight );

    // Record Gain

    virtual IOReturn           getRecordGainRange(
                                       IOAC97GainValue * minValue,
                                       IOAC97GainValue * maxValue,
                                       IOFixed *         minDB,
                                       IOFixed *         maxDB );

    virtual IOReturn           setRecordGainValue(
                                       IOAC97AnalogChannel analogChannel,
                                       IOAC97GainValue     gainValue );

    virtual IOReturn           setRecordGainMute(
                                       IOAC97AnalogChannel analogChannel,
                                       bool                isMute );

    virtual IOAC97CodecWord    getRecordGainMuteBitMask(
                                       IOAC97AnalogChannel analogChannel );

    // Codec Access

    virtual IOAC97CodecWord    codecRead(
                                       IOAC97CodecOffset offset,
                                       UInt32            page = 0 );

    virtual void               codecWrite(
                                       IOAC97CodecOffset offset,
                                       IOAC97CodecWord   word,
                                       UInt32            page = 0 );

    virtual bool               isRegisterPageSupported(
                                       UInt32 page );

    virtual IOAC97CodecID      getCodecID( void ) const;

    virtual void *             getCodecParameter( void ) const;

    virtual IOAC97Controller * getAudioController( void ) const;

    // Jack Sensing

    enum {
        kSenseFunctionDAC1     = 0,
        kSenseFunctionDAC2     = 1,
        kSenseFunctionDAC3     = 2,
        kSenseFunctionSPDIFOut = 3,
        kSenseFunctionPhoneIn  = 4,
        kSenseFunctionMIC1     = 5,
        kSenseFunctionMIC2     = 6,
        kSenseFunctionLineIn   = 7,
        kSenseFunctionCDIn     = 8,
        kSenseFunctionVideoIn  = 9,
        kSenseFunctionAuxIn    = 10,
        kSenseFunctionMonoOut  = 11
    };

    enum {
        kSenseFromTip  = 0,
        kSenseFromRing = 1
    };

    virtual IOReturn           performJackSense(
                                       IOOptionBits      senseFunction,
                                       IOOptionBits      senseTipOrRing,
                                       IOAC97CodecWord * senseInfo,
                                       IOAC97CodecWord * senseDetail,
                                       UInt32 *          senseResult );

    // Codec Power Management

    enum {
        kPowerStateSleep = 0,
        kPowerStateIdle,
        kPowerStateActive
    };

    virtual IOReturn           raisePowerState( UInt32 newPowerState );

    virtual IOReturn           lowerPowerState( UInt32 newPowerState );

    // Audio Configuration

    virtual IOReturn           prepareAudioConfiguration(
                                       IOAC97AudioConfig * config );

    virtual IOReturn           activateAudioConfiguration(
                                       IOAC97AudioConfig * config );

    virtual void               deactivateAudioConfiguration(
                                       IOAC97AudioConfig * config );

    virtual IOReturn           createAudioControls(
                                       IOAC97AudioConfig * config,
                                       OSArray *           array );

    virtual IOReturn           message( UInt32      type,
                                        IOService * provider,
                                        void *      argument = 0 );
};

#endif /* !__IOAC97AUDIOCODEC_H */
