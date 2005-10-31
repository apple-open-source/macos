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


enum {
    kAppleRAIDHeaderV1_0_0	= 0x00010000,
    kAppleRAIDMaxOFPath		= 0x200,
};

// RAID levels  (version 1)
enum {
    kAppleRAIDStripe		= 0x00000000,
    kAppleRAIDMirror		= 0x00000001,
    kAppleRAIDConcat		= 0x00000100
};

struct AppleRAIDHeaderV1 {
    char	raidSignature[16];		// 0x0000 - kAppleRAIDSignature
    UInt32	raidHeaderSize;			// 0x0010 - Defaults to kAppleRAIDHeaderSize
    UInt32	raidHeaderVersion;		// 0x0014 - kAppleRAIDHeaderV1_0_0
    UInt32	raidHeaderSequence;		// 0x0018 - 0 member is bad, >0 member could be good
    UInt32	raidLevel;			// 0x001C - one of kAppleRAIDStripe, kAppleRAIDMirror or kAppleRAIDConcat
    uuid_t	raidUUID;			// 0x0020 - 128 bit univeral unique identifier
    char	raidSetName[32];		// 0x0030 - Null Terminated 31 Character UTF8 String
    UInt32	raidMemberCount;		// 0x0050 - Number of members in set
    UInt32	raidMemberIndex;		// 0x0054 - 0 <= raidMemberIndex < raidMemberCount
    UInt32	raidChunkSize;			// 0x0058 - Usually 32 KB
    UInt32	raidChunkCount;			// 0x005C - Number of full chunks in set
    UInt32	reserved1[104];			// 0x0060 - inited to zero, but preserved on update
    char	raidOFPaths[0];			// 0x0200 - Allow kAppleRAIDMaxOFPath for each member
                                                //        - Zero fill to size of header
};
typedef struct AppleRAIDHeaderV1 AppleRAIDHeaderV1;


#define super IOStorage
OSDefineMetaClassAndStructors(AppleRAIDMember, IOStorage);


bool AppleRAIDMember::init(OSDictionary * properties)
{
    IOLog1("AppleRAIDMember::init(%p) isSet = %s\n", this, isRAIDSet() ? "yes":"no");

    if (super::init(properties) == false) return false;

    // get the controller object
    arController = gAppleRAIDGlobals.getController();
    if (!arController) return false;

    arHeader = OSDictionary::withCapacity(32);
    if (!arHeader) return false;
    
    arTarget = 0;
    arBaseOffset = 0xdeaddeaddeadbeefLL;
    arHeaderOffset = 0xdeaddeaddeadbeefLL;

    arIsWritable = false;
    arIsEjectable = false;
    arNativeBlockSize = 0;
    arSyncronizeCacheThreadCall = 0;
    
#ifdef DEBUG
    IOSleep(500);  // let the system log catch up
#endif

    return true;
}


void AppleRAIDMember::free(void)
{
    IOLog1("AppleRAIDMember::free(%p)\n", this);

    if (arHeaderBuffer) arHeaderBuffer->release();
    if (arHeader) arHeader->release();

    if (arSyncronizeCacheThreadCall) {
	thread_call_free(arSyncronizeCacheThreadCall);
	arSyncronizeCacheThreadCall = 0;
    }

    gAppleRAIDGlobals.releaseController();

    super::free();
}


//*************************************************************************************************************
//***************************************************************************************************************
//***************************************************************************************************************


bool AppleRAIDMember::start(IOService * provider)
{ 
    IOLog1("AppleRAIDMember::start(%p) isSet = %s\n", this, isRAIDSet() ? "yes":"no");

    if (!isRAIDSet()) {

	assert(provider);
	arTarget = (IOMedia *)provider;

	if (super::start(provider) == false) return false;

	arIsWritable = arTarget->isWritable();
	arIsEjectable = arTarget->isEjectable();
	arNativeBlockSize = arTarget->getPreferredBlockSize();

	if (!arSyncronizeCacheThreadCall) {
	    thread_call_func_t syncCacheMethod = OSMemberFunctionCast(thread_call_func_t, this, &AppleRAIDMember::synchronizeCacheCallout);
	    arSyncronizeCacheThreadCall = thread_call_allocate(syncCacheMethod, (thread_call_param_t)this);
	    if (arSyncronizeCacheThreadCall == 0) return false;
	}
    }

    arIsRAIDMember = false;
    arMemberState = kAppleRAIDMemberStateClosed;
    setProperty(kAppleRAIDMemberStatusKey, kAppleRAIDStatusOffline);

    //  if no raid header just return
    if (readRAIDHeader()) {
	return false;
    }

#ifdef DEBUG
    if (isRAIDSet()) IOLog1("AppleRAIDMember::start(%p) this set is part of stacked raid.\n", this);
#endif		

    arIsRAIDMember = true;

    return (arController->newMember(this) == kIOReturnSuccess);
}


