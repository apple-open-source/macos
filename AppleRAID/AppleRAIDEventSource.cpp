/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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
/*
 *  DRI: Josh de Cesare
 *
 */

#include "AppleRAIDEventSource.h"

#undef super
#define super IOEventSource
OSDefineMetaClassAndStructors(AppleRAIDEventSource, IOEventSource);

AppleRAIDEventSource *AppleRAIDEventSource::withAppleRAIDSet(AppleRAID *appleRAID, Action action)
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

bool AppleRAIDEventSource::initWithAppleRAIDSet(AppleRAID *appleRAID, Action action)
{
    if (!super::init(appleRAID, (IOEventSource::Action)action)) return false;
    
    queue_init(&fCompletedHead);
    
    return true;
}

void AppleRAIDEventSource::sliceCompleteRequest(AppleRAIDMemoryDescriptor *memoryDescriptor,
                                                IOReturn status, UInt64 actualByteCount)
{
    UInt32			sliceNumber = memoryDescriptor->mdSliceNumber;
    AppleRAIDStorageRequest	*storageRequest = memoryDescriptor->mdStorageRequest;
    
    closeGate();
    
    // Count the slice as completed.
    storageRequest->srCompletedCount++;
    
    // Save the slices results.
    storageRequest->srSliceStatus[sliceNumber] = status;
    storageRequest->srSliceByteCounts[sliceNumber] = actualByteCount;
    
    if (storageRequest->srCompletedCount == storageRequest->srSliceCount) {
        queue_enter(&fCompletedHead, storageRequest, AppleRAIDStorageRequest *, fCommandChain);
        
	signalWorkAvailable();
    }
    
    openGate();
}

void AppleRAIDEventSource::terminateRAIDMedia(IOMedia *media)
{
    closeGate();
    
    while (doTerminateRAIDMedia != 0) {
        sleepGate(&doTerminateRAIDMedia, THREAD_UNINT);
    }
    
    doTerminateRAIDMedia = media;
    doTerminateRAIDMedia->retain();
    
    signalWorkAvailable();
    
    openGate();
}

bool AppleRAIDEventSource::checkForWork(void)
{
    AppleRAID			*appleRAID = (AppleRAID *)owner;
    AppleRAIDStorageRequest 	*storageRequest;
    
    if (doTerminateRAIDMedia != 0) {
        appleRAID->terminateRAIDMedia(doTerminateRAIDMedia);
        doTerminateRAIDMedia->release();
        
        doTerminateRAIDMedia = 0;
        wakeupGate(&doTerminateRAIDMedia, true);
    }
    
    if (!queue_empty(&fCompletedHead)) {
      queue_remove_first(&fCompletedHead, storageRequest, AppleRAIDStorageRequest *, fCommandChain);
      
      (*(Action)action)(appleRAID, storageRequest);
    }
    
    return !queue_empty(&fCompletedHead);
}

IOStorageCompletionAction AppleRAIDEventSource::getStorageCompletionAction(void)
{
    return (IOStorageCompletionAction)&AppleRAIDEventSource::sliceCompleteRequest;
}
