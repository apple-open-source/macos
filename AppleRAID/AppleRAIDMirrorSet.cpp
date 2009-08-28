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

#define super AppleRAIDSet
OSDefineMetaClassAndStructors(AppleRAIDMirrorSet, AppleRAIDSet);

AppleRAIDSet * AppleRAIDMirrorSet::createRAIDSet(AppleRAIDMember * firstMember)
{
    AppleRAIDMirrorSet *raidSet = new AppleRAIDMirrorSet;

    IOLog1("AppleRAIDMirrorSet::createRAIDSet(%p) called, new set = %p  *********\n", firstMember, raidSet);

    while (raidSet){

	if (!raidSet->init()) break;
	if (!raidSet->initWithHeader(firstMember->getHeader(), true)) break;
	if (raidSet->resizeSet(raidSet->getMemberCount())) return raidSet;

	break;
    }

    if (raidSet) raidSet->release();

    return 0;
}    

bool AppleRAIDMirrorSet::init()
{
    IOLog1("AppleRAIDMirrorSet::init() called\n");

    if (super::init() == false) return false;

    arRebuildThreadCall = 0;
    arSetCompleteThreadCall = 0;
    arExpectingLiveAdd = 0;
    arMaxReadRequestFactor = 32;	// with the default 32KB blocksize -> 1 MB

    queue_init(&arFailedRequestQueue);

    setProperty(kAppleRAIDLevelNameKey, kAppleRAIDLevelNameMirror);

    arAllocateRequestMethod = OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::allocateRAIDRequest);
    
    return true;
}

bool AppleRAIDMirrorSet::initWithHeader(OSDictionary * header, bool firstTime)
{
    if (super::initWithHeader(header, firstTime) == false) return false;

    setProperty(kAppleRAIDSetAutoRebuildKey, header->getObject(kAppleRAIDSetAutoRebuildKey));
    setProperty(kAppleRAIDSetTimeoutKey, header->getObject(kAppleRAIDSetTimeoutKey));

    //	arQuickRebuildBitSize = 0;  //XXX 

    // schedule a timeout to start up degraded sets
    if (firstTime) startSetCompleteTimer();

    return true;
}

