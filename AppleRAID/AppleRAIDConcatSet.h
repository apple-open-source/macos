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


#ifndef _APPLERAIDCONCATSET_H
#define _APPLERAIDCONCATSET_H

#define kAppleRAIDLevelNameConcat "Concat"

#ifdef KERNEL

class AppleRAIDConcatSet : public AppleRAIDSet
{
    OSDeclareDefaultStructors(AppleRAIDConcatSet);
    
 private:
    UInt64 *	arMemberBlockCounts;
    UInt32	arExpectingLiveAdd;
    
 protected:
    virtual bool init();
    virtual void free();

    virtual void unpauseSet(void);
    
 public:
    static AppleRAIDSet * createRAIDSet(AppleRAIDMember * firstMember);
    virtual bool addSpare(AppleRAIDMember * member);
    virtual bool addMember(AppleRAIDMember * member);
    virtual bool removeMember(AppleRAIDMember * member, IOOptionBits options);

    virtual bool resizeSet(UInt32 newMemberCount);
    virtual bool startSet(void);
    virtual bool publishSet(void);

    virtual UInt64 getMemberSize(UInt32 memberIndex) const;

    virtual AppleRAIDMemoryDescriptor * allocateMemoryDescriptor(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex);
};



class AppleRAIDConcatMemoryDescriptor : public AppleRAIDMemoryDescriptor
{
    OSDeclareDefaultStructors(AppleRAIDConcatMemoryDescriptor);
    
private:
    UInt64		mdMemberOffset;
    UInt64		mdMemberStart;
    UInt64		mdMemberEnd;
    
protected:
    virtual bool initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex);
    virtual bool configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart, UInt32 activeIndex);
    
 public:
    static AppleRAIDMemoryDescriptor *withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex);
    virtual addr64_t getPhysicalSegment(IOByteCount offset, IOByteCount * length, IOOptionBits options = 0);
};

#endif KERNEL

#endif /* ! _APPLERAIDCONCATSET_H */