bool AppleRAIDMember::requestTerminate(IOService *provider, IOOptionBits options)
{
    IOLog1("AppleRAIDMember::requestTerminate(%p) isSet = %s\n", this, isRAIDSet() ? "yes":"no");
    
    //
    //  If the raid member is in use (opened) the stop method will not be called.
    //  luckily, the client still gets notified here and we can fix things.
    //  setting the state to closing will cause us to catch this on the next i/o
    //

    if (isRAIDSet()) {
	return false;
    } else {
	if (arMemberState > kAppleRAIDMemberStateClosed) {
	    changeMemberState(kAppleRAIDMemberStateClosing);
	    arController->recoverMember(this);
	}
    }

    return super::requestTerminate(provider, options);
}


void AppleRAIDMember::stop(IOService * provider)
{
    IOLog1("AppleRAIDMember::stop(%p) isMember = %s isSet = %s\n", this, isRAIDMember() ? "yes":"no", isRAIDSet() ? "yes":"no");

    //
    // when a member is pulled, it's parent set gets called here first.
    // just ignore requests to stop sets, they are terminated in 
    // by the controller when all of the member/spares are gone.
    //

    if (!isRAIDSet()) {
	arController->oldMember(this);
	arIsRAIDMember = false;
	super::stop(provider);		// this is noop
    }
}


//***************************************************************************************************************
//***************************************************************************************************************
//***************************************************************************************************************


bool AppleRAIDMember::handleOpen(IOService * /* client */,
				 IOOptionBits options,
				 void* argument)
{
    bool isOpen = arTarget->open(this, options, (IOStorageAccess) argument);
    if (isOpen) changeMemberState(kAppleRAIDMemberStateOpen);
    return isOpen;
}


bool AppleRAIDMember::handleIsOpen(const IOService * /* client */) const
{
    return arTarget->isOpen(this);
}


void AppleRAIDMember::handleClose(IOService * /* client */,
				  IOOptionBits options)
{
    changeMemberState(kAppleRAIDMemberStateClosing);
    arTarget->close(this, options);
    changeMemberState(kAppleRAIDMemberStateClosed);
}


//***************************************************************************************************************
//***************************************************************************************************************
//***************************************************************************************************************

void AppleRAIDMember::read(IOService * /* client */,
			   UInt64 byteStart,
			   IOMemoryDescriptor * buffer,
			   IOStorageCompletion completion)
{
    IOLogRW("AppleRAIDMember::read, this %p start %llu size 0x%llx\n", this, byteStart, (UInt64)buffer->getLength());
    assert(handleIsOpen(NULL));
    arTarget->read(this, byteStart, buffer, completion);
}


void AppleRAIDMember::write(IOService * /* client */,
			    UInt64 byteStart,
			    IOMemoryDescriptor * buffer,
			    IOStorageCompletion completion)
{
    IOLogRW("AppleRAIDMember::write, this %p start %llu size 0x%llx\n", this, byteStart, (UInt64)buffer->getLength());
    assert(handleIsOpen(NULL));
    arTarget->write(this, byteStart, buffer, completion);
}


IOReturn AppleRAIDMember::synchronizeCache(IOService * client)
{
    AppleRAIDSet * masterSet = OSDynamicCast(AppleRAIDSet, client);
    assert(masterSet);

    // we are running on the master's workloop
    masterSet->synchronizeStarted();

    bool bumped = thread_call_enter1(arSyncronizeCacheThreadCall, (thread_call_param_t)masterSet);

    // we bumped another sync request, decrement count for them
    assert(!bumped);
    if (bumped) masterSet->synchronizeCompleted();

    return 0;
}