void AppleRAIDMirrorSet::free(void)
{
    if (arRebuildThreadCall) thread_call_free(arRebuildThreadCall);
    arRebuildThreadCall = 0;
    if (arSetCompleteThreadCall) thread_call_free(arSetCompleteThreadCall);
    arSetCompleteThreadCall = 0;

    if (arLastSeek) IODelete(arLastSeek, UInt64, arLastAllocCount);
    if (arSkippedIOCount) IODelete(arSkippedIOCount, UInt64, arLastAllocCount);

    assert(queue_empty(&arFailedRequestQueue));

    super::free();
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

IOBufferMemoryDescriptor * AppleRAIDMirrorSet::readPrimaryMetaData(AppleRAIDMember * member)
{
    IOBufferMemoryDescriptor * primaryBuffer = super::readPrimaryMetaData(member);

    // XXX
    
    return primaryBuffer;
}

IOReturn AppleRAIDMirrorSet::writePrimaryMetaData(IOBufferMemoryDescriptor * primaryBuffer)
{

    // XXX
    
    return super::writePrimaryMetaData(primaryBuffer);
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

bool AppleRAIDMirrorSet::addMember(AppleRAIDMember * member)
{
    if (arExpectingLiveAdd) {
	// for mirrors the set is not paused for adding while adding new
	// members, mark it as a spare here to avoid having it marked broken
	member->changeMemberState(kAppleRAIDMemberStateSpare, true);
    }
		
    if (super::addMember(member) == false) return false;

    // set block count = member block count
    OSNumber * number = OSDynamicCast(OSNumber, member->getHeaderProperty(kAppleRAIDChunkCountKey));
    if (!number) return false;
    arSetBlockCount = number->unsigned64BitValue();
    arSetMediaSize = arSetBlockCount * arSetBlockSize;
    
    if (arOpenLevel == kIOStorageAccessNone) startSetCompleteTimer();
    
    return true;
}

bool AppleRAIDMirrorSet::removeMember(AppleRAIDMember * member, IOOptionBits options)
{
    if (!super::removeMember(member, options)) return false;

    // if the set is not currently in use act like we are still gathering members
    if (arOpenLevel == kIOStorageAccessNone) {
	startSetCompleteTimer();
	arController->restartSet(this, false);
    }
    
    return true;
}

bool AppleRAIDMirrorSet::resizeSet(UInt32 newMemberCount)
{
    UInt32 oldMemberCount = arMemberCount;

    // if downsizing, just hold on to the extra space
    if (arLastAllocCount < newMemberCount) {
	if (arLastSeek) IODelete(arLastSeek, UInt64, arLastAllocCount);
	arLastSeek = IONew(UInt64, newMemberCount);
	if (!arLastSeek) return false;

	if (arSkippedIOCount) IODelete(arSkippedIOCount, UInt64, arLastAllocCount);
	arSkippedIOCount = IONew(UInt64, newMemberCount);
	if (!arSkippedIOCount) return false;
    }
    bzero(arLastSeek, sizeof(UInt64) * newMemberCount);
    bzero(arSkippedIOCount, sizeof(UInt64) * newMemberCount);

    if (super::resizeSet(newMemberCount) == false) return false;

    if (oldMemberCount && arMemberCount > oldMemberCount) arExpectingLiveAdd += arMemberCount - oldMemberCount;

    return true;
}

UInt32 AppleRAIDMirrorSet::nextSetState(void)
{
    UInt32 nextState = super::nextSetState();

    if (nextState == kAppleRAIDSetStateOnline) {
	if (arActiveCount < arMemberCount) {
	    nextState = kAppleRAIDSetStateDegraded;
	}
    }
	
    return nextState;
}

OSDictionary * AppleRAIDMirrorSet::getSetProperties(void)
{
    OSDictionary * props = super::getSetProperties();

    if (props) {
	props->setObject(kAppleRAIDSetAutoRebuildKey, getProperty(kAppleRAIDSetAutoRebuildKey));
	props->setObject(kAppleRAIDSetTimeoutKey, getProperty(kAppleRAIDSetTimeoutKey));
//	props->setObject(kAppleRAIDSetQuickRebuildKey, kOSBooleanTrue);  // XXX
    }

    return props;
}
    
bool AppleRAIDMirrorSet::startSet(void)
{
    IOLog1("AppleRAIDMirrorSet::startSet() - parallel read request max %lld bytes.\n", getSmallestMaxByteCount());
    arMaxReadRequestFactor = getSmallestMaxByteCount() / arSetBlockSize;
		
    if (super::startSet() == false) return false;

    if (getSetState() == kAppleRAIDSetStateDegraded) {

	if (getSpareCount()) rebuildStart();

    } else {
	// clear the timeout once the set is complete
	arSetCompleteTimeout = kARSetCompleteTimeoutNone;
    }

    return true;
}

bool AppleRAIDMirrorSet::publishSet(void)
{
    if (arExpectingLiveAdd) {
	IOLog1("AppleRAIDMirror::publishSet() publish ignored.\n");
	return false;
    }

    return super::publishSet();
}

bool AppleRAIDMirrorSet::isSetComplete(void)
{
    if (super::isSetComplete()) return true;

    // if timeout is still active return false
    if (arSetCompleteTimeout) return false;

    // set specific checks
    return arActiveCount != 0;
}

bool AppleRAIDMirrorSet::bumpOnError(void)
{
    return true;
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

void AppleRAIDMirrorSet::activeReadMembers(AppleRAIDMember ** activeMembers, UInt64 byteStart, UInt32 byteCount)
{
    // this code try's to do three things:
    //   1) send large single sequential i/o requests to each disk		(arMaxReadRequestFactor)
    //   2) send i/o requests to the disk with the smallest seek distance	(arLastSeek)
    //   3) balance the number of i/o requests between the available drives	(arSkippedIOCount)
    //
    // this code completely ignores the effects of writes on the head position since writes move
    // the heads on all disks.  if the disk is doing track caching then ignoring the writes can
    // still get us to a disk that may have that data already cached.
    //
    // note that arLastSeek is the last previously scheduled head position, the head may not
    // be anywhere near there yet, hence this code can schedule multiple future i/o requests

#define isOnline(member) ((UInt32)(member) >= 0x1000)    
#define isOffline(member) ((UInt32)(member) < 0x1000)    

    UInt64 distances[arMemberCount];

    for (UInt32 index = 0; index < arMemberCount; index++) {

	AppleRAIDMember * member = arMembers[index];
	if (member) {

	    UInt32 memberState = member->getMemberState();
	    if (memberState == kAppleRAIDMemberStateOpen || memberState == kAppleRAIDMemberStateClosing) {

//		UInt64 distance = (arLastSeek[index] <= byteStart) ? (byteStart - arLastSeek[index]) : 0xfffffffffffffffeULL;  // elevator
		UInt64 distance = max(arLastSeek[index], byteStart) - min(arLastSeek[index], byteStart);
//		if (arSkippedIOCount[index] >= (arMaxReadRequestFactor / 2)) distance = 0;
		if (arSkippedIOCount[index] >= 12) distance = 1;
		
		UInt32 sort = index;
		while (sort) {

		    if (isOnline((uintptr_t)activeMembers[sort-1]) && distance > distances[sort-1]) break;

		    activeMembers[sort] = activeMembers[sort-1];
		    distances[sort] = distances[sort-1];

		    sort--;
		}
		activeMembers[sort] = member;
		distances[sort] = distance;
		continue;
	    } 
	}
	activeMembers[index] = (AppleRAIDMember *)index;
	distances[index] = 0xffffffffffffffffULL;
    }

    assert((arActiveCount != arMemberCount) ? (isOffline((uintptr_t)activeMembers[arActiveCount])) : (isOnline((uintptr_t)activeMembers[arMemberCount-1])));

    // adjust last seeked to pointers and skipped counts
    UInt64 balancedBlockCount = arSetBlockSize * arMaxReadRequestFactor;
    UInt64 perMemberCount = byteCount / balancedBlockCount / arActiveCount * balancedBlockCount;
    UInt64 count = 0;

    for (UInt32 virtualIndex = 0; virtualIndex < arActiveCount; virtualIndex++) {

	AppleRAIDMember * member = activeMembers[virtualIndex];
	if (isOffline((uintptr_t)member)) break;
	UInt32 memberIndex = member->getMemberIndex();

	count = perMemberCount ? min(byteCount, perMemberCount) : min(byteCount, balancedBlockCount);
	if (count) {
	    byteStart += count;
	    byteCount -= count;
	    arLastSeek[memberIndex] = byteStart;
	    arSkippedIOCount[memberIndex] = 0;
	} else {
	    arSkippedIOCount[memberIndex]++;
	}
    }
    assert(byteCount == 0);

#ifdef DEBUG2
    static UInt32 sumCount = 0, skippedSum0 = 0, skippedSum1 = 0, overflowCount = 0;
    static UInt64 averageSeekSum = 0;

    skippedSum0 += arSkippedIOCount[0];
    skippedSum1 += arSkippedIOCount[1];
    averageSeekSum += distances[0];
    if (perMemberCount) overflowCount++;
    if (sumCount++ >= 99) {
	printf("skip0=%ld skip1=%ld, over=%ld, lastseek0=%llx lastseek1=%llx, avseek=%llx\n",
	       skippedSum0, skippedSum1, overflowCount,
	       arLastSeek[0], arLastSeek[1], averageSeekSum/100);
	sumCount = skippedSum0 = skippedSum1 = overflowCount = 0;
	averageSeekSum = 0;
    }
#endif    
}

//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

void AppleRAIDMirrorSet::completeRAIDRequest(AppleRAIDStorageRequest *storageRequest)
{
    UInt32		cnt;
    UInt64              byteCount;
    UInt64              expectedByteCount;
    IOReturn            status;
    bool		isWrite;

    isWrite = (storageRequest->srMemoryDescriptorDirection == kIODirectionOut);
    byteCount = 0;
    expectedByteCount = isWrite ? storageRequest->srByteCount * storageRequest->srActiveCount : storageRequest->srByteCount;
    status = kIOReturnSuccess;

    // Collect the status and byte count for each member.
    for (cnt = 0; cnt < arMemberCount; cnt++) {

	// Ignore missing members.
	if (arMembers[cnt] == 0) continue;

	// rebuilding members
	if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateRebuilding) {

	    if (!isWrite) {
		assert(storageRequest->srRequestByteCounts[cnt] == 0);
		continue;
	    }
	    
	    if (storageRequest->srRequestStatus[cnt] != kIOReturnSuccess ||
		storageRequest->srRequestByteCounts[cnt] != storageRequest->srByteCount) {
		
		// This will terminate the rebuild thread
		arMembers[cnt]->changeMemberState(kAppleRAIDMemberStateBroken);
		IOLog("AppleRAID::completeRAIDRequest - write error 0x%x detected during rebuild for set \"%s\" (%s) on member %s, set byte offset = %llu.\n",
		      storageRequest->srRequestStatus[cnt], getSetNameString(), getUUIDString(),
		      arMembers[cnt]->getUUIDString(), storageRequest->srByteStart);
	    }
	    continue;
	}

	// offline members
	if (arMembers[cnt]->getMemberState() != kAppleRAIDMemberStateOpen) {
	    IOLogRW("AppleRAIDMirrorSet::completeRAIDRequest - [%u] tbc 0x%llx, sbc 0x%llx bc 0x%llx, member %p, member state %u\n",
		    (uint32_t)cnt, storageRequest->srByteCount, storageRequest->srRequestByteCounts[cnt],
		    byteCount, arMembers[cnt], (uint32_t)arMembers[cnt]->getMemberState());

	    status = kIOReturnIOError;
	    
	    continue;
	}
        
        // failing members
        if (storageRequest->srRequestStatus[cnt] != kIOReturnSuccess) {
	    IOLog("AppleRAID::completeRAIDRequest - error 0x%x detected for set \"%s\" (%s), member %s, set byte offset = %llu.\n",
		  storageRequest->srRequestStatus[cnt], getSetNameString(), getUUIDString(),
		  arMembers[cnt]->getUUIDString(), storageRequest->srByteStart);

            status = storageRequest->srRequestStatus[cnt];

	    // mark this member to be removed
	    arMembers[cnt]->changeMemberState(kAppleRAIDMemberStateClosing);
	    continue;
        }

	byteCount += storageRequest->srRequestByteCounts[cnt];

	IOLogRW("AppleRAIDMirrorSet::completeRAIDRequest - [%u] tbc 0x%llx, sbc 0x%llx bc 0x%llx, member %p\n",
		(uint32_t)cnt, storageRequest->srByteCount, storageRequest->srRequestByteCounts[cnt],
		byteCount, arMembers[cnt]);
    }

    // Return an underrun error if the byte count is not complete.
    // dkreadwrite should clip any requests beyond our published size
    // however we still see underruns with pulled disks (bug?)

    if (status == kIOReturnSuccess) {

	if (byteCount != expectedByteCount) {
	    IOLog("AppleRAID::completeRAIDRequest - underrun detected on set = \"%s\" (%s)\n", getSetNameString(), getUUIDString());
	    IOLog1("AppleRAID::completeRAIDRequest - total expected = 0x%llx (0x%llx), actual = 0x%llx\n",
		   expectedByteCount, storageRequest->srByteCount, byteCount);
	    status = kIOReturnUnderrun;
	    byteCount = 0;

	} else {

	    // fix up write byte count
	    byteCount = storageRequest->srByteCount;
	}

    } else {
    
	IOLog1("AppleRAID::completeRAIDRequest - error detected\n");
	       
	UInt32 stillAliveCount = 0;

	for (cnt = 0; cnt < arMemberCount; cnt++) {

	    if (arMembers[cnt] == 0) continue;

	    if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateOpen) {
		stillAliveCount++;
	    }
	}

	// if we haven't lost the entire set, retry the failed requests
	if (stillAliveCount) {

	    bool recoveryActive = queue_empty(&arFailedRequestQueue) != true;

	    arStorageRequestsPending--;
	    queue_enter(&arFailedRequestQueue, storageRequest, AppleRAIDStorageRequest *, fCommandChain);
	    arSetCommandGate->commandWakeup(&arStorageRequestPool, /* oneThread */ false);
	    
	    // kick off the recovery thread if it isn't already active
	    if (!recoveryActive) {
		recoverStart();
	    }
	    
	    return;

	} else {

	    // or let the recovery thread finish off the set
	    recoverStart();
	}
	
	byteCount = 0;
    }

    storageRequest->srMemoryDescriptor->release();
    returnRAIDRequest(storageRequest);
    
    // Call the clients completion routine, bad status is returned here.
    IOStorage::complete(&storageRequest->srClientsCompletion, status, byteCount);
}

void AppleRAIDMirrorSet::getRecoverQueue(queue_head_t *oldRequestQueue, queue_head_t *newRequestQueue)
{
    queue_new_head(oldRequestQueue, newRequestQueue, AppleRAIDStorageRequest *, fCommandChain);
    queue_init(oldRequestQueue);
}

bool AppleRAIDMirrorSet::recover()
{
    // this is on a separate thread
    // the set is paused.

    // move failed i/o queue now in case we lose the set
    queue_head_t safeFailedRequestQueue;
    IOCommandGate::Action getRecoverQMethod = OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDMirrorSet::getRecoverQueue);
    arSetCommandGate->runAction(getRecoverQMethod, &arFailedRequestQueue, &safeFailedRequestQueue);

    // remove the bad members and rebuild the set 
    bool stillHere = super::recover();

    // the set no longer paused.
    
    IOLog1("AppleRAIDMirrorSet::recover() entered.\n");

    // requeue any previously failed i/o's
    while (!queue_empty(&safeFailedRequestQueue)) {
	AppleRAIDStorageRequest * oldStorageRequest;
	queue_remove_first(&safeFailedRequestQueue, oldStorageRequest, AppleRAIDStorageRequest *, fCommandChain);

	IOLog1("AppleRAIDMirrorSet::recover() requeuing request %p\n", oldStorageRequest);
    
	IOService *client;
	UInt64 byteStart;
	IOMemoryDescriptor *buffer;
	IOStorageCompletion completion;

	oldStorageRequest->extractRequest(&client, &byteStart, &buffer, &completion);
	oldStorageRequest->release();

	if (stillHere) {

	    AppleRAIDStorageRequest * newStorageRequest;
	    arSetCommandGate->runAction(arAllocateRequestMethod, &newStorageRequest);
	    if (newStorageRequest) {

		// retry failed request
		if (buffer->getDirection() == kIODirectionOut) {
		    newStorageRequest->write(client, byteStart, buffer, NULL, &completion);
		} else {
		    newStorageRequest->read(client, byteStart, buffer, NULL, &completion);
		}

		continue;
	    }
	} 

	// give up, return an error
	IOStorage::complete(&completion, kIOReturnIOError, 0);
    }

    IOLog1("AppleRAIDMirrorSet::recover exiting\n");
    return true;
}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

