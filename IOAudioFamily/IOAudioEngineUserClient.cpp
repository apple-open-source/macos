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

#include <IOKit/audio/IOAudioEngineUserClient.h>
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioDebug.h>
#include <IOKit/audio/IOAudioDefines.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#define super OSObject

class IOAudioClientBufferSet : public OSObject
{
    OSDeclareDefaultStructors(IOAudioClientBufferSet);

public:
    UInt32					bufferSetID;
    IOAudioEngineUserClient	*userClient;
    IOAudioClientBuffer		*outputBufferList;
    IOAudioClientBuffer		*inputBufferList;
    IOAudioEnginePosition	nextOutputPosition;
    AbsoluteTime			outputTimeout;
    AbsoluteTime			sampleInterval;
    IOAudioClientBufferSet	*next;
    thread_call_t			watchdogThreadCall;
    UInt32					generationCount;
    bool					timerPending;
    
    bool init(UInt32 setID, IOAudioEngineUserClient *client);
    void free();
    
#ifdef DEBUG
    void retain() const;
    void release() const;
#endif

    void resetNextOutputPosition();

    void allocateWatchdogTimer();
    void freeWatchdogTimer();
    
    void setWatchdogTimeout(AbsoluteTime *timeout);
    void cancelWatchdogTimer();

    static void watchdogTimerFired(IOAudioClientBufferSet *clientBufferSet, UInt32 generationCount);
};

OSDefineMetaClassAndStructors(IOAudioClientBufferSet, OSObject)

bool IOAudioClientBufferSet::init(UInt32 setID, IOAudioEngineUserClient *client)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioClientBufferSet[%p]::init(%lx, %p)\n", this, setID, client);
#endif

    if (!super::init()) {
        return false;
    }
    
    if (client == NULL) {
        return false;
    }
    
    bufferSetID = setID;
    client->retain();
    userClient = client;
    
    outputBufferList = NULL;
    inputBufferList = NULL;
    next = NULL;
    watchdogThreadCall = NULL;
    generationCount = 0;
    timerPending = false;
    
    resetNextOutputPosition();
    
    return true;
}

void IOAudioClientBufferSet::free()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioClientBufferSet[%p]::free()\n", this);
#endif

    if (userClient != NULL) {
        userClient->release();
        userClient = NULL;
    }
    
    if (watchdogThreadCall) {
        freeWatchdogTimer();
    }
    
    super::free();
}

#ifdef DEBUG
void IOAudioClientBufferSet::retain() const
{
    IOLog("IOAudioClientBufferSet[%p]::retain() - %d\n", this, getRetainCount());
    super::retain();
}

void IOAudioClientBufferSet::release() const
{
    IOLog("IOAudioClientBufferSet[%p]::release() - %d\n", this, getRetainCount());
    super::release();
}
#endif

void IOAudioClientBufferSet::resetNextOutputPosition()
{
    nextOutputPosition.fLoopCount = 0;
    nextOutputPosition.fSampleFrame = 0;
}

void IOAudioClientBufferSet::allocateWatchdogTimer()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioClientBufferSet[%p]::allocateWatchdogTimer()\n", this);
#endif

    if (watchdogThreadCall == NULL) {
        watchdogThreadCall = thread_call_allocate((thread_call_func_t)IOAudioClientBufferSet::watchdogTimerFired, (thread_call_param_t)this);
    }
}

void IOAudioClientBufferSet::freeWatchdogTimer()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioClientBufferSet[%p]::freeWatchdogTimer()\n", this);
#endif

    if (watchdogThreadCall != NULL) {
        cancelWatchdogTimer();
        thread_call_free(watchdogThreadCall);
        watchdogThreadCall = NULL;
    }
}

void IOAudioClientBufferSet::setWatchdogTimeout(AbsoluteTime *timeout)
{
    if (watchdogThreadCall == NULL) {
        // allocate it here
        IOLog("IOAudioClientBufferSet[%p]::setWatchdogTimeout() - no thread call.\n", this);
    }
    
    assert(watchdogThreadCall);
    
    outputTimeout = *timeout;
    
    generationCount++;
    
	userClient->lockBuffers();

    if (!timerPending) {
        retain();
    }
    
    timerPending = true;
	userClient->unlockBuffers();

    thread_call_enter1_delayed(watchdogThreadCall, (thread_call_param_t)generationCount, outputTimeout);
}

void IOAudioClientBufferSet::cancelWatchdogTimer()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioClientBufferSet[%p]::cancelWatchdogTimer()\n", this);
#endif

	if (NULL != userClient) {
		userClient->lockBuffers();
		timerPending = false;
		userClient->unlockBuffers();
	}
}

