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

#include <IOKit/audio/IOAudioDMAEngineUserClient.h>
#include <IOKit/audio/IOAudioDMAEngine.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioDebug.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

typedef struct IOAudioClientBuffer {
    IOAudioStream *audioStream;
    void *sourceBuffer;
    IOMemoryDescriptor *sourceBufferDescriptor;
    IOMemoryMap *sourceBufferMap;
    void *unmappedSourceBuffer;
    struct IOAudioClientBuffer *next;
} IOAudioClientBuffer;

typedef struct IOAudioFormatNotification {
    IOAudioStream				*audioStream;
    mach_port_t					port;
    IOAudioFormatNotification	*next;
} IOAudioFormatNotification;

#define super IOUserClient

OSDefineMetaClassAndStructors(IOAudioDMAEngineUserClient, IOUserClient)

IOAudioDMAEngineUserClient *IOAudioDMAEngineUserClient::withDMAEngine(IOAudioDMAEngine *dmaEngine, task_t clientTask, void *securityToken, UInt32 type)
{
    IOAudioDMAEngineUserClient *client;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient::withDMAEngine(0x%x, 0x%x, 0x%x, 0x%x)\n", dmaEngine, clientTask, securityToken, type);
#endif

    client = new IOAudioDMAEngineUserClient;

    if (client) {
        if (!client->initWithDMAEngine(dmaEngine, clientTask, securityToken, type)) {
            client->release();
            client = 0;
        }
    }

    return client;
}

bool IOAudioDMAEngineUserClient::initWithDMAEngine(IOAudioDMAEngine *dmaEngine, task_t task, void *securityToken, UInt32 type)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::initWithDMAEngine(0x%x, 0x%x, 0x%x, 0x%x)\n", this, dmaEngine, task, securityToken, type);
#endif
    
    if (!initWithTask(task, securityToken, type)) {
        return false;
    }

    if (!dmaEngine || !task) {
        return false;
    }

    clientTask = task;
    audioDMAEngine = dmaEngine;
    
    clientBufferList = NULL;
    formatNotificationList = NULL;
    
    clientBufferListLock = IORecursiveLockAlloc();
    if (!clientBufferListLock) {
        return false;
    }
    
    workLoop = audioDMAEngine->getWorkLoop();
    if (!workLoop) {
        return false;
    }
    
    workLoop->retain();
    
    commandGate = IOCommandGate::commandGate(this);
    if (!commandGate) {
        return false;
    }
    
    workLoop->addEventSource(commandGate);
    
    methods[kAudioDMAEngineCallRegisterClientBuffer].object = this;
    methods[kAudioDMAEngineCallRegisterClientBuffer].func = (IOMethod) &IOAudioDMAEngineUserClient::registerClientBuffer;
    methods[kAudioDMAEngineCallRegisterClientBuffer].count0 = 3;
    methods[kAudioDMAEngineCallRegisterClientBuffer].count1 = 0;
    methods[kAudioDMAEngineCallRegisterClientBuffer].flags = kIOUCScalarIScalarO;
    
    methods[kAudioDMAEngineCallUnregisterClientBuffer].object = this;
    methods[kAudioDMAEngineCallUnregisterClientBuffer].func = (IOMethod) &IOAudioDMAEngineUserClient::unregisterClientBuffer;
    methods[kAudioDMAEngineCallUnregisterClientBuffer].count0 = 1;
    methods[kAudioDMAEngineCallUnregisterClientBuffer].count1 = 0;
    methods[kAudioDMAEngineCallUnregisterClientBuffer].flags = kIOUCScalarIScalarO;
    
    methods[kAudioDMAEngineCallGetConnectionID].object = this;
    methods[kAudioDMAEngineCallGetConnectionID].func = (IOMethod) &IOAudioDMAEngineUserClient::getConnectionID;
    methods[kAudioDMAEngineCallGetConnectionID].count0 = 0;
    methods[kAudioDMAEngineCallGetConnectionID].count1 = 1;
    methods[kAudioDMAEngineCallGetConnectionID].flags = kIOUCScalarIScalarO;
    
    trap.object = this;
    trap.func = (IOTrap) &IOAudioDMAEngineUserClient::performClientIO;

    return true;
}

