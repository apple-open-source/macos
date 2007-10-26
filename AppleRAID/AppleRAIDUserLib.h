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

#ifndef _APPLERAIDUSERLIB_H
#define _APPLERAIDUSERLIB_H

#define kAppleRAIDUserClassName		"AppleRAID"
#define kAppleRAIDSetClassName		"AppleRAIDSet"
#define kAppleLogicalVolumeClassName	"AppleLVMVolume"

// extras in IOMedia object for raid sets
#define kAppleRAIDIsRAIDKey		"RAID"				// CFBoolean - true
#define kAppleLVMIsLogicalVolumeKey	"LVM"				// CFBoolean - true

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
#define kAppleRAIDSetContentHintKey	"AppleRAID-ContentHint"		// CFString

#define kAppleRAIDNoMediaExport			"RAIDNoMedia"		// CFString - for LVG
#define kAppleRAIDNoFileSystem			"RAIDNoFS"		// CFString - for stacked raid sets 

#define kAppleRAIDSetQuickRebuildKey	"AppleRAID-QuickRebuild"	// CFBoolean       (faked, not in header)
#define kAppleRAIDSetTimeoutKey		"AppleRAID-SetTimeout"		// CFNumber 32bit
#define kAppleRAIDPrimaryMetaDataUsedKey "AppleRAID-MetaData1Used"	// CFNumber 64bit  (freespace, lvg toc)

#define kAppleRAIDCanAddMembersKey	"AppleRAID-CanAddMembers"	// CFBoolean
#define kAppleRAIDCanAddSparesKey	"AppleRAID-CanAddSpares"	// CFBoolean
#define kAppleRAIDSizesCanVaryKey	"AppleRAID-SizesCanVary"	// CFBoolean - true for concat, lvg
#define kAppleRAIDRemovalAllowedKey	"AppleRAID-RemovalAllowed"	// CFString

#define kAppleRAIDRemovalNone			"None"			// stripe
#define kAppleRAIDRemovalLastMember		"Last"			// concat
#define kAppleRAIDRemovalAnyOneMember		"AnyOne"		// raid v
#define kAppleRAIDRemovalAnyMember		"Any"			// mirror

#define kAppleRAIDCanBeConvertedToKey	"AppleRAID-CanBeConvertedTo"	// CFBoolean

#define kAppleRAIDLVGExtentsKey		"AppleRAID-LVGExtents"		// CFNumber 64bit  (faked, lvg only)
#define kAppleRAIDLVGVolumeCountKey	"AppleRAID-LVGVolumeCount"	// CFNumber 32bit  (faked, lvg only)
#define kAppleRAIDLVGFreeSpaceKey	"AppleRAID-LVGFreeSpace"	// CFNumber 64bit  (faked, lvg only)

// member defines (varies per member)

#define kAppleRAIDMemberTypeKey		"AppleRAID-MemberType"		// CFStrings
#define kAppleRAIDMemberUUIDKey		"AppleRAID-MemberUUID"		// CFString
#define kAppleRAIDMemberIndexKey	"AppleRAID-MemberIndex"		// CFNumber 32bit
#define kAppleRAIDChunkCountKey		"AppleRAID-ChunkCount"		// CFNumber 64bit
#define kAppleRAIDMemberStartKey	"AppleRAID-MemberStart"		// CFNumber 64bit  (faked, lvg only)
#define kAppleRAIDSecondaryMetaDataSizeKey "AppleRAID-MetaData2Size"	// CFNumber 64bit  (lve data, varies per member)

// logical volume defines

