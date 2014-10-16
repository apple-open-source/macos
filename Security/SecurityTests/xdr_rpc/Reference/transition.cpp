/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
#include <securityd_client/ucsp.h>
#include "server.h"
#include "session.h"
#include "database.h"
#include "kcdatabase.h"
#include "tokendatabase.h"
#include "kckey.h"
#include "transwalkers.h"
#include "child.h"
#include <mach/mach_error.h>
#include "securityd_data_saver.h"		// for SecuritydDataSave

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFPropertyList.h>


//
// Bracket Macros
//
#define UCSP_ARGS	mach_port_t servicePort, mach_port_t replyPort, audit_token_t auditToken, \
                    CSSM_RETURN *rcode
#define CONTEXT_ARGS Context context, Pointer contextBase, Context::Attr *attributes, mach_msg_type_number_t attrSize

#define BEGIN_IPCN	*rcode = CSSM_OK; try {
#define BEGIN_IPC	BEGIN_IPCN RefPointer<Connection> connRef(&Server::connection(replyPort)); \
Connection &connection __attribute__((unused)) = *connRef;
#define END_IPC(base)	END_IPCN(base) Server::requestComplete(); return KERN_SUCCESS;
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
#define OPTDATA(base)	(base ? &CssmData(base, base##Length) : NULL)

#define SSBLOB(Type, name) makeBlob<Type>(DATA(name))

#define COPY_IN(type,name)	type *name, mach_msg_type_number_t name##Length, type *name##Base
#define COPY_OUT(type,name)	\
	type **name, mach_msg_type_number_t *name##Length, type **name##Base
	

using LowLevelMemoryUtilities::increment;
using LowLevelMemoryUtilities::difference;


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


//
// Common database operations
//
kern_return_t ucsp_server_authenticateDb(UCSP_ARGS, DbHandle db,
	CSSM_DB_ACCESS_TYPE accessType, COPY_IN(AccessCredentials, cred))
{
	BEGIN_IPC
    secdebug("dl", "authenticateDb");
    relocate(cred, credBase, credLength);
	// ignoring accessType
    Server::database(db)->authenticate(accessType, cred);
	END_IPC(DL)
}

kern_return_t ucsp_server_releaseDb(UCSP_ARGS, DbHandle db)
{
	BEGIN_IPC
	connection.process().kill(*Server::database(db));
	END_IPC(DL)
}


kern_return_t ucsp_server_getDbName(UCSP_ARGS, DbHandle db, char name[PATH_MAX])
{
	BEGIN_IPC
	string result = Server::database(db)->dbName();
	assert(result.length() < PATH_MAX);
	memcpy(name, result.c_str(), result.length() + 1);
	END_IPC(DL)
}

kern_return_t ucsp_server_setDbName(UCSP_ARGS, DbHandle db, const char *name)
{
	BEGIN_IPC
	Server::database(db)->dbName(name);
	END_IPC(DL)
}


//
// External database interface
//
kern_return_t ucsp_server_openToken(UCSP_ARGS, uint32 ssid, FilePath name,
	COPY_IN(AccessCredentials, accessCredentials), DbHandle *db)
{
	BEGIN_IPC
	relocate(accessCredentials, accessCredentialsBase, accessCredentialsLength);
	*db = (new TokenDatabase(ssid, connection.process(), name, accessCredentials))->handle();
	END_IPC(DL)
}

