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
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOCDBlockStorageDriver.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOCDAudioControl.h>
#include <IOKit/storage/IOCDBlockStorageDevice.h>
#include <libkern/OSByteOrder.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#define	super	IOBlockStorageDriver
OSDefineMetaClassAndStructors(IOCDBlockStorageDriver,IOBlockStorageDriver)

static char * strclean(char * s)
{
    //
    // strclean() trims any spaces at either end of the string, strips any
    // control characters within the string, and collapses any sequence of
    // spaces within the string into a single space.
    //
 
    int sourceIndex = 0, targetIndex = 0, targetLength = 0;

    for ( ; s[sourceIndex] > '\0' && s[sourceIndex] <= ' '; sourceIndex++ );

    for ( ; s[sourceIndex]; sourceIndex++ )
    {
        if ( s[sourceIndex] < '\0' || s[sourceIndex] >= ' ' )
        {
            if ( s[sourceIndex] != ' ' )
            {
                if ( targetLength < targetIndex )
                {
                    targetIndex = targetLength + 1;
                }

                targetLength = targetIndex + 1;
            }

            s[targetIndex++] = s[sourceIndex];
        }

    }

    s[targetLength] = '\0';

    return s;
}

IOCDBlockStorageDevice *
IOCDBlockStorageDriver::getProvider() const
{
    return (IOCDBlockStorageDevice *) IOService::getProvider();
}


/* Accept a new piece of media, doing whatever's necessary to make it
 * show up properly to the system. The arbitration lock is assumed to
 * be held during the call.
 */
