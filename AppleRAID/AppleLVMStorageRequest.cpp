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

#define super AppleRAIDStorageRequest
OSDefineMetaClassAndStructors(AppleLVMStorageRequest, AppleRAIDStorageRequest);


AppleLVMStorageRequest * AppleLVMStorageRequest::withAppleRAIDSet(AppleRAIDSet *set)
{
    AppleLVMStorageRequest *storageRequest = new AppleLVMStorageRequest;
    
    if (storageRequest != 0) {
        if (!storageRequest->initWithAppleRAIDSet(set)) {
            storageRequest->release();
            storageRequest = 0;
        }
    }
    
    return storageRequest;
}


void AppleLVMStorageRequest::read(IOService *client, UInt64 byteStart, IOMemoryDescriptor *buffer,
				  IOStorageAttributes *attributes, IOStorageCompletion *completion)
{
    AppleRAIDMember		*member;
    IOStorageCompletion		storageCompletion;
    AppleRAIDMemoryDescriptor	*memoryDescriptor;
    
    srClient			= client;
    srClientsCompletion 	= *completion;
    srCompletedCount		= 0;
    srMemoryDescriptor 		= buffer;
    srMemoryDescriptorDirection	= buffer->getDirection();
    srByteCount 		= buffer->getLength();
    srByteStart			= byteStart;

    storageCompletion.target    = srEventSource;
    storageCompletion.action    = srEventSource->getStorageCompletionActionLVG();

    // only handle lv
    AppleLVMVolume * lv = OSDynamicCast(AppleLVMVolume, client);
    if (!lv) {
	IOLog("AppleLVMStorageRequest::read() received a non-lvm request?\n");
        IOStorage::complete(completion, kIOReturnInternalError, 0);
	return;
    }

    // XXXSNAP
    if (lv->getTypeID() == kLVMTypeSnapRO) {
	lv = lv->parentVolume();    // XXX hack
	assert(lv);
    }

    /*	    
	each storage request has a fixed number of descriptors, if we need more than
	that just recycle the current storage request (this)
    */
    
    UInt64 requestSize = srByteCount;
    UInt64 requestStart = srByteStart;
    UInt32 requestCountMax = srRAIDSet->getMaxRequestCount();
    srRequestCount = 0;

    IOLogRW(" lvm read (%u)  requestStart %llu requestSize %llu\n", (uint32_t)srRequestCount, requestStart, requestSize);

    while (requestSize && srRequestCount < requestCountMax) {

	memoryDescriptor = srMemoryDescriptors[srRequestCount];
	assert(memoryDescriptor);

	if (!memoryDescriptor->configureForMemoryDescriptor(buffer, requestStart, requestSize, lv)) {

	    IOLog(" lvm read configure failed\n");
	    
	    // XXX set length to zero?

	    break;
	}

	requestStart += memoryDescriptor->getLength();
	requestSize -= memoryDescriptor->getLength();
	srRequestCount++;
	
	IOLogRW(" lvm read (%u)  requestStart %llu requestSize %llu\n", (uint32_t)srRequestCount, requestStart, requestSize);
    }

    if (requestSize) {

	if (srRequestCount < requestCountMax) {

	    // XXX something went very wrong

	    IOLog(" lvm read failed #1, srByteStart %llu srByteCount %llu srRequestCount %u requestStart %llu requestSize %llu\n",
		  srByteStart, srByteCount, (uint32_t)srRequestCount, requestStart, requestSize);

	    IOStorage::complete(completion, kIOReturnInternalError, 0);
	    return;
	}

	// XXX set storageCompletion to reschedule more i/o;
	// srBytesScheduled  ??

	IOLog(" lvm read failed #2, srByteStart %llu srByteCount %llu srRequestCount %u requestStart %llu requestSize %llu\n",
	      srByteStart, srByteCount, (uint32_t)srRequestCount, requestStart, requestSize);

        IOStorage::complete(completion, kIOReturnInternalError, 0);
	return;
    }

    assert(srMemberBaseOffset == 0);

    // second pass, kick off the i/o's if device exists

    UInt32 i;
    for (i=0; i < srRequestCount; i++) {

	memoryDescriptor = srMemoryDescriptors[i];
	storageCompletion.parameter = memoryDescriptor;

	member = srRAIDSet->arMembers[memoryDescriptor->mdMemberIndex];

	if (member) {
	    storageCompletion.parameter = memoryDescriptor;
	    member->read(srRAIDSet, memoryDescriptor->mdMemberByteStart, memoryDescriptor, attributes, &storageCompletion);
	} else {
	    srEventSource->completeRequest(memoryDescriptor, kIOReturnSuccess, 0);   // no bytes & no error in completion
	}
    }
}


