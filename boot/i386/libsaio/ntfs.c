/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2004 Apple Computer, Inc.  All Rights Reserved.
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

#include "libsaio.h"
#include "sl.h"

#define BYTE_ORDER_MARK	0xFEFF

#include "ntfs_private.h"

#define FS_TYPE			"ntfs"
#define FS_NAME_FILE		"NTFS"

#define MAX_BLOCK_SIZE		2048
#define MAX_CLUSTER_SIZE	32768

#define LABEL_LENGTH	1024
#define UNKNOWN_LABEL	"Untitled NTFS"

#define FSUR_IO_FAIL -1
#define FSUR_UNRECOGNIZED -1
#define FSUR_RECOGNIZED 0

#define ERROR -1

/*
 * Process per-sector "fixups" that NTFS uses to detect corruption of
 * multi-sector data structures, like MFT records.
 */
static int
ntfs_fixup(
            char *buf,
            size_t len,
            u_int32_t magic,
            u_int32_t bytesPerSector)
{
	struct fixuphdr *fhp = (struct fixuphdr *) buf;
	int             i;
	u_int16_t       fixup;
	u_int16_t      *fxp;
	u_int16_t      *cfxp;
        u_int32_t	fixup_magic;
        u_int16_t	fixup_count;
        u_int16_t	fixup_offset;
        
        fixup_magic = OSReadLittleInt32(&fhp->fh_magic,0);
	if (fixup_magic != magic) {
		error("ntfs_fixup: magic doesn't match: %08x != %08x\n",
		       fixup_magic, magic);
		return (ERROR);
	}
        fixup_count = OSReadLittleInt16(&fhp->fh_fnum,0);
	if ((fixup_count - 1) * bytesPerSector != len) {
		error("ntfs_fixup: " \
		       "bad fixups number: %d for %ld bytes block\n", 
		       fixup_count, (long)len);	/* XXX printf kludge */
		return (ERROR);
	}
        fixup_offset = OSReadLittleInt16(&fhp->fh_foff,0);
	if (fixup_offset >= len) {
		error("ntfs_fixup: invalid offset: %x", fixup_offset);
		return (ERROR);
	}
	fxp = (u_int16_t *) (buf + fixup_offset);
	cfxp = (u_int16_t *) (buf + bytesPerSector - 2);
	fixup = *fxp++;
	for (i = 1; i < fixup_count; i++, fxp++) {
		if (*cfxp != fixup) {
			error("ntfs_fixup: fixup %d doesn't match\n", i);
			return (ERROR);
		}
		*cfxp = *fxp;
		((caddr_t) cfxp) += bytesPerSector;
	}
	return (0);
}

/*
 * Find a resident attribute of a given type.  Returns a pointer to the
 * attribute data, and its size in bytes.
 */
static int
ntfs_find_attr(
                char *buf,
                u_int32_t attrType,
                void **attrData,
                size_t *attrSize)
{
    struct filerec *filerec;
    struct attr *attr;
    u_int16_t offset;
    
    filerec = (struct filerec *) buf;
    offset = OSReadLittleInt16(&filerec->fr_attroff,0);
    attr = (struct attr *) (buf + offset);
    
    /*ее Should we also check offset < buffer size? */
    while (attr->a_hdr.a_type != 0xFFFFFFFF)	/* same for big/little endian */
    {
        if (OSReadLittleInt32(&attr->a_hdr.a_type,0) == attrType)
        {
            if (attr->a_hdr.a_flag != 0)
            {
                //verbose("NTFS: attriubte 0x%X is non-resident\n", attrType);
                return 1;
            }
            
            *attrSize = OSReadLittleInt16(&attr->a_r.a_datalen,0);
            *attrData = buf + offset + OSReadLittleInt16(&attr->a_r.a_dataoff,0);
            return 0;	/* found it! */
        }
        
        /* Skip to the next attribute */
        offset += OSReadLittleInt32(&attr->a_hdr.reclen,0);
        attr = (struct attr *) (buf + offset);
    }
    
    return 1;	/* No matching attrType found */
}

static int
memcmp(char *p1, char *p2, int len)
{
    while (len--) {
        if (*p1++ != *p2++)
            return -1;
    }
    return 0;
}

/*
 * Examine a volume to see if we recognize it as a mountable.
 */
