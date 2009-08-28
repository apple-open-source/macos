/*
 * Copyright (c) 1999-2009 Apple Inc. All rights reserved.
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
/*
	File:		makehfs.c

	Contains:	Initialization code for HFS and HFS Plus volumes.

	Copyright:	© 1984-1999 by Apple Computer, Inc., all rights reserved.

*/

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wipefs.h>

/*
 * CommonCrypto is meant to be a more stable API than OpenSSL.
 * Defining COMMON_DIGEST_FOR_OPENSSL gives API-compatibility
 * with OpenSSL, so we don't have to change the code.
 */
#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>

#include <libkern/OSByteOrder.h>

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFStringEncodingExt.h>
#include <DiskArbitration/DiskArbitration.h>

#include <TargetConditionals.h>

extern Boolean _CFStringGetFileSystemRepresentation(CFStringRef string, UInt8 *buffer, CFIndex maxBufLen);


#include <hfs/hfs_format.h>
#include <hfs/hfs_mount.h>
#include "hfs_endian.h"

#include "newfs_hfs.h"
#include "readme.h"


#define HFS_BOOT_DATA	"/usr/share/misc/hfsbootdata"

#define HFS_JOURNAL_FILE	".journal"
#define HFS_JOURNAL_INFO	".journal_info_block"

#define kJournalFileType	0x6a726e6c	/* 'jrnl' */


typedef HFSMasterDirectoryBlock HFS_MDB;

struct filefork {
	UInt16	startBlock;
	UInt16	blockCount;
	UInt32	logicalSize;
	UInt32	physicalSize;
};

struct filefork	gDTDBFork, gSystemFork, gReadMeFork;


static void WriteMDB __P((const DriveInfo *driveInfo, HFS_MDB *mdbp));
static void InitMDB __P((hfsparams_t *defaults, UInt32 driveBlocks, HFS_MDB *mdbp));

static void WriteVH __P((const DriveInfo *driveInfo, HFSPlusVolumeHeader *hp));
static void InitVH __P((hfsparams_t *defaults, UInt64 sectors,
		HFSPlusVolumeHeader *header));

static void AllocateExtent(UInt8 *buffer, UInt32 startBlock, UInt32 blockCount);
static void WriteBitmap __P((const DriveInfo *dip, UInt64 startingSector,
        UInt32 alBlksUsed, UInt8 *buffer));

static void WriteExtentsFile __P((const DriveInfo *dip, UInt64 startingSector,
        const hfsparams_t *dp, HFSExtentDescriptor *bbextp, void *buffer,
        UInt32 *bytesUsed, UInt32 *mapNodes));
static void InitExtentsRoot __P((UInt16 btNodeSize, HFSExtentDescriptor *bbextp,
		void *buffer));

static void WriteAttributesFile(const DriveInfo *driveInfo, UInt64 startingSector,
        const hfsparams_t *dp, HFSExtentDescriptor *bbextp, void *buffer,
        UInt32 *bytesUsed, UInt32 *mapNodes);

static void WriteCatalogFile __P((const DriveInfo *dip, UInt64 startingSector,
        const hfsparams_t *dp, HFSPlusVolumeHeader *header, void *buffer,
        UInt32 *bytesUsed, UInt32 *mapNodes));
static int  WriteJournalInfo(const DriveInfo *driveInfo, UInt64 startingSector,
			     const hfsparams_t *dp, HFSPlusVolumeHeader *header,
			     void *buffer);
static void InitCatalogRoot_HFSPlus __P((const hfsparams_t *dp, const HFSPlusVolumeHeader *header, void * buffer));
static void InitCatalogRoot_HFS __P((const hfsparams_t *dp, void * buffer));
static void InitFirstCatalogLeaf __P((const hfsparams_t *dp, void * buffer,
		int wrapper));
static void InitSecondCatalogLeaf __P((const hfsparams_t *dp, void *buffer));

static void WriteDesktopDB(const hfsparams_t *dp, const DriveInfo *driveInfo,
        UInt64 startingSector, void *buffer, UInt32 *mapNodes);

static void WriteSystemFile __P((const DriveInfo *dip, UInt64 startingSector,
		UInt32 *filesize));
static void WriteReadMeFile __P((const DriveInfo *dip, UInt64 startingSector,
		UInt32 *filesize));
static void WriteMapNodes __P((const DriveInfo *driveInfo, UInt64 diskStart,
		UInt32 firstMapNode, UInt32 mapNodes, UInt16 btNodeSize, void *buffer));
static void WriteBuffer __P((const DriveInfo *driveInfo, UInt64 startingSector,
		UInt64 byteCount, const void *buffer));
static UInt32 Largest __P((UInt32 a, UInt32 b, UInt32 c, UInt32 d ));

static void MarkBitInAllocationBuffer __P((HFSPlusVolumeHeader *header,
		UInt32 allocationBlock, void* sectorBuffer, UInt64 *sector));

static UInt32 GetDefaultEncoding();

static UInt32 UTCToLocal __P((UInt32 utcTime));

static UInt32 DivideAndRoundUp __P((UInt32 numerator, UInt32 denominator));

static int ConvertUTF8toUnicode __P((const UInt8* source, size_t bufsize,
		UniChar* unibuf, UInt16 *charcount));

static int getencodinghint(unsigned char *name);

#define VOLUMEUUIDVALUESIZE 2
typedef union VolumeUUID {
	UInt32 value[VOLUMEUUIDVALUESIZE];
	struct {
		UInt32 high;
		UInt32 low;
	} v;
} VolumeUUID;
void GenerateVolumeUUID(VolumeUUID *newVolumeID);

void SETOFFSET (void *buffer, UInt16 btNodeSize, SInt16 recOffset, SInt16 vecOffset);
#define SETOFFSET(buf,ndsiz,offset,rec)		\
	(*(SInt16 *)((UInt8 *)(buf) + (ndsiz) + (-2 * (rec))) = (SWAP_BE16 (offset)))

#define BYTESTOBLKS(bytes,blks)		DivideAndRoundUp((bytes),(blks))

#define ROUNDUP(x, u)	(((x) % (u) == 0) ? (x) : ((x)/(u) + 1) * (u))

#if TARGET_OS_EMBEDDED
#define ENCODING_TO_BIT(e)				 \
	  ((e) < 48 ? (e) : 0)
#else
#define ENCODING_TO_BIT(e)                               \
          ((e) < 48 ? (e) :                              \
          ((e) == kCFStringEncodingMacUkrainian ? 48 :   \
          ((e) == kCFStringEncodingMacFarsi ? 49 : 0)))
#endif

/*
 * wipefs() in -lutil knows about multiple filesystem formats.
 * This replaces the code:
 *	WriteBuffer(driveInfo, 0, diskBlocksUsed * kBytesPerSector, NULL);
 *	WriteBuffer(driveInfo, driveInfo->totalSectors - 8, 4 * 1024, NULL);
 * which was used to erase the beginning and end of the filesystem.
 *
 */
static int
dowipefs(int fd)
{
	int err;
	wipefs_ctx handle;

	err = wipefs_alloc(fd, 0/*sectorSize*/, &handle);
	if (err == 0) {
		err = wipefs_wipe(handle);
	}
	wipefs_free(&handle);
	return err;
}

/*
 * make_hfs
 *	
 * This routine writes an initial HFS volume structure onto a volume.
 * It is assumed that the disk has already been formatted and verified.
 * 
 * For information on the HFS volume format see "Data Organization on Volumes"
 * in "Inside Macintosh: Files" (p. 2-52).
 * 
 */
int
make_hfs(const DriveInfo *driveInfo,
	  hfsparams_t *defaults,
	  uint32_t *plusSectors,
	  uint32_t *plusOffset)
{
	UInt32 sector;
	UInt32 diskBlocksUsed;
	UInt32 mapNodes;
	UInt32 sectorsPerBlock;
	void *nodeBuffer = NULL;
	HFS_MDB	*mdbp = NULL;
	UInt32 bytesUsed;
	
	*plusSectors = 0;
	*plusOffset = 0;

	/* assume sectorSize <= blockSize */
	sectorsPerBlock = defaults->blockSize / driveInfo->sectorSize;
	

	/*--- CREATE A MASTER DIRECTORY BLOCK:  */

	mdbp = (HFS_MDB*)malloc((size_t)kBytesPerSector);
	nodeBuffer = malloc(8192);  /* max bitmap bytes is 8192 bytes */
	if (nodeBuffer == NULL || mdbp == NULL) 
		err(1, NULL);

	defaults->encodingHint = getencodinghint(defaults->volumeName);

	/* MDB Initialized in native byte order */
	InitMDB(defaults, (UInt32)driveInfo->totalSectors, mdbp);

	
	/*--- ZERO OUT BEGINNING OF DISK (bitmap and b-trees):  */
	
	diskBlocksUsed = (mdbp->drAlBlSt + 1) +
		(mdbp->drXTFlSize + mdbp->drCTFlSize) / kBytesPerSector;
	if (defaults->flags & kMakeHFSWrapper) {
		diskBlocksUsed += MAX(kDTDB_Size, mdbp->drAlBlkSiz) / kBytesPerSector;
		diskBlocksUsed += MAX(sizeof(hfswrap_readme), mdbp->drAlBlkSiz) / kBytesPerSector;
		diskBlocksUsed += MAX(24 * 1024, mdbp->drAlBlkSiz) / kBytesPerSector;
	}
	(void)dowipefs(driveInfo->fd);

	/* If this is a wrapper, add boot files... */
	if (defaults->flags & kMakeHFSWrapper) {

		sector = mdbp->drAlBlSt + 
			 mdbp->drXTFlSize/kBytesPerSector +
			 mdbp->drCTFlSize/kBytesPerSector;

		WriteDesktopDB(defaults, driveInfo, sector, nodeBuffer, &mapNodes);

		if (mapNodes > 0)
			WriteMapNodes(driveInfo, (sector + 1), 1, mapNodes, kHFSNodeSize, nodeBuffer);

		gDTDBFork.logicalSize = MAX(kDTDB_Size, mdbp->drAlBlkSiz);
		gDTDBFork.startBlock = (sector - mdbp->drAlBlSt) / sectorsPerBlock;
		gDTDBFork.blockCount = BYTESTOBLKS(gDTDBFork.logicalSize, mdbp->drAlBlkSiz);
		gDTDBFork.physicalSize = gDTDBFork.blockCount * mdbp->drAlBlkSiz;

		sector += gDTDBFork.physicalSize / kBytesPerSector;

		WriteReadMeFile(driveInfo, sector, &gReadMeFork.logicalSize);
		gReadMeFork.startBlock = gDTDBFork.startBlock + gDTDBFork.blockCount;
		gReadMeFork.blockCount = BYTESTOBLKS(gReadMeFork.logicalSize, mdbp->drAlBlkSiz);
		gReadMeFork.physicalSize = gReadMeFork.blockCount * mdbp->drAlBlkSiz;

		sector += gReadMeFork.physicalSize / kBytesPerSector;

		WriteSystemFile(driveInfo, sector, &gSystemFork.logicalSize);
		gSystemFork.startBlock = gReadMeFork.startBlock + gReadMeFork.blockCount;
		gSystemFork.blockCount = BYTESTOBLKS(gSystemFork.logicalSize, mdbp->drAlBlkSiz);
		gSystemFork.physicalSize = gSystemFork.blockCount * mdbp->drAlBlkSiz;

		mdbp->drFreeBks -= gDTDBFork.blockCount + gReadMeFork.blockCount + gSystemFork.blockCount;
		mdbp->drEmbedExtent.startBlock = mdbp->drNmAlBlks - (UInt16)mdbp->drFreeBks;
		mdbp->drEmbedExtent.blockCount = (UInt16)mdbp->drFreeBks;
		mdbp->drFreeBks = 0;
	}


	/*--- WRITE ALLOCATION BITMAP TO DISK:  */

	WriteBitmap(driveInfo, mdbp->drVBMSt, mdbp->drNmAlBlks - (UInt16)mdbp->drFreeBks, nodeBuffer);


	/*--- WRITE FILE EXTENTS B*-TREE TO DISK:  */

	sector = mdbp->drAlBlSt;	/* reset */
	WriteExtentsFile(driveInfo, sector, defaults, &mdbp->drEmbedExtent, nodeBuffer, &bytesUsed, &mapNodes);

	if (mapNodes > 0)
		WriteMapNodes(driveInfo, (sector + bytesUsed/kBytesPerSector),
			bytesUsed/kHFSNodeSize, mapNodes, kHFSNodeSize, nodeBuffer);

	sector += (mdbp->drXTFlSize/kBytesPerSector);


	/*--- WRITE CATALOG B*-TREE TO DISK:  */

	WriteCatalogFile(driveInfo, sector, defaults, NULL, nodeBuffer, &bytesUsed, &mapNodes);

	if (mapNodes > 0)
		WriteMapNodes(driveInfo, (sector + bytesUsed/kBytesPerSector),
			bytesUsed/kHFSNodeSize, mapNodes, kHFSNodeSize, nodeBuffer);


	/*--- WRITE MASTER DIRECTORY BLOCK TO DISK:  */
	
	*plusSectors = mdbp->drEmbedExtent.blockCount *
		(mdbp->drAlBlkSiz / driveInfo->sectorSize);
	*plusOffset = mdbp->drAlBlSt + mdbp->drEmbedExtent.startBlock *
		(mdbp->drAlBlkSiz / driveInfo->sectorSize);

	/* write mdb last in case we fail along the way */

	/* Writes both copies of the MDB */
	WriteMDB (driveInfo, mdbp);
	/* MDB is now big-endian */

	free(nodeBuffer);		
	free(mdbp);	

	return (0);
}


