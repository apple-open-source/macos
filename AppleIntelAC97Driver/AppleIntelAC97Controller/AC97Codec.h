/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#ifndef __APPLE_INTEL_AC97_CODEC_H
#define __APPLE_INTEL_AC97_CODEC_H

#include <IOKit/IOService.h>
#include "AC97Defines.h"

//
// Property table keys.
//
#define kCodecIDKey              "Codec ID"
#define kCodecFunctionKey        "Codec Function"
#define kResetRegisterKey        "Reset Register"
#define kExtAudioIDRegisterKey   "Extended Audio ID"
#define kPNPVendorIDKey          "PNP Vendor ID"

//
// AUX output configurations.
//
enum {
    kAuxOutModeLineOut,
    kAuxOutModeHeadphoneOut,
    kAuxOutMode4ChannelOut
};

class AppleIntelAC97Controller;

class AppleIntelAC97Codec : public IOService
{
    OSDeclareDefaultStructors( AppleIntelAC97Codec )

protected:
    AppleIntelAC97Controller *  _controller;
    CodecID                     _codecID;
    void *                      _codecParam;
    UInt16                      _codecRegs[ kCodecRegisterCount ];
    UInt8                       _auxOutputMode;
    UInt8                       _masterVolumeBitCount;

    virtual bool     probeAudioCodec();
    virtual bool     probeModemCodec();

public:
    // Codec creation and destruction.

    static AppleIntelAC97Codec *
                     codec(AppleIntelAC97Controller * controller,
                           CodecID                    codecID,
                           void *                     codecParam = 0 );

    virtual bool     init( AppleIntelAC97Controller * controller,
                           CodecID                    codecID,
                           void *                     codecParam = 0 );

    virtual void     free();

    // Handle client open/close.

    virtual bool     handleOpen( IOService *  client,
                                 IOOptionBits options,
                                 void *       arg );

    virtual void     handleClose( IOService *  client,
                                  IOOptionBits options );

    // AC97 codec introspection.

    inline CodecID   getCodecID() const
    { return _codecID; }

    inline void *    getCodecParam() const
    { return _codecParam; }

    inline AppleIntelAC97Controller * getController() const
    { return _controller; }

    // Raw codec register access.

    virtual UInt16   _readRegister( UInt8 offset );

    virtual IOReturn _writeRegister( UInt8 offset, UInt16 value );

    // Cached codec register access.

    virtual UInt16   readRegister( UInt8 offset ) const;

    virtual IOReturn writeRegister( UInt8 offset, UInt16 value );

    // Audio codec functions.

    virtual IOFixed  getOutputVolumeMinDB() const;

	virtual IOFixed  getOutputVolumeMaxDB() const;

    virtual UInt32   getOutputVolumeMin() const;
    
    virtual UInt32   getOutputVolumeMax() const;

    virtual void     setOutputVolumeLeft( UInt16 volume );

    virtual void     setOutputVolumeRight( UInt16 volume );

    virtual void     setOutputVolumeMute( bool isMute );

    virtual IOReturn setDACSampleRate( UInt32 rate, UInt32 * actualRate = 0 );

    virtual UInt32   getDACSampleRate() const;

    // DMA channel status and control.

    virtual IOReturn setDescriptorBaseAddress( DMAChannel        channel,
                                               IOPhysicalAddress baseAddress );

    virtual void     setLastValidIndex( DMAChannel channel, UInt8 index );

    virtual UInt8    getCurrentIndexValue( DMAChannel channel ) const;

    virtual UInt32   getCurrentBufferPosition( DMAChannel channel,
                                               UInt8 *    index ) const;

    virtual IOReturn startDMAChannel( DMAChannel channel );

    virtual void     stopDMAChannel( DMAChannel channel );

    virtual bool     serviceChannelInterrupt( DMAChannel channel );
};

#endif /* !__APPLE_INTEL_AC97_CODEC_H */
