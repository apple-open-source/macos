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

#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioDMAEngine.h>
#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>

#define super IOService
OSDefineMetaClassAndStructors(IOAudioStream, IOService)

const OSSymbol *IOAudioStream::gDirectionKey = NULL;
const OSSymbol *IOAudioStream::gNumChannelsKey = NULL;
const OSSymbol *IOAudioStream::gSampleFormatKey = NULL;
const OSSymbol *IOAudioStream::gNumericRepresentationKey = NULL;
const OSSymbol *IOAudioStream::gBitDepthKey = NULL;
const OSSymbol *IOAudioStream::gBitWidthKey = NULL;
const OSSymbol *IOAudioStream::gAlignmentKey = NULL;
const OSSymbol *IOAudioStream::gByteOrderKey = NULL;
const OSSymbol *IOAudioStream::gIsMixableKey = NULL;
const OSSymbol *IOAudioStream::gMinimumSampleRateKey = NULL;
const OSSymbol *IOAudioStream::gMaximumSampleRateKey = NULL;

void IOAudioStream::initKeys()
{
    if (!gNumChannelsKey) {
        gNumChannelsKey = OSSymbol::withCString(IOAUDIOSTREAM_NUM_CHANNELS_KEY);
        gSampleFormatKey = OSSymbol::withCString(IOAUDIOSTREAM_SAMPLE_FORMAT_KEY);
        gNumericRepresentationKey = OSSymbol::withCString(IOAUDIOSTREAM_NUMERIC_REPRESENTATION_KEY);
        gBitDepthKey = OSSymbol::withCString(IOAUDIOSTREAM_BIT_DEPTH_KEY);
        gBitWidthKey = OSSymbol::withCString(IOAUDIOSTREAM_BIT_WIDTH_KEY);
        gAlignmentKey = OSSymbol::withCString(IOAUDIOSTREAM_ALIGNMENT_KEY);
        gByteOrderKey = OSSymbol::withCString(IOAUDIOSTREAM_BYTE_ORDER_KEY);
        gIsMixableKey = OSSymbol::withCString(IOAUDIOSTREAM_IS_MIXABLE_KEY);

        gDirectionKey = OSSymbol::withCString(IOAUDIOSTREAM_DIRECTION_KEY);
        
        gMinimumSampleRateKey = OSSymbol::withCString(IOAUDIOSTREAM_MINIMUM_SAMPLE_RATE_KEY);
        gMaximumSampleRateKey = OSSymbol::withCString(IOAUDIOSTREAM_MAXIMUM_SAMPLE_RATE_KEY);
    }
}

OSDictionary *IOAudioStream::createDictionaryFromFormat(const IOAudioStreamFormat *streamFormat, OSDictionary *formatDict = 0)
{
    OSDictionary *newDict = NULL;

    if (streamFormat) {
        if (formatDict) {
            newDict = formatDict;
        } else {
            newDict = OSDictionary::withCapacity(7);
        }

        if (newDict) {
            OSNumber *num;

            if (!gNumChannelsKey) {
                initKeys();
            }
            
            num = OSNumber::withNumber(streamFormat->fNumChannels, 32);
            newDict->setObject(gNumChannelsKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fSampleFormat, 32);
            newDict->setObject(gSampleFormatKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fNumericRepresentation, 32);
            newDict->setObject(gNumericRepresentationKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fBitDepth, 8);
            newDict->setObject(gBitDepthKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fBitWidth, 8);
            newDict->setObject(gBitWidthKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fAlignment, 8);
            newDict->setObject(gAlignmentKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fByteOrder, 8);
            newDict->setObject(gByteOrderKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fIsMixable, 8);
            newDict->setObject(gIsMixableKey, num);
            num->release();
        }
    }


    return newDict;
}

