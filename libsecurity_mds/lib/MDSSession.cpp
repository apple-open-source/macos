/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


#include "MDSSession.h"

#include <security_cdsa_plugin/DbContext.h>
#include "MDSModule.h"
#include "MDSAttrParser.h"
#include "MDSAttrUtils.h"

#include <memory>
#include <Security/cssmerr.h>
#include <security_utilities/logging.h>
#include <security_utilities/debugging.h>
#include <security_utilities/cfutilities.h>
#include <security_cdsa_client/dlquery.h>
#include <securityd_client/ssclient.h>
#include <Security/mds_schema.h>
#include <CoreFoundation/CFBundle.h>

#include <sys/types.h>
#include <sys/param.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

using namespace CssmClient;


/* 
 * The layout of the various MDS DB files on disk is as follows:
 *
 * /var/tmp/mds				-- owner = root, mode = 01777, world writable, sticky
 *    system/				-- owner = root, mode = 0755
 *       mdsObject.db		-- owner = root, mode = 0644, object DB
 *       mdsDirectory.db	-- owner = root, mode = 0644, MDS directory DB
 *	     mds.lock           -- temporary, owner = root, protects creation of and 
 *							   updates to previous two files
 *       mds.install.lock	-- owner = root, protects MDS_Install operation
 *    <uid>/				-- owner = <uid>, mode = 0700
 *     	 mdsObject.db		-- owner = <uid>, mode = 000, object DB
 *       mdsDirectory.db	-- owner = <uid>, mode = 000, MDS directory DB
 *	     mds.lock			-- owner = <uid>, protects updates of previous two files
 * 
 * The /var/tmp/mds/system directory is created at OS install time. The DB files in 
 * it are created by root at MDS_Install time. The ownership and mode of this directory and
 * of its parent is also re-checked and forced to the correct state at MDS_Install time. 
 * Each user has their own private directory with two DB files and a lock. The first time 
 * a user accesses MDS, the per-user directory is created and the per-user DB files are 
 * created as copies of the system DB files. Fcntl() with a F_RDLCK is used to lock the system
 * DB files when they are the source of these copies; this is the same mechanism
 * used by the underlying AtomicFile. 
 *
 * The sticky bit in /var/tmp/mds ensures that users cannot modify other userss private 
 * MDS directories. 
 */
namespace Security
{

/*
 * Nominal location of Security.framework.
 */
#define MDS_SYSTEM_PATH		"/System/Library/Frameworks"
#define MDS_SYSTEM_FRAME	"Security.framework"

/*
 * Nominal location of standard plugins.
 */
#define MDS_BUNDLE_PATH		"/System/Library/Security"
#define MDS_BUNDLE_EXTEN	".bundle"


/*
 * Location of MDS database and lock files.
 */
#define MDS_BASE_DB_DIR			"/private/var/tmp/mds"
#define MDS_SYSTEM_DB_COMP		"system"
#define MDS_SYSTEM_DB_DIR		MDS_BASE_DB_DIR "/" MDS_SYSTEM_DB_COMP

#define MDS_LOCK_FILE_NAME		"mds.lock"			
#define MDS_INSTALL_LOCK_NAME	"mds.install.lock"	
#define MDS_OBJECT_DB_NAME		"mdsObject.db"
#define MDS_DIRECT_DB_NAME		"mdsDirectory.db"

#define MDS_INSTALL_LOCK_PATH	MDS_SYSTEM_DB_DIR "/" MDS_INSTALL_LOCK_NAME
#define MDS_OBJECT_DB_PATH		MDS_SYSTEM_DB_DIR "/" MDS_OBJECT_DB_NAME
#define MDS_DIRECT_DB_PATH		MDS_SYSTEM_DB_DIR "/" MDS_DIRECT_DB_NAME

/* hard coded modes and a symbolic UID for root */
#define MDS_BASE_DB_DIR_MODE	(mode_t)01777
#define MDS_SYSTEM_DB_DIR_MODE	(mode_t)0755
#define MDS_SYSTEM_DB_MODE		(mode_t)0644
#define MDS_USER_DB_DIR_MODE	(mode_t)0700
#define MDS_USER_DB_MODE		(mode_t)0600
#define MDS_SYSTEM_UID			(uid_t)0

/*
 * Location of per-user bundles, relative to home directory.
 * Per-user DB files are in MDS_BASE_DB_DIR/<uid>/. 
 */
#define MDS_USER_BUNDLE		"Library/Security"

/* time to wait in ms trying to acquire lock */
#define DB_LOCK_TIMEOUT		(2 * 1000)

/* Minimum interval, in seconds, between rescans for plugin changes */
#define MDS_SCAN_INTERVAL 	5

/* trace file I/O */
#define MSIoDbg(args...)		secdebug("MDS_IO", ## args)

/* Trace cleanDir() */
#define MSCleanDirDbg(args...)	secdebug("MDS_CleanDir", ## args)

/*
 * Given a path to a directory, remove everything in the directory except for the optional
 * keepFileNames. Returns 0 on success, else an errno. 
 */
static int cleanDir(
	const char *dirPath,
	const char **keepFileNames,		// array of strings, size numKeepFileNames
	unsigned numKeepFileNames)
{
    DIR *dirp;
    struct dirent *dp;
	char fullPath[MAXPATHLEN];
	int rtn = 0;
	
	MSCleanDirDbg("cleanDir(%s) top", dirPath);
    if ((dirp = opendir(dirPath)) == NULL) {
		rtn = errno;
		MSCleanDirDbg("opendir(%s) returned  %d", dirPath, rtn);
        return rtn;
    }

    for(;;) {
		bool skip = false;
		const char *d_name = NULL;
		
		/* this block is for breaking on unqualified entries */
		do {
			errno = 0;
			dp = readdir(dirp);
			if(dp == NULL) {
				/* end of directory or error */
				rtn = errno;
				if(rtn) {
					MSCleanDirDbg("cleanDir(%s): readdir err %d", dirPath, rtn);
				}
				break;
			}
			d_name = dp->d_name;
			
			/* skip "." and ".." */
			if( (d_name[0] == '.') &&
			    ( (d_name[1] == '\0') ||
				  ( (d_name[1] == '.') && (d_name[2] == '\0') ) ) ) {
				skip = true;
				break;
			}
			
			/* skip entries in keepFileNames */
			for(unsigned dex=0; dex<numKeepFileNames; dex++) {
				if(!strcmp(keepFileNames[dex], d_name)) {
					skip = true;
					break;
				}
			}
		} while(0);
		if(rtn || (dp == NULL)) {
			/* one way or another, we're done */
			break;
		}
		if(skip) {
			/* try again */
			continue;
		}

		/* We have an entry to delete. Delete it, or recurse. */

		snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, d_name);
		if(dp->d_type == DT_DIR) {
			/* directory. Clean it, then delete. */
			MSCleanDirDbg("cleanDir recursing for dir %s", fullPath);
			rtn = cleanDir(fullPath, NULL, 0);
			if(rtn) {
				break;
			}
			MSCleanDirDbg("cleanDir deleting dir %s", fullPath);
			if(rmdir(fullPath)) {
				rtn = errno;
				MSCleanDirDbg("unlink(%s) returned %d", fullPath, rtn);
				break;
			}
		}
		else {
			MSCleanDirDbg("cleanDir deleting file %s", fullPath);
			if(unlink(fullPath)) {
				rtn = errno;
				MSCleanDirDbg("unlink(%s) returned %d", fullPath, rtn);
				break;
			}
		}
		
		/* 
		 * Back to beginning of directory for clean scan.
		 * Normally we'd just do a rewinddir() here but the RAMDisk filesystem,
		 * used when booting from DVD, does not implement that properly.
		 */
		closedir(dirp);
		if ((dirp = opendir(dirPath)) == NULL) {
			rtn = errno;
			MSCleanDirDbg("opendir(%s) returned  %d", dirPath, rtn);
			return rtn;
		}
    } /* main loop */

