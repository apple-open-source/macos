/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

// AppleCDDAFileSystemDefines.h created by CJS on Mon 10-Apr-2000

#ifndef __APPLE_CDDA_FS_DEFINES_H__
#define __APPLE_CDDA_FS_DEFINES_H__

#ifndef __AIFF_SUPPORT_H__
#include "AIFFSupport.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/attr.h>
#include <libkern/OSTypes.h>

#if KERNEL
#include <sys/systm.h>
#endif


// Enums for Control field in SubChannelQ
enum
{
	kPreEmphasisBit		= 0,
	kDigitalDataBit		= 2
};

enum
{
	kPreEmphasisMask	= ( 1 << kPreEmphasisBit ),
	kDigitalDataMask 	= ( 1 << kDigitalDataBit )
};


// Time constants
enum
{
	kSecondsPerMinute	= 60,
	kFramesPerSecond	= 75,
	kMSFToLBA			= (kFramesPerSecond << 1)		// 2 seconds
};


// Node types
enum cddaNodeType
{
	kAppleCDDADirectoryType 	= 1,
	kAppleCDDATrackType			= 2,
	kAppleCDDAXMLFileType 		= 3
};


// File references
enum
{
	kAppleCDDANumberOfFileReferences 	= 1,
	kAppleCDDANumberOfRootDirReferences	= 2
};


// Flags
enum
{
	kAppleCDDAAccessedBit 			= 0
};


// Masks for flags parameter in AppleCDDANode structure
enum
{
	kAppleCDDAAccessedMask 			= ( 1 << kAppleCDDAAccessedBit )
};


// ".TOC.plist" file stuff
enum
{
	kAppleCDDAXMLFileID		= 3
};

enum
{
	kUnknownUserID	= 99,
	kUnknownGroupID	= 99
};

enum
{
	kCDDAMaxFileNameBytes = 3 * 255
};


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	FinderInfo flags and structures
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

enum
{
	kFinderInfoOnDesktopBit			= 0,
	kFinderInfoOwnApplBit			= 1,
	kFinderInfoNoFileExtensionBit	= 4,
	kFinderInfoSharedBit			= 6,
	kFinderInfoCachedBit			= 7,
	kFinderInfoInitedBit			= 8,
	kFinderInfoChangedBit			= 9,
	kFinderInfoBusyBit				= 10,
	kFinderInfoNoCopyBit			= 11,
	kFinderInfoSystemBit			= 12,
	kFinderInfoHasBundleBit			= 13,
	kFinderInfoInvisibleBit			= 14,
	kFinderInfoLockedBit			= 15
};

enum
{
	kFinderInfoOnDesktopMask		= (1 << kFinderInfoOnDesktopBit),
	kFinderInfoOwnApplMask			= (1 << kFinderInfoOwnApplBit),
	kFinderInfoNoFileExtensionMask	= (1 << kFinderInfoNoFileExtensionBit),
	kFinderInfoSharedMask			= (1 << kFinderInfoSharedBit),
	kFinderInfoCachedMask			= (1 << kFinderInfoCachedBit),
	kFinderInfoInitedMask			= (1 << kFinderInfoInitedBit),
	kFinderInfoChangedMask			= (1 << kFinderInfoChangedBit),
	kFinderInfoBusyMask				= (1 << kFinderInfoBusyBit),
	kFinderInfoNoCopyMask			= (1 << kFinderInfoNoCopyBit),
	kFinderInfoSystemMask			= (1 << kFinderInfoSystemBit),
	kFinderInfoHasBundleMask		= (1 << kFinderInfoHasBundleBit),
	kFinderInfoInvisibleMask		= (1 << kFinderInfoInvisibleBit),
	kFinderInfoLockedMask			= (1 << kFinderInfoLockedBit)
};

#define	kExtendedFinderInfoSize		16

struct FinderInfo
{
	UInt32		fileType;
	UInt32		fileCreator;
	UInt16		finderFlags;
	struct
	{
	    SInt16	v;		/* file's location */
	    SInt16	h;
	} location;
	UInt16		reserved;
};
typedef struct FinderInfo FinderInfo;


