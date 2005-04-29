/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/* Various system headers use standard int types */
#include <stdint.h>

/* Get the boolean_t type. */
#include <mach/machine/boolean.h>

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/loadable_fs.h>

#include <sys/disk.h>

#include <err.h>
#include <sysexits.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h> 

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFStringEncodingExt.h>

#define BYTE_ORDER_MARK	0xFEFF

#include "../ntfs.kextproj/ntfs.h"

#define FS_TYPE			"ntfs"
#define FS_NAME_FILE		"NTFS"
#define FS_BUNDLE_NAME		"ntfs.kext"
#define FS_KEXT_DIR		"/System/Library/Extensions/ntfs.kext"
#define FS_KMOD_DIR		"/System/Library/Extensions/ntfs.kext/ntfs"
#define RAWDEV_PREFIX		"/dev/r"
#define BLOCKDEV_PREFIX		"/dev/"
#define MOUNT_COMMAND		"/sbin/mount"
#define UMOUNT_COMMAND		"/sbin/umount"
#define KEXTLOAD_COMMAND	"/sbin/kextload"
#define READWRITE_OPT		"-w"
#define READONLY_OPT		"-r"
#define SUID_OPT		"suid"
#define NOSUID_OPT		"nosuid"
#define DEV_OPT			"dev"
#define NODEV_OPT		"nodev"

#define FSUC_LABEL		'n'

#define	DEVICE_SUID		"suid"
#define	DEVICE_NOSUID		"nosuid"

#define	DEVICE_DEV		"dev"
#define	DEVICE_NODEV		"nodev"

#define MAX_BLOCK_SIZE		2048
#define MAX_CLUSTER_SIZE	32768

/* globals */
const char	*progname;	/* our program name, from argv[0] */
int		debug;		/* use -D to enable debug printfs */



/*
 * The following code is re-usable for all FS_util programs
 */
void usage(void);

static int fs_probe(char *devpath, int removable, int writable);
static int fs_mount(char *devpath, char *mount_point, int removable, 
	int writable, int suid, int dev);
static int fs_unmount(char *devpath);
static int fs_label(char *devpath, char *volName);
static void fs_set_label_file(char *labelPtr);
static void safe_execv(const char *args[]);

#ifdef DEBUG
static void report_exit_code(int ret);
#endif

static int checkLoadable();

#define LABEL_LENGTH	1024
#define UNKNOWN_LABEL	"Untitled"
char diskLabel[LABEL_LENGTH];


void usage()
{
        fprintf(stderr, "usage: %s action_arg device_arg [mount_point_arg] [Flags]\n", progname);
        fprintf(stderr, "action_arg:\n");
        fprintf(stderr, "       -%c (Probe)\n", FSUC_PROBE);
        fprintf(stderr, "       -%c (Mount)\n", FSUC_MOUNT);
        fprintf(stderr, "       -%c (Unmount)\n", FSUC_UNMOUNT);
        fprintf(stderr, "       -%c name\n", 'n');
        fprintf(stderr, "device_arg:\n");
        fprintf(stderr, "       device we are acting upon (for example, 'disk0s2')\n");
        fprintf(stderr, "mount_point_arg:\n");
        fprintf(stderr, "       required for Mount and Force Mount \n");
        fprintf(stderr, "Flags:\n");
        fprintf(stderr, "       required for Mount, Force Mount and Probe\n");
        fprintf(stderr, "       indicates removable or fixed (for example 'fixed')\n");
        fprintf(stderr, "       indicates readonly or writable (for example 'readonly')\n");
        fprintf(stderr, "Flags (mount only):\n");
        fprintf(stderr, "	indicates suid or nosuid\n");
        fprintf(stderr, "	indicates dev or nodev\n");
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "		%s -p disk0s2 fixed writable\n", progname);
        fprintf(stderr, "		%s -m disk0s2 /my/hfs removable readonly nosuid nodev\n", progname);
        exit(FSUR_INVAL);
}

