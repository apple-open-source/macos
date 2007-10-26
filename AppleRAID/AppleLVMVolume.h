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


#ifndef _APPLELVMVOLUME_H
#define _APPLELVMVOLUME_H

#define kAppleLVMVolumeMagic		"AppleLVMVolume"
#define kAppleLVMVolumeNextMagic	"AppleLVMVolumeNext"
#define kAppleLVMVolumeFreeMagic	"AppleLVMVolumeFree"

#define kAppleLVMVolumeOnDiskMinSize	0x1000			// 4K

typedef struct AppleLVMVolumeOnDisk
{
    char				lvMagic[32];		// Logical Volume { Header, Next, Free }
    UInt32				lvHeaderSize;		// total structure size 4k
    UInt32				lvExtentsStart;		// offset to first extent (after plist)
    UInt32				lvExtentsCount;		// number of extents (also in plist)
    char				reserved[20];		// 32 + 4 + 4 + 4 + 20 = 64
    char				plist[];
} AppleLVMVolumeOnDisk;

#define AppleLVMVolumeOnDiskSanityCheck()	assert(sizeof(AppleLVMVolumeOnDisk) == 64);

#define ByteSwapLVMVolumeHeader(header) \
{ \
        (header)->lvHeaderSize		= OSSwapBigToHostInt32((header)->lvHeaderSize); \
        (header)->lvExtentsStart	= OSSwapBigToHostInt32((header)->lvExtentsStart); \
        (header)->lvExtentsCount	= OSSwapBigToHostInt32((header)->lvExtentsCount); \
}	// also need to swap extents

#ifdef KERNEL

#define kAppleLVMSkipListSize 4		// keep even for alignment

typedef struct AppleLVMLogicalExtent
{
    AppleLVMLogicalExtent *	skip[kAppleLVMSkipListSize];	// skip list next pointers
    AppleLVMLogicalExtent *	lvgNext;			// global (within lvg) list
    UInt64			lvExtentVolumeOffset;		// offset within the logical volume
    UInt64			lvExtentSize;			// size of the extent

    UInt64			lvExtentGroupOffset;		// offset within the logical volume group
    UInt64			lvExtentMemberOffset;		// offset within the member
    UInt32			lvMemberIndex;			// member that holds this logical extent
} AppleLVMLogicalExtent;

enum {
    kLVMTypeConcat		=	0x1,
    kLVMTypeStripe		=	0x2,
    kLVMTypeMirror		=	0x4,
    kLVMTypeIsAVolume		=	0x7,
    
    kLVMTypeSnapRO		=	0x10,
    kLVMTypeSnapRW		=	0x20,
    kLVMTypeIsASnapShot		=	0x30,
    kLVMTypeBitMap		=	0x40,
    kLVMTypeMaster		=	0x80
};

class AppleLVMGroup;

class AppleLVMVolume : public IOMedia
{
    OSDeclareDefaultStructors(AppleLVMVolume)
	
 protected:

    UInt32				lvIndex;		// index in LVG TOC
    OSString *				lvUUID;			
    UInt32				lvSequenceNumber;	// must be less than lvg sequence number
    UInt64				lvClaimedSize;		// size in header
    UInt64				lvCalculatedSize;	// sum of extents
    UInt32				lvTypeID;
    UInt64				lvEntryOffset;
    UInt32				lvEntrySize;

    OSDictionary *			lvProps;

    UInt64				lvExtentCount;		// how many extents
    AppleLVMLogicalExtent *		lvExtent[kAppleLVMSkipListSize];

    bool				lvPublished;

    AppleLVMVolume *			lvParent;
    AppleLVMVolume *			lvSnapShot;
    AppleLVMVolume *			lvBitMap;
    
 protected:
    virtual bool init(void);

    virtual AppleLVMLogicalExtent * addExtent(AppleLVMGroup * lvg, AppleRAIDExtentOnDisk * extent);

 public:
    using IOMedia::init;   // why?  
    
    static OSDictionary *   propsFromHeader(AppleLVMVolumeOnDisk * lve);
    static AppleLVMVolume * withHeader(AppleLVMVolumeOnDisk * lve, OSDictionary * lveProps = 0);
    virtual bool initWithHeader(OSDictionary * lveProps);
    virtual void free();

    virtual OSDictionary * getVolumeProperties(void);
    
    // from disk only set from user space
    virtual const OSString * getVolumeUUID(void);
    virtual const char * getVolumeUUIDString(void);
    virtual const OSString * getGroupUUID(void);
    virtual const OSString * getParentUUID(void);
    virtual const OSString * getHint(void);

    inline UInt64 getClaimedSize(void)		{ return lvClaimedSize; };
    inline UInt64 getExtentCount(void) const	{ return lvExtentCount; };
    inline UInt32 getSequenceNumber(void) const { return lvSequenceNumber; };
    inline UInt32 getEntrySize(void) const	{ return lvEntrySize; };

    // not in lv disk properties
    virtual const OSString * getDiskName(void);
    inline UInt32 getIndex(void) const		{ return lvIndex; };
    inline void setIndex(UInt32 index)		{ lvIndex = index; };
    inline UInt64 getEntryOffset(void) const	{ return lvEntryOffset; };
    inline void setEntryOffset(UInt64 offset)	{ lvEntryOffset = offset; };
    inline bool isPublished(void) const		{ return lvPublished; };
    inline void setPublished(bool published)	{ lvPublished = published; };

    virtual bool addExtents(AppleLVMGroup * lvg, AppleLVMVolumeOnDisk * lve);
    virtual bool removeExtents(AppleLVMGroup * lvg);
    virtual AppleLVMLogicalExtent * findExtent(UInt64 offset);
    virtual bool hasExtentsOnMember(AppleRAIDMember * member);
    virtual bool buildExtentList(AppleRAIDExtentOnDisk * extents);

    // snapshots

    inline bool isAVolume(void) const		{ return (lvTypeID & kLVMTypeIsAVolume) != 0; };
    inline bool isASnapShot(void) const		{ return (lvTypeID & kLVMTypeIsASnapShot) != 0; };
    inline bool isABitMap(void) const		{ return (lvTypeID & kLVMTypeBitMap) != 0; };
    inline UInt32 getTypeID(void) const		{ return lvTypeID; };

    inline AppleLVMVolume * parentVolume(void) const	{ return lvParent; };
    inline AppleLVMVolume * snapShotVolume(void) const	{ return lvSnapShot; };
    inline AppleLVMVolume * bitMapVolume(void) const	{ return lvBitMap; };
    virtual bool setParent(AppleLVMVolume * parent);
    virtual bool setSnapShot(AppleLVMVolume * snapshot);
    virtual bool setBitMap(AppleLVMVolume * bitmap);

    virtual bool zeroVolume(void);
};

#endif KERNEL

#endif _APPLELVMVOLUME_H