	closedir(dirp);
	return rtn;
}

/* 
 * Determine if a file exists as regular file with specified owner. Returns true if so.
 * If the purge argument is true, and there is something at the specified path that
 * doesn't meet spec, we do everything we can to delete it. If that fails we throw
 * CssmError(CSSM_ERRCODE_MDS_ERROR). If the delete succeeds we return false.
 * Returns the stat info on success for further processing by caller. 
 */
static bool doesFileExist(
	const char *filePath,
	uid_t forUid,
	bool purge,
	struct stat &sb)		// RETURNED
{
	MSIoDbg("stat %s in doesFileExist", filePath);
	if(lstat(filePath, &sb)) {
		/* doesn't exist or we can't even get to it. */
		if(errno == ENOENT) {
			return false;
		}
		if(purge) {
			/* If we can't stat it we sure can't delete it. */
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}
		return false;
	}
	
	/* it's there...how does it look? */
	mode_t fileType = sb.st_mode & S_IFMT;
	if((fileType == S_IFREG) && (sb.st_uid == forUid)) {
		return true;
	}
	if(!purge) {
		return false;
	}

	/* not what we want: get rid of it. */
	if(fileType == S_IFDIR) {
		/* directory: clean then remove */
		if(cleanDir(filePath, NULL, 0)) {
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}
		if(rmdir(filePath)) {
			MSDebug("rmdir(%s) returned %d", filePath, errno);
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}
	}
	else {
		if(unlink(filePath)) {
			MSDebug("unlink(%s) returned %d", filePath, errno);
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}
	}
	
	/* caller should be somewhat happy */
	return false;
}

/*
 * Determine if both of the specified DB files exist as accessible regular files with specified 
 * owner. Returns true if they do. 
 *
 * If the purge argument is true, we'll ensure that either both files exist with
 * the right owner, or neither of the files exist on exit. An error on that operation
 * throws a CSSM_ERRCODE_MDS_ERROR CssmError exception (i.e., we're hosed). 
 * Returns the stat info for both files on success for further processing by caller. 
 */
static bool doFilesExist(
	const char *objDbFile,
	const char *directDbFile,
	uid_t forUid,
	bool purge,					// false means "passive" check 
	struct stat &objDbSb,		// RETURNED
	struct stat &directDbSb)	// RETURNED
	
{
	bool objectExist = doesFileExist(objDbFile, forUid, purge, objDbSb);
	bool directExist = doesFileExist(directDbFile, forUid, purge, directDbSb);
	if(objectExist && directExist) {
		return true;
	}
	else if(!purge) {
		return false;
	}
	
	/* 
	 * At least one does not exist - ensure neither of them do.
	 * Note that if we got this far, we know the one that exists is a regular file
	 * so it's safe to just unlink it. 
	 */
	if(objectExist) {
		if(unlink(objDbFile)) {
			MSDebug("unlink(%s) returned %d", objDbFile, errno);
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}
	}
	if(directExist) {
		if(unlink(directDbFile)) {
			MSDebug("unlink(%s) returned %d", directDbFile, errno);
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}
	}
	return false;
}

/*
 * Determine if specified directory exists with specified owner and mode. 
 * Returns true if copacetic, else returns false and also indicates
 * via the directStatus out param what went wrong. 
 */
typedef enum {
	MDS_NotPresent,		/* nothing there */
	MDS_NotDirectory,	/* not a directory */
	MDS_BadOwnerMode,	/* wrong owner or mode */
	MDS_Access			/* couldn't search the directories */
} MdsDirectStatus;

static bool doesDirectExist(
	const char		*dirPath,
	uid_t			forUid,
	mode_t			mode,
	MdsDirectStatus	&directStatus)		/* RETURNED */
{
	struct stat sb;
	
	MSIoDbg("stat %s in doesDirectExist", dirPath);
	if (lstat(dirPath, &sb)) {
		int err = errno;
		switch(err) {
			case EACCES:
				directStatus = MDS_Access;
				break;
			case ENOENT:
				directStatus = MDS_NotPresent;
				break;
			/* Any others? Is this a good SWAG to handle the default? */
			default:
				directStatus = MDS_NotDirectory;
				break;
		}
		return false;
	}
	mode_t fileType = sb.st_mode & S_IFMT;
	if(fileType != S_IFDIR) {
		directStatus = MDS_NotDirectory;
		return false;
	}
	if(sb.st_uid != forUid) {
		directStatus = MDS_BadOwnerMode;
		return false;
	}
	if((sb.st_mode & 07777) != mode) {
		directStatus = MDS_BadOwnerMode;
		return false;
	}
	return true;
}

/*
 * Create specified directory if it doesn't already exist. If there is something 
 * already there that doesn't meet spec (not a directory, wrong mode, wrong owner)
 * we'll do everything we can do delete what is there and then try to create it
 * correctly.
 *
 * Returns an errno on any unrecoverable error. 
 */
static int createDir(
	const char *dirPath,
	uid_t forUid,			// for checking - we don't try to set this
	mode_t dirMode)
{
	MdsDirectStatus directStatus;
	
	if(doesDirectExist(dirPath, forUid, dirMode, directStatus)) {
		/* we're done */
		return 0;
	}
	
	/*
	 * Attempt recovery if there is *something* there.
	 * Anything other than "not present" should be considered to be a possible
	 * attack; syslog it. 
	 */
	int rtn;
	switch(directStatus) {
		case MDS_NotPresent:
			/* normal trivial case: proceed. */
			break;
			
		case MDS_NotDirectory:
			/* there's a file or symlink in the way */
			Syslog::alert("MDS error: %s is not a directory", dirPath);
			if(unlink(dirPath)) {
				rtn = errno;
				MSDebug("createDir(%s): unlink() returned %d", dirPath, rtn);
				return rtn;
			}
			break;
			
		case MDS_BadOwnerMode:
			/* 
			 * It's a directory; try to clean it out (which may well fail if we're
			 * not root).
			 */
			Syslog::alert("MDS error: %s with bad owner/mode", dirPath);
			rtn = cleanDir(dirPath, NULL, 0);
			if(rtn) {
				return rtn;
			}
			if(rmdir(dirPath)) {
				rtn = errno;
				MSDebug("createDir(%s): rmdir() returned %d", dirPath, rtn);
				return rtn;
			}
			/* good to go */
			break;
			
		case MDS_Access:		/* hopeless */
			Syslog::alert("MDS error: %s is inaccessible", dirPath);
			MSDebug("createDir(%s): access failure, bailing", dirPath);
			return EACCES;
	}
	rtn = mkdir(dirPath, dirMode);
	if(rtn) {
		rtn = errno;
		MSDebug("createDir(%s): mkdir() returned %d", dirPath, errno);
	}
	else {
		/* make sure umask does't trick us */
		rtn = chmod(dirPath, dirMode);
		if(rtn) {
			MSDebug("chmod(%s) returned  %d", dirPath, errno);
		}
	}
	return rtn;
}

/*
 * Create an MDS session.
 */
MDSSession::MDSSession (const Guid *inCallerGuid,
                        const CSSM_MEMORY_FUNCS &inMemoryFunctions) :
	DatabaseSession(MDSModule::get().databaseManager()),
	mCssmMemoryFunctions (inMemoryFunctions),
	mModule(MDSModule::get())
{
	MSDebug("MDSSession::MDSSession");
		
    mCallerGuidPresent =  inCallerGuid != nil;
    if (mCallerGuidPresent) {
        mCallerGuid = *inCallerGuid;
	}
}

MDSSession::~MDSSession ()
{
	MSDebug("MDSSession::~MDSSession");
}

void
MDSSession::terminate ()
{
	MSDebug("MDSSession::terminate");
    closeAll();
}

