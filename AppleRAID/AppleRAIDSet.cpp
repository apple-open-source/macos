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
#include <IOKit/IOPolledInterface.h>
#include "sys/param.h"  // MAXBSIZE
    
#define super AppleRAIDMember
OSDefineMetaClassAndAbstractStructors(AppleRAIDSet, AppleRAIDMember);

void AppleRAIDSet::free(void)
{
    if (arOpenReaders)		arOpenReaders->release();

    if (arMembers)		IODelete(arMembers, AppleRAIDMember *, arLastAllocCount);
    if (arSpareMembers)		arSpareMembers->release();

//    UInt32 count = 0;    // XXXXXXXXXXXXXXX  LVM
    if (arStorageRequestPool) {
        while (1) {
	    AppleRAIDStorageRequest * storageRequest;
	    storageRequest = (AppleRAIDStorageRequest *)arStorageRequestPool->getCommand(false);
            if (storageRequest == 0) break;
//	    count++;  // XXXXXXXXXXXXXXX
            storageRequest->release();
        }
        arStorageRequestPool->release();
	arStorageRequestPool = 0;
    }
//    IOLog1("ARSFree: freed %lu SR's, pending %lu  XXXXXXXXXXXXXXX\n", count, arStorageRequestsPending);
    
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
    // this will disable the writing of hibernation data on RAID volumes
    setProperty(kIOPolledInterfaceSupportKey, kOSBooleanFalse);
    
    arMemberCount	= 0;
    arLastAllocCount	= 0;
    arSequenceNumber	= 0;
    arMedia		= NULL;
    arOpenLevel		= kIOStorageAccessNone;
    arOpenReaders	= OSSet::withCapacity(10);
    arOpenReaderWriters	= OSSet::withCapacity(10);

    arSetCompleteTimeout = kARSetCompleteTimeoutNone;
    arSetBlockCount	= 0;
    arSetMediaSize	= 0;
    arMaxReadRequestFactor = 0;
    arPrimaryMetaDataUsed = 0;
    arPrimaryMetaDataMax = 0;

    arMembers		= 0;
    arSpareMembers	= OSSet::withCapacity(10);

    arSetIsSyncingCount	= 0;

    if (!arSpareMembers || !arOpenReaders) return false;

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

    number = OSDynamicCast(OSNumber, header->getObject(kAppleRAIDPrimaryMetaDataUsedKey));
    if (number) {
	arPrimaryMetaDataUsed = number->unsigned64BitValue();
    }

    // don't care if these fail
    setProperty(kAppleRAIDSetContentHintKey, header->getObject(kAppleRAIDSetContentHintKey));
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
    IOLog1("AppleRAIDSet::addSpare(%p) entered, spare count was %u.\n", member, (uint32_t)getSpareCount());

    assert(gAppleRAIDGlobals.islocked());

    if (!this->attach(member)) {
	IOLog1("AppleRAIDSet::addSpare(%p) this->attach(%p) failed\n", this, member);
	member->changeMemberState(kAppleRAIDMemberStateBroken);
	return false;
    }

    arSpareMembers->setObject(member);

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
	IOLog("AppleRAIDSet::addMember() too many members, active = %u, count = %u, member = %s\n",
	      (uint32_t)arActiveCount, (uint32_t)arMemberCount, member->getUUIDString());
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
	IOLog("AppleRAIDSet::addMember() detected expired sequenceNumber (%u) for member %s\n",
	      (uint32_t)memberSequenceNumber, member->getUUIDString());
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

	// calculate the max size for the primary data, the primary data is always the
	// same size but may be at different offsets depending on the type of raid set
	assert(arPrimaryMetaDataMax ? (arPrimaryMetaDataMax == member->getPrimaryMaxSize()) : 1);
	arPrimaryMetaDataMax = member->getPrimaryMaxSize();
    }
    
    // Make sure this is the only member in this slot.
    if (arMembers[memberIndex] != 0) {
	IOLog("AppleRAIDSet::addMember() detected the same member index (%u) twice?\n", (uint32_t)memberIndex);
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
    
    arSpareMembers->removeObject(member);

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
    if (arOpenReaderWriters->getCount() || arOpenReaders->getCount()) {
	IOStorageAccess level = arOpenReaderWriters->getCount() ? kIOStorageAccessReaderWriter : kIOStorageAccessReader;
	IOLog1("AppleRAIDSet::upgradeMember(%p) opening for read%s.\n", this, arOpenReaderWriters->getCount() ? "/write" : " only");
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

    IOLog1("AppleRAIDSet::resizeSet(%p) entered. alloc = %d old = %d new = %d\n",
	   this, (int)arLastAllocCount, (int)arMemberCount, (int)newMemberCount);
    
    UInt32 oldMemberCount = arMemberCount;

    // if downsizing, just hold on to the extra space
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
    }
    
    arMembers = IONew(AppleRAIDMember *, newMemberCount);
    if (!arMembers) return false;
            
    // Clear the new arrays.
    bzero(arMembers, sizeof(AppleRAIDMember *) * newMemberCount);

    // copy the old into the new, if needed
    if (arLastAllocCount) {
	bcopy(oldMembers, arMembers, sizeof(AppleRAIDMember *) * oldMemberCount);
	IODelete(oldMembers, AppleRAIDMember *, arLastAllocCount);
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

    if (arActiveCount == 0 && getSpareCount() == 0) {
	IOLog1("AppleRAIDSet::nextSetState: %p is empty, setting state to terminating.\n", this);
	return kAppleRAIDSetStateTerminating;
    } 

    if (getSetState() != kAppleRAIDSetStateInitializing) {
	IOLog1("AppleRAIDSet::nextSetState: set \"%s\" failed to come online.\n", getSetNameString());
    }

    return kAppleRAIDSetStateInitializing;
}


UInt64 AppleRAIDSet::getSmallestMaxByteCount(void)
{
    UInt64 minimum = MAXBSIZE;  // currently 1MB
    UInt64 newMinimum;

    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
	AppleRAIDMember * target = arMembers[cnt];
	if (target) {

	    newMinimum = 0;

	    OSNumber * number = OSDynamicCast(OSNumber, target->getProperty(kIOMaximumByteCountReadKey, gIOServicePlane));
            if (number) {
		newMinimum = number->unsigned64BitValue();
		if (newMinimum) minimum = min(minimum, newMinimum);
	    }

	    if (!newMinimum) {
		OSNumber * number = OSDynamicCast(OSNumber, target->getProperty(kIOMaximumBlockCountReadKey, gIOServicePlane));
		if (number) {
		    newMinimum = number->unsigned64BitValue() * 512;
		    if (newMinimum) minimum = min(minimum, newMinimum);
		}
	    }
	}
    }

    return minimum;
}

