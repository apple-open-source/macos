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
 
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioEngineUserClient.h>
#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioDebug.h>
#include <IOKit/audio/IOAudioControl.h>
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

OSMetaClassDefineReservedUnused(IOAudioEngine, 1);
OSMetaClassDefineReservedUnused(IOAudioEngine, 2);
OSMetaClassDefineReservedUnused(IOAudioEngine, 3);
OSMetaClassDefineReservedUnused(IOAudioEngine, 4);
OSMetaClassDefineReservedUnused(IOAudioEngine, 5);
OSMetaClassDefineReservedUnused(IOAudioEngine, 6);
OSMetaClassDefineReservedUnused(IOAudioEngine, 7);
OSMetaClassDefineReservedUnused(IOAudioEngine, 8);
OSMetaClassDefineReservedUnused(IOAudioEngine, 9);
OSMetaClassDefineReservedUnused(IOAudioEngine, 10);
OSMetaClassDefineReservedUnused(IOAudioEngine, 11);
OSMetaClassDefineReservedUnused(IOAudioEngine, 12);
OSMetaClassDefineReservedUnused(IOAudioEngine, 13);
OSMetaClassDefineReservedUnused(IOAudioEngine, 14);
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

// New Code:
IOReturn IOAudioEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioStreamFormatExtension *formatExtension, const IOAudioSampleRate *newSampleRate)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::performFormatChange(%p, %p, %p, %p)\n", this, audioStream, newFormat, formatExtension, newSampleRate);
#endif

    return kIOReturnUnsupported;
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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::init(%p)\n", this, properties);
#endif

    if (!super::init(properties)) {
        return false;
    }
    
    duringStartup = true;

    sampleRate.whole = 0;
    sampleRate.fraction = 0;
    
    numErasesPerBuffer = IOAUDIOENGINE_DEFAULT_NUM_ERASES_PER_BUFFER;
    isRegistered = false;
    
    numActiveUserClients = 0;

    status = (IOAudioEngineStatus *)IOMallocAligned(round_page(sizeof(IOAudioEngineStatus)), PAGE_SIZE);

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
    
    maxNumOutputChannels = 0;
    maxNumInputChannels = 0;

    setSampleOffset(0);

    userClients = OSSet::withCapacity(1);
    if (!userClients) {
        return false;
    }
    
    bzero(status, round_page(sizeof(IOAudioEngineStatus)));
    status->fVersion = kIOAudioEngineCurrentStatusStructVersion;

    setState(kIOAudioEngineStopped);

    return true;
}

void IOAudioEngine::free()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::free()\n", this);
#endif

    if (status) {
        IOFreeAligned(status, round_page(sizeof(IOAudioEngineStatus)));
        status = 0;
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

    super::free();
}

bool IOAudioEngine::initHardware(IOService *provider)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::initHardware(%p)\n", this, provider);
#endif

    return true;
}

bool IOAudioEngine::start(IOService *provider)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::start(%p)\n", this, provider);
#endif

    return start(provider, OSDynamicCast(IOAudioDevice, provider));
}

bool IOAudioEngine::start(IOService *provider, IOAudioDevice *device)
{
    bool result = true;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::start(%p, %p)\n", this, provider, device);
#endif

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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::stop(%p)\n", this, provider);
#endif

    if (commandGate) {
        commandGate->runAction(detachUserClientsAction);
    }
    
    stopAudioEngine();

    detachAudioStreams();
    removeAllDefaultAudioControls();

    super::stop(provider);
}

IOWorkLoop *IOAudioEngine::getWorkLoop() const
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::getWorkLoop()\n", this);
#endif

    return workLoop;
}

IOCommandGate *IOAudioEngine::getCommandGate() const
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::getCommandGate()\n", this);
#endif

    return commandGate;
}

void IOAudioEngine::registerService(IOOptionBits options)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::registerService(0x%lx)\n", this, options);
#endif

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
    bzero(uniqueIDStr, uniqueIDSize);
    
    if (uniqueIDStr) {
        if (className) {
            sprintf(uniqueIDStr, "%s:", className);
        }
        
        if (location) {
            strcat(uniqueIDStr, location);
            strcat(uniqueIDStr, ":");
        }
        
        if (localID) {
            strcat(uniqueIDStr, localID->getCStringNoCopy());
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
    char localUniqueIDStr[(sizeof(UInt32)*2)+1];
    
    sprintf(localUniqueIDStr, "%lx", index);
    
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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::resetStatusBuffer()\n", this);
#endif

    assert(status);
    
    status->fCurrentLoopCount = 0;
    status->fLastLoopTime.hi = 0;
    status->fLastLoopTime.lo = 0;
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
    IOReturn				result = kIOReturnSuccess;
    IOAudioEngineUserClient	*client;

#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::newUserClient(0x%x, %p, 0x%lx, %p)\n", this, (unsigned int)task, securityID, type, handler);
#endif

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
                assert(commandGate);
    
                result = commandGate->runAction(addUserClientAction, client);
                
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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::clientClosed(%p)\n", this, client);
#endif

    if (client) {
        assert(commandGate);

        commandGate->runAction(removeUserClientAction, client);
    }
}

IOReturn IOAudioEngine::addUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine::addUserClientAction(%p, %p)\n", owner, arg1);
#endif

    if (owner) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, owner);
        if (audioEngine) {
            result = audioEngine->addUserClient((IOAudioEngineUserClient *)arg1);
        }
    }
    
    return result;
}