kern_return_t ucsp_server_findFirst(UCSP_ARGS, DbHandle db,
	COPY_IN(CssmQuery, query),
	COPY_IN(CssmDbRecordAttributeData, inAttributes),
	COPY_OUT(CssmDbRecordAttributeData, outAttributes),
	boolean_t getData,
	DATA_OUT(data), KeyHandle *hKey, SearchHandle *hSearch, RecordHandle *hRecord)
{
	BEGIN_IPC
	relocate(query, queryBase, queryLength);
	SecuritydDataSave sds("/var/tmp/Query_findFirst");
	sds.writeQuery(query, queryLength);
	relocate(inAttributes, inAttributesBase, inAttributesLength);

	RefPointer<Database::Search> search;
	RefPointer<Database::Record> record;
	RefPointer<Key> key;
	CssmData outData; //OutputData outData(data, dataLength);
	CssmDbRecordAttributeData *outAttrs; mach_msg_type_number_t outAttrsLength;
	Server::database(db)->findFirst(*query, inAttributes, inAttributesLength,
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

		// return attributes (assumes relocated flat blob)
		flips(outAttrs, outAttributes, outAttributesBase);
		*outAttributesLength = outAttrsLength;

		// return data (temporary fix)
		if (getData) {
			*data = outData.data();
			*dataLength = outData.length();
		}
	}
	END_IPC(DL)
}

kern_return_t ucsp_server_findNext(UCSP_ARGS, SearchHandle hSearch,
	COPY_IN(CssmDbRecordAttributeData, inAttributes),
	COPY_OUT(CssmDbRecordAttributeData, outAttributes),
	boolean_t getData, DATA_OUT(data), KeyHandle *hKey,
	RecordHandle *hRecord)
{
	BEGIN_IPC
	relocate(inAttributes, inAttributesBase, inAttributesLength);
	RefPointer<Database::Search> search =
		Server::find<Database::Search>(hSearch, CSSMERR_DL_INVALID_RESULTS_HANDLE);
	RefPointer<Database::Record> record;
	RefPointer<Key> key;
	CssmData outData; //OutputData outData(data, dataLength);
	CssmDbRecordAttributeData *outAttrs; mach_msg_type_number_t outAttrsLength;
	search->database().findNext(search, inAttributes, inAttributesLength,
		getData ? &outData : NULL, key, record, outAttrs, outAttrsLength);
	
	// handle nothing-found case without exceptions
	if (!record) {
		*hRecord = noRecord;
		*hKey = noKey;
	} else {
		// return handles
		*hRecord = record->handle();
		*hKey = key ? key->handle() : noKey;

		// return attributes (assumes relocated flat blob)
		flips(outAttrs, outAttributes, outAttributesBase);
		*outAttributesLength = outAttrsLength;

		// return data (temporary fix)
		if (getData) {
			*data = outData.data();
			*dataLength = outData.length();
		}
	}
	END_IPC(DL)
}

kern_return_t ucsp_server_findRecordHandle(UCSP_ARGS, RecordHandle hRecord,
	COPY_IN(CssmDbRecordAttributeData, inAttributes),
	COPY_OUT(CssmDbRecordAttributeData, outAttributes),
	boolean_t getData, DATA_OUT(data), KeyHandle *hKey)
{
	BEGIN_IPC
	relocate(inAttributes, inAttributesBase, inAttributesLength);
	RefPointer<Database::Record> record =
		Server::find<Database::Record>(hRecord, CSSMERR_DL_INVALID_RECORD_UID);
	RefPointer<Key> key;
	CssmData outData; //OutputData outData(data, dataLength);
	CssmDbRecordAttributeData *outAttrs; mach_msg_type_number_t outAttrsLength;
	record->database().findRecordHandle(record, inAttributes, inAttributesLength,
		getData ? &outData : NULL, key, outAttrs, outAttrsLength);
	
	// return handles
	*hKey = key ? key->handle() : noKey;

	// return attributes (assumes relocated flat blob)
	flips(outAttrs, outAttributes, outAttributesBase);
	*outAttributesLength = outAttrsLength;

	// return data (temporary fix)
	if (getData) {
		*data = outData.data();
		*dataLength = outData.length();
	}
	END_IPC(DL)
}

kern_return_t ucsp_server_insertRecord(UCSP_ARGS, DbHandle db, CSSM_DB_RECORDTYPE recordType,
	COPY_IN(CssmDbRecordAttributeData, attributes), DATA_IN(data), RecordHandle *record)
{
	BEGIN_IPC
	relocate(attributes, attributesBase, attributesLength);
	Server::database(db)->insertRecord(recordType, attributes, attributesLength,
		DATA(data), *record);
	END_IPC(DL)
}

