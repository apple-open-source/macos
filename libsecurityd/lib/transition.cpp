/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
#include "sstransit.h"
#include <security_cdsa_client/cspclient.h>
#include <security_utilities/ktracecodes.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

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
    DatabaseAccessCredentials creds(cred, internalAllocator);
	IPC(ucsp_client_authenticateDb(UCSP_ARGS, db, type, COPY(creds)));
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
	IPC(ucsp_client_openToken(UCSP_ARGS, ssid, name ? name : "", COPY(creds), &db));
	return db;
}


RecordHandle ClientSession::insertRecord(DbHandle db,
						  CSSM_DB_RECORDTYPE recordType,
						  const CssmDbRecordAttributeData *attributes,
						  const CssmData *data)
{
	RecordHandle record;
	Copier<CssmDbRecordAttributeData> attrs(attributes, internalAllocator);
	IPC(ucsp_client_insertRecord(UCSP_ARGS, db, recordType, COPY(attrs), OPTIONALDATA(data), &record));
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
	Copier<CssmDbRecordAttributeData> attrs(attributes, internalAllocator);
	IPC(ucsp_client_modifyRecord(UCSP_ARGS, db, &record, recordType, COPY(attrs),
	data != NULL, OPTIONALDATA(data), modifyMode));
}


RecordHandle ClientSession::findFirst(DbHandle db,
							  const CssmQuery &inQuery,
							  SearchHandle &hSearch,
							  CssmDbRecordAttributeData *attributes,
							  CssmData *data, KeyHandle &hKey)
{
	Copier<CssmQuery> query(&inQuery, internalAllocator);
	DataRetrieval attrs(attributes, returnAllocator);
	DataOutput outData(data, returnAllocator);
	RecordHandle hRecord;
	IPC(ucsp_client_findFirst(UCSP_ARGS, db, COPY(query), COPY(attrs),
		attrs, attrs, attrs.base(), data != NULL, DATA(outData),
		&hKey, &hSearch, &hRecord));
	return hRecord;
}


RecordHandle ClientSession::findNext(SearchHandle hSearch,
							 CssmDbRecordAttributeData *attributes,
							 CssmData *data, KeyHandle &hKey)
{
	DataRetrieval attrs(attributes, returnAllocator);
	DataOutput outData(data, returnAllocator);
	RecordHandle hRecord;
	IPC(ucsp_client_findNext(UCSP_ARGS, hSearch, COPY(attrs),
		attrs, attrs, attrs.base(), data != NULL, DATA(outData),
		&hKey, &hRecord));
	return hRecord;
}


void ClientSession::findRecordHandle(RecordHandle hRecord,
								   CssmDbRecordAttributeData *attributes,
								   CssmData *data, KeyHandle &hKey)
{
	DataRetrieval attrs(attributes, returnAllocator);
	DataOutput outData(data, returnAllocator);
	IPC(ucsp_client_findRecordHandle(UCSP_ARGS, hRecord, COPY(attrs),
		attrs, attrs, attrs.base(), data != NULL, DATA(outData),
		&hKey));
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
	Copier<AclEntryPrototype> proto(owner ? &owner->proto() : NULL, internalAllocator);
    DataWalkers::DLDbFlatIdentifier ident(dbId);
    Copier<DataWalkers::DLDbFlatIdentifier> id(&ident, internalAllocator);
	DbHandle db;
	IPC(ucsp_client_createDb(UCSP_ARGS, &db, COPY(id), COPY(creds), COPY(proto), params));
	return db;
}

// First step in keychain synchronization: copy the keychain, but substitute
// a set of known operational secrets (usually from an existing keychain)
DbHandle ClientSession::cloneDbForSync(const CssmData &secretsBlob, 
									   DbHandle srcDb, 
									   const CssmData &agentData)
{
	DbHandle newDb;
	IPC(ucsp_client_cloneDbForSync(UCSP_ARGS, DATA(secretsBlob), srcDb, DATA(agentData), &newDb));
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
	DatabaseAccessCredentials creds(cred, internalAllocator);
    DataWalkers::DLDbFlatIdentifier ident(dbId);
    Copier<DataWalkers::DLDbFlatIdentifier> id(&ident, internalAllocator);
	DbHandle db;
	IPC(ucsp_client_decodeDb(UCSP_ARGS, &db, COPY(id), COPY(creds), DATA(blob)));
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
    Copier<AccessCredentials> creds(cred, internalAllocator);
    IPC(ucsp_client_changePassphrase(UCSP_ARGS, db, COPY(creds)));
}


