/*
 * Copyright (c) 2002, 2004, 2005, 2007-2008 Apple Inc. All rights reserved.
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
 * hfs_endian.c
 *
 * This file implements endian swapping routines for the HFS/HFS Plus
 * volume format.
 */

#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libkern/OSByteOrder.h>
#include <hfs/hfs_format.h>

#include "Scavenger.h"
#include "BTreePrivate.h"
#include "hfs_endian.h"

#undef ENDIAN_DEBUG

/*
 * Internal swapping routines
 *
 * These routines handle swapping the records of leaf and index nodes.  The
 * layout of the keys and records varies depending on the kind of B-tree
 * (determined by fileID).
 *
 * The direction parameter must be kSwapBTNodeBigToHost or kSwapBTNodeHostToBig.
 * The kSwapBTNodeHeaderRecordOnly "direction" is not valid for these routines.
 */
static int hfs_swap_HFSPlusBTInternalNode (BlockDescriptor *src, SFCB *fcb, enum HFSBTSwapDirection direction);
static int hfs_swap_HFSBTInternalNode (BlockDescriptor *src, SFCB *fcb, enum HFSBTSwapDirection direction);

/*
 * hfs_swap_HFSPlusForkData
 */
static void
hfs_swap_HFSPlusForkData (
    HFSPlusForkData *src
)
{
    int i;

	src->logicalSize		= SWAP_BE64 (src->logicalSize);

	src->clumpSize			= SWAP_BE32 (src->clumpSize);
	src->totalBlocks		= SWAP_BE32 (src->totalBlocks);

    for (i = 0; i < kHFSPlusExtentDensity; i++) {
        src->extents[i].startBlock	= SWAP_BE32 (src->extents[i].startBlock);
        src->extents[i].blockCount	= SWAP_BE32 (src->extents[i].blockCount);
    }
}

/*
 * hfs_swap_HFSMasterDirectoryBlock
 *
 *  Specially modified to swap parts of the finder info
 */
void
hfs_swap_HFSMasterDirectoryBlock (
    void *buf
)
{
    HFSMasterDirectoryBlock *src = (HFSMasterDirectoryBlock *)buf;

    src->drSigWord		= SWAP_BE16 (src->drSigWord);
    src->drCrDate		= SWAP_BE32 (src->drCrDate);
    src->drLsMod		= SWAP_BE32 (src->drLsMod);
    src->drAtrb			= SWAP_BE16 (src->drAtrb);
    src->drNmFls		= SWAP_BE16 (src->drNmFls);
    src->drVBMSt		= SWAP_BE16 (src->drVBMSt);
    src->drAllocPtr		= SWAP_BE16 (src->drAllocPtr);
    src->drNmAlBlks		= SWAP_BE16 (src->drNmAlBlks);
    src->drAlBlkSiz		= SWAP_BE32 (src->drAlBlkSiz);
    src->drClpSiz		= SWAP_BE32 (src->drClpSiz);
    src->drAlBlSt		= SWAP_BE16 (src->drAlBlSt);
    src->drNxtCNID		= SWAP_BE32 (src->drNxtCNID);
    src->drFreeBks		= SWAP_BE16 (src->drFreeBks);

    /* Don't swap drVN */

    src->drVolBkUp		= SWAP_BE32 (src->drVolBkUp);
    src->drVSeqNum		= SWAP_BE16 (src->drVSeqNum);
    src->drWrCnt		= SWAP_BE32 (src->drWrCnt);
    src->drXTClpSiz		= SWAP_BE32 (src->drXTClpSiz);
    src->drCTClpSiz		= SWAP_BE32 (src->drCTClpSiz);
    src->drNmRtDirs		= SWAP_BE16 (src->drNmRtDirs);
    src->drFilCnt		= SWAP_BE32 (src->drFilCnt);
    src->drDirCnt		= SWAP_BE32 (src->drDirCnt);

    /* Swap just the 'blessed folder' in drFndrInfo */
    src->drFndrInfo[0]	= SWAP_BE32 (src->drFndrInfo[0]);

    src->drEmbedSigWord	= SWAP_BE16 (src->drEmbedSigWord);
	src->drEmbedExtent.startBlock = SWAP_BE16 (src->drEmbedExtent.startBlock);
	src->drEmbedExtent.blockCount = SWAP_BE16 (src->drEmbedExtent.blockCount);

    src->drXTFlSize		= SWAP_BE32 (src->drXTFlSize);
	src->drXTExtRec[0].startBlock = SWAP_BE16 (src->drXTExtRec[0].startBlock);
	src->drXTExtRec[0].blockCount = SWAP_BE16 (src->drXTExtRec[0].blockCount);
	src->drXTExtRec[1].startBlock = SWAP_BE16 (src->drXTExtRec[1].startBlock);
	src->drXTExtRec[1].blockCount = SWAP_BE16 (src->drXTExtRec[1].blockCount);
	src->drXTExtRec[2].startBlock = SWAP_BE16 (src->drXTExtRec[2].startBlock);
	src->drXTExtRec[2].blockCount = SWAP_BE16 (src->drXTExtRec[2].blockCount);

    src->drCTFlSize		= SWAP_BE32 (src->drCTFlSize);
	src->drCTExtRec[0].startBlock = SWAP_BE16 (src->drCTExtRec[0].startBlock);
	src->drCTExtRec[0].blockCount = SWAP_BE16 (src->drCTExtRec[0].blockCount);
	src->drCTExtRec[1].startBlock = SWAP_BE16 (src->drCTExtRec[1].startBlock);
	src->drCTExtRec[1].blockCount = SWAP_BE16 (src->drCTExtRec[1].blockCount);
	src->drCTExtRec[2].startBlock = SWAP_BE16 (src->drCTExtRec[2].startBlock);
	src->drCTExtRec[2].blockCount = SWAP_BE16 (src->drCTExtRec[2].blockCount);
}

/*
 * hfs_swap_HFSPlusVolumeHeader
 */
