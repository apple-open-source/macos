//
//  format.c
//  newfs_msdos
//  This file contains the functions that were used to format a disk copied from newfs_msdos.c file.
//  This file allows us to encapsulate the format logic into this file, and use the "format" function to format a device
//  from newfs_msdos CLI tool, or from FSKit.
//
//  Created by Kujan Lauz on 06/09/2022.
//

#include "format.h"

#define DEFAULT_PARTITION_BASE   0
#define DEFAULT_BLOCK_COUNT      8192
#define DEFAULT_BLOCK_SIZE       512

struct DiskSizeToClusterSize fat16Sizes[] = {
    {   4200,        0},    /* Disks up to 4.1 MB; the 0 triggers an error */
    {  16340,     1024},    /* Disks up to  16 MB => 1 KB cluster */
    { 131072,     2048},    /* Disks up to 128 MB => 2 KB cluster */
    { 262144,     4096},    /* Disks up to 256 MB => 4 KB cluster */
    { 524288,     8192},    /* Disks up to 512 MB => 8 KB cluster */
    /* The following entries are used only if FAT16 is forced */
    {1048576,    16384},    /* Disks up to 1 GB => 16 KB cluster */
    {2097152,    32768},    /* Disks up to 2 GB => 32KB cluster (total size may be limited) */
    {UINT64_MAX,     0}     // Hard stop
};
struct DiskSizeToClusterSize fat32Sizes[] = {
    {   33300,        0},    /* Disks up to 32.5 MB; the 0 triggers an error */
    {  266240,      512},    /* Disks up to 260 MB => 512 byte cluster; not used unles FAT32 forced */
    { 8388608,     4096},    /* Disks up to   8 GB =>  4 KB cluster */
    {16777216,     8192},    /* Disks up to  16 GB =>  8 KB cluster */
    {33554432,    16384},    /* Disks up to  32 GB => 16 KB cluster */
    {17179869184, 32768},    /* Disks up to  16 TB => 32 KB cluster */
    {UINT64_MAX,      0}     // Hard stop
};

struct StdFormat stdfmt[] = {
    {"160",  {512, 1, 1, 2,  64,  320, 0xfe, 1,  8, 1}},
    {"180",  {512, 1, 1, 2,  64,  360, 0xfc, 2,  9, 1}},
    {"320",  {512, 2, 1, 2, 112,  640, 0xff, 1,  8, 2}},
    {"360",  {512, 2, 1, 2, 112,  720, 0xfd, 2,  9, 2}},
    {"640",  {512, 2, 1, 2, 112, 1280, 0xfb, 2,  8, 2}},
    {"720",  {512, 2, 1, 2, 112, 1440, 0xf9, 3,  9, 2}},
    {"1200", {512, 1, 1, 2, 224, 2400, 0xf9, 7, 15, 2}},
    {"1232", {1024,1, 1, 2, 192, 1232, 0xfe, 2,  8, 2}},
    {"1440", {512, 1, 1, 2, 224, 2880, 0xf0, 9, 18, 2}},
    {"2880", {512, 2, 1, 2, 240, 5760, 0xf0, 9, 36, 2}}
};

static int newfs_wipefs(lib_newfs_ctx_t c, WipeFSProperties props)
{
    if (c.wipefs) {
        return c.wipefs(c.client_ctx, props);
    }
    return 0;
}

