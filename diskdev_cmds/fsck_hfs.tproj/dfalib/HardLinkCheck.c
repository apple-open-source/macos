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

#define FILE_ID_COUNT	10
#define kBadLinkID 0xffffffff

/* info saved for each indirect link encountered */
struct IndirectLinkInfo {
	UInt32	linkID;
	UInt32	linkCount;
	UInt32  linkFileID[FILE_ID_COUNT];
};

struct HardLinkInfo {
	UInt32	privDirID;
	UInt32	linkSlots;
	UInt32	slotsUsed;
	SGlobPtr globals;
	struct IndirectLinkInfo linkInfo[1];
};


HFSPlusCatalogKey gMetaDataDirKey = {
	48,		/* key length */
	2,		/* parent directory ID */
	{
		21,	/* number of unicode characters */
		{
			'\0','\0','\0','\0',
			'H','F','S','+',' ',
			'P','r','i','v','a','t','e',' ',
			'D','a','t','a'
		}
	}
};


/* private local routines */
static int  GetPrivateDir(SGlobPtr gp, CatalogRecord *rec);
static int  RecordOrphanINode(SGlobPtr gp, UInt32 parID, char * filename);
static int  RecordBadLinkCount(SGlobPtr gp, UInt32 parID, char * filename,
                                int badcnt, int goodcnt);

/*
 * HardLinkCheckBegin
 *
 * Get ready to capture indirect link info.
 * Called before iterating over all the catalog leaf nodes.
 */
int
HardLinkCheckBegin(SGlobPtr gp, void** cookie)
{
	struct HardLinkInfo *info;
	CatalogRecord rec;
	UInt32 folderID;
	int entries, slots;

	if (GetPrivateDir(gp, &rec) == 0 && rec.hfsPlusFolder.valence != 0) {
		entries = rec.hfsPlusFolder.valence;
		folderID = rec.hfsPlusFolder.folderID;
	} else {
		entries = 0;
		folderID = 0;
	};

	slots = entries ? entries + 10 : 100;

	info = (struct HardLinkInfo *) AllocateClearMemory(
		sizeof(struct HardLinkInfo) + sizeof(struct IndirectLinkInfo) * slots);

	info->privDirID = folderID;
	info->linkSlots = slots;
	info->slotsUsed = 0;
	info->globals = gp;
	
	* cookie = info;
	return (0);
}

/*
 * HardLinkCheckEnd
 *
 * Dispose of captured data.
 * Called after calling CheckHardLinks.
 */
void
HardLinkCheckEnd(void * cookie)
{
#if 0
	struct HardLinkInfo * info = (struct HardLinkInfo *) cookie;
	struct IndirectLinkInfo *linkInfo;
	int i, j;

	linkInfo = &info->linkInfo[0];
	for (i = 0; i < info->slotsUsed; ++linkInfo, ++i) {
		printf(" link %d has %d links: ", linkInfo->linkID, linkInfo->linkCount);
		for (j = 0; j < linkInfo->linkCount; ++j)
			printf(", %d", linkInfo->linkFileID[j]);
		printf("\n");
	}
#endif
	if (cookie)
		DisposeMemory(cookie);
}

/*
 * CaptureHardLink
 *
 * Capture indirect link info.
 * Called for every indirect link in the catalog.
 */
void
CaptureHardLink(void * cookie, UInt32 linkID, UInt32 fileID)
{
	struct HardLinkInfo * info = (struct HardLinkInfo *) cookie;
	struct IndirectLinkInfo *linkInfo;
	int linkCount;
	int i;

	if (info == NULL)
		return;

	if (linkID == 0)
		linkID = kBadLinkID;	/* reported later */

	linkInfo = &info->linkInfo[0];
	for (i = 0; i < info->linkSlots; ++linkInfo, ++i) {
		if (linkInfo->linkID == kBadLinkID)
			continue;
		if (linkInfo->linkID == linkID || linkInfo->linkID == 0) {
			linkCount = linkInfo->linkCount;
			++linkInfo->linkCount;
			if (linkCount < FILE_ID_COUNT)
				linkInfo->linkFileID[linkCount] = fileID;
			if (linkInfo->linkID == 0) {
				linkInfo->linkID = linkID;
				++info->slotsUsed;
			}
			break;
		}
	}
}

/*
 * CheckHardLinks
 *
 * Check indirect node link counts aginst the indirect
 * links that were found. There are 4 possible problems
 * that can occur.
 *  1. orphaned indirect node (i.e. no links found)
 *  2. orphaned indirect link (i.e. indirect node missing)
 *  3. incorrect link count
 *  4. indirect link id was 0 (i.e. link id wasn't preserved)
 */