void
hfs_swap_HFSPlusVolumeHeader (
    void *buf
)
{
    HFSPlusVolumeHeader *src = (HFSPlusVolumeHeader *)buf;

    src->signature			= SWAP_BE16 (src->signature);
    src->version			= SWAP_BE16 (src->version);
    src->attributes			= SWAP_BE32 (src->attributes);
    src->lastMountedVersion	= SWAP_BE32 (src->lastMountedVersion);

    /* Don't swap reserved */

    src->createDate			= SWAP_BE32 (src->createDate);
    src->modifyDate			= SWAP_BE32 (src->modifyDate);
    src->backupDate			= SWAP_BE32 (src->backupDate);
    src->checkedDate		= SWAP_BE32 (src->checkedDate);
    src->fileCount			= SWAP_BE32 (src->fileCount);
    src->folderCount		= SWAP_BE32 (src->folderCount);
    src->blockSize			= SWAP_BE32 (src->blockSize);
    src->totalBlocks		= SWAP_BE32 (src->totalBlocks);
    src->freeBlocks			= SWAP_BE32 (src->freeBlocks);
    src->nextAllocation		= SWAP_BE32 (src->nextAllocation);
    src->rsrcClumpSize		= SWAP_BE32 (src->rsrcClumpSize);
    src->dataClumpSize		= SWAP_BE32 (src->dataClumpSize);
    src->nextCatalogID		= SWAP_BE32 (src->nextCatalogID);
    src->writeCount			= SWAP_BE32 (src->writeCount);
    src->encodingsBitmap	= SWAP_BE64 (src->encodingsBitmap);

    /* Don't swap finderInfo */

    hfs_swap_HFSPlusForkData (&src->allocationFile);
    hfs_swap_HFSPlusForkData (&src->extentsFile);
    hfs_swap_HFSPlusForkData (&src->catalogFile);
    hfs_swap_HFSPlusForkData (&src->attributesFile);
    hfs_swap_HFSPlusForkData (&src->startupFile);
}

/*
 * hfs_swap_BTNode
 *
 *  NOTE: This operation is not naturally symmetric.
 *        We have to determine which way we're swapping things.
 */
int
hfs_swap_BTNode (
    BlockDescriptor *src,
    SFCB *fcb,
    enum HFSBTSwapDirection direction
)
{
    BTNodeDescriptor *srcDesc = src->buffer;
    BTreeControlBlockPtr btcb = fcb->fcbBtree;
    UInt16 *srcOffs = NULL;
    UInt32 i;
    int error = 0;

//			WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);

#ifdef ENDIAN_DEBUG
    if (direction == kSwapBTNodeBigToHost) {
        if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "BE -> Native Swap\n");
    } else if (direction == kSwapBTNodeHostToBig) {
        if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "Native -> BE Swap\n");
    } else if (direction == kSwapBTNodeHeaderRecordOnly) {
        if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "Not swapping descriptors\n");
    } else {
        if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_BTNode: This is impossible");
        exit(99);
    }