void AppleRAIDMirrorSet::startSetCompleteTimer()
{
    IOLog1("AppleRAIDMirrorSet::startSetCompleteTimer(%p) - timer %s running.\n",
	   this, arSetCompleteTimeout ? "is already" : "was not");

    // prevent timer from firing with no backing object
    retain();  

    // once the set is live, arSetCompleteTimeout must stay zero
    OSNumber * number = OSDynamicCast(OSNumber, getProperty(kAppleRAIDSetTimeoutKey));
    if (number) arSetCompleteTimeout = number->unsigned32BitValue();
    if (!arSetCompleteTimeout) arSetCompleteTimeout = kARSetCompleteTimeoutDefault;

    // set up the timer (first time only)
    if (!arSetCompleteThreadCall) {
	thread_call_func_t setCompleteMethod = OSMemberFunctionCast(thread_call_func_t, this, &AppleRAIDMirrorSet::setCompleteTimeout);
	arSetCompleteThreadCall = thread_call_allocate(setCompleteMethod, (thread_call_param_t)this);
    }

    // start timer
    AbsoluteTime deadline;
    clock_interval_to_deadline(arSetCompleteTimeout, kSecondScale, &deadline);
    // an overlapping timer request will cancel the earlier request
    bool overlap = thread_call_enter_delayed(arSetCompleteThreadCall, deadline);
    if (overlap) release();
}