IOAudioStreamFormat *IOAudioStream::createFormatFromDictionary(const OSDictionary *formatDict, IOAudioStreamFormat *streamFormat = 0)
{
    IOAudioStreamFormat *format = NULL;
    static IOAudioStreamFormat staticFormat;

    if (formatDict) {
        if (streamFormat) {
            format = streamFormat;
        } else {
            format = &staticFormat;
        }

        if (format) {
            OSNumber *num;

            if (!gNumChannelsKey) {
                initKeys();
            }

            bzero(format, sizeof(IOAudioStreamFormat));

            num = OSDynamicCast(OSNumber, formatDict->getObject(gNumChannelsKey));
            if (num) {
                format->fNumChannels = num->unsigned32BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gSampleFormatKey));
            if (num) {
                format->fSampleFormat = num->unsigned32BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gNumericRepresentationKey));
            if (num) {
                format->fNumericRepresentation = num->unsigned32BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gBitDepthKey));
            if (num) {
                format->fBitDepth = num->unsigned8BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gBitWidthKey));
            if (num) {
                format->fBitWidth = num->unsigned8BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gAlignmentKey));
            if (num) {
                format->fAlignment = num->unsigned8BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gByteOrderKey));
            if (num) {
                format->fByteOrder = num->unsigned8BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gIsMixableKey));
            if (num) {
                format->fIsMixable = num->unsigned8BitValue();
            }
        }
    }

    return format;
}


bool IOAudioStream::initWithAudioDMAEngine(IOAudioDMAEngine *dmaEngine, IOAudioStreamDirection dir, OSDictionary *properties)
{
    if (!gNumChannelsKey) {
        initKeys();
    }

    if (!dmaEngine) {
        return false;
    }

    if (!super::init(properties)) {
        return false;
    }

    audioDMAEngine = dmaEngine;
    
    workLoop = audioDMAEngine->getWorkLoop();
    if (!workLoop) {
        return false;
    }

    workLoop->retain();
    
    commandGate = IOCommandGate::commandGate(this);
    if (!commandGate) {
        return false;
    }
    
    streamIOLock = IORecursiveLockAlloc();
    if (!streamIOLock) {
        return false;
    }
    
    setDirection(dir);
    
    availableFormats = OSArray::withCapacity(1);
    if (!availableFormats) {
        return false;
    }
    setProperty(IOAUDIOSTREAM_AVAILABLE_FORMATS_KEY, availableFormats);
    
    setProperty(IOAUDIOSTREAM_ID_KEY, (UInt32)this, sizeof(UInt32)*8);
    
    numClients = 0;
    updateNumClients();

    workLoop->addEventSource(commandGate);

    return true;
}

void IOAudioStream::free()
{
    if (availableFormats) {
        availableFormats->release();
        availableFormats = NULL;
    }

    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }

        commandGate->release();
        commandGate = NULL;
    }

    if (workLoop) {
	workLoop->release();
        workLoop = NULL;
    }
    
    if (streamIOLock) {
        IORecursiveLockFree(streamIOLock);
        streamIOLock = NULL;
    }
    
    super::free();
}

void IOAudioStream::stop(IOService *provider)
{
    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }
        
        commandGate->release();
        commandGate = NULL;
    }
    
    super::stop(provider);
}

IOWorkLoop *IOAudioStream::getWorkLoop()
{	
    return workLoop;
}

IOReturn IOAudioStream::setProperties(OSObject *properties)
{
    OSDictionary *props;
    IOReturn result = kIOReturnSuccess;

    if (properties && (props = OSDynamicCast(OSDictionary, properties))) {
        OSCollectionIterator *iterator;
        OSObject *iteratorKey;

        iterator = OSCollectionIterator::withCollection(props);
        if (iterator) {
            while (iteratorKey = iterator->getNextObject()) {
                OSSymbol *key;

                key = OSDynamicCast(OSSymbol, iteratorKey);
                if (key && key->isEqualTo(IOAUDIOSTREAM_FORMAT_KEY)) {
                    OSDictionary *formatDict = OSDynamicCast(OSDictionary, props->getObject(key));
                    if (formatDict) {
                        //result = this->setFormat(formatDict);
                        assert(commandGate);
                        result = commandGate->runAction(setFormatAction, formatDict);
                    }
                }
            }
            iterator->release();
        } else {
            result = kIOReturnError;
        }
    } else {
        result = kIOReturnBadArgument;
    }

    return result;
}

