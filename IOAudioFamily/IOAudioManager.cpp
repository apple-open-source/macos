/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/audio/IOAudioManager.h>
#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>

#include <libkern/c++/OSSet.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <IOKit/hidsystem/IOHIDShared.h>

#include <IOKit/audio/IOAudioDMAEngineUserClient.h>

#define DEFAULT_INITIAL_VOLUME 		MASTER_VOLUME_MAX
#define DEFAULT_INITIAL_MUTE		false
#define DEFAULT_INITIAL_INCREMENT	4096

static MasterAudioFunctions masterFunctions;
IOAudioManager *IOAudioManager::amInstance = 0;

extern bool (*playBeep)(IOService *outputDMAEngine);

bool IOBeep(IOService *outputDMAEngine)
{
    /*
    int dmaBlockStart, dmaBlockEnd;
    int i;
    IOAudioDMAEngineStatus *status;
    short *sampleBuffer;
    IOByteCount bufSize;
    IOUserClient *handler;
    IOMemoryMap *statusMap;
    IOMemoryMap *sampleMap;
    IOReturn ret;
    int j, div, mul;
    int size;
    int val;

    ret = outputDMAEngine->newUserClient( kernel_task, NULL, 0, &handler );
    if(kIOReturnSuccess != ret)
        return false;

    sampleMap = handler->mapClientMemory( kSampleBuffer, kernel_task  );
    assert(sampleMap);
    sampleBuffer = (short *) sampleMap->getVirtualAddress();
    bufSize = sampleMap->getLength();

    statusMap = handler->mapClientMemory( kStatusBuffer, kernel_task );
    assert(statusMap);
    status = (IOAudioDMAEngineStatus *) statusMap->getVirtualAddress();

    assert(status->fSampleSize == 2);
    // Put 1/10 second of 440Hz sound in the buffer, starting just after the current block.
    div = status->fDataRate/status->fSampleSize/status->fChannels/440;
    mul = 0x4000/div;
    size = status->fDataRate/status->fSampleSize/status->fChannels/10;
    dmaBlockStart = status->fCurrentBlock * status->fBlockSize;
    dmaBlockEnd = dmaBlockStart + status->fBlockSize;
    for(i=dmaBlockEnd/2; i<status->fBufSize/2; i += status->fChannels){
        if(!--size)
            break;
        val = (size) % div;
        if(val > div/2)
            val = div/2 - val;
        val = val*mul - 0x1000;
        for(j=0; j<status->fChannels; j++) {
            sampleBuffer[i+j] = val;
        }
    }
    if(size)
        for(i=0; i<dmaBlockStart/2; i += status->fChannels){
            if(!--size)
                break;
            val = (size) % div;
            if(val > div/2)
                val = div/2 - val;
            val = val*mul - 0x1000;
            for(j=0; j<status->fChannels; j++) {
                sampleBuffer[i+j] = val;
            }
        }

    sampleMap->release();
    statusMap->release();
    handler->clientClose();
     */
    return true;
}

UInt16 sharedIncrementMasterVolume()
{
    UInt16 result = 0;
    IOAudioManager *am = IOAudioManager::sharedInstance();

    if (am) {
        result = am->incrementMasterVolume();
    }

    return result;
}

UInt16 sharedDecrementMasterVolume()
{
    UInt16 result = 0;
    IOAudioManager *am = IOAudioManager::sharedInstance();

    if (am) {
        result = am->decrementMasterVolume();
    }

    return result;
}

bool sharedToggleMasterMute()
{
    bool result = false;
    IOAudioManager *am = IOAudioManager::sharedInstance();

    if (am) {
        result = am->toggleMasterMute();
    }

    return result;
}

OSDefineMetaClassAndStructors(IOAudioManager, IOService);

IOAudioManager *IOAudioManager::sharedInstance()
{
    return amInstance;
}

kern_return_t IOAudioClientIOTrap(IOAudioDMAEngineUserClient *userClient, UInt32 firstSampleFrame, UInt32 inputIO)
{
    kern_return_t result = kIOReturnBadArgument;
    
#ifdef DEBUGLOG
    IOLog("IOAudioClientIOTrap(0x%x, 0x%x, %d)\n", userClient, firstSampleFrame, inputIO);
#endif
    
    if (userClient) {
        result = userClient->performClientIO(firstSampleFrame, (inputIO != 0));
    }
    
    return result;
}
bool IOAudioManager::init(OSDictionary *properties)
{
    if (!IOService::init(properties)) {
        return false;
    }

    masterVolumeLeft = DEFAULT_INITIAL_VOLUME;
    masterVolumeRight = DEFAULT_INITIAL_VOLUME;
    masterMute = DEFAULT_INITIAL_MUTE;
    masterVolumeIncrement = DEFAULT_INITIAL_INCREMENT;

    setProperty(kMasterVolumeLeft, (unsigned long long)masterVolumeLeft, sizeof(masterVolumeLeft)*8);
    setProperty(kMasterVolumeRight, (unsigned long long)masterVolumeRight, sizeof(masterVolumeRight)*8);
    setProperty(kMasterMute, (unsigned long long)masterMute, sizeof(masterMute)*8);
    setProperty(kMasterVolumeIncrement, (unsigned long long)masterVolumeIncrement, sizeof(masterVolumeIncrement)*8);

    audioDevices = NULL;
    publishNotify = NULL;
    driverLock = NULL;

    masterFunctions.incrementMasterVolume = &sharedIncrementMasterVolume;
    masterFunctions.decrementMasterVolume = &sharedDecrementMasterVolume;
    masterFunctions.toggleMasterMute = &sharedToggleMasterMute;

    masterAudioFunctions = &masterFunctions;
    playBeep = &IOBeep;
    
    return true;
}

