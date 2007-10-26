/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 *  Token.h
 *  TokendMuscle
 */

#ifndef _TOKEND_TOKEN_H_
#define _TOKEND_TOKEN_H_

#include <SecurityTokend/SecTokend.h>
#include <security_utilities/osxcode.h>
#include <security_cdsa_utilities/context.h>
#include <security_cdsa_utilities/cssmpods.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_utilities/cssmcred.h>
#include <security_utilities/debugging.h>
#include <security_utilities/pcsc++.h>
#include <string>

#include "TokenContext.h"

namespace Tokend
{

class Cursor;
class Schema;
class TokenContext;

//
// "The" token
//
class Token : public SecTokendSupport
{
	NOCOPY(Token)
public:
	Token();
	virtual ~Token();

	bool cachedObject(CSSM_DB_RECORDTYPE relationId, const std::string &name,
		CssmData &data) const;
	void cacheObject(CSSM_DB_RECORDTYPE relationId, const std::string &name,
		const CssmData &object) const;

	virtual const SecTokendCallbacks *callbacks();
	virtual SecTokendSupport *support();

    virtual void initial();
    virtual uint32 probe(SecTokendProbeFlags flags,
		char tokenUid[TOKEND_MAX_UID]) = 0;
	virtual void establish(const CSSM_GUID *guid, uint32 subserviceId,
		SecTokendEstablishFlags flags, const char *cacheDirectory,
		const char *workDirectory, char mdsDirectory[PATH_MAX],
		char printName[PATH_MAX]);
	virtual void terminate(uint32 reason, uint32 options);

	virtual void authenticate(CSSM_DB_ACCESS_TYPE mode,
		const AccessCredentials *cred);
	virtual void getOwner(AclOwnerPrototype &owner) = 0;
	virtual void getAcl(const char *tag, uint32 &count,
		AclEntryInfo *&acls) = 0;

	virtual	Cursor *createCursor(const CSSM_QUERY *inQuery);

	virtual void changeOwner(const AclOwnerPrototype &owner);
	virtual void changeAcl(const AccessCredentials &cred, const AclEdit &edit);

	virtual void generateRandom(const Context &context, CssmData &result);
	virtual void getStatistics(CSSM_CSP_OPERATIONAL_STATISTICS &result);
	virtual void getTime(CSSM_ALGORITHMS algorithm, CssmData &result);
	virtual void getCounter(CssmData &result);
	virtual void selfVerify();

	virtual void changePIN(int pinNum,
		const unsigned char *oldPin, size_t oldPinLength,
		const unsigned char *newPin, size_t newPinLength);
	virtual uint32_t pinStatus(int pinNum);
	virtual void verifyPIN(int pinNum,
		const unsigned char *pin, size_t pinLength);
	virtual void unverifyPIN(int pinNum);

	virtual bool isLocked();

	TokenContext *tokenContext() { return mTokenContext; }

protected:
	std::string cachedObjectPath(CSSM_DB_RECORDTYPE relationId,
		const std::string &name) const;

	static CSSM_RETURN _initial();
    static CSSM_RETURN _probe(SecTokendProbeFlags flags, uint32 *score,
		char tokenUid[TOKEND_MAX_UID]);
	static CSSM_RETURN _establish(const CSSM_GUID *guid, uint32 subserviceId,
		SecTokendEstablishFlags flags, const char *cacheDirectory,
		const char *workDirectory, char mdsDirectory[PATH_MAX],
		char printName[PATH_MAX]);
	static CSSM_RETURN _terminate(uint32 reason, uint32 options);

	static CSSM_RETURN _findFirst(const CSSM_QUERY *query,
		TOKEND_RETURN_DATA *data, CSSM_HANDLE *hSearch);
	static CSSM_RETURN _findNext(CSSM_HANDLE hSearch,
		TOKEND_RETURN_DATA *data);
	static CSSM_RETURN _findRecordHandle(CSSM_HANDLE hRecord,
		TOKEND_RETURN_DATA *data);
	static CSSM_RETURN _insertRecord(CSSM_DB_RECORDTYPE recordType,
		const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes, const CSSM_DATA *data,
		CSSM_HANDLE *hRecord);
	static CSSM_RETURN _modifyRecord(CSSM_DB_RECORDTYPE recordType,
		CSSM_HANDLE *hRecord, const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
		const CSSM_DATA *data, CSSM_DB_MODIFY_MODE modifyMode);
	static CSSM_RETURN _deleteRecord(CSSM_HANDLE hRecord);
	static CSSM_RETURN _releaseSearch(CSSM_HANDLE hSearch);
	static CSSM_RETURN _releaseRecord(CSSM_HANDLE hRecord);
	
