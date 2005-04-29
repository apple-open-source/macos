/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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
/*
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "libsaio.h"
#include "sl.h"

#include "msdos_private.h"
#include "msdos.h"

#define LABEL_LENGTH		11
#define MAX_DOS_BLOCKSIZE	2048

#define	CLUST_FIRST	2		/* first legal cluster number */
#define	CLUST_RSRVD	0xfffffff6	/* reserved cluster range */


#define false 0
#define true 1

#if DEBUG
#define DLOG(x) { outb(0x80, (x)); getc(); }
#else
#define DLOG(x)
#endif

#if UNUSED
/*
 * Check a volume label.
 */
static int
oklabel(const char *src)
{
    int c, i;

    for (i = 0, c = 0; i <= 11; i++) {
        c = (u_char)*src++;
        if (c < ' ' + !i || strchr("\"*+,./:;<=>?[\\]|", c))
            break;
    }
    return i && !c;
}
#endif /* UNUSED */

/* Fix up volume label. */
static void
fixLabel(char *label, char *str, long strMaxLen)
{
    int			i;
    //unsigned char	labelUTF8[LABEL_LENGTH*3];

    /* Convert leading 0x05 to 0xE5 for multibyte languages like Japanese */
    if (label[0] == 0x05)
        label[0] = 0xE5;

#if UNUSED
    /* Check for illegal characters */
    if (!oklabel(label))
        label[0] = 0;
#endif /* UNUSED */

    /* Remove any trailing spaces */
    for (i=LABEL_LENGTH-1; i>=0; --i) {
        if (label[i] == ' ')
            label[i] = 0;
        else
            break;
    }

    /* TODO: Convert it to UTF-8 from DOSLatin1 encoding */
    strncpy(str, label, strMaxLen);
}