/*
 * make_hfs
 *	
 * This routine writes an initial HFS volume structure onto a volume.
 * It is assumed that the disk has already been formatted and verified.
 *
 */
int
make_hfsplus(const DriveInfo *driveInfo, hfsparams_t *defaults)
{
	UInt16			btNodeSize;
	UInt32			sectorsPerBlock;
	UInt32			mapNodes;
	UInt32			sectorsPerNode;
	UInt32			temp;
	UInt32 			bytesUsed;
	UInt32			endOfAttributes;
	void			*nodeBuffer = NULL;
	HFSPlusVolumeHeader	*header = NULL;
	UInt64			sector;
	UInt64			bytesToZero;

	/* --- Create an HFS Plus header:  */

	header = (HFSPlusVolumeHeader*)malloc((size_t)kBytesPerSector);
	if (header == NULL)
		err(1, NULL);

	defaults->encodingHint = getencodinghint(defaults->volumeName);

	/* VH Initialized in native byte order */
	InitVH(defaults, driveInfo->totalSectors, header);

	sectorsPerBlock = header->blockSize / kBytesPerSector;


	/*--- ZERO OUT BEGINNING OF DISK:  */
	/*
	 * Clear out the space to be occupied by the bitmap and B-Trees.
	 * The first chunk is the boot sectors, volume header, allocation bitmap,
	 * journal, Extents B-tree, and Attributes B-tree (if any).
	 * The second chunk is the Catalog B-tree.
	 */
	endOfAttributes = header->extentsFile.extents[0].startBlock +
	                  header->extentsFile.extents[0].blockCount +
	                  header->attributesFile.extents[0].blockCount;
	bytesToZero = (UInt64) endOfAttributes * header->blockSize;
	WriteBuffer(driveInfo, 0, bytesToZero, NULL);

	bytesToZero = (UInt64) header->catalogFile.extents[0].blockCount * header->blockSize;
	sector = header->catalogFile.extents[0].startBlock * sectorsPerBlock;
	WriteBuffer(driveInfo, sector, bytesToZero, NULL);
	
	/* use wipefs() API to clear metadata from device */
	(void) dowipefs(driveInfo->fd);

	/*--- Allocate a buffer for the rest of our IO:  */

	temp = Largest( defaults->catalogNodeSize * 2,
			defaults->extentsNodeSize,
			header->blockSize,
			(header->catalogFile.extents[0].startBlock + header->catalogFile.extents[0].blockCount + 7) / 8 );
	/* 
	 * If size is not a mutiple of 512, round up to nearest sector
	 */
	if ( (temp & 0x01FF) != 0 )
		temp = (temp + kBytesPerSector) & 0xFFFFFE00;
	
	nodeBuffer = valloc((size_t)temp);
	if (nodeBuffer == NULL)
		err(1, NULL);


		
	/*--- WRITE ALLOCATION BITMAP BITS TO DISK:  */

	sector = header->allocationFile.extents[0].startBlock * sectorsPerBlock;
	bzero(nodeBuffer, temp);
	AllocateExtent(nodeBuffer, 0, endOfAttributes);
	AllocateExtent(nodeBuffer,
	               header->catalogFile.extents[0].startBlock,
	               header->catalogFile.extents[0].blockCount);
	WriteBuffer(driveInfo, sector, temp, nodeBuffer);
	
	/*
	 * Write alternate Volume Header bitmap bit to allocations file at
	 * 2nd to last sector on HFS+ volume
	 */
	if (header->totalBlocks > kBitsPerSector)
		bzero(nodeBuffer, kBytesPerSector);
	MarkBitInAllocationBuffer( header, header->totalBlocks - 1, nodeBuffer, &sector );

	if ( header->blockSize == 512 ) {
		UInt64	sector2;
		MarkBitInAllocationBuffer( header, header->totalBlocks - 2,
			nodeBuffer, &sector2 );
		
		/* cover the case when altVH and last block are on different bitmap sectors. */
		if ( sector2 != sector ) {
			bzero(nodeBuffer, kBytesPerSector);
			MarkBitInAllocationBuffer(header, header->totalBlocks - 1,
				nodeBuffer, &sector);
			WriteBuffer(driveInfo, sector, kBytesPerSector, nodeBuffer);

			bzero(nodeBuffer, kBytesPerSector);
			MarkBitInAllocationBuffer(header, header->totalBlocks - 2,
				nodeBuffer, &sector);
		}
	}
	WriteBuffer(driveInfo, sector, kBytesPerSector, nodeBuffer);


	/*--- WRITE FILE EXTENTS B-TREE TO DISK:  */

	btNodeSize = defaults->extentsNodeSize;
	sectorsPerNode = btNodeSize/kBytesPerSector;

	sector = header->extentsFile.extents[0].startBlock * sectorsPerBlock;
	WriteExtentsFile(driveInfo, sector, defaults, NULL, nodeBuffer, &bytesUsed, &mapNodes);

	if (mapNodes > 0) {
		WriteMapNodes(driveInfo, (sector + bytesUsed/kBytesPerSector),
			bytesUsed/btNodeSize, mapNodes, btNodeSize, nodeBuffer);
	}

	

	/*--- WRITE FILE ATTRIBUTES B-TREE TO DISK:  */
	if (defaults->attributesClumpSize) {

		btNodeSize = defaults->attributesNodeSize;
		sectorsPerNode = btNodeSize/kBytesPerSector;
	
		sector = header->attributesFile.extents[0].startBlock * sectorsPerBlock;
		WriteAttributesFile(driveInfo, sector, defaults, NULL, nodeBuffer, &bytesUsed, &mapNodes);
		if (mapNodes > 0) {
			WriteMapNodes(driveInfo, (sector + bytesUsed/kBytesPerSector),
				bytesUsed/btNodeSize, mapNodes, btNodeSize, nodeBuffer);
		}
	}
	
	/*--- WRITE CATALOG B-TREE TO DISK:  */
	
	btNodeSize = defaults->catalogNodeSize;
	sectorsPerNode = btNodeSize/kBytesPerSector;

	sector = header->catalogFile.extents[0].startBlock * sectorsPerBlock;
	WriteCatalogFile(driveInfo, sector, defaults, header, nodeBuffer, &bytesUsed, &mapNodes);

	if (mapNodes > 0) {
		WriteMapNodes(driveInfo, (sector + bytesUsed/kBytesPerSector),
			bytesUsed/btNodeSize, mapNodes, btNodeSize, nodeBuffer);
	}

	/*--- JOURNALING SETUP */
	if (defaults->journaledHFS) {
	    sector = header->journalInfoBlock * sectorsPerBlock;
	    if (WriteJournalInfo(driveInfo, sector, defaults, header, nodeBuffer) != 0) {
		err(EINVAL, "Failed to create the journal");
	    }
	}
	
	/*--- WRITE VOLUME HEADER TO DISK:  */

	/* write header last in case we fail along the way */

	/* Writes both copies of the volume header */
	WriteVH (driveInfo, header);
	/* VH is now big-endian */

	free(nodeBuffer);
	free(header);

	return (0);
}


/*
 * WriteMDB
 *
 * Writes the Master Directory Block (MDB) to disk.
 *
 * The MDB is byte-swapped if necessary to big endian. Since this
 * is always the last operation, there's no point in unswapping it.
 */
static void
WriteMDB (const DriveInfo *driveInfo, HFS_MDB *mdbp)
{
	SWAP_HFSMDB (mdbp);

	WriteBuffer(driveInfo, kMDBStart, kBytesPerSector, mdbp);
	WriteBuffer(driveInfo, driveInfo->totalSectors - 2, kBytesPerSector, mdbp);
}


/*
 * InitMDB
 *
 * Initialize a Master Directory Block (MDB) record.
 * 
 * If the alignment parameter is non-zero, it indicates the aligment
 * (in 512 byte sectors) that should be used for allocation blocks.
 * For example, if alignment is 8, then allocation blocks will begin
 * on a 4K boundary relative to the start of the partition.
 *
 */
static void
InitMDB(hfsparams_t *defaults, UInt32 driveBlocks, HFS_MDB *mdbp)
{
	UInt32	alBlkSize;
	UInt16	numAlBlks;
	UInt32	timeStamp;
	UInt16	bitmapBlocks;
	UInt32	alignment;
	VolumeUUID	newVolumeUUID;	
	VolumeUUID*	finderInfoUUIDPtr;

	alignment = defaults->hfsAlignment;
	bzero(mdbp, kBytesPerSector);
	
	alBlkSize = defaults->blockSize;

	/* calculate the number of sectors needed for bitmap (rounded up) */
	if (defaults->flags & kMakeMaxHFSBitmap)
		bitmapBlocks = kHFSMaxAllocationBlks / kBitsPerSector;
	else
		bitmapBlocks = ((driveBlocks / (alBlkSize >> kLog2SectorSize)) +
				kBitsPerSector-1) / kBitsPerSector;

	mdbp->drAlBlSt = kVolBitMapStart + bitmapBlocks;  /* in sectors (disk blocks) */

	/* If requested, round up block start to a multiple of "alignment" blocks */
	if (alignment != 0)
		mdbp->drAlBlSt = ((mdbp->drAlBlSt + alignment - 1) / alignment) * alignment;
	
	/* Now find out how many whole allocation blocks remain... */
	numAlBlks = (driveBlocks - mdbp->drAlBlSt - kTailBlocks) /
			(alBlkSize >> kLog2SectorSize);

	timeStamp = UTCToLocal(defaults->createDate);
	
	mdbp->drSigWord = kHFSSigWord;
	mdbp->drCrDate = timeStamp;
	mdbp->drLsMod = timeStamp;
	mdbp->drAtrb = kHFSVolumeUnmountedMask;
	mdbp->drVBMSt = kVolBitMapStart;
	mdbp->drNmAlBlks = numAlBlks;
	mdbp->drAlBlkSiz = alBlkSize;
	mdbp->drClpSiz = defaults->dataClumpSize;
	mdbp->drNxtCNID = defaults->nextFreeFileID;
	mdbp->drFreeBks = numAlBlks;
	
	/*
	 * Map UTF-8 input into a Mac encoding.
	 * On conversion errors "untitled" is used as a fallback.
	 */
	{
		UniChar unibuf[kHFSMaxVolumeNameChars];
		CFStringRef cfstr;
		CFIndex maxchars;
		Boolean cfOK;
	
		cfstr = CFStringCreateWithCString(kCFAllocatorDefault, (char *)defaults->volumeName, kCFStringEncodingUTF8);

		/* Find out what Mac encoding to use: */
		maxchars = MIN(sizeof(unibuf)/sizeof(UniChar), CFStringGetLength(cfstr));
		CFStringGetCharacters(cfstr, CFRangeMake(0, maxchars), unibuf);
		cfOK = CFStringGetPascalString(cfstr, mdbp->drVN, sizeof(mdbp->drVN), defaults->encodingHint);
		CFRelease(cfstr);

		if (!cfOK) {
			mdbp->drVN[0] = strlen(kDefaultVolumeNameStr);
			bcopy(kDefaultVolumeNameStr, &mdbp->drVN[1], mdbp->drVN[0]);
			defaults->encodingHint = 0;
			warnx("invalid HFS name: \"%s\", using \"%s\" instead",
			      defaults->volumeName, kDefaultVolumeNameStr);
		}
		/* defaults->volumeName is used later for the root dir key */
		bcopy(&mdbp->drVN[1], defaults->volumeName, mdbp->drVN[0]);
		defaults->volumeName[mdbp->drVN[0]] = '\0';
	}
	/* Save the encoding hint in the Finder Info (field 4). */
	mdbp->drFndrInfo[4] = SET_HFS_TEXT_ENCODING(defaults->encodingHint);

	mdbp->drWrCnt = kWriteSeqNum;

	mdbp->drXTFlSize = mdbp->drXTClpSiz = defaults->extentsClumpSize;
	mdbp->drXTExtRec[0].startBlock = 0;
	mdbp->drXTExtRec[0].blockCount = mdbp->drXTFlSize / alBlkSize;
	mdbp->drFreeBks -= mdbp->drXTExtRec[0].blockCount;

	mdbp->drCTFlSize = mdbp->drCTClpSiz = defaults->catalogClumpSize;
	mdbp->drCTExtRec[0].startBlock = mdbp->drXTExtRec[0].startBlock +
					 mdbp->drXTExtRec[0].blockCount;
	mdbp->drCTExtRec[0].blockCount = mdbp->drCTFlSize / alBlkSize;
	mdbp->drFreeBks -= mdbp->drCTExtRec[0].blockCount;

	if (defaults->flags & kMakeHFSWrapper) {
		mdbp->drFilCnt = mdbp->drNmFls = kWapperFileCount;
		mdbp->drNxtCNID += kWapperFileCount;

		/* set blessed system folder to be root folder (2) */
		mdbp->drFndrInfo[0] = kHFSRootFolderID;

		mdbp->drEmbedSigWord = kHFSPlusSigWord;

		/* software lock it and tag as having "bad" blocks */
		mdbp->drAtrb |= kHFSVolumeSparedBlocksMask;
		mdbp->drAtrb |= kHFSVolumeSoftwareLockMask;
	}
	/* Generate and write UUID for the HFS disk */
	GenerateVolumeUUID(&newVolumeUUID);
	finderInfoUUIDPtr = (VolumeUUID *)(&mdbp->drFndrInfo[6]);
	finderInfoUUIDPtr->v.high = OSSwapHostToBigInt32(newVolumeUUID.v.high); 
	finderInfoUUIDPtr->v.low = OSSwapHostToBigInt32(newVolumeUUID.v.low); 
}