	static CSSM_RETURN _freeRetrievedData(TOKEND_RETURN_DATA *data);
	
	static CSSM_RETURN _releaseKey(CSSM_HANDLE hKey);
	static CSSM_RETURN _getKeySize(CSSM_HANDLE hKey, CSSM_KEY_SIZE *size);
	static CSSM_RETURN _getOutputSize(const CSSM_CONTEXT *context,
		CSSM_HANDLE hKey, uint32 inputSize, CSSM_BOOL encrypting,
		uint32 *outputSize);
	
	static CSSM_RETURN _generateSignature(const CSSM_CONTEXT *context,
		CSSM_HANDLE hKey, CSSM_ALGORITHMS signOnly, const CSSM_DATA *input,
		CSSM_DATA *signature);
	static CSSM_RETURN _verifySignature(const CSSM_CONTEXT *context,
		CSSM_HANDLE hKey, CSSM_ALGORITHMS signOnly, const CSSM_DATA *input,
		const CSSM_DATA *signature);
	static CSSM_RETURN _generateMac(const CSSM_CONTEXT *context,
		CSSM_HANDLE hKey, const CSSM_DATA *input, CSSM_DATA *mac);
	static CSSM_RETURN _verifyMac(const CSSM_CONTEXT *context,
		CSSM_HANDLE hKey, const CSSM_DATA *input, const CSSM_DATA *mac);
	static CSSM_RETURN _encrypt(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
		const CSSM_DATA *clear, CSSM_DATA *cipher);
	static CSSM_RETURN _decrypt(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
		const CSSM_DATA *cipher, CSSM_DATA *clear);
	static CSSM_RETURN _generateKey(const CSSM_CONTEXT *context,
		const CSSM_ACCESS_CREDENTIALS *creds,
		const CSSM_ACL_ENTRY_PROTOTYPE *owner, CSSM_KEYUSE usage,
		CSSM_KEYATTR_FLAGS attrs, CSSM_HANDLE *hKey, CSSM_KEY *header);
	static CSSM_RETURN _generateKeyPair(const CSSM_CONTEXT *context,
		const CSSM_ACCESS_CREDENTIALS *creds,
		const CSSM_ACL_ENTRY_PROTOTYPE *owner,
		CSSM_KEYUSE pubUsage, CSSM_KEYATTR_FLAGS pubAttrs,
		CSSM_KEYUSE privUsage, CSSM_KEYATTR_FLAGS privAttrs,
		CSSM_HANDLE *hPubKey, CSSM_KEY *pubHeader,
		CSSM_HANDLE *hPrivKey, CSSM_KEY *privHeader);
	static CSSM_RETURN _wrapKey(const CSSM_CONTEXT *context,
		CSSM_HANDLE hWrappingKey, const CSSM_KEY *wrappingKey,
		const CSSM_ACCESS_CREDENTIALS *cred, CSSM_HANDLE hSubjectKey,
		const CSSM_KEY *subjectKey, const CSSM_DATA *descriptiveData,
		CSSM_KEY *wrappedKey);
	static CSSM_RETURN _unwrapKey(const CSSM_CONTEXT *context,
		CSSM_HANDLE hWrappingKey, const CSSM_KEY *wrappingKey,
		const CSSM_ACCESS_CREDENTIALS *cred,
		const CSSM_ACL_ENTRY_PROTOTYPE *access,
		CSSM_HANDLE hPublicKey, const CSSM_KEY *publicKey,
		const CSSM_KEY *wrappedKey, CSSM_KEYUSE usage,
		CSSM_KEYATTR_FLAGS attributes, CSSM_DATA *descriptiveData,
		CSSM_HANDLE *hUnwrappedKey, CSSM_KEY *unwrappedKey);
	static CSSM_RETURN _deriveKey(const CSSM_CONTEXT *context,
		CSSM_HANDLE hSourceKey, const CSSM_KEY *sourceKey,
		const CSSM_ACCESS_CREDENTIALS *cred, 
		const CSSM_ACL_ENTRY_PROTOTYPE *access, CSSM_DATA *parameters,
		CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attributes,
		CSSM_HANDLE *hKey, CSSM_KEY *hKey);

