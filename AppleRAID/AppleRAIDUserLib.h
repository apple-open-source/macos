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

#ifndef _APPLERAIDUSERLIB_H
#define _APPLERAIDUSERLIB_H

#define kAppleRAIDUserClassName		"AppleRAID"
#define kAppleRAIDSetClassName		"AppleRAIDSet"

// extras in IOMedia object for raid sets
#define kAppleRAIDIsRAIDKey		"RAID"				// CFBoolean - true

// header defines

#define kAppleRAIDHeaderVersionKey	"AppleRAID-HeaderVersion"	// CFNumber 32bit "0xMMMMmmmm" 
#define kAppleRAIDLevelNameKey		"AppleRAID-LevelName"		// CFString
#define kAppleRAIDSetNameKey		"AppleRAID-SetName"		// CFString
#define kAppleRAIDSetUUIDKey		"AppleRAID-SetUUID"		// CFString
#define kAppleRAIDSequenceNumberKey	"AppleRAID-SequenceNumber"	// CFNumber 32bit
#define kAppleRAIDChunkSizeKey		"AppleRAID-ChunkSize"		// CFNumber 64bit

#define kAppleRAIDMembersKey		"AppleRAID-Members"		// CFArray of member UUIDs CFStrings
									// (also used to determine member count)
#define kAppleRAIDSparesKey		"AppleRAID-Spares"		// CFArray of spare UUIDs CFStrings

// capabilities
#define kAppleRAIDSetAutoRebuildKey	"AppleRAID-AutoRebuild"		// CFBoolean
#define kAppleRAIDSetTimeoutKey		"AppleRAID-SetTimeout"		// CFNumber 32bit
#define kAppleRAIDSetContentHintKey	"AppleRAID-ContentHint"		// CFString

#define kAppleRAIDCanAddMembersKey	"AppleRAID-CanAddMembers"	// CFBoolean
#define kAppleRAIDCanAddSparesKey	"AppleRAID-CanAddSpares"	// CFBoolean
#define kAppleRAIDSizesCanVaryKey	"AppleRAID-SizesCanVary"	// CFBoolean - true for concat
#define kAppleRAIDRemovalAllowedKey	"AppleRAID-RemovalAllowed"	// CFString

#define kAppleRAIDRemovalNone			"None"			// stripe
#define kAppleRAIDRemovalLastMember		"Last"			// concat
#define kAppleRAIDRemovalAnyOneMember		"AnyOne"		// raid v
#define kAppleRAIDRemovalAnyMember		"Any"			// mirror

#define kAppleRAIDCanBeConvertedToKey	"AppleRAID-CanBeConvertedTo"	// CFBoolean

// member defines (varies per member)

#define kAppleRAIDMemberTypeKey		"AppleRAID-MemberType"		// CFStrings
#define kAppleRAIDMemberUUIDKey		"AppleRAID-MemberUUID"		// CFString
#define kAppleRAIDMemberIndexKey	"AppleRAID-MemberIndex"		// CFNumber 32bit
#define kAppleRAIDChunkCountKey		"AppleRAID-ChunkCount"		// CFNumber 64bit

// status defines

#define kAppleRAIDStatusKey		"AppleRAID-SetStatus"
#define kAppleRAIDMemberStatusKey	"AppleRAID-MemberStatus"

#define kAppleRAIDStatusOffline		"Offline"
#define kAppleRAIDStatusDegraded	"Degraded"
#define kAppleRAIDStatusOnline		"Online"

#define kAppleRAIDStatusMissing		"Missing"	// member only
#define kAppleRAIDStatusFailed		"Failed"	// member only
#define kAppleRAIDStatusSpare		"Standby"	// member only
#define kAppleRAIDStatusRebuilding	"Rebuilding"	// member only

#define kAppleRAIDRebuildStatus		"AppleRAID-Rebuild-Progress"

// dummy string for deleted members
#define kAppleRAIDDeletedUUID   "00000000-0000-0000-0000-000000000000"
#define kAppleRAIDMissingUUID   "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"

// Get List of Sets Flags
enum {
    kAppleRAIDOfflineSets	= 0x00000001,
    kAppleRAIDDegradedSets	= 0x00000002,
    kAppleRAIDOnlineSets	= 0x00000004,
    kAppleRAIDState		= 0x000000ff,

    kAppleRAIDVisibleSets	= 0x00000100,
    kAppleRAIDInternalSets	= 0x00000200,
    kAppleRAIDVisibility	= 0x00000f00,

    kAppleRAIDRunning		= 0x00000106,	// usable user visible sets

    kAppleRAIDAllSets		= 0xffffffff
};

#ifndef KERNEL

// CFString that contains the set's UUID string
typedef CFStringRef AppleRAIDSetRef;

// CFString that contains the member's UUID string
typedef CFStringRef AppleRAIDMemberRef;


// ********************************************************************************************
// ********************************************************************************************

#define kAppleRAIDNotificationSetDiscovered	"kAppleRAIDNotificationSetDiscovered"
#define kAppleRAIDNotificationSetChanged	"kAppleRAIDNotificationSetChanged"
#define kAppleRAIDNotificationSetTerminated	"kAppleRAIDNotificationSetTerminated"

CF_EXPORT
kern_return_t AppleRAIDEnableNotifications(void);

CF_EXPORT
kern_return_t AppleRAIDDisableNotifications(void);


// ********************************************************************************************
// ********************************************************************************************


/*!
	@function AppleRAIDGetListOfSets
	@discussion Returns a "filtered" list of the RAID sets on system, as a CFArray of AppleRAIDSetRefs.
	@param Flags - Used to determine which RAID sets to return.
*/
CF_EXPORT
CFMutableArrayRef AppleRAIDGetListOfSets(UInt32 filter);