/*
 * WriteVH
 *
 * Writes the Volume Header (VH) to disk.
 *
 * The VH is byte-swapped if necessary to big endian. Since this
 * is always the last operation, there's no point in unswapping it.
 */
static void
WriteVH (const DriveInfo *driveInfo, HFSPlusVolumeHeader *hp)
{
	SWAP_HFSPLUSVH (hp);

	WriteBuffer(driveInfo, 2, kBytesPerSector, hp);
	WriteBuffer(driveInfo, driveInfo->totalSectors - 2, kBytesPerSector, hp);
}


/*
 * InitVH
 *
 * Initialize a Volume Header record.
 */
static void
InitVH(hfsparams_t *defaults, UInt64 sectors, HFSPlusVolumeHeader *hp)
{
	UInt32	blockSize;
	UInt32	blockCount;
	UInt32	blocksUsed;
	UInt32	bitmapBlocks;
	UInt16	burnedBlocksBeforeVH = 0;
	UInt16	burnedBlocksAfterAltVH = 0;
	UInt32  nextBlock;
	VolumeUUID	newVolumeUUID;	
	VolumeUUID*	finderInfoUUIDPtr;
	UInt64	hotFileBandSize;
	UInt64 volsize;

/* 
 * 2 MB is the minimum size for the new behavior with 
 * space after the attr b-tree, and hotfile stuff. 
 */
#define MINVOLSIZE_WITHSPACE 2097152 

	bzero(hp, kBytesPerSector);

	blockSize = defaults->blockSize;
	blockCount = sectors / (blockSize >> kLog2SectorSize);

	/*
	 * HFSPlusVolumeHeader is located at sector 2, so we may need
	 * to invalidate blocks before HFSPlusVolumeHeader.
	 */
	if ( blockSize == 512 ) {
		burnedBlocksBeforeVH = 2;		/* 2 before VH */
		burnedBlocksAfterAltVH = 1;		/* 1 after altVH */
	} else if ( blockSize == 1024 ) {
		burnedBlocksBeforeVH = 1;
	}
	nextBlock = burnedBlocksBeforeVH + 1;		/* +1 for VH itself */
	
	bitmapBlocks = defaults->allocationClumpSize / blockSize;

	/* note: add 2 for the Alternate VH, and VH */
	blocksUsed = 2 + burnedBlocksBeforeVH + burnedBlocksAfterAltVH + bitmapBlocks;

	if (defaults->flags & kMakeCaseSensitive) {
		hp->signature = kHFSXSigWord;
		hp->version = kHFSXVersion;
	} else {
		hp->signature = kHFSPlusSigWord;
		hp->version = kHFSPlusVersion;
	}
	hp->attributes = kHFSVolumeUnmountedMask | kHFSUnusedNodeFixMask;
	hp->lastMountedVersion = kHFSPlusMountVersion;

	/* NOTE: create date is in local time, not GMT!  */
	hp->createDate = UTCToLocal(defaults->createDate);
	hp->modifyDate = defaults->createDate;
	hp->backupDate = 0;
	hp->checkedDate = defaults->createDate;

//	hp->fileCount = 0;
//	hp->folderCount = 0;

	hp->blockSize = blockSize;
	hp->totalBlocks = blockCount;
	hp->freeBlocks = blockCount;	/* will be adjusted at the end */

	volsize = (UInt64) blockCount * (UInt64) blockSize;

	hp->rsrcClumpSize = defaults->rsrcClumpSize;
	hp->dataClumpSize = defaults->dataClumpSize;
	hp->nextCatalogID = defaults->nextFreeFileID;
	hp->encodingsBitmap = 1 | (1 << ENCODING_TO_BIT(defaults->encodingHint));

	/* set up allocation bitmap file */
	hp->allocationFile.clumpSize = defaults->allocationClumpSize;
	hp->allocationFile.logicalSize = defaults->allocationClumpSize;
	hp->allocationFile.totalBlocks = bitmapBlocks;
  	hp->allocationFile.extents[0].startBlock = nextBlock;
	hp->allocationFile.extents[0].blockCount = bitmapBlocks;
	nextBlock += hp->allocationFile.extents[0].blockCount;

	/* set up journal files */
	if (defaults->journaledHFS) {
		hp->fileCount           = 2;
		hp->attributes         |= kHFSVolumeJournaledMask;
		hp->nextCatalogID      += 2;

		/*
		 * Allocate 1 block for the journalInfoBlock.  The
		 * journal file size is passed in hfsparams_t.
		 */
	    hp->journalInfoBlock = nextBlock;
	    /*XXX What if journal is on a different device? */
	    blocksUsed += 1 + ((defaults->journalSize+blockSize-1) / blockSize);
	    nextBlock += 1 + ((defaults->journalSize+blockSize-1) / blockSize);
	} else {
	    hp->journalInfoBlock = 0;
	}

	/* set up extents b-tree file */
	hp->extentsFile.clumpSize = defaults->extentsClumpSize;
	hp->extentsFile.logicalSize = defaults->extentsClumpSize;
	hp->extentsFile.totalBlocks = defaults->extentsClumpSize / blockSize;
	hp->extentsFile.extents[0].startBlock = nextBlock;
	hp->extentsFile.extents[0].blockCount = hp->extentsFile.totalBlocks;
	blocksUsed += hp->extentsFile.totalBlocks;
	nextBlock += hp->extentsFile.totalBlocks;
	
	/* set up attributes b-tree file */
	if (defaults->attributesClumpSize) {
		hp->attributesFile.clumpSize = defaults->attributesClumpSize;
		hp->attributesFile.logicalSize = defaults->attributesClumpSize;
		hp->attributesFile.totalBlocks = defaults->attributesClumpSize / blockSize;
		hp->attributesFile.extents[0].startBlock = nextBlock;
		hp->attributesFile.extents[0].blockCount = hp->attributesFile.totalBlocks;
		blocksUsed += hp->attributesFile.totalBlocks;
		nextBlock += hp->attributesFile.totalBlocks;
		
		/*
		 * Leave some room for the Attributes B-tree to grow, if the volsize >= 2MB
		 */
		if (volsize >= MINVOLSIZE_WITHSPACE) {
			nextBlock += 10 * (hp->attributesFile.clumpSize / blockSize);
		}
	}

	/* set up catalog b-tree file */
	hp->catalogFile.clumpSize = defaults->catalogClumpSize;
	hp->catalogFile.logicalSize = defaults->catalogClumpSize;
	hp->catalogFile.totalBlocks = defaults->catalogClumpSize / blockSize;
	hp->catalogFile.extents[0].startBlock = nextBlock;
	hp->catalogFile.extents[0].blockCount = hp->catalogFile.totalBlocks;
	blocksUsed += hp->catalogFile.totalBlocks;
	nextBlock += hp->catalogFile.totalBlocks;
	
	/*
	 * Add some room for the catalog file to grow...
	 */
	nextBlock += 10 * (hp->catalogFile.clumpSize / hp->blockSize);

	/*
	 * Add some room for the hot file band.  This uses the same 5MB per GB
	 * as the kernel.  The #defines below were copied from the kernel.  But again,
	 * only do this if the volume is bigger than 2 MB. 
	 */
	if (volsize >= MINVOLSIZE_WITHSPACE) {
#define HOTBAND_MINIMUM_SIZE  (10*1024*1024)
#define HOTBAND_MAXIMUM_SIZE  (512*1024*1024)
		hotFileBandSize = (UInt64) blockCount * blockSize / 1024 * 5;
		if (hotFileBandSize > HOTBAND_MAXIMUM_SIZE)
			hotFileBandSize = HOTBAND_MAXIMUM_SIZE;
		else if (hotFileBandSize < HOTBAND_MINIMUM_SIZE)
			hotFileBandSize = HOTBAND_MINIMUM_SIZE;
		nextBlock += hotFileBandSize / blockSize;
	}
	hp->nextAllocation = nextBlock;
	
	/* Adjust free blocks to reflect everything we have allocated. */
	hp->freeBlocks -= blocksUsed;

	/* Generate and write UUID for the HFS+ disk */
	GenerateVolumeUUID(&newVolumeUUID);
	finderInfoUUIDPtr = (VolumeUUID *)(&hp->finderInfo[24]);
	finderInfoUUIDPtr->v.high = OSSwapHostToBigInt32(newVolumeUUID.v.high); 
	finderInfoUUIDPtr->v.low = OSSwapHostToBigInt32(newVolumeUUID.v.low); 
}


/*
 * AllocateExtent
 *
 * Mark the given extent as in-use in the given bitmap buffer.
 */
static void AllocateExtent(UInt8 *buffer, UInt32 startBlock, UInt32 blockCount)
{
	UInt8 *p;
	
	/* Point to start of extent in bitmap buffer */
	p = buffer + (startBlock / 8);
	
	/* Partial byte at start of extent */
	if (startBlock & 7)
	{
		*(p++) |= 0xFF >> (startBlock & 7);
		blockCount -= 8 - (startBlock & 7);
	}
	
	/* Fill in whole bytes */
	if (blockCount >= 8)
	{
		memset(p, 0xFF, blockCount / 8);
		p += blockCount / 8;
		blockCount &= 7;
	}
	
	/* Partial byte at end of extent */
	if (blockCount)
	{
		*(p++) |= 0xFF << (8 - blockCount);
	}
}

/*
 * InitBitmap
 * 	
 * This routine initializes the Allocation Bitmap. Allocation blocks
 * that are in use have their corresponding bit set.
 * 
 * It assumes that initially there are no gaps between allocated blocks.
 * Allocation blocks 0 through alBlksUsed-1 will be marked in-use.
 * 
 * It also assumes the buffer is big enough to hold all the bits
 * (ie its at least (alBlksUsed/8) bytes in size.
 */
static void
WriteBitmap(const DriveInfo *driveInfo, UInt64 startingSector,
        UInt32 alBlksUsed, UInt8 *buffer)
{
	UInt32	bytes, bits, bytesUsed;

	bytes = alBlksUsed >> 3;
	bits  = alBlksUsed & 0x0007;

	(void)memset(buffer, 0xFF, bytes);

	if (bits) {
		*(UInt8 *)(buffer + bytes) = (0xFF00 >> bits) & 0xFF;
		++bytes;
	}

	bytesUsed = ROUNDUP(bytes, driveInfo->sectorSize);

	if (bytesUsed > bytes)
		bzero(buffer + bytes, bytesUsed - bytes);
	WriteBuffer(driveInfo, startingSector, bytesUsed, buffer);
}


/*
 * WriteExtentsFile
 *
 * Initializes and writes out the extents b-tree file.
 *
 * Byte swapping is performed in place. The buffer should not be
 * accessed through direct casting once it leaves this function.
 */