IOReturn IOAudioEngine::removeUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine::removeUserClientAction(%p, %p)\n", owner, arg1);
#endif

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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::addUserClient(%p)\n", this, newUserClient);
#endif

    assert(userClients);
    
    userClients->setObject(newUserClient);
    
    return result;
}

IOReturn IOAudioEngine::removeUserClient(IOAudioEngineUserClient *userClient)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::removeUserClient(%p)\n", this, userClient);
#endif

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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::detachUserClients\n", this);
#endif

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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::startClient(%p)\n", this, userClient);
#endif

    if (userClient) {
        result = incrementActiveUserClients();
    }
    
    return result;
}

IOReturn IOAudioEngine::stopClient(IOAudioEngineUserClient *userClient)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::stopClient(%p)\n", this, userClient);
#endif

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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::incrementActiveUserClients() - %ld\n", this, numActiveUserClients);
#endif
    
    numActiveUserClients++;

    setProperty(kIOAudioEngineNumActiveUserClientsKey, numActiveUserClients, sizeof(UInt32)*8);

    if (numActiveUserClients == 1) {
        result = startAudioEngine();
    }
    
    return result;
}

IOReturn IOAudioEngine::decrementActiveUserClients()
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::decrementActiveUserClients() - %ld\n", this, numActiveUserClients);
#endif
    
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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::addAudioStream(%p)\n", this, stream);
#endif

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
                break;
            case kIOAudioStreamDirectionInput:
                assert(inputStreams);
                
                inputStreams->setObject(stream);
                
                maxNumInputChannels += stream->getMaxNumChannels();
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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::detachAudioStreams()\n", this);
#endif

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
   
#ifdef DEBUG_CALLS
IOLog("IOAudioEngine[%p]::updateChannelNumbers() - o=%ld i=%ld\n", this, maxNumOutputChannels, maxNumInputChannels);
#endif

    if (maxNumOutputChannels > 0) {
        outputChannelNumbers = (SInt32 *)IOMallocAligned(maxNumOutputChannels * sizeof(SInt32), sizeof (SInt32));
    }
    
    if (maxNumInputChannels > 0) {
        inputChannelNumbers = (SInt32 *)IOMallocAligned(maxNumInputChannels * sizeof(SInt32), sizeof (SInt32));
    }
    
    currentChannelID = 1;
    currentChannelNumber = 1;
    
    assert(outputStreams);
    
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
                    
                    assert(currentChannelID + maxNumChannels <= maxNumOutputChannels);
                    
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
    
    assert(inputStreams);
    
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
                    
                    assert(currentChannelID + maxNumChannels <= maxNumInputChannels);
                    
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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::startAudioEngine()\n", this);
#endif

    switch(getState()) {
        case kIOAudioEnginePaused:
            result = resumeAudioEngine();
            break;
        case kIOAudioEngineStopped:
            audioDevice->audioEngineStarting();
        case kIOAudioEngineResumed:
            resetStatusBuffer();
            
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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::stopAudioEngine()\n", this);
#endif

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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::pauseAudioEngine()\n", this);
#endif

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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::resumeAudioEngine()\n", this);
#endif
    
    if (getState() == kIOAudioEnginePaused) {
        setState(kIOAudioEngineResumed);
        sendNotification(kIOAudioEngineResumedNotification);
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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::getStatus()\n", this);
#endif

    return status;
}

void IOAudioEngine::setNumSampleFramesPerBuffer(UInt32 numSampleFrames)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::setNumSampleFramesPerBuffer(0x%lx)\n", this, numSampleFrames);
#endif

    if (getState() == kIOAudioEngineRunning) {
        IOLog("IOAudioEngine[%p]::setNumSampleFramesPerBuffer(0x%lx) - Error: can't change num sample frames while engine is running.\n", this, numSampleFrames);
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
#ifdef DEBUG_IO
    IOLog("IOAudioEngine[%p]::getNumSampleFramesPerBuffer()\n", this);
#endif

    return numSampleFramesPerBuffer;
}

IOAudioEngineState IOAudioEngine::getState()
{
    return state;
}

IOAudioEngineState IOAudioEngine::setState(IOAudioEngineState newState)
{
    IOAudioEngineState oldState;

#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::setState(0x%x)\n", this, newState);
#endif

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
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::setSampleRate(%p)\n", this, newSampleRate);
#endif

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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::setSampleLatency(0x%lx)\n", this, numSamples);
#endif
    setOutputSampleLatency(numSamples);
    setInputSampleLatency(numSamples);
}

void IOAudioEngine::setOutputSampleLatency(UInt32 numSamples)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::setOutputSampleLatency(0x%lx)\n", this, numSamples);
#endif
    setProperty(kIOAudioEngineOutputSampleLatencyKey, numSamples, sizeof(UInt32)*8);
}

