//
//  lf_hfs_volume_identifiers.c
//  livefiles_hfs
//
//  The routines on this file are derived from hfsutil_main.c
//
//  Created by Adam Hijaze on 04/10/2022.
//

#include <sys/disk.h>
#include <sys/types.h>
#include <sys/loadable_fs.h>

/*
 * CommonCrypto provides a more stable API than OpenSSL guarantees;
 * the #define causes it to use the same API for MD5 and SHA256, so the rest of
 * the code need not change.
 */
#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>

#include <CoreFoundation/CFString.h>
#include <System/uuid/namespace.h>

#include "lf_hfs_btree.h"
#include "lf_hfs_logger.h"
#include "lf_hfs_volume_identifiers.h"



/*
 * Create a version 3 UUID from a unique "name" in the given "name space".
 * Version 3 UUID are derived using "name" via MD5 checksum.
 *
 * Parameters:
 *    result_uuid   - resulting UUID.
 *    namespace     - namespace in which given name exists and UUID should be created.
 *    name          - unique string used to create version 3 UUID.
 *    namelen       - length of the name string.
 */
static void
uuid_create_md5_from_name(uuid_t result_uuid, const uuid_t namespace, const void *name, int namelen)
{
    CC_MD5_CTX c;

    CC_MD5_Init(&c);
    CC_MD5_Update(&c, namespace, sizeof(uuid_t));
    CC_MD5_Update(&c, name, namelen);
    CC_MD5_Final(result_uuid, &c);

    result_uuid[6] = (result_uuid[6] & 0x0F) | 0x30;
    result_uuid[8] = (result_uuid[8] & 0x3F) | 0x80;
}

typedef struct {
    BTNodeDescriptor    node;
    BTHeaderRec        header;
} __attribute__((aligned(2), packed)) HeaderRec, *HeaderPtr;

/*
 *    readAt = lseek() + read()
 *
 *    Returns: FSUR_IO_SUCCESS, FSUR_IO_FAIL
 *
 */
static ssize_t
readAt(int fd, void * bufPtr, off_t offset, ssize_t length, int blockSize)
{
    off_t lseekResult;
    ssize_t readResult;
    void *rawData = NULL;
    off_t rawOffset;
    ssize_t rawLength;
    ssize_t dataOffset = 0;
    int result = FSUR_IO_SUCCESS;
    
    /* Put offset and length in terms of device blocksize */
    rawOffset = offset / blockSize * blockSize;
    dataOffset = offset - rawOffset;
    rawLength = ((length + dataOffset + blockSize - 1) / blockSize) * blockSize;
    rawData = malloc(rawLength);
    if (rawData == NULL) {
        result = FSUR_IO_FAIL;
        goto exit;
    }

    lseekResult = lseek( fd, rawOffset, SEEK_SET );
    if ( lseekResult != rawOffset ) {
        result = FSUR_IO_FAIL;
        goto exit;
    }

    readResult = read(fd, rawData, rawLength);
    if ( readResult != rawLength ) {
        LFHFS_LOG(LEVEL_ERROR, "%s: attempt to read data from device failed (errno = %d)\n", __FUNCTION__, errno);
        result = FSUR_IO_FAIL;
        goto exit;
    }
    bcopy(rawData + dataOffset, bufPtr, length);

exit:
    if (rawData) {
        free(rawData);
    }
    return result;

} /* readAt */

/*
 *    LogicalToPhysical - Map a logical file position and size to volume-relative physical
 *    position and number of contiguous bytes at that position.
 *
 *    Inputs:
 *        logicalOffset    Logical offset in bytes from start of file
 *        length            Maximum number of bytes to map
 *        blockSize        Number of bytes per allocation block
 *        extentCount        Number of extents in file
 *        extentList        The file's extents
 *
 *    Outputs:
 *        physicalOffset    Physical offset in bytes from start of volume
 *        availableBytes    Number of bytes physically contiguous (up to length)
 *
 *    Returns: FSUR_IO_SUCCESS, FSUR_IO_FAIL
 */