IOReturn
IOCDBlockStorageDriver::acceptNewMedia(void)
{
    IOReturn result;
    bool ok;
    int i;
    UInt64 nblocks;
    int nentries;
    int nDataTracks;
    int nAudioTracks;
    char name[128];
    bool nameSep;

    /* First, we cache information about the tracks on the disc: */
    
    result = cacheTocInfo();
    if (result != kIOReturnSuccess) {
        assert(_toc == NULL);
    }

    /* Scan thru the track list, counting up the number of Data and Audio tracks. */
    
    nDataTracks = 0;
    nAudioTracks = 0;
    nblocks = 0;

    _minBlockNumberAudio = 0xFFFFFFFF;
    _maxBlockNumberAudio = 0xFFFFFFFF;

    if (_toc) {
        nentries = (_toc->length - sizeof(UInt16)) / sizeof(CDTOCDescriptor);

        for (i = 0; i < nentries; i++) {   
            UInt32 lba = CDConvertMSFToLBA(_toc->descriptors[i].p);
            /* tracks 1-99, not leadout or skip intervals */
            if (_toc->descriptors[i].point <= 99 && _toc->descriptors[i].adr == 1) {
                if ((_toc->descriptors[i].control & 0x04)) {
                    /* it's a data track */
                    nDataTracks++;
                    _maxBlockNumberAudio = min(_maxBlockNumberAudio, lba ? (lba - 1) : 0);
                } else {
                    nAudioTracks++;
                    _minBlockNumberAudio = min(_minBlockNumberAudio, lba);
                }
            /* leadout */
            } else if (_toc->descriptors[i].point == 0xA2 && _toc->descriptors[i].adr == 1) {
                if (nblocks < lba) {
                    nblocks = lba;
                }
                _maxBlockNumberAudio = min(_maxBlockNumberAudio, lba ? (lba - 1) : 0);
            }
        }

        if (nblocks < _maxBlockNumber + 1) {
            nblocks = _maxBlockNumber + 1;
        }

        if (_maxBlockNumberAudio < _minBlockNumberAudio) {
            _maxBlockNumberAudio = 0xFFFFFFFF;

            /* find first data track or leadout after the audio tracks */
            for (i = 0; i < nentries; i++) {   
                UInt32 lba = CDConvertMSFToLBA(_toc->descriptors[i].p);
                /* tracks 1-99, not leadout or skip intervals */
                if (_toc->descriptors[i].point <= 99 && _toc->descriptors[i].adr == 1) {
                    if ((_toc->descriptors[i].control & 0x04)) {
                        /* it's a data track */
                        if (lba > _minBlockNumberAudio) {
                            _maxBlockNumberAudio = min(_maxBlockNumberAudio, lba - 1);
                        }
                    }
                /* leadout */
                } else if (_toc->descriptors[i].point == 0xA2 && _toc->descriptors[i].adr == 1) {
                    if (lba > _minBlockNumberAudio) {
                        _maxBlockNumberAudio = min(_maxBlockNumberAudio, lba - 1);
                    }
                }
            }
        }
    } else if (_maxBlockNumber) {
        nblocks = _maxBlockNumber + 1;
    }

    /* Instantiate a CD Media nub above ourselves. */

    name[0] = 0;
    nameSep = false;
    if (getProvider()->getVendorString()) {
        strcat(name, getProvider()->getVendorString());
        nameSep = true;
    }
    if (getProvider()->getProductString()) {
        if (nameSep == true)  strcat(name, " ");
        strcat(name, getProvider()->getProductString());
        nameSep = true;
    }
    if (nameSep == true)  strcat(name, " ");
    strcat(name, "Media");
    strclean(name);

    _mediaObject = instantiateMediaObject(0,nblocks*kBlockSizeCD,kBlockSizeCD,name);
    result = (_mediaObject) ? kIOReturnSuccess : kIOReturnBadArgument;

    if (result == kIOReturnSuccess) {
        ok = _mediaObject->attach(this);
    } else {
        IOLog("%s[IOCDBlockStorageDriver]::acceptNewMedia; can't instantiate CD media nub.\n",getName());
        return(result);			/* give up now */
    }
    if (!ok) {
        IOLog("%s[IOCDBlockStorageDriver]::acceptNewMedia; can't attach CD media nub.\n",getName());
        _mediaObject->release();
        _mediaObject = NULL;
        return(kIOReturnNoMemory);	/* give up now */
    }
        
    /* Instantiate an audio control nub for the audio portion of the media. */

    if (nAudioTracks) {
        _acNub = new IOCDAudioControl;
        if (_acNub) {
            _acNub->init();
            ok = _acNub->attach(this);
            if (!ok) {
                IOLog("%s[IOCDBlockStorageDriver]::acceptNewMedia; can't attach audio control nub.\n",getName());
                _acNub->release();
                _acNub = NULL;
            }
        } else {
            IOLog("%s[IOCDBlockStorageDriver]::acceptNewMedia; can't instantiate audio control nub.\n",
                  getName());
        }
    }

    /* Now that the nubs are attached, register them. */

    _mediaPresent = true;
    if (_toc) {
        _mediaObject->setProperty(kIOCDMediaTOCKey,(void*)_toc,_tocSize);
    }
    _mediaObject->registerService();

    if (_acNub) {
        _acNub->registerService();
    }

    return(result);
}

IOReturn
IOCDBlockStorageDriver::audioPause(bool pause)
{
    return(getProvider()->audioPause(pause));
}

IOReturn
IOCDBlockStorageDriver::audioPlay(CDMSF timeStart,CDMSF timeStop)
{
    return(getProvider()->audioPlay(timeStart,timeStop));
}

IOReturn
IOCDBlockStorageDriver::audioScan(CDMSF timeStart,bool reverse)
{
    return(getProvider()->audioScan(timeStart,reverse));
}

IOReturn
IOCDBlockStorageDriver::audioStop()
{
    return(getProvider()->audioStop());
}

