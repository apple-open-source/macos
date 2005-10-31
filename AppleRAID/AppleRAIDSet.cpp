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

#define super AppleRAIDMember
OSDefineMetaClassAndAbstractStructors(AppleRAIDSet, AppleRAIDMember);

void AppleRAIDSet::free(void)
{
    if (arOpenReaders)		arOpenReaders->release();

    if (arMembers)		IODelete(arMembers, AppleRAIDMember *, arLastAllocCount);
    if (arSpareMembers)		IODelete(arSpareMembers, AppleRAIDMember *, arLastAllocCount);

    if (arStorageRequestPool) {
        while (1) {
	    AppleRAIDStorageRequest * storageRequest;
	    storageRequest = (AppleRAIDStorageRequest *)arStorageRequestPool->getCommand(false);
            if (storageRequest == 0) break;
            storageRequest->release();
        }
        arStorageRequestPool->release();
	arStorageRequestPool = 0;
    }
    
    if (arSetCommandGate != 0) {
        arSetWorkLoop->removeEventSource(arSetCommandGate);
        arSetCommandGate->release();
        arSetCommandGate = 0;
    }
    if (arSetEventSource != 0) {
        arSetWorkLoop->removeEventSource(arSetEventSource);
        arSetEventSource->release();
        arSetEventSource = 0;
    }
    if (arSetWorkLoop != 0) arSetWorkLoop->release();
    arSetWorkLoop = 0;

    if (arRecoveryThreadCall) thread_call_free(arRecoveryThreadCall);
    arRecoveryThreadCall = 0;

    super::free();
}

// once only init
bool AppleRAIDSet::init()
{
    if (super::init() == false) return false;
    
    arSetState = kAppleRAIDSetStateInitializing;
    setProperty(kAppleRAIDStatusKey, kAppleRAIDStatusOffline);
    
    arMemberCount	= 0;
    arLastAllocCount	= 0;
    arSequenceNumber	= 0;
    arMedia		= NULL;
    arPublishedSetState = kAppleRAIDSetStateInitializing;
    arOpenLevel		= kIOStorageAccessNone;
    arOpenReaders	= OSSet::withCapacity(1);
    arOpenReaderWriter	= 0;

    arSetCompleteTimeout = kARSetCompleteTimeoutNone;
    arSetBlockCount	= 0;
    arSetMediaSize	= 0;

    arMembers		= 0;
    arSpareMembers	= 0;

    arSetIsSyncingCount	= 0;

    // Get the WorkLoop.
    if (getWorkLoop() != 0) {
	arSetCommandGate = IOCommandGate::commandGate(this);
	if (arSetCommandGate != 0) {
	    getWorkLoop()->addEventSource(arSetCommandGate);
	}
                
	AppleRAIDEventSource::Action completeRequestMethod = OSMemberFunctionCast(AppleRAIDEventSource::Action, this, &AppleRAIDSet::completeRAIDRequest);
	arSetEventSource = AppleRAIDEventSource::withAppleRAIDSet(this, completeRequestMethod);
	if (arSetEventSource != 0) {
	    getWorkLoop()->addEventSource(arSetEventSource);
	}
    }

    thread_call_func_t recoverMethod = OSMemberFunctionCast(thread_call_func_t, this, &AppleRAIDSet::recover);
    arRecoveryThreadCall = thread_call_allocate(recoverMethod, (thread_call_param_t)this);
    if (arRecoveryThreadCall == 0) return false;

    arAllocateRequestMethod = (IOCommandGate::Action)0xdeadbeef;
    
    return true;
}

bool AppleRAIDSet::initWithHeader(OSDictionary * header, bool firstTime)
{
    if (!header) return false;

    OSString * string;
    string = OSDynamicCast(OSString, header->getObject(kAppleRAIDSetNameKey));
    if (string) setProperty(kAppleRAIDSetNameKey, string);

    string = OSDynamicCast(OSString, header->getObject(kAppleRAIDSetUUIDKey));
    if (string) {
	setProperty(kAppleRAIDMemberUUIDKey, string);	// the real uuid for this set
	setProperty(kAppleRAIDSetUUIDKey, string);	// is overridden in addMember if stacked
    } else {
	if (firstTime) return false;
    }
    // this is only in v2 headers, the spare list is built on the fly
    OSArray * members = OSDynamicCast(OSArray, header->getObject(kAppleRAIDMembersKey));
    if (members) setProperty(kAppleRAIDMembersKey, members);

    OSNumber * number;
    number = OSDynamicCast(OSNumber, header->getObject(kAppleRAIDHeaderVersionKey));
    if (number) {
	arHeaderVersion = number->unsigned32BitValue();
    } else {
	if (firstTime) return false;
    }
	
    number = OSDynamicCast(OSNumber, header->getObject(kAppleRAIDSequenceNumberKey));
    if (number) {
	arSequenceNumber = number->unsigned32BitValue();
    } else {
	if (firstTime) return false;
    }

    number = OSDynamicCast(OSNumber, header->getObject(kAppleRAIDChunkSizeKey));
    if (number) {
	arSetBlockSize = number->unsigned64BitValue();
    } else {
	if (firstTime) return false;
    }

    // not really in v2 header 
    number = OSDynamicCast(OSNumber, header->getObject(kAppleRAIDMemberCountKey));
    if (number) {
	arMemberCount = number->unsigned32BitValue();
    } else {
	if (firstTime) return false;
    }

    number = OSDynamicCast(OSNumber, header->getObject(kAppleRAIDBaseOffsetKey));
    if (number) {
	arBaseOffset = number->unsigned64BitValue();
    } else {
	if (firstTime) return false;
    }
    
    number = OSDynamicCast(OSNumber, header->getObject(kAppleRAIDNativeBlockSizeKey));
    if (number) {
	arNativeBlockSize = number->unsigned64BitValue();
    } else {
	if (firstTime) return false;
    }

    // don't care if these fail
    setProperty(kAppleRAIDSetAutoRebuildKey, header->getObject(kAppleRAIDSetAutoRebuildKey));
    setProperty(kAppleRAIDSetContentHintKey, header->getObject(kAppleRAIDSetContentHintKey));
    setProperty(kAppleRAIDSetTimeoutKey, header->getObject(kAppleRAIDSetTimeoutKey));

    setProperty(kAppleRAIDCanAddMembersKey, header->getObject(kAppleRAIDCanAddMembersKey));
    setProperty(kAppleRAIDCanAddSparesKey, header->getObject(kAppleRAIDCanAddSparesKey));
    setProperty(kAppleRAIDRemovalAllowedKey, header->getObject(kAppleRAIDRemovalAllowedKey));
    setProperty(kAppleRAIDSizesCanVaryKey, header->getObject(kAppleRAIDSizesCanVaryKey));

    return true;
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

bool AppleRAIDSet::addSpare(AppleRAIDMember * member)
{
    IOLog1("AppleRAIDSet::addSpare(%p) entered, arSpareCount was %d.\n", member, (int)arSpareCount);

    assert(gAppleRAIDGlobals.islocked());

    // only take as many spares as members
    if (arSpareCount >= arMemberCount) return false;

    if (!this->attach(member)) {
	IOLog1("AppleRAIDSet::addSpare(%p) this->attach(%p) failed\n", this, member);
	member->changeMemberState(kAppleRAIDMemberStateBroken);
	return false;
    }

    arSpareMembers[arSpareCount] = member;
    arSpareCount++;

    return true;
}

bool AppleRAIDSet::addMember(AppleRAIDMember * member)
{
    IOLog1("AppleRAIDSet::addMember(%p) called\n", member);

    assert(gAppleRAIDGlobals.islocked());

    if (member->isBroken()) return false;

    // deal with spare members separately
    if (member->isSpare()) return false;

    // new members should be closed ...
    if (member->getMemberState() != kAppleRAIDMemberStateClosed) {
	IOLog1("AppleRAIDSet::addMember(%p) member is not closed.\n", member);
	member->changeMemberState(kAppleRAIDMemberStateBroken);
	return false;
    }
    
    // check the current state of the raid set, can we accept another member?
    if (arActiveCount >= arMemberCount) {
	IOLog("AppleRAIDSet::addMember() too many members, active = %lu, count = %lu, member = %s\n",
	      arActiveCount, arMemberCount, member->getUUIDString());
	member->changeMemberState(kAppleRAIDMemberStateSpare);
        return false;
    }

    // double check, can the set take more members?
    // degraded sets should reject new members unless paused
    if (getSetState() >= kAppleRAIDSetStateOnline && !arSetIsPaused) {
	IOLog("AppleRAIDSet::addMember() set already started, ignoring new member %s\n", member->getUUIDString());
	member->changeMemberState(kAppleRAIDMemberStateSpare);
        return false;
    }

    OSNumber * number;
    number = OSDynamicCast(OSNumber, member->getHeaderProperty(kAppleRAIDHeaderVersionKey));
    if (!number) return false;
    UInt32 memberHeaderVersion = number->unsigned32BitValue();

    // double check that this member is a part of the set, only for v2 headers
    OSArray * memberUUIDs = OSDynamicCast(OSArray, getProperty(kAppleRAIDMembersKey));
    if (memberUUIDs) {
	const OSString * uuid = member->getUUID();
	if (!uuid) return false;
	bool foundit = false;
	for (UInt32 i = 0; i < arMemberCount; i++) {

	    if (uuid->isEqualTo(memberUUIDs->getObject(i))) {
		foundit = arMembers[i] == 0;
	    }
	    if (foundit) break;
	}
	if (!foundit) return false;
    }

    // no mix and match header versions
    if (memberHeaderVersion != arHeaderVersion) {
	IOLog("AppleRAIDSet::addMember() header version mismatch for member %s\n",
	      member->getUUIDString());
	// just punt, this is fatal and requires user interaction
	// it is possible to get here during a failed set upgrade
	changeSetState(kAppleRAIDSetStateFailed);
	member->changeMemberState(kAppleRAIDMemberStateBroken);
	return false;
    }

    number = OSDynamicCast(OSNumber, member->getHeaderProperty(kAppleRAIDSequenceNumberKey));
    if (!number) return false;
    UInt32 memberSequenceNumber = number->unsigned32BitValue();
    UInt32 memberIndex = member->getMemberIndex();
    
    // Don't use members that have sequence numbers older than the raid set.
    if (memberSequenceNumber < arSequenceNumber) {
	IOLog("AppleRAIDSet::addMember() detected expired sequenceNumber (%lu) for member %s\n",
	      memberSequenceNumber, member->getUUIDString());
	member->changeMemberState(kAppleRAIDMemberStateSpare);
        return false;
    }

    // If this new member is newer than the others then remove the old ones.
    if (memberSequenceNumber > arSequenceNumber) {
        for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
	    if (arMembers[cnt] != 0) {

		OSNumber * number = OSDynamicCast(OSNumber, arMembers[cnt]->getHeaderProperty(kAppleRAIDSequenceNumberKey));
		if (number) {
		    IOLog("AppleRAIDSet::addMember() detected expired sequenceNumber (%u) for member %s\n",
			  number->unsigned32BitValue(), arMembers[cnt]->getUUIDString());
		}
		AppleRAIDMember * expiredMember = arMembers[cnt];
		removeMember(arMembers[cnt], 0);
		expiredMember->changeMemberState(kAppleRAIDMemberStateSpare);
		addSpare(expiredMember);
            }
	}

	// we may have removed everything
	if (arActiveCount == 0) {
	    arSetState = kAppleRAIDSetStateInitializing;
	    setProperty(kAppleRAIDStatusKey, kAppleRAIDStatusOffline);
	    initWithHeader(member->getHeader(), true);
	}
										   
	// Update the raid set's sequence number.
	arSequenceNumber = memberSequenceNumber;

	// reset the block count
	arSetBlockCount = 0;
    }
    
    // Make sure this is the only member in this slot.
    if (arMembers[memberIndex] != 0) {
	IOLog("AppleRAIDSet::addMember() detected the same member index (%lu) twice?\n", memberIndex);
	// take the entire set set offline, this is fatal
	changeSetState(kAppleRAIDSetStateFailed);
	member->changeMemberState(kAppleRAIDMemberStateBroken);
        return false;
    }

    //
    // at this point we should have a valid member
    //

    // Save the AppleRAIDMember in the raid set.
    arMembers[memberIndex] = member;

    if (!this->attach(member)) {
	IOLog1("AppleRAIDSet::addMember(%p) this->attach(%p) failed\n", this, member);
	member->changeMemberState(kAppleRAIDMemberStateBroken);
	return false;
    }

    // Count this member as started.
    arActiveCount++;

    IOLog1("AppleRAIDSet::addMember(%p) was successful.\n", member);

    return true;
}