void AppleRAIDMirrorSet::setCompleteTimeout(void)
{
    IOLog1("AppleRAIDMirrorSet::setCompleteTimeout(%p) - the timeout is %sactive.\n", this, arSetCompleteTimeout ? "":"in");

    // this code is outside the global lock and the workloop
    // to simplify handling race conditions with cancelling the timeout
    // we always let it fire and only release the set here.

    arSetCompleteTimeout = kARSetCompleteTimeoutNone;

    arController->degradeSet(this);
    release();
}

void AppleRAIDMirrorSet::rebuildStart(void)
{
    IOLog1("AppleRAIDMirrorSet::rebuildStart(%p) - entered\n", this);

    // are we already rebuilding a member
    if (arRebuildingMember) return;

    // sanity checks
    if (getSpareCount() == 0) return;
    if (arActiveCount == 0) return;

    // find a missing member that can be replaced
    UInt32 memberIndex;
    for (memberIndex = 0; memberIndex < arMemberCount; memberIndex++) {
	if (arMembers[memberIndex] == 0) {
	    break;
	}
    }
    if (memberIndex >= arMemberCount) return;
    
    // find a spare that is usable
    AppleRAIDMember * target = 0;
    bool autoRebuild = OSDynamicCast(OSBoolean, getProperty(kAppleRAIDSetAutoRebuildKey)) == kOSBooleanTrue;
    OSCollectionIterator * iter = OSCollectionIterator::withCollection(arSpareMembers);
    if (!iter) return;

    while (AppleRAIDMember * candidate = (AppleRAIDMember *)iter->getNextObject()) {

	if (candidate->isBroken()) {
	    IOLog1("AppleRAIDMirrorSet::rebuildStart(%p) - skipping candidate %p, it is broken.\n", this, candidate);
	    continue;
	}

	// live adds have priority over regular spares
	if (arExpectingLiveAdd) {

	    OSNumber * number = OSDynamicCast(OSNumber, candidate->getHeaderProperty(kAppleRAIDMemberIndexKey));
	    if (!number) continue;
	    UInt32 candidateIndex = number->unsigned32BitValue();
	    if (arMembers[candidateIndex]) continue;
	    memberIndex = candidateIndex;
	    candidate->changeMemberState(kAppleRAIDMemberStateSpare);
	    arExpectingLiveAdd--;

	} else {

	    // if autorebuild is not on, only use current spares
	    if (!autoRebuild) {
		if (candidate->isSpare()) {
		    OSNumber * number = OSDynamicCast(OSNumber, candidate->getHeaderProperty(kAppleRAIDSequenceNumberKey));
		    if (!number) continue;
		    UInt32 sequenceNumber = number->unsigned32BitValue();
		    if (sequenceNumber != getSequenceNumber()) {
			IOLog1("AppleRAIDMirrorSet::rebuildStart(%p) - skipping candidate %p, expired seq num %d.\n",
			       this, candidate, (int)sequenceNumber);
			continue;
		    }
		} else {
		    IOLog1("AppleRAIDMirrorSet::rebuildStart(%p) - skipping candidate %p, autorebuild is off.\n", this, candidate);
		    continue;
		}
	    }
	}

	arSpareMembers->removeObject(candidate);  // must break, this breaks iter
	target = candidate;
	break;
    }
    iter->release();
    if (!target) return;

    // pull the spare uuid out of the spare uuid list, only for v2 headers
    OSArray * spareUUIDs = OSDynamicCast(OSArray, getProperty(kAppleRAIDSparesKey));
    if (spareUUIDs) spareUUIDs = OSArray::withArray(spareUUIDs);
    if (spareUUIDs) {
	UInt32 spareCount = spareUUIDs ? spareUUIDs->getCount() : 0;
	for (UInt32 i = 0; i < spareCount; i++) {
	    OSString * uuid = OSDynamicCast(OSString, spareUUIDs->getObject(i));
	    if (uuid && uuid->isEqualTo(target->getUUID())) {
		spareUUIDs->removeObject(i);
	    }
	}
	setProperty(kAppleRAIDSparesKey, spareUUIDs);
	spareUUIDs->release();
    }
    
    // if this member was part of the set, rebuild it at it's old index
    OSArray * memberUUIDs = OSDynamicCast(OSArray, getProperty(kAppleRAIDMembersKey));
    if (memberUUIDs) memberUUIDs = OSArray::withArray(memberUUIDs);
    if (memberUUIDs) {
	UInt32 memberCount = memberUUIDs ? memberUUIDs->getCount() : 0;
	for (UInt32 i = 0; i < memberCount; i++) {
	    OSString * uuid = OSDynamicCast(OSString, memberUUIDs->getObject(i));
	    if (uuid && uuid->isEqualTo(target->getUUID())) {
		if (arMembers[i] == NULL) {
		    memberIndex = i;
		    break;
		}
		IOLog("AppleRAIDMirrorSet::rebuildStart() - spare already active at index = %d?\n", (int)memberIndex);
		assert(0);  // this should never happen
		return;
	    }
	}
    }

    target->setMemberIndex(memberIndex);
    target->setHeaderProperty(kAppleRAIDSequenceNumberKey, getSequenceNumber(), 32);
    
    IOLog1("AppleRAIDMirrorSet::rebuildStart(%p) - found a target %p for index = %d\n", this, target, (int)memberIndex);

    // let any current i/o's finish before reconfiguring the mirror as writes then are expected to go to the rebuilding member.
    arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDMirrorSet::pauseSet), (void *)false);

    arRebuildingMember = target;

    // add member to set at the index we are rebuilding
    // note that arActiveCount is not bumped
    if (memberUUIDs) {
	memberUUIDs->replaceObject(memberIndex, target->getUUID());
	setProperty(kAppleRAIDMembersKey, memberUUIDs);
	memberUUIDs->release();
    }
    arMembers[memberIndex] = target;
    arMembers[memberIndex]->changeMemberState(kAppleRAIDMemberStateRebuilding);

    arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDMirrorSet::unpauseSet));

    if (!arRebuildThreadCall) {
	thread_call_func_t rebuildMethod = OSMemberFunctionCast(thread_call_func_t, this, &AppleRAIDMirrorSet::rebuild);
	arRebuildThreadCall = thread_call_allocate(rebuildMethod, (thread_call_param_t)this);
    }

    // the rebuild runs outside the workloop and global raid lock
    // if the whole set goes, it has no idea, this keeps the set
    // from disappearing underneath the rebuild
    retain();

    if (arRebuildThreadCall) (void)thread_call_enter(arRebuildThreadCall);
}


