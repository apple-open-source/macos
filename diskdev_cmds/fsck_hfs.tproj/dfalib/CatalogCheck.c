/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include "Scavenger.h"

/*
 * information collected when visiting catalog records
 */
struct CatalogIterationSummary {
	UInt32 parentID;
	UInt32 rootDirCount;   /* hfs only */
	UInt32 rootFileCount;  /* hfs only */
	UInt32 dirCount;
	UInt32 dirThreads;
	UInt32 fileCount;
	UInt32 filesWithThreads; /* hfs only */
	UInt32 fileThreads;
	UInt32 nextCNID;
	UInt64 encodings;
	void * hardLinkRef;
};

/* Globals used during Catalog record checks */
struct CatalogIterationSummary gCIS;

SGlobPtr gScavGlobals;

/* Local routines for checking catalog structures */
static int  CheckCatalogRecord(const HFSPlusCatalogKey *key,
                               const CatalogRecord *rec, UInt16 reclen);
static int  CheckCatalogRecord_HFS(const HFSCatalogKey *key,
                                   const CatalogRecord *rec, UInt16 reclen);

static int  CheckDirectory(const HFSPlusCatalogKey * key, const HFSPlusCatalogFolder * dir);
static int  CheckFile(const HFSPlusCatalogKey * key, const HFSPlusCatalogFile * file);
static int  CheckThread(const HFSPlusCatalogKey * key, const HFSPlusCatalogThread * thread);

static int  CheckDirectory_HFS(const HFSCatalogKey * key, const HFSCatalogFolder * dir);
static int  CheckFile_HFS(const HFSCatalogKey * key, const HFSCatalogFile * file);
static int  CheckThread_HFS(const HFSCatalogKey * key, const HFSCatalogThread * thread);

static void CheckBSDInfo(const HFSPlusCatalogKey * key, const HFSPlusBSDInfo * bsdInfo, int isdir);
static int  CheckCatalogName(u_int16_t charCount, const u_int16_t *uniChars,
                             u_int32_t parentID, Boolean thread);
static int  CheckCatalogName_HFS(u_int16_t charCount, const u_char *filename,
                                 u_int32_t parentID, Boolean thread);

static int  RecordBadAllocation(UInt32 parID, char * filename, UInt32 forkType, UInt32 oldBlkCnt, UInt32 newBlkCnt);
static int  RecordTruncation(UInt32 parID, char * filename, UInt32 forkType, UInt64 oldSize,  UInt64 newSize);

static int  CaptureMissingThread(UInt32 threadID, const HFSPlusCatalogKey *nextKey);

/*
 * CheckCatalogBTree - Verifies the catalog B-tree structure
 *
 * Causes CheckCatalogRecord to be called for every leaf record
 */
OSErr
CheckCatalogBTree( SGlobPtr GPtr )
{
	OSErr err;
	int hfsplus;
	
	gScavGlobals = GPtr;
	hfsplus = gScavGlobals->isHFSPlus;
	
	ClearMemory(&gCIS, sizeof(gCIS));
	gCIS.parentID = kHFSRootParentID;
	gCIS.nextCNID = kHFSFirstUserCatalogNodeID;

	if (hfsplus)
        	HardLinkCheckBegin(gScavGlobals, &gCIS.hardLinkRef);

	/* for compatibility, init these globals */
	gScavGlobals->TarID = kHFSCatalogFileID;
	gScavGlobals->TarBlock = gScavGlobals->idSector;

	/*
	 * Check out the BTree structure
	 */
	err = BTCheck(gScavGlobals, kCalculatedCatalogRefNum, (CheckLeafRecordProcPtr)CheckCatalogRecord);
	if (err) goto exit;

	if (gCIS.dirCount != gCIS.dirThreads)
		gScavGlobals->CBTStat |= S_Orphan;  /* a directory record is missing */

	if (hfsplus && (gCIS.fileCount != gCIS.fileThreads))
		gScavGlobals->CBTStat |= S_Orphan;

	if (!hfsplus && (gCIS.fileThreads != gCIS.filesWithThreads))
		gScavGlobals->CBTStat |= S_Orphan;

	gScavGlobals->calculatedVCB->vcbEncodingsBitmap = gCIS.encodings;
	gScavGlobals->calculatedVCB->vcbNextCatalogID = gCIS.nextCNID;
	gScavGlobals->calculatedVCB->vcbFolderCount = gCIS.dirCount - 1;
	gScavGlobals->calculatedVCB->vcbFileCount = gCIS.fileCount;
	if (!hfsplus) {
		gScavGlobals->calculatedVCB->vcbNmRtDirs = gCIS.rootDirCount;
		gScavGlobals->calculatedVCB->vcbNmFls = gCIS.rootFileCount;
	}

	/*
	 * Check out the allocation map structure
	 */
	err = BTMapChk(gScavGlobals, kCalculatedCatalogRefNum);
	if (err) goto exit;

	/*
	 * Compare BTree header record on disk with scavenger's BTree header record 
	 */
	err = CmpBTH(gScavGlobals, kCalculatedCatalogRefNum);
	if (err) goto exit;

	/*
	 * Compare BTree map on disk with scavenger's BTree map
	 */
	err = CmpBTM(gScavGlobals, kCalculatedCatalogRefNum);

	if (hfsplus)
		(void) CheckHardLinks(gCIS.hardLinkRef);

 exit:
	if (hfsplus)
		HardLinkCheckEnd(gCIS.hardLinkRef);

	return (err);
}

