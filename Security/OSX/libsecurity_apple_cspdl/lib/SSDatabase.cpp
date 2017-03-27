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
// SSDatabase.cpp - Security Server database object
//
#include "SSDatabase.h"

#include <security_cdsa_utilities/KeySchema.h>
#include <security_utilities/CSPDLTransaction.h>
#include <Security/SecBasePriv.h>

using namespace CssmClient;
using namespace SecurityServer;

const char *const SSDatabaseImpl::DBBlobRelationName = "DBBlob";


SSDatabaseImpl::SSDatabaseImpl(ClientSession &inClientSession, const CssmClient::DL &dl,
							   const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation)
	: Db::Impl(dl, inDbName, inDbLocation), mClientSession(inClientSession), mSSDbHandle(noDb)
{
    mTransaction = NULL;
}

SSDatabaseImpl::~SSDatabaseImpl()
try
{
	if (mSSDbHandle != noDb)
		mClientSession.releaseDb(mSSDbHandle);
}
catch (...)
{
    return;	// Prevent re-throw of exception [function-try-block]
}

SSUniqueRecord
SSDatabaseImpl::ssInsert(CSSM_DB_RECORDTYPE recordType,
					   const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
					   const CSSM_DATA *data)
{
	SSUniqueRecord uniqueId(SSDatabase(this));
	check(CSSM_DL_DataInsert(handle(), recordType,
							 attributes,
							 data, uniqueId));
	// Activate uniqueId so CSSM_DL_FreeUniqueRecord() gets called when it goes out of scope.
	uniqueId->activate();
	return uniqueId;
}

void
SSDatabaseImpl::authenticate(CSSM_DB_ACCESS_TYPE inAccessRequest,
						const CSSM_ACCESS_CREDENTIALS *inAccessCredentials)
{
	mClientSession.authenticateDb(dbHandle(), inAccessRequest,
		AccessCredentials::overlay(inAccessCredentials));
}

void
SSDatabaseImpl::lock()
{
	mClientSession.lock(dbHandle());

}

void
SSDatabaseImpl::unlock()
{
	mClientSession.unlock(dbHandle());
}

void
SSDatabaseImpl::unlock(const CSSM_DATA &password)
{
	mClientSession.unlock(dbHandle(), CssmData::overlay(password));
}

void
SSDatabaseImpl::stash()
{
    mClientSession.stashDb(dbHandle());
}

void
SSDatabaseImpl::stashCheck()
{
    mClientSession.stashDbCheck(dbHandle());
}

void
SSDatabaseImpl::getSettings(uint32 &outIdleTimeout, bool &outLockOnSleep)
{
	DBParameters parameters;
	mClientSession.getDbParameters(dbHandle(), parameters);
	outIdleTimeout = parameters.idleTimeout;
	outLockOnSleep = parameters.lockOnSleep;
}

void
SSDatabaseImpl::setSettings(uint32 inIdleTimeout, bool inLockOnSleep)
{
	DBParameters parameters;
	parameters.idleTimeout = inIdleTimeout;
	parameters.lockOnSleep = inLockOnSleep;
	mClientSession.setDbParameters(dbHandle(), parameters);

	// Reencode the db blob.
	CssmDataContainer dbb(allocator());
	mClientSession.encodeDb(mSSDbHandle, dbb, allocator());
	getDbBlobId()->modify(DBBlobRelationID, NULL, &dbb, CSSM_DB_MODIFY_ATTRIBUTE_NONE);
}

bool
SSDatabaseImpl::isLocked()
{
	return mClientSession.isLocked(dbHandle());
}

void
SSDatabaseImpl::changePassphrase(const CSSM_ACCESS_CREDENTIALS *cred)
{
	mClientSession.changePassphrase(dbHandle(), AccessCredentials::overlay(cred));

	// Reencode the db blob.
	CssmDataContainer dbb(allocator());
	mClientSession.encodeDb(mSSDbHandle, dbb, allocator());
	getDbBlobId()->modify(DBBlobRelationID, NULL, &dbb, CSSM_DB_MODIFY_ATTRIBUTE_NONE);
}

