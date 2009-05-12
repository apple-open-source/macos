/*
 * Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
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


//
// transition - securityd IPC-to-class-methods transition layer
//
// This file contains all server-side MIG implementations for the main
// securityd protocol ("ucsp"). It dispatches them into the vast object
// conspiracy that is securityd, anchored in the Server object.
//
#include <securityd_client/ss_types.h>
#include <securityd_client/ucsp.h>
#include "server.h"
#include "session.h"
#include "database.h"
#include "kcdatabase.h"
#include "tokendatabase.h"
#include "kckey.h"
#include "child.h"
#include <mach/mach_error.h>
#include <securityd_client/xdr_cssm.h>
#include <securityd_client/xdr_auth.h>
#include <securityd_client/xdr_dldb.h>

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFPropertyList.h>

//
// Bracket Macros
//
#define UCSP_ARGS	mach_port_t servicePort, mach_port_t replyPort, \
	audit_token_t auditToken, CSSM_RETURN *rcode

#define BEGIN_IPCN	*rcode = CSSM_OK; try {
#define BEGIN_IPC	BEGIN_IPCN RefPointer<Connection> connRef(&Server::connection(replyPort)); \
		Connection &connection __attribute__((unused)) = *connRef;
#define END_IPC(base)	END_IPCN(base) Server::requestComplete(*rcode); return KERN_SUCCESS;
#define END_IPCN(base) 	} \
	catch (const CommonError &err) { *rcode = CssmError::cssmError(err, CSSM_ ## base ## _BASE_ERROR); } \
	catch (const std::bad_alloc &) { *rcode = CssmError::merge(CSSM_ERRCODE_MEMORY_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
	catch (Connection *conn) { *rcode = 0; } \
	catch (...) { *rcode = CssmError::merge(CSSM_ERRCODE_INTERNAL_ERROR, CSSM_ ## base ## _BASE_ERROR); }

#define BEGIN_IPCS		try {
#define	END_IPCS(more)	} catch (...) { } \
						mach_port_deallocate(mach_task_self(), serverPort); more; return KERN_SUCCESS;

#define DATA_IN(base)	void *base, mach_msg_type_number_t base##Length
#define DATA_OUT(base)	void **base, mach_msg_type_number_t *base##Length
#define DATA(base)		CssmData(base, base##Length)

#define SSBLOB(Type, name) makeBlob<Type>(DATA(name))

using LowLevelMemoryUtilities::increment;
using LowLevelMemoryUtilities::difference;

class CopyOutAccessCredentials : public CopyOut {
public:
	CopyOutAccessCredentials(void *copy, size_t size) : CopyOut(copy, size + sizeof(CSSM_ACCESS_CREDENTIALS), reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS_PTR)) { }
	operator AccessCredentials *() { return static_cast<AccessCredentials *>(reinterpret_cast<CSSM_ACCESS_CREDENTIALS_PTR>(data())); }
};


class CopyOutEntryAcl : public CopyOut {
public:
	CopyOutEntryAcl(void *copy, size_t size) : CopyOut(copy, size + sizeof(CSSM_ACL_ENTRY_PROTOTYPE), reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_PROTOTYPE_PTR)) { }
	operator AclEntryPrototype *() { return static_cast<AclEntryPrototype *>(reinterpret_cast<CSSM_ACL_ENTRY_PROTOTYPE_PTR>(data())); }
};

class CopyOutOwnerAcl : public CopyOut {
public:
	CopyOutOwnerAcl(void *copy, size_t size) : CopyOut(copy, size + sizeof(CSSM_ACL_OWNER_PROTOTYPE), reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_OWNER_PROTOTYPE_PTR)) { }
	operator AclOwnerPrototype *() { return static_cast<AclOwnerPrototype *>(reinterpret_cast<CSSM_ACL_OWNER_PROTOTYPE_PTR>(data())); }
};

class CopyOutAclEntryInput : public CopyOut {
public:
	CopyOutAclEntryInput(void *copy, size_t size) : CopyOut(copy, size + sizeof(CSSM_ACL_ENTRY_INPUT), reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_INPUT_PTR)) { }
	operator AclEntryInput *() { return static_cast<AclEntryInput *>(reinterpret_cast<CSSM_ACL_ENTRY_INPUT_PTR>(data())); }
};


class CopyOutDeriveData : public CopyOut {
public:
	CopyOutDeriveData(void *copy, size_t size) : CopyOut(copy, size + sizeof(CSSM_DERIVE_DATA), reinterpret_cast<xdrproc_t>(xdr_CSSM_DERIVE_DATA_PTR)) { }
	CSSM_DERIVE_DATA * derive_data() { return reinterpret_cast<CSSM_DERIVE_DATA *>(data()); }
	CSSM_DATA &cssm_data() { return derive_data()->baseData; }
	CSSM_ALGORITHMS algorithm() { return derive_data()->algorithm; }
};


class CopyOutContext : public CopyOut {
public:
	CopyOutContext(void *copy, size_t size) : CopyOut(copy, size + sizeof(CSSM_CONTEXT), reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT_PTR)) { }
	operator Context *() { return static_cast<Context *>(reinterpret_cast<CSSM_CONTEXT_PTR>(data())); }
	Context &context() { return *static_cast<Context *>(reinterpret_cast<CSSM_CONTEXT_PTR>(data())); }
};

class CopyOutKey : public CopyOut {
public:
	CopyOutKey(void *copy, size_t size) : CopyOut(copy, size + sizeof(CSSM_KEY), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEY_PTR)) { }
	operator CssmKey *() { return static_cast<CssmKey *>(reinterpret_cast<CSSM_KEY_PTR>(data())); }
	CssmKey &key() { return *static_cast<CssmKey *>(reinterpret_cast<CSSM_KEY_PTR>(data())); }
};

class CopyOutDbRecordAttributes : public CopyOut {
public:
	CopyOutDbRecordAttributes(void *copy, size_t size) : CopyOut(copy, size + sizeof(CSSM_DB_RECORD_ATTRIBUTE_DATA), reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR)) { }
	CssmDbRecordAttributeData *attribute_data() { return static_cast<CssmDbRecordAttributeData *>(reinterpret_cast<CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR>(data())); }
};

class CopyOutQuery : public CopyOut {
public:
	CopyOutQuery(void *copy, size_t size) : CopyOut(copy, size, reinterpret_cast<xdrproc_t>(xdr_CSSM_QUERY_PTR)) { }
	operator CssmQuery *() { return static_cast<CssmQuery *>(reinterpret_cast<CSSM_QUERY_PTR>(data())); }
};

//
// Take a DATA type RPC argument purportedly representing a Blob of some kind,
// turn it into a Blob, and fail properly if it's not kosher.
//
template <class BlobType>
const BlobType *makeBlob(const CssmData &blobData, CSSM_RETURN error = CSSM_ERRCODE_INVALID_DATA)
{
	if (!blobData.data() || blobData.length() < sizeof(BlobType))
		CssmError::throwMe(error);
	const BlobType *blob = static_cast<const BlobType *>(blobData.data());
	if (blob->totalLength != blobData.length())
		CssmError::throwMe(error);
	return blob;
}

//
// An OutputData object will take memory allocated within securityd,
// hand it to the MIG return-output parameters, and schedule it to be released
// after the MIG reply has been sent. It will also get rid of it in case of
// error.
//
class OutputData : public CssmData {
public:
	OutputData(void **outP, mach_msg_type_number_t *outLength)
		: mData(*outP), mLength(*outLength) { }
	~OutputData()
	{ mData = data(); mLength = length(); Server::releaseWhenDone(mData); }
    
    void operator = (const CssmData &source)
    { CssmData::operator = (source); }
	
private:
	void * &mData;
	mach_msg_type_number_t &mLength;
};

//
// Choose a Database from a choice of two sources, giving preference
// to persistent stores and to earlier sources.
//
Database *pickDb(Database *db1, Database *db2);

static inline Database *dbOf(Key *key)	{ return key ? &key->database() : NULL; }

inline Database *pickDb(Key *k1, Key *k2) { return pickDb(dbOf(k1), dbOf(k2)); }
inline Database *pickDb(Database *db1, Key *k2) { return pickDb(db1, dbOf(k2)); }
inline Database *pickDb(Key *k1, Database *db2) { return pickDb(dbOf(k1), db2); }

//
// Choose a Database from a choice of two sources, giving preference
// to persistent stores and to earlier sources.
//
Database *pickDb(Database *db1, Database *db2)
{
	// persistent db1 always wins
	if (db1 && !db1->transient())
		return db1;
	
	// persistent db2 is next choice
	if (db2 && !db2->transient())
		return db2;
	
	// pick any existing transient database
	if (db1)
		return db1;
	if (db2)
		return db2;
	
	// none at all. use the canonical transient store
	return Server::optionalDatabase(noDb);
}

//
// Setup/Teardown functions.
//
kern_return_t ucsp_server_setup(UCSP_ARGS, mach_port_t taskPort, ClientSetupInfo info, const char *identity)
{
	BEGIN_IPCN
	Server::active().setupConnection(Server::connectNewProcess, servicePort, replyPort,
		taskPort, auditToken, &info, identity);
	END_IPCN(CSSM)
	return KERN_SUCCESS;
}

kern_return_t ucsp_server_setupNew(UCSP_ARGS, mach_port_t taskPort,
	ClientSetupInfo info, const char *identity,
	mach_port_t *newServicePort)
{
	BEGIN_IPCN
	try {
		RefPointer<Session> session = new DynamicSession(taskPort);
		Server::active().setupConnection(Server::connectNewSession, session->servicePort(), replyPort,
			taskPort, auditToken, &info, identity);
		*newServicePort = session->servicePort();
	} catch (const MachPlusPlus::Error &err) {
		switch (err.error) {
		case BOOTSTRAP_SERVICE_ACTIVE:
			MacOSError::throwMe(errSessionAuthorizationDenied);	// translate
		default:
			throw;
		}
	}
	END_IPCN(CSSM)
	return KERN_SUCCESS;
}

kern_return_t ucsp_server_setupThread(UCSP_ARGS, mach_port_t taskPort)
{
	BEGIN_IPCN
	Server::active().setupConnection(Server::connectNewThread, servicePort, replyPort,
		taskPort, auditToken);
	END_IPCN(CSSM)
	return KERN_SUCCESS;
}


kern_return_t ucsp_server_teardown(UCSP_ARGS)
{
	BEGIN_IPCN
	Server::active().endConnection(replyPort);
	END_IPCN(CSSM)
	return KERN_SUCCESS;
}

kern_return_t ucsp_server_verifyPrivileged(UCSP_ARGS)
{
	BEGIN_IPCN
	// This line intentionally left blank.
	END_IPCN(CSSM)
	return KERN_SUCCESS;
}

//
// Common database operations
//
kern_return_t ucsp_server_authenticateDb(UCSP_ARGS, IPCDbHandle db,
	CSSM_DB_ACCESS_TYPE accessType, DATA_IN(cred))
{
	BEGIN_IPC
    secdebug("dl", "authenticateDb");
	CopyOutAccessCredentials creds(cred, credLength);
	// ignoring accessType
    Server::database(db)->authenticate(accessType, creds);
	END_IPC(DL)
}

kern_return_t ucsp_server_releaseDb(UCSP_ARGS, IPCDbHandle db)
{
	BEGIN_IPC
	connection.process().kill(*Server::database(db));
	END_IPC(DL)
}


kern_return_t ucsp_server_getDbName(UCSP_ARGS, IPCDbHandle db, char name[PATH_MAX])
{
	BEGIN_IPC
	string result = Server::database(db)->dbName();
	assert(result.length() < PATH_MAX);
	memcpy(name, result.c_str(), result.length() + 1);
	END_IPC(DL)
}

kern_return_t ucsp_server_setDbName(UCSP_ARGS, IPCDbHandle db, const char *name)
{
	BEGIN_IPC
	Server::database(db)->dbName(name);
	END_IPC(DL)
}


//
// External database interface
//
kern_return_t ucsp_server_openToken(UCSP_ARGS, uint32 ssid, FilePath name,
	DATA_IN(accessCredentials), IPCDbHandle *db)
{
	BEGIN_IPC
	CopyOutAccessCredentials creds(accessCredentials, accessCredentialsLength);
	*db = (new TokenDatabase(ssid, connection.process(), name, creds))->handle();
	END_IPC(DL)
}

kern_return_t ucsp_server_findFirst(UCSP_ARGS, IPCDbHandle db,
	DATA_IN(inQuery), DATA_IN(inAttributes), DATA_OUT(outAttributes),
	boolean_t getData, DATA_OUT(data), 
    IPCKeyHandle *hKey, IPCSearchHandle *hSearch, IPCRecordHandle *hRecord)
{
	BEGIN_IPC
	CopyOutQuery query(inQuery, inQueryLength);
	CopyOutDbRecordAttributes attrs(inAttributes, inAttributesLength);

	RefPointer<Database::Search> search;
	RefPointer<Database::Record> record;
	RefPointer<Key> key;
	CssmData outData;
	CssmDbRecordAttributeData *outAttrs = NULL; mach_msg_type_number_t outAttrsLength;
	Server::database(db)->findFirst(*query, 
        attrs.attribute_data(), attrs.length(),
		getData ? &outData : NULL, key, search, record, outAttrs, outAttrsLength);
	
	// handle nothing-found case without exceptions
	if (!record) {
		*hRecord = noRecord;
		*hSearch = noSearch;
		*hKey = noKey;
	} else {
		// return handles
		*hRecord = record->handle();
		*hSearch = search->handle();
		*hKey = key ? key->handle() : noKey;

        if (outAttrsLength && outAttrs) {
            Server::releaseWhenDone(outAttrs); // exception proof it against next line
            if (!copyin(outAttrs, reinterpret_cast<xdrproc_t> (xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA), outAttributes, outAttributesLength))
                CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
            Server::releaseWhenDone(*outAttributes);
        }
        
		// return data (temporary fix)
		if (getData) {
			Server::releaseWhenDone(outData.data());
            xdrproc_t encode_proc = reinterpret_cast<xdrproc_t>(xdr_CSSM_NO_KEY_IN_DATA);
            if (key)
                encode_proc = reinterpret_cast<xdrproc_t>(xdr_CSSM_KEY_IN_DATA);
			if (!copyin(&outData, encode_proc, data, dataLength))
				CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
			Server::releaseWhenDone(*data);
		}
	}
	END_IPC(DL)
}


kern_return_t ucsp_server_findNext(UCSP_ARGS, IPCSearchHandle hSearch,
	DATA_IN(inAttributes),
	DATA_OUT(outAttributes),
	boolean_t getData, DATA_OUT(data), IPCKeyHandle *hKey,
	IPCRecordHandle *hRecord)
{
	BEGIN_IPC
	CopyOutDbRecordAttributes attrs(inAttributes, inAttributesLength);
	RefPointer<Database::Search> search =
		Server::find<Database::Search>(hSearch, CSSMERR_DL_INVALID_RESULTS_HANDLE);
	RefPointer<Database::Record> record;
	RefPointer<Key> key;
	CssmData outData;
	CssmDbRecordAttributeData *outAttrs = NULL; mach_msg_type_number_t outAttrsLength;
	search->database().findNext(search, attrs.attribute_data(), attrs.length(),
		getData ? &outData : NULL, key, record, outAttrs, outAttrsLength);
	
	// handle nothing-found case without exceptions
	if (!record) {
		*hRecord = noRecord;
		*hKey = noKey;
	} else {
		// return handles
		*hRecord = record->handle();
		*hKey = key ? key->handle() : noKey;

        if (outAttrsLength && outAttrs) {
			secdebug("attrmem", "Found attrs: %p of length: %d", outAttrs, outAttrsLength);
            Server::releaseWhenDone(outAttrs); // exception proof it against next line
            if (!copyin(outAttrs, reinterpret_cast<xdrproc_t> (xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA), outAttributes, outAttributesLength))
                CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
			secdebug("attrmem", "Copied attrs: %p of length: %d", *outAttributes, *outAttributesLength);
            Server::releaseWhenDone(*outAttributes);
        }
        
		// return data (temporary fix)
		if (getData) {
			Server::releaseWhenDone(outData.data());
            xdrproc_t encode_proc = reinterpret_cast<xdrproc_t>(xdr_CSSM_NO_KEY_IN_DATA);
            if (key)
                encode_proc = reinterpret_cast<xdrproc_t>(xdr_CSSM_KEY_IN_DATA);
            if (!copyin(&outData, encode_proc, data, dataLength))
                CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
			Server::releaseWhenDone(*data);
		}
	}
	END_IPC(DL)
}

kern_return_t ucsp_server_findRecordHandle(UCSP_ARGS, IPCRecordHandle hRecord,
	DATA_IN(inAttributes), DATA_OUT(outAttributes),
	boolean_t getData, DATA_OUT(data), IPCKeyHandle *hKey)
{
	BEGIN_IPC
	CopyOutDbRecordAttributes attrs(inAttributes, inAttributesLength);
	RefPointer<Database::Record> record =
		Server::find<Database::Record>(hRecord, CSSMERR_DL_INVALID_RECORD_UID);
	RefPointer<Key> key;
	CssmData outData;
	CssmDbRecordAttributeData *outAttrs; mach_msg_type_number_t outAttrsLength;
	record->database().findRecordHandle(record, attrs.attribute_data(), attrs.length(),
		getData ? &outData : NULL, key, outAttrs, outAttrsLength);
	
	// return handles
	*hKey = key ? key->handle() : noKey;

    if (outAttrsLength && outAttrs) {
        Server::releaseWhenDone(outAttrs); // exception proof it against next line
        if (!copyin(outAttrs, reinterpret_cast<xdrproc_t> (xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA), outAttributes, outAttributesLength))
            CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
        Server::releaseWhenDone(*outAttributes);
    }
    
	// return data (temporary fix)
	if (getData) {
		Server::releaseWhenDone(outData.data());
        xdrproc_t encode_proc = reinterpret_cast<xdrproc_t>(xdr_CSSM_NO_KEY_IN_DATA);
        if (key)
            encode_proc = reinterpret_cast<xdrproc_t>(xdr_CSSM_KEY_IN_DATA);
        if (!copyin(&outData, encode_proc, data, dataLength))
            CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
        Server::releaseWhenDone(*data);
	}
	END_IPC(DL)
}

kern_return_t ucsp_server_insertRecord(UCSP_ARGS, IPCDbHandle db, CSSM_DB_RECORDTYPE recordType,
	DATA_IN(inAttributes), DATA_IN(data), IPCRecordHandle *record)
{
	BEGIN_IPC
	RecordHandle recordHandle;
	CopyOutDbRecordAttributes attrs(inAttributes, inAttributesLength);
	Server::database(db)->insertRecord(recordType, attrs.attribute_data(), attrs.length(),
		DATA(data), recordHandle);
	*record = recordHandle;
	END_IPC(DL)
}

kern_return_t ucsp_server_modifyRecord(UCSP_ARGS, IPCDbHandle db, IPCRecordHandle *hRecord,
	CSSM_DB_RECORDTYPE recordType, DATA_IN(attributes),
	boolean_t setData, DATA_IN(data), CSSM_DB_MODIFY_MODE modifyMode)
{
	BEGIN_IPC
	CopyOutDbRecordAttributes attrs(attributes, attributesLength);
	CssmData newData(DATA(data));
	RefPointer<Database::Record> record =
		Server::find<Database::Record>(*hRecord, CSSMERR_DL_INVALID_RECORD_UID);
	Server::database(db)->modifyRecord(recordType, record, attrs.attribute_data(), attrs.length(),
		setData ? &newData : NULL, modifyMode);
	// note that the record handle presented to the client never changes here
	// (we could, but have no reason to - our record handles are just always up to date)
	END_IPC(DL)
}

kern_return_t ucsp_server_deleteRecord(UCSP_ARGS, IPCDbHandle db, IPCRecordHandle hRecord)
{
	BEGIN_IPC
	Server::database(db)->deleteRecord(
		Server::find<Database::Record>(hRecord, CSSMERR_DL_INVALID_RECORD_UID));
	END_IPC(DL)
}

kern_return_t ucsp_server_releaseSearch(UCSP_ARGS, IPCSearchHandle hSearch)
{
	BEGIN_IPC
	RefPointer<Database::Search> search = Server::find<Database::Search>(hSearch, 0);
	search->database().releaseSearch(*search);
	END_IPC(DL)
}

kern_return_t ucsp_server_releaseRecord(UCSP_ARGS, IPCRecordHandle hRecord)
{
	BEGIN_IPC
	RefPointer<Database::Record> record = Server::find<Database::Record>(hRecord, 0);
	record->database().releaseRecord(*record);
	END_IPC(DL)
}


//
// Internal database management
//
kern_return_t ucsp_server_createDb(UCSP_ARGS, IPCDbHandle *db,
	DATA_IN(ident), DATA_IN(cred), DATA_IN(owner),
    DBParameters params)
{
	BEGIN_IPC
	CopyOutAccessCredentials creds(cred, credLength);
	CopyOutEntryAcl owneracl(owner, ownerLength);
	CopyOut flatident(ident, identLength, reinterpret_cast<xdrproc_t>(xdr_DLDbFlatIdentifierRef));
	*db = (new KeychainDatabase(*reinterpret_cast<DLDbFlatIdentifier*>(flatident.data()), params, connection.process(), creds, owneracl))->handle();
	END_IPC(DL)
}

// keychain synchronization
// @@@  caller should be required to call decodeDb() to get a DbHandle
//      instead of passing the blob itself
kern_return_t ucsp_server_cloneDbForSync(UCSP_ARGS, DATA_IN(blob), 
	IPCDbHandle srcDb, DATA_IN(agentData), IPCDbHandle *newDb)
{
	BEGIN_IPC
	RefPointer<KeychainDatabase> srcKC = Server::keychain(srcDb);
	*newDb = (new KeychainDatabase(*srcKC, connection.process(),
		SSBLOB(DbBlob, blob), DATA(agentData)))->handle();
	END_IPC(DL)
}

kern_return_t ucsp_server_commitDbForSync(UCSP_ARGS, IPCDbHandle srcDb,
    IPCDbHandle cloneDb, DATA_OUT(blob))
{
	BEGIN_IPC
    RefPointer<KeychainDatabase> srcKC = Server::keychain(srcDb);
    RefPointer<KeychainDatabase> cloneKC = Server::keychain(cloneDb);
    srcKC->commitSecretsForSync(*cloneKC);

	// re-encode blob for convenience
	if (blob && blobLength) {
		DbBlob *dbBlob = srcKC->blob();
		*blob = dbBlob;
		*blobLength = dbBlob->length();
	} else {
		secdebug("kcrecode", "No blob can be returned to client");
	}
	END_IPC(DL)
}

kern_return_t ucsp_server_decodeDb(UCSP_ARGS, IPCDbHandle *db,
    DATA_IN(ident), DATA_IN(cred), DATA_IN(blob))
{
	BEGIN_IPC
	CopyOutAccessCredentials creds(cred, credLength);
	CopyOut flatident(ident, identLength, reinterpret_cast<xdrproc_t>(xdr_DLDbFlatIdentifierRef));
	*db = (new KeychainDatabase(*reinterpret_cast<DLDbFlatIdentifier*>(flatident.data()), SSBLOB(DbBlob, blob),
        connection.process(), creds))->handle();
	END_IPC(DL)
}

kern_return_t ucsp_server_encodeDb(UCSP_ARGS, IPCDbHandle db, DATA_OUT(blob))
{
	BEGIN_IPC
    DbBlob *dbBlob = Server::keychain(db)->blob();	// memory owned by database
    *blob = dbBlob;
    *blobLength = dbBlob->length();
	END_IPC(DL)
}

kern_return_t ucsp_server_setDbParameters(UCSP_ARGS, IPCDbHandle db, DBParameters params)
{
	BEGIN_IPC
	Server::keychain(db)->setParameters(params);
	END_IPC(DL)
}

kern_return_t ucsp_server_getDbParameters(UCSP_ARGS, IPCDbHandle db, DBParameters *params)
{
	BEGIN_IPC
	Server::keychain(db)->getParameters(*params);
	END_IPC(DL)
}

kern_return_t ucsp_server_changePassphrase(UCSP_ARGS, IPCDbHandle db,
    DATA_IN(cred))
{
	BEGIN_IPC
	CopyOutAccessCredentials creds(cred, credLength);
	Server::keychain(db)->changePassphrase(creds);
	END_IPC(DL)
}

kern_return_t ucsp_server_lockAll (UCSP_ARGS, boolean_t)
{
	BEGIN_IPC
	connection.session().processLockAll();
	END_IPC(DL)
}

kern_return_t ucsp_server_unlockDb(UCSP_ARGS, IPCDbHandle db)
{
	BEGIN_IPC
	Server::keychain(db)->unlockDb();
	END_IPC(DL)
}

kern_return_t ucsp_server_unlockDbWithPassphrase(UCSP_ARGS, IPCDbHandle db, DATA_IN(passphrase))
{
	BEGIN_IPC
	Server::keychain(db)->unlockDb(DATA(passphrase));
	END_IPC(DL)
}

kern_return_t ucsp_server_isLocked(UCSP_ARGS, IPCDbHandle db, boolean_t *locked)
{
    BEGIN_IPC
    *locked = Server::database(db)->isLocked();
    END_IPC(DL)
}


//
// Key management
//
kern_return_t ucsp_server_encodeKey(UCSP_ARGS, IPCKeyHandle keyh, DATA_OUT(blob),
    boolean_t wantUid, DATA_OUT(uid))
{
	BEGIN_IPC
	RefPointer<Key> gKey = Server::key(keyh);
	if (KeychainKey *key = dynamic_cast<KeychainKey *>(gKey.get())) {
		KeyBlob *keyBlob = key->blob();	// still owned by key
		*blob = keyBlob;
		*blobLength = keyBlob->length();
		if (wantUid) {	// uid generation is not implemented
			CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
		} else {
			*uidLength = 0;	// do not return this
		}
	} else {	// not a KeychainKey
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
	}
	END_IPC(CSP)
}

kern_return_t ucsp_server_decodeKey(UCSP_ARGS, IPCKeyHandle *keyh, DATA_OUT(keyHeader),
	IPCDbHandle db, DATA_IN(blob))
{
	BEGIN_IPC
    RefPointer<Key> key = new KeychainKey(*Server::keychain(db), SSBLOB(KeyBlob, blob));
	CssmKey::Header header;
	KeyHandle keyHandle;
    key->returnKey(keyHandle, header);
	*keyh = keyHandle;
	if (!copyin(&header, reinterpret_cast<xdrproc_t> (xdr_CSSM_KEYHEADER), keyHeader, keyHeaderLength))
		CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
	Server::releaseWhenDone(*keyHeader);
	END_IPC(CSP)
}

// keychain synchronization
kern_return_t ucsp_server_recodeKey(UCSP_ARGS, IPCDbHandle oldDb, IPCKeyHandle keyh, 
	IPCDbHandle newDb, DATA_OUT(newBlob))
{
	BEGIN_IPC
	// If the old key is passed in as DATA_IN(oldBlob):
	// RefPointer<KeychainKey> key = new KeychainKey(*Server::keychain(oldDb), SSBLOB(KeyBlob, oldBlob));
	RefPointer<Key> key = Server::key(keyh);
	if (KeychainKey *kckey = dynamic_cast<KeychainKey *>(key.get())) {
		KeyBlob *blob = Server::keychain(newDb)->recodeKey(*kckey);
		*newBlob = blob;
		*newBlobLength = blob->length();
		Server::releaseWhenDone(*newBlob);
		// @@@  stop leaking blob
	} else {	// not a KeychainKey
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
	}		
	END_IPC(CSP)
}

kern_return_t ucsp_server_releaseKey(UCSP_ARGS, IPCKeyHandle keyh)
{
	BEGIN_IPC
	RefPointer<Key> key = Server::key(keyh);
	key->database().releaseKey(*key);
	END_IPC(CSP)
}

kern_return_t ucsp_server_queryKeySizeInBits(UCSP_ARGS, IPCKeyHandle keyh, CSSM_KEY_SIZE *length)
{
	BEGIN_IPC
	RefPointer<Key> key = Server::key(keyh);
	key->database().queryKeySizeInBits(*key, CssmKeySize::overlay(*length));
	END_IPC(CSP)
}

kern_return_t ucsp_server_getOutputSize(UCSP_ARGS, DATA_IN(context), IPCKeyHandle keyh,
    uint32 inputSize, boolean_t encrypt, uint32 *outputSize)
{
    BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	RefPointer<Key> key = Server::key(keyh);
    key->database().getOutputSize(*ctx, *key, inputSize, encrypt, *outputSize);
    END_IPC(CSP)
}

kern_return_t ucsp_server_getKeyDigest(UCSP_ARGS, IPCKeyHandle key, DATA_OUT(digest))
{
	BEGIN_IPC
	CssmData digestData = Server::key(key)->canonicalDigest();
	*digest = digestData.data();
	*digestLength = digestData.length();
	END_IPC(CSP)
}


//
// Signatures and MACs
//
kern_return_t ucsp_server_generateSignature(UCSP_ARGS, DATA_IN(context), IPCKeyHandle keyh,
        CSSM_ALGORITHMS signOnlyAlgorithm, DATA_IN(data), DATA_OUT(signature))
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	RefPointer<Key> key = Server::key(keyh);
	OutputData sigData(signature, signatureLength);
	key->database().generateSignature(*ctx, *key, signOnlyAlgorithm,
		DATA(data), sigData);
	END_IPC(CSP)
}

kern_return_t ucsp_server_verifySignature(UCSP_ARGS, DATA_IN(context), IPCKeyHandle keyh,
		CSSM_ALGORITHMS verifyOnlyAlgorithm, DATA_IN(data), DATA_IN(signature))
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	RefPointer<Key> key = Server::key(keyh);
	key->database().verifySignature(*ctx, *key, verifyOnlyAlgorithm,
		DATA(data), DATA(signature));
	END_IPC(CSP)
}

kern_return_t ucsp_server_generateMac(UCSP_ARGS, DATA_IN(context), IPCKeyHandle keyh,
		DATA_IN(data), DATA_OUT(mac))
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	RefPointer<Key> key = Server::key(keyh);
	OutputData macData(mac, macLength);
	key->database().generateMac(*ctx, *key, DATA(data), macData);
	END_IPC(CSP)
}

kern_return_t ucsp_server_verifyMac(UCSP_ARGS, DATA_IN(context), IPCKeyHandle keyh,
		DATA_IN(data), DATA_IN(mac))
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	RefPointer<Key> key = Server::key(keyh);
	key->database().verifyMac(*ctx, *key, DATA(data), DATA(mac));
	END_IPC(CSP)
}


//
// Encryption/Decryption
//
kern_return_t ucsp_server_encrypt(UCSP_ARGS, DATA_IN(context), IPCKeyHandle keyh,
	DATA_IN(clear), DATA_OUT(cipher))
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	RefPointer<Key> key = Server::key(keyh);
	OutputData cipherOut(cipher, cipherLength);
	key->database().encrypt(*ctx, *key, DATA(clear), cipherOut);
	END_IPC(CSP)
}

kern_return_t ucsp_server_decrypt(UCSP_ARGS, DATA_IN(context), IPCKeyHandle keyh,
	DATA_IN(cipher), DATA_OUT(clear))
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	RefPointer<Key> key = Server::key(keyh);
	OutputData clearOut(clear, clearLength);
	key->database().decrypt(*ctx, *key, DATA(cipher), clearOut);
	END_IPC(CSP)
}


//
// Key generation
//
kern_return_t ucsp_server_generateKey(UCSP_ARGS, IPCDbHandle db, DATA_IN(context),
	DATA_IN(cred), DATA_IN(owner),
	uint32 usage, uint32 attrs, IPCKeyHandle *newKey, DATA_OUT(keyHeader))
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	CopyOutAccessCredentials creds(cred, credLength);

	CopyOutEntryAcl owneracl(owner, ownerLength);
	//@@@ preliminary interpretation - will get "type handle"
	RefPointer<Database> database =
		Server::optionalDatabase(db, attrs & CSSM_KEYATTR_PERMANENT);
	RefPointer<Key> key;
	database->generateKey(*ctx, creds, owneracl, usage, attrs, key);
	CssmKey::Header newHeader;
	KeyHandle keyHandle;
    key->returnKey(keyHandle, newHeader);
	*newKey = keyHandle;

	if (!copyin(&newHeader, reinterpret_cast<xdrproc_t> (xdr_CSSM_KEYHEADER), keyHeader, keyHeaderLength))
		CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
	Server::releaseWhenDone(*keyHeader);
	END_IPC(CSP)
}

kern_return_t ucsp_server_generateKeyPair(UCSP_ARGS, IPCDbHandle db, DATA_IN(context),
	DATA_IN(cred), DATA_IN(owner),
	uint32 pubUsage, uint32 pubAttrs, uint32 privUsage, uint32 privAttrs,
	IPCKeyHandle *pubKey, DATA_OUT(pubHeader), IPCKeyHandle *privKey, DATA_OUT(privHeader))
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	CopyOutAccessCredentials creds(cred, credLength);
	CopyOutEntryAcl owneracl(owner, ownerLength);
	RefPointer<Database> database =
		Server::optionalDatabase(db, (privAttrs | pubAttrs) & CSSM_KEYATTR_PERMANENT);
	RefPointer<Key> pub, priv;
	database->generateKey(*ctx, creds, owneracl,
		pubUsage, pubAttrs, privUsage, privAttrs, pub, priv);
	CssmKey::Header tmpPubHeader, tmpPrivHeader;
	KeyHandle pubKeyHandle, privKeyHandle;
	
    pub->returnKey(pubKeyHandle, tmpPubHeader);
	*pubKey = pubKeyHandle;
	if (!copyin(&tmpPubHeader, reinterpret_cast<xdrproc_t> (xdr_CSSM_KEYHEADER), pubHeader, pubHeaderLength))
		CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
	Server::releaseWhenDone(*pubHeader);

    priv->returnKey(privKeyHandle, tmpPrivHeader);
	*privKey = privKeyHandle;
	if (!copyin(&tmpPrivHeader, reinterpret_cast<xdrproc_t> (xdr_CSSM_KEYHEADER), privHeader, privHeaderLength))
		CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
	Server::releaseWhenDone(*privHeader);

	END_IPC(CSP)
}


//
// Key wrapping and unwrapping
//
kern_return_t ucsp_server_wrapKey(UCSP_ARGS, DATA_IN(context), IPCKeyHandle hWrappingKey,
	DATA_IN(cred), IPCKeyHandle hKeyToBeWrapped,
	DATA_IN(descriptiveData), DATA_OUT(wrappedKeyData))
{
	BEGIN_IPC
	CssmKey wrappedKey;
	CopyOutContext ctx(context, contextLength);
	CopyOutAccessCredentials creds(cred, credLength);
	RefPointer<Key> subjectKey = Server::key(hKeyToBeWrapped);
	RefPointer<Key> wrappingKey = Server::optionalKey(hWrappingKey);
	if ((ctx.context().algorithm() == CSSM_ALGID_NONE && subjectKey->attribute(CSSM_KEYATTR_SENSITIVE))
		|| !subjectKey->attribute(CSSM_KEYATTR_EXTRACTABLE))
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
	pickDb(subjectKey, wrappingKey)->wrapKey(*ctx, creds, wrappingKey, *subjectKey, DATA(descriptiveData), wrappedKey);
	Server::releaseWhenDone(wrappedKey.keyData().data());

	if (!copyin(&wrappedKey, reinterpret_cast<xdrproc_t> (xdr_CSSM_KEY), wrappedKeyData, wrappedKeyDataLength))
		CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
        
	Server::releaseWhenDone(*wrappedKeyData);
	END_IPC(CSP)
}

kern_return_t ucsp_server_unwrapKey(UCSP_ARGS, IPCDbHandle db, DATA_IN(context),
	IPCKeyHandle hWrappingKey, DATA_IN(cred), DATA_IN(owner),
	IPCKeyHandle hPublicKey, DATA_IN(wrappedKeyData),
	CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs, DATA_OUT(descriptiveData),
    IPCKeyHandle *newKey, DATA_OUT(keyHeader)/*CssmKey::Header *newHeader*/)
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	CopyOutKey wrappedKey(wrappedKeyData, wrappedKeyDataLength);
	CopyOutAccessCredentials creds(cred, credLength);
	CopyOutEntryAcl owneracl(owner, ownerLength);
	OutputData descriptiveDatas(descriptiveData, descriptiveDataLength);
	RefPointer<Key> wrappingKey = Server::optionalKey(hWrappingKey);
    RefPointer<Key> unwrappedKey;
	pickDb(Server::optionalDatabase(db), wrappingKey)->unwrapKey(*ctx, creds, owneracl,
		wrappingKey, Server::optionalKey(hPublicKey),
		usage, attrs, wrappedKey.key(), unwrappedKey, descriptiveDatas);
		
	CssmKey::Header newHeader;
	KeyHandle keyHandle;
	unwrappedKey->returnKey(keyHandle, newHeader);
	*newKey = keyHandle;
	if (!copyin(&newHeader, reinterpret_cast<xdrproc_t> (xdr_CSSM_KEYHEADER), keyHeader, keyHeaderLength))
		CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
	Server::releaseWhenDone(*keyHeader);

	END_IPC(CSP)
}