#define kAppleLVMVolumeVersionKey	"AppleLVM-Version"		// CFNumber 32bit "0xMMMMmmmm" 
#define kAppleLVMVolumeUUIDKey		"AppleLVM-VolumeUUID"		// CFString
#define kAppleLVMGroupUUIDKey		"AppleLVM-GroupUUID"		// CFString
//#define kAppleLVMSnapShotUUIDKey	"AppleLVM-SnapUUID"		// CFString
//#define kAppleLVMBitMapUUIDKey	"AppleLVM-BitMapUUID"		// CFString
#define kAppleLVMParentUUIDKey		"AppleLVM-ParentUUID"		// CFString
#define kAppleLVMVolumeSequenceKey	"AppleLVM-Sequence"		// CFNumber 32bit
#define kAppleLVMVolumeSizeKey		"AppleLVM-VolumeSize"		// CFNumber 64bit  (faked)
#define kAppleLVMVolumeExtentCountKey	"AppleLVM-ExtentCount"		// CFNumber 64bit
#define kAppleLVMVolumeTypeKey		"AppleLVM-Type"			// CFString
#define kAppleLVMVolumeTypeConcat		"concat"
#define kAppleLVMVolumeTypeStripe		"stripe"
#define kAppleLVMVolumeTypeMirror		"mirror"
#define kAppleLVMVolumeTypeSnapRO		"snap ro"		// AppleLVMSnapShotVolume only
#define kAppleLVMVolumeTypeSnapRW		"snap rw"		// AppleLVMSnapShotVolume only
#define kAppleLVMVolumeTypeBitMap		"bitmap"		// internal use only
#define kAppleLVMVolumeTypeMaster		"master"		// internal use only
#define kAppleLVMVolumeLocationKey	"AppleLVM-Location"		// CFString
#define kAppleLVMVolumeLocationFast		"fast"	
#define kAppleLVMVolumeLocationMedium		"medium"	
#define kAppleLVMVolumeLocationSlow		"slow"	
#define kAppleLVMVolumeContentHintKey	"AppleLVM-ContentHint"		// CFString
#define kAppleLVMVolumeStatusKey	"AppleLVM-Status"		// CFString
#define kAppleLVMVolumeNameKey		"AppleLVM-Name"			// CFString

// XXXSNAP remaining free space for snap

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

#define kAppleRAIDRebuildStatus		"AppleRAID-Rebuild-Progress"	// CFNumber 64bit (bytes completed)

// dummy string for deleted members
#define kAppleRAIDDeletedUUID   "00000000-0000-0000-0000-000000000000"
#define kAppleRAIDMissingUUID   "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"

enum {
    kAppleRAIDDummySpareIndex	= 9999
};

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

#define kAppleRAIDNotificationSetDiscovered	"AppleRAIDNotificationSetDiscovered"
#define kAppleRAIDNotificationSetChanged	"AppleRAIDNotificationSetChanged"
#define kAppleRAIDNotificationSetTerminated	"AppleRAIDNotificationSetTerminated"

#define kAppleLVMNotificationVolumeDiscovered	"AppleLVMNotificationVolumeDiscovered"
#define kAppleLVMNotificationVolumeChanged	"AppleLVMNotificationVolumeChanged"
#define kAppleLVMNotificationVolumeTerminated	"AppleLVMNotificationVolumeTerminated"

CF_EXPORT
kern_return_t AppleRAIDEnableNotifications(void);

CF_EXPORT
kern_return_t AppleRAIDDisableNotifications(void);


// ********************************************************************************************
// ********************************************************************************************


/*!
	@function AppleRAIDGetListOfSets
	@discussion Returns a "filtered" list of the RAID sets on system, as a CFArray of AppleRAIDSetRefs.
	@param filter - Used to determine which types RAID sets to return.
*/
CF_EXPORT
CFMutableArrayRef AppleRAIDGetListOfSets(UInt32 filter);


/*!
	@function AppleRAIDGetSetProperties
	@discussion Returns CFMutableDictionaryRef - a dict with information on this set - type of set, ...
	@param setRef A handle for finding the RAID set.
*/
CF_EXPORT
CFMutableDictionaryRef AppleRAIDGetSetProperties(AppleRAIDSetRef setRef);


/*!
	@function AppleRAIDGetMemberProperties
	@discussion Returns CFMutableDictionaryRef - a dict with information on this member of the set
	Mostly useful for find out the member's status, like online, offline, missing, spare, % rebuild. 
	@param memberRef A handle for finding the RAID set's member.
*/
CF_EXPORT
CFMutableDictionaryRef AppleRAIDGetMemberProperties(AppleRAIDMemberRef memberRef);


// ********************************************************************************************

/*!
	@function AppleRAIDGetUsableSize
	@discussion Returns the amount of space available to for use after RAID headers have been added.
	The primary use would be for converting a regular volume into a mirrored or JBOD volume.
	The returned value is the size the current filesystem needs to be shrunk to in order to fit.
	@param partitionSize - size of the partition in bytes
	@param chunkSize - size of the chunkSize in bytes.
	@param options - options that this set will support that require additional disk space
*/	
CF_EXPORT
UInt64 AppleRAIDGetUsableSize(UInt64 partitionSize, UInt64 chunkSize, UInt32 options);

// XXX use raid type strings here instead?  there is no mirror with quickrebuild string

#define kAppleRAIDUsableSizeOptionNone			0x0
#define kAppleRAIDUsableSizeOptionQuickRebuild		0x1
#define kAppleRAIDUsableSizeOptionLVG			0x2	// for enable