IOReturn
IOCDBlockStorageDriver::cacheTocInfo(void)
{
    IOBufferMemoryDescriptor *buffer = NULL;
    IOReturn result;
    CDTOC *toc;
    UInt16 tocSize;

    assert(sizeof(CDTOC) == 4);		/* (compiler/platform check) */
    assert(sizeof(CDTOCDescriptor) == 11);		/* (compiler/platform check) */
    
    assert(_toc == NULL);

    /* Read the TOC header: */

    buffer = IOBufferMemoryDescriptor::withCapacity(sizeof(CDTOC),kIODirectionIn);
    if (buffer == NULL) {
        return(kIOReturnNoMemory);
    }

    result = getProvider()->readTOC(buffer);
    if (result != kIOReturnSuccess) {
        buffer->release();
        return(result);
    }

    toc = (CDTOC *) buffer->getBytesNoCopy();
    tocSize = OSSwapBigToHostInt16(toc->length) + sizeof(UInt16);

    buffer->release();

    /* Reject TOC if its size is too small: */

    if (tocSize < sizeof(CDTOC))
        return kIOReturnError;

    /* Read the TOC in full: */

    buffer = IOBufferMemoryDescriptor::withCapacity(tocSize,kIODirectionIn);
    if (buffer == NULL) {
        return(kIOReturnNoMemory);
    }

    result = getProvider()->readTOC(buffer);
    if (result != kIOReturnSuccess) {
        buffer->release();
        return(result);
    }
    
    toc = (CDTOC *) IOMalloc(tocSize);
    if (toc == NULL) {
        buffer->release();
        return(kIOReturnNoMemory);
    }

    if (buffer->readBytes(0,toc,tocSize) != tocSize) {
        buffer->release();
        IOFree(toc,tocSize);
        return(kIOReturnNoMemory);
    }

    _toc = toc;
    _tocSize = tocSize;

    buffer->release();

    /* Convert big-endian values in TOC to host-endianess: */

    if (_tocSize >= sizeof(UInt16)) {
        _toc->length = OSSwapBigToHostInt16(_toc->length);
    }

    return(result);
}

/* Decommission all nubs. The arbitration lock is assumed to
 * be held during the call.
 */
IOReturn
IOCDBlockStorageDriver::decommissionMedia(bool forcible)
{
    IOReturn result;

    if (_mediaObject) {
        /* If this is a forcible decommission (i.e. media is gone), we don't
         * care whether the teardown worked; we forget about the media.
         */
        if (_mediaObject->terminate(forcible ? kIOServiceRequired : 0) || forcible) {
            _mediaObject->release();
            _mediaObject = 0;

            initMediaState();        /* clear all knowledge of the media */
            result = kIOReturnSuccess;

        } else {
            result = kIOReturnBusy;
        }
    } else {
        result = kIOReturnNoMedia;
    }

    /* We only attempt to decommission the audio portion of the
     * CD if all the data tracks decommissioned successfully.
     */

    if (result == kIOReturnSuccess) {
        if (_acNub) {
            _acNub->terminate(kIOServiceRequired);
            _acNub->release();
            _acNub = 0;
        }
        if (_toc) {
            IOFree(_toc,_tocSize);
            _toc = NULL;
            _tocSize = 0;
        }

        _minBlockNumberAudio = 0;
        _maxBlockNumberAudio = 0;
    }

    return(result);
}

/* We should check with other clients using the other nubs before we allow
 * the client of the IOCDMedia to eject the media.
 */
IOReturn
IOCDBlockStorageDriver::ejectMedia(void)
{
    /* For now, we don't check with the other clients. */
    
    return(super::ejectMedia());
}

void
IOCDBlockStorageDriver::executeRequest(UInt64 byteStart,
                                       IOMemoryDescriptor *buffer,
                                       IOStorageCompletion completion,
                                       IOBlockStorageDriver::Context *context)
{
    UInt32 block;
    UInt32 nblks;
    IOReturn result;

    if (!_mediaPresent) {		/* no media? you lose */
        complete(completion, kIOReturnNoMedia,0);
        return;
    }

    /* We know that we are never called with a request too large,
     * nor one that is misaligned with a block.
     */
    assert((byteStart           % context->block.size) == 0);
    assert((buffer->getLength() % context->block.size) == 0);
    
    block = byteStart           / context->block.size;
    nblks = buffer->getLength() / context->block.size;

/* Now the protocol-specific provider implements the actual
     * start of the data transfer: */

    if (context->block.type == kBlockTypeCD) {
        /* Some drives have firmware that performs better at audio reads when
         * the block type is specifically set to CDDA, rather than "unknown";
         * this is a temporary measure until all clients cut over to the CDDA
         * based APIs; this measure should not be depended upon in the future.
         */
        if (context->block.typeSub[1] == kCDSectorTypeUnknown &&
            block                     >= _minBlockNumberAudio &&
            block + nblks - 1         <= _maxBlockNumberAudio) {
            context->block.typeSub[1] = kCDSectorTypeCDDA;

            if (context->block.typeSub[0] & 0xF8) {
                context->block.typeSub[0] &= ~(0xF8);
                context->block.typeSub[0] |= kCDSectorAreaUser;
            }
        }

        result = getProvider()->doAsyncReadCD(buffer,block,nblks,
                               (CDSectorArea)context->block.typeSub[0],
                               (CDSectorType)context->block.typeSub[1],
                               completion);
    } else {
        result = getProvider()->doAsyncReadWrite(buffer,block,nblks,completion);
    }

    if (result != kIOReturnSuccess) {		/* it failed to start */
        complete(completion,result);
        return;
    }
}

