/*
 * Copyright (c) 1998-2010 Apple Computer, Inc. All rights reserved.
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
 
#include "IOAudioDebug.h"
#include "IOAudioEngine.h"
#include "IOAudioEngineUserClient.h"
#include "IOAudioDevice.h"
#include "IOAudioStream.h"
#include "IOAudioTypes.h"
#include "IOAudioDefines.h"
#include "IOAudioControl.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSOrderedSet.h>

#include <kern/clock.h>

#define WATCHDOG_THREAD_LATENCY_PADDING_NS	(125000)	// 125us

#define super IOService

OSDefineMetaClassAndAbstractStructors(IOAudioEngine, IOService)

OSMetaClassDefineReservedUsed(IOAudioEngine, 0);
OSMetaClassDefineReservedUsed(IOAudioEngine, 1);
OSMetaClassDefineReservedUsed(IOAudioEngine, 2);
OSMetaClassDefineReservedUsed(IOAudioEngine, 3);
OSMetaClassDefineReservedUsed(IOAudioEngine, 4);
OSMetaClassDefineReservedUsed(IOAudioEngine, 5);
OSMetaClassDefineReservedUsed(IOAudioEngine, 6);
OSMetaClassDefineReservedUsed(IOAudioEngine, 7);
OSMetaClassDefineReservedUsed(IOAudioEngine, 8);
OSMetaClassDefineReservedUsed(IOAudioEngine, 9);
OSMetaClassDefineReservedUsed(IOAudioEngine, 10);
OSMetaClassDefineReservedUsed(IOAudioEngine, 11);
OSMetaClassDefineReservedUsed(IOAudioEngine, 12);
OSMetaClassDefineReservedUsed(IOAudioEngine, 13);
OSMetaClassDefineReservedUsed(IOAudioEngine, 14);

OSMetaClassDefineReservedUnused(IOAudioEngine, 15);
OSMetaClassDefineReservedUnused(IOAudioEngine, 16);
OSMetaClassDefineReservedUnused(IOAudioEngine, 17);
OSMetaClassDefineReservedUnused(IOAudioEngine, 18);
OSMetaClassDefineReservedUnused(IOAudioEngine, 19);
OSMetaClassDefineReservedUnused(IOAudioEngine, 20);
OSMetaClassDefineReservedUnused(IOAudioEngine, 21);
OSMetaClassDefineReservedUnused(IOAudioEngine, 22);
OSMetaClassDefineReservedUnused(IOAudioEngine, 23);
OSMetaClassDefineReservedUnused(IOAudioEngine, 24);
OSMetaClassDefineReservedUnused(IOAudioEngine, 25);
OSMetaClassDefineReservedUnused(IOAudioEngine, 26);
OSMetaClassDefineReservedUnused(IOAudioEngine, 27);
OSMetaClassDefineReservedUnused(IOAudioEngine, 28);
OSMetaClassDefineReservedUnused(IOAudioEngine, 29);
OSMetaClassDefineReservedUnused(IOAudioEngine, 30);
OSMetaClassDefineReservedUnused(IOAudioEngine, 31);
OSMetaClassDefineReservedUnused(IOAudioEngine, 32);
OSMetaClassDefineReservedUnused(IOAudioEngine, 33);
OSMetaClassDefineReservedUnused(IOAudioEngine, 34);
OSMetaClassDefineReservedUnused(IOAudioEngine, 35);
OSMetaClassDefineReservedUnused(IOAudioEngine, 36);
OSMetaClassDefineReservedUnused(IOAudioEngine, 37);
OSMetaClassDefineReservedUnused(IOAudioEngine, 38);
OSMetaClassDefineReservedUnused(IOAudioEngine, 39);
OSMetaClassDefineReservedUnused(IOAudioEngine, 40);
OSMetaClassDefineReservedUnused(IOAudioEngine, 41);
OSMetaClassDefineReservedUnused(IOAudioEngine, 42);
OSMetaClassDefineReservedUnused(IOAudioEngine, 43);
OSMetaClassDefineReservedUnused(IOAudioEngine, 44);
OSMetaClassDefineReservedUnused(IOAudioEngine, 45);
OSMetaClassDefineReservedUnused(IOAudioEngine, 46);
OSMetaClassDefineReservedUnused(IOAudioEngine, 47);

// OSMetaClassDefineReservedUsed(IOAudioEngine, 13);
IOReturn IOAudioEngine::setAttributeForConnection( SInt32 connectIndex, UInt32 attribute, uintptr_t value )
{
	return kIOReturnUnsupported;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 14);
IOReturn IOAudioEngine::getAttributeForConnection( SInt32 connectIndex, UInt32 attribute, uintptr_t * value )
{
	return kIOReturnUnsupported;
}

// New Code:
// OSMetaClassDefineReservedUsed(IOAudioEngine, 12);
IOReturn IOAudioEngine::createUserClient(task_t task, void *securityID, UInt32 type, IOAudioEngineUserClient **newUserClient, OSDictionary *properties)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioEngineUserClient *userClient;
    
    userClient = IOAudioEngineUserClient::withAudioEngine(this, task, securityID, type, properties);
    
    if (userClient) {
        *newUserClient = userClient;
    } else {
        result = kIOReturnNoMemory;
    }
    
    return result;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 11);
void IOAudioEngine::setInputSampleOffset(UInt32 numSamples) {
    audioDebugIOLog(3, "IOAudioEngine[%p]::setInputSampleOffset(0x%lx)", this, numSamples);
	assert(reserved);
	reserved->inputSampleOffset = numSamples;
    setProperty(kIOAudioEngineInputSampleOffsetKey, numSamples, sizeof(UInt32)*8);
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 10);
void IOAudioEngine::setOutputSampleOffset(UInt32 numSamples) {
    audioDebugIOLog(3, "IOAudioEngine[%p]::setOutputSampleOffset(0x%lx)", this, numSamples);
	setSampleOffset(numSamples);
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 9);
IOReturn IOAudioEngine::convertInputSamplesVBR(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 &numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	IOReturn result;

    result = convertInputSamples(sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
		
	return result;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 8);
void IOAudioEngine::setClockDomain(UInt32 inClockDomain) {

	UInt32		clockDomain;

	if (kIOAudioNewClockDomain == inClockDomain) {
#if __LP64__	
		clockDomain = (UInt32) ((UInt64)this >> 2) ; // grab a couple of bits from the high address to help randomness
#else
		clockDomain = (UInt32) this ;
#endif

	} else {
		clockDomain = inClockDomain;
	}

	setProperty(kIOAudioEngineClockDomainKey, clockDomain, sizeof(UInt32)*8);
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 7);
void IOAudioEngine::setClockIsStable(bool clockIsStable) {
	setProperty(kIOAudioEngineClockIsStableKey, clockIsStable);
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 6);
IOAudioStream * IOAudioEngine::getStreamForID(UInt32 streamID) {
	IOAudioStream *			stream = NULL;

	assert(reserved);
	if (reserved->streams) {
		stream = OSDynamicCast (IOAudioStream, reserved->streams->getObject(streamID));
	}

	return stream;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 5);
UInt32 IOAudioEngine::getNextStreamID(IOAudioStream * newStream) {
	bool			inserted;

	assert(reserved);
	if (!reserved->streams) {
		reserved->streams = OSArray::withCapacity(1);
	}

	inserted = reserved->streams->setObject(newStream);

	return reserved->streams->getCount() - 1;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 4);
void IOAudioEngine::lockStreamForIO(IOAudioStream *stream) {
	stream->lockStreamForIO();
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 3);
void IOAudioEngine::unlockStreamForIO(IOAudioStream *stream) {
	stream->unlockStreamForIO();
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 2);
IOReturn IOAudioEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioStreamFormatExtension *formatExtension, const IOAudioSampleRate *newSampleRate)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::performFormatChange(%p, %p, %p, %p)", this, audioStream, newFormat, formatExtension, newSampleRate);

    return kIOReturnUnsupported;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 1);
IOBufferMemoryDescriptor *IOAudioEngine::getStatusDescriptor()
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::getStatusDescriptor()", this);
	assert(reserved);

	return reserved->statusDescriptor;
}

IOReturn IOAudioEngine::getNearestStartTime(IOAudioStream *audioStream, IOAudioTimeStamp *ioTimeStamp, bool isInput)
{
	return kIOReturnSuccess;
}

IOBufferMemoryDescriptor * IOAudioEngine::getBytesInInputBufferArrayDescriptor()
{
	assert(reserved);

	return reserved->bytesInInputBufferArrayDescriptor;
}

IOBufferMemoryDescriptor * IOAudioEngine::getBytesInOutputBufferArrayDescriptor()
{
	assert(reserved);

	return reserved->bytesInOutputBufferArrayDescriptor;
}

IOReturn IOAudioEngine::eraseOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	if (mixBuf) {
		int csize = streamFormat->fNumChannels * kIOAudioEngineDefaultMixBufferSampleSize;
		bzero((UInt8*)mixBuf + firstSampleFrame * csize, numSampleFrames * csize);
	}
	if (sampleBuf) {
		int csize = streamFormat->fNumChannels * streamFormat->fBitWidth / 8;
		bzero((UInt8*)sampleBuf + (firstSampleFrame * csize), numSampleFrames * csize);
	}
	return kIOReturnSuccess;
}

void IOAudioEngine::setMixClipOverhead(UInt32 newMixClipOverhead)
{
	if (newMixClipOverhead > 1 && newMixClipOverhead < 99) {
		reserved->mixClipOverhead = newMixClipOverhead;
	}
}

// Original code from here forward:
SInt32 compareAudioStreams(IOAudioStream *stream1, IOAudioStream *stream2, void *ref)
{
    UInt32 startingChannelID1, startingChannelID2;
    
    startingChannelID1 = stream1->getStartingChannelID();
    startingChannelID2 = stream2->getStartingChannelID();
    
    return (startingChannelID1 > startingChannelID2) ? 1 : ((startingChannelID2 > startingChannelID2) ? -1 : 0);
}

const OSSymbol *IOAudioEngine::gSampleRateWholeNumberKey = NULL;
const OSSymbol *IOAudioEngine::gSampleRateFractionKey = NULL;

void IOAudioEngine::initKeys()
{
    if (!gSampleRateWholeNumberKey) {
        gSampleRateWholeNumberKey = OSSymbol::withCString(kIOAudioSampleRateWholeNumberKey);
        gSampleRateFractionKey = OSSymbol::withCString(kIOAudioSampleRateFractionKey);
    }
}

OSDictionary *IOAudioEngine::createDictionaryFromSampleRate(const IOAudioSampleRate *sampleRate, OSDictionary *rateDict)
{
    OSDictionary *newDict = NULL;
    
    if (sampleRate) {
        if (rateDict) {
            newDict = rateDict;
        } else {
            newDict = OSDictionary::withCapacity(2);
        }
        
        if (newDict) {
            OSNumber *num;
            
            if (!gSampleRateWholeNumberKey) {
                initKeys();
            }
            
            num = OSNumber::withNumber(sampleRate->whole, sizeof(UInt32)*8);
            newDict->setObject(gSampleRateWholeNumberKey, num);
            num->release();
            
            num = OSNumber::withNumber(sampleRate->fraction, sizeof(UInt32)*8);
            newDict->setObject(gSampleRateFractionKey, num);
            num->release();
        }
    }
    
    return newDict;
}

IOAudioSampleRate *IOAudioEngine::createSampleRateFromDictionary(const OSDictionary *rateDict, IOAudioSampleRate *sampleRate)
{
    IOAudioSampleRate *rate = NULL;
    static IOAudioSampleRate staticSampleRate;
    
    if (rateDict) {
        if (sampleRate) {
            rate = sampleRate;
        } else {
            rate = &staticSampleRate;
        }
        
        if (rate) {
            OSNumber *num;
            
            if (!gSampleRateWholeNumberKey) {
                initKeys();
            }
            
            bzero(rate, sizeof(IOAudioSampleRate));
            
            num = OSDynamicCast(OSNumber, rateDict->getObject(gSampleRateWholeNumberKey));
            if (num) {
                rate->whole = num->unsigned32BitValue();
            }
            
            num = OSDynamicCast(OSNumber, rateDict->getObject(gSampleRateFractionKey));
            if (num) {
                rate->fraction = num->unsigned32BitValue();
            }
        }
    }
    
    return rate;
}

bool IOAudioEngine::init(OSDictionary *properties)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::init(%p)", this, properties);

	OSDictionary * pDict = NULL;	
	
	if ( properties ) // properties is normally NULL
	{
		pDict = (OSDictionary*)properties->copyCollection();
			
		audioDebugIOLog(3, "IOAudioEngine[%p]::init() Make copy of properties(%p) != pDict(%p)", this, properties,pDict);
	}
	else
	{
		audioDebugIOLog(3, "IOAudioEngine[%p]::init() properties(%p) == NULL", this, properties);
	}
	
    if (!super::init(pDict)) {
        return false;
    }
    duringStartup = true;

    sampleRate.whole = 0;
    sampleRate.fraction = 0;
    
    numErasesPerBuffer = IOAUDIOENGINE_DEFAULT_NUM_ERASES_PER_BUFFER;
    isRegistered = false;
    
    numActiveUserClients = 0;

	reserved = (ExpansionData *)IOMalloc (sizeof(struct ExpansionData));
	if (!reserved) {
		return false;
	}
	reserved->pauseCount = 0;
	reserved->bytesInInputBufferArrayDescriptor = NULL;
	reserved->bytesInOutputBufferArrayDescriptor = NULL;
	reserved->mixClipOverhead = 10;		// The default value is 10%
	reserved->streams = NULL;

	reserved->statusDescriptor = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn | kIOMemoryKernelUserShared, round_page_32(sizeof(IOAudioEngineStatus)), page_size);
//	reserved->statusDescriptor = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn | kIOMemoryKernelUserShared, round_page(sizeof(IOAudioEngineStatus)), page_size);
	if (!reserved->statusDescriptor) {
		return false;
	}
	status = (IOAudioEngineStatus *)reserved->statusDescriptor->getBytesNoCopy();

    if (!status) {
        return false;
    }

    outputStreams = OSOrderedSet::withCapacity(1, (OSOrderedSet::OSOrderFunction)compareAudioStreams);
    if (!outputStreams) {
        return false;
    }

    inputStreams = OSOrderedSet::withCapacity(1, (OSOrderedSet::OSOrderFunction)compareAudioStreams);
    if (!inputStreams) {
        return false;
    }

	setClockDomain();
    
    maxNumOutputChannels = 0;
    maxNumInputChannels = 0;

    setSampleOffset(0);

    userClients = OSSet::withCapacity(1);
    if (!userClients) {
        return false;
    }
    
	bzero(status, round_page_32(sizeof(IOAudioEngineStatus)));
    status->fVersion = kIOAudioEngineCurrentStatusStructVersion;

    setState(kIOAudioEngineStopped);

#if __i386__ || __x86_64__
	setProperty(kIOAudioEngineFlavorKey, (UInt32)kIOAudioStreamByteOrderLittleEndian, sizeof(UInt32)*8);
#elif __ppc__
	setProperty(kIOAudioEngineFlavorKey, (unsigned long long)kIOAudioStreamByteOrderBigEndian, sizeof(UInt32)*8);
#endif

    return true;
}

void IOAudioEngine::free()
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::free()", this);

	if (reserved->statusDescriptor) {
		reserved->statusDescriptor->release();
		reserved->statusDescriptor = NULL;
		status = NULL;
	}

	if (reserved->bytesInInputBufferArrayDescriptor) {
		reserved->bytesInInputBufferArrayDescriptor->release();
		reserved->bytesInInputBufferArrayDescriptor = NULL;
	}

	if (reserved->bytesInOutputBufferArrayDescriptor) {
		reserved->bytesInOutputBufferArrayDescriptor->release();
		reserved->bytesInOutputBufferArrayDescriptor = NULL;
	}

	if (reserved->streams) {
		reserved->streams->release();
		reserved->streams = NULL;
	}
	
    if (outputStreams) {
        outputStreams->release();
        outputStreams = NULL;
    }

    if (inputStreams) {
        inputStreams->release();
        inputStreams = NULL;
    }

    if (userClients) {
        userClients->release();
        userClients = NULL;
    }
    
    if (defaultAudioControls) {
        removeAllDefaultAudioControls();
        defaultAudioControls->release();
        defaultAudioControls = NULL;
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

	if (reserved) {
		IOFree (reserved, sizeof(struct ExpansionData));
	}

    super::free();
}

bool IOAudioEngine::initHardware(IOService *provider)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::initHardware(%p)", this, provider);

    return true;
}

bool IOAudioEngine::start(IOService *provider)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::start(%p)", this, provider);

    return start(provider, OSDynamicCast(IOAudioDevice, provider));
}

bool IOAudioEngine::start(IOService *provider, IOAudioDevice *device)
{
    bool result = true;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::start(%p, %p)", this, provider, device);

    if (!super::start(provider)) {
        return false;
    }

    if (!device) {
        return false;
    }

	
    setAudioDevice(device);
    
    workLoop = audioDevice->getWorkLoop();
    if (!workLoop) {
        return false;
    }
    workLoop->retain();

    commandGate = IOCommandGate::commandGate(this);
    if (!commandGate) {
        return false;
    }
    
    workLoop->addEventSource(commandGate);
    
	// for 2761764 & 3111501
	setWorkLoopOnAllAudioControls(workLoop);
	
    result = initHardware(provider);
    
    duringStartup = false;
        
    return result;
}

void IOAudioEngine::stop(IOService *provider)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::stop(%p)", this, provider);

    if (commandGate) {
        commandGate->runAction(detachUserClientsAction);
    }
    
    stopAudioEngine();

    detachAudioStreams();
    removeAllDefaultAudioControls();

	// <rdar://7233118>, <rdar://7029696> Remove the event source here as performing heavy workloop operation in free() could lead
	// to deadlock since the context which free() is called is not known. stop() is called on the workloop, so it is safe to remove 
	// the event source here.
    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }
        
        commandGate->release();
        commandGate = NULL;
    }

    super::stop(provider);
}

IOWorkLoop *IOAudioEngine::getWorkLoop() const
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::getWorkLoop()", this);

    return workLoop;
}

IOCommandGate *IOAudioEngine::getCommandGate() const
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::getCommandGate()", this);

    return commandGate;
}

void IOAudioEngine::registerService(IOOptionBits options)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::registerService(0x%lx)", this, options);

    if (!isRegistered) {
        OSCollectionIterator *iterator;
        IOAudioStream *stream;
        
        updateChannelNumbers();
        
        super::registerService(options);

        if (outputStreams && (outputStreams->getCount() > 0)) {
            iterator = OSCollectionIterator::withCollection(outputStreams);
            if (iterator) {
                while (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) {
                    stream->registerService();
                }
                iterator->release();
            }
        }

        if (inputStreams && (inputStreams->getCount() > 0)) {
            iterator = OSCollectionIterator::withCollection(inputStreams);
            if (iterator) {
                while (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) {
                    stream->registerService();
                }
                iterator->release();
            }
        }

        isRegistered = true;
        
    }
}

OSString *IOAudioEngine::getGlobalUniqueID()
{
    const OSMetaClass *metaClass;
    const char *className = NULL;
    const char *location = NULL;
    char *uniqueIDStr;
    OSString *localID = NULL;
    OSString *uniqueID = NULL;
    UInt32 uniqueIDSize;
    
    metaClass = getMetaClass();
    if (metaClass) {
        className = metaClass->getClassName();
    }
    
    location = getLocation();
    
    localID = getLocalUniqueID();
    
    uniqueIDSize = 3;
    
    if (className) {
        uniqueIDSize += strlen(className);
    }
    
    if (location) {
        uniqueIDSize += strlen(location);
    }
    
    if (localID) {
        uniqueIDSize += localID->getLength();
    }
        
    uniqueIDStr = (char *)IOMallocAligned(uniqueIDSize, sizeof (char));
    
    if (uniqueIDStr) {
		bzero(uniqueIDStr, uniqueIDSize);

        if (className) {
            snprintf(uniqueIDStr, uniqueIDSize, "%s:", className);
        }
        
        if (location) {
            strncat(uniqueIDStr, location, uniqueIDSize);
            strncat(uniqueIDStr, ":", uniqueIDSize);
        }
        
        if (localID) {
            strncat(uniqueIDStr, localID->getCStringNoCopy(), uniqueIDSize);
            localID->release();
        }
        
        uniqueID = OSString::withCString(uniqueIDStr);
        
        IOFreeAligned(uniqueIDStr, uniqueIDSize);
    }

    return uniqueID;
}

OSString *IOAudioEngine::getLocalUniqueID()
{
    OSString *localUniqueID;
	int strSize = (sizeof(UInt32)*2)+1;
    char localUniqueIDStr[strSize];
   
    snprintf(localUniqueIDStr, strSize, "%lx", (long unsigned int)index);
    
    localUniqueID = OSString::withCString(localUniqueIDStr);
    
    return localUniqueID;
}

void IOAudioEngine::setIndex(UInt32 newIndex)
{
    OSString *uniqueID;
    
    index = newIndex;
    
    uniqueID = getGlobalUniqueID();
    if (uniqueID) {
        setProperty(kIOAudioEngineGlobalUniqueIDKey, uniqueID);
        uniqueID->release();
    }
}

void IOAudioEngine::setAudioDevice(IOAudioDevice *device)
{
    audioDevice = device;
}

void IOAudioEngine::setDescription(const char *description)
{
    if (description) {
        setProperty(kIOAudioEngineDescriptionKey, description);
    }
}

void IOAudioEngine::resetStatusBuffer()
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::resetStatusBuffer()", this);

    assert(status);
    
    status->fCurrentLoopCount = 0;
    
#if __LP64__
	status->fLastLoopTime = 0;
#else
	status->fLastLoopTime.hi = 0;
	status->fLastLoopTime.lo = 0;
#endif

    status->fEraseHeadSampleFrame = 0;
    
    stopEngineAtPosition(NULL);
    
    return;
}

void IOAudioEngine::clearAllSampleBuffers()
{
    OSCollectionIterator *iterator;
    IOAudioStream *stream;
    
    if (outputStreams && (outputStreams->getCount() > 0)) {
        iterator = OSCollectionIterator::withCollection(outputStreams);
        if (iterator) {
            while (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) {
                stream->clearSampleBuffer();
            }
            iterator->release();
        }
    }
    
    if (inputStreams && (inputStreams->getCount() > 0)) {
        iterator = OSCollectionIterator::withCollection(inputStreams);
        if (iterator) {
            while (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) {
                stream->clearSampleBuffer();
            }
            iterator->release();
        }
    }
}

IOReturn IOAudioEngine::createUserClient(task_t task, void *securityID, UInt32 type, IOAudioEngineUserClient **newUserClient)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioEngineUserClient *userClient;
    
    userClient = IOAudioEngineUserClient::withAudioEngine(this, task, securityID, type);
    
    if (userClient) {
        *newUserClient = userClient;
    } else {
        result = kIOReturnNoMemory;
    }
    
    return result;
}

IOReturn IOAudioEngine::newUserClient(task_t task, void *securityID, UInt32 type, IOUserClient **handler)
{
#if __i386__ || __x86_64__
    return kIOReturnUnsupported;
#else
    IOReturn				result = kIOReturnSuccess;
    IOAudioEngineUserClient	*client;

    audioDebugIOLog(3, "IOAudioEngine[%p]::newUserClient(0x%x, %p, 0x%lx, %p)", this, (unsigned int)task, securityID, type, handler);

    if (!isInactive()) {
        result = createUserClient(task, securityID, type, &client);
    
        if ((result == kIOReturnSuccess) && (client != NULL)) {
            if (!client->attach(this)) {
                client->release();
                result = kIOReturnError;
            } else if (!client->start(this)) {
                client->detach(this);
                client->release();
                result = kIOReturnError;
            } else {
                assert(workLoop);	// <rdar://7324947>
    
                result = workLoop->runAction(_addUserClientAction, this, client);	// <rdar://7324947>, <rdar://7529580>
                
                if (result == kIOReturnSuccess) {
                    *handler = client;
                }
			}
        } else {
            result = kIOReturnNoMemory;
        }
    } else {
        result = kIOReturnNoDevice;
    }
	
    return result;
#endif
}

IOReturn IOAudioEngine::newUserClient(task_t task, void *securityID, UInt32 type, OSDictionary *properties, IOUserClient **handler)
{
    IOReturn				result = kIOReturnSuccess;
    IOAudioEngineUserClient	*client;
	
	if (kIOReturnSuccess == newUserClient(task, securityID, type, handler)) {
		return kIOReturnSuccess;
	}
	
    audioDebugIOLog(3, "IOAudioEngine[%p]::newUserClient(0x%p, %p, 0x%lx, %p, %p)", this, task, securityID, type, properties, handler);
	
    if (!isInactive()) {
        result = createUserClient(task, securityID, type, &client, properties);
    
        if ((result == kIOReturnSuccess) && (client != NULL)) {
            if (!client->attach(this)) {
                client->release();
                result = kIOReturnError;
            } else if (!client->start(this)) {
                client->detach(this);
                client->release();
                result = kIOReturnError;
            } else {
                assert(workLoop);	// <rdar://7324947>
    
                result = workLoop->runAction(_addUserClientAction, this, client);	// <rdar://7324947>, <rdar://7529580>
                
                if (result == kIOReturnSuccess) {
                    *handler = client;
                }
			}
        } else {
            result = kIOReturnNoMemory;
        }
    } else {
        result = kIOReturnNoDevice;
    }

    return result;
}

void IOAudioEngine::clientClosed(IOAudioEngineUserClient *client)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::clientClosed(%p)", this, client);

    if (client) {
        assert(workLoop);												// <rdar://7529580>

        workLoop->runAction(_removeUserClientAction, this, client);		//	<rdar://7529580>
    }
}

// <rdar://7529580>
IOReturn IOAudioEngine::_addUserClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, target);
        if (audioEngine) {
            IOCommandGate *cg;
            
            cg = audioEngine->getCommandGate();
            
            if (cg) {
                result = cg->runAction(addUserClientAction, arg0, arg1, arg2, arg3);
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngine::addUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    audioDebugIOLog(3, "IOAudioEngine::addUserClientAction(%p, %p)", owner, arg1);

    if (owner) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, owner);
        if (audioEngine) {
            result = audioEngine->addUserClient((IOAudioEngineUserClient *)arg1);
        }
    }
    
    return result;
}

// <rdar://7529580>
IOReturn IOAudioEngine::_removeUserClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, target);
        if (audioEngine) {
            IOCommandGate *cg;
            
            cg = audioEngine->getCommandGate();
            
            if (cg) {
                result = cg->runAction(removeUserClientAction, arg0, arg1, arg2, arg3);
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngine::removeUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    audioDebugIOLog(3, "IOAudioEngine::removeUserClientAction(%p, %p)", owner, arg1);

    if (owner) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, owner);
        if (audioEngine) {
            result = audioEngine->removeUserClient((IOAudioEngineUserClient *)arg1);
        }
    }
    
    return result;
}

IOReturn IOAudioEngine::detachUserClientsAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, owner);
        if (audioEngine) {
            result = audioEngine->detachUserClients();
        }
    }
    
    return result;
}

IOReturn IOAudioEngine::addUserClient(IOAudioEngineUserClient *newUserClient)
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::addUserClient(%p)", this, newUserClient);

    assert(userClients);
    
    userClients->setObject(newUserClient);
    
    return result;
}

IOReturn IOAudioEngine::removeUserClient(IOAudioEngineUserClient *userClient)
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::removeUserClient(%p)", this, userClient);

    assert(userClients);
    
    userClient->retain();
    
    userClients->removeObject(userClient);
    
    if (userClient->isOnline()) {
        decrementActiveUserClients();
    }
    
    if (!isInactive()) {
        userClient->terminate();
    }
    
    userClient->release();
    
    return result;
}

IOReturn IOAudioEngine::detachUserClients()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::detachUserClients", this);

    assert(userClients);
    
    if (!isInactive()) {	// Iterate through and terminate each user client
        OSIterator *iterator;
        
        iterator = OSCollectionIterator::withCollection(userClients);
        
        if (iterator) {
            IOAudioEngineUserClient *userClient;
            
            while (userClient = (IOAudioEngineUserClient *)iterator->getNextObject()) {
                userClient->terminate();
            }
            iterator->release();
        }
    }
    
    userClients->flushCollection();
    
    if (getState() == kIOAudioEngineRunning) {
        IOAudioEnginePosition stopPosition;
        
        assert(status);
        
        stopPosition.fSampleFrame = getCurrentSampleFrame();
        stopPosition.fLoopCount = status->fCurrentLoopCount + 1;

        stopEngineAtPosition(&stopPosition);
    }
    
    return result;
}

IOReturn IOAudioEngine::startClient(IOAudioEngineUserClient *userClient)
{
    IOReturn result = kIOReturnBadArgument;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::startClient(%p)", this, userClient);

	while ( audioDevice->getPowerState() == kIOAudioDeviceSleep )
	{
		retain();
		
		commandGate->commandSleep( &audioDevice->currentPowerState );
		
		if ( isInactive() )
		{
			release();
			return kIOReturnNoDevice;
		}
		
		release();
	}

    if (userClient) {
        result = incrementActiveUserClients();
    }
    
    return result;
}

IOReturn IOAudioEngine::stopClient(IOAudioEngineUserClient *userClient)
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::stopClient(%p)", this, userClient);

    if (userClient) {
        if (userClient->isOnline()) {
            result = decrementActiveUserClients();
        }
    } else {
        result = kIOReturnBadArgument;
    }
    
    return result;
}
    
IOReturn IOAudioEngine::incrementActiveUserClients()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::incrementActiveUserClients() - %ld", this, numActiveUserClients);
    
    numActiveUserClients++;

    setProperty(kIOAudioEngineNumActiveUserClientsKey, numActiveUserClients, sizeof(UInt32)*8);

    if (numActiveUserClients == 1) {
        result = startAudioEngine();
    }
    
	if (result != kIOReturnSuccess) {
		decrementActiveUserClients();
	}
	
    return result;
}

IOReturn IOAudioEngine::decrementActiveUserClients()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::decrementActiveUserClients() - %ld", this, numActiveUserClients);
    
    numActiveUserClients--;
    
    setProperty(kIOAudioEngineNumActiveUserClientsKey, numActiveUserClients, sizeof(UInt32)*8);

    if ((numActiveUserClients == 0) && (getState() == kIOAudioEngineRunning)) {
        IOAudioEnginePosition stopPosition;
        
        assert(status);
        
        stopPosition.fSampleFrame = getCurrentSampleFrame();
        stopPosition.fLoopCount = status->fCurrentLoopCount + 1;

        stopEngineAtPosition(&stopPosition);
    }
    
    return result;
}

IOReturn IOAudioEngine::addAudioStream(IOAudioStream *stream)
{
    IOReturn result = kIOReturnBadArgument;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::addAudioStream(%p)", this, stream);

    if (stream) {

        if (!stream->attach(this)) {
            return kIOReturnError;
        }

        if (!stream->start(this)) {
            stream->detach(this);
            return kIOReturnError;
        }
        
        switch (stream->getDirection()) {
            case kIOAudioStreamDirectionOutput:
                assert(outputStreams);

                outputStreams->setObject(stream);
                
                maxNumOutputChannels += stream->getMaxNumChannels();
                
                if (outputStreams->getCount() == 1) {
                    setRunEraseHead(true);
                }

				if (reserved->bytesInOutputBufferArrayDescriptor) {
					reserved->bytesInOutputBufferArrayDescriptor->release();
				}
				reserved->bytesInOutputBufferArrayDescriptor = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn | kIOMemoryKernelUserShared, round_page(outputStreams->getCount() * sizeof(UInt32)), page_size);
                break;
            case kIOAudioStreamDirectionInput:
                assert(inputStreams);
                
                inputStreams->setObject(stream);
                
                maxNumInputChannels += stream->getMaxNumChannels();

				if (reserved->bytesInInputBufferArrayDescriptor) {
					reserved->bytesInInputBufferArrayDescriptor->release();
				}
				reserved->bytesInInputBufferArrayDescriptor = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn | kIOMemoryKernelUserShared, round_page(inputStreams->getCount() * sizeof(UInt32)), page_size);
                break;
        }

        if (isRegistered) {
            stream->registerService();
        }
        
        result = kIOReturnSuccess;
    }

    return result;
}

void IOAudioEngine::detachAudioStreams()
{
    OSCollectionIterator *iterator;
    IOAudioStream *stream;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::detachAudioStreams()", this);

    if (outputStreams && (outputStreams->getCount() > 0)) {
        iterator = OSCollectionIterator::withCollection(outputStreams);
        if (iterator) {
            while (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) {
                if (!isInactive()) {
                    stream->terminate();
                }
            }
            iterator->release();
        }
        outputStreams->flushCollection();
		if (reserved->bytesInOutputBufferArrayDescriptor) {
			reserved->bytesInOutputBufferArrayDescriptor->release();
			reserved->bytesInOutputBufferArrayDescriptor = NULL;
		}
    }
    
    if (inputStreams && (inputStreams->getCount() > 0)) {
        iterator = OSCollectionIterator::withCollection(inputStreams);
        if (iterator) {
            while (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) {
                if (!isInactive()) {
                    stream->terminate();
                }
            }
            iterator->release();
        }
        inputStreams->flushCollection();
		if (reserved->bytesInInputBufferArrayDescriptor) {
			reserved->bytesInInputBufferArrayDescriptor->release();
			reserved->bytesInInputBufferArrayDescriptor = NULL;
		}
    }

	if (reserved->streams) {
		reserved->streams->flushCollection();
	}
}

void IOAudioEngine::lockAllStreams()
{
    OSCollectionIterator *streamIterator;
    
    if (outputStreams) {
        streamIterator = OSCollectionIterator::withCollection(outputStreams);
        if (streamIterator) {
            IOAudioStream *stream;
            
            while (stream = (IOAudioStream *)streamIterator->getNextObject()) {
                stream->lockStreamForIO();
            }
            streamIterator->release();
        }
    }

    if (inputStreams) {
        streamIterator = OSCollectionIterator::withCollection(inputStreams);
        if (streamIterator) {
            IOAudioStream *stream;
            
            while (stream = (IOAudioStream *)streamIterator->getNextObject()) {
                stream->lockStreamForIO();
            }
            streamIterator->release();
        }
    }
}

void IOAudioEngine::unlockAllStreams()
{
    OSCollectionIterator *streamIterator;
    
    if (outputStreams) {
        streamIterator = OSCollectionIterator::withCollection(outputStreams);
        if (streamIterator) {
            IOAudioStream *stream;
            
            while (stream = (IOAudioStream *)streamIterator->getNextObject()) {
                stream->unlockStreamForIO();
            }
            streamIterator->release();
        }
    }

    if (inputStreams) {
        streamIterator = OSCollectionIterator::withCollection(inputStreams);
        if (streamIterator) {
            IOAudioStream *stream;
            
            while (stream = (IOAudioStream *)streamIterator->getNextObject()) {
                stream->unlockStreamForIO();
            }
            streamIterator->release();
        }
    }
}

IOAudioStream *IOAudioEngine::getAudioStream(IOAudioStreamDirection direction, UInt32 channelID)
{
    IOAudioStream *audioStream = NULL;
    OSCollection *streamCollection = NULL;
    
    if (direction == kIOAudioStreamDirectionOutput) {
        streamCollection = outputStreams;
    } else {	// input
        streamCollection = inputStreams;
    }
    
    if (streamCollection) {
        OSCollectionIterator *streamIterator;
        
        streamIterator = OSCollectionIterator::withCollection(streamCollection);
        if (streamIterator) {
            IOAudioStream *stream;
            
            while (stream = (IOAudioStream *)streamIterator->getNextObject()) {
                if ((channelID >= stream->startingChannelID) && (channelID < (stream->startingChannelID + stream->maxNumChannels))) {
                    audioStream = stream;
                    break;
                }
            }
            streamIterator->release();
        }
    }
    
    return audioStream;
}

void IOAudioEngine::updateChannelNumbers()
{
    OSCollectionIterator *iterator;
    SInt32 *outputChannelNumbers = NULL, *inputChannelNumbers = NULL;
    UInt32 currentChannelID;
    SInt32 currentChannelNumber;
   
	// BEGIN <rdar://6997438> maxNumOutputChannels may not represent the true number of output channels at this point
	//					because the the number of formats in the stream may have changed. We recalculate the correct value here.
	
	maxNumOutputChannels = 0;
	maxNumInputChannels = 0;
    assert(outputStreams);
    assert(inputStreams);

    if (outputStreams->getCount() > 0) {
        iterator = OSCollectionIterator::withCollection(outputStreams);
        if (iterator) {
            IOAudioStream *audioStream;
            
            while (audioStream = (IOAudioStream *)iterator->getNextObject()) {
				maxNumOutputChannels += audioStream->getMaxNumChannels();
			}
		}
	}

	if (inputStreams->getCount() > 0) {
        iterator = OSCollectionIterator::withCollection(inputStreams);
        if (iterator) {
            IOAudioStream *audioStream;
            
            while (audioStream = (IOAudioStream *)iterator->getNextObject()) {
				maxNumInputChannels += audioStream->getMaxNumChannels();
			}
		}
	}
	// END <rdar://6997438>
	
	audioDebugIOLog(3, "IOAudioEngine[%p]::updateChannelNumbers() - o=%ld i=%ld", this, maxNumOutputChannels, maxNumInputChannels);
	
    if (maxNumOutputChannels > 0) {
        outputChannelNumbers = (SInt32 *)IOMallocAligned(maxNumOutputChannels * sizeof(SInt32), sizeof (SInt32));
    }
    
    if (maxNumInputChannels > 0) {
        inputChannelNumbers = (SInt32 *)IOMallocAligned(maxNumInputChannels * sizeof(SInt32), sizeof (SInt32));
    }
    
    currentChannelID = 1;
    currentChannelNumber = 1;

    if (outputStreams->getCount() > 0) {
        iterator = OSCollectionIterator::withCollection(outputStreams);
        if (iterator) {
            IOAudioStream *audioStream;
            
            while (audioStream = (IOAudioStream *)iterator->getNextObject()) {
                const IOAudioStreamFormat *format;
                
                format = audioStream->getFormat();
                if (format) {
                    UInt32 numChannels, maxNumChannels;
                    UInt32 i;
                    
                    numChannels = format->fNumChannels;
                    maxNumChannels = audioStream->getMaxNumChannels();
                    
//                    assert(currentChannelID + maxNumChannels <= maxNumOutputChannels);		// double check that this calc is right.  MPC
                    
                    if (audioStream->getStreamAvailable()) {
                        audioStream->setStartingChannelNumber(currentChannelNumber);
                    } else {
                        numChannels = 0;
                        audioStream->setStartingChannelNumber(0);
                    }
                    
                    for (i = 0; i < numChannels; i++) {
                        outputChannelNumbers[currentChannelID + i - 1] = currentChannelNumber + i;
                    }
                    
                    for (i = numChannels; i < maxNumChannels; i++) {
                        outputChannelNumbers[currentChannelID + i - 1] = kIOAudioControlChannelNumberInactive;
                    }

                    currentChannelID += maxNumChannels;
                    currentChannelNumber += numChannels;
                }
            }
            
            iterator->release();
        }
    }
    
    currentChannelID = 1;
    currentChannelNumber = 1;
    
    if (inputStreams->getCount() > 0) {
        iterator = OSCollectionIterator::withCollection(inputStreams);
        if (iterator) {
            IOAudioStream *audioStream;
            
            while (audioStream = (IOAudioStream *)iterator->getNextObject()) {
                const IOAudioStreamFormat *format;
                
                format = audioStream->getFormat();
                if (format) {
                    UInt32 numChannels, maxNumChannels;
                    UInt32 i;
                    
                    numChannels = format->fNumChannels;
                    maxNumChannels = audioStream->getMaxNumChannels();
                    
//                    assert(currentChannelID + maxNumChannels <= maxNumInputChannels);		// double check that this calc is right.  MPC
                    
                    if (audioStream->getStreamAvailable()) {
                        audioStream->setStartingChannelNumber(currentChannelNumber);
                    } else {
                        numChannels = 0;
                        audioStream->setStartingChannelNumber(0);
                    }
                    
                    for (i = 0; i < numChannels; i++) {
                        inputChannelNumbers[currentChannelID + i - 1] = currentChannelNumber + i;
                    }
                    
                    for (i = numChannels; i < maxNumChannels; i++) {
                        inputChannelNumbers[currentChannelID + i - 1] = kIOAudioControlChannelNumberInactive;
                    }
                    
                    currentChannelID += maxNumChannels;
                    currentChannelNumber += numChannels;
                }
            }
            
            iterator->release();
        }
    }
    
    if (defaultAudioControls) {
        iterator = OSCollectionIterator::withCollection(defaultAudioControls);
        if (iterator) {
            IOAudioControl *control;
            while (control = (IOAudioControl *)iterator->getNextObject()) {
                UInt32 channelID;
                
                channelID = control->getChannelID();
                
                if (channelID != 0) {
                    switch (control->getUsage()) {
                        case kIOAudioControlUsageOutput:
								if (outputChannelNumbers && (channelID <= maxNumOutputChannels)) {
									control->setChannelNumber(outputChannelNumbers[channelID - 1]);
								} else {
									control->setChannelNumber(kIOAudioControlChannelNumberInactive);
							}
                            break;
                        case kIOAudioControlUsageInput:
							if (inputChannelNumbers && (channelID <= maxNumInputChannels)) {
								control->setChannelNumber(inputChannelNumbers[channelID - 1]);
							} else {
								control->setChannelNumber(kIOAudioControlChannelNumberInactive);
							}
                            break;
                        case kIOAudioControlUsagePassThru:
                            if (inputChannelNumbers) {
                                if (channelID <= maxNumInputChannels) {
                                    control->setChannelNumber(inputChannelNumbers[channelID - 1]);
                                } else {
                                    control->setChannelNumber(kIOAudioControlChannelNumberInactive);
                                }
                            } else if (outputChannelNumbers) {
                                if (channelID <= maxNumOutputChannels) {
                                    control->setChannelNumber(outputChannelNumbers[channelID - 1]);
                                } else {
                                    control->setChannelNumber(kIOAudioControlChannelNumberInactive);
                                }
                            } else {
                                control->setChannelNumber(kIOAudioControlChannelNumberInactive);
                            }
                            break;
                        default:
                            break;
                    }
                } else {
                    control->setChannelNumber(0);
                }
            }
            iterator->release();
        }
    }
    
    if (outputChannelNumbers && (maxNumOutputChannels > 0)) {
        IOFreeAligned(outputChannelNumbers, maxNumOutputChannels * sizeof(SInt32));
    }
    
    if (inputChannelNumbers && (maxNumInputChannels > 0)) {
        IOFreeAligned(inputChannelNumbers, maxNumInputChannels * sizeof(SInt32));
    }
}

IOReturn IOAudioEngine::startAudioEngine()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::startAudioEngine()", this);

    switch(getState()) {
        case kIOAudioEnginePaused:
            result = resumeAudioEngine();
            break;
        case kIOAudioEngineStopped:
            audioDevice->audioEngineStarting();
        case kIOAudioEngineResumed:
            resetStatusBuffer();
            
			reserved->pauseCount = 0;
            result = performAudioEngineStart();
            if (result == kIOReturnSuccess) {
                setState(kIOAudioEngineRunning);
                sendNotification(kIOAudioEngineStartedNotification);
            } else if (getState() == kIOAudioEngineStopped) {
                audioDevice->audioEngineStopped();
            }
            break;
        default:
            break;
    }
    
    return result;
}

IOReturn IOAudioEngine::stopAudioEngine()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::stopAudioEngine()", this);

    switch (getState()) {
        case kIOAudioEngineRunning:
            result = performAudioEngineStop();
        case kIOAudioEngineResumed:
            if (result == kIOReturnSuccess) {
                setState(kIOAudioEngineStopped);
                sendNotification(kIOAudioEngineStoppedNotification);
    
                assert(audioDevice);
                audioDevice->audioEngineStopped();
            }
            break;
        default:
            break;
    }
        
    return result;
}

IOReturn IOAudioEngine::pauseAudioEngine()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::pauseAudioEngine()", this);

	reserved->pauseCount++;
    switch(getState()) {
        case kIOAudioEngineRunning:
        case kIOAudioEngineResumed:
            // We should probably have the streams locked around performAudioEngineStop()
            // but we can't ensure that it won't make a call out that would attempt to take
            // one of the clientBufferLocks on an IOAudioEngineUserClient
            // If it did, that would create the potential for a deadlock
            result = performAudioEngineStop();
            if (result == kIOReturnSuccess) {
                lockAllStreams();
                setState(kIOAudioEnginePaused);
                unlockAllStreams();
				sendNotification(kIOAudioEnginePausedNotification);
                
                clearAllSampleBuffers();
            }
            break;
        default:
            break;
    }
    
    return result;
}

IOReturn IOAudioEngine::resumeAudioEngine()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::resumeAudioEngine()", this);
	
	if (0 != reserved->pauseCount) {
		if (0 == --reserved->pauseCount) {
			if (getState() == kIOAudioEnginePaused) {
				setState(kIOAudioEngineResumed);
				sendNotification(kIOAudioEngineResumedNotification);
			}
		}
	}
	else {
		audioDebugIOLog(1, "IOAudioEngine[%p]::resumeAudioEngine() - attempting to resume while not paused", this);
	}
    
    return result;
}

IOReturn IOAudioEngine::performAudioEngineStart()
{
    return kIOReturnSuccess;
}

IOReturn IOAudioEngine::performAudioEngineStop()
{
    return kIOReturnSuccess;
}

const IOAudioEngineStatus *IOAudioEngine::getStatus()
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::getStatus()", this);

    return status;
}

void IOAudioEngine::setNumSampleFramesPerBuffer(UInt32 numSampleFrames)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::setNumSampleFramesPerBuffer(0x%lx)", this, numSampleFrames);

    if (getState() == kIOAudioEngineRunning) {
        IOLog("IOAudioEngine[%p]::setNumSampleFramesPerBuffer(0x%ld) - Error: can't change num sample frames while engine is running.\n", this, (long int) numSampleFrames);
    } else {
        numSampleFramesPerBuffer = numSampleFrames;
        setProperty(kIOAudioEngineNumSampleFramesPerBufferKey, numSampleFramesPerBuffer, sizeof(UInt32)*8);
        
        // Notify all output streams
        if (outputStreams) {
            OSCollectionIterator *streamIterator;
            
            streamIterator = OSCollectionIterator::withCollection(outputStreams);
            if (streamIterator) {
                IOAudioStream *audioStream;
                
                while (audioStream = (IOAudioStream *)streamIterator->getNextObject()) {
                    audioStream->numSampleFramesPerBufferChanged();
                }
                streamIterator->release();
            }
        }
    }
}

UInt32 IOAudioEngine::getNumSampleFramesPerBuffer()
{
    audioDebugIOLog(7, "IOAudioEngine[%p]::getNumSampleFramesPerBuffer()", this);

    return numSampleFramesPerBuffer;
}

IOAudioEngineState IOAudioEngine::getState()
{
    return state;
}

IOAudioEngineState IOAudioEngine::setState(IOAudioEngineState newState)
{
    IOAudioEngineState oldState;

    audioDebugIOLog(3, "IOAudioEngine[%p]::setState(0x%x)", this, newState);

    oldState = state;
    state = newState;

    switch (state) {
        case kIOAudioEngineRunning:
            if (oldState != kIOAudioEngineRunning) {
                addTimer();
            }
            break;
        case kIOAudioEngineStopped:
            if (oldState == kIOAudioEngineRunning) {
                removeTimer();
                performErase();
            }
            break;
        default:
            break;
    }

    setProperty(kIOAudioEngineStateKey, newState, sizeof(UInt32)*8);

    return oldState;
}

const IOAudioSampleRate *IOAudioEngine::getSampleRate()
{
    return &sampleRate;
}

void IOAudioEngine::setSampleRate(const IOAudioSampleRate *newSampleRate)
{
    OSDictionary *sampleRateDict;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::setSampleRate(%p)", this, newSampleRate);

    sampleRate = *newSampleRate;
    
    sampleRateDict = createDictionaryFromSampleRate(&sampleRate);
    if (sampleRateDict) {
        setProperty(kIOAudioSampleRateKey, sampleRateDict);
        sampleRateDict->release();
    }
}

IOReturn IOAudioEngine::hardwareSampleRateChanged(const IOAudioSampleRate *newSampleRate)
{
    if ((newSampleRate->whole != sampleRate.whole) || (newSampleRate->fraction != sampleRate.fraction)) {
        bool engineWasRunning;
        
        engineWasRunning = (state == kIOAudioEngineRunning);
        
        if (engineWasRunning) {
            pauseAudioEngine();
        }
        
        setSampleRate(newSampleRate);
        if (!configurationChangeInProgress) {
            sendNotification(kIOAudioEngineChangeNotification);
        }
        
        if (engineWasRunning) {
            resumeAudioEngine();
        }
    }
    
    return kIOReturnSuccess;
}

void IOAudioEngine::setSampleLatency(UInt32 numSamples)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::setSampleLatency(0x%lx)", this, numSamples);
    setOutputSampleLatency(numSamples);
    setInputSampleLatency(numSamples);
}

void IOAudioEngine::setOutputSampleLatency(UInt32 numSamples)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::setOutputSampleLatency(0x%lx)", this, numSamples);
    setProperty(kIOAudioEngineOutputSampleLatencyKey, numSamples, sizeof(UInt32)*8);
}

void IOAudioEngine::setInputSampleLatency(UInt32 numSamples)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::setInputSampleLatency(0x%lx)", this, numSamples);
    setProperty(kIOAudioEngineInputSampleLatencyKey, numSamples, sizeof(UInt32)*8);
}

void IOAudioEngine::setSampleOffset(UInt32 numSamples)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::setSampleOffset(0x%lx)", this, numSamples);
    sampleOffset = numSamples;
    setProperty(kIOAudioEngineSampleOffsetKey, numSamples, sizeof(UInt32)*8);
}

void IOAudioEngine::setRunEraseHead(bool erase)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::setRunEraseHead(%d)", this, erase);
    runEraseHead = erase;
}

bool IOAudioEngine::getRunEraseHead()
{
    audioDebugIOLog(7, "IOAudioEngine[%p]::getRunEraseHead()", this);

    return runEraseHead;
}

AbsoluteTime IOAudioEngine::getTimerInterval()
{
    AbsoluteTime interval;
    const IOAudioSampleRate *currentRate;
    
    audioDebugIOLog(3, "IOAudioEngine[%p]::getTimerInterval()", this);

    assert(status);

    currentRate = getSampleRate();
    
    if ((getNumSampleFramesPerBuffer() == 0) || (currentRate && (currentRate->whole == 0))) {
        nanoseconds_to_absolutetime(NSEC_PER_SEC, &interval);
    } else if ((numErasesPerBuffer == 0) || (!getRunEraseHead())) {	// Run once per ring buffer
        nanoseconds_to_absolutetime(((UInt64)NSEC_PER_SEC * (UInt64)getNumSampleFramesPerBuffer() / (UInt64)currentRate->whole), &interval);
    } else {
        OSCollectionIterator *outputIterator;
        IOAudioStream *outputStream;
		UInt32 bufferSize;
		UInt32 newNumErasesPerBuffer;

		outputIterator = OSCollectionIterator::withCollection(outputStreams);

		if (outputIterator) 
		{
			while (outputStream = (IOAudioStream *)outputIterator->getNextObject()) {
				bufferSize = outputStream->getSampleBufferSize();
				if ((bufferSize / numErasesPerBuffer) > 65536) {
					newNumErasesPerBuffer = bufferSize / 65536;
					if (newNumErasesPerBuffer > numErasesPerBuffer) {
						numErasesPerBuffer = newNumErasesPerBuffer;
					}
				}
			}
			outputIterator->release();
		}
		nanoseconds_to_absolutetime(((UInt64)NSEC_PER_SEC * (UInt64)getNumSampleFramesPerBuffer() / (UInt64)currentRate->whole / (UInt64)numErasesPerBuffer), &interval);
    }
    return interval;
}

void IOAudioEngine::timerCallback(OSObject *target, IOAudioDevice *device)
{
    IOAudioEngine *audioEngine;

    audioDebugIOLog(7, "IOAudioEngine::timerCallback(%p, %p)", target, device);

    audioEngine = OSDynamicCast(IOAudioEngine, target);
    if (audioEngine) {
        audioEngine->timerFired();
    }
}

void IOAudioEngine::timerFired()
{
    audioDebugIOLog(7, "IOAudioEngine[%p]::timerFired()", this);

    performErase();
    performFlush();
}

void IOAudioEngine::performErase()
{
    audioDebugIOLog(7, "IOAudioEngine[%p]::performErase()", this);

    assert(status);
    
    if (getRunEraseHead() && getState() == kIOAudioEngineRunning) {
        OSCollectionIterator *outputIterator;
        IOAudioStream *outputStream;
        
        assert(outputStreams);
        
        outputIterator = OSCollectionIterator::withCollection(outputStreams);
        if (outputIterator) {
            UInt32 currentSampleFrame, eraseHeadSampleFrame;
            
            currentSampleFrame = getCurrentSampleFrame();
            eraseHeadSampleFrame = status->fEraseHeadSampleFrame;
                
            while (outputStream = (IOAudioStream *)outputIterator->getNextObject()) {
                char *sampleBuf, *mixBuf;
                UInt32 sampleBufferFrameSize, mixBufferFrameSize;
                
				outputStream->lockStreamForIO();

                sampleBuf = (char *)outputStream->getSampleBuffer();
                mixBuf = (char *)outputStream->getMixBuffer();
                
                sampleBufferFrameSize = outputStream->format.fNumChannels * outputStream->format.fBitWidth / 8;
                mixBufferFrameSize = outputStream->format.fNumChannels * kIOAudioEngineDefaultMixBufferSampleSize;
                
                if (currentSampleFrame < eraseHeadSampleFrame) {
                    audioDebugIOLog(7, "IOAudioEngine[%p]::performErase() - erasing from frame: 0x%lx to 0x%lx", this, eraseHeadSampleFrame, numSampleFramesPerBuffer);
                    audioDebugIOLog(7, "IOAudioEngine[%p]::performErase() - erasing from frame: 0x%x to 0x%lx", this, 0, currentSampleFrame);
					eraseOutputSamples(mixBuf, sampleBuf, 0, currentSampleFrame, &outputStream->format, outputStream);
					eraseOutputSamples(mixBuf, sampleBuf, eraseHeadSampleFrame, numSampleFramesPerBuffer - eraseHeadSampleFrame, &outputStream->format, outputStream);
/*					if ((currentSampleFrame * sampleBufferFrameSize <= outputStream->getSampleBufferSize()) &&
						((eraseHeadSampleFrame * sampleBufferFrameSize) + (numSampleFramesPerBuffer - eraseHeadSampleFrame) * sampleBufferFrameSize) <= outputStream->getSampleBufferSize()) {
						if (sampleBuf && (sampleBufferFrameSize > 0)) {
							bzero(sampleBuf, currentSampleFrame * sampleBufferFrameSize);
							bzero(sampleBuf + (eraseHeadSampleFrame * sampleBufferFrameSize), (numSampleFramesPerBuffer - eraseHeadSampleFrame) * sampleBufferFrameSize);
						}
					}
					if ((currentSampleFrame * mixBufferFrameSize <= outputStream->getMixBufferSize()) &&
						((eraseHeadSampleFrame * mixBufferFrameSize) + (numSampleFramesPerBuffer - eraseHeadSampleFrame) * mixBufferFrameSize) <= outputStream->getMixBufferSize()) {
						if (mixBuf && (mixBufferFrameSize > 0)) {
							bzero(mixBuf, currentSampleFrame * mixBufferFrameSize);
							bzero(mixBuf + (eraseHeadSampleFrame * mixBufferFrameSize), (numSampleFramesPerBuffer - eraseHeadSampleFrame) * mixBufferFrameSize);
						}
					} */
                } else {
                    audioDebugIOLog(7, "IOAudioEngine[%p]::performErase() - erasing from frame: 0x%lx to 0x%lx", this, eraseHeadSampleFrame, currentSampleFrame);

					eraseOutputSamples(mixBuf, sampleBuf, eraseHeadSampleFrame, currentSampleFrame - eraseHeadSampleFrame, &outputStream->format, outputStream);
/*					if ((eraseHeadSampleFrame * sampleBufferFrameSize + (currentSampleFrame - eraseHeadSampleFrame) * sampleBufferFrameSize) <= outputStream->getSampleBufferSize()) {
						if (sampleBuf && (sampleBufferFrameSize > 0)) {
							bzero(sampleBuf + (eraseHeadSampleFrame * sampleBufferFrameSize), (currentSampleFrame - eraseHeadSampleFrame) * sampleBufferFrameSize);
						}
					}
					if ((eraseHeadSampleFrame * mixBufferFrameSize + (currentSampleFrame - eraseHeadSampleFrame) * mixBufferFrameSize) <= outputStream->getMixBufferSize()) {
						if (mixBuf && (mixBufferFrameSize > 0)) {
							bzero(mixBuf + (eraseHeadSampleFrame * mixBufferFrameSize), (currentSampleFrame - eraseHeadSampleFrame) * mixBufferFrameSize);
						}
					} */
                }

				outputStream->unlockStreamForIO();
            }
            
            status->fEraseHeadSampleFrame = currentSampleFrame;
            
            outputIterator->release();
        }
    }
}

