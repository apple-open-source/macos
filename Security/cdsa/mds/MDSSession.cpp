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

#include <memory>
#include <Security/cssmerr.h>
#include <Security/utilities.h>
#include <Security/logging.h>

#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>

// Location of security plugins.

#define kPluginPath "/System/Library/Security/"

// Location of MDS database and lock files.

#define kDatabasePath "/var/tmp/"
#define kLockFilename kDatabasePath "mds.lock"

// Minimum interval, in seconds, between rescans for plugin changes.

#define kScanInterval 10

//
// Get the current time in a format that matches that in which
// a file's modification time is expressed.
//

static void
getCurrentTime(struct timespec &now)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	TIMEVAL_TO_TIMESPEC(&tv, &now);
}

//
// Create an MDS session.
//

MDSSession::MDSSession (const Guid *inCallerGuid,
                        const CSSM_MEMORY_FUNCS &inMemoryFunctions) :
    DatabaseSession(MDSModule::get().databaseManager()),
    mCssmMemoryFunctions (inMemoryFunctions),
	mLockFd(-1)
{
	fprintf(stderr, "MDSSession::MDSSession\n");
		
    mCallerGuidPresent =  inCallerGuid != nil;
    if (mCallerGuidPresent)
        mCallerGuid = *inCallerGuid;
		
	// make sure the MDS databases have been created, and the required
	// tables have been constructed
	initializeDatabases();
	
	// schedule a scan for plugin changes
	getCurrentTime(mLastScanTime);
}

MDSSession::~MDSSession ()
{
	fprintf(stderr, "MDSSession::~MDSSession\n");
	releaseLock();
}

void
MDSSession::terminate ()
{
	fprintf(stderr, "MDSSession::terminate\n");

    closeAll();
}

//
// In this implementation, install() does nothing, since the databases
// are implicitly created as needed by initialize().
//