const char* kExceptionDeletePath = "messages";


/*
 * Called by security server via MDS_Install().
 */
void
MDSSession::install ()
{
	//
	// Installation requires root
	//
	if(geteuid() != (uid_t)0) { 
		CssmError::throwMe(CSSMERR_DL_OS_ACCESS_DENIED);
	}
	
	//
	// install() is only (legitimately) called from securityd.
	// Mark "server mode" so we don't end up waiting for ourselves when the databases open.
	//
	mModule.setServerMode();
	
	int sysFdLock = -1;
	try {
		/* ensure MDS base directory exists with correct permissions */
		if(createDir(MDS_BASE_DB_DIR, MDS_SYSTEM_UID, MDS_BASE_DB_DIR_MODE)) {
			MSDebug("Error creating base MDS dir; aborting.");
			CssmError::throwMe(CSSMERR_DL_OS_ACCESS_DENIED);
		}
		       
		/* ensure the the system MDS DB directory exists with correct permissions */
		if(createDir(MDS_SYSTEM_DB_DIR, MDS_SYSTEM_UID, MDS_SYSTEM_DB_DIR_MODE)) {
			MSDebug("Error creating system MDS dir; aborting.");
			CssmError::throwMe(CSSMERR_DL_OS_ACCESS_DENIED);
		}

		if(!obtainLock(MDS_INSTALL_LOCK_PATH, sysFdLock, DB_LOCK_TIMEOUT)) {
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}

		/* 
		 * We own the whole MDS system. Clean everything out except for our lock
		 * (and the directory it's in :-)
		 */
		
		const char *savedFile = MDS_INSTALL_LOCK_NAME;
		if(cleanDir(MDS_SYSTEM_DB_DIR, &savedFile, 1)) {
			/* this should never happen - we're root */
			Syslog::alert("MDS error: unable to clean %s", MDS_SYSTEM_DB_DIR);
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}
		
		const char *savedFiles[] = {MDS_SYSTEM_DB_COMP, kExceptionDeletePath};
		if(cleanDir(MDS_BASE_DB_DIR, savedFiles, 2)) {
			/* this should never happen - we're root */
			Syslog::alert("MDS error: unable to clean %s", MDS_BASE_DB_DIR);
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}
				
		/* 
		 * Do initial population of system DBs.
		 */
		createSystemDatabases(CSSM_FALSE, MDS_SYSTEM_DB_MODE);
		DbFilesInfo dbFiles(*this, MDS_SYSTEM_DB_DIR);
		dbFiles.updateSystemDbInfo(MDS_SYSTEM_PATH, MDS_BUNDLE_PATH);
	}
	catch(...) {
		if(sysFdLock != -1) {
			releaseLock(sysFdLock);
		}
		throw;
	}
	releaseLock(sysFdLock);
}

//
// In this implementation, the uninstall() call is not supported since
// we do not allow programmatic deletion of the MDS databases.
//

void
MDSSession::uninstall ()
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

/*
 * Common private open routine given a full specified path.
 */
CSSM_DB_HANDLE MDSSession::dbOpen(const char *dbName, bool batched)
{
	static CSSM_APPLEDL_OPEN_PARAMETERS batchOpenParams = {
		sizeof(CSSM_APPLEDL_OPEN_PARAMETERS),
		CSSM_APPLEDL_OPEN_PARAMETERS_VERSION,
		CSSM_FALSE,		// do not auto-commit
		0				// mask - do not use following fields
	};
	
	MSDebug("Opening %s%s", dbName, batched ? " in batch mode" : "");
	MSIoDbg("open %s in dbOpen(name, batched)", dbName);
	CSSM_DB_HANDLE dbHand;
	DatabaseSession::DbOpen(dbName,
		NULL,				// DbLocation
		CSSM_DB_ACCESS_READ,
		NULL,				// AccessCred - hopefully optional 
		batched ? &batchOpenParams : NULL,
		dbHand);
	return dbHand;
}

/* DatabaseSession routines we need to override */
void MDSSession::DbOpen(const char *DbName,
		const CSSM_NET_ADDRESS *DbLocation,
		CSSM_DB_ACCESS_TYPE AccessRequest,
		const AccessCredentials *AccessCred,
		const void *OpenParameters,
		CSSM_DB_HANDLE &DbHandle)
{
	if (!mModule.serverMode()) {
		/*
		 * Make sure securityd has finished initializing (system) MDS data.
		 * Note that activate() only does IPC once and retains global state after that.
		 */
		SecurityServer::ClientSession client(Allocator::standard(), Allocator::standard());
		client.activate();		/* contact securityd - won't return until MDS is ready */
	}

	/* make sure DBs are up-to-date */
	updateDataBases();
	
	/* 
	 * Only task here is map incoming DbName - specified in the CDSA 
	 * spec - to a filename we actually use (which is a path to either 
	 * a system MDS DB file or a per-user MDS DB file).  
	 */
	if(DbName == NULL) {
		CssmError::throwMe(CSSMERR_DL_INVALID_DB_NAME);
	}
	const char *dbName;
	if(!strcmp(DbName, MDS_OBJECT_DIRECTORY_NAME)) {
		dbName = MDS_OBJECT_DB_NAME;
	}
	else if(!strcmp(DbName, MDS_CDSA_DIRECTORY_NAME)) {
		dbName = MDS_DIRECT_DB_NAME;
	}
	else {
		CssmError::throwMe(CSSMERR_DL_INVALID_DB_NAME);
	}
	char fullPath[MAXPATHLEN];
	dbFullPath(dbName, fullPath);
	MSIoDbg("open %s in dbOpen(name, loc, accessReq...)", dbName);
	DatabaseSession::DbOpen(fullPath, DbLocation, AccessRequest, AccessCred,
		OpenParameters, DbHandle);
}

CSSM_HANDLE MDSSession::DataGetFirst(CSSM_DB_HANDLE DBHandle,
                             const CssmQuery *Query,
                             CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR Attributes,
                             CssmData *Data,
                             CSSM_DB_UNIQUE_RECORD_PTR &UniqueId)
{
	updateDataBases();
	return DatabaseSession::DataGetFirst(DBHandle, Query, Attributes, Data, UniqueId);
}


void
MDSSession::GetDbNames(CSSM_NAME_LIST_PTR &outNameList)
{
	outNameList = new CSSM_NAME_LIST[1];
	outNameList->NumStrings = 2;
	outNameList->String = new char*[2];
	outNameList->String[0] = MDSCopyCstring(MDS_OBJECT_DIRECTORY_NAME);
	outNameList->String[1] = MDSCopyCstring(MDS_CDSA_DIRECTORY_NAME);
}

void
MDSSession::FreeNameList(CSSM_NAME_LIST &inNameList)
{
	delete [] inNameList.String[0];
	delete [] inNameList.String[1];
	delete [] inNameList.String;
}

void MDSSession::GetDbNameFromHandle(CSSM_DB_HANDLE DBHandle,
	char **DbName)
{
	printf("GetDbNameFromHandle: code on demand\n");
	CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
}

//
// Attempt to obtain an exclusive lock over the the MDS databases. The
// parameter is the maximum amount of time, in milliseconds, to spend
// trying to obtain the lock. A value of zero means to return failure
// right away if the lock cannot be obtained.
//
bool
MDSSession::obtainLock(
	const char *lockFile,	// e.g. MDS_INSTALL_LOCK_PATH
	int &fd,				// IN/OUT
	int timeout)			// default 0
{
	fd = -1;
	for(;;) {
		secdebug("mdslock", "obtainLock: calling open(%s)", lockFile);
		fd = open(lockFile, O_EXLOCK | O_CREAT | O_RDWR, 0644);
		if(fd == -1) {
			int err = errno;
			secdebug("mdslock", "obtainLock: open error %d", errno);
			if(err == EINTR) {
				/* got a signal, go again */
				continue;
			}
			else {
				/* theoretically should never happen */
				return false;
			}
		}
		else {
			secdebug("mdslock", "obtainLock: success");
			return true;
		}
	}
	
	/* not reached */
	return false;
}