void IOAudioEngine::stopEngineAtPosition(IOAudioEnginePosition *endingPosition)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::stopEngineAtPosition(%lx,%lx)", this, endingPosition ? endingPosition->fLoopCount : 0, endingPosition ? endingPosition->fSampleFrame : 0);

    if (endingPosition) {
        audioEngineStopPosition = *endingPosition;
    } else {
        audioEngineStopPosition.fLoopCount = 0;
        audioEngineStopPosition.fSampleFrame = 0;
    }
}

void IOAudioEngine::performFlush()
{
    audioDebugIOLog(6, "IOAudioEngine[%p]::performFlush()", this);

    if ((numActiveUserClients == 0) && (getState() == kIOAudioEngineRunning)) {
        IOAudioEnginePosition currentPosition;
        
        assert(status);
        
        currentPosition.fLoopCount = status->fCurrentLoopCount;
        currentPosition.fSampleFrame = getCurrentSampleFrame();
        
        if (CMP_IOAUDIOENGINEPOSITION(&currentPosition, &audioEngineStopPosition) > 0) {
            stopAudioEngine();
        }
    }
}

void IOAudioEngine::addTimer()
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::addTimer()", this);

    if (!audioDevice) {
        return;
    }

    audioDevice->addTimerEvent(this, &IOAudioEngine::timerCallback, getTimerInterval());
}