void IOAudioStream::setDirection(IOAudioStreamDirection dir)
{
    UInt8 value = 0;
    
    direction = dir;

    switch (direction) {
        case kAudioOutput:
            value = IOAUDIOSTREAM_DIRECTION_OUTPUT;
            break;
        case kAudioInput:
            value = IOAUDIOSTREAM_DIRECTION_INPUT;
            break;
    }

    setProperty(IOAUDIOSTREAM_DIRECTION_KEY, value, 8);
}

IOAudioStreamDirection IOAudioStream::getDirection()
{
    return direction;
}

void IOAudioStream::setSampleBuffer(void *buffer, UInt32 size)
{
    assert(streamIOLock);
    
    IORecursiveLockLock(streamIOLock);
    
    sampleBuffer = buffer;

    if (sampleBuffer) {
        sampleBufferSize = size;
        bzero(sampleBuffer, sampleBufferSize);
    } else {
        sampleBufferSize = 0;
    }
    
    IORecursiveLockUnlock(streamIOLock);
}

void *IOAudioStream::getSampleBuffer()
{
    return sampleBuffer;
}

UInt32 IOAudioStream::getSampleBufferSize()
{
    return sampleBufferSize;
}

void IOAudioStream::setMixBuffer(void *buffer, UInt32 size)
{
    assert(streamIOLock);
    
    IORecursiveLockLock(streamIOLock);
    
    mixBuffer = buffer;

    if (mixBuffer) {
        mixBufferSize = size;
        bzero(mixBuffer, mixBufferSize);
    } else {
        mixBufferSize = 0;
    }
    
    IORecursiveLockUnlock(streamIOLock);
}

void *IOAudioStream::getMixBuffer()
{
    return mixBuffer;
}

UInt32 IOAudioStream::getMixBufferSize()
{
    return mixBufferSize;
}

void IOAudioStream::clearSampleBuffer()
{
    if (sampleBuffer && (sampleBufferSize > 0)) {
        bzero(sampleBuffer, sampleBufferSize);
    }
    
    if (mixBuffer && (mixBufferSize > 0)) {
        bzero(mixBuffer, mixBufferSize);
    }
}

const IOAudioStreamFormat *IOAudioStream::getFormat()
{
    return &format;
}

IOReturn IOAudioStream::setFormatAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioStream *audioStream = OSDynamicCast(IOAudioStream, owner);
        if (audioStream) {
            result = audioStream->setFormat((OSDictionary *)arg1);
        }
    }
    
    return result;
}

IOReturn IOAudioStream::setFormat(const IOAudioStreamFormat *streamFormat)
{
    IOReturn result = kIOReturnSuccess;
    OSDictionary *formatDict = NULL;
    
    if (streamFormat) {
        if (formatDict = createDictionaryFromFormat(streamFormat)) {
            result = setFormat(streamFormat, formatDict);
            formatDict->release();
        } else {
            result = kIOReturnError;
        }
    } else {
        result = kIOReturnBadArgument;
    }

    return result;
}

IOReturn IOAudioStream::setFormat(OSDictionary *formatDict)
{
    IOReturn result = kIOReturnSuccess;

    if (formatDict) {
        IOAudioStreamFormat *streamFormat = createFormatFromDictionary(formatDict);
        if (streamFormat) {
            result = setFormat(streamFormat, formatDict);
        } else {
            result = kIOReturnBadArgument;
        }
    } else {
        result = kIOReturnBadArgument;
    }

    return result;
}

