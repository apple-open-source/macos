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


#ifndef _APPLERAIDMIRRORSET_H
#define _APPLERAIDMIRRORSET_H

#define kAppleRAIDLevelNameMirror "Mirror"

#ifdef KERNEL

class AppleRAIDMirrorSet : public AppleRAIDSet
{
    OSDeclareDefaultStructors(AppleRAIDMirrorSet);

 private:
    UInt32			arExpectingLiveAdd;
    thread_call_t		arSetCompleteThreadCall;

    UInt64 *			arLastSeek;			
    UInt64 *			arSkippedIOCount;
    
    AppleRAIDMember *		arRebuildingMember;
    thread_call_t		arRebuildThreadCall;
    
    queue_head_t		arFailedRequestQueue;


 protected:
    virtual bool init(void);
    virtual bool initWithHeader(OSDictionary * header, bool firstTime);
    virtual void free();

    virtual IOBufferMemoryDescriptor * readPrimaryMetaData(AppleRAIDMember * member);
    virtual IOReturn writePrimaryMetaData(IOBufferMemoryDescriptor * primaryBuffer);

    virtual void rebuildStart(void);
    virtual void rebuild(void);
    virtual void rebuildComplete(bool wasRebuilt);

    virtual void getRecoverQueue(queue_head_t *oldRequestQueue, queue_head_t *newRequestQueue);
    virtual bool recover(void);

    virtual void startSetCompleteTimer();
    virtual void setCompleteTimeout(void);

public:
    static AppleRAIDSet * createRAIDSet(AppleRAIDMember * firstMember);
    virtual bool addMember(AppleRAIDMember * member);
    virtual bool removeMember(AppleRAIDMember * member, IOOptionBits options);

    virtual bool resizeSet(UInt32 newMemberCount);
    virtual bool startSet(void);
    virtual bool publishSet(void);

    virtual bool isSetComplete(void);
    virtual bool bumpOnError(void);
    virtual UInt32 nextSetState(void);
    virtual OSDictionary * getSetProperties(void);

    virtual void activeReadMembers(AppleRAIDMember ** activeMembers, UInt64 byteStart, UInt32 byteCount);

    virtual void completeRAIDRequest(AppleRAIDStorageRequest *storageRequest);
    
    virtual AppleRAIDMemoryDescriptor * allocateMemoryDescriptor(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex);
};



class AppleRAIDMirrorMemoryDescriptor : public AppleRAIDMemoryDescriptor
{
    OSDeclareDefaultStructors(AppleRAIDMirrorMemoryDescriptor);
    
private:
    UInt32		mdSetBlockSize;
    UInt32		mdSetBlockStart;
    UInt32		mdSetBlockOffset;
    
protected:
    virtual bool initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex);
    virtual bool configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart, UInt32 activeIndex);
    
public:
    static AppleRAIDMemoryDescriptor *withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex);
    virtual addr64_t getPhysicalSegment(IOByteCount offset, IOByteCount * length, IOOptionBits options = 0);
};

#endif KERNEL

#endif /* ! _APPLERAIDMIRRORSET_H */
