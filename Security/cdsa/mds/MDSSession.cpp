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

#include <Security/DbContext.h>
#include "MDSModule.h"
#include "MDSAttrParser.h"
#include "MDSAttrUtils.h"

#include <memory>
#include <Security/cssmerr.h>
#include <Security/utilities.h>
#include <Security/logging.h>
#include <Security/debugging.h>
#include <Security/mds_schema.h>

#include <sys/types.h>
#include <sys/param.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

/* 
 * The layout of the various MDS DB files on disk is as follows:
 *
 * /var/tmp/mds				-- owner = root, mode = 01777, world writable, sticky
 *     mdsObject.db			-- owner = root, mode = 0644, object DB
 *     mdsDirectory.db		-- owner = root, mode = 0644, MDS directory DB
 *	   mds.lock             -- temporary, owner = root, protects creation of 
 *							   previous two files
 *     <uid>/				-- owner = <uid>, mode = 0644
 *     	  mdsObject.db		-- owner = <uid>, mode = 0644, object DB
 *        mdsDirectory.db	-- owner = <uid>, mode = 0644, MDS directory DB
 *	   		  mds.lock      -- temporary, owner = <uid>, protects creation of 
 *							   previous two files
 * 
 * The /var/tmp/mds directory and the two db files in it are created by root
 * via SS or an AEWP call. Each user except for root has their own private
 * directory with two DB files and a lock. The first time a user accesses MDS,
 * the per-user directory is created and the per-user DB files are created as 
 * copies of the system DB files. Fcntl() with a F_RDLCK is used to lock the system
 * DB files when they are the source of these copies; this is the same mechanism
 * used by the underlying AtomincFile. 
 *
 * The sticky bit in /var/tmp/mds ensures that users cannot delete, rename, and/or
 * replace the root-owned DB files in that directory, and that users can not 
 * modify other user's private MDS directories. 
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
 * Location of system MDS database and lock files.
 */
#define MDS_SYSTEM_DB_DIR 	"/private/var/tmp/mds"
#define MDS_LOCK_FILE_NAME	"mds.lock"
#define MDS_OBJECT_DB_NAME	"mdsObject.db"
#define MDS_DIRECT_DB_NAME	"mdsDirectory.db"
#define MDS_LOCK_FILE_PATH	MDS_SYSTEM_DB_DIR "/" MDS_LOCK_FILE_NAME
#define MDS_OBJECT_DB_PATH	MDS_SYSTEM_DB_DIR "/" MDS_OBJECT_DB_NAME
#define MDS_DIRECT_DB_PATH	MDS_SYSTEM_DB_DIR "/" MDS_DIRECT_DB_NAME

/*
 * Location of per-user bundles, relative to home directory.
 * PEr-user DB files are in MDS_SYSTEM_DB_DIR/<uid>/. 
 */
#define MDS_USER_DB_DIR		"Library/Security"
#define MDS_USER_BUNDLE		"Library/Security"

/* time to wait in ms trying to acquire lock */
#define DB_LOCK_TIMEOUT		(2 * 1000)

/* Minimum interval, in seconds, between rescans for plugin changes */
#define MDS_SCAN_INTERVAL 	10

/* initial debug - start from scratch each time */
#define START_FROM_SCRATCH	0

/* debug - skip file-level locking */
#define SKIP_FILE_LOCKING	0

/* Only allow root to create and update system DB files - in the final config this
 * will be true */
#define SYSTEM_MDS_ROOT_ONLY	0

/*
 * Early development; no Security Server/root involvement with system DB creation.
 * If this is true, SYSTEM_MDS_ROOT_ONLY must be false (though both can be
 * false for intermediate testing).
 */ 
#define SYSTEM_DBS_VIA_USER		1
		
/* when true, turn autocommit off when building system DB */
#define AUTO_COMMIT_OPT			1


/*
 * Determine if both of the specified DB files exist as
 * accessible regular files. Returns true if they do. If the purge argument
 * is true, we'll ensure that either both or neither of the files exist on
 * exit.
 */