//
// Key derivation.
//
// Note that the "param" argument can have structure. The walker for the
// (artificial) POD CssmDeriveData handles those that are known; if you add
// an algorithm with structured param, you need to add a case there.
//
kern_return_t ucsp_server_deriveKey(UCSP_ARGS, IPCDbHandle db, DATA_IN(context), IPCKeyHandle hKey,
	DATA_IN(cred), DATA_IN(owner),
    DATA_IN(paramInput), DATA_OUT(paramOutput),
	uint32 usage, uint32 attrs, IPCKeyHandle *newKey, DATA_OUT(keyHeader))
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	CopyOutAccessCredentials creds(cred, credLength);
	CopyOutEntryAcl owneracl(owner, ownerLength);
	CopyOutDeriveData deriveParam(paramInput, paramInputLength);
	if (deriveParam.algorithm() != ctx.context().algorithm())
		CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);	// client layer fault
    
	RefPointer<Database> database =
		Server::optionalDatabase(db, attrs & CSSM_KEYATTR_PERMANENT);
	RefPointer<Key> key = Server::optionalKey(hKey);
	CSSM_DATA param = deriveParam.cssm_data();
	RefPointer<Key> derivedKey;
	pickDb(Server::optionalDatabase(db, attrs & CSSM_KEYATTR_PERMANENT),
		key)->deriveKey(*ctx, key, creds, owneracl, static_cast<CssmData*>(&param), usage, attrs, derivedKey);
		
	CssmKey::Header newHeader;
	KeyHandle keyHandle;
	derivedKey->returnKey(keyHandle, newHeader);
	*newKey = keyHandle;
	
	if (!copyin(&newHeader, reinterpret_cast<xdrproc_t> (xdr_CSSM_KEYHEADER), keyHeader, keyHeaderLength))
		CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
	Server::releaseWhenDone(*keyHeader);
		
	if (param.Length) {
        if (!param.Data)	// CSP screwed up
            CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
		OutputData(paramOutput, paramOutputLength) = CssmAutoData(Server::csp().allocator(), param).release();
    }
	END_IPC(CSP)
}