// For pre-Panther systems
#ifndef VOL_CAP_FMT_JOURNAL
#define VOL_CAP_FMT_JOURNAL			0x00000008
#define VOL_CAP_FMT_JOURNAL_ACTIVE	0x00000010
#define VOL_CAP_FMT_NO_ROOT_TIMES	0x00000020
#define VOL_CAP_FMT_SPARSE_FILES	0x00000040
#define VOL_CAP_FMT_ZERO_RUNS		0x00000080
#define VOL_CAP_FMT_CASE_SENSITIVE	0x00000100
#define VOL_CAP_FMT_CASE_PRESERVING 0x00000200
#define VOL_CAP_FMT_FAST_STATFS		0x00000400
#endif

#ifndef VOL_CAP_INT_EXCHANGEDATA
#define VOL_CAP_INT_EXCHANGEDATA	0x00000010
#define VOL_CAP_INT_COPYFILE		0x00000020
#define VOL_CAP_INT_ALLOCATE		0x00000040
#define VOL_CAP_INT_VOL_RENAME		0x00000080
#define VOL_CAP_INT_ADVLOCK			0x00000100
#define VOL_CAP_INT_FLOCK			0x00000200
#endif

#ifndef	ATTR_VOL_VCBFSID
#define ATTR_VOL_VCBFSID			0x00040000
#endif


enum
{
	
	kAppleCDDACommonAttributesValidMask = 	ATTR_CMN_NAME | ATTR_CMN_DEVID | ATTR_CMN_FSID |
											ATTR_CMN_OBJTYPE | ATTR_CMN_OBJTAG | ATTR_CMN_OBJID	|
											ATTR_CMN_OBJPERMANENTID | ATTR_CMN_PAROBJID | ATTR_CMN_SCRIPT |
											ATTR_CMN_CRTIME | ATTR_CMN_MODTIME | ATTR_CMN_CHGTIME |
											ATTR_CMN_ACCTIME | ATTR_CMN_BKUPTIME | ATTR_CMN_FNDRINFO |
											ATTR_CMN_OWNERID | ATTR_CMN_GRPID | ATTR_CMN_ACCESSMASK |
											ATTR_CMN_FLAGS | ATTR_CMN_USERACCESS,
	
	kAppleCDDAVolumeAttributesValidMask	=	ATTR_VOL_FSTYPE | ATTR_VOL_SIGNATURE | ATTR_VOL_SIZE |
											ATTR_VOL_SPACEFREE | ATTR_VOL_SPACEAVAIL |
											ATTR_VOL_MINALLOCATION | ATTR_VOL_ALLOCATIONCLUMP |
											ATTR_VOL_IOBLOCKSIZE | ATTR_VOL_OBJCOUNT | ATTR_VOL_FILECOUNT |
											ATTR_VOL_DIRCOUNT | ATTR_VOL_MAXOBJCOUNT |
											ATTR_VOL_MOUNTPOINT | ATTR_VOL_NAME | ATTR_VOL_MOUNTFLAGS |
											ATTR_VOL_MOUNTEDDEVICE | ATTR_VOL_ENCODINGSUSED |
											ATTR_VOL_CAPABILITIES | ATTR_VOL_VCBFSID |
											ATTR_VOL_ATTRIBUTES | ATTR_VOL_INFO,
	
	kAppleCDDADirectoryAttributesValidMask = ATTR_DIR_LINKCOUNT | ATTR_DIR_ENTRYCOUNT | ATTR_DIR_MOUNTSTATUS,
	
	kAppleCDDAFileAttributesValidMask	=	ATTR_FILE_LINKCOUNT | ATTR_FILE_TOTALSIZE | ATTR_FILE_ALLOCSIZE |
											ATTR_FILE_IOBLOCKSIZE | ATTR_FILE_CLUMPSIZE | ATTR_FILE_DEVTYPE |
											ATTR_FILE_DATALENGTH | ATTR_FILE_DATAALLOCSIZE |
											ATTR_FILE_RSRCLENGTH | ATTR_FILE_RSRCALLOCSIZE,
	
