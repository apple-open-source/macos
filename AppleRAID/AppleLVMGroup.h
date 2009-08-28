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


#ifndef _APPLELVMGROUP_H
#define _APPLELVMGROUP_H

#define kAppleRAIDLevelNameLVG "LVG"

// The LVG TOC is part of the primary meta data and is duplicated across all disks.
// The primary meta data has a sequence number as do the logical volume enties
// and the raid header.  The order of writing these out should be first the logical
// volume entry, then the table of contents and finally the raid header.  Any structure
// with sequence number that is higher than a raid header sequence number can be
// assumed to be part of a failed update to the logical volume group.

typedef struct AppleLVGTOCEntryOnDisk {

    char			lveUUID[64];		// XXX lve -> toc
    UInt64			lveVolumeSize;
    UInt64			lveEntryOffset;
    UInt64			lveEntrySize;
    char			reserved[40];		// 64 + 8 + 8 + 8 + 40 = 128

} AppleLVGTOCEntryOnDisk;

#define AppleLVGTOCEntrySanityCheck()	assert(sizeof(AppleLVGTOCEntryOnDisk) == 128);

#define ByteSwapLVGTOCEntry(entry) \
{ \
        (entry)->lveVolumeSize		= OSSwapBigToHostInt64((entry)->lveVolumeSize); \
        (entry)->lveEntryOffset		= OSSwapBigToHostInt64((entry)->lveEntryOffset); \
        (entry)->lveEntrySize		= OSSwapBigToHostInt32((entry)->lveEntrySize); \
}

#ifdef KERNEL


class AppleLVMGroup : public AppleRAIDSet
{
    OSDeclareDefaultStructors(AppleLVMGroup);

    friend class AppleRAIDStorageRequest;
    
 private:
    UInt64 *				arMemberBlockCounts;
    UInt64 *				arMemberStartingOffset;
    AppleLVMVolume **			arMetaDataVolumes;
    UInt32				arExpectingLiveAdd;
    
    bool				arPrimaryNeedsUpdate;
    IOBufferMemoryDescriptor *		arPrimaryBuffer;
    UInt32				arLogicalVolumeCount;
    UInt32				arLogicalVolumeActiveCount;
    AppleLVMVolume **			arLogicalVolumes;
    
    UInt64				arExtentCount;
    AppleLVMLogicalExtent *		arExtents;

 protected:
    virtual bool init();
    virtual void free();

    virtual void unpauseSet(void);
    virtual bool updateLVGTOC(void);
    virtual UInt64 findFreeLVEOffset(AppleLVMVolume * newLV);
    
 public:
    static AppleRAIDSet * createRAIDSet(AppleRAIDMember * firstMember);
    virtual bool addSpare(AppleRAIDMember * member);
    virtual bool addMember(AppleRAIDMember * member);
    virtual bool removeMember(AppleRAIDMember * member, IOOptionBits options);

    virtual bool resizeSet(UInt32 newMemberCount);
    virtual bool startSet(void);
    virtual bool publishSet(void);
    virtual bool unpublishSet(void);
    virtual bool handleOpen(IOService * client, IOOptionBits options, void * access);

    virtual UInt32 getMaxRequestCount(void) const { return 16; };   // XXX 32bit / min lv size + 1
    
    virtual UInt64 getMemberSize(UInt32 memberIndex) const;
    virtual UInt64 getMemberStartingOffset(UInt32 memberIndex) const;
    virtual UInt32 getMemberIndexFromOffset(UInt64 offset) const;
    virtual bool memberOffsetFromLVGOffset(UInt64 lvgOffset, AppleRAIDMember ** member, UInt64 * memberOffset);
    virtual OSDictionary * getSetProperties(void);

    virtual AppleRAIDMemoryDescriptor * allocateMemoryDescriptor(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex);

    // LVG stuff
    
    virtual IOBufferMemoryDescriptor * readPrimaryMetaData(AppleRAIDMember * member);
    virtual IOReturn writePrimaryMetaData(IOBufferMemoryDescriptor * primaryBuffer);

    virtual IOBufferMemoryDescriptor * readLogicalVolumeEntry(UInt64 offset, UInt32 size);
    virtual IOReturn writeLogicalVolumeEntry(IOBufferMemoryDescriptor * lveBuffer, UInt64 offset);
    virtual bool clearLogicalVolumeEntry(UInt64 offset, UInt32 size);
    
    virtual bool initializeSecondary(void);
    virtual bool addExtentToLVG(AppleLVMLogicalExtent * newExtent);
    virtual bool removeExtentFromLVG(AppleLVMLogicalExtent * extent);
    virtual bool buildExtentList(AppleRAIDExtentOnDisk * onDiskExtent);
    virtual UInt64 calculateFreeSpace(void);

    inline UInt64 getExtentCount(void) const	{ return arExtentCount; };
    
    virtual bool addLogicalVolumeToTOC(AppleLVMVolume * lv);
    virtual bool removeLogicalVolumeFromTOC(AppleLVMVolume * lv);
    virtual OSArray * buildLogicalVolumeListFromTOC(AppleRAIDMember * member);
    virtual bool initializeVolumes(AppleRAIDPrimaryOnDisk * primary);

    virtual IOReturn createLogicalVolume(OSDictionary * lveProps, AppleLVMVolumeOnDisk * lvOnDisk);
    virtual IOReturn updateLogicalVolume(AppleLVMVolume * lv, OSDictionary * lveProps, AppleLVMVolumeOnDisk * lve);
    virtual IOReturn destroyLogicalVolume(AppleLVMVolume * lv);

    virtual bool publishVolume(AppleLVMVolume * lv);
    virtual bool unpublishVolume(AppleLVMVolume * lv);

    virtual void completeRAIDRequest(AppleRAIDStorageRequest * storageRequest);
};



class AppleLVMMemoryDescriptor : public AppleRAIDMemoryDescriptor
{
    OSDeclareDefaultStructors(AppleLVMMemoryDescriptor);
    
    friend class AppleRAIDEventSource;		// XXX remove this
    friend class AppleLVMGroup;			// XXX remove this
    
 private:
    UInt64		mdRequestIndex;
    IOByteCount		mdRequestOffset;
    
 protected:
    virtual bool initWithStorageRequest(AppleRAIDStorageRequest * storageRequest, UInt32 requestIndex);
    virtual bool configureForMemoryDescriptor(IOMemoryDescriptor * memoryDescriptor, UInt64 requestStart, UInt64 requestSize, AppleLVMVolume * lv);
    
 public:
    static AppleRAIDMemoryDescriptor * withStorageRequest(AppleRAIDStorageRequest * storageRequest, UInt32 memberIndex);
    virtual addr64_t getPhysicalSegment(IOByteCount offset, IOByteCount * length, IOOptionBits options = 0);
};


#endif KERNEL

#endif /* ! _APPLELVMGROUP_H */
