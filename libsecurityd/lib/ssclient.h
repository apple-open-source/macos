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
// ssclient - SecurityServer client interface library
//
// This interface is private to the Security system. It is not a public interface,
// and it may change at any time. You have been warned.
//
#ifndef _H_SSCLIENT
#define _H_SSCLIENT

#include "sscommon.h"
#include <Security/Authorization.h>
#include <Security/AuthSession.h>
#include <Security/SecCodeHost.h>

#ifdef __cplusplus

#include <security_utilities/osxcode.h>
#include <security_utilities/unix++.h>
#include <security_utilities/globalizer.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include "ssnotify.h"


namespace Security {
namespace SecurityServer {

#endif //__cplusplus


//
// Unique-identifier blobs for key objects
//
typedef struct KeyUID {
    uint8 signature[20];
} KeyUID;


//
// Maximum length of hash (digest) arguments (bytes)
//
#define maxUcspHashLength 64


//
// Authorization blobs
//
typedef struct AuthorizationBlob {
    uint32 data[2];
	
#ifdef __cplusplus
	bool operator < (const AuthorizationBlob &other) const
	{ return memcmp(data, other.data, sizeof(data)) < 0; }
	
	bool operator == (const AuthorizationBlob &other) const
	{ return memcmp(data, other.data, sizeof(data)) == 0; }
    
    size_t hash() const {	//@@@ revisit this hash
        return data[0] ^ data[1] << 3;
    }
#endif
} AuthorizationBlob;


//
// Initial-setup data for versioning etc.
//
typedef struct {
	uint32_t order;
	uint32_t version;
} ClientSetupInfo;

#define SSPROTOVERSION 20000


//
// Database parameter structure
//
typedef struct {
	uint32_t idleTimeout;				// seconds idle timout lock
	uint8_t lockOnSleep;				// lock keychain when system sleeps
} DBParameters;


#ifdef __cplusplus


//
// A client connection (session)
//
class ClientSession : public ClientCommon {
public:
	ClientSession(Allocator &standard = Allocator::standard(),
		Allocator &returning = Allocator::standard());
	virtual ~ClientSession();

public:
	void activate();
	void reset();
	
public:
	// use this only if you know what you're doing...
	void contactName(const char *name);
	const char *contactName() const;

    static GenericHandle toIPCHandle(CSSM_HANDLE h) {
        // implementation subject to change
        if (h & (CSSM_HANDLE(~0) ^ GenericHandle(~0)))
            CssmError::throwMe(CSSM_ERRCODE_INVALID_CONTEXT_HANDLE);
        return h & GenericHandle(~0);
    }


public:
	//
	// common database interface
	//
	void authenticateDb(DbHandle db, CSSM_DB_ACCESS_TYPE type, const AccessCredentials *cred);
	void releaseDb(DbHandle db);
	
	//
	// External database interface
	//
	DbHandle openToken(uint32 ssid, const AccessCredentials *cred, const char *name = NULL);

	RecordHandle insertRecord(DbHandle db,
					  CSSM_DB_RECORDTYPE recordType,
					  const CssmDbRecordAttributeData *attributes,
					  const CssmData *data);
	void deleteRecord(DbHandle db, RecordHandle record);
	void modifyRecord(DbHandle db, RecordHandle &record,
					  CSSM_DB_RECORDTYPE recordType,
					  const CssmDbRecordAttributeData *attributesToBeModified,
					  const CssmData *dataToBeModified,
					  CSSM_DB_MODIFY_MODE modifyMode);

	RecordHandle findFirst(DbHandle db,
						const CssmQuery &query,
						SearchHandle &outSearchHandle,
						CssmDbRecordAttributeData *inOutAttributes,
						CssmData *outData, KeyHandle &key);
	RecordHandle findNext(SearchHandle searchHandle,
					   CssmDbRecordAttributeData *inOutAttributes,
					   CssmData *inOutData, KeyHandle &key);
	void findRecordHandle(RecordHandle record,
						 CssmDbRecordAttributeData *inOutAttributes,
						 CssmData *inOutData, KeyHandle &key);
	void releaseSearch(SearchHandle searchHandle);
	void releaseRecord(RecordHandle record);
	
