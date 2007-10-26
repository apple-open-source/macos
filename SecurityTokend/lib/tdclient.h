/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
// tdclient - Security tokend client interface library
//
// This interface is private to the Security system. It is not a public interface,
// and it may change at any time. You have been warned.
//
#ifndef _H_TDCLIENT
#define _H_TDCLIENT

#include <securityd_client/sscommon.h>
#include <SecurityTokend/SecTokend.h>
#include <security_cdsa_utilities/cssmkey.h>
#include <security_utilities/unix++.h>

#define __MigTypeCheck 1


namespace Security {
namespace Tokend {

using namespace SecurityServer;


//
// Securityd/tokend protocol version.
// Tokend must support this or abort immediately (i.e. don't even try to
// bring up the server loop if you don't).
//
#define TDPROTOVERSION	5


//
// A client connection (session)
//
class ClientSession : public ClientCommon {
public:
	ClientSession(Allocator &standard, Allocator &returning);
	virtual ~ClientSession();

	Port servicePort() const { return mServicePort; }

public:
	// tokend setup/teardown interface
	typedef uint32 Score;
	void probe(Score &score, std::string &tokenUid);
	void establish(Guid &guid, uint32 subserviceID,
		uint32 flags, const char *cacheDirectory, const char *workDirectory,
		char mdsDirectory[PATH_MAX], char printName[PATH_MAX]);
	void terminate(uint32 reason, uint32 options);

	// search/retrieve interface
	RecordHandle findFirst(const CssmQuery &query,
		CssmDbRecordAttributeData *inAttributes, size_t inAttributesLength,
		SearchHandle &hSearch, CssmData *outData, KeyHandle &hKey,
		CssmDbRecordAttributeData *&outAttributes, mach_msg_type_number_t &outAttributesLength);
	RecordHandle findNext(SearchHandle hSearch,
		CssmDbRecordAttributeData *inAttributes, size_t inAttributesLength,
		CssmData *outData, KeyHandle &hKey,
		CssmDbRecordAttributeData *&outAttributes, mach_msg_type_number_t &outAttributesLength);
	void findRecordHandle(RecordHandle record,
		CssmDbRecordAttributeData *inAttributes, size_t inAttributesLength,
		CssmData *inOutData, KeyHandle &hKey,
		CssmDbRecordAttributeData *&outAttributes, mach_msg_type_number_t &outAttributesLength);
	void insertRecord(CSSM_DB_RECORDTYPE recordType,
		const CssmDbRecordAttributeData *attributes, size_t attributesLength,
		const CssmData &data, RecordHandle &hRecord);
	void modifyRecord(CSSM_DB_RECORDTYPE recordType, RecordHandle &hRecord,
		const CssmDbRecordAttributeData *attributes, size_t attributesLength,
		const CssmData *data, CSSM_DB_MODIFY_MODE modifyMode);
	void deleteRecord(RecordHandle hRecord);
		
	void releaseSearch(SearchHandle hSeearch);
	void releaseRecord(RecordHandle hRecord);
	
public:
	// key objects
	void releaseKey(KeyHandle key);

	void queryKeySizeInBits(KeyHandle key, CssmKeySize &result);
    void getOutputSize(const Security::Context &context, KeyHandle key,
        uint32 inputSize, bool encrypt, uint32 &result);
	
    // encrypt/decrypt
	void encrypt(const Security::Context &context, KeyHandle key,
        const CssmData &in, CssmData &out);
	void decrypt(const Security::Context &context, KeyHandle key,
        const CssmData &in, CssmData &out);

    // signatures
	void generateSignature(const Security::Context &context, KeyHandle key,
        const CssmData &data, CssmData &signature,
        CSSM_ALGORITHMS signOnlyAlgorithm = CSSM_ALGID_NONE);
	void verifySignature(const Security::Context &context, KeyHandle key,
		const CssmData &data, const CssmData &signature,
        CSSM_ALGORITHMS verifyOnlyAlgorithm = CSSM_ALGID_NONE);
		
    // MACs
	void generateMac(const Security::Context &context, KeyHandle key,
		const CssmData &data, CssmData &mac);
	void verifyMac(const Security::Context &context, KeyHandle key,
		const CssmData &data, const CssmData &mac);

public:
    // key generation and derivation
	void generateKey(const Security::Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		uint32 keyUsage, uint32 keyAttr,
        KeyHandle &hKey, CssmKey *&key);
	void generateKey(const Security::Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		CSSM_KEYUSE pubKeyUsage, CSSM_KEYATTR_FLAGS pubKeyAttr,
		CSSM_KEYUSE privKeyUsage, CSSM_KEYATTR_FLAGS privKeyAttr,
		KeyHandle &hPublic, CssmKey *&publicKey,
		KeyHandle &hPrivate, CssmKey *&privateKey);
	//void generateAlgorithmParameters();	// not implemented
		
public:
    // key wrapping and unwrapping
	void wrapKey(const Security::Context &context, const AccessCredentials *cred,
		KeyHandle hWrappingKey, const CssmKey *wrappingKey,
		KeyHandle hSubjectKey, const CssmKey *subjectKey,
		const CssmData &descriptiveData, CssmWrappedKey *&wrappedKey);
	void unwrapKey(const Security::Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		KeyHandle hSourceKey, const CssmKey *sourceKey,
		KeyHandle hPublicKey, const CssmKey *publicKey,
		const CssmWrappedKey &wrappedKey, uint32 keyUsage, uint32 keyAttr,
		CssmData &data, KeyHandle &hUnwrappedKey, CssmKey *&unwrappedKey);

	// key derivation
	void deriveKey(DbHandle db, const Security::Context &context,
		KeyHandle hBaseKey, const CssmKey *baseKey,
        uint32 keyUsage, uint32 keyAttr, CssmData &param,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
        KeyHandle &hDerivedKey, CssmKey *&derivedKey);

	void generateRandom(CssmData &data);
	
public:
	// access control
	void authenticate(CSSM_DB_ACCESS_TYPE mode, const AccessCredentials *cred);
	
	void getAcl(AclKind kind, GenericHandle key, const char *tag,
		uint32 &count, AclEntryInfo * &info);
	void changeAcl(AclKind kind, GenericHandle key,
		const AccessCredentials &cred, const AclEdit &edit);
	void getOwner(AclKind kind, GenericHandle key, AclOwnerPrototype *&owner);
	void changeOwner(AclKind kind, GenericHandle key, const AccessCredentials &cred,
		const AclOwnerPrototype &edit);

	bool isLocked();

public:
	virtual void fault();		// called if tokend connection fails

protected:
	void servicePort(Port p);

private:
	void check(kern_return_t rc);

private:
	Port mServicePort;			// tokend's service port
	ReceivePort mReplyPort;		// the reply port we use
};


} // end namespace Tokend
} // end namespace Security


#endif //_H_TDCLIENT