int format(NewfsOptions sopts, NewfsProperties newfsProps, format_context context)
{
    char *phasesNamesFails[3] = {"Format device: Checking parameters: Failed", "Format device: Wiping file system: Failed", "Format device: Writing file system: Failed"};
    char *phasesNames[3] = {"Format device: Checking parameters", "Format device: Wiping file system", "Format device: Writing file system"};
    int reportProgress = (context != NULL && context->updater != NULL &&
                          context->startPhase != NULL && context->endPhase != NULL);
    const char *devName = newfsProps.devName;
    const char *bname = newfsProps.bname;
    unsigned int progressTracker = 0;
    int fd = newfsProps.fd;
    int bootFD = newfsProps.bootFD;
    struct stat sb = newfsProps.sb;
    char buf[MAXPATHLEN];
    struct timeval tv;
    struct bpb bpb;
    struct tm *tm;
    struct bs *bs;
    struct bsbpb *bsbpb;
    struct bsxbpb *bsxbpb;
    struct bsx *bsx;
    struct de *de;
    u_int8_t *bpb_buffer;
    u_int8_t *io_buffer;    /* The buffer for sectors being constructed/written */
    u_int fat, bss, rds, cls, dir, lsn, x, x1, x2;
    int currPhase = 0;
    u_int8_t *img;        /* Current sector within io_buffer */
    int ret = 0;
    ssize_t n;
    time_t now;

    if (reportProgress) {
        context->startPhase(phasesNames[currPhase], 10, 10, &progressTracker,
                            context->updater);
    }

    if (!S_ISCHR(sb.st_mode)) {
        newfs_print(newfs_ctx, LOG_INFO, "warning: %s is not a character device\n", devName);
    }

    memset(&bpb, 0, sizeof(bpb));
    if (sopts.standardFormat) {
        ret = getstdfmt(sopts.standardFormat, &bpb);
        if (ret != 0) {
            goto exit;
        }
        bpb.bsec = bpb.sec;
        bpb.sec = 0;
        bpb.bspf = bpb.spf;
        bpb.spf = 0;
    }

    if (sopts.numDriveHeads) {
        bpb.hds = sopts.numDriveHeads;
    }

    if (sopts.numOfSectorsPerTrack) {
        bpb.spt = sopts.numOfSectorsPerTrack;
    }

    if (reportProgress) {
        progressTracker++;
    }

    if (sopts.sectorSize) {
        bpb.bps = sopts.sectorSize;
    }

    if (sopts.fsSizeInSectors) {
        bpb.bsec = sopts.fsSizeInSectors;
    }

    if (sopts.hiddenSectorsFlag) {
        bpb.hid = sopts.numOfHiddenSectors;
    }

    if (!(sopts.standardFormat || (sopts.numDriveHeads && sopts.numOfSectorsPerTrack && sopts.sectorSize && sopts.fsSizeInSectors && sopts.hiddenSectorsFlag))) {
        ret = getdiskinfo(newfsProps, sopts.hiddenSectorsFlag, &bpb);
        if (ret != 0) {
            goto exit;
        }
    }

    if (reportProgress) {
        progressTracker++;
    }

    if (!powerof2(bpb.bps)) {
        newfs_print(newfs_ctx, LOG_ERR, "bytes/sector (%u) is not a power of 2", bpb.bps);
        ret = 1;
        goto exit;
    }

    if (bpb.bps < MINBPS) {
        newfs_print(newfs_ctx, LOG_ERR, "bytes/sector (%u) is too small; minimum is %u", bpb.bps, MINBPS);
        ret = 1;
        goto exit;
    }

    if (bpb.bps > MAXBPS) {
        newfs_print(newfs_ctx, LOG_ERR, "bytes/sector (%u) is too large; maximum is %u", bpb.bps, MAXBPS);
        ret = 1;
        goto exit;
    }

    if (sopts.physicalBytes != 0 && !powerof2(sopts.physicalBytes)) {
        newfs_print(newfs_ctx, LOG_ERR, "physical bytes/sector (%u) is not a power of 2", sopts.physicalBytes);
        ret = 1;
        goto exit;
    }

    if (reportProgress) {
        progressTracker++;
    }

    if (sopts.physicalBytes != 0 && sopts.physicalBytes < bpb.bps) {
        newfs_print(newfs_ctx, LOG_ERR, "physical bytes/sector (%u) is less than logical bytes/sector (%u)", sopts.physicalBytes, bpb.bps);
        ret = 1;
		goto exit;
    }

    if (sopts.physicalBytes == 0) {
        uint32_t physBlockSize = newfsProps.physBlockSize;
        if (physBlockSize == UINT32_MAX) {
            newfs_print(newfs_ctx, LOG_INFO, "Physical block size wasn't initialized, because of ioctl(DKIOCGETPHYSICALBLOCKSIZE) not being supported\n");
            sopts.physicalBytes = bpb.bps;
        } else {
            newfs_print(newfs_ctx, LOG_INFO, "%u bytes per physical sector\n", physBlockSize);
            sopts.physicalBytes = physBlockSize;
        }
    }

    if (reportProgress) {
        progressTracker++;
    }

    if (!(fat = sopts.FATType)) {
        if (sopts.standardFormat) {
            fat = 12;
        }
        else if (!sopts.numOfRootDirEnts && (sopts.systemSectorLocation || sopts.backupSectorLocation)) {
            fat = 32;
        }
    }

    if ((fat == 32 && sopts.numOfRootDirEnts) || (fat != 32 && (sopts.systemSectorLocation || sopts.backupSectorLocation))) {
        newfs_print(newfs_ctx, LOG_ERR, "-%c is not a legal FAT%s option",
                    fat == 32 ? 'e' : sopts.systemSectorLocation ? 'i' : 'k',
                    fat == 32 ? "32" : "12/16");
        ret = 1;
		goto exit;
    }

    if (sopts.standardFormat && fat == 32) {
        bpb.rde = 0;
    }

    if (sopts.blockSize) {
        if (!powerof2(sopts.blockSize)) {
            newfs_print(newfs_ctx, LOG_ERR, "block size (%u) is not a power of 2", sopts.blockSize);
            ret = 1;
		    goto exit;
        }

        if (sopts.blockSize < bpb.bps) {
            newfs_print(newfs_ctx, LOG_ERR, "block size (%u) is too small; minimum is %u", sopts.blockSize, bpb.bps);
            ret = 1;
            goto exit;
        }

        if (sopts.blockSize > bpb.bps * MAXSPC) {
            newfs_print(newfs_ctx, LOG_ERR, "block size (%u) is too large; maximum is %u", sopts.blockSize, bpb.bps * MAXSPC);
            ret = 1;
		    goto exit;
        }
        bpb.spc = sopts.blockSize / bpb.bps;
    }

    if (sopts.clusterSize) {
        if (!powerof2(sopts.clusterSize)) {
            newfs_print(newfs_ctx, LOG_ERR, "sectors/cluster (%u) is not a power of 2", sopts.clusterSize);
            ret = 1;
            goto exit;
        }
        bpb.spc = sopts.clusterSize;
    }

    if (sopts.numOfReservedSectors) {
        bpb.res = sopts.numOfReservedSectors;
    }

    if (reportProgress) {
        progressTracker++;
    }

    if (sopts.numbOfFATs) {
        if (sopts.numbOfFATs > MAXNFT) {
            newfs_print(newfs_ctx, LOG_ERR, "number of FATs (%u) is too large; maximum is %u",
                        sopts.numbOfFATs, MAXNFT);
        }
        bpb.nft = sopts.numbOfFATs;
    }

    if (sopts.numOfRootDirEnts) {
        bpb.rde = sopts.numOfRootDirEnts;
    }

    if (sopts.mediaDescriptorFlag) {
        if (sopts.mediaDescriptor < 0xf0) {
            newfs_print(newfs_ctx, LOG_ERR, "illegal media descriptor (%#x)", sopts.mediaDescriptor);
            ret = 1;
		    goto exit;
        }
        bpb.mid = sopts.mediaDescriptor;
    }

    if (sopts.numOfSectorsPerFAT) {
        bpb.bspf = sopts.numOfSectorsPerFAT;
    }

    if (sopts.systemSectorLocation) {
        bpb.infs = sopts.systemSectorLocation;
    }

    if (sopts.backupSectorLocation) {
        bpb.bkbs = sopts.backupSectorLocation;
    }
    bss = 1;

    if (sopts.bootStrapFromFile) {
        bname = sopts.bootStrapFromFile;
        if (!S_ISREG(sb.st_mode) || sb.st_size % bpb.bps ||
            sb.st_size < bpb.bps || sb.st_size > bpb.bps * MAXU16) {
            newfs_print(newfs_ctx, LOG_ERR, "%s: inappropriate file type or format", bname);
            ret = 1;
		    goto exit;
        }
        bss = (u_int)(sb.st_size / bpb.bps);
    }

    if (!bpb.nft) {
        bpb.nft = 2;
    }

    if (reportProgress) {
        progressTracker++;
    }

    sd_card_set_defaults(devName, &fat, &bpb);

    if (reportProgress) {
        progressTracker++;
    }

    /*
     * [2873851] If the FAT type or sectors per cluster were not explicitly specified,
     * set them to default values.
     */
    if (!bpb.spc)
    {
        u_int64_t kilobytes = (u_int64_t) bpb.bps * (u_int64_t) bpb.bsec / 1024U;
        u_int32_t bytes_per_cluster;

        /*
         * If the user didn't specify the FAT type, then pick a default based on
         * the size of the volume.
         */
        if (!fat)
        {
            if (bpb.bps == 512 && bpb.bsec <= MAX_SEC_FAT12_512) {
                fat = 12;
            }
            else if (bpb.bps != 512 && bpb.bsec <= MAX_SEC_FAT12) {
                fat = 12;
            }
            else if (kilobytes <= MAX_KB_FAT16) {
                fat = 16;
            }
            else {
                fat = 32;
            }
        }

        switch (fat)
        {
            case 12:
                /*
                 * There is no general table for FAT12, so try all possible
                 * bytes-per-cluster values until it all fits, or we try the
                 * maximum cluster size.
                 */
                for (bytes_per_cluster = bpb.bps; bytes_per_cluster <= 32768; bytes_per_cluster *= 2)
                {
                    bpb.spc = bytes_per_cluster / bpb.bps;

                    /* Start with number of reserved sectors */
                    x = bpb.res ? bpb.res : bss;
                    /* Plus number of sectors used by FAT */
                    x += howmany((RESFTE+MAXCLS12+1)*(12/BPN), bpb.bps*NPB) * bpb.nft;
                    /* Plus root directory */
                    x += howmany(bpb.rde ? bpb.rde : DEFRDE, bpb.bps / sizeof(struct de));
                    /* Plus data clusters */
                    x += (MAXCLS12+1) * bpb.spc;

                    /*
                     * We now know how many sectors the volume would occupy with the given
                     * sectors per cluster, and the maximum number of FAT12 clusters.  If
                     * this is as big as or bigger than the actual volume, we've found the
                     * minimum sectors per cluster.
                     */
                    if (x >= bpb.bsec) {
                        break;
                    }
                }
                break;
            case 16:
                for (x=0; kilobytes > fat16Sizes[x].kilobytes; ++x);
                bytes_per_cluster = fat16Sizes[x].bytes_per_cluster;
                if (!bytes_per_cluster) {
                    newfs_print(newfs_ctx, LOG_ERR, "FAT%d is impossible for disk size of %lluKiB", fat, kilobytes);
                    ret = 1;
                    goto exit;
                }
                if (bytes_per_cluster < bpb.bps) {
                    bytes_per_cluster = bpb.bps;
                }
                bpb.spc = bytes_per_cluster / bpb.bps;
                break;
            case 32:
                for (x=0; kilobytes > fat32Sizes[x].kilobytes; ++x);
                bytes_per_cluster = fat32Sizes[x].bytes_per_cluster;
                if (!bytes_per_cluster) {
                    newfs_print(newfs_ctx, LOG_ERR, "FAT%d is impossible for disk size of %lluKiB", fat, kilobytes);
                    ret = 1;
                    goto exit;
                }
                if (bytes_per_cluster < bpb.bps) {
                    bytes_per_cluster = bpb.bps;
                }
                bpb.spc = bytes_per_cluster / bpb.bps;
                break;
            default:
                newfs_print(newfs_ctx, LOG_ERR, "Invalid FAT type: %d", fat);
                ret = 1;
                goto exit;
        }

        if (bpb.spc == 0) {
            newfs_print(newfs_ctx, LOG_ERR, "FAT%d is impossible with %u sectors", fat, bpb.bsec);
            ret = 1;
            goto exit;
        }
    }
    else
    {
        /*
         * User explicitly specified sectors per cluster.  If they didn't
         * specify the FAT type, pick one that uses up the available sectors.
         */
        if (!fat)
        {
            /* See if a maximum number of FAT clusters would fill it up. */
            if (bpb.bsec < (bpb.res ? bpb.res : bss) +
                howmany((RESFTE+MAXCLS12+1) * (12/BPN), bpb.bps * BPN) * bpb.nft +
                howmany(bpb.rde ? bpb.rde : DEFRDE, bpb.bps / sizeof(struct de)) +
                (MAXCLS12+1) * bpb.spc)
            {
                fat = 12;
            }
            else if (bpb.bsec < (bpb.res ? bpb.res : bss) +
                     howmany((RESFTE+MAXCLS16) * 2, bpb.bps) * bpb.nft +
                     howmany(bpb.rde ? bpb.rde : DEFRDE, bpb.bps / sizeof(struct de)) +
                     (MAXCLS16+1) * bpb.spc)
            {
                fat = 16;
            }
            else
            {
                fat = 32;
            }
        }
    }

    if (reportProgress) {
        progressTracker += 3;
    }

    x = bss;
    if (fat == 32) {
        if (!bpb.infs) {
            if (x == MAXU16 || x == bpb.bkbs) {
                newfs_print(newfs_ctx, LOG_ERR, "no room for info sector");
                ret = 1;
                goto exit;
            }
            bpb.infs = x;
        }
        if (bpb.infs != MAXU16 && x <= bpb.infs) {
            x = bpb.infs + 1;
        }
        if (!bpb.bkbs) {
            if (x == MAXU16) {
                newfs_print(newfs_ctx, LOG_ERR, "no room for backup sector");
                ret = 1;
                goto exit;
            }
            if (x <= BACKUP_BOOT_SECTOR) {
                bpb.bkbs = BACKUP_BOOT_SECTOR;
            }
            else {
                bpb.bkbs = x;
            }
        } else if (bpb.bkbs != MAXU16 && bpb.bkbs == bpb.infs) {
            newfs_print(newfs_ctx, LOG_ERR, "backup sector would overwrite info sector");
            ret = 1;
            goto exit;
        }
        if (bpb.bkbs != MAXU16 && x <= bpb.bkbs) {
            x = bpb.bkbs + 1;
        }
    }
    if (!bpb.res) {
        bpb.res = fat == 32 ? MAX(x, FAT32_RESERVED_SECTORS) : x;
    }
    else if (bpb.res < x) {
        newfs_print(newfs_ctx, LOG_ERR, "too few reserved sectors");
        ret = 1;
        goto exit;
    }
    if (fat != 32 && !bpb.rde) {
        bpb.rde = DEFRDE;
    }
    rds = howmany(bpb.rde, bpb.bps / sizeof(struct de));
    if (fat != 32 && bpb.bspf > MAXU16) {
        newfs_print(newfs_ctx, LOG_ERR, "too many sectors/FAT for FAT12/16");
        ret = 1;
        goto exit;
    }
    x1 = bpb.res + rds;
    x = bpb.bspf ? bpb.bspf : 1;
    if (x1 + (u_int64_t)x * bpb.nft > bpb.bsec) {
        newfs_print(newfs_ctx, LOG_ERR, "meta data exceeds file system size");
        ret = 1;
        goto exit;
    }
    x1 += x * bpb.nft;
    x = (u_int)((u_int64_t)(bpb.bsec - x1) * bpb.bps * NPB /
                (bpb.spc * bpb.bps * NPB + fat / BPN * bpb.nft));
    x2 = howmany((RESFTE + MIN(x, maxcls(fat))) * (fat / BPN),
                 bpb.bps * NPB);
    if (!bpb.bspf) {
        bpb.bspf = x2;

        /* Round up bspf to a multiple of physical sector size */
        if (sopts.physicalBytes > bpb.bps) {
            u_int phys_per_log = sopts.physicalBytes / bpb.bps;
            u_int remainder = bpb.bspf % phys_per_log;
            if (remainder) {
                bpb.bspf += phys_per_log - remainder;
            }
        }

        x1 += (bpb.bspf - 1) * bpb.nft;
    }
    cls = (bpb.bsec - x1) / bpb.spc;
    x = (u_int)((u_int64_t)bpb.bspf * bpb.bps * NPB / (fat / BPN) - RESFTE);
    if (cls > x)
    {
        /*
         * This indicates that there are more sectors available
         * for data clusters than there are usable entries in the
         * FAT.  In this case, we need to limit the number of
         * clusters, and also reduce the number of sectors.
         */
        bpb.bsec = bpb.res + bpb.bspf*bpb.nft + rds + x*bpb.spc;
        newfs_print(newfs_ctx, LOG_INFO, "warning: sectors/FAT limits sectors to %u, clusters to %u\n", bpb.bsec, x);
        cls = x;
    }
    if (bpb.bspf < x2) {
        newfs_print(newfs_ctx, LOG_INFO, "warning: sectors/FAT limits file system to %u clusters\n",
                    cls);
    }
    if (cls < mincls(fat)) {
        newfs_print(newfs_ctx, LOG_ERR, "%u clusters too few clusters for FAT%u, need %u", cls, fat,
                    mincls(fat));
        ret = 1;
		goto exit;
    }
    if (cls > maxcls(fat)) {
        cls = maxcls(fat);
        bpb.bsec = x1 + (cls + 1) * bpb.spc - 1;
        newfs_print(newfs_ctx, LOG_INFO, "warning: FAT type limits file system to %u sectors\n",
                    bpb.bsec);
    }
    newfs_print(newfs_ctx, LOG_INFO, "%s: %u sector%s in %u FAT%u cluster%s "
                "(%u bytes/cluster)\n", devName, cls * bpb.spc,
                cls * bpb.spc == 1 ? "" : "s", cls, fat,
                cls == 1 ? "" : "s", bpb.bps * bpb.spc);
    if (!bpb.mid) {
        bpb.mid = !bpb.hid ? 0xf0 : 0xf8;
    }
    if (fat == 32) {
        bpb.rdcl = RESFTE;
    }
    if (bpb.bsec <= MAXU16) {
        bpb.sec = bpb.bsec;
        bpb.bsec = 0;
    }
    if (fat != 32) {
        bpb.spf = bpb.bspf;
        bpb.bspf = 0;
    } else {
        if (bpb.bsec == 0) {
            bpb.bsec = bpb.sec;
        }
        bpb.spf = 0;
        bpb.sec = 0;
    }

    if (reportProgress) {
        progressTracker = 10;
        context->endPhase(phasesNames[currPhase++], context->updater);
    }

    print_bpb(&bpb);

    if (!sopts.dryRun)
    {
        u_int sectors_to_write;

        if (reportProgress) {
            progressTracker = 0;
            context->startPhase(phasesNames[currPhase], 10, 1, &progressTracker,
                                context->updater);
        }

        /*
         * Get the current date and time in case we need it for the volume ID.
         */
        gettimeofday(&tv, NULL);
        now = tv.tv_sec;
        tm = localtime(&now);

        /*
         * Allocate a buffer for assembling the to-be-written sectors, and
         * a separate buffer for the boot sector (which will be written last).
         */
        if (!(io_buffer = malloc(IO_BUFFER_SIZE))) {
            newfs_print(newfs_ctx, LOG_ERR, strerror(errno), NULL);
            ret = 1;
            goto exit;
        }
        if (!(bpb_buffer = malloc(bpb.bps))) {
            newfs_print(newfs_ctx, LOG_ERR, strerror(errno), NULL);
            ret = 1;
            goto exit;
        }
        img = io_buffer;
        dir = bpb.res + (bpb.spf ? bpb.spf : bpb.bspf) * bpb.nft;
        sectors_to_write = dir + (fat == 32 ? bpb.spc : rds);

        // Do wipefs
        WipeFSProperties wipefsProps;
        memset(&wipefsProps, 0, sizeof(wipefsProps)); // just in case (if new fields are added in the future and this code isn't updated)
        wipefsProps.fd = fd;
        wipefsProps.block_size = bpb.bps;
        wipefsProps.except_block_start = 0;
        wipefsProps.except_block_length = sectors_to_write;
        wipefsProps.include_block_start = 0;
        wipefsProps.include_block_length = 0;
        int wipeFSRetVal = newfs_wipefs(newfs_ctx, wipefsProps);
        if(wipeFSRetVal != 0) {
            newfs_print(newfs_ctx, LOG_ERR, "Encountered errors trying to wipe resource");
            ret = wipeFSRetVal;
            goto exit;
        }

        if (reportProgress) {
            progressTracker = 1;
            context->endPhase(phasesNames[currPhase++], context->updater);
            progressTracker = 0;
            context->startPhase(phasesNames[currPhase], 80, sectors_to_write + 2, &progressTracker,
                                context->updater);
        }
        /*
         * Now start writing the new file system to disk.
         */
        for (lsn = 0; lsn < sectors_to_write; lsn++) {
            x = lsn;
            if (sopts.bootStrapFromFile && fat == 32 && bpb.bkbs != MAXU16 &&
                bss <= bpb.bkbs && x >= bpb.bkbs) {
                x -= bpb.bkbs;
                if (!x && lseek(bootFD, 0, SEEK_SET)) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: %s", strerror(errno), bname);
                    ret = 1;
		            goto exit;
                }
            }
            if (sopts.bootStrapFromFile && x < bss) {
                if ((n = read(bootFD, img, bpb.bps)) == -1) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: %s", strerror(errno), bname);
                    ret = 1;
		            goto exit;
                }
                if (n != bpb.bps) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: can't read sector %u", bname, x);
                    ret = 1;
		            goto exit;
                }
            } else {
                memset(img, 0, bpb.bps);
            }
            if (!lsn || (fat == 32 && bpb.bkbs != MAXU16 && lsn == bpb.bkbs)) {
                x1 = sizeof(struct bs);
                bsbpb = (struct bsbpb *)(img + x1);
                mk2(bsbpb->bps, bpb.bps);
                mk1(bsbpb->spc, bpb.spc);
                mk2(bsbpb->res, bpb.res);
                mk1(bsbpb->nft, bpb.nft);
                mk2(bsbpb->rde, bpb.rde);
                mk2(bsbpb->sec, bpb.sec);
                mk1(bsbpb->mid, bpb.mid);
                mk2(bsbpb->spf, bpb.spf);
                mk2(bsbpb->spt, bpb.spt);
                mk2(bsbpb->hds, bpb.hds);
                mk4(bsbpb->hid, bpb.hid);
                mk4(bsbpb->bsec, bpb.bsec);
                x1 += sizeof(struct bsbpb);
                if (fat == 32) {
                    bsxbpb = (struct bsxbpb *)(img + x1);
                    mk4(bsxbpb->bspf, bpb.bspf);
                    mk2(bsxbpb->xflg, 0);
                    mk2(bsxbpb->vers, 0);
                    mk4(bsxbpb->rdcl, bpb.rdcl);
                    mk2(bsxbpb->infs, bpb.infs);
                    mk2(bsxbpb->bkbs, bpb.bkbs);
                    x1 += sizeof(struct bsxbpb);
                }
                bsx = (struct bsx *)(img + x1);
                mk1(bsx->drv, bpb.driveNum);
                mk1(bsx->sig, 0x29);
                if (sopts.volumeIDFlag) {
                    x = sopts.volumeID;
                } else {
                    x = (((u_int)(1 + tm->tm_mon) << 8 | (u_int)tm->tm_mday) + ((u_int)tm->tm_sec << 8 | (u_int)(tv.tv_usec / 10))) << 16 | ((u_int)(1900 + tm->tm_year) + ((u_int)tm->tm_hour << 8 | (u_int)tm->tm_min));
                }
                mk4(bsx->volid, x);
                mklabel(bsx->label, sopts.volumeName ? sopts.volumeName : "NO NAME");
                snprintf(buf, sizeof(buf), "FAT%u", fat);
                setstr(bsx->type, buf, sizeof(bsx->type));
                if (!sopts.bootStrapFromFile) {
                    x1 += sizeof(struct bsx);
                    bs = (struct bs *)img;
                    mk1(bs->jmp[0], 0xeb);
                    mk1(bs->jmp[1], x1 - 2);
                    mk1(bs->jmp[2], 0x90);
                    setstr(bs->oem, sopts.OEMString ? sopts.OEMString : "BSD  4.4", sizeof(bs->oem));
                    memcpy(img + x1, bootcode, sizeof(bootcode));
                    mk2(img + 510, DOSMAGIC);
                }
            } else if (fat == 32 && bpb.infs != MAXU16 && (lsn == bpb.infs || (bpb.bkbs != MAXU16 && lsn == bpb.bkbs + bpb.infs))) {
                mk4(img, 0x41615252);
                mk4(img + 484, 0x61417272);
                mk4(img + 488, cls-1);
                mk4(img + 492, bpb.rdcl+1);
                /* Offsets 508-509 remain zero */
                mk2(img + 510, DOSMAGIC);
            } else if (lsn >= bpb.res && lsn < dir && !((lsn - bpb.res) % (bpb.spf ? bpb.spf : bpb.bspf))) {
                mk1(img[0], bpb.mid);
                for (x = 1; x < fat * (fat == 32 ? 3 : 2) / 8; x++) {
                    mk1(img[x], fat == 32 && x % 4 == 3 ? 0x0f : 0xff);
                }
            } else if (lsn == dir && sopts.volumeName && *sopts.volumeName) {
                de = (struct de *)img;
                mklabel(de->namext, sopts.volumeName);
                mk1(de->attr, 050);
                x = (u_int)tm->tm_hour << 11 |
                (u_int)tm->tm_min << 5 |
                (u_int)tm->tm_sec >> 1;
                mk2(de->time, x);
                x = (u_int)(tm->tm_year - 80) << 9 |
                (u_int)(tm->tm_mon + 1) << 5 |
                (u_int)tm->tm_mday;
                mk2(de->date, x);
            }
            img += bpb.bps;

            if (lsn == 0) {
                /* Zero out boot sector for now and save it to be written at the end */
                memcpy(bpb_buffer, io_buffer, bpb.bps);
                bzero(io_buffer, bpb.bps);
            }

            if (img >= (io_buffer + IO_BUFFER_SIZE)) {
                /* We filled the I/O buffer, so write it out now */
                if ((n = write(fd, io_buffer, IO_BUFFER_SIZE)) == -1) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: %s", strerror(errno), devName);
                    ret = 1;
		            goto exit;
                }
                if (n != IO_BUFFER_SIZE) {
                    newfs_print(newfs_ctx, LOG_ERR, "%s: can't write sector %u", devName, lsn);
                    ret = 1;
                    goto exit;
                }
                img = io_buffer;
            }
            progressTracker++;
        }
        if (img != io_buffer) {
            /* The I/O buffer was partially full; write it out before exit */
            if ((n = write(fd, io_buffer, img-io_buffer)) == -1) {
                newfs_print(newfs_ctx, LOG_ERR, "%s: %s", strerror(errno), devName);
                ret = 1;
                goto exit;
            }
            if (n != (img-io_buffer)) {
                newfs_print(newfs_ctx, LOG_ERR, "%s: can't write sector %u", devName, lsn);
                ret = 1;
                goto exit;
            }
        }
        progressTracker++;
        /* Write out boot sector at the end now */
        if (lseek(fd, 0, SEEK_SET) == -1) {
            newfs_print(newfs_ctx, LOG_ERR, "%s: lseek: %s", strerror(errno), devName);
            ret = 1;
            goto exit;
        }
        if ((n = write(fd, bpb_buffer, bpb.bps)) == -1) {
            newfs_print(newfs_ctx, LOG_ERR, "%s: write: %s", strerror(errno), devName);
            ret = 1;
            goto exit;
        }
        if (n != bpb.bps) {
            newfs_print(newfs_ctx, LOG_ERR, "%s: can't write boot sector", devName);
            ret = 1;
            goto exit;
        }
        progressTracker++;
    }