void IOAudioEngine::removeTimer()
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::removeTimer()", this);

    if (!audioDevice) {
        return;
    }

    audioDevice->removeTimerEvent(this);
}

IOReturn IOAudioEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    audioDebugIOLog(6, "IOAudioEngine[%p]::clipOutputSamples(%p, %p, 0x%lx, 0x%lx, %p, %p)", this, mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);

    return kIOReturnUnsupported;
}

void IOAudioEngine::resetClipPosition(IOAudioStream *audioStream, UInt32 clipSampleFrame)
{
    audioDebugIOLog(6, "IOAudioEngine[%p]::resetClipPosition(%p, 0x%lx)", this, audioStream, clipSampleFrame);

    return;
}


IOReturn IOAudioEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    audioDebugIOLog(6, "IOAudioEngine[%p]::convertInputSamples(%p, %p, 0x%lx, 0x%lx, %p, %p)", this, sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);

    return kIOReturnUnsupported;
}

void IOAudioEngine::takeTimeStamp(bool incrementLoopCount, AbsoluteTime *timestamp)
{
    AbsoluteTime uptime, *ts;
    
    if (timestamp) {
        ts = timestamp;
    } else {
        clock_get_uptime(&uptime);
        ts = &uptime;
    }
    
    assert(status);
    
#if __LP64__
    status->fLastLoopTime = *ts;
#else
    status->fLastLoopTime.hi = ts->hi;
    status->fLastLoopTime.lo = ts->lo;
#endif
    
    if (incrementLoopCount) {
        ++status->fCurrentLoopCount;
    }
}

