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

#define super IOMedia
OSDefineMetaClassAndStructors(AppleLVMVolume, IOMedia)


OSDictionary * AppleLVMVolume::propsFromHeader(AppleLVMVolumeOnDisk * lve)
{
    OSString * errmsg = 0;
    OSDictionary * lvProps = OSDynamicCast(OSDictionary, OSUnserializeXML(lve->plist, &errmsg));
    if (!lvProps) {
	if (errmsg) {
	    IOLog("AppleLVMVolume::propsFromHeader - XML parsing failed with %s\n", errmsg->getCStringNoCopy());
	    errmsg->release();
	}
	return NULL;
    }

    return lvProps;
}


AppleLVMVolume * AppleLVMVolume::withHeader(AppleLVMVolumeOnDisk * lve, OSDictionary * lvProps)
{
    if (!lve) return false;

    if (!lvProps) lvProps = AppleLVMVolume::propsFromHeader(lve);
    if (!lvProps) return false;

    AppleLVMVolume *me = new AppleLVMVolume;
    if (!me) return NULL;

    if (!me->init() || !me->initWithHeader(lvProps)) {
        me->release();
        return NULL;
    }

    me->lvEntrySize = lve->lvHeaderSize;

    return me;
}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888


void AppleLVMVolume::free()
{
    if (lvProps) lvProps->release();

//    if (lvSnapShot) lvSnapShot->release();
//    if (lvParent) lvParent->release();
//    if (lvBitMap) lvBitMap->release();

    AppleLVMLogicalExtent * extent = NULL;
    AppleLVMLogicalExtent * nextExtent = lvExtent[0];
    while (nextExtent) {
	extent = nextExtent;
	nextExtent = nextExtent->skip[0];
	delete extent;
    }

    super::free();
}


bool AppleLVMVolume::init(void)
{
    if (!OSObject::init()) return false;  // why?  more "using" weirdness?
    
    lvIndex = 0xffffffff;
    lvSequenceNumber = 0;
    lvClaimedSize = 0;
    lvCalculatedSize = 0;
    lvTypeID = 0;
    lvEntryOffset = 0;

    lvExtentCount = 0;
    lvExtent[0] = NULL;

    lvPublished = false;

    lvParent = NULL;
    lvSnapShot = NULL;
    lvBitMap = NULL;
    
    return true;
}

bool AppleLVMVolume::initWithHeader(OSDictionary * props)
{
    IOLog1("AppleLVMVolume::initWithHeader() entered\n");

    if (lvProps) lvProps->release();
    lvProps = props;
    lvProps->retain();

    if (!getVolumeUUID()) return false;
    if (!getGroupUUID()) return false;
    
    OSNumber * number;
    number = OSDynamicCast(OSNumber, lvProps->getObject(kAppleLVMVolumeSequenceKey));
    if (!number) return false;
    lvSequenceNumber = number->unsigned32BitValue();

    number = OSDynamicCast(OSNumber, lvProps->getObject(kAppleLVMVolumeExtentCountKey));
    if (!number) return false;
    lvExtentCount = number->unsigned64BitValue();
    
    number = OSDynamicCast(OSNumber, lvProps->getObject(kAppleLVMVolumeSizeKey));
    if (!number) return false;
    lvClaimedSize = number->unsigned64BitValue();

    OSString * type = OSDynamicCast(OSString, lvProps->getObject(kAppleLVMVolumeTypeKey));
    if (!type) return false;
    if (type->isEqualTo(kAppleLVMVolumeTypeConcat)) lvTypeID = kLVMTypeConcat;
    if (type->isEqualTo(kAppleLVMVolumeTypeBitMap)) lvTypeID = kLVMTypeBitMap;
    if (type->isEqualTo(kAppleLVMVolumeTypeSnapRO)) lvTypeID = kLVMTypeSnapRO;
    if (type->isEqualTo(kAppleLVMVolumeTypeSnapRW)) lvTypeID = kLVMTypeSnapRW;
    if (type->isEqualTo(kAppleLVMVolumeTypeMaster)) lvTypeID = kLVMTypeMaster;
    if (!lvTypeID) return false;

    lvSnapShot = NULL;  // just clear these, they might not exist yet.
    lvBitMap = NULL;
    lvParent = NULL;

    IOLog1("AppleLVMVolume::initWithHeader() successful for %s, size = %llu extent count = %llu\n",
	   getVolumeUUIDString(), lvClaimedSize, lvExtentCount);
    
    return true;
}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888