void IOAudioManager::free()
{
    if (audioDevices) {
        audioDevices->release();
        audioDevices = 0;
    }

    if (publishNotify) {
        publishNotify->remove();
        publishNotify = 0;
    }

    if (driverLock) {
        IOLockFree(driverLock);
        driverLock = 0;
    }
    
    IOService::free();
}

bool IOAudioManager::start(IOService *provider)
{
    bool success = false;

    amInstance = this;

    do {
        if (!IOService::start(provider)) {
            break;
        }

        audioDevices = OSSet::withCapacity(1);

        driverLock = IOLockAlloc();

        publishNotify = addNotification(gIOPublishNotification,
                                        serviceMatching("IOAudioDevice"),
                                        (IOServiceNotificationHandler) &audioPublishNotificationHandler,
                                        this,
                                        0);

        if (!publishNotify || !driverLock) {
            break;
        }

        IOLockInit(driverLock);

        success = true;
    } while (false);

    if (!success) {
        amInstance = NULL;
    }

    return success;
}

IOReturn IOAudioManager::setProperties( OSObject * properties )
{
    OSDictionary *props;
    IOReturn result = kIOReturnSuccess;

    if (properties && (props = OSDynamicCast(OSDictionary, properties))) {
        OSCollectionIterator *iterator;
        OSObject *iteratorKey;

        iterator = OSCollectionIterator::withCollection(props);
        while ((iteratorKey = iterator->getNextObject())) {
            OSSymbol *key;

            key = OSDynamicCast(OSSymbol, iteratorKey);
            if (key) {
                OSNumber *value;
                value = OSDynamicCast(OSNumber, props->getObject(key));

                if (value) {
                    if (key->isEqualTo(kMasterVolumeLeft)) {
                        setMasterVolumeLeft(value->unsigned16BitValue());
                    } else if (key->isEqualTo(kMasterVolumeRight)) {
                        setMasterVolumeRight(value->unsigned16BitValue());
                    } else if (key->isEqualTo(kMasterMute)) {
                        setMasterMute(value->unsigned8BitValue());
                    } else if (key->isEqualTo(kMasterVolumeIncrement)) {
                        setMasterVolumeIncrement(value->unsigned16BitValue());
                    }
                } else {
                    result = kIOReturnBadArgument;
                    break;
                }
            } else {
                result = kIOReturnBadArgument;
                break;
            }
        }
    } else {
        result = kIOReturnBadArgument;
    }

    return result;
}

bool IOAudioManager::audioPublishNotificationHandler(IOAudioManager *self,
                                                     void *ref,
                                                     IOService *newService)
{
    if (OSDynamicCast(IOAudioDevice, newService)) {
        self->registerAudioDevice((IOAudioDevice *)newService);
    }

    return true;
}

bool IOAudioManager::registerAudioDevice(IOAudioDevice *device)
{
    this->attach(device);

    IOTakeLock(driverLock);
    
    audioDevices->setObject(device);

    device->setMasterMute(masterMute);
    device->setMasterVolumeLeft(masterVolumeLeft);
    device->setMasterVolumeRight(masterVolumeRight);

    IOUnlock(driverLock);
    
    return true;
}

void IOAudioManager::removeAudioDevice(IOAudioDevice *device)
{
    IOTakeLock(driverLock);

    audioDevices->removeObject(device);

    IOUnlock(driverLock);

    this->detach(device);
}

UInt16 IOAudioManager::getMasterVolumeLeft()
{
    return masterVolumeLeft;
}

UInt16 IOAudioManager::getMasterVolumeRight()
{
    return masterVolumeRight;
}

UInt16 IOAudioManager::setMasterVolumeLeft(UInt16 newMasterVolumeLeft)
{
    UInt16 oldMasterVolumeLeft;

    oldMasterVolumeLeft = masterVolumeLeft;
    masterVolumeLeft = newMasterVolumeLeft;

    if (masterVolumeLeft != oldMasterVolumeLeft) {
        OSCollectionIterator *iter;

        setProperty(kMasterVolumeLeft, (unsigned long long)masterVolumeLeft, sizeof(masterVolumeLeft)*8);

        IOTakeLock(driverLock);
        
        iter = OSCollectionIterator::withCollection(audioDevices);
        if (iter) {
            IOAudioDevice *device;

            while ((device = (IOAudioDevice *)iter->getNextObject()) != NULL) {
                device->setMasterVolumeLeft(masterVolumeLeft);
            }

            iter->release();
        }

        IOUnlock(driverLock);
    }

    return oldMasterVolumeLeft;
}

