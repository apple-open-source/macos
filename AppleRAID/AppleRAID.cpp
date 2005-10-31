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

#define super IOService
OSDefineMetaClassAndStructors(AppleRAID, IOService);

bool AppleRAID::init()
{
    if (super::init() == false) return false;
    
    raidSets = OSDictionary::withCapacity(0x10);
    raidMembers = OSDictionary::withCapacity(0x10);
    
    return (raidSets && raidMembers);
}

void AppleRAID::free()
{
    if (raidSets) {
        raidSets->release();
        raidSets = 0;
    }
    if (raidMembers) {
        raidMembers->release();
        raidMembers = 0;
    }

    super::free();
}

// **************************************************************************************************

void AppleRAID::addSet(AppleRAIDSet *set)
{
    const OSString * uuid = set->getUUID();

    if (uuid) {
	raidSets->setObject(uuid, set);
    }
}

void AppleRAID::removeSet(AppleRAIDSet * set)
{
    const OSString * uuid = set->getUUID();

    if (uuid) {
	raidSets->removeObject(uuid);
    }
}

AppleRAIDSet *AppleRAID::findSet(const OSString *uuid)
{
    return OSDynamicCast(AppleRAIDSet, raidSets->getObject(uuid));
}

AppleRAIDSet *AppleRAID::findSet(AppleRAIDMember * member)
{
    const OSString * setUUID = member->getSetUUID();
    if (setUUID == 0) return 0;

    // top level set has both UUID's set the same
        
    return OSDynamicCast(AppleRAIDSet, raidSets->getObject(setUUID));
}

// **************************************************************************************************

void AppleRAID::addMember(AppleRAIDMember *member)
{
    const OSString * uuid = member->getUUID();

    if (uuid) {
	raidMembers->setObject(uuid, member);
    }
}

void AppleRAID::removeMember(AppleRAIDMember * member)
{
    const OSString * uuid = member->getUUID();

    if (uuid) {
	raidMembers->removeObject(uuid);
    }
}

AppleRAIDMember *AppleRAID::findMember(const OSString *uuid)
{
    return OSDynamicCast(AppleRAIDMember, raidMembers->getObject(uuid));
}

// **************************************************************************************************

// this should only fail in drastic cases