exit:
    if (reportProgress) {
        if (ret) {
            context->endPhase(phasesNamesFails[currPhase], context->updater);
        } else {
            context->endPhase(phasesNames[currPhase], context->updater);
        }
    }
    return ret;
}

/*
 * Get a standard format.
 */
int
getstdfmt(const char *fmt, struct bpb *bpb)
{
    u_int x, i;

    x = sizeof(stdfmt) / sizeof(stdfmt[0]);
    for (i = 0; i < x && strcmp(fmt, stdfmt[i].name); i++);
    if (i == x) {
        newfs_print(newfs_ctx, LOG_ERR, "%s: unknown standard format", fmt);
        return 1;
    }
    *bpb = stdfmt[i].bpb;
    return 0;
}

/*
 * Get disk partition, and geometry information.
 */
int
getdiskinfo(NewfsProperties newfsProps, int oflag, struct bpb *bpb)
{
    uint64_t partitionBase = newfsProps.partitionBase; /* in bytes from start of device */
    uint64_t blockCount    = newfsProps.blockCount;
    uint32_t blockSize     = newfsProps.blockSize;

    if (partitionBase == UINT64_MAX) {
        partitionBase = DEFAULT_PARTITION_BASE;
        newfs_print(newfs_ctx, LOG_INFO, "%s: %s: Partition offset wasn't initialized, setting to default value (%llu)", strerror(errno), newfsProps.devName, partitionBase);
    }

    /*
     * If we'll need the block count or block size, get them now.
     */
    if (!bpb->bsec || !bpb->hds)
    {
        if (blockCount == UINT64_MAX) {
            blockCount = DEFAULT_BLOCK_COUNT;
            newfs_print(newfs_ctx, LOG_INFO, "%s: %s: Block count wasn't initialized, setting to default value (%llu)", strerror(errno), newfsProps.devName, blockCount);
        }
    }
    if (!bpb->bps || !bpb->bsec)
    {
        /*
         * Note: if user specified bytes per sector, but not number of sectors,
         * then we'll need the sector size in order to calculate the total
         * bytes in this partition.
         */
        if (blockSize == UINT32_MAX) {
            blockSize = DEFAULT_BLOCK_SIZE;
            newfs_print(newfs_ctx, LOG_INFO, "%s: %s: Block size wasn't initialized, setting to default value (%u)", strerror(errno), newfsProps.devName, blockSize);
        }
    }

    /*
     * If bytes-per-sector was explicitly specified, but total number of
     * sectors was not explicitly specified, then find out how many sectors
     * of the given size would fit into the given partition (calculate the
     * size of the partition in bytes, and divide by the desired bytes per
     * sector).
     *
     * This makes it possible to create a disk image, and format it in
     * preparation for copying to a device with a different sector size.
     */
    if (bpb->bps && !bpb->bsec)
    {
        u_int64_t bsec = (blockCount * blockSize) / bpb->bps;
        if (bsec > UINT32_MAX) {
            newfs_print(newfs_ctx, LOG_ERR, "%s: %s: Drive is too large, the number of blocks is larger than any FAT FS can support", strerror(errno), newfsProps.devName);
            return 1;
        }
        bpb->bsec = (u_int)bsec;
    }

    if (!bpb->bsec)
    {
        if (blockCount > UINT32_MAX) {
            newfs_print(newfs_ctx, LOG_ERR, "%s: %s: Drive is too large, the number of blocks is larger than any FAT FS can support", strerror(errno), newfsProps.devName);
            return 1;
        }
        bpb->bsec = (u_int)blockCount;
    }

    if (!bpb->bps) {
        bpb->bps = blockSize;
    }

    if (!oflag) {
        bpb->hid = (u_int)(partitionBase / bpb->bps);
    }

    /*
     * Set up the INT 0x13 style drive number for BIOS.  The FAT specification
     * says "0x00 for floppies, 0x80 for hard disks".  I assume that means
     * 0x80 if partitioned, and 0x000 otherwise.
     */
    bpb->driveNum = partitionBase != 0 ? 0x80 : 0x00;

    /*
     * Compute default values for sectors per track and number of heads
     * (number of tracks per cylinder) if the user didn't explicitly provide
     * them.  This calculation mimics the dkdisklabel() routine from
     * disklib.
     */
    if (!bpb->spt) {
        bpb->spt = 32;  /* The same constant that dkdisklabel() used. */
    }
    if (!bpb->hds) {
        /*
         * These are the same values used by dkdisklabel().
         *
         * Note the use of block_count instead of bpb->bsec here.
         * dkdisklabel() computed its fake geometry based on the block
         * count returned by DKIOCGETBLOCKCOUNT, without adjusting for
         * a new block size.
         */
        if (blockCount < 8*32*1024) {
            bpb->hds = 16;
        }
        else if (blockCount < 16*32*1024) {
            bpb->hds = 32;
        }
        else if (blockCount < 32*32*1024) {
            bpb->hds = 54;  /* Should be 64?  Bug in dkdisklabel()? */
        }
        else if (blockCount < 64*32*1024) {
            bpb->hds = 128;
        }
        else {
            bpb->hds = 255;
        }
    }
    return 0;
}