//
// Random generation
//
kern_return_t ucsp_server_generateRandom(UCSP_ARGS, uint32 ssid, DATA_IN(context), DATA_OUT(data))
{
	BEGIN_IPC
	CopyOutContext ctx(context, contextLength);
	if (ssid)
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	
	// default version (use /dev/random)
	Allocator &allocator = Allocator::standard(Allocator::sensitive);
	if (size_t bytes = ctx.context().getInt(CSSM_ATTRIBUTE_OUTPUT_SIZE)) {
		void *buffer = allocator.malloc(bytes);
		Server::active().random(buffer, bytes);
		*data = buffer;
		*dataLength = bytes;
		Server::releaseWhenDone(allocator, buffer);
	}
	END_IPC(CSP)
}


//
// ACL management.
// Watch out for the memory-management tap-dance.
//
kern_return_t ucsp_server_getOwner(UCSP_ARGS, AclKind kind, IPCKeyHandle key,
	DATA_OUT(ownerOut))
{
	BEGIN_IPC
	AclOwnerPrototype owner;
	Server::aclBearer(kind, key).getOwner(owner);	// allocates memory in owner
	void *owners_data; u_int owners_length;
	if (!::copyin(&owner, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_OWNER_PROTOTYPE), &owners_data, &owners_length))
			CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR); 	

	{ ChunkFreeWalker free; walk(free, owner); } // release chunked original
	Server::releaseWhenDone(owners_data); // throw flat copy out when done
	*ownerOut = owners_data;
	*ownerOutLength = owners_length;
	END_IPC(CSP)
}