//
// Release the exclusive lock over the MDS databases. If this session
// does not hold the lock, this method does nothing.
//

void
MDSSession::releaseLock(int &fd)
{
	secdebug("mdslock", "releaseLock");
	assert(fd != -1);
	flock(fd, LOCK_UN);
	close(fd);
	fd = -1;
}

/* given DB file name, fill in fully specified path */
void MDSSession::dbFullPath(
	const char *dbName,
	char fullPath[MAXPATHLEN+1])
{
	mModule.getDbPath(fullPath);
	assert(fullPath[0] != '\0');
	strcat(fullPath, "/");
	strcat(fullPath, dbName);
}

/*
 * See if any per-user bundles exist in specified directory. Returns true if so.
 * First the check for one entry....
 */
static bool isBundle(
	const struct dirent *dp)
{
	if(dp == NULL) {
		return false;
	}
	/* NFS directories show up as DT_UNKNOWN */
	switch(dp->d_type) {
		case DT_UNKNOWN:
		case DT_DIR:
			break;
		default:
			return false;
	}
	int suffixLen = strlen(MDS_BUNDLE_EXTEN);
	int len = strlen(dp->d_name);
	
	return (len >= suffixLen) && 
	       !strcmp(dp->d_name + len - suffixLen, MDS_BUNDLE_EXTEN);
}

/* now the full directory search */
static bool checkUserBundles(
	const char *bundlePath)
{
	MSDebug("searching for user bundles in %s", bundlePath);
	DIR *dir = opendir(bundlePath);
	if (dir == NULL) {
		return false;
	}
	struct dirent *dp;
	bool rtn = false;
	while ((dp = readdir(dir)) != NULL) {
		if(isBundle(dp)) {
			/* any other checking to do? */
			rtn = true;
			break;
		}
	}
	closedir(dir);
	MSDebug("...%s bundle(s) found", rtn ? "" : "No");
	return rtn;
}

#define COPY_BUF_SIZE	1024

/* 
 * Single file copy with locking. 
 * Ensures that the source is a regular file with specified owner. 
 * Caller specifies mode of destination file. 
 * Throws a CssmError if the source file doesn't meet spec; throws a
 *    UnixError on any other error (which is generally recoverable by 
 *    having the user MDS session use the system DB files).
 */
static void safeCopyFile(
	const char *fromPath,
	uid_t fromUid,
	const char *toPath,
	mode_t toMode)
{
	int error = 0;
	bool haveLock = false;
	int destFd = 0;
	int srcFd = 0;
	struct stat sb;
	char tmpToPath[MAXPATHLEN+1];
		
	MSIoDbg("open %s, %s in safeCopyFile", fromPath, toPath);

	if(!doesFileExist(fromPath, fromUid, false, sb)) {
		MSDebug("safeCopyFile: bad system DB file %s", fromPath);
		CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
	}
	
	/* create temp destination */
	snprintf(tmpToPath, sizeof(tmpToPath), "%s_", toPath);
	destFd = open(tmpToPath, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC | O_EXCL, toMode);
	if(destFd < 0) {
		error = errno;
		MSDebug("Error %d opening user DB file %s\n", error, tmpToPath);
		UnixError::throwMe(error);
	}
	
	struct flock fl;
	try {
		/* don't get tripped up by umask */
		if(fchmod(destFd, toMode)) {
			error = errno;
			MSDebug("Error %d chmoding user DB file %s\n", error, tmpToPath);
			UnixError::throwMe(error);
		}

		/* open source for reading */
		srcFd = open(fromPath, O_RDONLY, 0);
		if(srcFd < 0) {
			error = errno;
			MSDebug("Error %d opening system DB file %s\n", error, fromPath);
			UnixError::throwMe(error);
		}
		
		/* acquire the same kind of lock AtomicFile uses */
		fl.l_start = 0;
		fl.l_len = 1;
		fl.l_pid = getpid();
		fl.l_type = F_RDLCK;		// AtomicFile gets F_WRLCK
		fl.l_whence = SEEK_SET;

		// Keep trying to obtain the lock if we get interupted.
		for (;;) {
			if (::fcntl(srcFd, F_SETLKW, &fl) == -1) {
				error = errno;
				if (error == EINTR) {
					error = 0;
					continue;
				}
				MSDebug("Error %d locking system DB file %s\n", error, fromPath);
				UnixError::throwMe(error);
			}
			else {
				break;
				haveLock = true;
			}
		}

		/* copy */
		char buf[COPY_BUF_SIZE];
		while(1) {
			int bytesRead = read(srcFd, buf, COPY_BUF_SIZE);
			if(bytesRead == 0) {
				break;
			}
			if(bytesRead < 0) {
				error = errno;
				MSDebug("Error %d reading system DB file %s\n", error, fromPath);
				UnixError::throwMe(error);
			}
			int bytesWritten = write(destFd, buf, bytesRead);
			if(bytesWritten < 0) {
				error = errno;
				MSDebug("Error %d writing user DB file %s\n", error, tmpToPath);
				UnixError::throwMe(error);
			}
		}
	}
	catch(...) {
		/* error is nonzero, we'll re-throw below...still have some cleanup */
	}
	
	/* unlock source and close both */
	if(haveLock) {
		fl.l_type = F_UNLCK;
		if (::fcntl(srcFd, F_SETLK, &fl) == -1) {
			MSDebug("Error %d unlocking system DB file %s\n", errno, fromPath);
		}
	}
	MSIoDbg("close %s, %s in safeCopyFile", fromPath, tmpToPath);
	if(srcFd) {
		close(srcFd);
	}
	if(destFd) {
		close(destFd);
	}
	if(error == 0) {
		/* commit temp file */
		if(::rename(tmpToPath, toPath)) {
			error = errno;
			MSDebug("Error %d committing %s\n", error, toPath);
		}
	}
	if(error) {
		UnixError::throwMe(error);
	}
}

/* 
 * Copy system DB files to specified user dir. Caller holds user DB lock. 
 * Throws a UnixError on error.
 */
static void copySystemDbs(
	const char *userDbFileDir)
{
	char toPath[MAXPATHLEN+1];
	
	snprintf(toPath, sizeof(toPath), "%s/%s", userDbFileDir, MDS_OBJECT_DB_NAME);
	safeCopyFile(MDS_OBJECT_DB_PATH, MDS_SYSTEM_UID, toPath, MDS_USER_DB_MODE);
	snprintf(toPath, sizeof(toPath), "%s/%s", userDbFileDir, MDS_DIRECT_DB_NAME);
	safeCopyFile(MDS_DIRECT_DB_PATH, MDS_SYSTEM_UID, toPath, MDS_USER_DB_MODE);
}

/*
 * Ensure current DB files exist and are up-to-date.
 * Called from MDSSession constructor and from DataGetFirst, DbOpen, and any
 * other public functions which access a DB from scratch.
 */