DbHandle
SSDatabaseImpl::dbHandle()
{
	activate();
	if (mForked()) {
		// re-establish the dbHandle with the SecurityServer
		CssmDataContainer dbb(allocator());
		getDbBlobId(&dbb);
		mSSDbHandle = mClientSession.decodeDb(mIdentifier,
			AccessCredentials::overlay(accessCredentials()), dbb);
	}
	return mSSDbHandle;
}

void
SSDatabaseImpl::commonCreate(const DLDbIdentifier &dlDbIdentifier, bool &autoCommit)
{
	mIdentifier = dlDbIdentifier;
	// Set to false if autocommit should remain off after the create. 
	autoCommit = true;

	// OpenParameters to use
	CSSM_APPLEDL_OPEN_PARAMETERS newOpenParameters =
	{
		sizeof(CSSM_APPLEDL_OPEN_PARAMETERS),
		CSSM_APPLEDL_OPEN_PARAMETERS_VERSION,
		CSSM_FALSE,		// do not auto-commit
		0				// mask - do not use following fields
	};

	// Get the original openParameters and apply them to the ones we
	// are passing in.
	const CSSM_APPLEDL_OPEN_PARAMETERS *inOpenParameters =
		reinterpret_cast<const CSSM_APPLEDL_OPEN_PARAMETERS *>(openParameters());
	if (inOpenParameters)
	{
		switch (inOpenParameters->version)
		{
		case 1:
			if (inOpenParameters->length < sizeof(CSSM_APPLEDL_OPEN_PARAMETERS))
				CssmError::throwMe(CSSMERR_APPLEDL_INVALID_OPEN_PARAMETERS);

			newOpenParameters.mask = inOpenParameters->mask;
			newOpenParameters.mode = inOpenParameters->mode;
			/*DROPTHROUGH*/
		case 0:
			//if (inOpenParameters->length < sizeof(CSSM_APPLEDL_OPEN_PARAMETERS_V0))
			//	CssmError::throwMe(CSSMERR_APPLEDL_INVALID_OPEN_PARAMETERS);

			// This will determine whether we leave autocommit off or not.
			autoCommit = inOpenParameters->autoCommit == CSSM_FALSE ? false : true;
			break;

		default:
			CssmError::throwMe(CSSMERR_APPLEDL_INVALID_OPEN_PARAMETERS);
		}
	}

	// Use the new openParameters
	openParameters(&newOpenParameters);
	try
	{
		DbImpl::create();
		// Restore the original openparameters again. 
		openParameters(inOpenParameters);
	}
	catch (...)
	{
		// Make sure restore the original openparameters again even if
		// create throws. 
		openParameters(inOpenParameters);
		throw;
	}

	// @@@ The CSSM_DB_SCHEMA_ATTRIBUTE_INFO and CSSM_DB_SCHEMA_INDEX_INFO
	// arguments should be optional.
	createRelation(DBBlobRelationID, DBBlobRelationName,
				0, (CSSM_DB_SCHEMA_ATTRIBUTE_INFO *)42,
				0, (CSSM_DB_SCHEMA_INDEX_INFO *)42);

	// @@@ Only iff not already in mDbInfo
	createRelation(CSSM_DL_DB_RECORD_PUBLIC_KEY, "CSSM_DL_DB_RECORD_PUBLIC_KEY",
				KeySchema::KeySchemaAttributeCount, KeySchema::KeySchemaAttributeList,
				KeySchema::KeySchemaIndexCount, KeySchema::KeySchemaIndexList);

	// @@@ Only iff not already in mDbInfo
	createRelation(CSSM_DL_DB_RECORD_PRIVATE_KEY, "CSSM_DL_DB_RECORD_PRIVATE_KEY",
				KeySchema::KeySchemaAttributeCount, KeySchema::KeySchemaAttributeList,
				KeySchema::KeySchemaIndexCount, KeySchema::KeySchemaIndexList);

	// @@@ Only iff not already in mDbInfo
	createRelation(CSSM_DL_DB_RECORD_SYMMETRIC_KEY, "CSSM_DL_DB_RECORD_SYMMETRIC_KEY",
				KeySchema::KeySchemaAttributeCount, KeySchema::KeySchemaAttributeList,
				KeySchema::KeySchemaIndexCount, KeySchema::KeySchemaIndexList);
}