// XXX the read and write code is almost the same, could merge them together?


void AppleLVMStorageRequest::write(IOService * client, UInt64 byteStart, IOMemoryDescriptor *buffer,
				   IOStorageAttributes *attributes, IOStorageCompletion *completion)
{
    AppleRAIDMember		*member;
    IOStorageCompletion		storageCompletion;
    AppleRAIDMemoryDescriptor	*memoryDescriptor;
    
    srClient			= client;
    srClientsCompletion 	= *completion;
    srCompletedCount		= 0;
    srMemoryDescriptor 		= buffer;
    srMemoryDescriptorDirection	= buffer->getDirection();
    srByteCount 		= buffer->getLength();
    srByteStart			= byteStart;

    storageCompletion.target    = srEventSource;
    storageCompletion.action    = srEventSource->getStorageCompletionActionLVG();

    // only handle lv
    AppleLVMVolume * lv = OSDynamicCast(AppleLVMVolume, client);
    if (!lv) {
	IOLog("AppleLVMStorageRequest::write() received a non-lvm request?\n");
        IOStorage::complete(completion, kIOReturnInternalError, 0);
	return;
    }

    // XXXSNAP
    if (lv->getTypeID() == kLVMTypeSnapRO) {
	lv = lv->parentVolume();    // XXX hack
	assert(lv);
    }

    /*	    
	each storage request has a fixed number of descriptors, if we need more than
	that just recycle the current storage request (this)
    */
    
    UInt64 requestSize = srByteCount;
    UInt64 requestStart = srByteStart;
    UInt32 requestCountMax = srRAIDSet->getMaxRequestCount();
    srRequestCount = 0;

    IOLogRW(" lvm write (%u) requestStart %llu requestSize %llu\n", (uint32_t)srRequestCount, requestStart, requestSize);
    
    while (requestSize && srRequestCount < requestCountMax) {

	memoryDescriptor = srMemoryDescriptors[srRequestCount];
	assert(memoryDescriptor);

	if (!memoryDescriptor->configureForMemoryDescriptor(buffer, requestStart, requestSize, lv)) {

	    IOLog(" lvm write configure failed\n");

	    // XXX set length to zero?

	    break;
	}

	requestStart += memoryDescriptor->getLength();
	requestSize -= memoryDescriptor->getLength();
	srRequestCount++;

	IOLogRW(" lvm write (%u) requestStart %llu requestSize %llu\n", (uint32_t)srRequestCount, requestStart, requestSize);
    }

    if (requestSize) {

	if (srRequestCount < requestCountMax) {

	    // XXX something went very wrong

	    IOLog(" lvm write failed #1, srByteStart %llu srByteCount %llu srRequestCount %u requestStart %llu requestSize %llu\n",
		  srByteStart, srByteCount, (uint32_t)srRequestCount, requestStart, requestSize);

	    IOStorage::complete(completion, kIOReturnInternalError, 0);
	    return;
	}

	// XXX set storageCompletion to reschedule more i/o;
	// srBytesScheduled  ??

	IOLog(" lvm write failed #2, srByteStart %llu srByteCount %llu srRequestCount %u requestStart %llu requestSize %llu\n",
	      srByteStart, srByteCount, (uint32_t)srRequestCount, requestStart, requestSize);

        IOStorage::complete(completion, kIOReturnInternalError, 0);
	return;
    }

//	IOLog(" lvm write   extent %p offset %llu byteStart %llu\n", extent, extent->lvExtentGroupOffset, srByteStart);

    assert(srMemberBaseOffset == 0);

    // second pass, kick off the i/o's if device exists

    UInt32 i;
    for (i=0; i < srRequestCount; i++) {

	memoryDescriptor = srMemoryDescriptors[i];
	storageCompletion.parameter = memoryDescriptor;

	member = srRAIDSet->arMembers[memoryDescriptor->mdMemberIndex];

	if (member) {
	    storageCompletion.parameter = memoryDescriptor;
	    member->write(srRAIDSet, memoryDescriptor->mdMemberByteStart, memoryDescriptor, attributes, &storageCompletion);
	} else {
	    srEventSource->completeRequest(memoryDescriptor, kIOReturnSuccess, 0);   // no bytes & no error in completion
	}
    }
}