/*
 * Given the path we're formatting, see if it looks like an SD card.
 */
enum SDCardType
sd_card_type_for_path(const char *path)
{
    enum SDCardType result = kCardTypeNone;
    const char *disk = NULL;
    io_service_t obj = 0;
    CFDictionaryRef cardCharacteristics = NULL;
    CFStringRef cardType = NULL;

    /*
     * We're looking for the "disk1s1" part of the path, so see if the
     * path starts with "/dev/" or "/dev/r" and point past that.
     */
    if (!strncmp(path, "/dev/", 5)) {
        disk = path + 5;    /* Skip over "/dev/". */
        if (*disk == 'r') {
            ++disk;         /* Skip over the "r" in "/dev/r". */
        }
    }

    /*
     * Look for an IOService with the given BSD disk name.
     */
    if (disk) {
        obj = IOServiceGetMatchingService(kIOMainPortDefault,
                                          IOBSDNameMatching(kIOMainPortDefault, 0, disk));
    }

    /* See if the object has a card characteristics dictionary. */
    if (obj) {
        cardCharacteristics = IORegistryEntrySearchCFProperty(
                                                              obj, kIOServicePlane,
                                                              CFSTR(kIOPropertyCardCharacteristicsKey),
                                                              kCFAllocatorDefault,
                                                              kIORegistryIterateRecursively|kIORegistryIterateParents);
    }

    /* See if the dictionary contains a card type string. */
    if (cardCharacteristics && CFGetTypeID(cardCharacteristics) == CFDictionaryGetTypeID()) {
    cardType = CFDictionaryGetValue(cardCharacteristics, CFSTR(kIOPropertyCardTypeKey));
    }

    /* Turn the card type string into one of our constants. */
    if (cardType && CFGetTypeID(cardType) == CFStringGetTypeID()) {
        if (CFEqual(cardType, CFSTR(kIOPropertyCardTypeSDSCKey))) {
            result = kCardTypeSDSC;
        } else if (CFEqual(cardType, CFSTR(kIOPropertyCardTypeSDHCKey))) {
            result = kCardTypeSDHC;
        } else if (CFEqual(cardType, CFSTR(kIOPropertyCardTypeSDXCKey))) {
            result = kCardTypeSDXC;
        }
    }

    if (cardCharacteristics) {
        CFRelease(cardCharacteristics);
    }
    if (obj) {
        IOObjectRelease(obj);
    }

    return result;
}

