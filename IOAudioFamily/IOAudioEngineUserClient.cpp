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

#include <IOKit/audio/IOAudioDebug.h>
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
    audioDebugIOLog(3, "IOAudioClientBufferSet[%p]::init(%lx, %p)", this, setID, client);

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
    audioDebugIOLog(3, "IOAudioClientBufferSet[%p]::free()", this);

    if (watchdogThreadCall) {
        freeWatchdogTimer();
    }
    
    if (userClient != NULL) {
        userClient->release();
        userClient = NULL;
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
    audioDebugIOLog(3, "IOAudioClientBufferSet[%p]::allocateWatchdogTimer()", this);

    if (watchdogThreadCall == NULL) {
        watchdogThreadCall = thread_call_allocate((thread_call_func_t)IOAudioClientBufferSet::watchdogTimerFired, (thread_call_param_t)this);
    }
}

void IOAudioClientBufferSet::freeWatchdogTimer()
{
    audioDebugIOLog(3, "IOAudioClientBufferSet[%p]::freeWatchdogTimer()", this);

    if (watchdogThreadCall != NULL) {
        cancelWatchdogTimer();
        thread_call_free(watchdogThreadCall);
        watchdogThreadCall = NULL;
    }
}

void IOAudioClientBufferSet::setWatchdogTimeout(AbsoluteTime *timeout)
{
	bool				result;

    if (watchdogThreadCall == NULL) {
        // allocate it here
        IOLog("IOAudioClientBufferSet[%p]::setWatchdogTimeout() - no thread call.\n", this);
    }
    
    assert(watchdogThreadCall);
    
    outputTimeout = *timeout;
    
    generationCount++;
    
	userClient->lockBuffers();

//	if (!timerPending) {
//		kprintf ("retain %p\n", this);
		retain();
//	}
    
    timerPending = true;

    result = thread_call_enter1_delayed(watchdogThreadCall, (thread_call_param_t)generationCount, outputTimeout);
	if (result) {
//		kprintf ("release0 %p\n", this);
		release();		// canceled the previous call
	}

	userClient->unlockBuffers();
}

void IOAudioClientBufferSet::cancelWatchdogTimer()
{
    audioDebugIOLog(3, "IOAudioClientBufferSet[%p]::cancelWatchdogTimer()", this);

	if (NULL != userClient) {
		userClient->retain();
		userClient->lockBuffers();
		if (timerPending) {
			timerPending = false;
			if (thread_call_cancel(watchdogThreadCall))
				release();
		}
		userClient->unlockBuffers();
		userClient->release();
	}
}

void IOAudioClientBufferSet::watchdogTimerFired(IOAudioClientBufferSet *clientBufferSet, UInt32 generationCount)
{
    IOAudioEngineUserClient *userClient;

	assert(clientBufferSet);
    assert(clientBufferSet->userClient);

	if (clientBufferSet) {
#ifdef DEBUG
		AbsoluteTime now;
		clock_get_uptime(&now);
		audioDebugIOLog(3, "IOAudioClientBufferSet[%p]::watchdogTimerFired(%ld):(%lx,%lx)(%lx,%lx)(%lx,%lx)", clientBufferSet, generationCount, now.hi, now.lo, clientBufferSet->outputTimeout.hi, clientBufferSet->outputTimeout.lo, clientBufferSet->nextOutputPosition.fLoopCount, clientBufferSet->nextOutputPosition.fSampleFrame);
#endif

		userClient = clientBufferSet->userClient;
		if (userClient) {
			userClient->retain();
			userClient->lockBuffers();
	
			if(clientBufferSet->timerPending != false) {
				userClient->performWatchdogOutput(clientBufferSet, generationCount);
			}
	
			// If there's no timer pending once we attempt to do the watchdog I/O
			// then we need to release the set
//			if (!clientBufferSet->timerPending) {
//				kprintf ("release2 %p\n", clientBufferSet);
				clientBufferSet->release();
//			}

			userClient->unlockBuffers();
			userClient->release();
		}

		// clientBufferSet->release code was down here...
	} else {
		IOLog ("IOAudioClientBufferSet::watchdogTimerFired assert (clientBufferSet == NULL) failed\n");
	}
}

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOAudioEngineUserClient, IOUserClient)

OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 0);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 1);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 2);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 3);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 4);

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