void IOAudioClientBufferSet::watchdogTimerFired(IOAudioClientBufferSet *clientBufferSet, UInt32 generationCount)
{
    IOAudioEngineUserClient *userClient;
    
#ifdef DEBUG_CALLS
    AbsoluteTime now;
    clock_get_uptime(&now);
    IOLog("IOAudioClientBufferSet[%p]::watchdogTimerFired(%ld):(%lx,%lx)(%lx,%lx)(%lx,%lx)\n", clientBufferSet, generationCount, now.hi, now.lo, clientBufferSet->outputTimeout.hi, clientBufferSet->outputTimeout.lo, clientBufferSet->nextOutputPosition.fLoopCount, clientBufferSet->nextOutputPosition.fSampleFrame);
#endif

    assert(clientBufferSet);
    assert(clientBufferSet->userClient);

	userClient = clientBufferSet->userClient;
	userClient->retain();
	userClient->lockBuffers();

	if(clientBufferSet->timerPending != false) {
		userClient->performWatchdogOutput(clientBufferSet, generationCount);
	}

	// If there's no timer pending once we attempt to do the watchdog I/O
	// then we need to release the set
	if (!clientBufferSet->timerPending) {
		clientBufferSet->release();
	}

	userClient->unlockBuffers();
	userClient->release();
}

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOAudioEngineUserClient, IOUserClient)
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 0);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 1);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 2);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 3);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 4);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 5);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 6);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 7);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 8);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 9);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 10);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 11);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 12);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 13);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 14);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 15);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 16);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 17);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 18);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 19);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 20);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 21);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 22);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 23);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 24);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 25);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 26);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 27);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 28);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 29);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 30);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 31);


IOAudioEngineUserClient *IOAudioEngineUserClient::withAudioEngine(IOAudioEngine *engine, task_t clientTask, void *securityToken, UInt32 type)
{
    IOAudioEngineUserClient *client;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient::withAudioEngine(%p, 0x%lx, %p, 0x%lx)\n", engine, (UInt32)clientTask, securityToken, type);
#endif

    client = new IOAudioEngineUserClient;

    if (client) {
        if (!client->initWithAudioEngine(engine, clientTask, securityToken, type)) {
            client->release();
            client = 0;
        }
    }

    return client;
}

bool IOAudioEngineUserClient::initWithAudioEngine(IOAudioEngine *engine, task_t task, void *securityToken, UInt32 type)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::initWithAudioEngine(%p, 0x%lx, %p, 0x%lx)\n", this, engine, (UInt32)task, securityToken, type);
#endif
    
    if (!initWithTask(task, securityToken, type)) {
        return false;
    }

    if (!engine || !task) {
        return false;
    }

    clientTask = task;
    audioEngine = engine;
    
    setOnline(false);

    clientBufferSetList = NULL;
    
    clientBufferLock = IORecursiveLockAlloc();
    if (!clientBufferLock) {
        return false;
    }
    
    workLoop = audioEngine->getWorkLoop();
    if (!workLoop) {
        return false;
    }
    
    workLoop->retain();
    
    commandGate = IOCommandGate::commandGate(this);
    if (!commandGate) {
        return false;
    }
    
    workLoop->addEventSource(commandGate);
    
    methods[kIOAudioEngineCallRegisterClientBuffer].object = this;
    methods[kIOAudioEngineCallRegisterClientBuffer].func = (IOMethod) &IOAudioEngineUserClient::registerBuffer;
    methods[kIOAudioEngineCallRegisterClientBuffer].count0 = 4;
    methods[kIOAudioEngineCallRegisterClientBuffer].count1 = 0;
    methods[kIOAudioEngineCallRegisterClientBuffer].flags = kIOUCScalarIScalarO;
    
    methods[kIOAudioEngineCallUnregisterClientBuffer].object = this;
    methods[kIOAudioEngineCallUnregisterClientBuffer].func = (IOMethod) &IOAudioEngineUserClient::unregisterBuffer;
    methods[kIOAudioEngineCallUnregisterClientBuffer].count0 = 2;
    methods[kIOAudioEngineCallUnregisterClientBuffer].count1 = 0;
    methods[kIOAudioEngineCallUnregisterClientBuffer].flags = kIOUCScalarIScalarO;
    
    methods[kIOAudioEngineCallGetConnectionID].object = this;
    methods[kIOAudioEngineCallGetConnectionID].func = (IOMethod) &IOAudioEngineUserClient::getConnectionID;
    methods[kIOAudioEngineCallGetConnectionID].count0 = 0;
    methods[kIOAudioEngineCallGetConnectionID].count1 = 1;
    methods[kIOAudioEngineCallGetConnectionID].flags = kIOUCScalarIScalarO;
    
    methods[kIOAudioEngineCallStart].object = this;
    methods[kIOAudioEngineCallStart].func = (IOMethod) &IOAudioEngineUserClient::clientStart;
    methods[kIOAudioEngineCallStart].count0 = 0;
    methods[kIOAudioEngineCallStart].count1 = 0;
    methods[kIOAudioEngineCallStart].flags = kIOUCScalarIScalarO;
    
    methods[kIOAudioEngineCallStop].object = this;
    methods[kIOAudioEngineCallStop].func = (IOMethod) &IOAudioEngineUserClient::clientStop;
    methods[kIOAudioEngineCallStop].count0 = 0;
    methods[kIOAudioEngineCallStop].count1 = 0;
    methods[kIOAudioEngineCallStop].flags = kIOUCScalarIScalarO;
    
    trap.object = this;
    trap.func = (IOTrap) &IOAudioEngineUserClient::performClientIO;

    return true;
}

