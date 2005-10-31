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

const OSSymbol * gAppleRAIDMirrorName;

#define super AppleRAIDSet
OSDefineMetaClassAndStructors(AppleRAIDMirrorSet, AppleRAIDSet);

AppleRAIDSet * AppleRAIDMirrorSet::createRAIDSet(AppleRAIDMember * firstMember)
{
    AppleRAIDMirrorSet *raidSet = new AppleRAIDMirrorSet;

    IOLog1("AppleRAIDMirrorSet::createRAIDSet(%p) called, new set = %p  *********\n", firstMember, raidSet);

    if (!gAppleRAIDMirrorName) gAppleRAIDMirrorName = OSSymbol::withCString(kAppleRAIDLevelNameMirror);  // XXX free
            
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

    retain();  // for timeout

    arRebuildThreadCall = 0;
    arSetCompleteThreadCall = 0;
    arExpectingLiveAdd = 0;

    queue_init(&arFailedRequestQueue);

    setProperty(kAppleRAIDLevelNameKey, kAppleRAIDLevelNameMirror);

    arAllocateRequestMethod = OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDSet::allocateRAIDRequest);
    
    return true;
}

bool AppleRAIDMirrorSet::initWithHeader(OSDictionary * header, bool firstTime)
{
    if (super::initWithHeader(header, firstTime) == false) return false;

    // schedule a timeout to start up degraded sets
    if (firstTime) {
	// once the set is live, arSetCompleteTimeout must stay zero
	OSNumber * number = OSDynamicCast(OSNumber, header->getObject(kAppleRAIDSetTimeoutKey));
	if (number) arSetCompleteTimeout = number->unsigned32BitValue();
	if (!arSetCompleteTimeout) arSetCompleteTimeout = kARSetCompleteTimeoutDefault;

	AbsoluteTime deadline;
	clock_interval_to_deadline(arSetCompleteTimeout, kSecondScale, &deadline);
	if (!arSetCompleteThreadCall) {
	    thread_call_func_t setCompleteMethod = OSMemberFunctionCast(thread_call_func_t, this, &AppleRAIDMirrorSet::setCompleteTimeout);
	    arSetCompleteThreadCall = thread_call_allocate(setCompleteMethod, (thread_call_param_t)this);
	}
	(void)thread_call_enter_delayed(arSetCompleteThreadCall, deadline);
    }

    return true;
}

void AppleRAIDMirrorSet::free(void)
{
    if (arRebuildThreadCall) thread_call_free(arRebuildThreadCall);
    arRebuildThreadCall = 0;
    if (arSetCompleteThreadCall) thread_call_free(arSetCompleteThreadCall);
    arSetCompleteThreadCall = 0;

    assert(queue_empty(&arFailedRequestQueue));

    super::free();
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
    
    return true;
}