kern_return_t ucsp_server_setOwner(UCSP_ARGS, AclKind kind, IPCKeyHandle key,
	DATA_IN(cred), DATA_IN(owner))
{
	BEGIN_IPC
	CopyOutAccessCredentials creds(cred, credLength);
	CopyOutOwnerAcl owneracl(owner, ownerLength);
	Server::aclBearer(kind, key).changeOwner(*owneracl, creds);
	END_IPC(CSP)
}

kern_return_t ucsp_server_getAcl(UCSP_ARGS, AclKind kind, IPCKeyHandle key,
	boolean_t haveTag, const char *tag,
	uint32 *countp, DATA_OUT(acls))
{
	BEGIN_IPC
	uint32 count;
	AclEntryInfo *aclList;
	Server::aclBearer(kind, key).getAcl(haveTag ? tag : NULL, count, aclList);

	CSSM_ACL_ENTRY_INFO_ARRAY aclsArray = { count, aclList };
	void *acls_data; u_int acls_length;
	if (!::copyin(&aclsArray, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_INFO_ARRAY), &acls_data, &acls_length))
			CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR); 	
	
	{       // release the chunked memory originals
			ChunkFreeWalker free;
			for (uint32 n = 0; n < count; n++)
					walk(free, aclList[n]);
			
			// release the memory allocated for the list itself when we are done
			Allocator::standard().free (aclList);
	}
	
	
	*countp = count; // XXX/cs count becomes part of the blob
	*aclsLength = acls_length;
	*acls = acls_data;
	Server::releaseWhenDone(acls_data);
	END_IPC(CSP)
}

