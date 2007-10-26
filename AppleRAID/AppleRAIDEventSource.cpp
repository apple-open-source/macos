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

#undef super
#define super IOEventSource
OSDefineMetaClassAndStructors(AppleRAIDEventSource, IOEventSource);

AppleRAIDEventSource *AppleRAIDEventSource::withAppleRAIDSet(AppleRAIDSet *appleRAID, Action action)
{
    AppleRAIDEventSource *eventSource = new AppleRAIDEventSource;
    
    if (eventSource != 0) {
        if (!eventSource->initWithAppleRAIDSet(appleRAID, action)) {
            eventSource->release();
            eventSource = 0;
        }
    }
    
    return eventSource;
}

bool AppleRAIDEventSource::initWithAppleRAIDSet(AppleRAIDSet *appleRAID, Action action)
{
    if (!super::init(appleRAID, (IOEventSource::Action)action)) return false;
    
    queue_init(&fCompletedHead);
    
    return true;
}

void AppleRAIDEventSource::completeRequest(AppleRAIDMemoryDescriptor * memoryDescriptor,
					   IOReturn status, UInt64 actualByteCount)
{
    UInt32			memberIndex = memoryDescriptor->mdMemberIndex;
    AppleRAIDStorageRequest *	storageRequest = memoryDescriptor->mdStorageRequest;
    
    closeGate();

    // Count the member as completed.
    storageRequest->srCompletedCount++;
    
    // Save the members results.
    storageRequest->srRequestStatus[memberIndex] = status;
    storageRequest->srRequestByteCounts[memberIndex] = actualByteCount;
    
    if (storageRequest->srCompletedCount == storageRequest->srRequestCount) {
        queue_enter(&fCompletedHead, storageRequest, AppleRAIDStorageRequest *, fCommandChain);
        
	signalWorkAvailable();
    }
    
    openGate();
}


void AppleRAIDEventSource::completeRequestLVG(AppleLVMMemoryDescriptor * memoryDescriptor,
					      IOReturn status, UInt64 actualByteCount)
{
    UInt32			requestIndex = memoryDescriptor->mdRequestIndex;
    AppleRAIDStorageRequest *	storageRequest = memoryDescriptor->mdStorageRequest;
    
    closeGate();

    // Count the member as completed.
    storageRequest->srCompletedCount++;
    
    // Save the members results.
    storageRequest->srRequestStatus[requestIndex] = status;
    storageRequest->srRequestByteCounts[requestIndex] = actualByteCount;
    
    if (storageRequest->srCompletedCount == storageRequest->srRequestCount) {
        queue_enter(&fCompletedHead, storageRequest, AppleRAIDStorageRequest *, fCommandChain);
        
	signalWorkAvailable();
    }
    
    openGate();
}


bool AppleRAIDEventSource::checkForWork(void)
{
    AppleRAIDSet	        *appleRAID = (AppleRAIDSet *)owner;
    AppleRAIDStorageRequest 	*storageRequest;
    
    if (!queue_empty(&fCompletedHead)) {
	queue_remove_first(&fCompletedHead, storageRequest, AppleRAIDStorageRequest *, fCommandChain);
      
	(*(Action)action)(appleRAID, storageRequest);
    }
    
    return !queue_empty(&fCompletedHead);
}

IOStorageCompletionAction AppleRAIDEventSource::getStorageCompletionAction(void)
{
    return OSMemberFunctionCast(IOStorageCompletionAction, this, &AppleRAIDEventSource::completeRequest);
}

IOStorageCompletionAction AppleRAIDEventSource::getStorageCompletionActionLVG(void)
{
    return OSMemberFunctionCast(IOStorageCompletionAction, this, &AppleRAIDEventSource::completeRequestLVG);
}