/*
 * CheckCatalogRecord - verify a catalog record
 *
 * Called in leaf-order for every leaf record in the Catalog B-tree
 */
static int
CheckCatalogRecord(const HFSPlusCatalogKey *key, const CatalogRecord *rec, UInt16 reclen)
{
	int result = 0;

	++gScavGlobals->itemsProcessed;

	if (!gScavGlobals->isHFSPlus)
		return CheckCatalogRecord_HFS((HFSCatalogKey *)key, rec, reclen);

	gScavGlobals->CNType = rec->recordType;

	switch (rec->recordType) {
	case kHFSPlusFolderRecord:
		++gCIS.dirCount;
		if (reclen != sizeof(HFSPlusCatalogFolder)){
			RcdError(gScavGlobals, E_LenDir);
			result = E_LenDir;
			break;
		}
		if (key->parentID != gCIS.parentID) {
			result = CaptureMissingThread(key->parentID, key);
			if (result) break;
			/* Pretend thread record was there */
			++gCIS.dirThreads;
			gCIS.parentID = key->parentID;
		}
		result = CheckDirectory(key, (HFSPlusCatalogFolder *)rec);
		break;

	case kHFSPlusFileRecord:
		++gCIS.fileCount;
		if (reclen != sizeof(HFSPlusCatalogFile)){
			RcdError(gScavGlobals, E_LenFil);
			result = E_LenFil;
			break;
		}
		if (key->parentID != gCIS.parentID) {
			result = CaptureMissingThread(key->parentID, key);
			if (result) break;
			/* Pretend thread record was there */
			++gCIS.dirThreads;
			gCIS.parentID = key->parentID;
		}
		result = CheckFile(key, (HFSPlusCatalogFile *)rec);
		break;

	case kHFSPlusFolderThreadRecord:
		++gCIS.dirThreads;
		gCIS.parentID = key->parentID;
		/* Fall through */

	case kHFSPlusFileThreadRecord:
		if (rec->recordType == kHFSPlusFileThreadRecord)
			++gCIS.fileThreads;

		if (reclen > sizeof(HFSPlusCatalogThread) ||
			reclen < sizeof(HFSPlusCatalogThread) - sizeof(HFSUniStr255)) {
			RcdError(gScavGlobals, E_LenThd);
			result = E_LenThd;
			break;
		} else if (reclen == sizeof(HFSPlusCatalogThread)) {
			gScavGlobals->VeryMinorErrorsStat |= S_BloatedThreadRecordFound;
		}
		result = CheckThread(key, (HFSPlusCatalogThread *)rec);
		break;

	default:
		RcdError(gScavGlobals, E_CatRec);
		result = E_CatRec;
	}
	
	return (result);
}

/*
 * CheckCatalogRecord_HFS - verify an HFS catalog record
 *
 * Called in leaf-order for every leaf record in the Catalog B-tree
 */