kern_return_t ucsp_server_changeAcl(UCSP_ARGS, AclKind kind, IPCKeyHandle key,
	DATA_IN(cred), CSSM_ACL_EDIT_MODE mode, IPCGenericHandle handle,
	DATA_IN(acl))
{
	BEGIN_IPC
	CopyOutAccessCredentials creds(cred, credLength);
	CopyOutAclEntryInput entryacl(acl, aclLength);

	Server::aclBearer(kind, key).changeAcl(AclEdit(mode, handle, entryacl), creds);
	END_IPC(CSP)
}


//
// Login/Logout
//
kern_return_t ucsp_server_login(UCSP_ARGS, DATA_IN(cred), DATA_IN(name))
{
	BEGIN_IPC
	CopyOutAccessCredentials creds(cred, credLength);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END_IPC(CSP)
}

kern_return_t ucsp_server_logout(UCSP_ARGS)
{
	BEGIN_IPC
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END_IPC(CSP)
}


//
// Miscellaneous CSP-related calls
//
kern_return_t ucsp_server_getStatistics(UCSP_ARGS, uint32 ssid, CSSM_CSP_OPERATIONAL_STATISTICS *statistics)
{
	BEGIN_IPC
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END_IPC(CSP)
}

kern_return_t ucsp_server_getTime(UCSP_ARGS, uint32 ssid, CSSM_ALGORITHMS algorithm, DATA_OUT(data))
{
	BEGIN_IPC
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END_IPC(CSP)
}