void MDSSession::updateDataBases()
{
	RecursionBlock::Once once(mUpdating);
	if (once())
		return;	// already updating; don't recurse
	
	uid_t ourUid = geteuid();
	bool isRoot = (ourUid == 0);
	
	/* if we scanned recently, we're done */
	double delta = mModule.timeSinceLastScan();
	if(delta < (double)MDS_SCAN_INTERVAL) {
		return;
	}

	/*
	 * If we're root, the first thing we do is to ensure that system DBs are present.
	 * Note that this is a necessary artifact of the problem behind Radar 3800811.
	 * When that is fixed, install() should ONLY be called from the public MDS_Install()
	 * routine.
	 * Anyway, if we *do* have to install here, we're done.
	 */
	if(isRoot && !systemDatabasesPresent(false)) {
		install();
		mModule.setDbPath(MDS_SYSTEM_DB_DIR);
		mModule.lastScanIsNow();
		return;
	}
	
	/* 
	 * Obtain various per-user paths. Root is a special case but follows most
	 * of the same logic from here on.
	 */
	char userDbFileDir[MAXPATHLEN+1];
	char userObjDbFilePath[MAXPATHLEN+1];
	char userDirectDbFilePath[MAXPATHLEN+1];
	char userBundlePath[MAXPATHLEN+1];
	char userDbLockPath[MAXPATHLEN+1];
	
	/* this means "no user bundles" */
	userBundlePath[0] = '\0';
	if(isRoot) {
		strcpy(userDbFileDir, MDS_SYSTEM_DB_DIR);
		/* no userBundlePath */
	}
	else {
		char *userHome = getenv("HOME");
		if((userHome == NULL) ||
		   (strlen(userHome) + strlen(MDS_USER_BUNDLE) + 2) > sizeof(userBundlePath)) {
			/* Can't check for user bundles */
			MSDebug("missing or invalid HOME; skipping user bundle check");
		}
		/* TBD: any other checking of userHome? */
		else {
			snprintf(userBundlePath, sizeof(userBundlePath), 
				"%s/%s", userHome, MDS_USER_BUNDLE);
		}
		
		/* DBs go in a per-UID directory in the base MDS DB directory */
		snprintf(userDbFileDir, sizeof(userDbFileDir),
			"%s/%d", MDS_BASE_DB_DIR, (int)ourUid);
	}
	snprintf(userObjDbFilePath,    sizeof(userObjDbFilePath), 
		"%s/%s", userDbFileDir, MDS_OBJECT_DB_NAME);
	snprintf(userDirectDbFilePath, sizeof(userDirectDbFilePath), 
		"%s/%s", userDbFileDir, MDS_DIRECT_DB_NAME);
	snprintf(userDbLockPath,       sizeof(userDbLockPath),
		"%s/%s", userDbFileDir, MDS_LOCK_FILE_NAME);
	
	/* 
	 * Create the per-user directory...that's where the lock we'll be using lives.
	 */
	if(!isRoot) {
		if(createDir(userDbFileDir, ourUid, MDS_USER_DB_DIR_MODE)) {
			/* 
			 * We'll just have to limp along using the read-only system DBs.
			 * Note that this protects (somewhat) against the DoS attack in 
			 * Radar 3801292. The only problem is that this user won't be able 
			 * to use per-user bundles. 
			 */
			Syslog::alert("MDS Error: unable to create %s", userDbFileDir);
			MSDebug("Error creating user DBs; using system DBs");
			mModule.setDbPath(MDS_SYSTEM_DB_DIR);
			return;
		}
	}

	/* always release userLockFd no matter what happens */
	int userLockFd = -1;
	if(!obtainLock(userDbLockPath, userLockFd, DB_LOCK_TIMEOUT)) {
		CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
	}
	try {
		if(!isRoot) {
			try {
				/* 
				 * We copy the system DBs to the per-user DBs in two cases:
				 * -- user DBs don't exist, or
				 * -- system DBs have changed since the the last update to the user DBs. 
				 *    This happens on smart card insertion and removal. 
				 */
				bool doCopySystem = false;
				struct stat userObjStat, userDirectStat;
				if(!doFilesExist(userObjDbFilePath, userDirectDbFilePath, ourUid, true,
						userObjStat, userDirectStat)) {
					doCopySystem = true;
				}
				else {
					/* compare the two mdsDirectory.db files */
					MSIoDbg("stat %s, %s in updateDataBases",
						MDS_DIRECT_DB_PATH, userDirectDbFilePath);
					struct stat sysStat;
					if (!stat(MDS_DIRECT_DB_PATH, &sysStat)) {
						doCopySystem = (sysStat.st_mtimespec.tv_sec > userDirectStat.st_mtimespec.tv_sec) ||
							((sysStat.st_mtimespec.tv_sec == userDirectStat.st_mtimespec.tv_sec) &&
								(sysStat.st_mtimespec.tv_nsec > userDirectStat.st_mtimespec.tv_nsec));
						if(doCopySystem) {
							MSDebug("user DB files obsolete at %s", userDbFileDir);
						}
					}
				}
				if(doCopySystem) {
					/* copy system DBs to user DBs */
					MSDebug("copying system DBs to user at %s", userDbFileDir);
					copySystemDbs(userDbFileDir);
				}
				else {
					MSDebug("Using existing user DBs at %s", userDbFileDir);
				}
			}
			catch(const CssmError &cerror) {
				/*
				 * Bad system DB file detected. Fatal.
				 */
				Syslog::alert("MDS Error: bad system DB file");
				throw;
			}
			catch(...) {
				/* 
				 * Error on delete or create user DBs; fall back on system DBs. 
				 */
				Syslog::alert("MDS Error: unable to create user DBs in %s", userDbFileDir);
				MSDebug("doFilesExist(purge) error; using system DBs");
				mModule.setDbPath(MDS_SYSTEM_DB_DIR);
				releaseLock(userLockFd);
				return;
			}
		}
		else {
			MSDebug("Using system DBs only");
		}
		
		/* 
		 * Update per-user DBs from both bundle sources (System bundles, user bundles)
		 * as appropriate. 
		 */
		DbFilesInfo dbFiles(*this, userDbFileDir);
		dbFiles.removeOutdatedPlugins();
		dbFiles.updateSystemDbInfo(NULL, MDS_BUNDLE_PATH);
		if(userBundlePath[0]) {
			/* skip for invalid or missing $HOME... */
			if(checkUserBundles(userBundlePath)) {
				dbFiles.updateForBundleDir(userBundlePath);
			}
		}
		mModule.setDbPath(userDbFileDir);
	}	/* main block protected by mLockFd */
	catch(...) {
		releaseLock(userLockFd);
		throw;
	}
	mModule.lastScanIsNow();
	releaseLock(userLockFd);
}

/*
 * Remove all records with specified guid (a.k.a. ModuleID) from specified DB.
 */
void MDSSession::removeRecordsForGuid(
	const char *guid,
	CSSM_DB_HANDLE dbHand)
{
	// tell the DB to flush its intermediate data to disk
	PassThrough(dbHand, CSSM_APPLEFILEDL_COMMIT, NULL, NULL);
	CssmClient::Query query = Attribute("ModuleID") == guid;
	clearRecords(dbHand, query.cssmQuery());
}


void MDSSession::clearRecords(CSSM_DB_HANDLE dbHand, const CssmQuery &query)
{
	CSSM_DB_UNIQUE_RECORD_PTR record = NULL;
	CSSM_HANDLE resultHand = DataGetFirst(dbHand,
		&query,
		NULL,
		NULL,			// No data
		record);
	if (resultHand == CSSM_INVALID_HANDLE)
		return; // no matches
	try {
		do {
			DataDelete(dbHand, *record);
			FreeUniqueRecord(dbHand, *record);
			record = NULL;
		} while (DataGetNext(dbHand,
			resultHand,
			NULL,
			NULL,
			record));
	} catch (...) {
		if (record)
			FreeUniqueRecord(dbHand, *record);
		DataAbortQuery(dbHand, resultHand);
	}
}


/*
 * Determine if system databases are present. 
 * If the purge argument is true, we'll ensure that either both or neither 
 * DB files exist on exit; in that case caller must be holding MDS_INSTALL_LOCK_PATH.
 */
