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

#include <IOKit/audio/IOAudioDMAEngine.h>
#include <IOKit/audio/IOAudioDMAEngineUserClient.h>
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

#define super IOService

OSDefineMetaClassAndAbstractStructors(IOAudioDMAEngine, IOService)

const OSSymbol *IOAudioDMAEngine::gSampleRateWholeNumberKey = NULL;
const OSSymbol *IOAudioDMAEngine::gSampleRateFractionKey = NULL;

void IOAudioDMAEngine::initKeys()
{
    if (!gSampleRateWholeNumberKey) {
        gSampleRateWholeNumberKey = OSSymbol::withCString(IOAUDIO_SAMPLE_RATE_WHOLE_NUMBER_KEY);
        gSampleRateFractionKey = OSSymbol::withCString(IOAUDIO_SAMPLE_RATE_FRACTION_KEY);
    }
}

OSDictionary *IOAudioDMAEngine::createDictionaryFromSampleRate(const IOAudioSampleRate *sampleRate, OSDictionary *rateDict = 0)
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

IOAudioSampleRate *IOAudioDMAEngine::createSampleRateFromDictionary(const OSDictionary *rateDict, IOAudioSampleRate *sampleRate = 0)
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

bool IOAudioDMAEngine::init(OSDictionary *properties)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::init(%p)\n", this, properties);
#endif

    if (!super::init(properties)) {
        return false;
    }

    sampleRate.whole = 0;
    sampleRate.fraction = 0;
    
    numErasesPerBuffer = IOAUDIODMAENGINE_DEFAULT_NUM_ERASES_PER_BUFFER;
    isRegistered = false;

    status = (IOAudioDMAEngineStatus *)IOMallocAligned(round_page(sizeof(IOAudioDMAEngineStatus)), PAGE_SIZE);

    if (!status) {
        return false;
    }

    outputStreams = OSSet::withCapacity(1);
    if (!outputStreams) {
        return false;
    }

    inputStreams = OSSet::withCapacity(1);
    if (!inputStreams) {
        return false;
    }

    userClients = OSSet::withCapacity(1);
    if (!userClients) {
        return false;
    }
    
    bzero(status, round_page(sizeof(IOAudioDMAEngineStatus)));
    status->fVersion = CURRENT_IOAUDIODMAENGINE_STATUS_STRUCT_VERSION;

    _setState(kAudioDMAEngineStopped);

    return true;
}

void IOAudioDMAEngine::free()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::free()\n", this);
#endif

    if (status) {
        IOFreeAligned(status, round_page(sizeof(IOAudioDMAEngineStatus)));
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

bool IOAudioDMAEngine::initHardware(IOService *provider)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::initHardware(%p)\n", this, provider);
#endif

    return true;
}

bool IOAudioDMAEngine::start(IOService *provider)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::start(%p)\n", this, provider);
#endif

    return start(provider, OSDynamicCast(IOAudioDevice, provider));
}

bool IOAudioDMAEngine::start(IOService *provider, IOAudioDevice *device)
{
    bool result = true;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::start(%p, %p)\n", this, provider, device);
#endif

    if (!super::start(provider)) {
        return false;
    }

    if (!device) {
        return false;
    }

    audioDevice = device;
    
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
    
    result = initHardware(provider);
    
    if (result) {
        registerService();
    }
    
    return result;
}

void IOAudioDMAEngine::stop(IOService *provider)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::stop(%p)\n", this, provider);
#endif

    if (commandGate) {
        commandGate->runAction(detachUserClientsAction);
    }

    detachAudioStreams();

    super::stop(provider);
}

IOWorkLoop *IOAudioDMAEngine::getWorkLoop()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::getWorkLoop()\n", this);
#endif

    return workLoop;
}

IOCommandGate *IOAudioDMAEngine::getCommandGate()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::getCommandGate()\n", this);
#endif

    return commandGate;
}

void IOAudioDMAEngine::registerService(IOOptionBits options = 0)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::registerService(0x%lx)\n", this, options);
#endif

    super::registerService(options);

    if (!isRegistered) {
        OSCollectionIterator *iterator;
        IOAudioStream *stream;
        
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

void IOAudioDMAEngine::resetStatusBuffer()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::resetStatusBuffer()\n", this);
#endif

    assert(status);
    
    status->fCurrentLoopCount = 0;
    status->fLastLoopTime.hi = 0;
    status->fLastLoopTime.lo = 0;
    status->fEraseHeadSampleFrame = 0;
    
    dmaEngineStopPosition.fSampleFrame = 0;
    dmaEngineStopPosition.fLoopCount = 0;
    
    return;
}