	kAppleCDDAForkAttributesValidMask	=	0x00000000
	
};

enum
{
	kAppleCDDAFileSystemVolumeSignature = 0x4244,	// 'BD'
	kAppleCDDAFileSystemVCBFSID			= 0x4A48	// 'JH'
};


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SubQTOCInfo - 	Structure which describes the SubQTOCInfo defined in
//					¥ MMC-2 NCITS T10/1228D SCSI MultiMedia Commands Version 2
//					  rev 9.F April 1, 1999, p. 215				
//					¥ ATAPI SFF-8020 rev 1.2 Feb 24, 1994, p. 149
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct SubQTOCInfo
{
	
	UInt8		sessionNumber;
#if defined(__BIG_ENDIAN__)
	UInt8		address:4;
	UInt8		control:4;
#elif defined(__LITTLE_ENDIAN__)
	UInt8		control:4;
	UInt8		address:4;
#else
#error Unknown byte order
#endif /* __LITTLE_ENDIAN__ */
	UInt8		tno;
	UInt8		point;
	UInt8		ATIP[3];
	UInt8		zero;
	union {
		
		struct {
			UInt8		minutes;
			UInt8		seconds;
			UInt8		frames;
		} startPosition;
		
		struct {
			UInt8		firstTrackNum;
			UInt8		discType;
			UInt8		reserved;
		} A0PMSF;
		
		struct {
			UInt8		lastTrackNum;
			UInt8		reserved[2];
		} A1PMSF;
		
		struct {
			UInt8		minutes;
			UInt8		seconds;
			UInt8		frames;
		} leadOutStartPosition;
		
	} PMSF;
	
};
typedef struct SubQTOCInfo SubQTOCInfo;
typedef struct SubQTOCInfo * SubQTOCInfoPtr;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	QTOCDataFormat10 - 	Structure which describes the QTOCDataFormat10 defined in
//					¥ MMC-2 NCITS T10/1228D SCSI MultiMedia Commands Version 2
//					  rev 9.F April 1, 1999, p. 215				
//					¥ ATAPI SFF-8020 rev 1.2 Feb 24, 1994, p. 149
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct QTOCDataFormat10
{
	UInt16			TOCDataLength;
	UInt8			firstSessionNumber;
	UInt8			lastSessionNumber;
	SubQTOCInfo		trackDescriptors[1];
};
typedef struct QTOCDataFormat10 QTOCDataFormat10;
typedef struct QTOCDataFormat10 * QTOCDataFormat10Ptr;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	AppleCDDAArguments - 	These are the arguments passed to the filesystem
//							at mount time
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct AppleCDDAArguments
{
    char *		device;			// Name of special device mounted from (/dev/disk#)
    UInt8		numTracks;		// Number of audio tracks
	UInt32		nameDataSize;	// Size of buffer
	char *		nameData;		// Buffer for track names and album name
	UInt32		xmlFileSize;	// Size of XML plist-style buffer/file
	UInt8 *		xmlData;		// Pointer to XML data
	UInt32		fileType;		// Type in FOUR_CHAR_CODE
	UInt32		fileCreator;	// Creator in FOUR_CHAR_CODE
};
typedef struct AppleCDDAArguments AppleCDDAArguments;
typedef struct AppleCDDAArguments * AppleCDDAArgumentsPtr;


