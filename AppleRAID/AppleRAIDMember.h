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

#ifndef _AppleRAIDMember_H
#define _AppleRAIDMember_H

#define kAppleRAIDSignature "AppleRAIDHeader"

enum {
    kAppleRAIDHeaderSize	= 0x1000,
    kAppleRAIDDefaultChunkSize	= 0x8000
};

#define ARHEADER_OFFSET(s)  ( (UInt64)(s) / kAppleRAIDHeaderSize * kAppleRAIDHeaderSize - kAppleRAIDHeaderSize )
#define ARCHUNK_COUNT(s, cs)  ( ARHEADER_OFFSET(s) / (UInt64)(cs) )

struct AppleRAIDHeaderV2 {
    char	raidSignature[16];
    char	raidUUID[64];
    char	memberUUID[64];
    UInt64	size;
    char 	plist[];
};
typedef struct AppleRAIDHeaderV2 AppleRAIDHeaderV2;

#ifdef KERNEL

#include <uuid/uuid.h>

#define kAppleRAIDBaseOffsetKey		"appleraid-BaseOffset"		// CFNumber 64bit
#define kAppleRAIDNativeBlockSizeKey	"appleraid-NativeBlockSize"	// CFNumber 64bit
#define kAppleRAIDMemberCountKey	"appleraid-MemberCount"		// CFNumber 32bit


enum {
    kAppleRAIDMemberStateBroken = 0,
    kAppleRAIDMemberStateSpare,
    kAppleRAIDMemberStateClosed,
    kAppleRAIDMemberStateClosing,
    kAppleRAIDMemberStateRebuilding,
    kAppleRAIDMemberStateOpen
};

class AppleRAID;

class AppleRAIDMember: public IOStorage {

    OSDeclareDefaultStructors(AppleRAIDMember)

private:
    IOMedia *			arTarget;		// short cut to provider

    UInt64			arHeaderOffset;
    IOBufferMemoryDescriptor	*arHeaderBuffer;
    thread_call_t		arSyncronizeCacheThreadCall;

protected:

    OSDictionary *		arHeader;

    AppleRAID *			arController;

    UInt32			arMemberState;

    UInt64			arNativeBlockSize;
    UInt64			arBaseOffset;

    bool			arIsEjectable;
    bool			arIsWritable;
    bool			arIsRAIDMember;

    virtual bool handleOpen(IOService * client, IOOptionBits options, void * access);
    virtual bool handleIsOpen(const IOService* client) const;
    virtual void handleClose(IOService*client, IOOptionBits options);

public:

    virtual bool init(OSDictionary * properties = 0);
    virtual void free(void);

    virtual bool start(IOService * provider);
    virtual void stop(IOService * provider);
    virtual bool requestTerminate(IOService *provider, IOOptionBits options);

    virtual void read(IOService *         client,
		      UInt64              byteStart,
		      IOMemoryDescriptor* buffer,
		      IOStorageCompletion completion);
    virtual void write(IOService *         client,
                       UInt64              byteStart,
                       IOMemoryDescriptor* buffer,
                       IOStorageCompletion completion);
    virtual IOReturn synchronizeCache(IOService * client);
    virtual IOReturn synchronizeCacheCallout(AppleRAIDSet *masterSet);

    virtual IOReturn readRAIDHeader(void);
    virtual IOReturn writeRAIDHeader(void);
    virtual IOReturn updateRAIDHeader(OSDictionary * props);
    virtual IOReturn zeroRAIDHeader(void);

    virtual IOReturn parseRAIDHeaderV1(void);
    virtual IOReturn buildOnDiskHeaderV1(void);
    virtual IOReturn parseRAIDHeaderV2(void);
    virtual IOReturn buildOnDiskHeaderV2(void);

    virtual OSDictionary * getHeader(void);
    virtual OSObject * getHeaderProperty( const OSString * aKey) const;
    virtual OSObject * getHeaderProperty( const char * aKey) const;
    virtual bool setHeaderProperty(const OSString * aKey, OSObject * anObject);
    virtual bool setHeaderProperty(const char * aKey, OSObject * anObject);
    virtual bool setHeaderProperty(const char * aKey, const char * cString);
    virtual bool setHeaderProperty(const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits);

    virtual const OSString * getSetName(void);
    virtual const char * getSetNameString(void);
    virtual const OSString * getUUID(void);
    virtual const char * getUUIDString(void);
    virtual const OSString * getSetUUID(void);
    virtual const char * getSetUUIDString(void);
    virtual const OSString * getDiskName(void);

    virtual IOStorage * getTarget(void) const;
    virtual bool isRAIDSet(void);
    virtual bool isRAIDMember(void);
    virtual bool isSpare(void);
    virtual bool isBroken(void);
    virtual UInt64 getSize(void) const;
    virtual UInt32 getMemberIndex(void);

    virtual bool isEjectable(void) const;
    virtual bool isWritable(void) const;
    virtual UInt64 getPreferredBlockSize(void) const;
    virtual UInt64 getBase(void) const;

    virtual bool addBootDeviceInfo(OSArray * bootArray);
    virtual OSDictionary * getMemberProperties(void);

    virtual bool changeMemberState(UInt32 newState, bool force = 0);
    inline  UInt32 getMemberState(void)		{ return arMemberState; };
};

#endif KERNEL

#endif /* ! _AppleRAIDMember_H */