kern_return_t ucsp_server_modifyRecord(UCSP_ARGS, DbHandle db, RecordHandle *hRecord,
	CSSM_DB_RECORDTYPE recordType, COPY_IN(CssmDbRecordAttributeData, attributes),
	boolean_t setData, DATA_IN(data), CSSM_DB_MODIFY_MODE modifyMode)
{
	BEGIN_IPC
	relocate(attributes, attributesBase, attributesLength);
	CssmData newData(DATA(data));
	RefPointer<Database::Record> record =
		Server::find<Database::Record>(*hRecord, CSSMERR_DL_INVALID_RECORD_UID);
	Server::database(db)->modifyRecord(recordType, record, attributes, attributesLength,
		setData ? &newData : NULL, modifyMode);
	// note that the record handle presented to the client never changes here
	// (we could, but have no reason to - our record handles are just always up to date)
	END_IPC(DL)
}

kern_return_t ucsp_server_deleteRecord(UCSP_ARGS, DbHandle db, RecordHandle hRecord)
{
	BEGIN_IPC
	Server::database(db)->deleteRecord(
		Server::find<Database::Record>(hRecord, CSSMERR_DL_INVALID_RECORD_UID));
	END_IPC(DL)
}

kern_return_t ucsp_server_releaseSearch(UCSP_ARGS, SearchHandle hSearch)
{
	BEGIN_IPC
	RefPointer<Database::Search> search = Server::find<Database::Search>(hSearch, 0);
	search->database().releaseSearch(*search);
	END_IPC(DL)
}

kern_return_t ucsp_server_releaseRecord(UCSP_ARGS, RecordHandle hRecord)
{
	BEGIN_IPC
	RefPointer<Database::Record> record = Server::find<Database::Record>(hRecord, 0);
	record->database().releaseRecord(*record);
	END_IPC(DL)
}

kern_return_t ucsp_server_getRecordFromHandle(UCSP_ARGS, RecordHandle record,
	COPY_IN(CssmDbRecordAttributeData, inAttributes),
	COPY_OUT(CssmDbRecordAttributeData, outAttributes),
	boolean_t getData,
	DATA_OUT(data))
{
	BEGIN_IPC
    secdebug("dl", "getRecordFromHandle");
	relocate(inAttributes, inAttributesBase, inAttributesLength);
	// @@@
	END_IPC(DL)
}


//
// Internal database management
//
kern_return_t ucsp_server_createDb(UCSP_ARGS, DbHandle *db,
	COPY_IN(DLDbFlatIdentifier, ident),
    COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
    DBParameters params)
{
	BEGIN_IPC
	relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
    relocate(ident, identBase, identLength);
	*db = (new KeychainDatabase(*ident, params, connection.process(), cred, owner))->handle();
	END_IPC(DL)
}

// keychain synchronization
// @@@  caller should be required to call decodeDb() to get a DbHandle
//      instead of passing the blob itself
kern_return_t ucsp_server_cloneDbForSync(UCSP_ARGS, DATA_IN(blob), 
	DbHandle srcDb, DATA_IN(agentData), DbHandle *newDb)
{
	BEGIN_IPC
	RefPointer<KeychainDatabase> srcKC = Server::keychain(srcDb);
	*newDb = (new KeychainDatabase(*srcKC, connection.process(),
		SSBLOB(DbBlob, blob), DATA(agentData)))->handle();
	END_IPC(DL)
}

kern_return_t ucsp_server_commitDbForSync(UCSP_ARGS, DbHandle srcDb,
    DbHandle cloneDb, DATA_OUT(blob))
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

kern_return_t ucsp_server_decodeDb(UCSP_ARGS, DbHandle *db,
    COPY_IN(DLDbFlatIdentifier, ident), COPY_IN(AccessCredentials, cred), DATA_IN(blob))
{
	BEGIN_IPC
	relocate(cred, credBase, credLength);
    relocate(ident, identBase, identLength);
	*db = (new KeychainDatabase(*ident, SSBLOB(DbBlob, blob),
        connection.process(), cred))->handle();
	END_IPC(DL)
}

