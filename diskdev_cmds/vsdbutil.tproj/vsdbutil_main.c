/*
 * Copyright (c) 2000-2018 Apple Inc. All rights reserved.
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
	About vsdbutil.c:
		Contains code to manipulate the volume status DB (/var/db/volinfo.database).

	Change History:
	18-Apr-2000	Pat Dirks	New Today.

 */


/* ************************************** I N C L U D E S ***************************************** */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ucred.h>
#include <sys/resource.h>
#include <sys/vmmeter.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <dirent.h>
#include <strings.h>

#include <sys/attr.h>
#include <uuid/uuid.h>
#include <System/uuid/namespace.h>

// This flags array is shared with the mount(8) tool. 
#include "../mount_flags_dir/mount_flags.h"

//from mount_flags_dir/mount_flags.c
extern mountopt_t optnames[];


/*
 * CommonCrypto is meant to be a more stable API than OpenSSL.
 * Defining COMMON_DIGEST_FOR_OPENSSL gives API-compatibility
 * with OpenSSL, so we don't have to change the code.
 */
#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>
#include <libkern/OSByteOrder.h>

static char usage[] = "Usage: %s [-a path] | [-c path ] [-d path] [-i]\n";

static char gHFSTypeName[] = "hfs";
static char gAPFSTypeName[] = "apfs";

/*****************************************************************************
 *
 * The following should really be in some system header file:
 *
 *****************************************************************************/

typedef struct VolumeUUID {
	uuid_t uuid;
} VolumeUUID;

#define VOLUMEUUID64LENGTH 16

#define VOLUME_USEPERMISSIONS 0x00000001
#define VOLUME_VALIDSTATUSBITS ( VOLUME_USEPERMISSIONS )

typedef void *VolumeStatusDBHandle;

void ConvertVolumeUUIDString64ToUUID(const char *UUIDString64, VolumeUUID *volumeID);

int OpenVolumeStatusDB(VolumeStatusDBHandle *DBHandlePtr);
int ConvertVolumeStatusDB(VolumeStatusDBHandle DBHandle);
int GetVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID, u_int32_t *VolumeStatus);
int SetVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID, u_int32_t VolumeStatus);
int DeleteVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID);
int CloseVolumeStatusDB(VolumeStatusDBHandle DBHandle);

/*****************************************************************************
 *
 * Internal function prototypes:
 *
 *****************************************************************************/

static void check_uid(void);
static int GetVolumeUUID(const char *path, VolumeUUID *volumeUUIDPtr);
static int AdoptAllLocalVolumes(void);
static int AdoptVolume(const char *path);
static int DisownVolume(const char *path);
static int ClearVolumeUUID(const char *path);
static int DisplayVolumeStatus(const char *path);
static int UpdateMountStatus(const char *path, u_int32_t volstatus);


static int isVolumeHFS(const char*path);

int main (int argc, const char *argv[])
{
	int arg;
	char option;
	int result = 0;

	if (argc < 2) {
		fprintf(stderr, usage, argv[0]);
		exit(1);
	};

	for (arg = 1; arg < argc; ++arg) {
		if ((argv[arg][0] == '-') &&
			((option = argv[arg][1]) != (char)0) &&
			(argv[arg][2] == (char)0)) {
			switch (option) {
				case 'a':
				case 'A':
					/* Pick out the pathname argument: */
					if (++arg >= argc) {
						fprintf(stderr, usage, argv[0]);
						exit(1);
					}
					
					check_uid();
					result = AdoptVolume(argv[arg]);
					break;

				case 'c':
				case 'C':
					/* Pick out the pathname argument: */
					if (++arg >= argc) {
						fprintf(stderr, usage, argv[0]);
						exit(1);
					};

					result = DisplayVolumeStatus(argv[arg]);
					break;
				
				case 'd':
				case 'D':
					/* Pick out the pathname argument: */
					if (++arg >= argc) {
						fprintf(stderr, usage, argv[0]);
						exit(1);
					};

					check_uid();
					result = DisownVolume(argv[arg]);
					break;
				
				case 'h':
				case 'H':
					printf(usage, argv[0]);
					printf("where\n");
					printf("\t-a adopts (activates) on-disk permissions on the specified path,\n");
					printf("\t-c checks the status of the permissions usage on the specified path\n");
					printf("\t-d disowns (deactivates) the on-disk permissions on the specified path\n");
					printf("\t-i initializes the permissions database to include all mounted HFS/HFS+ volumes\n");
					break;
				
				case 'i':
				case 'I':
					check_uid();
					result = AdoptAllLocalVolumes();
					break;
				
				case 'x':
				case 'X':
					/* Pick out the pathname argument: */
					if (++arg >= argc) {
						fprintf(stderr, usage, argv[0]);
						exit(1);
					};

					check_uid();
					result = ClearVolumeUUID(argv[arg]);
					break;
					
				default:
					fprintf(stderr, usage, argv[0]);
					exit(1);
			}
		}
	}

	if (result < 0) result = 1;		// Make sure only positive exit status codes are generated
	
	exit(result);	   				// ensure the process exit status is returned
	return result;      			// ...and make main fit the ANSI spec.
}