void
NTFSGetDescription(CICell ih, char *str, long strMaxLen)
{
    struct bootfile *boot;
    unsigned bytesPerSector;
    unsigned sectorsPerCluster;
    int mftRecordSize;
    u_int64_t totalClusters;
    u_int64_t cluster, mftCluster;
    size_t mftOffset;
    void *nameAttr;
    size_t nameSize;
    char *buf;

    buf = (char *)malloc(MAX_CLUSTER_SIZE);
    if (buf == 0) {
        goto error;
    }

    /*
     * Read the boot sector, check signatures, and do some minimal
     * sanity checking.  NOTE: the size of the read below is intended
     * to be a multiple of all supported block sizes, so we don't
     * have to determine or change the device's block size.
     */
    Seek(ih, 0);
    Read(ih, (long)buf, MAX_BLOCK_SIZE);

    boot = (struct bootfile *) buf;
    
    /*
     * The first three bytes are an Intel x86 jump instruction.  I assume it
     * can be the same forms as DOS FAT:
     *    0xE9 0x?? 0x??
     *    0xEC 0x?? 0x90
     * where 0x?? means any byte value is OK.
     */
    if (boot->reserved1[0] != 0xE9
        && (boot->reserved1[0] != 0xEB || boot->reserved1[2] != 0x90))
    {
        goto error;
    }

    /*
     * Check the "NTFS    " signature.
     */
    if (memcmp(boot->bf_sysid, "NTFS    ", 8) != 0)
    {
        goto error;
    }

    /*
     * Make sure the bytes per sector and sectors per cluster are
     * powers of two, and within reasonable ranges.
     */
    bytesPerSector = OSReadLittleInt16(&boot->bf_bps,0);
    if ((bytesPerSector & (bytesPerSector-1)) || bytesPerSector < 512 || bytesPerSector > 32768)
    {
        //verbose("NTFS: invalid bytes per sector (%d)\n", bytesPerSector);
        goto error;
    }

    sectorsPerCluster = boot->bf_spc;	/* Just one byte; no swapping needed */
    if ((sectorsPerCluster & (sectorsPerCluster-1)) || sectorsPerCluster > 128)
    {
        //verbose("NTFS: invalid sectors per cluster (%d)\n", bytesPerSector);
        goto error;
    }
    
    /*
     * Calculate the number of clusters from the number of sectors.
     * Then bounds check the $MFT and $MFTMirr clusters.
     */
    totalClusters = OSReadLittleInt64(&boot->bf_spv,0) / sectorsPerCluster;
    mftCluster = OSReadLittleInt64(&boot->bf_mftcn,0);
    if (mftCluster > totalClusters)
    {
        ////verbose("NTFS: invalid $MFT cluster (%lld)\n", mftCluster);
        goto error;
    }
    cluster = OSReadLittleInt64(&boot->bf_mftmirrcn,0);
    if (cluster > totalClusters)
    {
        //verbose("NTFS: invalid $MFTMirr cluster (%lld)\n", cluster);
        goto error;
    }
    
    /*
     * Determine the size of an MFT record.
     */
    mftRecordSize = (int8_t) boot->bf_mftrecsz;
    if (mftRecordSize < 0)
        mftRecordSize = 1 << -mftRecordSize;
    else
        mftRecordSize *= bytesPerSector * sectorsPerCluster;
    //verbose("NTFS: MFT record size = %d\n", mftRecordSize);

    /*
     * Read the MFT record for $Volume.  This assumes the first four
     * file records in the MFT are contiguous; if they aren't, we
     * would have to map the $MFT itself.
     *
     * This will fail if the device sector size is larger than the
     * MFT record size, since the $Volume record won't be aligned
     * on a sector boundary.
     */
    mftOffset = mftCluster * sectorsPerCluster * bytesPerSector;
    mftOffset += mftRecordSize * NTFS_VOLUMEINO;

    Seek(ih, mftOffset);
    Read(ih, (long)buf, mftRecordSize);
#if UNUSED
    if (lseek(fd, mftOffset, SEEK_SET) == -1)
    {
        //verbose("NTFS: lseek to $Volume failed: %s\n", strerror(errno));
        goto error;
    }
    if (read(fd, buf, mftRecordSize) != mftRecordSize)
    {
        //verbose("NTFS: error reading MFT $Volume record: %s\n",
                strerror(errno));
        goto error;
    }
#endif

    if (ntfs_fixup(buf, mftRecordSize, NTFS_FILEMAGIC, bytesPerSector) != 0)
    {
        //verbose("NTFS: block fixup failed\n");
        goto error;
    }
    
    /*
     * Loop over the attributes, looking for $VOLUME_NAME (0x60).
     */
    if(ntfs_find_attr(buf, NTFS_A_VOLUMENAME, &nameAttr, &nameSize) != 0)
    {
        //verbose("NTFS: $VOLUME_NAME attribute not found\n");
        goto error;
    }
    
    str[0] = '\0';

    utf_encodestr( nameAttr, nameSize / 2, str, strMaxLen, OSLittleEndian );

    free(buf);
    return;

 error:
    if (buf) free(buf);
    return;
}