IOReturn AppleRAID::newMember(IORegistryEntry * child)
{
    AppleRAIDMember * member = OSDynamicCast(AppleRAIDMember, child);

    // this code is running under the global raid lock        
    gAppleRAIDGlobals.lock();

    IOLog1("AppleRAID::newMember(%p) entered.\n", child);

    while (1) {

	if (member == 0) break;
	
	// Look up the members's uuid
	const OSString * memberUUID = member->getUUID();
        if (memberUUID == 0) break;

	// check if member already exists?
	if (findMember(memberUUID)) {
	    IOLog("AppleRAID::newMember detected duplicate member %s in set \"%s\" (%s).\n",
		  member->getUUIDString(), member->getSetNameString(), member->getSetUUIDString());
	    // XXX should break the set, this is bad
	    break;
	}

	addMember(member);
	
        // Look up the set's uuid
	const OSString * setUUID = member->getSetUUID();
        if (setUUID == 0) {
	    IOLog("AppleRAID::newMember member %s in set \"%s\" has a corrupted header (no set UUID).\n",
		  member->getUUIDString(), member->getSetNameString());
	    break;
	}

	AppleRAIDSet * set = findSet(setUUID);

	bool firstTime = set == 0;

        // If the unique name was not found then create a new raid set
        if (!set) {

	    OSString * raidLevel = OSDynamicCast(OSString, member->getHeaderProperty(kAppleRAIDLevelNameKey));
	    if (!raidLevel) {
		IOLog("AppleRAID::newMember member %s in set \"%s\" (%s) has a corrupted header (no RAID level).\n",
		  member->getUUIDString(), member->getSetNameString(), member->getSetUUIDString());
		break;
	    }

	    IOLog1("AppleRAID::newMember(%p) new raid set, level = %s.\n", child, raidLevel->getCStringNoCopy());

	    // XXX - should make this dynamic at run time, have raid levels register with the controller
	    // XXX - or try to dynamically load plugins right here based on the raid level string

	    if (raidLevel->isEqualTo(kAppleRAIDLevelNameConcat)) {
		set = AppleRAIDConcatSet::createRAIDSet(member);
	    } else 
	    if (raidLevel->isEqualTo(kAppleRAIDLevelNameMirror)) {
		set = AppleRAIDMirrorSet::createRAIDSet(member);
	    } else 
	    if (raidLevel->isEqualTo(kAppleRAIDLevelNameStripe)) {
		set = AppleRAIDStripeSet::createRAIDSet(member);
            }

	    if (set) {
		IOLog1("AppleRAID::newMember(%p) raid set \"%s\" (%s) successfully created.\n",
		       child, set->getSetNameString(), set->getUUIDString());
		addSet(set);
		set->release();
	    } else {
		IOLog("AppleRAID::newMember unknown raid level %s.\n", raidLevel->getCStringNoCopy());
	    }
	}

	// only punt on headerless raid partitions
	if (!set) break;

	// is this a live add of a new member?
	// concat only, mirrors come in as spares
	if (set->isPaused() && !set->isSetComplete()) {
	    if (set->upgradeMember(member)) {
		restartSet(set, true);
		IOLog1("AppleRAID::newMember(%p) was successful (live add).\n", child);
		gAppleRAIDGlobals.unlock();
		return kIOReturnSuccess;
	    }
	    break;
	}

	// add member to raid set...
	if (!set->addMember(member)) {
	    if (!set->addSpare(member)) {
		IOLog("AppleRAID::newMember was unable to add member %s to set \"%s\" (%s).\n",
		      member->getUUIDString(), set->getSetNameString(), set->getUUIDString());
		break;
	    }
	}
	
	// try starting up the set
	startSet(set);

	// needed for user notifications
	if (firstTime) set->registerService();

	IOLog1("AppleRAID::newMember(%p) was successful.\n", child);
	gAppleRAIDGlobals.unlock();
	return kIOReturnSuccess;
    }

    IOLog1("AppleRAID::newMember(%p) failed.\n", child);

    if (member) removeMember(member);
    
    gAppleRAIDGlobals.unlock();
    return kIOReturnUnformattedMedia;
}

IOReturn AppleRAID::oldMember(IORegistryEntry * child)
{
    IOLog1("AppleRAID::oldMember(%p) entered.\n", child);

    // this code can not make any i/o requests, or it
    // may deadlock this is because the driver calling
    // this is holding it's lock

    // this code is running under the global raid lock        
    gAppleRAIDGlobals.lock();

    while (1) {

	AppleRAIDMember * member = OSDynamicCast(AppleRAIDMember, child);
	if (member == 0) break;
	
	AppleRAIDSet * set = findSet(member);
	if (!set) break;

	removeMember(member);
	set->removeMember(member, 0);

	// if this member's set is empty then nuke the set as well
	if ((set->getActiveCount() == 0) && (set->getSpareCount() == 0)) {

	    // if this set is part of another set then handle that first
	    if (set->isRAIDMember()) {
		oldMember(set);
	    }

	    IOLog1("AppleRAID::oldMember(%p) terminating parent raid set %p.\n", child, set);
	    removeSet(set);
	} else {
	    set = NULL;
	}

	IOLog1("AppleRAID::oldMember(%p) was successful.\n", child);
	gAppleRAIDGlobals.unlock();

	if (set) {
	    IOLog("AppleRAID: terminating set \"%s\" (%s).\n", set->getSetNameString(), set->getUUIDString());
	    set->terminate();
	}

	return kIOReturnSuccess;
    }

    IOLog1("AppleRAID::oldMember(%p) failed.\n", child);
    gAppleRAIDGlobals.unlock();
    return kIOReturnError;
}


void AppleRAID::recoverMember(IORegistryEntry * child)
{
    IOLog1("AppleRAID::recoverMember(%p) entered.\n", child);

    AppleRAIDMember * member = OSDynamicCast(AppleRAIDMember, child);
    if (member) {
	AppleRAIDSet * set = findSet(member);
	if (set) set->arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, set, &AppleRAIDSet::recoverStart));
    }
}


// *****************************************************************************