static void check_uid(void) {
	if (geteuid() != 0) {
		fprintf(stderr, "###\n");
		fprintf(stderr, "### You must be root to perform this operation.\n");
		fprintf(stderr, "###\n");
		exit(1);
	};
}



/*
 --	UpdateMountStatus
 --
 --	Returns: error code (0 if successful).
 */
static int
UpdateMountStatus(const char *path, u_int32_t volstatus) {
	struct statfs mntstat;
	int result;
	union wait status;
	int pid;

	/* 
	 * selectors to determine whether or not certain features 
	 * should be re-enabled via mount -u 
	 */
#ifndef MAXMOUNTLEN
#define MAXMOUNTLEN 255
#endif

	char mountline[MAXMOUNTLEN];
	char mountstring[MAXMOUNTLEN];

	mountopt_t* opt = NULL;
    uint64_t flags;
    uint64_t flags_mask = (MNT_NOSUID | MNT_NODEV |
                           MNT_NOEXEC | MNT_RDONLY |
                           MNT_CPROTECT | MNT_QUARANTINE |
                           MNT_UNION | MNT_DONTBROWSE);

	result = statfs(path, &mntstat);
	if (result != 0) {
		warn("couldn't look up mount status for '%s'", path);
		return errno;
	}

	bzero (mountline, MAXMOUNTLEN);
	bzero (mountstring, MAXMOUNTLEN);

	/* first, check for permissions */
	if (volstatus & VOLUME_USEPERMISSIONS) {
		strlcpy(mountline, "perm", MAXMOUNTLEN);
	}
	else  {
		strlcpy(mountline, "noperm", MAXMOUNTLEN);
	}

	/* check the flags */
	flags = (mntstat.f_flags & flags_mask);

	/* 
	 * now iterate over all of the strings in the optname array and
	 * add them into the "mount" string if the flag they represent is set.
	 * The optnames array is extern'd (at the top of this file), and is defined
	 * in a .c file within the mount_flags directory  
	 */
	for (opt = optnames; flags && opt->o_opt; opt++) {
		if (flags & opt->o_opt) {
			snprintf(mountstring, sizeof(mountstring), ",%s", opt->o_name);
			result = strlcat(mountline, mountstring, MAXMOUNTLEN);
			if (result >= MAXMOUNTLEN) {
				// bail out, string is too long. 
				return EINVAL;
			}
			flags &= ~opt->o_opt;
			bzero (mountstring, MAXMOUNTLEN);
		}
	}

#ifdef MAXMOUNTLEN
#undef MAXMOUNTLEN
#endif

	pid = fork();
	if (pid == 0) {
		result = execl("/sbin/mount", "mount",
						"-t", mntstat.f_fstypename,
						"-u","-o", mountline,
						mntstat.f_mntfromname, mntstat.f_mntonname, NULL);
		/* IF WE ARE HERE, WE WERE UNSUCCESFULL */
		return (1);
	}

	if (pid == -1) {
		warn("couldn't fork to execute mount command");
		return errno;
	};

	/* Success! */
	if ((wait4(pid, (int *)&status, 0, NULL) == pid) && (WIFEXITED(status))) {
		result = status.w_retcode;
	} else {
		result = -1;
	};

	return result;
}



/*
 --	AdoptAllLocalVolumes
 --
 --	Returns: error code (0 if successful).
 */
static int
AdoptAllLocalVolumes(void) {
	struct statfs *mntstatptr;
	int fscount;
	
	fscount = getmntinfo(&mntstatptr, MNT_WAIT);
	if (fscount == 0) {
		warn("couldn't get information on mounted volumes");
		return errno;
	};
	
	while (fscount > 0) {
		if ((strncmp(mntstatptr->f_fstypename, gHFSTypeName, MFSNAMELEN) == 0) ||
            (strncmp(mntstatptr->f_fstypename, gAPFSTypeName, MFSNAMELEN) == 0)) {
			(void)AdoptVolume(mntstatptr->f_mntonname);
		};
		
		++mntstatptr;
		--fscount;
	};
	
	return 0;
}



/*
 --	AdoptVolume
 --
 --	Returns: error code (0 if successful).
 */