int
CheckHardLinks(void *cookie)
{
	struct HardLinkInfo *info = (struct HardLinkInfo *)cookie;
	SGlobPtr gp;
	UInt32              folderID;
	UInt32 linkID;
	SFCB              * fcb;
	CatalogRecord       rec;
	HFSPlusCatalogKey * keyp;
	BTreeIterator       iterator;
	FSBufferDescriptor  btrec;
	UInt16              reclen;
	size_t              len;
	int                 i;
	int linkCount;
	int prefixlen;
	int result;
	struct IndirectLinkInfo * linkInfo;
	char filename[64];

	/* All done if no hard links exist. */
	if (info == NULL || (info->privDirID == 0 && info->slotsUsed == 0))
		return (0);

	gp = info->globals;
	PrintStatus(gp, M_MultiLinkChk, 0);

	folderID = info->privDirID;
	linkInfo = &info->linkInfo[0];
	fcb = gp->calculatedCatalogFCB;
	prefixlen = strlen(HFS_INODE_PREFIX);
	ClearMemory(&iterator, sizeof(iterator));
	keyp = (HFSPlusCatalogKey*)&iterator.key;
	btrec.bufferAddress = &rec;
	btrec.itemCount = 1;
	btrec.itemSize = sizeof(rec);
	/*
	 * position iterator at folder thread record
	 * (i.e. one record before first child)
	 */
	ClearMemory(&iterator, sizeof(iterator));
	BuildCatalogKey(folderID, NULL, true, (CatalogKey *)keyp);
	result = BTSearchRecord(fcb, &iterator, kInvalidMRUCacheKey, &btrec,
				&reclen, &iterator);
	if (result) goto exit;

	/* Visit all the children in private directory. */
	for (;;) {
		result = BTIterateRecord(fcb, kBTreeNextRecord, &iterator,
					&btrec, &reclen);
		if (result || keyp->parentID != folderID)
			break;
		if (rec.recordType != kHFSPlusFileRecord)
			continue;

		(void) utf_encodestr(keyp->nodeName.unicode,
					keyp->nodeName.length * 2,
					filename, &len);
		filename[len] = '\0';
		if (strstr(filename, HFS_INODE_PREFIX) != filename)
			continue;
		
		linkID = atol(&filename[prefixlen]);
		linkCount = rec.hfsPlusFile.bsdInfo.special.linkCount;

		/* look for matching links */
		for (i = 0; i < info->slotsUsed; ++i) {
			if (linkID == linkInfo[i].linkID) {
#if 0
				printf("   found match for link %d\n", linkID);
#endif
				if (linkCount != linkInfo[i].linkCount)
					RecordBadLinkCount(gp, folderID, filename,
					                   linkCount, linkInfo[i].linkCount);
				linkInfo[i].linkID = 0;
				break;
			}
		}

		/* no match means this is an orphan indirect node */
		if (i == info->slotsUsed)
			RecordOrphanINode(gp, folderID, filename);

		filename[0] = '\0';
	}

	/*
	 * Any remaining indirect links are orphans.
	 *
	 * TBD: what to do with them...
	 */
#if 0
	if (gp->logLevel >= kDebugLog) {
	    for (i = 0; i < info->slotsUsed; ++i) {
		if (linkInfo[i].linkID == kBadLinkID) {
		    printf("missing link number (copied under Mac OS 9 ?)\n");
		    /*
		     * To do: loop through each file ID and report
		     */
		} else if (linkInfo[i].linkID != 0) {
		    printf("\torphaned link (indirect node %d missing)\n", linkInfo[i].linkID);
		}
	}
#endif

exit:
	return (result);
}

/*
 * GetPrivateDir
 *
 * Get catalog entry for the "HFS+ Private Data" directory.
 * The indirect nodes are stored in this directory.
 */
static int
GetPrivateDir(SGlobPtr gp, CatalogRecord * rec)
{
	HFSPlusCatalogKey * keyp;
	BTreeIterator       iterator;
	FSBufferDescriptor  btrec;
	UInt16              reclen;
	int                 result;

	if (!gp->isHFSPlus)
		return (-1);

	ClearMemory(&iterator, sizeof(iterator));
	keyp = (HFSPlusCatalogKey*)&iterator.key;

	btrec.bufferAddress = rec;
	btrec.itemCount = 1;
	btrec.itemSize = sizeof(CatalogRecord);

	/* look up record for HFS+ private folder */
	ClearMemory(&iterator, sizeof(iterator));
	CopyMemory(&gMetaDataDirKey, keyp, sizeof(gMetaDataDirKey));
	result = BTSearchRecord(gp->calculatedCatalogFCB, &iterator,
	                        kInvalidMRUCacheKey, &btrec, &reclen, &iterator);

	return (result);
}

/*
 * RecordOrphanINode
 *
 * Record a repair to delete an orphaned indirect node.
 */
static int
RecordOrphanINode(SGlobPtr gp, UInt32 parID, char* filename)
{
	RepairOrderPtr p;
	int n;
	
	PrintError(gp, E_UnlinkedFile, 1, filename);
	
	n = strlen(filename);
	p = AllocMinorRepairOrder(gp, n + 1);
	if (p == NULL)
		return (R_NoMem);

	p->type = E_UnlinkedFile;
	p->correct = 0;
	p->incorrect = 0;
	p->hint = 0;
	p->parid = parID;
	p->name[0] = n;  /* pascal string */
	CopyMemory(filename, &p->name[1], n);

	gp->CatStat |= S_UnlinkedFile;
	return (noErr);
}


/* 
 * RecordBadLinkCount
 *
 * Record a repair to adjust an indirect node's link count.
 */
static int
RecordBadLinkCount(SGlobPtr gp, UInt32 parID, char * filename,
                   int badcnt, int goodcnt)
{
	RepairOrderPtr p;
	char goodstr[16];
	char badstr[16];
	int n;
	
	PrintError(gp, E_InvalidLinkCount, 1, filename);
	sprintf(goodstr, "%d", goodcnt);
	sprintf(badstr, "%d", badcnt);
	PrintError(gp, E_BadValue, 2, goodstr, badstr);

	n = strlen(filename);
	p = AllocMinorRepairOrder(gp, n + 1);
	if (p == NULL)
		return (R_NoMem);

	p->type = E_InvalidLinkCount;
	p->incorrect = badcnt;
	p->correct = goodcnt;
	p->hint = 0;
	p->parid = parID;
	p->name[0] = n;  /* pascal string */
	CopyMemory(filename, &p->name[1], n);

	gp->CatStat |= S_LinkCount;
	return (0);
}