kern_return_t ucsp_server_getCounter(UCSP_ARGS, uint32 ssid, DATA_OUT(data))
{
	BEGIN_IPC
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END_IPC(CSP)
}

kern_return_t ucsp_server_selfVerify(UCSP_ARGS, uint32 ssid)
{
	BEGIN_IPC
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END_IPC(CSP)
}


//
// Passthrough calls (separate for CSP and DL passthroughs)
//
kern_return_t ucsp_server_cspPassThrough(UCSP_ARGS, uint32 ssid, uint32 id, DATA_IN(context),
	IPCKeyHandle hKey, DATA_IN(inData), DATA_OUT(outData))
{
	BEGIN_IPC
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END_IPC(CSP)
}

kern_return_t ucsp_server_dlPassThrough(UCSP_ARGS, uint32 ssid, uint32 id,
	DATA_IN(inData), DATA_OUT(outData))
{
	BEGIN_IPC
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END_IPC(DL)
}


//
// Database key management.
// ExtractMasterKey looks vaguely like a key derivation operation, and is in fact
// presented by the CSPDL's CSSM layer as such.
//
kern_return_t ucsp_server_extractMasterKey(UCSP_ARGS, IPCDbHandle db, DATA_IN(context), IPCDbHandle sourceDb,
	DATA_IN(cred), DATA_IN(owner),
	uint32 usage, uint32 attrs, IPCKeyHandle *newKey, DATA_OUT(keyHeader))
{
	BEGIN_IPC
	CopyOutAccessCredentials creds(cred, credLength);
	CopyOutEntryAcl owneracl(owner, ownerLength);
	CopyOutContext ctx(context, contextLength);
	RefPointer<KeychainDatabase> keychain = Server::keychain(sourceDb);
	RefPointer<Key> masterKey = keychain->extractMasterKey(
		*Server::optionalDatabase(db, attrs & CSSM_KEYATTR_PERMANENT),
		creds, owneracl, usage, attrs);
	KeyHandle keyHandle;
	CssmKey::Header header;
	masterKey->returnKey(keyHandle, header);
	*newKey = keyHandle;
	if (!copyin(&header, reinterpret_cast<xdrproc_t> (xdr_CSSM_KEYHEADER), keyHeader, keyHeaderLength))
		CssmError::throwMe(CSSMERR_CSSM_MEMORY_ERROR);
	Server::releaseWhenDone(*keyHeader);
	END_IPC(CSP)
}


