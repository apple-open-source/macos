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


//
// sstransit - SecurityServer client library transition code.
//
// These are the functions that implement CssmClient methods in terms of
// MIG IPC client calls, plus their supporting machinery.
//
#include "sstransit.h"
#include <Security/cspclient.h>
#include <Security/ktracecodes.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

namespace Security {

using MachPlusPlus::check;
using MachPlusPlus::VMGuard;


//
// DataOutput helper.
// This happens "at the end" of a glue method, via the DataOutput destructor.
//
DataOutput::~DataOutput()
{
	VMGuard _(mData, mLength);
	if (mData) {	// was assigned to; IPC returned OK
		if (argument) {	// buffer was provided
			if (argument.length() < mLength)
				CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
			argument.length(mLength);
		} else {	// allocate buffer
			argument = CssmData(allocator.malloc(mLength), mLength);
		}
		memcpy(argument.data(), mData, mLength);
	}
}


CssmList chunkCopy(CssmList &list, CssmAllocator &alloc)
{
	CssmList copy = list;
	ChunkCopyWalker w(alloc);
	walk(w, copy);
	return copy;
}


//
// Create a packaged-up Context for IPC transmission.
// In addition to collecting the context into a contiguous blob for transmission,
// we also evaluate CssmCryptoData callbacks at this time.
//
SendContext::SendContext(const Security::Context &ctx) : context(ctx)
{
	CssmCryptoData cryptoDataValue;	// holding area for CssmCryptoData element
	IFDEBUG(uint32 cryptoDataUsed = 0);
	Context::Builder builder(CssmAllocator::standard());
	for (unsigned n = 0; n < ctx.attributesInUse(); n++) {
		switch (ctx[n].baseType()) {
		case CSSM_ATTRIBUTE_DATA_CRYPTO_DATA: {
			CssmCryptoData &data = ctx[n];	// extract CssmCryptoData value
			cryptoDataValue = data();		// evaluate callback (if any)
			builder.setup(&cryptoDataValue); // use evaluted value
			IFDEBUG(cryptoDataUsed++);
			break;
		}
		default:
			builder.setup(ctx[n]);
			break;
		}
	}
	attributeSize = builder.make();
	for (unsigned n = 0; n < ctx.attributesInUse(); n++) {
		const Context::Attr &attr = ctx[n];
		switch (attr.baseType()) {
		case CSSM_ATTRIBUTE_DATA_CRYPTO_DATA:
			builder.put(attr.type(), &cryptoDataValue);
			break;
		default:
			builder.put(attr);
			break;
		}
	}
	uint32 count;	// not needed
	builder.done(attributes, count);
	assert(cryptoDataUsed <= 1);	// no more than one slot converted
}


//
// Copy an AccessCredentials for shipment.
// In addition, scan the samples for "special" database locking samples
// and translate certain items for safe shipment. Note that this overwrites
// part of the CssmList value (CSPHandle -> SS/KeyHandle), but we do it on
// the COPY, so that's okay.
//
DatabaseAccessCredentials::DatabaseAccessCredentials(const AccessCredentials *creds, CssmAllocator &alloc)
	: Copier<AccessCredentials>(creds, alloc)
{
	if (creds) {
		for (uint32 n = 0; n < value()->samples().length(); n++) {
			TypedList sample = value()->samples()[n];
			sample.checkProper();
			switch (sample.type()) {
			case CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK:
			case CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK:
				sample.snip();	// skip sample type
				sample.checkProper();
				if (sample.type() == CSSM_WORDID_SYMMETRIC_KEY) {
					secdebug("SSclient", "key sample encountered");
					// proper form is sample[1] = DATA:CSPHandle, sample[2] = DATA:CSSM_KEY
					if (sample.length() != 3
						|| sample[1].type() != CSSM_LIST_ELEMENT_DATUM
						|| sample[2].type() != CSSM_LIST_ELEMENT_DATUM)
							CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
					mapKeySample(
						*sample[1].data().interpretedAs<CSSM_CSP_HANDLE>(CSSM_ERRCODE_INVALID_SAMPLE_VALUE),
						*sample[2].data().interpretedAs<CssmKey>(CSSM_ERRCODE_INVALID_SAMPLE_VALUE));
				}
				break;
			default:
				break;
			}
		}
	}
}

void DatabaseAccessCredentials::mapKeySample(CSSM_CSP_HANDLE &cspHandle, CssmKey &key)
{
	// if the key belongs to the AppleCSPDL, look it up and write the SS KeyHandle
	// into the CSPHandle element for transmission
	if (key.header().cspGuid() == gGuidAppleCSPDL) {
		// @@@ can't use CssmClient (it makes its own attachments)
		CSSM_CC_HANDLE ctx;
		if (CSSM_RETURN err = CSSM_CSP_CreatePassThroughContext(cspHandle, &key, &ctx))
			CssmError::throwMe(err);
		KeyHandle ssKey;
		CSSM_RETURN passthroughError =
			CSSM_CSP_PassThrough(ctx, CSSM_APPLESCPDL_CSP_GET_KEYHANDLE, NULL, (void **)&ssKey);
		CSSM_DeleteContext(ctx);	// ignore error
		if (passthroughError)
			CssmError::throwMe(passthroughError);
		// we happen to know that they're both uint32 values
		assert(sizeof(CSSM_CSP_HANDLE) >= sizeof(KeyHandle));
		cspHandle = ssKey;
		secdebug("SSclient", "key sample mapped to key 0x%lx", ssKey);
	}
}


namespace SecurityServer
{

//
// Database control
//
DbHandle ClientSession::createDb(const DLDbIdentifier &dbId,
    const AccessCredentials *cred, const AclEntryInput *owner,
    const DBParameters &params)
{
	DatabaseAccessCredentials creds(cred, internalAllocator);
	Copier<AclEntryPrototype> proto(&owner->proto(), internalAllocator);
    DataWalkers::DLDbFlatIdentifier ident(dbId);
    Copier<DataWalkers::DLDbFlatIdentifier> id(&ident, internalAllocator);
	DbHandle db;
	IPC(ucsp_client_createDb(UCSP_ARGS, &db, COPY(id), COPY(creds), COPY(proto), params));
	return db;
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

void ClientSession::encodeDb(DbHandle db, CssmData &blob, CssmAllocator &alloc)
{
	DataOutput outBlob(blob, alloc);
	IPC(ucsp_client_encodeDb(UCSP_ARGS, db, DATA(outBlob)));
}

void ClientSession::releaseDb(DbHandle db)
{
	IPC(ucsp_client_releaseDb(UCSP_ARGS, db));
}

void ClientSession::getDbSuggestedIndex(DbHandle db, CssmData &index, CssmAllocator &alloc)
{
	DataOutput outBlob(index, alloc);
	IPC(ucsp_client_getDbIndex(UCSP_ARGS, db, DATA(outBlob)));
}

void ClientSession::authenticateDb(DbHandle db, DBAccessType type,
	const AccessCredentials *cred)
{
    DatabaseAccessCredentials creds(cred, internalAllocator);
	IPC(ucsp_client_authenticateDb(UCSP_ARGS, db, COPY(creds)));
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
	IPC(ucsp_client_lockDb(UCSP_ARGS, db));
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
    KeyUID *uid, CssmAllocator &alloc)
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
void ClientSession::generateRandom(CssmData &data)
{
	void *result;
	mach_msg_type_number_t resultLength;
	IPC(ucsp_client_generateRandom(UCSP_ARGS, data.length(), &result, &resultLength));
	assert(resultLength == data.length());
	memcpy(data.data(), result, data.length());
}


//
// Signatures and MACs
//
void ClientSession::generateSignature(const Context &context, KeyHandle key,
	const CssmData &data, CssmData &signature, CssmAllocator &alloc, CSSM_ALGORITHMS signOnlyAlgorithm)
{
	SendContext ctx(context);
	DataOutput sig(signature, alloc);
	IPC(ucsp_client_generateSignature(UCSP_ARGS, CONTEXT(ctx), key, signOnlyAlgorithm,
		DATA(data), DATA(sig)));
}

void ClientSession::verifySignature(const Context &context, KeyHandle key,
	const CssmData &data, const CssmData &signature, CSSM_ALGORITHMS verifyOnlyAlgorithm)
{
	SendContext ctx(context);
	IPC(ucsp_client_verifySignature(UCSP_ARGS, CONTEXT(ctx), key, verifyOnlyAlgorithm,
		DATA(data), DATA(signature)));
}


void ClientSession::generateMac(const Context &context, KeyHandle key,
	const CssmData &data, CssmData &signature, CssmAllocator &alloc)
{
	SendContext ctx(context);
	DataOutput sig(signature, alloc);
	IPC(ucsp_client_generateMac(UCSP_ARGS, CONTEXT(ctx), key,
		DATA(data), DATA(sig)));
}

void ClientSession::verifyMac(const Context &context, KeyHandle key,
	const CssmData &data, const CssmData &signature)
{
	SendContext ctx(context);
	IPC(ucsp_client_verifyMac(UCSP_ARGS, CONTEXT(ctx), key,
		DATA(data), DATA(signature)));
}


//
// Encryption/Decryption
//
	
void ClientSession::encrypt(const Context &context, KeyHandle key,
	const CssmData &clear, CssmData &cipher, CssmAllocator &alloc)
{
	SendContext ctx(context);
	DataOutput cipherOut(cipher, alloc);
	IPC(ucsp_client_encrypt(UCSP_ARGS, CONTEXT(ctx), key, DATA(clear), DATA(cipherOut)));
}

void ClientSession::decrypt(const Context &context, KeyHandle key,
	const CssmData &cipher, CssmData &clear, CssmAllocator &alloc)
{
    Debug::trace (kSecTraceUCSPServerDecryptBegin);
    
	SendContext ctx(context);
	DataOutput clearOut(clear, alloc);
	IPC(ucsp_client_decrypt(UCSP_ARGS, CONTEXT(ctx), key, DATA(cipher), DATA(clearOut)));
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
// This is a bit strained; the incoming 'param' value may have structure
// and needs to be handled on a per-algorithm basis, which means we have to
// know which key derivation algorithms we support for passing to our CSP(s).
// The default behavior is to handle "flat" data blobs, which is as good
// a default as we can manage.
// NOTE: The param-specific handling must be synchronized with the server
// transition layer code (in transition.cpp).
//
void ClientSession::deriveKey(DbHandle db, const Context &context, KeyHandle baseKey,
    uint32 keyUsage, uint32 keyAttr, CssmData &param,
    const AccessCredentials *cred, const AclEntryInput *owner,
    KeyHandle &newKey, CssmKey::Header &newHeader, CssmAllocator &allocator)
{
    SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> proto(&owner->proto(), internalAllocator);
    DataOutput paramOutput(param, allocator);
    switch (context.algorithm()) {
    case CSSM_ALGID_PKCS5_PBKDF2: {
        typedef CSSM_PKCS5_PBKDF2_PARAMS Params;
        Copier<Params> params(param.interpretedAs<Params>(CSSM_ERRCODE_INVALID_INPUT_POINTER),
			internalAllocator);
        IPC(ucsp_client_deriveKey(UCSP_ARGS, db, CONTEXT(ctx), baseKey,
            COPY(creds), COPY(proto), COPY(params), DATA(paramOutput),
            keyUsage, keyAttr, &newKey, &newHeader));
        break; }
    default: {
        IPC(ucsp_client_deriveKey(UCSP_ARGS, db, CONTEXT(ctx), baseKey,
            COPY(creds), COPY(proto),
            param.data(), param.length(), param.data(),
            DATA(paramOutput),
            keyUsage, keyAttr, &newKey, &newHeader));
        break; }
    }
}


//
// Digest generation
//
void ClientSession::getKeyDigest(KeyHandle key, CssmData &digest, CssmAllocator &allocator)
{
	DataOutput dig(digest, allocator);
	IPC(ucsp_client_getKeyDigest(UCSP_ARGS, key, DATA(dig)));
}


//
// Key wrapping and unwrapping
//
void ClientSession::wrapKey(const Context &context, KeyHandle wrappingKey,
    KeyHandle keyToBeWrapped, const AccessCredentials *cred,
	const CssmData *descriptiveData, CssmWrappedKey &wrappedKey, CssmAllocator &alloc)
{
	SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	DataOutput keyData(wrappedKey, alloc);
	IPC(ucsp_client_wrapKey(UCSP_ARGS, CONTEXT(ctx), wrappingKey, COPY(creds),
		keyToBeWrapped, OPTIONALDATA(descriptiveData),
		&wrappedKey, DATA(keyData)));
	wrappedKey = CssmData();	// null out data section (force allocation for key data)
}

void ClientSession::unwrapKey(DbHandle db, const Context &context, KeyHandle key,
    KeyHandle publicKey, const CssmWrappedKey &wrappedKey,
	uint32 usage, uint32 attr,
	const AccessCredentials *cred, const AclEntryInput *acl,
	CssmData &descriptiveData,
    KeyHandle &newKey, CssmKey::Header &newHeader, CssmAllocator &alloc)
{
	SendContext ctx(context);
	DataOutput descriptor(descriptiveData, alloc);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> proto(&acl->proto(), internalAllocator);
	IPC(ucsp_client_unwrapKey(UCSP_ARGS, db, CONTEXT(ctx), key,
		COPY(creds), COPY(proto),
		publicKey, wrappedKey, DATA(wrappedKey), usage, attr, DATA(descriptor),
        &newKey, &newHeader));
}


//
// ACL management
//
void ClientSession::getAcl(AclKind kind, KeyHandle key, const char *tag,
	uint32 &infoCount, AclEntryInfo * &infoArray, CssmAllocator &alloc)
{
	uint32 count;
	AclEntryInfo *info, *infoBase;
	mach_msg_type_number_t infoLength;
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

void ClientSession::changeAcl(AclKind kind, KeyHandle key, const AccessCredentials &cred,
	const AclEdit &edit)
{
	Copier<AccessCredentials> creds(&cred, internalAllocator);
	//@@@ ignoring callback
	Copier<AclEntryInput> newEntry(edit.newEntry(), internalAllocator);
	IPC(ucsp_client_changeAcl(UCSP_ARGS, kind, key, COPY(creds),
		edit.mode(), edit.handle(), COPY(newEntry)));
}

void ClientSession::getOwner(AclKind kind, KeyHandle key, AclOwnerPrototype &owner,
    CssmAllocator &alloc)
{
	AclOwnerPrototype *proto, *protoBase;
	mach_msg_type_number_t protoLength;
	IPC(ucsp_client_getOwner(UCSP_ARGS, kind, key, COPY_OUT(proto)));
	// turn the returned AclOwnerPrototype into its proper output form
	relocate(proto, protoBase);
	owner.TypedSubject = chunkCopy(proto->subject(), alloc);
	owner.Delegate = proto->delegate();
}

void ClientSession::changeOwner(AclKind kind, KeyHandle key,
	const AccessCredentials &cred, const AclOwnerPrototype &proto)
{
	Copier<AccessCredentials> creds(&cred, internalAllocator);
	Copier<AclOwnerPrototype> protos(&proto, internalAllocator);
	IPC(ucsp_client_setOwner(UCSP_ARGS, kind, key, COPY(creds), COPY(protos)));
}


void ClientSession::getKeyAcl(DbHandle db, const char *tag,
	uint32 &count, AclEntryInfo * &info, CssmAllocator &alloc)
{ getAcl(keyAcl, db, tag, count, info, alloc); }

void ClientSession::changeKeyAcl(DbHandle db, const AccessCredentials &cred,
	const AclEdit &edit)
{ changeAcl(keyAcl, db, cred, edit); }

void ClientSession::getKeyOwner(DbHandle db, AclOwnerPrototype &owner, CssmAllocator &alloc)
{ getOwner(keyAcl, db, owner, alloc); }

void ClientSession::changeKeyOwner(DbHandle db, const AccessCredentials &cred,
	const AclOwnerPrototype &edit)
{ changeOwner(keyAcl, db, cred, edit); }

void ClientSession::getDbAcl(DbHandle db, const char *tag,
	uint32 &count, AclEntryInfo * &info, CssmAllocator &alloc)
{ getAcl(dbAcl, db, tag, count, info, alloc); }

void ClientSession::changeDbAcl(DbHandle db, const AccessCredentials &cred,
	const AclEdit &edit)
{ changeAcl(dbAcl, db, cred, edit); }

void ClientSession::getDbOwner(DbHandle db, AclOwnerPrototype &owner, CssmAllocator &alloc)
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
	KeyHandle &newKey, CssmKey::Header &newHeader, CssmAllocator &alloc)
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
// Notification subsystem
//
void ClientSession::requestNotification(Port receiver, Listener::Domain domain, Listener::EventMask events)
{
    IPC(ucsp_client_requestNotification(UCSP_ARGS, receiver, domain, events));
}

void ClientSession::stopNotification(Port port)
{
    IPC(ucsp_client_stopNotification(UCSP_ARGS, port.port()));
}

void ClientSession::postNotification(Listener::Domain domain, Listener::Event event, const CssmData &data)
{
    IPC(ucsp_client_postNotification(UCSP_ARGS, domain, event, DATA(data)));
}

OSStatus ClientSession::dispatchNotification(const mach_msg_header_t *message,
    ConsumeNotification *consumer, void *context) throw()
{
    struct Message {
        mach_msg_header_t Head;
        /* start of the kernel processed data */
        mach_msg_body_t msgh_body;
        mach_msg_ool_descriptor_t data;
        /* end of the kernel processed data */
        NDR_record_t NDR;
        uint32 domain;
        uint32 event;
        mach_msg_type_number_t dataCnt;
        uint32 sender;
    } *msg = (Message *)message;

	OSStatus status;
	try
	{
		status = consumer(msg->domain, msg->event, msg->data.address, msg->dataCnt, context);
	}
	catch (const CssmCommonError &err) { status = err.osStatus(); }
	catch (const std::bad_alloc &) { status = memFullErr; }
	catch (...) { status = internalComponentErr; }

    mig_deallocate((vm_offset_t) msg->data.address, msg->dataCnt);
    msg->data.address = (vm_offset_t) 0;
    msg->data.size = (mach_msg_size_t) 0;

    return status;
}


//
// authorizationdbGet/Set/Remove
//
void ClientSession::authorizationdbGet(const AuthorizationString rightname, CssmData &rightDefinition, CssmAllocator &alloc)
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