	void getDbName(DbHandle db, std::string &name);
	void setDbName(DbHandle db, const std::string &name);

	//
	// Internal database interface
	//
	DbHandle createDb(const DLDbIdentifier &dbId,
        const AccessCredentials *cred, const AclEntryInput *owner,
        const DBParameters &params);
	DbHandle cloneDbForSync(const CssmData &secretsBlob, DbHandle srcDb, 
							const CssmData &agentData);
	DbHandle recodeDbForSync(DbHandle dbToClone, DbHandle srcDb);
	DbHandle authenticateDbsForSync(const CssmData &dbHandleArray, const CssmData &agentData);
    void commitDbForSync(DbHandle srcDb, DbHandle cloneDb, CssmData &blob, Allocator &alloc);
	DbHandle decodeDb(const DLDbIdentifier &dbId,
        const AccessCredentials *cred, const CssmData &blob);
	void encodeDb(DbHandle db, CssmData &blob, Allocator &alloc);
    void encodeDb(DbHandle db, CssmData &blob) { return encodeDb(db, blob, returnAllocator); }
	void setDbParameters(DbHandle db, const DBParameters &params);
	void getDbParameters(DbHandle db, DBParameters &params);
    void changePassphrase(DbHandle db, const AccessCredentials *cred);
    void lock(DbHandle db);
    void lockAll(bool forSleep);
    void unlock(DbHandle db);
    void unlock(DbHandle db, const CssmData &passPhrase);
    bool isLocked(DbHandle db);
	
public:
	//
	// Key objects
	//
	void encodeKey(KeyHandle key, CssmData &blob, KeyUID *uid, Allocator &alloc);
	void encodeKey(KeyHandle key, CssmData &blob, KeyUID *uid = NULL)
    { return encodeKey(key, blob, uid, returnAllocator); }
	KeyHandle decodeKey(DbHandle db, const CssmData &blob, CssmKey::Header &header);
	void recodeKey(DbHandle oldDb, KeyHandle key, DbHandle newDb, CssmData &blob);
	void releaseKey(KeyHandle key);

	CssmKeySize queryKeySizeInBits(KeyHandle key);
    uint32 getOutputSize(const Security::Context &context, KeyHandle key,
        uint32 inputSize, bool encrypt = true);
		
	void getKeyDigest(KeyHandle key, CssmData &digest, Allocator &alloc);
	void getKeyDigest(KeyHandle key, CssmData &digest)
	{ return getKeyDigest(key, digest, returnAllocator); }


    // key wrapping and unwrapping
	void wrapKey(const Security::Context &context, KeyHandle key, KeyHandle keyToBeWrapped,
		const AccessCredentials *cred,
		const CssmData *descriptiveData, CssmWrappedKey &wrappedKey, Allocator &alloc);
	void wrapKey(const Security::Context &context, KeyHandle key, KeyHandle keyToBeWrapped,
		const AccessCredentials *cred,
		const CssmData *descriptiveData, CssmWrappedKey &wrappedKey)
    { return wrapKey(context, key, keyToBeWrapped, cred,
        descriptiveData, wrappedKey, returnAllocator); }
    
