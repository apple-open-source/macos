/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include <mach/port.h>
#include <mach/kern_return.h>
#include <mach/mach_error.h>


#include "ClientToServer.h"
#include "ServerToClient.h"
#include "DiskArbitrationTypes.h"
#include "DiskArbitrationServerMain.h"
#include "GetRegistry.h"
#include "FSParticular.h"


struct FinderAttrBuf {
	unsigned long info_length;
	unsigned long finderinfo[8];
};

/*****************************************************************************
 *
 * The following should really be in some system header file:
 *
 *****************************************************************************/

#define VOLUMEUUIDVALUESIZE 2
typedef union VolumeUUID {
	unsigned long value[VOLUMEUUIDVALUESIZE];
	struct {
		unsigned long high;
		unsigned long low;
	} v;
} VolumeUUID;

#define VOLUMEUUIDLENGTH 16
typedef char VolumeUUIDString[VOLUMEUUIDLENGTH+1];

#define VOLUME_RECORDED 0x80000000
#define VOLUME_USEPERMISSIONS 0x00000001
#define VOLUME_VALIDSTATUSBITS ( VOLUME_USEPERMISSIONS )

typedef void *VolumeStatusDBHandle;

void GenerateVolumeUUID(VolumeUUID *newVolumeID);
void ConvertVolumeUUIDStringToUUID(const char *UUIDString, VolumeUUID *volumeID);
void ConvertVolumeUUIDToString(VolumeUUID *volumeID, char *UUIDString);

int OpenVolumeStatusDB(VolumeStatusDBHandle *DBHandlePtr);
int GetVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID, unsigned long *VolumeStatus);
int SetVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID, unsigned long VolumeStatus);
int DeleteVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID);
int CloseVolumeStatusDB(VolumeStatusDBHandle DBHandle);

/*****************************************************************************
 *
 * Internal function prototypes:
 *
 *****************************************************************************/

static int GetVolumeUUID(const char *path, VolumeUUID *volumeUUIDPtr, boolean_t generate);
static int AdoptVolume(const char *path);
static int DisownVolume(const char *path);
static int UpdateMountStatus(const char *path, unsigned long volstatus);

/*
 --	UpdateMountStatus
 --
 --	Returns: error code (0 if successful).
 */