void AppleRAIDSet::setSmallest64BitMemberPropertyFor(const char * key, UInt32 multiplier)
{
    UInt64 minimum = UINT64_MAX;

    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
	AppleRAIDMember * target = arMembers[cnt];
	if (target) {
	    OSNumber * number = OSDynamicCast(OSNumber, target->getProperty(key, gIOServicePlane));
            if (number) {
		UInt64 newMinimum = number->unsigned64BitValue();
		if (newMinimum) minimum = min(minimum, newMinimum);
	    }
	}
    }

    if (minimum < UINT64_MAX) {
	setProperty(key, minimum * multiplier, 64);
    } else {
	removeProperty(key);
    }
}


void AppleRAIDSet::setLargest64BitMemberPropertyFor(const char * key, UInt32 multiplier)
{
    UInt64 maximum = 0;

    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
	AppleRAIDMember * target = arMembers[cnt];
	if (target) {
	    OSNumber * number = OSDynamicCast(OSNumber, target->getProperty(key, gIOServicePlane));
            if (number) {
		UInt64 newMaximum = number->unsigned64BitValue();
		if (newMaximum) maximum = max(maximum, newMaximum);
	    }
	}
    }

    if (maximum > 0) {
	setProperty(key, maximum * multiplier, 64);
    } else {
	removeProperty(key);
    }
}


bool AppleRAIDSet::startSet(void)
{
    IOLog1("AppleRAIDSet::startSet %p called with %u of %u members (%u spares).\n",
	   this, (uint32_t)arActiveCount, (uint32_t)arMemberCount, (uint32_t)getSpareCount());

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

	    AppleRAIDStorageRequest * storageRequest;
	    if (OSDynamicCast(AppleLVMGroup, this)) {
		storageRequest = AppleLVMStorageRequest::withAppleRAIDSet(this);
	    } else {
		storageRequest = AppleRAIDStorageRequest::withAppleRAIDSet(this);
	    }
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

    setSmallest64BitMemberPropertyFor(kIOMaximumBlockCountReadKey, 1);
    setSmallest64BitMemberPropertyFor(kIOMaximumBlockCountWriteKey, 1);
    setSmallest64BitMemberPropertyFor(kIOMaximumByteCountReadKey, 1);
    setSmallest64BitMemberPropertyFor(kIOMaximumByteCountWriteKey, 1);

    setSmallest64BitMemberPropertyFor(kIOMaximumSegmentCountReadKey, 1);
    setSmallest64BitMemberPropertyFor(kIOMaximumSegmentCountWriteKey, 1);
    setSmallest64BitMemberPropertyFor(kIOMaximumSegmentByteCountReadKey, 1);		// don't scale this
    setSmallest64BitMemberPropertyFor(kIOMaximumSegmentByteCountWriteKey, 1);		// don't scale this

    setLargest64BitMemberPropertyFor(kIOMinimumSegmentAlignmentByteCountKey, 1);	// don't scale this
    setSmallest64BitMemberPropertyFor(kIOMaximumSegmentAddressableBitCountKey, 1);	// don't scale this
    
    IOLog1("AppleRAIDSet::startSet %p was successful.\n", this);
    return true;
}


