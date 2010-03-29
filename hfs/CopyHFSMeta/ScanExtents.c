#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "hfsmeta.h"
#include "Data.h"

/*
 * Functions to scan through the extents overflow file, grabbing
 * overflow extents for the special files.
 */

/*
 * Given an extent record, return the logical block address (in the volume)
 * for the requested block offset into the file.  It returns 0 if it can't
 * find it.
 */
static unsigned int
FindBlock(HFSPlusExtentRecord *erp, unsigned int blockNum)
{
	unsigned int lba = 0;
	unsigned int base = 0;
	HFSPlusExtentDescriptor *ep = &(*erp)[0];
	int i;

	for (i = 0; i < kHFSPlusExtentDensity; i++) {
		if (ep->startBlock == 0 || ep->blockCount == 0)
			break;
		if ((base + S32(ep->blockCount)) > blockNum) {
			lba = S32(ep->startBlock) + (blockNum - base);
			break;
		}
		base += S32(ep->blockCount);
		ep++;
	}
	return lba;
}

/*
 * Get the given node from the extents-overflow file.  Returns -1 on error, and
 * 0 on success.
 */
static int
GetNode(DeviceInfo_t *devp, HFSPlusVolumeHeader *hp, int nodeNum, int blocksPerNode, void *nodePtr)
{
	int retval = 0;
	unsigned char *ptr, *endPtr;
	unsigned int offset;
	HFSPlusExtentRecord *erp = &hp->extentsFile.extents;

	ptr = nodePtr;
	endPtr = ptr + (blocksPerNode * S32(hp->blockSize));
	offset = nodeNum * blocksPerNode;

	/*
	 * We have two block sizes to consider here.  The device blocksize, and the
	 * btree node size.
	 */
	while (ptr < endPtr) {
		ssize_t rv;
		off_t lba;
		int i;


		lba = FindBlock(erp, offset);
		if (lba == 0) {
			warnx("Cannot find block %u in extents overflow file", offset);
			return -1;
		}
		lba = lba * S32(hp->blockSize);
		for (i = 0; i < S32(hp->blockSize) / devp->blockSize; i++) {
//			printf("Trying to get block %lld\n", lba + i);
			rv = GetBlock(devp, lba + (i * devp->blockSize), ptr);
			if (rv == -1) {
				warnx("Cannot read block %u in extents overflow file", lba + i);
				return -1;
			}
			ptr += devp->blockSize;
		}
		offset++;
	}

done:
	return retval;
}

/*
 * Scan through an extentes overflow node, looking for File ID's less than
 * the first user file ID.  For each one it finds, it adds the extents to
 * the volume structure list.  It returns the number of the next node
 * (which will be 0 when we've hit the end of the list); it also returns 0
 * when it encounters a CNID larger than the system files'.
 */
static unsigned int
ScanNode(VolumeObjects_t *vop, uint8_t *nodePtr, size_t nodeSize, off_t blockSize)
{
	u_int16_t *offsetPtr;
	BTNodeDescriptor *descp;
	int indx;
	int numRecords;
	HFSPlusExtentKey *keyp;
	HFSPlusExtentRecord *datap;
	uint8_t *recp;
	unsigned int retval;

	descp = (BTNodeDescriptor*)nodePtr;

	if (descp->kind != kBTLeafNode)
		return 0;

	numRecords = S16(descp->numRecords);
	offsetPtr = (u_int16_t*)((uint8_t*)nodePtr + nodeSize);

	retval = S32(descp->fLink);
	for (indx = 1; indx <= numRecords; indx++) {
		int recOffset = S16(offsetPtr[-indx]);
		recp = nodePtr + recOffset;
		if (recp > (nodePtr + nodeSize)) {
			return -1;	// Corrupt node
		}
		keyp = (HFSPlusExtentKey*)recp;
		datap = (HFSPlusExtentRecord*)(recp + sizeof(HFSPlusExtentKey));
//		printf("Node index #%d:  fileID = %u\n", indx, S32(keyp->fileID));
		if (S32(keyp->fileID) >= kHFSFirstUserCatalogNodeID) {
			if (debug) printf("Done scanning extents overflow file\n");
			retval = 0;
			break;
		} else {
			int i;
			for (i = 0; i < kHFSPlusExtentDensity; i++) {
				off_t start = S32((*datap)[i].startBlock) * (off_t)blockSize;
				off_t len = S32((*datap)[i].blockCount) * (off_t)blockSize;
				if (start && len)
					AddExtent(vop, start, len);
			}
		}
	}

	return retval;
}

/*
 * Given a volme structure list, scan through the extents overflow file
 * looking for system-file extents (those with a CNID < 16).  If useAltHdr
 * is set, it'll use the extents overflow descriptor in the alternate header.
 */
int
ScanExtents(VolumeObjects_t *vop, int useAltHdr)
{
	int retval = -1;
	ssize_t rv;
	char buffer[vop->devp->blockSize];
	struct RootNode {
		BTNodeDescriptor desc;
		BTHeaderRec header;
	} *headerNode;
	HFSPlusVolumeHeader *hp;
	HFSPlusExtentRecord *erp;
	off_t vBlockSize;
	size_t tBlockSize;
	int blocksPerNode;
	void *nodePtr = NULL;
	unsigned int nodeNum = 0;

	hp = useAltHdr ? &vop->vdp->altHeader : & vop->vdp->priHeader;
	vBlockSize = S32(hp->blockSize);

	rv = GetBlock(vop->devp, S32(hp->extentsFile.extents[0].startBlock) * vBlockSize, buffer);
	if (rv == -1) {
		warnx("Cannot get btree header node for extents file for %s header", useAltHdr ? "alternate" : "primary");
		retval = -1;
		goto done;
	}
	headerNode = (struct RootNode*)buffer;

	if (headerNode->desc.kind != kBTHeaderNode) {
		warnx("Root node is not a header node (%x)", headerNode->desc.kind);
		goto done;
	}

	tBlockSize = S16(headerNode->header.nodeSize);
	blocksPerNode = tBlockSize / vBlockSize;

	nodePtr = malloc(tBlockSize);
	if (nodePtr == NULL) {
		warn("cannot allocate buffer for node");
		goto done;
	}
	nodeNum = S32(headerNode->header.firstLeafNode);

	if (debug) printf("nodenum = %u\n", nodeNum);

	/*
	 * Iterate through the leaf nodes.
	 */
	while (nodeNum != 0) {
		if (debug) printf("Getting node %u\n", nodeNum);

		rv = GetNode(vop->devp, hp, nodeNum, blocksPerNode, nodePtr);
		if (rv == -1) {
			warnx("Cannot get node %u", nodeNum);
			retval = -1;
			goto done;
		}
		nodeNum = ScanNode(vop, nodePtr, tBlockSize, vBlockSize);
	}
	retval = 0;

done:
	if (nodePtr)
		free(nodePtr);
	return retval;

}