void IOAudioEngineUserClient::free()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::free()\n", this);
#endif

    if (clientBufferLock) {
        IORecursiveLockFree(clientBufferLock);
        clientBufferLock = NULL;
    }
    
    freeClientBufferSetList();
    
    if (notificationMessage) {
        IOFreeAligned(notificationMessage, sizeof(IOAudioNotificationMessage));
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

void IOAudioEngineUserClient::freeClientBufferSetList()
{
    while (clientBufferSetList) {
        IOAudioClientBufferSet *nextSet;
        
        while (clientBufferSetList->outputBufferList) {
            IOAudioClientBuffer *next = clientBufferSetList->outputBufferList->next;
            
            freeClientBuffer(clientBufferSetList->outputBufferList);
            
            clientBufferSetList->outputBufferList = next;
        }

        while (clientBufferSetList->inputBufferList) {
            IOAudioClientBuffer *next = clientBufferSetList->inputBufferList->next;
            
            freeClientBuffer(clientBufferSetList->inputBufferList);
            
            clientBufferSetList->inputBufferList = next;
        }
        
        nextSet = clientBufferSetList->next;
        
        clientBufferSetList->cancelWatchdogTimer();
        clientBufferSetList->release();
        
        clientBufferSetList = nextSet;
    }
    
}

void IOAudioEngineUserClient::freeClientBuffer(IOAudioClientBuffer *clientBuffer) 
{
    if (clientBuffer) {
        if (clientBuffer->audioStream) {
            clientBuffer->audioStream->removeClient(clientBuffer);
            clientBuffer->audioStream->release();
        }
        
        if (clientBuffer->sourceBufferDescriptor != NULL) {
            clientBuffer->sourceBufferDescriptor->complete();
            clientBuffer->sourceBufferDescriptor->release();
        }
        
        if (clientBuffer->sourceBufferMap != NULL) {
            clientBuffer->sourceBufferMap->release();
        }

        IOFreeAligned(clientBuffer, sizeof(IOAudioClientBuffer));
    }
}

void IOAudioEngineUserClient::stop(IOService *provider)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::stop(%p)\n", this, provider);
#endif

    assert(commandGate);
    
    commandGate->runAction(stopClientAction);
    
    // We should be both inactive and offline at this point, 
    // so it is safe to free the client buffer set list without holding the lock
    
    freeClientBufferSetList();

    super::stop(provider);
}

IOReturn IOAudioEngineUserClient::clientClose()
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::clientClose()\n", this);
#endif

    if (audioEngine && !isInactive()) {
        assert(commandGate);
            
        result = commandGate->runAction(closeClientAction);
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::clientDied()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::clientDied()\n", this);
#endif

    return clientClose();
}

IOReturn IOAudioEngineUserClient::closeClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        if (userClient) {
            result = userClient->closeClient();
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::closeClient()
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::closeClient()\n", this);
#endif

    if (audioEngine && !isInactive()) {
        if (isOnline()) {
            stopClient();
        }
        audioEngine->clientClosed(this);
        audioEngine = NULL;
    }
    
    return kIOReturnSuccess;
}

void IOAudioEngineUserClient::setOnline(bool newOnline)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::setOnline(%d)\n", this, newOnline);
#endif

    if (online != newOnline) {
        online = newOnline;
        setProperty(kIOAudioEngineUserClientActiveKey, (unsigned long long)(online ? 1 : 0), sizeof(unsigned long long)*8);
    }
}

bool IOAudioEngineUserClient::isOnline()
{
    return online;
}

void IOAudioEngineUserClient::lockBuffers()
{
    assert(clientBufferLock);
    
    IORecursiveLockLock(clientBufferLock);
}

void IOAudioEngineUserClient::unlockBuffers()
{
    assert(clientBufferLock);
    
    IORecursiveLockUnlock(clientBufferLock);
}