static int
LogicalToPhysical(off_t offset, ssize_t length, u_int32_t blockSize,
                             u_int32_t extentCount, const HFSPlusExtentDescriptor *extentList,
                             off_t *physicalOffset, ssize_t *availableBytes)
{
    off_t        temp;
    u_int32_t    logicalBlock;
    u_int32_t    extent;
    u_int32_t    blockCount = 0;
    
    /* Determine allocation block containing logicalOffset */
    logicalBlock = (u_int32_t)(offset / blockSize);    // This can't overflow for valid volumes
    offset %= blockSize;    // Offset from start of allocation block
    
    /* Find the extent containing logicalBlock */
    for (extent = 0; extent < extentCount; ++extent)
    {
        blockCount = OSSwapBigToHostInt32(extentList[extent].blockCount);
        
        if (blockCount == 0)
            return FSUR_IO_FAIL;    // Tried to map past physical end of file
        
        if (logicalBlock < blockCount)
            break;  // Found it!
        
        logicalBlock -= blockCount;
    }

    if (extent >= extentCount)
        return FSUR_IO_FAIL;    // Tried to map past physical end of file

    /*
     *    When we get here, extentList[extent] is the extent containing logicalOffset.
     *    The desired allocation block is logicalBlock blocks into the extent.
     */
    
    /* Compute the physical starting position */
    temp = OSSwapBigToHostInt32(extentList[extent].startBlock) + logicalBlock;  // First physical block
    temp *= blockSize;  // Byte offset of first physical block
    *physicalOffset = temp + offset;

    /* Compute the available contiguous bytes. */
    temp = blockCount - logicalBlock;   // Number of blocks available in extent
    temp *= blockSize;
    temp -= offset; // Number of bytes available
    
    if (temp < length)
        *availableBytes = temp;
    else
        *availableBytes = length;
    
    return FSUR_IO_SUCCESS;
}

/*
 *    ReadFile - Read bytes from a file.  Handles cases where the starting and/or
 *    ending position are not allocation or device block aligned.
 *
 *    Inputs:
 *        fd            Descriptor for reading the volume
 *        buffer        The bytes are read into here
 *        offset        Offset in file to start reading
 *        length        Number of bytes to read
 *        volOffset     Byte offset from start of device to start of volume
 *        blockSize     Number of bytes per allocation block
 *        extentCount   Number of extents in file
 *        extentList    The file's exents
 *
 *    Returns: FSUR_IO_SUCCESS, FSUR_IO_FAIL
 */
static int
ReadFile(int fd, void *buffer, off_t offset, ssize_t length,
                    off_t volOffset, u_int32_t blockSize,
                    u_int32_t extentCount, const HFSPlusExtentDescriptor *extentList)
{
    int result = FSUR_IO_SUCCESS;
    off_t physOffset;
    ssize_t physLength;
    
    while (length > 0)
    {
        result = LogicalToPhysical(offset, length, blockSize, extentCount, extentList,
                                    &physOffset, &physLength);
        if (result != FSUR_IO_SUCCESS)
            break;
        
        result = (int)readAt(fd, buffer, volOffset+physOffset, physLength, blockSize);
        if (result != FSUR_IO_SUCCESS)
            break;
        
        length -= physLength;
        offset += physLength;
        buffer = (char *) buffer + physLength;
    }
    
    return result;
}

static int
GetBTreeNodeInfo(int fd, off_t hfsPlusVolumeOffset, u_int32_t blockSize,
                u_int32_t extentCount, const HFSPlusExtentDescriptor *extentList,
                u_int32_t *nodeSize, u_int32_t *firstLeafNode)
{
    int result;
    HeaderRec * bTreeHeaderPtr = NULL;

    bTreeHeaderPtr = (HeaderRec *) malloc(HFS_BLOCK_SIZE);
    if (bTreeHeaderPtr == NULL) {
        return (FSUR_IO_FAIL);
    }
    
    /*  Read the b-tree header node */
    result = ReadFile(fd, bTreeHeaderPtr, 0, HFS_BLOCK_SIZE,
                    hfsPlusVolumeOffset, blockSize,
                    extentCount, extentList);
    if ( result == FSUR_IO_FAIL ) {
        goto free;
    }

    if ( bTreeHeaderPtr->node.kind != kBTHeaderNode ) {
        result = FSUR_IO_FAIL;
        goto free;
    }

    *nodeSize = OSSwapBigToHostInt16(bTreeHeaderPtr->header.nodeSize);

    if (OSSwapBigToHostInt32(bTreeHeaderPtr->header.leafRecords) == 0)
        *firstLeafNode = 0;
    else
        *firstLeafNode = OSSwapBigToHostInt32(bTreeHeaderPtr->header.firstLeafNode);

free:
    if (bTreeHeaderPtr) {
        free((char*) bTreeHeaderPtr);
    }
    return result;
} /* GetBTreeNodeInfo */