bool AppleRAIDMirrorSet::resizeSet(UInt32 newMemberCount)
{
    UInt32 oldMemberCount = arMemberCount;

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

bool AppleRAIDMirrorSet::startSet(void)
{
    if (super::startSet() == false) return false;

    if (getSetState() == kAppleRAIDSetStateDegraded) {

	if (!arSetIsPaused && arSpareCount) {
	    rebuildStart();
	}

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
    return arActiveCount >= 1;
}

bool AppleRAIDMirrorSet::bumpOnError(void)
{
    return true;
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
    IOStorageCompletion storageCompletion;

    isWrite = (storageRequest->srMemoryDescriptorDirection == kIODirectionOut);
    byteCount = 0;
    expectedByteCount = isWrite ? storageRequest->srByteCount * storageRequest->srActiveCount : storageRequest->srByteCount;
    status = kIOReturnSuccess;

    // Collect the status and byte count for each member.
    for (cnt = 0; cnt < arMemberCount; cnt++) {

	// Ignore missing members.
	if (arMembers[cnt] == 0) continue;

	// rebuild members
	if (arMembers[cnt]->getMemberState() == kAppleRAIDMemberStateRebuilding) {

	    if (!isWrite) {
		assert(storageRequest->srMemberByteCounts[cnt] == 0);
		continue;
	    }
	    
	    if (storageRequest->srMemberStatus[cnt] != kIOReturnSuccess ||
		storageRequest->srMemberByteCounts[cnt] != storageRequest->srByteCount) {
		
		// This will terminate the rebuild thread
		arMembers[cnt]->changeMemberState(kAppleRAIDMemberStateBroken);
		IOLog("AppleRAID::completeRAIDRequest - write error %u detected during rebuild for set \"%s\" (%s) on target member %s, set byte offset = %llu.\n",
		      storageRequest->srMemberStatus[cnt], getSetNameString(), getUUIDString(),
		      arMembers[cnt]->getUUIDString(), storageRequest->srByteStart);
	    }
	    continue;
	}

	// offline members
	if (arMembers[cnt]->getMemberState() != kAppleRAIDMemberStateOpen) {
	    IOLogRW("AppleRAIDMirrorSet::completeRAIDRequest - [%lu] tbc 0x%llx, sbc 0x%llx bc 0x%llx, member %p, member state %lu\n",
		    cnt, storageRequest->srByteCount, storageRequest->srMemberByteCounts[cnt],
		    byteCount, arMembers[cnt], arMembers[cnt]->getMemberState());

	    status = kIOReturnIOError;
	    
	    continue;
	}
        
        // failing members
        if (storageRequest->srMemberStatus[cnt] != kIOReturnSuccess) {
	    IOLog("AppleRAID::completeRAIDRequest - error 0x%x detected for set \"%s\" (%s), member %s, set byte offset = %llu.\n",
		  storageRequest->srMemberStatus[cnt], getSetNameString(), getUUIDString(),
		  arMembers[cnt]->getUUIDString(), storageRequest->srByteStart);

            status = storageRequest->srMemberStatus[cnt];

	    // mark this member to be removed
	    arMembers[cnt]->changeMemberState(kAppleRAIDMemberStateClosing);
	    continue;
        }

	byteCount += storageRequest->srMemberByteCounts[cnt];

	IOLogRW("AppleRAIDMirrorSet::completeRAIDRequest - [%lu] tbc 0x%llx, sbc 0x%llx bc 0x%llx, member %p\n",
		cnt, storageRequest->srByteCount, storageRequest->srMemberByteCounts[cnt],
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

    // bad status is returned here
	
    storageRequest->srMemoryDescriptor->release();
    storageCompletion = storageRequest->srCompletion;
        
    returnRAIDRequest(storageRequest);
    
    // Call the clients completion routine.
    IOStorage::complete(storageCompletion, status, byteCount);
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
		    newStorageRequest->write(client, byteStart, buffer, completion);
		} else {
		    newStorageRequest->read(client, byteStart, buffer, completion);
		}

		continue;
	    }
	} 

	// give up, return an error
	IOStorage::complete(completion, kIOReturnIOError, 0);
    }

    IOLog1("AppleRAIDMirrorSet::recover exiting\n");
    return true;
}


//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888
//8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888

