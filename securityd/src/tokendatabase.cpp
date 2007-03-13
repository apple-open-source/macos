/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// tokendatabase - software database container implementation.
//
#include "tokendatabase.h"
#include "tokenkey.h"
#include "tokenaccess.h"
#include "process.h"
#include "server.h"
#include "localkey.h"		// to retrieve local raw keys
#include <security_cdsa_client/wrapkey.h>


//
// Construct a TokenDbCommon
//
TokenDbCommon::TokenDbCommon(Session &ssn, Token &tk, const char *name)
	: DbCommon(ssn), mDbName(name ? name : ""), mResetLevel(0)
{
	secdebug("tokendb", "creating tokendbcommon %p: with token %p", this, &tk);
	parent(tk);
}

TokenDbCommon::~TokenDbCommon()
{
	secdebug("tokendb", "destroying tokendbcommon %p", this);
	token().removeCommon(*this);		// unregister from Token
}

Token &TokenDbCommon::token() const
{
	return parent<Token>();
}

const std::string &TokenDbCommon::dbName() const
{
	return token().printName();
}


//
// A TokenDbCommon holds per-session adornments for the ACL machine
//
Adornable &TokenDbCommon::store()
{
	StLock<Mutex> _(*this);
	
	// if this is the first one, hook for lifetime
	if (mAdornments.empty()) {
		session().addReference(*this);		// hold and slave to SSN lifetime
		token().addCommon(*this);			// register with Token
	}

	// return our (now active) adornments
	return mAdornments;
}

void TokenDbCommon::resetAcls()
{
	StLock<Mutex> _(*this);
	if (!mAdornments.empty()) {
		mAdornments.clearAdornments();		// clear ACL state
		session().removeReference(*this);	// unhook from SSN
	}
	token().removeCommon(*this);			// unregister from Token
}


//
// Process (our part of) a "lock all" request.
// Smartcard tokens interpret a "lock" as a forced card reset, transmitted
// to tokend as an authenticate request.
// @@@ Virtual reset for multi-session tokens. Right now, we're using the sledge hammer.
//
void TokenDbCommon::lockProcessing()
{
	Access access(token());
	access().authenticate(CSSM_DB_ACCESS_RESET, NULL);
}

//
// Construct a TokenDatabase given subservice information.
// We are currently ignoring the 'name' argument.
//
TokenDatabase::TokenDatabase(uint32 ssid, Process &proc,
	const char *name, const AccessCredentials *cred)
	: Database(proc)
{
	// locate Token object
	RefPointer<Token> token = Token::find(ssid);
	
	Session &session = process().session();
	StLock<Mutex> _(session);
	if (TokenDbCommon *dbcom = session.findFirst<TokenDbCommon, uint32>(&TokenDbCommon::subservice, ssid)) {
		parent(*dbcom);
		secdebug("tokendb", "open tokendb %p(%ld) at known common %p",
			this, subservice(), dbcom);
	} else {
		// DbCommon not present; make a new one
		parent(*new TokenDbCommon(proc.session(), *token, name));
		secdebug("tokendb", "open tokendb %p(%ld) with new common %p",
			this, subservice(), &common());
	}
	mOpenCreds = copy(cred, Allocator::standard());
	proc.addReference(*this);
}

TokenDatabase::~TokenDatabase()
{
	Allocator::standard().free(mOpenCreds);
}


//
// Basic Database virtual implementations
//
TokenDbCommon &TokenDatabase::common() const
{
	return parent<TokenDbCommon>();
}

TokenDaemon &TokenDatabase::tokend()
{
	return common().token().tokend();
}

const char *TokenDatabase::dbName() const
{
	return common().dbName().c_str();
}

bool TokenDatabase::transient() const
{
	//@@@ let tokend decide? Are there any secure transient keystores?
	return false;
}


SecurityServerAcl &TokenDatabase::acl()
{
	return token();
}

bool TokenDatabase::isLocked() const
{
	Access access(token());
	return access().isLocked();
}


//
// TokenDatabases implement the dbName-setting function.
// This sets the print name of the token, which is persistently
// stored in the token cache. So this is a de-facto rename of
// the token, at least on this system.
//
void TokenDatabase::dbName(const char *name)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}


//
// Given a key handle and CssmKey returned from tokend, create a Key representing
// it. This takes care of raw returns by turning them into keys of the process's
// local transient store.
//
RefPointer<Key> TokenDatabase::makeKey(KeyHandle hKey, const CssmKey *key,
	const AclEntryPrototype *owner)
{
	switch (key->blobType()) {
	case CSSM_KEYBLOB_REFERENCE:
		return new TokenKey(*this, hKey, key->header());
	case CSSM_KEYBLOB_RAW:
		return process().makeTemporaryKey(*key, 0, owner);
	default:
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	// bad key return from tokend
	}
	//@@@ Server::releaseWhenDone(key);
}


