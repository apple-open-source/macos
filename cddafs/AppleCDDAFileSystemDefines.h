/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
	kAppleCDDANodeBusyBit 			= 0,
	kAppleCDDANodeWantedBit			= 1,
	kAppleCDDANodeBusyMask 			= ( 1 << kAppleCDDANodeBusyBit ),
	kAppleCDDANodeWantedMask		= ( 1 << kAppleCDDANodeWantedBit )
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


//-----------------------------------------------------------------------------
//	FinderInfo flags and structures
//-----------------------------------------------------------------------------

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
											ATTR_VOL_CAPABILITIES | ATTR_VOL_ATTRIBUTES | ATTR_VOL_INFO,
	
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


//-----------------------------------------------------------------------------
//	SubQTOCInfo - 	Structure which describes the SubQTOCInfo defined in
//					MMC-2 NCITS T10/1228D SCSI MultiMedia Commands Version 2
//					  rev 9.F April 1, 1999, p. 215				
//					ATAPI SFF-8020 rev 1.2 Feb 24, 1994, p. 149
//-----------------------------------------------------------------------------


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


//-----------------------------------------------------------------------------
//	QTOCDataFormat10 - 	Structure which describes the QTOCDataFormat10 defined in
//					MMC-2 NCITS T10/1228D SCSI MultiMedia Commands Version 2
//					  rev 9.F April 1, 1999, p. 215				
//					ATAPI SFF-8020 rev 1.2 Feb 24, 1994, p. 149
//-----------------------------------------------------------------------------


struct QTOCDataFormat10
{
	UInt16			TOCDataLength;
	UInt8			firstSessionNumber;
	UInt8			lastSessionNumber;
	SubQTOCInfo		trackDescriptors[1];
};
typedef struct QTOCDataFormat10 QTOCDataFormat10;
typedef struct QTOCDataFormat10 * QTOCDataFormat10Ptr;


//-----------------------------------------------------------------------------
//	AppleCDDAArguments - 	These are the arguments passed to the filesystem
//							at mount time
//-----------------------------------------------------------------------------

struct AppleCDDAArguments
{
#ifndef KERNEL
	char *			device;
#endif
    UInt32			numTracks;		// Number of audio tracks
	user_addr_t		nameData;		// Buffer for track names and album name
	UInt32			nameDataSize;	// Size of buffer
	user_addr_t		xmlData;		// Pointer to XML data
	UInt32			xmlFileSize;	// Size of XML plist-style buffer/file
	UInt32			fileType;		// Type in FOUR_CHAR_CODE
	UInt32			fileCreator;	// Creator in FOUR_CHAR_CODE
} __attribute__((packed));
typedef struct AppleCDDAArguments AppleCDDAArguments;



#define kMaxNameSize			257
#define kMaxTrackCount			99
#define kMaxNameDataSize		((kMaxNameSize * kMaxTrackCount) + PAGE_SIZE)

#define kMaxXMLDataSize			PAGE_SIZE * 10	// 40K = Arbitrary size. Definitely shouldn't be bigger than this...


#if KERNEL

//-----------------------------------------------------------------------------
//	AppleCDDANodeInfo - 	Structure used to store node information
//-----------------------------------------------------------------------------


struct AppleCDDANodeInfo
{
	vnode_t					vNodePtr;			// Ptr to vnode
	UInt8					nameSize;			// Size of the name
	char *					name;				// The name
	SubQTOCInfo				trackDescriptor;	// TOC info for a node
	UInt32					LBA;				// Logical Block Address on disc
	UInt32					numBytes;			// File size in number of bytes
	UInt32					flags;				// Flags
};
typedef struct AppleCDDANodeInfo AppleCDDANodeInfo;
typedef struct AppleCDDANodeInfo * AppleCDDANodeInfoPtr;


//-----------------------------------------------------------------------------
//	AppleCDDAMount - 	Private Mount data
//-----------------------------------------------------------------------------


