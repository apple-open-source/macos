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
OSDefineMetaClassAndStructors(AppleRAIDConcatSet, AppleRAIDSet);

AppleRAIDSet * AppleRAIDConcatSet::createRAIDSet(AppleRAIDMember * firstMember)
{
    AppleRAIDConcatSet *raidSet = new AppleRAIDConcatSet;

    IOLog1("AppleRAIDConcatSet::createRAIDSet(%p) called, new set = %p  *********\n", firstMember, raidSet);

    while (raidSet){

	if (!raidSet->init()) break;
	if (!raidSet->initWithHeader(firstMember->getHeader(), true)) break;
	if (raidSet->resizeSet(raidSet->getMemberCount())) return raidSet;

	break;
    }

    if (raidSet) raidSet->release();
    
    return 0;
}    


bool AppleRAIDConcatSet::init()
{
    IOLog1("AppleRAIDConcatSet::init() called\n");
            
    if (super::init() == false) return false;

    arMemberBlockCounts = 0;
    arExpectingLiveAdd = 0;

    setProperty(kAppleRAIDLevelNameKey, kAppleRAIDLevelNameConcat);

    arAllocateRequestMethod = OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::allocateRAIDRequest);
    
    return true;
}

void AppleRAIDConcatSet::free(void)
{
    if (arMemberBlockCounts) IODelete(arMemberBlockCounts, UInt64, arMemberCount);
    
    super::free();
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

bool AppleRAIDConcatSet::addSpare(AppleRAIDMember * member)
{
    if (super::addSpare(member) == false) return false;

    member->changeMemberState(kAppleRAIDMemberStateBroken);
    
    return true;
}

bool AppleRAIDConcatSet::addMember(AppleRAIDMember * member)
{
    if (super::addMember(member) == false) return false;

    OSNumber * number = OSDynamicCast(OSNumber, member->getHeaderProperty(kAppleRAIDChunkCountKey));
    if (!number) return false;
    UInt64 memberBlockCount = number->unsigned64BitValue();

    UInt32 memberIndex = member->getMemberIndex();
    arMemberBlockCounts[memberIndex] = memberBlockCount;
    
    // total up the block count as we go
    arSetBlockCount += memberBlockCount;
    arSetMediaSize = arSetBlockCount * arSetBlockSize;

    return true;
}

bool AppleRAIDConcatSet::removeMember(AppleRAIDMember * member, IOOptionBits options)
{
    UInt32 memberIndex = member->getMemberIndex();
    UInt64 memberBlockCount = arMemberBlockCounts[memberIndex];

    IOLog1("AppleRAIDConcatSet::removeMember(%p) called for index %d block count %lld\n",
	   member, (int)memberIndex, memberBlockCount);
    
    // remove this member's blocks from the total block count
    arSetBlockCount -= memberBlockCount;
    arSetMediaSize = arSetBlockCount * arSetBlockSize;
    arMemberBlockCounts[memberIndex] = 0;

    return super::removeMember(member, options);
}

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

bool AppleRAIDConcatSet::resizeSet(UInt32 newMemberCount)
{
    UInt32 oldMemberCount = arMemberCount;
    UInt64 *oldBlockCounts = arMemberBlockCounts;

    arMemberBlockCounts = IONew(UInt64, newMemberCount);
    bzero(arMemberBlockCounts, sizeof(UInt64) * newMemberCount);

    if (oldBlockCounts) {
	bcopy(oldBlockCounts, arMemberBlockCounts, sizeof(UInt64) * MIN(newMemberCount, oldMemberCount));
	IODelete(oldBlockCounts, sizeof(UInt64), oldMemberCount);
    }

    if (super::resizeSet(newMemberCount) == false) return false;

    if (oldMemberCount && arMemberCount > oldMemberCount) arExpectingLiveAdd += arMemberCount - oldMemberCount;

    return true;
}

bool AppleRAIDConcatSet::startSet(void)
{
    if (super::startSet() == false) return false;

    // the set remains paused when a new member is added 
    if (arExpectingLiveAdd) {
	arExpectingLiveAdd--;
	if (arExpectingLiveAdd == 0) unpauseSet();
    }

    return true;
}

bool AppleRAIDConcatSet::publishSet(void)
{
    if (arExpectingLiveAdd) {
	IOLog1("AppleRAIDConcat::publishSet() publish ignored.\n");
	return false;
    }

    return super::publishSet();
}

void AppleRAIDConcatSet::unpauseSet()
{
    if (arExpectingLiveAdd) {
	IOLog1("AppleRAIDConcat::unpauseSet() unpause ignored.\n");
	return;
    }

    super::unpauseSet();
}

UInt64 AppleRAIDConcatSet::getMemberSize(UInt32 memberIndex) const
{
    assert(arMemberBlockCounts);
    assert(memberIndex < arMemberCount);
    
    return arMemberBlockCounts[memberIndex] * arSetBlockSize;;
}

AppleRAIDMemoryDescriptor * AppleRAIDConcatSet::allocateMemoryDescriptor(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    return AppleRAIDConcatMemoryDescriptor::withStorageRequest(storageRequest, memberIndex);
}


// AppleRAIDConcatMemoryDescriptor
// AppleRAIDConcatMemoryDescriptor
// AppleRAIDConcatMemoryDescriptor


#undef super
#define super AppleRAIDMemoryDescriptor
OSDefineMetaClassAndStructors(AppleRAIDConcatMemoryDescriptor, AppleRAIDMemoryDescriptor);

AppleRAIDMemoryDescriptor *
AppleRAIDConcatMemoryDescriptor::withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    AppleRAIDMemoryDescriptor *memoryDescriptor = new AppleRAIDConcatMemoryDescriptor;
    
    if (memoryDescriptor != 0) {
        if (!memoryDescriptor->initWithStorageRequest(storageRequest, memberIndex)) {
            memoryDescriptor->release();
            memoryDescriptor = 0;
        }
    }
    
    return memoryDescriptor;
}

