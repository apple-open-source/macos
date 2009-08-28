/*
 * Copyright (c) 2000-2008 Apple Inc. All Rights Reserved.
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
#include <security_cdsa_client/cspclient.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

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
	IPC(ucsp_client_authenticateDb(UCSP_ARGS, db, type, copy.data(), copy.length()));
}


void ClientSession::releaseDb(DbHandle db)
{
	IPC(ucsp_client_releaseDb(UCSP_ARGS, db));
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
    
	IPC(ucsp_client_openToken(UCSP_ARGS, ssid, name ? name : "", copycreds.data(), copycreds.length(), &db));
    
	return db;
}


RecordHandle ClientSession::insertRecord(DbHandle db,
						  CSSM_DB_RECORDTYPE recordType,
						  const CssmDbRecordAttributeData *attributes,
						  const CssmData *data)
{
	RecordHandle record;
	CopyIn db_record_attr_data(attributes, reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA));
    
	IPC(ucsp_client_insertRecord(UCSP_ARGS, db, recordType, db_record_attr_data.data(), db_record_attr_data.length(), OPTIONALDATA(data), &record));
    
	return record;
}


void ClientSession::deleteRecord(DbHandle db, RecordHandle record)
{
	IPC(ucsp_client_deleteRecord(UCSP_ARGS, db, record));
}


void ClientSession::modifyRecord(DbHandle db, RecordHandle &record,
				  CSSM_DB_RECORDTYPE recordType,
				  const CssmDbRecordAttributeData *attributes,
				  const CssmData *data,
				  CSSM_DB_MODIFY_MODE modifyMode)
{
	CopyIn db_record_attr_data(attributes, reinterpret_cast<xdrproc_t>(xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA));
    
	IPC(ucsp_client_modifyRecord(UCSP_ARGS, db, &record, recordType, db_record_attr_data.data(), db_record_attr_data.length(),
        data != NULL, OPTIONALDATA(data), modifyMode));
}

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

	IPC(ucsp_client_findFirst(UCSP_ARGS, db, 
		query.data(), query.length(), in_attr.data(), in_attr.length(), 
		&out_attr_data, &out_attr_length, (data != NULL), &out_data, &out_data_length,
		&hKey, &hSearch, &ipcHRecord));
		
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

	IPC(ucsp_client_findNext(UCSP_ARGS, hSearch, 
		in_attr.data(), in_attr.length(), &out_attr_data, &out_attr_length, 
		(data != NULL), &out_data, &out_data_length, &hKey, &ipcHRecord));

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
	IPC(ucsp_client_findRecordHandle(UCSP_ARGS, hRecord, 
		in_attr.data(), in_attr.length(), &out_attr_data, &out_attr_length, 
		data != NULL, &out_data, &out_data_length, &hKey));
	
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
	IPC(ucsp_client_releaseSearch(UCSP_ARGS, searchHandle));
}


void ClientSession::releaseRecord(RecordHandle record)
{
	IPC(ucsp_client_releaseRecord(UCSP_ARGS, record));
}

void ClientSession::getDbName(DbHandle db, string &name)
{
	char result[PATH_MAX];
    
	IPC(ucsp_client_getDbName(UCSP_ARGS, db, result));
    
	name = result;
}

void ClientSession::setDbName(DbHandle db, const string &name)
{
	IPC(ucsp_client_setDbName(UCSP_ARGS, db, name.c_str()));
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
    
	IPC(ucsp_client_createDb(UCSP_ARGS, &db, id.data(), id.length(), copycreds.data(), copycreds.length(), proto.data(), proto.length(), params));
    
	return db;
}

DbHandle ClientSession::recodeDbForSync(DbHandle dbToClone, 
									   DbHandle srcDb)
{
	DbHandle newDb;
    
	IPC(ucsp_client_recodeDbForSync(UCSP_ARGS, dbToClone, srcDb, &newDb));
    
	return newDb;
}

DbHandle ClientSession::authenticateDbsForSync(const CssmData &dbHandleArray,
											   const CssmData &agentData)
{
	DbHandle newDb;
    
	IPC(ucsp_client_authenticateDbsForSync(UCSP_ARGS, DATA(dbHandleArray), DATA(agentData), &newDb));
    
	return newDb;
}

void ClientSession::commitDbForSync(DbHandle srcDb, DbHandle cloneDb, 
                                    CssmData &blob, Allocator &alloc)
{
    DataOutput outBlob(blob, alloc);
    IPC(ucsp_client_commitDbForSync(UCSP_ARGS, srcDb, cloneDb, DATA(outBlob)));
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
    
	IPC(ucsp_client_decodeDb(UCSP_ARGS, &db, id.data(), id.length(), creds.data(), creds.length(), DATA(blob)));
    
	return db;
}

void ClientSession::encodeDb(DbHandle db, CssmData &blob, Allocator &alloc)
{
	DataOutput outBlob(blob, alloc);
	IPC(ucsp_client_encodeDb(UCSP_ARGS, db, DATA(outBlob)));
}

void ClientSession::setDbParameters(DbHandle db, const DBParameters &params)
{
	IPC(ucsp_client_setDbParameters(UCSP_ARGS, db, params));
}

void ClientSession::getDbParameters(DbHandle db, DBParameters &params)
{
	IPC(ucsp_client_getDbParameters(UCSP_ARGS, db, &params));
}

void ClientSession::changePassphrase(DbHandle db, const AccessCredentials *cred)
{
	CopyIn creds(cred, reinterpret_cast<xdrproc_t>(xdr_CSSM_ACCESS_CREDENTIALS));
    IPC(ucsp_client_changePassphrase(UCSP_ARGS, db, creds.data(), creds.length()));
}


void ClientSession::lock(DbHandle db)
{
	IPC(ucsp_client_authenticateDb(UCSP_ARGS, db, CSSM_DB_ACCESS_RESET, NULL, 0));
//@@@VIRTUAL	IPC(ucsp_client_lockDb(UCSP_ARGS, db));
}

void ClientSession::lockAll (bool forSleep)
{
	IPC(ucsp_client_lockAll (UCSP_ARGS, forSleep));
}

void ClientSession::unlock(DbHandle db)
{
	IPC(ucsp_client_unlockDb(UCSP_ARGS, db));
}

void ClientSession::unlock(DbHandle db, const CssmData &passphrase)
{
	IPC(ucsp_client_unlockDbWithPassphrase(UCSP_ARGS, db, DATA(passphrase)));
}

bool ClientSession::isLocked(DbHandle db)
{
    boolean_t locked;
	IPC(ucsp_client_isLocked(UCSP_ARGS, db, &locked));
    return locked;
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
    
	IPC(ucsp_client_encodeKey(UCSP_ARGS, key, oBlob.data(), oBlob.length(),
        (uid != NULL), &uidp, &uidLength));
        
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

	IPC(ucsp_client_decodeKey(UCSP_ARGS, &key, &keyHeaderData, &keyHeaderDataLength, db, blob.data(), blob.length()));

	CopyOut wrappedKeyHeaderXDR(keyHeaderData, keyHeaderDataLength + sizeof(CSSM_KEYHEADER), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEYHEADER_PTR), true);
	header = *static_cast<CssmKey::Header *>(reinterpret_cast<CSSM_KEYHEADER*>(wrappedKeyHeaderXDR.data()));

	return key;
}

// keychain synchronization
void ClientSession::recodeKey(DbHandle oldDb, KeyHandle key, DbHandle newDb, 
	CssmData &blob)
{
	DataOutput outBlob(blob, returnAllocator);
	IPC(ucsp_client_recodeKey(UCSP_ARGS, oldDb, key, newDb, DATA(outBlob)));
}

void ClientSession::releaseKey(KeyHandle key)
{
	IPC(ucsp_client_releaseKey(UCSP_ARGS, key));
}


CssmKeySize ClientSession::queryKeySizeInBits(KeyHandle key)
{
    CssmKeySize length;
    IPC(ucsp_client_queryKeySizeInBits(UCSP_ARGS, key, &length));
    return length;
}


uint32 ClientSession::getOutputSize(const Context &context, KeyHandle key,
    uint32 inputSize, bool encrypt)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
    uint32 outputSize;
    
    IPC(ucsp_client_getOutputSize(UCSP_ARGS, ctxcopy.data(), ctxcopy.length(), key, inputSize, encrypt, &outputSize));
    return outputSize;
}


//
// Random number generation.
// This interfaces to the secure RNG inside the SecurityServer; it does not access
// a PRNG in its CSP. If you need a reproducible PRNG, attach a local CSP and use it.
// Note that this function does not allocate a buffer; it always fills the buffer provided.
//
void ClientSession::generateRandom(const Security::Context &context, CssmData &data, Allocator &alloc)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	DataOutput result(data, alloc);
    
	IPC(ucsp_client_generateRandom(UCSP_ARGS, 0, ctxcopy.data(), ctxcopy.length(), DATA(result)));
}


//
// Signatures and MACs
//
void ClientSession::generateSignature(const Context &context, KeyHandle key,
	const CssmData &data, CssmData &signature, Allocator &alloc, CSSM_ALGORITHMS signOnlyAlgorithm)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	DataOutput sig(signature, alloc);
    
	IPCKEY(ucsp_client_generateSignature(UCSP_ARGS, ctxcopy.data(), ctxcopy.length(), key, signOnlyAlgorithm,
		DATA(data), DATA(sig)),
		   key, CSSM_ACL_AUTHORIZATION_SIGN);
}

void ClientSession::verifySignature(const Context &context, KeyHandle key,
	const CssmData &data, const CssmData &signature, CSSM_ALGORITHMS verifyOnlyAlgorithm)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
    
	IPC(ucsp_client_verifySignature(UCSP_ARGS, ctxcopy.data(), ctxcopy.length(), key, verifyOnlyAlgorithm, DATA(data), DATA(signature)));
}


void ClientSession::generateMac(const Context &context, KeyHandle key,
	const CssmData &data, CssmData &signature, Allocator &alloc)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	DataOutput sig(signature, alloc);
    
	IPCKEY(ucsp_client_generateMac(UCSP_ARGS, ctxcopy.data(), ctxcopy.length(), key, DATA(data), DATA(sig)),
		key, CSSM_ACL_AUTHORIZATION_MAC);
}

void ClientSession::verifyMac(const Context &context, KeyHandle key,
	const CssmData &data, const CssmData &signature)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
    
	IPCKEY(ucsp_client_verifyMac(UCSP_ARGS, ctxcopy.data(), ctxcopy.length(), key,
		DATA(data), DATA(signature)),
		key, CSSM_ACL_AUTHORIZATION_MAC);
}


//
// Encryption/Decryption
//
	
void ClientSession::encrypt(const Context &context, KeyHandle key,
	const CssmData &clear, CssmData &cipher, Allocator &alloc)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	DataOutput cipherOut(cipher, alloc);
	IPCKEY(ucsp_client_encrypt(UCSP_ARGS, ctxcopy.data(), ctxcopy.length(), key, DATA(clear), DATA(cipherOut)),
		key, CSSM_ACL_AUTHORIZATION_ENCRYPT);
}

void ClientSession::decrypt(const Context &context, KeyHandle key,
	const CssmData &cipher, CssmData &clear, Allocator &alloc)
{
	CopyIn ctxcopy(&context, reinterpret_cast<xdrproc_t>(xdr_CSSM_CONTEXT));
	DataOutput clearOut(clear, alloc);
    
	IPCKEY(ucsp_client_decrypt(UCSP_ARGS, ctxcopy.data(), ctxcopy.length(), key, DATA(cipher), DATA(clearOut)),
		key, CSSM_ACL_AUTHORIZATION_DECRYPT);
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
    
	IPC(ucsp_client_generateKey(UCSP_ARGS, db, ctxcopy.data(), ctxcopy.length(),
		creds.data(), creds.length(), proto.data(), proto.length(), 
		keyUsage, keyAttr, &newKey, &keyHeaderData, &keyHeaderDataLength));
        
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
    
	IPC(ucsp_client_generateKeyPair(UCSP_ARGS, db, ctxcopy.data(), ctxcopy.length(),
		creds.data(), creds.length(), proto.data(), proto.length(),
		pubKeyUsage, pubKeyAttr, privKeyUsage, privKeyAttr,
		&pubKey, &pubKeyHeaderData, &pubKeyHeaderDataLength,
		&privKey, &privKeyHeaderData, &privKeyHeaderDataLength));
        
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
			
			IPCKEY(ucsp_client_deriveKey(UCSP_ARGS, db, ctxcopy.data(), ctxcopy.length(), baseKey,
				creds.data(), creds.length(), proto.data(), proto.length(), 
				inParam.data(), inParam.length(), DATA(paramOutput),
				usage, attrs, &newKey, &keyHeaderData, &keyHeaderDataLength),
				baseKey, CSSM_ACL_AUTHORIZATION_DERIVE);
			
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
	IPC(ucsp_client_getKeyDigest(UCSP_ARGS, key, DATA(dig)));
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
    
	IPCKEY(ucsp_client_wrapKey(UCSP_ARGS, ctxcopy.data(), ctxcopy.length(), wrappingKey, 
		creds.data(), creds.length(),
		keyToBeWrapped, OPTIONALDATA(descriptiveData),
		&keyData, &keyDataLength),
		keyToBeWrapped,
		context.algorithm() == CSSM_ALGID_NONE
			? CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR : CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED);

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

	IPCKEY(ucsp_client_unwrapKey(UCSP_ARGS, db, ctxcopy.data(), ctxcopy.length(), key,
		creds.data(), creds.length(), proto.data(), proto.length(),
		publicKey, wrappedKeyXDR.data(), wrappedKeyXDR.length(), usage, attr, DATA(descriptor),
        &newKey, &keyHeaderData, &keyHeaderDataLength),
		key, CSSM_ACL_AUTHORIZATION_DECRYPT);

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
	IPC(ucsp_client_getAcl(UCSP_ARGS, kind, key,
		(tag != NULL), tag ? tag : "",
		&count, &info, &infoLength));

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
    
	IPCKEY(ucsp_client_changeAcl(UCSP_ARGS, kind, key, creds.data(), creds.length(),
		edit.mode(), toIPCHandle(edit.handle()), newEntry.data(), newEntry.length()),
		key, CSSM_ACL_AUTHORIZATION_CHANGE_ACL);
}

void ClientSession::getOwner(AclKind kind, GenericHandle key, AclOwnerPrototype &owner,
    Allocator &alloc)
{
	void* proto; mach_msg_type_number_t protoLength;
	IPC(ucsp_client_getOwner(UCSP_ARGS, kind, key, &proto, &protoLength));
    
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
	IPCKEY(ucsp_client_setOwner(UCSP_ARGS, kind, key, creds.data(), creds.length(), protos.data(), protos.length()),
		key, CSSM_ACL_AUTHORIZATION_CHANGE_OWNER);
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
    
	IPC(ucsp_client_extractMasterKey(UCSP_ARGS, db, ctxcopy.data(), ctxcopy.length(), sourceDb,
		creds.data(), creds.length(), proto.data(), proto.length(), 
		keyUsage, keyAttr, &newKey, &keyHeaderData, &keyHeaderDataLength));
        
	CopyOut wrappedKeyHeaderXDR(keyHeaderData, keyHeaderDataLength + sizeof(CSSM_KEYHEADER), reinterpret_cast<xdrproc_t>(xdr_CSSM_KEYHEADER_PTR), true);
	newHeader = *static_cast<CssmKey::Header *>(reinterpret_cast<CSSM_KEYHEADER*>(wrappedKeyHeaderXDR.data()));
}


//
// Authorization subsystem entry
//
void ClientSession::authCreate(const AuthorizationItemSet *rights,
	const AuthorizationItemSet *environment, AuthorizationFlags flags,
	AuthorizationBlob &result)
{
	void *rightSet = NULL; mach_msg_size_t rightSet_size = 0;
	void *environ = NULL; mach_msg_size_t environ_size = 0;

	if ((rights && 
		!copyin_AuthorizationItemSet(rights, &rightSet, &rightSet_size)) ||
		(environment && 
		!copyin_AuthorizationItemSet(environment, &environ, &environ_size)))
			CssmError::throwMe(errAuthorizationInternal);

	activate();
	IPCSTART(ucsp_client_authorizationCreate(UCSP_ARGS,
		rightSet, rightSet_size, 
		flags,
		environ, environ_size, 
		&result));
	
	free(rightSet);
	free(environ);
	
	if (rcode == CSSMERR_CSSM_NO_USER_INTERACTION)
	  CssmError::throwMe(errAuthorizationInteractionNotAllowed);
	IPCEND_CHECK;
}

void ClientSession::authRelease(const AuthorizationBlob &auth, 
	AuthorizationFlags flags)
{
	activate();
	IPCSTART(ucsp_client_authorizationRelease(UCSP_ARGS, auth, flags));
	if (rcode == CSSMERR_CSSM_NO_USER_INTERACTION)
	  CssmError::throwMe(errAuthorizationInteractionNotAllowed);
	IPCEND_CHECK;
}

void ClientSession::authCopyRights(const AuthorizationBlob &auth,
	const AuthorizationItemSet *rights, const AuthorizationItemSet *environment,
	AuthorizationFlags flags,
	AuthorizationItemSet **grantedRights)
{
	void *rightSet = NULL; mach_msg_size_t rightSet_size = 0;
	void *environ = NULL; mach_msg_size_t environ_size = 0;
	void *result = NULL; mach_msg_type_number_t resultLength = 0;
	
	if ((rights && !copyin_AuthorizationItemSet(rights, &rightSet, &rightSet_size)) ||
		(environment && !copyin_AuthorizationItemSet(environment, &environ, &environ_size)))
          CssmError::throwMe(errAuthorizationInternal); // allocation error probably

	activate();
	IPCSTART(ucsp_client_authorizationCopyRights(UCSP_ARGS,
		auth,
		rightSet, rightSet_size, 
		flags | (grantedRights ? 0 : kAuthorizationFlagNoData),
		environ, environ_size, 
		&result, &resultLength));
		
	free(rightSet);
	free(environ);
	
	// XXX/cs return error when copyout returns false
	if (rcode == CSSM_OK && grantedRights) 
		copyout_AuthorizationItemSet(result, resultLength, grantedRights);
	
	if (result)
		mig_deallocate(reinterpret_cast<vm_address_t>(result), resultLength);
	if (rcode == CSSMERR_CSSM_NO_USER_INTERACTION)
	  CssmError::throwMe(errAuthorizationInteractionNotAllowed);
	IPCEND_CHECK;
}

void ClientSession::authCopyInfo(const AuthorizationBlob &auth,
	const char *tag,
	AuthorizationItemSet * &info)
{
    if (tag == NULL)
        tag = "";
    else if (tag[0] == '\0')
        MacOSError::throwMe(errAuthorizationInvalidTag);
		
	activate();
	void *result; mach_msg_type_number_t resultLength;
	IPCSTART(ucsp_client_authorizationCopyInfo(UCSP_ARGS, auth, tag, &result, &resultLength));

	// XXX/cs return error when copyout returns false
	if (rcode == CSSM_OK)
		copyout_AuthorizationItemSet(result, resultLength, &info);
	
	if (result)
		mig_deallocate(reinterpret_cast<vm_address_t>(result), resultLength);

	if (rcode == CSSMERR_CSSM_NO_USER_INTERACTION)
	  CssmError::throwMe(errAuthorizationInteractionNotAllowed);
	IPCEND_CHECK;
}

void ClientSession::authExternalize(const AuthorizationBlob &auth,
	AuthorizationExternalForm &extForm)
{
	activate();
	IPCSTART(ucsp_client_authorizationExternalize(UCSP_ARGS, auth, &extForm));
	if (rcode == CSSMERR_CSSM_NO_USER_INTERACTION)
	  CssmError::throwMe(errAuthorizationInteractionNotAllowed);
	IPCEND_CHECK;
}

void ClientSession::authInternalize(const AuthorizationExternalForm &extForm,
	AuthorizationBlob &auth)
{
	activate();
	IPCSTART(ucsp_client_authorizationInternalize(UCSP_ARGS, extForm, &auth));
	if (rcode == CSSMERR_CSSM_NO_USER_INTERACTION)
	  CssmError::throwMe(errAuthorizationInteractionNotAllowed);
	IPCEND_CHECK;
}


//
// Get session information (security session status)
//
void ClientSession::getSessionInfo(SecuritySessionId &sessionId, SessionAttributeBits &attrs)
{
    IPC(ucsp_client_getSessionInfo(UCSP_ARGS, &sessionId, &attrs));
}


//
// Create a new session.
//
// Caveat: This discards all SecurityServer held state for this process, including
// authorizations, database handles, etc. If you are multi-threaded at this point,
// and other threads have talked to SecurityServer, they will leak a few resources
// (mach ports and the like). Nothing horrendous, unless you create masses of sessions
// that way (which we wouldn't exactly recommend for other reasons).
//
// Hacker's note: This engages in an interesting dance with SecurityServer's state tracking.
// If you don't know the choreography, don't change things here until talking to an expert.
//
// Yes, if the client had multiple threads each of which has talked to SecurityServer,
// the reply ports for all but the calling thread will leak. If that ever turns out to
// be a real problem, we can fix it by keeping a (locked) set of client replyPorts to ditch.
// Hardly worth it, though. This is a rare call.
//
void ClientSession::setupSession(SessionCreationFlags flags, SessionAttributeBits attrs)
{
	mGlobal().thread().replyPort.destroy(); // kill this thread's reply port
	mGlobal.reset();			// kill existing cache (leak all other threads)
	mSetupSession = true;		// global flag to Global constructor
	IPC(ucsp_client_setupSession(UCSP_ARGS, flags, attrs)); // reinitialize and call
}


//
// Get/set distinguished uid
//
void ClientSession::setSessionDistinguishedUid(SecuritySessionId sessionId, uid_t user)
{
	assert(sizeof(uid_t) <= sizeof(uint32_t));	// (just in case uid_t gets too big one day)
	IPC(ucsp_client_setSessionDistinguishedUid(UCSP_ARGS, sessionId, user));
}

void ClientSession::getSessionDistinguishedUid(SecuritySessionId sessionId, uid_t &user)
{
	IPC(ucsp_client_getSessionDistinguishedUid(UCSP_ARGS, sessionId, &user));
}


//
// Push user preferences from an app in user space to securityd
//
void ClientSession::setSessionUserPrefs(SecuritySessionId sessionId, uint32_t userPreferencesLength, const void *userPreferences)
{
	IPC(ucsp_client_setSessionUserPrefs(UCSP_ARGS, sessionId, const_cast<void *>(userPreferences), userPreferencesLength));
}


void ClientSession::postNotification(NotificationDomain domain, NotificationEvent event, const CssmData &data)
{
	uint32 seq = ++mGlobal().thread().notifySeq;
#if !defined(NDEBUG)
	if (getenv("NOTIFYJITTER")) {
		// artificially reverse odd/even sequences to test securityd's jitter buffer
		seq += 2 * (seq % 2) - 1;
		secdebug("notify", "POSTING FAKE SEQUENCE %d NOTIFICATION", seq);
	}
#endif //NDEBUG
	secdebug("notify", "posting domain 0x%x event %d sequence %d",
		domain, event, seq);
	IPC(ucsp_client_postNotification(UCSP_ARGS, domain, event, DATA(data), seq));
}

//
// authorizationdbGet/Set/Remove
//
void ClientSession::authorizationdbGet(const AuthorizationString rightname, CssmData &rightDefinition, Allocator &alloc)
{
	DataOutput definition(rightDefinition, alloc);
	activate();
	IPCSTART(ucsp_client_authorizationdbGet(UCSP_ARGS, rightname, DATA(definition)));
	if (rcode == CSSMERR_CSSM_NO_USER_INTERACTION)
	  CssmError::throwMe(errAuthorizationInteractionNotAllowed);
	IPCEND_CHECK;
}

void ClientSession::authorizationdbSet(const AuthorizationBlob &auth, const AuthorizationString rightname, uint32_t rightDefinitionLength, const void *rightDefinition)
{
	// @@@ DATA_IN in transition.cpp is not const void *
	activate();
	IPCSTART(ucsp_client_authorizationdbSet(UCSP_ARGS, auth, rightname, const_cast<void *>(rightDefinition), rightDefinitionLength));
	if (rcode == CSSMERR_CSSM_NO_USER_INTERACTION)
	  CssmError::throwMe(errAuthorizationInteractionNotAllowed);
	IPCEND_CHECK;
}

void ClientSession::authorizationdbRemove(const AuthorizationBlob &auth, const AuthorizationString rightname)
{
	activate();
	IPCSTART(ucsp_client_authorizationdbRemove(UCSP_ARGS, auth, rightname));
	if (rcode == CSSMERR_CSSM_NO_USER_INTERACTION)
	  CssmError::throwMe(errAuthorizationInteractionNotAllowed);
	IPCEND_CHECK;
}


//
// Miscellaneous administrative calls
//
void ClientSession::addCodeEquivalence(const CssmData &oldHash, const CssmData &newHash,
	const char *name, bool forSystem /* = false */)
{
	IPC(ucsp_client_addCodeEquivalence(UCSP_ARGS, DATA(oldHash), DATA(newHash),
		name, forSystem));
}

