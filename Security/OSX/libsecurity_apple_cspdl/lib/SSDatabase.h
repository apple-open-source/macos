/*
 * Copyright (c) 2000-2001,2011-2014 Apple Inc. All Rights Reserved.
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


//
// SSDatabase.h - Security Server database object
//
#ifndef	_H_SSDATABASE_
#define _H_SSDATABASE_

#include <security_cdsa_client/dlclient.h>
#include <security_utilities/unix++.h>
#include <securityd_client/ssclient.h>
#include <securityd_client/ssblob.h>
#include <security_utilities/CSPDLTransaction.h>

class SSCSPDLSession;
class SSUniqueRecord;

//
// Protected please ignore this class unless subclassing SSDatabase.
//
class SSDatabase;

class SSDatabaseImpl : public CssmClient::DbImpl
{
public:
	static const char *const DBBlobRelationName;
	static const CSSM_DB_RECORDTYPE DBBlobRelationID =
		CSSM_DB_RECORDTYPE_APP_DEFINED_START + 0x8000;

public:
	SSDatabaseImpl(SecurityServer::ClientSession &inClientSession,
				   const CssmClient::DL &dl,
				   const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation);
	virtual ~SSDatabaseImpl();

	void ssCreate(const DLDbIdentifier &dlDbIdentifier);
	void ssCreateWithBlob(const DLDbIdentifier &dlDbIdentifier, const CSSM_DATA &blob);
	void ssOpen(const DLDbIdentifier &dlDbIdentifier);
	SSUniqueRecord ssInsert(CSSM_DB_RECORDTYPE recordType,
						  const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
						  const CSSM_DATA *data);
	void authenticate(CSSM_DB_ACCESS_TYPE inAccessRequest,
						const CSSM_ACCESS_CREDENTIALS *inAccessCredentials);

	// Passthrough functions (only implemented by AppleCSPDL).
	void lock();
	void unlock();
	void unlock(const CSSM_DATA &password);
    void stash();
    void stashCheck();
	void getSettings(uint32 &outIdleTimeout, bool &outLockOnSleep);
	void setSettings(uint32 inIdleTimeout, bool inLockOnSleep);
	bool isLocked();
	void changePassphrase(const CSSM_ACCESS_CREDENTIALS *cred);
	void ssRecode(const CssmData &data, const CssmData &extraData);



    // Attempt to recode this database to the new blob version
    // Returns new version
    uint32 recodeDbToVersion(uint32 newBlobVersion);

    // Tell securityd that we're done with the upgrade operation
    void recodeFinished();

    // Try to take or release the file lock on the underlying database.
    // You _must_ call these as a pair. They start a transaction on the
    // underlying DL object, and that transaction is only finished when release
    // is called. Pass success=true if you want the transaction to commit; otherwise
    // it will roll back.
    void takeFileLock();
    void releaseFileLock(bool success);


	// DbUniqueRecordMaker
	CssmClient::DbUniqueRecordImpl *newDbUniqueRecord();

	// New methods not inherited from DbImpl
	SecurityServer::DbHandle dbHandle();

	void getRecordIdentifier(const CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord, CSSM_DATA &data);
	void ssCopyBlob(CSSM_DATA& blob);

    // Get the version of this database's encoding
    uint32 dbBlobVersion();

    // Try to make a backup copy of this database on the filesystem
    void makeBackup();

    // Try to make a backup copy of this database on the filesystem
    void makeCopy(const char* path);

    // Try to delete the backing file of this database
    // AFter you've done this, operations might fail in strange ways.
    void deleteFile();

    // Duplicate this database to this location, and return the clone.
    // For best results, use on an unlocked SSDatabase, but it should work on a locked one as well.
    SSDatabase ssCloneTo(const DLDbIdentifier& dldbidentifier);

protected:
	CssmClient::DbUniqueRecord getDbBlobId(CssmDataContainer *dbb = NULL);
	void commonCreate (const DLDbIdentifier &dlDbIdentifier, bool &autocommit);

    // Load the database from disk, but don't talk with securityd about it
    void load(const DLDbIdentifier &dlDbIdentifier);

    static uint32 getDbVersionFromBlob(const CssmData& dbb);
    uint32 recodeHelper(SecurityServer::DbHandle clonedDbHandle, CssmClient::DbUniqueRecord& dbBlobId);

private:
	// 5 minute default autolock time
 	static const uint32 kDefaultIdleTimeout = 5 * 60;
	static const uint8 kDefaultLockOnSleep = true;
	static const unsigned kNumIDWords = 4;

	DLDbIdentifier mIdentifier;
	UnixPlusPlus::ForkMonitor mForked;
	
	SecurityServer::ClientSession &mClientSession;
	SecurityServer::DbHandle mSSDbHandle;

    // Transaction for remembering if we've taken the file lock
    DLTransaction* mTransaction;
};


//
// SSDatabase --  A Security Server aware Db object.
//
class SSDatabase : public CssmClient::Db
{
public:
	typedef SSDatabaseImpl Impl;

	explicit SSDatabase(SSDatabaseImpl *impl) : CssmClient::Db(impl) {}
	SSDatabase() : CssmClient::Db(NULL) {}
	SSDatabase(SecurityServer::ClientSession &inClientSession,
			   const CssmClient::DL &dl,
			   const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation)
	: CssmClient::Db(new SSDatabaseImpl(inClientSession, dl, inDbName, inDbLocation)) {}

	SSDatabaseImpl *operator ->() const { return &impl<SSDatabaseImpl>(); }
	SSDatabaseImpl &operator *() const { return impl<SSDatabaseImpl>(); }

	// For convinience only
	SecurityServer::DbHandle dbHandle() { return (*this) ? (*this)->dbHandle() : SecurityServer::noDb; }
};


class SSUniqueRecordImpl : public CssmClient::DbUniqueRecordImpl
{
public:
	SSUniqueRecordImpl(const SSDatabase &db);
	virtual ~SSUniqueRecordImpl();

	SSDatabase database() const;
};


class SSUniqueRecord : public CssmClient::DbUniqueRecord
{
public:
	typedef SSUniqueRecordImpl Impl;

	explicit SSUniqueRecord(SSUniqueRecordImpl *impl) : CssmClient::DbUniqueRecord(impl) {}
	SSUniqueRecord() : CssmClient::DbUniqueRecord(NULL) {}
	SSUniqueRecord(const SSDatabase &db) : CssmClient::DbUniqueRecord(new SSUniqueRecordImpl(db)) {}

	SSUniqueRecordImpl *operator ->() const { return &impl<SSUniqueRecordImpl>(); }
	SSUniqueRecordImpl &operator *() const { return impl<SSUniqueRecordImpl>(); }
};


#endif	// _H_SSDATABASE_