void IOAudioDMAEngine::clearAllSampleBuffers()
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

IOReturn IOAudioDMAEngine::newUserClient(task_t task, void *securityID, UInt32 type, IOUserClient **handler)
{
    IOReturn			result = kIOReturnSuccess;
    IOAudioDMAEngineUserClient *	client;

#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::newUserClient(0x%x, %p, 0x%lx, %p)\n", this, (unsigned int)task, securityID, type, handler);
#endif

    if (!isInactive()) {
        client = IOAudioDMAEngineUserClient::withDMAEngine(this, task, securityID, type);
    
        if (client) {
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

void IOAudioDMAEngine::clientClosed(IOAudioDMAEngineUserClient *client)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::clientClosed(%p)\n", this, client);
#endif

    if (client) {
        assert(commandGate);

        commandGate->runAction(removeUserClientAction, client);
        client->detach(this);
    }
}

IOReturn IOAudioDMAEngine::addUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine::addUserClientAction(%p, %p)\n", owner, arg1);
#endif

    if (owner) {
        IOAudioDMAEngine *audioDMAEngine = OSDynamicCast(IOAudioDMAEngine, owner);
        if (audioDMAEngine) {
            result = audioDMAEngine->addUserClient((IOAudioDMAEngineUserClient *)arg1);
        }
    }
    
    return result;
}

IOReturn IOAudioDMAEngine::removeUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine::removeUserClientAction(%p, %p)\n", owner, arg1);
#endif

    if (owner) {
        IOAudioDMAEngine *audioDMAEngine = OSDynamicCast(IOAudioDMAEngine, owner);
        if (audioDMAEngine) {
            result = audioDMAEngine->removeUserClient((IOAudioDMAEngineUserClient *)arg1);
        }
    }
    
    return result;
}

IOReturn IOAudioDMAEngine::detachUserClientsAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioDMAEngine *audioDMAEngine = OSDynamicCast(IOAudioDMAEngine, owner);
        if (audioDMAEngine) {
            result = audioDMAEngine->detachUserClients();
        }
    }
    
    return result;
}

IOReturn IOAudioDMAEngine::addUserClient(IOAudioDMAEngineUserClient *newUserClient)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::addUserClient(%p)\n", this, newUserClient);
#endif

    assert(userClients);
    
    userClients->setObject(newUserClient);
    
    if ((userClients->getCount() == 1) && (getState() == kAudioDMAEngineStopped)) {
        resetStatusBuffer();
        result = startDMAEngine();
        
        if (result == kIOReturnSuccess) {
            _setState(kAudioDMAEngineRunning);
        } else {
            userClients->removeObject(newUserClient);
        }
    }
    
    return result;
}

IOReturn IOAudioDMAEngine::removeUserClient(IOAudioDMAEngineUserClient *userClient)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::removeUserClient(%p)\n", this, userClient);
#endif

    assert(userClients);
    
    userClient->retain();
    
    userClients->removeObject(userClient);
    
    if ((userClients->getCount() == 0) && (getState() == kAudioDMAEngineRunning)) {
        assert(status);
        
        dmaEngineStopPosition.fSampleFrame = getCurrentSampleFrame();
        dmaEngineStopPosition.fLoopCount = status->fCurrentLoopCount + 1;
    }
    
    if (!isInactive()) {
        userClient->terminate();
    }
    
    userClient->release();
    
    return result;
}

IOReturn IOAudioDMAEngine::detachUserClients()
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::detachUserClients\n", this);
#endif

    assert(userClients);
    
    if (!isInactive()) {	// Iterate through and terminate each user client
        OSIterator *iterator;
        
        iterator = OSCollectionIterator::withCollection(userClients);
        
        if (iterator) {
            IOAudioDMAEngineUserClient *userClient;
            
            while (userClient = (IOAudioDMAEngineUserClient *)iterator->getNextObject()) {
                userClient->terminate();
            }
            iterator->release();
        }
    }
    
    userClients->flushCollection();
    
    dmaEngineStopPosition.fSampleFrame = getCurrentSampleFrame();
    dmaEngineStopPosition.fLoopCount = status->fCurrentLoopCount + 1;
    
    return result;
}