OSDictionary * AppleLVMVolume::getVolumeProperties(void)
{
    // make a copy
    assert(lvProps);
    OSDictionary * props = OSDictionary::withDictionary(lvProps, lvProps->getCount() + 2);
    if (!props) return NULL;
    
    // not from header
    OSString * status = NULL;
    if (isPublished()) {
	status = OSString::withCString(kAppleRAIDStatusOnline);
    } else {
	status = OSString::withCString(kAppleRAIDStatusOffline);
    }
    if (status) {
	props->setObject(kAppleLVMVolumeStatusKey, status);
	status->release();
    }

    props->setObject(kIOBSDNameKey, getDiskName());

    return props;
}


const OSString * AppleLVMVolume::getVolumeUUID(void)
{
    assert(lvProps);
    const OSString * string = OSDynamicCast(OSString, lvProps->getObject(kAppleLVMVolumeUUIDKey));

    return string;
}


const char * AppleLVMVolume::getVolumeUUIDString(void)
{
    const OSString * uuid = getVolumeUUID();

    return uuid ? uuid->getCStringNoCopy() : "--internal error, uuid not set--";
}


const OSString * AppleLVMVolume::getDiskName(void)
{
    if (!getPropertyTable()) return NULL;  // this panics if not set (unpublished)
    const OSMetaClassBase * name = getProperty(kIOBSDNameKey);
    if (!name) return NULL;
    return OSDynamicCast(OSString, name);  
}


const OSString * AppleLVMVolume::getGroupUUID(void)
{
    assert(lvProps);
    const OSString * lvgUUID = OSDynamicCast(OSString, lvProps->getObject(kAppleLVMGroupUUIDKey));

    return lvgUUID;
}


const OSString * AppleLVMVolume::getParentUUID(void)
{
    assert(lvProps);
    const OSString * parentUUID = (OSDynamicCast(OSString, lvProps->getObject(kAppleLVMParentUUIDKey)));

    return parentUUID;
}


const OSString * AppleLVMVolume::getHint(void)   // IOMedia also has getContentHint()
{
    assert(lvProps);
    const OSString * hint = OSDynamicCast(OSString, lvProps->getObject(kAppleLVMVolumeContentHintKey));

    return hint;
}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888


bool AppleLVMVolume::addExtents(AppleLVMGroup * lvg, AppleLVMVolumeOnDisk * lve)
{

    // XXX failure cases need to mark lv as broken
    
    AppleRAIDExtentOnDisk * extent = (AppleRAIDExtentOnDisk *)((char *)lve + lve->lvExtentsStart);
    if ((void *)(extent + lvExtentCount) > (void *)((char *)lve + lve->lvHeaderSize - sizeof(AppleRAIDExtentOnDisk))) {
	IOLog("AppleLVMVolume::addExtents() too many extents detected for logical volume \"%s\"\n", getVolumeUUIDString());
	return false;
    }
	
    UInt64 count = lvExtentCount;
    while (count) {
	AppleLVMLogicalExtent * newExtent = addExtent(lvg, extent);
	if (!newExtent) {
	    IOLog("AppleLVMVolume::addExtents() overlapping extent detected in logical volume \"%s\"\n", getVolumeUUIDString());
	    return false;
	}
	if (!lvg->addExtentToLVG(newExtent)) {
	    IOLog("AppleLVMVolume::addExtents() overlapping logical volumes detected for logical volume \"%s\"\n", getVolumeUUIDString());
	    return false;
	}
	count--;
	extent++;
    }

    if (lvClaimedSize != lvCalculatedSize) {
	IOLog("AppleLVMVolume::addExtents() size error for extent list for logical volume \"%s\"\n", getVolumeUUIDString());
	IOLog("AppleLVMVolume::addExtents() expected size %llu, calculated size %llu\n", lvClaimedSize, lvCalculatedSize);
	return false;
    }
    
    IOLog1("AppleLVMVolume::addExtents() successful for %s, extent count = %llu, size = %llu\n",
	   getVolumeUUIDString(), lvExtentCount, lvCalculatedSize);

    return true;
}