bool MDSSession::systemDatabasesPresent(bool purge)
{
	bool rtn = false;
	
	try {
		/* 
		 * This can throw on a failed attempt to delete sole existing file....
		 * But if that happens while we're root, our goose is fully cooked. 
		 */
		struct stat objDbSb, directDbSb;
		if(doFilesExist(MDS_OBJECT_DB_PATH, MDS_DIRECT_DB_PATH, 
				MDS_SYSTEM_UID, purge, objDbSb, directDbSb)) {
			rtn = true;
		}
	}
	catch(...) {
	
	}
	return rtn;
}

/* 
 * Given a DB name (which is used as an absolute path) and an array of 
 * RelationInfos, create a DB.
 */
void
MDSSession::createSystemDatabase(
	const char *dbName,
	const RelationInfo *relationInfo,
	unsigned numRelations,
	CSSM_BOOL autoCommit,
	mode_t mode,
	CSSM_DB_HANDLE &dbHand)			// RETURNED
{
	CSSM_DBINFO dbInfo;
	CSSM_DBINFO_PTR dbInfoP = &dbInfo;
	
	memset(dbInfoP, 0, sizeof(CSSM_DBINFO));
	dbInfoP->NumberOfRecordTypes = numRelations;
	dbInfoP->IsLocal = CSSM_TRUE;		// TBD - what does this mean?
	dbInfoP->AccessPath = NULL;		// TBD
	
	/* alloc numRelations elements for parsingModule, recordAttr, and recordIndex
	 * info arrays */
	unsigned size = sizeof(CSSM_DB_PARSING_MODULE_INFO) * numRelations;
	dbInfoP->DefaultParsingModules = (CSSM_DB_PARSING_MODULE_INFO_PTR)malloc(size);
	memset(dbInfoP->DefaultParsingModules, 0, size);
	size = sizeof(CSSM_DB_RECORD_ATTRIBUTE_INFO) * numRelations;
	dbInfoP->RecordAttributeNames = (CSSM_DB_RECORD_ATTRIBUTE_INFO_PTR)malloc(size);
	memset(dbInfoP->RecordAttributeNames, 0, size);
	size = sizeof(CSSM_DB_RECORD_INDEX_INFO) * numRelations;
	dbInfoP->RecordIndexes = (CSSM_DB_RECORD_INDEX_INFO_PTR)malloc(size);
	memset(dbInfoP->RecordIndexes, 0, size);
	
	/* cook up attribute and index info for each relation */
	unsigned relation;
	for(relation=0; relation<numRelations; relation++) {
		const struct RelationInfo *relp = &relationInfo[relation];	// source
		CSSM_DB_RECORD_ATTRIBUTE_INFO_PTR attrInfo = 
			&dbInfoP->RecordAttributeNames[relation];					// dest 1
		CSSM_DB_RECORD_INDEX_INFO_PTR indexInfo = 
			&dbInfoP->RecordIndexes[relation];						// dest 2
			
		attrInfo->DataRecordType = relp->DataRecordType;
		attrInfo->NumberOfAttributes = relp->NumberOfAttributes;
		attrInfo->AttributeInfo = (CSSM_DB_ATTRIBUTE_INFO_PTR)relp->AttributeInfo;
		
		indexInfo->DataRecordType = relp->DataRecordType;
		indexInfo->NumberOfIndexes = relp->NumberOfIndexes;
		indexInfo->IndexInfo = (CSSM_DB_INDEX_INFO_PTR)relp->IndexInfo;
	}

	/* set autocommit and mode */
	CSSM_APPLEDL_OPEN_PARAMETERS openParams;
	memset(&openParams, 0, sizeof(openParams));
	openParams.length = sizeof(openParams);
	openParams.version = CSSM_APPLEDL_OPEN_PARAMETERS_VERSION;
	openParams.autoCommit = autoCommit;
	openParams.mask = kCSSM_APPLEDL_MASK_MODE;
	openParams.mode = mode;
	
	try {
		DbCreate(dbName,
			NULL,			// DbLocation
			*dbInfoP,
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL,			// CredAndAclEntry
			&openParams,
			dbHand);
	}
	catch(...) {
		MSDebug("Error on DbCreate");
		free(dbInfoP->DefaultParsingModules);
		free(dbInfoP->RecordAttributeNames);
		free(dbInfoP->RecordIndexes);
		throw;
	}
	free(dbInfoP->DefaultParsingModules);
	free(dbInfoP->RecordAttributeNames);
	free(dbInfoP->RecordIndexes);
	
}

/*
 * Create system databases from scratch if they do not already exist. 
 * MDS_INSTALL_LOCK_PATH held on entry and exit. MDS_SYSTEM_DB_DIR assumed to
 * exist (that's our caller's job, before acquiring MDS_INSTALL_LOCK_PATH). 
 * Returns true if we actually built the files, false if they already 
 * existed.
 */
bool MDSSession::createSystemDatabases(
	CSSM_BOOL autoCommit,
	mode_t mode)
{
	CSSM_DB_HANDLE objectDbHand = 0;
	CSSM_DB_HANDLE directoryDbHand = 0;
	
	assert(geteuid() == (uid_t)0);
	if(systemDatabasesPresent(true)) {
		/* both databases exist as regular files with correct owner - we're done */
		MSDebug("system DBs already exist");
		return false;
	}

	/* create two DBs - any exception here results in deleting both of them */
	MSDebug("Creating MDS DBs");
	try {
		createSystemDatabase(MDS_OBJECT_DB_PATH, &kObjectRelation, 1, 
			autoCommit, mode, objectDbHand);
		MSIoDbg("close objectDbHand in createSystemDatabases");
		DbClose(objectDbHand);
		objectDbHand = 0;
		createSystemDatabase(MDS_DIRECT_DB_PATH, kMDSRelationInfo, kNumMdsRelations,
			autoCommit, mode, directoryDbHand);
		MSIoDbg("close directoryDbHand in createSystemDatabases");
		DbClose(directoryDbHand);
		directoryDbHand = 0;
	}
	catch (...) {
		MSDebug("Error creating MDS DBs - deleting both DB files");
		unlink(MDS_OBJECT_DB_PATH);
		unlink(MDS_DIRECT_DB_PATH);
		throw;
	}
	return true;
}

/*
 * DbFilesInfo helper class
 */
 
/* Note both DB files MUST exist at construction time */
MDSSession::DbFilesInfo::DbFilesInfo(
	MDSSession &session, 
	const char *dbPath) :
		mSession(session),
		mObjDbHand(0),
		mDirectDbHand(0),
		mLaterTimestamp(0)
{
	assert(strlen(dbPath) < MAXPATHLEN);
	strcpy(mDbPath, dbPath);
	
	/* stat the two DB files, snag the later timestamp */
	char path[MAXPATHLEN];
	sprintf(path, "%s/%s", mDbPath, MDS_OBJECT_DB_NAME);
	struct stat sb;
	MSIoDbg("stat %s in DbFilesInfo()", path);
	int rtn = ::stat(path, &sb);
	if(rtn) {
		int error = errno;
		MSDebug("Error %d statting DB file %s", error, path);
		UnixError::throwMe(error);
	}
	mLaterTimestamp = sb.st_mtimespec.tv_sec;
	sprintf(path, "%s/%s", mDbPath, MDS_DIRECT_DB_NAME);
	MSIoDbg("stat %s in DbFilesInfo()", path);
	rtn = ::stat(path, &sb);
	if(rtn) {
		int error = errno;
		MSDebug("Error %d statting DB file %s", error, path);
		UnixError::throwMe(error);
	}
	if(sb.st_mtimespec.tv_sec > mLaterTimestamp) {
		mLaterTimestamp = sb.st_mtimespec.tv_sec;
	}
}

