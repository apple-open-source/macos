/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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


#ifndef _MDSMODULE_H_
#define _MDSMODULE_H_  1

#include <security_filedb/AppleDatabase.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/timeflow.h>
#include <security_utilities/threading.h>
#include <sys/param.h>

namespace Security
{

class MDSModule
{
public:
    static MDSModule &get ();

    MDSModule ();
    ~MDSModule ();

    DatabaseManager 		&databaseManager () { return mDatabaseManager; }
	void					lastScanIsNow();
	double					timeSinceLastScan();
	void					getDbPath(char *path);
	void					setDbPath(const char *path);
	
	bool					serverMode() const	{ return mServerMode; }
	void					setServerMode();
	
private:
    static ModuleNexus<MDSModule> mModuleNexus;

    AppleDatabaseManager 	mDatabaseManager;
	
	/*
	 * Manipulated by MDSSession objects when they hold the system-wide per-user
	 * MDS file lock. mDbPath readable any time; it's protected process-wide
	 * by mDbPathLock.
	 */
	char 					mDbPath[MAXPATHLEN + 1];
	Time::Absolute			mLastScanTime;
	Mutex					mDbPathLock;
	bool					mServerMode;
};

} // end namespace Security

#endif // _MDSMODULE_H_