bool AppleRAIDSet::publishSet(void)
{
    IOLog1("AppleRAIDSet::publishSet called %p\n", this);

    // are we (still) connected to the io registry?
    if (arActiveCount == 0 && getSpareCount() == 0) {
	IOLog1("AppleRAIDSet::publishSet: the set %p is empty, aborting.\n", this);
	return false;
    }

    if (getSetState() < kAppleRAIDSetStateOnline || isRAIDMember()) {
	IOLog1("AppleRAIDSet::publishSet: skipping offline or stacked raid set.\n");
	unpublishSet();
	return true;
    }

    // logical volume groups do not export a media object
    const char * contentHint = 0;
    OSString * theHint = OSDynamicCast(OSString, getProperty(kAppleRAIDSetContentHintKey));
    if (theHint) {
	if (theHint->isEqualTo(kAppleRAIDNoMediaExport)) {
	    IOLog1("AppleRAIDSet::publishSet: shortcircuiting publish for no media set.\n");
	    return true;
	}
	contentHint = theHint->getCStringNoCopy();
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
	    snprintf(location, sizeof(location), "%d", 0);
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
	    }  else {
                arMedia->messageClients(kIOMessageServicePropertyChange);
	    }
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
    IOLog1("AppleRAIDSet::destroySet(%p) entered.\n", this);

    assert(gAppleRAIDGlobals.islocked());

    if (isRAIDMember()) {
	IOLog("AppleRAIDSet::destroySet() failed, an attempt was made to destroy subordinate set\n");
	return false;
    }

    // zero headers on members
    for (UInt32 i = 0; i < arMemberCount; i++) {
	if (arMembers[i]) {
	    (void)arMembers[i]->zeroRAIDHeader();
	    if (arMembers[i]->getMemberState() == kAppleRAIDMemberStateRebuilding) {
		arMembers[i]->changeMemberState(kAppleRAIDMemberStateSpare, true);
		while (arMembers[i]->getMemberState() == kAppleRAIDMemberStateSpare) {
		    IOSleep(50);
		}
		// drag member back to spare list, keeping it attached, ** active count doesn't change **
		AppleRAIDMember * member = arMembers[i];
		arMembers[i] = 0;
		arSpareMembers->setObject(member);
	    }
	}
    }

    // zero headers on spares
    OSCollectionIterator * iter = OSCollectionIterator::withCollection(arSpareMembers);
    if (!iter) return false;
    while (AppleRAIDMember * spare = (AppleRAIDMember *)iter->getNextObject()) {
	(void)spare->zeroRAIDHeader();
    }
    iter->release();
    
    // this keeps us from bumping sequence numbers on the way down
    changeSetState(kAppleRAIDSetStateTerminating);

    // remove the members from the set
    for (UInt32 i = 0; i < arMemberCount; i++) {
	if (arMembers[i]) {
	    if (arMembers[i]->isRAIDSet()) {
		arController->oldMember(arMembers[i]);
	    } else {
		arMembers[i]->stop(NULL);
	    }
	}
    }

    // remove the spares from the set
    // make a copy since this changes the spare list
    OSSet * copy = OSSet::withSet(arSpareMembers, arSpareMembers->getCount());
    if (!copy) return false;
    while (AppleRAIDMember * spare = (AppleRAIDMember *)copy->getAnyObject()) {
	copy->removeObject(spare);
	if (spare->isRAIDSet()) {
	    arController->oldMember(spare);
	} else {
	    spare->stop(NULL);
	}
    }
    copy->release();

    IOLog1("AppleRAIDSet::destroySet(%p) was successful.\n", this);

    return true;
}