void IOAudioDMAEngineUserClient::free()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::free()\n", this);
#endif

    if (clientBufferListLock) {
        IORecursiveLockFree(clientBufferListLock);
        clientBufferListLock = NULL;
    }
    
    while (clientBufferList) {
        IOAudioClientBuffer *next = clientBufferList->next;
        
        if (clientBufferList->audioStream) {
            clientBufferList->audioStream->removeClient();
            clientBufferList->audioStream->release();
        }
        
        if (clientBufferList->sourceBufferDescriptor != NULL) {
            clientBufferList->sourceBufferDescriptor->complete();
            clientBufferList->sourceBufferDescriptor->release();
        }
        
        if (clientBufferList->sourceBufferMap != NULL) {
            clientBufferList->sourceBufferMap->release();
        }

        IOFree(clientBufferList, sizeof(IOAudioClientBuffer));
        clientBufferList = next;
    }
    
    if (notificationMessage) {
        IOFree(notificationMessage, sizeof(IOAudioNotificationMessage));
        notificationMessage = NULL;
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

IOReturn IOAudioDMAEngineUserClient::clientClose()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::clientClose()\n", this);
#endif

    if (audioDMAEngine && !isInactive()) {
        audioDMAEngine->clientClosed(this);
        audioDMAEngine = NULL;
    }
    return kIOReturnSuccess;
}

IOReturn IOAudioDMAEngineUserClient::clientDied()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::clientDied()\n", this);
#endif

    return clientClose();
}

IOReturn IOAudioDMAEngineUserClient::clientMemoryForType(UInt32 type, UInt32 *flags, IOMemoryDescriptor **memory)
{
    IOReturn	result = kIOReturnSuccess;
    void *	sharedMemory = 0;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::clientMemoryForType(0x%x, 0x%x, 0x%x)\n", this, type, flags, memory);
#endif

    switch(type) {
        case kStatusBuffer:
            assert(audioDMAEngine);
            
            sharedMemory = (void *)audioDMAEngine->getStatus();
            
            if (sharedMemory) {
                *memory = IOMemoryDescriptor::withAddress(sharedMemory, sizeof(IOAudioDMAEngineStatus), kIODirectionNone);
                *flags = kIOMapReadOnly;
            } else {
                result = kIOReturnError;
            }
            
            break;
        default:
            result = kIOReturnUnsupported;
            break;
    }

    return result;
}

IOExternalMethod *IOAudioDMAEngineUserClient::getExternalMethodForIndex(UInt32 index)
{
    IOExternalMethod *method = 0;

#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::getExternalMethodForIndex(0x%x)\n", this, index);
#endif

    if (index < IOAUDIODMAENGINE_NUM_CALLS) {
        method = &methods[index];
    }

    return method;
}

IOExternalTrap *IOAudioDMAEngineUserClient::getExternalTrapForIndex( UInt32 index )
{
    if (index != kAudioDMAEngineTrapPerformClientIO) {
        return NULL;
    }
    
    return &trap;
}

IOReturn IOAudioDMAEngineUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::registerNotificationPort(0x%x, 0x%x, 0x%x)\n", this, port, type, refCon);
#endif

    switch (type) {
        case kAudioDMAEngineStreamFormatChangeNotification:
            if (!OSDynamicCast(IOAudioStream, (OSObject *)refCon) && (port != MACH_PORT_NULL)) {
                result = kIOReturnBadArgument;
                break;
            }
            
            assert(commandGate);
            
            result = commandGate->runAction(registerFormatNotificationAction, (void *)port, (void *)refCon);
            
            break;
        default:
            result = kIOReturnBadArgument;
            break;
    }
    // Create a single message, but keep a dict or something of all of the IOAudioStreams registered for
    // refCon is IOAudioStream *
    
    return result;
}