kern_return_t ucsp_server_encodeDb(UCSP_ARGS, DbHandle db, DATA_OUT(blob))
{
	BEGIN_IPC
    DbBlob *dbBlob = Server::keychain(db)->blob();	// memory owned by database
    *blob = dbBlob;
    *blobLength = dbBlob->length();
	END_IPC(DL)
}

kern_return_t ucsp_server_setDbParameters(UCSP_ARGS, DbHandle db, DBParameters params)
{
	BEGIN_IPC
	Server::keychain(db)->setParameters(params);
	END_IPC(DL)
}

kern_return_t ucsp_server_getDbParameters(UCSP_ARGS, DbHandle db, DBParameters *params)
{
	BEGIN_IPC
	Server::keychain(db)->getParameters(*params);
	END_IPC(DL)
}

kern_return_t ucsp_server_changePassphrase(UCSP_ARGS, DbHandle db,
    COPY_IN(AccessCredentials, cred))
{
	BEGIN_IPC
    relocate(cred, credBase, credLength);
	Server::keychain(db)->changePassphrase(cred);
	END_IPC(DL)
}

kern_return_t ucsp_server_lockAll (UCSP_ARGS, boolean_t)
{
	BEGIN_IPC
	connection.session().processLockAll();
	END_IPC(DL)
}

kern_return_t ucsp_server_unlockDb(UCSP_ARGS, DbHandle db)
{
	BEGIN_IPC
	Server::keychain(db)->unlockDb();
	END_IPC(DL)
}

kern_return_t ucsp_server_unlockDbWithPassphrase(UCSP_ARGS, DbHandle db, DATA_IN(passphrase))
{
	BEGIN_IPC
	Server::keychain(db)->unlockDb(DATA(passphrase));
	END_IPC(DL)
}

kern_return_t ucsp_server_isLocked(UCSP_ARGS, DbHandle db, boolean_t *locked)
{
    BEGIN_IPC
    *locked = Server::database(db)->isLocked();
    END_IPC(DL)
}


//
// Key management
//
kern_return_t ucsp_server_encodeKey(UCSP_ARGS, KeyHandle keyh, DATA_OUT(blob),
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

kern_return_t ucsp_server_decodeKey(UCSP_ARGS, KeyHandle *keyh, CssmKey::Header *header,
	DbHandle db, DATA_IN(blob))
{
	BEGIN_IPC
    RefPointer<Key> key = new KeychainKey(*Server::keychain(db), SSBLOB(KeyBlob, blob));
    key->returnKey(*keyh, *header);
	flip(*header);
	END_IPC(CSP)
}

// keychain synchronization
kern_return_t ucsp_server_recodeKey(UCSP_ARGS, DbHandle oldDb, KeyHandle keyh, 
	DbHandle newDb, DATA_OUT(newBlob))
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

kern_return_t ucsp_server_releaseKey(UCSP_ARGS, KeyHandle keyh)
{
	BEGIN_IPC
	RefPointer<Key> key = Server::key(keyh);
	key->database().releaseKey(*key);
	END_IPC(CSP)
}

kern_return_t ucsp_server_queryKeySizeInBits(UCSP_ARGS, KeyHandle keyh, CSSM_KEY_SIZE *length)
{
	BEGIN_IPC
	RefPointer<Key> key = Server::key(keyh);
	key->database().queryKeySizeInBits(*key, CssmKeySize::overlay(*length));
	END_IPC(CSP)
}

kern_return_t ucsp_server_getOutputSize(UCSP_ARGS, CONTEXT_ARGS, KeyHandle keyh,
    uint32 inputSize, boolean_t encrypt, uint32 *outputSize)
{
    BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	RefPointer<Key> key = Server::key(keyh);
    key->database().getOutputSize(context, *key, inputSize, encrypt, *outputSize);
    END_IPC(CSP)
}

kern_return_t ucsp_server_getKeyDigest(UCSP_ARGS, KeyHandle key, DATA_OUT(digest))
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
kern_return_t ucsp_server_generateSignature(UCSP_ARGS, CONTEXT_ARGS, KeyHandle keyh,
        CSSM_ALGORITHMS signOnlyAlgorithm, DATA_IN(data), DATA_OUT(signature))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	RefPointer<Key> key = Server::key(keyh);
	OutputData sigData(signature, signatureLength);
	key->database().generateSignature(context, *key, signOnlyAlgorithm,
		DATA(data), sigData);
	END_IPC(CSP)
}