	void unwrapKey(DbHandle db, const Security::Context &context, KeyHandle key, KeyHandle publicKey,
		const CssmWrappedKey &wrappedKey, uint32 keyUsage, uint32 keyAttr,
		const AccessCredentials *cred, const AclEntryInput *owner,
		CssmData &data, KeyHandle &newKey, CssmKey::Header &newKeyHeader, Allocator &alloc);
	void unwrapKey(DbHandle db, const Security::Context &context, KeyHandle key, KeyHandle publicKey,
		const CssmWrappedKey &wrappedKey, uint32 keyUsage, uint32 keyAttr,
		const AccessCredentials *cred, const AclEntryInput *owner, CssmData &data,
        KeyHandle &newKey, CssmKey::Header &newKeyHeader)
    { return unwrapKey(db, context, key, publicKey, wrappedKey, keyUsage, keyAttr,
      cred, owner, data, newKey, newKeyHeader, returnAllocator); }

    // key generation and derivation
	void generateKey(DbHandle db, const Security::Context &context, uint32 keyUsage, uint32 keyAttr,
		const AccessCredentials *cred, const AclEntryInput *owner,
        KeyHandle &newKey, CssmKey::Header &newHeader);
	void generateKey(DbHandle db, const Security::Context &context,
		uint32 pubKeyUsage, uint32 pubKeyAttr,
		uint32 privKeyUsage, uint32 privKeyAttr,
		const AccessCredentials *cred, const AclEntryInput *owner,
		KeyHandle &pubKey, CssmKey::Header &pubHeader,
        KeyHandle &privKey, CssmKey::Header &privHeader);
	void deriveKey(DbHandle db, const Security::Context &context, KeyHandle baseKey,
        uint32 keyUsage, uint32 keyAttr, CssmData &param,
		const AccessCredentials *cred, const AclEntryInput *owner,
        KeyHandle &newKey, CssmKey::Header &newHeader, Allocator &alloc);
	void deriveKey(DbHandle db, const Security::Context &context, KeyHandle baseKey,
        uint32 keyUsage, uint32 keyAttr, CssmData &param,
		const AccessCredentials *cred, const AclEntryInput *owner,
        KeyHandle &newKey, CssmKey::Header &newHeader)
    { return deriveKey(db, context, baseKey, keyUsage, keyAttr, param, cred, owner, newKey, newHeader, returnAllocator); }
	//void generateAlgorithmParameters();	// not implemented

	void generateRandom(const Security::Context &context, CssmData &data, Allocator &alloc);
	void generateRandom(const Security::Context &context, CssmData &data)
	{ return generateRandom(context, data, returnAllocator); }
	
    // encrypt/decrypt
	void encrypt(const Security::Context &context, KeyHandle key,
        const CssmData &in, CssmData &out, Allocator &alloc);
	void encrypt(const Security::Context &context, KeyHandle key, const CssmData &in, CssmData &out)
    { return encrypt(context, key, in, out, returnAllocator); }
	void decrypt(const Security::Context &context, KeyHandle key,
        const CssmData &in, CssmData &out, Allocator &alloc);
	void decrypt(const Security::Context &context, KeyHandle key, const CssmData &in, CssmData &out)
    { return decrypt(context, key, in, out, returnAllocator); }

    // signatures
	void generateSignature(const Security::Context &context, KeyHandle key,
        const CssmData &data, CssmData &signature, Allocator &alloc,
        CSSM_ALGORITHMS signOnlyAlgorithm = CSSM_ALGID_NONE);
	void generateSignature(const Security::Context &context, KeyHandle key,
		const CssmData &data, CssmData &signature, CSSM_ALGORITHMS signOnlyAlgorithm = CSSM_ALGID_NONE)
    { return generateSignature(context, key, data, signature, returnAllocator, signOnlyAlgorithm); }
	void verifySignature(const Security::Context &context, KeyHandle key,
		const CssmData &data, const CssmData &signature,
        CSSM_ALGORITHMS verifyOnlyAlgorithm = CSSM_ALGID_NONE);
		
    // MACs
	void generateMac(const Security::Context &context, KeyHandle key,
		const CssmData &data, CssmData &mac, Allocator &alloc);
	void generateMac(const Security::Context &context, KeyHandle key,
		const CssmData &data, CssmData &mac)
    { return generateMac(context, key, data, mac, returnAllocator); }
	void verifyMac(const Security::Context &context, KeyHandle key,
		const CssmData &data, const CssmData &mac);
	
