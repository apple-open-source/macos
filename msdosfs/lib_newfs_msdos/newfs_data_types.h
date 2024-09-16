//
//  newfs_data_types.h
//  msdosfs
//  The contents of this file were taken from main.c (newfs_msdos)
//
//  Created by Kujan Lauz on 04/09/2022.
//

#ifndef newfs_data_types_h
#define newfs_data_types_h

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/disk.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOStorageCardCharacteristics.h>

#define MAXU16      0xffff    /* maximum unsigned 16-bit quantity */
#define BPN      4        /* bits per nibble */
#define NPB      2        /* nibbles per byte */

#define DOSMAGIC  0xaa55    /* DOS magic number */
#define MINBPS      128        /* minimum bytes per sector */
#define MAXBPS    4096        /* maximum bytes per sector */
#define MAXSPC      128        /* maximum sectors per cluster */
#define MAXNFT      16        /* maximum number of FATs */
#define DEFBLK      4096        /* default block size */
#define DEFBLK16  2048        /* default block size FAT16 */
#define DEFRDE      512        /* default root directory entries */
#define RESFTE      2        /* reserved FAT entries */

/*
 * The size of our in-memory I/O buffer.  This is the size of the writes we
 * do to the device (except perhaps a few odd sectors at the end).
 *
 * This must be a multiple of the sector size.  Larger is generally faster,
 * but some old devices have bugs if you ask them to do more than 128KB
 * per I/O.
 */
#define IO_BUFFER_SIZE    (128*1024)

/*
 * [2873845]  FAT12 volumes can have 1..4084 clusters.  FAT16 can have
 * 4085..65524 clusters.  FAT32 is 65525 clusters or more.
 * Since many other implementations are off by 1, 2, 4, 8, 10, or 16,
 * Microsoft recommends staying at least 16 clusters away from these
 * boundary points.  They also recommend that FAT32 volumes avoid
 * making the bad cluster mark an allocatable cluster number.
 *
 * So, the minimum and maximum values listed below aren't the strict
 * limits (smaller or larger values may work on more robust implementations).
 * The limits below are safe limits that should be compatible with a
 * wide variety of implementations.
 */
#define MINCLS12  1        /* minimum FAT12 clusters */
#define MINCLS16  4085        /* minimum FAT16 clusters */
#define MINCLS32  65525        /* minimum FAT32 clusters */
#define MAXCLS12  4084         /* maximum FAT12 clusters */
#define MAXCLS16  65524        /* maximum FAT16 clusters */
#define MAXCLS32  0x0FFFFFF5    /* maximum FAT32 clusters */

#define BACKUP_BOOT_SECTOR 6    /* Default location for backup boot sector on FAT32 */
#define FAT32_RESERVED_SECTORS 32

#define mincls(fat)  ((fat) == 12 ? MINCLS12 :    \
              (fat) == 16 ? MINCLS16 :    \
                    MINCLS32)

#define maxcls(fat)  ((fat) == 12 ? MAXCLS12 :    \
              (fat) == 16 ? MAXCLS16 :    \
                    MAXCLS32)

#define mk1(p, x)                \
    (p) = (u_int8_t)(x)

#define mk2(p, x)                \
    (p)[0] = (u_int8_t)(x),            \
    (p)[1] = (u_int8_t)((x) >> 010)

#define mk4(p, x)                \
    (p)[0] = (u_int8_t)(x),            \
    (p)[1] = (u_int8_t)((x) >> 010),        \
    (p)[2] = (u_int8_t)((x) >> 020),        \
    (p)[3] = (u_int8_t)((x) >> 030)

#define argto1(arg, lo, msg)  argtou(arg, lo, 0xff, msg)
#define argto2(arg, lo, msg)  argtou(arg, lo, 0xffff, msg)
#define argto4(arg, lo, msg)  argtou(arg, lo, 0xffffffff, msg)
#define argtox(arg, lo, msg)  argtou(arg, lo, UINT_MAX, msg)

struct bs {
    u_int8_t jmp[3];        /* bootstrap entry point */
    u_int8_t oem[8];        /* OEM name and version */
};

struct bsbpb {
    u_int8_t bps[2];        /* bytes per sector */
    u_int8_t spc;        /* sectors per cluster */
    u_int8_t res[2];        /* reserved sectors */
    u_int8_t nft;        /* number of FATs */
    u_int8_t rde[2];        /* root directory entries */
    u_int8_t sec[2];        /* total sectors */
    u_int8_t mid;        /* media descriptor */
    u_int8_t spf[2];        /* sectors per FAT */
    u_int8_t spt[2];        /* sectors per track */
    u_int8_t hds[2];        /* drive heads */
    u_int8_t hid[4];        /* hidden sectors */
    u_int8_t bsec[4];        /* big total sectors */
};

struct bsxbpb {
    u_int8_t bspf[4];        /* big sectors per FAT */
    u_int8_t xflg[2];        /* FAT control flags */
    u_int8_t vers[2];        /* file system version */
    u_int8_t rdcl[4];        /* root directory start cluster */
    u_int8_t infs[2];        /* file system info sector */
    u_int8_t bkbs[2];        /* backup boot sector */
    u_int8_t rsvd[12];        /* reserved */
};