kern_return_t ucsp_server_verifySignature(UCSP_ARGS, CONTEXT_ARGS, KeyHandle keyh,
		CSSM_ALGORITHMS verifyOnlyAlgorithm, DATA_IN(data), DATA_IN(signature))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	RefPointer<Key> key = Server::key(keyh);
	key->database().verifySignature(context, *key, verifyOnlyAlgorithm,
		DATA(data), DATA(signature));
	END_IPC(CSP)
}

kern_return_t ucsp_server_generateMac(UCSP_ARGS, CONTEXT_ARGS, KeyHandle keyh,
		DATA_IN(data), DATA_OUT(mac))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	RefPointer<Key> key = Server::key(keyh);
	OutputData macData(mac, macLength);
	key->database().generateMac(context, *key, DATA(data), macData);
	END_IPC(CSP)
}

kern_return_t ucsp_server_verifyMac(UCSP_ARGS, CONTEXT_ARGS, KeyHandle keyh,
		DATA_IN(data), DATA_IN(mac))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	RefPointer<Key> key = Server::key(keyh);
	key->database().verifyMac(context, *key, DATA(data), DATA(mac));
	END_IPC(CSP)
}


//
// Encryption/Decryption
//
kern_return_t ucsp_server_encrypt(UCSP_ARGS, CONTEXT_ARGS, KeyHandle keyh,
	DATA_IN(clear), DATA_OUT(cipher))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	RefPointer<Key> key = Server::key(keyh);
	OutputData cipherOut(cipher, cipherLength);
	key->database().encrypt(context, *key, DATA(clear), cipherOut);
	END_IPC(CSP)
}

kern_return_t ucsp_server_decrypt(UCSP_ARGS, CONTEXT_ARGS, KeyHandle keyh,
	DATA_IN(cipher), DATA_OUT(clear))
{
	BEGIN_IPC
	SecuritydDataSave sds("/var/tmp/securityd_Context_decrypt");	// XXX/gh  get sample Context for XDR testing
	relocate(context, contextBase, attributes, attrSize);
	// save attributes base addr for backwards compatibility
	intptr_t attraddr = reinterpret_cast<intptr_t>(&context->ContextAttributes);
	sds.writeContext(&context, attraddr, attrSize);
	RefPointer<Key> key = Server::key(keyh);
	OutputData clearOut(clear, clearLength);
	key->database().decrypt(context, *key, DATA(cipher), clearOut);
	END_IPC(CSP)
}


//
// Key generation
//
kern_return_t ucsp_server_generateKey(UCSP_ARGS, DbHandle db, CONTEXT_ARGS,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	uint32 usage, uint32 attrs, KeyHandle *newKey, CssmKey::Header *newHeader)
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
	//@@@ preliminary interpretation - will get "type handle"
	RefPointer<Database> database =
		Server::optionalDatabase(db, attrs & CSSM_KEYATTR_PERMANENT);
	RefPointer<Key> key;
	database->generateKey(context, cred, owner, usage, attrs, key);
    key->returnKey(*newKey, *newHeader);
	flip(*newHeader);
	END_IPC(CSP)
}

kern_return_t ucsp_server_generateKeyPair(UCSP_ARGS, DbHandle db, CONTEXT_ARGS,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	uint32 pubUsage, uint32 pubAttrs, uint32 privUsage, uint32 privAttrs,
	KeyHandle *pubKey, CssmKey::Header *pubHeader, KeyHandle *privKey, CssmKey::Header *privHeader)
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
	RefPointer<Database> database =
		Server::optionalDatabase(db, (privAttrs | pubAttrs) & CSSM_KEYATTR_PERMANENT);
	RefPointer<Key> pub, priv;
	database->generateKey(context, cred, owner,
		pubUsage, pubAttrs, privUsage, privAttrs, pub, priv);
    pub->returnKey(*pubKey, *pubHeader);
	flip(*pubHeader);
    priv->returnKey(*privKey, *privHeader);
	flip(*privHeader);
	END_IPC(CSP)
}