MDSSession::DbFilesInfo::~DbFilesInfo()
{
	if(mObjDbHand != 0) {
		/* autocommit on, henceforth */
		mSession.PassThrough(mObjDbHand,
			CSSM_APPLEFILEDL_COMMIT, NULL, NULL);
		MSIoDbg("close objectDbHand in ~DbFilesInfo()");
		mSession.DbClose(mObjDbHand);
		mObjDbHand = 0;
	}
	if(mDirectDbHand != 0) {
		mSession.PassThrough(mDirectDbHand,
			CSSM_APPLEFILEDL_COMMIT, NULL, NULL);
		MSIoDbg("close mDirectDbHand in ~DbFilesInfo()");
		mSession.DbClose(mDirectDbHand);
		mDirectDbHand = 0;
	}
}

/* lazy evaluation of both DB handles */
CSSM_DB_HANDLE MDSSession::DbFilesInfo::objDbHand()
{
	if(mObjDbHand != 0) {
		return mObjDbHand;
	}
	char fullPath[MAXPATHLEN + 1];
	sprintf(fullPath, "%s/%s", mDbPath, MDS_OBJECT_DB_NAME);
	MSIoDbg("open %s in objDbHand()", fullPath);
	mObjDbHand = mSession.dbOpen(fullPath, true);	// batch mode
	return mObjDbHand;
}

CSSM_DB_HANDLE MDSSession::DbFilesInfo::directDbHand()
{
	if(mDirectDbHand != 0) {
		return mDirectDbHand;
	}
	char fullPath[MAXPATHLEN + 1];
	sprintf(fullPath, "%s/%s", mDbPath, MDS_DIRECT_DB_NAME);
	MSIoDbg("open %s in directDbHand()", fullPath);
	mDirectDbHand = mSession.dbOpen(fullPath, true);	// batch mode
	return mDirectDbHand;
}

/*
 * Update the info for Security.framework and the system bundles.
 */
void MDSSession::DbFilesInfo::updateSystemDbInfo(
	const char *systemPath,		// e.g., /System/Library/Frameworks
	const char *bundlePath)		// e.g., /System/Library/Security
{
	/* 
	 * Security.framework - CSSM and built-in modules - only for initial population of
	 * system DB files. 
	 */
	if (systemPath) {
		string path;
		if (CFRef<CFBundleRef> me = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.security")))
			if (CFRef<CFURLRef> url = CFBundleCopyBundleURL(me))
				if (CFRef<CFStringRef> cfpath = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle))
					path = cfString(cfpath);	// path to my bundle

		if (path.empty())   // use system default
			path = string(systemPath) + "/" MDS_SYSTEM_FRAME;
		updateForBundle(path.c_str());
	}
	
	/* Standard loadable bundles */
	updateForBundleDir(bundlePath);
}


MDSSession::DbFilesInfo::TbdRecord::TbdRecord(
	const CSSM_DATA &guid)
{
	assert(guid.Length <= MAX_GUID_LEN);
	assert(guid.Length != 0);
	memmove(mGuid, guid.Data, guid.Length);
	if(mGuid[guid.Length - 1] != '\0') {
		mGuid[guid.Length] = '\0';
	}
}

/*
 * Test if plugin specified by pluginPath needs to be deleted from DBs. 
 * If so, add to tbdVector.
 */
void MDSSession::DbFilesInfo::checkOutdatedPlugin(
	const CSSM_DATA &pathValue, 
	const CSSM_DATA &guidValue, 
	TbdVector &tbdVector)
{
	/* stat the specified plugin */
	struct stat sb;
	bool obsolete = false;
	string path = CssmData::overlay(pathValue).toString();
	if (!path.empty() && path[0] == '*') {
		/* builtin pseudo-path; never obsolete this */
		return;
	}
	MSIoDbg("stat %s in checkOutdatedPlugin()", path.c_str());
	int rtn = ::stat(path.c_str(), &sb);
	if(rtn) {
		/* not there or inaccessible; delete */
		obsolete = true;
	}
	else if(sb.st_mtimespec.tv_sec > mLaterTimestamp) {
		/* timestamp of plugin's main directory later than that of DBs */
		obsolete = true;
	}
	if(obsolete) {
		TbdRecord *tbdRecord = new TbdRecord(guidValue);
		tbdVector.push_back(tbdRecord);
		MSDebug("checkOutdatedPlugin: flagging %s obsolete", path.c_str());
	}
}

/*
 * Examine dbFiles.objDbHand; remove all fields associated with any bundle
 * i.e., with any path) which are either not present on disk, or which 
 * have changed since dbFiles.laterTimestamp().
 */
void MDSSession::DbFilesInfo::removeOutdatedPlugins()
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_HANDLE						resultHand;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA			theAttrs[2];
	CSSM_DB_ATTRIBUTE_INFO_PTR		attrInfo;
	TbdVector						tbdRecords;
	
	/* 
	 * First, scan object directory. All we need are the path and GUID attributes. 
	 */
	recordAttrs.DataRecordType = MDS_OBJECT_RECORDTYPE;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = 2;
	recordAttrs.AttributeData = theAttrs;
	
	attrInfo = &theAttrs[0].Info;
	attrInfo->AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attrInfo->Label.AttributeName = "ModuleID";
	attrInfo->AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_STRING;
	theAttrs[0].NumberOfValues = 0;
	theAttrs[0].Value = NULL;
	attrInfo = &theAttrs[1].Info;
	attrInfo->AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attrInfo->Label.AttributeName = "Path";
	attrInfo->AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_STRING;
	theAttrs[1].NumberOfValues = 0;
	theAttrs[1].Value = NULL;
	
	/* just search by recordType, no predicates */
	query.RecordType = MDS_OBJECT_RECORDTYPE;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;		// CSSM_QUERY_RETURN_DATA...FIXME - used?

	CssmQuery perryQuery(query);
	try {
		resultHand = mSession.DataGetFirst(objDbHand(),
			&perryQuery,
			&recordAttrs,
			NULL,			// No data
			record);
	}
	catch(...) {
		MSDebug("removeOutdatedPlugins: DataGetFirst threw");
		return;		// ???
	}
	if(record) {
		mSession.FreeUniqueRecord(mObjDbHand, *record);
	}
	if(resultHand) {
		if(theAttrs[0].NumberOfValues && theAttrs[1].NumberOfValues) {
			checkOutdatedPlugin(*theAttrs[1].Value, *theAttrs[0].Value, 
				tbdRecords);
		}
		else {
			MSDebug("removeOutdatedPlugins: incomplete record found (1)!");
		}
		for(unsigned dex=0; dex<2; dex++) {
			CSSM_DB_ATTRIBUTE_DATA *attr = &theAttrs[dex];
			for (unsigned attrDex=0; attrDex<attr->NumberOfValues; attrDex++) {
				if(attr->Value[attrDex].Data) {
					mSession.free(attr->Value[attrDex].Data);
				}
			}
			if(attr->Value) {
				mSession.free(attr->Value);
			}
		}
	}
	else {
		/* empty Object DB - we're done */
		MSDebug("removeOutdatedPlugins: empty object DB");
		return;
	}
	
	/* now the rest of the object DB records */
	for(;;) {
		bool brtn = mSession.DataGetNext(objDbHand(),
			resultHand, 
			&recordAttrs,
			NULL,
			record);
		if(!brtn) {
			/* end of data */
			break;
		}
		if(record) {
			mSession.FreeUniqueRecord(mObjDbHand, *record);
		}
		if(theAttrs[0].NumberOfValues && theAttrs[1].NumberOfValues) {
			checkOutdatedPlugin(*theAttrs[1].Value, 
				*theAttrs[0].Value, 
				tbdRecords);
		}
		else {
			MSDebug("removeOutdatedPlugins: incomplete record found (2)!");
		}
		for(unsigned dex=0; dex<2; dex++) {
			CSSM_DB_ATTRIBUTE_DATA *attr = &theAttrs[dex];
			for (unsigned attrDex=0; attrDex<attr->NumberOfValues; attrDex++) {
				if(attr->Value[attrDex].Data) {
					mSession.free(attr->Value[attrDex].Data);
				}
			}
			if(attr->Value) {
				mSession.free(attr->Value);
			}
		}
	}
	/* no DataAbortQuery needed; we scanned until completion */
	
	/*
	 * We have a vector of plugins to be deleted. Remove all records from both
	 * DBs associated with the plugins, as specified by guid.
	 */
	unsigned numRecords = tbdRecords.size();
	for(unsigned i=0; i<numRecords; i++) {
		TbdRecord *tbdRecord = tbdRecords[i];
		mSession.removeRecordsForGuid(tbdRecord->guid(), objDbHand());
		mSession.removeRecordsForGuid(tbdRecord->guid(), directDbHand());
	}
	for(unsigned i=0; i<numRecords; i++) {
		delete tbdRecords[i];
	}
}