IOReturn IOAudioEngine::getLoopCountAndTimeStamp(UInt32 *loopCount, AbsoluteTime *timestamp)
{
    IOReturn result = kIOReturnBadArgument;
    UInt32 nextLoopCount;
    AbsoluteTime nextTimestamp;
    
    if (loopCount && timestamp) {
        assert(status);
        
#if __LP64__
		*timestamp = status->fLastLoopTime;
#else
        timestamp->hi = status->fLastLoopTime.hi;
		timestamp->lo = status->fLastLoopTime.lo;
#endif
        *loopCount = status->fCurrentLoopCount;
        
#if __LP64__
        nextTimestamp = status->fLastLoopTime;
#else
        nextTimestamp.hi = status->fLastLoopTime.hi;
        nextTimestamp.lo = status->fLastLoopTime.lo;
#endif
        nextLoopCount = status->fCurrentLoopCount;
        
        while ((*loopCount != nextLoopCount) || (CMP_ABSOLUTETIME(timestamp, &nextTimestamp) != 0)) {
            *timestamp = nextTimestamp;
            *loopCount = nextLoopCount;
            
#if __LP64__
            nextTimestamp = status->fLastLoopTime;

#else
            nextTimestamp.hi = status->fLastLoopTime.hi;
            nextTimestamp.lo = status->fLastLoopTime.lo;
#endif
            nextLoopCount = status->fCurrentLoopCount;
        }
        
        result = kIOReturnSuccess;
    }
    
    return result;
}