//
// Key wrapping and unwrapping
//
kern_return_t ucsp_server_wrapKey(UCSP_ARGS, CONTEXT_ARGS, KeyHandle hWrappingKey,
	COPY_IN(AccessCredentials, cred), KeyHandle hKeyToBeWrapped,
	DATA_IN(descriptiveData), CssmKey *wrappedKey, DATA_OUT(keyData))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase, credLength);
	RefPointer<Key> subjectKey = Server::key(hKeyToBeWrapped);
	RefPointer<Key> wrappingKey = Server::optionalKey(hWrappingKey);
	if ((context.algorithm() == CSSM_ALGID_NONE && subjectKey->attribute(CSSM_KEYATTR_SENSITIVE))
		|| !subjectKey->attribute(CSSM_KEYATTR_EXTRACTABLE))
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
	pickDb(subjectKey, wrappingKey)->wrapKey(context, cred, wrappingKey, *subjectKey, DATA(descriptiveData), *wrappedKey);

    // transmit key data back as a separate blob
	OutputData keyDatas(keyData, keyDataLength);
	keyDatas = wrappedKey->keyData();
	flip(*wrappedKey);
	END_IPC(CSP)
}

kern_return_t ucsp_server_unwrapKey(UCSP_ARGS, DbHandle db, CONTEXT_ARGS,
	KeyHandle hWrappingKey, COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	KeyHandle hPublicKey, CssmKey wrappedKey, DATA_IN(wrappedKeyData),
	CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs, DATA_OUT(descriptiveData),
    KeyHandle *newKey, CssmKey::Header *newHeader)
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	flip(wrappedKey);
	wrappedKey.keyData() = DATA(wrappedKeyData);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
	OutputData descriptiveDatas(descriptiveData, descriptiveDataLength);
	RefPointer<Key> wrappingKey = Server::optionalKey(hWrappingKey);
    RefPointer<Key> unwrappedKey;
	pickDb(Server::optionalDatabase(db), wrappingKey)->unwrapKey(context, cred, owner,
		wrappingKey, Server::optionalKey(hPublicKey),
		usage, attrs, wrappedKey, unwrappedKey, descriptiveDatas);
	unwrappedKey->returnKey(*newKey, *newHeader);
	flip(*newHeader);
	END_IPC(CSP)
}


//
// Key derivation.
//
// Note that the "param" argument can have structure. The walker for the
// (artificial) POD CssmDeriveData handles those that are known; if you add
// an algorithm with structured param, you need to add a case there.
//
kern_return_t ucsp_server_deriveKey(UCSP_ARGS, DbHandle db, CONTEXT_ARGS, KeyHandle hKey,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
    COPY_IN(CssmDeriveData, paramInput), DATA_OUT(paramOutput),
	uint32 usage, uint32 attrs, KeyHandle *newKey, CssmKey::Header *newHeader)
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
	relocate(paramInput, paramInputBase, paramInputLength);
	if (!paramInput || paramInput->algorithm != context.algorithm())
		CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);	// client layer fault
    
	RefPointer<Database> database =
		Server::optionalDatabase(db, attrs & CSSM_KEYATTR_PERMANENT);
	RefPointer<Key> key = Server::optionalKey(hKey);
	CssmData *param = paramInput ? &paramInput->baseData : NULL;
    RefPointer<Key> derivedKey;
	pickDb(Server::optionalDatabase(db, attrs & CSSM_KEYATTR_PERMANENT),
		key)->deriveKey(context, key, cred, owner, param, usage, attrs, derivedKey);
	derivedKey->returnKey(*newKey, *newHeader);
	flip(*newHeader);
	if (param && param->length()) {
        if (!param->data())	// CSP screwed up
            CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
        if (paramInputLength)		// using incoming buffer; make a copy
            *param = CssmAutoData(Server::csp().allocator(), *param).release();
        OutputData(paramOutput, paramOutputLength) = *param;	// return the data
    }
	END_IPC(CSP)
}