void ClientSession::lock(DbHandle db)
{
	IPC(ucsp_client_authenticateDb(UCSP_ARGS, db, CSSM_DB_ACCESS_RESET, NULL, 0, NULL));
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
	IPC(ucsp_client_decodeKey(UCSP_ARGS, &key, &header, db, blob.data(), blob.length()));
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
	SendContext ctx(context);
    uint32 outputSize;
    IPC(ucsp_client_getOutputSize(UCSP_ARGS, CONTEXT(ctx), key, inputSize, encrypt, &outputSize));
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
	SendContext ctx(context);
	DataOutput result(data, alloc);
	IPC(ucsp_client_generateRandom(UCSP_ARGS, 0, CONTEXT(ctx), DATA(result)));
}


//
// Signatures and MACs
//
void ClientSession::generateSignature(const Context &context, KeyHandle key,
	const CssmData &data, CssmData &signature, Allocator &alloc, CSSM_ALGORITHMS signOnlyAlgorithm)
{
	SendContext ctx(context);
	DataOutput sig(signature, alloc);
	IPCKEY(ucsp_client_generateSignature(UCSP_ARGS, CONTEXT(ctx), key, signOnlyAlgorithm,
		DATA(data), DATA(sig)),
		key, CSSM_ACL_AUTHORIZATION_SIGN);
}

void ClientSession::verifySignature(const Context &context, KeyHandle key,
	const CssmData &data, const CssmData &signature, CSSM_ALGORITHMS verifyOnlyAlgorithm)
{
	SendContext ctx(context);
	IPC(ucsp_client_verifySignature(UCSP_ARGS, CONTEXT(ctx), key, verifyOnlyAlgorithm,
		DATA(data), DATA(signature)));
}


void ClientSession::generateMac(const Context &context, KeyHandle key,
	const CssmData &data, CssmData &signature, Allocator &alloc)
{
	SendContext ctx(context);
	DataOutput sig(signature, alloc);
	IPCKEY(ucsp_client_generateMac(UCSP_ARGS, CONTEXT(ctx), key,
		DATA(data), DATA(sig)),
		key, CSSM_ACL_AUTHORIZATION_MAC);
}

void ClientSession::verifyMac(const Context &context, KeyHandle key,
	const CssmData &data, const CssmData &signature)
{
	SendContext ctx(context);
	IPCKEY(ucsp_client_verifyMac(UCSP_ARGS, CONTEXT(ctx), key,
		DATA(data), DATA(signature)),
		key, CSSM_ACL_AUTHORIZATION_MAC);
}


//
// Encryption/Decryption
//
	
void ClientSession::encrypt(const Context &context, KeyHandle key,
	const CssmData &clear, CssmData &cipher, Allocator &alloc)
{
	SendContext ctx(context);
	DataOutput cipherOut(cipher, alloc);
	IPCKEY(ucsp_client_encrypt(UCSP_ARGS, CONTEXT(ctx), key, DATA(clear), DATA(cipherOut)),
		key, CSSM_ACL_AUTHORIZATION_ENCRYPT);
}

void ClientSession::decrypt(const Context &context, KeyHandle key,
	const CssmData &cipher, CssmData &clear, Allocator &alloc)
{
    Debug::trace (kSecTraceUCSPServerDecryptBegin);
    
	SendContext ctx(context);
	DataOutput clearOut(clear, alloc);
	IPCKEY(ucsp_client_decrypt(UCSP_ARGS, CONTEXT(ctx), key, DATA(cipher), DATA(clearOut)),
		key, CSSM_ACL_AUTHORIZATION_DECRYPT);
}