static bool doFilesExist(
	const char *objDbFile,
	const char *directDbFile,
	bool purge)					// false means "passive" check 
{
	struct stat sb;
	bool objectExist = false;
	bool directExist = false;
	
	if (stat(objDbFile, &sb) == 0) {
		/* Object DB exists */
		if(!(sb.st_mode & S_IFREG)) {
			MSDebug("deleting non-regular file %s", objDbFile);
			if(purge && unlink(objDbFile)) {
				MSDebug("unlink(%s) returned %d", objDbFile, errno);
				CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
			}
		}
		else {
			objectExist = true;
		}
	}
	if (stat(directDbFile, &sb) == 0) {
		/* directory DB exists */
		if(!(sb.st_mode & S_IFREG)) {
			MSDebug("deleting non-regular file %s", directDbFile);
			if(purge & unlink(directDbFile)) {
				MSDebug("unlink(%s) returned %d", directDbFile, errno);
				CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
			}
		}
		directExist = true;
	}
	if(objectExist && directExist) {
		/* both databases exist as regular files */
		return true;
	}
	else if(!purge) {
		return false;
	}
	
	/* at least one does not exist - ensure neither of them do */
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
 * Determine if specified directory exists. 
 */
static bool doesDirectExist(
	const char *dirPath)
{
	struct stat sb;
	
	if (stat(dirPath, &sb)) {
		return false;
	}
	if(!(sb.st_mode & S_IFDIR)) {
		return false;
	}
	return true;
}

/*
 * Create specified directory if it doesn't already exist. 
 * Zero for mode means "use the default provided by 0755 modified by umask".
 */
static int createDir(
	const char *dirPath,
	mode_t dirMode = 0)
{
	if(doesDirectExist(dirPath)) {
		return 0;
	}
	int rtn = mkdir(dirPath, 0755);
	if(rtn) {
		if(errno == EEXIST) {
			/* this one's OK */
			rtn = 0;
		}
		else {
			rtn = errno;
			MSDebug("mkdir(%s) returned  %d", dirPath, errno);
		}
	}
	if((rtn == 0) && (dirMode != 0)) {
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
	mModule(MDSModule::get()),
	mLockFd(-1)
{
	MSDebug("MDSSession::MDSSession");
		
	#if START_FROM_SCRATCH
	unlink(MDS_LOCK_FILE_PATH);
	unlink(MDS_OBJECT_DB_PATH);
	unlink(MDS_DIRECT_DB_PATH);	
	#endif
	
    mCallerGuidPresent =  inCallerGuid != nil;
    if (mCallerGuidPresent)
        mCallerGuid = *inCallerGuid;
	
	/*
	 * Create DB files if necessary; make sure they are up-to-date
	 */
	// no! done in either install or open! updateDataBases();
}

MDSSession::~MDSSession ()
{
	MSDebug("MDSSession::~MDSSession");
	releaseLock(mLockFd);
}

void
MDSSession::terminate ()
{
	MSDebug("MDSSession::terminate");
	releaseLock(mLockFd);
    closeAll();
}

/*
 * Called by security server or AEWP-executed privileged tool.
 */
void
MDSSession::install ()
{
	if((getuid() != (uid_t)0) && SYSTEM_MDS_ROOT_ONLY) {
		CssmError::throwMe(CSSMERR_DL_OS_ACCESS_DENIED);
	}
	
	int sysFdLock = -1;
	try {
		/* before we obtain the lock, ensure the the system MDS DB directory exists */
		if(createDir(MDS_SYSTEM_DB_DIR, 01777)) {
			MSDebug("Error creating system MDS dir; aborting.");
			CssmError::throwMe(CSSMERR_DL_OS_ACCESS_DENIED);
		}

		if(!obtainLock(MDS_LOCK_FILE_PATH, sysFdLock, DB_LOCK_TIMEOUT)) {
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}
		if(!systemDatabasesPresent(true)) {
			bool created = createSystemDatabases();
			if(created) {
				/* 
				 * Skip possible race condition in which this is called twice,
				 * both via SS by user procs who say "no system DBs present"
				 * in their updateDataBases() method. 
				 *
				 * Do initial population of system DBs.
				 */
				DbFilesInfo dbFiles(*this, MDS_SYSTEM_DB_DIR);
				#if 	AUTO_COMMIT_OPT
				dbFiles.autoCommit(CSSM_FALSE);
				#endif
				dbFiles.updateSystemDbInfo(MDS_SYSTEM_PATH, MDS_BUNDLE_PATH);
			}
		}
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
 *
 * FIXME: both of these dbOpen routines leak like crazy even though
 * we know we close properly. 
 * Typical stack trace (from MallocDebug) of a leak is
 *
 *	DatabaseSession::DbOpen(char const *, cssm_net_address const...)
 *	DatabaseManager::dbOpen(Security::DatabaseSession &, ...)
 *	Database::_dbOpen(Security::DatabaseSession &, unsigned long, ...)
 *	AppleDatabase::dbOpen(Security::DbContext &)
 *	DbModifier::openDatabase(void)
 *	DbModifier::getDbVersion(void) 
 *	DbVersion::DbVersion(Security::AtomicFile &, ...)
 *	DbVersion::open(void) 
 *	MetaRecord::unpackRecord(Security::ReadSection const &, ...)
 *	MetaRecord::unpackAttribute(Security::ReadSection const &, ...)
 *	MetaAttribute::unpackAttribute(Security::ReadSection const &, ..)
 *	TypedMetaAttribute<Security::StringValue>::unpackValue(...)
 *	TrackingAllocator::malloc(unsigned long) 
 */
CSSM_DB_HANDLE MDSSession::dbOpen(
	const char *dbName)
{
	MSDebug("Opening %s", dbName);
	CSSM_DB_HANDLE dbHand;
	DatabaseSession::DbOpen(dbName,
		NULL,				// DbLocation
		CSSM_DB_ACCESS_READ,
		NULL,				// AccessCred - hopefully optional 
		NULL,				// OpenParameters
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
	DatabaseSession::DbOpen(fullPath, DbLocation, AccessRequest, AccessCred,
		OpenParameters, DbHandle);
}

void
MDSSession::GetDbNames(CSSM_NAME_LIST_PTR &outNameList)
{
	outNameList = new CSSM_NAME_LIST[1];
	outNameList->NumStrings = 2;
	outNameList->String = new (char *)[2];
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
	const char *lockFile,	// e.g. MDS_LOCK_FILE_PATH
	int &fd,				// IN/OUT
	int timeout)			// default 0
{
	#if	SKIP_FILE_LOCKING
	return true;
	#else
	
	static const int kRetryDelay = 250; // ms
	
	fd = open(MDS_LOCK_FILE_PATH, O_CREAT | O_EXCL, 0544);
	while (fd == -1 && timeout >= kRetryDelay) {
		timeout -= kRetryDelay;
		usleep(1000 * kRetryDelay);
		mLockFd = open(MDS_LOCK_FILE_PATH, O_CREAT | O_EXCL, 0544);
	}
	
	return (fd != -1);
	#endif	/* SKIP_FILE_LOCKING */
}

//
// Release the exclusive lock over the MDS databases. If this session
// does not hold the lock, this method does nothing.
//

void
MDSSession::releaseLock(int &fd)
{
	#if !SKIP_FILE_LOCKING
	if (fd != -1) {
		close(fd);
		unlink(MDS_LOCK_FILE_PATH);
		fd = -1;
	}
	#endif
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

/* Single file copy with locking */
static void safeCopyFile(
	const char *fromPath,
	const char *toPath)
{
	/* open source for reading */
	int srcFd = open(fromPath, O_RDONLY, 0);
	if(srcFd < 0) {
		/* FIXME - what error would we see if the file is locked for writing
		 * by someone else? We definitely have to handle that. */
		int error = errno;
		MSDebug("Error %d opening system DB file %s\n", error, fromPath);
		UnixError::throwMe(error);
	}
	
	/* acquire the same kind of lock AtomicFile uses */
	struct flock fl;
	fl.l_start = 0;
	fl.l_len = 1;
	fl.l_pid = getpid();
	fl.l_type = F_RDLCK;		// AtomicFile gets F_WRLCK
	fl.l_whence = SEEK_SET;

	// Keep trying to obtain the lock if we get interupted.
	for (;;) {
		if (::fcntl(srcFd, F_SETLKW, reinterpret_cast<int>(&fl)) == -1) {
			int error = errno;
			if (error == EINTR) {
				continue;
			}
			MSDebug("Error %d locking system DB file %s\n", error, fromPath);
			UnixError::throwMe(error);
		}
		else {
			break;
		}
	}

	/* create destination */
	int destFd = open(toPath, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC | O_EXCL, 0644);
	if(destFd < 0) {
		int error = errno;
		MSDebug("Error %d opening user DB file %s\n", error, toPath);
		UnixError::throwMe(error);
	}
	
	/* copy */
	char buf[COPY_BUF_SIZE];
	while(1) {
		int bytesRead = read(srcFd, buf, COPY_BUF_SIZE);
		if(bytesRead == 0) {
			break;
		}
		if(bytesRead < 0) {
			int error = errno;
			MSDebug("Error %d reading system DB file %s\n", error, fromPath);
			UnixError::throwMe(error);
		}
		int bytesWritten = write(destFd, buf, bytesRead);
		if(bytesWritten < 0) {
			int error = errno;
			MSDebug("Error %d writing user DB file %s\n", error, toPath);
			UnixError::throwMe(error);
		}
	}
	
	/* unlock source and close both */
	fl.l_type = F_UNLCK;
	if (::fcntl(srcFd, F_SETLK, reinterpret_cast<int>(&fl)) == -1) {
		MSDebug("Error %d unlocking system DB file %s\n", errno, fromPath);
	}
	close(srcFd);
	close(destFd);
}

/* Copy system DB files to specified user dir. */
static void copySystemDbs(
	const char *userDbFileDir)
{
	char toPath[MAXPATHLEN+1];
	
	sprintf(toPath, "%s/%s", userDbFileDir, MDS_OBJECT_DB_NAME);
	safeCopyFile(MDS_OBJECT_DB_PATH, toPath);
	sprintf(toPath, "%s/%s", userDbFileDir, MDS_DIRECT_DB_NAME);
	safeCopyFile(MDS_DIRECT_DB_PATH, toPath);
}

/*
 * Ensure current DB files exist and are up-to-date.
 * Called from MDSSession constructor and from DataGetFirst, DbOpen, and any
 * other public functions which access a DB from scratch.
 */
void MDSSession::updateDataBases()
{
	bool isRoot = (getuid() == (uid_t)0);
	bool createdSystemDb = false;
	
	/*
	 * The first thing we do is to ensure that system DBs are present.
	 * This call right here is the reason for the purge argument in 
	 * systemDatabasesPresent(); if we're a user proc, we can't grab the system
	 * MDS lock. 
	 */
	if(!systemDatabasesPresent(false)) {
		if(isRoot || SYSTEM_DBS_VIA_USER) {
			/* Either doing actual MDS op as root, or development case: 
			 * install as current user */
			install();
		}
		else {
			/* This path TBD; it involves either a SecurityServer RPC or
			 * a privileged tool exec'd via AEWP. */
			assert(0);
		}
		/* remember this - we have to delete possible existing user DBs */
		createdSystemDb = true;
	}
	
	/* if we scanned recently, we're done */
	double delta = mModule.timeSinceLastScan();
	if(delta < (double)MDS_SCAN_INTERVAL) {
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
	
	if(isRoot) {
		strcat(userDbFileDir, MDS_SYSTEM_DB_DIR);
		/* no userBundlePath */
	}
	else {
		char *userHome = getenv("HOME");
		if(userHome == NULL) {
			/* FIXME - what now, batman? */
			MSDebug("updateDataBases: no HOME");
			userHome = "/";
		}
		sprintf(userBundlePath, "%s/%s", userHome, MDS_USER_BUNDLE);
		
		/* DBs go in a per-UID directory in the system MDS DB directory */
		sprintf(userDbFileDir, "%s/%d", MDS_SYSTEM_DB_DIR, (int)(getuid()));
	}
	sprintf(userObjDbFilePath,    "%s/%s", userDbFileDir, MDS_OBJECT_DB_NAME);
	sprintf(userDirectDbFilePath, "%s/%s", userDbFileDir, MDS_DIRECT_DB_NAME);
	sprintf(userDbLockPath,       "%s/%s", userDbFileDir, MDS_LOCK_FILE_NAME);
	
	/* 
	 * Create the per-user directory first...that's where the lock we'll be using
	 * lives. Our createDir() is tolerant of EEXIST errors. 
	 */
	if(!isRoot) {
		if(createDir(userDbFileDir)) {
			/* We'll just have to limp along using the read-only system DBs */
			Syslog::alert("Error creating %s", userDbFileDir);
			MSDebug("Error creating user DBs; using system DBs");
			mModule.setDbPath(MDS_SYSTEM_DB_DIR);
			return;
		}
	}

	/* always release mLockFd no matter what happens */
	if(!obtainLock(userDbLockPath, mLockFd, DB_LOCK_TIMEOUT)) {
		CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
	}
	try {
		if(!isRoot) {
			if(createdSystemDb) {
				/* initial creation of system DBs by user - start from scratch */
				unlink(userObjDbFilePath);
				unlink(userDirectDbFilePath);
			}
		
			/*
			 * System DBs exist and are as up-to-date as we are allowed to make them. 
			 * Create per-user DBs if they don't exist.
			 */
			if(createdSystemDb || 		// optimization - if this is true, the
										// per-user DBs do not exist since we just
										// deleted them
				!doFilesExist(userObjDbFilePath, userDirectDbFilePath,
					true)) {
			
				/* copy system DBs to user DBs */
				MSDebug("copying system DBs to user at %s", userDbFileDir);
				copySystemDbs(userDbFileDir);
			}
			else {
				MSDebug("Using existing user DBs at %s", userDbFileDir);
			}
		}
		else {
			MSDebug("Using system DBs only");
		}
		
		/* 
		 * Update per-user DBs from all three sources (System.framework, 
		 * System bundles, user bundles) as appropriate. Note that if we
		 * just created the system DBs, we don't have to update with
		 * respect to system framework or system bundles. 
		 */
		DbFilesInfo dbFiles(*this, userDbFileDir);
		if(!createdSystemDb) {
			dbFiles.removeOutdatedPlugins();
			dbFiles.updateSystemDbInfo(MDS_SYSTEM_PATH, MDS_BUNDLE_PATH);
		}
		if(!isRoot) {
			/* root doesn't have user bundles */
			if(checkUserBundles(userBundlePath)) {
				dbFiles.updateForBundleDir(userBundlePath);
			}
		}
		mModule.setDbPath(userDbFileDir);
	}	/* main block protected by mLockFd */
	catch(...) {
		releaseLock(mLockFd);
		throw;
	}
	mModule.lastScanIsNow();
	releaseLock(mLockFd);
}

/*
 * Remove all records with specified guid (a.k.a. ModuleID) from specified DB.
 */
void MDSSession::removeRecordsForGuid(
	const char *guid,
	CSSM_DB_HANDLE dbHand)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_HANDLE						resultHand;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DATA						predData;
	
	/* don't want any attributes back, just a record ptr */
	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_ANY;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = 0;
	recordAttrs.AttributeData = NULL;
	
	/* one predicate, == guid */
	predicate.DbOperator = CSSM_DB_EQUAL;
	predicate.Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = "ModuleID";
	predicate.Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_STRING;
	predData.Data = (uint8 *)guid;
	predData.Length = strlen(guid) + 1;
	predicate.Attribute.Value = &predData;
	predicate.Attribute.NumberOfValues = 1;
	
	query.RecordType = CSSM_DL_DB_RECORD_ANY;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	query.SelectionPredicate = &predicate;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;		// CSSM_QUERY_RETURN_DATA...FIXME - used?

	/* 
	 * Each search starts from scratch - not sure if we can delete a record
	 * associated with an active query and continue on with that query. 
	 */
	try {
		for(;;) {
			DLQuery perryQuery(query);
			resultHand = DataGetFirst(dbHand,
				&perryQuery,
				&recordAttrs,
				NULL,			// No data
				record);
			if(resultHand) {
				try {
					MSDebug("...deleting a record for guid %s", guid);
					DataDelete(dbHand, *record);
					DataAbortQuery(dbHand, resultHand);
				}
				catch(...) {
					MSDebug("exception (1) while deleting record for guid %s", guid);
					/* proceed.... */
				}
			}
			else if(record) {
				FreeUniqueRecord(dbHand, *record);
				break;
			}
		}	/* main loop */
	}
	catch (...) {
		MSDebug("exception (2) while deleting record for guid %s", guid);
	}
}

/*
 * Determine if system databases are present. 
 * If the purge argument is true, we'll ensure that either both or neither 
 * DB files exist on exit; in that case caller need to hold MDS_LOCK_FILE_PATH.
 */
bool MDSSession::systemDatabasesPresent(bool purge)
{
	bool rtn = false;
	
	try {
		/* this can throw on a failed attempt to delete sole existing file */
		if(doFilesExist(MDS_OBJECT_DB_PATH, MDS_DIRECT_DB_PATH, purge)) {
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

	try {
		DbCreate(dbName,
			NULL,			// DbLocation
			*dbInfoP,
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL,			// CredAndAclEntry
			NULL,			// OpenParameters
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
 * MDS_LOCK_FILE_PATH held on entry and exit. MDS_SYSTEM_DB_DIR assumed to
 * exist (that's our caller's job, before acquiring MDS_LOCK_FILE_PATH). 
 * Returns true if we actually built the files, false if they already 
 * existed.
 */
bool MDSSession::createSystemDatabases()
{
	CSSM_DB_HANDLE objectDbHand = 0;
	CSSM_DB_HANDLE directoryDbHand = 0;
	
	assert((getuid() == (uid_t)0) || !SYSTEM_MDS_ROOT_ONLY);
	if(systemDatabasesPresent(true)) {
		/* both databases exist as regular files - we're done */
		MSDebug("system DBs already exist");
		return false;
	}

	/* create two DBs - any exception here results in deleting both of them */
	MSDebug("Creating MDS DBs");
	try {
		createSystemDatabase(MDS_OBJECT_DB_PATH, &kObjectRelation, 1, objectDbHand);
		DbClose(objectDbHand);
		objectDbHand = 0;
		createSystemDatabase(MDS_DIRECT_DB_PATH, kMDSRelationInfo, kNumMdsRelations,
			directoryDbHand);
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
	int rtn = ::stat(path, &sb);
	if(rtn) {
		int error = errno;
		MSDebug("Error %d statting DB file %s", error, path);
		UnixError::throwMe(error);
	}
	mLaterTimestamp = sb.st_mtimespec.tv_sec;
	sprintf(path, "%s/%s", mDbPath, MDS_DIRECT_DB_NAME);
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

#define AUTO_COMMIT_OFF_ON_CLOSE	1

MDSSession::DbFilesInfo::~DbFilesInfo()
{
	if(mObjDbHand != 0) {
		#if AUTO_COMMIT_OPT && AUTO_COMMIT_OFF_ON_CLOSE
		mSession.PassThrough(mObjDbHand,
			CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
			reinterpret_cast<void *>(CSSM_TRUE),
			NULL);
		#endif
		mSession.DbClose(mObjDbHand);
		mObjDbHand = 0;
	}
	if(mDirectDbHand != 0) {
		#if AUTO_COMMIT_OPT && AUTO_COMMIT_OFF_ON_CLOSE
		mSession.PassThrough(mDirectDbHand,
			CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
			reinterpret_cast<void *>(CSSM_TRUE),
			NULL);
		#endif
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
	mObjDbHand = mSession.dbOpen(fullPath);
	return mObjDbHand;
}

CSSM_DB_HANDLE MDSSession::DbFilesInfo::directDbHand()
{
	if(mDirectDbHand != 0) {
		return mDirectDbHand;
	}
	char fullPath[MAXPATHLEN + 1];
	sprintf(fullPath, "%s/%s", mDbPath, MDS_DIRECT_DB_NAME);
	mDirectDbHand = mSession.dbOpen(fullPath);
	return mDirectDbHand;
}

/*
 * Update the info for System.framework and the system bundles.
 */
void MDSSession::DbFilesInfo::updateSystemDbInfo(
	const char *systemPath,		// e.g., /System/Library/Frameworks
	const char *bundlePath)		// e.g., /System/Library/Security
{
	/* System.framework - CSSM and built-in modules */
	char fullPath[MAXPATHLEN];
	sprintf(fullPath, "%s/%s", systemPath, MDS_SYSTEM_FRAME);
	updateForBundle(fullPath);
	
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
	int rtn = ::stat((char *)pathValue.Data, &sb);
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
		MSDebug("checkOutdatedPlugin: flagging %s obsolete", pathValue.Data);
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

	DLQuery perryQuery(query);
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
			if(theAttrs[dex].Value) {
				if(theAttrs[dex].Value->Data) {
					mSession.free(theAttrs[dex].Value->Data);
				}
				mSession.free(theAttrs[dex].Value);
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
			if(theAttrs[dex].Value) {
				if(theAttrs[dex].Value->Data) {
					mSession.free(theAttrs[dex].Value->Data);
				}
				mSession.free(theAttrs[dex].Value);
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
	predData.Length = strlen(path) + 1;
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
		DLQuery perryQuery(query);
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
	if(theAttr.Value) {
		if(theAttr.Value->Data) {
			mSession.free(theAttr.Value->Data);
		}
		mSession.free(theAttr.Value);
	}
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
	parser.parseAttrs();
}

/* DB autocommit on/off */
void MDSSession::DbFilesInfo::autoCommit(CSSM_BOOL val)
{
	try {
		mSession.PassThrough(objDbHand(),
			CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
			reinterpret_cast<void *>(val),
			NULL);
		mSession.PassThrough(directDbHand(),
			CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
			reinterpret_cast<void *>(val),
			NULL);
	}
	catch (...) {
		MSDebug("DbFilesInfo::autoCommit error!");
		/* but proceed */
	}
}


} // end namespace Security