void
SSDatabaseImpl::ssCreate(const DLDbIdentifier &dlDbIdentifier)
{
	try
	{
		bool autoCommit;
		commonCreate(dlDbIdentifier, autoCommit);
		
		DBParameters dbParameters;
		memset(&dbParameters, 0, sizeof(DBParameters));
		dbParameters.idleTimeout = kDefaultIdleTimeout;
		dbParameters.lockOnSleep = kDefaultLockOnSleep;

		const AccessCredentials *cred = NULL;
		const AclEntryInput *owner = NULL;
		if (resourceControlContext())
		{
			cred = AccessCredentials::overlay(resourceControlContext()->AccessCred);
			owner = &AclEntryInput::overlay(resourceControlContext()->InitialAclEntry);
		}
		mSSDbHandle = mClientSession.createDb(dlDbIdentifier, cred, owner, dbParameters);
		CssmDataContainer dbb(allocator());
		mClientSession.encodeDb(mSSDbHandle, dbb, allocator());
        secnotice("integrity", "opening %s", name());
		Db::Impl::insert(DBBlobRelationID, NULL, &dbb);
		if (autoCommit)
		{
			passThrough(CSSM_APPLEFILEDL_COMMIT, NULL);
			passThrough(CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
				reinterpret_cast<const void *>(1));
		}
	}
	catch(CssmError e)
	{
		if (e.error != CSSMERR_DL_DATASTORE_ALREADY_EXISTS)
		{
			DbImpl::deleteDb();
		}
		throw;
	}
	catch(...)
	{
		DbImpl::deleteDb();
		throw;
	}
}

void
SSDatabaseImpl::ssCreateWithBlob(const DLDbIdentifier &dlDbIdentifier, const CSSM_DATA &blob)
{
	try
	{
		bool autoCommit;
		commonCreate(dlDbIdentifier, autoCommit);
        secnotice("integrity", "opening %s", name());
		Db::Impl::insert(DBBlobRelationID, NULL, &blob);
		if (autoCommit)
		{
			passThrough(CSSM_APPLEFILEDL_COMMIT, NULL);
			passThrough(CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
				reinterpret_cast<const void *>(1));
		}
	}
	catch(...)
	{
		DbImpl::deleteDb();
		throw;
	}
}

void
SSDatabaseImpl::ssOpen(const DLDbIdentifier &dlDbIdentifier)
{
    load(dlDbIdentifier);

    CssmDataContainer dbb(allocator());
    getDbBlobId(&dbb);

    // Pull our version out of the database blob
	mSSDbHandle = mClientSession.decodeDb(dlDbIdentifier, AccessCredentials::overlay(accessCredentials()), dbb);
}

void
SSDatabaseImpl::load(const DLDbIdentifier &dlDbIdentifier) {
    mIdentifier = dlDbIdentifier;
    Db::Impl::open();

    CssmDataContainer dbb(allocator());
    getDbBlobId(&dbb);

    secinfo("integrity", "loading %s", name());
}

void
SSDatabaseImpl::ssRecode(const CssmData &dbHandleArray, const CssmData &agentData)
{
    // Start a transaction (Implies activate()).
    passThrough(CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT, 0);

    try
    {
        CssmDataContainer dbb(allocator());
        // Make sure mSSDbHandle is valid.
        CssmClient::DbUniqueRecord dbBlobId = getDbBlobId(&dbb);
        if (mForked()) {
            // re-establish the dbHandle with the SecurityServer
            mSSDbHandle = mClientSession.decodeDb(mIdentifier,
                    AccessCredentials::overlay(accessCredentials()), dbb);
        }
        dbb.clear();

        DbHandle successfulHdl = mClientSession.authenticateDbsForSync(dbHandleArray, agentData);

        // Create a newDbHandle using the master secrets from the dbBlob we are
        // recoding to.
        SecurityServer::DbHandle clonedDbHandle =
            mClientSession.recodeDbForSync(successfulHdl, mSSDbHandle);

        // @@@ If the dbb changed since we fetched it we should abort or
        // retry the operation here.

        recodeHelper(clonedDbHandle, dbBlobId);

        // Commit the transaction to the db
        passThrough(CSSM_APPLEFILEDL_COMMIT, NULL);
        passThrough(CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
                reinterpret_cast<const void *>(1));
    }
    catch (...)
    {
        // Something went wrong rollback the transaction
        passThrough(CSSM_APPLEFILEDL_ROLLBACK, NULL);
        passThrough(CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
                reinterpret_cast<const void *>(1));
        throw;
    }
}