static int
AdoptVolume(const char *path) {
	VolumeUUID targetuuid;
	VolumeStatusDBHandle vsdb;
	u_int32_t volstatus;
	int result = 0;

	/* Look up the target volume UUID: */
	result = GetVolumeUUID(path, &targetuuid);
	if (result != 0) {
		warnx("no valid volume UUID found on '%s': %s", path, strerror(result));
		return result;
	};
	
	if (uuid_is_null(targetuuid.uuid)) {
		warnx("internal error: incomplete UUID generated.");
		return EINVAL;
	};
	
	/* Open the volume status DB to look up the entry for the volume in question: */
	if ((result = OpenVolumeStatusDB(&vsdb)) != 0) {
		warnx("couldn't access volume status database: %s", strerror(result));
		return result;
	};
	
	/* Check to see if an entry exists.  If not, prepare a default initial status value: */
	if ((result = GetVolumeStatusDBEntry(vsdb, &targetuuid, &volstatus)) != 0) {
		volstatus = 0;
	};
	
	/* Enable permissions on the specified volume: */
	volstatus = (volstatus & VOLUME_VALIDSTATUSBITS) | VOLUME_USEPERMISSIONS;

	/* Update the entry in the volume status database: */
	if ((result = SetVolumeStatusDBEntry(vsdb, &targetuuid, volstatus)) != 0) {
		warnx("couldn't update volume status database: %s", strerror(result));
		return result;
	};
	
	(void)CloseVolumeStatusDB(vsdb);

	if ((result = UpdateMountStatus(path, volstatus)) != 0) {
		warnx("couldn't update mount status of '%s': %s", path, strerror(result));
		return result;
	};
	
	return 0;
}



/*
 --	DisownVolume
 --
 --	Returns: error code (0 if successful).
 */
static int
DisownVolume(const char *path) {
	VolumeUUID targetuuid;
	VolumeStatusDBHandle vsdb;
	u_int32_t volstatus;
	int result = 0;

	/* Look up the target volume UUID: */
	result = GetVolumeUUID(path, &targetuuid);
	if (result != 0) {
		warnx("no valid volume UUID found on '%s': %s", path, strerror(result));
		return result;
	};
	
	volstatus = 0;
	if (!uuid_is_null(targetuuid.uuid)) {
		/* Open the volume status DB to look up the entry for the volume in question: */
		if ((result = OpenVolumeStatusDB(&vsdb)) != 0) {
			warnx("couldn't access volume status database: %s", strerror(result));
			return result;
		};
		
		/* Check to see if an entry exists.  If not, prepare a default initial status value: */
		if ((result = GetVolumeStatusDBEntry(vsdb, &targetuuid, &volstatus)) != 0) {
			volstatus = 0;
		};
		
		/* Disable permissions on the specified volume: */
		volstatus = (volstatus & VOLUME_VALIDSTATUSBITS) & ~VOLUME_USEPERMISSIONS;

		/* Update the entry in the volume status database: */
		if ((result = SetVolumeStatusDBEntry(vsdb, &targetuuid, volstatus)) != 0) {
			warnx("couldn't update volume status database: %s", strerror(result));
			return result;
		};
		
		(void)CloseVolumeStatusDB(vsdb);

	};
	
	if ((result = UpdateMountStatus(path, volstatus)) != 0) {
		warnx("couldn't update mount status of '%s': %s", path, strerror(result));
		return result;
	};
	
	return result;
};



/*
 --	ClearVolumeUUID
 --
 --	Returns: error code (0 if successful).
 */
static int
ClearVolumeUUID(const char *path) {
	VolumeUUID targetuuid;
	VolumeStatusDBHandle vsdb;
	u_int32_t volstatus;
	int result = 0;

	/* Check to see whether the target volume has an assigned UUID: */
	result = GetVolumeUUID(path, &targetuuid);
	if (result != 0) {
		warnx("couldn't read volume UUID on '%s': %s", path, strerror(result));
		return result;
	};
	
	if (uuid_is_null(targetuuid.uuid) == 0) {
		/* Open the volume status DB to look up the entry for the volume in question: */
		if ((result = OpenVolumeStatusDB(&vsdb)) != 0) {
			warnx("couldn't access volume status database: %s", strerror(result));
			return result;
		};
		
		/* Check to see if an entry exists: */
		if (GetVolumeStatusDBEntry(vsdb, &targetuuid, &volstatus) == 0) {
			/* Remove the entry from the volume status database: */
			if ((result = DeleteVolumeStatusDBEntry(vsdb, &targetuuid)) != 0) {
				warnx("couldn't update volume status database: %s", strerror(result));
				return result;
			};
		};
		
		(void)CloseVolumeStatusDB(vsdb);

		if ((result = UpdateMountStatus(path, 0)) != 0) {
			warnx("couldn't update mount status of '%s': %s", path, strerror(result));
			return result;
		};
	
	};

	return result;
};