// this also removes spares

bool AppleRAIDSet::removeMember(AppleRAIDMember * member, IOOptionBits options)
{
    IOLog1("AppleRAIDSet::removeMember(%p) called\n", member);
    
    assert(gAppleRAIDGlobals.islocked());

    bool shouldBeClosed = member->changeMemberState(kAppleRAIDMemberStateClosing);

    // spares are not open
    if (shouldBeClosed) member->close(this, options);
    
    member->changeMemberState(kAppleRAIDMemberStateClosed);

    UInt32 memberIndex = member->getMemberIndex();
	
    if (arMembers[memberIndex] == member) {
	arMembers[memberIndex] = 0;
	arActiveCount--;
    }
    
    for (UInt32 i=0; i < arSpareCount; i++) {

	if (arSpareMembers[i] == member) {

	    arSpareCount--;
	    
	    // slide in the remaining spares
	    for (UInt32 j=i; j < arSpareCount; j++) {
		arSpareMembers[j] = arSpareMembers[j+1];
	    }
	    arSpareMembers[arSpareCount] = 0;
	}
    }

    this->detach(member);

    return true;
}


bool AppleRAIDSet::upgradeMember(AppleRAIDMember *member)
{
    IOLog1("AppleRAIDSet::upgradeMember(%p) entered.\n", this);
    
    // this is running in the workloop (when called from rebuildComplete)
    
    // the set is paused
    assert(arSetIsPaused);
    
    // update member & spare uuid lists in raid headers, only for v2 headers
    OSArray * memberUUIDs = OSDynamicCast(OSArray, getProperty(kAppleRAIDMembersKey));
    OSArray * spareUUIDs = OSDynamicCast(OSArray, getProperty(kAppleRAIDSparesKey));
    for (UInt32 i = 0; i < arMemberCount; i++) {
	if (arMembers[i]) {
	    arMembers[i]->setHeaderProperty(kAppleRAIDMembersKey, memberUUIDs);
	    arMembers[i]->setHeaderProperty(kAppleRAIDSparesKey, spareUUIDs);
	}
    }

    // fix up the member
    member->setHeaderProperty(kAppleRAIDMemberTypeKey, kAppleRAIDMembersKey);
    member->setHeaderProperty(kAppleRAIDSequenceNumberKey, arSequenceNumber, 32);
    member->setHeaderProperty(kAppleRAIDMembersKey, memberUUIDs);
    member->setHeaderProperty(kAppleRAIDSparesKey, spareUUIDs);

    // add member into the raid set (special cased for paused sets)
    if (!addMember(member)) return false;

    // force it open (if needed)
    if (arOpenReaderWriter || arOpenReaders->getCount()) {
	IOStorageAccess level = arOpenReaderWriter ? kIOStorageAccessReaderWriter : kIOStorageAccessReader;
	IOLog1("AppleRAIDSet::upgradeMember(%p) opening for read%s.\n", this, arOpenReaderWriter ? "/write" : " only");
	if (!member->open(this, 0, level)) {
	    IOLog("AppleRAIDSet::upgradeMember(%p) open failed.\n", this);
	    return false;
	}
    }

    return true;
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888


bool AppleRAIDSet::resizeSet(UInt32 newMemberCount)
{
    AppleRAIDMember	**oldMembers = 0;
    AppleRAIDMember	**oldSpareMembers = 0;

    IOLog1("AppleRAIDSet::resizeSet(%p) entered. alloc = %d old = %d new = %d\n",
	   this, (int)arLastAllocCount, (int)arMemberCount, (int)newMemberCount);
    
    UInt32 oldMemberCount = arMemberCount;

    // if downsizing, just hold on the extra space
    if (arLastAllocCount && (arLastAllocCount >= newMemberCount)) {
	arMemberCount = newMemberCount;
	// zero out the deleted stuff;
	for (UInt32 i = newMemberCount; i < arLastAllocCount; i++) {
	    arMembers[i] = NULL;
	}
	return true;
    }

    // back up the old member info if we need to increase the set size
    if (arLastAllocCount) {
	oldMembers = arMembers;
	oldSpareMembers = arSpareMembers;
    }
    
    arMembers = IONew(AppleRAIDMember *, newMemberCount);
    arSpareMembers = IONew(AppleRAIDMember *, newMemberCount);
    if (!arMembers || !arSpareMembers) return false;
            
    // Clear the new arrays.
    bzero(arMembers, sizeof(AppleRAIDMember *) * newMemberCount);
    bzero(arSpareMembers, sizeof(AppleRAIDMember *) * newMemberCount);

    // copy the old into the new, if needed
    if (arLastAllocCount) {
	bcopy(oldMembers, arMembers, sizeof(AppleRAIDMember *) * oldMemberCount);
	bcopy(oldSpareMembers, arSpareMembers, sizeof(AppleRAIDMember *) * oldMemberCount);

	IODelete(oldMembers, AppleRAIDMember *, arLastAllocCount);
	IODelete(oldSpareMembers, AppleRAIDMember *, arLastAllocCount);
    }

    arLastAllocCount = newMemberCount;
    arMemberCount = newMemberCount;

    IOLog1("AppleRAIDSet::resizeSet(%p) successful\n", this);
    
    return true;
}


UInt32 AppleRAIDSet::nextSetState(void)
{
    if (isSetComplete()) {
	return kAppleRAIDSetStateOnline;
    }

    if (arActiveCount == 0 && arSpareCount == 0) {
	IOLog1("AppleRAIDSet::nextSetState: %p is empty, setting state to terminating.\n", this);
	return kAppleRAIDSetStateTerminating;
    } 

    if (getSetState() != kAppleRAIDSetStateInitializing) {
	IOLog1("AppleRAIDSet::nextSetState: set \"%s\" failed to come online.\n", getSetNameString());
    }

    return kAppleRAIDSetStateInitializing;
}


bool AppleRAIDSet::startSet(void)
{
    IOLog1("AppleRAIDSet::startSet %p called with %lu of %lu members (%lu spares).\n",
	   this, arActiveCount, arMemberCount, arSpareCount);

    // if terminating, stay that way
    if (getSetState() <= kAppleRAIDSetStateTerminating) {
	IOLog1("AppleRAIDSet::startSet: the set \"%s\" is terminating or broken (%d).\n", getSetNameString(), (int)getSetState());
	return false;
    }

    // update set status
    UInt32 nextState = nextSetState();
    changeSetState(nextState);
    if (nextState < kAppleRAIDSetStateOnline) {
	IOLog1("AppleRAIDSet::startSet %p was unsuccessful.\n", this);
	return false;
    }
    
    // clean out the old storage requests and their memory descriptors

    if (arStorageRequestPool && arSetIsPaused) {
	assert(arStorageRequestsPending == 0);
        while (1) {
	    AppleRAIDStorageRequest * storageRequest;
	    storageRequest = (AppleRAIDStorageRequest *)arStorageRequestPool->getCommand(false);
            if (storageRequest == 0) break;
            storageRequest->release();
        }
        arStorageRequestPool->release();
	arStorageRequestPool = 0;
    }
    
    // Create and populate the storage request pool.

    if (!arStorageRequestPool) {

	// XXX fix this? looks like the code always calls getCommand with false
	// XXX while already inside the workloop, just use fCommandChain directly ?
	
	arStorageRequestPool = IOCommandPool::withWorkLoop(getWorkLoop());
	if (arStorageRequestPool == 0) return kIOReturnNoMemory;
	for (UInt32 cnt = 0; cnt < kAppleRAIDStorageRequestCount; cnt++) {
	    
	    AppleRAIDStorageRequest * storageRequest = AppleRAIDStorageRequest::withAppleRAIDSet(this);
	    if (storageRequest == 0) break;
	    arStorageRequestPool->returnCommand(storageRequest);
	}
    }
        
    // (re)calculate ejectable and writeable, ...

    arIsWritable = true;
    arIsEjectable = true;
    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {

	// if any members are not ejectable/writable turn off the entire set
	
	if (arMembers[cnt] && (arMembers[cnt]->getMemberState() >= kAppleRAIDMemberStateClosed)) {
	    if (!arMembers[cnt]->isEjectable()) arIsEjectable = false;
	    if (!arMembers[cnt]->isWritable())  arIsWritable  = false;
	}
    }
    
    IOLog1("AppleRAIDSet::startSet %p was successful.\n", this);
    return true;
}

bool AppleRAIDSet::publishSet(void)
{
    IOLog1("AppleRAIDSet::publishSet called %p\n", this);

    // are we (still) connected to the io registry?
    if (arActiveCount == 0 && arSpareCount == 0) {
	IOLog1("AppleRAIDSet::publishSet: the set %p is empty, aborting.\n", this);
	return false;
    }

    // disk arbitration doesn't recognize multiple registrations on same media object
    // so if we switch from offline to online, DA will not re-scan the media object
    // the work around is wack the old media object and start over
    // this code will also wack the media object if the set goes offline
    if (arMedia && (arPublishedSetState < kAppleRAIDSetStateOnline) != (getSetState() < kAppleRAIDSetStateOnline)) {
	unpublishSet();
    }
    
    // Create the member object for the raid set.
    bool firstTime = false;
    if (arMedia) {
	arMedia->retain();
    } else {
	arMedia = new IOMedia;
	firstTime = true;
    }
    
    if (arMedia) {
	const char * contentHint = 0;
	OSString * theHint = OSDynamicCast(OSString, getProperty(kAppleRAIDSetContentHintKey));
	if (theHint) contentHint = theHint->getCStringNoCopy();

	IOMediaAttributeMask attributes = arIsEjectable ? (kIOMediaAttributeEjectableMask | kIOMediaAttributeRemovableMask) : 0;
        if (arMedia->init(/* base               */ 0,
			  /* size               */ arSetMediaSize,
			  /* preferredBlockSize */ arNativeBlockSize,
			  /* attributes         */ attributes,
			  /* isWhole            */ true, 
			  /* isWritable         */ arIsWritable,
			  /* contentHint        */ contentHint)) {
	    
	    arMedia->setName(getSetNameString());

	    // Set a location value (partition number) for this partition.
	    char location[12];
	    sprintf(location, "%ld", 0);
	    arMedia->setLocation(location);
	    
	    OSArray * bootArray = OSArray::withCapacity(arMemberCount);
	    if (bootArray) {
		// if any of the devices are not in the device tree
		// just return an empty array
		(void)addBootDeviceInfo(bootArray);
		arMedia->setProperty(kIOBootDeviceKey, bootArray);
		bootArray->release();
	    }

	    arMedia->setProperty(kIOMediaUUIDKey, (OSObject *)getUUID());
	    arMedia->setProperty(kAppleRAIDIsRAIDKey, kOSBooleanTrue);
	    if (getSetState() < kAppleRAIDSetStateOnline || isRAIDMember()) {
		arMedia->setProperty("autodiskmount", kOSBooleanFalse);
	    } else {
		arMedia->removeProperty("autodiskmount");
	    }

	    if (firstTime) {
		arMedia->attach(this);
		arMedia->registerService();
	    }
	    
	    arPublishedSetState = getSetState();
	}
    } else {
	IOLog("AppleRAIDSet::publishSet(void): failed for set \"%s\" (%s)\n", getSetNameString(), getUUIDString());
    }
	
    if (arMedia) arMedia->release();

    IOLog1("AppleRAIDSet::publishSet: was %ssuccessful.\n", arMedia ? "" : "un");
    return arMedia != NULL;
}


bool AppleRAIDSet::unpublishSet(void)
{
    bool success = true;

    IOLog1("AppleRAIDSet::unpublishSet(%p) entered, arMedia = %p\n", this, arMedia);

    if (arMedia) {
	success = arMedia->terminate(kIOServiceRequired | kIOServiceSynchronous);
	arMedia = 0;
    }

    return success;
}

bool AppleRAIDSet::destroySet(void)
{
    IOReturn rc = kIOReturnSuccess;
    
    IOLog1("AppleRAIDSet::destroySet(%p) entered.\n", this);

    if (isRAIDMember()) {
	IOLog("AppleRAIDSet::destroySet() failed, an attempt was made to destroy subordinate set\n");
	return false;
    }

    for (UInt32 i = 0; i < arMemberCount; i++) {
	if (arMembers[i]) {
	    rc = arMembers[i]->zeroRAIDHeader();
	    if (arMembers[i]->getMemberState() == kAppleRAIDMemberStateRebuilding) {
		arMembers[i]->changeMemberState(kAppleRAIDMemberStateSpare, true);
		while (arMembers[i]->getMemberState() == kAppleRAIDMemberStateSpare) {
		    IOSleep(50);
		}
	    }
	}
	if (arSpareMembers[i]) rc = arSpareMembers[i]->zeroRAIDHeader();
    }

    if (rc) {
	IOLog1("AppleRAIDSet::destroySet(%p) failed.\n", this);
	return false;
    }
    
    // this keeps us from bumping sequence numbers on the way down
    changeSetState(kAppleRAIDSetStateTerminating);

    // take the set offline member by member
    for (UInt32 i = 0; i < arMemberCount; i++) {
	if (arMembers[i]) {
	    if (arMembers[i]->isRAIDSet()) {
		arController->oldMember(arMembers[i]);
	    } else {
		arMembers[i]->stop(NULL);
	    }
	}
	if (arSpareMembers[i]) {
	    if (arSpareMembers[i]->isRAIDSet()) {
		arController->oldMember(arSpareMembers[i]);
	    } else {
		arSpareMembers[i]->stop(NULL);
	    }
	}
    }

    IOLog1("AppleRAIDSet::destroySet(%p) was successful.\n", this);

    return true;
}


bool AppleRAIDSet::reconfigureSet(OSDictionary * updateInfo)
{
    bool updateHeader = false;
    UInt32 newMemberCount = 0;
    
    IOLog1("AppleRAIDSet::reconfigureSet(%p) entered.\n", this);

    OSString * deleted = OSString::withCString(kAppleRAIDDeletedUUID);
    if (!deleted) return false;

    // XXX need to guard against v1 sets getting here?
    
    OSArray * oldMemberList = OSDynamicCast(OSArray, getProperty(kAppleRAIDMembersKey));
    OSArray * newMemberList = OSDynamicCast(OSArray, updateInfo->getObject(kAppleRAIDMembersKey));
    if (oldMemberList && newMemberList) {

	IOLog1("AppleRAIDSet::reconfigureSet(%p) updating member list.\n", this);
	
	// look for kAppleRAIDDeletedUUID
	newMemberCount = arMemberCount;
	for (UInt32 i = 0; i < newMemberCount; i++) {
	    OSString * uuid = OSDynamicCast(OSString, newMemberList->getObject(i));
	    if ((uuid) && (uuid->isEqualTo(deleted))) {

		if (arMembers[i]) {

		    arMembers[i]->zeroRAIDHeader();
		    if (arMembers[i]->getMemberState() == kAppleRAIDMemberStateRebuilding) {
			// hack, this will cause the rebuild to abort 
			arMembers[i]->changeMemberState(kAppleRAIDMemberStateSpare, true);
			while (arMembers[i]->getMemberState() == kAppleRAIDMemberStateSpare) {
			    arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::unpauseSet));
			    IOSleep(50);
			    arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::pauseSet), (void *)false);
			}
		    } else {
			arMembers[i]->stop(NULL);
		    }

		} else {
		    // if the member is broken it might be in the spare list
		    OSString * olduuid = OSDynamicCast(OSString, oldMemberList->getObject(i));
		    for (UInt32 j = 0; j < arSpareCount; j++) {
			if (arSpareMembers[j] && arSpareMembers[j]->getUUID()->isEqualTo(olduuid)) {
			    arSpareMembers[j]->zeroRAIDHeader();
			    arSpareMembers[j]->stop(NULL);
			}
		    }
		}

		// slide everything in one to fill the deleted spot
		newMemberCount--;
		newMemberList->removeObject(i);
		for (UInt32 j = i; j < newMemberCount; j++) {
		    arMembers[j] = arMembers[j + 1];
		    arMembers[j]->setHeaderProperty(kAppleRAIDMemberIndexKey, j, 32);
		}
	    }
	}

	newMemberCount = newMemberList->getCount();

	setProperty(kAppleRAIDMembersKey, newMemberList);
	updateInfo->removeObject(kAppleRAIDMembersKey);
	updateHeader = true;
    }

    OSArray * oldSpareList = OSDynamicCast(OSArray, getProperty(kAppleRAIDSparesKey));
    OSArray * newSpareList = OSDynamicCast(OSArray, updateInfo->getObject(kAppleRAIDSparesKey));
    if (oldSpareList && newSpareList) {

	IOLog1("AppleRAIDSet::reconfigureSet(%p) updating spare list.\n", this);
	
	// look for kAppleRAIDDeletedUUID in new list
	UInt32 spareCount = newSpareList->getCount();
	for (UInt32 i = 0; i < spareCount; i++) {
	    
	    OSString * uuid = OSDynamicCast(OSString, newSpareList->getObject(i));
	    if (!uuid || !uuid->isEqualTo(deleted)) continue;

	    // remove "deleted uuid" from the new list
	    newSpareList->removeObject(i);

	    // get the old uuid based on the position of the deleted uuid marker
	    OSString * olduuid = OSDynamicCast(OSString, oldSpareList->getObject(i));
	    if (!olduuid) return false;

	    // Find && nuke the old spare
	    for (UInt32 j = 0; j < arSpareCount; j++) {
		if (arSpareMembers[j] && arSpareMembers[j]->getUUID()->isEqualTo(olduuid)) {
		    arSpareMembers[j]->zeroRAIDHeader();
		    arSpareMembers[j]->stop(NULL);
		}
	    }
	    
	    break;	// XXX this can only do one delete, the UI allows more
	}
	setProperty(kAppleRAIDSparesKey, newSpareList);
	updateInfo->removeObject(kAppleRAIDSparesKey);
	updateHeader = true;
    }
    deleted->release();

    // pull out the remaining stuff
    if (updateInfo->getCount()) {
	initWithHeader(updateInfo, false);
	updateHeader = true;
    }

    if (newMemberCount) {

	resizeSet(newMemberCount);
	
	// update the shadow member count
	OSNumber * number = OSNumber::withNumber(newMemberCount, 32);
	if (number) {
	    updateInfo->setObject(kAppleRAIDMemberCountKey, number);
	    number->release();
	}
    }

    if (updateHeader) {

	changeSetState(kAppleRAIDSetStateInitializing);

	OSArray * memberUUIDs = OSDynamicCast(OSArray, getProperty(kAppleRAIDMembersKey));
	OSArray * spareUUIDs = OSDynamicCast(OSArray, getProperty(kAppleRAIDSparesKey));
	for (UInt32 i = 0; i < arMemberCount; i++) {
	    if (arMembers[i]) {

		// merge new properties into each member
		arMembers[i]->updateRAIDHeader(updateInfo);

		// update member & spare uuid lists in raid headers, only for v2 headers
		arMembers[i]->setHeaderProperty(kAppleRAIDMembersKey, memberUUIDs);
		arMembers[i]->setHeaderProperty(kAppleRAIDSparesKey, spareUUIDs);
	    }
	}
    }

    return true;
}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