IOReturn AppleRAIDMember::synchronizeCacheCallout(AppleRAIDSet *masterSet)
{
    assert(masterSet);

    IOReturn result = arTarget->synchronizeCache(this);
    if (result) IOLog("AppleRAIDMember::synchronizeCacheCallout: failed with %x on %s\n", result, getUUIDString());

    masterSet->synchronizeCompleted();
    
    return result;
}


//***************************************************************************************************************
//***************************************************************************************************************
//***************************************************************************************************************

IOReturn AppleRAIDMember::readRAIDHeader(void)
{
    UInt64 size = getSize();
    UInt64 headerSize = (UInt64)kAppleRAIDHeaderSize;

    assert(size);

    // look for version 2 header first
    arHeaderOffset = ARHEADER_OFFSET(size);

    // Allocate a buffer to read the AppleRAID Header.
    if (arHeaderBuffer == 0) {
	arHeaderBuffer = IOBufferMemoryDescriptor::withCapacity(headerSize, kIODirectionNone);
	if (arHeaderBuffer == 0) return kIOReturnNoMemory;
    }

readheader:

    IOLog2("AppleRAIDMember::readRAIDHeader(%p) size %llu  hdr off %llu\n", this, size, arHeaderOffset);
    
    // Open the member
    if (!getTarget()->open(this, 0, kIOStorageAccessReader)) return kIOReturnIOError;

    // Read the raid header
    arHeaderBuffer->setDirection(kIODirectionIn);
    IOReturn rc = getTarget()->read(this, arHeaderOffset, arHeaderBuffer);
        
    // Close the member.
    getTarget()->close(this, 0);

    if (rc) return rc;
    
    // Make sure the AppleRAID Header contains the correct signature.
    char * buffer = (char *)arHeaderBuffer->getBytesNoCopy();
    if (strcmp(buffer, kAppleRAIDSignature)) {

	if (arHeaderOffset) {
	    arHeaderOffset = 0;	// try for old v1 header at beginning of disk
	    goto readheader;
	}

	if (!isRAIDSet()) {
	    const OSString * diskname = getDiskName();
	    if (diskname) IOLog("AppleRAIDMember::readRAIDHeader: failed, no header signature present on %s.\n",
				diskname->getCStringNoCopy());
	}
	
	return kIOReturnUnformattedMedia;
    }

    if (arHeaderOffset) {
	arBaseOffset = 0;
	rc = parseRAIDHeaderV2();
    } else {
	arBaseOffset = kAppleRAIDHeaderSize;
	rc = parseRAIDHeaderV1();
    }
    setHeaderProperty(kAppleRAIDBaseOffsetKey, arBaseOffset, 64);
    setHeaderProperty(kAppleRAIDNativeBlockSizeKey, arNativeBlockSize, 64);

    setProperty(kAppleRAIDMemberUUIDKey, getHeaderProperty(kAppleRAIDMemberUUIDKey));
    setProperty(kAppleRAIDSetUUIDKey, getHeaderProperty(kAppleRAIDSetUUIDKey));

    IOLog1(">>>>> %s %s %s <<<<<\n", getSetNameString(), getSetUUIDString(), getUUIDString());
    IOLog2("AppleRAIDMember::readRAIDHeader(%p): was %ssuccessful\n", this, rc ? "un" : "");
    return rc;
}

IOReturn AppleRAIDMember::writeRAIDHeader(void)
{
    IOLog2("AppleRAIDMember::writeRAIDHeader(%p): entered.\n", this);

    IOReturn rc = kIOReturnSuccess;

    if ((arHeaderBuffer == 0) || (!handleIsOpen(0))) {
	IOLog1("AppleRAIDMember::writeRAIDHeader(%p): aborting, rc = %x.\n", this, kIOReturnIOError);
	return kIOReturnIOError;
    }

    if (arHeaderOffset) {
	rc = buildOnDiskHeaderV2();
    } else {
	rc = buildOnDiskHeaderV1();
    }

    // write the raid header
    if (!rc) {
	arHeaderBuffer->setDirection(kIODirectionOut);
	rc = getTarget()->write(this, arHeaderOffset, arHeaderBuffer);
    }

    // XXX if this fails we should change state or something?
        
    IOLog1("AppleRAIDMember::writeRAIDHeader(%p): finished rc = %x.\n", this, rc);
    return rc;
}