/*!
	@function AppleRAIDGetSetDescriptions
	@discussion Returns an array of dictionaries. Each dictionary contains an list of capabilites for that raid type:
	
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
	@param raidType Get valid types from AppleRAIDGetSetDescriptions().
	@param setName The RAID set name in UTF-8 format.
*/
CF_EXPORT
CFMutableDictionaryRef AppleRAIDCreateSet(CFStringRef raidType, CFStringRef setName);


/*!
	@function AppleRAIDAddMember
	@discussion Returns a handle to the RAID member on success.  The disk partition is not changed.
	This can be used both for creating new sets and adding to current sets.
	@param setInfo The dictionary returned from either GetSetProperties or CreateSet.
	@param partitionName Path to disk partition or raid set.
	@param memberType - kAppleRAIDMembersKey, kAppleRAIDSparesKey
*/
CF_EXPORT
AppleRAIDMemberRef AppleRAIDAddMember(CFMutableDictionaryRef setInfo, CFStringRef partitionName, CFStringRef memberType);


/*!
	@function AppleRAIDRemoveMember
	@discussion Removes a member from a live RAID volume.  The disk partition is not changed.
	This call can be used both for removing set members and spares.
	@param setInfo The dictionary returned from either GetSetProperties.
	@param memberRef The member or spare to be removed.
*/
CF_EXPORT
bool AppleRAIDRemoveMember(CFMutableDictionaryRef setInfo, AppleRAIDMemberRef memberRef);


/*!
	@function AppleRAIDModifySet
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
	@param setRef The handle for the RAID set.
*/
CF_EXPORT
bool AppleRAIDDestroySet(AppleRAIDSetRef setRef);


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
//
//                                     L V M
//
// ********************************************************************************************
// ********************************************************************************************


// CFString that contains the Logical Volume's UUID string
typedef CFStringRef AppleLVMVolumeRef;



// ********************************************************************************************
// Logical Volume state 
// ********************************************************************************************

/*!
	@function AppleLVMGetVolumesForGroup
	@discussion Returns the list of logical volumes contained (whole or partial) in the
	the specied logical volume group's member.  If the member is not specified this function
	returns all logical volumes in the logical volume group.  This call should work if the
	member is missing, allowing volumes on a dead disk to be removed.
	@param setRef The handle for the logical volume group returned from AppleRAIDCreateSet().
	@param memberRef A member from the logical volume group or nil.
*/
CF_EXPORT
CFMutableArrayRef AppleLVMGetVolumesForGroup(AppleRAIDSetRef setRef, AppleRAIDMemberRef member);

/*!
	@function AppleLVMGetVolumeProperties
	@discussion Returns a dictionary of properties for this logical volume.

		- size of logical volume
		- location of logical volume 
		- snapshot of (UUID of snapshoted volume)
		- ...
	
	@param volRef The handle to the logical volume.
*/
CF_EXPORT
CFMutableDictionaryRef AppleLVMGetVolumeProperties(AppleLVMVolumeRef volRef);

/*!
	@function AppleLVMGetVolumeExtents
	@discussion Returns an C array of extents for this logical volume.
	@param volRef The handle to the logical volume.
*/
CF_EXPORT
CFDataRef AppleLVMGetVolumeExtents(AppleLVMVolumeRef volRef);

/*!
	@function AppleLVMGetVolumeDescription(void);
	@discussion Returns a dictionary contains an list of supported keys
	for each type of logical volume.
	
		- content hint
		- location of logical volume (fast, average, slow)
*/
CF_EXPORT
CFMutableArrayRef AppleLVMGetVolumeDescription(void);


// ********************************************************************************************
// Logical Volume creation and deletion
// ********************************************************************************************


/*!
	@function AppleLVMCreateVolume
	@discussion Creates a new logical volume dictionary for an existing logical volume group.
	It does not write any information to disk, use AppleLVMUpdateVolume() for that.
	Returns a dictionary for the new logical volume on success.
	@param setRef The handle for the logical volume group returned from AppleRAIDCreateSet().
	@param volumeType Type of logical volume, concat, mirror, stripe, ...
	@param volumeSize Requested size for the new logical volume. Will be rounded up to the nearest volume allocation multiple.
	@param volumeLocation Where on the disk, fast section, slow section, ..
*/
CF_EXPORT
CFMutableDictionaryRef AppleLVMCreateVolume(AppleRAIDSetRef setRef, CFStringRef volumeType, UInt64 volumeSize, CFStringRef volumeLocation);