static int
CheckCatalogRecord_HFS(const HFSCatalogKey *key, const CatalogRecord *rec, UInt16 reclen)
{
	int result = 0;

	gScavGlobals->CNType = rec->recordType;

	switch (rec->recordType) {
	case kHFSFolderRecord:
		++gCIS.dirCount;
		if (key->parentID == kHFSRootFolderID )
			++gCIS.rootDirCount;
		if (reclen != sizeof(HFSCatalogFolder)){
			RcdError(gScavGlobals, E_LenDir);
			result = E_LenDir;
			break;
		}
		if (key->parentID != gCIS.parentID) {
			result = CaptureMissingThread(key->parentID, (HFSPlusCatalogKey *)key);
			if (result) break;
			/* Pretend thread record was there */
			++gCIS.dirThreads;
			gCIS.parentID = key->parentID;
		}
		result = CheckDirectory_HFS(key, (HFSCatalogFolder *)rec);
		break;

	case kHFSFileRecord:
		++gCIS.fileCount;
		if (key->parentID == kHFSRootFolderID )
			++gCIS.rootFileCount;
		if (reclen != sizeof(HFSCatalogFile)){
			RcdError(gScavGlobals, E_LenFil);
			result = E_LenFil;
			break;
		}
		if (key->parentID != gCIS.parentID) {
			result = CaptureMissingThread(key->parentID, (HFSPlusCatalogKey *)key);
			if (result) break;
			/* Pretend thread record was there */
			++gCIS.dirThreads;
			gCIS.parentID = key->parentID;
		}
		result = CheckFile_HFS(key, (HFSCatalogFile *)rec);
		break;

	case kHFSFolderThreadRecord:
		++gCIS.dirThreads;
		gCIS.parentID = key->parentID;
		/* Fall through */
	case kHFSFileThreadRecord:
		if (rec->recordType == kHFSFileThreadRecord)
			++gCIS.fileThreads;

		if (reclen != sizeof(HFSCatalogThread)) {
			RcdError(gScavGlobals, E_LenThd);
			result = E_LenThd;
			break;
		}
		result = CheckThread_HFS(key, (HFSCatalogThread *)rec);
		break;


	default:
		RcdError(gScavGlobals, E_CatRec);
		result = E_CatRec;
	}
	
	return (result);
}

/*
 * CheckDirectory - verify a catalog directory record
 *
 * Also collects info for later processing.
 * Called in leaf-order for every directory record in the Catalog B-tree
 */
static int 
CheckDirectory(const HFSPlusCatalogKey * key, const HFSPlusCatalogFolder * dir)
{
	UInt32 dirID;
	int result = 0;

	dirID = dir->folderID;

	if (dirID == kHFSRootFolderID) {
		size_t len;

		(void) utf_encodestr(key->nodeName.unicode,
					key->nodeName.length * 2,
					gScavGlobals->volumeName, &len);
		gScavGlobals->volumeName[len] = '\0';
	}

	if (dir->flags != 0) {
		RcdError(gScavGlobals, E_CatalogFlagsNotZero);
		gScavGlobals->CBTStat |= S_ReservedNotZero;
	}

	if (dirID < kHFSFirstUserCatalogNodeID  &&
            dirID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}
	if (dirID >= gCIS.nextCNID )
		gCIS.nextCNID = dirID + 1;

	gCIS.encodings |= (1 << MapEncodingToIndex(dir->textEncoding & 0x7F));

        CheckBSDInfo(key, &dir->bsdInfo, true);
	
	CheckCatalogName(key->nodeName.length, &key->nodeName.unicode[0], key->parentID, false);
	
	return (result);
}

/*
 * CheckFile - verify a HFS+ catalog file record
 * - sanity check values
 * - collect info for later processing
 *
 * Called in leaf-order for every file record in the Catalog B-tree
 */