//
// Adjust key attributes for newly created keys
//
static CSSM_KEYATTR_FLAGS modattrs(CSSM_KEYATTR_FLAGS attrs)
{
	static const CSSM_KEYATTR_FLAGS CSSM_KEYATTR_RETURN_FLAGS = 0xff000000;
	switch (attrs & CSSM_KEYATTR_RETURN_FLAGS) {
	case CSSM_KEYATTR_RETURN_REF:
	case CSSM_KEYATTR_RETURN_DATA:
		break;	// as requested
	case CSSM_KEYATTR_RETURN_NONE:
		CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK);
	case CSSM_KEYATTR_RETURN_DEFAULT:
		if (attrs & CSSM_KEYATTR_PERMANENT)
			attrs |= CSSM_KEYATTR_RETURN_REF;
		else
			attrs |= CSSM_KEYATTR_RETURN_DATA;
		break;
	}
	return attrs;
}


//
// TokenDatabases support remote secret validation by sending a secret
// (aka passphrase et al) to tokend for processing.
//
bool TokenDatabase::validateSecret(const AclSubject *subject, const AccessCredentials *cred)
{
	secdebug("tokendb", "%p attempting remote validation", this);
	try {
		Access access(token());
		// @@@ Use cached mode
		access().authenticate(CSSM_DB_ACCESS_READ, cred);
		secdebug("tokendb", "%p remote validation successful", this);
		return true;
	}
	catch (...) {
		secdebug("tokendb", "%p remote validation failed", this);
	//	return false;
	throw;	// try not to mask error
	}
}


//
// Key inquiries
//
void TokenDatabase::queryKeySizeInBits(Key &key, CssmKeySize &result)
{
	Access access(token());
	TRY
	GUARD
	access().queryKeySizeInBits(myKey(key).tokenHandle(), result);
	DONE
}


//
// Signatures and MACs
//
void TokenDatabase::generateSignature(const Context &context, Key &key,
	CSSM_ALGORITHMS signOnlyAlgorithm, const CssmData &data, CssmData &signature)
{
	Access access(token(), key);
	TRY
	key.validate(CSSM_ACL_AUTHORIZATION_SIGN, context);
	GUARD
	access().generateSignature(context, myKey(key).tokenHandle(), data, signature, signOnlyAlgorithm);
	DONE
}


void TokenDatabase::verifySignature(const Context &context, Key &key,
	CSSM_ALGORITHMS verifyOnlyAlgorithm, const CssmData &data, const CssmData &signature)
{
	Access access(token(), key);
	TRY
	GUARD
	access().verifySignature(context, myKey(key).tokenHandle(), data, signature, verifyOnlyAlgorithm);
	DONE
}

void TokenDatabase::generateMac(const Context &context, Key &key,
	const CssmData &data, CssmData &mac)
{
	Access access(token());
	TRY
	key.validate(CSSM_ACL_AUTHORIZATION_MAC, context);
	GUARD
	access().generateMac(context, myKey(key).tokenHandle(), data, mac);
	DONE
}

void TokenDatabase::verifyMac(const Context &context, Key &key,
	const CssmData &data, const CssmData &mac)
{
	Access access(token());
	TRY
	key.validate(CSSM_ACL_AUTHORIZATION_MAC, context);
	GUARD
	access().verifyMac(context, myKey(key).tokenHandle(), data, mac);
	DONE
}


//
// Encryption/decryption
//
void TokenDatabase::encrypt(const Context &context, Key &key,
	const CssmData &clear, CssmData &cipher)
{
	Access access(token());
	TRY
	key.validate(CSSM_ACL_AUTHORIZATION_ENCRYPT, context);
	GUARD
	access().encrypt(context, myKey(key).tokenHandle(), clear, cipher);
	DONE
}
	

void TokenDatabase::decrypt(const Context &context, Key &key,
	const CssmData &cipher, CssmData &clear)
{
	Access access(token());
	TRY
	key.validate(CSSM_ACL_AUTHORIZATION_DECRYPT, context);
	GUARD
	access().decrypt(context, myKey(key).tokenHandle(), cipher, clear);
	DONE
}