UInt32 AppleRAIDSet::getSequenceNumber()
{
    return arSequenceNumber;
}

void AppleRAIDSet::bumpSequenceNumber(void)
{
    UInt32 cnt;

    assert(gAppleRAIDGlobals.islocked());

    arSequenceNumber++;

    IOLog1("AppleRAIDSet::bumpSequenceNumber(%p) bumping to %lu\n", this, arSequenceNumber);

    for (cnt = 0; cnt < arMemberCount; cnt++) {

	if (arMembers[cnt]) {
	    arMembers[cnt]->setHeaderProperty(kAppleRAIDSequenceNumberKey, arSequenceNumber, 32);
	}
    }
}

IOReturn AppleRAIDSet::writeRAIDHeader(void)
{
    UInt32 cnt;
    IOReturn rc = kIOReturnSuccess, rc2;

    IOLog1("AppleRAIDSet::writeRAIDHeader(%p) entered.\n", this);

    assert(gAppleRAIDGlobals.islocked());

    if ((arActiveCount == 0) || getSetState() <= kAppleRAIDSetStateTerminating) {
	IOLog1("AppleRAIDSet::writeRAIDHeader(%p) ignoring request, the set is empty or broken/terminating.\n", this);
	return rc;
    }

    // opening the set changes it's state
    UInt32 formerSetState = getSetState();

    // we need to be opened for write
    bool openedForWrite = (arOpenReaderWriter != 0);
    bool openedForRead  = arOpenReaders->getCount() != 0;
    if (!openedForWrite) {
	IOLog1("AppleRAIDSet::writeRAIDHeader(%p): opening set for writing.\n", this);
	if (!open(this, 0, kIOStorageAccessReaderWriter)) return kIOReturnIOError;
    }

    for (cnt = 0; cnt < arMemberCount; cnt++) {

	if (!arMembers[cnt] || (arMembers[cnt]->getMemberState() < kAppleRAIDMemberStateOpen)) continue;

	if ((rc2 = arMembers[cnt]->writeRAIDHeader()) != kIOReturnSuccess) {
	    IOLog("AppleRAIDSet::writeRAIDHeader() update failed on set \"%s\" (%s) member %s, rc = %x\n",
		  getSetNameString(), getUUIDString(), arMembers[cnt]->getUUIDString(), rc2);
	    rc = rc2;
	    // keep going ...
	}
    }
        
    if (!openedForWrite) {
	if (!openedForRead) {
	    IOLog1("AppleRAIDSet::writeRAIDHeader(%p): closing set.\n", this);
	    close(this, 0);
	} else {
	    IOLog1("AppleRAIDSet::writeRAIDHeader(%p): downgrading set to read only.\n", this);
	    if (!open(this, 0, kIOStorageAccessReader)) {	// downgrades should "always" work
		IOLog1("AppleRAIDSet::writeRAIDHeader(%p): downgrade back to RO failed.\n", this);
		changeSetState(kAppleRAIDSetStateFailed);
		return kIOReturnError;
	    }
	}
	changeSetState(formerSetState);
    }
    IOLog1("AppleRAIDSet::writeRAIDHeader(%p) exiting with 0x%x.\n", this, rc);

    return rc;
}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888