void AppleRAID::startSet(AppleRAIDSet * set)
{
    IOLog1("AppleRAID::startSet(%p) entered.\n", set);

    // this code is running under the global raid lock
    assert(gAppleRAIDGlobals.islocked());

    assert(!set->isPaused());

    if (set->startSet()) {

	IOLog1("AppleRAID::startSet: the set %p is started.\n", set);
	    
	// check for a "stacked" raid header, the first time
	// we get here we don't know if the set is a raid member
	if (!set->isRAIDMember()) set->start(NULL);
    }

    // let the evilness begin...
    (void)set->publishSet();

    return;
}

void AppleRAID::degradeSet(AppleRAIDSet *set)
{
    // this code is running under the global raid lock        
    gAppleRAIDGlobals.lock();

    // make sure we still know about this set and it is in the right state
    // the set has an extra retain on it to make this work (see ARM::init)
    const OSString * setUUID = set->getUUID();
    if ((set == findSet(setUUID)) && (set->getSetState() == kAppleRAIDSetStateInitializing)) {

	assert(!set->isPaused());
    
	IOLog("AppleRAID::degradeSet - starting the set \"%s\" (%s).\n",
	      set->getSetNameString(), set->getUUIDString());

	if (set->startSet()) {
	    
	    IOLog1("AppleRAID::degradeSet: the set %p is started.\n", set);
	    
	    // check for a "stacked" raid header
	    if (!set->isRAIDMember()) set->start(NULL);
	}

	if (set->getSetState() == kAppleRAIDSetStateDegraded) {
	    set->bumpSequenceNumber();
	    set->writeRAIDHeader();
	}
    
	// let the evilness begin...
	(void)set->publishSet();
    }

    gAppleRAIDGlobals.unlock();
}

void AppleRAID::restartSet(AppleRAIDSet *set, bool bump)
{
    IOLog1("AppleRAID::restartSet(%p) entered.\n", set);
    
    // this code is running under the global raid lock        
    gAppleRAIDGlobals.lock();

    IOLog("AppleRAID::restartSet - restarting set \"%s\" (%s).\n", set->getSetNameString(), set->getUUIDString());

    (void)set->startSet();
    if (bump) {
	set->bumpSequenceNumber();
	(void)set->writeRAIDHeader();
    }
    (void)set->publishSet();

    gAppleRAIDGlobals.unlock();
}