//
// Key generation
//
void ClientSession::generateKey(DbHandle db, const Context &context, uint32 keyUsage, uint32 keyAttr,
    const AccessCredentials *cred, const AclEntryInput *owner,
    KeyHandle &newKey, CssmKey::Header &newHeader)
{
	SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> proto(&owner->proto(), internalAllocator);
	IPC(ucsp_client_generateKey(UCSP_ARGS, db, CONTEXT(ctx),
		COPY(creds), COPY(proto), keyUsage, keyAttr, &newKey, &newHeader));
}

void ClientSession::generateKey(DbHandle db, const Context &context,
    uint32 pubKeyUsage, uint32 pubKeyAttr,
    uint32 privKeyUsage, uint32 privKeyAttr,
    const AccessCredentials *cred, const AclEntryInput *owner,
    KeyHandle &pubKey, CssmKey::Header &pubHeader,
    KeyHandle &privKey, CssmKey::Header &privHeader)
{
	SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> proto(&owner->proto(), internalAllocator);
	IPC(ucsp_client_generateKeyPair(UCSP_ARGS, db, CONTEXT(ctx),
		COPY(creds), COPY(proto),
		pubKeyUsage, pubKeyAttr, privKeyUsage, privKeyAttr,
		&pubKey, &pubHeader, &privKey, &privHeader));
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
    SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> proto(&owner->proto(), internalAllocator);
	CssmDeriveData inParamForm(param, context.algorithm());
	Copier<CssmDeriveData> inParam(&inParamForm, internalAllocator);
    DataOutput paramOutput(param, allocator);
	IPCKEY(ucsp_client_deriveKey(UCSP_ARGS, db, CONTEXT(ctx), baseKey,
		COPY(creds), COPY(proto), COPY(inParam), DATA(paramOutput),
		usage, attrs, &newKey, &newHeader),
		baseKey, CSSM_ACL_AUTHORIZATION_DERIVE);
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
	SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	DataOutput keyData(wrappedKey, alloc);
	IPCKEY(ucsp_client_wrapKey(UCSP_ARGS, CONTEXT(ctx), wrappingKey, COPY(creds),
		keyToBeWrapped, OPTIONALDATA(descriptiveData),
		&wrappedKey, DATA(keyData)),
		keyToBeWrapped,
		context.algorithm() == CSSM_ALGID_NONE
			? CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR : CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED);
	wrappedKey = CssmData();	// null out data section (force allocation for key data)
}

void ClientSession::unwrapKey(DbHandle db, const Context &context, KeyHandle key,
    KeyHandle publicKey, const CssmWrappedKey &wrappedKey,
	uint32 usage, uint32 attr,
	const AccessCredentials *cred, const AclEntryInput *acl,
	CssmData &descriptiveData,
    KeyHandle &newKey, CssmKey::Header &newHeader, Allocator &alloc)
{
	SendContext ctx(context);
	DataOutput descriptor(descriptiveData, alloc);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> proto(&acl->proto(), internalAllocator);
	IPCKEY(ucsp_client_unwrapKey(UCSP_ARGS, db, CONTEXT(ctx), key,
		COPY(creds), COPY(proto),
		publicKey, wrappedKey, DATA(wrappedKey), usage, attr, DATA(descriptor),
        &newKey, &newHeader),
		key, CSSM_ACL_AUTHORIZATION_DECRYPT);
}


//
// ACL management
//
void ClientSession::getAcl(AclKind kind, GenericHandle key, const char *tag,
	uint32 &infoCount, AclEntryInfo * &infoArray, Allocator &alloc)
{
	uint32 count;
	COPY_OUT_DECL(AclEntryInfo,info);
	IPC(ucsp_client_getAcl(UCSP_ARGS, kind, key,
		(tag != NULL), tag ? tag : "",
		&count, COPY_OUT(info)));
	VMGuard _(info, infoLength);
	infoCount = count;

	// relocate incoming AclEntryInfo array
	ReconstituteWalker relocator(info, infoBase);
	for (uint32 n = 0; n < count; n++)
		walk(relocator, info[n]);

	// copy AclEntryInfo array into discrete memory nodes
	infoArray = alloc.alloc<AclEntryInfo>(count);
	ChunkCopyWalker chunker(alloc);
	for (uint32 n = 0; n < count; n++) {
		infoArray[n] = info[n];
		walk(chunker, infoArray[n]);
	}
}