const OSString * AppleRAIDSet::getSetName(void)
{
    return OSDynamicCast(OSString, getProperty(kAppleRAIDSetNameKey));
}

const OSString * AppleRAIDSet::getUUID(void)
{
    return OSDynamicCast(OSString, getProperty(kAppleRAIDMemberUUIDKey));
}

const OSString * AppleRAIDSet::getSetUUID(void)
{
    return OSDynamicCast(OSString, getProperty(kAppleRAIDSetUUIDKey));
}

const OSString * AppleRAIDSet::getDiskName(void)
{
    return arMedia ? OSDynamicCast(OSString, arMedia->getProperty(kIOBSDNameKey)) : NULL;
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

IOStorage * AppleRAIDSet::getTarget(void) const
{
    return (IOStorage *)this;
}
 
bool AppleRAIDSet::isRAIDSet(void)
{
    return true;
}

bool AppleRAIDSet::isSetComplete(void)
{
    return arActiveCount == arMemberCount;
}

bool AppleRAIDSet::bumpOnError(void)
{
    return false;
}

UInt64 AppleRAIDSet::getSize() const
{
    return arSetMediaSize;
}

IOWorkLoop * AppleRAIDSet::getWorkLoop(void)
{
    // Create a WorkLoop if it has not already been done.
    if (arSetWorkLoop == 0) {
        arSetWorkLoop = IOWorkLoop::workLoop();
    }
    
    return arSetWorkLoop;
}

bool AppleRAIDSet::changeSetState(UInt32 newState)
{
    bool	swapState = false;
    char	*newStatus = "bogus";

#ifdef DEBUG
    const char	*oldStatus = "not set";
    OSString    *oldStatusString = OSDynamicCast(OSString, getProperty(kAppleRAIDStatusKey));
    if (oldStatusString) oldStatus = oldStatusString->getCStringNoCopy();
#endif

    // short cut
    if (arSetState == newState) return true;

    switch (newState) {

    case kAppleRAIDSetStateFailed:		// 0
	swapState = true;
	newStatus = kAppleRAIDStatusFailed;
	break;
	
    case kAppleRAIDSetStateTerminating:		// 1
	swapState = arSetState > kAppleRAIDSetStateFailed;
	newStatus = kAppleRAIDStatusOffline;
	break;
	
    case kAppleRAIDSetStateInitializing:	// 2
	swapState = arSetState > kAppleRAIDSetStateTerminating;
	newStatus = kAppleRAIDStatusOffline;
	break;

    case kAppleRAIDSetStateOnline:		// 3
	swapState = arSetState >= kAppleRAIDSetStateInitializing;
	newStatus = kAppleRAIDStatusOnline;
	break;

    case kAppleRAIDSetStateDegraded:		// 4
	swapState = arSetState >= kAppleRAIDSetStateInitializing;
	newStatus = kAppleRAIDStatusDegraded;
	break;

    default:
	IOLog("AppleRAIDSet::changeSetState() this \"%s\" (%s), bogus state %lu?\n",
	      getSetNameString(), getUUIDString(), newState);
    }

    if (swapState) {
	IOLog1("AppleRAIDSet::changeSetState(%p) from %lu (%s) to %lu (%s).\n",
	       this, arSetState, oldStatus, newState, newStatus);

	if (isRAIDMember()) {
	    if ((newState >= kAppleRAIDSetStateOnline) && (newState > arSetState)) {
		if (getMemberState() >= kAppleRAIDMemberStateClosing) {
		    changeMemberState(kAppleRAIDMemberStateOpen);
		} else {
		    changeMemberState(kAppleRAIDMemberStateClosed);
		}
	    }
	    if ((newState < kAppleRAIDSetStateOnline) && (newState < arSetState)) {
		changeMemberState(kAppleRAIDMemberStateClosing);
	    }
	}

	arSetState = newState;
	setProperty(kAppleRAIDStatusKey, newStatus);
	messageClients(kAppleRAIDMessageSetChanged);

    } else {
	IOLog1("AppleRAIDSet::changeSetState(%p) FAILED from %lu (%s) to %lu (%s).\n",
	       this, arSetState, oldStatus, newState, newStatus);
    }
    
    return swapState;
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

bool
AppleRAIDSet::addBootDeviceInfo(OSArray * bootArray)
{
    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
	if (arMembers[cnt] != NULL) {
	    if ((arMembers[cnt]->getMemberState() >= kAppleRAIDMemberStateClosed) &&
	        (arMembers[cnt]->getMemberState() != kAppleRAIDMemberStateRebuilding)) {

		if (!arMembers[cnt]->addBootDeviceInfo(bootArray)) {
		    // if any of the devices are not in the device tree
		    // just return an empty array
		    bootArray->flushCollection();
		    return false;
		}
	    }
	}
    }

    return true;
}


OSDictionary * AppleRAIDSet::getSetProperties(void)
{
    OSNumber * tmpNumber;

    OSDictionary * props = OSDictionary::withCapacity(16);
    if (!props) return NULL;

    props->setObject(kAppleRAIDSetNameKey, getSetName());
    props->setObject(kAppleRAIDSetUUIDKey, getUUID());
    props->setObject(kAppleRAIDLevelNameKey, getProperty(kAppleRAIDLevelNameKey));

    tmpNumber = OSNumber::withNumber(arHeaderVersion, 32);
    if (tmpNumber){
	props->setObject(kAppleRAIDHeaderVersionKey, tmpNumber);
	tmpNumber->release();
    }

    tmpNumber = OSNumber::withNumber(arSequenceNumber, 32);
    if (tmpNumber){
	props->setObject(kAppleRAIDSequenceNumberKey, tmpNumber);
	tmpNumber->release();
    }
    
    tmpNumber = OSNumber::withNumber(arSetBlockSize, 64);
    if (tmpNumber){
	props->setObject(kAppleRAIDChunkSizeKey, tmpNumber);
	tmpNumber->release();
    }

    tmpNumber = OSNumber::withNumber(arSetBlockCount, 64);
    if (tmpNumber){
	props->setObject(kAppleRAIDChunkCountKey, tmpNumber);
	tmpNumber->release();
    }

    props->setObject(kAppleRAIDSetAutoRebuildKey, getProperty(kAppleRAIDSetAutoRebuildKey));
    props->setObject(kAppleRAIDSetContentHintKey, getProperty(kAppleRAIDSetContentHintKey));
    props->setObject(kAppleRAIDSetTimeoutKey, getProperty(kAppleRAIDSetTimeoutKey));

    props->setObject(kAppleRAIDCanAddMembersKey, getProperty(kAppleRAIDCanAddMembersKey));
    props->setObject(kAppleRAIDCanAddSparesKey, getProperty(kAppleRAIDCanAddSparesKey));
    props->setObject(kAppleRAIDRemovalAllowedKey, getProperty(kAppleRAIDRemovalAllowedKey));
    props->setObject(kAppleRAIDSizesCanVaryKey, getProperty(kAppleRAIDSizesCanVaryKey));

    // not from header
    
    props->setObject(kAppleRAIDStatusKey, getProperty(kAppleRAIDStatusKey));
    props->setObject(kIOMaximumBlockCountReadKey, getProperty(kIOMaximumBlockCountReadKey));
    props->setObject(kIOMaximumSegmentCountReadKey, getProperty(kIOMaximumSegmentCountReadKey));
    props->setObject(kIOMaximumBlockCountWriteKey, getProperty(kIOMaximumBlockCountWriteKey));
    props->setObject(kIOMaximumSegmentCountWriteKey, getProperty(kIOMaximumSegmentCountWriteKey));

    props->setObject(kIOBSDNameKey, getDiskName());
    
    // set up the members array, only v2 headers contain a list of the members
    OSArray * members = OSDynamicCast(OSArray, getProperty(kAppleRAIDMembersKey));
    if (members) {

	props->setObject(kAppleRAIDMembersKey, members);

    } else {

	members = OSArray::withCapacity(arMemberCount);
	if (members) {
	    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
		if (arMembers[cnt] != 0) {
		    const OSString * uuid = arMembers[cnt]->getUUID();
		    if (uuid) members->setObject(uuid);
		} else {
		    OSString * uuid = OSString::withCString(kAppleRAIDMissingUUID);
		    if (uuid) {
			members->setObject(uuid);
			uuid->release();
		    }
		}
	    }
	    props->setObject(kAppleRAIDMembersKey, members);
	    members->release();
	}
    }

    // we don't worry about losing spares from this spare uuid list,
    // if they ever come online again they will be returned to the list 
    OSArray * spares = OSArray::withCapacity(arMemberCount);
    if (spares) {
	for (UInt32 cnt = 0; cnt < arSpareCount; cnt++) {

	    assert(arSpareMembers[cnt]);
	    if (!arSpareMembers[cnt]) continue;

	    const OSString * uuid = arSpareMembers[cnt]->getUUID();
	    assert(uuid);
	    if (!uuid) continue;

	    // skip spares that are in the member list
	    OSArray * members = (OSArray *)props->getObject(kAppleRAIDMembersKey);
	    UInt32 memberCount = members ? members->getCount() : 0;
	    bool foundIt = false;
	    for (UInt32 cnt2 = 0; cnt2 < memberCount; cnt2++) {
		foundIt = members->getObject(cnt2)->isEqualTo(uuid);
		if (foundIt) break;
	    }
	    if (foundIt) continue;

	    // finally, add it to the spare list
	    spares->setObject(uuid);
	}
	props->setObject(kAppleRAIDSparesKey, spares);
	setProperty(kAppleRAIDSparesKey, spares);   // lazy update
	spares->release();
    }

    return props;
}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