/*
 --	DisplayVolumeStatus
 --
 --	Returns: error code (0 if successful).
 */
static int
DisplayVolumeStatus(const char *path) {
	VolumeUUID targetuuid;
	VolumeStatusDBHandle vsdb;
	u_int32_t volstatus;
	int result = 0;

	/* Look up the target volume UUID, exactly as stored on disk: */
	result = GetVolumeUUID(path, &targetuuid);
	if (result != 0) {
		warnx("couldn't read volume UUID on '%s': %s", path, strerror(result));
		return result;
	};
	
	if (uuid_is_null(targetuuid.uuid)) {
		warnx("no valid volume UUID found on '%s': permissions are disabled.", path);
		return 0;
	};
	
	/* Open the volume status DB to look up the entry for the volume in question: */
	if ((result = OpenVolumeStatusDB(&vsdb)) != 0) {
		warnx("couldn't access volume status database: %s", strerror(result));
		return result;
	};
	
	if ((result = GetVolumeStatusDBEntry(vsdb, &targetuuid, &volstatus)) != 0) {
		printf("No entry found for '%s'.\n", path);
		goto Std_Exit;
	};
	
	if (volstatus & VOLUME_USEPERMISSIONS) {
		printf("Permissions on '%s' are enabled.\n", path);
	} else {
		printf("Permissions on '%s' are disabled.\n", path);
	};

Std_Exit:
	(void)CloseVolumeStatusDB(vsdb);
	
	return result;
}

static int isVolumeHFS (const char* path) {
	
	/* default to no */
	int result = 0;
	int isHFS = 0;

	struct statfs statfs_buf;

	result = statfs (path, &statfs_buf);
	if (result == 0) {
		if (!strncmp(statfs_buf.f_fstypename, gHFSTypeName, 3)) {
			isHFS = 1;
		}
	}
	
	return isHFS;
}



//struct definition for calling getattrlist for finderinfos 
typedef struct FinderInfoBuf {
	uint32_t info_length;
	uint32_t finderinfo[8];
} FinderInfoBuf_t;

typedef struct hfsUUID {
	uint32_t high;
	uint32_t low;
} hfsUUID_t;

/*
 --	GetVolumeUUID
 --
 --	Returns: error code (0 if successful).
 */

static int
GetVolumeUUID(const char *path, VolumeUUID *volumeUUIDPtr) {
	struct attrlist alist;
	struct { uint32_t size; uuid_t uuid; } volUUID;
	
	FinderInfoBuf_t finfo;

	int result;

	/*
	 * For a bit more definition on why we have to do this, check out
	 * hfs.util source.  The gist is that IFF the volume is HFS, then
	 * we must check the finderinfo UUID FIRST before querying the 
	 * fs for its full UUID via the getattrlist volume call.
	 */

	if (isVolumeHFS(path)) {
		/* then go get the finderinfo, first... */
		memset (&alist, 0, sizeof(alist));
		alist.bitmapcount = ATTR_BIT_MAP_COUNT;
		alist.reserved = 0;
		alist.commonattr = ATTR_CMN_FNDRINFO;
		alist.volattr = ATTR_VOL_INFO;
		alist.dirattr = 0;
		alist.fileattr = 0;
		alist.forkattr = 0;

		result = getattrlist (path, &alist, &finfo, sizeof(finfo), 0);
		if (result) {
			warn ("failed to getattrlist finderinfo for %s", path);
			result = errno;
			goto Err_Exit;
		}

		hfsUUID_t* hfs_finfo_uuid = (hfsUUID_t*)(&finfo.finderinfo[6]);

		/*
		 * Note: this is a bit of HFS-specific chicanery.  When HFS+ generates
		 * the volume UUID, the formula it uses to generate the 8 bytes of internal
		 * UUID is re-looped/restarted if either high or low is zero.  Thus, if we
		 * see either word as '0' then that means we should treat it as an uninitialized
		 * UUID. 
		 *
		 * As a result, if we see either word as zero, then clear out the caller's buffer
		 * and return the NULL UUID. Otherwise, we'd get the 8 bytes which potentially include 
		 * one or more zeroes run through HFS+'s MD5 algorithm which is not what we want.
		 */
		//technically should endian-swap this but not necessary here
		if ((hfs_finfo_uuid->high == 0) || (hfs_finfo_uuid->low == 0)) {
			uuid_clear (volumeUUIDPtr->uuid);
			return 0;
		}
	}


	/* Set up the attrlist structure to get the volume's UUID: */
	alist.bitmapcount = ATTR_BIT_MAP_COUNT;
	alist.reserved = 0;
	alist.commonattr = 0;
	alist.volattr = (ATTR_VOL_INFO | ATTR_VOL_UUID);
	alist.dirattr = 0;
	alist.fileattr = 0;
	alist.forkattr = 0;

	result = getattrlist(path, &alist, &volUUID, sizeof(volUUID), 0);
	if (result) {
		warn("Couldn't get volume information for '%s'", path);
		result = errno;
		goto Err_Exit;
	}

	uuid_copy(volumeUUIDPtr->uuid, volUUID.uuid);
	result = 0;

Err_Exit:
	return result;
};





