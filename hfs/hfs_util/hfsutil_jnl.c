/*
 * Copyright (c) 1999-2001 Apple Computer, Inc. All rights reserved.
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
/*
 Copyright (c) 2002 Apple Computer, Inc.
 All Rights Reserved.

 This file contains the routine to make an HFS+ volume journaled
 and a corresponding routine to turn it off.
 
 */

#include <sys/types.h>
#include <sys/attr.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/vmmeter.h>
#include <sys/mount.h>
#include <sys/wait.h>

#include <dev/disk.h>
#include <sys/loadable_fs.h>
#include <hfs/hfs_format.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <architecture/byte_order.h>

//
// Secret HFS sysctl's to instruct it to turn
// journaling on/off for a volume.
//
#define HFS_BECOME_JOURNALED   0x082969
#define HFS_BECOME_UNJOURNALED 0x031272


/* getattrlist buffers start with an extra length field */
struct ExtentsAttrBuf {
	unsigned long	infoLength;
	HFSPlusExtentRecord	extents;
};
typedef struct ExtentsAttrBuf ExtentsAttrBuf;



#define kIsInvisible 0x4000

/*
 * Generic Finder file/dir data
 */
struct FinderInfo {
	u_int32_t 	opaque_1[2];
	u_int16_t 	fdFlags;	/* Finder flags */
	int16_t 	opaque_2[11];
};
typedef struct FinderInfo FinderInfo;

/* getattrlist buffers start with an extra length field */
struct FinderAttrBuf {
	unsigned long	infoLength;
	FinderInfo	finderInfo;
};
typedef struct FinderAttrBuf FinderAttrBuf;


int hide_file(const char * file)
{
    struct attrlist alist = {0};
    FinderAttrBuf finderInfoBuf = {0};
    int result;
    
    alist.bitmapcount = ATTR_BIT_MAP_COUNT;
    alist.commonattr = ATTR_CMN_FNDRINFO;

    result = getattrlist(file, &alist, &finderInfoBuf, sizeof(finderInfoBuf), 0);
    if (result) {
		return (errno);
	}
	
    if (finderInfoBuf.finderInfo.fdFlags & kIsInvisible) {
		printf("hide: %s is alreadly invisible\n", file);
		return (0);
    }

    finderInfoBuf.finderInfo.fdFlags |= kIsInvisible;

    result = setattrlist(file, &alist, &finderInfoBuf.finderInfo, sizeof(FinderInfo), 0);
    
    return (result == -1 ? errno : result);
}

int
get_start_block(const char *file)
{
    struct attrlist alist = {0};
    ExtentsAttrBuf extentsbuf = {0};

    alist.bitmapcount = ATTR_BIT_MAP_COUNT;
    alist.fileattr = ATTR_FILE_DATAEXTENTS;

    if (getattrlist(file, &alist, &extentsbuf, sizeof(extentsbuf), 0)) {
		fprintf(stderr, "could not get attrlist for %s (%s)", file, strerror(errno));
		return -1;
    }

    if (extentsbuf.extents[1].startBlock != 0) {
		fprintf(stderr, "Journal File not contiguous!\n");
		return -1;
    }

    return extentsbuf.extents[0].startBlock;
}

static const char *journal_fname = ".journal";
static const char *jib_fname = ".journal_info_block";