int main(int argc, char **argv)
{
    char		rawdevpath[MAXPATHLEN];
    char		blockdevpath[MAXPATHLEN];
    char		opt;
    struct stat	sb;
    int			ret = FSUR_INVAL;

    /* save & strip off program name */
    progname = argv[0];
    argc--;
    argv++;

    /* secret debug flag - must be 1st flag */
    debug = (argc > 0 && !strcmp(argv[0], "-D"));
    if (debug) { /* strip off debug flag argument */
        argc--;
        argv++;
    }

    if (argc < 2 || argv[0][0] != '-')
        usage();
    opt = argv[0][1];
    if (opt != FSUC_PROBE && opt != FSUC_MOUNT && opt != FSUC_UNMOUNT && opt != FSUC_LABEL)
        usage(); /* Not supported action */
    if ((opt == FSUC_MOUNT || opt == FSUC_UNMOUNT || opt == FSUC_LABEL) && argc < 3)
        usage(); /* mountpoint arg missing! */

    sprintf(rawdevpath, "%s%s", RAWDEV_PREFIX, argv[1]);
    if (stat(rawdevpath, &sb) != 0) {
        fprintf(stderr, "%s: stat %s failed, %s\n", progname, rawdevpath,
                strerror(errno));
        exit(FSUR_INVAL);
    }

    sprintf(blockdevpath, "%s%s", BLOCKDEV_PREFIX, argv[1]);
    if (stat(blockdevpath, &sb) != 0) {
        fprintf(stderr, "%s: stat %s failed, %s\n", progname, blockdevpath,
                strerror(errno));
        exit(FSUR_INVAL);
    }

    switch (opt) {
        case FSUC_PROBE: {
            if (argc != 4)
                usage();
            ret = fs_probe(rawdevpath,
                        strcmp(argv[2], DEVICE_FIXED),
                        strcmp(argv[3], DEVICE_READONLY));
            break;
        }

        case FSUC_MOUNT:
        case FSUC_MOUNT_FORCE:
            if (argc != 7)
                usage();
            if (strcmp(argv[3], DEVICE_FIXED) && strcmp(argv[3], DEVICE_REMOVABLE)) {
                printf("ntfs.util: ERROR: unrecognized flag (removable/fixed) argv[%d]='%s'\n",3,argv[3]);
                usage();
            }
                if (strcmp(argv[4], DEVICE_READONLY) && strcmp(argv[4], DEVICE_WRITABLE)) {
                    printf("ntfs.util: ERROR: unrecognized flag (readonly/writable) argv[%d]='%s'\n",4,argv[4]);
                    usage();
                }
                if (strcmp(argv[5], DEVICE_SUID) && strcmp(argv[5], DEVICE_NOSUID)) {
                    printf("ntfs.util: ERROR: unrecognized flag (suid/nosuid) argv[%d]='%s'\n",5,argv[5]);
                    usage();
                }
                if (strcmp(argv[6], DEVICE_DEV) && strcmp(argv[6], DEVICE_NODEV)) {
                    printf("ntfs.util: ERROR: unrecognized flag (dev/nodev) argv[%d]='%s'\n",6,argv[6]);
                    usage();
                }
                ret = fs_mount(blockdevpath,
                            argv[2],
                            strcmp(argv[3], DEVICE_FIXED),
                            strcmp(argv[4], DEVICE_READONLY),
                            strcmp(argv[5], DEVICE_NOSUID),
                            strcmp(argv[6], DEVICE_NODEV));
            break;
        case FSUC_UNMOUNT:
            ret = fs_unmount(rawdevpath);
            break;
        case FSUC_LABEL:
            ret = fs_label(rawdevpath, argv[2]);
            break;
        default:
            usage();
    }

    #ifdef DEBUG
    report_exit_code(ret);
    #endif
    exit(ret);

    return(ret);
}

/*
 * Mount the filesystem
 */
static int fs_mount(char *devpath, char *mount_point, int removable, int writable, int suid, int dev) {
    int ret;
    const char *kextargs[] = {KEXTLOAD_COMMAND, FS_KEXT_DIR, NULL};
    const char *mountargs[] = {MOUNT_COMMAND, READWRITE_OPT, "-o", SUID_OPT, "-o",
        DEV_OPT, "-t", FS_TYPE, devpath, mount_point, NULL};

/*¥    if (! writable)		// Force NTFS to mount read-only in Darwin */
        mountargs[1] = READONLY_OPT;

    if (! suid)
        mountargs[3] = NOSUID_OPT;

    if (! dev)
        mountargs[5] = NODEV_OPT;

    if (checkLoadable())
        safe_execv(kextargs);
    safe_execv(mountargs);
    ret = FSUR_IO_SUCCESS;

    return ret;
}