static void
WriteExtentsFile(const DriveInfo *driveInfo, UInt64 startingSector,
        const hfsparams_t *dp, HFSExtentDescriptor *bbextp, void *buffer,
        UInt32 *bytesUsed, UInt32 *mapNodes)
{
	BTNodeDescriptor	*ndp;
	BTHeaderRec		*bthp;
	UInt8			*bmp;
	UInt32			nodeBitsInHeader;
	UInt32			fileSize;
	UInt32			nodeSize;
	UInt32			temp;
	SInt16			offset;
	int			wrapper = (dp->flags & kMakeHFSWrapper);

	*mapNodes = 0;
	fileSize = dp->extentsClumpSize;
	nodeSize = dp->extentsNodeSize;

	bzero(buffer, nodeSize);


	/* FILL IN THE NODE DESCRIPTOR:  */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind			= kBTHeaderNode;
	ndp->numRecords		= SWAP_BE16 (3);
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);


	/* FILL IN THE HEADER RECORD:  */
	bthp = (BTHeaderRec *)((UInt8 *)buffer + offset);
    //	bthp->treeDepth		= 0;
    //	bthp->rootNode		= 0;
    //	bthp->firstLeafNode	= 0;
    //	bthp->lastLeafNode	= 0;
    //	bthp->leafRecords	= 0;
	bthp->nodeSize		= SWAP_BE16 (nodeSize);
	bthp->totalNodes	= SWAP_BE32 (fileSize / nodeSize);
	bthp->freeNodes		= SWAP_BE32 (SWAP_BE32 (bthp->totalNodes) - 1);  /* header */
	bthp->clumpSize		= SWAP_BE32 (fileSize);

	if (dp->flags & kMakeStandardHFS) {
		bthp->maxKeyLength = SWAP_BE16 (kHFSExtentKeyMaximumLength);

		/* wrapper has a bad-block extent record */
		if (wrapper) {
			bthp->treeDepth     = SWAP_BE16 (SWAP_BE16 (bthp->treeDepth) + 1);
			bthp->leafRecords   = SWAP_BE32 (SWAP_BE32 (bthp->leafRecords) + 1);
			bthp->rootNode      = SWAP_BE32 (1);
			bthp->firstLeafNode = SWAP_BE32 (1);
			bthp->lastLeafNode  = SWAP_BE32 (1);
			bthp->freeNodes     = SWAP_BE32 (SWAP_BE32 (bthp->freeNodes) - 1);
		}
	} else {
		bthp->attributes |= SWAP_BE32 (kBTBigKeysMask);
		bthp->maxKeyLength = SWAP_BE16 (kHFSPlusExtentKeyMaximumLength);
	}
	offset += sizeof(BTHeaderRec);

	SETOFFSET(buffer, nodeSize, offset, 2);

	offset += kBTreeHeaderUserBytes;

	SETOFFSET(buffer, nodeSize, offset, 3);


	/* FIGURE OUT HOW MANY MAP NODES (IF ANY):  */
	nodeBitsInHeader = 8 * (nodeSize
					- sizeof(BTNodeDescriptor)
					- sizeof(BTHeaderRec)
					- kBTreeHeaderUserBytes
					- (4 * sizeof(SInt16)) );

	if (SWAP_BE32 (bthp->totalNodes) > nodeBitsInHeader) {
		UInt32	nodeBitsInMapNode;
		
		ndp->fLink		= SWAP_BE32 (SWAP_BE32 (bthp->lastLeafNode) + 1);
		nodeBitsInMapNode = 8 * (nodeSize
						- sizeof(BTNodeDescriptor)
						- (2 * sizeof(SInt16))
						- 2 );
		*mapNodes = (SWAP_BE32 (bthp->totalNodes) - nodeBitsInHeader +
			(nodeBitsInMapNode - 1)) / nodeBitsInMapNode;
		bthp->freeNodes = SWAP_BE32 (SWAP_BE32 (bthp->freeNodes) - *mapNodes);
	}


	/* 
	 * FILL IN THE MAP RECORD, MARKING NODES THAT ARE IN USE.
	 * Note - worst case (32MB alloc blk) will have only 18 nodes in use.
	 */
	bmp = ((UInt8 *)buffer + offset);
	temp = SWAP_BE32 (bthp->totalNodes) - SWAP_BE32 (bthp->freeNodes);

	/* Working a byte at a time is endian safe */
	while (temp >= 8) { *bmp = 0xFF; temp -= 8; bmp++; }
	*bmp = ~(0xFF >> temp);
	offset += nodeBitsInHeader/8;

	SETOFFSET(buffer, nodeSize, offset, 4);

	if (wrapper) {
		InitExtentsRoot(nodeSize, bbextp, (buffer + nodeSize));
	}
	
	*bytesUsed = (SWAP_BE32 (bthp->totalNodes) - SWAP_BE32 (bthp->freeNodes) - *mapNodes) * nodeSize;

	WriteBuffer(driveInfo, startingSector, *bytesUsed, buffer);
}


static void
InitExtentsRoot(UInt16 btNodeSize, HFSExtentDescriptor *bbextp, void *buffer)
{
	BTNodeDescriptor	*ndp;
	HFSExtentKey		*ekp;
	HFSExtentRecord		*edp;
	SInt16				offset;

	bzero(buffer, btNodeSize);

	/*
	 * All nodes have a node descriptor...
	 */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind			= kBTLeafNode;
	ndp->numRecords		= SWAP_BE16 (1);
	ndp->height			= 1;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, btNodeSize, offset, 1);

	/*
	 * First and only record is bad block extents...
	 */
	ekp = (HFSExtentKey *)((UInt8 *) buffer + offset);
	ekp->keyLength		= kHFSExtentKeyMaximumLength;
	// ekp->forkType	= 0;
	ekp->fileID			= SWAP_BE32 (kHFSBadBlockFileID);
	// ekp->startBlock	= 0;
	offset += sizeof(HFSExtentKey);

	edp = (HFSExtentRecord *)((UInt8 *)buffer + offset);
	edp[0]->startBlock	= SWAP_BE16 (bbextp->startBlock);
	edp[0]->blockCount	= SWAP_BE16 (bbextp->blockCount);
	offset += sizeof(HFSExtentRecord);

	SETOFFSET(buffer, btNodeSize, offset, 2);
}


/*
 * WriteAttributesFile
 *
 * Initializes and writes out the attributes b-tree file.
 *
 * Byte swapping is performed in place. The buffer should not be
 * accessed through direct casting once it leaves this function.
 */
static void
WriteAttributesFile(const DriveInfo *driveInfo, UInt64 startingSector,
        const hfsparams_t *dp, HFSExtentDescriptor *bbextp, void *buffer,
        UInt32 *bytesUsed, UInt32 *mapNodes)
{
	BTNodeDescriptor	*ndp;
	BTHeaderRec		*bthp;
	UInt8			*bmp;
	UInt32			nodeBitsInHeader;
	UInt32			fileSize;
	UInt32			nodeSize;
	UInt32			temp;
	SInt16			offset;

	*mapNodes = 0;
	fileSize = dp->attributesClumpSize;
	nodeSize = dp->attributesNodeSize;

	bzero(buffer, nodeSize);


	/* FILL IN THE NODE DESCRIPTOR:  */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind			= kBTHeaderNode;
	ndp->numRecords		= SWAP_BE16 (3);
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);


	/* FILL IN THE HEADER RECORD:  */
	bthp = (BTHeaderRec *)((UInt8 *)buffer + offset);
    //	bthp->treeDepth		= 0;
    //	bthp->rootNode		= 0;
    //	bthp->firstLeafNode	= 0;
    //	bthp->lastLeafNode	= 0;
    //	bthp->leafRecords	= 0;
	bthp->nodeSize		= SWAP_BE16 (nodeSize);
	bthp->totalNodes	= SWAP_BE32 (fileSize / nodeSize);
	bthp->freeNodes		= SWAP_BE32 (SWAP_BE32 (bthp->totalNodes) - 1);  /* header */
	bthp->clumpSize		= SWAP_BE32 (fileSize);

	bthp->attributes |= SWAP_BE32 (kBTBigKeysMask);
	bthp->maxKeyLength = SWAP_BE16 (kHFSPlusAttrKeyMaximumLength);

	offset += sizeof(BTHeaderRec);

	SETOFFSET(buffer, nodeSize, offset, 2);

	offset += kBTreeHeaderUserBytes;

	SETOFFSET(buffer, nodeSize, offset, 3);


	/* FIGURE OUT HOW MANY MAP NODES (IF ANY):  */
	nodeBitsInHeader = 8 * (nodeSize
					- sizeof(BTNodeDescriptor)
					- sizeof(BTHeaderRec)
					- kBTreeHeaderUserBytes
					- (4 * sizeof(SInt16)) );
	if (SWAP_BE32 (bthp->totalNodes) > nodeBitsInHeader) {
		UInt32	nodeBitsInMapNode;
		
		ndp->fLink		= SWAP_BE32 (SWAP_BE32 (bthp->lastLeafNode) + 1);
		nodeBitsInMapNode = 8 * (nodeSize
						- sizeof(BTNodeDescriptor)
						- (2 * sizeof(SInt16))
						- 2 );
		*mapNodes = (SWAP_BE32 (bthp->totalNodes) - nodeBitsInHeader +
			(nodeBitsInMapNode - 1)) / nodeBitsInMapNode;
		bthp->freeNodes = SWAP_BE32 (SWAP_BE32 (bthp->freeNodes) - *mapNodes);
	}


	/* 
	 * FILL IN THE MAP RECORD, MARKING NODES THAT ARE IN USE.
	 * Note - worst case (32MB alloc blk) will have only 18 nodes in use.
	 */
	bmp = ((UInt8 *)buffer + offset);
	temp = SWAP_BE32 (bthp->totalNodes) - SWAP_BE32 (bthp->freeNodes);

	/* Working a byte at a time is endian safe */
	while (temp >= 8) { *bmp = 0xFF; temp -= 8; bmp++; }
	*bmp = ~(0xFF >> temp);
	offset += nodeBitsInHeader/8;

	SETOFFSET(buffer, nodeSize, offset, 4);

	*bytesUsed = (SWAP_BE32 (bthp->totalNodes) - SWAP_BE32 (bthp->freeNodes) - *mapNodes) * nodeSize;
	WriteBuffer(driveInfo, startingSector, *bytesUsed, buffer);
}


static int
get_dev_uuid(const char *disk_name, char *dev_uuid_str, int dev_uuid_len)
{
    DADiskRef dref;
    CFDictionaryRef description;
    CFUUIDRef dev_uuid;
    CFStringRef uuid_str;
    int ret = EINVAL;
    DASessionRef session;

    session = DASessionCreate(kCFAllocatorDefault);
    if (session == NULL) {
	printf("Could not create a diskarb session.\n");
	return EINVAL;
    }

    dev_uuid_str[0] = '\0';

    dref =  DADiskCreateFromBSDName(NULL, session, disk_name);
    if (dref != NULL) {
	description = DADiskCopyDescription(dref);
	if (description != NULL) {
	    dev_uuid = CFDictionaryGetValue(description, kDADiskDescriptionMediaUUIDKey);
	    if (dev_uuid != NULL) {
		uuid_str = CFUUIDCreateString(NULL, dev_uuid);
		if (uuid_str) {
		    if (CFStringGetFileSystemRepresentation(uuid_str, dev_uuid_str, dev_uuid_len) != 0) {
			ret = 0;
		    }
		    CFRelease(uuid_str);
		}
	    }
	    CFRelease(description);
	}
	CFRelease(dref);
    }

    CFRelease(session);
    
    return ret;
}

static int
clear_journal_dev(const char *dev_name)
{
    int fd;

    fd = open(dev_name, O_RDWR);
    if (fd < 0) {
	printf("Failed to open the journal device %s (%s)\n", dev_name, strerror(errno));
	return -1;
    }

    dowipefs(fd);

    close(fd);
    return 0;
}