uint32
SSDatabaseImpl::recodeDbToVersion(uint32 newBlobVersion) {
    // Start a transaction (Implies activate()).
    DLTransaction transaction(handle());

    try
    {
        if(isLocked()) {
            secnotice("integrity", "is currently locked");
        } else {
            secnotice("integrity", "is already unlocked");
        }

        CssmDataContainer dbb(allocator());
        // Make sure mSSDbHandle is valid.
        CssmClient::DbUniqueRecord dbBlobId = getDbBlobId(&dbb);

        // always establish the dbHandle with the SecurityServer
        mSSDbHandle = mClientSession.decodeDb(mIdentifier, AccessCredentials::overlay(accessCredentials()), dbb);
        dbb.clear();

        // Create a newDbHandle using the master secrets from the dbBlob we are recoding to.
        secnotice("integrity", "recoding db with handle %d", mSSDbHandle);
        SecurityServer::DbHandle clonedDbHandle = mClientSession.recodeDbToVersion(newBlobVersion, mSSDbHandle);
        secnotice("integrity", "received db with handle %d", clonedDbHandle);

        // @@@ If the dbb changed since we fetched it we should abort or
        // retry the operation here.

        uint32 newBlobVersion = recodeHelper(clonedDbHandle, dbBlobId);
        secnotice("integrity", "committing transaction %d", clonedDbHandle);

        // Commit the transaction to the db
        transaction.commit();
        return newBlobVersion;
    }
    catch (...)
    {
        throw;
    }
}

void
SSDatabaseImpl::recodeFinished() {
    mClientSession.recodeFinished(mSSDbHandle);
}

void SSDatabaseImpl::takeFileLock() {
    if(mTransaction) {
        // you're already in the middle of a file lock.
        return;
    }
    mTransaction = new DLTransaction(handle());
    passThrough(CSSM_APPLEFILEDL_TAKE_FILE_LOCK, NULL, NULL);
}

void SSDatabaseImpl::releaseFileLock(bool success) {
    if(mTransaction) {
        try {
            if(success) {
                mTransaction->commit();
            }
            // If we didn't commit, the destructor will roll back and re-enable autocommit
            delete mTransaction;
            mTransaction = NULL;
        } catch(...) {
            mTransaction = NULL;
            throw;
        }
    }
}

void SSDatabaseImpl::makeBackup() {
    passThrough(CSSM_APPLEFILEDL_MAKE_BACKUP, NULL, NULL);
}

void SSDatabaseImpl::makeCopy(const char* path) {
    passThrough(CSSM_APPLEFILEDL_MAKE_COPY, path, NULL);
}

void SSDatabaseImpl::deleteFile() {
    passThrough(CSSM_APPLEFILEDL_DELETE_FILE, NULL, NULL);
}

SSDatabase SSDatabaseImpl::ssCloneTo(const DLDbIdentifier& dldbidentifier) {
    makeCopy(dldbidentifier.dbName());
    SSDatabase db(mClientSession, dl(), dldbidentifier.dbName(), dldbidentifier.dbLocation());

    db->load(dldbidentifier);
    db->mSSDbHandle = mClientSession.cloneDb(dldbidentifier, mSSDbHandle);

    return db;
}