IOReturn AppleRAID::updateSet(char * setInfoBuffer, char * retBuffer, IOByteCount setInfoBufferSize, IOByteCount * retBufferSize)
{
    IOReturn rc = kIOReturnSuccess;
    IOLog1("AppleRAID::updateSet() entered\n");

    if (!isOpen()) return kIOReturnNotOpen;
    if (!setInfoBuffer || !setInfoBufferSize) return kIOReturnBadArgument;

    // this code is running under the global raid lock        
    gAppleRAIDGlobals.lock();

    while (1) {
	OSString * errmsg = 0;
	OSDictionary * updateInfo = OSDynamicCast(OSDictionary, OSUnserializeXML(setInfoBuffer, &errmsg));
	if (!updateInfo) {
	    if (errmsg) IOLog("AppleRAID::updateSet - header parsing failed with %s\n", errmsg->getCStringNoCopy());
	    errmsg->release();
	    rc = kIOReturnBadArgument;
	    break;
	}

	// find the set
	const OSString * setUUIDString = OSDynamicCast(OSString, updateInfo->getObject(kAppleRAIDSetUUIDKey));
	AppleRAIDSet * set = findSet(setUUIDString);
	if (!set) { rc = kIOReturnBadArgument; break; };
	updateInfo->removeObject(kAppleRAIDSetUUIDKey);

	// if sequence number has changed then bail, something has changed
	OSNumber * number = OSDynamicCast(OSNumber, updateInfo->getObject(kAppleRAIDSequenceNumberKey));
	if (!number) { rc = kIOReturnBadArgument;  break; };
	UInt32 seqNum = number->unsigned32BitValue();
	if (seqNum && seqNum != set->getSequenceNumber()) { rc = kIOReturnBadMessageID; break; };
	updateInfo->removeObject(kAppleRAIDSequenceNumberKey);

	number = OSDynamicCast(OSNumber, updateInfo->getObject("_update command_"));
	if (number) {
	    UInt32 subcommand = number->unsigned32BitValue();
	    
	    IOLog1("AppleRAID::updateSet() executing subcommand %d\n", (int)subcommand);

	    switch (subcommand) {

	    case kAppleRAIDUpdateResetSet:

		startSet(set);
		break;

	    case kAppleRAIDUpdateDestroySet:

		if (set->unpublishSet()) {
		    if (!set->destroySet()) {
			rc = kIOReturnError;
		    }
		}
		break;

	    default:
		IOLog("AppleRAID::updateSet() unknown subcommand %d\n", (int)subcommand);
	    }
	}
	updateInfo->removeObject("_update command_");

	// for each remaining prop that has changed call a specific set function or merge in
	if (updateInfo->getCount()) {

	    // we only need to go one level higher since we are not changing
	    // the state of any of member sets at that level
	    AppleRAIDSet * parentSet = 0;
	    if (set->isRAIDMember()) {
		parentSet = findSet((AppleRAIDMember *)set);
		IOLog1("AppleRAID::updateSet() pausing parent set %p.\n", parentSet);
		IOCommandGate::Action pauseSetMethod = OSMemberFunctionCast(IOCommandGate::Action, parentSet, &AppleRAIDSet::pauseSet);
		if (parentSet) parentSet->arSetCommandGate->runAction(pauseSetMethod, (void *)false);
	    }
	    IOLog1("AppleRAID::updateSet() pausing set %p.\n", set);
	    IOCommandGate::Action pauseSetMethod = OSMemberFunctionCast(IOCommandGate::Action, set, &AppleRAIDSet::pauseSet);
	    set->arSetCommandGate->runAction(pauseSetMethod, (void *)false);
	    
	    if (!set->reconfigureSet(updateInfo)) rc = kIOReturnError;

	    // reallocate the member arrays and republish the set's IOMedia
	    restartSet(set, true);
    
	    // if the set is waiting for a new added disk leave it paused
	    IOLog1("AppleRAID::updateSet() unpausing set.\n");

	    set->arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, set, &AppleRAIDSet::unpauseSet));
	    if (parentSet) parentSet->arSetCommandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, parentSet, &AppleRAIDSet::unpauseSet));
	}

	*(UInt32 *)retBuffer = set->getSequenceNumber();
	*retBufferSize = sizeof(UInt32);

	updateInfo->release();

	IOLog1("AppleRAID::updateSet() was %ssuccessful.\n", rc ? "un" : "");
	break;
    }
    
    gAppleRAIDGlobals.unlock();

    return rc;
}


// *****************************************************************************



IOReturn AppleRAID::getListOfSets(UInt32 inFlags, char * outList, IOByteCount * outListSize)
{
    OSCollectionIterator * iter = 0;
    OSArray * keys = 0;
    OSSerialize * s = 0;
    IOReturn rc = kIOReturnError;

    IOLog1("AppleRAID::getListOfSets() entered\n");

    if (!isOpen()) return kIOReturnNotOpen;
    if (!inFlags || !outList || !outListSize) return kIOReturnBadArgument;

    outList[0] = 0;

    // this code is running under the global raid lock        
    gAppleRAIDGlobals.lock();

    unsigned int keyCount = raidSets->getCount();

    while (keyCount) {

	keys = OSArray::withCapacity(keyCount);
	if (!keys) break;
    
	iter = OSCollectionIterator::withCollection(raidSets);
	if (!iter) break;

	while (const OSString * setName = OSDynamicCast(OSString, iter->getNextObject())) {

	    AppleRAIDSet * set = findSet(setName);
	    if (!set) continue;

	    bool addToList = false;

	    if (inFlags && kAppleRAIDState) {
		UInt32 state = set->getSetState();
		addToList = (((state < kAppleRAIDSetStateOnline)    && (inFlags & kAppleRAIDOfflineSets)) ||
			     ((state == kAppleRAIDSetStateOnline)   && (inFlags & kAppleRAIDOnlineSets))  ||
			     ((state == kAppleRAIDSetStateDegraded) && (inFlags & kAppleRAIDDegradedSets)));
	    }
	    
	    if (inFlags && kAppleRAIDVisibility) {
		bool visible = !set->isRAIDMember();
		addToList = visible ? (inFlags & kAppleRAIDVisibleSets) : (inFlags & kAppleRAIDInternalSets);
	    }

	    if (addToList) (void)keys->setObject(setName);
	}

	s = OSSerialize::withCapacity(keys->getCount() * 128);
        if (!s) break;

        s->clearText();
        if (!keys->serialize(s)) break;

	if (*outListSize < s->getLength()) {
	    IOLog("AppleRAID::getListOfSets() return buffer too small, need %d bytes, received %d.\n",
		  (int)s->getLength(), (int)*outListSize);
	    rc = kIOReturnNoSpace;
	    break;
	}
	*outListSize = s->getLength();

	bcopy(s->text(), outList, *outListSize);

	rc = kIOReturnSuccess;
	break;
    }
    if (iter) iter->release();
    if (keys) keys->release();
    if (s) s->release();
    if (keyCount == 0) rc = kIOReturnNoDevice;

    if (rc) *outListSize = 0;
    
    gAppleRAIDGlobals.unlock();

    return rc;
}