/*
 * Unmount a filesystem
 */
static int fs_unmount(char *devpath) {
        const char *umountargs[] = {UMOUNT_COMMAND, devpath, NULL};

        safe_execv(umountargs);
        return(FSUR_IO_SUCCESS);
}


/* Return non-zero if the file system is not yet loaded. */
static int checkLoadable(void)
{
	int error;
	struct vfsconf vfc;
	
	error = getvfsbyname(FS_TYPE, &vfc);

	return error;
}



#ifdef DEBUG
static void
report_exit_code(int ret)
{
    printf("...ret = %d\n", ret);

    switch (ret) {
    case FSUR_RECOGNIZED:
	printf("File system recognized; a mount is possible.\n");
	break;
    case FSUR_UNRECOGNIZED:
	printf("File system unrecognized; a mount is not possible.\n");
	break;
    case FSUR_IO_SUCCESS:
	printf("Mount, unmount, or repair succeeded.\n");
	break;
    case FSUR_IO_FAIL:
	printf("Unrecoverable I/O error.\n");
	break;
    case FSUR_IO_UNCLEAN:
	printf("Mount failed; file system is not clean.\n");
	break;
    case FSUR_INVAL:
	printf("Invalid argument.\n");
	break;
    case FSUR_LOADERR:
	printf("kern_loader error.\n");
	break;
    case FSUR_INITRECOGNIZED:
	printf("File system recognized; initialization is possible.\n");
	break;
    }
}
#endif

static void
safe_execv(const char *args[])
{
	int		pid;
	union wait	status;

	pid = fork();
	if (pid == 0) {
		(void)execv(args[0], args);
		fprintf(stderr, "%s: execv %s failed, %s\n", progname, args[0],
			strerror(errno));
		exit(FSUR_IO_FAIL);
	}
	if (pid == -1) {
		fprintf(stderr, "%s: fork failed, %s\n", progname,
			strerror(errno));
		exit(FSUR_IO_FAIL);
	}
	if (wait4(pid, (int *)&status, 0, NULL) != pid) {
		fprintf(stderr, "%s: BUG executing %s command\n", progname,
			args[0]);
		exit(FSUR_IO_FAIL);
	} else if (!WIFEXITED(status)) {
		fprintf(stderr, "%s: %s command aborted by signal %d\n",
			progname, args[0], WTERMSIG(status));
		exit(FSUR_IO_FAIL);
	} else if (WEXITSTATUS(status)) {
		fprintf(stderr, "%s: %s command failed, exit status %d: %s\n",
			progname, args[0], WEXITSTATUS(status),
			strerror(WEXITSTATUS(status)));
		exit(FSUR_IO_FAIL);
	}
}