static int
UpdateMountStatus(const char *path, unsigned long volstatus) {
	struct statfs mntstat;
	int result;
	union wait status;
	int pid;
        int nosuid;
        int nodev;
        int noexec;
        int readonly;
        char mountline[255];
        
	result = statfs(path, &mntstat);
	if (result != 0) {
		warn("couldn't look up mount status for '%s'", path);
		return errno;
	};

        nosuid = mntstat.f_flags & MNT_NOSUID;
        nodev = mntstat.f_flags & MNT_NODEV;
        noexec = mntstat.f_flags & MNT_NOEXEC;
        readonly = mntstat.f_flags & MNT_RDONLY;

        sprintf(mountline, "%s%s%s%s%s", (volstatus & VOLUME_USEPERMISSIONS) ? "perm" : "noperm",(nosuid)?",nosuid":"",(nodev)?",nodev":"",(noexec)?",noexec":"",(readonly)?",rdonly":"");

	pid = fork();
	if (pid == 0) {
		result = execl("/sbin/mount", "mount",
						"-t", mntstat.f_fstypename,
						"-u","-o", mountline,
						mntstat.f_mntfromname, mntstat.f_mntonname, NULL);
#if DEBUG_TRACE
		if (result) {
			fprintf(stderr, "Error from execl(/sbin/mount...): result = %d.\n", result);
		};
		fprintf(stderr, "UpdateMountStatus in child: returning 1...\n");
#endif
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
 --	AdoptVolume
 --
 --	Returns: error code (0 if successful).
 */
static int
AdoptVolume(const char *path) {
	VolumeUUID targetuuid;
	VolumeStatusDBHandle vsdb;
	unsigned long volstatus;
	int result = 0;

	/* Look up the target volume UUID, generating one if no valid one has been assigned yet: */
	result = GetVolumeUUID(path, &targetuuid, TRUE);
	if (result != 0) {
		warnx("no valid volume UUID found on '%s': %s", path, strerror(result));
		return result;
	};
	
	if ((targetuuid.v.high == 0) || (targetuuid.v.low == 0)) {
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
	
	(void)CloseVolumeStatusDB(CloseVolumeStatusDB);

	sync();

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
int
DisownVolume(const char *path) {
	VolumeUUID targetuuid;
	VolumeStatusDBHandle vsdb;
	unsigned long volstatus;
	int result = 0;

	/* Look up the target volume UUID, generating one if no valid one has been assigned yet: */
	result = GetVolumeUUID(path, &targetuuid, TRUE);
	if (result != 0) {
		warnx("no valid volume UUID found on '%s': %s", path, strerror(result));
		return result;
	};
	
	volstatus = 0;
	if ((targetuuid.v.high != 0) || (targetuuid.v.low != 0)) {
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
		
		(void)CloseVolumeStatusDB(CloseVolumeStatusDB);

		sync();
	};
	
	if ((result = UpdateMountStatus(path, volstatus)) != 0) {
		warnx("couldn't update mount status of '%s': %s", path, strerror(result));
		return result;
	};
	
	return result;
};

/*
 --	GetVolumeUUID
 --
 --	Returns: error code (0 if successful).
 */

static int
GetVolumeUUID(const char *path, VolumeUUID *volumeUUIDPtr, boolean_t generate) {
	struct attrlist alist;
	struct FinderAttrBuf volFinderInfo;
	VolumeUUID *finderInfoUUIDPtr;
	int result;

	/* Set up the attrlist structure to get the volume's Finder Info: */
	alist.bitmapcount = 5;
	alist.reserved = 0;
	alist.commonattr = ATTR_CMN_FNDRINFO;
	alist.volattr = ATTR_VOL_INFO;
	alist.dirattr = 0;
	alist.fileattr = 0;
	alist.forkattr = 0;

	result = getattrlist(path, &alist, &volFinderInfo, sizeof(volFinderInfo), 0);
	if (result) {
		warn("Couldn't get volume information for '%s'", path);
		result = errno;
		goto Err_Exit;
	}

	finderInfoUUIDPtr = (VolumeUUID *)(&volFinderInfo.finderinfo[6]);
	if (generate && ((finderInfoUUIDPtr->v.high == 0) || (finderInfoUUIDPtr->v.low == 0))) {
		GenerateVolumeUUID(volumeUUIDPtr);
		bcopy(volumeUUIDPtr, finderInfoUUIDPtr, sizeof(*finderInfoUUIDPtr));
		result = setattrlist(path, &alist, &volFinderInfo.finderinfo, sizeof(volFinderInfo.finderinfo), 0);
		if (result) {
			warn("Couldn't update volume information for '%s'", path);
			result = errno;
			goto Err_Exit;
		};
	};

	bcopy(finderInfoUUIDPtr, volumeUUIDPtr, sizeof(*volumeUUIDPtr));
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
	char uuid[16];
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

#define DBKEYSEPARATOR ':'
#define DBBLANKSPACE ' '
#define DBRECORDTERMINATOR '\n'

/* In-memory data structures: */

struct VSDBState {
	unsigned long signature;
	int dbfile;
	int dbmode;
	off_t recordPosition;
};

typedef struct VSDBState *VSDBStatePtr;

typedef struct {
	unsigned long state[5];
	unsigned long count[2];
	unsigned char buffer[64];
} SHA1_CTX;



/* Internal function prototypes: */
static int LockDB(VSDBStatePtr dbstateptr, int lockmode);
static int UnlockDB(VSDBStatePtr dbstateptr);

static int FindVolumeRecordByUUID(VSDBStatePtr dbstateptr, VolumeUUID *volumeID, struct VSDBEntry *dbentry, unsigned long options);
static int AddVolumeRecord(VSDBStatePtr dbstateptr, struct VSDBEntry *dbentry);
static int UpdateVolumeRecord(VSDBStatePtr dbstateptr, struct VSDBEntry *dbentry);
static int GetVSDBEntry(VSDBStatePtr dbstateptr, struct VSDBEntry *dbentry);
static int CompareVSDBKeys(struct VSDBKey *key1, struct VSDBKey *key2);

static void FormatULong(unsigned long u, char *s);
static void FormatUUID(VolumeUUID *volumeID, char *UUIDField);
static void FormatDBKey(VolumeUUID *volumeID, struct VSDBKey *dbkey);
static void FormatDBRecord(unsigned long volumeStatusFlags, struct VSDBRecord *dbrecord);
static void FormatDBEntry(VolumeUUID *volumeID, unsigned long volumeStatusFlags, struct VSDBEntry *dbentry);
static unsigned long ConvertHexStringToULong(const char *hs, long maxdigits);

static void SHA1Transform(unsigned long state[5], unsigned char buffer[64]);
static void SHA1Init(SHA1_CTX* context);
static void SHA1Update(SHA1_CTX* context, void* data, size_t len);
static void SHA1Final(unsigned char digest[20], SHA1_CTX* context);



/******************************************************************************
 *
 *  P U B L I S H E D   I N T E R F A C E   R O U T I N E S
 *
 *****************************************************************************/

void GenerateVolumeUUID(VolumeUUID *newVolumeID) {
	SHA1_CTX context;
	char randomInputBuffer[26];
	unsigned char digest[20];
	time_t now;
	clock_t uptime;
	int mib[2];
	int sysdata;
	char sysctlstring[128];
	size_t datalen;
	struct loadavg sysloadavg;
	struct vmtotal sysvmtotal;
	
	do {
		/* Initialize the SHA-1 context for processing: */
		SHA1Init(&context);
		
		/* Now process successive bits of "random" input to seed the process: */
		
		/* The current system's uptime: */
		uptime = clock();
		SHA1Update(&context, &uptime, sizeof(uptime));
		
		/* The kernel's boot time: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_BOOTTIME;
		datalen = sizeof(sysdata);
		sysctl(mib, 2, &sysdata, &datalen, NULL, 0);
		SHA1Update(&context, &sysdata, datalen);
		
		/* The system's host id: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_HOSTID;
		datalen = sizeof(sysdata);
		sysctl(mib, 2, &sysdata, &datalen, NULL, 0);
		SHA1Update(&context, &sysdata, datalen);

		/* The system's host name: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_HOSTNAME;
		datalen = sizeof(sysctlstring);
		sysctl(mib, 2, sysctlstring, &datalen, NULL, 0);
		SHA1Update(&context, sysctlstring, datalen);

		/* The running kernel's OS release string: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_OSRELEASE;
		datalen = sizeof(sysctlstring);
		sysctl(mib, 2, sysctlstring, &datalen, NULL, 0);
		SHA1Update(&context, sysctlstring, datalen);

		/* The running kernel's version string: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_VERSION;
		datalen = sizeof(sysctlstring);
		sysctl(mib, 2, sysctlstring, &datalen, NULL, 0);
		SHA1Update(&context, sysctlstring, datalen);

		/* The system's load average: */
		mib[0] = CTL_VM;
		mib[1] = VM_LOADAVG;
		datalen = sizeof(sysloadavg);
		sysctl(mib, 2, &sysloadavg, &datalen, NULL, 0);
		SHA1Update(&context, &sysloadavg, datalen);

		/* The system's VM statistics: */
		mib[0] = CTL_VM;
		mib[1] = VM_METER;
		datalen = sizeof(sysvmtotal);
		sysctl(mib, 2, &sysvmtotal, &datalen, NULL, 0);
		SHA1Update(&context, &sysvmtotal, datalen);

		/* The current GMT (26 ASCII characters): */
		time(&now);
		strncpy(randomInputBuffer, asctime(gmtime(&now)), 26);	/* "Mon Mar 27 13:46:26 2000" */
		SHA1Update(&context, randomInputBuffer, 26);
		
		/* Pad the accumulated input and extract the final digest hash: */
		SHA1Final(digest, &context);
	
		memcpy(newVolumeID, digest, sizeof(*newVolumeID));
	} while ((newVolumeID->v.high == 0) || (newVolumeID->v.low == 0));
}



void ConvertVolumeUUIDStringToUUID(const char *UUIDString, VolumeUUID *volumeID) {
	int i;
	char c;
	unsigned long nextdigit;
	unsigned long high = 0;
	unsigned long low = 0;
	unsigned long carry;
	
	for (i = 0; (i < VOLUMEUUIDLENGTH) && ((c = UUIDString[i]) != (char)0) ; ++i) {
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
	
	volumeID->v.high = high;
	volumeID->v.low = low;
}



void ConvertVolumeUUIDToString(VolumeUUID *volumeID, char *UUIDString) {
	FormatUUID(volumeID, UUIDString);
	*(UUIDString+16) = (char)0;		/* Append a terminating null character */
}



int OpenVolumeStatusDB(VolumeStatusDBHandle *DBHandlePtr) {
	VSDBStatePtr dbstateptr;
	
	*DBHandlePtr = NULL;

	dbstateptr = (VSDBStatePtr)malloc(sizeof(*dbstateptr));
	if (dbstateptr == NULL) {
		return ENOMEM;
	};
	
	dbstateptr->dbmode = O_RDWR;
	dbstateptr->dbfile = open(gVSDBPath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (dbstateptr->dbfile == -1) {
		/*
		   The file couldn't be opened for read/write access:
		   try read-only access before giving up altogether.
		 */
		dbstateptr->dbmode = O_RDONLY;
		dbstateptr->dbfile = open(gVSDBPath, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (dbstateptr->dbfile == -1) {
			return errno;
		};
	};
	
	dbstateptr->signature = DBHANDLESIGNATURE;
	*DBHandlePtr = (VolumeStatusDBHandle)dbstateptr;
	return 0;
}



int GetVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID, unsigned long *VolumeStatus) {
	VSDBStatePtr dbstateptr = (VSDBStatePtr)DBHandle;
	struct VSDBEntry dbentry;
	int result;

	if (dbstateptr->signature != DBHANDLESIGNATURE) return EINVAL;

	if ((result = LockDB(dbstateptr, LOCK_SH)) != 0) return result;
	
	if ((result = FindVolumeRecordByUUID(dbstateptr, volumeID, &dbentry, 0)) != 0) {
		goto ErrExit;
	};
	*VolumeStatus = VOLUME_RECORDED | ConvertHexStringToULong(dbentry.record.statusFlags, sizeof(dbentry.record.statusFlags));
	
	result = 0;

ErrExit:
	UnlockDB(dbstateptr);
	return result;
}



int SetVolumeStatusDBEntry(VolumeStatusDBHandle DBHandle, VolumeUUID *volumeID, unsigned long VolumeStatus) {
	VSDBStatePtr dbstateptr = (VSDBStatePtr)DBHandle;
	struct VSDBEntry dbentry;
	int result;
	
	if (dbstateptr->signature != DBHANDLESIGNATURE) return EINVAL;
	if (VolumeStatus & ~VOLUME_VALIDSTATUSBITS) return EINVAL;
	
	if ((result = LockDB(dbstateptr, LOCK_EX)) != 0) return result;
	
	FormatDBEntry(volumeID, VolumeStatus, &dbentry);
	if ((result = FindVolumeRecordByUUID(dbstateptr, volumeID, NULL, DBMARKPOSITION)) == 0) {
#if DEBUG_TRACE
		fprintf(stderr,"AddLocalVolumeUUID: found record in database; updating in place.\n");
#endif
		result = UpdateVolumeRecord(dbstateptr, &dbentry);
	} else if (result == -1) {
#if DEBUG_TRACE
		fprintf(stderr,"AddLocalVolumeUUID: record not found in database; appending at end.\n");
#endif
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
	unsigned long iobuffersize;
	void *iobuffer = NULL;
	off_t dataoffset;
	unsigned long iotransfersize;
	unsigned long bytestransferred;

	if (dbstateptr->signature != DBHANDLESIGNATURE) return EINVAL;

	if ((result = LockDB(dbstateptr, LOCK_EX)) != 0) return result;
	
	if ((result = FindVolumeRecordByUUID(dbstateptr, volumeID, NULL, DBMARKPOSITION)) != 0) {
#if DEBUG_TRACE
		fprintf(stderr, "DeleteLocalVolumeUUID: No record with matching volume UUID in DB (result = %d).\n", result);
#endif
		if (result == -1) result = 0;	/* Entry wasn't in the database to begin with? */
		goto StdEdit;
	} else {
#if DEBUG_TRACE
		fprintf(stderr, "DeleteLocalVolumeUUID: Found record with matching volume UUID...\n");
#endif
		if ((result = stat(gVSDBPath, &dbinfo)) != 0) goto ErrExit;
		if ((dbinfo.st_size - dbstateptr->recordPosition - sizeof(struct VSDBEntry)) <= MAXIOMALLOC) {
			iobuffersize = dbinfo.st_size - dbstateptr->recordPosition - sizeof(struct VSDBEntry);
		} else {
			iobuffersize = MAXIOMALLOC;
		};
#if DEBUG_TRACE
		fprintf(stderr, "DeleteLocalVolumeUUID: DB size = 0x%08lx; recordPosition = 0x%08lx;\n", 
							(unsigned long)dbinfo.st_size, (unsigned long)dbstateptr->recordPosition);
		fprintf(stderr, "DeleteLocalVolumeUUID: I/O buffer size = 0x%lx\n", iobuffersize);
#endif
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
	
#if DEBUG_TRACE
					fprintf(stderr, "DeleteLocalVolumeUUID: reading 0x%08lx bytes starting at 0x%08lx ...\n", iotransfersize, (unsigned long)dataoffset);
#endif
					lseek(dbstateptr->dbfile, dataoffset, SEEK_SET);
					bytestransferred = read(dbstateptr->dbfile, iobuffer, iotransfersize);
					if (bytestransferred != iotransfersize) {
						result = errno;
						goto ErrExit;
					};
	
#if DEBUG_TRACE
					fprintf(stderr, "DeleteLocalVolumeUUID: writing 0x%08lx bytes starting at 0x%08lx ...\n", iotransfersize, (unsigned long)(dataoffset - (off_t)sizeof(struct VSDBEntry)));
#endif
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
#if DEBUG_TRACE
		fprintf(stderr, "DeleteLocalVolumeUUID: truncating database file to 0x%08lx bytes.\n", (unsigned long)(dbinfo.st_size - (off_t)(sizeof(struct VSDBEntry))));
#endif
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
#if DEBUG_TRACE
	fprintf(stderr, "LockDB: Locking VSDB file...\n");
#endif
	return flock(dbstateptr->dbfile, lockmode);
}

	

static int UnlockDB(VSDBStatePtr dbstateptr) {
#if DEBUG_TRACE
	fprintf(stderr, "UnlockDB: Unlocking VSDB file...\n");
#endif
	return flock(dbstateptr->dbfile, LOCK_UN);
}



static int FindVolumeRecordByUUID(VSDBStatePtr dbstateptr, VolumeUUID *volumeID, struct VSDBEntry *targetEntry, unsigned long options) {
	struct VSDBKey searchkey;
	struct VSDBEntry dbentry;
	int result;
	
	FormatDBKey(volumeID, &searchkey);
	lseek(dbstateptr->dbfile, 0, SEEK_SET);
	
	do {
		result = GetVSDBEntry(dbstateptr, &dbentry);
		if ((result == 0) && (CompareVSDBKeys(&dbentry.key, &searchkey) == 0)) {
			if (targetEntry != NULL) {
#if DEBUG_TRACE
				fprintf(stderr, "FindVolumeRecordByUUID: copying %d. bytes from %08xl to %08l...\n", sizeof(*targetEntry), &dbentry, targetEntry);
#endif
				memcpy(targetEntry, &dbentry, sizeof(*targetEntry));
			};
			return 0;
		};
	} while (result == 0);
	
	return -1;
}



static int AddVolumeRecord(VSDBStatePtr dbstateptr , struct VSDBEntry *dbentry) {
#if DEBUG_TRACE
	VolumeUUIDString id;
#endif

#if DEBUG_TRACE
	strncpy(id, dbentry->key.uuid, sizeof(dbentry->key.uuid));
	id[sizeof(dbentry->key.uuid)] = (char)0;
	fprintf(stderr, "AddVolumeRecord: Adding record for UUID #%s...\n", id);
#endif
	lseek(dbstateptr->dbfile, 0, SEEK_END);
	return write(dbstateptr->dbfile, dbentry, sizeof(struct VSDBEntry));
}




static int UpdateVolumeRecord(VSDBStatePtr dbstateptr, struct VSDBEntry *dbentry) {
#if DEBUG_TRACE
	VolumeUUIDString id;
#endif

#if DEBUG_TRACE
	strncpy(id, dbentry->key.uuid, sizeof(dbentry->key.uuid));
	id[sizeof(dbentry->key.uuid)] = (char)0;
	fprintf(stderr, "UpdateVolumeRecord: Updating record for UUID #%s at offset 0x%08lx in database...\n", id, (unsigned long)dbstateptr->recordPosition);
#endif
	lseek(dbstateptr->dbfile, dbstateptr->recordPosition, SEEK_SET);
#if DEBUG_TRACE
	fprintf(stderr, "UpdateVolumeRecord: Writing %d. bytes...\n", sizeof(*dbentry));
#endif
	return write(dbstateptr->dbfile, dbentry, sizeof(*dbentry));
}



static int GetVSDBEntry(VSDBStatePtr dbstateptr, struct VSDBEntry *dbentry) {
	struct VSDBEntry entry;
	int result;
#if DEBUG_TRACE
	VolumeUUIDString id;
#endif
	
	dbstateptr->recordPosition = lseek(dbstateptr->dbfile, 0, SEEK_CUR);
#if 0 // DEBUG_TRACE
	fprintf(stderr, "GetVSDBEntry: starting reading record at offset 0x%08lx...\n", (unsigned long)dbstateptr->recordPosition);
#endif
	result = read(dbstateptr->dbfile, &entry, sizeof(entry));
	if ((result != sizeof(entry)) ||
		(entry.keySeparator != DBKEYSEPARATOR) ||
		(entry.space != DBBLANKSPACE) ||
		(entry.terminator != DBRECORDTERMINATOR)) {
		return -1;
	};
	
#if DEBUG_TRACE
	strncpy(id, entry.key.uuid, sizeof(entry.key.uuid));
	id[sizeof(entry.key.uuid)] = (char)0;
	fprintf(stderr, "GetVSDBEntry: returning entry for UUID #%s...\n", id);
#endif
	memcpy(dbentry, &entry, sizeof(*dbentry));
	return 0;
};



static int CompareVSDBKeys(struct VSDBKey *key1, struct VSDBKey *key2) {
#if 0 // DEBUG_TRACE
	VolumeUUIDString id;

	strncpy(id, key1->uuid, sizeof(key1->uuid));
	id[sizeof(key1->uuid)] = (char)0;
	fprintf(stderr, "CompareVSDBKeys: comparing #%s to ", id);
	strncpy(id, key2->uuid, sizeof(key2->uuid));
	id[sizeof(key2->uuid)] = (char)0;
	fprintf(stderr, "%s (%d.)...\n", id, sizeof(key1->uuid));
#endif
	
	return memcmp(key1->uuid, key2->uuid, sizeof(key1->uuid));
}



/******************************************************************************
 *
 *  F O R M A T T I N G   A N D   C O N V E R S I O N   R O U T I N E S
 *
 *****************************************************************************/

static void FormatULong(unsigned long u, char *s) {
	unsigned long d;
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



static void FormatUUID(VolumeUUID *volumeID, char *UUIDField) {
	FormatULong(volumeID->v.high, UUIDField);
	FormatULong(volumeID->v.low, UUIDField+8);

};



static void FormatDBKey(VolumeUUID *volumeID, struct VSDBKey *dbkey) {
	FormatUUID(volumeID, dbkey->uuid);
}



static void FormatDBRecord(unsigned long volumeStatusFlags, struct VSDBRecord *dbrecord) {
	FormatULong(volumeStatusFlags, dbrecord->statusFlags);
}



static void FormatDBEntry(VolumeUUID *volumeID, unsigned long volumeStatusFlags, struct VSDBEntry *dbentry) {
	FormatDBKey(volumeID, &dbentry->key);
	dbentry->keySeparator = DBKEYSEPARATOR;
	dbentry->space = DBBLANKSPACE;
	FormatDBRecord(volumeStatusFlags, &dbentry->record);
#if 0 // DEBUG_TRACE
	dbentry->terminator = (char)0;
	fprintf(stderr, "FormatDBEntry: '%s' (%d.)\n", dbentry, sizeof(*dbentry));
#endif
	dbentry->terminator = DBRECORDTERMINATOR;
}



static unsigned long ConvertHexStringToULong(const char *hs, long maxdigits) {
	int i;
	char c;
	unsigned long nextdigit;
	unsigned long n;
	
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



/******************************************************************************
 *
 *  S H A - 1   I M P L E M E N T A T I O N   R O U T I N E S
 *
 *****************************************************************************/

/*
	Derived from SHA-1 in C
	By Steve Reid <steve@edmweb.com>
	100% Public Domain

	Test Vectors (from FIPS PUB 180-1)
		"abc"
			A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
		"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
			84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
		A million repetitions of "a"
			34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
*/

/* #define LITTLE_ENDIAN * This should be #define'd if true. */
/* #define SHA1HANDSOFF * Copies data before messing with it. */

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#ifdef LITTLE_ENDIAN
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
	|(rol(block->l[i],8)&0x00FF00FF))
#else
#define blk0(i) block->l[i]
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
	^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#if TRACE_HASH
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);printf("t = %2d: %08lX %08lX %08lX %08lX %08lX\n", i, a, b, c, d, e);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);printf("t = %2d: %08lX %08lX %08lX %08lX %08lX\n", i, a, b, c, d, e);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);printf("t = %2d: %08lX %08lX %08lX %08lX %08lX\n", i, a, b, c, d, e);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);printf("t = %2d: %08lX %08lX %08lX %08lX %08lX\n", i, a, b, c, d, e);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);printf("t = %2d: %08lX %08lX %08lX %08lX %08lX\n", i, a, b, c, d, e);
#else
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);
#endif


/* Hash a single 512-bit block. This is the core of the algorithm. */

static void SHA1Transform(unsigned long state[5], unsigned char buffer[64])
{
unsigned long a, b, c, d, e;
typedef union {
	unsigned char c[64];
	unsigned long l[16];
} CHAR64LONG16;
CHAR64LONG16* block;
#ifdef SHA1HANDSOFF
static unsigned char workspace[64];
	block = (CHAR64LONG16*)workspace;
	memcpy(block, buffer, 64);
#else
	block = (CHAR64LONG16*)buffer;
#endif
	/* Copy context->state[] to working vars */
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
#if TRACE_HASH
	printf("            A        B        C        D        E\n");
	printf("        -------- -------- -------- -------- --------\n");
	printf("        %08lX %08lX %08lX %08lX %08lX\n", a, b, c, d, e);
#endif
	/* 4 rounds of 20 operations each. Loop unrolled. */
	R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
	R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
	R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
	R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
	R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
	R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
	R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
	R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
	R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
	R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
	R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
	R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
	R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
	R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
	R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
	R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
	R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
	R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
	R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
	R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);
	/* Add the working vars back into context.state[] */
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	/* Wipe variables */
	a = b = c = d = e = 0;
}


/* SHA1Init - Initialize new context */

static void SHA1Init(SHA1_CTX* context)
{
	/* SHA1 initialization constants */
	context->state[0] = 0x67452301;
	context->state[1] = 0xEFCDAB89;
	context->state[2] = 0x98BADCFE;
	context->state[3] = 0x10325476;
	context->state[4] = 0xC3D2E1F0;
	context->count[0] = context->count[1] = 0;
}


/* Run your data through this. */

static void SHA1Update(SHA1_CTX* context, void* data, size_t len)
{
	unsigned char *dataptr = (char *)data;
	unsigned int i, j;

	j = (context->count[0] >> 3) & 63;
	if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
	context->count[1] += (len >> 29);
	if ((j + len) > 63) {
		memcpy(&context->buffer[j], dataptr, (i = 64-j));
		SHA1Transform(context->state, context->buffer);
		for ( ; i + 63 < len; i += 64) {
			SHA1Transform(context->state, &dataptr[i]);
		}
		j = 0;
	}
	else i = 0;
	memcpy(&context->buffer[j], &dataptr[i], len - i);
}


/* Add padding and return the message digest. */

static void SHA1Final(unsigned char digest[20], SHA1_CTX* context)
{
unsigned long i, j;
unsigned char finalcount[8];

	for (i = 0; i < 8; i++) {
		finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]
		 >> ((3-(i & 3)) * 8) ) & 255);  /* Endian independent */
	}
	SHA1Update(context, (unsigned char *)"\200", 1);
	while ((context->count[0] & 504) != 448) {
		SHA1Update(context, (unsigned char *)"\0", 1);
	}
	SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform() */
	for (i = 0; i < 20; i++) {
		digest[i] = (unsigned char)
		 ((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
	}
	/* Wipe variables */
	i = j = 0;
	memset(context->buffer, 0, 64);
	memset(context->state, 0, 20);
	memset(context->count, 0, 8);
	memset(&finalcount, 0, 8);
#ifdef SHA1HANDSOFF  /* make SHA1Transform overwrite it's own static vars */
	SHA1Transform(context->state, context->buffer);
#endif
}

kern_return_t DiskArbVSDBAdoptVolume_rpc(
                                         mach_port_t server,
                                         pid_t clientPid,
                                         DiskArbDiskIdentifier diskIdentifier)
{

    DiskPtr diskPtr = LookupDiskByIOBSDName( diskIdentifier );
    ClientPtr clientPtr = LookupClientByPID(clientPid);

    if ( NULL == diskPtr )
    {
            if (clientPtr) {
                    SendCallFailedMessage(clientPtr, NULL, kDiskArbVSDBAdoptRequestFailed, kDiskArbVolumeDoesNotExist);
            }

            return -1;
    }

    if (!requestingClientHasPermissionToModifyDisk(clientPid, diskPtr, "system.volume.volinfo.adopt")) {
            if (clientPtr) {
                    SendCallFailedMessage(clientPtr, diskPtr, kDiskArbVSDBAdoptRequestFailed, kDiskArbInsecureRequest);
            }
            return -1;
    }
    
    dwarning(("Adopting volume %s\n", diskPtr->mountpoint));

    // get the volume path and "adopt it"
    if ( AdoptVolume(diskPtr->mountpoint) == 0) {
        if (clientPtr) {
            SendCallSucceededMessage(clientPtr, diskPtr, kDiskArbVSDBAdoptRequestSucceeded);
        }

    } else {
        if (clientPtr) {
            SendCallFailedMessage(clientPtr, diskPtr, kDiskArbVSDBAdoptRequestFailed, kDiskArbVSDBCannotAdopt);
        }
    }

    return 0;
}

kern_return_t DiskArbVSDBDisownVolume_rpc(
                                         mach_port_t server,
                                          pid_t clientPid,
                                          DiskArbDiskIdentifier diskIdentifier)
{
    DiskPtr diskPtr = LookupDiskByIOBSDName( diskIdentifier );
    ClientPtr clientPtr = LookupClientByPID(clientPid);

    if (!diskPtr) {
            if (clientPtr) {
                    SendCallFailedMessage(clientPtr, NULL, kDiskArbVSDBDisownRequestFailed, kDiskArbVolumeDoesNotExist);
            }
            return -1;
    }

    if (!requestingClientHasPermissionToModifyDisk(clientPid, diskPtr, "system.volume.volinfo.disown")) {
            if (clientPtr) {
                    SendCallFailedMessage(clientPtr, diskPtr, kDiskArbVSDBDisownRequestFailed, kDiskArbInsecureRequest);
            }
            return -1;
    }

    dwarning(("Disowning volume %s\n", diskPtr->mountpoint));

    // get the volume path and "disown it"

    if ( DisownVolume(diskPtr->mountpoint) == 0) {
        if (clientPtr) {
            SendCallSucceededMessage(clientPtr, diskPtr, kDiskArbVSDBDisownRequestSucceeded);
        }
        
    } else {
        if (clientPtr) {
            SendCallFailedMessage(clientPtr, diskPtr, kDiskArbVSDBDisownRequestFailed, kDiskArbVSDBCannotDisown);
        }
    }

    return 0;
}

kern_return_t DiskArbVSDBGetVolumeStatus_rpc(
                                       mach_port_t server,
                                       DiskArbDiskIdentifier diskIdentifier,
                                       int *status)
{
    VolumeUUID targetuuid;
    VolumeStatusDBHandle vsdb;
    unsigned long volstatus;
    int result = 0;
    DiskPtr diskPtr;

    diskPtr = LookupDiskByIOBSDName( diskIdentifier );

    if (diskPtr && diskPtr->mountpoint && (strcmp("", diskPtr->mountpoint))) {
        dwarning(("returning permission on volume %s\n", diskPtr->mountpoint));
    } else {
        dwarning(("permission on volume %s cannot be determined\n", diskIdentifier));
        *status = (int)kDiskArbVSDBPermissionsNotExist;
        return 0;
    }


    /* Look up the target volume UUID, exactly as stored on disk: */
    result = GetVolumeUUID(diskPtr->mountpoint, &targetuuid, FALSE);
    if (result != 0) {
        *status = (int)kDiskArbVSDBPermissionsNotExist;
        return result;
    };

    if ((targetuuid.v.high == 0) || (targetuuid.v.low == 0)) {
        *status = (int)kDiskArbVSDBPermissionsNotExist;
        return 0;
    };

    /* Open the volume status DB to look up the entry for the volume in question: */
    if ((result = OpenVolumeStatusDB(&vsdb)) != 0) {
            return result;
    };

    if ((result = GetVolumeStatusDBEntry(vsdb, &targetuuid, &volstatus)) != 0) {
            goto Std_Exit;
    };

    if (volstatus & VOLUME_USEPERMISSIONS) {
        *status = (int)kDiskArbVSDBPermissionsEnabled;
    } else {
        *status = (int)kDiskArbVSDBPermissionsDisabled;
    };

Std_Exit:
    (void)CloseVolumeStatusDB(CloseVolumeStatusDB);

    return result;
}

