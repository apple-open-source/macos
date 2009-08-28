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


#ifndef _APPLERAIDSET_H
#define _APPLERAIDSET_H

#define kARSetCompleteTimeoutNone	0
#define kARSetCompleteTimeoutDefault	30

enum {
    kAppleRAIDSetStateFailed = 0,
    kAppleRAIDSetStateTerminating,
    kAppleRAIDSetStateInitializing,
    kAppleRAIDSetStateOnline,
    kAppleRAIDSetStateDegraded
};

class AppleRAIDSet : public AppleRAIDMember
{
    OSDeclareAbstractStructors(AppleRAIDSet)

    friend class AppleRAIDStorageRequest;
    friend class AppleLVMStorageRequest;
    friend class AppleRAID;

protected:
    UInt32			arSetState;

    UInt32			arHeaderVersion;
    UInt64			arSetBlockSize;
    UInt64			arSetBlockCount;
    UInt64			arSetMediaSize;
    UInt32			arSequenceNumber;

    UInt32			arSetCompleteTimeout;		// for degradeable sets

    IOStorageAccess		arOpenLevel;
    OSSet *			arOpenReaders;
    OSSet *			arOpenReaderWriters;

    IOMedia *			arMedia;

    UInt32			arSetIsPaused;
    bool			arSetWasBlockedByPause;
    UInt32			arStorageRequestsPending;
    UInt32			arMaxReadRequestFactor;

    UInt64			arPrimaryMetaDataUsed;		// mirror rebuild, lvg toc
    UInt64			arPrimaryMetaDataMax;		// mirror rebuild, lvg toc

    thread_call_t		arRecoveryThreadCall;
    IOCommandGate::Action	arAllocateRequestMethod;
    
    UInt32			arActiveCount;
    UInt32			arMemberCount;
    UInt32			arLastAllocCount;
    AppleRAIDMember		**arMembers;

    OSSet			*arSpareMembers;

    UInt32			*arLogicalMemberIndexes;

    IOWorkLoop			*arSetWorkLoop;
    IOCommandGate		*arSetCommandGate;
    AppleRAIDEventSource	*arSetEventSource;

    IOCommandPool		*arStorageRequestPool;

    SInt32			arSetIsSyncingCount;


 protected:
    virtual void free(void);
    virtual bool init(void);
    virtual bool initWithHeader(OSDictionary * header, bool firstTime);

    virtual bool handleOpen(IOService * client, IOOptionBits options, void * access);
    virtual bool handleIsOpen(const IOService *client) const;
    virtual void handleClose(IOService * client, IOOptionBits options);

    virtual void recoverStart(void);
    virtual void recoverWait(void);
    virtual bool recover(void);

    virtual bool pauseSet(bool whenIdle);
    virtual void unpauseSet(void);
    
 public:

    virtual bool addSpare(AppleRAIDMember * member);
    virtual bool addMember(AppleRAIDMember * member);
    virtual bool removeMember(AppleRAIDMember * member, IOOptionBits options);
    virtual bool upgradeMember(AppleRAIDMember *member);

    virtual bool resizeSet(UInt32 newMemberCount);
    virtual bool startSet(void);
    virtual bool publishSet(void);
    virtual bool unpublishSet(void);
    virtual bool destroySet(void);
    virtual bool reconfigureSet(OSDictionary * updateInfo);

    virtual UInt32 getSequenceNumber(void);
    virtual void bumpSequenceNumber(void);
    virtual IOReturn writeRAIDHeader(void);

    virtual IOBufferMemoryDescriptor * readPrimaryMetaData(AppleRAIDMember * member);
    virtual IOReturn writePrimaryMetaData(IOBufferMemoryDescriptor * primaryBuffer);
    virtual bool readIntoBuffer(AppleRAIDMember * member, IOBufferMemoryDescriptor * buffer, UInt64 offset);
    virtual IOReturn writeFromBuffer(AppleRAIDMember * member, IOBufferMemoryDescriptor * buffer, UInt64 offset);

    virtual const OSString * getSetName(void);
    virtual const OSString * getUUID(void);
    virtual const OSString * getSetUUID(void);
    virtual const OSString * getDiskName(void);

    virtual IOStorage * getTarget(void) const;
    virtual bool isRAIDSet(void);
    virtual bool isSetComplete(void);
    virtual bool bumpOnError(void);
    virtual UInt64 getSize(void) const;
    virtual IOWorkLoop *getWorkLoop(void);
    virtual bool changeSetState(UInt32 newState);
    virtual UInt32 nextSetState(void);
    virtual UInt64 getSmallestMaxByteCount(void);
    virtual void setSmallest64BitMemberPropertyFor(const char * key, UInt32 multiplier);
    virtual void setLargest64BitMemberPropertyFor(const char * key, UInt32 multiplier);

    inline  UInt32 getActiveCount(void) const	{ return arActiveCount; };
    inline  UInt32 getMemberCount(void)	const	{ return arMemberCount; };
    inline  UInt32 getSpareCount(void) const	{ return arSpareMembers->getCount(); };
    inline  UInt32 getSetState(void) const	{ return arSetState; };
    virtual UInt32 getMaxRequestCount(void) const { return arMemberCount; };

    virtual bool addBootDeviceInfo(OSArray * bootArray);
    virtual OSDictionary * getSetProperties(void);
    
    virtual void read(IOService * client, UInt64 byteStart, IOMemoryDescriptor* buffer, IOStorageAttributes * attributes, IOStorageCompletion * completion);
    virtual void write(IOService * client, UInt64 byteStart, IOMemoryDescriptor* buffer, IOStorageAttributes * attributes, IOStorageCompletion * completion);
    virtual void activeReadMembers(AppleRAIDMember ** activeMembers, UInt64 byteStart, UInt32 byteCount);
    virtual void activeWriteMembers(AppleRAIDMember ** activeMembers, UInt64 byteStart, UInt32 byteCount);

    virtual IOReturn synchronizeCache(IOService* client);
    virtual IOReturn synchronizeCacheGated(IOService *client);
    virtual void synchronizeStarted(void);
    virtual void synchronizeCompleted(void);
    virtual void synchronizeCompletedGated(void);

    inline  bool isPaused(void)	const		{ return arSetIsPaused != 0; };

    virtual IOReturn allocateRAIDRequest(AppleRAIDStorageRequest **storageRequest);
    virtual void returnRAIDRequest(AppleRAIDStorageRequest *storageRequest);
    virtual void completeRAIDRequest(AppleRAIDStorageRequest *storageRequest);

    virtual AppleRAIDMemoryDescriptor * allocateMemoryDescriptor(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex) = 0;
};

#endif /* ! _APPLERAIDSET_H */