IOReturn IOAudioEngine::calculateSampleTimeout(AbsoluteTime *sampleInterval, UInt32 numSampleFrames, IOAudioEnginePosition *startingPosition, AbsoluteTime *wakeupTime)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (sampleInterval && (numSampleFrames != 0) && startingPosition) {
        IOAudioEnginePosition wakeupPosition;
        UInt32 wakeupOffset;
        AbsoluteTime lastLoopTime;
        UInt32 currentLoopCount;
        AbsoluteTime wakeupInterval;
        UInt64 wakeupIntervalScalar;
        UInt32 samplesFromLoopStart;
        AbsoluteTime wakeupThreadLatencyPaddingInterval;
        
        // Total wakeup interval now calculated at 90% minus 125us
        
        wakeupOffset = (numSampleFrames / reserved->mixClipOverhead) + sampleOffset;
        
        if (wakeupOffset <= startingPosition->fSampleFrame) {
            wakeupPosition = *startingPosition;
            wakeupPosition.fSampleFrame -= wakeupOffset;
        } else {
            wakeupPosition.fLoopCount = startingPosition->fLoopCount - 1;
            wakeupPosition.fSampleFrame = numSampleFramesPerBuffer - (wakeupOffset - startingPosition->fSampleFrame);
        }
        
        getLoopCountAndTimeStamp(&currentLoopCount, &lastLoopTime);
        
        samplesFromLoopStart = ((wakeupPosition.fLoopCount - currentLoopCount) * numSampleFramesPerBuffer) + wakeupPosition.fSampleFrame;
        
        wakeupIntervalScalar = AbsoluteTime_to_scalar(sampleInterval);
        wakeupIntervalScalar *= samplesFromLoopStart;
        
        //wakeupInterval = scalar_to_AbsoluteTime(&wakeupIntervalScalar);
        wakeupInterval = *(AbsoluteTime *)(&wakeupIntervalScalar);
        
        nanoseconds_to_absolutetime(WATCHDOG_THREAD_LATENCY_PADDING_NS, &wakeupThreadLatencyPaddingInterval);
        
        SUB_ABSOLUTETIME(&wakeupInterval, &wakeupThreadLatencyPaddingInterval);
        
        *wakeupTime = lastLoopTime;
        ADD_ABSOLUTETIME(wakeupTime, &wakeupInterval);
        
        result = kIOReturnSuccess;
    }
    
    return result;
}