static int
CheckFile(const HFSPlusCatalogKey * key, const HFSPlusCatalogFile * file)
{
	UInt32 fileID;
	UInt32 blocks;
	UInt64 bytes;
	int result = 0;
	size_t	len;
	char filename[256 * 3];

	(void) utf_encodestr(key->nodeName.unicode,
				key->nodeName.length * 2,
				filename, &len);
	filename[len] = '\0';

	/* Check reserved fields
	 *
	 * NOTE: the bit 7 (mask 0x80) of the flags byte isn't used by HFS or HFS Plus.
	 * It was used by MFS to indicate that a file record was in use.  However, Inside
	 * Macintosh: Files documents this bit for HFS volumes, and some non-Mac implementations
	 * appear to set the bit.  Therefore, we ignore it.
	 */
	if ((file->flags & (UInt16) ~(0X83)) != 0) {
		RcdError(gScavGlobals, E_CatalogFlagsNotZero);
		gScavGlobals->CBTStat |= S_ReservedNotZero;
	}
	fileID = file->fileID;
	if (fileID < kHFSFirstUserCatalogNodeID) {
		RcdError(gScavGlobals, E_InvalidID);
		result = E_InvalidID;
		return (result);
	}
	if (fileID >= gCIS.nextCNID )
		gCIS.nextCNID = fileID + 1;

	gCIS.encodings |= (1 << MapEncodingToIndex(file->textEncoding & 0x7F));

        CheckBSDInfo(key, &file->bsdInfo, false);

	/* check out data fork extent info */
	result = CheckFileExtents(gScavGlobals, file->fileID, 0,
                                file->dataFork.extents, &blocks);
	if (result != noErr)
		return (result);

	if (file->dataFork.totalBlocks != blocks) {
		result = RecordBadAllocation(key->parentID, filename, kDataFork,
					file->dataFork.totalBlocks, blocks);
		if (result)
			return (result);
	} else {
		bytes = (UInt64)blocks * (UInt64)gScavGlobals->calculatedVCB->vcbBlockSize;
		if (file->dataFork.logicalSize > bytes) {
			result = RecordTruncation(key->parentID, filename, kDataFork,
					file->dataFork.logicalSize, bytes);
			if (result)
				return (result);
		}
	}
	/* check out resource fork extent info */
	result = CheckFileExtents(gScavGlobals, file->fileID, 0xFF,
	                          file->resourceFork.extents, &blocks);
	if (result != noErr)
		return (result);

	if (file->resourceFork.totalBlocks != blocks) {
		result = RecordBadAllocation(key->parentID, filename, kRsrcFork,
					file->resourceFork.totalBlocks, blocks);
		if (result)
			return (result);
	} else {
		bytes = (UInt64)blocks * (UInt64)gScavGlobals->calculatedVCB->vcbBlockSize;
		if (file->resourceFork.logicalSize > bytes) {
			result = RecordTruncation(key->parentID, filename, kRsrcFork,
					file->resourceFork.logicalSize, bytes);
			if (result)
				return (result);
		}
	}

	/* Collect indirect link info for later */
	if (file->userInfo.fdType == kHardLinkFileType  &&
            file->userInfo.fdCreator == kHFSPlusCreator)
		CaptureHardLink(gCIS.hardLinkRef, file->bsdInfo.special.iNodeNum,  file->fileID);

	CheckCatalogName(key->nodeName.length, &key->nodeName.unicode[0], key->parentID, false);

	return (result);
}

/*
 * CheckThread - verify a catalog thread
 *
 * Called in leaf-order for every thread record in the Catalog B-tree
 */
static int 
CheckThread(const HFSPlusCatalogKey * key, const HFSPlusCatalogThread * thread)
{
	int result = 0;

	if (key->nodeName.length != 0) {
		RcdError(gScavGlobals, E_ThdKey);
		return (E_ThdKey);
	}

	result = CheckCatalogName(thread->nodeName.length, &thread->nodeName.unicode[0],
                         thread->parentID, true);
	if (result != noErr) {
		RcdError(gScavGlobals, E_ThdCN);
		return (E_ThdCN);
	}	

	if (key->parentID < kHFSFirstUserCatalogNodeID  &&
            key->parentID != kHFSRootParentID  &&
            key->parentID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}

	if (thread->parentID == kHFSRootParentID) {
		if (key->parentID != kHFSRootFolderID) {
			RcdError(gScavGlobals, E_InvalidID);
			return (E_InvalidID);
		}
	} else if (thread->parentID < kHFSFirstUserCatalogNodeID &&
	           thread->parentID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}

	return (0);
}

/*
 * CheckDirectory - verify an HFS catalog directory record
 *
 * Also collects info for later processing.
 * Called in leaf-order for every directory record in the Catalog B-tree
 */