static int
GetCatalogOverflowExtents(int fd, off_t hfsPlusVolumeOffset,
        HFSPlusVolumeHeader *volHdrPtr,
        HFSPlusExtentDescriptor **catalogExtents,
        u_int32_t *catalogExtCount)
{
    off_t offset;
    u_int32_t numRecords;
    u_int32_t nodeSize;
    u_int32_t leafNode;
    u_int32_t blockSize;
    BTNodeDescriptor *bTreeNodeDescriptorPtr;
    HFSPlusExtentDescriptor *extents;
    size_t listsize;
    char *bufPtr = NULL;
    uint32_t i;
    int result;

    blockSize = OSSwapBigToHostInt32(volHdrPtr->blockSize);
    listsize = *catalogExtCount * sizeof(HFSPlusExtentDescriptor);
    extents = *catalogExtents;
    offset = (off_t)OSSwapBigToHostInt32(volHdrPtr->extentsFile.extents[0].startBlock) *
            (off_t)blockSize;

    /* Read the header node of the extents B-Tree */
    result = GetBTreeNodeInfo(fd, hfsPlusVolumeOffset, blockSize,
            kHFSPlusExtentDensity, volHdrPtr->extentsFile.extents,
            &nodeSize, &leafNode);
    if (result != FSUR_IO_SUCCESS || leafNode == 0)
        goto exit;

    /* Calculate the logical position of the first leaf node */
    offset = (off_t) leafNode * (off_t) nodeSize;

    /* Read the first leaf node of the extents b-tree */
    bufPtr = (char *)malloc(nodeSize);
    if (! bufPtr) {
        result = FSUR_IO_FAIL;
        goto exit;
    }

    bTreeNodeDescriptorPtr = (BTNodeDescriptor *)bufPtr;

again:
    result = ReadFile(fd, bufPtr, offset, nodeSize,
                    hfsPlusVolumeOffset, blockSize,
                    kHFSPlusExtentDensity, volHdrPtr->extentsFile.extents);
    if ( result == FSUR_IO_FAIL ) {
        goto exit;
    }

    if (bTreeNodeDescriptorPtr->kind != kBTLeafNode) {
        result = FSUR_IO_FAIL;
        goto exit;
    }

    numRecords = OSSwapBigToHostInt16(bTreeNodeDescriptorPtr->numRecords);
    for (i = 1; i <= numRecords; ++i) {
        u_int16_t * v;
        char * p;
        HFSPlusExtentKey * k;

        /*
         * Get the offset (in bytes) of the record from the
         * list of offsets at the end of the node
         */
        p = bufPtr + nodeSize - (sizeof(u_int16_t) * i);
        v = (u_int16_t *)p;

        /* Get a pointer to the record */
        p = bufPtr + OSSwapBigToHostInt16(*v); /* pointer arithmetic in bytes */
        k = (HFSPlusExtentKey *)p;

        if (OSSwapBigToHostInt32(k->fileID) != kHFSCatalogFileID)
            goto exit;

        /* Grow list and copy additional extents */
        listsize += sizeof(HFSPlusExtentRecord);
        extents = (HFSPlusExtentDescriptor *) realloc(extents, listsize);
        bcopy(p + OSSwapBigToHostInt16(k->keyLength) + sizeof(u_int16_t),
            &extents[*catalogExtCount], sizeof(HFSPlusExtentRecord));

        *catalogExtCount += kHFSPlusExtentDensity;
        *catalogExtents = extents;
    }
    
    if ((leafNode = OSSwapBigToHostInt32(bTreeNodeDescriptorPtr->fLink)) != 0) {
    
        offset = (off_t) leafNode * (off_t) nodeSize;
        
        goto again;
    }

exit:
    if (bufPtr) {
        free(bufPtr);
    }
    return (result);
}

/*
 * Convert an HFS+ UUID in binary form to a full UUID
 *
 * Assumes that the HFS UUID argument is stored in native endianness
 * If the input UUID is zeroes, then it will emit a NULL'd out UUID.
 */