/******************************************************************************
 *
 *  V O L U M E   S T A T U S   D A T A B A S E   R O U T I N E S
 *
 *****************************************************************************/

#define DBHANDLESIGNATURE 0x75917737

/* Flag values for operation options: */
#define DBMARKPOSITION 1

static char gVSDBPath[] = "/var/db/volinfo.database";

#define MAXIOMALLOC 16384

/* Database layout: */

struct VSDBKey {
	char uuidString[36];
};

struct VSDBRecord {
	char statusFlags[8];
};

struct VSDBEntry {
	struct VSDBKey key;
	char keySeparator;
	char space;
	struct VSDBRecord record;
	char terminator;
};

struct VSDBKey64 {
	char uuid[16];
};

struct VSDBEntry64 {
	struct VSDBKey64 key;
	char keySeparator;
	char space;
	struct VSDBRecord record;
	char terminator;
};

#define DBKEYSEPARATOR ':'
#define DBBLANKSPACE ' '
#define DBRECORDTERMINATOR '\n'

/* In-memory data structures: */

struct VSDBState {
	u_int32_t signature;
	int dbfile;
	int dbmode;
	off_t recordPosition;
};

typedef struct VSDBState *VSDBStatePtr;



/* Internal function prototypes: */
static int LockDB(VSDBStatePtr dbstateptr, int lockmode);
static int UnlockDB(VSDBStatePtr dbstateptr);

static int FindVolumeRecordByUUID(VSDBStatePtr dbstateptr, VolumeUUID *volumeID, struct VSDBEntry *dbentry, u_int32_t options);
static int AddVolumeRecord(VSDBStatePtr dbstateptr, struct VSDBEntry *dbentry);
static int UpdateVolumeRecord(VSDBStatePtr dbstateptr, struct VSDBEntry *dbentry);
static int GetVSDBEntry(VSDBStatePtr dbstateptr, struct VSDBEntry *dbentry);
static int CompareVSDBKeys(struct VSDBKey *key1, struct VSDBKey *key2);

static void FormatULong(u_int32_t u, char *s);
static void FormatDBKey(VolumeUUID *volumeID, struct VSDBKey *dbkey);
static void FormatDBRecord(u_int32_t volumeStatusFlags, struct VSDBRecord *dbrecord);
static void FormatDBEntry(VolumeUUID *volumeID, u_int32_t volumeStatusFlags, struct VSDBEntry *dbentry);
static u_int32_t ConvertHexStringToULong(const char *hs, long maxdigits);



/******************************************************************************
 *
 *  P U B L I S H E D   I N T E R F A C E   R O U T I N E S
 *
 *****************************************************************************/

void ConvertVolumeUUIDString64ToUUID(const char *UUIDString64, VolumeUUID *volumeID) {
	int i;
	char c;
	u_int32_t nextdigit;
	u_int32_t high = 0;
	u_int32_t low = 0;
	u_int32_t carry;
	MD5_CTX ctx;
	
	for (i = 0; (i < VOLUMEUUID64LENGTH) && ((c = UUIDString64[i]) != (char)0) ; ++i) {
		if ((c >= '0') && (c <= '9')) {
			nextdigit = c - '0';
		} else if ((c >= 'A') && (c <= 'F')) {
			nextdigit = c - 'A' + 10;
		} else if ((c >= 'a') && (c <= 'f')) {
			nextdigit = c - 'a' + 10;
		} else {
			nextdigit = 0;
		};
		carry = ((low & 0xF0000000) >> 28) & 0x0000000F;
		high = (high << 4) | carry;
		low = (low << 4) | nextdigit;
	};
	
	high = OSSwapHostToBigInt32(high);
	low = OSSwapHostToBigInt32(low);

	MD5_Init(&ctx);
	MD5_Update(&ctx, kFSUUIDNamespaceSHA1, sizeof(uuid_t));
	MD5_Update(&ctx, &high, sizeof(high));
	MD5_Update(&ctx, &low, sizeof(low));
	MD5_Final(volumeID->uuid, &ctx);

	volumeID->uuid[6] = (volumeID->uuid[6] & 0x0F) | 0x30;
	volumeID->uuid[8] = (volumeID->uuid[8] & 0x3F) | 0x80;
}