IOReturn IOAudioEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::performFormatChange(%p, %p, %p)", this, audioStream, newFormat, newSampleRate);

    return kIOReturnSuccess;
}

void IOAudioEngine::sendFormatChangeNotification(IOAudioStream *audioStream)
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::sendFormatChangeNotification(%p)", this, audioStream);

    if (!configurationChangeInProgress) {
        OSCollectionIterator *userClientIterator;
        IOAudioEngineUserClient *userClient;
    
        assert(userClients);
    
        userClientIterator = OSCollectionIterator::withCollection(userClients);
        if (userClientIterator) {
            while (userClient = OSDynamicCast(IOAudioEngineUserClient, userClientIterator->getNextObject())) {
                userClient->sendFormatChangeNotification(audioStream);
            }
            
            userClientIterator->release();
        }
    }
}

void IOAudioEngine::sendNotification(UInt32 notificationType)
{
    OSCollectionIterator *userClientIterator;
    IOAudioEngineUserClient *userClient;

    assert(userClients);

    userClientIterator = OSCollectionIterator::withCollection(userClients);
    if (userClientIterator) {
        while (userClient = OSDynamicCast(IOAudioEngineUserClient, userClientIterator->getNextObject())) {
            userClient->sendNotification(notificationType);
        }
        
        userClientIterator->release();
    }
}