// New code added here
// Used so that a pointer to a kernel IOAudioStream isn't passed out of the kernel
IOReturn IOAudioEngineUserClient::safeRegisterClientBuffer(UInt32 audioStreamIndex, void *sourceBuffer, UInt32 bufSizeInBytes, UInt32 bufferSetID) {
	IOAudioStream *					audioStream;

	audioStream = audioEngine->getStreamForID(audioStreamIndex);
	if (!audioStream) {
		return kIOReturnBadArgument;
	}

	return registerClientBuffer(audioStream, sourceBuffer, bufSizeInBytes, bufferSetID);
}

// Used to pass extra information about how many samples are actually in a buffer and other things related to interesting non-mixable audio formats.
IOReturn IOAudioEngineUserClient::registerClientParameterBuffer (void *paramBuffer, UInt32 bufferSetID)
{
	IOReturn						result = kIOReturnSuccess;
	IOAudioClientBufferSet			*bufferSet = NULL;
	IOAudioClientBufferExtendedInfo *extendedInfo;

    if (!isInactive()) {
        if (!paramBuffer || ((IOAudioStreamDataDescriptor *)paramBuffer)->fVersion > kStreamDataDescriptorCurrentVersion) {
            return kIOReturnBadArgument;
        }
        
        lockBuffers();		// added here because it was turned off in findBufferSet // MPC

		// this buffer set can't have already been registered with extended info
        extendedInfo = findExtendedInfo (bufferSetID);
		if (extendedInfo) {
            return kIOReturnBadArgument;
		}

		// make sure that this buffer set has already been registered for output
        bufferSet = findBufferSet(bufferSetID);

		unlockBuffers();
		
        if (bufferSet) {
			IOAudioClientBufferExtendedInfo *info;
			
			extendedInfo = (IOAudioClientBufferExtendedInfo*)IOMalloc (sizeof (IOAudioClientBufferExtendedInfo));
			if (!extendedInfo) {
				return kIOReturnError;
			}

			// Can only be for output buffers, so always kIODirectionIn
			extendedInfo->paramBufferDescriptor = IOMemoryDescriptor::withAddress((vm_address_t)paramBuffer, (((IOAudioStreamDataDescriptor *)paramBuffer)->fNumberOfStreams * 4) + 8, kIODirectionIn, clientTask);
			if (!extendedInfo->paramBufferDescriptor) {
				result = kIOReturnInternalError;
				goto Exit;
			}
			
			if ((result = extendedInfo->paramBufferDescriptor->prepare()) != kIOReturnSuccess) {
				goto Exit;
			}
			
			extendedInfo->paramBufferMap = extendedInfo->paramBufferDescriptor->map();
			
			if (extendedInfo->paramBufferMap == NULL) {
				IOLog("IOAudioEngineUserClient<0x%x>::registerClientParameterBuffer() - error mapping memory.\n", (unsigned int)this);
				result = kIOReturnVMError;
				goto Exit;
			}
			
			extendedInfo->paramBuffer = (void *)extendedInfo->paramBufferMap->getVirtualAddress();
			if (extendedInfo->paramBuffer == NULL) {
				result = kIOReturnVMError;
				goto Exit;
			}
	
			extendedInfo->unmappedParamBuffer = paramBuffer;
			extendedInfo->next = NULL;
			
			if (reserved->extendedInfo) {
				// Get to the end of the linked list of extended info and add this new entry there
				info = reserved->extendedInfo;
				while (info) {
					info = info->next;
				}

				info->next = extendedInfo;
			} else {
				// The list is empty, so this the start of the list
				reserved->extendedInfo = extendedInfo;
			}
		}
     } else {
        result = kIOReturnNoDevice;
    }

Exit:
	return result;
}

IOAudioClientBufferExtendedInfo *IOAudioEngineUserClient::findExtendedInfo(UInt32 bufferSetID)
{
    IOAudioClientBufferExtendedInfo *extendedInfo;
    
//    lockBuffers();
    
    extendedInfo = reserved->extendedInfo;
    while (extendedInfo && (extendedInfo->bufferSetID != bufferSetID)) {
        extendedInfo = extendedInfo->next;
    }
    
//    unlockBuffers();
    
    return extendedInfo;
}