void
IOCDBlockStorageDriver::free(void)
{
    if (_expansionData) {
        IODelete(_expansionData, ExpansionData, 1);
    }

    super::free();
}

IOReturn
IOCDBlockStorageDriver::getAudioStatus(CDAudioStatus *status)
{
    return(getProvider()->getAudioStatus(status));
}

IOReturn
IOCDBlockStorageDriver::getAudioVolume(UInt8 *leftVolume,UInt8 *rightVolume)
{
    return(getProvider()->getAudioVolume(leftVolume,rightVolume));
}

const char *
IOCDBlockStorageDriver::getDeviceTypeName(void)
{
    return(kIOBlockStorageDeviceTypeCDROM);
}

UInt64
IOCDBlockStorageDriver::getMediaBlockSize(CDSectorArea area,CDSectorType type)
{
    UInt64 blockSize = 0;

    const SInt16 areaSize[kCDSectorTypeCount][8] =
    {                  /* 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 */
       /* Unknown    */ {   96,  294,   -1,  280, 2048,    4,    8,   12 },
       /* CDDA       */ {   96,  294,   -1,    0, 2352,    0,    0,    0 },
       /* Mode1      */ {   96,  294,   -1,  288, 2048,    4,    0,   12 },
       /* Mode2      */ {   96,  294,   -1,    0, 2336,    4,    0,   12 },
       /* Mode2Form1 */ {   96,  294,   -1,  280, 2048,    4,    8,   12 },
       /* Mode2Form2 */ {   96,  294,   -1,    0, 2328,    4,    8,   12 },
    };

    if ( type >= kCDSectorTypeCount )  return 0;

    for ( UInt32 index = 0; index < 8; index++ )
    {
        if ( ((area >> index) & 0x01) )
        {
            if ( areaSize[type][index] == -1 )  return 0;
            blockSize += areaSize[type][index];
        }
    }

    return blockSize;
}

UInt32
IOCDBlockStorageDriver::getMediaType(void)
{
    return(getProvider()->getMediaType());
}

CDTOC *
IOCDBlockStorageDriver::getTOC(void)
{
    return(_toc);
}

bool
IOCDBlockStorageDriver::init(OSDictionary * properties)
{
    if (super::init(properties) == false) {
        return false;
    }

    _expansionData = IONew(ExpansionData, 1);

    if (_expansionData == NULL) {
        return false;
    }

    _acNub = NULL;
    _minBlockNumberAudio = 0;
    _maxBlockNumberAudio = 0;
    _toc = NULL;
    _tocSize = 0;

    return(true);
}

IOMedia *
IOCDBlockStorageDriver::instantiateDesiredMediaObject(void)
{
    return(new IOCDMedia);
}

IOMedia *
IOCDBlockStorageDriver::instantiateMediaObject(UInt64 base,UInt64 byteSize,
                                        UInt32 blockSize,char *mediaName)
{
    IOMedia *media;

    media = super::instantiateMediaObject(base,byteSize,blockSize,mediaName);

    if (media) {
        char *description = NULL;

        switch (getMediaType()) {
            case kCDMediaTypeROM:
                description = kIOCDMediaTypeROM;
                break;
            case kCDMediaTypeR:
                description = kIOCDMediaTypeR;
                break;
            case kCDMediaTypeRW:
                description = kIOCDMediaTypeRW;
                break;
        }

        if (description) {
            media->setProperty(kIOCDMediaTypeKey, description);
        }
    }

    return media;
}