/*!
	@function AppleLVMModifyVolume
	@discussion Modifies the properties of a logical volume while doing some sanity checks.  Returns true on success.
	@param volumeProperties The dictionary for the logical volume, either from AppleLVMCreateVolume() or AppleLVMGetVolumeProperties(),
	@param key Must a key that is specified by AppleLVMGetVolumeDescriptions()
	@param value Must be valid for this key as specified by AppleLVMGetVolumeDescriptions and IOCFSerialize-able.
*/
CF_EXPORT
bool AppleLVMModifyVolume(CFMutableDictionaryRef volumeProperties, CFStringRef key, void * value);


/*!
	@function AppleLVMUpdateVolume
	@discussion Write logical volume while doing some sanity checks.
	Returns the handle to the new or updated logical volume.
	@param volumeProperties The dictionary for the logical volume, either from AppleLVMCreateVolume() or AppleLVMGetVolumeProperties(),
*/
CF_EXPORT
AppleLVMVolumeRef AppleLVMUpdateVolume(CFMutableDictionaryRef volumeProperties);


/*!
	@function AppleLVMDestroyVolume
	@discussion Destroys a logical volume and frees up the space it was using.
	This call is supported on logical volumes that are missing.
	@param volRef The handle to the logical volume.
*/
CF_EXPORT
bool AppleLVMDestroyVolume(AppleLVMVolumeRef volRef);


// ********************************************************************************************
// Logical Volume level manipulations
// ********************************************************************************************


/*!
	@function AppleLVMResizeVolume
	@discussion Used to resize a volume up or down in size.  May cause a logical volume to start
	spaning logical volume group members or stop spanning them if reducing the size. The size
	returned will be rounded up to the nearest allocation multiple.
	@param volumeProperties The dictionary for the logical volume, either from AppleLVMCreateVolume() or AppleLVMGetVolumeProperties(),
	@param size The new size for the volume. 
*/
CF_EXPORT
UInt64 AppleLVMResizeVolume(CFMutableDictionaryRef volumeProperties, UInt64 size);


/*!
	@function AppleLVMSnapShotVolume
	@discussion Returns a new logical volume that is a snapshot of the specified volume.
	Snapshots of snapshots are allowed. If the changes to the original volume are larger than
	than the snapshot can track then the snapshot will fail.  As will any snapshots of it.
	@param volumeProperties The dictionary for the logical volume from AppleLVMGetVolumeProperties(),
	@param type The handle to the original logical volume.
	@param snapshotSize The maximum size of the snapshot.
*/
CF_EXPORT
CFMutableDictionaryRef AppleLVMSnapShotVolume(CFMutableDictionaryRef volumeProperties, CFStringRef type, UInt64 snapshotSize);


/*!
	@function AppleLVMMigrateVolume
	@discussion Used to do a live move of a logical volume from one disk member to another disk member or
	from one location to another location on the same member.
	@param volRef The handle to the logical volume.
	@param toRef A member from the logical volume group (can be same as fromRef)
	@param volumeLocation Where on the disk, fast section, slow section, ..
*/
CF_EXPORT
bool AppleLVMMigrateVolume(AppleLVMVolumeRef volRef, AppleRAIDMemberRef toRef, CFStringRef volumeLocation);


// ********************************************************************************************
// Logical Group Member level manipulations
// ********************************************************************************************

/*!
	@function AppleLVMRemoveMember
	@discussion Used to remove a member disk from a logical volume group.  Works for members
	that do or do not have logical volumes on them.  If the member is not empty, a new logical
	volume group will be created if possible.  The logical volumes on the member must be closed (unmounted).
	@param volRef The handle to the logical volume.
	@param memberRef A member from the logical volume group.
*/
CF_EXPORT
AppleLVMVolumeRef AppleLVMRemoveMember(AppleLVMVolumeRef volRef, AppleRAIDMemberRef memberRef);


/*!
	@function AppleLVMMergeGroups
	@discussion Used to merge two logical volume groups together into one.  The logical volume
	is deleted the it's volumes are added to the other.  The logical volumes on the donor
	must be closed (unmounted).
	@param setRef A handle for the logical volume group being added to.
	@param donorSetRef The handle for the logical volume group being added.
*/
CF_EXPORT
AppleLVMVolumeRef AppleLVMMergeGroups(AppleRAIDSetRef setRef, AppleRAIDSetRef donorSetRef);


#endif !KERNEL

#endif _APPLERAIDUSERLIB_H