void AppleRAIDMirrorSet::setCompleteTimeout(void)
{
    IOLog1("AppleRAIDMirrorSet::setCompleteTimeout(%p) - timeout was for %d seconds.\n", this, (int)arSetCompleteTimeout);

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
    if (!arSpareCount) return;
    if (!arActiveCount) return;

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
    UInt32 spareIndex;
    bool autoRebuild = OSDynamicCast(OSBoolean, getProperty(kAppleRAIDSetAutoRebuildKey)) == kOSBooleanTrue;
    for (spareIndex = 0; spareIndex < arSpareCount; spareIndex++) {

	AppleRAIDMember * candidate = arSpareMembers[spareIndex];
	if (candidate) {

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

	    arSpareCount--;
	    // fill the hole
	    arSpareMembers[spareIndex] = arSpareMembers[arSpareCount];
	    arSpareMembers[arSpareCount] = 0;
	    target = candidate;
	    break;
	}
    }
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
	    
    target->setHeaderProperty(kAppleRAIDMemberIndexKey, memberIndex, 32);
    target->setHeaderProperty(kAppleRAIDSequenceNumberKey, getSequenceNumber(), 32);
    
    IOLog1("AppleRAIDMirrorSet::rebuildStart(%p) - found a target %p for index = %d\n", this, target, (int)memberIndex);

    arRebuildingMember = target;

    // add member to set at the index we are rebuilding
    if (memberUUIDs) {
	memberUUIDs->replaceObject(memberIndex, target->getUUID());
	setProperty(kAppleRAIDMembersKey, memberUUIDs);
	memberUUIDs->release();
    }
    arMembers[memberIndex] = target;
    arMembers[memberIndex]->changeMemberState(kAppleRAIDMemberStateRebuilding);

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
	UInt32 percentDone = 99, currentDone = 0x99;
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

	    // calculate % done
	    currentDone = ((offset / arSetBlockSize) * 100) / arSetBlockCount;
	    if (percentDone != currentDone) {
		percentDone = currentDone;
                OSNumber * percent = OSNumber::withNumber(percentDone, 32);
                if (percent) {
		    target->setProperty(kAppleRAIDRebuildStatus, percent);
                    percent->release();
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

    // if the target state went back to spare that mean the member is being removed from the set
    bool aborting = target->getMemberState() == kAppleRAIDMemberStateSpare;
    if (aborting) target->changeMemberState(kAppleRAIDMemberStateBroken);

    if (arSetIsPaused) arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDMirrorSet::unpauseSet));

    if (aborting) {
	arRebuildingMember = 0;
    } else {
	bool success = offset >= arSetMediaSize;
	IOCommandGate::Action rebuildCompleteMethod = OSMemberFunctionCast(IOCommandGate::Action, this, &AppleRAIDMirrorSet::rebuildComplete);
	arSetCommandGate->runAction(rebuildCompleteMethod, (void *)success);
    }

    if (arSpareCount) {
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
    
    mdMemberCount = storageRequest->srMemberCount;
    mdSetBlockSize = storageRequest->srSetBlockSize;
    
    return true;
}

bool AppleRAIDMirrorMemoryDescriptor::configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart, UInt32 activeIndex)
{
    UInt32 raidBlockStop, raidBlockEndOffset;
    UInt32 virtualMemberIndex, startMember, stopMember;
    UInt32 blockCount, extraBlocks, memberBlockCount, memberBlockStart;
    UInt32 byteCount = memoryDescriptor->getLength();
    UInt32 activeCount = mdStorageRequest->srActiveCount;
    
    _direction = memoryDescriptor->getDirection();
    
    if (_direction == kIODirectionOut) {
        mdMemberByteStart = byteStart;
        _length = byteCount;
    } else {
        mdSetBlockStart		= byteStart / mdSetBlockSize;
        mdSetBlockOffset	= byteStart % mdSetBlockSize;
        startMember		= mdSetBlockStart % activeCount;
        raidBlockStop		= (byteStart + byteCount - 1) / mdSetBlockSize;
        raidBlockEndOffset	= (byteStart + byteCount - 1) % mdSetBlockSize;
        stopMember		= raidBlockStop % activeCount;
        blockCount		= raidBlockStop - mdSetBlockStart + 1;
        memberBlockCount	= blockCount / activeCount;
        extraBlocks		= blockCount % activeCount;
        virtualMemberIndex	= (activeCount + activeIndex - startMember) % activeCount;
	memberBlockStart	= mdSetBlockStart + virtualMemberIndex * memberBlockCount + min(virtualMemberIndex, extraBlocks);

	if (virtualMemberIndex < extraBlocks) memberBlockCount++;
        
        mdMemberByteStart = (UInt64)memberBlockStart * mdSetBlockSize;
        _length = memberBlockCount * mdSetBlockSize;
        
        if (virtualMemberIndex == 0) {
            mdMemberByteStart += mdSetBlockOffset;
            _length -= mdSetBlockOffset;
        }
        
        if (virtualMemberIndex == min(blockCount - 1, activeCount - 1)) _length -= mdSetBlockSize - raidBlockEndOffset - 1;
	
	IOLogRW("mirror activeIndex = %ul, mdMemberByteStart = %llu _length =0x%lx\n", (int)activeIndex, mdMemberByteStart, _length);
    }
    
    mdMemoryDescriptor = memoryDescriptor;
        
    return _length != 0;
}

IOPhysicalAddress AppleRAIDMirrorMemoryDescriptor::getPhysicalSegment(IOByteCount offset, IOByteCount *length)
{
    UInt32		memberBlockNumber, memberBlockOffset, raidBlockNumber;
    IOByteCount		raidOffset = offset;
    IOPhysicalAddress	physAddress;
    
    if (_direction != kIODirectionOut) {
        memberBlockNumber = (mdMemberByteStart + offset) / mdSetBlockSize;
        memberBlockOffset = (mdMemberByteStart + offset) % mdSetBlockSize;
        raidBlockNumber = memberBlockNumber - mdSetBlockStart;
        raidOffset = raidBlockNumber * mdSetBlockSize + memberBlockOffset - mdSetBlockOffset;
    }
    
    physAddress = mdMemoryDescriptor->getPhysicalSegment(raidOffset, length);
    
    return physAddress;
}