IOReturn AppleRAIDMember::updateRAIDHeader(OSDictionary * props)
{
    // merge props into member's property list
    OSCollectionIterator * iter = OSCollectionIterator::withCollection(props);
    if (!iter) return kIOReturnNoMemory;

    while (const OSString * key = OSDynamicCast(OSString, iter->getNextObject())) {

	// if the key starts with "AppleRAID-" add it to the incore header
	char * match = "AppleRAID-";
	int matchSize = sizeof(match) - 1;
    
	if (!strncmp(match, key->getCStringNoCopy(), matchSize)) {
	    setHeaderProperty(key, props->getObject(key));
	}
    }
    iter->release();

    return kIOReturnSuccess;
}

IOReturn AppleRAIDMember::zeroRAIDHeader(void)
{
    IOLog1("AppleRAIDMember::zeroRAIDHeader(%p): entered.\n", this);

    if (arHeaderBuffer == 0) {
	IOLog1("AppleRAIDMember::zeroRAIDHeader(%p): no header buffer?\n", this);
	return kIOReturnIOError;
    }

    // don't need to worry about read only/read-write state
    // the member being removed from the set
    bool alreadyOpen = handleIsOpen(0);
    if ((!alreadyOpen) && (!getTarget()->open(this, 0, kIOStorageAccessReaderWriter))) {
	IOLog("AppleRAIDMember::zeroRAIDHeader: failed trying to open member %s.\n", getUUIDString());
	return kIOReturnIOError;
    }

    char * header = (char *)arHeaderBuffer->getBytesNoCopy();
    bzero(header, arHeaderBuffer->getLength());

    // zero out the raid header
    arHeaderBuffer->setDirection(kIODirectionOut);
    IOReturn rc = getTarget()->write(this, arHeaderOffset, arHeaderBuffer);

    if (!alreadyOpen) getTarget()->close(this, 0);

    arIsRAIDMember = false;
    
    IOLog1("AppleRAIDMember::zeroRAIDHeader(%p): exiting, rc = %x.\n", this, rc);
    return rc;
}


//***************************************************************************************************************
//***************************************************************************************************************
//***************************************************************************************************************

#define ByteSwapHeaderV1(header) \
{ \
        (header)->raidHeaderSize	= OSSwapBigToHostInt32((header)->raidHeaderSize); \
	(header)->raidHeaderVersion	= OSSwapBigToHostInt32((header)->raidHeaderVersion); \
        (header)->raidHeaderSequence	= OSSwapBigToHostInt32((header)->raidHeaderSequence); \
	(header)->raidLevel		= OSSwapBigToHostInt32((header)->raidLevel); \
        (header)->raidMemberCount	= OSSwapBigToHostInt32((header)->raidMemberCount); \
	(header)->raidMemberIndex	= OSSwapBigToHostInt32((header)->raidMemberIndex); \
        (header)->raidChunkSize		= OSSwapBigToHostInt32((header)->raidChunkSize); \
	(header)->raidChunkCount	= OSSwapBigToHostInt32((header)->raidChunkCount); \
}