//
// Key generation and derivation.
// Currently, we consider symmetric key generation to be fast, but
// asymmetric key generation to be (potentially) slow.
//
void TokenDatabase::generateKey(const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs, RefPointer<Key> &newKey)
{
	Access access(token());
	TRY
	GUARD
	KeyHandle hKey;
	CssmKey *result;
	access().generateKey(context, cred, owner, usage, modattrs(attrs), hKey, result);
	newKey = makeKey(hKey, result, owner);
	DONE
}

void TokenDatabase::generateKey(const Context &context,
	const AccessCredentials *cred, const AclEntryPrototype *owner,
	CSSM_KEYUSE pubUsage, CSSM_KEYATTR_FLAGS pubAttrs,
	CSSM_KEYUSE privUsage, CSSM_KEYATTR_FLAGS privAttrs,
    RefPointer<Key> &publicKey, RefPointer<Key> &privateKey)
{
	Access access(token());
	TRY
	GUARD
	KeyHandle hPrivate, hPublic;
	CssmKey *privKey, *pubKey;
	access().generateKey(context, cred, owner,
		pubUsage, modattrs(pubAttrs), privUsage, modattrs(privAttrs),
		hPublic, pubKey, hPrivate, privKey);
	publicKey = makeKey(hPublic, pubKey, owner);
	privateKey = makeKey(hPrivate, privKey, owner);
	DONE
}


//
// Key wrapping and unwrapping.
// Note that the key argument (the key in the context) is optional because of the special
// case of "cleartext" (null algorithm) wrapping for import/export.
//
void TokenDatabase::wrapKey(const Context &context, const AccessCredentials *cred,
		Key *wrappingKey, Key &subjectKey,
        const CssmData &descriptiveData, CssmKey &wrappedKey)
{
	Access access(token());
	InputKey cWrappingKey(wrappingKey);
	InputKey cSubjectKey(subjectKey);
	TRY
	subjectKey.validate(context.algorithm() == CSSM_ALGID_NONE ?
            CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR : CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED,
        cred);
    if (wrappingKey)
		wrappingKey->validate(CSSM_ACL_AUTHORIZATION_ENCRYPT, context);
	GUARD
	CssmKey *rWrappedKey;
	access().wrapKey(context, cred,
		cWrappingKey, cWrappingKey, cSubjectKey, cSubjectKey,
		descriptiveData, rWrappedKey);
	wrappedKey = *rWrappedKey;
	//@@@ ownership of wrappedKey.keyData() ??
	DONE
}

void TokenDatabase::unwrapKey(const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		Key *wrappingKey, Key *publicKey, CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs,
		const CssmKey wrappedKey, RefPointer<Key> &unwrappedKey, CssmData &descriptiveData)
{
	Access access(token());
	InputKey cWrappingKey(wrappingKey);
	InputKey cPublicKey(publicKey);
	TRY
    if (wrappingKey)
		wrappingKey->validate(CSSM_ACL_AUTHORIZATION_DECRYPT, context);
	// we are not checking access on the public key, if any
	GUARD
	KeyHandle hKey;
	CssmKey *result;
	access().unwrapKey(context, cred, owner,
		cWrappingKey, cWrappingKey, cPublicKey, cPublicKey,
		wrappedKey, usage, modattrs(attrs), descriptiveData, hKey, result);
	unwrappedKey = makeKey(hKey, result, owner);
	DONE
}


//
// Key derivation
//
void TokenDatabase::deriveKey(const Context &context, Key *sourceKey,
	const AccessCredentials *cred, const AclEntryPrototype *owner,
	CssmData *param, CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs, RefPointer<Key> &derivedKey)
{
	Access access(token());
	InputKey cSourceKey(sourceKey);
	TRY
    if (sourceKey)
		sourceKey->validate(CSSM_ACL_AUTHORIZATION_DERIVE, cred);
	GUARD
	KeyHandle hKey;
	CssmKey *result;
	CssmData params = param ? *param : CssmData();
	access().deriveKey(noDb, context,
		cSourceKey, cSourceKey,
		usage, modattrs(attrs), params, cred, owner,
		hKey, result);
	if (param) {
		*param = params;
		//@@@ leak? what's the rule here?
	}
	derivedKey = makeKey(hKey, result, owner);
	DONE
}


//
// Miscellaneous CSSM functions
//
void TokenDatabase::getOutputSize(const Context &context, Key &key,
	uint32 inputSize, bool encrypt, uint32 &result)
{
	Access access(token());
	TRY
	GUARD
	access().getOutputSize(context, myKey(key).tokenHandle(), inputSize, encrypt, result);
	DONE
}