static int
WriteJournalInfo(const DriveInfo *driveInfo, UInt64 startingSector,
		 const hfsparams_t *dp, HFSPlusVolumeHeader *header,
		 void *buffer)
{
    JournalInfoBlock *jibp = buffer;
    
    memset(buffer, 0xdb, driveInfo->physSectorSize);
    memset(jibp, 0, sizeof(JournalInfoBlock));
    
    if (dp->journalDevice) {
	char uuid_str[64];

	if (get_dev_uuid(dp->journalDevice, uuid_str, sizeof(uuid_str)) == 0) {
	    strlcpy((char *)&jibp->reserved[0], uuid_str, sizeof(jibp->reserved));
	    
	    // we also need to blast out some zeros to the journal device
	    // in case it had a file system on it previously.  that way
	    // it's "initialized" in the sense that the previous contents
	    // won't get mounted accidently.  if this fails we'll bail out.
	    if (clear_journal_dev(dp->journalDevice) != 0) {
		return -1;
	    }
	} else {
	    printf("FAILED to get the device uuid for device %s\n", dp->journalDevice);
	    strlcpy((char *)&jibp->reserved[0], "NO-DEV-UUID", sizeof(jibp->reserved));
	    return -1;
	}
    } else {
	jibp->flags = kJIJournalInFSMask;
    }
    jibp->flags  |= kJIJournalNeedInitMask;
    jibp->offset  = ((UInt64) header->journalInfoBlock + 1) * header->blockSize;
    jibp->size    = dp->journalSize;

    jibp->flags  = SWAP_BE32(jibp->flags);
    jibp->offset = SWAP_BE64(jibp->offset);
    jibp->size   = SWAP_BE64(jibp->size);
    
    WriteBuffer(driveInfo, startingSector, driveInfo->physSectorSize, buffer);

    jibp->flags  = SWAP_BE32(jibp->flags);
    jibp->offset = SWAP_BE64(jibp->offset);
    jibp->size   = SWAP_BE64(jibp->size);

    return 0;
}


/*
 * WriteCatalogFile
 *	
 * This routine initializes a Catalog B-Tree.
 *
 * Note: Since large volumes can have bigger b-trees they
 * might need to have map nodes setup.
 */
static void
WriteCatalogFile(const DriveInfo *driveInfo, UInt64 startingSector,
        const hfsparams_t *dp, HFSPlusVolumeHeader *header, void *buffer,
        UInt32 *bytesUsed, UInt32 *mapNodes)
{
	BTNodeDescriptor	*ndp;
	BTHeaderRec		*bthp;
	UInt8			*bmp;
	UInt32			nodeBitsInHeader;
	UInt32			fileSize;
	UInt32			nodeSize;
	UInt32			temp;
	SInt16			offset;
	int			wrapper = (dp->flags & kMakeHFSWrapper);

	*mapNodes = 0;
	fileSize = dp->catalogClumpSize;
	nodeSize = dp->catalogNodeSize;

	bzero(buffer, nodeSize);


	/* FILL IN THE NODE DESCRIPTOR:  */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind			= kBTHeaderNode;
	ndp->numRecords		= SWAP_BE16 (3);
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);


	/* FILL IN THE HEADER RECORD:  */
	bthp = (BTHeaderRec *)((UInt8 *)buffer + offset);
	bthp->treeDepth		= SWAP_BE16 (1);
	bthp->rootNode		= SWAP_BE32 (1);
	bthp->firstLeafNode	= SWAP_BE32 (1);
	bthp->lastLeafNode	= SWAP_BE32 (1);
	bthp->leafRecords	= SWAP_BE32 (dp->journaledHFS ? 6 : 2);
	bthp->nodeSize		= SWAP_BE16 (nodeSize);
	bthp->totalNodes	= SWAP_BE32 (fileSize / nodeSize);
	bthp->freeNodes		= SWAP_BE32 (SWAP_BE32 (bthp->totalNodes) - 2);  /* header and root */
	bthp->clumpSize		= SWAP_BE32 (fileSize);

	if (dp->flags & kMakeStandardHFS) {
		bthp->maxKeyLength	=  SWAP_BE16 (kHFSCatalogKeyMaximumLength);

		if (dp->flags & kMakeHFSWrapper) {
			bthp->treeDepth		= SWAP_BE16 (SWAP_BE16 (bthp->treeDepth) + 1);
			bthp->leafRecords	= SWAP_BE32 (SWAP_BE32 (bthp->leafRecords) + kWapperFileCount);
			bthp->firstLeafNode	= SWAP_BE32 (SWAP_BE32 (bthp->rootNode) + 1);
			bthp->lastLeafNode	= SWAP_BE32 (SWAP_BE32 (bthp->firstLeafNode) + 1);
			bthp->freeNodes		= SWAP_BE32 (SWAP_BE32 (bthp->freeNodes) - 2);  /* tree now split with 2 leaf nodes */
		}
	} else /* HFS+ */ {
		bthp->attributes	|= SWAP_BE32 (kBTVariableIndexKeysMask + kBTBigKeysMask);
		bthp->maxKeyLength	=  SWAP_BE16 (kHFSPlusCatalogKeyMaximumLength);
		if (dp->flags & kMakeCaseSensitive)
			bthp->keyCompareType = kHFSBinaryCompare;
		else
			bthp->keyCompareType = kHFSCaseFolding;
	}
	offset += sizeof(BTHeaderRec);

	SETOFFSET(buffer, nodeSize, offset, 2);

	offset += kBTreeHeaderUserBytes;

	SETOFFSET(buffer, nodeSize, offset, 3);

	/* FIGURE OUT HOW MANY MAP NODES (IF ANY):  */
	nodeBitsInHeader = 8 * (nodeSize
					- sizeof(BTNodeDescriptor)
					- sizeof(BTHeaderRec)
					- kBTreeHeaderUserBytes
					- (4 * sizeof(SInt16)) );

	if (SWAP_BE32 (bthp->totalNodes) > nodeBitsInHeader) {
		UInt32	nodeBitsInMapNode;
		
		ndp->fLink = SWAP_BE32 (SWAP_BE32 (bthp->lastLeafNode) + 1);
		nodeBitsInMapNode = 8 * (nodeSize
						- sizeof(BTNodeDescriptor)
						- (2 * sizeof(SInt16))
						- 2 );
		*mapNodes = (SWAP_BE32 (bthp->totalNodes) - nodeBitsInHeader +
			(nodeBitsInMapNode - 1)) / nodeBitsInMapNode;
		bthp->freeNodes = SWAP_BE32 (SWAP_BE32 (bthp->freeNodes) - *mapNodes);
	}

	/* 
	 * FILL IN THE MAP RECORD, MARKING NODES THAT ARE IN USE.
	 * Note - worst case (32MB alloc blk) will have only 18 nodes in use.
	 */
	bmp = ((UInt8 *)buffer + offset);
	temp = SWAP_BE32 (bthp->totalNodes) - SWAP_BE32 (bthp->freeNodes);

	/* Working a byte at a time is endian safe */
	while (temp >= 8) { *bmp = 0xFF; temp -= 8; bmp++; }
	*bmp = ~(0xFF >> temp);
	offset += nodeBitsInHeader/8;

	SETOFFSET(buffer, nodeSize, offset, 4);

	if ((dp->flags & kMakeStandardHFS) == 0) {
		InitCatalogRoot_HFSPlus(dp, header, buffer + nodeSize);

	} else if (wrapper) {
		InitCatalogRoot_HFS  (dp, buffer + (1 * nodeSize));
		InitFirstCatalogLeaf (dp, buffer + (2 * nodeSize), TRUE);
		InitSecondCatalogLeaf(dp, buffer + (3 * nodeSize));

	} else /* plain HFS */ {
		InitFirstCatalogLeaf(dp, buffer + nodeSize, FALSE);
	}

	*bytesUsed = (SWAP_BE32 (bthp->totalNodes) - SWAP_BE32 (bthp->freeNodes) - *mapNodes) * nodeSize;

	WriteBuffer(driveInfo, startingSector, *bytesUsed, buffer);
}