UInt16 IOAudioManager::setMasterVolumeRight(UInt16 newMasterVolumeRight)
{
    UInt16 oldMasterVolumeRight;

    oldMasterVolumeRight = masterVolumeRight;
    masterVolumeRight = newMasterVolumeRight;

    if (masterVolumeRight != oldMasterVolumeRight) {
        OSCollectionIterator *iter;

        setProperty(kMasterVolumeRight, (unsigned long long)masterVolumeRight, sizeof(masterVolumeRight)*8);

        IOTakeLock(driverLock);

        iter = OSCollectionIterator::withCollection(audioDevices);
        if (iter) {
            IOAudioDevice *device;

            while ((device = (IOAudioDevice *)iter->getNextObject()) != NULL) {
                device->setMasterVolumeRight(masterVolumeRight);
            }

            iter->release();
        }

        IOUnlock(driverLock);
    }

    return oldMasterVolumeRight;
}

bool IOAudioManager::getMasterMute()
{
    return masterMute;
}

bool IOAudioManager::setMasterMute(bool newMasterMute)
{
    bool oldMasterMute;

    oldMasterMute = masterMute;
    masterMute = newMasterMute;

    if (masterMute != oldMasterMute) {
        OSCollectionIterator *iter;

        setProperty(kMasterMute, (unsigned long long)masterMute, sizeof(masterMute)*8);

        IOTakeLock(driverLock);

        iter = OSCollectionIterator::withCollection(audioDevices);
        if (iter) {
            IOAudioDevice *device;

            while ((device = (IOAudioDevice *)iter->getNextObject()) != NULL) {
                device->setMasterMute(masterMute);
            }

            iter->release();
        }

        IOUnlock(driverLock);
    }

    return oldMasterMute;
}

UInt16 IOAudioManager::getMasterVolumeIncrement()
{
    return masterVolumeIncrement;
}

UInt16 IOAudioManager::setMasterVolumeIncrement(UInt16 newMasterVolumeIncrement)
{
    UInt16 oldMasterVolumeIncrement;

    oldMasterVolumeIncrement = masterVolumeIncrement;
    masterVolumeIncrement = newMasterVolumeIncrement;

    setProperty(kMasterVolumeIncrement, (unsigned long long)masterVolumeIncrement, sizeof(masterVolumeIncrement)*8);

    return oldMasterVolumeIncrement;
}

UInt16 IOAudioManager::incrementMasterVolume()
{
    SInt32 newMasterVolumeLeft;
    SInt32 newMasterVolumeRight;

    newMasterVolumeLeft = masterVolumeLeft + masterVolumeIncrement;
    newMasterVolumeRight = masterVolumeRight + masterVolumeIncrement;
    
    if (newMasterVolumeLeft > MASTER_VOLUME_MAX) {
        newMasterVolumeRight = newMasterVolumeRight - (newMasterVolumeLeft - MASTER_VOLUME_MAX);
        newMasterVolumeLeft = MASTER_VOLUME_MAX;
    }

    if (newMasterVolumeRight > MASTER_VOLUME_MAX) {
        newMasterVolumeLeft = newMasterVolumeLeft - (newMasterVolumeRight - MASTER_VOLUME_MAX);
        newMasterVolumeRight = MASTER_VOLUME_MAX;
    }

    setMasterVolumeLeft(newMasterVolumeLeft);
    setMasterVolumeRight(newMasterVolumeRight);

    return masterVolumeLeft > masterVolumeRight ? masterVolumeLeft : masterVolumeRight;
}

UInt16 IOAudioManager::decrementMasterVolume()
{
    SInt32 newMasterVolumeLeft;
    SInt32 newMasterVolumeRight;

    newMasterVolumeLeft = masterVolumeLeft - masterVolumeIncrement;
    newMasterVolumeRight = masterVolumeRight - masterVolumeIncrement;
    
    if (newMasterVolumeLeft < MASTER_VOLUME_MIN) {
        newMasterVolumeRight = newMasterVolumeRight + (MASTER_VOLUME_MIN - newMasterVolumeLeft);
        newMasterVolumeLeft = MASTER_VOLUME_MIN;
    }

    if (newMasterVolumeRight < MASTER_VOLUME_MIN) {
        newMasterVolumeLeft = newMasterVolumeLeft + (MASTER_VOLUME_MIN - newMasterVolumeRight);
        newMasterVolumeRight = MASTER_VOLUME_MIN;
    }

    setMasterVolumeLeft(newMasterVolumeLeft);
    setMasterVolumeRight(newMasterVolumeRight);

    return masterVolumeLeft > masterVolumeRight ? masterVolumeLeft : masterVolumeRight;
}

bool IOAudioManager::toggleMasterMute()
{
    setMasterMute(!masterMute);
    return masterMute;
}