//
// (Re-)Authenticate the database.
// We use dbAuthenticate as the catch-all "do something about authentication" call.
//
void TokenDatabase::authenticate(CSSM_DB_ACCESS_TYPE mode, const AccessCredentials *cred)
{
	Access access(token());
	TRY
	GUARD
	if (mode != CSSM_DB_ACCESS_RESET && cred) {
		secdebug("tokendb", "%p authenticate calling validate", this);
		int pin;
		if (sscanf(cred->EntryTag, "PIN%d", &pin) == 1)
			return validate(CSSM_ACL_AUTHORIZATION_PREAUTH(pin), cred);
	}

	access().authenticate(mode, cred);
	switch (mode) {
	case CSSM_DB_ACCESS_RESET:
		// this mode is known to trigger "lockdown" (i.e. reset)
		common().resetAcls();
		break;
	default:
	{
		// no idea what that did to the token; 
		// But let's remember the new creds for our own sake.
		AccessCredentials *newCred = copy(cred, Allocator::standard());
		Allocator::standard().free(mOpenCreds);
		mOpenCreds = newCred;
		break;
	}
	}
	DONE
}

//
// Data access interface.
//
// Note that the attribute vectors are passed between our two IPC interfaces
// as relocated but contiguous memory blocks (to avoid an extra copy). This means
// you can read them at will, but can't change them in transit unless you're
// willing to repack them right here.
//
void TokenDatabase::findFirst(const CssmQuery &query,
		CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
		CssmData *data, RefPointer<Key> &key,
		RefPointer<Database::Search> &rSearch, RefPointer<Database::Record> &rRecord,
		CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	Access access(token());
	RefPointer<Search> search = new Search(*this);
	RefPointer<Record> record = new Record(*this);
	TRY
	KeyHandle hKey = noKey;
    validate(CSSM_ACL_AUTHORIZATION_DB_READ, openCreds());
	GUARD
	record->tokenHandle() = access().Tokend::ClientSession::findFirst(query,
		inAttributes, inAttributesLength, search->tokenHandle(), NULL, hKey,
		outAttributes, outAttributesLength);
	if (!record->tokenHandle()) {	// no match (but no other error)
		rRecord = NULL;				// return null record
		return;
	}
	if (data) {
		if (!hKey)
			record->validate(CSSM_ACL_AUTHORIZATION_DB_READ, openCreds());
		CssmDbRecordAttributeData *noAttributes;
		mach_msg_type_number_t noAttributesLength;
		access().Tokend::ClientSession::findRecordHandle(record->tokenHandle(),
			NULL, 0, data, hKey, noAttributes, noAttributesLength);
		if (hKey) {		// tokend returned a key reference & data
			CssmKey &keyForm = *data->interpretedAs<CssmKey>(CSSMERR_CSP_INVALID_KEY);
			key = new TokenKey(*this, hKey, keyForm.header());
		}
	}
	rSearch = search->commit();
	rRecord = record->commit();
	DONE
}

void TokenDatabase::findNext(Database::Search *rSearch,
	CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
	CssmData *data, RefPointer<Key> &key, RefPointer<Database::Record> &rRecord,
	CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	Access access(token());
	RefPointer<Record> record = new Record(*this);
	Search *search = safe_cast<Search *>(rSearch);
	TRY
	KeyHandle hKey = noKey;
	validate(CSSM_ACL_AUTHORIZATION_DB_READ, openCreds());
	GUARD
	record->tokenHandle() = access().Tokend::ClientSession::findNext(
		search->tokenHandle(), inAttributes, inAttributesLength,
		NULL, hKey, outAttributes, outAttributesLength);
	if (!record->tokenHandle()) {	// no more matches
		releaseSearch(*search);		// release search handle (consumed by EOD)
		rRecord = NULL;				// return null record
		return;
	}
	if (data) {
		if (!hKey)
			record->validate(CSSM_ACL_AUTHORIZATION_DB_READ, openCreds());
		CssmDbRecordAttributeData *noAttributes;
		mach_msg_type_number_t noAttributesLength;
		access().Tokend::ClientSession::findRecordHandle(record->tokenHandle(),
			NULL, 0, data, hKey, noAttributes, noAttributesLength);
		if (hKey) {		// tokend returned a key reference & data
			CssmKey &keyForm = *data->interpretedAs<CssmKey>(CSSMERR_CSP_INVALID_KEY);
			key = new TokenKey(*this, hKey, keyForm.header());
		}
	}
	rRecord = record->commit();
	DONE
}

