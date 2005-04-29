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
#include "IOAC97AudioConfig.h"
#include "IOAC97AudioCodec.h"
#include "IOAC97Debug.h"

#define kMaxValidSlotMaps  4

struct AudioEngineInfo {
    IOAC97DMAEngineID          engineID;
    IOItemCount                bufferCount;
    IOByteCount                bufferSize;
    IOBufferMemoryDescriptor * bufferMemory;
    IOOptionBits               validSlotMaps[kMaxValidSlotMaps];
};

struct AudioCodecInfo {
    IOAC97AudioCodec * codec;
    IOOptionBits       slotMap;
};

#define CLASS IOAC97AudioConfig
#define super OSObject
OSDefineMetaClassAndStructors( IOAC97AudioConfig, OSObject )

//---------------------------------------------------------------------------

CLASS * CLASS::audioConfig( IOOptionBits  engineType,
                            IOOptionBits  dataDirection,
                            IOItemCount   numChannels,
                            IOOptionBits  sampleFormat,
                            UInt32        sampleRate )
{
    CLASS * config = new CLASS;

    if (config && !config->initAudioConfig( engineType,
        dataDirection, numChannels, sampleFormat, sampleRate ))
    {
        config->release();
        config = 0;
    }

    return config;
}

//---------------------------------------------------------------------------

bool CLASS::initAudioConfig( IOOptionBits  engineType,
                             IOOptionBits  dataDirection,
                             IOItemCount   numChannels,
                             IOOptionBits  sampleFormat,
                             UInt32        sampleRate )
{
    if (super::init() == false) return false;

    if (dataDirection != kIOAC97DMADataDirectionOutput &&
        dataDirection != kIOAC97DMADataDirectionInput)
        return false;

    if (numChannels == 0 || sampleRate == 0)
        return false;

    fDMAEngineType    = engineType;
    fDMADataDirection = dataDirection;
    fNumChannels      = numChannels;
    fSampleFormat     = sampleFormat;
    fSampleRate       = sampleRate;

    // Allocate storage for codecs

    fCodecArray = IONew(AudioCodecInfo, kIOAC97MaxCodecCount);
    if (!fCodecArray)
        return false;

    bzero(fCodecArray, sizeof(AudioCodecInfo) * kIOAC97MaxCodecCount);

    // Allocate storage for engine

    fEngineInfo = IONew(AudioEngineInfo, 1);
    if (!fEngineInfo)
        return false;

    bzero(fEngineInfo, sizeof(*fEngineInfo));

    return true;
}

//---------------------------------------------------------------------------