IOReturn AppleRAID::getSetProperties(char * setString, char * outProp, IOByteCount setStringSize, IOByteCount * outPropSize)
{
    IOReturn rc = kIOReturnError;
    const OSString * setName = 0;
    AppleRAIDSet * set = 0;
    OSDictionary * props = 0;
    OSSerialize * s = 0;

    IOLog1("AppleRAID::getSetProperties() entered\n");

    if (!isOpen()) return kIOReturnNotOpen;
    if (!setString || !outProp || !outPropSize) return kIOReturnBadArgument;

    outProp[0] = 0;

    // this code is running under the global raid lock        
    gAppleRAIDGlobals.lock();

    while (1) {
    
	setName = OSString::withCString(setString);
	if (!setName) break;
    
	set = findSet(setName);
	if (!set) break;

	IOLog1("setName = %s (%p)\n", setString, set);

	// get the prop list from the set
	props = set->getSetProperties();
	if (!props) break;

	s = OSSerialize::withCapacity(512);
	if (!s) break;
	
	s->clearText();
	if (!props->serialize(s)) break;

	if (*outPropSize < s->getLength()) {
	    IOLog("AppleRAID::getSetProperties() return buffer too small, need %d bytes, received %d.\n",
		  (int)s->getLength(), (int)*outPropSize);
	    rc = kIOReturnNoSpace;
	    break;
	}
	
	*outPropSize = s->getLength();
	bcopy(s->text(), outProp, *outPropSize);

	rc = kIOReturnSuccess;
	break;
    }
    if (setName) setName->release();
    if (props) props->release();
    if (s) s->release();

    if (rc) *outPropSize = 0;

    gAppleRAIDGlobals.unlock();

    return rc;
}


IOReturn AppleRAID::getMemberProperties(char * memberString, char * outProp, IOByteCount memberStringSize, IOByteCount * outPropSize)
{
    IOReturn rc = kIOReturnError;
    const OSString * memberName = 0;
    AppleRAIDMember * member = 0;
    OSDictionary * props = 0;
    OSSerialize * s = 0;

    IOLog1("AppleRAID::getMemberProperties() entered\n");

    if (!isOpen()) return kIOReturnNotOpen;
    if (!memberString || !outProp || !outPropSize) return kIOReturnBadArgument;

    outProp[0] = 0;

    // this code is running under the global raid lock        
    gAppleRAIDGlobals.lock();

    while (1) {
    
	memberName = OSString::withCString(memberString);
	if (!memberName) break;
    
	member = findMember(memberName);
	if (!member) break;

	IOLog1("memberName = %s (%p)\n", memberString, member);

	// get the prop list from the member
	props = member->getMemberProperties();
	if (!props) break;

	s = OSSerialize::withCapacity(512);
	if (!s) break;
	
	s->clearText();
	if (!props->serialize(s)) break;

	if (*outPropSize < s->getLength()) {
	    IOLog("AppleRAID::getMemberProperties() return buffer too small, need %d bytes, received %d.\n",
		  (int)s->getLength(), (int)*outPropSize);
	    rc = kIOReturnNoSpace;
	    break;
	}
	
	*outPropSize = s->getLength();
	bcopy(s->text(), outProp, *outPropSize);

	rc = kIOReturnSuccess;
	break;
    }
    if (memberName) memberName->release();
    if (props) props->release();
    if (s) s->release();

    if (rc) *outPropSize = 0;

    gAppleRAIDGlobals.unlock();

    return rc;
}
