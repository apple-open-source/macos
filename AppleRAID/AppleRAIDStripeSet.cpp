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

#define super AppleRAIDSet
OSDefineMetaClassAndStructors(AppleRAIDStripeSet, AppleRAIDSet);

AppleRAIDSet * AppleRAIDStripeSet::createRAIDSet(AppleRAIDMember * firstMember)
{
    AppleRAIDStripeSet *raidSet = new AppleRAIDStripeSet;

    IOLog1("AppleRAIDStripeSet::createRAIDSet(%p) called, new set = %p  *********\n", firstMember, raidSet);

    while (raidSet){

	if (!raidSet->init()) break;
	if (!raidSet->initWithHeader(firstMember->getHeader(), true)) break;
	if (raidSet->resizeSet(raidSet->getMemberCount())) return raidSet;

	break;
    }

    if (raidSet) raidSet->release();
    
    return 0;
}    


bool AppleRAIDStripeSet::init()
{
    IOLog1("AppleRAIDStripeSet::init() called\n");
            
    if (super::init() == false) return false;

    setProperty(kAppleRAIDLevelNameKey, kAppleRAIDLevelNameStripe);

    arAllocateRequestMethod = OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::allocateRAIDRequest);
    
    return true;
}

void AppleRAIDStripeSet::free(void)
{
    super::free();
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

bool AppleRAIDStripeSet::addSpare(AppleRAIDMember * member)
{
    if (super::addSpare(member) == false) return false;

    member->changeMemberState(kAppleRAIDMemberStateBroken);
    
    return true;
}

bool AppleRAIDStripeSet::addMember(AppleRAIDMember * member)
{
    if (super::addMember(member) == false) return false;

    // block count = # of stripes * per member block count
    OSNumber * number = OSDynamicCast(OSNumber, member->getHeaderProperty(kAppleRAIDChunkCountKey));
    if (!number) return false;
    arSetBlockCount = number->unsigned64BitValue() * arMemberCount;
    arSetMediaSize = arSetBlockCount * arSetBlockSize;
    
    return true;
}

bool AppleRAIDStripeSet::startSet(void)
{
    if (super::startSet() == false) return false;

    // scale up these properties by the member count of the stripe
    setSmallest64BitMemberPropertyFor(kIOMaximumBlockCountReadKey, arMemberCount);
    setSmallest64BitMemberPropertyFor(kIOMaximumBlockCountWriteKey, arMemberCount);
    setSmallest64BitMemberPropertyFor(kIOMaximumByteCountReadKey, arMemberCount);
    setSmallest64BitMemberPropertyFor(kIOMaximumByteCountWriteKey, arMemberCount);

    setSmallest64BitMemberPropertyFor(kIOMaximumSegmentCountReadKey, arMemberCount);
    setSmallest64BitMemberPropertyFor(kIOMaximumSegmentCountWriteKey, arMemberCount);

    return true;
}

AppleRAIDMemoryDescriptor * AppleRAIDStripeSet::allocateMemoryDescriptor(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    return AppleRAIDStripeMemoryDescriptor::withStorageRequest(storageRequest, memberIndex);
}

// AppleRAIDStripeMemoryDescriptor
// AppleRAIDStripeMemoryDescriptor
// AppleRAIDStripeMemoryDescriptor

#undef super
#define super AppleRAIDMemoryDescriptor
OSDefineMetaClassAndStructors(AppleRAIDStripeMemoryDescriptor, AppleRAIDMemoryDescriptor);

AppleRAIDMemoryDescriptor *
AppleRAIDStripeMemoryDescriptor::withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    AppleRAIDMemoryDescriptor *memoryDescriptor = new AppleRAIDStripeMemoryDescriptor;
    
    if (memoryDescriptor != 0) {
        if (!memoryDescriptor->initWithStorageRequest(storageRequest, memberIndex)) {
            memoryDescriptor->release();
            memoryDescriptor = 0;
        }
    }
    
    return memoryDescriptor;
}

bool AppleRAIDStripeMemoryDescriptor::initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    if (!super::initWithStorageRequest(storageRequest, memberIndex)) return false;
    
    mdMemberCount = storageRequest->srMemberCount;
    mdSetBlockSize = storageRequest->srSetBlockSize;
    
    return true;
}

bool AppleRAIDStripeMemoryDescriptor::configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart, UInt32 activeIndex)
{
    UInt32 byteCount = memoryDescriptor->getLength();
    UInt32 blockCount, memberBlockCount;
    UInt64 memberBlockStart, setBlockStop;
    UInt32 setBlockEndOffset;
    UInt32 startMember, stopMember;

    mdSetBlockStart	= byteStart / mdSetBlockSize;
    mdSetBlockOffset	= byteStart % mdSetBlockSize;
    setBlockStop	= (byteStart + byteCount - 1) / mdSetBlockSize;
    setBlockEndOffset	= (byteStart + byteCount - 1) % mdSetBlockSize;
    blockCount		= setBlockStop - mdSetBlockStart + 1;
    memberBlockCount	= blockCount / mdMemberCount;
    memberBlockStart	= mdSetBlockStart / mdMemberCount;
    startMember		= mdSetBlockStart % mdMemberCount;
    stopMember		= setBlockStop % mdMemberCount;
    
    // per member stuff
    assert(mdMemberIndex == activeIndex);
    if (((mdMemberCount + mdMemberIndex - startMember) % mdMemberCount) < (blockCount % mdMemberCount)) memberBlockCount++;
    
    if (startMember > mdMemberIndex) memberBlockStart++;
    
    mdMemberByteStart = memberBlockStart * mdSetBlockSize;
    _length = memberBlockCount * mdSetBlockSize;
    
    if (startMember == mdMemberIndex) {
        mdMemberByteStart += mdSetBlockOffset;
        _length -= mdSetBlockOffset;
    }
        
    if (stopMember == mdMemberIndex) _length -= mdSetBlockSize - setBlockEndOffset - 1;
    
    mdMemoryDescriptor = memoryDescriptor;
    
    _flags = (_flags & ~kIOMemoryDirectionMask) | memoryDescriptor->getDirection();
    
    return _length != 0;
}

addr64_t AppleRAIDStripeMemoryDescriptor::getPhysicalSegment(IOByteCount offset, IOByteCount *length, IOOptionBits options)
{
    UInt32		memberBlockStart = (mdMemberByteStart + offset) / mdSetBlockSize;
    UInt32		memberBlockOffset = (mdMemberByteStart + offset) % mdSetBlockSize;
    UInt32		setBlockNumber = memberBlockStart * mdMemberCount + mdMemberIndex - mdSetBlockStart;
    IOByteCount		setOffset = setBlockNumber * mdSetBlockSize + memberBlockOffset - mdSetBlockOffset;
    addr64_t		physAddress;
    
    physAddress = mdMemoryDescriptor->getPhysicalSegment(setOffset, length, options);
    
    memberBlockOffset = mdSetBlockSize - memberBlockOffset;
    if (length && (*length > memberBlockOffset)) *length = memberBlockOffset;
    
    return physAddress;
}