// *** this in not inside the workloop ***

void AppleRAIDMirrorSet::rebuild()
{
    IOLog1("AppleRAIDMirrorSet::rebuild(%p) - entered\n", this);

    AppleRAIDMember * target = arRebuildingMember;
    AppleRAIDMember * source = 0;
    bool targetOpen = false;
    bool sourceOpen = false;
    UInt32 sourceIndex = 0;
    IOBufferMemoryDescriptor * rebuildBuffer = 0;
    UInt64 offset = 0;
    IOReturn rc;

    // the rebuild is officially started
    messageClients(kAppleRAIDMessageSetChanged);

    // all failures need to call rebuildComplete

    while (true) {

	// XXX this code should be double buffered

	// there is a race between the code that kicks off this thread and this thread.
	// the other thread is updating the raid headers and if the set is not opened
	// it closes the members when it is done.  since there is no open/close counting
	// that causes problems in this code by closing the member underneath us.
	// since the other thread is holding the global lock if we also try to grab the
	// lock this code will block until the headers are updated.
	gAppleRAIDGlobals.lock();
	// shake your head in disgust
	gAppleRAIDGlobals.unlock();
	
	// allocate copy buffers
	rebuildBuffer = IOBufferMemoryDescriptor::withCapacity(arSetBlockSize, kIODirectionNone);
	if (rebuildBuffer == 0) break;
	
	// Open the target member
	targetOpen = target->open(this, 0, kIOStorageAccessReaderWriter);
	if (!targetOpen) break;

	// clear the on disk spare state and reset the sequence number
	target->setHeaderProperty(kAppleRAIDMemberTypeKey, kAppleRAIDMembersKey);	
	target->setHeaderProperty(kAppleRAIDSequenceNumberKey, 0, 32);
	target->writeRAIDHeader();

	offset = arBaseOffset;
	clock_sec_t oldTime = 0;
	while (offset < arSetMediaSize) {

	    IOLog2("AppleRAIDMirrorSet::rebuild(%p) - offset = %llu bs=%llu\n", this, offset, arSetBlockSize);

	    // if the set is idle pause regular i/o
	    IOCommandGate::Action pauseMethod = OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDMirrorSet::pauseSet);
	    while (arSetCommandGate->runAction(pauseMethod, (void *)true) == false) {
		IOSleep(100);
	    }

	    // check if we failed during normal i/o
	    if (target->getMemberState() != kAppleRAIDMemberStateRebuilding) break;

	    // find a source drive, also check if it changed
	    // the set is paused here, this should be safe
	    if (!sourceOpen || !arMembers[sourceIndex]) {
		if (sourceOpen) close(this, 0);
		sourceOpen = false;
		for (sourceIndex = 0; sourceIndex < arMemberCount; sourceIndex++) {
		    if (arMembers[sourceIndex] == target) continue;
		    if ((source = arMembers[sourceIndex])) break;
		}
		if (!source) break;
		sourceOpen = open(this, 0, kIOStorageAccessReader);
		if (!sourceOpen) break;
	    }
	    
	    // Fill the read buffer
	    rebuildBuffer->setDirection(kIODirectionIn);
	    rc = source->IOStorage::read((IOService *)this, offset, rebuildBuffer);
	    if (rc) {
		    IOLog("AppleRAIDMirrorSet::rebuild() - read failed with 0x%x on member %s, member byte offset = %llu\n",
			  rc, source->getUUIDString(), offset);
		break;
	    }

	    rebuildBuffer->setDirection(kIODirectionOut);
	    rc = target->IOStorage::write((IOService *)this, offset, rebuildBuffer);
	    if (rc) {
		// give up
		IOLog("AppleRAIDMirrorSet::rebuild() - write failed with 0x%x on member %s, member byte offset = %llu\n",
		      rc, target->getUUIDString(), offset);
		break;
	    }

	    arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDMirrorSet::unpauseSet));

	    // update rebuild status once a second
	    clock_sec_t newTime;
	    clock_usec_t dontcare;	
	    clock_get_system_microtime(&newTime, &dontcare);
	    if (newTime != oldTime) {	
		oldTime = newTime;

		OSNumber * bytesCompleted = OSDynamicCast(OSNumber, target->getProperty(kAppleRAIDRebuildStatus));
		if (bytesCompleted) {
		    // avoids a race with getMemberProperties
		    bytesCompleted->setValue(offset);
		} else {
		    bytesCompleted = OSNumber::withNumber(offset, 64);
		    if (bytesCompleted) {
			target->setProperty(kAppleRAIDRebuildStatus, bytesCompleted);
			bytesCompleted->release();
		    }
		}
	    }

	    // keep requests aligned (header != block size)
	    if ((offset % arSetBlockSize) != 0) offset = (offset / arSetBlockSize) * arSetBlockSize;
	    
	    offset += arSetBlockSize;
	}

	break;
    }

    // rebuilding member state changes: spare -> rebuilding -> rebuilding (open) -> closed -> open or broken

    // clean up
    if (rebuildBuffer) {
	rebuildBuffer->release();
	rebuildBuffer = 0;
    }

    if (sourceOpen) close(this, 0);
    if (targetOpen) target->close(this, 0);

    // if the target state went back to spare that means the member is being removed from the set
    bool aborting = target->getMemberState() == kAppleRAIDMemberStateSpare;
    if (aborting) target->changeMemberState(kAppleRAIDMemberStateBroken);

    if (arSetIsPaused) arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDMirrorSet::unpauseSet));

    if (aborting) {
	// calling rebuildComplete hangs on the global lock, just bail out
	arRebuildingMember = 0;
    } else {
	bool success = offset >= arSetMediaSize;
	IOCommandGate::Action rebuildCompleteMethod = OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDMirrorSet::rebuildComplete);
	arSetCommandGate->runAction(rebuildCompleteMethod, (void *)success);
    }

    if (getSpareCount()) {
	gAppleRAIDGlobals.lock();
	rebuildStart();
	gAppleRAIDGlobals.unlock();
    }

    // just in case the set's status does not need to change
    messageClients(kAppleRAIDMessageSetChanged);

    release();
}