/*
 * If the given path is to some kind of SD card, then use the default FAT type
 * and cluster size specified by the SD Card Association.
 *
 * Note that their specification refers to card capacity, which means the size
 * of the entire media (not just the partition containing the file system).
 * Below, the size of the partition is being compared since that is what we
 * have most convenient access to, and its size is only slightly smaller than
 * the size of the entire media.  This program does not write the partition
 * map, so we can't enforce the recommended partition offset.
 */
void
sd_card_set_defaults(const char *path, u_int *fat, struct bpb *bpb)
{
    /*
     * Only use SD card defaults if the sector size is 512 bytes, and the
     * user did not explicitly specify the FAT type or cluster size.
     */
    if (*fat != 0 || bpb->spc != 0 || bpb->bps != 512) {
        return;
    }

    enum SDCardType cardType = sd_card_type_for_path(path);

    switch (cardType)
    {
        case kCardTypeNone:
            break;
        case kCardTypeSDSC:
            if (bpb->bsec < 16384)
            {
                /* Up to 8MiB, use FAT12 and 16 sectors per cluster */
                *fat = 12;
                bpb->spc = 16;
            }
            else if (bpb->bsec < 128 * 1024)
            {
                /* Up to 64MiB, use FAT12 and 32 sectors per cluster */
                *fat = 12;
                bpb->spc = 32;
            }
            else if (bpb->bsec < 2 * 1024 * 1024)
            {
                /* Up to 1GiB, use FAT16 and 32 sectors per cluster */
                *fat = 16;
                bpb->spc = 32;
            }
            else
            {
                /* 1GiB or larger, use FAT16 and 64 sectors per cluster */
                *fat = 16;
                bpb->spc = 64;
            }
            break;
        case kCardTypeSDHC:
            *fat = 32;
            bpb->spc = 64;
            break;
        case kCardTypeSDXC:
            newfs_print(newfs_ctx, LOG_INFO, "%s: newfs_exfat should be used for SDXC media", path);
            break;
    }
}