int
DoMakeJournaled(char *volname, int jsize)
{
    int              fd, i, block_size, journal_size = 8*1024*1024;
    char            *buf;
    int              ret;
    fstore_t         fst;
    int              jstart_block, jinfo_block, sysctl_info[8];
    JournalInfoBlock jib;
	struct statfs    sfs;
	static char      tmpname[MAXPATHLEN];

	if (jsize != 0) {
		journal_size = jsize;
	}

	if (statfs(volname, &sfs) != 0) {
		fprintf(stderr, "Can't stat volume %s (%s).\n", volname, strerror(errno));
		return 10;
	}

	// Make sure that we're HFS+.  First we check the fstypename.
	// If that's ok then we try to create a symlink (which won't
	// work on plain hfs volumes but will work on hfs+ volumes).
	//
	sprintf(tmpname, "%s/is_vol_hfs_plus", volname);
	if (strcmp(sfs.f_fstypename, "hfs") != 0 ||
		((ret = symlink(tmpname, tmpname)) != 0 && errno == ENOTSUP)) {
		fprintf(stderr, "%s is not an HFS+ volume.  Journaling only works on HFS+ volumes.\n",
				volname);
		return 10;
	}
	unlink(tmpname);

	if (sfs.f_flags & MNT_JOURNALED) {
		fprintf(stderr, "Volume %s is already journaled.\n", volname);
		return 1;
	}

    if (chdir(volname) != 0) {
		fprintf(stderr, "Can't locate volume %s to make it journaled (%s).\n",
				volname, strerror(errno));
		return 10;
	}

    fd = open(journal_fname, O_CREAT|O_TRUNC|O_RDWR, 000);
    if (fd < 0) {
		fprintf(stderr, "Can't create journal file on volume %s (%s)\n",
				volname, strerror(errno));
		return 5;
    }

	// make sure that it has no r/w/x privs (only could happen if
	// the file already existed since open() doesn't reset the mode
	// bits).
	//
	fchmod(fd, 0);

    block_size = sfs.f_bsize;
    if ((journal_size % block_size) != 0) {
		fprintf(stderr, "Journal size %dk is not a multiple of volume %s block size (%d).\n",
				journal_size/1024, volname, block_size);
		close(fd);
		unlink(journal_fname);
		return 5;
    }

    memset(&fst, 0, sizeof(fst));
    fst.fst_flags   = F_ALLOCATECONTIG|F_ALLOCATEALL;
    fst.fst_length  = journal_size;
    fst.fst_posmode = F_PEOFPOSMODE;
    
    ret = fcntl(fd, F_PREALLOCATE, &fst);
    if (ret < 0) {
		fprintf(stderr, "Pre-allocating the journal file failed on volume %s (%s)\n",
				volname, strerror(errno));
		fprintf(stderr, "Try using a smaller (%d k) journal size\n", journal_size/2/1024);
		close(fd);
		unlink(journal_fname);
		return 10;
    }

    printf("Allocated %lldK for journal file.\n", fst.fst_bytesalloc/1024LL);
    buf = (char *)calloc(block_size, 1);
    if (buf) {
		for(i=0; i < journal_size/block_size; i++) {
			ret = write(fd, buf, block_size);
			if (ret != block_size) {
				break;
			}
		}
		
		if (i*block_size != journal_size) {
			fprintf(stderr, "Failed to write %dk to journal on volume %s (%s)\n",
					journal_size/1024, volname, strerror(errno));
		}
    } else {
		printf("Could not allocate memory to write to the journal on volume %s (%s)\n",
			   volname, strerror(errno));
    }

    fsync(fd);
    close(fd);
    hide_file(journal_fname);

    jstart_block = get_start_block(journal_fname);

    memset(&jib, 0, sizeof(jib));
    jib.flags  = kJIJournalInFSMask;
    jib.offset = (off_t)jstart_block * (off_t)block_size;
    jib.size   = (off_t)journal_size;

    fd = open(jib_fname, O_CREAT|O_TRUNC|O_RDWR, 000);
    if (fd < 0) {
		fprintf(stderr, "Could not create journal info block file on volume %s (%s)\n",
				volname, strerror(errno));
		unlink(journal_fname);
		return 5;
    }
    
    memcpy(buf, &jib, sizeof(jib));
    if (write(fd, buf, block_size) != block_size) {
		fprintf(stderr, "Failed to write journal info block on volume %s (%s)!\n",
				volname, strerror(errno));
		unlink(journal_fname);
		return 10;
    }

    fsync(fd);
    close(fd);
    hide_file(jib_fname);

    jinfo_block = get_start_block(jib_fname);


    //
    // Now make the volume journaled!
    //
    memset(sysctl_info, 0, sizeof(sysctl_info));
    sysctl_info[0] = CTL_VFS;
    sysctl_info[1] = sfs.f_fsid.val[1];
    sysctl_info[2] = HFS_BECOME_JOURNALED;
    sysctl_info[3] = jinfo_block;
    sysctl_info[4] = jstart_block;
    sysctl_info[5] = journal_size;
     
    //printf("fs type: 0x%x\n", sysctl_info[1]);
    //printf("jinfo block : 0x%x\n", jinfo_block);
    //printf("jstart block: 0x%x\n", jstart_block);
    //printf("journal size: 0x%x\n", journal_size);

    ret = sysctl((void *)sysctl_info, 6, NULL, NULL, NULL, 0);
    if (ret != 0) {
		fprintf(stderr, "Failed to make volume %s journaled (%s)\n",
				volname, strerror(errno));
		unlink(journal_fname);
		unlink(jib_fname);
		return 20;
    }

	return 0;
}


int
DoUnJournal(char *volname)
{
	int           result;
    int           sysctl_info[8];
	struct statfs sfs;
	char          jbuf[MAXPATHLEN];
	
	if (statfs(volname, &sfs) != 0) {
		fprintf(stderr, "Can't stat volume %s (%s).\n", volname, strerror(errno));
		return 10;
	}

	if ((sfs.f_flags & MNT_JOURNALED) == 0) {
		fprintf(stderr, "Volume %s is not journaled.\n", volname);
		return 1;
	}

	if (chdir(volname) != 0) {
		fprintf(stderr, "Can't locate volume %s to turn off journaling (%s).\n",
				volname, strerror(errno));
		return 10;
	}
	
	memset(sysctl_info, 0, sizeof(sysctl_info));
	sysctl_info[0] = CTL_VFS;
	sysctl_info[1] = sfs.f_fsid.val[1];
	sysctl_info[2] = HFS_BECOME_UNJOURNALED;
	
	result = sysctl((void *)sysctl_info, 6, NULL, NULL, NULL, 0);
	if (result != 0) {
		fprintf(stderr, "Failed to make volume %s UN-journaled (%s)\n",
				volname, strerror(errno));
		return 20;
	}

	sprintf(jbuf, "%s/%s", volname, journal_fname);
	if (unlink(jbuf) != 0) {
		fprintf(stderr, "Failed to remove the journal %s (%s)\n",
				jbuf, strerror(errno));
	}

	sprintf(jbuf, "%s/%s", volname, jib_fname);
	if (unlink(jbuf) != 0) {
		fprintf(stderr, "Failed to remove the journal info block %s (%s)\n",
				jbuf, strerror(errno));
	}

	printf("Journaling disabled on %s\n", volname);
	    
	return 0;
}