IOReturn IOAudioDMAEngineUserClient::registerFormatNotificationAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient::registerFormatNotificationAction(0x%x, 0x%x, 0x%x)\n", owner, arg1, arg2);
#endif

    if (owner) {
        IOAudioDMAEngineUserClient *userClient = OSDynamicCast(IOAudioDMAEngineUserClient, owner);
        
        if (userClient) {
            result = userClient->registerFormatNotification((mach_port_t)arg1, (IOAudioStream *)arg2);
        }
    }
    
    return result;
}

IOReturn IOAudioDMAEngineUserClient::registerFormatNotification(mach_port_t port, IOAudioStream *audioStream)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioFormatNotification *formatNotification;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::registerFormatNotification(0x%x, 0x%x)\n", this, port, audioStream);
#endif

    if (!isInactive()) {
        if (port == MACH_PORT_NULL) {	// We need to remove this notification
            IOAudioFormatNotification *previousNotification = NULL;
            
            formatNotification = formatNotificationList;
            while (formatNotification) {
                if ((formatNotification->port == port) && ((audioStream == NULL) || (formatNotification->audioStream == audioStream))) {
                    IOAudioFormatNotification *nextNotification;
                    
                    if (previousNotification) {
                        previousNotification->next = formatNotification->next;
                    } else {
                        formatNotificationList = formatNotification->next;
                    }
                    
                    nextNotification = formatNotification->next;
                    
                    IOFree(formatNotification, sizeof(IOAudioFormatNotification));
                    
                    formatNotification = nextNotification;
                    
                    if (audioStream != NULL) {
                        break;
                    }
                } else {
                    previousNotification = formatNotification;
                    formatNotification = formatNotification->next;
                }
            }
        } else {
            formatNotification = formatNotificationList;
            while (formatNotification) {
                if ((formatNotification->port == port) && (formatNotification->audioStream == audioStream)) {
                    break;
                }
                formatNotification = formatNotification->next;
            }
            
            if (!formatNotification) {
                if (notificationMessage == NULL) {
                    notificationMessage = (IOAudioNotificationMessage *)IOMalloc(sizeof(IOAudioNotificationMessage));
                    
                    if (notificationMessage) {
                        notificationMessage->messageHeader.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
                        notificationMessage->messageHeader.msgh_size = sizeof(IOAudioNotificationMessage);
                        notificationMessage->messageHeader.msgh_local_port = MACH_PORT_NULL;
                        notificationMessage->messageHeader.msgh_reserved = 0;
                        notificationMessage->messageHeader.msgh_id = 0;
                    } else {
                        result = kIOReturnNoMemory;
                    }
                }
                
                if (notificationMessage) {
                    formatNotification = (IOAudioFormatNotification *)IOMalloc(sizeof(IOAudioFormatNotification));
                    if (formatNotification) {
                        formatNotification->audioStream = audioStream;
                        formatNotification->port = port;
                        formatNotification->next = formatNotificationList;
                        
                        formatNotificationList = formatNotification;
                    } else {
                        result = kIOReturnNoMemory;
                    }
                }
            }
        }
    } else {
        result = kIOReturnNoDevice;
    }
    
    return result;
}

IOReturn IOAudioDMAEngineUserClient::registerClientBuffer(IOAudioStream *audioStream, void *sourceBuffer, UInt32 bufSizeInBytes)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioClientBuffer *clientBuffer;
    IODirection bufferDirection;
    const IOAudioStreamFormat *streamFormat;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::registerClientBuffer(0x%x, 0x%x, 0x%x)\n", (unsigned int)this, (unsigned int)audioStream, (unsigned int)sourceBuffer, (unsigned int)bufSizeInBytes);