/*
 * Begin Filesystem-specific code
 */

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
		printf("ntfs_fixup: magic doesn't match: %08x != %08x\n",
		       fixup_magic, magic);
		return (EINVAL);
	}
        fixup_count = OSReadLittleInt16(&fhp->fh_fnum,0);
	if ((fixup_count - 1) * bytesPerSector != len) {
		printf("ntfs_fixup: " \
		       "bad fixups number: %d for %ld bytes block\n", 
		       fixup_count, (long)len);	/* XXX printf kludge */
		return (EINVAL);
	}
        fixup_offset = OSReadLittleInt16(&fhp->fh_foff,0);
	if (fixup_offset >= len) {
		printf("ntfs_fixup: invalid offset: %x", fixup_offset);
		return (EINVAL);
	}
	fxp = (u_int16_t *) (buf + fixup_offset);
	cfxp = (u_int16_t *) (buf + bytesPerSector - 2);
	fixup = *fxp++;
	for (i = 1; i < fixup_count; i++, fxp++) {
		if (*cfxp != fixup) {
			printf("ntfs_fixup: fixup %d doesn't match\n", i);
			return (EINVAL);
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
    
    /*¥ Should we also check offset < buffer size? */
    while (attr->a_hdr.a_type != 0xFFFFFFFF)	/* same for big/little endian */
    {
        if (OSReadLittleInt32(&attr->a_hdr.a_type,0) == attrType)
        {
            if (attr->a_hdr.a_flag != 0)
            {
                if (debug)
                    fprintf(stderr, "%s: attriubte 0x%X is non-resident\n", progname, attrType);
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


/*
 * Examine a volume to see if we recognize it as a mountable.
 */
static int fs_probe(char *devpath, int removable, int writable)
{
    int fd;
    struct bootfile *boot;
    unsigned bytesPerSector;
    unsigned sectorsPerCluster;
    int mftRecordSize;
    u_int64_t totalClusters;
    u_int64_t cluster, mftCluster;
    size_t mftOffset;
    void *nameAttr;
    CFStringRef str;
    size_t nameSize;
    char buf[MAX_CLUSTER_SIZE];

    fd = open(devpath, O_RDONLY, 0);
    if (fd < 0)
    {
        fprintf(stderr, "%s: open %s failed, %s\n", progname, devpath,
                strerror(errno));
        exit(FSUR_IO_FAIL);
    }
    
    /*
     * Read the boot sector, check signatures, and do some minimal
     * sanity checking.  NOTE: the size of the read below is intended
     * to be a multiple of all supported block sizes, so we don't
     * have to determine or change the device's block size.
     */
    if (read(fd, buf, MAX_BLOCK_SIZE) != MAX_BLOCK_SIZE)
    {
        fprintf(stderr, "%s: error reading boot sector: %s\n", progname,
                strerror(errno));
        exit(FSUR_IO_FAIL);
    }
    
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
        return FSUR_UNRECOGNIZED;
    }

    /*
     * Check the "NTFS    " signature.
     */
    if (memcmp(boot->bf_sysid, "NTFS    ", 8) != 0)
    {
        return FSUR_UNRECOGNIZED;
    }

    /*
     * Make sure the bytes per sector and sectors per cluster are
     * powers of two, and within reasonable ranges.
     */
    bytesPerSector = OSReadLittleInt16(&boot->bf_bps,0);
    if ((bytesPerSector & (bytesPerSector-1)) || bytesPerSector < 512 || bytesPerSector > 32768)
    {
        if (debug)
            fprintf(stderr, "%s: invalid bytes per sector (%d)\n", progname, bytesPerSector);
        return FSUR_UNRECOGNIZED;
    }

    sectorsPerCluster = boot->bf_spc;	/* Just one byte; no swapping needed */
    if ((sectorsPerCluster & (sectorsPerCluster-1)) || sectorsPerCluster > 128)
    {
        if (debug)
            fprintf(stderr, "%s: invalid sectors per cluster (%d)\n", progname, bytesPerSector);
        return FSUR_UNRECOGNIZED;
    }
    
    /*
     * Calculate the number of clusters from the number of sectors.
     * Then bounds check the $MFT and $MFTMirr clusters.
     */
    totalClusters = OSReadLittleInt64(&boot->bf_spv,0) / sectorsPerCluster;
    mftCluster = OSReadLittleInt64(&boot->bf_mftcn,0);
    if (mftCluster > totalClusters)
    {
        if (debug)
            fprintf(stderr, "%s: invalid $MFT cluster (%lld)\n", progname, mftCluster);
        return FSUR_UNRECOGNIZED;
    }
    cluster = OSReadLittleInt64(&boot->bf_mftmirrcn,0);
    if (cluster > totalClusters)
    {
        if (debug)
            fprintf(stderr, "%s: invalid $MFTMirr cluster (%lld)\n", progname, cluster);
        return FSUR_UNRECOGNIZED;
    }
    
    /*
     * Determine the size of an MFT record.
     */
    mftRecordSize = (int8_t) boot->bf_mftrecsz;
    if (mftRecordSize < 0)
        mftRecordSize = 1 << -mftRecordSize;
    else
        mftRecordSize *= bytesPerSector * sectorsPerCluster;
    if (debug)
        fprintf(stderr, "%s: MFT record size = %d\n", progname, mftRecordSize);

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
    if (lseek(fd, mftOffset, SEEK_SET) == -1)
    {
        if (debug)
            fprintf(stderr, "%s: lseek to $Volume failed: %s\n", progname, strerror(errno));
        return FSUR_IO_FAIL;
    }
    if (read(fd, buf, mftRecordSize) != mftRecordSize)
    {
        if (debug)
        {
            fprintf(stderr, "%s: error reading MFT $Volume record: %s\n", progname,
                strerror(errno));
        }
        return FSUR_IO_FAIL;
    }
    if (ntfs_fixup(buf, mftRecordSize, NTFS_FILEMAGIC, bytesPerSector) != 0)
    {
        if (debug)
            fprintf(stderr, "%s: block fixup failed\n", progname);
        return FSUR_UNRECOGNIZED;
    }
    
    /*
     * Loop over the attributes, looking for $VOLUME_NAME (0x60).
     */
    if(ntfs_find_attr(buf, NTFS_A_VOLUMENAME, &nameAttr, &nameSize) != 0)
    {
        if (debug)
            fprintf(stderr, "%s: $VOLUME_NAME attribute not found\n", progname);
        return FSUR_UNRECOGNIZED;
    }
    
    /*
     * Convert the volume name from UTF-16 (little endian) to UTF-8.
     * The CoreFoundation routines don't have any way to indicate the
     * byte order of UTF-16; they always assume the platform native order.
     * To work around this, stuff a byte order mark before the start of
     * the UTF-16, which will clobber some other data in the MFT record
     * that we no longer need.  Another alternative would be to endian
     * convert each character before calling CF.
     */
    diskLabel[0] = 0;
    nameAttr = (UInt16 *) nameAttr - 1;
    OSWriteLittleInt16(nameAttr, 0, BYTE_ORDER_MARK);
    nameSize += sizeof(UInt16);
    str = CFStringCreateWithBytes(kCFAllocatorDefault, nameAttr, nameSize, kCFStringEncodingUnicode, true);
    (void) CFStringGetCString(str, diskLabel, sizeof(diskLabel), kCFStringEncodingUTF8);

    /*
     * Return/store the UTF-8 name
     */
    fs_set_label_file(diskLabel);

    (void) close(fd);
    
    return FSUR_RECOGNIZED;
}

static int fs_label(char *devpath, char *volName)
{
    if (debug)
        fprintf(stderr, "%s: setting volume name is unimplemented\n", progname);
    return FSUR_IO_FAIL;
}


/* Store and return the name of this volume */
static void fs_set_label_file(char *labelPtr)
{
    int 		fd;
    unsigned char	filename[MAXPATHLEN];
    unsigned char	*tempPtr;

    sprintf(filename, "%s/%s%s/%s.label", FS_DIR_LOCATION,
            FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
    unlink(filename);

    sprintf(filename, "%s/%s%s/%s.name", FS_DIR_LOCATION,
            FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
    unlink(filename);

if (0) {
    if (labelPtr[0] != '\0') {
        /* replace any embedded spaces or slashes (mount doesn't like them) */
        tempPtr = labelPtr;
        while(*tempPtr != '\0') {
            if(*tempPtr == ' ' || *tempPtr == '/') {
                *tempPtr = '_';
            }
            tempPtr++;
        }
    } else {
        strncpy(labelPtr, UNKNOWN_LABEL, LABEL_LENGTH);
    }
}

    if (labelPtr[0] == '\0') {
    	strncpy(labelPtr, UNKNOWN_LABEL, LABEL_LENGTH);
    }

    /* At this point, label should contain a correct formatted name */
    write(1, labelPtr, strlen(labelPtr));

    /* backwards compatibility */
    /* write the .label file */
    sprintf(filename, "%s/%s%s/%s.label", FS_DIR_LOCATION,
            FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
    fd = open(filename, O_WRONLY|O_CREAT|O_EXCL, 0755);
    if (fd >= 0) {
	write(fd, labelPtr, strlen(labelPtr) + 1);
	close(fd);
    }
    /* write the .name file */
    sprintf(filename, "%s/%s%s/%s.name", FS_DIR_LOCATION,
            FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
    fd = open(filename, O_WRONLY|O_CREAT|O_EXCL, 0755);
    if (fd >= 0) {
	write(fd, FS_NAME_FILE, 1 + strlen(FS_NAME_FILE));
	close(fd);
    }
}