static int 
CheckDirectory_HFS(const HFSCatalogKey * key, const HFSCatalogFolder * dir)
{
	UInt32 dirID;
	int result = 0;

	dirID = dir->folderID;

	if (dirID == kHFSRootFolderID) {
		bcopy(&key->nodeName[1], gScavGlobals->volumeName, key->nodeName[0]);
		gScavGlobals->volumeName[key->nodeName[0]] = '\0';
	}

	if (dir->flags != 0) {
		RcdError(gScavGlobals, E_CatalogFlagsNotZero);
		gScavGlobals->CBTStat |= S_ReservedNotZero;
	}

	if (dirID < kHFSFirstUserCatalogNodeID  &&
            dirID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}
	if (dirID >= gCIS.nextCNID )
		gCIS.nextCNID = dirID + 1;
	
	CheckCatalogName_HFS(key->nodeName[0], &key->nodeName[1], key->parentID, false);
	
	return (result);
}

/*
 * CheckFile_HFS - verify a HFS catalog file record
 * - sanity check values
 * - collect info for later processing
 *
 * Called in b-tree leaf order for every HFS file
 * record in the Catalog B-tree.
 */
static int
CheckFile_HFS(const HFSCatalogKey * key, const HFSCatalogFile * file)
{
	UInt32 fileID;
	UInt32 blocks;
	int result = 0;

	if (file->flags & kHFSThreadExistsMask)
		++gCIS.filesWithThreads;

	/* Check reserved fields
	 *
	 * NOTE: the bit 7 (mask 0x80) of the flags byte isn't used
	 * by HFS It was used by MFS to indicate that a file record
	 * was in use.  However, Inside Macintosh: Files documents
	 * this bit for HFS volumes, and some non-Mac implementations
	 * appear to set the bit.  Therefore, we ignore it.
	 */
	if ((file->flags & (UInt8) ~(0X83)) ||
	    (file->dataStartBlock)          ||
	    (file->rsrcStartBlock)          ||
	    (file->reserved))
	{
		RcdError(gScavGlobals, E_CatalogFlagsNotZero);
		gScavGlobals->CBTStat |= S_ReservedNotZero;
	}

	fileID = file->fileID;
	if (fileID < kHFSFirstUserCatalogNodeID) {
		RcdError(gScavGlobals, E_InvalidID);
		result = E_InvalidID;
		return (result);
	}
	if (fileID >= gCIS.nextCNID )
		gCIS.nextCNID = fileID + 1;

	/* check out data fork extent info */
	result = CheckFileExtents(gScavGlobals, file->fileID, 0,
                                file->dataExtents, &blocks);
	if (result != noErr)
		return (result);
	if (file->dataPhysicalSize > (blocks * gScavGlobals->calculatedVCB->vcbBlockSize)) {
		PrintError(gScavGlobals, E_PEOF, 1, "");
		return (noErr);		/* we don't fix this, ignore the error */
	}
	if (file->dataLogicalSize > file->dataPhysicalSize) {
		PrintError(gScavGlobals, E_LEOF, 1, "");
                return (noErr);		/* we don't fix this, ignore the error */
	}

	/* check out resource fork extent info */
	result = CheckFileExtents(gScavGlobals, file->fileID, 0xFF,
				file->rsrcExtents, &blocks);
	if (result != noErr)
		return (result);
	if (file->rsrcPhysicalSize > (blocks * gScavGlobals->calculatedVCB->vcbBlockSize)) {
		PrintError(gScavGlobals, E_PEOF, 1, "");
                return (noErr);		/* we don't fix this, ignore the error */
	}
	if (file->rsrcLogicalSize > file->rsrcPhysicalSize) {
		PrintError(gScavGlobals, E_LEOF, 1, "");
                return (noErr);		/* we don't fix this, ignore the error */
	}
#if 1
	/* Keeping handle in globals of file ID's for HFS volume only */
	if (PtrAndHand(&file->fileID, (Handle)gScavGlobals->validFilesList, sizeof(UInt32) ) )
		return (R_NoMem);
#endif
	CheckCatalogName_HFS(key->nodeName[0], &key->nodeName[1], key->parentID, false);

	return (result);
}

/*
 * CheckThread - verify a catalog thread
 *
 * Called in leaf-order for every thread record in the Catalog B-tree
 */
