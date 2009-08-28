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
OSDefineMetaClassAndStructors(AppleLVMGroup, AppleRAIDSet);

AppleRAIDSet * AppleLVMGroup::createRAIDSet(AppleRAIDMember * firstMember)
{
    AppleLVMGroup *raidSet = new AppleLVMGroup;

    AppleLVGTOCEntrySanityCheck();	// debug only
    AppleLVMVolumeOnDiskSanityCheck();	// debug only

    IOLog1("AppleLVMGroup::createRAIDSet(%p) called, new set = %p  *********\n", firstMember, raidSet);

    while (raidSet){

	if (!raidSet->init()) break;
	if (!raidSet->initWithHeader(firstMember->getHeader(), true)) break;
	if (raidSet->resizeSet(raidSet->getMemberCount())) return raidSet;

	break;
    }

    if (raidSet) raidSet->release();
    
    return 0;
}    


bool AppleLVMGroup::init()
{
    IOLog1("AppleLVMGroup::init() called\n");
            
    if (super::init() == false) return false;

    arMemberBlockCounts = 0;
    arMemberStartingOffset = 0;
    arExpectingLiveAdd = 0;
    arPrimaryNeedsUpdate = false;
    arPrimaryBuffer = NULL;
    arLogicalVolumeCount = 0;
    arLogicalVolumeActiveCount = 0;
    arLogicalVolumes = NULL;
    arMetaDataVolumes = 0;
    arExtentCount = 0;
    arExtents = NULL;
	
    setProperty(kAppleRAIDLevelNameKey, kAppleRAIDLevelNameLVG);

    arAllocateRequestMethod = OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::allocateRAIDRequest);
    
    return true;
}

void AppleLVMGroup::free(void)
{

    if (arMemberBlockCounts) IODelete(arMemberBlockCounts, UInt64, arMemberCount);
    if (arMemberStartingOffset) IODelete(arMemberStartingOffset, UInt64, arMemberCount);
    if (arPrimaryBuffer) arPrimaryBuffer->release();

    UInt32 i;
    if (arLogicalVolumes) {
	for (i = 0; i < arLogicalVolumeCount; i++) {
	    if (arLogicalVolumes[i]) {
		arController->removeLogicalVolume(arLogicalVolumes[i]);
		arLogicalVolumes[i]->release();
		arLogicalVolumes[i] = NULL;
	    }
	}
	IODelete(arLogicalVolumes, AppleLVMVolume *, 1024);  // XXXTOC
    }

    if (arMetaDataVolumes) {
	for (i = 0; i < arMemberCount; i++) {
	    if (arMetaDataVolumes[i]) arMetaDataVolumes[i]->release();
	}
	IODelete(arMetaDataVolumes, AppleLVMVolume *, arMemberCount);
    }
    
    super::free();
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

bool AppleLVMGroup::addSpare(AppleRAIDMember * member)
{
    if (super::addSpare(member) == false) return false;

    member->changeMemberState(kAppleRAIDMemberStateBroken);
    
    return true;
}

bool AppleLVMGroup::addMember(AppleRAIDMember * member)
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

    // read the TOC on this member
    IOBufferMemoryDescriptor * newPrimaryBuffer = readPrimaryMetaData(member);
    if (newPrimaryBuffer && arPrimaryBuffer != newPrimaryBuffer) {
	if (arPrimaryBuffer) arPrimaryBuffer->release();
	arPrimaryBuffer = newPrimaryBuffer;
    }

    // scan the logical volumes in start

    return true;
}

bool AppleLVMGroup::removeMember(AppleRAIDMember * member, IOOptionBits options)
{
    UInt32 memberIndex = member->getMemberIndex();
    UInt64 memberBlockCount = arMemberBlockCounts[memberIndex];

    IOLog1("AppleLVMGroup::removeMember(%p) called for index %d block count %lld\n",
	   member, (int)memberIndex, memberBlockCount);

    // XXX
    //               tbd
    // XXX
    
    // remove this member's blocks from the total block count
    arSetBlockCount -= memberBlockCount;
    arSetMediaSize = arSetBlockCount * arSetBlockSize;

    if (arMetaDataVolumes[memberIndex]) arMetaDataVolumes[memberIndex]->release();
    arMetaDataVolumes[memberIndex] = 0;
	
    return super::removeMember(member, options);
}

bool AppleLVMGroup::resizeSet(UInt32 newMemberCount)
{
    UInt32 oldMemberCount = arMemberCount;

    UInt64 * oldBlockCounts = arMemberBlockCounts;
    arMemberBlockCounts = IONew(UInt64, newMemberCount);
    bzero(arMemberBlockCounts, sizeof(UInt64) * newMemberCount);
    if (oldBlockCounts) {
	bcopy(oldBlockCounts, arMemberBlockCounts, sizeof(UInt64) * oldMemberCount);
	IODelete(oldBlockCounts, sizeof(UInt64), oldMemberCount);
    }

    UInt64 * oldStartingOffset = arMemberStartingOffset;
    arMemberStartingOffset = IONew(UInt64, newMemberCount);
    bzero(arMemberStartingOffset, sizeof(UInt64) * newMemberCount);
    if (oldStartingOffset) {
	bcopy(oldStartingOffset, arMemberStartingOffset, sizeof(UInt64) * oldMemberCount);
	IODelete(oldStartingOffset, sizeof(UInt64), oldMemberCount);
    }

    AppleLVMVolume ** oldMetaDataVolumes = arMetaDataVolumes;
    arMetaDataVolumes = IONew(AppleLVMVolume *, newMemberCount);
    bzero(arMetaDataVolumes, sizeof(AppleLVMVolume *) * newMemberCount);
    if (oldMetaDataVolumes) {
	bcopy(oldMetaDataVolumes, arMetaDataVolumes, sizeof(AppleLVMVolume *) * oldMemberCount);
	IODelete(oldMetaDataVolumes, sizeof(AppleLVMVolume *), oldMemberCount);
    }

    if (super::resizeSet(newMemberCount) == false) return false;

    if (oldMemberCount && arMemberCount > oldMemberCount) arExpectingLiveAdd += arMemberCount - oldMemberCount;

    return true;
}

bool AppleLVMGroup::startSet(void)
{
    if (super::startSet() == false) return false;

    // the set remains paused when a new member is added 
    if (arExpectingLiveAdd) {
	arExpectingLiveAdd--;
	if (arExpectingLiveAdd == 0) unpauseSet();

	// XXXTOC will need to update the TOC on the new member
    }

    assert(arPrimaryBuffer);
    if (!arPrimaryBuffer) return false;

    // once all the disks have been scanned we should have the
    // the best available TOC and also should be able to read
    // the logical volume entries from any of the disks
    AppleRAIDPrimaryOnDisk * primary = (AppleRAIDPrimaryOnDisk *)arPrimaryBuffer->getBytesNoCopy();
    if (!primary) return false;

    arLogicalVolumeCount = primary->pri.volumeCount;
    arLogicalVolumeActiveCount = 0;

    if (!arLogicalVolumes) {
	arLogicalVolumes = IONew(AppleLVMVolume *, 1024);  // XXXTOC
	if (!arLogicalVolumes) return false;
	bzero(arLogicalVolumes, 1024 * sizeof(AppleLVMVolume *));
    }

    if (arPrimaryNeedsUpdate) {
	IOLog("AppleLVMGroup::startSet: updating primary meta data for LVG \"%s\" (%s)\n", getSetNameString(), getUUIDString());
	IOReturn rc = writePrimaryMetaData(arPrimaryBuffer);
	if (rc) return false;
	arPrimaryNeedsUpdate = false;
    }

    if (!initializeSecondary()) return false;

    if (!initializeVolumes(primary)) return false;
    
    return true;
}