bool IOAudioDMAEngine::addAudioStream(IOAudioStream *stream)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::addAudioStream(%p)\n", this, stream);
#endif

    if (stream) {

        if (!stream->attach(this)) {
            return false;
        }

        if (!stream->start(this)) {
            stream->detach(this);
            return false;
        }
        
        switch (stream->getDirection()) {
            case kAudioOutput:
                if (!outputStreams) {
                    outputStreams = OSSet::withObjects(&(const OSObject *)stream, 1, 1);
                } else {
                    outputStreams->setObject(stream);
                }
                
                if (outputStreams->getCount() == 1) {
                    _setRunEraseHead(true);
                }
                break;
            case kAudioInput:
                if (!inputStreams) {
                    inputStreams = OSSet::withObjects(&(const OSObject *)stream, 1, 1);
                } else {
                    inputStreams->setObject(stream);
                }
                break;
        }

        if (isRegistered) {
            stream->registerService();
        }
    }

    return true;
}

void IOAudioDMAEngine::removeAudioStream(IOAudioStream *stream)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::removeAudioStream(%p)\n", this, stream);
#endif
}

void IOAudioDMAEngine::detachAudioStreams()
{
    OSCollectionIterator *iterator;
    IOAudioStream *stream;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::detachAudioStreams()\n", this);
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

IOReturn IOAudioDMAEngine::startDMAEngine()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::startDMAEngine()\n", this);
#endif

    return kIOReturnUnsupported;
}

IOReturn IOAudioDMAEngine::stopDMAEngine()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::stopDMAEngine()\n", this);
#endif

    return kIOReturnUnsupported;
}

const IOAudioDMAEngineStatus *IOAudioDMAEngine::getStatus()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::getStatus()\n", this);
#endif

    return status;
}

void IOAudioDMAEngine::_setNumSampleFramesPerBuffer(UInt32 numSampleFrames)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::_setNumSampleFramesPerBuffer(0x%lx)\n", this, numSampleFrames);
#endif

    numSampleFramesPerBuffer = numSampleFrames;
    setProperty(IOAUDIODMAENGINE_NUM_SAMPLE_FRAMES_PER_BUFFER_KEY, numSampleFramesPerBuffer, sizeof(UInt32)*8);
}

UInt32 IOAudioDMAEngine::getNumSampleFramesPerBuffer()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::getNumSampleFramesPerBuffer()\n", this);
#endif

    return numSampleFramesPerBuffer;
}

IOAudioDMAEngineState IOAudioDMAEngine::getState()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::getState()\n", this);
#endif

    return state;
}

IOAudioDMAEngineState IOAudioDMAEngine::_setState(IOAudioDMAEngineState newState)
{
    IOAudioDMAEngineState oldState;
    UInt32 value = 0;

#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::_setState(0x%x)\n", this, newState);
#endif

    oldState = state;
    state = newState;

    switch (state) {
        case kAudioDMAEngineRunning:
            value = IOAUDIODMAENGINE_STATE_RUNNING;
            if (oldState != kAudioDMAEngineRunning) {
                addTimer();
            }
            break;
        case kAudioDMAEngineStopped:
            value = IOAUDIODMAENGINE_STATE_STOPPED;
            if (oldState == kAudioDMAEngineRunning) {
                removeTimer();
                performErase();
            }
            break;
    }

    setProperty(IOAUDIODMAENGINE_STATE_KEY, value, sizeof(UInt32)*8);

    return oldState;
}

const IOAudioSampleRate *IOAudioDMAEngine::getSampleRate()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::getSampleRate()\n", this);
#endif

    return &sampleRate;
}

void IOAudioDMAEngine::_setSampleRate(const IOAudioSampleRate *newSampleRate)
{
    OSDictionary *sampleRateDict;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::_setSampleRate(%p)\n", this, newSampleRate);
#endif

    sampleRate = *newSampleRate;
    
    sampleRateDict = createDictionaryFromSampleRate(&sampleRate);
    if (sampleRateDict) {
        setProperty(IOAUDIO_SAMPLE_RATE_KEY, sampleRateDict);
        sampleRateDict->release();
    }
}

void IOAudioDMAEngine::_setSampleLatency(UInt32 numSamples)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::_setSampleLatency(0x%lx)\n", this, numSamples);
#endif
    setProperty(IOAUDIODMAENGINE_SAMPLE_LATENCY_KEY, numSamples, sizeof(UInt32)*8);
}