static void
InitCatalogRoot_HFSPlus(const hfsparams_t *dp, const HFSPlusVolumeHeader *header, void * buffer)
{
	BTNodeDescriptor		*ndp;
	HFSPlusCatalogKey		*ckp;
	HFSPlusCatalogKey		*tkp;
	HFSPlusCatalogFolder	*cdp;
	HFSPlusCatalogFile		*cfp;
	HFSPlusCatalogThread	*ctp;
	UInt16					nodeSize;
	SInt16					offset;
	size_t					unicodeBytes;
	UInt8 canonicalName[256];
	CFStringRef cfstr;
	Boolean	cfOK;
	int index = 0;

	nodeSize = dp->catalogNodeSize;
	bzero(buffer, nodeSize);

	/*
	 * All nodes have a node descriptor...
	 */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind   = kBTLeafNode;
	ndp->height = 1;
	ndp->numRecords = SWAP_BE16 (dp->journaledHFS ? 6 : 2);
	offset = sizeof(BTNodeDescriptor);
	SETOFFSET(buffer, nodeSize, offset, ++index);

	/*
	 * First record is always the root directory...
	 */
	ckp = (HFSPlusCatalogKey *)((UInt8 *)buffer + offset);
	
	/* Use CFString functions to get a HFSPlus Canonical name */
	cfstr = CFStringCreateWithCString(kCFAllocatorDefault, (char *)dp->volumeName, kCFStringEncodingUTF8);
	cfOK = _CFStringGetFileSystemRepresentation(cfstr, canonicalName, sizeof(canonicalName));

	if (!cfOK || ConvertUTF8toUnicode(canonicalName, sizeof(ckp->nodeName.unicode),
		ckp->nodeName.unicode, &ckp->nodeName.length)) {

		/* On conversion errors "untitled" is used as a fallback. */
		(void) ConvertUTF8toUnicode((UInt8 *)kDefaultVolumeNameStr,
									sizeof(ckp->nodeName.unicode),
									ckp->nodeName.unicode,
									&ckp->nodeName.length);
		warnx("invalid HFS+ name: \"%s\", using \"%s\" instead",
		      dp->volumeName, kDefaultVolumeNameStr);
	}
	CFRelease(cfstr);
	ckp->nodeName.length = SWAP_BE16 (ckp->nodeName.length);

	unicodeBytes = sizeof(UniChar) * SWAP_BE16 (ckp->nodeName.length);

	ckp->keyLength		= SWAP_BE16 (kHFSPlusCatalogKeyMinimumLength + unicodeBytes);
	ckp->parentID		= SWAP_BE32 (kHFSRootParentID);
	offset += SWAP_BE16 (ckp->keyLength) + 2;

	cdp = (HFSPlusCatalogFolder *)((UInt8 *)buffer + offset);
	cdp->recordType		= SWAP_BE16 (kHFSPlusFolderRecord);
	/* folder count is only supported on HFSX volumes */
	if (dp->flags & kMakeCaseSensitive) {
		cdp->flags 		= SWAP_BE16 (kHFSHasFolderCountMask);
	}
	cdp->valence        = SWAP_BE32 (dp->journaledHFS ? 2 : 0);
	cdp->folderID		= SWAP_BE32 (kHFSRootFolderID);
	cdp->createDate		= SWAP_BE32 (dp->createDate);
	cdp->contentModDate	= SWAP_BE32 (dp->createDate);
	cdp->textEncoding	= SWAP_BE32 (dp->encodingHint);
	if (dp->flags & kUseAccessPerms) {
		cdp->bsdInfo.ownerID  = SWAP_BE32 (dp->owner);
		cdp->bsdInfo.groupID  = SWAP_BE32 (dp->group);
		cdp->bsdInfo.fileMode = SWAP_BE16 (dp->mask | S_IFDIR);
	}
	offset += sizeof(HFSPlusCatalogFolder);
	SETOFFSET(buffer, nodeSize, offset, ++index);

	/*
	 * Second record is always the root directory thread...
	 */
	tkp = (HFSPlusCatalogKey *)((UInt8 *)buffer + offset);
	tkp->keyLength		= SWAP_BE16 (kHFSPlusCatalogKeyMinimumLength);
	tkp->parentID		= SWAP_BE32 (kHFSRootFolderID);
	// tkp->nodeName.length = 0;

	offset += SWAP_BE16 (tkp->keyLength) + 2;

	ctp = (HFSPlusCatalogThread *)((UInt8 *)buffer + offset);
	ctp->recordType		= SWAP_BE16 (kHFSPlusFolderThreadRecord);
	ctp->parentID		= SWAP_BE32 (kHFSRootParentID);
	bcopy(&ckp->nodeName, &ctp->nodeName, sizeof(UInt16) + unicodeBytes);
	offset += (sizeof(HFSPlusCatalogThread)
			- (sizeof(ctp->nodeName.unicode) - unicodeBytes) );

	SETOFFSET(buffer, nodeSize, offset, ++index);
	
	/*
	 * Add records for ".journal" and ".journal_info_block" files:
	 */
	if (dp->journaledHFS) {
		struct HFSUniStr255 *nodename1, *nodename2;
		size_t uBytes1, uBytes2;

		/* File record #1 */
		ckp = (HFSPlusCatalogKey *)((UInt8 *)buffer + offset);
		(void) ConvertUTF8toUnicode((UInt8 *)HFS_JOURNAL_FILE, sizeof(ckp->nodeName.unicode),
		                            ckp->nodeName.unicode, &ckp->nodeName.length);
		ckp->nodeName.length = SWAP_BE16 (ckp->nodeName.length);
		uBytes1 = sizeof(UniChar) * SWAP_BE16 (ckp->nodeName.length);
		ckp->keyLength = SWAP_BE16 (kHFSPlusCatalogKeyMinimumLength + uBytes1);
		ckp->parentID  = SWAP_BE32 (kHFSRootFolderID);
		offset += SWAP_BE16 (ckp->keyLength) + 2;
	
		cfp = (HFSPlusCatalogFile *)((UInt8 *)buffer + offset);
		cfp->recordType     = SWAP_BE16 (kHFSPlusFileRecord);
		cfp->flags          = SWAP_BE16 (kHFSThreadExistsMask);
		cfp->fileID         = SWAP_BE32 (dp->nextFreeFileID);
		cfp->createDate     = SWAP_BE32 (dp->createDate + 1);
		cfp->contentModDate = SWAP_BE32 (dp->createDate + 1);
		cfp->textEncoding   = 0;

		cfp->bsdInfo.fileMode     = SWAP_BE16 (S_IFREG);
		cfp->bsdInfo.ownerFlags   = SWAP_BE16 (UF_NODUMP);
		cfp->userInfo.fdType	  = SWAP_BE32 (kJournalFileType);
		cfp->userInfo.fdCreator	  = SWAP_BE32 (kHFSPlusCreator);
		cfp->userInfo.fdFlags     = SWAP_BE16 (kIsInvisible + kNameLocked);
		cfp->dataFork.logicalSize = SWAP_BE64 (dp->journalSize);
		cfp->dataFork.totalBlocks = SWAP_BE32 ((dp->journalSize+dp->blockSize-1) / dp->blockSize);

		cfp->dataFork.extents[0].startBlock = SWAP_BE32 (header->journalInfoBlock + 1);
		cfp->dataFork.extents[0].blockCount = cfp->dataFork.totalBlocks;

		offset += sizeof(HFSPlusCatalogFile);
		SETOFFSET(buffer, nodeSize, offset, ++index);
		nodename1 = &ckp->nodeName;

		/* File record #2 */
		ckp = (HFSPlusCatalogKey *)((UInt8 *)buffer + offset);
		(void) ConvertUTF8toUnicode((UInt8 *)HFS_JOURNAL_INFO, sizeof(ckp->nodeName.unicode),
		                            ckp->nodeName.unicode, &ckp->nodeName.length);
		ckp->nodeName.length = SWAP_BE16 (ckp->nodeName.length);
		uBytes2 = sizeof(UniChar) * SWAP_BE16 (ckp->nodeName.length);
		ckp->keyLength = SWAP_BE16 (kHFSPlusCatalogKeyMinimumLength + uBytes2);
		ckp->parentID  = SWAP_BE32 (kHFSRootFolderID);
		offset += SWAP_BE16 (ckp->keyLength) + 2;
	
		cfp = (HFSPlusCatalogFile *)((UInt8 *)buffer + offset);
		cfp->recordType     = SWAP_BE16 (kHFSPlusFileRecord);
		cfp->flags          = SWAP_BE16 (kHFSThreadExistsMask);
		cfp->fileID         = SWAP_BE32 (dp->nextFreeFileID + 1);
		cfp->createDate     = SWAP_BE32 (dp->createDate);
		cfp->contentModDate = SWAP_BE32 (dp->createDate);
		cfp->textEncoding   = 0;

		cfp->bsdInfo.fileMode     = SWAP_BE16 (S_IFREG);
		cfp->bsdInfo.ownerFlags   = SWAP_BE16 (UF_NODUMP);
		cfp->userInfo.fdType	  = SWAP_BE32 (kJournalFileType);
		cfp->userInfo.fdCreator	  = SWAP_BE32 (kHFSPlusCreator);
		cfp->userInfo.fdFlags     = SWAP_BE16 (kIsInvisible + kNameLocked);
		cfp->dataFork.logicalSize = SWAP_BE64(dp->blockSize);;
		cfp->dataFork.totalBlocks = SWAP_BE32(1);

		cfp->dataFork.extents[0].startBlock = SWAP_BE32 (header->journalInfoBlock);
		cfp->dataFork.extents[0].blockCount = cfp->dataFork.totalBlocks;

		offset += sizeof(HFSPlusCatalogFile);
		SETOFFSET(buffer, nodeSize, offset, ++index);
		nodename2 = &ckp->nodeName;

		/* Thread record for file #1 */
		tkp = (HFSPlusCatalogKey *)((UInt8 *)buffer + offset);
		tkp->keyLength = SWAP_BE16 (kHFSPlusCatalogKeyMinimumLength);
		tkp->parentID  = SWAP_BE32 (dp->nextFreeFileID);
		tkp->nodeName.length = 0;
		offset += SWAP_BE16 (tkp->keyLength) + 2;
	
		ctp = (HFSPlusCatalogThread *)((UInt8 *)buffer + offset);
		ctp->recordType = SWAP_BE16 (kHFSPlusFileThreadRecord);
		ctp->parentID   = SWAP_BE32 (kHFSRootFolderID);
		bcopy(nodename1, &ctp->nodeName, sizeof(UInt16) + uBytes1);
		offset += (sizeof(HFSPlusCatalogThread)
				- (sizeof(ctp->nodeName.unicode) - uBytes1) );
		SETOFFSET(buffer, nodeSize, offset, ++index);

		/* Thread record for file #2 */
		tkp = (HFSPlusCatalogKey *)((UInt8 *)buffer + offset);
		tkp->keyLength = SWAP_BE16 (kHFSPlusCatalogKeyMinimumLength);
		tkp->parentID  = SWAP_BE32 (dp->nextFreeFileID + 1);
		tkp->nodeName.length = 0;
		offset += SWAP_BE16 (tkp->keyLength) + 2;
	
		ctp = (HFSPlusCatalogThread *)((UInt8 *)buffer + offset);
		ctp->recordType = SWAP_BE16 (kHFSPlusFileThreadRecord);
		ctp->parentID   = SWAP_BE32 (kHFSRootFolderID);
		bcopy(nodename2, &ctp->nodeName, sizeof(UInt16) + uBytes2);
		offset += (sizeof(HFSPlusCatalogThread)
				- (sizeof(ctp->nodeName.unicode) - uBytes2) );
		SETOFFSET(buffer, nodeSize, offset, ++index);
	}
}


static void
InitFirstCatalogLeaf(const hfsparams_t *dp, void * buffer, int wrapper)
{
	BTNodeDescriptor	*ndp;
	HFSCatalogKey		*ckp;
	HFSCatalogKey		*tkp;
	HFSCatalogFolder	*cdp;
	HFSCatalogFile		*cfp;
	HFSCatalogThread	*ctp;
	UInt16			nodeSize;
	SInt16			offset;
	UInt32			timeStamp;

	nodeSize = dp->catalogNodeSize;
	timeStamp = UTCToLocal(dp->createDate);
	bzero(buffer, nodeSize);

	/*
	 * All nodes have a node descriptor...
	 */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind			= kBTLeafNode;
	ndp->numRecords		= SWAP_BE16 (2);
	ndp->height			= 1;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);

	/*
	 * First record is always the root directory...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->nodeName[0]	= strlen((char *)dp->volumeName);
	bcopy(dp->volumeName, &ckp->nodeName[1], ckp->nodeName[0]);
	ckp->keyLength		= 1 + 4 + ((ckp->nodeName[0] + 2) & 0xFE);  /* pad to word */
	ckp->parentID		= SWAP_BE32 (kHFSRootParentID);
	offset += ckp->keyLength + 1;

	cdp = (HFSCatalogFolder *)((UInt8 *)buffer + offset);
	cdp->recordType		= SWAP_BE16 (kHFSFolderRecord);
	if (wrapper)
		cdp->valence	= SWAP_BE16 (SWAP_BE16 (cdp->valence) + kWapperFileCount);
	cdp->folderID		= SWAP_BE32 (kHFSRootFolderID);
	cdp->createDate		= SWAP_BE32 (timeStamp);
	cdp->modifyDate		= SWAP_BE32 (timeStamp);
	offset += sizeof(HFSCatalogFolder);

	SETOFFSET(buffer, nodeSize, offset, 2);

	/*
	 * Second record is always the root directory thread...
	 */
	tkp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	tkp->keyLength		= kHFSCatalogKeyMinimumLength;
	tkp->parentID		= SWAP_BE32 (kHFSRootFolderID);
	// tkp->nodeName[0] = 0;
	offset += tkp->keyLength + 2;

	ctp = (HFSCatalogThread *)((UInt8 *)buffer + offset);
	ctp->recordType		= SWAP_BE16 (kHFSFolderThreadRecord);
	ctp->parentID		= SWAP_BE32 (kHFSRootParentID);
	bcopy(ckp->nodeName, ctp->nodeName, ckp->nodeName[0]+1);
	offset += sizeof(HFSCatalogThread);

	SETOFFSET(buffer, nodeSize, offset, 3);
	
	/*
	 * For Wrapper volumes there are more file records...
	 */
	if (wrapper) {
		ndp->fLink				= SWAP_BE32 (3);
		ndp->numRecords			= SWAP_BE16 (SWAP_BE16 (ndp->numRecords) + 2);

		/*
		 * Add "Desktop DB" file...
		 */
		ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
		ckp->keyLength			= 1 + 4 + ((kDTDB_Chars + 2) & 0xFE);  /* pad to word */
		ckp->parentID			= SWAP_BE32 (kHFSRootFolderID);
		ckp->nodeName[0]		= kDTDB_Chars;
		bcopy(kDTDB_Name, &ckp->nodeName[1], kDTDB_Chars);
		offset += ckp->keyLength + 1;

		cfp = (HFSCatalogFile *)((UInt8 *)buffer + offset);
		cfp->recordType			= SWAP_BE16 (kHFSFileRecord);
		cfp->userInfo.fdType	= SWAP_BE32 (kDTDB_Type);
		cfp->userInfo.fdCreator	= SWAP_BE32 (kDTDB_Creator);
		cfp->userInfo.fdFlags	= SWAP_BE16 (kIsInvisible);
		cfp->fileID				= SWAP_BE32 (kDTDB_FileID);
		cfp->createDate			= SWAP_BE32 (timeStamp);
		cfp->modifyDate			= SWAP_BE32 (timeStamp);
		cfp->dataExtents[0].startBlock = SWAP_BE16 (gDTDBFork.startBlock);
		cfp->dataExtents[0].blockCount = SWAP_BE16 (gDTDBFork.blockCount);
		cfp->dataPhysicalSize	= SWAP_BE32 (gDTDBFork.physicalSize);
		cfp->dataLogicalSize	= SWAP_BE32 (gDTDBFork.logicalSize);
		offset += sizeof(HFSCatalogFile);

		SETOFFSET(buffer, nodeSize, offset, 4);

		/*
		 * Add empty "Desktop DF" file...
		 */
		ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
		ckp->keyLength			= 1 + 4 + ((kDTDF_Chars + 2) & 0xFE);  /* pad to word */
		ckp->parentID			= SWAP_BE32 (kHFSRootFolderID);
		ckp->nodeName[0]		= kDTDF_Chars;
		bcopy(kDTDF_Name, &ckp->nodeName[1], kDTDF_Chars);
		offset += ckp->keyLength + 1;

		cfp = (HFSCatalogFile *)((UInt8 *)buffer + offset);
		cfp->recordType			= SWAP_BE16 (kHFSFileRecord);
		cfp->userInfo.fdType	= SWAP_BE32 (kDTDF_Type);
		cfp->userInfo.fdCreator	= SWAP_BE32 (kDTDF_Creator);
		cfp->userInfo.fdFlags	= SWAP_BE16 (kIsInvisible);
		cfp->fileID				= SWAP_BE32 (kDTDF_FileID);
		cfp->createDate			= SWAP_BE32 (timeStamp);
		cfp->modifyDate			= SWAP_BE32 (timeStamp);
		offset += sizeof(HFSCatalogFile);

		SETOFFSET(buffer, nodeSize, offset, 5);
	}
}