bool AppleLVMGroup::publishSet(void)
{
    IOLog1("AppleLVMGroup::publishSet called %p\n", this);

    if (arExpectingLiveAdd || arActiveCount == 0) {
	IOLog1("AppleLVMGroup::publishSet() publish ignored.\n");
	return false;
    }

    bool success = super::publishSet();

    UInt32 index;
    for (index = 0; index < arLogicalVolumeCount; index++) {

	AppleLVMVolume * lv = arLogicalVolumes[index];

	// XXX check for errors?
	if (lv && lv->isAVolume()) publishVolume(lv);
	if (lv && lv->isASnapShot()) publishVolume(lv);
    }

    return success;
}

bool AppleLVMGroup::unpublishSet(void)
{
    UInt32 index;
    for (index = 0; index < arLogicalVolumeCount; index++) {

	AppleLVMVolume * lv = arLogicalVolumes[index];

	if (lv && lv->isPublished()) (void)unpublishVolume(lv);
    }

    return super::unpublishSet();
}

void AppleLVMGroup::unpauseSet()
{
    if (arExpectingLiveAdd) {
	IOLog1("AppleLVMGroup::unpauseSet() unpause ignored.\n");
	return;
    }

    super::unpauseSet();
}

bool AppleLVMGroup::handleOpen(IOService * client, IOOptionBits options, void * argument)
{
    // only allow clients that are logical volumes to open (or ourself)
    if (OSDynamicCast(AppleLVMVolume, client) || client == this) {

	return super::handleOpen(client, options, argument);
    }

    return false;
}

UInt64 AppleLVMGroup::getMemberSize(UInt32 memberIndex) const
{
    assert(arMemberBlockCounts);
    assert(memberIndex < arMemberCount);
    
    return arMemberBlockCounts[memberIndex] * arSetBlockSize;;
}

UInt64 AppleLVMGroup::getMemberStartingOffset(UInt32 memberIndex) const
{
    assert(memberIndex < arMemberCount);
    return arMemberStartingOffset[memberIndex];
}

UInt32 AppleLVMGroup::getMemberIndexFromOffset(UInt64 offset) const
{
    UInt32 index = 0;
    UInt64 memberDataSize;
    while (index < arMemberCount) {

	memberDataSize = arMemberBlockCounts[index] * arSetBlockSize;

	IOLog2("getMemberIndexFromOffset(%llu) index %u, start %llu, end %llu\n",
	       offset, (uint32_t)index, arMemberStartingOffset[index], arMemberStartingOffset[index] + memberDataSize);
	
	if (offset < arMemberStartingOffset[index] + memberDataSize) break;
	index++;
    }

    assert(index < arMemberCount);
    
    return index;
}


bool AppleLVMGroup::memberOffsetFromLVGOffset(UInt64 lvgOffset, AppleRAIDMember ** member, UInt64 * memberOffset)
{

    UInt32 index = getMemberIndexFromOffset(lvgOffset);
    if (index >= arMemberCount) return false;
    if (!arMembers[index]) return false;

    *member = arMembers[index];
    *memberOffset = lvgOffset - getMemberStartingOffset(index);

    return true;
}


OSDictionary * AppleLVMGroup::getSetProperties(void)
{
    OSDictionary * props = super::getSetProperties();
    OSNumber * tmpNumber;

    if (props) {
	tmpNumber = OSNumber::withNumber(arExtentCount, 64);
	if (tmpNumber) {
	    props->setObject(kAppleRAIDLVGExtentsKey, tmpNumber);
	    tmpNumber->release();
	}
	
	tmpNumber = OSNumber::withNumber(arLogicalVolumeActiveCount, 32);
	if (tmpNumber) {
	    props->setObject(kAppleRAIDLVGVolumeCountKey, tmpNumber);
	    tmpNumber->release();
	}

	tmpNumber = OSNumber::withNumber(calculateFreeSpace(), 64);
	if (tmpNumber) {
	    props->setObject(kAppleRAIDLVGFreeSpaceKey, tmpNumber);
	    tmpNumber->release();
	}
    }

    return props;
}
    
AppleRAIDMemoryDescriptor * AppleLVMGroup::allocateMemoryDescriptor(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    return AppleLVMMemoryDescriptor::withStorageRequest(storageRequest, memberIndex);
}

// AppleLVMMemoryDescriptor
// AppleLVMMemoryDescriptor
// AppleLVMMemoryDescriptor

#undef super
#define super AppleRAIDMemoryDescriptor
OSDefineMetaClassAndStructors(AppleLVMMemoryDescriptor, AppleRAIDMemoryDescriptor);

AppleRAIDMemoryDescriptor * AppleLVMMemoryDescriptor::withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 requestIndex)
{
    AppleLVMMemoryDescriptor *memoryDescriptor = new AppleLVMMemoryDescriptor;
    
    if (memoryDescriptor != 0) {
        if (!memoryDescriptor->initWithStorageRequest(storageRequest, requestIndex)) {
            memoryDescriptor->release();
            memoryDescriptor = 0;
        }
    }

    memoryDescriptor->mdRequestIndex = requestIndex;

    return memoryDescriptor;
}

bool AppleLVMMemoryDescriptor::initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 requestIndex)
{
    if (!super::initWithStorageRequest(storageRequest, requestIndex)) return false;

    return true;
}

bool AppleLVMMemoryDescriptor::configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 requestStart, UInt64 requestSize, AppleLVMVolume * lv)
{
    IOLogRW("LVG bytestart=%llu requestSize=%llu\n", requestStart, requestSize);

    AppleLVMLogicalExtent * extent = lv->findExtent(requestStart);
    if (!extent) return false;

    mdMemberIndex = extent->lvMemberIndex;

    // find start within the extent
    UInt64 startingOffset = requestStart - extent->lvExtentVolumeOffset;
    mdMemberByteStart = extent->lvExtentMemberOffset + startingOffset;

    // clip requests that go past the end of the extent
    if (startingOffset + requestSize > extent->lvExtentSize) requestSize = extent->lvExtentSize - startingOffset;
    _length = requestSize;

    // find this extent's offset back to lv addressing
    mdRequestOffset = (IOByteCount)(requestStart - mdStorageRequest->srByteStart);

    mdMemoryDescriptor = memoryDescriptor;
    
    _flags = (_flags & ~kIOMemoryDirectionMask) | memoryDescriptor->getDirection();
    
    IOLogRW("LVG mdbytestart=%llu _length=0x%x\n", requestStart, (uint32_t)_length);

    return true;
}