static int
CheckThread_HFS(const HFSCatalogKey * key, const HFSCatalogThread * thread)
{
	int result = 0;

	if (key->nodeName[0] != 0) {
		RcdError(gScavGlobals, E_ThdKey);
		return (E_ThdKey);
	}

	result = CheckCatalogName_HFS(thread->nodeName[0], &thread->nodeName[1],
                         	  thread->parentID, true);
	if (result != noErr) {
		RcdError(gScavGlobals, E_ThdCN);
		return (E_ThdCN);
	}	

	if (key->parentID < kHFSFirstUserCatalogNodeID  &&
            key->parentID != kHFSRootParentID  &&
            key->parentID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}

	if (thread->parentID == kHFSRootParentID) {
		if (key->parentID != kHFSRootFolderID) {
			RcdError(gScavGlobals, E_InvalidID);
			return (E_InvalidID);
		}
	} else if (thread->parentID < kHFSFirstUserCatalogNodeID &&
	           thread->parentID != kHFSRootFolderID) {
		RcdError(gScavGlobals, E_InvalidID);
		return (E_InvalidID);
	}

	return (0);
}


/* File types from BSD Mode */
#define FT_MASK    0170000	/* Mask of file type. */
#define FT_FIFO    0010000	/* Named pipe (fifo). */
#define FT_CHR     0020000	/* Character device. */
#define FT_DIR     0040000	/* Directory file. */
#define FT_BLK     0060000	/* Block device. */
#define FT_REG     0100000	/* Regular file. */
#define FT_LNK     0120000	/* Symbolic link. */
#define FT_SOCK    0140000	/* BSD domain socket. */

/*
 * CheckBSDInfo - Check BSD Pemissions data
 * (HFS Plus volumes only)
 *
 * if repairable then log the error and create a repair order
 */
static void
CheckBSDInfo(const HFSPlusCatalogKey * key, const HFSPlusBSDInfo * bsdInfo, int isdir)
{
#define kObsoleteUnknownUID  (-3)
#define kUnknownUID          (99)	

	Boolean reset = false;

	/* skip uninitialized BSD info */
	if (bsdInfo->fileMode == 0)
		return;
	
	switch (bsdInfo->fileMode & FT_MASK) {
	  case FT_DIR:
		if (!isdir)
			reset = true;
		break;
	  case FT_REG:
	  case FT_BLK:
	  case FT_CHR:
	  case FT_LNK:
	  case FT_SOCK:
	  case FT_FIFO:
		if (isdir)
			reset = true;
		break;
	  default:
		reset = true;
	}
	
	if (reset ||
	    ((long)bsdInfo->ownerID == kObsoleteUnknownUID) ||
	    ((long)bsdInfo->groupID == kObsoleteUnknownUID)) {
		RepairOrderPtr p;
		int n;
		
		if (reset) {
			gScavGlobals->TarBlock = bsdInfo->fileMode & FT_MASK;
			RcdError(gScavGlobals, E_InvalidPermissions);
		}

                n = CatalogNameSize(&key->nodeName, true);
		
		p = AllocMinorRepairOrder(gScavGlobals, n);
		if (p == NULL) return;

                CopyCatalogName((const CatalogName *)&key->nodeName,
			(CatalogName*)&p->name, true);
		
		if (reset) {
			p->type      = E_InvalidPermissions;
			p->correct   = 0;
			p->incorrect = bsdInfo->fileMode;
		} else {
			p->type      = E_InvalidUID;
			p->correct   = kUnknownUID;
			if ((long)bsdInfo->ownerID == kObsoleteUnknownUID)
				p->incorrect = bsdInfo->ownerID;
			else 
				p->incorrect = bsdInfo->groupID;
		}

                p->parid = key->parentID;
		p->hint = 0;
		
		gScavGlobals->CatStat |= S_Permissions;
	}
}

/*
 * Validate a Unicode filename 
 *
 * check character count
 * check for embedded NULLs and ':'
 * check for precomposed (illegal) characters
 *
 * if repairable then log the error and create a repair order
 */
static int
CheckCatalogName(u_int16_t charCount, const u_int16_t *uniChars, u_int32_t parentID, Boolean thread)
{
	int result = 0;

	/*
	 * if the count is wrong for a key then lookup the thread
	 * if the count is wrong for a thread then find the key
	 */
	if ((charCount == 0) || (charCount > kHFSPlusMaxFileNameChars))
		result = E_CName;

	return (result);
}

/*
 * Validate an HFS filename 
 *
 * check character count
 * check for embedded NULLs and ':'
 *
 * if repairable then log the error and create a repair order
 */