void ClientSession::changeAcl(AclKind kind, GenericHandle key, const AccessCredentials &cred,
	const AclEdit &edit)
{
	Copier<AccessCredentials> creds(&cred, internalAllocator);
	//@@@ ignoring callback
	Copier<AclEntryInput> newEntry(edit.newEntry(), internalAllocator);
	IPCKEY(ucsp_client_changeAcl(UCSP_ARGS, kind, key, COPY(creds),
		edit.mode(), edit.handle(), COPY(newEntry)),
		key, CSSM_ACL_AUTHORIZATION_CHANGE_ACL);
}

void ClientSession::getOwner(AclKind kind, GenericHandle key, AclOwnerPrototype &owner,
    Allocator &alloc)
{
	COPY_OUT_DECL(AclOwnerPrototype, proto);
	IPC(ucsp_client_getOwner(UCSP_ARGS, kind, key, COPY_OUT(proto)));
	// turn the returned AclOwnerPrototype into its proper output form
	relocate(proto, protoBase);
	owner.TypedSubject = chunkCopy(proto->subject(), alloc);
	owner.Delegate = proto->delegate();
}

void ClientSession::changeOwner(AclKind kind, GenericHandle key,
	const AccessCredentials &cred, const AclOwnerPrototype &proto)
{
	Copier<AccessCredentials> creds(&cred, internalAllocator);
	Copier<AclOwnerPrototype> protos(&proto, internalAllocator);
	IPCKEY(ucsp_client_setOwner(UCSP_ARGS, kind, key, COPY(creds), COPY(protos)),
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
    SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> proto(&owner->proto(), internalAllocator);
	IPC(ucsp_client_extractMasterKey(UCSP_ARGS, db, CONTEXT(ctx), sourceDb,
		COPY(creds), COPY(proto),
		keyUsage, keyAttr, &newKey, &newHeader));
}


//
// Authorization subsystem entry
//
void ClientSession::authCreate(const AuthorizationItemSet *rights,
	const AuthorizationItemSet *environment, AuthorizationFlags flags,
	AuthorizationBlob &result)
{
	Copier<AuthorizationItemSet> rightSet(rights, internalAllocator);
	Copier<AuthorizationItemSet> environ(environment, internalAllocator);
	IPC(ucsp_client_authorizationCreate(UCSP_ARGS,
		COPY(rightSet), flags, COPY(environ), &result));
}

void ClientSession::authRelease(const AuthorizationBlob &auth, 
	AuthorizationFlags flags)
{
	IPC(ucsp_client_authorizationRelease(UCSP_ARGS, auth, flags));
}

void ClientSession::authCopyRights(const AuthorizationBlob &auth,
	const AuthorizationItemSet *rights, const AuthorizationItemSet *environment,
	AuthorizationFlags flags,
	AuthorizationItemSet **grantedRights)
{
	Copier<AuthorizationItemSet> rightSet(rights, internalAllocator);
	Copier<AuthorizationItemSet> environ(environment, internalAllocator);
	COPY_OUT_DECL(AuthorizationItemSet, result);
	IPC(ucsp_client_authorizationCopyRights(UCSP_ARGS, auth, COPY(rightSet),
		flags | (grantedRights ? 0 : kAuthorizationFlagNoData),
		COPY(environ), COPY_OUT(result)));
	VMGuard _(result, resultLength);
	// return rights vector (only) if requested
	if (grantedRights) {
		relocate(result, resultBase);
		*grantedRights = copy(result, returnAllocator);
	}
}

void ClientSession::authCopyInfo(const AuthorizationBlob &auth,
	const char *tag,
	AuthorizationItemSet * &info)
{
	COPY_OUT_DECL(AuthorizationItemSet, result);
    if (tag == NULL)
        tag = "";
    else if (tag[0] == '\0')
        MacOSError::throwMe(errAuthorizationInvalidTag);
	IPC(ucsp_client_authorizationCopyInfo(UCSP_ARGS, auth, tag, COPY_OUT(result)));
	VMGuard _(result, resultLength);
	relocate(result, resultBase);
	info = copy(result, returnAllocator);
}

void ClientSession::authExternalize(const AuthorizationBlob &auth,
	AuthorizationExternalForm &extForm)
{
	IPC(ucsp_client_authorizationExternalize(UCSP_ARGS, auth, &extForm));
}

void ClientSession::authInternalize(const AuthorizationExternalForm &extForm,
	AuthorizationBlob &auth)
{
	IPC(ucsp_client_authorizationInternalize(UCSP_ARGS, extForm, &auth));
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


//
// Notification subsystem
//
void ClientSession::requestNotification(Port receiver, NotificationDomain domain, NotificationMask events)
{
    IPC(ucsp_client_requestNotification(UCSP_ARGS, receiver, domain, events));
}

void ClientSession::stopNotification(Port port)
{
    IPC(ucsp_client_stopNotification(UCSP_ARGS, port.port()));
}

void ClientSession::postNotification(NotificationDomain domain, NotificationEvent event, const CssmData &data)
{
	// this is a simpleroutine
	ucsp_client_postNotification(mGlobal().serverPort, domain, event, DATA(data));
}

OSStatus ClientSession::dispatchNotification(const mach_msg_header_t *message,
    ConsumeNotification *consumer, void *context) throw()
{
	const __Request__notify_t *msg = reinterpret_cast<const __Request__notify_t *>(message);
	OSStatus status;
	try {
		status = consumer(msg->domain, msg->event, msg->data.address, msg->dataCnt, context);
	} catch (const CommonError &err) {
		status = err.osStatus();
	} catch (const std::bad_alloc &) {
		status = memFullErr;
	} catch (...) {
		status = internalComponentErr;
	}

    mig_deallocate((vm_offset_t) msg->data.address, msg->dataCnt);
#if 0
    msg->data.address = (vm_offset_t) 0;
    msg->data.size = (mach_msg_size_t) 0;
#endif

    return status;
}


OSStatus ClientSession::dispatchNotification(const mach_msg_header_t *message,
	NotificationConsumer *consumer) throw ()
{
	return dispatchNotification(message, consumerDispatch, consumer);
}

OSStatus ClientSession::consumerDispatch(NotificationDomain domain, NotificationEvent event,
	const void *data, size_t dataLength, void *context)
{
	try {
		reinterpret_cast<NotificationConsumer *>(context)->consume(
			domain, event, CssmData::wrap(data, dataLength));
		return noErr;
	} catch (const CommonError &error) {
		return error.osStatus();
	} catch (const std::bad_alloc &) {
		return memFullErr;
	} catch (...) {
		return internalComponentErr;
	}
}

ClientSession::NotificationConsumer::~NotificationConsumer()
{ }


//
// authorizationdbGet/Set/Remove
//
void ClientSession::authorizationdbGet(const AuthorizationString rightname, CssmData &rightDefinition, Allocator &alloc)
{
	DataOutput definition(rightDefinition, alloc);
	IPC(ucsp_client_authorizationdbGet(UCSP_ARGS, rightname, DATA(definition)));
}

void ClientSession::authorizationdbSet(const AuthorizationBlob &auth, const AuthorizationString rightname, uint32_t rightDefinitionLength, const void *rightDefinition)
{
	// @@@ DATA_IN in transition.cpp is not const void *
	IPC(ucsp_client_authorizationdbSet(UCSP_ARGS, auth, rightname, const_cast<void *>(rightDefinition), rightDefinitionLength));
}

void ClientSession::authorizationdbRemove(const AuthorizationBlob &auth, const AuthorizationString rightname)
{
	IPC(ucsp_client_authorizationdbRemove(UCSP_ARGS, auth, rightname));
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


} // end namespace SecurityServer
} // end namespace Security