void AppleRAIDMirrorSet::rebuildComplete(bool rebuiltComplete)
{
    AppleRAIDMember * target = arRebuildingMember;
    UInt32 memberIndex = target->getMemberIndex();

    // this is running in the workloop
    // target is closed

    pauseSet(false);

    // clear rebuild progress from target
    target->removeProperty(kAppleRAIDRebuildStatus);
    
    // remove from set
    this->detach(arMembers[memberIndex]);
    arMembers[memberIndex] = 0;

    gAppleRAIDGlobals.lock();
	
    // add member back into the raid set, update raid headers
    if (rebuiltComplete && upgradeMember(target)) {

	arController->restartSet(this, true);

	IOLog("AppleRAIDMirrorSet::rebuild complete for set \"%s\" (%s).\n", getSetNameString(), getUUIDString());
	    
    } else {

	IOLog("AppleRAIDMirrorSet::rebuild: copy failed for set \"%s\" (%s).\n", getSetNameString(), getUUIDString());

	// just leave this member in the set's member uuid list
	// but mark member as broken
	target->changeMemberState(kAppleRAIDMemberStateBroken);

	// and toss it back in the spare pile
	addSpare(target);
    }
    
    gAppleRAIDGlobals.unlock();

    unpauseSet();
	
    // kick off next rebuild (if needed)
    arRebuildingMember = 0;
}