addr64_t AppleLVMMemoryDescriptor::getPhysicalSegment(IOByteCount offset, IOByteCount *length, IOOptionBits options)
{
    IOByteCount		volumeOffset = offset + mdRequestOffset;
    addr64_t		physAddress;
    
    physAddress = mdMemoryDescriptor->getPhysicalSegment(volumeOffset, length, options);

    return physAddress;
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

#undef super
#define super AppleRAIDSet

IOBufferMemoryDescriptor * AppleLVMGroup::readPrimaryMetaData(AppleRAIDMember * member)
{
    IOLog1("AppleLVMGroup::readPrimaryMetaData(%p) entered\n", member);

    AppleRAIDPrimaryOnDisk * primary = NULL;
    IOBufferMemoryDescriptor * primaryBuffer = super::readPrimaryMetaData(member);
    if (!primaryBuffer) goto error;

    primary = (AppleRAIDPrimaryOnDisk *)primaryBuffer->getBytesNoCopy();
    if (!primary) goto error;

#if defined(__LITTLE_ENDIAN__)
    {
	ByteSwapPrimaryHeader(primary);
	UInt64 index;
	AppleLVGTOCEntryOnDisk * tocEntry = (AppleLVGTOCEntryOnDisk *)(primary + 1);
	for (index = 0; index < primary->pri.volumeCount; index++) {
	    ByteSwapLVGTOCEntry(tocEntry + index);
	}
    }
#endif    

    // compare against raid header sequence number
    if (primary->priSequenceNumber > arSequenceNumber) {
	IOLog("AppleLVMGroup::readPrimaryMetaData() sequence number in future, new volumes may have been lost.\n");
	goto error;
    }

    // compare against the current "best" primary
    if (arPrimaryBuffer) {

	AppleRAIDPrimaryOnDisk * current = (AppleRAIDPrimaryOnDisk *)arPrimaryBuffer->getBytesNoCopy();
	
	// if the sequence numbers match we are done
	if (current->priSequenceNumber != primary->priSequenceNumber) {
	    IOLog("AppleLVMGroup::readPrimaryMetaData() the sequence number was out of date.\n");
	    if (current->priSequenceNumber < primary->priSequenceNumber) goto error;
	}
    }

    IOLog1("AppleLVMGroup::readPrimaryMetaData(%p) successful, volumes = %llu\n", member, primary->pri.volumeCount);

    return primaryBuffer;

error:
    IOLog("AppleLVMGroup::readPrimaryMetaData() was unsuccessful for member %s.\n", member->getUUIDString());
    if (primaryBuffer) primaryBuffer->release();

    arPrimaryNeedsUpdate = true;
    return NULL;
}


IOReturn AppleLVMGroup::writePrimaryMetaData(IOBufferMemoryDescriptor * primaryBuffer)
{
    IOLog1("AppleLVMGroup::writePrimaryMetaData(%p) entered\n", this);

    AppleRAIDPrimaryOnDisk * primary = (AppleRAIDPrimaryOnDisk *)primaryBuffer->getBytesNoCopy();
    if (!primary) return false;

    // XXX
    //
    // only allow writes if all members are active
    // 
    // disallow all changes to LVG (create, destroy, ...)
    // 
    // XXX

    primary->priSequenceNumber = arSequenceNumber;

#if defined(__LITTLE_ENDIAN__)
    UInt64 index;
    AppleLVGTOCEntryOnDisk * tocEntry = (AppleLVGTOCEntryOnDisk *)(primary + 1);
    for (index = 0; index < primary->pri.extentCount; index++) {
	ByteSwapLVGTOCEntry(tocEntry + index);
    }
    ByteSwapPrimaryHeader(primary);
#endif    

    IOReturn rc = super::writePrimaryMetaData(primaryBuffer);

    // we hold onto this buffer so we need to should swap it back
    // it is tempting to grab a new buffer, but that can fail and
    // this can be called by the driver for non-user reasons.

#if defined(__LITTLE_ENDIAN__)
    ByteSwapPrimaryHeader(primary);
    tocEntry = (AppleLVGTOCEntryOnDisk *)(primary + 1);
    for (index = 0; index < primary->pri.extentCount; index++) {
	ByteSwapLVGTOCEntry(tocEntry + index);
    }
#endif    

    IOLog1("AppleLVMGroup::writePrimaryMetaData(%p): was %ssuccessful\n", this, rc ? "un" : "");
    return rc;
}



//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

IOBufferMemoryDescriptor * AppleLVMGroup::readLogicalVolumeEntry(UInt64 offset, UInt32 size)
{
    bool autoSize = size ? false : true;
    
    IOLog1("AppleLVMGroup::readLogicalVolumeEntry(%p) lve offset %llu size %u %s\n", this, offset, (uint32_t)size, autoSize ? "autosized = true" : "");

    if (autoSize) size = kAppleLVMVolumeOnDiskMinSize;

    AppleRAIDMember * member;
    UInt64 memberOffset;
    if (!memberOffsetFromLVGOffset(offset, &member, &memberOffset)) {
	return NULL;
    }
    
retry:
    
    // Allocate a buffer to read into
    IOBufferMemoryDescriptor * lveBuffer = IOBufferMemoryDescriptor::withCapacity(size, kIODirectionNone);
    if (lveBuffer == 0) return NULL;
    
    // read 
    if (!readIntoBuffer(member, lveBuffer, memberOffset)) {
	lveBuffer->release();
	return NULL;
    }
    
    // Make sure the logical volume header contains the correct signature.
    AppleLVMVolumeOnDisk * lve = (AppleLVMVolumeOnDisk *)lveBuffer->getBytesNoCopy();
    if (strncmp(lve->lvMagic, kAppleLVMVolumeMagic, sizeof(lve->lvMagic))) {
	lveBuffer->release();
	return NULL;
    }

    ByteSwapLVMVolumeHeader(lve);
    if (lve->lvHeaderSize != size) {
	if (autoSize) {
	    autoSize = false;
	    size = lve->lvHeaderSize;
	    lveBuffer->release();
	    goto retry;
	}
	IOLog("AppleLVMGroup::readLogicalVolumeEntry(): size mismatch %u <> %u\n", (uint32_t)lve->lvHeaderSize, (uint32_t)size);
	if (lve->lvHeaderSize > size) {
	    lveBuffer->release();
	    return NULL;
	}
    }

#if defined(__LITTLE_ENDIAN__)
    AppleRAIDExtentOnDisk *extent = (AppleRAIDExtentOnDisk *)((char *)lve + lve->lvExtentsStart);
    UInt32 i;
    for (i=0; i < lve->lvExtentsCount; i++) {
	ByteSwapExtent(&extent[i]);
    }
#endif    
    
    IOLog1("AppleLVMGroup::readLogicalVolumeEntry(%p): was successful, extent count = %u\n", this, (uint32_t)lve->lvExtentsCount);
    return lveBuffer;
}

IOReturn AppleLVMGroup::writeLogicalVolumeEntry(IOBufferMemoryDescriptor * lveBuffer, UInt64 offset)
{
    IOLog1("AppleLVMGroup::writeLogicalVolumeEntry(%p) lve offset %llu\n", this, offset);

    AppleLVMVolumeOnDisk * lve = (AppleLVMVolumeOnDisk *)lveBuffer->getBytesNoCopy();
    if (strncmp(lve->lvMagic, kAppleLVMVolumeMagic, sizeof(lve->lvMagic))) {
	lveBuffer->release();
	return NULL;
    }

#if defined(__LITTLE_ENDIAN__)
    AppleRAIDExtentOnDisk *extent = (AppleRAIDExtentOnDisk *)((char *)lve + lve->lvExtentsStart);
    UInt32 i;
    for (i=0; i < lve->lvExtentsCount; i++) {
	ByteSwapExtent(&extent[i]);
    }
    ByteSwapLVMVolumeHeader(lve);
#endif    
    
    AppleRAIDMember * member;
    UInt64 memberOffset;
    if (!memberOffsetFromLVGOffset(offset, &member, &memberOffset)) {
	return NULL;
    }

    return writeFromBuffer(member, lveBuffer, memberOffset);
}

bool AppleLVMGroup::clearLogicalVolumeEntry(UInt64 offset, UInt32 size)
{
    IOLog1("AppleLVMGroup::clearLogicalVolumeEntry(%p) lve offset %llu size %u\n", this, offset, (uint32_t)size);

    // Allocate a buffer to read into
    IOBufferMemoryDescriptor * lveBuffer = IOBufferMemoryDescriptor::withCapacity(size, kIODirectionNone);
    if (lveBuffer == 0) return NULL;

    AppleLVMVolumeOnDisk * lve = (AppleLVMVolumeOnDisk *)lveBuffer->getBytesNoCopy();
    bzero(lve, size);
    strncpy((char *)kAppleLVMVolumeFreeMagic, lve->lvMagic, sizeof(lve->lvMagic));

    AppleRAIDMember * member;
    UInt64 memberOffset;
    if (!memberOffsetFromLVGOffset(offset, &member, &memberOffset)) {
	return NULL;
    }
    IOReturn rc = writeFromBuffer(member, lveBuffer, memberOffset);

    lveBuffer->release();

    return rc == kIOReturnSuccess;
}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888


bool AppleLVMGroup::initializeSecondary(void)
{
    arExtentCount = 0;
    
    UInt64 startingOffset = 0;
    UInt32 index;
    for (index = 0; index < arMemberCount; index++) {

	AppleRAIDMember * member = arMembers[index];
	UInt64 memberDataSize = arMemberBlockCounts[index] * arSetBlockSize;
	if (!member || !memberDataSize) return false;

	member->setHeaderProperty(kAppleRAIDMemberStartKey, startingOffset, 64);

	arMemberStartingOffset[index] = startingOffset;

	// the first extent represents the secondary metadata (in case it gets fragmented)
	UInt64 secondaryOffset = startingOffset + member->getUsableSize() - member->getSecondarySize();
	UInt64 secondarySize = member->getSecondarySize();
	
	if (!secondaryOffset || !secondarySize) return false;
	IOBufferMemoryDescriptor * lveBuffer = readLogicalVolumeEntry(secondaryOffset, /* auto size */ 0);
	if (!lveBuffer) return false;
	AppleLVMVolumeOnDisk * lve = (AppleLVMVolumeOnDisk *)lveBuffer->getBytesNoCopy();
	
	// patch the secondary metadata lve to lvg relative
	AppleRAIDExtentOnDisk *extent = (AppleRAIDExtentOnDisk *)((char *)lve + lve->lvExtentsStart);
	UInt32 i;
	for (i=0; i < lve->lvExtentsCount; i++) {
	    extent[i].extentByteOffset = extent[i].extentByteOffset + startingOffset;

	    IOLog1("LVG initializeSecondary(%u) adding secondary offset %llu, size %llu\n",
		   (uint32_t)i, extent[i].extentByteOffset, extent[i].extentByteCount);
	}
	
	AppleLVMVolume * lv = AppleLVMVolume::withHeader(lve, NULL);
	if (!lv) return false;

	// add extents for secondary lve the logical volume group
	if (!lv->addExtents(this, lve)) return false;

	// track and later release master logical volumes
	arMetaDataVolumes[index] = lv;

	startingOffset += memberDataSize;
    }

    return true;
}

bool AppleLVMGroup::addExtentToLVG(AppleLVMLogicalExtent * extentToAdd)
{
    // adjacent extents are not combined here as they could
    // belong to different logical volumes

    IOLog1("AppleLVMGroup::addExtentToLVG(): new extent start %llu size %llu\n", extentToAdd->lvExtentGroupOffset, extentToAdd->lvExtentSize);
    
    // no entries
    if (!arExtents) {
	arExtents = extentToAdd;
	arExtentCount++;
	return true;
    }

    // first entry
    if ((extentToAdd->lvExtentGroupOffset + extentToAdd->lvExtentSize) <= arExtents->lvExtentGroupOffset) {
	extentToAdd->lvgNext = arExtents;
	arExtents = extentToAdd;
	arExtentCount++;
	return true;
    }

    // find the extent before and after the new extent
    AppleLVMLogicalExtent * extent = arExtents;
    AppleLVMLogicalExtent * nextExtent = extent->lvgNext;
    while (nextExtent) {
	if (nextExtent->lvExtentGroupOffset > extentToAdd->lvExtentGroupOffset) break;
	extent = nextExtent;
	nextExtent = nextExtent->lvgNext;
    }

    if (extent->lvExtentGroupOffset == extentToAdd->lvExtentGroupOffset) {
	IOLog("AppleLVMGroup::addExtentToLVG(): failed, the start of the new extent is already in use\n");
	return false;
    }

    if ((extent->lvExtentGroupOffset + extent->lvExtentSize) > extentToAdd->lvExtentGroupOffset) {
	IOLog("AppleLVMGroup::addExtentToLVG(): failed, the new extent overlaps the end of a current extent\n");
	return false;
    }

    if (nextExtent && ((extentToAdd->lvExtentGroupOffset + extentToAdd->lvExtentSize) > nextExtent->lvExtentGroupOffset)) {
	IOLog("AppleLVMGroup::addExtentToLVG(): failed, the new extent overlaps the beginning of a current extent\n");
	return false;
    }

    // new entry can be inserted after this entry
    extent->lvgNext = extentToAdd;
    extentToAdd->lvgNext = nextExtent;
    arExtentCount++;
    return true;
}

bool AppleLVMGroup::removeExtentFromLVG(AppleLVMLogicalExtent * extentToRemove)
{
    // are there any extents to remove?
    if (!arExtents) return false;

    // find the extent that contains this region
    AppleLVMLogicalExtent * prevExtent = 0;
    AppleLVMLogicalExtent * extent = arExtents;
    while (extent) {
	if (extent == extentToRemove) break;
	if (extent->lvExtentGroupOffset > extentToRemove->lvExtentGroupOffset) break;
	prevExtent = extent;
	extent = extent->lvgNext;
    }

    if (extent != extentToRemove) return false;

    if (prevExtent) {
	prevExtent->lvgNext = extent->lvgNext;
    } else {
	arExtents = extent->lvgNext;
    }
    extent->lvgNext = 0;
    arExtentCount--;
    return true;
}

bool AppleLVMGroup::buildExtentList(AppleRAIDExtentOnDisk * extentList)
{
    AppleLVMLogicalExtent * incoreExtent = arExtents;
    while (incoreExtent) {
	extentList->extentByteOffset = incoreExtent->lvExtentGroupOffset;
	extentList->extentByteCount = incoreExtent->lvExtentSize;

	IOLog1("LVG build extent at %lld, size %lld\n", extentList->extentByteOffset, extentList->extentByteCount);

	extentList++;
	incoreExtent = incoreExtent->lvgNext;
    }

    return true;
}

UInt64 AppleLVMGroup::calculateFreeSpace(void)
{
    AppleLVMLogicalExtent dummy;
    dummy.lvgNext = arExtents;
    dummy.lvExtentGroupOffset = dummy.lvExtentSize = 0;

    UInt64 freeSpace = 0;
    AppleLVMLogicalExtent * extent = &dummy;
    AppleLVMLogicalExtent * next;
    while ((next = extent->lvgNext)) {

	freeSpace += next->lvExtentGroupOffset - (extent->lvExtentGroupOffset + extent->lvExtentSize);

	extent = next;
    }

    return freeSpace;
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888


bool AppleLVMGroup::addLogicalVolumeToTOC(AppleLVMVolume * lv)
{
    // find empty index for this volume
    UInt32 index = 0;
    while (index < arLogicalVolumeCount) {
	if (!arLogicalVolumes[index]) break;
	index++;
    }
    if (index == arLogicalVolumeCount) arLogicalVolumeCount++;

    arLogicalVolumeActiveCount++;
    assert(arLogicalVolumeActiveCount <= arLogicalVolumeCount);

    arLogicalVolumes[index] = lv;
    lv->retain();

    lv->setIndex(index);
    
    return true;
}

bool AppleLVMGroup::removeLogicalVolumeFromTOC(AppleLVMVolume * lv)
{
    UInt32 index = lv->getIndex();

    if (arLogicalVolumes[index] == lv) {

	arLogicalVolumes[index] = NULL;
	lv->release();

	if (index == arLogicalVolumeCount) arLogicalVolumeCount--;
	arLogicalVolumeActiveCount--;
	assert(arLogicalVolumeActiveCount <= arLogicalVolumeCount);

	return true;
    }

    IOLog("AppleLVMGroup::removeLogicalVolumeFromTOC() failed for lv %p\n", lv);

    return false;
}


OSArray * AppleLVMGroup::buildLogicalVolumeListFromTOC(AppleRAIDMember * member)
{
    UInt32 index;
    OSArray * array = OSArray::withCapacity(arLogicalVolumeCount);
    if (!array) return NULL;

    for (index = 0; index < arLogicalVolumeCount; index++) {

	AppleLVMVolume * lv = arLogicalVolumes[index];
	if (!lv) continue;

	if (member && !lv->hasExtentsOnMember(member)) continue;

	const OSString * uuid = lv->getVolumeUUID();
	if (uuid) {
	    array->setObject(uuid);
	}
    }
    return array;
}

bool AppleLVMGroup::initializeVolumes(AppleRAIDPrimaryOnDisk * primary)
{
    AppleLVGTOCEntryOnDisk * firstEntry = (AppleLVGTOCEntryOnDisk *)((char *)primary + sizeof(AppleRAIDPrimaryOnDisk));

    UInt32 index;
    for (index = 0; index < arLogicalVolumeCount; index++) {

	AppleLVGTOCEntryOnDisk * tocEntry = &firstEntry[index];

	UInt64 lveOffset = tocEntry->lveEntryOffset;
	UInt64 lveSize = tocEntry->lveEntrySize;
	if (!lveOffset || !lveSize) continue;		// empty entries are zeroed

	IOLog1("AppleLVMGroup::initializeVolumes: reading volume[%u] %s, size %llu\n", (uint32_t)index, tocEntry->lveUUID, tocEntry->lveVolumeSize);
	
	IOBufferMemoryDescriptor * lveBuffer = readLogicalVolumeEntry(lveOffset, lveSize);
	if (!lveBuffer) continue;

	AppleLVMVolumeOnDisk * lve = (AppleLVMVolumeOnDisk *)lveBuffer->getBytesNoCopy();
	assert(lve);

	AppleLVMVolume * lv = AppleLVMVolume::withHeader(lve, NULL);
	if (!lv) {
	    lveBuffer->release();
	    continue;
	}

	lv->setIndex(index);
	lv->setEntryOffset(lveOffset);

	// add extents to the logical volume group
	if (!lv->addExtents(this, lve)) {
	    // XXX set volume status to offline, need check other calls to addExtents
	}
	lveBuffer->release();
	
	arLogicalVolumes[index] = lv;    // no release
	arLogicalVolumeActiveCount++;
	assert(arLogicalVolumeActiveCount <= arLogicalVolumeCount);

	arController->addLogicalVolume(lv);
    }

    // rescan list to enable snapshots
    for (index = 0; index < arLogicalVolumeCount; index++) {

	AppleLVMVolume * lv = arLogicalVolumes[index];
	if (!lv) continue;

	AppleLVMVolume * parent = NULL;;
	const OSString * parentUUID = lv->getParentUUID();
	if (parentUUID) parent = arController->findLogicalVolume(parentUUID);
	if (parent) {
	    lv->setParent(parent);
	    if (lv->isABitMap()) parent->setBitMap(lv);
	    if (lv->isASnapShot()) parent->setSnapShot(lv);
	}
    }

    return true;
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888


// this was lifted from osfmk/kern/bits.c, there are also some better looking asm files
// XXX this code sucks and needs a serious rewrite.


#include <mach/machine/vm_param.h>	/* for BYTE_SIZE */

#define INT_SIZE (BYTE_SIZE * sizeof (UInt32))

/*
 * Set indicated bit in bit string.
 */
static void
setbit(UInt32 bitno, UInt32 *s)
{
	for ( ; INT_SIZE <= bitno; bitno -= INT_SIZE, ++s)
		;
	*s |= 1 << bitno;
}

/*
 * Clear indicated bit in bit string.
 */
static void
clrbit(UInt32 bitno, UInt32 *s)
{
	for ( ; INT_SIZE <= bitno; bitno -= INT_SIZE, ++s)
		;
	*s &= ~(1 << bitno);
}

/*
 * Find first bit set in bit string.
 */
static UInt32
ffsbit(UInt32 *s)
{
	UInt32 offset, mask;

	for (offset = 0; !*s; offset += INT_SIZE, ++s)
		;
	for (mask = 1; mask; mask <<= 1, ++offset)
		if (mask & *s)
			return (offset);
	/*
	 * Shouldn't get here
	 */
	return (0);
}

/*
 * Test if indicated bit is set in bit string.
 */
//static UInt32
//testbit(UInt32 bitno, UInt32 *s)
//{
//	for ( ; INT_SIZE <= bitno; bitno -= INT_SIZE, ++s)
//		;
//	return(*s & (1 << bitno));
//}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888


// it would be nice to do this at user level but we already have most this cached in kernel

UInt64 AppleLVMGroup::findFreeLVEOffset(AppleLVMVolume * lvNew)
{
    // find the member that this should this go on
    AppleLVMLogicalExtent * firstExtent = lvNew->findExtent(0);
    if (!firstExtent) return 0;
    UInt32 lveMemberIndex = firstExtent->lvMemberIndex;

    IOLog1("AppleLVMGroup::findFreeLVEOffset(): member %u selected\n", (uint32_t)lveMemberIndex);

    // find secondary range for that member
    AppleRAIDMember * member = arMembers[lveMemberIndex];
    if (!member) return 0;
    UInt64 secondaryStart = member->getUsableSize() - member->getSecondarySize();
    UInt64 secondaryEnd = member->getUsableSize();

    IOLog1("AppleLVMGroup::findFreeLVEOffset(): secondaryStart = %llu\n", secondaryStart);
    IOLog1("AppleLVMGroup::findFreeLVEOffset(): secondaryEnd   = %llu\n", secondaryEnd);
    
    // allocate temp bitmap  (1024 volumes / 8 bits/byte -> 256 bytes)
    UInt32 bitmapSize = member->getSecondarySize() / kAppleLVMVolumeOnDiskMinSize / 32;
    UInt32 * bitmap = IONew(UInt32, bitmapSize + 1);
    if (!bitmap) return 0;
    bzero(bitmap, bitmapSize * sizeof(UInt32));
    bitmap[bitmapSize] = 0x55555555;  // the end (backwards)

    IOLog1("AppleLVMGroup::findFreeLVEOffset(): bitmap %p, size %u\n", bitmap, (uint32_t)bitmapSize * 32);

    // add in metadata lve
    UInt32 bitIndex = 0;
    UInt32 entrySize = arMetaDataVolumes[lveMemberIndex]->getEntrySize() / kAppleLVMVolumeOnDiskMinSize;
    while (entrySize--) {
	setbit(bitIndex, bitmap);
	bitIndex++;
    }

    // build the free list bitmap
    UInt32 i;
    for (i = 0; i < arLogicalVolumeCount; i++) {
	AppleLVMVolume * lv = arLogicalVolumes[i];
	if (lv) {
	    if (lv == lvNew)				continue;
	    if (lv->getEntryOffset() < secondaryStart)	continue;
	    if (lv->getEntryOffset() > secondaryEnd)	continue;

	    // got one, substract it from free list bitmap
	    bitIndex = (lv->getEntryOffset() - secondaryStart) / kAppleLVMVolumeOnDiskMinSize;

	    assert(bitIndex < bitmapSize * 32);
	    if (bitIndex >= bitmapSize * 32) goto error;

	    entrySize = lv->getEntrySize();
	    assert(entrySize % kAppleLVMVolumeOnDiskMinSize == 0);
	    if (entrySize % kAppleLVMVolumeOnDiskMinSize) goto error;
	    
	    entrySize /= kAppleLVMVolumeOnDiskMinSize;
	    while (entrySize--) {
		setbit(bitIndex, bitmap);
		bitIndex++;
	    }
	}
    }

    IOLog1("AppleLVMGroup::findFreeLVEOffset(): bitmap (31-0) 0x%08x 0x%08x 0x%08x 0x%08x\n", (uint32_t)bitmap[0], (uint32_t)bitmap[1], (uint32_t)bitmap[2], (uint32_t)bitmap[3]);

    {
	UInt32 bitsNeeded = lvNew->getEntrySize() / kAppleLVMVolumeOnDiskMinSize;
	UInt32 lastBit = bitmapSize * 32;

	IOLog1("AppleLVMGroup::findFreeLVEOffset(): bitsNeeded %u, lastBit %u\n", (uint32_t)bitsNeeded, (uint32_t)lastBit);
    
	UInt32 firstBit = ffsbit(bitmap);
	if (firstBit >= lastBit) goto full;
	clrbit(firstBit, bitmap);

	UInt32 nextBit = ffsbit(bitmap);
	clrbit(nextBit, bitmap);

	UInt32 bitsGap = nextBit - firstBit - 1;
	while (bitsGap < bitsNeeded) {

	    firstBit = nextBit;
	    if (firstBit >= lastBit) goto full;
	
	    nextBit = ffsbit(bitmap);
	    clrbit(nextBit, bitmap);
	    bitsGap = nextBit - firstBit - 1;
	}

	IOLog1("AppleLVMGroup::findFreeLVEOffset(): firstBit %u, nextBit %u\n", (uint32_t)firstBit, (uint32_t)nextBit);

	if (bitsGap >= bitsNeeded) return (firstBit + 1) * kAppleLVMVolumeOnDiskMinSize + secondaryStart;
    }
	
full:

    IOLog("AppleLVMGroup::findFreeLVEOffset(): there is no room for more volume entries\n");

    // XXXTOC should have code to compact the logical volume entries
    // XXXTOC should have code to allocate a larger secondary metadata area

error:
    if (bitmap) IODelete(bitmap, UInt32, bitmapSize + 1);

    return 0;
}


bool AppleLVMGroup::updateLVGTOC(void)
{
    if (!arPrimaryBuffer) return false;
    
    AppleRAIDPrimaryOnDisk * header = (AppleRAIDPrimaryOnDisk *)arPrimaryBuffer->getBytesNoCopy();
    header->priSequenceNumber = arSequenceNumber;
    header->pri.volumeCount = arLogicalVolumeCount;
    header->priUsed = sizeof(AppleRAIDPrimaryOnDisk);
    header->priUsed += sizeof(AppleLVGTOCEntryOnDisk) * arLogicalVolumeCount;

    if (header->priUsed > header->priSize) return false;   // XXXTOC the header has already been messed with
							   // so this is basically a fatal condition

    if (header->priSize < arPrimaryBuffer->getLength()) {

	// XXXTOC  need to resize the buffer

	// vm_size_t IOBufferMemoryDescriptor::getCapacity() const
	// IOBufferMemoryDescriptor::appendBytes(const void * bytes, vm_size_t withLength)
	// void IOBufferMemoryDescriptor::setLength(vm_size_t length)
	
	return false;
    }

    AppleLVGTOCEntryOnDisk * firstEntry = (AppleLVGTOCEntryOnDisk *)((char *)header + sizeof(AppleRAIDPrimaryOnDisk));

    UInt32 i;
    for (i = 0; i < arLogicalVolumeCount; i++) {

	AppleLVGTOCEntryOnDisk * tocEntry = &firstEntry[i];
	AppleLVMVolume * lv = arLogicalVolumes[i];
	if (lv) {
	    strncpy(tocEntry->lveUUID, (char *)lv->getVolumeUUIDString(), sizeof(tocEntry->lveUUID));
	    tocEntry->lveVolumeSize = lv->getClaimedSize();
	    tocEntry->lveEntryOffset = lv->getEntryOffset();
	    tocEntry->lveEntrySize = lv->getEntrySize();
	    bzero(&tocEntry->reserved, sizeof(tocEntry->reserved));
	} else {
	    bzero(tocEntry, sizeof(AppleLVGTOCEntryOnDisk));
	}
    }

    return true;
}


IOReturn AppleLVMGroup::createLogicalVolume(OSDictionary * lveProps, AppleLVMVolumeOnDisk * lve)
{
    const OSString * parentUUID;
    AppleLVMVolume * parent;
    IOReturn rc = 0;
    UInt64 lveOffset = 0;
    IOBufferMemoryDescriptor * lveBuffer = NULL;


    // make sure the sequence number is not out of date
    UInt32 sequenceNumber = 0;
    OSNumber * number = OSDynamicCast(OSNumber, lveProps->getObject(kAppleLVMVolumeSequenceKey));
    if (number) sequenceNumber = number->unsigned32BitValue();
    if (!sequenceNumber || sequenceNumber != arSequenceNumber) {
	IOLog("AppleLVMGroup::createLogicalVolume() sequenceNumber mismatch (%u) for group %s (%u)\n",
	      (uint32_t)sequenceNumber, getUUIDString(), (uint32_t)arSequenceNumber);
	return kIOReturnBadArgument;
    }
    
    // create logical volume object, set up lv extents
    AppleLVMVolume * lv = AppleLVMVolume::withHeader(lve, lveProps);
    if (!lv) { rc = kIOReturnBadArgument; goto error; }

    // add extents to the logical volume group
    if (!lv->addExtents(this, lve)) { rc = kIOReturnInternalError; goto error; }
    
    // find empty slot to write the lve
    lveOffset = findFreeLVEOffset(lv);
    if (!lveOffset) return kIOReturnInternalError;
    IOLog1("AppleLVMGroup::createLogicalVolume() writing logical volume entry for %s at loffset %llu\n", lv->getVolumeUUIDString(), lveOffset);

    // update lv
    lv->setEntryOffset(lveOffset);

    // write entry to disk
    lveBuffer = IOBufferMemoryDescriptor::withBytes(lve, lve->lvHeaderSize, kIODirectionOut);
    rc = writeLogicalVolumeEntry(lveBuffer, lveOffset);
    lveBuffer->release();
    if (rc) goto error;

    // add to internal lists
    addLogicalVolumeToTOC(lv);
    arController->addLogicalVolume(lv);

    bumpSequenceNumber();

    if (!updateLVGTOC()) goto error;
    rc = writePrimaryMetaData(arPrimaryBuffer);
    if (rc) goto error;
    
    // update raid header
    writeRAIDHeader();

    if (lv->isAVolume() && !publishVolume(lv)) {
	rc = kIOReturnError;   // XXX better error ?
	goto error;
    }

//    IOSleep(1000);  // uncomment for 4943003 a "#1 bug"

    parent = NULL;
    parentUUID = lv->getParentUUID();
    if (parentUUID) parent = arController->findLogicalVolume(parentUUID);
    if (parent) {

	lv->setParent(parent);
    
	if (lv->isABitMap()) {
	   if (!lv->zeroVolume()) {
		rc = kIOReturnError;   // XXX better error ?
		goto error;
	   }
	   parent->setBitMap(lv);
	}

	if (lv->isASnapShot()) {
	   parent->setSnapShot(lv);
	}
    }

    lv->release();  // keep here

    return kIOReturnSuccess;

error:

    if (lv) lv->release();
    return rc;
}


IOReturn AppleLVMGroup::updateLogicalVolume(AppleLVMVolume * lv, OSDictionary * lveProps, AppleLVMVolumeOnDisk * lve)
{
    // make sure the sequence number is not out of date
    UInt32 sequenceNumber = 0;
    OSNumber * number = OSDynamicCast(OSNumber, lveProps->getObject(kAppleLVMVolumeSequenceKey));
    if (number) sequenceNumber = number->unsigned32BitValue();
    if (!sequenceNumber || sequenceNumber != arSequenceNumber) {
	IOLog("AppleLVMGroup::updateLogicalVolume() sequenceNumber mismatch (%u) for group %s (%u)\n",
	      (uint32_t)sequenceNumber, getUUIDString(), (uint32_t)arSequenceNumber);
	return kIOReturnBadArgument;
    }

    // XXX pause set?  pause volume

    if (!lv->initWithHeader(lveProps)) {
	return kIOReturnBadArgument;
    }

    // refresh extent lists
    lv->removeExtents(this);
    if (!lv->addExtents(this, lve)) return kIOReturnInternalError;

    // XXXTOC need to double check that the entry is still the same size
    // or find a new place to put it or just always find a new place

    // find the offset for this lve
    UInt64 lveOffset = lv->getEntryOffset();

    // write entry to disk
    IOBufferMemoryDescriptor * lveBuffer = IOBufferMemoryDescriptor::withBytes(lve, lve->lvHeaderSize, kIODirectionOut);
    IOReturn rc = writeLogicalVolumeEntry(lveBuffer, lveOffset);
    lveBuffer->release();
    if (rc) goto error;

    bumpSequenceNumber();

    if (!updateLVGTOC()) goto error;
    rc = writePrimaryMetaData(arPrimaryBuffer);
    if (rc) goto error;
    
    // update raid header
    writeRAIDHeader();
    
    if (lv->isAVolume() && !publishVolume(lv)) {
	return kIOReturnError;   // XXX better error ?
    }

    lv->messageClients(kAppleLVMMessageVolumeChanged);

    return kIOReturnSuccess;

error:

    return rc;
}


IOReturn AppleLVMGroup::destroyLogicalVolume(AppleLVMVolume * lv)
{
    
    //XXX  set state to offline, block and drain i/o

    AppleLVMVolume * parent = lv->parentVolume();
    if (parent && lv->isASnapShot()) parent->setSnapShot(NULL);
    if (parent && lv->isABitMap()) parent->setBitMap(NULL);

    AppleLVMVolume * snapshot = lv->snapShotVolume();
    if (snapshot) {
	lv->setSnapShot(NULL);
	destroyLogicalVolume(snapshot);
    }

    AppleLVMVolume * bitmap = lv->bitMapVolume();
    if (bitmap) {
	lv->setBitMap(NULL);
	destroyLogicalVolume(bitmap);
    }

    // clear lv entry on the disk
    UInt64 lveOffset = lv->getEntryOffset();
    UInt32 lveSize = lv->getEntrySize();
    IOReturn rc = clearLogicalVolumeEntry(lveOffset, lveSize);
    if (rc) {
	// log an error, keep going
    }

    // terminate logical volume
    if (lv->isPublished()) (void)unpublishVolume(lv);

    // remove extents from global list   (XXX do this from terminate on lv object?)
    lv->removeExtents(this);

    // clear the TOC entry
    if (!removeLogicalVolumeFromTOC(lv)) return kIOReturnError;

    arController->removeLogicalVolume(lv);

    bumpSequenceNumber();

    if (!updateLVGTOC()) return kIOReturnInternalError;
    rc = writePrimaryMetaData(arPrimaryBuffer);
    if (rc) return rc;

    // update raid header
    writeRAIDHeader();

    return kIOReturnSuccess;
}


bool AppleLVMGroup::publishVolume(AppleLVMVolume * lv)
{
    IOLog1("AppleLVMGroup::publishVolume called %p, volume = %p\n", this, lv);

    // are we (still) connected to the io registry?
    if (arActiveCount == 0) {
	IOLog1("AppleLVMGroup::publishVolume: the set %p is empty, aborting.\n", this);
	return false;
    }

    if (!lv->getSequenceNumber() || lv->getSequenceNumber() > getSequenceNumber()) {
	// XXX set lv state to broken?
	return false;
    }

    if (!lv->isAVolume()) {
	IOLog1("AppleLVMGroup::publishVolume: ignoring %p it is not a volume.\n", lv);
	return true;
    }

    // Create the member object for the raid set.
    UInt64 volumeSize = lv->getClaimedSize();
    bool isWritable = (lv->getTypeID() == kLVMTypeSnapRO) ? false : arIsWritable;
    const OSString * theHint = lv->getHint();
    const char * contentHint = 0;
    if (theHint) contentHint = theHint->getCStringNoCopy();

    IOMediaAttributeMask attributes = arIsEjectable ? (kIOMediaAttributeEjectableMask | kIOMediaAttributeRemovableMask) : 0;
    if (lv->init(/* base               */ 0,
		 /* size               */ volumeSize,
		 /* preferredBlockSize */ arNativeBlockSize,
		 /* attributes         */ attributes,
		 /* isWhole            */ true,
		 /* isWritable         */ isWritable,
		 /* contentHint        */ contentHint)) {
	    
	lv->setName(getSetNameString());

	// Set a location value (partition number) for this partition.   XXX this doesn't work
	// IOMediaBSDClient::getWholeMedia makes assumptions about the
	// ioreg layout, this tries to hack around that.
	char location[12];
	snprintf(location, sizeof(location), "%ss%d", getLocation(), (uint32_t)lv->getIndex());
	lv->setLocation(location);
	    
	OSArray * bootArray = OSArray::withCapacity(arMemberCount);
	if (bootArray) {
	    // if any of the devices are not in the device tree
	    // just return an empty array
	    (void)addBootDeviceInfo(bootArray);
	    lv->setProperty(kIOBootDeviceKey, bootArray);
	    bootArray->release();
	}

	lv->setProperty(kIOMediaUUIDKey, (OSObject *)lv->getVolumeUUID());
	lv->setProperty(kAppleLVMIsLogicalVolumeKey, kOSBooleanTrue);

	if (getSetState() < kAppleRAIDSetStateOnline) {
	    lv->setProperty("autodiskmount", kOSBooleanFalse);
	} else {
	    lv->removeProperty("autodiskmount");
	}

	if (!lv->isPublished()) {
	    lv->attach(this);
	    lv->registerService();
	    lv->setPublished(true);
	} else {
	    lv->messageClients(kIOMessageServicePropertyChange);
	}
    }

    IOLog1("AppleLVMGroup::publishVolume: was successful for %p.\n", lv);
    
    return true;
}


bool AppleLVMGroup::unpublishVolume(AppleLVMVolume * lv)
{
    IOLog1("AppleLVMGroup::unpublishSet(%p) entered, volume = %p\n", this, lv);

    lv->setPublished(false);

    return lv->terminate(kIOServiceRequired | kIOServiceSynchronous);
}


void AppleLVMGroup::completeRAIDRequest(AppleRAIDStorageRequest * storageRequest)
{
    UInt32		cnt;
    UInt64              byteCount;
    IOReturn            status;
    bool		isWrite;

    // this is running in the workloop, via a AppleRAIDEvent
    
    isWrite = (storageRequest->srMemoryDescriptorDirection == kIODirectionOut);
    byteCount = 0;
    status = kIOReturnSuccess;

    // Collect the status and byte count for each request
    for (cnt = 0; cnt < storageRequest->srRequestCount; cnt++) {

        // Return any status errors.
        if (storageRequest->srRequestStatus[cnt] != kIOReturnSuccess) {
            status = storageRequest->srRequestStatus[cnt];
            byteCount = 0;

	    AppleRAIDMemoryDescriptor *  memoryDescriptor = storageRequest->srMemoryDescriptors[cnt];
	    AppleRAIDMember * member = arMembers[memoryDescriptor->mdMemberIndex];

	    if (!member) continue;  // XXX should have already logged an error
	    
	    IOLog("AppleLVMGroup::completeRAIDRequest - error 0x%x detected for set \"%s\" (%s), member %s, near lv byte offset = %llu.\n",
		  status, getSetNameString(), getUUIDString(), member->getUUIDString(), storageRequest->srByteStart);

	    // mark this member to be removed
	    member->changeMemberState(kAppleRAIDMemberStateClosing);
	    continue;
	}

	byteCount += storageRequest->srRequestByteCounts[cnt];

	IOLogRW("AppleLVMGroup::completeRAIDRequest - [%u] tbc 0x%llx, sbc 0x%llx bc 0x%llx, member %p\n",
		(uint32_t)cnt, storageRequest->srByteCount, storageRequest->srRequestByteCounts[cnt],
		byteCount, arMembers[cnt]);
    }

    // XXX before checking for an underrun, first check to see if we need to schedule more i/o
    // the check below should probably be checking the expected bytes for a partial i/o
    
    // Return an underrun error if the byte count is not complete.
    // This can happen if one or more members reported a smaller byte count.
    if ((status == kIOReturnSuccess) && (byteCount != storageRequest->srByteCount)) {
	IOLog("AppleLVMGroup::completeRAIDRequest - underrun detected, expected = 0x%llx, actual = 0x%llx, set = \"%s\" (%s)\n",
	      storageRequest->srByteCount, byteCount, getSetNameString(), getUUIDString());
        status = kIOReturnUnderrun;
        byteCount = 0;
    }

    storageRequest->srMemoryDescriptor->release();
    returnRAIDRequest(storageRequest);
        
    // Call the clients completion routine, bad status is also returned here.
    IOStorage::complete(&storageRequest->srClientsCompletion, status, byteCount);
}