void IOAudioEngine::setInputSampleLatency(UInt32 numSamples)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::setInputSampleLatency(0x%lx)\n", this, numSamples);
#endif
    setProperty(kIOAudioEngineInputSampleLatencyKey, numSamples, sizeof(UInt32)*8);
}

void IOAudioEngine::setSampleOffset(UInt32 numSamples)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::setSampleOffset(0x%lx)\n", this, numSamples);
#endif
    sampleOffset = numSamples;
    setProperty(kIOAudioEngineSampleOffsetKey, numSamples, sizeof(UInt32)*8);
}

void IOAudioEngine::setRunEraseHead(bool erase)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::setRunEraseHead(%d)\n", this, erase);
#endif
    runEraseHead = erase;
}

bool IOAudioEngine::getRunEraseHead()
{
#ifdef DEBUG_TIMER_CALLS
    IOLog("IOAudioEngine[%p]::getRunEraseHead()\n", this);
#endif

    return runEraseHead;
}

AbsoluteTime IOAudioEngine::getTimerInterval()
{
    AbsoluteTime interval;
    const IOAudioSampleRate *currentRate;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::getTimerInterval()\n", this);
#endif

    assert(status);

    currentRate = getSampleRate();
    
    if ((getNumSampleFramesPerBuffer() == 0) || (currentRate && (currentRate->whole == 0))) {
        nanoseconds_to_absolutetime(NSEC_PER_SEC, &interval);
    } else if ((numErasesPerBuffer == 0) || (!getRunEraseHead())) {	// Run once per ring buffer
        nanoseconds_to_absolutetime(((UInt64)NSEC_PER_SEC * (UInt64)getNumSampleFramesPerBuffer() / (UInt64)currentRate->whole), &interval);
    } else {
        nanoseconds_to_absolutetime(((UInt64)NSEC_PER_SEC * (UInt64)getNumSampleFramesPerBuffer() / (UInt64)currentRate->whole / (UInt64)numErasesPerBuffer), &interval);
    }
    return interval;
}

void IOAudioEngine::timerCallback(OSObject *target, IOAudioDevice *device)
{
    IOAudioEngine *audioEngine;

#ifdef DEBUG_TIMER_CALLS
    IOLog("IOAudioEngine::timerCallback(%p, %p)\n", target, device);
#endif

    audioEngine = OSDynamicCast(IOAudioEngine, target);
    if (audioEngine) {
        audioEngine->timerFired();
    }
}

void IOAudioEngine::timerFired()
{
#ifdef DEBUG_TIMER_CALLS
    IOLog("IOAudioEngine[%p]::timerFired()\n", this);
#endif

    performErase();
    performFlush();
}