void CLASS::free( void )
{
    if (fCodecArray)
    {
        for (int i = 0; i < kIOAC97MaxCodecCount; i++)
        {
            if (fCodecArray[i].codec)
            {
                fCodecArray[i].codec->release();
                fCodecArray[i].codec = 0;
            }
        }
        IODelete(fCodecArray, AudioCodecInfo, kIOAC97MaxCodecCount);
        fCodecArray = 0;
    }

    if (fEngineInfo)
    {
        IODelete(fEngineInfo, AudioEngineInfo, 1);
        fEngineInfo = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------

IOOptionBits CLASS::getDMAEngineType( void ) const
{
    return fDMAEngineType;
}

IOOptionBits CLASS::getDMADataDirection( void ) const
{
    return fDMADataDirection;
}

IOItemCount CLASS::getAudioChannelCount( void ) const
{
    return fNumChannels;
}

IOOptionBits CLASS::getSampleFormat( void ) const
{
    return fSampleFormat;
}

UInt32 CLASS::getSampleRate( void ) const
{
    return fSampleRate;
}

void CLASS::setClientData( void * data )
{
    fClientData = data;
}

void * CLASS::getClientData( void ) const
{
    return fClientData;
}

//---------------------------------------------------------------------------

void CLASS::setDMAEngineID( IOAC97DMAEngineID engine )
{
    fEngineInfo->engineID = engine;
}

IOAC97DMAEngineID  CLASS::getDMAEngineID( void ) const
{
    return fEngineInfo->engineID;
}

void CLASS::setDMABufferCount( IOItemCount count )
{
    fEngineInfo->bufferCount = count;
}

IOItemCount CLASS::getDMABufferCount( void ) const
{
    return fEngineInfo->bufferCount;
}

void CLASS::setDMABufferSize( IOByteCount size )
{
    fEngineInfo->bufferSize = size;
}

IOByteCount CLASS::getDMABufferSize( void ) const
{
    return fEngineInfo->bufferSize;
}

void CLASS::setDMABufferMemory( IOBufferMemoryDescriptor * memory )
{
    fEngineInfo->bufferMemory = memory;
}

IOBufferMemoryDescriptor * CLASS::getDMABufferMemory( void ) const
{
    return fEngineInfo->bufferMemory;
}

void CLASS::setDMAEngineSlotMaps( IOOptionBits * maps, IOItemCount count )
{
    if (maps && count)
    {
        for (unsigned int i = 0; i < count; i++)
        {
            DebugLog("IOAC97AudioConfig[%p] Valid slots %u = %08lx\n",
                     this, i, maps[i]);
        }

        memcpy( fEngineInfo->validSlotMaps,
                maps,
                sizeof(*maps) * min(kMaxValidSlotMaps, count) );

        if (count < kMaxValidSlotMaps)
        {
            memset( &fEngineInfo->validSlotMaps[count],
                    0, sizeof(*maps) * (kMaxValidSlotMaps - count));
        }
    }
}

//---------------------------------------------------------------------------

bool CLASS::validateCodecSlotMap( IOAC97CodecID codecID, IOOptionBits newMap )
{
    IOOptionBits mergeMap = 0;

    // Slots cannot conflict with those from another codec.

    for (unsigned int i = 0; i < kIOAC97MaxCodecCount; i++)
    {
        if (i == codecID) continue;
        mergeMap |= getCodecSlotMap(i);
    }

    if (mergeMap & newMap)
    {
        return false;
    }

    // Is this (partial) map supported by the DMA engine?

    mergeMap |= newMap;

    for (int i = 0; i < kMaxValidSlotMaps; i++)
    {
        if ((fEngineInfo->validSlotMaps[i] & mergeMap) == mergeMap)
        {
            return true;
        }
    }
    
    return false;
}

bool CLASS::setCodecSlotMap( IOAC97CodecID      codecID,
                             IOAC97AudioCodec * codecPtr,
                             IOOptionBits       slotMap )
{
    if (!codecPtr || !slotMap ||
        (codecID >= kIOAC97MaxCodecCount) ||
        fCodecArray[codecID].codec ||
        !validateCodecSlotMap(codecID, slotMap))
    {
        return false;
    }

    codecPtr->retain();
    fCodecArray[codecID].codec = codecPtr;
    fCodecArray[codecID].slotMap = slotMap;

    return true;
}

IOOptionBits CLASS::getCodecSlotMap( IOAC97CodecID codecID ) const
{
    if (codecID >= kIOAC97MaxCodecCount)
        return 0;
    else
        return fCodecArray[codecID].slotMap;
}

IOOptionBits CLASS::getMergedCodecSlotMap( void ) const
{
    IOOptionBits merge = 0;

    for (int i = 0; i < kIOAC97MaxCodecCount; i++)
        merge |= getCodecSlotMap(i);
    
    return merge;
}

//---------------------------------------------------------------------------

bool CLASS::isValid( void ) const
{
    return (getSlotMap() != 0);
}

IOOptionBits CLASS::getSlotMap( void ) const
{
    IOOptionBits codecSlotMap;
    IOOptionBits mergeSlotMap = 0;

    codecSlotMap = getMergedCodecSlotMap();

    for (int i = 0; i < kMaxValidSlotMaps; i++)
    {
        if (fEngineInfo->validSlotMaps[i] == codecSlotMap)
        {
            mergeSlotMap = codecSlotMap;
            break;
        }
    }
    
    return mergeSlotMap;
}