void IOAudioDMAEngine::_setRunEraseHead(bool erase)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::_setRunEraseHead(%d)\n", this, erase);
#endif
    runEraseHead = erase;
}

bool IOAudioDMAEngine::getRunEraseHead()
{
#ifdef DEBUG_TIMER_CALLS
    IOLog("IOAudioDMAEngine[%p]::getRunEraseHead()\n", this);
#endif

    return runEraseHead;
}

AbsoluteTime IOAudioDMAEngine::getTimerInterval()
{
    AbsoluteTime interval;
    const IOAudioSampleRate *currentRate;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::getTimerInterval()\n", this);
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

void IOAudioDMAEngine::timerCallback(OSObject *target, IOAudioDevice *device)
{
    IOAudioDMAEngine *dmaEngine;

#ifdef DEBUG_TIMER_CALLS
    IOLog("IOAudioDMAEngine::timerCallback(%p, %p)\n", target, device);
#endif

    dmaEngine = OSDynamicCast(IOAudioDMAEngine, target);
    if (dmaEngine) {
        dmaEngine->timerFired();
    }
}

void IOAudioDMAEngine::timerFired()
{
#ifdef DEBUG_TIMER_CALLS
    IOLog("IOAudioDMAEngine[%p]::timerFired()\n", this);
#endif

    performErase();
    performFlush();
}

void IOAudioDMAEngine::performErase()
{
#ifdef DEBUG_TIMER_CALLS
    IOLog("IOAudioDMAEngine[%p]::performErase()\n", this);
#endif

    assert(status);
    
    if (getRunEraseHead()) {
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
                
                sampleBuf = (char *)outputStream->getSampleBuffer();
                mixBuf = (char *)outputStream->getMixBuffer();
                
                sampleBufferFrameSize = outputStream->format.fNumChannels * outputStream->format.fBitWidth / 8;
                mixBufferFrameSize = outputStream->format.fNumChannels * IOAUDIODMAENGINE_DEFAULT_MIX_BUFFER_SAMPLE_SIZE;
                
                if (currentSampleFrame < eraseHeadSampleFrame) {
#ifdef DEBUG_TIMER
                    IOLog("IOAudioDMAEngine[%p]::performErase() - erasing from frame: 0x%lx to 0x%lx\n", this, eraseHeadSampleFrame, numSampleFramesPerBuffer);
                    IOLog("IOAudioDMAEngine[%p]::performErase() - erasing from frame: 0x%lx to 0x%lx\n", this, 0, currentSampleFrame);
#endif
                    if (sampleBuf && (sampleBufferFrameSize > 0)) {
                        bzero(sampleBuf, currentSampleFrame * sampleBufferFrameSize);
                        bzero(sampleBuf + (eraseHeadSampleFrame * sampleBufferFrameSize), (numSampleFramesPerBuffer - eraseHeadSampleFrame) * sampleBufferFrameSize);
                    }
                    
                    if (mixBuf && (mixBufferFrameSize > 0)) {
                        bzero(mixBuf, currentSampleFrame * mixBufferFrameSize);
                        bzero(mixBuf + (eraseHeadSampleFrame * mixBufferFrameSize), (numSampleFramesPerBuffer - eraseHeadSampleFrame) * mixBufferFrameSize);
                    }
                } else {
#ifdef DEBUG_TIMER
                    IOLog("IOAudioDMAEngine[%p]::performErase() - erasing from frame: 0x%lx to 0x%lx\n", this, eraseHeadSampleFrame, currentSampleFrame);
#endif

                    if (sampleBuf && (sampleBufferFrameSize > 0)) {
                        bzero(sampleBuf + (eraseHeadSampleFrame * sampleBufferFrameSize), (currentSampleFrame - eraseHeadSampleFrame) * sampleBufferFrameSize);
                    }
                    
                    if (mixBuf && (mixBufferFrameSize > 0)) {
                        bzero(mixBuf + (eraseHeadSampleFrame * mixBufferFrameSize), (currentSampleFrame - eraseHeadSampleFrame) * mixBufferFrameSize);
                    }
                }
            }
            
            status->fEraseHeadSampleFrame = currentSampleFrame;
            
            outputIterator->release();
        }
    }
}