// from IOMedia.cpp:
// We are guaranteed that no other opens or closes will be processed until
// we make our decision, change our state, and return from this method.

// XXX In reality, IOMedia does not understand stacked raid sets and we do
// sometimes see two parallel opens on subordinate sets

bool AppleRAIDSet::handleOpen(IOService *  client,
			      IOOptionBits options,
			      void *       argument)
{
    IOStorageAccess access = (IOStorageAccess) argument;
    IOStorageAccess level  = kIOStorageAccessNone;

    IOLogOC("AppleRAIDSet::handleOpen(%p) called, client %p, access %lu, state %lu, client is a set = %s, raid member = %s.\n",
	    this, client, access, arSetState, OSDynamicCast(AppleRAIDSet, client) ? "y" : "n", isRAIDMember() ? "y" : "n");

    assert(client);

    // only allow "external" opens after we have published that we are online
    if (!OSDynamicCast(AppleRAIDSet, client) && arPublishedSetState < kAppleRAIDSetStateOnline) {
	IOLogOC("AppleRAIDSet::handleOpen(%p) open refused (set is not online (published)).\n", this);
	return false;
    }
    // only allow the set that we are a member of to open us if we are stacked.
    // however, until we open and read the header, "is member" will be false
    if (!OSDynamicCast(AppleRAIDSet, client) && isRAIDMember()) {
	IOLogOC("AppleRAIDSet::handleOpen(%p) open refused (set is stacked).\n", this);
	return false;
    }
    assert(arSetState >= kAppleRAIDSetStateOnline);

    switch (access)
    {
        case kIOStorageAccessReader:
        {
            if (arOpenReaders->containsObject(client))  // (access: no change)
                return true;
            else if (arOpenReaderWriter == client)      // (access: downgrade)
                level = kIOStorageAccessReader;
            else					// (access: new reader)
                level = arOpenReaderWriter ? kIOStorageAccessReaderWriter
                                           : kIOStorageAccessReader;
            break;
        }
        case kIOStorageAccessReaderWriter:
        {
            if (arOpenReaders->containsObject(client))  // (access: upgrade)
                level = kIOStorageAccessReaderWriter; 
            else if (arOpenReaderWriter == client)	// (access: no change)
                return true;
            else					// (access: new writer)
                level = kIOStorageAccessReaderWriter; 

	    if (arIsWritable == false)			// (is this member object writable?)
	    {
		IOLogOC("AppleRAIDSet::handleOpen(%p) client %p access %lu arIsWriteable == false\n", this, client, access);    
                return false;	// XXX the level was bumped above?  get newer code from IOMedia 
	    }

            if (arOpenReaderWriter)			// (does a reader-writer already exist?)
	    {
		IOLogOC("AppleRAIDSet::handleOpen(%p) client %p access %lu arOpenReaderWriter already set %p\n",
			this, client, access, arOpenReaderWriter);    
                return false;	// XXX the level was bumped above?  get newer code from IOMedia 
	    }

            break;
        }
        default:
        {
            assert(0);
            return false;
        }
    }

    //
    // If we are in the terminated state, we only accept downgrades.
    //

    if (isInactive() && arOpenReaderWriter != client) // (dead? not a downgrade?)
    {
	IOLogOC("AppleRAIDSet::handleOpen(%p) client %p access %lu isInactive && arOpenReadWriter !=client\n", this, client, access);    
        return false;
    }
    
    //
    // Determine whether the storage objects below us accept this open at this
    // multiplexed level of access -- new opens, upgrades, and downgrades (and
    // no changes in access) all enter through the same open api.
    //

    if (arOpenLevel != level)                        // (has open level changed?)
    {
	bool success = false;

	for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
	    if (arMembers[cnt] != 0) {

		IOLogOC("AppleRAIDSet::handleOpen(%p) opening %p member=%lu access=%lu level=%lu\n",
			this, arMembers[cnt], cnt, access, level);

		if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateRebuilding) continue;
		
		// XXX would it be faster if we did this in parallel?

		success = arMembers[cnt]->open(this, options, level);
		if (!success) {
		    IOLog("AppleRAIDSet::handleOpen(%p) client %p member %s failed to open for set \"%s\" (%s).\n",
			  this, client, arMembers[cnt]->getUUIDString(), getSetNameString(), getUUIDString());
		    IOLogOC("AppleRAIDSet::handleOpen() open failed on member %lu of %lu (active = %lu), state = %lu isOpen = %s",
			  cnt, arMemberCount, arActiveCount, arSetState, arMembers[cnt]->isOpen(NULL) ? "t" : "f");

		    // clean up any successfully opened members
		    for (UInt32 cnt2 = 0; cnt2 < cnt; cnt2++) {
			if (arMembers[cnt2] != 0) {
			    if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateRebuilding) continue;
			    arMembers[cnt2]->close(this, 0);
			}
		    }
		    
		    return false;
		}
	    }
	}
    }

    //
    // Process the open.
    //

    arOpenLevel = level;

    if (access == kIOStorageAccessReader)
    {
        arOpenReaders->setObject(client);

        if (arOpenReaderWriter == client)                // (for a downgrade)
        {
            arOpenReaderWriter = 0;
        }
    }
    else // (access == kIOStorageAccessReaderWriter)
    {
        arOpenReaderWriter = client;

        arOpenReaders->removeObject(client);             // (for an upgrade)
    }

    changeMemberState(kAppleRAIDMemberStateOpen);	// for stacked raid sets

    IOLogOC("AppleRAIDSet::handleOpen(%p) successful, client %p, access %lu, state %lu\n", this, client, access, arSetState);

    return true;
}