AppleRAIDMemoryDescriptor * AppleRAIDMirrorSet::allocateMemoryDescriptor(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    return AppleRAIDMirrorMemoryDescriptor::withStorageRequest(storageRequest, memberIndex);
}


// AppleRAIDMirrorMemoryDescriptor
// AppleRAIDMirrorMemoryDescriptor
// AppleRAIDMirrorMemoryDescriptor

#undef super
#define super AppleRAIDMemoryDescriptor
OSDefineMetaClassAndStructors(AppleRAIDMirrorMemoryDescriptor, AppleRAIDMemoryDescriptor);

AppleRAIDMemoryDescriptor *
AppleRAIDMirrorMemoryDescriptor::withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    AppleRAIDMemoryDescriptor *memoryDescriptor = new AppleRAIDMirrorMemoryDescriptor;
    
    if (memoryDescriptor != 0) {
        if (!memoryDescriptor->initWithStorageRequest(storageRequest, memberIndex)) {
            memoryDescriptor->release();
            memoryDescriptor = 0;
        }
    }
    
    return memoryDescriptor;
}

bool AppleRAIDMirrorMemoryDescriptor::initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    if (!super::initWithStorageRequest(storageRequest, memberIndex)) return false;
    
    mdSetBlockSize = storageRequest->srSetBlockSize;
    
    return true;
}