static void
InitSecondCatalogLeaf(const hfsparams_t *dp, void * buffer)
{
	BTNodeDescriptor	*ndp;
	HFSCatalogKey		*ckp;
	HFSCatalogFile		*cfp;
	UInt16			nodeSize;
	SInt16			offset;
	UInt32			timeStamp;

	nodeSize = dp->catalogNodeSize;
	timeStamp = UTCToLocal(dp->createDate);
	bzero(buffer, nodeSize);

	/*
	 * All nodes have a node descriptor...
	 */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->bLink		= SWAP_BE32 (2);
	ndp->kind		= kBTLeafNode;
	ndp->numRecords	= SWAP_BE16 (3);
	ndp->height		= 1;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);

	/*
	 * Add "Finder" file...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->keyLength			= 1 + 4 + ((kFinder_Chars + 2) & 0xFE);  /* pad to word */
	ckp->parentID			= SWAP_BE32 (kHFSRootFolderID);
	ckp->nodeName[0]		= kFinder_Chars;
	bcopy(kFinder_Name, &ckp->nodeName[1], kFinder_Chars);
	offset += ckp->keyLength + 1;

	cfp = (HFSCatalogFile *)((UInt8 *)buffer + offset);
	cfp->recordType			= SWAP_BE16 (kHFSFileRecord);
	cfp->userInfo.fdType	= SWAP_BE32 (kFinder_Type);
	cfp->userInfo.fdCreator	= SWAP_BE32 (kFinder_Creator);
	cfp->userInfo.fdFlags	= SWAP_BE16 (kIsInvisible + kNameLocked + kHasBeenInited);
	cfp->fileID				= SWAP_BE32 (kFinder_FileID);
	cfp->createDate			= SWAP_BE32 (timeStamp);
	cfp->modifyDate			= SWAP_BE32 (timeStamp);
	offset += sizeof(HFSCatalogFile);

	SETOFFSET(buffer, nodeSize, offset, 2);

	/*
	 * Add "ReadMe" file...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->keyLength			= 1 + 4 + ((kReadMe_Chars + 2) & 0xFE);  /* pad to word */
	ckp->parentID			= SWAP_BE32 (kHFSRootFolderID);
	ckp->nodeName[0]		= kReadMe_Chars;
	bcopy(kReadMe_Name, &ckp->nodeName[1], kReadMe_Chars);
	offset += ckp->keyLength + 1;

	cfp = (HFSCatalogFile *)((UInt8 *)buffer + offset);
	cfp->recordType			= SWAP_BE16 (kHFSFileRecord);
	cfp->userInfo.fdType	= SWAP_BE32 (kReadMe_Type);
	cfp->userInfo.fdCreator	= SWAP_BE32 (kReadMe_Creator);
	cfp->fileID				= SWAP_BE32 (kReadMe_FileID);
	cfp->createDate			= SWAP_BE32 (timeStamp);
	cfp->modifyDate			= SWAP_BE32 (timeStamp);
	cfp->dataExtents[0].startBlock = SWAP_BE16 (gReadMeFork.startBlock);
	cfp->dataExtents[0].blockCount = SWAP_BE16 (gReadMeFork.blockCount);
	cfp->dataPhysicalSize	= SWAP_BE32 (gReadMeFork.physicalSize);
	cfp->dataLogicalSize	= SWAP_BE32 (gReadMeFork.logicalSize);
	offset += sizeof(HFSCatalogFile);

	SETOFFSET(buffer, nodeSize, offset, 3);

	/*
	 * Add "System" file...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->keyLength			= 1 + 4 + ((kSystem_Chars + 2) & 0xFE);  /* pad to word */
	ckp->parentID			= SWAP_BE32 (kHFSRootFolderID);
	ckp->nodeName[0]		= kSystem_Chars;
	bcopy(kSystem_Name, &ckp->nodeName[1], kSystem_Chars);
	offset += ckp->keyLength + 1;

	cfp = (HFSCatalogFile *)((UInt8 *)buffer + offset);
	cfp->recordType			= SWAP_BE16 (kHFSFileRecord);
	cfp->userInfo.fdType	= SWAP_BE32 (kSystem_Type);
	cfp->userInfo.fdCreator	= SWAP_BE32 (kSystem_Creator);
	cfp->userInfo.fdFlags	= SWAP_BE16 (kIsInvisible + kNameLocked + kHasBeenInited);
	cfp->fileID				= SWAP_BE32 (kSystem_FileID);
	cfp->createDate			= SWAP_BE32 (timeStamp);
	cfp->modifyDate			= SWAP_BE32 (timeStamp);
	cfp->rsrcExtents[0].startBlock = SWAP_BE16 (gSystemFork.startBlock);
	cfp->rsrcExtents[0].blockCount = SWAP_BE16 (gSystemFork.blockCount);
	cfp->rsrcPhysicalSize	= SWAP_BE32 (gSystemFork.physicalSize);
	cfp->rsrcLogicalSize	= SWAP_BE32 (gSystemFork.logicalSize);
	offset += sizeof(HFSCatalogFile);

	SETOFFSET(buffer, nodeSize, offset, 4);
}


static void
InitCatalogRoot_HFS(const hfsparams_t *dp, void * buffer)
{
	BTNodeDescriptor	*ndp;
	HFSCatalogKey		*ckp;
	UInt32				*prp;	/* pointer record */
	UInt16				nodeSize;
	SInt16				offset;

	nodeSize = dp->catalogNodeSize;
	bzero(buffer, nodeSize);

	/*
	 * All nodes have a node descriptor...
	 */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind		= kBTIndexNode;
	ndp->numRecords	= SWAP_BE16 (2);
	ndp->height		= 2;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);

	/*
	 * Add root directory index...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->keyLength		= kHFSCatalogKeyMaximumLength;
	ckp->parentID		= SWAP_BE32 (kHFSRootParentID);
	ckp->nodeName[0]	= strlen((char *)dp->volumeName);
	bcopy(dp->volumeName, &ckp->nodeName[1], ckp->nodeName[0]);
	offset += ckp->keyLength + 1;

	prp = (UInt32 *)((UInt8 *)buffer + offset);
	*prp				= SWAP_BE32 (2);	/* point to first leaf node */
	offset += sizeof(UInt32);

	SETOFFSET(buffer, nodeSize, offset, 2);

	/*
	 * Add finder file index...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->keyLength		= kHFSCatalogKeyMaximumLength;
	ckp->parentID		= SWAP_BE32 (kHFSRootFolderID);
	ckp->nodeName[0]	= kFinder_Chars;
	bcopy(kFinder_Name, &ckp->nodeName[1], kFinder_Chars);
	offset += ckp->keyLength + 1;

	prp = (UInt32 *)((UInt8 *)buffer + offset);
	*prp				= SWAP_BE32 (3);	/* point to last leaf node */
	offset += sizeof(UInt32);

	SETOFFSET(buffer, nodeSize, offset, 3);
}


static void
WriteDesktopDB(const hfsparams_t *dp, const DriveInfo *driveInfo,
               UInt64 startingSector, void *buffer, UInt32 *mapNodes)
{
	BTNodeDescriptor *ndp;
	BTHeaderRec	*bthp;
	UInt8		*bmp;
	UInt32		nodeBitsInHeader;
	UInt32		fileSize;
	UInt32		nodeSize;
	UInt32		temp;
	SInt16		offset;
	UInt8		*keyDiscP;

	*mapNodes = 0;
	fileSize = gDTDBFork.logicalSize;
	nodeSize = kHFSNodeSize;

	bzero(buffer, nodeSize);

	/* FILL IN THE NODE DESCRIPTOR:  */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind		= kBTHeaderNode;
	ndp->numRecords	= SWAP_BE16 (3);
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);

	/* FILL IN THE HEADER RECORD:  */
	bthp = (BTHeaderRec *)((UInt8 *)buffer + offset);
    //	bthp->treeDepth		= 0;
    //	bthp->rootNode		= 0;
    //	bthp->firstLeafNode	= 0;
    //	bthp->lastLeafNode	= 0;
    //	bthp->leafRecords	= 0;
	bthp->nodeSize		= SWAP_BE16 (nodeSize);
	bthp->maxKeyLength	= SWAP_BE16 (37);
	bthp->totalNodes	= SWAP_BE32 (fileSize / nodeSize);
	bthp->freeNodes		= SWAP_BE32 (SWAP_BE32 (bthp->totalNodes) - 1);  /* header */
	bthp->clumpSize		= SWAP_BE32 (fileSize);
	bthp->btreeType		= 0xFF;
	offset += sizeof(BTHeaderRec);

	SETOFFSET(buffer, nodeSize, offset, 2);
	
	keyDiscP = (UInt8 *)((UInt8 *)buffer + offset);
	*keyDiscP++ = 2;			/* length of descriptor */
	*keyDiscP++ = KD_USEPROC;	/* always uses a compare proc */
	*keyDiscP++ = 1;			/* just one of them */
	offset += kBTreeHeaderUserBytes;

	SETOFFSET(buffer, nodeSize, offset, 3);

	/* FIGURE OUT HOW MANY MAP NODES (IF ANY):  */
	nodeBitsInHeader = 8 * (nodeSize
					- sizeof(BTNodeDescriptor)
					- sizeof(BTHeaderRec)
					- kBTreeHeaderUserBytes
					- (4 * sizeof(SInt16)) );

	if (SWAP_BE32 (bthp->totalNodes) > nodeBitsInHeader) {
		UInt32	nodeBitsInMapNode;
		
		ndp->fLink = SWAP_BE32 (SWAP_BE32 (bthp->lastLeafNode) + 1);
		nodeBitsInMapNode = 8 * (nodeSize
						- sizeof(BTNodeDescriptor)
						- (2 * sizeof(SInt16))
						- 2 );
		*mapNodes = (SWAP_BE32 (bthp->totalNodes) - nodeBitsInHeader +
		            (nodeBitsInMapNode - 1)) / nodeBitsInMapNode;
		bthp->freeNodes = SWAP_BE32 (SWAP_BE32 (bthp->freeNodes) - *mapNodes);
	}

	/* 
	 * FILL IN THE MAP RECORD, MARKING NODES THAT ARE IN USE.
	 * Note - worst case (32MB alloc blk) will have only 18 nodes in use.
	 */
	bmp = ((UInt8 *)buffer + offset);
	temp = SWAP_BE32 (bthp->totalNodes) - SWAP_BE32 (bthp->freeNodes);

	/* Working a byte at a time is endian safe */
	while (temp >= 8) { *bmp = 0xFF; temp -= 8; bmp++; }
	*bmp = ~(0xFF >> temp);
	offset += nodeBitsInHeader/8;

	SETOFFSET(buffer, nodeSize, offset, 4);

	WriteBuffer(driveInfo, startingSector, kHFSNodeSize, buffer);
}


static void
WriteSystemFile(const DriveInfo *dip, UInt64 startingSector, UInt32 *filesize)
{
	int fd;
	ssize_t	datasize, writesize;
	UInt8 *buf;
	struct stat stbuf;
	
	if (stat(HFS_BOOT_DATA, &stbuf) < 0)
		err(1, "stat %s", HFS_BOOT_DATA);
	
	if (stbuf.st_size > (64 * 1024))
		errx(1, "hfsbootdata file too big.");

	datasize = stbuf.st_size;
	writesize = ROUNDUP(datasize, dip->sectorSize);

	if ((buf = malloc(writesize)) == NULL)
		err(1, NULL);
	
	if ((fd = open(HFS_BOOT_DATA, O_RDONLY, 0)) < 0)
		err(1, "open %s", HFS_BOOT_DATA);
	
	if (read(fd, buf, datasize) != datasize) {
		if (errno)
			err(1, "read %s", HFS_BOOT_DATA);
		else
			errx(1, "problems reading %s", HFS_BOOT_DATA);
	}

	if (writesize > datasize)
		bzero(buf + datasize, writesize - datasize);

	WriteBuffer(dip, startingSector, writesize, buf);
	
	close(fd);
	
	free(buf);
	
	*filesize = datasize;
}


static void
WriteReadMeFile(const DriveInfo *dip, UInt64 startingSector, UInt32 *filesize)
{
	ssize_t	datasize, writesize;
	UInt8 *buf;
	
	datasize = sizeof(hfswrap_readme);
	writesize = ROUNDUP(datasize, dip->sectorSize);

	if ((buf = malloc(writesize)) == NULL)
		err(1, NULL);
	
	bcopy(hfswrap_readme, buf, datasize);
	if (writesize > datasize)
		bzero(buf + datasize, writesize - datasize);
	
	WriteBuffer(dip, startingSector, writesize, buf);
	
	*filesize = datasize;
}


/*
 * WriteMapNodes
 *	
 * Initializes a B-tree map node and writes it out to disk.
 */
static void
WriteMapNodes(const DriveInfo *driveInfo, UInt64 diskStart, UInt32 firstMapNode,
	UInt32 mapNodes, UInt16 btNodeSize, void *buffer)
{
	UInt32	sectorsPerNode;
	UInt32	mapRecordBytes;
	UInt16	i;
	BTNodeDescriptor *nd = (BTNodeDescriptor *)buffer;

	bzero(buffer, btNodeSize);

	nd->kind		= kBTMapNode;
	nd->numRecords	= SWAP_BE16 (1);
	
	/* note: must belong word aligned (hence the extra -2) */
	mapRecordBytes = btNodeSize - sizeof(BTNodeDescriptor) - 2*sizeof(SInt16) - 2;  

	SETOFFSET(buffer, btNodeSize, sizeof(BTNodeDescriptor), 1);
	SETOFFSET(buffer, btNodeSize, sizeof(BTNodeDescriptor) + mapRecordBytes, 2);
	
	sectorsPerNode = btNodeSize/kBytesPerSector;
	
	/*
	 * Note - worst case (32MB alloc blk) will have
	 * only 18 map nodes. So don't bother optimizing
	 * this section to do multiblock writes!
	 */
	for (i = 0; i < mapNodes; i++) {
		if ((i + 1) < mapNodes)
			nd->fLink = SWAP_BE32 (++firstMapNode);  /* point to next map node */
		else
			nd->fLink = 0;  /* this is the last map node */

		WriteBuffer(driveInfo, diskStart, btNodeSize, buffer);
			
		diskStart += sectorsPerNode;
	}
}