int OpenVolumeStatusDB(VolumeStatusDBHandle *DBHandlePtr) {
	VSDBStatePtr dbstateptr;
	
	*DBHandlePtr = NULL;

	dbstateptr = (VSDBStatePtr)malloc(sizeof(*dbstateptr));
	if (dbstateptr == NULL) {
		return ENOMEM;
	};
	
	dbstateptr->dbmode = O_RDWR;
	dbstateptr->dbfile = open(gVSDBPath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (dbstateptr->dbfile == -1) {
		/*
		   The file couldn't be opened for read/write access:
		   try read-only access before giving up altogether.
		 */
		dbstateptr->dbmode = O_RDONLY;
		dbstateptr->dbfile = open(gVSDBPath, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);
		if (dbstateptr->dbfile == -1) {
			return errno;
		};
	};
	
	dbstateptr->signature = DBHANDLESIGNATURE;
	*DBHandlePtr = (VolumeStatusDBHandle)dbstateptr;
	ConvertVolumeStatusDB(*DBHandlePtr);
	return 0;
}



int ConvertVolumeStatusDB(VolumeStatusDBHandle DBHandle) {
	VSDBStatePtr dbstateptr = (VSDBStatePtr)DBHandle;
	struct VSDBEntry64 entry64;
	struct stat dbinfo;
	int result;
	u_int32_t iobuffersize;
	void *iobuffer = NULL;
	int i;

	if (dbstateptr->signature != DBHANDLESIGNATURE) return EINVAL;

	if ((result = LockDB(dbstateptr, LOCK_EX)) != 0) return result;

	lseek(dbstateptr->dbfile, 0, SEEK_SET);
	result = read(dbstateptr->dbfile, &entry64, sizeof(entry64));
	if ((result != sizeof(entry64)) ||
		(entry64.keySeparator != DBKEYSEPARATOR) ||
		(entry64.space != DBBLANKSPACE) ||
		(entry64.terminator != DBRECORDTERMINATOR)) {
		result = 0;
		goto ErrExit;
	} else {
		if ((result = stat(gVSDBPath, &dbinfo)) != 0) goto ErrExit;
		iobuffersize = dbinfo.st_size;
		iobuffer = malloc(iobuffersize);
		if (iobuffer == NULL) {
			result = ENOMEM;
			goto ErrExit;
		};

		lseek(dbstateptr->dbfile, 0, SEEK_SET);
		result = read(dbstateptr->dbfile, iobuffer, iobuffersize);
		if (result != iobuffersize) {
			result = errno;
			goto ErrExit;
		};
		if ((result = ftruncate(dbstateptr->dbfile, 0)) != 0) {
			goto ErrExit;
		};

		for (i = 0; i < iobuffersize / sizeof(entry64); i++) {
			VolumeUUID volumeID;
			u_int32_t VolumeStatus;
			struct VSDBEntry dbentry;

			entry64 = *(((struct VSDBEntry64 *)iobuffer) + i);
			if ((entry64.keySeparator != DBKEYSEPARATOR) ||
				(entry64.space != DBBLANKSPACE) ||
				(entry64.terminator != DBRECORDTERMINATOR)) {
				continue;
			}

			ConvertVolumeUUIDString64ToUUID(entry64.key.uuid, &volumeID);
			VolumeStatus = ConvertHexStringToULong(entry64.record.statusFlags, sizeof(entry64.record.statusFlags));

			FormatDBEntry(&volumeID, VolumeStatus, &dbentry);
			if ((result = AddVolumeRecord(dbstateptr, &dbentry)) != sizeof(dbentry)) {
				warnx("couldn't convert volume status database: %s", strerror(result));
				goto ErrExit;
			};
		};

		fsync(dbstateptr->dbfile);
		
		result = 0;
	};

ErrExit:
	if (iobuffer) free(iobuffer);
	UnlockDB(dbstateptr);
	return result;
}



int GetVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID, u_int32_t *VolumeStatus) {
	VSDBStatePtr dbstateptr = (VSDBStatePtr)DBHandle;
	struct VSDBEntry dbentry;
	int result;

	if (dbstateptr->signature != DBHANDLESIGNATURE) return EINVAL;

	if ((result = LockDB(dbstateptr, LOCK_SH)) != 0) return result;
	
	if ((result = FindVolumeRecordByUUID(dbstateptr, volumeID, &dbentry, 0)) != 0) {
		goto ErrExit;
	};
	*VolumeStatus = ConvertHexStringToULong(dbentry.record.statusFlags, sizeof(dbentry.record.statusFlags));
	
	result = 0;

ErrExit:
	UnlockDB(dbstateptr);
	return result;
}



int SetVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID, u_int32_t VolumeStatus) {
	VSDBStatePtr dbstateptr = (VSDBStatePtr)DBHandle;
	struct VSDBEntry dbentry;
	int result;
	
	if (dbstateptr->signature != DBHANDLESIGNATURE) return EINVAL;
	if (VolumeStatus & ~VOLUME_VALIDSTATUSBITS) return EINVAL;
	
	if ((result = LockDB(dbstateptr, LOCK_EX)) != 0) return result;
	
	FormatDBEntry(volumeID, VolumeStatus, &dbentry);
	if ((result = FindVolumeRecordByUUID(dbstateptr, volumeID, NULL, DBMARKPOSITION)) == 0) {
		result = UpdateVolumeRecord(dbstateptr, &dbentry);
	} else if (result == -1) {
		result = AddVolumeRecord(dbstateptr, &dbentry);
	} else {
		goto ErrExit;
	};
	
	fsync(dbstateptr->dbfile);
	
	result = 0;

ErrExit:	
	UnlockDB(dbstateptr);
	return result;
}



int DeleteVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID) {
	VSDBStatePtr dbstateptr = (VSDBStatePtr)DBHandle;
	struct stat dbinfo;
	int result;
	u_int32_t iobuffersize;
	void *iobuffer = NULL;
	off_t dataoffset;
	u_int32_t iotransfersize;
	u_int32_t bytestransferred;

	if (dbstateptr->signature != DBHANDLESIGNATURE) return EINVAL;

	if ((result = LockDB(dbstateptr, LOCK_EX)) != 0) return result;
	
	if ((result = FindVolumeRecordByUUID(dbstateptr, volumeID, NULL, DBMARKPOSITION)) != 0) {
		if (result == -1) result = 0;	/* Entry wasn't in the database to begin with? */
		goto StdEdit;
	} else {
		if ((result = stat(gVSDBPath, &dbinfo)) != 0) goto ErrExit;
		if ((dbinfo.st_size - dbstateptr->recordPosition - sizeof(struct VSDBEntry)) <= MAXIOMALLOC) {
			iobuffersize = dbinfo.st_size - dbstateptr->recordPosition - sizeof(struct VSDBEntry);
		} else {
			iobuffersize = MAXIOMALLOC;
		};
		if (iobuffersize > 0) {
			iobuffer = malloc(iobuffersize);
			if (iobuffer == NULL) {
				result = ENOMEM;
				goto ErrExit;
			};
			
			dataoffset = dbstateptr->recordPosition + sizeof(struct VSDBEntry);
			do {
				iotransfersize = dbinfo.st_size - dataoffset;
				if (iotransfersize > 0) {
					if (iotransfersize > iobuffersize) iotransfersize = iobuffersize;
	
					lseek(dbstateptr->dbfile, dataoffset, SEEK_SET);
					bytestransferred = read(dbstateptr->dbfile, iobuffer, iotransfersize);
					if (bytestransferred != iotransfersize) {
						result = errno;
						goto ErrExit;
					};
	
					lseek(dbstateptr->dbfile, dataoffset - (off_t)sizeof(struct VSDBEntry), SEEK_SET);
					bytestransferred = write(dbstateptr->dbfile, iobuffer, iotransfersize);
					if (bytestransferred != iotransfersize) {
						result = errno;
						goto ErrExit;
					};
					
					dataoffset += (off_t)iotransfersize;
				};
			} while (iotransfersize > 0);
		};
		if ((result = ftruncate(dbstateptr->dbfile, dbinfo.st_size - (off_t)(sizeof(struct VSDBEntry)))) != 0) {
			goto ErrExit;
		};
		
		fsync(dbstateptr->dbfile);
		
		result = 0;
	};

ErrExit:
	if (iobuffer) free(iobuffer);
	UnlockDB(dbstateptr);
	
StdEdit:
	return result;
}



int CloseVolumeStatusDB(VolumeStatusDBHandle DBHandle) {
	VSDBStatePtr dbstateptr = (VSDBStatePtr)DBHandle;

	if (dbstateptr->signature != DBHANDLESIGNATURE) return EINVAL;

	dbstateptr->signature = 0;
	
	close(dbstateptr->dbfile);		/* Nothing we can do about any errors... */
	dbstateptr->dbfile = 0;
	
	free(dbstateptr);
	
	return 0;
}



/******************************************************************************
 *
 *  I N T E R N A L   O N L Y   D A T A B A S E   R O U T I N E S
 *
 *****************************************************************************/

static int LockDB(VSDBStatePtr dbstateptr, int lockmode) {
	return flock(dbstateptr->dbfile, lockmode);
}

	

