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

#ifndef __IOAC97AUDIOCONFIG_H
#define __IOAC97AUDIOCONFIG_H

#include <libkern/c++/OSObject.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "IOAC97Types.h"

class  IOAC97AudioCodec;
class  IOAudioControl;
struct AudioCodecInfo;
struct AudioEngineInfo;

class IOAC97AudioConfig : public OSObject
{
    OSDeclareDefaultStructors( IOAC97AudioConfig )

protected:
    IOOptionBits      fDMAEngineType;
    IOOptionBits      fDMADataDirection;
    IOItemCount       fNumChannels;
    IOOptionBits      fSampleFormat;
    UInt32            fSampleRate;
    AudioCodecInfo *  fCodecArray;
    AudioEngineInfo * fEngineInfo;
    void *            fClientData;

    virtual bool               validateCodecSlotMap(
                                       IOAC97CodecID codecID,
                                       IOOptionBits  newMap );

public:
    static IOAC97AudioConfig * audioConfig(
                                       IOOptionBits engineType,
                                       IOOptionBits dataDirection,
                                       IOItemCount  numChannels,
                                       IOOptionBits sampleFormat,
                                       UInt32       sampleRate );

    virtual bool               initAudioConfig(
                                       IOOptionBits engineType,
                                       IOOptionBits dataDirection,
                                       IOItemCount  numChannels,
                                       IOOptionBits sampleFormat,
                                       UInt32       sampleRate );

    virtual void               free( void );

    virtual IOOptionBits       getDMAEngineType( void ) const;

    virtual IOOptionBits       getDMADataDirection( void ) const;

    virtual IOItemCount        getAudioChannelCount( void ) const;

    virtual IOOptionBits       getSampleFormat( void ) const;

    virtual UInt32             getSampleRate( void ) const;

    virtual void               setClientData( void * data );

    virtual void *             getClientData( void ) const;

    // DMA Engine Configuration

    virtual void               setDMAEngineID( IOAC97DMAEngineID engine );

    virtual IOAC97DMAEngineID  getDMAEngineID( void ) const;

    virtual void               setDMABufferCount( IOItemCount count );

    virtual IOItemCount        getDMABufferCount( void ) const;

    virtual void               setDMABufferSize( IOByteCount size );

    virtual IOByteCount        getDMABufferSize( void ) const;

    virtual void               setDMABufferMemory(
                                       IOBufferMemoryDescriptor * memory );

    virtual IOBufferMemoryDescriptor *
                               getDMABufferMemory( void ) const;

    virtual void               setDMAEngineSlotMaps(
                                       IOOptionBits * maps,
                                       IOItemCount    count );

    // Audio Codec Configuration

    virtual bool               setCodecSlotMap(
                                       IOAC97CodecID      codecID,
                                       IOAC97AudioCodec * codecPtr,
                                       IOOptionBits       slotMap );

    virtual IOOptionBits       getCodecSlotMap(
                                       IOAC97CodecID codecID ) const;

    virtual IOOptionBits       getMergedCodecSlotMap( void ) const;

    // Validate Configuration

    virtual bool               isValid( void ) const;

    virtual IOOptionBits       getSlotMap( void ) const;
};

#endif /* !__IOAC97AUDIOCONFIG_H */