#endif

    /*
     * If we are doing a swap from on-disk to in-memory, then swap the node
     * descriptor and record offsets before we need to use them.
     */
    if (direction == kSwapBTNodeBigToHost) {
        srcDesc->fLink		= SWAP_BE32 (srcDesc->fLink);
        srcDesc->bLink		= SWAP_BE32 (srcDesc->bLink);
		if (srcDesc->fLink >= btcb->totalNodes) {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_BTNode: invalid forward link (0x%08X)\n", srcDesc->fLink);
		}
		if (srcDesc->bLink >= btcb->totalNodes) {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_BTNode: invalid backward link (0x%08X)\n", srcDesc->bLink);
		}

		/*
		 * Don't swap srcDesc->kind or srcDesc->height because they are only one byte.
		 * We don't check them here because the upper layers will check (and possibly
		 * repair) them more effectively.
		 */
		if (srcDesc->kind < kBTLeafNode || srcDesc->kind > kBTMapNode) {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_BTNode: invalid node kind (%d)\n", srcDesc->kind);
		}
		if (srcDesc->height > btcb->treeDepth) {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_BTNode: invalid node height (%d)\n", srcDesc->height);
		}
        
        /* Don't swap srcDesc->reserved */
    
        srcDesc->numRecords	= SWAP_BE16 (srcDesc->numRecords);
        
        /*
         * Swap the node offsets (including the free space one!).
         */
        srcOffs = (UInt16 *)((char *)src->buffer + (src->blockSize - ((srcDesc->numRecords + 1) * sizeof (UInt16))));

        /*
         * Sanity check that the record offsets are within the node itself.
         */
        if ((char *)srcOffs > ((char *)src->buffer + src->blockSize) ||
            (char *)srcOffs < ((char *)src->buffer + sizeof(BTNodeDescriptor))) {
            if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_BTNode: invalid record count (0x%04X)\n", srcDesc->numRecords);
			WriteError(fcb->fcbVolume->vcbGPtr, E_NRecs, fcb->fcbFileID, src->blockNum);
            error = E_NRecs;
            goto fail;
        }

		/*
		 * Swap and sanity check each of the record offsets.
		 */
        for (i = 0; i <= srcDesc->numRecords; i++) {
            srcOffs[i]	= SWAP_BE16 (srcOffs[i]);

            /*
             * Sanity check: must be even, and within the node itself.
             *
             * We may be called to swap an unused node, which contains all zeroes.
             * This is why we allow the record offset to be zero.
             */
            if ((srcOffs[i] & 1) || (srcOffs[i] < sizeof(BTNodeDescriptor) && srcOffs[i] != 0) || (srcOffs[i] >= src->blockSize)) {
            	if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_BTNode: offset #%d invalid  (0x%04X) (blockSize 0x%x numRecords %d)\n",
                                i, srcOffs[i], src->blockSize, srcDesc->numRecords);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
            	error = E_BadNode;
            	goto fail;
            }

            /*
             * Make sure the offsets are strictly increasing.  Note that we're looping over
             * them backwards, hence the order in the comparison.
             */
            if ((i != 0) && (srcOffs[i] >= srcOffs[i-1])) {
            	if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_BTNode: offsets %d and %d out of order (0x%04X, 0x%04X)\n",
            	    i, i-1, srcOffs[i], srcOffs[i-1]);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
            	error = E_BadNode;
            	goto fail;
            }
        }
    }
    
    /*
     * Swap the records (ordered by frequency of access)
     */
    if ((srcDesc->kind == kBTIndexNode) ||
        (srcDesc-> kind == kBTLeafNode)) {

        if (fcb->fcbVolume->vcbSignature == kHFSPlusSigWord) {
            error = hfs_swap_HFSPlusBTInternalNode (src, fcb, direction);
        } else {
            error = hfs_swap_HFSBTInternalNode (src, fcb, direction);
        }
        
        if (error) goto fail;
        
    } else if (srcDesc-> kind == kBTMapNode) {
        /* Don't swap the bitmaps, they'll be done in the bitmap routines */
    
    } else if (srcDesc-> kind == kBTHeaderNode) {
        /* The header's offset is hard-wired because we cannot trust the offset pointers. */
        BTHeaderRec *srcHead = (BTHeaderRec *)((char *)src->buffer + sizeof(BTNodeDescriptor));
        
        srcHead->treeDepth		=	SWAP_BE16 (srcHead->treeDepth);
        
        srcHead->rootNode		=	SWAP_BE32 (srcHead->rootNode);
        srcHead->leafRecords	=	SWAP_BE32 (srcHead->leafRecords);
        srcHead->firstLeafNode	=	SWAP_BE32 (srcHead->firstLeafNode);
        srcHead->lastLeafNode	=	SWAP_BE32 (srcHead->lastLeafNode);
        
        srcHead->nodeSize		=	SWAP_BE16 (srcHead->nodeSize);
        srcHead->maxKeyLength	=	SWAP_BE16 (srcHead->maxKeyLength);
        
        srcHead->totalNodes		=	SWAP_BE32 (srcHead->totalNodes);
        srcHead->freeNodes		=	SWAP_BE32 (srcHead->freeNodes);
        
        srcHead->clumpSize		=	SWAP_BE32 (srcHead->clumpSize);
        srcHead->attributes		=	SWAP_BE32 (srcHead->attributes);

        /* Don't swap srcHead->reserved1 */
        /* Don't swap srcHead->btreeType; it's only one byte */
        /* Don't swap srcHead->reserved2 */
        /* Don't swap srcHead->reserved3 */
        /* Don't swap bitmap */
    }
    /* Else: other node kinds will be caught by upper layers */

    /*
     * If we are doing a swap from in-memory to on-disk, then swap the node
     * descriptor and record offsets after we're done using them.
     */
    if (direction == kSwapBTNodeHostToBig) {
		/*
		 * Swap the forward and backward links.
		 */
		if (srcDesc->fLink >= btcb->totalNodes) {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_UNswap_BTNode: invalid forward link (0x%08X)\n", srcDesc->fLink);
		}
		if (srcDesc->bLink >= btcb->totalNodes) {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_UNswap_BTNode: invalid backward link (0x%08X)\n", srcDesc->bLink);
		}
        srcDesc->fLink		= SWAP_BE32 (srcDesc->fLink);
        srcDesc->bLink		= SWAP_BE32 (srcDesc->bLink);
    
		/*
		 * Don't swap srcDesc->kind or srcDesc->height because they are only one byte.
		 * We don't check them here because the upper layers will check (and possibly
		 * repair) them more effectively.
		 */
		if (srcDesc->kind < kBTLeafNode || srcDesc->kind > kBTMapNode) {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_UNswap_BTNode: invalid node kind (%d)\n", srcDesc->kind);
		}
		if (srcDesc->height > btcb->treeDepth) {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_UNswap_BTNode: invalid node height (%d)\n", srcDesc->height);
		}

        /* Don't swap srcDesc->reserved */
    
        /*
         * Swap the node offsets (including the free space one!).
         */
        srcOffs = (UInt16 *)((char *)src->buffer + (src->blockSize - ((srcDesc->numRecords + 1) * sizeof (UInt16))));

        /*
         * Sanity check that the record offsets are within the node itself.
         */
        if ((char *)srcOffs > ((char *)src->buffer + src->blockSize) ||
        	(char *)srcOffs < ((char *)src->buffer + sizeof(BTNodeDescriptor))) {
            if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_UNswap_BTNode: invalid record count (0x%04X)\n", srcDesc->numRecords);
			WriteError(fcb->fcbVolume->vcbGPtr, E_NRecs, fcb->fcbFileID, src->blockNum);
            error = E_NRecs;
            goto fail;
        }

		/*
		 * Swap and sanity check each of the record offsets.
		 */
        for (i = 0; i <= srcDesc->numRecords; i++) {
            /*
             * Sanity check: must be even, and within the node itself.
             *
             * We may be called to swap an unused node, which contains all zeroes.
             * This is why we allow the record offset to be zero.
             */
            if ((srcOffs[i] & 1) || (srcOffs[i] < sizeof(BTNodeDescriptor) && srcOffs[i] != 0) || (srcOffs[i] >= src->blockSize)) {
            	if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_UNswap_BTNode: offset #%d invalid  (0x%04X) (blockSize 0x%x numRecords %d)\n",
                                i, srcOffs[i], src->blockSize, srcDesc->numRecords);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
            	error = E_BadNode;
            	goto fail;
            }

            /*
             * Make sure the offsets are strictly increasing.  Note that we're looping over
             * them backwards, hence the order in the comparison.
             */
            if ((i < srcDesc->numRecords) && (srcOffs[i+1] >= srcOffs[i])) {
            	if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_UNswap_BTNode: offsets %d and %d out of order (0x%04X, 0x%04X)\n",
            	    i+1, i, srcOffs[i+1], srcOffs[i]);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
            	error = E_BadNode;
            	goto fail;
            }

            srcOffs[i]	= SWAP_BE16 (srcOffs[i]);
        }
        
        srcDesc->numRecords	= SWAP_BE16 (srcDesc->numRecords);
    }

fail:
	if (error && (state.cur_debug_level & d_dump_node))
	{
		fsck_print(ctx, LOG_TYPE_INFO, "Node %u:\n", src->blockNum);
		HexDump(src->buffer, src->blockSize, TRUE);
	}
    return (error);
}