void
hfs_ConvertHFSUUIDToUUID (hfs_UUID_t *hfsuuid, volUUID_t *uu)
{
    uint8_t rawUUID[8];

    /* If either high or low is 0, then return the NULL uuid */
    if ((hfsuuid->high == 0) || (hfsuuid->low == 0)) {
        uuid_clear (uu->uuid);
        return;
    }
    /*
     * If the input UUID was not zeroes, then run it through the normal md5
     *
     * NOTE: When using MD5 to compute the "full" UUID, we must pass in the
     * big-endian values of the two 32-bit fields.  In the kernel, HFS uses the
     * raw 4-byte fields of the finderinfo directly, without running them through
     * an endian-swap.  As a result, we must endian-swap back to big endian here.
     */
    ((uint32_t*)rawUUID)[0] = OSSwapHostToBigInt32(hfsuuid->high);
    ((uint32_t*)rawUUID)[1] = OSSwapHostToBigInt32(hfsuuid->low);
    uuid_create_md5_from_name(uu->uuid, kFSUUIDNamespaceSHA1, rawUUID, sizeof(rawUUID));
}

/*
 *    GetEmbeddedHFSPlusVol
 *
 *    In: hfsMasterDirectoryBlockPtr
 *    Out: startOffsetPtr - the disk offset at which the HFS+ volume starts
                 (that is, 2 blocks before the volume header)
 *
 */
static int
GetEmbeddedHFSPlusVol (HFSMasterDirectoryBlock * hfsMasterDirectoryBlockPtr, off_t * startOffsetPtr)
{
    int        result = FSUR_IO_SUCCESS;
    u_int32_t    allocationBlockSize, firstAllocationBlock, startBlock, blockCount;

    if (OSSwapBigToHostInt16(hfsMasterDirectoryBlockPtr->drSigWord) != kHFSSigWord) {
        result = FSUR_UNRECOGNIZED;
        goto exit;
    }

    allocationBlockSize = OSSwapBigToHostInt32(hfsMasterDirectoryBlockPtr->drAlBlkSiz);
    firstAllocationBlock = OSSwapBigToHostInt16(hfsMasterDirectoryBlockPtr->drAlBlSt);

    if (OSSwapBigToHostInt16(hfsMasterDirectoryBlockPtr->drEmbedSigWord) != kHFSPlusSigWord) {
        result = FSUR_UNRECOGNIZED;
        goto exit;
    }

    startBlock = OSSwapBigToHostInt16(hfsMasterDirectoryBlockPtr->drEmbedExtent.startBlock);
    blockCount = OSSwapBigToHostInt16(hfsMasterDirectoryBlockPtr->drEmbedExtent.blockCount);

    if ( startOffsetPtr ) {
        *startOffsetPtr = ((u_int64_t)startBlock * (u_int64_t)allocationBlockSize) +
            ((u_int64_t)firstAllocationBlock * (u_int64_t)HFS_BLOCK_SIZE);
    }
exit:
        return result;

}

/*
    ReadHeaderBlock
    
    Read the Master Directory Block or Volume Header Block from an HFS,
    HFS Plus, or HFSX volume into a caller-supplied buffer.  Return the
    offset of an embedded HFS Plus volume (or 0 if not embedded HFS Plus).
    Return a pointer to the volume UUID in the Finder Info.

    Returns: FSUR_IO_SUCCESS, FSUR_IO_FAIL, FSUR_UNRECOGNIZED
*/
static int
ReadHeaderBlock(int fd, void *bufPtr, off_t *startOffset, hfs_UUID_t **finderInfoUUIDPtr, int blockSize)
{
    int result;
    HFSMasterDirectoryBlock * mdbPtr;
    HFSPlusVolumeHeader * volHdrPtr;

    mdbPtr = bufPtr;
    volHdrPtr = bufPtr;

    /* Read the HFS Master Directory Block or Volume Header from sector 2 */
    *startOffset = 0;
    result = (int)readAt(fd, bufPtr, (off_t)(2 * HFS_BLOCK_SIZE), HFS_BLOCK_SIZE, blockSize);
    if (result != FSUR_IO_SUCCESS) {
        goto exit;
    }

    /*
     * If this is a wrapped HFS Plus volume, read the Volume Header from
     * sector 2 of the embedded volume.
     */
    if (OSSwapBigToHostInt16(mdbPtr->drSigWord) == kHFSSigWord &&
        OSSwapBigToHostInt16(mdbPtr->drEmbedSigWord) == kHFSPlusSigWord) {
        result = GetEmbeddedHFSPlusVol(mdbPtr, startOffset);
        if (result != FSUR_IO_SUCCESS) {
            goto exit;
        }
        result = (int)readAt(fd, bufPtr, *startOffset + (off_t)(2*HFS_BLOCK_SIZE), HFS_BLOCK_SIZE, blockSize);
        if (result != FSUR_IO_SUCCESS) {
            goto exit;
        }
    }
    
    /*
     * At this point, we have the MDB for plain HFS, or VHB for HFS Plus and HFSX
     * volumes (including wrapped HFS Plus).  Verify the signature and grab the
     * UUID from the Finder Info.
     */
    if (OSSwapBigToHostInt16(mdbPtr->drSigWord) == kHFSSigWord) {
        *finderInfoUUIDPtr = (hfs_UUID_t *)(&mdbPtr->drFndrInfo[6]);
    } else if (OSSwapBigToHostInt16(volHdrPtr->signature) == kHFSPlusSigWord ||
                OSSwapBigToHostInt16(volHdrPtr->signature) == kHFSXSigWord) {
        *finderInfoUUIDPtr = (hfs_UUID_t *)&volHdrPtr->finderInfo[24];
    } else {
        result = FSUR_UNRECOGNIZED;
    }

exit:
    return result;
}