uint32 SSDatabaseImpl::recodeHelper(SecurityServer::DbHandle clonedDbHandle, CssmClient::DbUniqueRecord& dbBlobId) {
    // Recode all keys
    DbCursor cursor(SSDatabase(this));
    cursor->recordType(CSSM_DL_DB_RECORD_ALL_KEYS);
    CssmDataContainer keyBlob(allocator());
    CssmClient::DbUniqueRecord keyBlobId;
    DbAttributes attributes;
    while (cursor->next(&attributes, &keyBlob, keyBlobId))
    {
        KeyHandle keyHandle = 0;
        try {
            // Decode the old key
            CssmKey::Header header;
            keyHandle = mClientSession.decodeKey(mSSDbHandle, keyBlob, header);
            // Recode the key
            CssmDataContainer newKeyBlob(mClientSession.returnAllocator);
            mClientSession.recodeKey(mSSDbHandle, keyHandle, clonedDbHandle, newKeyBlob);
            mClientSession.releaseKey(keyHandle);
            keyHandle = 0;
            // Write the recoded key blob to the database
            keyBlobId->modify(attributes.recordType(), NULL, &newKeyBlob,
                    CSSM_DB_MODIFY_ATTRIBUTE_NONE);
        } catch (CssmError cssme) {
            const char* errStr = cssmErrorString(cssme.error);
            secnotice("integrity", "corrupt item while recoding: %d %s", (int) cssme.error, errStr);
            secnotice("integrity", "deleting corrupt item");


            keyBlobId->deleteRecord();

            if(keyHandle != 0) {
                // tell securityd not to worry about this key again
                try {
                    secnotice("integrity", "releasing corrupt key");
                    mClientSession.releaseKey(keyHandle);
                } catch(CssmError cssme) {
                    // swallow the error
                    const char* errStr = cssmErrorString(cssme.error);
                    secnotice("integrity", "couldn't release corrupt key: %d %s", (int) cssme.error, errStr);
                }
            }
        }
    }

    // Commit the new blob to securityd, reencode the db blob, release the
    // cloned db handle and commit the new blob to the db.
    CssmDataContainer dbb(allocator());
    secnotice("integrity", "committing %d", clonedDbHandle);
    mClientSession.commitDbForSync(mSSDbHandle, clonedDbHandle,
            dbb, allocator());
    dbBlobId->modify(DBBlobRelationID, NULL, &dbb,
            CSSM_DB_MODIFY_ATTRIBUTE_NONE);
    return getDbVersionFromBlob(dbb);
}


void SSDatabaseImpl::getRecordIdentifier(CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord, CSSM_DATA &recordID)
{
	// the unique ID is composed of three uint32s (plus one filler word).  Pull
	// them out and byte swap them
	recordID.Length = sizeof (uint32) * kNumIDWords;
	recordID.Data = (uint8*) allocator().malloc(recordID.Length);
	
	// copy the data
	uint32* dest = (uint32*) recordID.Data;
	uint32* src = (uint32*) uniqueRecord->RecordIdentifier.Data;
	
	dest[0] = htonl (src[0]);
	dest[1] = htonl (src[1]);
	dest[2] = htonl (src[2]);
	dest[3] = 0;
}

void SSDatabaseImpl::ssCopyBlob(CSSM_DATA& data)
{
	// get the blob from the database
	CssmDataContainer dbb(allocator());
	getDbBlobId(&dbb);
	
	// copy the data back
	data.Data = dbb.Data;
	data.Length = dbb.Length;
	
	// zap the return structure so that we don't get zapped when dbb goes out of scope...
	dbb.Data = NULL;
	dbb.Length = 0;
}

uint32
SSDatabaseImpl::dbBlobVersion() {
    CssmDataContainer dbb(allocator());
    getDbBlobId(&dbb);
    return getDbVersionFromBlob(dbb);
}

uint32
SSDatabaseImpl::getDbVersionFromBlob(const CssmData& dbb) {
    DbBlob* x = Allocator::standard().malloc<DbBlob>(dbb.length());
    memcpy(x, dbb, dbb.length());
    uint32 version = x->version();
    Allocator::standard().free(x);
    return version;
}

DbUniqueRecordImpl *
SSDatabaseImpl::newDbUniqueRecord()
{
	return new SSUniqueRecordImpl(SSDatabase(this));
}

CssmClient::DbUniqueRecord
SSDatabaseImpl::getDbBlobId(CssmDataContainer *dbb)
{
	CssmClient::DbUniqueRecord dbBlobId;

	DbCursor cursor(SSDatabase(this));
	cursor->recordType(DBBlobRelationID);
	if (!cursor->next(NULL, dbb, dbBlobId))
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	return dbBlobId;
}



SSUniqueRecordImpl::SSUniqueRecordImpl(const SSDatabase &db)
: DbUniqueRecord::Impl(db)
{
}

SSUniqueRecordImpl::~SSUniqueRecordImpl()
{
}

SSDatabase
SSUniqueRecordImpl::database() const
{
	return parent<SSDatabase>();
}