bool AppleRAIDSet::handleIsOpen(const IOService * client) const
{
    if (client == 0)  return (arOpenLevel != kIOStorageAccessNone);

    bool open = arOpenReaderWriter == client || arOpenReaders->containsObject(client);

    IOLogOC("AppleRAIDSet::handleIsOpen(%p) client %p is %s\n", this, client, open ? "true" : "false");

    return open;
}


void AppleRAIDSet::handleClose(IOService * client, IOOptionBits options)
{
    IOLogOC("AppleRAIDSet::handleClose(%p) called, client %p current state %lu\n", this, client, arSetState);

    //
    // A client is informing us that it is giving up access to our contents.
    //
    // This method will work even when the member is in the terminated state.
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we change our state and return from this method.
    //

    IOStorageAccess level = kIOStorageAccessNone;

    assert(client);

    //
    // Process the close.
    //

    if (arOpenReaderWriter == client)         // (is the client a reader-writer?)
    {
        arOpenReaderWriter = 0;
    }
    else if (arOpenReaders->containsObject(client))  // (is the client a reader?)
    {
        arOpenReaders->removeObject(client);
    }
    else                                      // (is the client is an imposter?)
    {
        assert(0);
        return;
    }

    //
    // Reevaluate the open we have on the level below us.  If no opens remain,
    // we close, or if no reader-writer remains, but readers do, we downgrade.
    //

    if      (arOpenReaderWriter)         level = kIOStorageAccessReaderWriter;
    else if (arOpenReaders->getCount())  level = kIOStorageAccessReader;
    else                                 level = kIOStorageAccessNone;

    if (level == kIOStorageAccessNone) {
	changeMemberState(kAppleRAIDMemberStateClosing); // for stacked raid sets
    }
    
    if (arOpenLevel != level)			// (has open level changed?)
    {
        assert(level != kIOStorageAccessReaderWriter);

	if (level == kIOStorageAccessNone)	// (is a close in order?)
	{
	    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
		if (arMembers[cnt] != 0) {
		    if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateRebuilding) continue;
		    arMembers[cnt]->close(this, options);
		}
	    }

	} else {				// (is a downgrade in order?)

	    bool success;
	    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
		if (arMembers[cnt] != 0) {
		    if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateRebuilding) continue;
		    success = arMembers[cnt]->open(this, 0, level);
		    assert(success);		// (should never fail, unless avoided deadlock)
		}
	    }
	}

	arOpenLevel = level;                    // (set new open level)
    }

    if (level == kIOStorageAccessNone) {
	changeMemberState(kAppleRAIDMemberStateClosed);		// for stacked raid sets
    }
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

