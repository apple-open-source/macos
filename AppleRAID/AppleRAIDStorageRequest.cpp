/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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

    if (srMemberStatus != 0) IODelete(srMemberStatus, IOReturn, srMemberCount);
    if (srMemberByteCounts != 0) IODelete(srMemberByteCounts, UInt64, srMemberCount);
    
    if (srMemberMemoryDescriptors != 0) {
        for (index = 0; index < srMemberCount; index++) {
            if (srMemberMemoryDescriptors[index]) {
		srMemberMemoryDescriptors[index]->release();
	    }
	}
        
        IODelete(srMemberMemoryDescriptors, AppleRAIDMemoryDescriptor *, srMemberCount);
	srMemberMemoryDescriptors = 0;
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
    srSetBlockSize   	= set->arSetBlockSize;
    srMemberCount	= set->getMemberCount();
    srMemberBaseOffset	= set->getBase();
    
    srMemberStatus = IONew(IOReturn, srMemberCount);
    if (srMemberStatus == 0) return false;
    
    srMemberByteCounts = IONew(UInt64, srMemberCount);
    if (srMemberByteCounts == 0) return false;
    
    srMemberMemoryDescriptors = IONew(AppleRAIDMemoryDescriptor *, srMemberCount);
    if (srMemberMemoryDescriptors == 0) return false;
    
    for (index = 0; index < srMemberCount; index++) {
	srMemberMemoryDescriptors[index] = set->allocateMemoryDescriptor(this, index);
	if (!srMemberMemoryDescriptors[index]) return false;
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
    *completion 		= srCompletion;
}

void AppleRAIDStorageRequest::read(IOService *client, UInt64 byteStart, IOMemoryDescriptor *buffer,
				   IOStorageCompletion completion)
{
    UInt32			index;
    AppleRAIDMember		*member;
    IOStorageCompletion		storageCompletion;
    AppleRAIDMemoryDescriptor	*memoryDescriptor;
    
    srClient			= client;
    srCompletion 		= completion;
    srCompletedCount		= 0;
    srMemoryDescriptor 		= buffer;
    srMemoryDescriptorDirection	= buffer->getDirection();
    srByteCount 		= buffer->getLength();
    srByteStart			= byteStart;
    srActiveCount		= srRAIDSet->arActiveCount;

    // XXX this is hideously inefficent, it adds a context switch per i/o request
    // XXX replace event source code with a direct runAction call?
    storageCompletion.target    = srEventSource;
    storageCompletion.action    = srEventSource->getStorageCompletionAction();

    int activeIndex = -1;
    for (index = 0; index < srMemberCount; index++) {

	member = NULL;
        if (srRAIDSet->arMembers[index] &&
	    (srRAIDSet->arMembers[index]->getMemberState() == kAppleRAIDMemberStateOpen ||
	     srRAIDSet->arMembers[index]->getMemberState() == kAppleRAIDMemberStateClosing)) {
            member = srRAIDSet->arMembers[index];
	    activeIndex++;
        }

	memoryDescriptor = srMemberMemoryDescriptors[index];
        if ((member != 0) && memoryDescriptor->configureForMemoryDescriptor(buffer, byteStart, activeIndex)) {
            storageCompletion.parameter = memoryDescriptor;
            member->read(srRAIDSet, srMemberBaseOffset + memoryDescriptor->mdMemberByteStart,
			 memoryDescriptor, storageCompletion);
        } else {

	    // XXX this is lame, just have completion code check active count instead of the member count
	    // XXX instead of this we could just set the byte count and status here
	    // this would speed up concat and degraded mirrors
	    // would need to handle the case of all the members being DOA?
            srEventSource->memberCompleteRequest(memoryDescriptor, kIOReturnSuccess, 0);
        }
    }
}

void AppleRAIDStorageRequest::write(IOService *client, UInt64 byteStart, IOMemoryDescriptor *buffer,
                                    IOStorageCompletion completion)
{
    UInt32			index;
    AppleRAIDMember		*member;
    IOStorageCompletion		storageCompletion;
    AppleRAIDMemoryDescriptor	*memoryDescriptor;
    
    srClient			= client;
    srCompletion 		= completion;
    srCompletedCount		= 0;
    srMemoryDescriptor 		= buffer;
    srMemoryDescriptorDirection	= buffer->getDirection();
    srByteCount 		= buffer->getLength();
    srByteStart			= byteStart;
    srActiveCount		= srRAIDSet->arActiveCount;
    
    storageCompletion.target    = srEventSource;
    storageCompletion.action    = srEventSource->getStorageCompletionAction();

    int activeIndex = -1;
    for (index = 0; index < srMemberCount; index++) {

	member = NULL;
        if (srRAIDSet->arMembers[index] && srRAIDSet->arMembers[index]->getMemberState() >= kAppleRAIDMemberStateClosing) {
            member = srRAIDSet->arMembers[index];
	    activeIndex++;
        }

	memoryDescriptor = srMemberMemoryDescriptors[index];
        if ((member != 0) && memoryDescriptor->configureForMemoryDescriptor(buffer, byteStart, activeIndex)) {
            storageCompletion.parameter = memoryDescriptor;
            member->write(srRAIDSet, srMemberBaseOffset + memoryDescriptor->mdMemberByteStart,
			  memoryDescriptor, storageCompletion);
        } else {
            srEventSource->memberCompleteRequest(memoryDescriptor, kIOReturnSuccess, 0);
        }
    }
}
