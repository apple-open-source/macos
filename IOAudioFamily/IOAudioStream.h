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

#ifndef _IOKIT_IOAUDIOSTREAM_H
#define _IOKIT_IOAUDIOSTREAM_H

#include <IOKit/IOService.h>
#include <IOKit/audio/IOAudioTypes.h>

class OSSymbol;
class OSArray;
class OSDictionary;

class IOAudioDMAEngine;
class IOCommandGate;
class IOAudioControl;

/*!
* @enum IOAudioStreamDirection
 * @abstract Represents the direction of an IOAudioStream
 * @constant kAudioOutput Output buffer
 * @constant kAudioInput Input buffer
 */

typedef enum _IOAudioStreamDirection
{
    kAudioOutput = 0,
    kAudioInput
} IOAudioStreamDirection;


class IOAudioStream : public IOService
{
    OSDeclareDefaultStructors(IOAudioStream)
    
    friend class IOAudioDMAEngineUserClient;

public:

    static const OSSymbol	*gDirectionKey;
    static const OSSymbol	*gNumChannelsKey;
    static const OSSymbol	*gSampleFormatKey;
    static const OSSymbol	*gNumericRepresentationKey;
    static const OSSymbol	*gBitDepthKey;
    static const OSSymbol	*gBitWidthKey;
    static const OSSymbol	*gAlignmentKey;
    static const OSSymbol	*gByteOrderKey;
    static const OSSymbol	*gIsMixableKey;
    static const OSSymbol	*gMinimumSampleRateKey;
    static const OSSymbol	*gMaximumSampleRateKey;

    static void initKeys();

    static OSDictionary *createDictionaryFromFormat(const IOAudioStreamFormat *streamFormat, OSDictionary *formatDict = 0);
    static IOAudioStreamFormat *createFormatFromDictionary(const OSDictionary *formatDict, IOAudioStreamFormat *streamFormat = 0);
    

    IOAudioDMAEngine 			*audioDMAEngine;
    IOWorkLoop					*workLoop;
    IOCommandGate				*commandGate;
    IORecursiveLock				*streamIOLock;
    
    UInt32					numClients;

    IOAudioStreamDirection	direction;

    IOAudioStreamFormat		format;
    OSArray				*availableFormats;
    
    void				*sampleBuffer;
    UInt32				sampleBufferSize;

    void				*mixBuffer;
    UInt32				mixBufferSize;

    virtual bool initWithAudioDMAEngine(IOAudioDMAEngine *dmaEngine, IOAudioStreamDirection dir, OSDictionary *properties = 0);
    virtual void free();
    
    virtual void stop(IOService *provider);
    
    virtual IOWorkLoop *getWorkLoop();

    virtual IOReturn setProperties(OSObject *properties);

    virtual IOAudioStreamDirection getDirection();

    virtual void setSampleBuffer(void *buffer, UInt32 size);
    virtual void *getSampleBuffer();
    virtual UInt32 getSampleBufferSize();
    
    virtual void setMixBuffer(void *buffer, UInt32 size);
    virtual void *getMixBuffer();
    virtual UInt32 getMixBufferSize();
    
    virtual void clearSampleBuffer();

    virtual const IOAudioStreamFormat *getFormat();
    static IOReturn setFormatAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
    virtual IOReturn setFormat(const IOAudioStreamFormat *streamFormat);
    virtual IOReturn setFormat(OSDictionary *formatDict);
    virtual IOReturn setFormat(const IOAudioStreamFormat *streamFormat, OSDictionary *formatDict);
    virtual void addAvailableFormat(const IOAudioStreamFormat *streamFormat, const IOAudioSampleRate *minRate, const IOAudioSampleRate *maxRate);
    virtual void clearAvailableFormats();
    virtual bool validateFormat(const IOAudioStreamFormat *streamFormat);

    virtual bool addDefaultAudioControl(IOAudioControl *defaultAudioControl);
    virtual void removeDefaultAudioControl(IOAudioControl *defaultAudioControl);
    
protected:
    virtual void lockStreamForIO();
    virtual void unlockStreamForIO();
    
    virtual void updateNumClients();
    virtual void addClient();
    virtual void removeClient();
    
private:
    virtual void setDirection(IOAudioStreamDirection dir);

};

#endif /* _IOKIT_IOAUDIOSTREAM_H */