void AppleRAIDSet::read(IOService *client, UInt64 byteStart,
			IOMemoryDescriptor *buffer, IOStorageCompletion completion)
{
    AppleRAIDStorageRequest * storageRequest;

    IOLogRW("AppleRAIDSet::read(%p, %llu, 0x%lx) this %p, state %lu\n", client, byteStart, buffer ? buffer->getLength() : 0, this, arSetState);

    arSetCommandGate->runAction(arAllocateRequestMethod, &storageRequest);
    
    if (storageRequest != 0) {
        buffer->retain();
        storageRequest->read(client, byteStart, buffer, completion);
    } else {
	IOLogRW("AppleRAIDSet::read(%p, 0x%llx) could not allocate a storage request\n", client, byteStart);
        IOStorage::complete(completion, kIOReturnNoMedia, 0);
    }
}

void AppleRAIDSet::write(IOService *client, UInt64 byteStart,
			 IOMemoryDescriptor *buffer, IOStorageCompletion completion)
{
    AppleRAIDStorageRequest * storageRequest;
    
    IOLogRW("AppleRAIDSet::write(%p, %llu, 0x%lx) this %p, state %lu\n", client, byteStart, buffer ? buffer->getLength() : 0, this, arSetState);

    arSetCommandGate->runAction(arAllocateRequestMethod, &storageRequest);
    
    if (storageRequest != 0) {
        buffer->retain();
        storageRequest->write(client, byteStart, buffer, completion);
    } else {
	IOLogRW("AppleRAIDSet::write(%p, 0x%llx) could not allocate a storage request\n", client, byteStart);
        IOStorage::complete(completion, kIOReturnNoMedia, 0);
    }
}

// the top set (master) needs to go through the workloop
// members or members are then already called in that workloop
// member sets can just call through, the syncing count is already bumped

IOReturn AppleRAIDSet::synchronizeCache(IOService *client)
{
    if (OSDynamicCast(AppleRAIDSet, client)) return synchronizeCacheGated(client);
    
    IOCommandGate::Action syncCacheMethod = OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::synchronizeCacheGated);
    return arSetCommandGate->runAction(syncCacheMethod, (void *)client);
}

IOReturn AppleRAIDSet::synchronizeCacheGated(IOService *client)
{
    AppleRAIDSet * masterSet = OSDynamicCast(AppleRAIDSet, client);
	
    if (masterSet == NULL) {
	while (arSetIsSyncingCount > 0) {
	    IOLog1("AppleRAIDSet::requestSynchronizeCache(%p) stalled count=%ld \n", client, arSetIsSyncingCount);
	    arSetCommandGate->commandSleep(&arSetIsSyncingCount, THREAD_UNINT);
	}
	arSetIsSyncingCount++;  // prevents multiple drops to zero
    }
    
    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {

	if (!arMembers[cnt] || (arMembers[cnt]->getMemberState() < kAppleRAIDMemberStateRebuilding)) continue;

	arMembers[cnt]->synchronizeCache(masterSet ? masterSet : this);
    }

    // wait for members to complete
    if (masterSet == NULL) {
	while (arSetIsSyncingCount > 1) {
	    arSetCommandGate->commandSleep(&arSetIsSyncingCount, THREAD_UNINT);
	}

	// we are done, wake up any other blocked requests
	arSetIsSyncingCount--;
	assert(arSetIsSyncingCount == 0);
	arSetCommandGate->commandWakeup(&arSetIsSyncingCount, /* oneThread */ false);
    }

    return 0;
}

void AppleRAIDSet::synchronizeStarted(void)
{
    arSetIsSyncingCount++;
}

void AppleRAIDSet::synchronizeCompleted(void)
{
    arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::synchronizeCompletedGated));
}

void AppleRAIDSet::synchronizeCompletedGated(void)
{
    arSetIsSyncingCount--;
    if (arSetIsSyncingCount <= 1) {
	assert(arSetIsSyncingCount == 1);
	arSetCommandGate->commandWakeup(&arSetIsSyncingCount, /* oneThread */ false);
    }
}