IOReturn IOAudioEngineUserClient::getNearestStartTime(IOAudioStream *audioStream, IOAudioTimeStamp *ioTimeStamp, UInt32 isInput)
{
    assert(commandGate);
    
    return commandGate->runAction(getNearestStartTimeAction, (void *)audioStream, (void *)ioTimeStamp, (void *)isInput);
}

IOReturn IOAudioEngineUserClient::getNearestStartTimeAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        
        if (userClient) {
            result = userClient->getClientNearestStartTime((IOAudioStream *)arg1, (IOAudioTimeStamp *)arg2, (UInt32)arg3);
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::getClientNearestStartTime(IOAudioStream *audioStream, IOAudioTimeStamp *ioTimeStamp, UInt32 isInput)
{
    IOReturn result = kIOReturnSuccess;

    if (audioEngine && !isInactive()) {
		result = audioEngine->getNearestStartTime(audioStream, ioTimeStamp, isInput);
	}

	return result;
}

// Original code
IOAudioEngineUserClient *IOAudioEngineUserClient::withAudioEngine(IOAudioEngine *engine, task_t clientTask, void *securityToken, UInt32 type)
{
    IOAudioEngineUserClient *client;
    
    audioDebugIOLog(3, "IOAudioEngineUserClient::withAudioEngine(%p, 0x%lx, %p, 0x%lx)", engine, (UInt32)clientTask, securityToken, type);

    client = new IOAudioEngineUserClient;

    if (client) {
        if (!client->initWithAudioEngine(engine, clientTask, securityToken, type)) {
            client->release();
            client = NULL;
        }
    }

    return client;
}

bool IOAudioEngineUserClient::initWithAudioEngine(IOAudioEngine *engine, task_t task, void *securityToken, UInt32 type)
{
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::initWithAudioEngine(%p, 0x%lx, %p, 0x%lx)", this, engine, (UInt32)task, securityToken, type);
    
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
    
 	reserved = (ExpansionData *)IOMalloc (sizeof(struct ExpansionData));
	if (!reserved) {
		return false;
	}

	reserved->extendedInfo = NULL;
 	reserved->classicMode = 0;
//	reserved->securityToken = securityToken;

	workLoop->addEventSource(commandGate);
    
    reserved->methods[kIOAudioEngineCallRegisterClientBuffer].object = this;
    reserved->methods[kIOAudioEngineCallRegisterClientBuffer].func = (IOMethod) &IOAudioEngineUserClient::registerBuffer;
    reserved->methods[kIOAudioEngineCallRegisterClientBuffer].count0 = 4;
    reserved->methods[kIOAudioEngineCallRegisterClientBuffer].count1 = 0;
    reserved->methods[kIOAudioEngineCallRegisterClientBuffer].flags = kIOUCScalarIScalarO;
    
    reserved->methods[kIOAudioEngineCallUnregisterClientBuffer].object = this;
    reserved->methods[kIOAudioEngineCallUnregisterClientBuffer].func = (IOMethod) &IOAudioEngineUserClient::unregisterBuffer;
    reserved->methods[kIOAudioEngineCallUnregisterClientBuffer].count0 = 2;
    reserved->methods[kIOAudioEngineCallUnregisterClientBuffer].count1 = 0;
    reserved->methods[kIOAudioEngineCallUnregisterClientBuffer].flags = kIOUCScalarIScalarO;
    
    reserved->methods[kIOAudioEngineCallGetConnectionID].object = this;
    reserved->methods[kIOAudioEngineCallGetConnectionID].func = (IOMethod) &IOAudioEngineUserClient::getConnectionID;
    reserved->methods[kIOAudioEngineCallGetConnectionID].count0 = 0;
    reserved->methods[kIOAudioEngineCallGetConnectionID].count1 = 1;
    reserved->methods[kIOAudioEngineCallGetConnectionID].flags = kIOUCScalarIScalarO;
    
    reserved->methods[kIOAudioEngineCallStart].object = this;
    reserved->methods[kIOAudioEngineCallStart].func = (IOMethod) &IOAudioEngineUserClient::clientStart;
    reserved->methods[kIOAudioEngineCallStart].count0 = 0;
    reserved->methods[kIOAudioEngineCallStart].count1 = 0;
    reserved->methods[kIOAudioEngineCallStart].flags = kIOUCScalarIScalarO;
    
    reserved->methods[kIOAudioEngineCallStop].object = this;
    reserved->methods[kIOAudioEngineCallStop].func = (IOMethod) &IOAudioEngineUserClient::clientStop;
    reserved->methods[kIOAudioEngineCallStop].count0 = 0;
    reserved->methods[kIOAudioEngineCallStop].count1 = 0;
    reserved->methods[kIOAudioEngineCallStop].flags = kIOUCScalarIScalarO;

    reserved->methods[kIOAudioEngineCallGetNearestStartTime].object = this;
    reserved->methods[kIOAudioEngineCallGetNearestStartTime].func = (IOMethod) &IOAudioEngineUserClient::getNearestStartTime;
    reserved->methods[kIOAudioEngineCallGetNearestStartTime].count0 = 3;
    reserved->methods[kIOAudioEngineCallGetNearestStartTime].count1 = 0;
    reserved->methods[kIOAudioEngineCallGetNearestStartTime].flags = kIOUCScalarIScalarO;

    trap.object = this;
    trap.func = (IOTrap) &IOAudioEngineUserClient::performClientIO;

    return true;
}

void IOAudioEngineUserClient::free()
{
	IOAudioClientBufferExtendedInfo *			cur;
	IOAudioClientBufferExtendedInfo *			prev;

    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::free()", this);

    freeClientBufferSetList();			// Moved above clientBufferLock code below
    
    if (clientBufferLock) {
        IORecursiveLockFree(clientBufferLock);
        clientBufferLock = NULL;
    }
    
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
    
	if (reserved) {
		if (NULL != reserved->extendedInfo) {
			cur = reserved->extendedInfo;
			while (cur) {
				prev = cur;
				if (NULL != prev) {
					IOFree (prev, sizeof (IOAudioClientBufferExtendedInfo));
				}
				cur = cur->next;
			}
		}
		IOFree (reserved, sizeof(struct ExpansionData));
	}

    super::free();
}

void IOAudioEngineUserClient::freeClientBufferSetList()
{
    while (clientBufferSetList) {
        IOAudioClientBufferSet *nextSet;
        
		// Move call up here to fix 3472373
        clientBufferSetList->cancelWatchdogTimer();

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
			clientBuffer->audioStream = NULL;
        }
        
        if (clientBuffer->sourceBufferDescriptor != NULL) {
            clientBuffer->sourceBufferDescriptor->complete();
            clientBuffer->sourceBufferDescriptor->release();
			clientBuffer->sourceBufferDescriptor = NULL;
        }
        
        if (clientBuffer->sourceBufferMap != NULL) {
            clientBuffer->sourceBufferMap->release();
			clientBuffer->sourceBufferMap = NULL;
        }

        IOFreeAligned(clientBuffer, sizeof(IOAudioClientBuffer));
		clientBuffer = NULL;
    }
}