int
hfs_GetVolumeUUIDRaw(int fd, volUUID_t *volUUIDPtr, int devBlockSize)
{
    int result;
    char * bufPtr;
    off_t startOffset;
    hfs_UUID_t *finderInfoUUIDPtr;
    hfs_UUID_t hfs_uuid;
    volUUID_t fullUUID;
    
    bufPtr = (char *)malloc(HFS_BLOCK_SIZE);
    if ( ! bufPtr ) {
        result = FSUR_UNRECOGNIZED;
        goto exit;
    }
    
    /* Get the pointer to the volume UUID in the Finder Info */
    result = ReadHeaderBlock(fd, bufPtr, &startOffset, &finderInfoUUIDPtr, devBlockSize);
    if (result != FSUR_IO_SUCCESS) {
        goto exit;
    }

    /*
     * Copy the volume UUID out of the Finder Info. Note that the FinderInfo
     * stores the UUID in big-endian so we have to convert to native
     * endianness.
     */
    hfs_uuid.high = OSSwapBigToHostInt32(finderInfoUUIDPtr->high);
    hfs_uuid.low = OSSwapBigToHostInt32(finderInfoUUIDPtr->low);

    /*
     * Now convert to a full UUID using the same algorithm as HFS+
     * This makes sure to construct a full NULL-UUID if necessary.
     */
    hfs_ConvertHFSUUIDToUUID (&hfs_uuid, &fullUUID);

    /* Copy it out into the caller's buffer */
    uuid_copy(volUUIDPtr->uuid, fullUUID.uuid);

exit:
    if (bufPtr) {
        free(bufPtr);
    }
    return (result == FSUR_IO_SUCCESS) ? FSUR_IO_SUCCESS : FSUR_IO_FAIL;
}