//
// Random generation
//
kern_return_t ucsp_server_generateRandom(UCSP_ARGS, uint32 ssid, CONTEXT_ARGS, DATA_OUT(data))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	if (ssid)
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	
	// default version (use /dev/random)
	Allocator &allocator = Allocator::standard(Allocator::sensitive);
	if (size_t bytes = context.getInt(CSSM_ATTRIBUTE_OUTPUT_SIZE)) {
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
kern_return_t ucsp_server_getOwner(UCSP_ARGS, AclKind kind, KeyHandle key,
	COPY_OUT(AclOwnerPrototype, ownerOut))
{
	BEGIN_IPC
	AclOwnerPrototype owner;
	Server::aclBearer(kind, key).getOwner(owner);	// allocates memory in owner
	Copier<AclOwnerPrototype> owners(&owner, Allocator::standard()); // make flat copy
	{ ChunkFreeWalker free; walk(free, owner); } // release chunked original
	*ownerOutLength = owners.length();
	flips(owners.value(), ownerOut, ownerOutBase);
	Server::releaseWhenDone(owners.keep()); // throw flat copy out when done
	END_IPC(CSP)
}

kern_return_t ucsp_server_setOwner(UCSP_ARGS, AclKind kind, KeyHandle key,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclOwnerPrototype, owner))
{
	BEGIN_IPC
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
	Server::aclBearer(kind, key).changeOwner(*owner, cred);
	END_IPC(CSP)
}

kern_return_t ucsp_server_getAcl(UCSP_ARGS, AclKind kind, KeyHandle key,
	boolean_t haveTag, const char *tag,
	uint32 *countp, COPY_OUT(AclEntryInfo, acls))
{
	BEGIN_IPC
	uint32 count;
	AclEntryInfo *aclList;
	Server::aclBearer(kind, key).getAcl(haveTag ? tag : NULL, count, aclList);
	*countp = count;
	Copier<AclEntryInfo> aclsOut(aclList, count); // make flat copy

	{	// release the chunked memory originals
		ChunkFreeWalker free;
		for (uint32 n = 0; n < count; n++)
			walk(free, aclList[n]);
		
		// release the memory allocated for the list itself when we are done
		Allocator::standard().free (aclList);
	}
	
	// set result (note: this is *almost* flips(), but on an array)
	*aclsLength = aclsOut.length();
	*acls = *aclsBase = aclsOut;
	if (flipClient()) {
		FlipWalker w;
		for (uint32 n = 0; n < count; n++)
			walk(w, (*acls)[n]);
		w.doFlips();
		Flippers::flip(*aclsBase);
	}
	SecuritydDataSave sds("/var/tmp/AclEntryInfo_getAcl");
	sds.writeAclEntryInfo(*acls, *aclsLength);
	Server::releaseWhenDone(aclsOut.keep());
	END_IPC(CSP)
}

kern_return_t ucsp_server_changeAcl(UCSP_ARGS, AclKind kind, KeyHandle key,
	COPY_IN(AccessCredentials, cred), CSSM_ACL_EDIT_MODE mode, CSSM_ACL_HANDLE handle,
	COPY_IN(AclEntryInput, acl))
{
	BEGIN_IPC
    relocate(cred, credBase, credLength);
	relocate(acl, aclBase, aclLength);
	SecuritydDataSave sds("/var/tmp/AclEntryInput_changeAcl");
	sds.writeAclEntryInput(acl, aclLength);
	Server::aclBearer(kind, key).changeAcl(AclEdit(mode, handle, acl), cred);
	END_IPC(CSP)
}