//
// Authorization subsystem support
//
kern_return_t ucsp_server_authorizationCreate(UCSP_ARGS,
	void *inRights, mach_msg_type_number_t inRightsLength,
	uint32 flags,
	void *inEnvironment, mach_msg_type_number_t inEnvironmentLength,
	AuthorizationBlob *authorization)
{
	BEGIN_IPC
	AuthorizationItemSet *authrights = NULL, *authenvironment = NULL;

	if (inRights && !copyout_AuthorizationItemSet(inRights, inRightsLength, &authrights))
		CssmError::throwMe(errAuthorizationInternal); // allocation error probably

	if (inEnvironment && !copyout_AuthorizationItemSet(inEnvironment, inEnvironmentLength, &authenvironment))
	{
		free(authrights);
		CssmError::throwMe(errAuthorizationInternal); // allocation error probably
	}

	Authorization::AuthItemSet rights(authrights), environment(authenvironment);

	*rcode = connection.process().session().authCreate(rights, environment, 
		flags, *authorization, auditToken);

	// @@@ safe-guard against code throw()ing in here

	if (authrights)
		free(authrights);

	if (authenvironment)
		free(authenvironment);
	
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationRelease(UCSP_ARGS,
	AuthorizationBlob authorization, uint32 flags)
{
	BEGIN_IPC
	connection.process().session().authFree(authorization, flags);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationCopyRights(UCSP_ARGS,
	AuthorizationBlob authorization,
	void *inRights, mach_msg_type_number_t inRightsLength,
	uint32 flags,
	void *inEnvironment, mach_msg_type_number_t inEnvironmentLength,
	void **result, mach_msg_type_number_t *resultLength)
{
	BEGIN_IPC
	AuthorizationItemSet *authrights = NULL, *authenvironment = NULL;

	if (inRights && !copyout_AuthorizationItemSet(inRights, inRightsLength, &authrights))
		CssmError::throwMe(errAuthorizationInternal); // allocation error probably

	if (inEnvironment && !copyout_AuthorizationItemSet(inEnvironment, inEnvironmentLength, &authenvironment))
	{
		free(authrights);
		CssmError::throwMe(errAuthorizationInternal); // allocation error probably
	}

	Authorization::AuthItemSet rights(authrights), environment(authenvironment), grantedRights;
	*rcode = Session::authGetRights(authorization, rights, environment, flags, grantedRights);

	// @@@ safe-guard against code throw()ing in here

	if (authrights)
		free(authrights);

	if (authenvironment)
		free(authenvironment);
	
	if (result && resultLength)
	{
		AuthorizationItemSet *copyout = grantedRights.copy();
		if (!copyin_AuthorizationItemSet(copyout, result, resultLength))
		{
			free(copyout);
			CssmError::throwMe(errAuthorizationInternal);
		}
		free(copyout);
		Server::releaseWhenDone(*result);
	}
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationCopyInfo(UCSP_ARGS,
	AuthorizationBlob authorization,
	AuthorizationString tag,
	void **info, mach_msg_type_number_t *infoLength)
{
	BEGIN_IPC
    Authorization::AuthItemSet infoSet;
    *info = NULL;
    *infoLength = 0;
    *rcode = connection.process().session().authGetInfo(authorization,
        tag[0] ? tag : NULL, infoSet);
    if (*rcode == noErr)
	{
		AuthorizationItemSet *copyout = infoSet.copy();
		if (!copyin_AuthorizationItemSet(copyout, info, infoLength))
		{
			free(copyout);
			CssmError::throwMe(errAuthorizationInternal);
		}
		free(copyout);
        Server::releaseWhenDone(*info);
    }
    END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationExternalize(UCSP_ARGS,
	AuthorizationBlob authorization, AuthorizationExternalForm *extForm)
{
	BEGIN_IPC
	*rcode = connection.process().session().authExternalize(authorization, *extForm);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationInternalize(UCSP_ARGS,
	AuthorizationExternalForm extForm, AuthorizationBlob *authorization)
{
	BEGIN_IPC
	*rcode = connection.process().session().authInternalize(extForm, *authorization);
	END_IPC(CSSM)
}


//
// Session management subsystem
//
kern_return_t ucsp_server_getSessionInfo(UCSP_ARGS,
    SecuritySessionId *sessionId, SessionAttributeBits *attrs)
{
	BEGIN_IPC
    Session &session = Session::find(*sessionId);
    *sessionId = session.handle();
    *attrs = session.attributes();
	END_IPC(CSSM)
}

kern_return_t ucsp_server_setupSession(UCSP_ARGS,
    SessionCreationFlags flags, SessionAttributeBits attrs)
{
	BEGIN_IPC
	Server::process().session().setupAttributes(flags, attrs);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_setSessionDistinguishedUid(UCSP_ARGS,
	SecuritySessionId sessionId, uid_t user)
{
	BEGIN_IPC
	Session::find<DynamicSession>(sessionId).originatorUid(user);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_getSessionDistinguishedUid(UCSP_ARGS,
	SecuritySessionId sessionId, uid_t *user)
{
	BEGIN_IPC
	*user = Session::find(sessionId).originatorUid();
	END_IPC(CSSM)
}

kern_return_t ucsp_server_setSessionUserPrefs(UCSP_ARGS, SecuritySessionId sessionId, DATA_IN(userPrefs))
{
	BEGIN_IPC
	CFRef<CFDataRef> data(CFDataCreate(NULL, (UInt8 *)userPrefs, userPrefsLength));

	if (!data)
	{
		*rcode = errSessionValueNotSet;
		return 0;
	}

	Session::find<DynamicSession>(sessionId).setUserPrefs(data);
	*rcode = 0;

	END_IPC(CSSM)
}



//
// Notification core subsystem
//

kern_return_t ucsp_server_postNotification(UCSP_ARGS, uint32 domain, uint32 event,
	DATA_IN(data), uint32 sequence)
{
	BEGIN_IPC
		Listener::notify(domain, event, sequence, DATA(data));
	END_IPC(CSSM)
}


//
// AuthorizationDB modification
//
kern_return_t ucsp_server_authorizationdbGet(UCSP_ARGS, const char *rightname, DATA_OUT(rightDefinition))
{
	BEGIN_IPC
	CFDictionaryRef rightDict;

	*rcode = connection.process().session().authorizationdbGet(rightname, &rightDict);

	if (!*rcode && rightDict)
	{
		CFRef<CFDataRef> data(CFPropertyListCreateXMLData (NULL, rightDict));
		CFRelease(rightDict);
		if (!data)
			return errAuthorizationInternal;
	
		// @@@ copy data to avoid having to do a delayed cfrelease
		mach_msg_type_number_t length = CFDataGetLength(data);
		void *xmlData = Allocator::standard().malloc(length);
		memcpy(xmlData, CFDataGetBytePtr(data), length);
		Server::releaseWhenDone(xmlData);
	
		*rightDefinition = xmlData;
		*rightDefinitionLength = length;
	}
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationdbSet(UCSP_ARGS, AuthorizationBlob authorization, const char *rightname, DATA_IN(rightDefinition))
{
	BEGIN_IPC
	CFRef<CFDataRef> data(CFDataCreate(NULL, (UInt8 *)rightDefinition, rightDefinitionLength));

	if (!data)
		return errAuthorizationInternal;

	CFRef<CFDictionaryRef> rightDefinition(static_cast<CFDictionaryRef>(CFPropertyListCreateFromXMLData(NULL, data, kCFPropertyListImmutable, NULL)));

	if (!rightDefinition || (CFGetTypeID(rightDefinition) != CFDictionaryGetTypeID()))
		return errAuthorizationInternal;

	*rcode = connection.process().session().authorizationdbSet(authorization, rightname, rightDefinition);

	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationdbRemove(UCSP_ARGS, AuthorizationBlob authorization, const char *rightname)
{
	BEGIN_IPC
	*rcode = connection.process().session().authorizationdbRemove(authorization, rightname);
	END_IPC(CSSM)
}


//
// Miscellaneous administrative functions
//
kern_return_t ucsp_server_addCodeEquivalence(UCSP_ARGS, DATA_IN(oldHash), DATA_IN(newHash),
	const char *name, boolean_t forSystem)
{
	BEGIN_IPC
	Server::codeSignatures().addLink(DATA(oldHash), DATA(newHash), name, forSystem);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_removeCodeEquivalence(UCSP_ARGS, DATA_IN(hash),
	const char *name, boolean_t forSystem)
{
	BEGIN_IPC
	Server::codeSignatures().removeLink(DATA(hash), name, forSystem);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_setAlternateSystemRoot(UCSP_ARGS, const char *root)
{
	BEGIN_IPC
#if defined(NDEBUG)
	if (connection.process().uid() != 0)
		CssmError::throwMe(CSSM_ERRCODE_OS_ACCESS_DENIED);
#endif //NDEBUG
	Server::codeSignatures().open((string(root) + EQUIVALENCEDBPATH).c_str());
	END_IPC(CSSM)
}


//
// Child check-in service.
// Note that this isn't using the standard argument pattern.
//
kern_return_t ucsp_server_childCheckIn(mach_port_t serverPort,
	mach_port_t servicePort, mach_port_t taskPort)
{
	BEGIN_IPCS
		ServerChild::checkIn(servicePort, TaskPort(taskPort).pid());
	END_IPCS(mach_port_deallocate(mach_task_self(), taskPort))
}


//
// Code Signing Hosting registration.
// Note that the Code Signing Proxy facility (implementing the "cshosting"
// IPC protocol) is elsewhere.
//
kern_return_t ucsp_server_registerHosting(UCSP_ARGS, mach_port_t hostingPort, uint32 flags)
{
	BEGIN_IPC
	connection.process().registerCodeSigning(hostingPort, flags);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_hostingPort(UCSP_ARGS, pid_t hostPid, mach_port_t *hostingPort)
{
	BEGIN_IPC
	if (RefPointer<Process> process = Server::active().findPid(hostPid))
		*hostingPort = process->hostingPort();
	else
		*hostingPort = MACH_PORT_NULL;
	secdebug("hosting", "hosting port for for pid=%d is port %d", hostPid, *hostingPort);
	END_IPC(CSSM)
}


kern_return_t ucsp_server_setGuest(UCSP_ARGS, SecGuestRef guest, SecCSFlags flags)
{
	BEGIN_IPC
	connection.guestRef(guest, flags);
	END_IPC(CSSM)
}


kern_return_t ucsp_server_createGuest(UCSP_ARGS, SecGuestRef host,
	uint32_t status, const char *path, DATA_IN(attributes), SecCSFlags flags, SecGuestRef *newGuest)
{
	BEGIN_IPC
	*newGuest = connection.process().createGuest(host, status, path, DATA(attributes), flags);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_setGuestStatus(UCSP_ARGS, SecGuestRef guest,
	uint32_t status, DATA_IN(attributes))
{
	BEGIN_IPC
	connection.process().setGuestStatus(guest, status, DATA(attributes));
	END_IPC(CSSM)
}

kern_return_t ucsp_server_removeGuest(UCSP_ARGS, SecGuestRef host, SecGuestRef guest)
{
	BEGIN_IPC
	connection.process().removeGuest(host, guest);
	END_IPC(CSSM)
}