int
hfs_GetNameFromHFSPlusVolumeStartingAt(int fd, off_t hfsPlusVolumeOffset, unsigned char * name_o, int devBlockSize)
{
    int result = FSUR_IO_SUCCESS;
    u_int32_t blockSize;
    char *bufPtr = NULL;
    HFSPlusVolumeHeader *volHdrPtr;
    BTNodeDescriptor *bTreeNodeDescriptorPtr;
    u_int32_t catalogNodeSize;
    u_int32_t leafNode;
    u_int32_t catalogExtCount;
    HFSPlusExtentDescriptor *catalogExtents = NULL;

    volHdrPtr = (HFSPlusVolumeHeader *)malloc(HFS_BLOCK_SIZE);
    if ( ! volHdrPtr ) {
        result = FSUR_IO_FAIL;
        goto exit;
    }

    /*
     * Read the Volume Header
     * (This is a little redundant for a pure, unwrapped HFS+ volume)
     */
    result = (int)readAt( fd, volHdrPtr, hfsPlusVolumeOffset + (off_t)(2*HFS_BLOCK_SIZE), HFS_BLOCK_SIZE, devBlockSize);
    if (result == FSUR_IO_FAIL) {
        goto exit; // return FSUR_IO_FAIL
    }

    /* Verify that it is an HFS+ volume */
    if (OSSwapBigToHostInt16(volHdrPtr->signature) != kHFSPlusSigWord &&
        OSSwapBigToHostInt16(volHdrPtr->signature) != kHFSXSigWord) {
        result = FSUR_IO_FAIL;
        goto exit;
    }

    blockSize = OSSwapBigToHostInt32(volHdrPtr->blockSize);
    catalogExtents = (HFSPlusExtentDescriptor *) malloc(sizeof(HFSPlusExtentRecord));
    if ( ! catalogExtents ) {
        result = FSUR_IO_FAIL;
        goto exit;
    }
    bcopy(volHdrPtr->catalogFile.extents, catalogExtents, sizeof(HFSPlusExtentRecord));
    catalogExtCount = kHFSPlusExtentDensity;

    /* If there are overflow catalog extents, then go get them */
    if (OSSwapBigToHostInt32(catalogExtents[7].blockCount) != 0) {
        result = GetCatalogOverflowExtents(fd, hfsPlusVolumeOffset, volHdrPtr, &catalogExtents, &catalogExtCount);
        if (result != FSUR_IO_SUCCESS)
            goto exit;
    }

    /* Read the header node of the catalog B-Tree */
    result = GetBTreeNodeInfo(fd, hfsPlusVolumeOffset, blockSize,
                            catalogExtCount, catalogExtents,
                            &catalogNodeSize, &leafNode);
    if (result != FSUR_IO_SUCCESS)
        goto exit;

    /* Read the first leaf node of the catalog b-tree */
    bufPtr = (char *)malloc(catalogNodeSize);
    if ( ! bufPtr ) {
        result = FSUR_IO_FAIL;
        goto exit;
    }

    bTreeNodeDescriptorPtr = (BTNodeDescriptor *)bufPtr;

    result = ReadFile(fd, bufPtr, (off_t) leafNode * (off_t) catalogNodeSize, catalogNodeSize,
                        hfsPlusVolumeOffset, blockSize,
                        catalogExtCount, catalogExtents);
    if (result == FSUR_IO_FAIL) {
        goto exit; // return FSUR_IO_FAIL
    }

    {
        u_int16_t *v;
        char *p;
        HFSPlusCatalogKey *k;
        CFStringRef cfstr;

        if ( OSSwapBigToHostInt16(bTreeNodeDescriptorPtr->numRecords) < 1) {
            result = FSUR_IO_FAIL;
            goto exit;
        }

        /* Get the offset (in bytes) of the first record from the list of offsets at the end of the node. */
        p = bufPtr + catalogNodeSize - sizeof(u_int16_t); // pointer arithmetic in bytes
        v = (u_int16_t *)p;

        /* Get a pointer to the first record. */
        p = bufPtr + OSSwapBigToHostInt16(*v); // pointer arithmetic in bytes
        k = (HFSPlusCatalogKey *)p;

        /* There should be only one record whose parent is the root parent.  It should be the first record. */
        if (OSSwapBigToHostInt32(k->parentID) != kHFSRootParentID) {
            result = FSUR_IO_FAIL;
            goto exit;
        }

        if ((OSSwapBigToHostInt16(k->nodeName.length) >
            (sizeof(k->nodeName.unicode) / sizeof(k->nodeName.unicode[0]))) ||
            OSSwapBigToHostInt16(k->nodeName.length) > 255) {
            result = FSUR_IO_FAIL;
            goto exit;
        }

        /* Extract the name of the root directory */
        {
            HFSUniStr255 *swapped;
            int i;
        
            swapped = (HFSUniStr255 *)malloc(sizeof(HFSUniStr255));
            if (swapped == NULL) {
            result = FSUR_IO_FAIL;
            goto exit;
            }
            swapped->length = OSSwapBigToHostInt16(k->nodeName.length);
            
            for (i=0; i<swapped->length; i++) {
            swapped->unicode[i] = OSSwapBigToHostInt16(k->nodeName.unicode[i]);
            }
            swapped->unicode[i] = 0;
            cfstr = CFStringCreateWithCharacters(kCFAllocatorDefault, swapped->unicode, swapped->length);
            (void) CFStringGetCString(cfstr, (char *)name_o, NAME_MAX * 3 + 1, kCFStringEncodingUTF8);
            CFRelease(cfstr);
            free(swapped);
        }
    }

    result = FSUR_IO_SUCCESS;

exit:
    if (volHdrPtr) {
        free((char*) volHdrPtr);
    }
    if (catalogExtents) {
        free((char*) catalogExtents);
    }
    if (bufPtr) {
        free((char*)bufPtr);
    }
    return result;

} /* hfs_GetNameFromHFSPlusVolumeStartingAt */
