/*
 * Copyright (c) 2000-2008,2011-2013 Apple Inc. All Rights Reserved.
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
// transition - SecurityServer client library transition code.
//
// These are the functions that implement CssmClient methods in terms of
// MIG IPC client calls, plus their supporting machinery.
//
// WARNING! HERE BE DRAGONS!
// This code involves moderately arcane magic including (but not limited to)
// dancing macros paired off with self-maintaining stack objects. Don't take
// anything for granted! Be very afraid of ALL-CAPS names. Your best bet is
// probably to stick with the existing patterns.
//
// Dragons, the sequel.  You just don't go killing of that kind of prose, so
// we'll continue the saga here with a bit of an update.  In transitioning
// into securityd there are a couple of steps.  The current setup is there 
// to allow Security.framework to have 32 and 64 bit clients and either
// big or little endian.  Data is packaged up as hand-generated XDR, which
// means it's also in network byte-order.  
//
// CSSM_HANDLEs have remained longs in the 64 bit transition to keep the 
// optimization option open to allow cssm modules to hand back pointers as 
// handles.  Since we don't identify the client, handles across ipc will
// remain 32 bit.  Handles you see here are passed out by securityd, and
// are clipped and expanded in this layer (high bits always zero).
//
#include "sstransit.h"
#include "ucsp.h"
#include "ucsp_old.h"
#include <security_cdsa_client/cspclient.h>

#include <CommonCrypto/CommonRandom.h>
#include <securityd_client/xdr_auth.h>
#include <securityd_client/xdr_cssm.h>
#include <securityd_client/xdr_dldb.h>

namespace Security {
namespace SecurityServer {

using MachPlusPlus::check;
using MachPlusPlus::VMGuard;

//
// Common database interface
//
void ClientSession::authenticateDb(DbHandle db, CSSM_DB_ACCESS_TYPE type,
	const AccessCredentials *cred)
{
	// XXX/cs Leave it up to DatabaseAccessCredentials to rewrite it for now
    DatabaseAccessCredentials creds(cred, internalAllocator);
	CopyIn copy(creds.value(), reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	UCSP_CLIENT_IPC(authenticateDb, db, type, copy.data(), copy.length());
}


void ClientSession::releaseDb(DbHandle db)
{
	UCSP_CLIENT_IPC(releaseDb, db);
}


//
// External database interface
//
DbHandle ClientSession::openToken(uint32 ssid, const AccessCredentials *cred,
	const char *name)
{
	DbHandle db;
	DatabaseAccessCredentials creds(cred, internalAllocator);
	CopyIn copycreds(creds.value(), reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
    
    UCSP_CLIENT_IPC(openToken, ssid, name ? name : "", copycreds.data(), copycreds.length(), &db);
    
	return db;
}


RecordHandle ClientSession::insertRecord(DbHandle db,
						  CSSM_DB_RECORDTYPE recordType,
						  const CssmDbRecordAttributeData *attributes,
						  const CssmData *data)
{
	RecordHandle record;
	CopyIn db_record_attr_data(attributes, reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA));
    
    UCSP_CLIENT_IPC(insertRecord, db, recordType, db_record_attr_data.data(),
			(mach_msg_type_number_t)db_record_attr_data.length(), OPTIONALDATA(data), &record);
    
	return record;
}


void ClientSession::deleteRecord(DbHandle db, RecordHandle record)
{
	UCSP_CLIENT_IPC(deleteRecord, db, record);
}


void ClientSession::modifyRecord(DbHandle db, RecordHandle &record,
				  CSSM_DB_RECORDTYPE recordType,
				  const CssmDbRecordAttributeData *attributes,
				  const CssmData *data,
				  CSSM_DB_MODIFY_MODE modifyMode)
{
	CopyIn db_record_attr_data(attributes, reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA));
    
	UCSP_CLIENT_IPC(modifyRecord, db, &record, recordType, db_record_attr_data.data(),
			(mach_msg_type_number_t)db_record_attr_data.length(), data != NULL, OPTIONALDATA(data), modifyMode);
}

static
void copy_back_attribute_return_data(CssmDbRecordAttributeData *dest_attrs, CssmDbRecordAttributeData *source_attrs, Allocator &returnAllocator)
{
	assert(dest_attrs->size() == source_attrs->size());
	// global (per-record) fields
	dest_attrs->recordType(source_attrs->recordType());
	dest_attrs->semanticInformation(source_attrs->semanticInformation());
	
	// transfer data values (but not infos, which we keep in the original vector)
	for (uint32 n = 0; n < dest_attrs->size(); n++)
		dest_attrs->at(n).copyValues(source_attrs->at(n), returnAllocator);
}

RecordHandle ClientSession::findFirst(DbHandle db,
							  const CssmQuery &inQuery,
							  SearchHandle &hSearch,
							  CssmDbRecordAttributeData *attributes,
							  CssmData *data, KeyHandle &hKey)
{
	CopyIn query(&inQuery, reinterpret_cast<xdrproc_t>(xdr_CSSM_QUERY));
	CopyIn in_attr(attributes, reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA));
	void *out_attr_data = NULL, *out_data = NULL;
	mach_msg_size_t out_attr_length = 0, out_data_length = 0;
	RecordHandle ipcHRecord = 0;

	UCSP_CLIENT_IPC(findFirst, db,
			query.data(), query.length(), in_attr.data(), in_attr.length(),
			&out_attr_data, &out_attr_length, (data != NULL), &out_data, &out_data_length,
			&hKey, &hSearch, &ipcHRecord);
		
	if (ipcHRecord != 0)
	{
		CopyOut out_attrs(out_attr_data, out_attr_length, reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR), true);
		copy_back_attribute_return_data(attributes, reinterpret_cast<CssmDbRecordAttributeData*>(out_attrs.data()), returnAllocator);
	}
	
	// decode data from server as cssm_data or cssm_key (get data on keys returns cssm_key in data)
	CopyOut possible_key_in_data(out_data, out_data_length, reinterpret_cast<xdrproc_t>(xdr_CSSM_POSSIBLY_KEY_IN_DATA_PTR), true, data);
	
	return ipcHRecord;
}


RecordHandle ClientSession::findNext(SearchHandle hSearch,
							 CssmDbRecordAttributeData *attributes,
							 CssmData *data, KeyHandle &hKey)
{
	CopyIn in_attr(attributes, reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA));
	void *out_attr_data = NULL, *out_data = NULL;
	mach_msg_size_t out_attr_length = 0, out_data_length = 0;
	//DataOutput out_data(data, returnAllocator);
	RecordHandle ipcHRecord = 0;

	UCSP_CLIENT_IPC(findNext, hSearch,
			in_attr.data(), in_attr.length(), &out_attr_data, &out_attr_length,
			(data != NULL), &out_data, &out_data_length, &hKey, &ipcHRecord);

	if (ipcHRecord != 0)
	{
		CopyOut out_attrs(out_attr_data, out_attr_length, reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR), true);
		copy_back_attribute_return_data(attributes, reinterpret_cast<CssmDbRecordAttributeData*>(out_attrs.data()), returnAllocator);
	}

	// decode data from server as cssm_data or cssm_key (get data on keys returns cssm_key in data)
	CopyOut possible_key_in_data(out_data, out_data_length, reinterpret_cast<xdrproc_t>(xdr_CSSM_POSSIBLY_KEY_IN_DATA_PTR), true, data);

	return ipcHRecord;
}


void ClientSession::findRecordHandle(RecordHandle hRecord,
								   CssmDbRecordAttributeData *attributes,
								   CssmData *data, KeyHandle &hKey)
{
	CopyIn in_attr(attributes, reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA));
	void *out_attr_data = NULL, *out_data = NULL;
	mach_msg_size_t out_attr_length = 0, out_data_length = 0;
	UCSP_CLIENT_IPC(findRecordHandle, hRecord,
			in_attr.data(), in_attr.length(), &out_attr_data, &out_attr_length,
			data != NULL, &out_data, &out_data_length, &hKey);
	
	if (hRecord != 0)
	{
		CopyOut out_attrs(out_attr_data, out_attr_length, reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR), true);
		copy_back_attribute_return_data(attributes, reinterpret_cast<CssmDbRecordAttributeData*>(out_attrs.data()), returnAllocator);
	}

	// decode data from server as cssm_data or cssm_key (get data on keys returns cssm_key in data)
	CopyOut possible_key_in_data(out_data, out_data_length, reinterpret_cast<xdrproc_t>(xdr_CSSM_POSSIBLY_KEY_IN_DATA_PTR), true, data);
}


void ClientSession::releaseSearch(SearchHandle searchHandle)
{
	UCSP_CLIENT_IPC(releaseSearch, searchHandle);
}


void ClientSession::releaseRecord(RecordHandle record)
{
	UCSP_CLIENT_IPC(releaseRecord, record);
}

void ClientSession::getDbName(DbHandle db, string &name)
{
	char result[PATH_MAX];
    
    UCSP_CLIENT_IPC(getDbName, db, result);
    
	name = result;
}

void ClientSession::setDbName(DbHandle db, const string &name)
{
	UCSP_CLIENT_IPC(setDbName, db, name.c_str());
}


//
// Internal database management
//
DbHandle ClientSession::createDb(const DLDbIdentifier &dbId,
    const AccessCredentials *cred, const AclEntryInput *owner,
    const DBParameters &params)
{
	DatabaseAccessCredentials creds(cred, internalAllocator);
	CopyIn copycreds(creds.value(), reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	CopyIn proto(owner ? &owner->proto() : NULL, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_PROTOTYPE));
	// XXX/64 make xdr routines translate directly between dldbident and flat rep
    DataWalkers::DLDbFlatIdentifier ident(dbId);
	CopyIn id(&ident, reinterpret_cast<xdrproc_t>(xdr_DLDbFlatIdentifier));
	DbHandle db;
    
	UCSP_CLIENT_IPC(createDb, &db, id.data(), id.length(), copycreds.data(), copycreds.length(), proto.data(), proto.length(), params);
    
	return db;
}

DbHandle ClientSession::cloneDb(const DLDbIdentifier &newDbId, DbHandle srcDb) {
    DataWalkers::DLDbFlatIdentifier ident(newDbId);
    CopyIn id(&ident, reinterpret_cast<xdrproc_t>(xdr_DLDbFlatIdentifier));

    DbHandle db;
	UCSP_CLIENT_IPC(cloneDb, srcDb, id.data(), id.length(), &db);
    return db;
}

DbHandle ClientSession::recodeDbForSync(DbHandle dbToClone, 
									   DbHandle srcDb)
{
	DbHandle newDb;
    
    UCSP_CLIENT_IPC(recodeDbForSync,  dbToClone, srcDb, &newDb);

    return newDb;
}

DbHandle ClientSession::recodeDbToVersion(uint32 newVersion, DbHandle srcDb)
{
    DbHandle newDb;

	UCSP_CLIENT_IPC(recodeDbToVersion, newVersion, srcDb, &newDb);

    return newDb;
}

void ClientSession::recodeFinished(DbHandle db)
{
	UCSP_CLIENT_IPC(recodeFinished, db);
}

DbHandle ClientSession::authenticateDbsForSync(const CssmData &dbHandleArray,
											   const CssmData &agentData)
{
	DbHandle newDb;
    
    UCSP_CLIENT_IPC(authenticateDbsForSync, DATA(dbHandleArray), DATA(agentData), &newDb);
    
	return newDb;
}

void ClientSession::commitDbForSync(DbHandle srcDb, DbHandle cloneDb, 
                                    CssmData &blob, Allocator &alloc)
{
    DataOutput outBlob(blob, alloc);
	UCSP_CLIENT_IPC(commitDbForSync,  srcDb, cloneDb, DATA_OUT(outBlob));
}

DbHandle ClientSession::decodeDb(const DLDbIdentifier &dbId,
    const AccessCredentials *cred, const CssmData &blob)
{
	// XXX/64 fold into one translation
	DatabaseAccessCredentials credentials(cred, internalAllocator);
	CopyIn creds(credentials.value(), reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	// XXX/64 fold into one translation
    DataWalkers::DLDbFlatIdentifier ident(dbId);
	CopyIn id(&ident, reinterpret_cast<xdrproc_t>(xdr_DLDbFlatIdentifier));
	DbHandle db;
    
	UCSP_CLIENT_IPC(decodeDb, &db, id.data(), id.length(), creds.data(), creds.length(), DATA(blob));
    
	return db;
}

void ClientSession::encodeDb(DbHandle db, CssmData &blob, Allocator &alloc)
{
	DataOutput outBlob(blob, alloc);
	UCSP_CLIENT_IPC(encodeDb, db, DATA_OUT(outBlob));
}

void ClientSession::setDbParameters(DbHandle db, const DBParameters &params)
{
	UCSP_CLIENT_IPC(setDbParameters, db, params);
}

void ClientSession::getDbParameters(DbHandle db, DBParameters &params)
{
	UCSP_CLIENT_IPC(getDbParameters, db, &params);
}

void ClientSession::changePassphrase(DbHandle db, const AccessCredentials *cred)
{
	CopyIn creds(cred, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	UCSP_CLIENT_IPC(changePassphrase, db, creds.data(), creds.length());
}

void ClientSession::changePassphrase(DbHandle db, const KeyHandle kh)
{
	UCSP_CLIENT_IPC(changeDbPassphraseWithHandle, db, kh);
}

void ClientSession::changeKeybagPassphrase(DbHandle db, const CssmData &oldPassphrase, const CssmData &newPassphrase)
{
	UCSP_CLIENT_IPC(changeKeybagPassphrase, db, DATA(oldPassphrase), DATA(newPassphrase));
}


void ClientSession::lock(DbHandle db)
{
	UCSP_CLIENT_IPC(authenticateDb, db, CSSM_DB_ACCESS_RESET, NULL, 0);
//@@@VIRTUAL	IPC(ucsp_client_lockDb(UCSP_ARGS, db));
}

void ClientSession::lockAll (bool forSleep)
{
	UCSP_CLIENT_IPC(lockAll, forSleep);
}

void ClientSession::unlock(DbHandle db)
{
	UCSP_CLIENT_IPC(unlockDb, db);
}

void ClientSession::unlock(DbHandle db, const CssmData &passphrase)
{
	UCSP_CLIENT_IPC(unlockDbWithPassphrase, db, DATA(passphrase));
}

void ClientSession::unlockKeybag(DbHandle db, const CssmData &passphrase)
{
    UCSP_CLIENT_IPC(unlockKeybagWithPassphrase, db, DATA(passphrase));
}

KeyHandle ClientSession::pushForLaterUnlock(const CssmData &passphrase)
{
	KeyHandle kh = noKey;
	UCSP_CLIENT_IPC(pushForLaterUnlock, &kh, DATA(passphrase));
	return kh;
}

void ClientSession::unlock(DbHandle db, const KeyHandle kh)
{
	UCSP_CLIENT_IPC(unlockDbWithHandle, db, kh);
}

KeyHandle ClientSession::generateDerivedEntropy(const CssmData &salt, const CssmData &passphrase)
{
    KeyHandle kh = noKey;
    UCSP_CLIENT_IPC(generateDerivedEntropy, &kh, DATA(salt), DATA(passphrase));
    return kh;
}

void ClientSession::releaseHandle(const KeyHandle kh)
{
    UCSP_CLIENT_IPC(releaseHandle, kh);
}

void ClientSession::getDerivedEntropy(const KeyHandle kh, CssmData& data)
{
    secnotice("dp_login", "ClientSession::getDerivedEntropy %u", kh);
    DataOutput outData(data, returnAllocator);
    UCSP_CLIENT_IPC(getDerivedEntropy, kh, DATA_OUT(outData));
}

void ClientSession::stashDb(DbHandle db)
{
	UCSP_CLIENT_IPC(stashDb, db);
}

void ClientSession::stashDbCheck(DbHandle db)
{
	UCSP_CLIENT_IPC(stashDbCheck, db);
}
    
bool ClientSession::isLocked(DbHandle db)
{
    boolean_t locked;
	UCSP_CLIENT_IPC(isLocked, db, &locked);
    return locked;
}

void ClientSession::verifyKeyStorePassphrase(uint32_t retries)
{
	UCSP_CLIENT_IPC(verifyKeyStorePassphrase, retries);
}

void ClientSession::resetKeyStorePassphrase(const CssmData &passphrase)
{
	UCSP_CLIENT_IPC(resetKeyStorePassphrase, DATA(passphrase));
}

void ClientSession::changeKeyStorePassphrase()
{
	UCSP_CLIENT_IPC(changeKeyStorePassphrase);
}

//
// Key control
//
void ClientSession::encodeKey(KeyHandle key, CssmData &blob,
    KeyUID *uid, Allocator &alloc)
{
	// Not really used as output
	DataOutput oBlob(blob, alloc);
    void *uidp;
    mach_msg_type_number_t uidLength;
    
	UCSP_CLIENT_IPC(encodeKey, key, oBlob.data(), oBlob.length(),
			(uid != NULL), &uidp, &uidLength);
        
    // return key uid if requested
    if (uid) {
        assert(uidLength == sizeof(KeyUID));
        memcpy(uid, uidp, sizeof(KeyUID));
    }
}

KeyHandle ClientSession::decodeKey(DbHandle db, const CssmData &blob, CssmKey::Header &header)
{
	KeyHandle key;
	void *keyHeaderData;
	mach_msg_type_number_t keyHeaderDataLength;

	UCSP_CLIENT_IPC(decodeKey, &key, &keyHeaderData, &keyHeaderDataLength, db, blob.data(), (mach_msg_type_number_t)blob.length());

	CopyOut wrappedKeyHeaderXDR(keyHeaderData, keyHeaderDataLength + sizeof(CSSM_KEYHEADER), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEYHEADER_PTR), true);
	header = *static_cast<CssmKey::Header *>(reinterpret_cast<CSSM_KEYHEADER*>(wrappedKeyHeaderXDR.data()));

	return key;
}

// keychain synchronization
void ClientSession::recodeKey(DbHandle oldDb, KeyHandle key, DbHandle newDb, 
	CssmData &blob)
{
	DataOutput outBlob(blob, returnAllocator);
	UCSP_CLIENT_IPC(recodeKey, oldDb, key, newDb, DATA_OUT(outBlob));
}

void ClientSession::releaseKey(KeyHandle key)
{
	UCSP_CLIENT_IPC(releaseKey, key);
}


CssmKeySize ClientSession::queryKeySizeInBits(KeyHandle key)
{
    CssmKeySize length;
	UCSP_CLIENT_IPC(queryKeySizeInBits, key, &length);
    return length;
}


uint32 ClientSession::getOutputSize(const Context &context, KeyHandle key,
    uint32 inputSize, bool encrypt)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
    uint32 outputSize;
    
    UCSP_CLIENT_IPC(getOutputSize, ctxcopy.data(), ctxcopy.length(), key, inputSize, encrypt, &outputSize);
    return outputSize;
}


//
// Random number generation.
// This interfaces to the secure RNG inside the SecurityServer; it does not access
// a PRNG in its CSP. If you need a reproducible PRNG, attach a local CSP and use it.
// Note that this function does not allocate a buffer; it always fills the buffer provided.
//
// As of macOS 10.15 this no longer fetches random data from the daemon but generates it in-process
//
void ClientSession::generateRandom(const Security::Context &context, CssmData &data, Allocator &alloc)
{
    size_t count = context.getInt(CSSM_ATTRIBUTE_OUTPUT_SIZE);
    if (data.length() < count) {
        CssmError::throwMe(CSSM_ERRCODE_INVALID_DATA);
    }
    CCRNGStatus status = CCRandomGenerateBytes(data.data(), count);
    if (status != kCCSuccess) {
        CssmError::throwMe(status);
    }
}


//
// Signatures and MACs
//
void ClientSession::generateSignature(const Context &context, KeyHandle key,
	const CssmData &data, CssmData &signature, Allocator &alloc, CSSM_ALGORITHMS signOnlyAlgorithm)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	DataOutput sig(signature, alloc);
    
	UCSP_CLIENT_IPCKEY(generateSignature, key, CSSM_ACL_AUTHORIZATION_SIGN, ctxcopy.data(), ctxcopy.length(),
			key, signOnlyAlgorithm, DATA(data), DATA_OUT(sig));
}

void ClientSession::verifySignature(const Context &context, KeyHandle key,
	const CssmData &data, const CssmData &signature, CSSM_ALGORITHMS verifyOnlyAlgorithm)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
    
    UCSP_CLIENT_IPC(verifySignature,  ctxcopy.data(), ctxcopy.length(), key, verifyOnlyAlgorithm,
			DATA(data), DATA(signature));
}


void ClientSession::generateMac(const Context &context, KeyHandle key,
	const CssmData &data, CssmData &signature, Allocator &alloc)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	DataOutput sig(signature, alloc);
    
	UCSP_CLIENT_IPCKEY(generateMac, key, CSSM_ACL_AUTHORIZATION_MAC, ctxcopy.data(), ctxcopy.length(),
			key, DATA(data), DATA_OUT(sig));
}

void ClientSession::verifyMac(const Context &context, KeyHandle key,
	const CssmData &data, const CssmData &signature)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
    
	UCSP_CLIENT_IPCKEY(verifyMac, key, CSSM_ACL_AUTHORIZATION_MAC, ctxcopy.data(), ctxcopy.length(),
			key, DATA(data), DATA(signature));
}


//
// Encryption/Decryption
//
	
void ClientSession::encrypt(const Context &context, KeyHandle key,
	const CssmData &clear, CssmData &cipher, Allocator &alloc)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	DataOutput cipherOut(cipher, alloc);
	UCSP_CLIENT_IPCKEY(encrypt, key, CSSM_ACL_AUTHORIZATION_ENCRYPT, ctxcopy.data(), ctxcopy.length(),
			key, DATA(clear), DATA_OUT(cipherOut));
}

void ClientSession::decrypt(const Context &context, KeyHandle key,
	const CssmData &cipher, CssmData &clear, Allocator &alloc)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	DataOutput clearOut(clear, alloc);
    
	UCSP_CLIENT_IPCKEY(decrypt, key, CSSM_ACL_AUTHORIZATION_DECRYPT, ctxcopy.data(), ctxcopy.length(),
			key, DATA(cipher), DATA_OUT(clearOut));
}


//
// Key generation
//
void ClientSession::generateKey(DbHandle db, const Context &context, uint32 keyUsage, uint32 keyAttr,
    const AccessCredentials *cred, const AclEntryInput *owner,
    KeyHandle &newKey, CssmKey::Header &newHeader)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	CopyIn creds(cred, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	CopyIn proto(owner ? &owner->proto() : NULL, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_PROTOTYPE));
	void *keyHeaderData;
	mach_msg_type_number_t keyHeaderDataLength;
    
	UCSP_CLIENT_IPC(generateKey, db, ctxcopy.data(), ctxcopy.length(),
			creds.data(), creds.length(), proto.data(), proto.length(),
			keyUsage, keyAttr, &newKey, &keyHeaderData, &keyHeaderDataLength);
        
	CopyOut wrappedKeyHeaderXDR(keyHeaderData, keyHeaderDataLength + sizeof(CSSM_KEYHEADER), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEYHEADER_PTR), true);
	newHeader = *static_cast<CssmKey::Header *>(reinterpret_cast<CSSM_KEYHEADER*>(wrappedKeyHeaderXDR.data()));
}

void ClientSession::generateKey(DbHandle db, const Context &context,
    uint32 pubKeyUsage, uint32 pubKeyAttr,
    uint32 privKeyUsage, uint32 privKeyAttr,
    const AccessCredentials *cred, const AclEntryInput *owner,
    KeyHandle &pubKey, CssmKey::Header &pubHeader,
    KeyHandle &privKey, CssmKey::Header &privHeader)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	CopyIn creds(cred, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	CopyIn proto(owner ? &owner->proto() : NULL, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_PROTOTYPE));
	void *pubKeyHeaderData, *privKeyHeaderData;
	mach_msg_type_number_t pubKeyHeaderDataLength, privKeyHeaderDataLength;
    
	UCSP_CLIENT_IPC(generateKeyPair, db, ctxcopy.data(), ctxcopy.length(),
			creds.data(), creds.length(), proto.data(), proto.length(),
			pubKeyUsage, pubKeyAttr, privKeyUsage, privKeyAttr,
			&pubKey, &pubKeyHeaderData, &pubKeyHeaderDataLength,
			&privKey, &privKeyHeaderData, &privKeyHeaderDataLength);
        
	CopyOut wrappedPubKeyHeaderXDR(pubKeyHeaderData, pubKeyHeaderDataLength + sizeof(CSSM_KEYHEADER), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEYHEADER_PTR), true);
	pubHeader = *static_cast<CssmKey::Header *>(reinterpret_cast<CSSM_KEYHEADER*>(wrappedPubKeyHeaderXDR.data()));
	CopyOut wrappedPrivKeyHeaderXDR(privKeyHeaderData, privKeyHeaderDataLength + sizeof(CSSM_KEYHEADER), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEYHEADER_PTR), true);
	privHeader = *static_cast<CssmKey::Header *>(reinterpret_cast<CSSM_KEYHEADER*>(wrappedPrivKeyHeaderXDR.data()));

}


//
// Key derivation
// This is a bit strained; the incoming 'param' value may have structure,
// and we use a synthetic CssmDeriveData structure (with ad-hoc walker) to
// handle that. Param also is input/output, which is always a pain (not to mention
// ill-defined by the CDSA standard).
//
// If you're here because an algorithm of yours requires structured parameter
// input, go to security_cdsa_utilities/cssmwalkers.h and add a case to the
// CssmDeriveData walker.
//
void ClientSession::deriveKey(DbHandle db, const Context &context, KeyHandle baseKey,
    CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs, CssmData &param,
    const AccessCredentials *cred, const AclEntryInput *owner,
    KeyHandle &newKey, CssmKey::Header &newHeader, Allocator &allocator)
{
		CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
		CopyIn creds(cred, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
		CopyIn proto(owner ? &owner->proto() : NULL, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_PROTOTYPE));
		CSSM_DERIVE_DATA inParamForm = { context.algorithm(), param };
		CopyIn inParam(&inParamForm, reinterpret_cast<xdrproc_t>(xdr_CSSM_DERIVE_DATA));
		
		try
		{
			DataOutput paramOutput(param, allocator);
			void *keyHeaderData;
			mach_msg_type_number_t keyHeaderDataLength;
			
			UCSP_CLIENT_IPCKEY(deriveKey, baseKey, CSSM_ACL_AUTHORIZATION_DERIVE, db, ctxcopy.data(), ctxcopy.length(), baseKey,
					creds.data(), creds.length(), proto.data(), proto.length(),
					inParam.data(), inParam.length(), DATA_OUT(paramOutput),
					usage, attrs, &newKey, &keyHeaderData, &keyHeaderDataLength);
			
			CopyOut wrappedKeyHeaderXDR(keyHeaderData, keyHeaderDataLength + sizeof(CSSM_KEYHEADER), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEYHEADER_PTR), true);
			newHeader = *static_cast<CssmKey::Header *>(reinterpret_cast<CSSM_KEYHEADER*>(wrappedKeyHeaderXDR.data()));
		}
		catch (CssmError& e)
		{
			// filter out errors for CSSM_ALGID_PKCS5_PBKDF2
			if (context.algorithm() != CSSM_ALGID_PKCS5_PBKDF2 && e.error != CSSMERR_CSP_OUTPUT_LENGTH_ERROR)
			{
				throw;
			}
		}
}


//
// Digest generation
//
void ClientSession::getKeyDigest(KeyHandle key, CssmData &digest, Allocator &allocator)
{
	DataOutput dig(digest, allocator);
	UCSP_CLIENT_IPC(getKeyDigest, key, DATA_OUT(dig));
}


//
// Getting public key
//
void ClientSession::getPublicKey(const Context &context, KeyHandle key, CssmData &pubKey, Allocator &allocator)
{
    CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
    DataOutput pubk(pubKey, allocator);
	UCSP_CLIENT_IPC(getPublicKey, ctxcopy.data(), ctxcopy.length(), key, DATA_OUT(pubk));
}


//
// Key wrapping and unwrapping
//
void ClientSession::wrapKey(const Context &context, KeyHandle wrappingKey,
    KeyHandle keyToBeWrapped, const AccessCredentials *cred,
	const CssmData *descriptiveData, CssmWrappedKey &wrappedKey, Allocator &alloc)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	CopyIn creds(cred, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	void *keyData;
	mach_msg_type_number_t keyDataLength;

	CSSM_ACL_AUTHORIZATION_TAG tag = (context.algorithm() == CSSM_ALGID_NONE)
										? CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR : CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED;
    
	UCSP_CLIENT_IPCKEY(wrapKey, keyToBeWrapped, tag, ctxcopy.data(), ctxcopy.length(), wrappingKey,
			creds.data(), creds.length(),
			keyToBeWrapped, OPTIONALDATA(descriptiveData),
			&keyData, &keyDataLength);

	CopyOut wrappedKeyXDR(keyData, keyDataLength + sizeof(CSSM_KEY), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEY_PTR), true);
	CssmWrappedKey *wrappedKeyIPC = reinterpret_cast<CssmWrappedKey*>(wrappedKeyXDR.data());
	wrappedKey.header() = wrappedKeyIPC->header();
	wrappedKey.keyData() = CssmData(alloc.malloc(wrappedKeyIPC->keyData().length()), wrappedKeyIPC->keyData().length());
	memcpy(wrappedKey.keyData().data(), wrappedKeyIPC->keyData(), wrappedKeyIPC->keyData().length());
}

void ClientSession::unwrapKey(DbHandle db, const Context &context, KeyHandle key,
    KeyHandle publicKey, const CssmWrappedKey &wrappedKey,
	uint32 usage, uint32 attr,
	const AccessCredentials *cred, const AclEntryInput *acl,
	CssmData &descriptiveData,
    KeyHandle &newKey, CssmKey::Header &newHeader, Allocator &alloc)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	DataOutput descriptor(descriptiveData, alloc);
	CopyIn creds(cred, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	CopyIn proto(acl ? &acl->proto() : NULL, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_PROTOTYPE));
	CopyIn wrappedKeyXDR(&wrappedKey, reinterpret_cast<xdrproc_t>(xdr_CSSM_KEY));
	void *keyHeaderData;
	mach_msg_type_number_t keyHeaderDataLength;

	UCSP_CLIENT_IPCKEY(unwrapKey, key, CSSM_ACL_AUTHORIZATION_DECRYPT, db, ctxcopy.data(), ctxcopy.length(), key,
			creds.data(), creds.length(), proto.data(), proto.length(),
			publicKey, wrappedKeyXDR.data(), wrappedKeyXDR.length(), usage, attr, DATA_OUT(descriptor),
			&newKey, &keyHeaderData, &keyHeaderDataLength);

	CopyOut wrappedKeyHeaderXDR(keyHeaderData, keyHeaderDataLength + sizeof(CSSM_KEYHEADER), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEYHEADER_PTR), true);
	newHeader = *static_cast<CssmKey::Header *>(reinterpret_cast<CSSM_KEYHEADER*>(wrappedKeyHeaderXDR.data()));
}


//
// ACL management
//
void ClientSession::getAcl(AclKind kind, GenericHandle key, const char *tag,
	uint32 &infoCount, AclEntryInfo * &infoArray, Allocator &alloc)
{
	uint32 count;
	void* info; mach_msg_type_number_t infoLength;
	UCSP_CLIENT_IPC(getAcl, kind, key,
			(tag != NULL), tag ? tag : "",
			&count, &info, &infoLength);

	CSSM_ACL_ENTRY_INFO_ARRAY_PTR aclsArray;
	if (!::copyout_chunked(info, infoLength, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_INFO_ARRAY_PTR), reinterpret_cast<void**>(&aclsArray)))
			CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR); 	
	
	infoCount = aclsArray->count;
	infoArray = reinterpret_cast<AclEntryInfo*>(aclsArray->acls);
    free(aclsArray);
}

void ClientSession::changeAcl(AclKind kind, GenericHandle key, const AccessCredentials &cred,
	const AclEdit &edit)
{
	CopyIn creds(&cred, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	//@@@ ignoring callback
	CopyIn newEntry(edit.newEntry(), reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_INPUT));
    
    UCSP_CLIENT_IPCKEY(changeAcl, key, CSSM_ACL_AUTHORIZATION_CHANGE_ACL, kind, key, creds.data(), creds.length(),
			edit.mode(), toIPCHandle(edit.handle()), newEntry.data(), newEntry.length());
}

void ClientSession::getOwner(AclKind kind, GenericHandle key, AclOwnerPrototype &owner,
    Allocator &alloc)
{
	void* proto; mach_msg_type_number_t protoLength;
	UCSP_CLIENT_IPC(getOwner, kind, key, &proto, &protoLength);
    
    CSSM_ACL_OWNER_PROTOTYPE_PTR tmpOwner;
	if (!::copyout_chunked(proto, protoLength, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_OWNER_PROTOTYPE_PTR), reinterpret_cast<void **>(&tmpOwner)))
		CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR);
    owner = *static_cast<AclOwnerPrototypePtr>(tmpOwner);
    free(tmpOwner);
}

void ClientSession::changeOwner(AclKind kind, GenericHandle key,
	const AccessCredentials &cred, const AclOwnerPrototype &proto)
{
	CopyIn creds(&cred, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	CopyIn protos(&proto, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_OWNER_PROTOTYPE));
	UCSP_CLIENT_IPCKEY(setOwner, key, CSSM_ACL_AUTHORIZATION_CHANGE_OWNER, kind, key, creds.data(),
			creds.length(), protos.data(), protos.length());
}


void ClientSession::getKeyAcl(DbHandle db, const char *tag,
	uint32 &count, AclEntryInfo * &info, Allocator &alloc)
{ getAcl(keyAcl, db, tag, count, info, alloc); }

void ClientSession::changeKeyAcl(DbHandle db, const AccessCredentials &cred,
	const AclEdit &edit)
{ changeAcl(keyAcl, db, cred, edit); }

void ClientSession::getKeyOwner(DbHandle db, AclOwnerPrototype &owner, Allocator &alloc)
{ getOwner(keyAcl, db, owner, alloc); }

void ClientSession::changeKeyOwner(DbHandle db, const AccessCredentials &cred,
	const AclOwnerPrototype &edit)
{ changeOwner(keyAcl, db, cred, edit); }

void ClientSession::getDbAcl(DbHandle db, const char *tag,
	uint32 &count, AclEntryInfo * &info, Allocator &alloc)
{ getAcl(dbAcl, db, tag, count, info, alloc); }

void ClientSession::changeDbAcl(DbHandle db, const AccessCredentials &cred,
	const AclEdit &edit)
{ changeAcl(dbAcl, db, cred, edit); }

void ClientSession::getDbOwner(DbHandle db, AclOwnerPrototype &owner, Allocator &alloc)
{ getOwner(dbAcl, db, owner, alloc); }

void ClientSession::changeDbOwner(DbHandle db, const AccessCredentials &cred,
	const AclOwnerPrototype &edit)
{ changeOwner(dbAcl, db, cred, edit); }


//
// Database key management
//
void ClientSession::extractMasterKey(DbHandle db, const Context &context, DbHandle sourceDb,
	uint32 keyUsage, uint32 keyAttr,
	const AccessCredentials *cred, const AclEntryInput *owner,
	KeyHandle &newKey, CssmKey::Header &newHeader, Allocator &alloc)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	CopyIn creds(cred, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
	CopyIn proto(owner ? &owner->proto() : NULL, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_ENTRY_PROTOTYPE));
	void *keyHeaderData;
	mach_msg_type_number_t keyHeaderDataLength;
    
	UCSP_CLIENT_IPC(extractMasterKey, db, ctxcopy.data(), ctxcopy.length(), sourceDb,
			creds.data(), creds.length(), proto.data(), proto.length(),
			keyUsage, keyAttr, &newKey, &keyHeaderData, &keyHeaderDataLength);
        
	CopyOut wrappedKeyHeaderXDR(keyHeaderData, keyHeaderDataLength + sizeof(CSSM_KEYHEADER), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEYHEADER_PTR), true);
	newHeader = *static_cast<CssmKey::Header *>(reinterpret_cast<CSSM_KEYHEADER*>(wrappedKeyHeaderXDR.data()));
}


void ClientSession::postNotification(NotificationDomain domain, NotificationEvent event, const CssmData &data)
{
	uint32 seq = ++mGlobal().thread().notifySeq;
#if !defined(NDEBUG)
	if (getenv("NOTIFYJITTER")) {
		// artificially reverse odd/even sequences to test securityd's jitter buffer
		seq += 2 * (seq % 2) - 1;
		secinfo("notify", "POSTING FAKE SEQUENCE %d NOTIFICATION", seq);
	}
#endif //NDEBUG
	secinfo("notify", "posting domain 0x%x event %d sequence %d",
		domain, event, seq);
	UCSP_CLIENT_IPC(postNotification, domain, event, DATA(data), seq);
}


//
// Testing related
//

// Return the number of Keychain users prompts securityd has considered showing.
// On non-internal installs, this returns 0.
void ClientSession::getUserPromptAttempts(uint32_t& attempts) {
	UCSP_CLIENT_IPC(getUserPromptAttempts, &attempts);
}


} // end namespace SecurityServer
} // end namespace Security