void IOAudioEngine::beginConfigurationChange()
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::beginConfigurationChange()", this);

    configurationChangeInProgress = true;
}

void IOAudioEngine::completeConfigurationChange()
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::completeConfigurationChange()", this);

    if (configurationChangeInProgress) {
        configurationChangeInProgress = false;
        sendNotification(kIOAudioEngineChangeNotification);

		// If any controls have notifications queued up, now's the time to send them.
		if (defaultAudioControls) {
			if (!isInactive()) {
				OSCollectionIterator *controlIterator;
				
				controlIterator = OSCollectionIterator::withCollection(defaultAudioControls);
				
				if (controlIterator) {
					IOAudioControl *control;
					
					while (control = (IOAudioControl *)controlIterator->getNextObject()) {
						control->sendQueuedNotifications();
					}
				
					controlIterator->release();
				}
			}
		}
    }
}

void IOAudioEngine::cancelConfigurationChange()
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::cancelConfigurationChange()", this);

    configurationChangeInProgress = false;
}

IOReturn IOAudioEngine::addDefaultAudioControl(IOAudioControl *defaultAudioControl)
{
    IOReturn result = kIOReturnBadArgument;

    if (defaultAudioControl) {
		if (workLoop) {
			defaultAudioControl->setWorkLoop(workLoop);
		}
        if (defaultAudioControl->attachAndStart(this)) {
            if (!defaultAudioControls) {
                defaultAudioControls = OSSet::withObjects((const OSObject **)&defaultAudioControl, 1, 1);
            } else {
                defaultAudioControls->setObject(defaultAudioControl);
            }
            
            if (isRegistered) {
                updateChannelNumbers();
            }
            
            result = kIOReturnSuccess;
        } else {
            result = kIOReturnError;
        }
    }

    return result;
}