bool AppleRAIDSet::pauseSet(bool whenIdle)
{
    // this is running in the workloop
    if (whenIdle) {
	if (arStorageRequestsPending != 0) return false;
	if (arSetWasBlockedByPause) {
	    arSetWasBlockedByPause = false;
	    return false;
	}
    }

    // *** ALWAYS CALL SLEEPS IN THE SAME ORDER ***
	
    // only one pause at a time
    while (arSetIsPaused) {
	arSetWasBlockedByPause = true;
	arSetCommandGate->commandSleep(&arSetIsPaused, THREAD_UNINT);
    }

    arSetIsPaused++;

    // wait for any currently pending i/o to drain.
    while (arStorageRequestsPending != 0) {
        arSetCommandGate->commandSleep(&arStorageRequestPool, THREAD_UNINT);
    }

    return true;
}

void AppleRAIDSet::unpauseSet()
{
    // this is running in the workloop

    assert(arSetIsPaused);
    
    arSetIsPaused--;
    if (arSetIsPaused == 0) {
	arSetCommandGate->commandWakeup(&arSetIsPaused, /* oneThread */ false);
    }
}

IOReturn AppleRAIDSet::allocateRAIDRequest(AppleRAIDStorageRequest **storageRequest)
{
    while (1) {
	if ((arActiveCount == 0) || getSetState() <= kAppleRAIDSetStateTerminating) {
	    *storageRequest = 0;
	    return kIOReturnNoMedia;
	}
        
	// *** ALWAYS CALL SLEEPS IN THE SAME ORDER ***
	
        if (arSetIsPaused) {
	    arSetWasBlockedByPause = true;
            arSetCommandGate->commandSleep(&arSetIsPaused, THREAD_UNINT);
            continue;
        }
        
        *storageRequest = (AppleRAIDStorageRequest *)arStorageRequestPool->getCommand(false);
        if (*storageRequest == 0) {
            arSetCommandGate->commandSleep(&arStorageRequestPool, THREAD_UNINT);
            continue;
        }
        
        break;
    }
    
    arStorageRequestsPending++;
    
    return kIOReturnSuccess;
}

void AppleRAIDSet::returnRAIDRequest(AppleRAIDStorageRequest *storageRequest)
{
    arStorageRequestsPending--;
    arStorageRequestPool->returnCommand(storageRequest);
    arSetCommandGate->commandWakeup(&arStorageRequestPool, /* oneThread */ false);
}

void AppleRAIDSet::completeRAIDRequest(AppleRAIDStorageRequest *storageRequest)
{
    UInt32		cnt;
    UInt64              byteCount;
    IOReturn            status;
    bool		isWrite;
    IOStorageCompletion storageCompletion;

    // this is running in the workloop, via a AppleRAIDEvent
    
    isWrite = (storageRequest->srMemoryDescriptorDirection == kIODirectionOut);
    byteCount = 0;
    status = kIOReturnSuccess;

    // Collect the status and byte count for each member.
    for (cnt = 0; cnt < arMemberCount; cnt++) {

	// Ignore missing members.
	if (arMembers[cnt] == 0) continue;

	// Ignore offline members
	if (arMembers[cnt]->getMemberState() != kAppleRAIDMemberStateOpen) {
	    IOLogRW("AppleRAIDSet::completeRAIDRequest - [%lu] tbc 0x%llx, sbc 0x%llx bc 0x%llx, member %p, member state %lu\n",
		    cnt, storageRequest->srByteCount, storageRequest->srMemberByteCounts[cnt],
		    byteCount, arMembers[cnt], arMembers[cnt]->getMemberState());

	    if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateClosing) {
		status = kIOReturnOffline;
	    }

	    continue;
	}
        
        // Return any status errors.
        if (storageRequest->srMemberStatus[cnt] != kIOReturnSuccess) {
            status = storageRequest->srMemberStatus[cnt];
            byteCount = 0;
	    IOLog("AppleRAID::completeRAIDRequest - error 0x%x detected for set \"%s\" (%s), member %s, set byte offset = %llu.\n",
		  status, getSetNameString(), getUUIDString(), arMembers[cnt]->getUUIDString(), storageRequest->srByteStart);

	    // mark this member to be removed
	    arMembers[cnt]->changeMemberState(kAppleRAIDMemberStateClosing);
	    continue;
	}

	byteCount += storageRequest->srMemberByteCounts[cnt];

	IOLogRW("AppleRAIDSet::completeRAIDRequest - [%lu] tbc 0x%llx, sbc 0x%llx bc 0x%llx, member %p\n",
		cnt, storageRequest->srByteCount, storageRequest->srMemberByteCounts[cnt],
		byteCount, arMembers[cnt]);
    }
    
    // Return an underrun error if the byte count is not complete.
    // This can happen if one or more members reported a smaller byte count.
    if ((status == kIOReturnSuccess) && (byteCount != storageRequest->srByteCount)) {
	IOLog("AppleRAID::completeRAIDRequest - underrun detected, expected = 0x%llx, actual = 0x%llx, set = \"%s\" (%s)\n",
	      storageRequest->srByteCount, byteCount, getSetNameString(), getUUIDString());
        status = kIOReturnUnderrun;
        byteCount = 0;
    }
    
    // bad status is also returned here
	
    storageRequest->srMemoryDescriptor->release();
    storageCompletion = storageRequest->srCompletion;
        
    returnRAIDRequest(storageRequest);
        
    // Call the clients completion routine.
    IOStorage::complete(storageCompletion, status, byteCount);

    // remove any failing members from the set
    if (status != kIOReturnSuccess) recoverStart();
}

void AppleRAIDSet::recoverStart()
{
    IOLog1("AppleRAID::recoverStart entered\n");

    arSetIsPaused++;
    retain();  // the set also holds a controller ref

    bool bumped = thread_call_enter(arRecoveryThreadCall);

    if (bumped) {
	arSetIsPaused--;
	release();
    }
}

void AppleRAIDSet::recoverWait()
{
    // this is on a separate thread
    // running on the workloop

    assert(arSetIsPaused);

    IOLog1("AppleRAID::recover %lu requests are pending.\n", arStorageRequestsPending);
    while (arStorageRequestsPending != 0) {
        arSetCommandGate->commandSleep(&arStorageRequestPool, THREAD_UNINT);
    }
}

bool AppleRAIDSet::recover()
{
    // this is on a separate thread
    // the set is paused.

    // still here?
    if (arController->findSet(getUUID()) != this) return false;

    // wait for outstanding i/o
    arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::recoverWait));

    IOLog1("AppleRAID::recover wait for requests complete.\n");

    // at this point, the set should be paused and not allowing any new i/o
    // and there should be no active i/o outstanding other than the failed i/o

    // thread_call_enter() allows multiple threads to run at once
    // the first one that gets out of sleep will then do most of the work
    IOSleep(100);

    // remove any bad members from the set and reconfigure memory descriptors

    gAppleRAIDGlobals.lock();

    assert(arSetIsPaused);

    UInt32 oldSpareCount = arSpareCount;
    
    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
	
	if (arMembers[cnt] == 0) continue;

	if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateClosing) {

	    IOLog("AppleRAID::recover() member %s from set \"%s\" (%s) has been marked offline.\n",
		  arMembers[cnt]->getUUIDString(), getSetNameString(), getUUIDString());
	    
	    // manually move bad member to the spare list
	    // leaving it attached and then close them last

	    AppleRAIDMember * brokenMember = arMembers[cnt];

	    arMembers[cnt] = 0;
	    arActiveCount--;

	    if (arSpareCount < arMemberCount) {
		arSpareMembers[arSpareCount] = brokenMember;
		arSpareCount++;
	    }

	    brokenMember->changeMemberState(kAppleRAIDMemberStateBroken);
	}
    }

    if (oldSpareCount < arSpareCount) {

	// reconfigure the set with the remaining active members
	arController->restartSet(this, bumpOnError());

	// close the new spares
	while (oldSpareCount < arSpareCount) {
	    arSpareMembers[oldSpareCount++]->close(this, 0);
	}
    }
    
    bool stillAlive = arActiveCount > 0;
    
    gAppleRAIDGlobals.unlock();

    arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::unpauseSet));

    release();  // from recoverStart

    IOLog1("AppleRAID::recover finished\n");
    return stillAlive;
}