AppleLVMLogicalExtent * AppleLVMVolume::addExtent(AppleLVMGroup * lvg, AppleRAIDExtentOnDisk * extentOnDisk)
{
    AppleLVMLogicalExtent ** extent = &lvExtent[0];
    AppleLVMLogicalExtent * extentInCore = new AppleLVMLogicalExtent;
    UInt64 volumeOffset = 0;

    // find end of list
    while (*extent) {
	volumeOffset += (*extent)->lvExtentSize;
	extent = &((*extent)->skip[0]);
    }

    // XXX hm, there is no range checking here

    extentInCore->skip[0] = 0;
    extentInCore->lvgNext = 0;

    extentInCore->lvMemberIndex = lvg->getMemberIndexFromOffset(extentOnDisk->extentByteOffset);

    extentInCore->lvExtentVolumeOffset = volumeOffset;
    extentInCore->lvExtentGroupOffset = extentOnDisk->extentByteOffset;
    extentInCore->lvExtentMemberOffset = extentOnDisk->extentByteOffset - lvg->getMemberStartingOffset(extentInCore->lvMemberIndex);

    extentInCore->lvExtentSize = extentOnDisk->extentByteCount;

    *extent = extentInCore;
    lvCalculatedSize = volumeOffset + extentInCore->lvExtentSize;
    
    IOLog1("AppleLVMVolume::addExtent() successful, offset = %llu, size = %llu totalSize = %llu\n",   // XXX <<<<<<<< log2
	   extentInCore->lvExtentVolumeOffset, extentInCore->lvExtentSize, lvCalculatedSize);

    return extentInCore;
}
    
bool AppleLVMVolume::removeExtents(AppleLVMGroup * lvg)
{
    AppleLVMLogicalExtent * extent = lvExtent[0];
    AppleLVMLogicalExtent * prevExtent;
    lvExtent[0] = NULL;
    lvCalculatedSize = 0;
    while (extent) {

	lvg->removeExtentFromLVG(extent);

	prevExtent = extent;
	extent = extent->skip[0];

	delete prevExtent;
    }

    return true;
}

AppleLVMLogicalExtent * AppleLVMVolume::findExtent(UInt64 offset)
{
    // find the extent relative to the offset in this volume
    
    AppleLVMLogicalExtent * extent = lvExtent[0];
    AppleLVMLogicalExtent * nextExtent = extent ? extent->skip[0] : NULL;
    while (nextExtent) {
	if (nextExtent->lvExtentVolumeOffset > offset) break;
	extent = nextExtent;
	nextExtent = nextExtent->skip[0];
    }

    return extent;
}

bool AppleLVMVolume::hasExtentsOnMember(AppleRAIDMember * member)
{
    UInt32 memberIndex = member->getMemberIndex();
    
    AppleLVMLogicalExtent * extent = lvExtent[0];
    while (extent) {
	if (extent->lvMemberIndex == memberIndex) return true;
	extent = extent->skip[0];
    }

    return false;
}

bool AppleLVMVolume::buildExtentList(AppleRAIDExtentOnDisk * extentList)
{
    AppleLVMLogicalExtent * incoreExtent = lvExtent[0];
    while (incoreExtent) {
	extentList->extentByteOffset = incoreExtent->lvExtentGroupOffset;
	extentList->extentByteCount = incoreExtent->lvExtentSize;

	extentList++;
	incoreExtent = incoreExtent->skip[0];
    }

    return true;
}


//
//  snapshot stuff
//

bool AppleLVMVolume::setParent(AppleLVMVolume * parent)
{
    lvParent = parent;

    // XXXSNAP
    
    return true;
}

bool AppleLVMVolume::setBitMap(AppleLVMVolume * bitmap)
{
    lvBitMap = bitmap;

    // XXXSNAP
    
    return true;
}


bool AppleLVMVolume::setSnapShot(AppleLVMVolume * snapshot)
{
    lvSnapShot = snapshot;
    
    // XXXSNAP

    return true;
}


bool AppleLVMVolume::zeroVolume()
{
    assert(isABitMap());

    // XXXSNAP
    // call into a bitmap object?
    
    return true;
}