IOReturn IOAudioEngineUserClient::clientMemoryForType(UInt32 type, UInt32 *flags, IOMemoryDescriptor **memory)
{
    IOReturn	result = kIOReturnSuccess;
    void *	sharedMemory = 0;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::clientMemoryForType(0x%lx, 0x%lx, %p)\n", this, type, *flags, memory);
#endif

    switch(type) {
        case kStatusBuffer:
            assert(audioEngine);
            
            sharedMemory = (void *)audioEngine->getStatus();
            
            if (sharedMemory) {
                *memory = IOMemoryDescriptor::withAddress(sharedMemory, sizeof(IOAudioEngineStatus), kIODirectionNone);
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

IOExternalMethod *IOAudioEngineUserClient::getExternalMethodForIndex(UInt32 index)
{
    IOExternalMethod *method = 0;

    if (index < kIOAudioEngineNumCalls) {
        method = &methods[index];
    }

    return method;
}

IOExternalTrap *IOAudioEngineUserClient::getExternalTrapForIndex( UInt32 index )
{
    if (index != kIOAudioEngineTrapPerformClientIO) {
        return NULL;
    }
    
    return &trap;
}

IOReturn IOAudioEngineUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::registerNotificationPort(0x%lx, 0x%lx, 0x%lx)\n", this, (UInt32)port, type, refCon);
#endif

    switch (type) {
        case kIOAudioEngineAllNotifications:
            assert(commandGate);
            
            result = commandGate->runAction(registerNotificationAction, (void *)port, (void *)refCon);
            
            break;
        default:
            IOLog("IOAudioEngineUserClient[%p]::registerNotificationPort() - ERROR: invalid notification type specified - no notifications will be sent.\n", this);
            result = kIOReturnBadArgument;
            break;
    }
    // Create a single message, but keep a dict or something of all of the IOAudioStreams registered for
    // refCon is IOAudioStream *
    
    return result;
}

IOReturn IOAudioEngineUserClient::registerNotificationAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient::registerNotificationAction(%p, %p)\n", owner, arg1);
#endif

    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        
        if (userClient) {
            result = userClient->registerNotification((mach_port_t)arg1, (UInt32)arg2);
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::registerNotification(mach_port_t port, UInt32 refCon)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::registerFormatNotification(0x%lx, 0x%lx)\n", this, (UInt32)port, refCon);
#endif

    if (!isInactive()) {
        if (port == MACH_PORT_NULL) {	// We need to remove this notification
            if (notificationMessage != NULL) {
                notificationMessage->messageHeader.msgh_remote_port = MACH_PORT_NULL;
            }
        } else {
            if (notificationMessage == NULL) {
                notificationMessage = (IOAudioNotificationMessage *)IOMallocAligned(sizeof(IOAudioNotificationMessage), sizeof (IOAudioNotificationMessage *));
                
                if (notificationMessage) {
                    notificationMessage->messageHeader.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
                    notificationMessage->messageHeader.msgh_size = sizeof(IOAudioNotificationMessage);
                    notificationMessage->messageHeader.msgh_local_port = MACH_PORT_NULL;
                    notificationMessage->messageHeader.msgh_reserved = 0;
                    notificationMessage->messageHeader.msgh_id = 0;
                    notificationMessage->messageHeader.msgh_remote_port = port;
                    notificationMessage->ref = refCon;              
                } else {
                    result = kIOReturnNoMemory;
                }
            } else {
            	notificationMessage->messageHeader.msgh_remote_port = port;
                notificationMessage->ref = refCon;         
            }
        }
    } else {
        result = kIOReturnNoDevice;
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::registerBuffer(IOAudioStream *audioStream, void *sourceBuffer, UInt32 bufSizeInBytes, UInt32 bufferSetID)
{
    assert(commandGate);
    
    return commandGate->runAction(registerBufferAction, (void *)audioStream, sourceBuffer, (void *)bufSizeInBytes, (void *)bufferSetID);
}

IOReturn IOAudioEngineUserClient::unregisterBuffer(void *sourceBuffer, UInt32 bufferSetID)
{
    assert(commandGate);
    
    return commandGate->runAction(unregisterBufferAction, sourceBuffer, (void *)bufferSetID);
}
    
IOReturn IOAudioEngineUserClient::registerBufferAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        
        if (userClient) {
            result = userClient->registerClientBuffer((IOAudioStream *)arg1, arg2, (UInt32)arg3, (UInt32)arg4);
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::unregisterBufferAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        
        if (userClient) {
            result = userClient->unregisterClientBuffer(arg1, (UInt32)arg2);
        }
    }
    
    return result;
}
    
IOReturn IOAudioEngineUserClient::registerClientBuffer(IOAudioStream *audioStream, void *sourceBuffer, UInt32 bufSizeInBytes, UInt32 bufferSetID)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioClientBuffer *clientBuffer;
    IODirection bufferDirection;
    const IOAudioStreamFormat *streamFormat;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::registerClientBuffer(%p[%ld], %p, 0x%lx, 0x%lx)\n", this, audioStream, audioStream->getStartingChannelID(), sourceBuffer, bufSizeInBytes, bufferSetID);
#endif
    
    if (!isInactive()) {
        IOAudioClientBufferSet *clientBufferSet;
        IOAudioClientBuffer **clientBufferList;
        
        if (!sourceBuffer || !audioStream || (bufSizeInBytes == 0) ) {
            return kIOReturnBadArgument;
        }
        
        streamFormat = audioStream->getFormat();
        
        if (!streamFormat) {
            return kIOReturnError;
        }
        
        // Return an error if this is an unmixable stream and it already has a client
        if (!streamFormat->fIsMixable && (audioStream->getNumClients() != 0)) {
            return kIOReturnExclusiveAccess;
        }
        
        /* - removing this for now
        // make sure it isn't already registered
        if (getClientBuffer(sourceBuffer, audioStream) != NULL) {
            result = kIOReturnBadArgument;
            goto Exit;
        }
        */
        
        // allocate IOAudioClientBuffer to hold buffer descriptor, etc...
        clientBuffer = (IOAudioClientBuffer *)IOMallocAligned(sizeof(IOAudioClientBuffer), sizeof (IOAudioClientBuffer *));
        if (!clientBuffer) {
            result = kIOReturnNoMemory;
            goto Exit;
        }
		
		// make sure everthing is set to NULL [2851917]
		bzero(clientBuffer,sizeof(IOAudioClientBuffer));
       
        clientBuffer->userClient = this;
        
        bufferDirection = audioStream->getDirection() == kIOAudioStreamDirectionOutput ? kIODirectionIn : kIODirectionOut;
        
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
            IOLog("IOAudioEngineUserClient<0x%x>::registerClientBuffer() - error mapping memory.\n", (unsigned int)this);
            result = kIOReturnVMError;
            goto Exit;
        }
        
        clientBuffer->sourceBuffer = (void *)clientBuffer->sourceBufferMap->getVirtualAddress();
        if (clientBuffer->sourceBuffer == NULL) {
            result = kIOReturnVMError;
            goto Exit;
        }
        
        clientBuffer->numSampleFrames = bufSizeInBytes / (kIOAudioEngineDefaultMixBufferSampleSize * streamFormat->fNumChannels);
        clientBuffer->numChannels = streamFormat->fNumChannels;
                
        clientBuffer->unmappedSourceBuffer = sourceBuffer;
        clientBuffer->next = NULL;
        clientBuffer->nextClip = NULL;
        clientBuffer->previousClip = NULL;
        clientBuffer->nextClient = NULL;
        
        assert(clientBufferLock);
        
        IORecursiveLockLock(clientBufferLock);
        
        clientBufferSet = findBufferSet(bufferSetID);
        if (clientBufferSet == NULL) {
            clientBufferSet = new IOAudioClientBufferSet;

            if (clientBufferSet == NULL) {
                result = kIOReturnNoMemory;
                IORecursiveLockUnlock(clientBufferLock);
                goto Exit;
            }
            
            if (!clientBufferSet->init(bufferSetID, this)) {
                result = kIOReturnError;
                IORecursiveLockUnlock(clientBufferLock);
                goto Exit;
            }

            clientBufferSet->next = clientBufferSetList;

            clientBufferSetList = clientBufferSet;
        }
        
        if (audioStream->getDirection() == kIOAudioStreamDirectionOutput) {
            clientBufferList = &clientBufferSet->outputBufferList;
            if (clientBufferSet->watchdogThreadCall == NULL) {
                clientBufferSet->allocateWatchdogTimer();
                if (clientBufferSet->watchdogThreadCall == NULL) {
                    result = kIOReturnNoMemory;
                    IORecursiveLockUnlock(clientBufferLock);
                    goto Exit;
                }
            }
        } else {
            clientBufferList = &clientBufferSet->inputBufferList;
        }
        
        assert(clientBufferList);
        
        if (*clientBufferList == NULL) {
            *clientBufferList = clientBuffer;
        } else {
            IOAudioClientBuffer *clientBufPtr = *clientBufferList;
            while (clientBufPtr->next != NULL) {
                clientBufPtr = clientBufPtr->next;
            }
            clientBufPtr->next = clientBuffer;
        }
        
        IORecursiveLockUnlock(clientBufferLock);
        
    Exit:
        
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
                IOFreeAligned(clientBuffer, sizeof(IOAudioClientBuffer));
            }
        } else if (isOnline()) {
            result = audioStream->addClient(clientBuffer);
        }
    } else {
        result = kIOReturnNoDevice;
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::unregisterClientBuffer(void *sourceBuffer, UInt32 bufferSetID)
{
    IOReturn result = kIOReturnBadArgument;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::unregisterClientBuffer(%p, 0x%lx)\n", this, sourceBuffer, bufferSetID);
#endif

    if (sourceBuffer) {
        IOAudioClientBufferSet *bufferSet;

        assert(clientBufferLock);
        
        IORecursiveLockLock(clientBufferLock);
        
        bufferSet = findBufferSet(bufferSetID);
        
        if (bufferSet) {
            IOAudioClientBuffer *clientBuf = NULL, *previousBuf = NULL;
            IOAudioClientBuffer **clientBufferList = NULL;
            
            if (bufferSet->outputBufferList) {
                clientBufferList = &bufferSet->outputBufferList;
                clientBuf = bufferSet->outputBufferList;
                previousBuf = NULL;
                while (clientBuf && (clientBuf->unmappedSourceBuffer != sourceBuffer)) {
                    previousBuf = clientBuf;
                    clientBuf = clientBuf->next;
                }
            }
            
            // If we didn't find the buffer in the output list, check the input list
            if (!clientBuf && bufferSet->inputBufferList) {
                clientBufferList = &bufferSet->inputBufferList;
                clientBuf = bufferSet->inputBufferList;
                previousBuf = NULL;
                while (clientBuf && (clientBuf->unmappedSourceBuffer != sourceBuffer)) {
                    previousBuf = clientBuf;
                    clientBuf = clientBuf->next;
                }
            }

            if (clientBuf) {                
                assert(clientBuf->unmappedSourceBuffer == sourceBuffer);
                
                if (previousBuf) {
                    previousBuf->next = clientBuf->next;
                } else {
                    assert(clientBufferList);
                    *clientBufferList = clientBuf->next;
                }
                
                freeClientBuffer(clientBuf);
                
                if (bufferSet->outputBufferList == NULL) {
                    if (bufferSet->inputBufferList == NULL) {
                        removeBufferSet(bufferSet);
                    } else if (bufferSet->watchdogThreadCall != NULL) {
                        bufferSet->freeWatchdogTimer();
                    }
                }
                
                result = kIOReturnSuccess;
            } else {
                result = kIOReturnNotFound;
            }            
        } else {
            result = kIOReturnNotFound;
        }
        
        IORecursiveLockUnlock(clientBufferLock);
    }
    
    return result;
}

IOAudioClientBufferSet *IOAudioEngineUserClient::findBufferSet(UInt32 bufferSetID)
{
    IOAudioClientBufferSet *bufferSet;
    
    assert(clientBufferLock);
    
    IORecursiveLockLock(clientBufferLock);
    
    bufferSet = clientBufferSetList;
    while (bufferSet && (bufferSet->bufferSetID != bufferSetID)) {
        bufferSet = bufferSet->next;
    }
    
    IORecursiveLockUnlock(clientBufferLock);
    
    return bufferSet;
}

void IOAudioEngineUserClient::removeBufferSet(IOAudioClientBufferSet *bufferSet)
{
    IOAudioClientBufferSet *prevSet, *nextSet;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::removeBufferSet(%p)\n", this, bufferSet);
#endif
    
    assert(clientBufferLock);
    
    IORecursiveLockLock(clientBufferLock);
    
    nextSet = clientBufferSetList;
    prevSet = NULL;
    while (nextSet && (nextSet != bufferSet)) {
        prevSet = nextSet;
        nextSet = nextSet->next;
    }
    
    if (nextSet) {
        assert(nextSet == bufferSet);
        
        nextSet->cancelWatchdogTimer();
        
        if (prevSet) {
            prevSet->next = nextSet->next;
        } else {
            clientBufferSetList = nextSet->next;
        }
        
        nextSet->release();
    }
    
    IORecursiveLockUnlock(clientBufferLock);
}

IOReturn IOAudioEngineUserClient::performClientIO(UInt32 firstSampleFrame, UInt32 loopCount, bool inputIO, UInt32 bufferSetID, UInt32 sampleIntervalHi, UInt32 sampleIntervalLo)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_IO_CALLS
    IOLog("IOAudioEngineUserClient[%p]::performClientIO(0x%lx, 0x%lx, %d, 0x%lx)\n", this, firstSampleFrame, loopCount, inputIO, bufferSetID);
#endif
    
    assert(clientBufferLock);
    assert(audioEngine);
    
    if (!isInactive()) {
    
        IORecursiveLockLock(clientBufferLock);
        
        if (isOnline() && (audioEngine->getState() == kIOAudioEngineRunning)) {
            if (firstSampleFrame < audioEngine->numSampleFramesPerBuffer) {
                IOAudioClientBufferSet *bufferSet;
                
                bufferSet = findBufferSet(bufferSetID);
                if (bufferSet) {
                
                    if (inputIO) {
                        result = performClientInput(firstSampleFrame, bufferSet);
                    } else {
                        result = performClientOutput(firstSampleFrame, loopCount, bufferSet, sampleIntervalHi, sampleIntervalLo);
                    }
                }
            } else {
#ifdef DEBUG
    IOLog("IOAudioEngineUserClient[%p]::performClientIO(0x%lx, 0x%lx, %d, 0x%lx) - firstSampleFrame is out of range - 0x%lx frames per buffer.\n", this, firstSampleFrame, loopCount, inputIO, bufferSetID, audioEngine->numSampleFramesPerBuffer);
#endif
                result = kIOReturnBadArgument;
            }
        } else {
            result = kIOReturnOffline;
        }
        
        IORecursiveLockUnlock(clientBufferLock);
    } else {
        result = kIOReturnNoDevice;
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::performClientOutput(UInt32 firstSampleFrame, UInt32 loopCount, IOAudioClientBufferSet *bufferSet, UInt32 sampleIntervalHi, UInt32 sampleIntervalLo)
{
    IOReturn result = kIOReturnSuccess;
    
    bufferSet->sampleInterval.hi = sampleIntervalHi;
    bufferSet->sampleInterval.lo = sampleIntervalLo;
    
    if (bufferSet->outputBufferList != NULL) {
        IOAudioEnginePosition outputEndingPosition;
        UInt32 numSampleFrames, numSampleFramesPerBuffer;
        
        assert(audioEngine != NULL);
    
        // All buffers in this set must have the same number of samples
        numSampleFrames = bufferSet->outputBufferList->numSampleFrames;
        numSampleFramesPerBuffer = audioEngine->getNumSampleFramesPerBuffer();
        
        outputEndingPosition.fLoopCount = loopCount;
        outputEndingPosition.fSampleFrame = firstSampleFrame + numSampleFrames;
        
        if (outputEndingPosition.fSampleFrame >= numSampleFramesPerBuffer) {
            outputEndingPosition.fSampleFrame -= numSampleFramesPerBuffer;
            outputEndingPosition.fLoopCount++;
        }
        
        // We only want to do output if we haven't already gone past the new samples
        // If the samples are late, the watchdog will already have skipped them
        if (CMP_IOAUDIOENGINEPOSITION(&outputEndingPosition, &bufferSet->nextOutputPosition) >= 0) {
            IOAudioClientBuffer *clientBuf;
            AbsoluteTime outputTimeout;
            
            clientBuf = bufferSet->outputBufferList;
            
            while (clientBuf) {
                IOAudioStream *audioStream;
                IOReturn tmpResult;
                
                audioStream = clientBuf->audioStream;
        
                assert(audioStream);
                assert(audioStream->getDirection() == kIOAudioStreamDirectionOutput);
                assert(clientBuf->sourceBuffer != NULL);
                
                audioStream->lockStreamForIO();
                
                tmpResult = audioStream->processOutputSamples(clientBuf, firstSampleFrame, loopCount, true);
                
                audioStream->unlockStreamForIO();
                
                if (tmpResult != kIOReturnSuccess) {
                    result = tmpResult;
                }
                
                clientBuf = clientBuf->next;
            }
            
            bufferSet->nextOutputPosition = outputEndingPosition;
            
            audioEngine->calculateSampleTimeout(&bufferSet->sampleInterval, numSampleFrames, &bufferSet->nextOutputPosition, &outputTimeout);
            
            // We better have a thread call if we are doing output
            assert(bufferSet->watchdogThreadCall != NULL);

            bufferSet->setWatchdogTimeout(&outputTimeout);
        } else {
#ifdef DEBUG
    IOLog("IOAudioEngineUserClient[%p]::performClientOutput(%lx,%lx) - missed samples (%lx,%lx)\n", this, loopCount, firstSampleFrame, bufferSet->nextOutputPosition.fLoopCount, bufferSet->nextOutputPosition.fSampleFrame);
#endif
            result = kIOReturnIsoTooOld;
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::performClientInput(UInt32 firstSampleFrame, IOAudioClientBufferSet *bufferSet)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioClientBuffer *clientBuf;
    
    clientBuf = bufferSet->inputBufferList;
    
    while (clientBuf) {
        IOAudioStream *audioStream;
        IOReturn tmpResult;
        
        audioStream = clientBuf->audioStream;
        
        assert(audioStream);
        assert(audioStream->getDirection() == kIOAudioStreamDirectionInput);
        assert(clientBuf->sourceBuffer != NULL);

        audioStream->lockStreamForIO();
        
        tmpResult = audioStream->readInputSamples(clientBuf, firstSampleFrame);
        
        audioStream->unlockStreamForIO();
        
        if (tmpResult != kIOReturnSuccess) {
            result = tmpResult;
        }
        
        clientBuf = clientBuf->next;
    }
    
    return result;
}

void IOAudioEngineUserClient::performWatchdogOutput(IOAudioClientBufferSet *clientBufferSet, UInt32 generationCount)
{
    assert(clientBufferLock);

#ifdef DEBUG_CALLS
IOLog("IOAudioEngineUserClient[%p]::performWatchdogOutput(%p, %ld) - (%lx,%lx)\n", this, clientBufferSet, generationCount, clientBufferSet->nextOutputPosition.fLoopCount, clientBufferSet->nextOutputPosition.fSampleFrame);
#endif

    IORecursiveLockLock(clientBufferLock);
    
    if (!isInactive() && isOnline()) {
		// If the generation count of the clientBufferSet is different than the
		// generation count passed in, then a new client IO was received just before
		// the timer fired, and we don't need to do the fake IO
		// We just leave the timerPending field set
		if (clientBufferSet->generationCount == generationCount) {
			IOAudioClientBuffer *clientBuffer;
			
			clientBuffer = clientBufferSet->outputBufferList;
			
			while (clientBuffer) {
				IOAudioStream *audioStream;
				
				audioStream = clientBuffer->audioStream;
				
				assert(audioStream);
				assert(audioStream->getDirection() == kIOAudioStreamDirectionOutput);
				
				audioStream->lockStreamForIO();
				
				audioStream->processOutputSamples(clientBuffer, clientBufferSet->nextOutputPosition.fSampleFrame, clientBufferSet->nextOutputPosition.fLoopCount, false);
				
				audioStream->unlockStreamForIO();
				
				clientBuffer = clientBuffer->next;
			}

			if (clientBufferSet->outputBufferList != NULL) {
				UInt32 numSampleFrames, numSampleFramesPerBuffer;
				AbsoluteTime outputTimeout;
				
				numSampleFrames = clientBufferSet->outputBufferList->numSampleFrames;
				numSampleFramesPerBuffer = audioEngine->getNumSampleFramesPerBuffer();
				
				clientBufferSet->nextOutputPosition.fSampleFrame += numSampleFrames;
				
				if (clientBufferSet->nextOutputPosition.fSampleFrame >= numSampleFramesPerBuffer) {
					clientBufferSet->nextOutputPosition.fSampleFrame -= numSampleFramesPerBuffer;
					clientBufferSet->nextOutputPosition.fLoopCount++;
				}
				
				audioEngine->calculateSampleTimeout(&clientBufferSet->sampleInterval, numSampleFrames, &clientBufferSet->nextOutputPosition, &outputTimeout);
				
				clientBufferSet->setWatchdogTimeout(&outputTimeout);

			} else {
				clientBufferSet->timerPending = false;
			}
		}
    } else {
        clientBufferSet->timerPending = false;
    }
    
    IORecursiveLockUnlock(clientBufferLock);
}

IOReturn IOAudioEngineUserClient::getConnectionID(UInt32 *connectionID)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::getConnectionID(%p)\n", this, connectionID);
#endif

    *connectionID = (UInt32)this;
    return kIOReturnSuccess;
}

IOReturn IOAudioEngineUserClient::clientStart()
{
    assert(commandGate);
    
    return commandGate->runAction(startClientAction);
}

IOReturn IOAudioEngineUserClient::clientStop()
{
    assert(commandGate);
    
    return commandGate->runAction(stopClientAction);
}

IOReturn IOAudioEngineUserClient::startClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        if (userClient) {
            result = userClient->startClient();
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::stopClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        if (userClient) {
            result = userClient->stopClient();
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::startClient()
{
    IOReturn result = kIOReturnNoDevice;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::startClient() - %ld\n", this, audioEngine ? audioEngine->numActiveUserClients : 0);
#endif

    if (audioEngine && !isInactive()) {
        if (audioEngine->getState() != kIOAudioEnginePaused) {
            // We only need to start things up if we're not already online
            if (!isOnline()) {
                setOnline(true);
                result = audioEngine->startClient(this);
                
                if (result == kIOReturnSuccess) {
                    IOAudioClientBufferSet *bufferSet;
                    
                    assert(clientBufferLock);
                    
                    IORecursiveLockLock(clientBufferLock);
                    
                    // add buffers to streams
                    bufferSet = clientBufferSetList;
                    while (bufferSet) {
                        IOAudioClientBuffer *clientBuffer;
                        
                        clientBuffer = bufferSet->outputBufferList;
                        while (clientBuffer) {
                            if (clientBuffer->audioStream) {
                                result = clientBuffer->audioStream->addClient(clientBuffer);
                                if (result != kIOReturnSuccess) {
                                    break;
                                }
                            }
                            clientBuffer = clientBuffer->next;
                        }
            
                        clientBuffer = bufferSet->inputBufferList;
                        while (clientBuffer) {
                            if (clientBuffer->audioStream) {
                                clientBuffer->audioStream->addClient(clientBuffer);
                            }
                            clientBuffer = clientBuffer->next;
                        }
                        
                        bufferSet->resetNextOutputPosition();
            
                        bufferSet = bufferSet->next;
                    }
                    
                    IORecursiveLockUnlock(clientBufferLock);
                }
            } else {
                result = kIOReturnSuccess;
            }
        } else {
            result = kIOReturnOffline;
        }
    }

	if (kIOReturnSuccess != result) {
		setOnline(false);
	}

    return result;
}

IOReturn IOAudioEngineUserClient::stopClient()
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::stopClient() - %ld\n", this, audioEngine ? audioEngine->numActiveUserClients : 0);
#endif

    if (isOnline()) {
        IOAudioClientBufferSet *bufferSet;
        
        assert(clientBufferLock);
        
        IORecursiveLockLock(clientBufferLock);
        
        bufferSet = clientBufferSetList;
        while (bufferSet) {
            IOAudioClientBuffer *clientBuffer;
            
            bufferSet->cancelWatchdogTimer();
            
            clientBuffer = bufferSet->outputBufferList;
            while (clientBuffer) {
                if (clientBuffer->audioStream) {
                    clientBuffer->audioStream->removeClient(clientBuffer);
                }
                clientBuffer = clientBuffer->next;
            }

            clientBuffer = bufferSet->inputBufferList;
            while (clientBuffer) {
                if (clientBuffer->audioStream) {
                    clientBuffer->audioStream->removeClient(clientBuffer);
                }
                clientBuffer = clientBuffer->next;
            }
            
            bufferSet = bufferSet->next;
        }
        
        IORecursiveLockUnlock(clientBufferLock);

        if (audioEngine) {
            result = audioEngine->stopClient(this);
        }
    
        setOnline(false);
    }
    
    return result;
}

// Must be done on workLoop
void IOAudioEngineUserClient::sendFormatChangeNotification(IOAudioStream *audioStream)
{
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::sendFormatChangeNotification(%p)\n", this, audioStream);
#endif

    if (audioStream && notificationMessage && (notificationMessage->messageHeader.msgh_remote_port != MACH_PORT_NULL)) {
        io_object_t clientStreamRef;
        
        audioStream->retain();
        if (exportObjectToClient(clientTask, audioStream, &clientStreamRef) == kIOReturnSuccess) {
            kern_return_t kr;
            
            notificationMessage->type = kIOAudioEngineStreamFormatChangeNotification;
            notificationMessage->sender = clientStreamRef;
            
            kr = mach_msg_send_from_kernel(&notificationMessage->messageHeader, notificationMessage->messageHeader.msgh_size);
            if (kr != MACH_MSG_SUCCESS) {
                IOLog("IOAudioEngineUserClient::sendFormatChangeNotification() failed - msg_send returned: %d\n", kr);
                // Should also release the clientStreamRef here...
            }
        } else {
            IOLog("IOAudioEngineUserClient[%p]::sendFormatChangeNotification() - ERROR - unable to export stream object for notification - notification not sent\n", this);
        }
    } else {
        IOLog("IOAudioEngineUserClient[%p]::sendFormatChangeNotification() - ERROR - notification not sent - audioStream = %p - notificationMessage = %p - port = %ld\n", this, audioStream, notificationMessage, (UInt32)notificationMessage->messageHeader.msgh_remote_port);
    }
}

IOReturn IOAudioEngineUserClient::sendNotification(UInt32 notificationType)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioEngineUserClient[%p]::sendNotification(%ld)\n", this, notificationType);
#endif

    if (notificationType == kIOAudioEnginePausedNotification) {
        stopClient();
    }
        
    if (notificationMessage && (notificationMessage->messageHeader.msgh_remote_port != MACH_PORT_NULL)) {
        kern_return_t kr;
        
        notificationMessage->type = notificationType;
        notificationMessage->sender = NULL;
        
        kr = mach_msg_send_from_kernel(&notificationMessage->messageHeader, notificationMessage->messageHeader.msgh_size);
        if (kr != MACH_MSG_SUCCESS) {
            result = kIOReturnError;
        }
    }
    
    return result;
}