void ClientSession::removeCodeEquivalence(const CssmData &hash, const char *name, bool forSystem /* = false */)
{
	IPC(ucsp_client_removeCodeEquivalence(UCSP_ARGS, DATA(hash), name, forSystem));
}

void ClientSession::setAlternateSystemRoot(const char *path)
{
	IPC(ucsp_client_setAlternateSystemRoot(UCSP_ARGS, path));
}


//
// Code Signing related
//
void ClientSession::registerHosting(mach_port_t hostingPort, SecCSFlags flags)
{
	IPC(ucsp_client_registerHosting(UCSP_ARGS, hostingPort, flags));
}

mach_port_t ClientSession::hostingPort(pid_t pid)
{
	mach_port_t result;
	IPC(ucsp_client_hostingPort(UCSP_ARGS, pid, &result));
	return result;
}

SecGuestRef ClientSession::createGuest(SecGuestRef host,
		uint32_t status, const char *path, const CssmData &cdhash, const CssmData &attributes, SecCSFlags flags)
{
	SecGuestRef newGuest;
	IPC(ucsp_client_createGuest(UCSP_ARGS, host, status, path, DATA(cdhash), DATA(attributes), flags, &newGuest));
	if (flags & kSecCSDedicatedHost) {
		secdebug("ssclient", "setting dedicated guest to 0x%x (was 0x%x)",
			mDedicatedGuest, newGuest);
		mDedicatedGuest = newGuest;
	}
	return newGuest;
}

void ClientSession::setGuestStatus(SecGuestRef guest, uint32 status, const CssmData &attributes)
{
	IPC(ucsp_client_setGuestStatus(UCSP_ARGS, guest, status, DATA(attributes)));
}

void ClientSession::removeGuest(SecGuestRef host, SecGuestRef guest)
{
	IPC(ucsp_client_removeGuest(UCSP_ARGS, host, guest));
}

void ClientSession::selectGuest(SecGuestRef newGuest)
{
	if (mDedicatedGuest) {
		secdebug("ssclient", "ignoring selectGuest(0x%x) because dedicated guest=0x%x",
			newGuest, mDedicatedGuest);
	} else {
		secdebug("ssclient", "switching to guest 0x%x", newGuest);
		mGlobal().thread().currentGuest = newGuest;
	}
}

SecGuestRef ClientSession::selectedGuest() const
{
	if (mDedicatedGuest)
		return mDedicatedGuest;
	else
		return mGlobal().thread().currentGuest;
}


} // end namespace SecurityServer
} // end namespace Security