IOReturn AppleRAIDMember::parseRAIDHeaderV1()
{
    IOLog1("AppleRAIDMember::parseRAIDHeaderV1(%p): entered.\n", this);

    if (isRAIDSet()) return kIOReturnUnsupported;

    AppleRAIDHeaderV1 * header = (AppleRAIDHeaderV1 *)arHeaderBuffer->getBytesNoCopy();

    ByteSwapHeaderV1(header);
    
    // check header size (not that it ever changes)
    if (header->raidHeaderSize != kAppleRAIDHeaderSize) return kIOReturnUnformattedMedia;
    
    // Make sure the header version is understood.
    if (header->raidHeaderVersion != kAppleRAIDHeaderV1_0_0) return kIOReturnUnformattedMedia;
    setHeaderProperty(kAppleRAIDHeaderVersionKey, header->raidHeaderVersion, 32);

    // Make sure the header sequence is valid.  0 indicates a spare drive
    if (header->raidHeaderSequence == 0) {
	IOLog1("AppleRAIDMember::parseRAIDHeaderV1(%p): found a spare.\n", this);
	setHeaderProperty(kAppleRAIDMemberTypeKey, kAppleRAIDSparesKey);
	changeMemberState(kAppleRAIDMemberStateSpare);
    }

    setHeaderProperty(kAppleRAIDSequenceNumberKey, header->raidHeaderSequence, 32);

    switch (header->raidLevel) {
    case kAppleRAIDStripe:
    {
	setHeaderProperty(kAppleRAIDLevelNameKey, kAppleRAIDLevelNameStripe);
	break;
    }
    case kAppleRAIDMirror:
    {
	setHeaderProperty(kAppleRAIDLevelNameKey, kAppleRAIDLevelNameMirror);
	break;
    }
    default:
	const OSString * diskname = getDiskName();
	if (diskname) IOLog("AppleRAIDMember::parseRAIDHeaderV1: unknown raid type on %s.\n", diskname->getCStringNoCopy());
	return kIOReturnUnformattedMedia;
    }

    char tmpString[kAppleRAIDMaxUUIDStringSize];
    uuid_unparse(header->raidUUID, tmpString);
    setHeaderProperty(kAppleRAIDSetUUIDKey, (char *)tmpString);

    setHeaderProperty(kAppleRAIDSetNameKey, (char *)header->raidSetName);

    setHeaderProperty(kAppleRAIDMemberCountKey, header->raidMemberCount, 32);
    setHeaderProperty(kAppleRAIDMemberIndexKey, header->raidMemberIndex, 32);
    setHeaderProperty(kAppleRAIDChunkSizeKey, header->raidChunkSize, 64);
    
    UInt64 chunkCount = header->raidChunkCount;
    if (header->raidLevel == kAppleRAIDStripe) {
	chunkCount /= header->raidMemberCount;
    }    
    setHeaderProperty(kAppleRAIDChunkCountKey, chunkCount, 64);

    if (header->raidLevel == kAppleRAIDMirror) {

	setHeaderProperty(kAppleRAIDSetTimeoutKey, 30, 32);
	setHeaderProperty(kAppleRAIDSetAutoRebuildKey, kOSBooleanFalse);
    }

    // fake up the member unique name for old style headers (but only once)
    if (!getUUID()) {
	uuid_t fakeUUID;
	uuid_generate(fakeUUID);
	uuid_unparse(fakeUUID, tmpString);
	setHeaderProperty(kAppleRAIDMemberUUIDKey, (char *)tmpString);
    }

    return kIOReturnSuccess;
}

IOReturn AppleRAIDMember::buildOnDiskHeaderV1(void)
{
    AppleRAIDHeaderV1 * header = (AppleRAIDHeaderV1 *)arHeaderBuffer->getBytesNoCopy();

    OSNumber * number = OSDynamicCast(OSNumber, getHeaderProperty(kAppleRAIDSequenceNumberKey));
    if (number) header->raidHeaderSequence = number->unsigned32BitValue();

    ByteSwapHeaderV1(header);

    return kIOReturnSuccess;
}


//***************************************************************************************************************


IOReturn AppleRAIDMember::parseRAIDHeaderV2()
{
    IOLog1("AppleRAIDMember::parseRAIDHeaderV2(%p): entered.\n", this);

    AppleRAIDHeaderV2 * headerBuffer = (AppleRAIDHeaderV2 *)arHeaderBuffer->getBytesNoCopy();

    OSString * errmsg = 0;
    OSDictionary * props = OSDynamicCast(OSDictionary, OSUnserializeXML(headerBuffer->plist, &errmsg));
    if (!props) {
	if (errmsg) IOLog("AppleRAIDMember::parseRAIDHeaderV2 - RAID header parsing failed with %s\n", errmsg->getCStringNoCopy());
	errmsg->release();
	return kIOReturnBadArgument;
    }

    // merge in member's property list
    IOReturn rc = updateRAIDHeader(props);
    props->release();
    if (rc) return rc;

    // ok, we are done parsing the header
    // set up a few extra things

    OSArray * members = OSDynamicCast(OSArray, getHeaderProperty(kAppleRAIDMembersKey));
    if (members) setHeaderProperty(kAppleRAIDMemberCountKey, members->getCount(), 32);

    OSString * tmpString = OSDynamicCast(OSString, getHeaderProperty(kAppleRAIDMemberTypeKey));
    if (tmpString->isEqualTo(kAppleRAIDSparesKey)) {
	IOLog1("AppleRAIDMember::parseRAIDHeaderV2(%p): found a spare.\n", this);
	changeMemberState(kAppleRAIDMemberStateSpare);
    }

    return kIOReturnSuccess;
}