void IOAudioEngine::performErase()
{
#ifdef DEBUG_TIMER_CALLS
    IOLog("IOAudioEngine[%p]::performErase()\n", this);
#endif

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
#ifdef DEBUG_TIMER
                    IOLog("IOAudioEngine[%p]::performErase() - erasing from frame: 0x%lx to 0x%lx\n", this, eraseHeadSampleFrame, numSampleFramesPerBuffer);
                    IOLog("IOAudioEngine[%p]::performErase() - erasing from frame: 0x%x to 0x%lx\n", this, 0, currentSampleFrame);
#endif
					if ((currentSampleFrame * sampleBufferFrameSize <= outputStream->getSampleBufferSize()) &&
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
					}
                } else {
#ifdef DEBUG_TIMER
                    IOLog("IOAudioEngine[%p]::performErase() - erasing from frame: 0x%lx to 0x%lx\n", this, eraseHeadSampleFrame, currentSampleFrame);
#endif

					if ((eraseHeadSampleFrame * sampleBufferFrameSize + (currentSampleFrame - eraseHeadSampleFrame) * sampleBufferFrameSize) <= outputStream->getSampleBufferSize()) {
						if (sampleBuf && (sampleBufferFrameSize > 0)) {
							bzero(sampleBuf + (eraseHeadSampleFrame * sampleBufferFrameSize), (currentSampleFrame - eraseHeadSampleFrame) * sampleBufferFrameSize);
						}
					}
					if ((eraseHeadSampleFrame * mixBufferFrameSize + (currentSampleFrame - eraseHeadSampleFrame) * mixBufferFrameSize) <= outputStream->getMixBufferSize()) {
						if (mixBuf && (mixBufferFrameSize > 0)) {
							bzero(mixBuf + (eraseHeadSampleFrame * mixBufferFrameSize), (currentSampleFrame - eraseHeadSampleFrame) * mixBufferFrameSize);
						}
					}
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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::stopEngineAtPosition(%lx,%lx)\n", this, endingPosition ? endingPosition->fLoopCount : 0, endingPosition ? endingPosition->fSampleFrame : 0);
#endif

    if (endingPosition) {
        audioEngineStopPosition = *endingPosition;
    } else {
        audioEngineStopPosition.fLoopCount = 0;
        audioEngineStopPosition.fSampleFrame = 0;
    }
}

void IOAudioEngine::performFlush()
{
#ifdef DEBUG_TIMER_CALLS
    IOLog("IOAudioEngine[%p]::performFlush()\n", this);
#endif

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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::addTimer()\n", this);
#endif

    if (!audioDevice) {
        return;
    }

    audioDevice->addTimerEvent(this, &IOAudioEngine::timerCallback, getTimerInterval());
}

void IOAudioEngine::removeTimer()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::removeTimer()\n", this);
#endif

    if (!audioDevice) {
        return;
    }

    audioDevice->removeTimerEvent(this);
}

IOReturn IOAudioEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
#ifdef DEBUG_OUTPUT_CALLS
    IOLog("IOAudioEngine[%p]::clipOutputSamples(%p, %p, 0x%lx, 0x%lx, %p, %p)\n", this, mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
#endif

    return kIOReturnUnsupported;
}

void IOAudioEngine::resetClipPosition(IOAudioStream *audioStream, UInt32 clipSampleFrame)
{
#ifdef DEBUG_OUTPUT_CALLS
    IOLog("IOAudioEngine[%p]::resetClipPosition(%p, 0x%lx)\n", this, audioStream, clipSampleFrame);
#endif

    return;
}


IOReturn IOAudioEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
#ifdef DEBUG_INPUT_CALLS
    IOLog("IOAudioEngine[%p]::convertInputSamples(%p, %p, 0x%lx, 0x%lx, %p, %p)\n", this, sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
#endif

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
    
    status->fLastLoopTime.hi = ts->hi;
    status->fLastLoopTime.lo = ts->lo;
    
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
        
        timestamp->hi = status->fLastLoopTime.hi;
        timestamp->lo = status->fLastLoopTime.lo;
        *loopCount = status->fCurrentLoopCount;
        
        nextTimestamp.hi = status->fLastLoopTime.hi;
        nextTimestamp.lo = status->fLastLoopTime.lo;
        nextLoopCount = status->fCurrentLoopCount;
        
        while ((*loopCount != nextLoopCount) || (CMP_ABSOLUTETIME(timestamp, &nextTimestamp) != 0)) {
            *timestamp = nextTimestamp;
            *loopCount = nextLoopCount;
            
            nextTimestamp.hi = status->fLastLoopTime.hi;
            nextTimestamp.lo = status->fLastLoopTime.lo;
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
        
        wakeupOffset = (numSampleFrames / 10) + sampleOffset;
        
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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::performFormatChange(%p, %p, %p)\n", this, audioStream, newFormat, newSampleRate);
#endif

    return kIOReturnSuccess;
}

void IOAudioEngine::sendFormatChangeNotification(IOAudioStream *audioStream)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::sendFormatChangeNotification(%p)\n", this, audioStream);
#endif

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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::beginConfigurationChange()\n", this);
#endif

    configurationChangeInProgress = true;
}

void IOAudioEngine::completeConfigurationChange()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::completeConfigurationChange()\n", this);
#endif

    if (configurationChangeInProgress) {
        configurationChangeInProgress = false;
        sendNotification(kIOAudioEngineChangeNotification);
    }
}

void IOAudioEngine::cancelConfigurationChange()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::cancelConfigurationChange()\n", this);
#endif

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
                defaultAudioControls = OSSet::withObjects(&(const OSObject *)defaultAudioControl, 1, 1);
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
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngine[%p]::removeAllDefaultAudioControls()\n", this);
#endif

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