/*!
	@function AppleRAIDGetSetProperties
	@discussion Returns CFMutableDictionaryRef - a dict with information on this set - type of set, ...
	@param AppleRAIDSetRef A handle for finding the RAID set.
*/
CF_EXPORT
CFMutableDictionaryRef AppleRAIDGetSetProperties(AppleRAIDSetRef setRef);


/*!
	@function AppleRAIDGetMemberProperties
	@discussion Returns CFMutableDictionaryRef - a dict with information on this member of the set
	Mostly useful for find out the member's status, like online, offline, missing, spare, % rebuild. 
	@param AppleRAIDMemberRef A handle for finding the RAID set's member.
*/
CF_EXPORT
CFMutableDictionaryRef AppleRAIDGetMemberProperties(AppleRAIDMemberRef setRef);


// ********************************************************************************************

/*!
	@function AppleRAIDGetUsableSize
	@discussion Returns the amount of space available to for use after RAID headers have been added.
	The primary use would be for converting a regular volume into a mirrored volume.
	The returned value is the size the current filesystem needs to be shrunk to to fit.
	@param partitionSize - size of the partition in bytes
	@param chunkSize - size of the chunkSize in bytes.
*/	
CF_EXPORT
UInt64 AppleRAIDGetUsableSize(UInt64 partitionSize, UInt64 chunkSize);


/*!
	@function AppleRAIDGetSetDescriptions
	@discussion Returns an array of dictionaries.  each dictionary contains an list of capabilites for that raid type
	
		- raid type name
		- features: supports spares, members can vary in size
		- "tradeoffs", speed, space efficency, ...
		- allowable ranges for settings: chunk
		- list of what types it can be convert to (later)
*/
CF_EXPORT
CFMutableArrayRef AppleRAIDGetSetDescriptions(void);


/*!
	@function AppleRAIDCreateSet
	@discussion Returns an AppleRAIDSetInfo dict, picks the set's UUID, sets up default values for set.
	This just creates the dictionary representing the RAID to be created, nothing sent to kernel.
	@param CFString raidType Get valid types from AppleRAIDGetSetDescriptions().
	@param CFString setName The RAID set name in UTF-8 format.
*/
CF_EXPORT
CFMutableDictionaryRef AppleRAIDCreateSet(CFStringRef raidType, CFStringRef setName);


/*!
	@function AppleRAIDAddMember
	@discussion Returns a handle to the RAID member on success.  The disk partition is not changed.
	This can be used both for creating new sets and adding to current sets.
	@param setInfo The dictionary returned from either GetSetProperties or CreateSet.
	@param partitionName Path to disk partition or raid set.
	@param typeOfMember - kAppleRAIDMembersKey, kAppleRAIDSparesKey
*/
CF_EXPORT
AppleRAIDMemberRef AppleRAIDAddMember(CFMutableDictionaryRef setInfo, CFStringRef partitionName, CFStringRef typeOfMember);


/*!
	@function AppleRAIDRemoveMember
	@discussion Removes a member from a live RAID volume.  The disk partition is not changed.
	This call can be used both for removing set members and spares.
	@param setInfo The dictionary returned from either GetSetProperties.
	@param member The member or spare to be removed.
*/
CF_EXPORT
bool AppleRAIDRemoveMember(CFMutableDictionaryRef setInfo, AppleRAIDMemberRef member);


/*!
	@function AppleRAIDModifySetProperty
	@discussion Modifies the RAID set dictionary while doing some sanity checks.  Returns true on success.
	@param setInfo The dictionary returned from either GetSetProperties or CreateSet.
	@param key Must a key that is specified by AppleRAIDGetSetDescriptions()
	@param value Must be valid for this key as specified by AppleRAIDGetSetDescriptions and IOCFSerialize-able.
*/
CF_EXPORT
bool AppleRAIDModifySet(CFMutableDictionaryRef setInfo, CFStringRef key, void * value);


/*!
	@function AppleRAIDUpdateSet
	@discussion Writes out updated RAID headers.  If notifications are enabled,
	there will be a notification for each new member and the update is complete
	@param setInfo The dictionary returned from either GetSetProperties or CreateSet.
*/
CF_EXPORT
AppleRAIDSetRef AppleRAIDUpdateSet(CFMutableDictionaryRef setInfo);


/*!
	@function AppleRAIDDestroySet
	@discussion Shutdown a raid set and zero out it's headers.
	@param AppleRAIDSetRef The handle for the RAID set.
*/
CF_EXPORT
bool AppleRAIDDestroySet(AppleRAIDSetRef set);


// ********************************************************************************************

/*!
	@function AppleRAIDRemoveHeaders
	@discussion Cleans a disk partition of any old raid headers.
	@param partitionName Path to disk partition or raid set.
*/
CF_EXPORT
bool AppleRAIDRemoveHeaders(CFStringRef partitionName);


/*!
	@function AppleRAIDDumpHeader
	@discussion Returns a CFData object for the raw header data.
	This is intended for internal diagnostic use only.
	@param partitionName Path to disk partition or raid set.
*/
CF_EXPORT
CFDataRef AppleRAIDDumpHeader(CFStringRef partitionName);

// ********************************************************************************************
// ********************************************************************************************

// NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT
// NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT

// startups a verify scan on a set for mirrors/raid 5
// also checks raid headers on all set types
// could return status via notifications (like rebuilding)
CF_EXPORT
kern_return_t AppleRAIDVerifySet(AppleRAIDSetRef set);

// NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT
// NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT YET NOT

#endif !KERNEL

#endif _APPLERAIDUSERLIB_H