IOReturn AppleRAIDMember::buildOnDiskHeaderV2(void)
{
    IOLog2("AppleRAIDMember::buildOnDiskHeaderV2(%p) entered\n", this);

    // make a copy of incore header and filter out the internal stuff
    OSDictionary * copy = OSDictionary::withCapacity(arHeader->getCount());
    if (!copy) return kIOReturnNoMemory;
    OSCollectionIterator * iter = OSCollectionIterator::withCollection(arHeader);
    if (!iter) return kIOReturnNoMemory;

    while (const OSString * key = OSDynamicCast(OSString, iter->getNextObject())) {

	// if the key starts with "AppleRAID-" copy it
	char * match = "AppleRAID-";
	int matchSize = sizeof(match) - 1;
    
	if (!strncmp(match, key->getCStringNoCopy(), matchSize)) {
	    copy->setObject(key, arHeader->getObject(key));
	}
    }
    iter->release();

    // serialize header to on-disk format
    OSSerialize * s = OSSerialize::withCapacity(kAppleRAIDHeaderSize);
    if (!s) return kIOReturnNoMemory;

    s->clearText();
    if (!copy->serialize(s)) return kIOReturnInternalError;
    copy->release();

    if (s->getLength() >= (kAppleRAIDHeaderSize - sizeof(AppleRAIDHeaderV2))) return kIOReturnNoResources;

    AppleRAIDHeaderV2 * headerBuffer = (AppleRAIDHeaderV2 *)arHeaderBuffer->getBytesNoCopy();

    // set up header header
    strncpy(headerBuffer->raidSignature, kAppleRAIDSignature, 16);
    strncpy(headerBuffer->raidUUID, getSetUUIDString(), 64);
    strncpy(headerBuffer->memberUUID, getUUIDString(), 64);

    // calculate the byte size for the raid data
    OSNumber * number = OSDynamicCast(OSNumber, getHeaderProperty(kAppleRAIDChunkCountKey));
    if (!number) return kIOReturnInternalError;
    headerBuffer->size = number->unsigned64BitValue();

    number = OSDynamicCast(OSNumber, getHeaderProperty(kAppleRAIDChunkSizeKey));
    if (!number) return kIOReturnInternalError;
    headerBuffer->size *= number->unsigned64BitValue();
    if (headerBuffer->size == 0) return kIOReturnInternalError;

    bcopy(s->text(), headerBuffer->plist, s->getLength());
    UInt32 bzSize = kAppleRAIDHeaderSize - (UInt32)((char *)headerBuffer->plist - (char *)headerBuffer) - s->getLength();
    bzero(headerBuffer->plist + s->getLength(), bzSize);

    s->release();

    IOLog2("AppleRAIDMember::buildOnDiskHeaderV2(%p) successful.\n", this);
    return kIOReturnSuccess;
}    


//***************************************************************************************************************
//***************************************************************************************************************
//***************************************************************************************************************


OSDictionary * AppleRAIDMember::getHeader()
{
    return arHeader;
}

OSObject * AppleRAIDMember::getHeaderProperty(const OSString * aKey) const
{
    return arHeader->getObject(aKey);
}

OSObject * AppleRAIDMember::getHeaderProperty(const char * aKey) const
{
    return arHeader->getObject(aKey);
}

bool AppleRAIDMember::setHeaderProperty(const OSString * aKey, OSObject * anObject)
{
    return arHeader->setObject(aKey, anObject);
}

bool AppleRAIDMember::setHeaderProperty(const char * aKey, OSObject * anObject)
{
    return arHeader->setObject(aKey, anObject);
}

bool AppleRAIDMember::setHeaderProperty(const char * aKey, const char * cString)
{
    bool success = false;
    
    OSString * aString = OSString::withCString(cString);
    if (aString) {
	success = arHeader->setObject(aKey, aString);
	aString->release();
    }
    
    return success;
}

bool AppleRAIDMember::setHeaderProperty(const char * key, unsigned long long value, unsigned int numberOfBits)
{
    bool success = false;
    
    OSNumber * number = OSNumber::withNumber(value, numberOfBits);
    if (number) {
	success = arHeader->setObject(key, number);
	number->release();
    }
    
    return success;
}


//***************************************************************************************************************
//***************************************************************************************************************
//***************************************************************************************************************


