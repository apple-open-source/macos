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


#ifndef _MDSSESSION_H_
#define _MDSSESSION_H_  1

#include <Security/DatabaseSession.h>
#include <Security/handleobject.h>
#include <Security/mds.h>
#include <Security/MDSModule.h>
#include <Security/MDSSchema.h>
#include <map>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <list>

namespace Security
{

class MDSSession: public DatabaseSession, public HandleObject
{
    NOCOPY(MDSSession)
public:
    MDSSession (const Guid *inCallerGuid,
                const CSSM_MEMORY_FUNCS &inMemoryFunctions);
    virtual ~MDSSession ();

    void terminate ();
    void install ();
    void uninstall ();

	CSSM_DB_HANDLE MDSSession::dbOpen(
		const char *dbName);
		
	// some DatabaseSession routines we need to override
	void DbOpen(const char *DbName,
			const CSSM_NET_ADDRESS *DbLocation,
			CSSM_DB_ACCESS_TYPE AccessRequest,
			const AccessCredentials *AccessCred,
			const void *OpenParameters,
			CSSM_DB_HANDLE &DbHandle);
    void GetDbNames(CSSM_NAME_LIST_PTR &NameList);
    void FreeNameList(CSSM_NAME_LIST &NameList);
    void GetDbNameFromHandle(CSSM_DB_HANDLE DBHandle,
			char **DbName);

    // implement CssmHeap::Allocator
    void *malloc(size_t size) throw(std::bad_alloc)
	{ return mCssmMemoryFunctions.malloc(size); }
    void free(void *addr) throw()
	{ mCssmMemoryFunctions.free(addr); }
	void *realloc(void *addr, size_t size) throw(std::bad_alloc)
	{ return mCssmMemoryFunctions.realloc(addr, size); }

	MDSModule		&module() 	{ return mModule; }
	void removeRecordsForGuid(
		const char *guid,
		CSSM_DB_HANDLE dbHand);

	
	/* 
	 * represents two DB files in any location and state
	 */
	class DbFilesInfo
	{
	public:
		DbFilesInfo(MDSSession &session, const char *dbPath);
		~DbFilesInfo();
		/* these three may not be needed */
		CSSM_DB_HANDLE objDbHand();
		CSSM_DB_HANDLE directDbHand();
		time_t laterTimestamp()			{ return mLaterTimestamp; }

		/* public functions used by MDSSession */
		void updateSystemDbInfo(
			const char *systemPath,			// e.g., /System/Library/Frameworks
			const char *bundlePath);		// e.g., /System/Library/Security
		void removeOutdatedPlugins();
		void updateForBundleDir(
			const char *bundleDirPath);
		void updateForBundle(
			const char *bundlePath);
		void autoCommit(CSSM_BOOL val);		// DB autocommit on/off 
	private:
		bool lookupForPath(
			const char *path);

		/* object and list to keep track of "to be deleted" records */
		#define MAX_GUID_LEN	64		/* normally 37 */
		class TbdRecord
		{
		public:
			TbdRecord(const CSSM_DATA &guid);
			~TbdRecord() 		{ } 
			const char *guid() 	{ return mGuid; }
		private:
			char mGuid[MAX_GUID_LEN];
		};
		typedef vector<TbdRecord *> TbdVector;

		void checkOutdatedPlugin(
			const CSSM_DATA &pathValue, 
			const CSSM_DATA &guidValue, 
			TbdVector &tbdVector);

		MDSSession &mSession;
		char mDbPath[MAXPATHLEN];
		CSSM_DB_HANDLE mObjDbHand;
		CSSM_DB_HANDLE mDirectDbHand;
		time_t mLaterTimestamp;
	};	/* DbFilesInfo */
private:
	bool obtainLock(
		const char *lockFile,
		int &fd, 
		int timeout = 0);
	void releaseLock(
		int &fd);
	
	/* given DB file name, fill in fully specified path */
	void dbFullPath(
		const char *dbName,
		char fullPath[MAXPATHLEN+1]);
	
	void updateDataBases();

	bool systemDatabasesPresent(bool purge);
	void createSystemDatabase(
		const char *dbName,
		const RelationInfo *relationInfo,
		unsigned numRelations,
		CSSM_BOOL autoCommit,
		mode_t mode,
		CSSM_DB_HANDLE &dbHand);		// RETURNED
	bool createSystemDatabases(
		CSSM_BOOL autoCommit,
		mode_t mode);

    const CssmMemoryFunctions mCssmMemoryFunctions;
    Guid 			mCallerGuid;
    bool 			mCallerGuidPresent;
	
	MDSModule		&mModule;
	int				mLockFd;		// per-user MDS DB lock
};

} // end namespace Security

#endif //_MDSSESSION_H_