IOReturn IOAudioEngine::removeDefaultAudioControl(IOAudioControl *defaultAudioControl)
{
    IOReturn result = kIOReturnNotFound;
    
    if (defaultAudioControl) {
        if ((state != kIOAudioEngineRunning) && (state != kIOAudioEngineResumed)) {
            if (defaultAudioControls && defaultAudioControls->containsObject(defaultAudioControl)) {
                defaultAudioControl->retain();
                
                defaultAudioControls->removeObject(defaultAudioControl);
                
                if (defaultAudioControl->getProvider() == this) {
                    defaultAudioControl->terminate();
                } else {
                    defaultAudioControl->detach(this);
                }
                
                defaultAudioControl->release();
                
                if (!configurationChangeInProgress) {
                    sendNotification(kIOAudioEngineChangeNotification);
                }
                
                result = kIOReturnSuccess;
            }
        } else {
            result = kIOReturnNotPermitted;
        }
    } else {
        result = kIOReturnBadArgument;
    }
    
    return result;
}

void IOAudioEngine::removeAllDefaultAudioControls()
{
    audioDebugIOLog(3, "IOAudioEngine[%p]::removeAllDefaultAudioControls()", this);

    if (defaultAudioControls) {
        if (!isInactive()) {
            OSCollectionIterator *controlIterator;
            
            controlIterator = OSCollectionIterator::withCollection(defaultAudioControls);
            
            if (controlIterator) {
                IOAudioControl *control;
                
                while (control = (IOAudioControl *)controlIterator->getNextObject()) {
                    if (control->getProvider() == this) {
                        control->terminate();
                    } else {
                        control->detach(this);
                    }
                }
            
                controlIterator->release();
            }
        }
        
        defaultAudioControls->flushCollection();
    }
}

void IOAudioEngine::setWorkLoopOnAllAudioControls(IOWorkLoop *wl)
{
    if (defaultAudioControls) {
        if (!isInactive()) {
            OSCollectionIterator *controlIterator;
            
            controlIterator = OSCollectionIterator::withCollection(defaultAudioControls);
            
            if (controlIterator) {
                IOAudioControl *control;
				while (control = (IOAudioControl *)controlIterator->getNextObject()) {
					if (control->getProvider() == this) {
						control->setWorkLoop(wl);
					}
				}
                controlIterator->release();
            }
        }
    }
}