static int
hfs_swap_HFSPlusBTInternalNode (
    BlockDescriptor *src,
    SFCB *fcb,
    enum HFSBTSwapDirection direction
)
{
    HFSCatalogNodeID fileID =fcb->fcbFileID;
    BTNodeDescriptor *srcDesc = src->buffer;
    UInt16 *srcOffs = (UInt16 *)((char *)src->buffer + (src->blockSize - (srcDesc->numRecords * sizeof (UInt16))));
	const char * const nodeStart = (char *)src->buffer + sizeof(BTNodeDescriptor);
	const char * const nodeEnd = (char *)src->buffer + src->blockSize;
	const char *freeSpace;
    int32_t i;
    UInt32 j;

    /*
     * Sanity check that the record offsets are within the node itself.
	 * Note that srcOffs is allowed to equal nodeEnd, corresponding to a
	 * node with 0 records.  Since the free space offset is stored at a negative
	 * offset from srcOffs, this is still valid in a structural sense, even
	 * though there really shouldn't ever be nodes with 0 records.
     */
    if ((char *)srcOffs < nodeStart || (char *)srcOffs > nodeEnd) {
        if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: invalid record count (0x%04X)\n", srcDesc->numRecords);
        WriteError(fcb->fcbVolume->vcbGPtr, E_NRecs, fcb->fcbFileID, src->blockNum);
        return E_NRecs;
    }

	/*
	 * Check the value of the free space offset, which is just behind where our srcOffs
	 * pointer is pointing. Note that the free space can have size 0, in which case its
	 * offset is the offset of the offsets table itself (e.g. this is always the case for
	 * header nodes). Hence the strictly-greater test below.
	 */
	freeSpace = (char *)src->buffer + srcOffs[-1];
	if (freeSpace < nodeStart || freeSpace > (char *)(srcOffs-1)) {
		if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: invalid free space offset (%X)\n",
									srcOffs[-1]);
		WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
		return E_BadNode;
	}

	/*
	 * Go through the records at each offset, validate those offsets, and do the swapping
	 */
	for (i = 0; i < srcDesc->numRecords; i++) {
		/* Get pointers to the start of the key/record we're currently checking, and the next one. */
		char *srcKey = ((char *)src->buffer + srcOffs[i]);
		char *nextKey = (char *)src->buffer + srcOffs[i-1];

		/*
		 * Make sure the start of this record is within the buffer, and that it
		 * comes before the next one.
		 */
		if (srcKey < nodeStart || srcKey >= nextKey) {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: invalid record offset (record #%d)\n",
										srcDesc->numRecords - i - 1);
			WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
			return E_BadNode;
		}

		/*
		 * Now we need to do different things, depending on what type of tree we're looking at.
		 */
		if (fileID == kHFSExtentsFileID) {
			HFSPlusExtentKey *srcExtentKey = (HFSPlusExtentKey *)srcKey;
			HFSPlusExtentDescriptor *srcRec;
			size_t recordSize = (srcDesc->kind == kBTIndexNode) ? sizeof(UInt32) : sizeof(HFSPlusExtentRecord);

			/*
			 * Make sure the record doesn't overlap the next key
			 */
			if (srcKey + sizeof(HFSPlusExtentKey) + recordSize > nextKey) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: extents key #%d offset too big (0x%04X)\n", srcDesc->numRecords-i-1, srcOffs[i]);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
				return E_BadNode;
			}

			if (direction == kSwapBTNodeBigToHost)
				srcExtentKey->keyLength = SWAP_BE16 (srcExtentKey->keyLength);
			if (srcExtentKey->keyLength != sizeof(*srcExtentKey) - sizeof(srcExtentKey->keyLength)) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: extents key #%d invalid length (%d)\n", srcDesc->numRecords-i-1, srcExtentKey->keyLength);
				WriteError(fcb->fcbVolume->vcbGPtr, E_KeyLen, fcb->fcbFileID, src->blockNum);
				return E_KeyLen;
			}
			srcRec = (HFSPlusExtentDescriptor *)(srcKey + srcExtentKey->keyLength + sizeof(srcExtentKey->keyLength));
			if (direction == kSwapBTNodeHostToBig)
				srcExtentKey->keyLength = SWAP_BE16 (srcExtentKey->keyLength);

			/* Don't swap srcExtentKey->forkType; it's only one byte */
			/* Don't swap srcExtentKey->pad */

			srcExtentKey->fileID			= SWAP_BE32 (srcExtentKey->fileID);
			srcExtentKey->startBlock		= SWAP_BE32 (srcExtentKey->startBlock);

			if (srcDesc->kind == kBTIndexNode) {
				/* For index nodes, the record data is just a child node number. */
				*((UInt32 *)srcRec) = SWAP_BE32 (*((UInt32 *)srcRec));
			} else {
				/* Swap the extent data */
				for (j = 0; j < kHFSPlusExtentDensity; j++) {
					srcRec[j].startBlock	= SWAP_BE32 (srcRec[j].startBlock);
					srcRec[j].blockCount	= SWAP_BE32 (srcRec[j].blockCount);
				}
			}
		} else if (fileID == kHFSCatalogFileID || fileID == kHFSRepairCatalogFileID) {
			HFSPlusCatalogKey *srcCatalogKey = (HFSPlusCatalogKey *)srcKey;
			SInt16 *srcPtr;
			u_int16_t keyLength;

			/*
			 * Make sure we can safely dereference the keyLength and parentID fields.
			 */
			if (srcKey + offsetof(HFSPlusCatalogKey, nodeName.unicode[0]) > nextKey) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: catalog key #%d offset too big (0x%04X)\n", srcDesc->numRecords-i-1, srcOffs[i]);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
				return E_BadNode;
			}

			/*
			 * Swap and sanity check the key length
			 */
			if (direction == kSwapBTNodeBigToHost)
				srcCatalogKey->keyLength = SWAP_BE16 (srcCatalogKey->keyLength);
			keyLength = srcCatalogKey->keyLength;	/* Put it in a local (native order) because we use it several times */
			if (direction == kSwapBTNodeHostToBig)
				srcCatalogKey->keyLength = SWAP_BE16 (keyLength);

			/* Sanity check the key length */
			if (keyLength < kHFSPlusCatalogKeyMinimumLength || keyLength > kHFSPlusCatalogKeyMaximumLength) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: catalog key #%d invalid length (%d)\n", srcDesc->numRecords-i-1, keyLength);
				WriteError(fcb->fcbVolume->vcbGPtr, E_KeyLen, fcb->fcbFileID, src->blockNum);
				return E_KeyLen;
			}

			/*
			 * Make sure that we can safely dereference the record's type field or
			 * an index node's child node number.
			 */
			srcPtr = (SInt16 *)(srcKey + keyLength + sizeof(srcCatalogKey->keyLength));
			if ((char *)srcPtr + sizeof(UInt32) > nextKey) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: catalog key #%d too big\n", srcDesc->numRecords-i-1);
				WriteError(fcb->fcbVolume->vcbGPtr, E_KeyLen, fcb->fcbFileID, src->blockNum);
				return E_KeyLen;
			}

			srcCatalogKey->parentID						= SWAP_BE32 (srcCatalogKey->parentID);

			/*
			 * Swap and sanity check the key's node name
			 */
			if (direction == kSwapBTNodeBigToHost)
				srcCatalogKey->nodeName.length	= SWAP_BE16 (srcCatalogKey->nodeName.length);
			/* Make sure name length is consistent with key length */
			if (keyLength < sizeof(srcCatalogKey->parentID) + sizeof(srcCatalogKey->nodeName.length) +
				srcCatalogKey->nodeName.length*sizeof(srcCatalogKey->nodeName.unicode[0])) {
				if (state.debug){
					uintptr_t keyOffset = (uintptr_t)srcKey - (uintptr_t)src->buffer;
					uintptr_t recordSize = (uintptr_t)nextKey - (uintptr_t)srcKey;
					unsigned recordIndex = srcDesc->numRecords - i;

					fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: catalog record #%d (0-based, offset 0x%lX) keyLength=%d expected=%lu\n",
						recordIndex, keyOffset, keyLength, sizeof(srcCatalogKey->parentID) + sizeof(srcCatalogKey->nodeName.length) +
							   srcCatalogKey->nodeName.length*sizeof(srcCatalogKey->nodeName.unicode[0]));
					if (state.cur_debug_level & d_dump_record) {
						fsck_print(ctx, LOG_TYPE_INFO, "Record %u (offset 0x%04X):\n", recordIndex, keyOffset);
						HexDump(srcCatalogKey, (unsigned)recordSize, FALSE);
					}
				}
				WriteError(fcb->fcbVolume->vcbGPtr, E_KeyLen, fcb->fcbFileID, src->blockNum);
				return E_KeyLen;
			}
			for (j = 0; j < srcCatalogKey->nodeName.length; j++) {
				srcCatalogKey->nodeName.unicode[j]	= SWAP_BE16 (srcCatalogKey->nodeName.unicode[j]);
			}
			if (direction == kSwapBTNodeHostToBig)
				srcCatalogKey->nodeName.length	= SWAP_BE16 (srcCatalogKey->nodeName.length);

			/*
			 * For index nodes, the record data is just the child's node number.
			 * Skip over swapping the various types of catalog record.
			 */
			if (srcDesc->kind == kBTIndexNode) {
				*((UInt32 *)srcPtr) = SWAP_BE32 (*((UInt32 *)srcPtr));
				continue;
			}

			/* Make sure the recordType is in native order before using it. */
			if (direction == kSwapBTNodeBigToHost)
				srcPtr[0] = SWAP_BE16 (srcPtr[0]);

			if (srcPtr[0] == kHFSPlusFolderRecord) {
				HFSPlusCatalogFolder *srcRec = (HFSPlusCatalogFolder *)srcPtr;
				if ((char *)srcRec + sizeof(*srcRec) > nextKey) {
					if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: catalog folder record #%d too big\n", srcDesc->numRecords-i-1);
					WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
					return E_BadNode;
				}

				srcRec->flags				= SWAP_BE16 (srcRec->flags);
				srcRec->valence				= SWAP_BE32 (srcRec->valence);
				srcRec->folderID			= SWAP_BE32 (srcRec->folderID);
				srcRec->createDate			= SWAP_BE32 (srcRec->createDate);
				srcRec->contentModDate		= SWAP_BE32 (srcRec->contentModDate);
				srcRec->attributeModDate	= SWAP_BE32 (srcRec->attributeModDate);
				srcRec->accessDate			= SWAP_BE32 (srcRec->accessDate);
				srcRec->backupDate			= SWAP_BE32 (srcRec->backupDate);

				srcRec->bsdInfo.ownerID		= SWAP_BE32 (srcRec->bsdInfo.ownerID);
				srcRec->bsdInfo.groupID		= SWAP_BE32 (srcRec->bsdInfo.groupID);

				/* Don't swap srcRec->bsdInfo.adminFlags; it's only one byte */
				/* Don't swap srcRec->bsdInfo.ownerFlags; it's only one byte */

				srcRec->bsdInfo.fileMode			= SWAP_BE16 (srcRec->bsdInfo.fileMode);
				srcRec->bsdInfo.special.iNodeNum	= SWAP_BE32 (srcRec->bsdInfo.special.iNodeNum);

				srcRec->textEncoding		= SWAP_BE32 (srcRec->textEncoding);

				/* The only field we use in srcRec->userInfo is frFlags (used in VLockedChk). */
				srcRec->userInfo.frFlags	= SWAP_BE16 (srcRec->userInfo.frFlags);

				/* Don't swap srcRec->finderInfo */
		srcRec->folderCount		= SWAP_BE32 (srcRec->folderCount);

			} else if (srcPtr[0] == kHFSPlusFileRecord) {
				HFSPlusCatalogFile *srcRec = (HFSPlusCatalogFile *)srcPtr;
				if ((char *)srcRec + sizeof(*srcRec) > nextKey) {
					if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: catalog file record #%d too big\n", srcDesc->numRecords-i-1);
					return fsBTInvalidNodeErr;
				}

				srcRec->flags				= SWAP_BE16 (srcRec->flags);

				srcRec->fileID				= SWAP_BE32 (srcRec->fileID);

				srcRec->createDate			= SWAP_BE32 (srcRec->createDate);
				srcRec->contentModDate		= SWAP_BE32 (srcRec->contentModDate);
				srcRec->attributeModDate	= SWAP_BE32 (srcRec->attributeModDate);
				srcRec->accessDate			= SWAP_BE32 (srcRec->accessDate);
				srcRec->backupDate			= SWAP_BE32 (srcRec->backupDate);

				srcRec->bsdInfo.ownerID		= SWAP_BE32 (srcRec->bsdInfo.ownerID);
				srcRec->bsdInfo.groupID		= SWAP_BE32 (srcRec->bsdInfo.groupID);

				/* Don't swap srcRec->bsdInfo.adminFlags; it's only one byte */
				/* Don't swap srcRec->bsdInfo.ownerFlags; it's only one byte */

				srcRec->bsdInfo.fileMode			= SWAP_BE16 (srcRec->bsdInfo.fileMode);
				srcRec->bsdInfo.special.iNodeNum	= SWAP_BE32 (srcRec->bsdInfo.special.iNodeNum);

				srcRec->textEncoding		= SWAP_BE32 (srcRec->textEncoding);

				srcRec->hl_firstLinkID 		= SWAP_BE32 (srcRec->hl_firstLinkID);

				srcRec->userInfo.fdType		= SWAP_BE32 (srcRec->userInfo.fdType);
				srcRec->userInfo.fdCreator	= SWAP_BE32 (srcRec->userInfo.fdCreator);
				srcRec->userInfo.fdFlags	= SWAP_BE16 (srcRec->userInfo.fdFlags);
				srcRec->userInfo.fdLocation.v = SWAP_BE16 (srcRec->userInfo.fdLocation.v);
				srcRec->userInfo.fdLocation.h = SWAP_BE16 (srcRec->userInfo.fdLocation.h);
				srcRec->userInfo.opaque		= SWAP_BE16 (srcRec->userInfo.opaque);

				/* Don't swap srcRec->finderInfo */
				/* Don't swap srcRec->reserved2 */

				hfs_swap_HFSPlusForkData (&srcRec->dataFork);
				hfs_swap_HFSPlusForkData (&srcRec->resourceFork);

			} else if ((srcPtr[0] == kHFSPlusFolderThreadRecord) ||
					   (srcPtr[0] == kHFSPlusFileThreadRecord)) {

				/*
				 * Make sure there is room for parentID and name length.
				 */
				HFSPlusCatalogThread *srcRec = (HFSPlusCatalogThread *)srcPtr;
				if ((char *) &srcRec->nodeName.unicode[0] > nextKey) {
					if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: catalog thread record #%d too big\n", srcDesc->numRecords-i-1);
					WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
					return E_BadNode;
				}

				/* Don't swap srcRec->reserved */

				srcRec->parentID						= SWAP_BE32 (srcRec->parentID);

				if (direction == kSwapBTNodeBigToHost)
					srcRec->nodeName.length	= SWAP_BE16 (srcRec->nodeName.length);

				/*
				 * Make sure there is room for the name in the buffer.
				 * Then swap the characters of the name itself.
				 */
				if ((char *) &srcRec->nodeName.unicode[srcRec->nodeName.length] > nextKey) {
					if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: catalog thread record #%d name too big\n", srcDesc->numRecords-i-1);
					WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
					return E_BadNode;
				}
				for (j = 0; j < srcRec->nodeName.length; j++) {
					srcRec->nodeName.unicode[j]	= SWAP_BE16 (srcRec->nodeName.unicode[j]);
				}

				if (direction == kSwapBTNodeHostToBig)
					srcRec->nodeName.length = SWAP_BE16 (srcRec->nodeName.length);

			} else {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: unrecognized catalog record type (0x%04X; record #%d)\n", srcPtr[0], srcDesc->numRecords-i-1);
			}

			/* We can swap the record type now that we're done using it. */
			if (direction == kSwapBTNodeHostToBig)
				srcPtr[0] = SWAP_BE16 (srcPtr[0]);

		} else if (fileID == kHFSAttributesFileID) {
			HFSPlusAttrKey *srcAttrKey = (HFSPlusAttrKey *)srcKey;
			HFSPlusAttrRecord *srcRec;
			u_int16_t keyLength;
			u_int32_t attrSize = 0;

			/* Make sure there is room in the buffer for a minimal key */
			if ((char *) &srcAttrKey->attrName[1] > nextKey) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: attr key #%d offset too big (0x%04X)\n", srcDesc->numRecords-i-1, srcOffs[i]);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
				return E_BadNode;
			}

			/* Swap the key length field */
			if (direction == kSwapBTNodeBigToHost)
				srcAttrKey->keyLength = SWAP_BE16(srcAttrKey->keyLength);
			keyLength = srcAttrKey->keyLength;	/* Keep a copy in native order */
			if (direction == kSwapBTNodeHostToBig)
				srcAttrKey->keyLength = SWAP_BE16(srcAttrKey->keyLength);

			/*
			 * Make sure that we can safely dereference the record's type field or
			 * an index node's child node number.
			 */
			srcRec = (HFSPlusAttrRecord *)(srcKey + keyLength + sizeof(srcAttrKey->keyLength));
			if ((char *)srcRec + sizeof(u_int32_t) > nextKey) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: attr key #%d too big (%d)\n", srcDesc->numRecords-i-1, keyLength);
				WriteError(fcb->fcbVolume->vcbGPtr, E_KeyLen, fcb->fcbFileID, src->blockNum);
				return E_KeyLen;
			}

			srcAttrKey->fileID = SWAP_BE32(srcAttrKey->fileID);
			srcAttrKey->startBlock = SWAP_BE32(srcAttrKey->startBlock);

			/*
			 * Swap and check the attribute name
			 */
			if (direction == kSwapBTNodeBigToHost)
				srcAttrKey->attrNameLen = SWAP_BE16(srcAttrKey->attrNameLen);
			/* Sanity check the attribute name length */
			if (srcAttrKey->attrNameLen > kHFSMaxAttrNameLen || keyLength < (kHFSPlusAttrKeyMinimumLength + sizeof(u_int16_t)*srcAttrKey->attrNameLen)) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: attr key #%d keyLength=%d attrNameLen=%d\n", srcDesc->numRecords-i-1, keyLength, srcAttrKey->attrNameLen);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
				return E_BadNode;
			}
			for (j = 0; j < srcAttrKey->attrNameLen; j++)
				srcAttrKey->attrName[j] = SWAP_BE16(srcAttrKey->attrName[j]);
			if (direction == kSwapBTNodeHostToBig)
				srcAttrKey->attrNameLen = SWAP_BE16(srcAttrKey->attrNameLen);

			/*
			 * For index nodes, the record data is just the child's node number.
			 * Skip over swapping the various types of attribute record.
			 */
			if (srcDesc->kind == kBTIndexNode) {
				*((UInt32 *)srcRec) = SWAP_BE32 (*((UInt32 *)srcRec));
				continue;
			}

			/* Swap the record data */
			if (direction == kSwapBTNodeBigToHost)
				srcRec->recordType = SWAP_BE32(srcRec->recordType);
			switch (srcRec->recordType) {
				case kHFSPlusAttrInlineData:
					/* Is there room for the inline data header? */
					if ((char *) &srcRec->attrData.attrData[0]  > nextKey) {
						if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: attr inline #%d too big\n", srcDesc->numRecords-i-1);
						WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
						return E_BadNode;
					}

					/* We're not swapping the reserved fields */

					/* Swap the attribute size */
					if (direction == kSwapBTNodeHostToBig)
						attrSize = srcRec->attrData.attrSize;
					srcRec->attrData.attrSize = SWAP_BE32(srcRec->attrData.attrSize);
					if (direction == kSwapBTNodeBigToHost)
						attrSize = srcRec->attrData.attrSize;

					/* Is there room for the inline attribute data? */
					if ((char *) &srcRec->attrData.attrData[attrSize] > nextKey) {
						if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: attr inline #%d too big (attrSize=%u)\n", srcDesc->numRecords-i-1, attrSize);
						WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
						return E_BadNode;
					}

					/* Not swapping the attribute data itself */
					break;

				case kHFSPlusAttrForkData:
					/* Is there room for the fork data record? */
					if ((char *)srcRec + sizeof(HFSPlusAttrForkData) > nextKey) {
						if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: attr fork data #%d too big\n", srcDesc->numRecords-i-1);
						WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
						return E_BadNode;
					}

					/* We're not swapping the reserved field */

					hfs_swap_HFSPlusForkData(&srcRec->forkData.theFork);
					break;

				case kHFSPlusAttrExtents:
					/* Is there room for an extent record? */
					if ((char *)srcRec + sizeof(HFSPlusAttrExtents) > nextKey) {
						if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: attr extents #%d too big\n", srcDesc->numRecords-i-1);
						WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
						return E_BadNode;
					}

					/* We're not swapping the reserved field */

					for (j = 0; j < kHFSPlusExtentDensity; j++) {
						srcRec->overflowExtents.extents[j].startBlock =
							SWAP_BE32(srcRec->overflowExtents.extents[j].startBlock);
						srcRec->overflowExtents.extents[j].blockCount =
							SWAP_BE32(srcRec->overflowExtents.extents[j].blockCount);
					}
					break;
				default:
					if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_BTNode: unrecognized attribute record type (%d)\n", srcRec->recordType);
			}
			if (direction == kSwapBTNodeHostToBig)
				srcRec->recordType = SWAP_BE32(srcRec->recordType);
		} else {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: fileID %u is not a system B-tree\n", fileID);
			exit(99);
		}
	}
    return (0);
}

