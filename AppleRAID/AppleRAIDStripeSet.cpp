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

const OSSymbol * gAppleRAIDStripeName;

#define super AppleRAIDSet
OSDefineMetaClassAndStructors(AppleRAIDStripeSet, AppleRAIDSet);

AppleRAIDSet * AppleRAIDStripeSet::createRAIDSet(AppleRAIDMember * firstMember)
{
    AppleRAIDStripeSet *raidSet = new AppleRAIDStripeSet;

    IOLog1("AppleRAIDStripeSet::createRAIDSet(%p) called, new set = %p  *********\n", firstMember, raidSet);

    if (!gAppleRAIDStripeName) gAppleRAIDStripeName = OSSymbol::withCString(kAppleRAIDLevelNameStripe);  // XXX free
            
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

    // Publish larger segment sizes for Stripes.
    UInt32	pagesPerChunk;
    UInt64	maxBlockCount, maxSegmentCount;
    OSNumber	* tmpNumber, * tmpNumber2;

    pagesPerChunk = arSetBlockSize / PAGE_SIZE;
        
    AppleRAIDMember * anyMember = NULL;
    for (UInt32 memberIndex = 0; memberIndex < arMemberCount; memberIndex++) {
	if (anyMember = arMembers[memberIndex]) break;
    }
    if (!anyMember) return false;

    // XXX move this to initWithHeader()
	
    tmpNumber  = OSDynamicCast(OSNumber, anyMember->getProperty(kIOMaximumBlockCountReadKey, gIOServicePlane));
    tmpNumber2 = OSDynamicCast(OSNumber, anyMember->getProperty(kIOMaximumSegmentCountReadKey, gIOServicePlane));
    if ((tmpNumber != 0) && (tmpNumber2 != 0)) {
	maxBlockCount   = tmpNumber->unsigned64BitValue();
	maxSegmentCount = tmpNumber2->unsigned64BitValue();
            
	maxBlockCount *= arMemberCount;
	maxSegmentCount *= arMemberCount;
            
	setProperty(kIOMaximumBlockCountReadKey, maxBlockCount, 64);
	setProperty(kIOMaximumSegmentCountReadKey, maxSegmentCount, 64);
    }
        
    tmpNumber  = OSDynamicCast(OSNumber, anyMember->getProperty(kIOMaximumBlockCountWriteKey, gIOServicePlane));
    tmpNumber2 = OSDynamicCast(OSNumber, anyMember->getProperty(kIOMaximumSegmentCountWriteKey, gIOServicePlane));
    if ((tmpNumber != 0) && (tmpNumber2 != 0)) {
	maxBlockCount   = tmpNumber->unsigned64BitValue();
	maxSegmentCount = tmpNumber2->unsigned64BitValue();
            
	maxBlockCount *= arMemberCount;
	maxSegmentCount *= arMemberCount;
            
	setProperty(kIOMaximumBlockCountWriteKey, maxBlockCount, 64);
	setProperty(kIOMaximumSegmentCountWriteKey, maxSegmentCount, 64);
    }

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
    UInt32 raidBlockStop, raidBlockEndOffset;
    UInt32 startMember, stopMember;
    UInt32 blockCount, memberBlockCount, memberBlockStart;
    UInt32 byteCount = memoryDescriptor->getLength();

    assert(mdMemberIndex == activeIndex);
    
    mdSetBlockStart	= byteStart / mdSetBlockSize;
    mdSetBlockOffset	= byteStart % mdSetBlockSize;
    startMember		= mdSetBlockStart % mdMemberCount;
    raidBlockStop	= (byteStart + byteCount - 1) / mdSetBlockSize;
    raidBlockEndOffset	= (byteStart + byteCount - 1) % mdSetBlockSize;
    stopMember		= raidBlockStop % mdMemberCount;
    blockCount		= raidBlockStop - mdSetBlockStart + 1;
    memberBlockCount	= blockCount / mdMemberCount;
    memberBlockStart	= mdSetBlockStart / mdMemberCount;
    
    if (((mdMemberCount + mdMemberIndex - startMember) % mdMemberCount) < (blockCount % mdMemberCount)) memberBlockCount++;
    
    if (startMember > mdMemberIndex) memberBlockStart++;
    
    mdMemberByteStart = (UInt64)memberBlockStart * mdSetBlockSize;
    _length = memberBlockCount * mdSetBlockSize;
    
    if (startMember == mdMemberIndex) {
        mdMemberByteStart += mdSetBlockOffset;
        _length -= mdSetBlockOffset;
    }
        
    if (stopMember == mdMemberIndex) _length -= mdSetBlockSize - raidBlockEndOffset - 1;
    
    mdMemoryDescriptor = memoryDescriptor;
    
    _direction = memoryDescriptor->getDirection();
    
    return _length != 0;
}

IOPhysicalAddress AppleRAIDStripeMemoryDescriptor::getPhysicalSegment(IOByteCount offset, IOByteCount *length)
{
    UInt32		memberBlockNumber = (mdMemberByteStart + offset) / mdSetBlockSize;
    UInt32		memberBlockOffset = (mdMemberByteStart + offset) % mdSetBlockSize;
    UInt32		raidBlockNumber = memberBlockNumber * mdMemberCount + mdMemberIndex - mdSetBlockStart;
    IOByteCount		raidOffset = raidBlockNumber * mdSetBlockSize + memberBlockOffset - mdSetBlockOffset;
    IOPhysicalAddress	physAddress;
    
    physAddress = mdMemoryDescriptor->getPhysicalSegment(raidOffset, length);
    
    memberBlockOffset = mdSetBlockSize - memberBlockOffset;
    if (*length > memberBlockOffset) *length = memberBlockOffset;
    
    return physAddress;
}

