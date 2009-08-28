/*
 * Copyright (c) 2001-2007 Apple Inc. All rights reserved.
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

#include "AppleRAID.h"

#define super IOCommand
OSDefineMetaClassAndStructors(AppleRAIDStorageRequest, IOCommand);

void AppleRAIDStorageRequest::free(void)
{
    UInt32	index;

    if (srRequestStatus != 0) IODelete(srRequestStatus, IOReturn, srRequestsAllocated);
    if (srRequestByteCounts != 0) IODelete(srRequestByteCounts, UInt64, srRequestsAllocated);
    if (srActiveMembers != 0) IODelete(srActiveMembers, AppleRAIDMember *, srMemberCount);
    
    if (srMemoryDescriptors != 0) {

        for (index = 0; index < srRequestsAllocated; index++) {
            if (srMemoryDescriptors[index]) {
		srMemoryDescriptors[index]->release();
	    }
	}
        
	IODelete(srMemoryDescriptors, AppleRAIDMemoryDescriptor *, srRequestsAllocated);

	srMemoryDescriptors = 0;
    }

    super::free();
}

AppleRAIDStorageRequest *AppleRAIDStorageRequest::withAppleRAIDSet(AppleRAIDSet *set)
{
    AppleRAIDStorageRequest *storageRequest = new AppleRAIDStorageRequest;
    
    if (storageRequest != 0) {
        if (!storageRequest->initWithAppleRAIDSet(set)) {
            storageRequest->release();
            storageRequest = 0;
        }
    }
    
    return storageRequest;
}


bool AppleRAIDStorageRequest::initWithAppleRAIDSet(AppleRAIDSet *set)
{
    UInt32	index;
    
    if (!super::init()) return false;

    // anything that can change, must go thru this
    srRAIDSet = set;

    // these can not change
    srEventSource	= set->arSetEventSource;
    srSetBlockSize   	= set->arMaxReadRequestFactor ? (set->arSetBlockSize * set->arMaxReadRequestFactor) : set->arSetBlockSize;
    srMemberCount	= set->getMemberCount();
    srRequestCount	= set->getMaxRequestCount();
    srRequestsAllocated = set->getMaxRequestCount();
    srMemberBaseOffset	= set->getBase();
    
    srRequestStatus = IONew(IOReturn, srRequestsAllocated);
    if (srRequestStatus == 0) return false;
    
    srRequestByteCounts = IONew(UInt64, srRequestsAllocated);
    if (srRequestByteCounts == 0) return false;
    
    srActiveMembers = IONew(AppleRAIDMember *, srMemberCount);
    if (srActiveMembers == 0) return false;

    srMemoryDescriptors = IONew(AppleRAIDMemoryDescriptor *, srRequestsAllocated);
    if (srMemoryDescriptors == 0) return false;

    for (index = 0; index < srRequestsAllocated; index++) {
	srMemoryDescriptors[index] = set->allocateMemoryDescriptor(this, index);
	if (!srMemoryDescriptors[index]) return false;
    }
    
    return true;
}


void AppleRAIDStorageRequest::extractRequest(IOService **client, UInt64 *byteStart,
					     IOMemoryDescriptor **buffer, IOStorageCompletion *completion)
{
    // copy out what we need to restart this i/o

    *client			= srClient;
    *byteStart			= srByteStart;
    *buffer	 		= srMemoryDescriptor;
    *completion 		= srClientsCompletion;
}

void AppleRAIDStorageRequest::read(IOService *client, UInt64 byteStart, IOMemoryDescriptor *buffer,
				   IOStorageAttributes * attributes, IOStorageCompletion * completion)
{
    UInt32			index, virtIndex;
    AppleRAIDMember		*member;
    bool			isOnline;
    AppleRAIDMemoryDescriptor	*memoryDescriptor;
    IOStorageCompletion		internalCompletion;
    
    srClient			= client;
    srClientsCompletion 	= *completion;
    srCompletedCount		= 0;
    srMemoryDescriptor 		= buffer;
    srMemoryDescriptorDirection	= buffer->getDirection();
    srByteCount 		= buffer->getLength();
    srByteStart			= byteStart;

    srRequestCount		= srRAIDSet->getMaxRequestCount();
    srActiveCount		= srRAIDSet->getActiveCount();
    srRAIDSet->activeReadMembers(srActiveMembers, srByteStart, srByteCount);

    // XXX this is hideously inefficent, it adds a context switch per i/o request
    // XXX replace event source code with a direct runAction call?
    internalCompletion.target    = srEventSource;
    internalCompletion.action    = srEventSource->getStorageCompletionAction();

    for (virtIndex = 0; virtIndex < srMemberCount; virtIndex++) {

	member = srActiveMembers[virtIndex];
	isOnline = (uintptr_t)(member) >= 0x1000;
	index = isOnline ? member->getMemberIndex() : (uintptr_t)member;
	memoryDescriptor = srMemoryDescriptors[index];

	if (isOnline && memoryDescriptor->configureForMemoryDescriptor(buffer, byteStart, virtIndex)) {
	    internalCompletion.parameter = memoryDescriptor;
	    member->read(srRAIDSet, srMemberBaseOffset + memoryDescriptor->mdMemberByteStart,
			 memoryDescriptor, attributes, &internalCompletion);
	} else {
	    // XXX this is lame, just have completion code check active count instead of the member count
	    // XXX instead of this we could just set the byte count and status here
	    // this would speed up any io request that does not hit all disks in the set
	    // would need to handle the case of all the members being DOA?
	    // there is also a race if we switch the count on the fly
	    srEventSource->completeRequest(memoryDescriptor, kIOReturnSuccess, 0);
	}
    }
}

void AppleRAIDStorageRequest::write(IOService *client, UInt64 byteStart, IOMemoryDescriptor *buffer,
				    IOStorageAttributes * attributes, IOStorageCompletion * completion)
{
    UInt32			index, virtIndex;
    AppleRAIDMember		*member;
    bool			isOnline;
    AppleRAIDMemoryDescriptor	*memoryDescriptor;
    IOStorageCompletion		internalCompletion;
    
    srClient			= client;
    srClientsCompletion 	= *completion;
    srCompletedCount		= 0;
    srMemoryDescriptor 		= buffer;
    srMemoryDescriptorDirection	= buffer->getDirection();
    srByteCount 		= buffer->getLength();
    srByteStart			= byteStart;

    srRequestCount		= srRAIDSet->getMaxRequestCount();
    srActiveCount		= srRAIDSet->getActiveCount();
    srRAIDSet->activeWriteMembers(srActiveMembers, srByteStart, srByteCount);

    internalCompletion.target    = srEventSource;
    internalCompletion.action    = srEventSource->getStorageCompletionAction();

    for (virtIndex = 0; virtIndex < srMemberCount; virtIndex++) {
	
	member = srActiveMembers[virtIndex];
	isOnline = (uintptr_t)(member) >= 0x1000;
	index = isOnline ? member->getMemberIndex() : (uintptr_t)member;
	memoryDescriptor = srMemoryDescriptors[index];

	if (isOnline && memoryDescriptor->configureForMemoryDescriptor(buffer, byteStart, virtIndex)) {
            internalCompletion.parameter = memoryDescriptor;
            member->write(srRAIDSet, srMemberBaseOffset + memoryDescriptor->mdMemberByteStart,
			  memoryDescriptor, attributes, &internalCompletion);
        } else {
            srEventSource->completeRequest(memoryDescriptor, kIOReturnSuccess, 0);
        }
    }
}