IOReturn IOAudioStream::setFormat(const IOAudioStreamFormat *streamFormat, OSDictionary *formatDict)
{
    IOReturn result = kIOReturnSuccess;
    
    if (!streamFormat || !formatDict) {
        return kIOReturnBadArgument;
    }
    
    if (validateFormat(streamFormat)) {
        OSDictionary *sampleRateDict;
        IOAudioSampleRate *newSampleRate = NULL;
        
        sampleRateDict = OSDynamicCast(OSDictionary, formatDict->getObject(IOAUDIO_SAMPLE_RATE_KEY));
        if (sampleRateDict) {
            const IOAudioSampleRate *currentSampleRate;
            
            newSampleRate = IOAudioDMAEngine::createSampleRateFromDictionary(sampleRateDict);
            currentSampleRate = audioDMAEngine->getSampleRate();
            if (newSampleRate && (newSampleRate->whole == currentSampleRate->whole) && (newSampleRate->fraction == currentSampleRate->fraction)) {
                newSampleRate = NULL;
            }
        }
        
        assert(streamIOLock);
        
        IORecursiveLockLock(streamIOLock);
        
        // Don't send the format change notifications until after releasing the lock
        result = audioDMAEngine->performFormatChange(this, streamFormat, newSampleRate);
        if (result == kIOReturnSuccess) {
            format = *streamFormat;
            setProperty(IOAUDIOSTREAM_FORMAT_KEY, formatDict);
            
            if (newSampleRate) {
                audioDMAEngine->_setSampleRate(newSampleRate);
            }
        } else {
            IOLog("IOAudioStream<0x%x>::setFormat(0x%x, 0x%x) - DMA engine unable to change format\n", (unsigned int)this, (unsigned int)streamFormat, (unsigned int)formatDict);
        }
        
        IORecursiveLockUnlock(streamIOLock);
        
        if (result == kIOReturnSuccess) {
            audioDMAEngine->sendFormatChangeNotification(this);
        }
    } else {
        IOLog("IOAudioStream<0x%x>::setFormat(0x%x, 0x%x) - invalid format.\n", (unsigned int)this, (unsigned int)streamFormat, (unsigned int)formatDict);
        result = kIOReturnBadArgument;
    }
    
    return result;
}

void IOAudioStream::addAvailableFormat(const IOAudioStreamFormat *streamFormat, const IOAudioSampleRate *minRate, const IOAudioSampleRate *maxRate)
{
    assert(availableFormats);

    if (streamFormat && minRate && maxRate) {
        OSDictionary *formatDict = createDictionaryFromFormat(streamFormat);
        if (formatDict) {
            OSDictionary *sampleRateDict;
        
            sampleRateDict = IOAudioDMAEngine::createDictionaryFromSampleRate(minRate);
            if (sampleRateDict) {
                formatDict->setObject(gMinimumSampleRateKey, sampleRateDict);
                sampleRateDict->release();
                sampleRateDict = IOAudioDMAEngine::createDictionaryFromSampleRate(maxRate);
                if (sampleRateDict) {
                    formatDict->setObject(gMaximumSampleRateKey, sampleRateDict);
                    sampleRateDict->release();
                    availableFormats->setObject(formatDict);
                }
            }
            formatDict->release();
        }
    }
}

void IOAudioStream::clearAvailableFormats()
{
    assert(availableFormats);

    availableFormats->flushCollection();
}

bool IOAudioStream::validateFormat(const IOAudioStreamFormat *streamFormat)
{
    return true;
}

void IOAudioStream::updateNumClients()
{
    setProperty(IOAUDIOSTREAM_NUM_CLIENTS_KEY, numClients, sizeof(UInt32));
}

void IOAudioStream::addClient()
{
    numClients++;
    updateNumClients();
    
    if (!mixBuffer && sampleBuffer && (sampleBufferSize > 0)) {
        assert(audioDMAEngine);
        
        UInt32 mixBufSize = format.fNumChannels * IOAUDIODMAENGINE_DEFAULT_MIX_BUFFER_SAMPLE_SIZE * audioDMAEngine->numSampleFramesPerBuffer;
        
        if (mixBufSize > 0) {
            void *mixBuf = IOMalloc(mixBufSize);
            if (mixBuf) {
                setMixBuffer(mixBuf, mixBufSize);
            }
        }
    }
}

void IOAudioStream::removeClient()
{
    numClients--;
    updateNumClients();
}

void IOAudioStream::lockStreamForIO()
{
    assert(streamIOLock);
    
    IORecursiveLockLock(streamIOLock);
}

void IOAudioStream::unlockStreamForIO()
{
    assert(streamIOLock);
    
    IORecursiveLockUnlock(streamIOLock);
}

bool IOAudioStream::addDefaultAudioControl(IOAudioControl *defaultAudioControl)
{
    return attach(defaultAudioControl);
}
 
void IOAudioStream::removeDefaultAudioControl(IOAudioControl *defaultAudioControl)
{
    detach(defaultAudioControl);
}