	static CSSM_RETURN _getObjectOwner(CSSM_HANDLE hKey,
		CSSM_ACL_OWNER_PROTOTYPE *owner);
	static CSSM_RETURN _getObjectAcl(CSSM_HANDLE hKey,
		const char *tag, uint32 *count, CSSM_ACL_ENTRY_INFO **entries);
	static CSSM_RETURN _getDatabaseOwner(CSSM_ACL_OWNER_PROTOTYPE *owner);
	static CSSM_RETURN _getDatabaseAcl(const char *tag, uint32 *count,
		CSSM_ACL_ENTRY_INFO **entries);
	static CSSM_RETURN _getKeyOwner(CSSM_HANDLE hKey,
		CSSM_ACL_OWNER_PROTOTYPE *owner);
	static CSSM_RETURN _getKeyAcl(CSSM_HANDLE hKey, const char *tag,
		uint32 *count, CSSM_ACL_ENTRY_INFO **entries);
	
	static CSSM_RETURN _freeOwnerData(CSSM_ACL_OWNER_PROTOTYPE *owner);
	static CSSM_RETURN _freeAclData(uint32 count,
		CSSM_ACL_ENTRY_INFO *entries);

	static CSSM_RETURN _authenticateDatabase(CSSM_DB_ACCESS_TYPE mode,
		const CSSM_ACCESS_CREDENTIALS *cred);

	static CSSM_RETURN _changeDatabaseOwner(const CSSM_ACL_OWNER_PROTOTYPE *
		owner);
	static CSSM_RETURN _changeDatabaseAcl(const CSSM_ACCESS_CREDENTIALS *cred,
		const CSSM_ACL_EDIT *edit);
	static CSSM_RETURN _changeObjectOwner(CSSM_HANDLE hRecord,
		const CSSM_ACL_OWNER_PROTOTYPE *owner);
	static CSSM_RETURN _changeObjectAcl(CSSM_HANDLE hRecord,
		const CSSM_ACCESS_CREDENTIALS *cred, const CSSM_ACL_EDIT *edit);
	static CSSM_RETURN _changeKeyOwner(CSSM_HANDLE key,
		const CSSM_ACL_OWNER_PROTOTYPE *owner);
	static CSSM_RETURN _changeKeyAcl(CSSM_HANDLE key,
		const CSSM_ACCESS_CREDENTIALS *cred, const CSSM_ACL_EDIT *edit);

	static CSSM_RETURN _generateRandom(const CSSM_CONTEXT *context,
		CSSM_DATA *result);
	static CSSM_RETURN _getStatistics(CSSM_CSP_OPERATIONAL_STATISTICS *result);
	static CSSM_RETURN _getTime(CSSM_ALGORITHMS algorithm, CSSM_DATA *result);
	static CSSM_RETURN _getCounter(CSSM_DATA *result);
	static CSSM_RETURN _selfVerify();

	static CSSM_RETURN _cspPassThrough(uint32 id, const CSSM_CONTEXT *context,
		CSSM_HANDLE hKey, const CSSM_KEY *key, const CSSM_DATA *input,
		CSSM_DATA *output);
	static CSSM_RETURN _dlPassThrough(uint32 id, const CSSM_DATA *input,
		CSSM_DATA *output);

	static CSSM_RETURN _isLocked(uint32 *locked);

private:
	static const SecTokendCallbacks mCallbacks;

protected:
	Schema *mSchema;
	TokenContext *mTokenContext;

	Guid mGuid;
	uint32 mSubserviceId;
	std::string mCacheDirectory;
};


class ISO7816Token : public Token, public TokenContext, public PCSC::Card
{
	NOCOPY(ISO7816Token)
public:
	ISO7816Token();
	virtual ~ISO7816Token();

    virtual uint32 probe(SecTokendProbeFlags flags,
		char tokenUid[TOKEND_MAX_UID]);
	virtual void establish(const CSSM_GUID *guid, uint32 subserviceId,
		SecTokendEstablishFlags flags, const char *cacheDirectory,
		const char *workDirectory, char mdsDirectory[PATH_MAX],
		char printName[PATH_MAX]);

	uint16_t transmitAPDU(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
		size_t dataSize = 0, const uint8_t *data = NULL,
		size_t outputLength = 0, std::vector<uint8_t> *output = NULL);

protected:
	PCSC::Session mSession;
	char mPrintName[PATH_MAX];
	
	virtual void name(const char *printName);
};


} // end namespace Tokend

//
// Singleton
//
extern Tokend::Token *token;

#endif /* !_TOKEND_TOKEN_H_ */

