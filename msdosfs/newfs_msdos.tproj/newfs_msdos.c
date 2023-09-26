/*
 * Copyright (c) 2000-2006, 2008, 2010-2011 Apple Inc. All rights reserved.
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

#include "lib_newfs_msdos.h"
#include "format.h"
#include <sys/ioctl.h>

/* ioctl selector to get the offset of the current partition
 * from the start of the disk to initialize hidden sectors
 * value in the boot sector.
 *
 * Note: This ioctl selector is not available in userspace
 * and we are assuming its existence by defining it here.
 * This behavior can change in future.
 */
#ifndef DKIOCGETBASE
#define DKIOCGETBASE    _IOR('d', 73, uint64_t)
#endif


static void usage(void);
static void check_mounted(const char *, mode_t);

static void vprint(newfs_client_ctx_t client, int level, const char *fmt, va_list ap)
{
    switch (level) {
        case LOG_INFO:
            vprintf(fmt, ap);
            break;
        case LOG_ERR:
            verrx(1, fmt, ap);
            break;
        default:
            break;
    }
}

/*
 * Construct a FAT12, FAT16, or FAT32 file system.
 */
int
main(int argc, char *argv[])
{
    newfs_set_context_properties(vprint, wipefs, NULL);
    NewfsOptions sopts;
    memset(&sopts, 0, sizeof(sopts));
    static char opts[] = "NB:F:I:O:S:P:a:b:c:e:f:h:i:k:m:n:o:r:s:u:v:";
    char buf[MAXPATHLEN];
    struct stat sb;
    const char *devName;
    int ch;

    while ((ch = getopt(argc, argv, opts)) != -1) {
        switch (ch) {
            case 'N':
                sopts.dryRun = 1;
                break;
            case 'B':
                sopts.bootStrapFromFile = optarg;
                break;
            case 'F':
                if (strcmp(optarg, "12") &&
                    strcmp(optarg, "16") &&
                    strcmp(optarg, "32")) {
                    errx(1, "%s: bad FAT type", optarg);
                }
                sopts.FATType = atoi(optarg);
                break;
            case 'I':
                sopts.volumeID = argto4(optarg, 0, "volume ID");
                if (sopts.volumeID == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid volumeID", optarg);
                    return 1;
                }
                sopts.volumeIDFlag = 1;
                break;
            case 'O':
                if (strlen(optarg) > 8) {
                    errx(1, "%s: bad OEM string", optarg);
                }
                sopts.OEMString = optarg;
                break;
            case 'S':
                sopts.sectorSize = argto2(optarg, 1, "bytes/sector");
                if (sopts.sectorSize == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid sector size", optarg);
                    return 1;
                }
                break;
            case 'P':
                sopts.physicalBytes = argto2(optarg, 1, "physical bytes/sector");
                if (sopts.physicalBytes == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid physical bytes", optarg);
                    return 1;
                }
                break;
            case 'a':
                sopts.numOfSectorsPerFAT = argto4(optarg, 1, "sectors/FAT");
                if (sopts.numOfSectorsPerFAT == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid sectors per FAT", optarg);
                    return 1;
                }
                break;
            case 'b':
                sopts.blockSize = argtox(optarg, 1, "block size");
                sopts.clusterSize = 0;
                break;
            case 'c':
                sopts.clusterSize = argto1(optarg, 1, "sectors/cluster");
                if (sopts.clusterSize == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid cluster size", optarg);
                    return 1;
                }
                sopts.blockSize = 0;
                break;
            case 'e':
                sopts.numOfRootDirEnts = argto2(optarg, 1, "directory entries");
                if (sopts.numOfRootDirEnts == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid directory entries", optarg);
                    return 1;
                }
                break;
            case 'f':
                sopts.standardFormat = optarg;
                break;
            case 'h':
                sopts.numDriveHeads = argto2(optarg, 1, "drive heads");
                if (sopts.numDriveHeads == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid drive heads", optarg);
                    return 1;
                }
                break;
            case 'i':
                sopts.systemSectorLocation = argto2(optarg, 1, "info sector");
                if (sopts.systemSectorLocation == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid info sector", optarg);
                    return 1;
                }
                break;
            case 'k':
                sopts.backupSectorLocation = argto2(optarg, 1, "backup sector");
                if (sopts.backupSectorLocation == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid backup sector", optarg);
                    return 1;
                }
                break;
            case 'm':
                sopts.mediaDescriptor = argto1(optarg, 0, "media descriptor");
                if (sopts.mediaDescriptor == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid media descriptor", optarg);
                    return 1;
                }
                sopts.mediaDescriptorFlag = 1;
                break;
            case 'n':
                sopts.numbOfFATs = argto1(optarg, 1, "number of FATs");
                if (sopts.numbOfFATs == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid number of FATs", optarg);
                    return 1;
                }
                break;
            case 'o':
                sopts.numOfHiddenSectors = argto4(optarg, 0, "hidden sectors");
                if (sopts.numOfHiddenSectors == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid hidden sectors", optarg);
                    return 1;
                }
                sopts.hiddenSectorsFlag = 1;
                break;
            case 'r':
                sopts.numOfReservedSectors = argto2(optarg, 1, "reserved sectors");
                if (sopts.numOfReservedSectors == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid reserved sectors", optarg);
                    return 1;
                }
                break;
            case 's':
                sopts.fsSizeInSectors = argto4(optarg, 1, "file system size (in sectors)");
                if (sopts.fsSizeInSectors == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid file system size (in sectors)", optarg);
                    return 1;
                }
                break;
            case 'u':
                sopts.numOfSectorsPerTrack = argto2(optarg, 1, "sectors/track");
                if (sopts.numOfSectorsPerTrack == UINT_MAX) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: Invalid sectors per track", optarg);
                    return 1;
                }
                break;
            case 'v':
                if (!oklabel(optarg)) {
                    errx(1, "%s: bad volume name", optarg);
                }
                sopts.volumeName = optarg;
                break;
            default:
                usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc < 1 || argc > 2) {
        usage();
    }
    devName = *argv++;
    if (!strchr(devName, '/')) {
        snprintf(buf, sizeof(buf), "%sr%s", _PATH_DEV, devName);
        if (stat(buf, &sb)) {
            snprintf(buf, sizeof(buf), "%s%s", _PATH_DEV, devName);
        }
        if (!(devName = strdup(buf))) {
            err(1, NULL);
        }
    }
    int fd;
    if ((fd = open(devName, sopts.dryRun ? O_RDONLY : O_RDWR)) == -1 || fstat(fd, &sb)) {
        err(1, "%s", devName);
    }
    int bootFD = -1;
    const char *bname = NULL;
    if (sopts.bootStrapFromFile) {
        bname = sopts.bootStrapFromFile;
        if (!strchr(bname, '/')) {
            snprintf(buf, sizeof(buf), "/boot/%s", bname);
            if (!(bname = strdup(buf))) {
                err(1, NULL);
            }
        }
        if ((bootFD = open(bname, O_RDONLY)) == -1 || fstat(bootFD, &sb)) {
            err(1, "%s", bname);
        }
    }
    if (!sopts.dryRun) {
        check_mounted(devName, sb.st_mode);
    }

    // Setup the newfs properties
    NewfsProperties newfsProps;
    memset(&newfsProps, 0, sizeof(newfsProps));
    uint64_t partitionBase   = UINT64_MAX; /* in bytes from start of device */
    uint64_t blockCount      = UINT64_MAX;
    uint32_t blockSize       = UINT32_MAX;
    uint32_t physBlockSize   = UINT32_MAX;
    if (ioctl(fd, DKIOCGETBASE, &partitionBase) == -1) {
        newfs_print(newfs_ctx, LOG_ERR, "%s: %s: Cannot get partition offset", strerror(errno), devName);
    }
    if (ioctl(fd, DKIOCGETBLOCKCOUNT, &blockCount) == -1) {
        newfs_print(newfs_ctx, LOG_ERR, "%s: %s: Cannot get block count", strerror(errno), devName);
    }
    if (ioctl(fd, DKIOCGETBLOCKSIZE, &blockSize) == -1) {
       newfs_print(newfs_ctx, LOG_ERR, "%s: %s: Cannot get block size", strerror(errno), devName);
    }
    if (ioctl(fd, DKIOCGETPHYSICALBLOCKSIZE, &physBlockSize) == -1) {
        newfs_print(newfs_ctx, LOG_INFO, "ioctl(DKIOCGETPHYSICALBLOCKSIZE) not supported\n");
    }
    newfsProps.fd = fd;
    newfsProps.devName = devName;
    newfsProps.partitionBase = partitionBase;
    newfsProps.blockSize = blockSize;
    newfsProps.blockCount = blockCount;
    newfsProps.physBlockSize = physBlockSize;
    newfsProps.bname = bname;
    newfsProps.bootFD = bootFD;
    newfsProps.sb = sb;
    return format(sopts, newfsProps, NULL);
}