/*
 * Print out BPB values.
 */
void
print_bpb(struct bpb *bpb)
{
    newfs_print(newfs_ctx, LOG_INFO, "bps=%u spc=%u res=%u nft=%u", bpb->bps, bpb->spc, bpb->res,
                bpb->nft);
    if (bpb->rde) {
        newfs_print(newfs_ctx, LOG_INFO, " rde=%u", bpb->rde);
    }
    if (bpb->sec) {
        newfs_print(newfs_ctx, LOG_INFO, " sec=%u", bpb->sec);
    }
    newfs_print(newfs_ctx, LOG_INFO, " mid=%#x", bpb->mid);
    if (bpb->spf) {
        newfs_print(newfs_ctx, LOG_INFO, " spf=%u", bpb->spf);
    }
    newfs_print(newfs_ctx, LOG_INFO, " spt=%u hds=%u hid=%u drv=0x%02X", bpb->spt, bpb->hds, bpb->hid, bpb->driveNum);
    if (bpb->bsec) {
        newfs_print(newfs_ctx, LOG_INFO, " bsec=%u", bpb->bsec);
    }
    if (!bpb->spf) {
        newfs_print(newfs_ctx, LOG_INFO, " bspf=%u rdcl=%u", bpb->bspf, bpb->rdcl);
        newfs_print(newfs_ctx, LOG_INFO, " infs=");
        newfs_print(newfs_ctx, LOG_INFO, bpb->infs == MAXU16 ? "%#x" : "%u", bpb->infs);
        newfs_print(newfs_ctx, LOG_INFO, " bkbs=");
        newfs_print(newfs_ctx, LOG_INFO, bpb->bkbs == MAXU16 ? "%#x" : "%u", bpb->bkbs);
    }
    newfs_print(newfs_ctx, LOG_INFO, "\n");
}