//
// Login/Logout
//
kern_return_t ucsp_server_login(UCSP_ARGS, COPY_IN(AccessCredentials, cred), DATA_IN(name))
{
	BEGIN_IPC
	relocate(cred, credBase, credLength);
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
kern_return_t ucsp_server_getStatistics(UCSP_ARGS, uint32 ssid, CSPOperationalStatistics *statistics)
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
kern_return_t ucsp_server_cspPassThrough(UCSP_ARGS, uint32 ssid, uint32 id, CONTEXT_ARGS,
	KeyHandle hKey, DATA_IN(inData), DATA_OUT(outData))
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
kern_return_t ucsp_server_extractMasterKey(UCSP_ARGS, DbHandle db, CONTEXT_ARGS, DbHandle sourceDb,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	uint32 usage, uint32 attrs, KeyHandle *newKey, CssmKey::Header *newHeader)
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
	RefPointer<KeychainDatabase> keychain = Server::keychain(sourceDb);
	RefPointer<Key> masterKey = keychain->extractMasterKey(
		*Server::optionalDatabase(db, attrs & CSSM_KEYATTR_PERMANENT),
		cred, owner, usage, attrs);
	masterKey->returnKey(*newKey, *newHeader);
	flip(*newHeader);
	END_IPC(CSP)
}


//
// Authorization subsystem support
//
kern_return_t ucsp_server_authorizationCreate(UCSP_ARGS,
	COPY_IN(AuthorizationItemSet, inRights),
	uint32 flags,
	COPY_IN(AuthorizationItemSet, inEnvironment),
	AuthorizationBlob *authorization)
{
	BEGIN_IPC
	relocate(inRights, inRightsBase, inRightsLength);
	relocate(inEnvironment, inEnvironmentBase, inEnvironmentLength);
	Authorization::AuthItemSet rights(inRights), environment(inEnvironment);

	*rcode = connection.process().session().authCreate(rights, environment, 
		flags, *authorization, auditToken);
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
	COPY_IN(AuthorizationItemSet, inRights),
	uint32 flags,
	COPY_IN(AuthorizationItemSet, inEnvironment),
	COPY_OUT(AuthorizationItemSet, result))
{
	BEGIN_IPC
	relocate(inRights, inRightsBase, inRightsLength);
	relocate(inEnvironment, inEnvironmentBase, inEnvironmentLength);
	Authorization::AuthItemSet rights(inRights), environment(inEnvironment), grantedRights;
	*rcode = connection.process().session().authGetRights(authorization,
		rights, environment, flags, grantedRights);
	if (result && resultLength)
	{
		size_t resultSize;
		grantedRights.copy(*result, resultSize);
		*resultLength = resultSize;
		*resultBase = *result;
		flips(*result, result, resultBase);
		Server::releaseWhenDone(*result);
	}
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationCopyInfo(UCSP_ARGS,
	AuthorizationBlob authorization,
	AuthorizationString tag,
	COPY_OUT(AuthorizationItemSet, info))
{
	BEGIN_IPC
    Authorization::AuthItemSet infoSet;
    *info = *infoBase = NULL;
    *infoLength = 0;
    *rcode = connection.process().session().authGetInfo(authorization,
        tag[0] ? tag : NULL, infoSet);
    if (*rcode == noErr) {
		size_t infoSize;
		infoSet.copy(*info, infoSize);
		*infoLength = infoSize;
		*infoBase = *info;
		flips(*info, info, infoBase);
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
kern_return_t ucsp_server_requestNotification(UCSP_ARGS, mach_port_t receiver, uint32 domain, uint32 events)
{
    BEGIN_IPC
    connection.process().requestNotifications(receiver, domain, events);
    END_IPC(CSSM)
}

kern_return_t ucsp_server_stopNotification(UCSP_ARGS, mach_port_t receiver)
{
    BEGIN_IPC
    connection.process().stopNotifications(receiver);
    END_IPC(CSSM)
}

kern_return_t ucsp_server_postNotification(mach_port_t serverPort, uint32 domain, uint32 event, DATA_IN(data))
{
	BEGIN_IPCS
		Listener::notify(domain, event, DATA(data));
	END_IPCS()
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