/*
 * Exit with error if file system is mounted.
 */
static void
check_mounted(const char *devName, mode_t mode)
{
    struct statfs *mp;
    const char *s1, *s2;
    size_t len;
    int n, r;

    if (!(n = getmntinfo(&mp, MNT_NOWAIT))) {
        err(1, "getmntinfo");
    }
    len = strlen(_PATH_DEV);
    s1 = devName;
    if (!strncmp(s1, _PATH_DEV, len)) {
        s1 += len;
    }
    r = S_ISCHR(mode) && s1 != devName && *s1 == 'r';
    for (; n--; mp++) {
        s2 = mp->f_mntfromname;
        if (!strncmp(s2, _PATH_DEV, len)) {
            s2 += len;
        }
        if ((r && s2 != mp->f_mntfromname && !strcmp(s1 + 1, s2)) ||
            !strcmp(s1, s2)) {
            errx(1, "%s is mounted on %s", devName, mp->f_mntonname);
        }
    }
}


/*
 * Print usage message.
 */
static void
usage(void)
{
    fprintf(stderr,
            "usage: newfs_msdos [ -options ] special [disktype]\n");
    fprintf(stderr, "where the options are:\n");
    fprintf(stderr, "\t-N don't create file system: "
            "just print out parameters\n");
    fprintf(stderr, "\t-B get bootstrap from file\n");
    fprintf(stderr, "\t-F FAT type (12, 16, or 32)\n");
    fprintf(stderr, "\t-I volume ID\n");
    fprintf(stderr, "\t-O OEM string\n");
    fprintf(stderr, "\t-S bytes/sector\n");
    fprintf(stderr, "\t-P physical bytes/sector\n");
    fprintf(stderr, "\t-a sectors/FAT\n");
    fprintf(stderr, "\t-b block size\n");
    fprintf(stderr, "\t-c sectors/cluster\n");
    fprintf(stderr, "\t-e root directory entries\n");
    fprintf(stderr, "\t-f standard format\n");
    fprintf(stderr, "\t-h drive heads\n");
    fprintf(stderr, "\t-i file system info sector\n");
    fprintf(stderr, "\t-k backup boot sector\n");
    fprintf(stderr, "\t-m media descriptor\n");
    fprintf(stderr, "\t-n number of FATs\n");
    fprintf(stderr, "\t-o hidden sectors\n");
    fprintf(stderr, "\t-r reserved sectors\n");
    fprintf(stderr, "\t-s file system size (in sectors)\n");
    fprintf(stderr, "\t-u sectors/track\n");
    fprintf(stderr, "\t-v filesystem/volume name\n");
    exit(1);
}