#endif
    
    if (!isInactive()) {
        if (!sourceBuffer || !audioStream || (bufSizeInBytes == 0) ) {
            return kIOReturnBadArgument;
        }
        
        streamFormat = audioStream->getFormat();
        
        if (!streamFormat) {
            return kIOReturnError;
        }
        
        numSampleFrames = bufSizeInBytes / (IOAUDIODMAENGINE_DEFAULT_MIX_BUFFER_SAMPLE_SIZE * streamFormat->fNumChannels) ;
        
        assert(clientBufferListLock);
        
        IORecursiveLockLock(clientBufferListLock);
        
        /* - removing this for now
        // make sure it isn't already registered
        if (getClientBuffer(sourceBuffer, audioStream) != NULL) {
            result = kIOReturnBadArgument;
            goto Exit;
        }
        */
        
        // allocate IOAudioClientBuffer to hold buffer descriptor, etc...
        clientBuffer = (IOAudioClientBuffer *)IOMalloc(sizeof(IOAudioClientBuffer));
        if (!clientBuffer) {
            result = kIOReturnNoMemory;
            goto Exit;
        }
        
        bufferDirection = audioStream->getDirection() == kAudioOutput ? kIODirectionIn : kIODirectionOut;
        
        audioStream->retain();
        clientBuffer->audioStream = audioStream;
        clientBuffer->sourceBufferDescriptor = IOMemoryDescriptor::withAddress((vm_address_t)sourceBuffer, bufSizeInBytes, bufferDirection, clientTask);
        if (!clientBuffer->sourceBufferDescriptor) {
            result = kIOReturnInternalError;
            goto Exit;
        }
        
        if ((result = clientBuffer->sourceBufferDescriptor->prepare()) != kIOReturnSuccess) {
            goto Exit;
        }
        
        clientBuffer->sourceBufferMap = clientBuffer->sourceBufferDescriptor->map();
        
        if (clientBuffer->sourceBufferMap == NULL) {
            IOLog("IOAudioDMAEngineUserClient<0x%x>::registerClientBuffer() - error mapping memory.\n", (unsigned int)this);
            result = kIOReturnVMError;
            goto Exit;
        }
        
        clientBuffer->sourceBuffer = (void *)clientBuffer->sourceBufferMap->getVirtualAddress();
        if (clientBuffer->sourceBuffer == NULL) {
            result = kIOReturnVMError;
            goto Exit;
        }
        
        clientBuffer->unmappedSourceBuffer = sourceBuffer;
        clientBuffer->next = NULL;
        
        if (clientBufferList == NULL) {
            clientBufferList = clientBuffer;
        } else {
            IOAudioClientBuffer *clientBufPtr = clientBufferList;
            while (clientBufPtr->next != NULL) {
                clientBufPtr = clientBufPtr->next;
            }
            clientBufPtr->next = clientBuffer;
        }
        
    Exit:
        
        IORecursiveLockUnlock(clientBufferListLock);
        
        if (result != kIOReturnSuccess) {
            if (clientBuffer != NULL) {
                if (clientBuffer->sourceBufferDescriptor != NULL) {
                    clientBuffer->sourceBufferDescriptor->release();
                }
                if (clientBuffer->sourceBufferMap != NULL) {
                    clientBuffer->sourceBufferMap->release();
                }
                if (clientBuffer->audioStream) {
                    clientBuffer->audioStream->release();
                }
                IOFree(clientBuffer, sizeof(IOAudioClientBuffer));
            }
        } else {
            audioStream->addClient();
        }
    } else {
        result = kIOReturnNoDevice;
    }
    
    return result;
}