bool AppleRAIDMirrorMemoryDescriptor::configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart, UInt32 activeIndex)
{
    UInt32 byteCount = memoryDescriptor->getLength();
    UInt32 blockCount, memberBlockCount;
    UInt64 setBlockStop, memberBlockStart;
    UInt32 extraBlocks, setBlockStopOffset;
    UInt32 startIndex, virtualIndex;
    UInt32 activeCount = mdStorageRequest->srActiveCount;
    
    _flags = (_flags & ~kIOMemoryDirectionMask) | memoryDescriptor->getDirection();
    
    if (_flags & kIODirectionOut) {
	mdMemberByteStart = byteStart;
	_length = byteCount;
    } else {
	mdSetBlockStart		= byteStart / mdSetBlockSize;
	mdSetBlockOffset	= byteStart % mdSetBlockSize;
	setBlockStop		= (byteStart + byteCount - 1) / mdSetBlockSize;
	setBlockStopOffset	= (byteStart + byteCount - 1) % mdSetBlockSize;
	blockCount		= setBlockStop - mdSetBlockStart + 1;
	memberBlockCount	= blockCount / activeCount;
	extraBlocks		= blockCount % activeCount;
	startIndex		= mdSetBlockStart % activeCount;

	// per member stuff

	// find our index relative to this starting member for this request
	virtualIndex = (activeCount + activeIndex - startIndex) % activeCount;
	memberBlockStart = mdSetBlockStart + virtualIndex * memberBlockCount + min(virtualIndex, extraBlocks);
	if (virtualIndex < extraBlocks) memberBlockCount++;

	// find the transfer size for this member
	mdMemberByteStart = memberBlockStart * mdSetBlockSize;
	_length = memberBlockCount * mdSetBlockSize;

	// adjust for the starting inter-block offset
	if (virtualIndex == 0) {
	    mdMemberByteStart += mdSetBlockOffset;   // XXX same as byteStart?
	    _length	      -= mdSetBlockOffset;
	}

	// adjust for ending inter-block offset
	if (virtualIndex == min(blockCount - 1, activeCount - 1)) _length -= mdSetBlockSize - setBlockStopOffset - 1;

	IOLogRW("mirror activeIndex = %u, mdMemberByteStart = %llu _length = 0x%x\n", (uint32_t)activeIndex, mdMemberByteStart, (uint32_t)_length);
    }
    
    mdMemoryDescriptor = memoryDescriptor;
	
    return _length != 0;
}

addr64_t AppleRAIDMirrorMemoryDescriptor::getPhysicalSegment(IOByteCount offset, IOByteCount *length, IOOptionBits options)
{
    IOByteCount		setOffset = offset;
    addr64_t		physAddress;
    UInt32		memberBlockStart, memberBlockOffset, blockCount;

    if (_flags & kIODirectionIn) {
        memberBlockStart = (mdMemberByteStart + offset) / mdSetBlockSize;
        memberBlockOffset = (mdMemberByteStart + offset) % mdSetBlockSize;
        blockCount = memberBlockStart - mdSetBlockStart;
        setOffset = blockCount * mdSetBlockSize + memberBlockOffset - mdSetBlockOffset;
    }
    
    physAddress = mdMemoryDescriptor->getPhysicalSegment(setOffset, length, options);
    
    return physAddress;
}