static int UnlockDB(VSDBStatePtr dbstateptr) {
	return flock(dbstateptr->dbfile, LOCK_UN);
}



static int FindVolumeRecordByUUID(VSDBStatePtr dbstateptr, VolumeUUID *volumeID, struct VSDBEntry *targetEntry, u_int32_t options) {
	struct VSDBKey searchkey;
	struct VSDBEntry dbentry;
	int result;
	
	FormatDBKey(volumeID, &searchkey);
	lseek(dbstateptr->dbfile, 0, SEEK_SET);
	
	do {
		result = GetVSDBEntry(dbstateptr, &dbentry);
		if ((result == 0) && (CompareVSDBKeys(&dbentry.key, &searchkey) == 0)) {
			if (targetEntry != NULL) {
				memcpy(targetEntry, &dbentry, sizeof(*targetEntry));
			};
			return 0;
		};
	} while (result == 0);
	
	return -1;
}



static int AddVolumeRecord(VSDBStatePtr dbstateptr , struct VSDBEntry *dbentry) {
	lseek(dbstateptr->dbfile, 0, SEEK_END);
	return write(dbstateptr->dbfile, dbentry, sizeof(struct VSDBEntry));
}




static int UpdateVolumeRecord(VSDBStatePtr dbstateptr, struct VSDBEntry *dbentry) {
	lseek(dbstateptr->dbfile, dbstateptr->recordPosition, SEEK_SET);
	return write(dbstateptr->dbfile, dbentry, sizeof(*dbentry));
}



static int GetVSDBEntry(VSDBStatePtr dbstateptr, struct VSDBEntry *dbentry) {
	struct VSDBEntry entry;
	int result;

	dbstateptr->recordPosition = lseek(dbstateptr->dbfile, 0, SEEK_CUR);
	result = read(dbstateptr->dbfile, &entry, sizeof(entry));
	if ((result != sizeof(entry)) ||
		(entry.keySeparator != DBKEYSEPARATOR) ||
		(entry.space != DBBLANKSPACE) ||
		(entry.terminator != DBRECORDTERMINATOR)) {
		return -1;
	};
	
	memcpy(dbentry, &entry, sizeof(*dbentry));
	return 0;
};



static int CompareVSDBKeys(struct VSDBKey *key1, struct VSDBKey *key2) {
	return memcmp(key1->uuidString, key2->uuidString, sizeof(key1->uuidString));
}



/******************************************************************************
 *
 *  F O R M A T T I N G   A N D   C O N V E R S I O N   R O U T I N E S
 *
 *****************************************************************************/

static void FormatULong(u_int32_t u, char *s) {
	u_int32_t d;
	int i;
	char *digitptr = s;

	for (i = 0; i < 8; ++i) {
		d = ((u & 0xF0000000) >> 28) & 0x0000000F;
		if (d < 10) {
			*digitptr++ = (char)(d + '0');
		} else {
			*digitptr++ = (char)(d - 10 + 'A');
		};
		u = u << 4;
	};
}



static void FormatDBKey(VolumeUUID *volumeID, struct VSDBKey *dbkey) {
	uuid_string_t uuid_str;

	uuid_unparse(volumeID->uuid, uuid_str);
	memcpy(dbkey->uuidString, uuid_str, sizeof(dbkey->uuidString));
}



static void FormatDBRecord(u_int32_t volumeStatusFlags, struct VSDBRecord *dbrecord) {
	FormatULong(volumeStatusFlags, dbrecord->statusFlags);
}



static void FormatDBEntry(VolumeUUID *volumeID, u_int32_t volumeStatusFlags, struct VSDBEntry *dbentry) {
	FormatDBKey(volumeID, &dbentry->key);
	dbentry->keySeparator = DBKEYSEPARATOR;
	dbentry->space = DBBLANKSPACE;
	FormatDBRecord(volumeStatusFlags, &dbentry->record);
	dbentry->terminator = DBRECORDTERMINATOR;
}



static u_int32_t ConvertHexStringToULong(const char *hs, long maxdigits) {
	int i;
	char c;
	u_int32_t nextdigit;
	u_int32_t n;
	
	n = 0;
	for (i = 0; (i < 8) && ((c = hs[i]) != (char)0) ; ++i) {
		if ((c >= '0') && (c <= '9')) {
			nextdigit = c - '0';
		} else if ((c >= 'A') && (c <= 'F')) {
			nextdigit = c - 'A' + 10;
		} else if ((c >= 'a') && (c <= 'f')) {
			nextdigit = c - 'a' + 10;
		} else {
			nextdigit = 0;
		};
		n = (n << 4) + nextdigit;
	};
	
	return n;
}