void IOAudioDMAEngine::performFlush()
{
#ifdef DEBUG_TIMER_CALLS
    IOLog("IOAudioDMAEngine[%p]::performFlush()\n", this);
#endif

    assert(userClients);
    
    if ((userClients->getCount() == 0) && (getState() == kAudioDMAEngineRunning)) {
        UInt32 currentSampleFrame, currentLoopCount;
        
        assert(status);
        
        currentLoopCount = status->fCurrentLoopCount;
        currentSampleFrame = getCurrentSampleFrame();
        
        if ((currentLoopCount > dmaEngineStopPosition.fLoopCount)
         || ((currentLoopCount == dmaEngineStopPosition.fLoopCount)
             && (currentSampleFrame > dmaEngineStopPosition.fSampleFrame))) {
                stopDMAEngine();
                _setState(kAudioDMAEngineStopped);
        }
    }
}

void IOAudioDMAEngine::addTimer()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::addTimer()\n", this);
#endif

    if (!audioDevice) {
        return;
    }

    audioDevice->addTimerEvent(this, &IOAudioDMAEngine::timerCallback, getTimerInterval());
}

void IOAudioDMAEngine::removeTimer()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::removeTimer()\n", this);
#endif

    if (!audioDevice) {
        return;
    }

    audioDevice->removeTimerEvent(this);
}

IOReturn IOAudioDMAEngine::clipToOutputStream(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
#ifdef DEBUG_OUTPUT_CALLS
    IOLog("IOAudioDMAEngine[%p]::clipToOutputStream(%p, %p, 0x%lx, 0x%lx, %p, %p)\n", this, mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
#endif

    return kIOReturnUnsupported;
}

IOReturn IOAudioDMAEngine::convertFromInputStream(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_INPUT_CALLS
    IOLog("IOAudioDMAEngine[%p]::convertFromInputStream(%p, %p, 0x%lx, 0x%lx, %p, %p)\n", this, sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
#endif

    if (!sampleBuf || !destBuf || !streamFormat || !audioStream) {
        return kIOReturnBadArgument;
    }
    
    if ((firstSampleFrame + numSampleFrames) > numSampleFramesPerBuffer) {
        result = convertFromInputStream_NoWrap(sampleBuf, destBuf, firstSampleFrame, numSampleFramesPerBuffer - firstSampleFrame, streamFormat, audioStream);
        if (result == kIOReturnSuccess) {
            result = convertFromInputStream_NoWrap(sampleBuf, &((float *)destBuf)[(numSampleFramesPerBuffer - firstSampleFrame) * streamFormat->fNumChannels], 0, numSampleFrames - (numSampleFramesPerBuffer - firstSampleFrame), streamFormat, audioStream);
        }
    } else {
        result = convertFromInputStream_NoWrap(sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
    }
    
    return result;
}

IOReturn IOAudioDMAEngine::convertFromInputStream_NoWrap(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
#ifdef DEBUG_INPUT_CALLS
    IOLog("IOAudioDMAEngine[%p]::convertFromInputStream_NoWrap(%p, %p, 0x%lx, 0x%lx, %p, %p)\n", this, sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
#endif

    return kIOReturnUnsupported;
}

IOReturn IOAudioDMAEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::performFormatChange(%p, %p, %p)\n", this, audioStream, newFormat, newSampleRate);
#endif

    assert(audioDevice);
    
    return audioDevice->performFormatChange(audioStream, newFormat, newSampleRate);
}

void IOAudioDMAEngine::sendFormatChangeNotification(IOAudioStream *audioStream)
{
    OSCollectionIterator *userClientIterator;
    IOAudioDMAEngineUserClient *userClient;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngine[%p]::sendFormatChangeNotification(%p)\n", this, audioStream);
#endif

    assert(userClients);

    userClientIterator = OSCollectionIterator::withCollection(userClients);
    if (userClientIterator) {
        while (userClient = OSDynamicCast(IOAudioDMAEngineUserClient, userClientIterator->getNextObject())) {
            userClient->sendFormatChangeNotification(audioStream);
        }
        
        userClientIterator->release();
    }
}

bool IOAudioDMAEngine::addDefaultAudioControl(IOAudioControl *defaultAudioControl)
{
    bool result = false;

    if (defaultAudioControl) {
       result = defaultAudioControl->attach(this);
    }

    return result;
}

void IOAudioDMAEngine::removeDefaultAudioControl(IOAudioControl *defaultAudioControl)
{
    if (defaultAudioControl) {
        defaultAudioControl->detach(this);
    }
}