void
MDSSession::install ()
{
	// this space intentionally left blank
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

//
// Obtain and free a list of names of current databases.
//

void
MDSSession::GetDbNames(CSSM_NAME_LIST_PTR &outNameList)
{
	outNameList = mDatabaseManager.getDbNames(*this);
}

void
MDSSession::FreeNameList(CSSM_NAME_LIST &inNameList)
{
	mDatabaseManager.freeNameList(*this, inNameList);
}

//
// Scan the plugin directory.
//

static bool intervalHasElapsed(const struct timespec &then, const struct timespec &now,
	int intervalSeconds)
{
	return (now.tv_sec - then.tv_sec > intervalSeconds) ||
		((now.tv_sec - then.tv_sec == intervalSeconds) && (now.tv_nsec >= then.tv_nsec));
}

static bool operator <=(const struct timespec &a, const struct timespec &b)
{
	return (a.tv_sec < b.tv_sec) || ((a.tv_sec == b.tv_sec) && (a.tv_nsec <= b.tv_nsec));
}

class PluginInfo
{
public:
	PluginInfo(const char *pluginName, const struct timespec &modTime) : mModTime(modTime) {
		mPluginName = new char[strlen(pluginName) + 1];
		strcpy(mPluginName, pluginName);
	}

	~PluginInfo() { delete [] mPluginName; }
	
	const char *name() { return mPluginName; }
	const struct timespec &modTime() { return mModTime; }
	
private:
	char *mPluginName;
	struct timespec mModTime;
};

//
// Attempt to obtain an exclusive lock over the the MDS databases. The
// parameter is the maximum amount of time, in milliseconds, to spend
// trying to obtain the lock. A value of zero means to return failure
// right away if the lock cannot be obtained.
//

bool
MDSSession::obtainLock(int timeout /* = 0 */)
{
	static const int kRetryDelay = 250; // ms
	
	if (mLockFd >= 0)
		// this session already holds the lock
		return true;
		
	mLockFd = open(kLockFilename, O_CREAT | O_EXCL, 0544);
	while (mLockFd == -1 && timeout >= kRetryDelay) {
		timeout -= kRetryDelay;
		usleep(1000 * kRetryDelay);
		mLockFd = open(kLockFilename, O_CREAT | O_EXCL, 0544);
	}
	
	return (mLockFd != -1);
}

//
// Release the exclusive lock over the MDS databases. If this session
// does not hold the lock, this method does nothing.
//

void
MDSSession::releaseLock()
{
	if (mLockFd != -1) {
		close(mLockFd);
		unlink(kLockFilename);
		mLockFd = -1;
	}
}

//
// If necessary, create the two MDS databases and construct the required
// tables in each database.
//

void
MDSSession::initializeDatabases()
{
	printf("MDSSession::initializeDatabases\n");
	
	static int kLockTimeout = 2000; // ms
	
	// obtain an exclusive lock. in this case we really want the lock, so
	// if it's not immediately available we wait around for a bit
	
	if (!obtainLock(kLockTimeout))
		// something is wrong; either a stale lock file is lying around or
		// some other process is stuck updating the databases
		CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
	
	try {
		// check for the existence of the MDS database file; if it exists,
		// assume that the databases have already been properly created
	
		// look for added/removed/changed plugins
	
		scanPluginDirectory();
	}
	catch (...) {
		releaseLock();
		throw;
	}

	// release the exclusive lock
	
	releaseLock();
}

//
// Update the databases due to added/removed/changed plugins. This obtains
// an exclusive lock over the databases, if possible, and then scans the
// module path. If the lock cannot be obtained, it does nothing. The intent
// is that this will be called periodically, so a failure at any given time
// is not a big deal and may simply imply that another process is already
// updating the MDS databases.
//

void
MDSSession::updateDatabases()
{
	// get the current time in the appropriate format
	
	struct timespec now;
	getCurrentTime(now);
	
	if (!intervalHasElapsed(mLastScanTime, now, kScanInterval))
		// its not yet time to rescan
		return;
		
	// regardless of what happens, we don't want to scan again for a while, so reset
	// the last scan time before proceeding
	
	mLastScanTime = now;

	// obtain a lock to avoid having multiple processes scanning for changed plugins;
	// if the lock cannot be obtained immediately, just return and do nothing
	
	if (!obtainLock())
		return;

	// we want to make sure that the lock gets released at all costs, hence
	// this try block:
	
	try {
		scanPluginDirectory();
	}
	catch (...) {
		releaseLock();
		throw;
	}
	
	releaseLock();
}

//
// Determine if a filesystem object is a bundle that should be considered
// as a potential CDSA module by MDS.
//

static bool
isBundle(const char *path)
{
	static const char *bundleSuffix = ".bundle";

	int suffixLen = strlen(bundleSuffix);
	int len = strlen(path);
	
	return (len >= suffixLen) && !strcmp(path + len - suffixLen, bundleSuffix);
}

//
// Scan the module directory looking for added/removed/changed plugins, and
// update the MDS databases accordingly. This assumes that an exclusive lock
// has already been obtained, and that the databases and the required tables
// already exist.
//

void
MDSSession::scanPluginDirectory()
{
	printf("MDSSession::scanPluginDirectory\n");

	// check the modification time on the plugin directory: if it has not changed
	// since the last scan, we're done
	
	struct stat sb;
	if (stat(kPluginPath, &sb)) {
		// can't stat the plugin directory...
		Syslog::warning("MDS: cannot stat plugin directory \"%s\"", kPluginPath);
		return;
	}

	if (sb.st_mtimespec <= mLastScanTime)
		// no changes, we're done until its time for the next scan
		return;
	
	// attempt to open the plugin directory
	
	DIR *dir = opendir(kPluginPath);
	if (dir == NULL) {
		// no plugin directory, hence no modules. clear the MDS directory
		// and log a warning
		Syslog::warning("MDS: cannot open plugin directory \"%s\"", kPluginPath);
		return;
	}
	
	// build a list of the plugins are are currently in the directory, along with
	// their modification times
	
	struct dirent *dp;
	PluginInfoList pluginList;
	
	char tempPath[PATH_MAX];
	
	while ((dp = readdir(dir)) != NULL) {
	
		// stat the file to get its modification time
		
		strncpy(tempPath, kPluginPath, PATH_MAX);
		strncat(tempPath, dp->d_name, PATH_MAX - strlen(kPluginPath));
		
		struct stat sb;
		if (stat(tempPath, &sb) == 0) {
			// do some checking to determine that this path refers to an
			// actual bundle that is likely to be a module
			if (isBundle(tempPath))
				pluginList.push_back(new PluginInfo(tempPath, sb.st_mtimespec));
		}
	}
	
	closedir(dir);
	
	// step 1: for any plugin in the common relation which is no longer present,
	// or which is present but which has been modified since the last scan, remove
	// all its records from the MDS database
		
	removeOutdatedPlugins(pluginList);

	// step 2: for any plugin present but not in the common relation (note it may
	// have been removed in step 1 because it was out-of-date), insert its records
	// into the MDS database

	insertNewPlugins(pluginList);
	
	// free the list of current plugins
	
	for_each_delete(pluginList.begin(), pluginList.end());
}

void
MDSSession::removeOutdatedPlugins(const PluginInfoList &pluginList)
{
	PluginInfoList::const_iterator it;
	for (it = pluginList.begin(); it != pluginList.end(); it++)
		fprintf(stderr, "%s\n", (*it)->name());
}

void
MDSSession::insertNewPlugins(const PluginInfoList &pluginList)
{
}