static int
hfs_swap_HFSBTInternalNode (
    BlockDescriptor *src,
    SFCB *fcb,
    enum HFSBTSwapDirection direction
)
{
    HFSCatalogNodeID fileID =fcb->fcbFileID;
    BTNodeDescriptor *srcDesc = src->buffer;
    UInt16 *srcOffs = (UInt16 *)((char *)src->buffer + (src->blockSize - (srcDesc->numRecords * sizeof (UInt16))));
	const char * const nodeStart = (char *)src->buffer + sizeof(BTNodeDescriptor);
	const char * const nodeEnd = (char *)src->buffer + src->blockSize;
	const char *freeSpace;
    int32_t i;
    UInt32 j;

    /*
     * Sanity check that the record offsets are within the node itself.
     */
    if ((char *)srcOffs < nodeStart || (char *)srcOffs > nodeEnd) {
        if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: invalid record count (0x%04X)\n", srcDesc->numRecords);
        WriteError(fcb->fcbVolume->vcbGPtr, E_NRecs, fcb->fcbFileID, src->blockNum);
        return E_NRecs;
    }

	/*
	 * Check the value of the free space offset, which is just behind where our srcOffs
	 * pointer is pointing.
	 */
	freeSpace = (char *)src->buffer + srcOffs[-1];
	if (freeSpace < nodeStart || freeSpace > (char *)(srcOffs-1)) {
		if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: invalid free space offset (%X)\n",
									srcOffs[-1]);
		WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
		return E_BadNode;
	}

	/*
	 * Go through the records at each offset, validate those offsets, and do the swapping
	 */
	for (i = 0; i < srcDesc->numRecords; i++) {
		/* Get pointers to the start of the key/record we're currently checking, and the next one. */
		char *srcKey = ((char *)src->buffer + srcOffs[i]);
		char *nextKey = (char *)src->buffer + srcOffs[i-1];

		/*
		 * Make sure the start of this record is within the buffer, and that it
		 * comes before the next one.
		 */
		if (srcKey < nodeStart || srcKey >= nextKey) {
			if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSPlusBTInternalNode: invalid record offset (record #%d)\n",
										srcDesc->numRecords - i - 1);
			WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
			return E_BadNode;
		}

		/*
		 * Now we need to do different things, depending on what type of tree we're looking at.
		 */
		if (fileID == kHFSExtentsFileID) {
			HFSExtentKey *srcExtentKey = (HFSExtentKey *)srcKey;
			HFSExtentDescriptor *srcRec;
			size_t recordSize = (srcDesc->kind == kBTIndexNode) ? sizeof(UInt32) : sizeof(HFSExtentRecord);

			/*
			 * Make sure this record doesn't overlap the next one
			 */
			if (srcKey + sizeof(HFSExtentKey) + recordSize > nextKey) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: extents key #%d offset too big (0x%04X)\n", srcDesc->numRecords-i-1, srcOffs[i]);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
				return E_BadNode;
			}

			/* Don't swap srcExtentKey->keyLength (it's only one byte), but do sanity check it */
			if (srcExtentKey->keyLength != sizeof(*srcExtentKey) - sizeof(srcExtentKey->keyLength)) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: extents key #%d invalid length (%d)\n", srcDesc->numRecords-i-1, srcExtentKey->keyLength);
				WriteError(fcb->fcbVolume->vcbGPtr, E_KeyLen, fcb->fcbFileID, src->blockNum);
				return E_KeyLen;
			}

			/* Don't swap srcExtentKey->forkType; it's only one byte */

			srcExtentKey->fileID			= SWAP_BE32 (srcExtentKey->fileID);
			srcExtentKey->startBlock		= SWAP_BE16 (srcExtentKey->startBlock);

			/* Point to record data (round up to even byte boundary) */
			srcRec = (HFSExtentDescriptor *)(srcKey + ((srcExtentKey->keyLength + 2) & ~1));
			if ((char *)srcRec < nodeStart || (char *)srcRec >= (char *)(srcOffs-1)) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: invalid record offset (0x%04X)\n",
											(char *)srcRec - (char *)src->buffer);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
				return E_BadNode;
			}

			if (srcDesc->kind == kBTIndexNode) {
				/* For index nodes, the record data is just a child node number. */
				*((UInt32 *)srcRec) = SWAP_BE32 (*((UInt32 *)srcRec));
			} else {
				/* Swap the extent data */
				for (j = 0; j < kHFSExtentDensity; j++) {
					srcRec[j].startBlock	= SWAP_BE16 (srcRec[j].startBlock);
					srcRec[j].blockCount	= SWAP_BE16 (srcRec[j].blockCount);
				}
			}

		} else if (fileID == kHFSCatalogFileID || fileID == kHFSRepairCatalogFileID) {
			HFSCatalogKey *srcCatalogKey = (HFSCatalogKey *)srcKey;
			SInt16 *srcPtr;
			size_t expectedKeyLength;

			/*
			 * Make sure we can safely dereference the keyLength and parentID fields.
			 * The value 8 below is 1 bytes for keyLength + 1 byte reserved + 4 bytes
			 * for parentID + 1 byte for nodeName's length + 1 byte to round up the
			 * record start to an even offset, which forms a minimal key.
			 */
			if (srcKey + 8 > nextKey) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: catalog key #%d offset too big (0x%04X)\n", srcDesc->numRecords-i-1, srcOffs[i]);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
				return E_BadNode;
			}

			/* Don't swap srcCatalogKey->keyLength (it's only one byte), but do sanity check it */
			if (srcCatalogKey->keyLength < kHFSCatalogKeyMinimumLength || srcCatalogKey->keyLength > kHFSCatalogKeyMaximumLength) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: catalog key #%d invalid length (%d)\n", srcDesc->numRecords-i-1, srcCatalogKey->keyLength);
				WriteError(fcb->fcbVolume->vcbGPtr, E_KeyLen, fcb->fcbFileID, src->blockNum);
				return E_KeyLen;
			}

			/* Don't swap srcCatalogKey->reserved */

			srcCatalogKey->parentID			= SWAP_BE32 (srcCatalogKey->parentID);

			/* Don't swap srcCatalogKey->nodeName */

			/* Make sure the keyLength is big enough for the key's content */
			if (srcDesc->kind == kBTIndexNode)
				expectedKeyLength = sizeof(*srcCatalogKey) - sizeof(srcCatalogKey->keyLength);
			else
				expectedKeyLength = srcCatalogKey->nodeName[0] + kHFSCatalogKeyMinimumLength;
			if (srcCatalogKey->keyLength < expectedKeyLength) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: catalog record #%d keyLength=%u expected=%u\n",
											srcDesc->numRecords-i, srcCatalogKey->keyLength, expectedKeyLength);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
				return E_BadNode;
			}

			/* Point to record data (round up to even byte boundary) */
			srcPtr = (SInt16 *)(srcKey + ((srcCatalogKey->keyLength + 2) & ~1));

			/*
			 * Make sure that we can safely dereference the record's type field or
			 * and index node's child node number.
			 */
			if ((char *)srcPtr + sizeof(UInt32) > nextKey) {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: catalog key #%d too big\n", srcDesc->numRecords-i-1);
				WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
				return E_BadNode;
			}

			/*
			 * For index nodes, the record data is just the child's node number.
			 * Skip over swapping the various types of catalog record.
			 */
			if (srcDesc->kind == kBTIndexNode) {
				*((UInt32 *)srcPtr) = SWAP_BE32 (*((UInt32 *)srcPtr));
				continue;
			}

			/* Make sure the recordType is in native order before using it. */
			if (direction == kSwapBTNodeBigToHost)
				srcPtr[0] = SWAP_BE16 (srcPtr[0]);

			if (srcPtr[0] == kHFSFolderRecord) {
				HFSCatalogFolder *srcRec = (HFSCatalogFolder *)srcPtr;
				if ((char *)srcRec + sizeof(*srcRec) > nextKey) {
					if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: catalog folder record #%d too big\n", srcDesc->numRecords-i-1);
					WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
					return E_BadNode;
				}

				srcRec->flags				= SWAP_BE16 (srcRec->flags);
				srcRec->valence				= SWAP_BE16 (srcRec->valence);

				srcRec->folderID			= SWAP_BE32 (srcRec->folderID);
				srcRec->createDate			= SWAP_BE32 (srcRec->createDate);
				srcRec->modifyDate			= SWAP_BE32 (srcRec->modifyDate);
				srcRec->backupDate			= SWAP_BE32 (srcRec->backupDate);

				/* The only field we use in srcRec->userInfo is frFlags (used in VLockedChk). */
				srcRec->userInfo.frFlags	= SWAP_BE16 (srcRec->userInfo.frFlags);

				/* Don't swap srcRec->finderInfo */
				/* Don't swap resserved array */

			} else if (srcPtr[0] == kHFSFileRecord) {
				HFSCatalogFile *srcRec = (HFSCatalogFile *)srcPtr;
				if ((char *)srcRec + sizeof(*srcRec) > nextKey) {
					if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: catalog file record #%d too big\n", srcDesc->numRecords-i-1);
					WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
					return E_BadNode;
				}

				srcRec->flags				= srcRec->flags;
				srcRec->fileType			= srcRec->fileType;

				/* Don't swap srcRec->userInfo */

				srcRec->fileID				= SWAP_BE32 (srcRec->fileID);

				srcRec->dataStartBlock		= SWAP_BE16 (srcRec->dataStartBlock);
				srcRec->dataLogicalSize		= SWAP_BE32 (srcRec->dataLogicalSize);
				srcRec->dataPhysicalSize	= SWAP_BE32 (srcRec->dataPhysicalSize);

				srcRec->rsrcStartBlock		= SWAP_BE16 (srcRec->rsrcStartBlock);
				srcRec->rsrcLogicalSize		= SWAP_BE32 (srcRec->rsrcLogicalSize);
				srcRec->rsrcPhysicalSize	= SWAP_BE32 (srcRec->rsrcPhysicalSize);

				srcRec->createDate			= SWAP_BE32 (srcRec->createDate);
				srcRec->modifyDate			= SWAP_BE32 (srcRec->modifyDate);
				srcRec->backupDate			= SWAP_BE32 (srcRec->backupDate);

				/* Don't swap srcRec->finderInfo */

				srcRec->clumpSize			= SWAP_BE16 (srcRec->clumpSize);

				/* Swap the two sets of extents as an array of six (three each) UInt16 */
				for (j = 0; j < kHFSExtentDensity * 2; j++) {
					srcRec->dataExtents[j].startBlock	= SWAP_BE16 (srcRec->dataExtents[j].startBlock);
					srcRec->dataExtents[j].blockCount	= SWAP_BE16 (srcRec->dataExtents[j].blockCount);
				}

				/* Don't swap srcRec->reserved */

			} else if ((srcPtr[0] == kHFSFolderThreadRecord) ||
					   (srcPtr[0] == kHFSFileThreadRecord)) {
				HFSCatalogThread *srcRec = (HFSCatalogThread *)srcPtr;

				/* Make sure there is room for parentID and name length */
				if ((char *) &srcRec->nodeName[1] > nextKey) {
					if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: catalog thread record #%d too big\n", srcDesc->numRecords-i-1);
					WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
					return E_BadNode;
				}

				/* Don't swap srcRec->reserved array */

				srcRec->parentID			= SWAP_BE32 (srcRec->parentID);

				/* Don't swap srcRec->nodeName */

				/* Make sure there is room for the name in the buffer */
				if ((char *) &srcRec->nodeName[srcRec->nodeName[0]] > nextKey) {
					if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: catalog thread record #%d name too big\n", srcDesc->numRecords-i-1);
					WriteError(fcb->fcbVolume->vcbGPtr, E_BadNode, fcb->fcbFileID, src->blockNum);
					return E_BadNode;
				}
			} else {
				if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: unrecognized catalog record type (0x%04X; record #%d)\n", srcPtr[0], srcDesc->numRecords-i-1);
			}

			/* We can swap the record type now that we're done using it */
			if (direction == kSwapBTNodeHostToBig)
				srcPtr[0] = SWAP_BE16 (srcPtr[0]);

		} else {
		   if (state.debug) fsck_print(ctx, LOG_TYPE_INFO, "hfs_swap_HFSBTInternalNode: fileID %u is not a system B-tree\n", fileID);
		   exit(99);
		}
	}
    return (0);
}