/*
 * Convert and check a numeric option argument.
 */
u_int
argtou(const char *arg, u_int lo, u_int hi, const char *msg)
{
    char *s;
    u_long x;

    errno = 0;
    x = strtoul(arg, &s, 0);
    if (errno || !*arg || *s || x < lo || x > hi) {
        newfs_print(newfs_ctx, LOG_ERR, "%s: bad %s", arg, msg);
        return UINT_MAX;
    }
    return (u_int)x;
}

/*
 * Check a volume label.
 */
int
oklabel(const char *src)
{
    int c = 0, i = 0;

    for (i = 0; i <= 11; i++) {
        c = (u_char)*src++;
        if (c < ' ' + !i || strchr("\"*+,./:;<=>?[\\]|", c)) {
            break;
        }
    }
    return !c;
}

/*
 * Make a volume label.
 */
void
mklabel(u_int8_t *dest, const char *src)
{
    int c, i;

    for (i = 0; i < 11; i++) {
        c = *src ? toupper(*src++) : ' ';
        *dest++ = !i && c == '\xe5' ? 5 : c;
    }
}

/*
 * Copy string, padding with spaces.
 */
void
setstr(u_int8_t *dest, const char *src, size_t len)
{
    while (len--) {
        *dest++ = *src ? *src++ : ' ';
    }
}

/** Wipes our device by calling directly to wipefs library */
int wipefs(newfs_client_ctx_t ctx, WipeFSProperties props)
{
    wipefs_ctx wiper;
    int error = wipefs_alloc(props.fd, props.block_size, &wiper);
    if (error) {
        newfs_print(newfs_ctx, LOG_ERR, "wipefs_alloc(): fd(%d) %s", props.fd, strerror(error));
        return error;
    }
    error = wipefs_except_blocks(wiper, props.except_block_start, props.except_block_length);
    if (error) {
        newfs_print(newfs_ctx, LOG_ERR, "wipefs_except_blocks(): fd(%d) %s", props.fd, strerror(error));
        goto exit;
    }
    error = wipefs_wipe(wiper);
    if (error) {
        newfs_print(newfs_ctx, LOG_ERR, "wipefs_wipe(): fd(%d) %s", props.fd, strerror(error));
        goto exit;
    }
exit:
    wipefs_free(&wiper);
    return error;
}