/*
 * @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 * NOTE: IF buffer IS NULL, THIS FUNCTION WILL WRITE ZERO'S.
 *
 * startingSector is in terms of 512-byte sectors.
 * @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 */
static void
WriteBuffer(const DriveInfo *driveInfo, UInt64 startingSector, UInt64 byteCount,
	const void *buffer)
{
	off_t sector;
	off_t physSector = 0;
	off_t byteOffsetInPhysSector;
	UInt32 numBytesToIO;
	UInt32 numPhysSectorsToIO;
	UInt32 tempbufSizeInPhysSectors;
	UInt32 tempbufSize;
	UInt32 fd = driveInfo->fd;
	UInt32 physSectorSize = driveInfo->physSectorSize;
	void *tempbuf = NULL;
	int sectorSizeRatio = driveInfo->physSectorSize / kBytesPerSector;
	int status = 0; /* 0: no error; 1: alloc; 2: read; 3: write */

	if (0 == byteCount) {
		goto exit;
	}

	/*@@@@@@@@@@ buffer allocation @@@@@@@@@@*/
	/* try a buffer size for optimal IO, __UP TO 4MB__. if that
	   fails, then try with the minimum allowed buffer size, which
	   is equal to physSectorSize */
	tempbufSizeInPhysSectors = MIN ( (byteCount - 1 + physSectorSize) / physSectorSize,
					     driveInfo->physSectorsPerIO );
	/* limit at 4MB */
	tempbufSizeInPhysSectors = MIN ( tempbufSizeInPhysSectors, (4 * 1024 * 1024) / physSectorSize );
	tempbufSize = tempbufSizeInPhysSectors * physSectorSize;

	if ((tempbuf = valloc(tempbufSize)) == NULL) {
		/* try allocation of smallest allowed size: one
		   physical sector.
		   NOTE: the previous valloc tempbufSize might have
		   already been one physical sector. we don't want to
		   check if that was the case, so just try again.
		*/
		tempbufSizeInPhysSectors = 1;
		tempbufSize = physSectorSize;
		if ((tempbuf = valloc(tempbufSize)) == NULL) {
			status = 1;
			goto exit;
		}
	}

	/*@@@@@@@@@@ io @@@@@@@@@@*/
	sector = driveInfo->sectorOffset + startingSector;
	physSector = sector / sectorSizeRatio;
	byteOffsetInPhysSector =  (sector % sectorSizeRatio) * kBytesPerSector;

	while (byteCount > 0) {
		numPhysSectorsToIO = MIN ( (byteCount - 1 + physSectorSize) / physSectorSize,
				       tempbufSizeInPhysSectors );
		numBytesToIO = MIN(byteCount, (numPhysSectorsToIO * physSectorSize) - byteOffsetInPhysSector);

		/* if IO does not align with physical sector boundaries */
		if ((0 != byteOffsetInPhysSector) || ((numBytesToIO % physSectorSize) != 0)) {
			if (pread(fd, tempbuf, numPhysSectorsToIO * physSectorSize, physSector * physSectorSize) < 0) {
				status = 2;
				goto exit;
			}
		}

		if (NULL != buffer) {
			memcpy(tempbuf + byteOffsetInPhysSector, buffer, numBytesToIO);
		}
		else {
			bzero(tempbuf + byteOffsetInPhysSector, numBytesToIO);
		}

		if (pwrite(fd, tempbuf, numPhysSectorsToIO * physSectorSize, physSector * physSectorSize) < 0) {
			status = 3;
			goto exit;
		}

		byteOffsetInPhysSector = 0;
		byteCount -= numBytesToIO;
		physSector += numPhysSectorsToIO;
		if (NULL != buffer) {
			buffer += numBytesToIO;
		}
	}

exit:
	if (tempbuf) {
		free(tempbuf);
		tempbuf = NULL;
	}

	if (1 == status) {
		err(1, NULL);
	}
	else if (2 == status) {
		err(1, "read (sector %llu)", physSector);
	}
	else if (3 == status) {
		err(1, "write (sector %llu)", physSector);
	}

	return;
}


static UInt32 Largest( UInt32 a, UInt32 b, UInt32 c, UInt32 d )
{
	/* a := max(a,b) */
	if (a < b)
		a = b;
	/* c := max(c,d) */
	if (c < d)
		c = d;
	
	/* return max(a,c) */
	if (a > c)
		return a;
	else
		return c;
}


/*
 * MarkBitInAllocationBuffer
 * 	
 * Given a buffer and allocation block, will mark off the corresponding
 * bitmap bit, and return the sector number the block belongs in.
 */
static void MarkBitInAllocationBuffer( HFSPlusVolumeHeader *header,
	UInt32 allocationBlock, void* sectorBuffer, UInt64 *sector )
{

	UInt8 *byteP;
	UInt8 mask;
	UInt32 sectorsPerBlock;
	UInt16 bitInSector = allocationBlock % kBitsPerSector;
	UInt16 bitPosition = allocationBlock % 8;
	
	sectorsPerBlock = header->blockSize / kBytesPerSector;

	*sector = (header->allocationFile.extents[0].startBlock * sectorsPerBlock) +
		  (allocationBlock / kBitsPerSector);
	
	byteP = (UInt8 *)sectorBuffer + (bitInSector >> 3);
	mask = ( 0x80 >> bitPosition );
	*byteP |= mask;
}


/*
 * UTCToLocal - convert from Mac OS GMT time to Mac OS local time
 */
static UInt32 UTCToLocal(UInt32 utcTime)
{
	UInt32 localTime = utcTime;
	struct timezone timeZone;
	struct timeval	timeVal;
	
	if (localTime != 0) {

                /* HFS volumes need timezone info to convert local to GMT */
                (void)gettimeofday( &timeVal, &timeZone );


		localTime -= (timeZone.tz_minuteswest * 60);
		if (timeZone.tz_dsttime)
			localTime += 3600;
	}

        return (localTime);
}


static UInt32
DivideAndRoundUp(UInt32 numerator, UInt32 denominator)
{
	UInt32	quotient;
	
	quotient = numerator / denominator;
	if (quotient * denominator != numerator)
		quotient++;
	
	return quotient;
}


#define __kCFUserEncodingFileName ("/.CFUserTextEncoding")

static UInt32
GetDefaultEncoding()
{
    struct passwd *passwdp;

    if ((passwdp = getpwuid(0))) { // root account
        char buffer[MAXPATHLEN + 1];
        int fd;

        strlcpy(buffer, passwdp->pw_dir, sizeof(buffer));
        strlcat(buffer, __kCFUserEncodingFileName, sizeof(buffer));

        if ((fd = open(buffer, O_RDONLY, 0)) > 0) {
            size_t readSize;

            readSize = read(fd, buffer, MAXPATHLEN);
            buffer[(readSize < 0 ? 0 : readSize)] = '\0';
            close(fd);
            return strtol(buffer, NULL, 0);
        }
    }
    return 0;
}


static int
ConvertUTF8toUnicode(const UInt8* source, size_t bufsize, UniChar* unibuf,
	UInt16 *charcount)
{
	UInt8 byte;
	UniChar* target;
	UniChar* targetEnd;

	*charcount = 0;
	target = unibuf;
	targetEnd = (UniChar *)((UInt8 *)unibuf + bufsize);

	while ((byte = *source++)) {
		
		/* check for single-byte ascii */
		if (byte < 128) {
			if (byte == ':')	/* ':' is mapped to '/' */
				byte = '/';

			*target++ = SWAP_BE16 (byte);
		} else {
			UniChar ch;
			UInt8 seq = (byte >> 4);

			switch (seq) {
			case 0xc: /* double-byte sequence (1100 and 1101) */
			case 0xd:
				ch = (byte & 0x1F) << 6;  /* get 5 bits */
				if (((byte = *source++) >> 6) != 2)
					return (EINVAL);
				break;

			case 0xe: /* triple-byte sequence (1110) */
				ch = (byte & 0x0F) << 6;  /* get 4 bits */
				if (((byte = *source++) >> 6) != 2)
					return (EINVAL);
				ch += (byte & 0x3F); ch <<= 6;  /* get 6 bits */
				if (((byte = *source++) >> 6) != 2)
					return (EINVAL);
				break;

			default:
				return (EINVAL);  /* malformed sequence */
			}

			ch += (byte & 0x3F);  /* get last 6 bits */

			if (target >= targetEnd)
				return (ENOBUFS);

			*target++ = SWAP_BE16 (ch);
		}
	}

	*charcount = target - unibuf;

	return (0);
}

/*
 * Derive the encoding hint for the given name.
 */
static int
getencodinghint(unsigned char *name)
{
        int mib[3];
        size_t buflen = sizeof(int);
        struct vfsconf vfc;
        int hint = 0;

        if (getvfsbyname("hfs", &vfc) < 0)
		goto error;

        mib[0] = CTL_VFS;
        mib[1] = vfc.vfc_typenum;
        mib[2] = HFS_ENCODINGHINT;
 
	if (sysctl(mib, 3, &hint, &buflen, name, strlen((char *)name) + 1) < 0)
 		goto error;
	return (hint);
error:
	hint = GetDefaultEncoding();
	return (hint);
}


/* Generate Volume UUID - similar to code existing in hfs_util */
void GenerateVolumeUUID(VolumeUUID *newVolumeID) {
	SHA_CTX context;
	char randomInputBuffer[26];
	unsigned char digest[20];
	time_t now;
	clock_t uptime;
	int mib[2];
	int sysdata;
	char sysctlstring[128];
	size_t datalen;
	double sysloadavg[3];
	struct vmtotal sysvmtotal;
	
	do {
		/* Initialize the SHA-1 context for processing: */
		SHA1_Init(&context);
		
		/* Now process successive bits of "random" input to seed the process: */
		
		/* The current system's uptime: */
		uptime = clock();
		SHA1_Update(&context, &uptime, sizeof(uptime));
		
		/* The kernel's boot time: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_BOOTTIME;
		datalen = sizeof(sysdata);
		sysctl(mib, 2, &sysdata, &datalen, NULL, 0);
		SHA1_Update(&context, &sysdata, datalen);
		
		/* The system's host id: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_HOSTID;
		datalen = sizeof(sysdata);
		sysctl(mib, 2, &sysdata, &datalen, NULL, 0);
		SHA1_Update(&context, &sysdata, datalen);

		/* The system's host name: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_HOSTNAME;
		datalen = sizeof(sysctlstring);
		sysctl(mib, 2, sysctlstring, &datalen, NULL, 0);
		SHA1_Update(&context, sysctlstring, datalen);

		/* The running kernel's OS release string: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_OSRELEASE;
		datalen = sizeof(sysctlstring);
		sysctl(mib, 2, sysctlstring, &datalen, NULL, 0);
		SHA1_Update(&context, sysctlstring, datalen);

		/* The running kernel's version string: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_VERSION;
		datalen = sizeof(sysctlstring);
		sysctl(mib, 2, sysctlstring, &datalen, NULL, 0);
		SHA1_Update(&context, sysctlstring, datalen);

		/* The system's load average: */
		datalen = sizeof(sysloadavg);
		getloadavg(sysloadavg, 3);
		SHA1_Update(&context, &sysloadavg, datalen);

		/* The system's VM statistics: */
		mib[0] = CTL_VM;
		mib[1] = VM_METER;
		datalen = sizeof(sysvmtotal);
		sysctl(mib, 2, &sysvmtotal, &datalen, NULL, 0);
		SHA1_Update(&context, &sysvmtotal, datalen);

		/* The current GMT (26 ASCII characters): */
		time(&now);
		strncpy(randomInputBuffer, asctime(gmtime(&now)), 26);	/* "Mon Mar 27 13:46:26 2000" */
		SHA1_Update(&context, randomInputBuffer, 26);
		
		/* Pad the accumulated input and extract the final digest hash: */
		SHA1_Final(digest, &context);
	
		memcpy(newVolumeID, digest, sizeof(*newVolumeID));
	} while ((newVolumeID->v.high == 0) || (newVolumeID->v.low == 0));
}