const OSString * AppleRAIDMember::getSetName(void)
{
    return OSDynamicCast(OSString, getHeaderProperty(kAppleRAIDSetNameKey));
}

const char * AppleRAIDMember::getSetNameString(void)
{
    const OSString * name = getSetName();

    return name ? name->getCStringNoCopy() : "--internal error, set name not set--";
}

const OSString * AppleRAIDMember::getUUID(void)
{
    return OSDynamicCast(OSString, getHeaderProperty(kAppleRAIDMemberUUIDKey));
}

const char * AppleRAIDMember::getUUIDString(void)
{
    const OSString * uuid = getUUID();

    return uuid ? uuid->getCStringNoCopy() : "--internal error, uuid not set--";
}

const OSString * AppleRAIDMember::getSetUUID(void)
{
    return OSDynamicCast(OSString, getHeaderProperty(kAppleRAIDSetUUIDKey));
}

const char * AppleRAIDMember::getSetUUIDString(void)
{
    const OSString * uuid = getSetUUID();

    return uuid ? uuid->getCStringNoCopy() : "--internal error, set uuid not set--";
}

const OSString * AppleRAIDMember::getDiskName(void)
{
    return OSDynamicCast(OSString, arTarget->getProperty(kIOBSDNameKey));
}

//***************************************************************************************************************
//***************************************************************************************************************
//***************************************************************************************************************


IOStorage * AppleRAIDMember::getTarget(void) const
{
    return (IOStorage *)arTarget;
}

bool AppleRAIDMember::isRAIDSet(void)
{
    return false;
}

bool AppleRAIDMember::isRAIDMember(void)
{
    return arIsRAIDMember;
}

bool AppleRAIDMember::isSpare(void)
{
    return getMemberState() == kAppleRAIDMemberStateSpare;
}

bool AppleRAIDMember::isBroken(void)
{
    return getMemberState() == kAppleRAIDMemberStateBroken;
}

UInt64 AppleRAIDMember::getSize() const
{
    return arTarget->getSize();
}

UInt32 AppleRAIDMember::getMemberIndex(void)
{
    OSNumber * number = OSDynamicCast(OSNumber, getHeaderProperty(kAppleRAIDMemberIndexKey));
    return number ? number->unsigned32BitValue() : 0xffffffff;
}

//***************************************************************************************************************

// IOMedia clones

bool AppleRAIDMember::isEjectable() const
{
    return arIsEjectable;
}

bool AppleRAIDMember::isWritable() const
{
    return arIsWritable;
}

UInt64 AppleRAIDMember::getPreferredBlockSize() const
{
    return arNativeBlockSize;
}

UInt64 AppleRAIDMember::getBase() const
{
    return arBaseOffset;
}

//***************************************************************************************************************

#include <IOKit/IODeviceTreeSupport.h>

bool
AppleRAIDMember::addBootDeviceInfo(OSArray * bootArray)
{
    int length = kAppleRAIDMaxOFPath;
    char ofPath[kAppleRAIDMaxOFPath];

    // get the path
    if (!arTarget || !arTarget->getPath(ofPath, &length, gIODTPlane)) {
	return false;
    }

    UInt64 memberSize = arTarget->getSize();    

    IOLog2("AppleRAIDMember::addBootDeviceInfo %p, path = %s, size %llu\n", this, ofPath, memberSize);

    OSString * string = OSString::withCString(ofPath);
    OSNumber * number = OSNumber::withNumber(memberSize, 64);
    
    if (bootArray && string && number) {
	OSDictionary * dict = OSDictionary::withCapacity(2);
	if (dict) {
	    dict->setObject(kIOBootDevicePathKey, string);
	    dict->setObject(kIOBootDeviceSizeKey, number);
	    bootArray->setObject(dict);
	    
	    string->release();
	    number->release();
	    dict->release();
	    
	    return true;
	}
    }

    return false;
}

//***************************************************************************************************************