#if KERNEL


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	AppleCDDANodeInfo - 	Structure used to store node information
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct AppleCDDANodeInfo
{
	struct vnode *			vNodePtr;			// Ptr to vnode
	UInt8					nameSize;			// size of the name
	char *					name;				// the name
	SubQTOCInfo				trackDescriptor;	// TOC info for a node
	UInt32					LBA;				// Logical Block Address on disc
	UInt32					numBytes;			// file size in number of bytes
};
typedef struct AppleCDDANodeInfo AppleCDDANodeInfo;
typedef struct AppleCDDANodeInfo * AppleCDDANodeInfoPtr;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	AppleCDDAMount - 	Private Mount data
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct AppleCDDAMount
{
	struct vnode *			root;				// Root VNode
	struct vnode *			xmlFileVNodePtr;	// XMLFile VNode
	UInt32					nameDataSize;		// Size of buffer
	UInt8 *					nameData;			// Buffer for track names and album name
	AppleCDDANodeInfoPtr	nodeInfoArrayPtr;	// Ptr to NodeInfo array
	struct lock__bsd__		nodeInfoLock;		// nodeInfo lock
	UInt8					numTracks;			// Number of audio tracks
	struct timespec			mountTime;			// The time we were mounted
	UInt32					fileType;			// Type in FOUR_CHAR_CODE
	UInt32					fileCreator;		// Creator in FOUR_CHAR_CODE
};
typedef struct AppleCDDAMount AppleCDDAMount;
typedef struct AppleCDDAMount * AppleCDDAMountPtr;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	AppleCDDADirectoryNode - Structure which describes everything about
//							 one of our directory nodes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct AppleCDDADirectoryNode
{
	UInt32 				entryCount;		// Number of directory entries
	UInt64				directorySize;	// Size of the directory
	char *				name;			// Name of the directory
};
typedef struct AppleCDDADirectoryNode AppleCDDADirectoryNode;
typedef struct AppleCDDADirectoryNode * AppleCDDADirectoryNodePtr;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	AppleCDDAFileNode - 	Structure which describes everything about one of
//							our track nodes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct AppleCDDAFileNode
{
	CDAIFFHeader			aiffHeader;		// AIFF File Header
	AppleCDDANodeInfoPtr	nodeInfoPtr;	// Ptr to AppleCDDANodeInfo about this track
};
typedef struct AppleCDDAFileNode AppleCDDAFileNode;
typedef struct AppleCDDAFileNode * AppleCDDAFileNodePtr;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	AppleCDDAXMLFileNode - 	Used to handle the ".TOC.plist" file
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct AppleCDDAXMLFileNode
{
	UInt32		fileSize;		// size of the XML file
	UInt8 *		fileDataPtr;	// Ptr to file data
};
typedef struct AppleCDDAXMLFileNode AppleCDDAXMLFileNode;
typedef struct AppleCDDAXMLFileNode * AppleCDDAXMLFileNodePtr;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	AppleCDDANode - 	Structure which describes everything about one of
//						our nodes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct AppleCDDANode
{
	enum cddaNodeType				nodeType;				// Node type: directory or track
	UInt32							flags;					// Flags
	struct vnode *					vNodePtr;				// Pointer to vnode this node is "hung-off"
	struct vnode *					blockDeviceVNodePtr;	// block device vnode pointer
	struct lock__bsd__				lock;					// node lock
	UInt32							nodeID;					// Node ID
	struct timespec					accessTime;				// Time last accessed
	struct timespec					lastModTime;			// Time last modified
	union
	{
		AppleCDDADirectoryNode	directory;
		AppleCDDAFileNode		file;
		AppleCDDAXMLFileNode	xmlFile;
	} u;
};
typedef struct AppleCDDANode AppleCDDANode;
typedef struct AppleCDDANode * AppleCDDANodePtr;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Conversion Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define VFSTOCDDA(mp)				((AppleCDDAMountPtr)((mp)->mnt_data))
#define	VTOCDDA(vp) 				((AppleCDDANodePtr)(vp)->v_data)
#define CDDATOV(cddaNodePtr) 		((cddaNodePtr)->vNodePtr)
#define CDDATONODEINFO(cddaNodePtr)	((cddaNodePtr)->nodeInfoPtr)
#define VFSTONODEINFO(mp)			((AppleCDDANodeInfoPtr)((AppleCDDAMountPtr)((mp)->mnt_data))->nodeInfoArrayPtr)
#define VFSTONAMEINFO(mp)			((UInt8 *)((AppleCDDAMountPtr)((mp)->mnt_data))->nameData)

#endif	/* KERNEL */

#ifdef __cplusplus
}
#endif

#endif // __APPLE_CDDA_FS_DEFINES_H__