static int
CheckCatalogName_HFS(u_int16_t charCount, const u_char *filename, u_int32_t parentID, Boolean thread)
{
	int result = 0;

	/*
	 * if the count is wrong for a key then lookup the thread
	 * if the count is wrong for a thread then find the key
	 */
	if ((charCount == 0) || (charCount > kHFSMaxFileNameChars))
		result = E_CName;

	return (result);
}

/* 
 * RecordBadAllocation
 *
 * Record a repair to adjust a file's allocation size.
 *
 * This could also trigger a truncation if the new block
 * count isn't large enough to cover the current LEOF.
 */
static int
RecordBadAllocation(UInt32 parID, char * filename, UInt32 forkType, UInt32 oldBlkCnt, UInt32 newBlkCnt)
{
	RepairOrderPtr p;
	char goodstr[16];
	char badstr[16];
	int n;
	
	PrintError(gScavGlobals, E_PEOF, 1, filename);
	sprintf(goodstr, "%d", newBlkCnt);
	sprintf(badstr, "%d", oldBlkCnt);
	PrintError(gScavGlobals, E_BadValue, 2, goodstr, badstr);

	/* Only HFS+ is repaired here */
	if ( !gScavGlobals->isHFSPlus )
		return (E_PEOF);

	n = strlen(filename);
	p = AllocMinorRepairOrder(gScavGlobals, n + 1);
	if (p == NULL)
		return (R_NoMem);

	p->type = E_PEOF;
	p->forkType = forkType;
	p->incorrect = oldBlkCnt;
	p->correct = newBlkCnt;
	p->hint = 0;
	p->parid = parID;
	p->name[0] = n;  /* pascal string */
	CopyMemory(filename, &p->name[1], n);

	gScavGlobals->CatStat |= S_FileAllocation;
	return (0);
}


/* 
 * RecordTruncation
 *
 * Record a repair to trucate a file's logical size.
 */
static int
RecordTruncation(UInt32 parID, char * filename, UInt32 forkType, UInt64 oldSize, UInt64 newSize)
{
	RepairOrderPtr p;
	char oldSizeStr[48];
	char newSizeStr[48];
	int n;
	
	PrintError(gScavGlobals, E_LEOF, 1, filename);
	sprintf(oldSizeStr, "%qd", oldSize);
	sprintf(newSizeStr, "%qd", newSize);
	PrintError(gScavGlobals, E_BadValue, 2, newSizeStr, oldSizeStr);

	/* Only HFS+ is repaired here */
	if ( !gScavGlobals->isHFSPlus )
		return (E_LEOF);

	n = strlen(filename);
	p = AllocMinorRepairOrder(gScavGlobals, n + 1);
	if (p == NULL)
		return (R_NoMem);

	p->type = E_LEOF;
	p->forkType = forkType;
	p->incorrect = oldSize;
	p->correct = newSize;
	p->hint = 0;
	p->parid = parID;
	p->name[0] = n;  /* pascal string */
	CopyMemory(filename, &p->name[1], n);

	gScavGlobals->CatStat |= S_FileAllocation;
	return (0);
}


/*
 * CaptureMissingThread
 *
 * Capture info for a missing thread record so it
 * can be repaired later.  The next key is saved
 * so that the Catalog Hierarchy check can proceed.
 * The thread PID/NAME are initialized during the
 * Catalog Hierarchy check phase.
 */
static int
CaptureMissingThread(UInt32 threadID, const HFSPlusCatalogKey *nextKey)
{
	MissingThread *mtp;
	char idStr[16];

	sprintf(idStr, "%d", threadID);
	PrintError(gScavGlobals, E_NoThd, 1, idStr);

	/* Only HFS+ missing threads are repaired here */
	if ( !gScavGlobals->isHFSPlus)
		return (E_NoThd);
	
	mtp = (MissingThread *) AllocateClearMemory(sizeof(MissingThread));
	if (mtp == NULL)
		return (R_NoMem);
	
	/* add it to the list of missing threads */
	mtp->link = gScavGlobals->missingThreadList;
	gScavGlobals->missingThreadList = mtp;
	
	mtp->threadID = threadID;	
	CopyMemory(nextKey, &mtp->nextKey, nextKey->keyLength + 2);

	if (gScavGlobals->RepLevel == repairLevelNoProblemsFound)
		gScavGlobals->RepLevel = repairLevelVolumeRecoverable;

	gScavGlobals->CatStat |= S_MissingThread;
	return (noErr);
}

