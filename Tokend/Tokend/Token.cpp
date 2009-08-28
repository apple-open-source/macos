/*
 *  Copyright (c) 2004,2007 Apple Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  Token.cpp
 *  TokendMuscle
 */

#include "Token.h"

#include "Cursor.h"
#include "KeyHandle.h"
#include "RecordHandle.h"
#include "Schema.h"
#include <memory>
#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_utilities/unix++.h>
#include <security_utilities/logging.h>

//
// SPI wrapper macros
//
#define BEGIN	try {
#define END(SS) \
		return CSSM_OK; \
	} catch (const CommonError &err) { \
		return CssmError::cssmError(err, CSSM_##SS##_BASE_ERROR); \
	} catch (...) { \
		return CSSM_ERRCODE_INTERNAL_ERROR; \
	}

//
// Singleton
//
Tokend::Token *token;

namespace Tokend
{

Token::Token() : mSchema(NULL), mTokenContext(NULL)
{
}

Token::~Token()
{
}


//
// Initial: Your first chance to do anything with the tokend framework
// initialized.
//
CSSM_RETURN Token::_initial()
{
	BEGIN
	token->initial();
	secdebug("tokend", "using reader %s",
		token->startupReaderInfo()->szReader);
	END(CSSM)
}


//
// Probe:
//  (1) See if we support this token. Return zero if not.
//      Return a score if we do - the lower, the better. 1 beats everyone else.
//  (2) Generate a unique id string for the token. This doesn't have to be
//      human readable. If you REALLY can't make one up, leave tokenUid alone.
//      But do try.
//
CSSM_RETURN Token::_probe(SecTokendProbeFlags flags, uint32 *score,
	char tokenUid[TOKEND_MAX_UID])
{
	BEGIN
	*score = token->probe(flags, tokenUid);
	secdebug("tokend", "flags=%d returning score=%d  uid='%s'",
		flags, *score, tokenUid);
	END(CSSM)
}


//
// Establish:
// Okay, you're the one. The token is yours. Here's your GUID and subservice ID
// (in case you care); it'll get automatically inserted into your MDS unless
// you override it. If you can make up a nice, user-friendly print name for
// your token, return it in printName. If you can't, leave it alone and
// securityd will make something up for you.
//
CSSM_RETURN Token::_establish(const CSSM_GUID *guid, uint32 subserviceID,
	SecTokendEstablishFlags flags, const char *cacheDirectory,
	const char *workDirectory, char mdsDirectory[PATH_MAX],
	char printName[PATH_MAX])
{
	BEGIN
	secdebug("tokend", "establish(%s,%d,0x%X)",
		Guid::required(guid).toString().c_str(), subserviceID, flags);

	token->establish(guid, subserviceID, flags, cacheDirectory, workDirectory,
		mdsDirectory, printName);
	// if printName is left alone, securityd will make one up
	// if mdsDirectory is left alone, all MDS resources in the Resource bundle
	// will be loaded
	END(CSSM)
}


//
// Terminate() is called by security when it wants you to go away.
// This function does not (currently) return anything, so the CSSM_RETURN is
// effectively ignored. (It's still here for consistency's sake.)
//
CSSM_RETURN Token::_terminate(uint32 reason, uint32 options)
{
	BEGIN
	secdebug("tokend", "terminate(%d,0x%d)", reason, options);
	token->terminate(reason, options);
	END(CSSM)
}


CSSM_RETURN Token::_findFirst(const CSSM_QUERY *query,
	TOKEND_RETURN_DATA *data, CSSM_HANDLE *hSearch)
{
	BEGIN
	secdebug("tokend", "findFirst()");
	std::auto_ptr<Cursor> curs(token->createCursor(query));
	TokenContext *tokenContext = token->tokenContext();
	std::auto_ptr<RecordHandle> rh(curs->next(tokenContext));
	if (!rh.get())
	{
		secdebug("tokend", "findFirst() returning: CSSMERR_DL_ENDOFDATA");
#if 1
		data->record = 0;
		data->keyhandle = 0;
		return 0;
#else
		return CSSMERR_DL_ENDOFDATA;
#endif
	}

	rh->get(tokenContext, *data);
	// Release the RecordHandle until the caller kills the handle we returned.
	rh.release();

	// We didn't throw so return a search handle and keep the Cursor around.
	*hSearch = curs->handle();
	curs.release();
	secdebug("tokend", "end findFirst() returned: %ld", *hSearch);
	END(DL)
}

CSSM_RETURN Token::_findNext(CSSM_HANDLE hSearch, TOKEND_RETURN_DATA *data)
{
	BEGIN
	secdebug("tokend", "findNext(%ld)", hSearch);
	Cursor& curs = Security::HandleObject::find<Cursor>(hSearch,
		CSSMERR_DL_RECORD_NOT_FOUND);
	TokenContext *tokenContext = token->tokenContext();
	std::auto_ptr<RecordHandle> rh(curs.next(tokenContext));
	if (!rh.get())
	{
		secdebug("tokend", "findNext(%ld) returning: CSSMERR_DL_ENDOFDATA",
			hSearch);
#if 1
		data->record = 0;
		data->keyhandle = 0;
		return 0;
#else
		return CSSMERR_DL_ENDOFDATA;
#endif
	}

	rh->get(tokenContext, *data);
	rh.release();
	END(DL)
}

CSSM_RETURN Token::_findRecordHandle(CSSM_HANDLE hRecord,
	TOKEND_RETURN_DATA *data)
{
	BEGIN
	secdebug("tokend", "findRecordHandle(%ld)", hRecord);
	RecordHandle &rh = Security::HandleObject::find<RecordHandle>(hRecord,
		CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
	rh.get(token->tokenContext(), *data);
	END(DL)
}

CSSM_RETURN Token::_insertRecord(CSSM_DB_RECORDTYPE recordType,
	const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes, const CSSM_DATA *data,
	CSSM_HANDLE *hRecord)
{
	BEGIN
	secdebug("tokend", "insertRecord");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END(DL)
}

CSSM_RETURN Token::_modifyRecord(CSSM_DB_RECORDTYPE recordType,
	CSSM_HANDLE *hRecord, const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
	const CSSM_DATA *data, CSSM_DB_MODIFY_MODE modifyMode)
{
	BEGIN
	secdebug("tokend", "modifyRecord");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END(DL)
}

CSSM_RETURN Token::_deleteRecord(CSSM_HANDLE hRecord)
{
	BEGIN
	secdebug("tokend", "deleteRecord");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END(DL)
}

CSSM_RETURN Token::_releaseSearch(CSSM_HANDLE hSearch)
{
	BEGIN
	secdebug("tokend", "releaseSearch(%ld)", hSearch);
	Cursor &curs = Security::HandleObject::findAndKill<Cursor>(hSearch,
		CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
	delete &curs;
	END(DL)
}

CSSM_RETURN Token::_releaseRecord(CSSM_HANDLE hRecord)
{
	BEGIN
	secdebug("tokend", "releaseRecord(%ld)", hRecord);
	RecordHandle &rech = Security::HandleObject::findAndKill<RecordHandle>(hRecord,
		CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
	delete &rech;
	END(DL)
}

CSSM_RETURN Token::_freeRetrievedData(TOKEND_RETURN_DATA *data)
{
	BEGIN
	secdebug("tokend", "freeRetrievedData");
	// Since we return pointers to our cached interal data this is also a noop
	END(DL)
}

CSSM_RETURN Token::_releaseKey(CSSM_HANDLE hKey)
{
	BEGIN
	secdebug("tokend", "releaseKey(%ld)", hKey);
	KeyHandle &keyh = Security::HandleObject::findAndKill<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
	delete &keyh;
	END(CSP)
}

CSSM_RETURN Token::_getKeySize(CSSM_HANDLE hKey, CSSM_KEY_SIZE *size)
{
	BEGIN
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
    key.getKeySize(Required(size));
	END(CSP)
}

CSSM_RETURN Token::_getOutputSize(const CSSM_CONTEXT *context,
	CSSM_HANDLE hKey, uint32 inputSize, CSSM_BOOL encrypting,
	uint32 *outputSize)
{
	BEGIN
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
    Required(outputSize) = key.getOutputSize(Context::required(context),
		inputSize, encrypting);
	END(CSP)
}
	
CSSM_RETURN Token::_generateSignature(const CSSM_CONTEXT *context,
	CSSM_HANDLE hKey, CSSM_ALGORITHMS signOnly, const CSSM_DATA *input,
	CSSM_DATA *signature)
{
	BEGIN
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
    key.generateSignature(Context::required(context), signOnly,
		CssmData::required(input), CssmData::required(signature));
	END(CSP)
}


CSSM_RETURN Token::_verifySignature(const CSSM_CONTEXT *context,
	CSSM_HANDLE hKey, CSSM_ALGORITHMS signOnly, const CSSM_DATA *input,
	const CSSM_DATA *signature)
{
	BEGIN
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
    key.verifySignature(Context::required(context), signOnly,
		CssmData::required(input), CssmData::required(signature));
	END(CSP)
}


CSSM_RETURN Token::_generateMac(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
	const CSSM_DATA *input, CSSM_DATA *output)
{
	BEGIN
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
    key.generateMac(Context::required(context), CssmData::required(input),
		CssmData::required(output));
	END(CSP)
}


CSSM_RETURN Token::_verifyMac(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
	const CSSM_DATA *input, const CSSM_DATA *compare)
{
	BEGIN
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
    key.verifyMac(Context::required(context), CssmData::required(input),
		CssmData::required(compare));
	END(CSP)
}


CSSM_RETURN Token::_encrypt(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
	const CSSM_DATA *clear, CSSM_DATA *cipher)
{
	BEGIN
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
    key.encrypt(Context::required(context), CssmData::required(clear),
		CssmData::required(cipher));
	END(CSP)
}


CSSM_RETURN Token::_decrypt(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
	const CSSM_DATA *cipher, CSSM_DATA *clear)
{
	BEGIN
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
    key.decrypt(Context::required(context), CssmData::required(cipher),
		CssmData::required(clear));
	END(CSP)
}

CSSM_RETURN Token::_generateKey(const CSSM_CONTEXT *context,
	const CSSM_ACCESS_CREDENTIALS *creds,
	const CSSM_ACL_ENTRY_PROTOTYPE *owner, CSSM_KEYUSE usage,
	CSSM_KEYATTR_FLAGS attrs, CSSM_HANDLE *hKey, CSSM_KEY *header)
{
	BEGIN
	secdebug("tokend", "generateKey");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END(CSP)
}

CSSM_RETURN Token::_generateKeyPair(const CSSM_CONTEXT *context,
	const CSSM_ACCESS_CREDENTIALS *creds,
	const CSSM_ACL_ENTRY_PROTOTYPE *owner,
	CSSM_KEYUSE pubUsage, CSSM_KEYATTR_FLAGS pubAttrs,
	CSSM_KEYUSE privUsage, CSSM_KEYATTR_FLAGS privAttrs,
	CSSM_HANDLE *hPubKey, CSSM_KEY *pubHeader,
	CSSM_HANDLE *hPrivKey, CSSM_KEY *privHeader)
{
	BEGIN
	secdebug("tokend", "generateKeyPair");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END(CSP)
}

CSSM_RETURN Token::_wrapKey(const CSSM_CONTEXT *context,
	CSSM_HANDLE hWrappingKey, const CSSM_KEY *wrappingKey,
	const CSSM_ACCESS_CREDENTIALS *cred,
	CSSM_HANDLE hSubjectKey, const CSSM_KEY *subjectKey,
	const CSSM_DATA *descriptiveData, CSSM_KEY *wrappedKey)
{
	BEGIN
	KeyHandle *subjectKeyHandle = hSubjectKey
		? &Security::HandleObject::find<KeyHandle>(hSubjectKey,
			CSSMERR_CSP_INVALID_KEY_REFERENCE) : NULL;
	KeyHandle *wrappingKeyHandle = hWrappingKey
		? &Security::HandleObject::find<KeyHandle>(hWrappingKey,
			CSSMERR_CSP_INVALID_KEY_REFERENCE) : NULL;

	if (subjectKeyHandle)
	{
		subjectKeyHandle->wrapUsingKey(Context::required(context),
			AccessCredentials::optional(cred),
			wrappingKeyHandle, CssmKey::optional(wrappingKey),
			CssmData::optional(descriptiveData),
				CssmKey::required(wrappedKey));
	}
	else if (wrappingKeyHandle)
	{
		wrappingKeyHandle->wrapKey(Context::required(context),
			CssmKey::required(subjectKey),
			CssmData::optional(descriptiveData),
				CssmKey::required(wrappedKey));
	}
	else
	{
		secdebug("tokend",
			"wrapKey without a reference subject or wrapping key not supported"
			);
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	}
	END(CSP)
}

CSSM_RETURN Token::_unwrapKey(const CSSM_CONTEXT *context,
	CSSM_HANDLE hWrappingKey, const CSSM_KEY *wrappingKey,
	const CSSM_ACCESS_CREDENTIALS *cred,
	const CSSM_ACL_ENTRY_PROTOTYPE *access,
	CSSM_HANDLE hPublicKey, const CSSM_KEY *publicKey,
	const CSSM_KEY *wrappedKey, CSSM_KEYUSE usage,
	CSSM_KEYATTR_FLAGS attributes, CSSM_DATA *descriptiveData,
	CSSM_HANDLE *hUnwrappedKey, CSSM_KEY *unwrappedKey)
{
	BEGIN
	if (hWrappingKey)
	{
		KeyHandle &unwrappingKey =
			Security::HandleObject::find<KeyHandle>(hWrappingKey,
				CSSMERR_CSP_INVALID_KEY_REFERENCE);
		if (hPublicKey)
		{
			secdebug("tokend", "unwrapKey with a public key not supported");
			CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
		}

		unwrappingKey.unwrapKey(Context::required(context),
			AccessCredentials::optional(cred),
			AclEntryPrototype::optional(access), CssmKey::required(wrappedKey),
			usage, attributes, CssmData::optional(descriptiveData),
			*hUnwrappedKey, CssmKey::required(unwrappedKey));
	}
	else
	{
		secdebug("tokend",
			"unwrapKey without a wrapping key not supported (import)");
		/* There is no key doing the unwrap so this is basically an import. */
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	}
	END(CSP)
}

CSSM_RETURN Token::_deriveKey(const CSSM_CONTEXT *context,
	CSSM_HANDLE hSourceKey, const CSSM_KEY *sourceKey,
	const CSSM_ACCESS_CREDENTIALS *cred,
	const CSSM_ACL_ENTRY_PROTOTYPE *access, CSSM_DATA *parameters,
	CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attributes,
	CSSM_HANDLE *hKey, CSSM_KEY *key)
{
	BEGIN
	secdebug("tokend", "deriveKey");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END(CSP)
}

CSSM_RETURN Token::_getObjectOwner(CSSM_HANDLE hRecord,
	CSSM_ACL_OWNER_PROTOTYPE *owner)
{
	BEGIN
	secdebug("tokend", "getObjectOwner");
	RecordHandle &rh = Security::HandleObject::find<RecordHandle>(hRecord,
		CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
	rh.getOwner(AclOwnerPrototype::required(owner));
	END(DL)
}

CSSM_RETURN Token::_getObjectAcl(CSSM_HANDLE hRecord,
	const char *tag, uint32 *count, CSSM_ACL_ENTRY_INFO **entries)
{
	BEGIN
	secdebug("tokend", "getObjectAcl");
	RecordHandle &rh = Security::HandleObject::find<RecordHandle>(hRecord,
		CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
	rh.getAcl(tag, Required(count), AclEntryInfo::overlayVar(*entries));
	END(DL)
}

CSSM_RETURN Token::_getDatabaseOwner(CSSM_ACL_OWNER_PROTOTYPE *owner)
{
	BEGIN
	token->getOwner(AclOwnerPrototype::required(owner));
	END(DL)
}

CSSM_RETURN Token::_getDatabaseAcl(const char *tag, uint32 *count,
	CSSM_ACL_ENTRY_INFO **entries)
{
	BEGIN
	token->getAcl(tag, *count, AclEntryInfo::overlayVar(*entries));
	END(DL)
}

CSSM_RETURN Token::_getKeyOwner(CSSM_HANDLE hKey,
	CSSM_ACL_OWNER_PROTOTYPE *owner)
{
	BEGIN
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
	key.getOwner(AclOwnerPrototype::required(owner));
	END(CSP)
}

CSSM_RETURN Token::_getKeyAcl(CSSM_HANDLE hKey,
	const char *tag, uint32 *count, CSSM_ACL_ENTRY_INFO **entries)
{
	BEGIN
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
	key.getAcl(tag, Required(count), AclEntryInfo::overlayVar(*entries));
	END(CSP)
}

CSSM_RETURN Token::_freeOwnerData(CSSM_ACL_OWNER_PROTOTYPE *owner)
{
	BEGIN
    // @@@ Do something here based on how we return data above.
	END(CSP)
}

CSSM_RETURN Token::_freeAclData(uint32 count, CSSM_ACL_ENTRY_INFO *entries)
{
	BEGIN
#if 0
    AutoAclEntryInfoList aclList(&Allocator::standard());
    // Invoke braindead overloaded operators since there are no setters on
	// AutoAclEntryInfoList
    *static_cast<uint32 *>(aclList) = count;
    *static_cast<CSSM_ACL_ENTRY_INFO_PTR *>(aclList) = entries;
#endif
	END(CSP)
}

CSSM_RETURN Token::_authenticateDatabase(CSSM_DB_ACCESS_TYPE mode,
	const CSSM_ACCESS_CREDENTIALS *cred)
{
	BEGIN
	secdebug("tokend", "authenticateDatabase");
	token->authenticate(mode, AccessCredentials::overlay(cred));
	END(DL)
}

CSSM_RETURN Token::_changeDatabaseOwner(const CSSM_ACL_OWNER_PROTOTYPE *owner)
{
	BEGIN
	secdebug("tokend", "changeDatabaseOwner");
	token->changeOwner(AclOwnerPrototype::required(owner));
	END(DL)
}

CSSM_RETURN Token::_changeDatabaseAcl(const CSSM_ACCESS_CREDENTIALS *cred,
	const CSSM_ACL_EDIT *edit)
{
	BEGIN
	secdebug("tokend", "changeDatabaseAcl");
	token->changeAcl(AccessCredentials::required(cred),
		AclEdit::required(edit));
	END(DL)
}

CSSM_RETURN Token::_changeObjectOwner(CSSM_HANDLE hRecord,
	const CSSM_ACL_OWNER_PROTOTYPE *owner)
{
	BEGIN
	secdebug("tokend", "changeObjectOwner");
	RecordHandle &rh = Security::HandleObject::find<RecordHandle>(hRecord,
		CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
	rh.changeOwner(AclOwnerPrototype::required(owner));
	END(DL)
}

CSSM_RETURN Token::_changeObjectAcl(CSSM_HANDLE hRecord,
	const CSSM_ACCESS_CREDENTIALS *cred, const CSSM_ACL_EDIT *edit)
{
	BEGIN
	secdebug("tokend", "changeObjectAcl");
	RecordHandle &rh = Security::HandleObject::find<RecordHandle>(hRecord,
		CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
	rh.changeAcl(AccessCredentials::required(cred), AclEdit::required(edit));
	END(DL)
}

CSSM_RETURN Token::_changeKeyOwner(CSSM_HANDLE hKey,
	const CSSM_ACL_OWNER_PROTOTYPE *owner)
{
	BEGIN
	secdebug("tokend", "changeKeyOwner");
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
	key.changeOwner(AclOwnerPrototype::required(owner));
	END(CSP)
}

CSSM_RETURN Token::_changeKeyAcl(CSSM_HANDLE hKey,
	const CSSM_ACCESS_CREDENTIALS *cred, const CSSM_ACL_EDIT *edit)
{
	BEGIN
	secdebug("tokend", "changeKeyAcl");
	KeyHandle &key = Security::HandleObject::find<KeyHandle>(hKey,
		CSSMERR_CSP_INVALID_KEY_REFERENCE);
	key.changeAcl(AccessCredentials::required(cred), AclEdit::required(edit));
	END(CSP)
}

CSSM_RETURN Token::_generateRandom(const CSSM_CONTEXT *context,
	CSSM_DATA *result)
{
	BEGIN
	secdebug("tokend", "generateRandom");
	token->generateRandom(Context::required(context),
		CssmData::required(result));
	END(CSP)
}

CSSM_RETURN Token::_getStatistics(CSSM_CSP_OPERATIONAL_STATISTICS *result)
{
	BEGIN
	secdebug("tokend", "getStatistics");
	token->getStatistics(Required(result));
	END(CSP)
}

CSSM_RETURN Token::_getTime(CSSM_ALGORITHMS algorithm, CSSM_DATA *result)
{
	BEGIN
	secdebug("tokend", "getTime");
	token->getTime(algorithm, CssmData::required(result));
	END(CSP)
}

CSSM_RETURN Token::_getCounter(CSSM_DATA *result)
{
	BEGIN
	secdebug("tokend", "getCounter");
	token->getCounter(CssmData::required(result));
	END(CSP)
}

CSSM_RETURN Token::_selfVerify()
{
	BEGIN
	secdebug("tokend", "selfVerify");
	token->selfVerify();
	END(CSP)
}

CSSM_RETURN Token::_cspPassThrough(uint32 id, const CSSM_CONTEXT *context,
	CSSM_HANDLE hKey, const CSSM_KEY *key, const CSSM_DATA *input,
	CSSM_DATA *output)
{
	BEGIN
	secdebug("tokend", "cspPassThrough");
	CssmError::throwMe(CSSM_ERRCODE_INVALID_PASSTHROUGH_ID);
	END(CSP)
}

CSSM_RETURN Token::_dlPassThrough(uint32 id, const CSSM_DATA *input,
	CSSM_DATA *output)
{
	BEGIN
	secdebug("tokend", "dlPassThrough");
	CssmError::throwMe(CSSM_ERRCODE_INVALID_PASSTHROUGH_ID);
	END(DL)
}

CSSM_RETURN Token::_isLocked(uint32 *locked)
{
	BEGIN
	secdebug("tokend", "_isLocked");
	Required(locked) = token->isLocked();
	secdebug("tokend", "_isLocked: %d", *locked);
	END(DL)
}

//
// Callback vector into SecTokendMain
//
const SecTokendCallbacks Token::mCallbacks = {
	kSecTokendCallbackVersion,
	kSecTokendCallbacksDefault,
	_initial, _probe, _establish, _terminate,
	_findFirst, _findNext, _findRecordHandle,
	_insertRecord, _modifyRecord, _deleteRecord,
	_releaseSearch, _releaseRecord,
	_freeRetrievedData,
	_releaseKey, _getKeySize, _getOutputSize,
	_generateSignature, _verifySignature,
	_generateMac, _verifyMac,
	_encrypt, _decrypt,
	_generateKey, _generateKeyPair,
	_wrapKey, _unwrapKey, _deriveKey,
	_getDatabaseOwner, _getDatabaseAcl,
	_getObjectOwner, _getObjectAcl,
	_getKeyOwner, _getKeyAcl,
	_freeOwnerData, _freeAclData,
	_authenticateDatabase,
	_changeDatabaseOwner, _changeDatabaseAcl,
	_changeObjectOwner, _changeObjectAcl,
	_changeKeyOwner, _changeKeyAcl,
	_generateRandom, _getStatistics,
	_getTime, _getCounter,
	_selfVerify,
	_cspPassThrough, _dlPassThrough,
	_isLocked
};

const SecTokendCallbacks *Token::callbacks()
{
	return &mCallbacks;
}

SecTokendSupport *Token::support()
{
	return this;
}

void Token::initial()
{
}

void Token::terminate(uint32 reason, uint32 options)
{
}

void Token::establish(const CSSM_GUID *guid, uint32 subserviceId,
	SecTokendEstablishFlags flags, const char *cacheDirectory,
	const char *workDirectory, char mdsDirectory[PATH_MAX],
	char printName[PATH_MAX])
{
	secdebug("establish", "cacheDirectory %s", cacheDirectory);
	mGuid = *guid;
	mSubserviceId = subserviceId;
	mCacheDirectory = cacheDirectory;
}


bool Token::cachedObject(CSSM_DB_RECORDTYPE relationId,
	const std::string &name, CssmData &object) const
{
	try
	{
		UnixPlusPlus::AutoFileDesc fd(cachedObjectPath(relationId, name));
		object.Length = fd.fileSize();
		object.Data = reinterpret_cast<uint8 *>(malloc(object.Length));
		object.Length = fd.readAll(object.Data, object.Length);
	}
	catch (const UnixError &error)
	{
		return false;
 	}

	return true;
}

void Token::cacheObject(CSSM_DB_RECORDTYPE relationId, const std::string &name,
	const CssmData &object) const
{
	std::string path(cachedObjectPath(relationId, name));
	try
	{
		UnixPlusPlus::AutoFileDesc fd(path, O_WRONLY|O_CREAT|O_TRUNC);
		fd.writeAll(object.Data, object.Length);
	}
	catch (const UnixError &e)
	{
		Syslog::error("error writing cache file: %s: %s\n", path.c_str(),
			strerror(e.unixError()));
		::unlink(path.c_str());
	}
}

std::string Token::cachedObjectPath(CSSM_DB_RECORDTYPE relationId,
	const std::string &name) const
{
	char buffer[9];
	sprintf(buffer, "%X", relationId);

	return mCacheDirectory + "/" + buffer + "-" + name;
}

Cursor *Token::createCursor(const CSSM_QUERY *inQuery)
{
	if (!inQuery || inQuery->RecordType == CSSM_DL_DB_RECORD_ANY
		|| inQuery->RecordType == CSSM_DL_DB_RECORD_ALL_KEYS)
	{
		return new MultiCursor(inQuery, *mSchema);
	}

	const Relation &relation = mSchema->findRelation(inQuery->RecordType);
	return new LinearCursor(inQuery, relation);
}

//
// Authenticate to the token
//
void Token::authenticate(CSSM_DB_ACCESS_TYPE mode,
	const AccessCredentials *cred)
{
	int pinNum = pinFromAclTag(cred->EntryTag);
	if (!pinNum || !cred)
		pinNum = -1; // No PIN in tag.

	if (mode == CSSM_DB_ACCESS_RESET)
	{
		// A mode of CSSM_DB_ACCESS_RESET is a request to deauthenticate
		// the card completely.
		secdebug("authenticate", "unverifying PIN%d", pinNum);
		return unverifyPIN(pinNum);
	}
	else if (cred && pinNum > 0)
	{ // tag="PINk"; unlock a PIN
		if (cred->size() != 1) // just one, please
			CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
		const TypedList &sample = (*cred)[0];
		switch (sample.type())
		{
		case CSSM_SAMPLE_TYPE_PASSWORD:
		case CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD:
		case CSSM_SAMPLE_TYPE_PROTECTED_PASSWORD:
		{
			if (sample.length() != 2)	// not recognized, may have non-existing data
				return;
			CssmData &pin = sample[1].data();
			return verifyPIN(pinNum, pin.Data, pin.Length);
		}
		default:
			break;
		}

		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
	}
	else
		secdebug("authenticate", "ignoring non-PIN authentication request");
}

void Token::changeOwner(const AclOwnerPrototype &owner)
{
	// Default changeOwner on a token always fails.
	CssmError::throwMe(CSSM_ERRCODE_OBJECT_MANIP_AUTH_DENIED);
}

void Token::changeAcl(const AccessCredentials &cred, const AclEdit &edit)
{
	// We don't allow adding or deleting of acls currently
	switch (edit.mode())
	{
	case CSSM_ACL_EDIT_MODE_DELETE:
		CssmError::throwMe(CSSM_ERRCODE_ACL_DELETE_FAILED);
	case CSSM_ACL_EDIT_MODE_REPLACE:
		break;
	case CSSM_ACL_EDIT_MODE_ADD:
		CssmError::throwMe(CSSM_ERRCODE_ACL_ADD_FAILED);
	default:
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_EDIT_MODE);
	}

#if 0
	// edit.handle() is the offset in mAclEntries of the acl we are replacing
	uint32 ix = edit.handle();
	if (ix >= mAclEntries.size())
		CssmError::throwMe(CSSM_ERRCODE_ACL_REPLACE_FAILED);

	// Now we have the actual AclEntryPrototype being changed
	const AclEntryPrototype &oldProto = mAclEntries.at(ix).proto();
#endif

	// Now get the new AclEntryPrototype for this entry.
	const AclEntryInput *newEntry = edit.newEntry();
	if (!newEntry)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_INPUT_POINTER);
	const AclEntryPrototype &newProto = newEntry->proto();

	unsigned int pinNum = pinFromAclTag(newProto.EntryTag);
	if (!pinNum)
		CssmError::throwMe(CSSM_ERRCODE_OBJECT_ACL_NOT_SUPPORTED);

	const TypedList &subject = newProto.subject();
	switch (subject.type()) 
	{
	case CSSM_ACL_SUBJECT_TYPE_PASSWORD:
	case CSSM_ACL_SUBJECT_TYPE_PROMPTED_PASSWORD:
	case CSSM_ACL_SUBJECT_TYPE_PROTECTED_PASSWORD:
		break;
	default:
		CssmError::throwMe(CSSM_ERRCODE_ACL_SUBJECT_TYPE_NOT_SUPPORTED);
	}
	const CssmData &newPin = subject[1].data();

	if (cred.size() != 1)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);

	const TypedList &value = cred[0].value();
	switch (value.type())
	{
	case CSSM_SAMPLE_TYPE_PASSWORD:
	case CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD:
	case CSSM_SAMPLE_TYPE_PROTECTED_PASSWORD:
		break;
	default:
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
	}
	const CssmData &oldPin = value[1].data();

	secdebug("tokend", "CHANGE PIN%d from \"%.*s\" to \"%.*s\"",
		pinNum, static_cast<int>(oldPin.Length), oldPin.Data,
		static_cast<int>(newPin.Length), newPin.Data);

	changePIN(pinNum, oldPin.Data, oldPin.Length, newPin.Data, newPin.Length);
}

void Token::generateRandom(const Context &context, CssmData &result)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Token::getStatistics(CSSM_CSP_OPERATIONAL_STATISTICS &result)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Token::getTime(CSSM_ALGORITHMS algorithm, CssmData &result)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Token::getCounter(CssmData &result)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Token::selfVerify()
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Token::changePIN(int pinNum,
	const unsigned char *oldPin, size_t oldPinLength,
	const unsigned char *newPin, size_t newPinLength)
{
	// Default changePIN on a token always fails.
	CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
}

uint32_t Token::pinStatus(int pinNum)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Token::verifyPIN(int pinNum,
	const unsigned char *pin, size_t pinLength)
{
	CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
}

void Token::unverifyPIN(int pinNum)
{
}

bool Token::isLocked()
{
	// Check pin1 by default.  Subclasses may override.
	return pinStatus(1) != 0x9000;
}


//
// ISO7816Token
//
ISO7816Token::ISO7816Token()
{
	mPrintName[0]=0;
}

ISO7816Token::~ISO7816Token()
{
}

uint32 ISO7816Token::probe(SecTokendProbeFlags flags,
	char tokenUid[TOKEND_MAX_UID])
{
	const SCARD_READERSTATE &readerState = *(*startupReaderInfo)();
	connect(mSession, readerState.szReader);
	return 0;
}

void ISO7816Token::establish(const CSSM_GUID *guid, uint32 subserviceId,
	SecTokendEstablishFlags flags, const char *cacheDirectory,
	const char *workDirectory, char mdsDirectory[PATH_MAX],
	char printName[PATH_MAX])
{
	secdebug("establish", "cacheDirectory %s, workDirectory: %s, name: %s",
		cacheDirectory, workDirectory, mPrintName);
	if (mPrintName[0])
		::strlcpy(printName, mPrintName, PATH_MAX);
	Token::establish(guid, subserviceId, flags, cacheDirectory,
		workDirectory, mdsDirectory, printName);

	if (!isConnected())
	{
		const SCARD_READERSTATE &readerState = *(*startupReaderInfo)();
		connect(mSession, readerState.szReader);
	}
}

uint16_t ISO7816Token::transmitAPDU(uint8_t cla, uint8_t ins, uint8_t p1,
	uint8_t p2, size_t dataSize, const uint8_t *data,
	size_t outputLength, std::vector<uint8_t> *output)
{
	std::vector<uint8_t> apdu;
	uint32_t lc = data ? dataSize : 0;

	// Worst case we need this much
	apdu.reserve(10 + lc);

	apdu.push_back(cla);
	apdu.push_back(ins);
	apdu.push_back(p1);
	apdu.push_back(p2);

	if (lc > 0)
	{
		if (lc < 0x100)
		{
			// Normal length Lc
			apdu.push_back(lc);
		}
		else if (lc < 0x10000)
		{
			// Extended length Lc
			apdu.push_back(0);
			apdu.push_back(lc >> 8);
			apdu.push_back(lc);
		}
		else
		{
			// Lc too big.
            PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
		}
		apdu.insert(apdu.end(), data, data + dataSize);
	}

	if (output && outputLength > 0)
	{
		if (outputLength < 0x100)
		{
			// Normal length Le
			apdu.push_back(outputLength);
		}
		else if (outputLength < 0x10000)
		{
			// Extended length Le
			apdu.push_back(0);
			apdu.push_back(outputLength >> 8);
			apdu.push_back(outputLength);
		}
		else
		{
			// Le too big
            PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
		}

		// Append the response to what's already in output.
		size_t oldSize = output->size();
		// Make enough room for the data we are requesting plus the sw
		output->resize(oldSize + outputLength + 2);
		uint8_t *response = &output->at(oldSize);
		size_t responseLength = outputLength + 2;
		transmit(&apdu[0], apdu.size(), response, responseLength);
		if (responseLength < 2)
		{
			output->resize(oldSize + responseLength);
			PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
		}

		uint16_t sw = (response[responseLength - 2] << 8)
			+ response[responseLength - 1];
		// Remove the sw from the output.
		output->resize(oldSize + responseLength - 2);

		return sw;
	}
	else
	{
		uint8_t response[2];
		size_t responseLength = sizeof(response);
		transmit(&apdu[0], apdu.size(), response, responseLength);
		if (responseLength < 2)
			PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);

		return (response[responseLength - 2] << 8)
			+ response[responseLength - 1];
	}
}

void ISO7816Token::name(const char *printName)
{
	// Set the printName
	::strlcpy(mPrintName,printName,min(1+strlen(printName),size_t(PATH_MAX)));
}

} // end namespace Tokend