void IOAudioEngineUserClient::stop(IOService *provider)
{
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::stop(%p)", this, provider);

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
    
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::clientClose()", this);

    if (audioEngine && !isInactive()) {
        assert(commandGate);
            
        result = commandGate->runAction(closeClientAction);
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::clientDied()
{
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::clientDied()", this);

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
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::closeClient()", this);

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
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::setOnline(%d)", this, newOnline);

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
    IOReturn						result = kIOReturnSuccess;
	IOBufferMemoryDescriptor		*theMemoryDescriptor = NULL;

    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::clientMemoryForType(0x%lx, 0x%lx, %p)", this, type, *flags, memory);

	assert(audioEngine);

    switch(type) {
        case kIOAudioStatusBuffer:
			theMemoryDescriptor = audioEngine->getStatusDescriptor();
            break;
		case kIOAudioBytesInInputBuffer:
			theMemoryDescriptor = audioEngine->getBytesInInputBufferArrayDescriptor();
			break;
		case kIOAudioBytesInOutputBuffer:
			theMemoryDescriptor = audioEngine->getBytesInOutputBufferArrayDescriptor();
			break;
        default:
            result = kIOReturnUnsupported;
            break;
    }

	if (!result && theMemoryDescriptor) {
		theMemoryDescriptor->retain();		// Don't release it, it will be released by mach-port automatically
		*memory = theMemoryDescriptor;
		*flags = kIOMapReadOnly;
	} else {
		result = kIOReturnError;
	}

    return result;
}

IOExternalMethod *IOAudioEngineUserClient::getExternalMethodForIndex(UInt32 index)
{
    IOExternalMethod *method = 0;

    if (index < kIOAudioEngineNumCalls) {
        method = &reserved->methods[index];
    }

    return method;
}

IOExternalTrap *IOAudioEngineUserClient::getExternalTrapForIndex( UInt32 index )
{
	IOExternalTrap *result = NULL;
	
    if (index == kIOAudioEngineTrapPerformClientIO) {
		result = &trap;
	} else if (index == (0x1000 | kIOAudioEngineTrapPerformClientIO)) {
		reserved->classicMode = 1;
		result = &trap;
    }

    return result;
}

IOReturn IOAudioEngineUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon)
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::registerNotificationPort(0x%lx, 0x%lx, 0x%lx)", this, (UInt32)port, type, refCon);

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
    
    audioDebugIOLog(3, "IOAudioEngineUserClient::registerNotificationAction(%p, %p)", owner, arg1);

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
    
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::registerFormatNotification(0x%lx, 0x%lx)", this, (UInt32)port, refCon);

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
//            result = userClient->registerClientBuffer((IOAudioStream *)arg1, arg2, (UInt32)arg3, (UInt32)arg4);
			result = userClient->safeRegisterClientBuffer((UInt32)arg1, arg2, (UInt32)arg3, (UInt32)arg4);
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
    
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::registerClientBuffer(%p[%ld], %p, 0x%lx, 0x%lx)", this, audioStream, audioStream->getStartingChannelID(), sourceBuffer, bufSizeInBytes, bufferSetID);
/*    
	// For 3019260
	result = clientHasPrivilege(reserved->securityToken, kIOClientPrivilegeLocalUser);
	if (result) {
		// You don't have enough privileges to play or record audio
		return result;
	}
*/
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

		numSampleFrames = bufSizeInBytes;
		if (streamFormat->fIsMixable) {
			// If it's mixable the data is floats, so that's the size of each sample
			clientBuffer->numSampleFrames = bufSizeInBytes / (kIOAudioEngineDefaultMixBufferSampleSize * streamFormat->fNumChannels);
		} else {
			// If it's not mixable then the size is whatever the bitwidth is
			clientBuffer->numSampleFrames = bufSizeInBytes / ((streamFormat->fBitWidth / 8) * streamFormat->fNumChannels);
		}
        clientBuffer->numChannels = streamFormat->fNumChannels;
                
        clientBuffer->unmappedSourceBuffer = sourceBuffer;
        clientBuffer->next = NULL;
        clientBuffer->nextClip = NULL;
        clientBuffer->previousClip = NULL;
        clientBuffer->nextClient = NULL;
        
        lockBuffers();
        
        clientBufferSet = findBufferSet(bufferSetID);
        if (clientBufferSet == NULL) {
            clientBufferSet = new IOAudioClientBufferSet;

            if (clientBufferSet == NULL) {
                result = kIOReturnNoMemory;
                unlockBuffers();
                goto Exit;
            }
            
            if (!clientBufferSet->init(bufferSetID, this)) {
                result = kIOReturnError;
                unlockBuffers();
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
                    unlockBuffers();
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
        
        unlockBuffers();
        
    Exit:
        
        if (result != kIOReturnSuccess) {
            if (clientBuffer != NULL) {
                if (clientBuffer->sourceBufferDescriptor != NULL) {
                    clientBuffer->sourceBufferDescriptor->release();
					clientBuffer->sourceBufferDescriptor = NULL;
                }
                if (clientBuffer->sourceBufferMap != NULL) {
                    clientBuffer->sourceBufferMap->release();
					clientBuffer->sourceBufferMap = NULL;
                }
                if (clientBuffer->audioStream) {
                    clientBuffer->audioStream->release();
					clientBuffer->audioStream = NULL;
                }
                IOFreeAligned(clientBuffer, sizeof(IOAudioClientBuffer));
				clientBuffer = NULL;
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
    
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::unregisterClientBuffer(%p, 0x%lx)", this, sourceBuffer, bufferSetID);

    if (sourceBuffer) {
        IOAudioClientBufferSet *bufferSet;
        
        lockBuffers();
        
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
                
                if (bufferSet->outputBufferList == NULL) {
                    if (bufferSet->inputBufferList == NULL) {
                        removeBufferSet(bufferSet);
                    } else if (bufferSet->watchdogThreadCall != NULL) {
                        bufferSet->freeWatchdogTimer();
                    }
                }

                freeClientBuffer(clientBuf);		// Moved below above if statement
                
                result = kIOReturnSuccess;
            } else {
                result = kIOReturnNotFound;
            }            
        } else {
            result = kIOReturnNotFound;
        }
        
        unlockBuffers();
    }
    
    return result;
}

IOAudioClientBufferSet *IOAudioEngineUserClient::findBufferSet(UInt32 bufferSetID)
{
    IOAudioClientBufferSet *bufferSet;
    
//    lockBuffers();
    
    bufferSet = clientBufferSetList;
    while (bufferSet && (bufferSet->bufferSetID != bufferSetID)) {
        bufferSet = bufferSet->next;
    }
    
//    unlockBuffers();
    
    return bufferSet;
}

void IOAudioEngineUserClient::removeBufferSet(IOAudioClientBufferSet *bufferSet)
{
    IOAudioClientBufferSet *prevSet, *nextSet;
    
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::removeBufferSet(%p)", this, bufferSet);
    
    lockBuffers();
    
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
    
    unlockBuffers();
}

IOReturn IOAudioEngineUserClient::performClientIO(UInt32 firstSampleFrame, UInt32 loopCount, bool inputIO, UInt32 bufferSetID, UInt32 sampleIntervalHi, UInt32 sampleIntervalLo)
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(7, "IOAudioEngineUserClient[%p]::performClientIO(0x%lx, 0x%lx, %d, 0x%lx)", this, firstSampleFrame, loopCount, inputIO, bufferSetID);
	
    assert(audioEngine);
    
    if (!isInactive()) {
    
        lockBuffers();
        
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
				audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::performClientIO(0x%lx, 0x%lx, %d, 0x%lx) - firstSampleFrame is out of range - 0x%lx frames per buffer.", this, firstSampleFrame, loopCount, inputIO, bufferSetID, audioEngine->numSampleFramesPerBuffer);
                result = kIOReturnBadArgument;
            }
        } else {
            result = kIOReturnOffline;
        }
        
        unlockBuffers();
    } else {
        result = kIOReturnNoDevice;
    }
    
    return result;
}