OSDictionary * AppleRAIDMember::getMemberProperties(void)
{
    OSDictionary * props = OSDictionary::withCapacity(16);
    if (!props) return NULL;

    props->setObject(kAppleRAIDHeaderVersionKey, getHeaderProperty(kAppleRAIDHeaderVersionKey));
    props->setObject(kAppleRAIDMemberUUIDKey, getHeaderProperty(kAppleRAIDMemberUUIDKey));
    props->setObject(kAppleRAIDSequenceNumberKey, getHeaderProperty(kAppleRAIDSequenceNumberKey));
    props->setObject(kAppleRAIDMemberIndexKey, getHeaderProperty(kAppleRAIDMemberIndexKey));
    props->setObject(kAppleRAIDChunkCountKey, getHeaderProperty(kAppleRAIDChunkCountKey));
    props->setObject(kAppleRAIDMemberTypeKey, getHeaderProperty(kAppleRAIDMemberTypeKey));

    props->setObject(kAppleRAIDMemberStatusKey, getProperty(kAppleRAIDMemberStatusKey));
    props->setObject(kAppleRAIDRebuildStatus, getProperty(kAppleRAIDRebuildStatus));

    props->setObject(kIOBSDNameKey, getDiskName());

    // broken members get spared, but we really want them to look broken here
    // scan to see if this members UUID matches in the member list
    if (arMemberState == kAppleRAIDMemberStateSpare) {
	const OSString * uuid = getUUID();
	OSArray * memberUUIDs = OSDynamicCast(OSArray, getHeaderProperty(kAppleRAIDMembersKey));
	if (uuid && memberUUIDs) {
	    UInt32 memberCount = memberUUIDs->getCount();
	    for (UInt32 i = 0; i < memberCount; i++) {
		if (uuid->isEqualTo(memberUUIDs->getObject(i))) {
		    OSString * statusString = OSString::withCString(kAppleRAIDStatusFailed);
		    if (statusString) props->setObject(kAppleRAIDMemberStatusKey, statusString);
		    break;
		}
	    }
	}
    }
    
    return props;
}


//***************************************************************************************************************
//***************************************************************************************************************
//***************************************************************************************************************


bool AppleRAIDMember::changeMemberState(UInt32 newState, bool force)
{
    bool	swapState = false;
    char	*newStatus = "bogus";

#ifdef DEBUG
    const char	*oldStatus = "not set";
    OSString    *oldStatusString = OSDynamicCast(OSString, getProperty(kAppleRAIDMemberStatusKey));
    if (oldStatusString) oldStatus = oldStatusString->getCStringNoCopy();
#endif

    if (arMemberState == newState) return true;

    switch (newState) {
	
    case kAppleRAIDMemberStateBroken : // 0
	swapState = true;
	newStatus = kAppleRAIDStatusFailed;
	break;
        
    case kAppleRAIDMemberStateSpare : // 1
	swapState = arMemberState <= kAppleRAIDMemberStateClosed;
	newStatus = kAppleRAIDStatusSpare;
	break;
        
    case kAppleRAIDMemberStateClosed : // 2
	swapState = arMemberState >= kAppleRAIDMemberStateClosing;
	newStatus = kAppleRAIDStatusOffline;
	break;

    case kAppleRAIDMemberStateClosing : // 3
	swapState = arMemberState >= kAppleRAIDMemberStateClosing;
	newStatus = kAppleRAIDStatusOffline;
	break;
        
    case kAppleRAIDMemberStateRebuilding : // 4
	swapState = arMemberState == kAppleRAIDMemberStateSpare;
	newStatus = kAppleRAIDStatusRebuilding;
	break;

    case kAppleRAIDMemberStateOpen : // 5
	newStatus = kAppleRAIDStatusOnline;
	if (arMemberState == kAppleRAIDMemberStateRebuilding) break;  // leave as is
	swapState = arMemberState >= kAppleRAIDMemberStateClosed;
	break;
    }
    
    if (swapState) {

	IOLog2("AppleRAIDMember::changeMemberState(%p) from %lu (%s) to %lu (%s) isSet = %s.\n",
	       this, arMemberState, oldStatus, newState, newStatus, isRAIDSet() ? "yes":"no");
	arMemberState = newState;

    } else {

#ifdef DEBUG
	if (arMemberState != newState) {
	    IOLog1("AppleRAIDMember::changeMemberState(%p) %s from %lu (%s) to %lu (%s) isSet = %s.\n",
		   this, force ? "FORCED" : "FAILED", arMemberState, oldStatus, newState, newStatus, isRAIDSet() ? "yes":"no");
	}
#endif
	if (force) arMemberState = newState;
    }

    if ((swapState || force) && isRAIDMember()) setProperty(kAppleRAIDMemberStatusKey, newStatus);
    
    return swapState;
}