bool AppleRAIDSet::reconfigureSet(OSDictionary * updateInfo)
{
    bool updateHeader = false;
    UInt32 newMemberCount = arMemberCount;
    
    IOLog1("AppleRAIDSet::reconfigureSet(%p) entered.\n", this);

    OSString * deleted = OSString::withCString(kAppleRAIDDeletedUUID);
    if (!deleted) return false;

    OSArray * oldMemberList = OSDynamicCast(OSArray, getProperty(kAppleRAIDMembersKey));
    OSArray * newMemberList = OSDynamicCast(OSArray, updateInfo->getObject(kAppleRAIDMembersKey));

    if (oldMemberList && newMemberList) {

	IOLog1("AppleRAIDSet::reconfigureSet(%p) updating member list.\n", this);

	assert(arMemberCount == oldMemberList->getCount());
	
	// look for kAppleRAIDDeletedUUID
	for (UInt32 i = 0; i < newMemberCount; i++) {

	    OSString * uuid = OSDynamicCast(OSString, newMemberList->getObject(i));

	    if (uuid && (uuid->isEqualTo(deleted))) {

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
			// drag member back to spare list, keeping it attached, ** active count doesn't change **
			AppleRAIDMember * member = arMembers[i];
			arMembers[i] = 0;
			arSpareMembers->setObject(member);
			if (member->isRAIDSet()) {
			    arController->oldMember(member);
			} else {
			    member->stop(NULL);
			}
		    } else {
			if (arMembers[i]->isRAIDSet()) {
			    arController->oldMember(arMembers[i]);
			} else {
			    arMembers[i]->stop(NULL);
			}
		    }
		} else {
		    // if the member is broken it might be in the spare list
		    OSString * olduuid = OSDynamicCast(OSString, oldMemberList->getObject(i));
		    OSCollectionIterator * iter = OSCollectionIterator::withCollection(arSpareMembers);
		    if (!iter) return false;

		    while (AppleRAIDMember * spare = (AppleRAIDMember *)iter->getNextObject()) {
			if (spare->getUUID()->isEqualTo(olduuid)) {
			    spare->zeroRAIDHeader();
			    if (spare->isRAIDSet()) {
				arController->oldMember(spare);
			    } else {
				spare->stop(NULL);
			    }
			    break;
			}
		    }
		    iter->release();
		}

		// slide everything in one to fill the deleted spot
		newMemberCount--;
		newMemberList->removeObject(i);
		for (UInt32 j = i; j < newMemberCount; j++) {
		    arMembers[j] = arMembers[j + 1];
		    if (arMembers[j]) arMembers[j]->setMemberIndex(j);
		}
                                             
		break;	// XXX this can only delete one member, the interface allows for more
	    }
	}

	// this catches new member adds, resizeSet() fixes arMemberCount below
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
	    OSCollectionIterator * iter = OSCollectionIterator::withCollection(arSpareMembers);
	    if (!iter) return false;

	    while (AppleRAIDMember * spare = (AppleRAIDMember *)iter->getNextObject()) {
		if (spare->getUUID()->isEqualTo(olduuid)) {
		    spare->zeroRAIDHeader();
		    if (spare->isRAIDSet()) {
			arController->oldMember(spare);
		    } else {
			spare->stop(NULL);
		    }
		    break;
		}
	    }
	    iter->release();
	    
	    break;	// XXX this can only delete one spare, the interface allows for more
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

    // newMemberCount will be zero when deleting last member
    if (newMemberCount != arMemberCount) {

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

    IOLog1("AppleRAIDSet::bumpSequenceNumber(%p) bumping to %u\n", this, (uint32_t)arSequenceNumber);

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
    bool openedForWrite = arOpenReaderWriters->getCount() != 0;
    bool openedForRead  = arOpenReaders->getCount() != 0;
    if (!openedForWrite) {
	IOLog1("AppleRAIDSet::writeRAIDHeader(%p): opening set for writing.\n", this);
	if (!open(this, 0, kIOStorageAccessReaderWriter)) return kIOReturnIOError;
    }

    for (cnt = 0; cnt < arMemberCount; cnt++) {

	if (!arMembers[cnt] || (arMembers[cnt]->getMemberState() < kAppleRAIDMemberStateOpen)) continue;

	if (arMembers[cnt]->isRAIDSet() && (rc2 = arMembers[cnt]->AppleRAIDMember::writeRAIDHeader()) != kIOReturnSuccess) {
	    IOLog("AppleRAIDSet::writeRAIDHeader() update failed on a set level on set \"%s\" (%s) member %s, rc = %x\n",
		  getSetNameString(), getUUIDString(), arMembers[cnt]->getUUIDString(), rc2);
	    rc = rc2;
	    // keep going ...
	}

	if ((rc2 = arMembers[cnt]->writeRAIDHeader()) != kIOReturnSuccess) {
	    IOLog("AppleRAIDSet::writeRAIDHeader() update failed at member level on set \"%s\" (%s) member %s, rc = %x\n",
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



IOBufferMemoryDescriptor * AppleRAIDSet::readPrimaryMetaData(AppleRAIDMember * member)
{
    if (!member) return NULL;
    return member->readPrimaryMetaData();
}

IOReturn AppleRAIDSet::writePrimaryMetaData(IOBufferMemoryDescriptor * primaryBuffer)
{
    UInt32 cnt;
    IOReturn rc = kIOReturnSuccess, rc2;

    IOLog1("AppleRAIDSet::writePrimaryMetaData(%p) entered.\n", this);

    // XXX assert(gAppleRAIDGlobals.islocked());

    if ((arActiveCount == 0) || getSetState() <= kAppleRAIDSetStateTerminating) {
	IOLog1("AppleRAIDSet::writePrimaryMetaData(%p) ignoring request, the set is empty or broken/terminating.\n", this);
	return rc;
    }

    // opening the set changes it's state
    UInt32 formerSetState = getSetState();

    // we need to be opened for write
    bool openedForWrite = arOpenReaderWriters->getCount() != 0;
    bool openedForRead  = arOpenReaders->getCount() != 0;
    if (!openedForWrite) {
	IOLog1("AppleRAIDSet::writePrimaryMetaData(%p): opening set for writing.\n", this);
	if (!open(this, 0, kIOStorageAccessReaderWriter)) return kIOReturnIOError;
    }

    for (cnt = 0; cnt < arMemberCount; cnt++) {

	if (!arMembers[cnt] || (arMembers[cnt]->getMemberState() < kAppleRAIDMemberStateOpen)) continue;

	if ((rc2 = arMembers[cnt]->writePrimaryMetaData(primaryBuffer)) != kIOReturnSuccess) {
	    IOLog("AppleRAIDSet::writePrimaryMetaData() update failed on set \"%s\" (%s) member %s, rc = %x\n",
		  getSetNameString(), getUUIDString(), arMembers[cnt]->getUUIDString(), rc2);
	    rc = rc2;
	    // keep going ...
	}
    }
        
    if (!openedForWrite) {
	if (!openedForRead) {
	    IOLog1("AppleRAIDSet::writePrimaryMetaData(%p): closing set.\n", this);
	    close(this, 0);
	} else {
	    IOLog1("AppleRAIDSet::writePrimaryMetaData(%p): downgrading set to read only.\n", this);
	    if (!open(this, 0, kIOStorageAccessReader)) {	// downgrades should "always" work
		IOLog1("AppleRAIDSet::writePrimaryMetaData(%p): downgrade back to RO failed.\n", this);
		changeSetState(kAppleRAIDSetStateFailed);
		return kIOReturnError;
	    }
	}
	changeSetState(formerSetState);
    }
    IOLog1("AppleRAIDSet::writePrimaryMetaData(%p) exiting with 0x%x.\n", this, rc);

    return rc;
}


// read into a buffer using member offsets

bool AppleRAIDSet::readIntoBuffer(AppleRAIDMember * member, IOBufferMemoryDescriptor * buffer, UInt64 offset)
{
    assert(buffer);
    assert(member);
    
    // Open the whole set
    bool openedForRead = isOpen();
    if (!openedForRead) {
	if (!getTarget()->open(this, 0, kIOStorageAccessReader)) return false;
    }

    // Read into the buffer
    buffer->setDirection(kIODirectionIn);
    IOReturn rc = member->getTarget()->read(this, offset, buffer);
        
    // Close the set
    if (!openedForRead) {
	getTarget()->close(this, 0);
    }

    return rc == kIOReturnSuccess;
}

// write from a buffer using member offsets

IOReturn AppleRAIDSet::writeFromBuffer(AppleRAIDMember * member, IOBufferMemoryDescriptor * buffer, UInt64 offset)
{
    IOReturn rc = kIOReturnSuccess;

    IOLog1("AppleRAIDSet::writeFromBuffer(%p) entered.\n", this);

//    assert(gAppleRAIDGlobals.islocked());

    if ((arActiveCount == 0) || getSetState() <= kAppleRAIDSetStateTerminating) {
	IOLog1("AppleRAIDSet::writeFromBuffer(%p) ignoring request, the set is empty or broken/terminating.\n", this);
	return rc;
    }

    // opening the set changes it's state
    UInt32 formerSetState = getSetState();

    // we need to be opened for write
    bool openedForWrite = arOpenReaderWriters->getCount() != 0;
    bool openedForRead  = arOpenReaders->getCount() != 0;
    if (!openedForWrite) {
	IOLog1("AppleRAIDSet::writeFromBuffer(%p): opening set for writing.\n", this);
	if (!open(this, 0, kIOStorageAccessReaderWriter)) return kIOReturnIOError;
    }

    buffer->setDirection(kIODirectionOut);
    rc = member->getTarget()->write(this, offset, buffer);
        
    if (!openedForWrite) {
	if (!openedForRead) {
	    IOLog1("AppleRAIDSet::writeFromBuffer(%p): closing set.\n", this);
	    close(this, 0);
	} else {
	    IOLog1("AppleRAIDSet::writeFromBuffer(%p): downgrading set to read only.\n", this);
	    if (!open(this, 0, kIOStorageAccessReader)) {	// downgrades should "always" work
		IOLog1("AppleRAIDSet::writeFromBuffer(%p): downgrade back to RO failed.\n", this);
		changeSetState(kAppleRAIDSetStateFailed);
		return kIOReturnError;
	    }
	}
	changeSetState(formerSetState);
    }
    IOLog1("AppleRAIDSet::writeFromBuffer(%p) exiting with 0x%x.\n", this, rc);

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
    const char	*newStatus = "bogus";

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
	IOLog("AppleRAIDSet::changeSetState() this \"%s\" (%s), bogus state %u?\n",
	      getSetNameString(), getUUIDString(), (uint32_t)newState);
    }

    if (swapState) {
	IOLog1("AppleRAIDSet::changeSetState(%p) from %u (%s) to %u (%s).\n",
	       this, (uint32_t)arSetState, oldStatus, (uint32_t)newState, newStatus);

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
	IOLog1("AppleRAIDSet::changeSetState(%p) FAILED from %u (%s) to %u (%s).\n",
	       this, (uint32_t)arSetState, oldStatus, (uint32_t)newState, newStatus);
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

    OSDictionary * props = OSDictionary::withCapacity(32);
    if (!props) return NULL;

    props->setObject(kAppleRAIDSetNameKey, getSetName());
    props->setObject(kAppleRAIDSetUUIDKey, getUUID());
    props->setObject(kAppleRAIDLevelNameKey, getProperty(kAppleRAIDLevelNameKey));

    tmpNumber = OSNumber::withNumber(arHeaderVersion, 32);
    if (tmpNumber) {
	props->setObject(kAppleRAIDHeaderVersionKey, tmpNumber);
	tmpNumber->release();
    }

    tmpNumber = OSNumber::withNumber(arSequenceNumber, 32);
    if (tmpNumber) {
	props->setObject(kAppleRAIDSequenceNumberKey, tmpNumber);
	tmpNumber->release();
    }
    
    tmpNumber = OSNumber::withNumber(arSetBlockSize, 64);
    if (tmpNumber) {
	props->setObject(kAppleRAIDChunkSizeKey, tmpNumber);
	tmpNumber->release();
    }

    tmpNumber = OSNumber::withNumber(arSetBlockCount, 64);
    if (tmpNumber) {
	props->setObject(kAppleRAIDChunkCountKey, tmpNumber);
	tmpNumber->release();
    }

    if (arPrimaryMetaDataUsed) {
	tmpNumber = OSNumber::withNumber(arPrimaryMetaDataUsed, 64);
	if (tmpNumber){
	    props->setObject(kAppleRAIDPrimaryMetaDataUsedKey, tmpNumber);
	    tmpNumber->release();
	}
    }
    props->setObject(kAppleRAIDSetContentHintKey, getProperty(kAppleRAIDSetContentHintKey));

    props->setObject(kAppleRAIDCanAddMembersKey, getProperty(kAppleRAIDCanAddMembersKey));
    props->setObject(kAppleRAIDCanAddSparesKey, getProperty(kAppleRAIDCanAddSparesKey));
    props->setObject(kAppleRAIDRemovalAllowedKey, getProperty(kAppleRAIDRemovalAllowedKey));
    props->setObject(kAppleRAIDSizesCanVaryKey, getProperty(kAppleRAIDSizesCanVaryKey));

    // not from header
    
    props->setObject(kAppleRAIDStatusKey, getProperty(kAppleRAIDStatusKey));
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
    OSCollectionIterator * iter = OSCollectionIterator::withCollection(arSpareMembers);
    if (spares && iter) {
	while (AppleRAIDMember * spare = (AppleRAIDMember *)iter->getNextObject()) {

	    const OSString * uuid = spare->getUUID();
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
	iter->release();
    }

    return props;
}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

// This code was lifted from IOPartitionScheme.cpp
//
// It has few major additions:
// 1) Instead of opening or closing one device it handles all the members in a set
// 2) Only logical volume groups allow more that one open for writing.
// 3) Rebuilding members are left writable even if the set is only open for read.
// 4) Opens are only allowed for sets that are online or degraded and at the top level.

bool AppleRAIDSet::handleOpen(IOService *  client,
			      IOOptionBits options,
			      void *       argument)
{
    //
    // The handleOpen method grants or denies permission to access this object
    // to an interested client.  The argument is an IOStorageAccess value that
    // specifies the level of access desired -- reader or reader-writer.
    //
    // This method can be invoked to upgrade or downgrade the access level for
    // an existing client as well.  The previous access level will prevail for
    // upgrades that fail, of course.   A downgrade should never fail.  If the
    // new access level should be the same as the old for a given client, this
    // method will do nothing and return success.  In all cases, one, singular
    // close-per-client is expected for all opens-per-client received.
    //
    // This implementation replaces the IOService definition of handleOpen().
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we make our decision, change our state, and return from this method.
    //

    IOStorageAccess access = (IOStorageAccess) (uintptr_t) argument;
    IOStorageAccess level;

    IOLogOC("AppleRAIDSet::handleOpen(%p) called, client %p, access %u, state %u, client is a set = %s, raid member = %s.\n",
			this, client, (uint32_t)access, (uint32_t)arSetState, OSDynamicCast(AppleRAIDSet, client) ? "y" : "n", isRAIDMember() ? "y" : "n");

    assert(client);
    assert( access == kIOStorageAccessReader       ||
            access == kIOStorageAccessReaderWriter );

    access &= kIOStorageAccessReaderWriter;   // just in case

    // only allow "external" opens after we have published that we are online
    if (!OSDynamicCast(AppleRAIDSet, client) && getSetState() < kAppleRAIDSetStateOnline) {
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

    unsigned writers = arOpenReaderWriters->getCount();

#ifdef XXX
    if (writers >= arOpenReaderWriterMax)
    {
	IOLogOC("AppleRAIDSet::handleOpen(%p) client %p access %lu arOpenReaderWriter already set %p\n",
		this, client, access, arOpenReaderWriter);    
	return false;
    }
#endif    

    if (arOpenReaderWriters->containsObject(client)) writers--;
    if (access == kIOStorageAccessReaderWriter)      writers++;

    level = (writers) ? kIOStorageAccessReaderWriter : kIOStorageAccessReader;

    //
    // Determine whether the levels below us accept this open or not (we avoid
    // the open if the required access is the access we already hold).
    //

    if (arOpenLevel != level)
    {
	bool success = false;

//XXX	level = (level | kIOStorageAccessSharedLock);     breaks stacked raid sets, see assert above
	
	for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
	    if (arMembers[cnt] != 0) {

		IOLogOC("AppleRAIDSet::handleOpen(%p) opening %p member=%u access=%u level=%u\n",
			this, arMembers[cnt], (uint32_t)cnt, (uint32_t)access, (uint32_t)level);

		if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateRebuilding) continue;
		
		success = arMembers[cnt]->open(this, options, level);

		if (!success) {
		    IOLog("AppleRAIDSet::handleOpen(%p) client %p member %s failed to open for set \"%s\" (%s).\n",
			  this, client, arMembers[cnt]->getUUIDString(), getSetNameString(), getUUIDString());
		    IOLogOC("AppleRAIDSet::handleOpen() open failed on member %u of %u (active = %u), state = %u isOpen = %s",
			  (uint32_t)cnt, (uint32_t)arMemberCount, (uint32_t)arActiveCount, (uint32_t)arSetState, arMembers[cnt]->isOpen(NULL) ? "t" : "f");

		    // XXX this is wrong, we might need to just downgrade instead
		    
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
	
	level = (level & kIOStorageAccessReaderWriter);
    }

    //
    // Process the open.
    //

    if (access == kIOStorageAccessReader)
    {
        arOpenReaders->setObject(client);

        arOpenReaderWriters->removeObject(client);           // (for a downgrade)
    }
    else // (access == kIOStorageAccessReaderWriter)
    {
        arOpenReaderWriters->setObject(client);

        arOpenReaders->removeObject(client);                  // (for an upgrade)
    }

    arOpenLevel = level;

    changeMemberState(kAppleRAIDMemberStateOpen);	// for stacked raid sets

    IOLogOC("AppleRAIDSet::handleOpen(%p) successful, client %p, access %u, state %u\n", this, client, (uint32_t)access, (uint32_t)arSetState);

    return true;
}

bool AppleRAIDSet::handleIsOpen(const IOService * client) const
{
    if (client == 0)  return (arOpenLevel != kIOStorageAccessNone);

    bool open = arOpenReaderWriters->containsObject(client) || arOpenReaders->containsObject(client);

    IOLogOC("AppleRAIDSet::handleIsOpen(%p) client %p is %s\n", this, client, open ? "true" : "false");

    return open;
}


void AppleRAIDSet::handleClose(IOService * client, IOOptionBits options)
{
    IOLogOC("AppleRAIDSet::handleClose(%p) called, client %p current state %u\n", this, client, (uint32_t)arSetState);

    //
    // The handleClose method closes the client's access to this object.
    //
    // This implementation replaces the IOService definition of handleClose().
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we change our state and return from this method.
    //

    assert(client);

    //
    // Process the close.
    //

    if (arOpenReaderWriters->containsObject(client))  // (is it a reader-writer?)
    {
        arOpenReaderWriters->removeObject(client);
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

    IOStorageAccess level;

    if (arOpenReaderWriters->getCount())  level = kIOStorageAccessReaderWriter;
    else if (arOpenReaders->getCount())   level = kIOStorageAccessReader;
    else                                  level = kIOStorageAccessNone;

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

//XXX	    level = (level | kIOStorageAccessSharedLock);

	    bool success;
	    for (UInt32 cnt = 0; cnt < arMemberCount; cnt++) {
		if (arMembers[cnt] != 0) {
		    if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateRebuilding) continue;
		    success = arMembers[cnt]->open(this, 0, level);
		    assert(success);		// (should never fail, unless avoided deadlock)
		}
	    }

	    level = (level & kIOStorageAccessReaderWriter);  // clear the shared bit
	}

	arOpenLevel = level;
    }

    if (level == kIOStorageAccessNone) {
	changeMemberState(kAppleRAIDMemberStateClosed);		// for stacked raid sets
    }
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

void AppleRAIDSet::read(IOService *client, UInt64 byteStart,
			IOMemoryDescriptor *buffer, IOStorageAttributes * attributes, IOStorageCompletion * completion)
{
    AppleRAIDStorageRequest * storageRequest;

    IOLogRW("AppleRAIDSet::read(%p, %llu, 0x%x) this %p, state %u\n", client, byteStart, buffer ? (uint32_t)buffer->getLength() : 0, this, (uint32_t)arSetState);

    arSetCommandGate->runAction(arAllocateRequestMethod, &storageRequest);
    
    if (storageRequest != 0) {
        buffer->retain();
        storageRequest->read(client, byteStart, buffer, attributes, completion);
    } else {
	IOLogRW("AppleRAIDSet::read(%p, 0x%llx) could not allocate a storage request\n", client, byteStart);
        IOStorage::complete(completion, kIOReturnNoMedia, 0);
    }
}

void AppleRAIDSet::write(IOService *client, UInt64 byteStart,
			 IOMemoryDescriptor *buffer, IOStorageAttributes * attributes, IOStorageCompletion * completion)
{
    AppleRAIDStorageRequest * storageRequest;
    
    IOLogRW("AppleRAIDSet::write(%p, %llu, 0x%x) this %p, state %u\n", client, byteStart, buffer ? (uint32_t)buffer->getLength() : 0, this, (uint32_t)arSetState);

    arSetCommandGate->runAction(arAllocateRequestMethod, &storageRequest);
    
    if (storageRequest != 0) {
        buffer->retain();
        storageRequest->write(client, byteStart, buffer, attributes, completion);
    } else {
	IOLogRW("AppleRAIDSet::write(%p, 0x%llx) could not allocate a storage request\n", client, byteStart);
        IOStorage::complete(completion, kIOReturnNoMedia, 0);
    }
}

void AppleRAIDSet::activeReadMembers(AppleRAIDMember ** activeMembers, UInt64 byteStart, UInt32 byteCount)
{
    // XXX the default code should be able to cache this, maybe in the storage request?

    for (UInt32 index = 0; index < arMemberCount; index++) {
	AppleRAIDMember * member = arMembers[index];
	if (member && member->getMemberState() >= kAppleRAIDMemberStateClosing) {
	    activeMembers[index] = arMembers[index];
	} else {
	    activeMembers[index] = (AppleRAIDMember *)index;
	}
    }
}

void AppleRAIDSet::activeWriteMembers(AppleRAIDMember ** activeMembers, UInt64 byteStart, UInt32 byteCount)
{
    // XXX the default code should be able to cache this, maybe in the storage request?

    for (UInt32 index = 0; index < arMemberCount; index++) {
	AppleRAIDMember * member = arMembers[index];
	if (member && member->getMemberState() >= kAppleRAIDMemberStateClosing) {
	    activeMembers[index] = arMembers[index];
	} else {
	    activeMembers[index] = (AppleRAIDMember *)index;
	}
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
	    IOLog1("AppleRAIDSet::requestSynchronizeCache(%p) stalled count=%d \n", client, (int32_t)arSetIsSyncingCount);
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
	    IOLogRW("AppleRAIDSet::completeRAIDRequest - [%u] tbc 0x%llx, sbc 0x%llx bc 0x%llx, member %p, member state %u\n",
		    (uint32_t)cnt, storageRequest->srByteCount, storageRequest->srRequestByteCounts[cnt],
		    byteCount, arMembers[cnt], (uint32_t)arMembers[cnt]->getMemberState());

	    if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateClosing) {
		status = kIOReturnOffline;
	    }

	    continue;
	}
        
        // Return any status errors.
        if (storageRequest->srRequestStatus[cnt] != kIOReturnSuccess) {
            status = storageRequest->srRequestStatus[cnt];
	    IOLog("AppleRAID::completeRAIDRequest - error 0x%x detected for set \"%s\" (%s), member %s, set byte offset = %llu.\n",
		  status, getSetNameString(), getUUIDString(), arMembers[cnt]->getUUIDString(), storageRequest->srByteStart);

	    continue;
	}

	// once the status goes bad, stop counting bytes transfered
	if (status == kIOReturnSuccess) {
	    byteCount += storageRequest->srRequestByteCounts[cnt];
	}

	IOLogRW("AppleRAIDSet::completeRAIDRequest - [%u] tbc 0x%llx, sbc 0x%llx bc 0x%llx, member %p\n",
		(uint32_t)cnt, storageRequest->srByteCount, storageRequest->srRequestByteCounts[cnt],
		byteCount, arMembers[cnt]);
    }
    
    // Return an underrun error if the byte count is not complete.
    // This can happen if one or more members reported a smaller than expected byte count.
    if ((status == kIOReturnSuccess) && (byteCount != storageRequest->srByteCount)) {
	IOLog("AppleRAID::completeRAIDRequest - underrun detected, expected = 0x%llx, actual = 0x%llx, set = \"%s\" (%s)\n",
	      storageRequest->srByteCount, byteCount, getSetNameString(), getUUIDString());
        status = kIOReturnUnderrun;
        byteCount = 0;
    }
    
    storageRequest->srMemoryDescriptor->release();
    returnRAIDRequest(storageRequest);

    // Call the clients completion routine, bad status is also returned here.
    IOStorage::complete(&storageRequest->srClientsCompletion, status, byteCount);

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

    IOLog1("AppleRAID::recover %u requests are pending.\n", (uint32_t)arStorageRequestsPending);
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

    UInt32 oldActiveCount = arActiveCount;
    OSSet * brokenMembers= OSSet::withCapacity(10);
    
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

	    brokenMembers->setObject(brokenMember);
	    arSpareMembers->setObject(brokenMember);

	    brokenMember->changeMemberState(kAppleRAIDMemberStateBroken);
	}
    }

    if (oldActiveCount != arActiveCount) {

	// reconfigure the set with the remaining active members
	arController->restartSet(this, bumpOnError());

	// close the new spares
	while (brokenMembers->getCount()) {

	    AppleRAIDMember * brokenMember = (AppleRAIDMember *)brokenMembers->getAnyObject();
	    brokenMember->close(this, 0);
	    brokenMembers->removeObject(brokenMember);
	}
    }

    brokenMembers->release();
    
    bool stillAlive = arActiveCount > 0;
    
    gAppleRAIDGlobals.unlock();

    arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::unpauseSet));

    release();  // from recoverStart

    IOLog1("AppleRAID::recover finished\n");
    return stillAlive;
}