void
MSDOSGetDescription(CICell ih, char *str, long strMaxLen)
{
    struct direntry     *dirp;
    union bootsector    *bsp;
    struct bpb33        *b33;
    struct bpb50        *b50;
    struct bpb710       *b710;
    u_int16_t	        bps;
    int8_t		spc;
    int 		rootDirSectors;
    int 		i, finished;
    char                *buf;
    unsigned char	label[LABEL_LENGTH+1];

    DLOG(0);
    buf = (char *)malloc(MAX_DOS_BLOCKSIZE);
    if (buf == 0) goto error;
    
    DLOG(1);
    /*
     * Read the boot sector of the filesystem, and then check the
     * boot signature.  If not a dos boot sector then error out.
     *
     * NOTE: 2048 is a maximum sector size in current...
     */
    Seek(ih, 0);
    Read(ih, (long)buf, MAX_DOS_BLOCKSIZE);

    DLOG(2);
    bsp = (union bootsector *)buf;
    b33 = (struct bpb33 *)bsp->bs33.bsBPB;
    b50 = (struct bpb50 *)bsp->bs50.bsBPB;
    b710 = (struct bpb710 *)bsp->bs710.bsBPB;
    
    DLOG(3);
    /* [2699033]
     *
     * The first three bytes are an Intel x86 jump instruction.  It should be one
     * of the following forms:
     *    0xE9 0x?? 0x??
     *    0xEC 0x?? 0x90
     * where 0x?? means any byte value is OK.
     */
    if (bsp->bs50.bsJump[0] != 0xE9
        && (bsp->bs50.bsJump[0] != 0xEB || bsp->bs50.bsJump[2] != 0x90)) {
        goto error;
    }

    DLOG(4);
    /* It is possible that the above check could match a partition table, or some */
    /* non-FAT disk meant to boot a PC.  Check some more fields for sensible values. */

    /* We only work with 512, 1024, and 2048 byte sectors */
    bps = OSSwapLittleToHostInt16(b33->bpbBytesPerSec);
    if ((bps < 0x200) || (bps & (bps - 1)) || (bps > 0x800)) {
        goto error;
    }

    DLOG(5);
    /* Check to make sure valid sectors per cluster */
    spc = b33->bpbSecPerClust;
    if ((spc == 0 ) || (spc & (spc - 1))) {
        goto error;
    }

    DLOG(6);
    /* we know this disk, find the volume label */
    /* First, find the root directory */
    label[0] = '\0';
    finished = false;
    rootDirSectors = ((OSSwapLittleToHostInt16(b50->bpbRootDirEnts) * sizeof(struct direntry)) +
                      (bps-1)) / bps;

    DLOG(7);

    if (rootDirSectors) {			/* FAT12 or FAT16 */
    	int firstRootDirSecNum;
        u_int8_t *rootDirBuffer;
        int j;

        rootDirBuffer = (char *)malloc(MAX_DOS_BLOCKSIZE);
    	
        DLOG(8);
        firstRootDirSecNum = OSSwapLittleToHostInt16(b33->bpbResSectors) +
            (b33->bpbFATs * OSSwapLittleToHostInt16(b33->bpbFATsecs));
        for (i=0; i< rootDirSectors; i++) {
            Seek(ih, (firstRootDirSecNum+i)*bps);
            Read(ih, (long)rootDirBuffer, bps);
            dirp = (struct direntry *)rootDirBuffer;
            for (j=0; j<bps; j+=sizeof(struct direntry), dirp++) {
                if (dirp->deName[0] == SLOT_EMPTY) {
                    finished = true;
                    break;
                }
                else if (dirp->deName[0] == SLOT_DELETED)
                    continue;
                else if (dirp->deAttributes == ATTR_WIN95)
                    continue;
                else if (dirp->deAttributes & ATTR_VOLUME) {
                    strncpy(label, dirp->deName, LABEL_LENGTH);
                    finished = true;
                    break;
                }
            }	/* j */
            if (finished == true) {
                break;
            }
        }	/* i */

        free(rootDirBuffer);

    } else {	/* FAT32 */
        u_int32_t cluster;
        u_int32_t bytesPerCluster;
        u_int8_t *rootDirBuffer;
        off_t readOffset;
        
        DLOG(9);
        bytesPerCluster = bps * spc;
        rootDirBuffer = malloc(bytesPerCluster);
        cluster = OSSwapLittleToHostInt32(b710->bpbRootClust);
        
        DLOG(0x20);
        finished = false;
        while (!finished && cluster >= CLUST_FIRST && cluster < CLUST_RSRVD) {

            DLOG(0x21);
            /* Find sector where clusters start */
            readOffset = OSSwapLittleToHostInt16(b710->bpbResSectors) +
                (b710->bpbFATs * OSSwapLittleToHostInt16(b710->bpbBigFATsecs));
            /* Find sector where "cluster" starts */
            readOffset += ((off_t) cluster - CLUST_FIRST) * (off_t) spc;
            /* Convert to byte offset */
            readOffset *= (off_t) bps;
            
            DLOG(0x22);
            /* Read in "cluster" */
            Seek(ih, readOffset);
            Read(ih, (long)rootDirBuffer, bytesPerCluster);
            dirp = (struct direntry *) rootDirBuffer;
            
            DLOG(0x23);
            /* Examine each directory entry in this cluster */
            for (i=0; i < bytesPerCluster; i += sizeof(struct direntry), dirp++) {

                DLOG(0x24);
                if (dirp->deName[0] == SLOT_EMPTY) {
                    finished = true;	// Reached end of directory (never used entry)
                    break;
                }
                else if (dirp->deName[0] == SLOT_DELETED)
                    continue;
                else if (dirp->deAttributes == ATTR_WIN95)
                    continue;
                else if (dirp->deAttributes & ATTR_VOLUME) {
                    DLOG(0x31);
                    strncpy(label, dirp->deName, LABEL_LENGTH);
                    finished = true;
                    break;
                }
                DLOG(0x25);
            }
            if (finished)
                break;

            DLOG(0x26);
            /* Find next cluster in the chain by reading the FAT */
            
            /* Find first sector of FAT */
            readOffset = OSSwapLittleToHostInt16(b710->bpbResSectors);
            /* Find sector containing "cluster" entry in FAT */
            readOffset += (cluster * 4) / bps;
            /* Convert to byte offset */
            readOffset *= bps;
            
            DLOG(0x27);
            /* Read one sector of the FAT */
            Seek(ih, readOffset);
            Read(ih, (long)rootDirBuffer, bps);
            
            DLOG(0x28);
            cluster = OSReadLittleInt32(rootDirBuffer + ((cluster * 4) % bps), 0);
            cluster &= 0x0FFFFFFF;	// ignore reserved upper bits
        }

        free(rootDirBuffer);

    }	/* rootDirSectors */

    DLOG(10);
    /* else look in the boot blocks */
    if (str[0] == '\0') {
        if (OSSwapLittleToHostInt16(b50->bpbRootDirEnts) == 0) { /* It's FAT32 */
            strncpy(label, ((struct extboot *)bsp->bs710.bsExt)->exVolumeLabel, LABEL_LENGTH);
        }
        else if (((struct extboot *)bsp->bs50.bsExt)->exBootSignature == EXBOOTSIG) {
            strncpy(label, ((struct extboot *)bsp->bs50.bsExt)->exVolumeLabel, LABEL_LENGTH);
        }
    }

    fixLabel(label, str, strMaxLen);

    free(buf);
    return;

 error:
    DLOG(0xee);
    if (buf) free(buf);
    return;
}