    // key ACL management
	void getKeyAcl(KeyHandle key, const char *tag,
        uint32 &count, AclEntryInfo * &info, Allocator &alloc);
	void getKeyAcl(KeyHandle key, const char *tag,
        uint32 &count, AclEntryInfo * &info)
    { return getKeyAcl(key, tag, count, info, returnAllocator); }
	void changeKeyAcl(KeyHandle key, const AccessCredentials &cred, const AclEdit &edit);
	void getKeyOwner(KeyHandle key, AclOwnerPrototype &owner, Allocator &alloc);
	void getKeyOwner(KeyHandle key, AclOwnerPrototype &owner)
    { return getKeyOwner(key, owner, returnAllocator); }
	void changeKeyOwner(KeyHandle key, const AccessCredentials &cred,
		const AclOwnerPrototype &edit);
	
    // database ACL management
	void getDbAcl(DbHandle db, const char *tag,
        uint32 &count, AclEntryInfo * &info, Allocator &alloc);
	void getDbAcl(DbHandle db, const char *tag,
        uint32 &count, AclEntryInfo * &info)
    { return getDbAcl(db, tag, count, info, returnAllocator); }
	void changeDbAcl(DbHandle db, const AccessCredentials &cred, const AclEdit &edit);
	void getDbOwner(DbHandle db, AclOwnerPrototype &owner, Allocator &alloc);
	void getDbOwner(DbHandle db, AclOwnerPrototype &owner)
    { return getDbOwner(db, owner, returnAllocator); }
    void changeDbOwner(DbHandle db, const AccessCredentials &cred,
		const AclOwnerPrototype &edit);
	
	// database key manipulations
	void extractMasterKey(DbHandle db, const Context &context, DbHandle sourceDb,
        uint32 keyUsage, uint32 keyAttr,
		const AccessCredentials *cred, const AclEntryInput *owner,
        KeyHandle &newKey, CssmKey::Header &newHeader, Allocator &alloc);
	void extractMasterKey(DbHandle db, const Context &context, DbHandle sourceDb,
        uint32 keyUsage, uint32 keyAttr,
		const AccessCredentials *cred, const AclEntryInput *owner,
        KeyHandle &newKey, CssmKey::Header &newHeader)
	{ return extractMasterKey(db, context, sourceDb, keyUsage, keyAttr, cred, owner,
		newKey, newHeader, returnAllocator); }
		
public:
	// Authorization API support
	void authCreate(const AuthorizationItemSet *rights,	const AuthorizationItemSet *environment, 
		AuthorizationFlags flags,AuthorizationBlob &result);
	void authRelease(const AuthorizationBlob &auth, AuthorizationFlags flags);
	void authCopyRights(const AuthorizationBlob &auth,
		const AuthorizationItemSet *rights, const AuthorizationItemSet *environment,
		AuthorizationFlags flags, AuthorizationItemSet **result);
	void authCopyInfo(const AuthorizationBlob &auth, const char *tag, AuthorizationItemSet * &info);
	void authExternalize(const AuthorizationBlob &auth, AuthorizationExternalForm &extForm);
	void authInternalize(const AuthorizationExternalForm &extForm, AuthorizationBlob &auth);
    
public:
    // Session API support
	void setSessionUserPrefs(SecuritySessionId sessionId, uint32_t userPreferencesLength, const void *userPreferences);
    
public:
    // Notification core support
    void postNotification(NotificationDomain domain, NotificationEvent event, const CssmData &data);
    