// model a SwapFloat32 after CF
inline uint32_t CFSwapInt32(uint32_t arg) {
#if defined(__i386__) && defined(__GNUC__)
    __asm__("bswap %0" : "+r" (arg));
    return arg;
#elif defined(__ppc__) && defined(__GNUC__)
    uint32_t result;
    __asm__("lwbrx %0,0,%1" : "=r" (result) : "r" (&arg), "m" (arg));
    return result;
#else
    uint32_t result;
    result = ((arg & 0xFF) << 24) | ((arg & 0xFF00) << 8) | ((arg >> 8) & 0xFF00) | ((arg >> 24) & 0xFF);
    return result;
#endif
}


void FlipFloats(void *p, long fcnt)
{
	UInt32 *ip = (UInt32 *)p;
	
	while (fcnt--) {
		*ip = CFSwapInt32(*ip);
		ip++;
	}
}

IOReturn IOAudioEngineUserClient::performClientOutput(UInt32 firstSampleFrame, UInt32 loopCount, IOAudioClientBufferSet *bufferSet, UInt32 sampleIntervalHi, UInt32 sampleIntervalLo)
{
    IOReturn result = kIOReturnSuccess;
	IOAudioClientBufferExtendedInfo *extendedInfo;
	IOAudioStreamDataDescriptor		*dataDescriptor;

//audioDebugIOLog(3, "%ld %ld", firstSampleFrame, loopCount);
    bufferSet->sampleInterval.hi = sampleIntervalHi;
    bufferSet->sampleInterval.lo = sampleIntervalLo;
    
    if (bufferSet->outputBufferList != NULL) {
        IOAudioEnginePosition outputEndingPosition;
        UInt32 numSampleFrames, numSampleFramesPerBuffer;
        
        assert(audioEngine != NULL);

		// check to see if there is extended info registered for this buffer set
		extendedInfo = findExtendedInfo(bufferSet->bufferSetID);
		if (extendedInfo) {
			dataDescriptor = (IOAudioStreamDataDescriptor *)extendedInfo->paramBuffer;
			if (dataDescriptor) {
				// fNumChannels should probably be 1 and fBitWidth should be 8 (1 byte) for when you want to pass raw data through IOAudioFamily and the driver.
				numSampleFrames = dataDescriptor->fStreamLength[0] / (bufferSet->outputBufferList->audioStream->format.fNumChannels * (bufferSet->outputBufferList->audioStream->format.fBitWidth / 8));
			} else {
				return kIOReturnError;
			}
		} else {
			// All buffers in this set must have the same number of samples if there isn't an extended info struct for this buffer set
			numSampleFrames = bufferSet->outputBufferList->numSampleFrames;
		}
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
                
#if __i386__
                if (reserved->classicMode && clientBuf->sourceBuffer != NULL) {
					const IOAudioStreamFormat *fmt = audioStream->getFormat();
					if (fmt->fIsMixable && fmt->fSampleFormat == kIOAudioStreamSampleFormatLinearPCM)
						FlipFloats(clientBuf->sourceBuffer, clientBuf->numSampleFrames * clientBuf->numChannels);
				}
#endif
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
			audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::performClientOutput(%lx,%lx) - missed samples (%lx,%lx)", this, loopCount, firstSampleFrame, bufferSet->nextOutputPosition.fLoopCount, bufferSet->nextOutputPosition.fSampleFrame);
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
        
#if __i386__
		if (reserved->classicMode && clientBuf->sourceBuffer != NULL) {
			const IOAudioStreamFormat *fmt = audioStream->getFormat();
			if (fmt->fIsMixable && fmt->fSampleFormat == kIOAudioStreamSampleFormatLinearPCM)
				FlipFloats(clientBuf->sourceBuffer, clientBuf->numSampleFrames * clientBuf->numChannels);
		}
#endif        
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
	audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::performWatchdogOutput(%p, %ld) - (%lx,%lx)", this, clientBufferSet, generationCount, clientBufferSet->nextOutputPosition.fLoopCount, clientBufferSet->nextOutputPosition.fSampleFrame);

    lockBuffers();
    
    if (!isInactive() && isOnline()) {
        if (clientBufferSet->timerPending) {
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
        }
    } else {
        clientBufferSet->timerPending = false;
    }
    
    unlockBuffers();
}

IOReturn IOAudioEngineUserClient::getConnectionID(UInt32 *connectionID)
{
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::getConnectionID(%p)", this, connectionID);

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
    
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::startClient() - %ld", this, audioEngine ? audioEngine->numActiveUserClients : 0);
	
	retain();

    if (audioEngine && !isInactive()) {
        if (audioEngine->getState() != kIOAudioEnginePaused) {
            // We only need to start things up if we're not already online
            if (!isOnline()) {
                setOnline(true);
                result = audioEngine->startClient(this);
                
                if (result == kIOReturnSuccess) {
                    IOAudioClientBufferSet *bufferSet;
                    
                    lockBuffers();
                    
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
                    
                    unlockBuffers();
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

	release();

	return result;
}

IOReturn IOAudioEngineUserClient::stopClient()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::stopClient() - %ld", this, audioEngine ? audioEngine->numActiveUserClients : 0);

    if (isOnline()) {
        IOAudioClientBufferSet *bufferSet;
        
        lockBuffers();
        
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
        
        unlockBuffers();

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
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::sendFormatChangeNotification(%p)", this, audioStream);

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
		if (notificationMessage) {
			IOLog("IOAudioEngineUserClient[%p]::sendFormatChangeNotification() - ERROR - notification not sent - audioStream = %p - notificationMessage = %p - port = %ld\n", this, audioStream, notificationMessage, (UInt32)notificationMessage->messageHeader.msgh_remote_port);
		} else {
			IOLog("IOAudioEngineUserClient[%p]::sendFormatChangeNotification() - ERROR - notification not sent - audioStream = %p - notificationMessage = %p\n", this, audioStream, notificationMessage);
		}
    }
}

IOReturn IOAudioEngineUserClient::sendNotification(UInt32 notificationType)
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::sendNotification(%ld)", this, notificationType);

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