struct bsx {
    u_int8_t drv;        /* drive number */
    u_int8_t rsvd;        /* reserved */
    u_int8_t sig;        /* extended boot signature */
    u_int8_t volid[4];        /* volume ID number */
    u_int8_t label[11];     /* volume label */
    u_int8_t type[8];        /* file system type */
};

struct de {
    u_int8_t namext[11];    /* name and extension */
    u_int8_t attr;        /* attributes */
    u_int8_t rsvd[10];        /* reserved */
    u_int8_t time[2];        /* creation time */
    u_int8_t date[2];        /* creation date */
    u_int8_t clus[2];        /* starting cluster */
    u_int8_t size[4];        /* size */
};

struct bpb {
    u_int bps;            /* bytes per sector */
    u_int spc;            /* sectors per cluster */
    u_int res;            /* reserved sectors */
    u_int nft;            /* number of FATs */
    u_int rde;            /* root directory entries */
    u_int sec;            /* total sectors */
    u_int mid;            /* media descriptor */
    u_int spf;            /* sectors per FAT */
    u_int spt;            /* sectors per track */
    u_int hds;            /* drive heads */
    u_int hid;            /* hidden sectors */
    u_int bsec;         /* big total sectors */
    u_int bspf;         /* big sectors per FAT */
    u_int rdcl;         /* root directory start cluster */
    u_int infs;         /* file system info sector */
    u_int bkbs;         /* backup boot sector */
    u_int driveNum;             /* INT 0x13 drive number (0x00 or 0x80) */
};



static u_int8_t bootcode[] = {
    0xfa,            /* cli            */
    0x31, 0xc0,         /* xor       ax,ax    */
    0x8e, 0xd0,         /* mov       ss,ax    */
    0xbc, 0x00, 0x7c,        /* mov       sp,7c00h */
    0xfb,            /* sti            */
    0x8e, 0xd8,         /* mov       ds,ax    */
    0xe8, 0x00, 0x00,        /* call    $ + 3    */
    0x5e,            /* pop       si        */
    0x83, 0xc6, 0x19,        /* add       si,+19h  */
    0xbb, 0x07, 0x00,        /* mov       bx,0007h */
    0xfc,            /* cld            */
    0xac,            /* lodsb        */
    0x84, 0xc0,         /* test    al,al    */
    0x74, 0x06,         /* jz       $ + 8    */
    0xb4, 0x0e,         /* mov       ah,0eh   */
    0xcd, 0x10,         /* int       10h        */
    0xeb, 0xf5,         /* jmp       $ - 9    */
    0x30, 0xe4,         /* xor       ah,ah    */
    0xcd, 0x16,         /* int       16h        */
    0xcd, 0x19,         /* int       19h        */
    0x0d, 0x0a,
    'N', 'o', 'n', '-', 's', 'y', 's', 't',
    'e', 'm', ' ', 'd', 'i', 's', 'k',
    0x0d, 0x0a,
    'P', 'r', 'e', 's', 's', ' ', 'a', 'n',
    'y', ' ', 'k', 'e', 'y', ' ', 't', 'o',
    ' ', 'r', 'e', 'b', 'o', 'o', 't',
    0x0d, 0x0a,
    0
};

/*
 * These values define the default crossover points for selecting the default
 * FAT type.  The intent here is to have the crossover points be the same as
 * Microsoft documents, at least for 512 bytes per sector devices.  As much
 * as possible, the same crossover point (in terms of bytes per volume) is used
 * for larger sector sizes.  But the 4.1MB crossover between FAT12 and FAT16
 * is not achievable for sector sizes larger than 1KB since it would result
 * in fewer than 4085 clusters, making FAT16 impossible; in that case, the
 * crossover is in terms of sectors, not bytes.
 *
 * Note that the FAT16 to FAT32 crossover is only good for sector sizes up to
 * and including 4KB.  For larger sector sizes, there would be too few clusters
 * for FAT32.
 */
enum {
    MAX_SEC_FAT12_512    = 8400,        /* (4.1 MB) Maximum 512 byte sectors to default to FAT12 */
    MAX_SEC_FAT12    = 4200,        /* Maximum sectors (>512 bytes) to default to FAT12 */
    MAX_KB_FAT16    = 524288    /* (512 MiB) Maximum kilobytes to default to FAT16 */
};

/*
 * [2873851] Tables of default cluster sizes for FAT16 and FAT32.
 *
 * These constants are derived from Microsoft's documentation, but adjusted
 * to represent kilobytes of volume size, not a number of 512-byte sectors.
 * Also, this table uses default cluster size, not sectors per cluster, so
 * that it can be independent of sector size.
 */

struct DiskSizeToClusterSize {
    u_int64_t kilobytes;        /* input: maximum kilobytes */
    u_int32_t bytes_per_cluster;    /* output: desired cluster size (in bytes) */
};

enum SDCardType {
    kCardTypeNone   =    0,
    kCardTypeSDSC,
    kCardTypeSDHC,
    kCardTypeSDXC
};

struct StdFormat {
    const char *name;
    struct bpb bpb;
};

#endif /* newfs_data_types_h */