void
IOCDBlockStorageDriver::readCD(IOService *client,
                               UInt64 byteStart,
                               IOMemoryDescriptor *buffer,
                               CDSectorArea sectorArea,
                               CDSectorType sectorType,
                               IOStorageCompletion completion)
{
    assert(buffer->getDirection() == kIODirectionIn);

    prepareRequest(byteStart, buffer, sectorArea, sectorType, completion);
}

void
IOCDBlockStorageDriver::prepareRequest(UInt64 byteStart,
                                       IOMemoryDescriptor *buffer,
                                       CDSectorArea sectorArea,
                                       CDSectorType sectorType,
                                       IOStorageCompletion completion)
{
    Context * context;

    // For a transfer that involves an unknown sector type, the sector area must
    // not describe a vague sector size (and hence, a vague transfer size).  The
    // SYNC, HEADER, SUBHEADER, USER and AUXILIARY sector areas differ in length
    // from one sector type to the next.  All together, however, the same sector
    // areas describe a consistent sector size from one sector type to the next,
    // hence we permit either all the above sector areas to be specified at once
    // or none to be specified at all.  The other sector areas are consistent in
    // size from one sector type to the next, hence need not be considered here.

    if (sectorType == kCDSectorTypeUnknown)
    {
        if ((sectorArea & 0xF8) != 0x00 && (sectorArea & 0xF8) != 0xF8)
        {
            complete(completion, kIOReturnBadArgument);
            return;
        }
    }

    // Allocate a context structure to hold some of our state.

    context = allocateContext();

    if (context == 0)
    {
        complete(completion, kIOReturnNoMemory);
        return;
    }
    
    // Fill in the context structure with some of our state.

    if ( ( sectorArea == kCDSectorAreaUser       )  &&
         ( sectorType == kCDSectorTypeMode1      ||
           sectorType == kCDSectorTypeMode2Form1 )  )
    {
        context->block.size       = _mediaBlockSize;
        context->block.type       = kBlockTypeStandard;
    }
    else
    {
        context->block.size       = getMediaBlockSize(sectorArea, sectorType);
        context->block.type       = kBlockTypeCD;
        context->block.typeSub[0] = sectorArea;
        context->block.typeSub[1] = sectorType;    
    }

    context->original.byteStart  = byteStart;
    context->original.buffer     = buffer;
    context->original.buffer->retain();
    context->original.completion = completion;

    completion.target    = this;
    completion.action    = prepareRequestCompletion;
    completion.parameter = context;

    // Deblock the transfer.

    deblockRequest(byteStart, buffer, completion, context);
}

IOReturn
IOCDBlockStorageDriver::readISRC(UInt8 track,CDISRC isrc)
{
    return(getProvider()->readISRC(track,isrc));
}

IOReturn
IOCDBlockStorageDriver::readMCN(CDMCN mcn)
{
    return(getProvider()->readMCN(mcn));
}

IOReturn
IOCDBlockStorageDriver::setAudioVolume(UInt8 leftVolume,UInt8 rightVolume)
{
    return(getProvider()->setAudioVolume(leftVolume,rightVolume));
}

IOReturn
IOCDBlockStorageDriver::getSpeed(UInt16 * kilobytesPerSecond)
{
    return(getProvider()->getSpeed(kilobytesPerSecond));
}

OSMetaClassDefineReservedUsed(IOCDBlockStorageDriver, 0);

IOReturn
IOCDBlockStorageDriver::setSpeed(UInt16 kilobytesPerSecond)
{
    return(getProvider()->setSpeed(kilobytesPerSecond));
}

OSMetaClassDefineReservedUsed(IOCDBlockStorageDriver, 1);

OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  2);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  3);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  4);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  5);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  6);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  7);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  8);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  9);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 10);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 11);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 12);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 13);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 14);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 15);