IOReturn IOAudioDMAEngineUserClient::unregisterClientBuffer(void *sourceBuffer)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioClientBuffer *clientBuf, *previousBuf = NULL;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::unregisterClientBuffer(0x%x)\n", (unsigned int)this, (unsigned int)sourceBuffer);
#endif

    if (!sourceBuffer) {
        return kIOReturnBadArgument;
    }
    
    assert(clientBufferListLock);
    
    IORecursiveLockLock(clientBufferListLock);
    
    clientBuf = clientBufferList;
    while (clientBuf) {
        if (clientBuf->unmappedSourceBuffer == sourceBuffer) {
            IOAudioClientBuffer *nextBuf;
            
            if (previousBuf) {
                previousBuf->next = clientBuf->next;
            } else {
                clientBufferList = clientBuf->next;
            }
            
            if (clientBuf->audioStream) {
                clientBuf->audioStream->removeClient();
                clientBuf->audioStream->release();
            }
            
            if (clientBuf->sourceBufferDescriptor != NULL) {
                clientBuf->sourceBufferDescriptor->complete();
                clientBuf->sourceBufferDescriptor->release();
            }
            
            if (clientBuf->sourceBufferMap != NULL) {
                clientBuf->sourceBufferMap->release();
            }

            nextBuf = clientBuf->next;
            
            IOFree(clientBuf, sizeof(IOAudioClientBuffer));
            
            clientBuf = nextBuf;
        } else {
            previousBuf = clientBuf;
            clientBuf = clientBuf->next;
        }
    }
    
    IORecursiveLockUnlock(clientBufferListLock);
    
    return result;
}

IOReturn IOAudioDMAEngineUserClient::performClientIO(UInt32 firstSampleFrame, bool inputIO)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioClientBuffer *clientBuf;
    
#ifdef DEBUG_IO_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::performClientIO(0x%x, %d)\n", this, firstSampleFrame, inputIO);
#endif
    
    assert(clientBufferListLock);
    assert(audioDMAEngine);
    
    if (!isInactive()) {
    
        IORecursiveLockLock(clientBufferListLock);
        
        clientBuf = clientBufferList;
        while (clientBuf) {
            IOReturn tmpResult;
            IOAudioStream *audioStream;
            const IOAudioStreamFormat *streamFormat;
                
            audioStream = clientBuf->audioStream;
            
            assert(audioStream);
            assert(clientBuf->sourceBuffer);
            
            audioStream->lockStreamForIO();
            
            streamFormat = audioStream->getFormat();
            
            assert(streamFormat);
            
            tmpResult = kIOReturnSuccess;
            
            if (inputIO) {
                if (audioStream->getDirection() == kAudioInput) {
                    tmpResult = audioDMAEngine->convertFromInputStream(audioStream->getSampleBuffer(), clientBuf->sourceBuffer, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
                }
            } else {
                if (audioStream->getDirection() == kAudioOutput) {
                    tmpResult = audioDMAEngine->mixAndClip(clientBuf->sourceBuffer, audioStream->getMixBuffer(), audioStream->getSampleBuffer(), firstSampleFrame, numSampleFrames, streamFormat, audioStream);
                }
            }
            
            audioStream->unlockStreamForIO();
            
            if (tmpResult != kIOReturnSuccess) {
                result = tmpResult;
            }
            
            clientBuf = clientBuf->next;
        }
        
        IORecursiveLockUnlock(clientBufferListLock);
    } else {
        result = kIOReturnNoDevice;
    }
    
    return result;
}

IOReturn IOAudioDMAEngineUserClient::getConnectionID(UInt32 *connectionID)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::getConnectionID(0x%x)\n", (unsigned int)this, (unsigned int)connectionID);
#endif

    *connectionID = (UInt32)this;
    return kIOReturnSuccess;
}

// Must be done on workLoop
void IOAudioDMAEngineUserClient::sendFormatChangeNotification(IOAudioStream *audioStream)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioDMAEngineUserClient[0x%x]::sendFormatChangeNotification(0x%x)\n", this, audioStream);
#endif

    if (notificationMessage && formatNotificationList) {
        IOAudioFormatNotification *formatNotification;
        
        formatNotification = formatNotificationList;
        while (formatNotification) {
            if (formatNotification->audioStream == audioStream) {
                notificationMessage->messageHeader.msgh_remote_port = formatNotification->port;
                notificationMessage->type = kAudioDMAEngineStreamFormatChangeNotification;
                notificationMessage->ref = (UInt32)formatNotification->audioStream;
            }
            
            formatNotification = formatNotification->next;
        }
    }
}