/*
 * Update DBs for all bundles in specified directory.
 */
void MDSSession::DbFilesInfo::updateForBundleDir(
	const char *bundleDirPath)
{
	/* do this with readdir(); CFBundleCreateBundlesFromDirectory is
	 * much too heavyweight */
	MSDebug("...updating DBs for dir %s", bundleDirPath);
	DIR *dir = opendir(bundleDirPath);
	if (dir == NULL) {
		MSDebug("updateForBundleDir: error %d opening %s", errno, bundleDirPath);
		return;
	}
	struct dirent *dp;
	char fullPath[MAXPATHLEN];
	while ((dp = readdir(dir)) != NULL) {
		if(isBundle(dp)) {
			sprintf(fullPath, "%s/%s", bundleDirPath, dp->d_name);
			updateForBundle(fullPath);
		}
	}
	closedir(dir);
}

/*
 * lookup by path - just returns true if there is a record assoociated with the path
 * in mObjDbHand. 
 */
bool MDSSession::DbFilesInfo::lookupForPath(
	const char *path)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_HANDLE						resultHand = 0;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA			theAttr;
	CSSM_DB_ATTRIBUTE_INFO_PTR		attrInfo = &theAttr.Info;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DATA						predData;
	
	recordAttrs.DataRecordType = MDS_OBJECT_RECORDTYPE;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = 1;
	recordAttrs.AttributeData = &theAttr;
	
	attrInfo->AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attrInfo->Label.AttributeName = "Path";
	attrInfo->AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_STRING;
	
	theAttr.NumberOfValues = 0;
	theAttr.Value = NULL;
	
	predicate.DbOperator = CSSM_DB_EQUAL;
	predicate.Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = "Path";
	predicate.Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_STRING;
	predData.Data = (uint8 *)path;
	predData.Length = strlen(path);
	predicate.Attribute.Value = &predData;
	predicate.Attribute.NumberOfValues = 1;
	
	query.RecordType = MDS_OBJECT_RECORDTYPE;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	query.SelectionPredicate = &predicate;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;		// CSSM_QUERY_RETURN_DATA...FIXME - used?

	bool ourRtn = true;
	try {
		CssmQuery perryQuery(query);
		resultHand = mSession.DataGetFirst(objDbHand(),
			&perryQuery,
			&recordAttrs,
			NULL,			// No data
			record);
	}
	catch (...) {
		ourRtn = false;
	}
	if(record) {
		mSession.FreeUniqueRecord(mObjDbHand, *record);
	}
	else {
		ourRtn = false;
	}
	if(resultHand && ourRtn) {
		/* more resulting pending; terminate the search */
		try {
			mSession.DataAbortQuery(mObjDbHand, resultHand);
		}
		catch(...) {
			MSDebug("exception on DataAbortQuery in lookupForPath");
		}
	}
	for(unsigned dex=0; dex<theAttr.NumberOfValues; dex++) {
		if(theAttr.Value[dex].Data) {
			mSession.free(theAttr.Value[dex].Data);
		}
	}
	mSession.free(theAttr.Value);
	return ourRtn;
}

/* update entry for one bundle, which is known to exist */
void MDSSession::DbFilesInfo::updateForBundle(
	const char *bundlePath)
{
	MSDebug("...updating DBs for bundle %s", bundlePath);
	
	/* Quick lookup - do we have ANY entry for a bundle with this path? */
	if(lookupForPath(bundlePath)) {
		/* Yep, we're done */
		return;
	}
	MDSAttrParser parser(bundlePath,
		mSession,
		objDbHand(),
		directDbHand());
	try {
		parser.parseAttrs();
	}
	catch (const CssmError &err) {
		// a corrupt MDS info file invalidates the entire plugin
		const char *guid = parser.guid();
		if (guid) {
			Syslog::alert("Plugin (GUID %s) being unloaded from MDS", guid);
			mSession.removeRecordsForGuid(guid, objDbHand());
			mSession.removeRecordsForGuid(guid, directDbHand());
		}
	}
}


//
// Private API: add MDS records from contents of file
// These files are typically written by securityd and handed to us in this call.
//
void MDSSession::installFile(const MDS_InstallDefaults *defaults,
	const char *inBundlePath, const char *subdir, const char *file)
{
	string bundlePath = inBundlePath ? inBundlePath : cfString(CFBundleGetMainBundle());
	DbFilesInfo dbFiles(*this, MDS_SYSTEM_DB_DIR);
	MDSAttrParser parser(bundlePath.c_str(),
		*this,
		dbFiles.objDbHand(),
		dbFiles.directDbHand());
	parser.setDefaults(defaults);

	try {
		if (file == NULL)	// parse a directory
			if (subdir)		// a particular directory
				parser.parseAttrs(CFTempString(subdir));
			else			// all resources in bundle
				parser.parseAttrs(NULL);
		else				// parse just one file
			parser.parseFile(CFRef<CFURLRef>(makeCFURL(file)), CFTempString(subdir));
	}
	catch (const CssmError &err) {
		if (file != NULL) {
			Syslog::alert("Error parsing MDS info file %s/%s/%s", 
						  bundlePath.c_str(), subdir ? subdir : "", file);
		}
		const char *guid = parser.guid();
		if (guid) {
			Syslog::alert("Plugin (GUID %s) being unloaded from MDS", guid);
			removeRecordsForGuid(guid, dbFiles.objDbHand());
			removeRecordsForGuid(guid, dbFiles.directDbHand());
		}
	}
}


//
// Private API: Remove all records for a guid/subservice
//
// Note: Multicursors searching for SSID fail because not all records in the
// database have this attribute. So we have to explicitly run through all tables
// that do.
//
void MDSSession::removeSubservice(const char *guid, uint32 ssid)
{
	DbFilesInfo dbFiles(*this, MDS_SYSTEM_DB_DIR);

	CssmClient::Query query =
		Attribute("ModuleID") == guid &&
		Attribute("SSID") == ssid;

	// only CSP and DL tables are cleared here
	// (this function is private to securityd, which only handles those types)
	clearRecords(dbFiles.directDbHand(),
		CssmQuery(query.cssmQuery(), MDS_CDSADIR_CSP_PRIMARY_RECORDTYPE));
	clearRecords(dbFiles.directDbHand(),
		CssmQuery(query.cssmQuery(), MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE));
	clearRecords(dbFiles.directDbHand(),
		CssmQuery(query.cssmQuery(), MDS_CDSADIR_CSP_ENCAPSULATED_PRODUCT_RECORDTYPE));
	clearRecords(dbFiles.directDbHand(),
		CssmQuery(query.cssmQuery(), MDS_CDSADIR_CSP_SC_INFO_RECORDTYPE));
	clearRecords(dbFiles.directDbHand(),
		CssmQuery(query.cssmQuery(), MDS_CDSADIR_DL_PRIMARY_RECORDTYPE));
	clearRecords(dbFiles.directDbHand(),
		CssmQuery(query.cssmQuery(), MDS_CDSADIR_DL_ENCAPSULATED_PRODUCT_RECORDTYPE));
}


} // end namespace Security