bool AppleRAIDConcatMemoryDescriptor::initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    if (!super::initWithStorageRequest(storageRequest, memberIndex)) return false;

    AppleRAIDConcatSet * set = (AppleRAIDConcatSet *)mdStorageRequest->srRAIDSet;

    mdMemberStart = 0;
    mdMemberEnd = set->getMemberSize(0) - 1;

    for (UInt32 index = 1; index <= memberIndex; index++) {

	mdMemberStart += set->getMemberSize(index - 1);
	mdMemberEnd += set->getMemberSize(index);
    }
    
    return true;
}

bool AppleRAIDConcatMemoryDescriptor::configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart, UInt32 activeIndex)
{
    UInt32 byteCount = memoryDescriptor->getLength();
    UInt64 byteEnd = byteStart + byteCount - 1;

    IOLogRW("concat start=%llu end=%llu bytestart=%llu byteCount=%u\n", mdMemberStart, mdMemberEnd, byteStart, (uint32_t)byteCount);

    assert(mdMemberIndex == activeIndex);

    if ((byteEnd < mdMemberStart) || (byteStart > mdMemberEnd)) return false;
    
    if (byteStart < mdMemberStart) {
        mdMemberByteStart = 0;
        mdMemberOffset = mdMemberStart - byteStart;
        byteCount -= mdMemberOffset;
    } else {
        mdMemberByteStart = byteStart - mdMemberStart;
        mdMemberOffset = 0;
    }
    
    if (byteEnd > mdMemberEnd) {
        byteCount -= byteEnd - mdMemberEnd;
    }
    
    _length = byteCount;
    
    mdMemoryDescriptor = memoryDescriptor;
    
    _flags = (_flags & ~kIOMemoryDirectionMask) | memoryDescriptor->getDirection();
    
    IOLogRW("concat mdbytestart=%llu _length=0x%x\n", byteStart, (uint32_t)_length);

    return true;
}

addr64_t AppleRAIDConcatMemoryDescriptor::getPhysicalSegment(IOByteCount offset, IOByteCount *length, IOOptionBits options)
{
    IOByteCount		setOffset = offset + mdMemberOffset;
    addr64_t		physAddress;
    
    physAddress = mdMemoryDescriptor->getPhysicalSegment(setOffset, length, options);
    
    return physAddress;
}