	// low-level callback (C form)
    typedef OSStatus ConsumeNotification(NotificationDomain domain, NotificationEvent event,
        const void *data, size_t dataLength, void *context);
	
public:
	// AuthorizationDB API
	void authorizationdbGet(const AuthorizationString rightname, CssmData &rightDefinition, Allocator &alloc);
	void authorizationdbSet(const AuthorizationBlob &auth, const AuthorizationString rightname, uint32_t rightdefinitionLength, const void *rightdefinition);
	void authorizationdbRemove(const AuthorizationBlob &auth, const AuthorizationString rightname);
	
public:
	// securityd helper support
	void childCheckIn(Port serverPort, Port taskPort);
	
public:
	// miscellaneous administrative calls
	void addCodeEquivalence(const CssmData &oldCode, const CssmData &newCode,
		const char *name, bool forSystem = false);
	void removeCodeEquivalence(const CssmData &code, const char *name, bool forSystem = false);
	void setAlternateSystemRoot(const char *path);

public:
	// temporary hack to deal with "edit acl" pseudo-error returns
	typedef void DidChangeKeyAclCallback(void *context, ClientSession &clientSession,
		KeyHandle key, CSSM_ACL_AUTHORIZATION_TAG tag);
	void registerForAclEdits(DidChangeKeyAclCallback *callback, void *context);
	
public:
	// Code Signing hosting interface
	void registerHosting(mach_port_t hostingPort, SecCSFlags flags);
	mach_port_t hostingPort(pid_t pid);
	
	SecGuestRef createGuest(SecGuestRef host,
		uint32_t status, const char *path, const CssmData &cdhash, const CssmData &attributes, SecCSFlags flags);
	void setGuestStatus(SecGuestRef guest, uint32 status, const CssmData &attributes);
	void removeGuest(SecGuestRef host, SecGuestRef guest);
	
	void selectGuest(SecGuestRef guest);
	SecGuestRef selectedGuest() const; 

private:
	static Port findSecurityd();
	void getAcl(AclKind kind, GenericHandle key, const char *tag,
		uint32 &count, AclEntryInfo * &info, Allocator &alloc);
	void changeAcl(AclKind kind, GenericHandle key,
		const AccessCredentials &cred, const AclEdit &edit);
	void getOwner(AclKind kind, GenericHandle key,
		AclOwnerPrototype &owner, Allocator &alloc);
	void changeOwner(AclKind kind, GenericHandle key,
		const AccessCredentials &cred, const AclOwnerPrototype &edit);
	
	static OSStatus consumerDispatch(NotificationDomain domain, NotificationEvent event,
		const void *data, size_t dataLength, void *context);

	void notifyAclChange(KeyHandle key, CSSM_ACL_AUTHORIZATION_TAG tag);

	void returnAttrsAndData(CssmDbRecordAttributeData *inOutAttributes,
		CssmDbRecordAttributeData *attrs, CssmDbRecordAttributeData *attrsBase, mach_msg_type_number_t attrsLength,
		CssmData *inOutData, void *dataPtr, mach_msg_type_number_t dataLength);
private:
	DidChangeKeyAclCallback *mCallback;
	void *mCallbackContext;

	static UnixPlusPlus::StaticForkMonitor mHasForked;	// global fork indicator

	struct Thread {
		Thread() : registered(false), notifySeq(0),
			currentGuest(kSecNoGuest), lastGuest(kSecNoGuest) { }
		operator bool() const { return registered; }
		
		ReceivePort replyPort;	// dedicated reply port (send right held by SecurityServer)
        bool registered;		// has been registered with SecurityServer
		uint32 notifySeq; // notification sequence number
		
		SecGuestRef currentGuest;	// last set guest path
		SecGuestRef lastGuest;		// last transmitted guest path
	};

	struct Global {
        Global();
		Port serverPort;
		RefPointer<OSXCode> myself;
		ThreadNexus<Thread> thread;
	};

	static ModuleNexus<Global> mGlobal;
	static const char *mContactName;
	static SecGuestRef mDedicatedGuest;
};


} // end namespace SecurityServer
} // end namespace Security

#endif //__cplusplus


#endif //_H_SSCLIENT