void TokenDatabase::findRecordHandle(Database::Record *rRecord,
	CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
	CssmData *data, RefPointer<Key> &key,
	CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	Access access(token());
	Record *record = safe_cast<Record *>(rRecord);
	access.add(*record);
	TRY
	KeyHandle hKey = noKey;
    validate(CSSM_ACL_AUTHORIZATION_DB_READ, openCreds());
    if (data)
        record->validate(CSSM_ACL_AUTHORIZATION_DB_READ, openCreds());
	GUARD
	access().Tokend::ClientSession::findRecordHandle(record->tokenHandle(),
		inAttributes, inAttributesLength, data, hKey, outAttributes, outAttributesLength);
	rRecord = record;
	if (hKey != noKey && data) {		// tokend returned a key reference & data
		CssmKey &keyForm = *data->interpretedAs<CssmKey>(CSSMERR_CSP_INVALID_KEY);
		key = new TokenKey(*this, hKey, keyForm.header());
	}
	DONE
}

void TokenDatabase::insertRecord(CSSM_DB_RECORDTYPE recordType,
	const CssmDbRecordAttributeData *attributes, mach_msg_type_number_t attributesLength,
	const CssmData &data, RefPointer<Database::Record> &rRecord)
{
	Access access(token());
	RefPointer<Record> record = new Record(*this);
	access.add(*record);
	TRY
	validate(CSSM_ACL_AUTHORIZATION_DB_INSERT, openCreds());
	GUARD
	access().Tokend::ClientSession::insertRecord(recordType,
		attributes, attributesLength, data, record->tokenHandle());
	rRecord = record;
	DONE
}

void TokenDatabase::modifyRecord(CSSM_DB_RECORDTYPE recordType, Record *rRecord,
	const CssmDbRecordAttributeData *attributes, mach_msg_type_number_t attributesLength,
	const CssmData *data, CSSM_DB_MODIFY_MODE modifyMode)
{
	Access access(token());
	Record *record = safe_cast<Record *>(rRecord);
	access.add(*record);
	TRY
	validate(CSSM_ACL_AUTHORIZATION_DB_MODIFY, openCreds());
	record->validate(CSSM_ACL_AUTHORIZATION_DB_MODIFY, openCreds());
	GUARD
	access().Tokend::ClientSession::modifyRecord(recordType,
		record->tokenHandle(), attributes, attributesLength, data, modifyMode);
	DONE
}

void TokenDatabase::deleteRecord(Database::Record *rRecord)
{
	Access access(token(), *this);
	Record *record = safe_cast<Record *>(rRecord);
	access.add(*record);
	TRY
	validate(CSSM_ACL_AUTHORIZATION_DB_DELETE, openCreds());
	record->validate(CSSM_ACL_AUTHORIZATION_DB_DELETE, openCreds());
	GUARD
	access().Tokend::ClientSession::deleteRecord(record->tokenHandle());
	DONE
}


//
// Record/Search object handling
//
TokenDatabase::Search::~Search()
{
	if (mHandle)
		try {
			database().token().tokend().Tokend::ClientSession::releaseSearch(mHandle);
		} catch (...) {
			secdebug("tokendb", "%p release search handle %ld threw (ignored)",
				this, mHandle);
		}
}

TokenDatabase::Record::~Record()
{
	if (mHandle)
		try {
			database().token().tokend().Tokend::ClientSession::releaseRecord(mHandle);
		} catch (...) {
			secdebug("tokendb", "%p release record handle %ld threw (ignored)",
				this, mHandle);
		}
}


//
// TokenAcl personality of Record
//		
AclKind TokenDatabase::Record::aclKind() const
{
	return objectAcl;
}

Token &TokenDatabase::Record::token()
{
	return safer_cast<TokenDatabase &>(database()).token();
}

GenericHandle TokenDatabase::Record::tokenHandle() const
{
	return Handler::tokenHandle();
}


//
// Local utility classes
//
void TokenDatabase::InputKey::setup(Key *key)
{
	if (TokenKey *myKey = dynamic_cast<TokenKey *>(key)) {
		// one of ours
		mKeyHandle = myKey->tokenHandle();
		mKeyPtr = NULL;
	} else if (LocalKey *hisKey = dynamic_cast<LocalKey *>(key)) {
		// a local key - turn into raw form
		CssmClient::WrapKey wrap(Server::csp(), CSSM_ALGID_NONE);
		wrap(hisKey->cssmKey(), mKey);
		mKeyHandle = noKey;
		mKeyPtr = &mKey;
	} else {
		// no key at all
		mKeyHandle = noKey;
		mKeyPtr = NULL;
	}
}


TokenDatabase::InputKey::~InputKey()
{
	if (mKeyPtr) {
		//@@@ Server::csp().freeKey(mKey) ??
		Server::csp()->allocator().free(mKey.keyData());
	}
}