struct AppleCDDAMount
{
	vnode_t					root;					// Root VNode
	UInt32					rootVID;				// Root VNode ID
	vnode_t					xmlFileVNodePtr;		// XMLFile VNode
	UInt32					xmlFileFlags;			// Flags
	UInt32					nameDataSize;			// Size of buffer
	UInt8 *					nameData;				// Buffer for track names and album name
	AppleCDDANodeInfoPtr	nodeInfoArrayPtr;		// Ptr to NodeInfo array
	lck_grp_t *				cddaMountLockGroup;		// Lock group
	lck_grp_attr_t *		cddaMountLockGroupAttr;	// Lock group attributes
	lck_mtx_t *				cddaMountLock;			// Locks access to AppleCDDAMount structures and NodeInfo array
	lck_attr_t *			cddaMountLockAttr;		// Lock attributes
	UInt8					numTracks;				// Number of audio tracks
	struct timespec			mountTime;				// The time we were mounted
	UInt32 					xmlDataSize;			// XML data size
	UInt8 * 				xmlData;				// XML data ptr
	UInt32					fileType;				// Type in FOUR_CHAR_CODE
	UInt32					fileCreator;			// Creator in FOUR_CHAR_CODE
};
typedef struct AppleCDDAMount AppleCDDAMount;
typedef struct AppleCDDAMount * AppleCDDAMountPtr;


//-----------------------------------------------------------------------------
//	AppleCDDADirectoryNode - Structure which describes everything about
//							 one of our directory nodes
//-----------------------------------------------------------------------------


struct AppleCDDADirectoryNode
{
	UInt32 				entryCount;		// Number of directory entries
	UInt64				directorySize;	// Size of the directory
};
typedef struct AppleCDDADirectoryNode AppleCDDADirectoryNode;
typedef struct AppleCDDADirectoryNode * AppleCDDADirectoryNodePtr;


//-----------------------------------------------------------------------------
//	AppleCDDAFileNode - 	Structure which describes everything about one of
//							our track nodes
//-----------------------------------------------------------------------------


struct AppleCDDAFileNode
{
	CDAIFFHeader			aiffHeader;		// AIFF File Header
	AppleCDDANodeInfoPtr	nodeInfoPtr;	// Ptr to AppleCDDANodeInfo about this track
};
typedef struct AppleCDDAFileNode AppleCDDAFileNode;
typedef struct AppleCDDAFileNode * AppleCDDAFileNodePtr;


//-----------------------------------------------------------------------------
//	AppleCDDAXMLFileNode - 	Used to handle the ".TOC.plist" file
//-----------------------------------------------------------------------------


struct AppleCDDAXMLFileNode
{
	UInt32		fileSize;		// size of the XML file
	UInt8 *		fileDataPtr;	// Ptr to file data
};
typedef struct AppleCDDAXMLFileNode AppleCDDAXMLFileNode;
typedef struct AppleCDDAXMLFileNode * AppleCDDAXMLFileNodePtr;


//-----------------------------------------------------------------------------
//	AppleCDDANode - 	Structure which describes everything about one of
//						our nodes
//-----------------------------------------------------------------------------


struct AppleCDDANode
{
	enum cddaNodeType				nodeType;				// Node type: directory or track
	vnode_t							vNodePtr;				// Pointer to vnode this node is "hung-off"
	vnode_t							blockDeviceVNodePtr;	// block device vnode pointer
	UInt32							nodeID;					// Node ID
	union
	{
		AppleCDDADirectoryNode	directory;
		AppleCDDAFileNode		file;
		AppleCDDAXMLFileNode	xmlFile;
	} u;
};
typedef struct AppleCDDANode AppleCDDANode;
typedef struct AppleCDDANode * AppleCDDANodePtr;


//-----------------------------------------------------------------------------
//	Conversion Macros
//-----------------------------------------------------------------------------

#define VFSTOCDDA(mp)				((AppleCDDAMountPtr)(vfs_fsprivate(mp)))
#define	VTOCDDA(vp) 				((AppleCDDANodePtr)(vnode_fsnode(vp)))
#define CDDATOV(cddaNodePtr) 		((cddaNodePtr)->vNodePtr)
#define CDDATONODEINFO(cddaNodePtr)	((cddaNodePtr)->u.file.nodeInfoPtr)
#define VFSTONODEINFO(mp)			((AppleCDDANodeInfoPtr)((AppleCDDAMountPtr)(vfs_fsprivate(mp)))->nodeInfoArrayPtr)
#define VFSTONAMEINFO(mp)			((UInt8 *)((AppleCDDAMountPtr)(vfs_fsprivate(mp)))->nameData)
#define VFSTOXMLDATA(mp)			((UInt8 *)((AppleCDDAMountPtr)(vfs_fsprivate(mp)))->xmlData)

#endif	/* KERNEL */

#ifdef __cplusplus
}
#endif


#endif // __APPLE_CDDA_FS_DEFINES_H__