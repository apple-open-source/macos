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
// ssclient - SecurityServer client interface library
//
// This interface is private to the Security system. It is not a public interface,
// and it may change at any time. You have been warned.
//
#ifndef _H_SSCLIENT
#define _H_SSCLIENT


#include <Security/cssm.h>
#include <Security/utilities.h>
#include <Security/cssmalloc.h>
#include <Security/cssmacl.h>
#include <Security/context.h>
#include <Security/globalizer.h>
#include <Security/unix++.h>
#include <Security/mach++.h>
#include <Security/cssmdb.h>
#include <Security/osxsigning.h>
#include <Security/Authorization.h>
#include <Security/AuthSession.h>
#include <Security/notifications.h>
#include <Security/context.h>

namespace Security {
namespace SecurityServer {

using MachPlusPlus::Port;
using MachPlusPlus::ReceivePort;


//
// The default Mach bootstrap registration name for SecurityServer,
// and the environment variable to override it
//
#define SECURITYSERVER_BOOTSTRAP_NAME	"com.apple.SecurityServer"
#define SECURITYSERVER_BOOTSTRAP_ENV	"SECURITYSERVER"


//
// Common data types
//
typedef CSSM_HANDLE KeyHandle;
typedef CSSM_HANDLE DbHandle;
	
static const CSSM_HANDLE noDb = 0;
static const CSSM_HANDLE noKey = 0;

struct KeyUID {
    uint8 signature[20];
};

struct AuthorizationBlob {
    uint32 data[2];
	
	bool operator < (const AuthorizationBlob &other) const
	{ return memcmp(data, other.data, sizeof(data)) < 0; }
	
	bool operator == (const AuthorizationBlob &other) const
	{ return memcmp(data, other.data, sizeof(data)) == 0; }
    
    size_t hash() const {	//@@@ revisit this hash
        return data[0] ^ data[1] << 3;
    }
};

struct ClientSetupInfo {
	uint32 version;
};
#define SSPROTOVERSION 4

enum AclKind { dbAcl, keyAcl, loginAcl };


//
// Database parameter structure
//
class DBParameters {
public:
	uint32 idleTimeout;				// seconds idle timout lock
	uint8 lockOnSleep;				// lock keychain when system sleeps
};


//
// A client connection (session)
//
class ClientSession {
	NOCOPY(ClientSession)
public:
	ClientSession(CssmAllocator &standard, CssmAllocator &returning);
	virtual ~ClientSession();
	
	CssmAllocator &internalAllocator;
	CssmAllocator &returnAllocator;
	
public:
	typedef CSSM_DB_ACCESS_TYPE DBAccessType;
	typedef Security::Context Context;

public:
	void activate();
	void terminate();
	
public:
	// use this only if you know what you're doing...
	void contactName(const char *name);
	const char *contactName() const;

public:
	// database sessions
	DbHandle createDb(const DLDbIdentifier &dbId,
        const AccessCredentials *cred, const AclEntryInput *owner,
        const DBParameters &params);
	DbHandle decodeDb(const DLDbIdentifier &dbId,
        const AccessCredentials *cred, const CssmData &blob);
	void encodeDb(DbHandle db, CssmData &blob, CssmAllocator &alloc);
    void encodeDb(DbHandle db, CssmData &blob) { return encodeDb(db, blob, returnAllocator); }
	void releaseDb(DbHandle db);
	void authenticateDb(DbHandle db, DBAccessType type, const AccessCredentials *cred);
	void setDbParameters(DbHandle db, const DBParameters &params);
	void getDbParameters(DbHandle db, DBParameters &params);
	void getDbSuggestedIndex(DbHandle db, CssmData &index, CssmAllocator &alloc);
	void getDbSuggestedIndex(DbHandle db, CssmData &index)
	{ return getDbSuggestedIndex(db, index, returnAllocator); }
    void changePassphrase(DbHandle db, const AccessCredentials *cred);
    void lock(DbHandle db);
    void lockAll(bool forSleep);
    void unlock(DbHandle db);
    void unlock(DbHandle db, const CssmData &passPhrase);
    bool isLocked(DbHandle db);
	
	// key objects
	void encodeKey(KeyHandle key, CssmData &blob, KeyUID *uid, CssmAllocator &alloc);
	void encodeKey(KeyHandle key, CssmData &blob, KeyUID *uid = NULL)
    { return encodeKey(key, blob, uid, returnAllocator); }
	KeyHandle decodeKey(DbHandle db, const CssmData &blob, CssmKey::Header &header);
	void releaseKey(KeyHandle key);

	CssmKeySize queryKeySizeInBits(KeyHandle key);
    uint32 getOutputSize(const Security::Context &context, KeyHandle key,
        uint32 inputSize, bool encrypt = true);
		
	void getKeyDigest(KeyHandle key, CssmData &digest, CssmAllocator &alloc);
	void getKeyDigest(KeyHandle key, CssmData &digest)
	{ return getKeyDigest(key, digest, returnAllocator); }


public:
    // key wrapping and unwrapping
	void wrapKey(const Security::Context &context, KeyHandle key, KeyHandle keyToBeWrapped,
		const AccessCredentials *cred,
		const CssmData *descriptiveData, CssmWrappedKey &wrappedKey, CssmAllocator &alloc);
	void wrapKey(const Security::Context &context, KeyHandle key, KeyHandle keyToBeWrapped,
		const AccessCredentials *cred,
		const CssmData *descriptiveData, CssmWrappedKey &wrappedKey)
    { return wrapKey(context, key, keyToBeWrapped, cred,
        descriptiveData, wrappedKey, returnAllocator); }
    
	void unwrapKey(DbHandle db, const Security::Context &context, KeyHandle key, KeyHandle publicKey,
		const CssmWrappedKey &wrappedKey, uint32 keyUsage, uint32 keyAttr,
		const AccessCredentials *cred, const AclEntryInput *owner,
		CssmData &data, KeyHandle &newKey, CssmKey::Header &newKeyHeader, CssmAllocator &alloc);
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
        KeyHandle &newKey, CssmKey::Header &newHeader, CssmAllocator &alloc);
	void deriveKey(DbHandle db, const Security::Context &context, KeyHandle baseKey,
        uint32 keyUsage, uint32 keyAttr, CssmData &param,
		const AccessCredentials *cred, const AclEntryInput *owner,
        KeyHandle &newKey, CssmKey::Header &newHeader)
    { return deriveKey(db, context, baseKey, keyUsage, keyAttr, param, cred, owner, newKey, newHeader, returnAllocator); }
	//void generateAlgorithmParameters();	// not implemented

	void generateRandom(CssmData &data);
	
    // encrypt/decrypt
	void encrypt(const Security::Context &context, KeyHandle key,
        const CssmData &in, CssmData &out, CssmAllocator &alloc);
	void encrypt(const Security::Context &context, KeyHandle key, const CssmData &in, CssmData &out)
    { return encrypt(context, key, in, out, returnAllocator); }
	void decrypt(const Security::Context &context, KeyHandle key,
        const CssmData &in, CssmData &out, CssmAllocator &alloc);
	void decrypt(const Security::Context &context, KeyHandle key, const CssmData &in, CssmData &out)
    { return decrypt(context, key, in, out, returnAllocator); }

    // signatures
	void generateSignature(const Security::Context &context, KeyHandle key,
        const CssmData &data, CssmData &signature, CssmAllocator &alloc,
        CSSM_ALGORITHMS signOnlyAlgorithm = CSSM_ALGID_NONE);
	void generateSignature(const Security::Context &context, KeyHandle key,
		const CssmData &data, CssmData &signature, CSSM_ALGORITHMS signOnlyAlgorithm = CSSM_ALGID_NONE)
    { return generateSignature(context, key, data, signature, returnAllocator, signOnlyAlgorithm); }
	void verifySignature(const Security::Context &context, KeyHandle key,
		const CssmData &data, const CssmData &signature,
        CSSM_ALGORITHMS verifyOnlyAlgorithm = CSSM_ALGID_NONE);
		
    // MACs
	void generateMac(const Security::Context &context, KeyHandle key,
		const CssmData &data, CssmData &mac, CssmAllocator &alloc);
	void generateMac(const Security::Context &context, KeyHandle key,
		const CssmData &data, CssmData &mac)
    { return generateMac(context, key, data, mac, returnAllocator); }
	void verifyMac(const Security::Context &context, KeyHandle key,
		const CssmData &data, const CssmData &mac);
	
    // key ACL management
	void getKeyAcl(KeyHandle key, const char *tag,
        uint32 &count, AclEntryInfo * &info, CssmAllocator &alloc);
	void getKeyAcl(KeyHandle key, const char *tag,
        uint32 &count, AclEntryInfo * &info)
    { return getKeyAcl(key, tag, count, info, returnAllocator); }
	void changeKeyAcl(KeyHandle key, const AccessCredentials &cred, const AclEdit &edit);
	void getKeyOwner(KeyHandle key, AclOwnerPrototype &owner, CssmAllocator &alloc);
	void getKeyOwner(KeyHandle key, AclOwnerPrototype &owner)
    { return getKeyOwner(key, owner, returnAllocator); }
	void changeKeyOwner(KeyHandle key, const AccessCredentials &cred,
		const AclOwnerPrototype &edit);
	
    // database ACL management
	void getDbAcl(DbHandle db, const char *tag,
        uint32 &count, AclEntryInfo * &info, CssmAllocator &alloc);
	void getDbAcl(DbHandle db, const char *tag,
        uint32 &count, AclEntryInfo * &info)
    { return getDbAcl(db, tag, count, info, returnAllocator); }
	void changeDbAcl(DbHandle db, const AccessCredentials &cred, const AclEdit &edit);
	void getDbOwner(DbHandle db, AclOwnerPrototype &owner, CssmAllocator &alloc);
	void getDbOwner(DbHandle db, AclOwnerPrototype &owner)
    { return getDbOwner(db, owner, returnAllocator); }
    void changeDbOwner(DbHandle db, const AccessCredentials &cred,
		const AclOwnerPrototype &edit);
	
	// database key manipulations
	void extractMasterKey(DbHandle db, const Context &context, DbHandle sourceDb,
        uint32 keyUsage, uint32 keyAttr,
		const AccessCredentials *cred, const AclEntryInput *owner,
        KeyHandle &newKey, CssmKey::Header &newHeader, CssmAllocator &alloc);
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
    void getSessionInfo(SecuritySessionId &sessionId, SessionAttributeBits &attrs);
    void setupSession(SessionCreationFlags flags, SessionAttributeBits attrs);
    
public:
    // Notification core support
    void requestNotification(Port receiver, Listener::Domain domain, Listener::EventMask events);
    void stopNotification(Port receiver);
    void postNotification(Listener::Domain domain, Listener::Event event, const CssmData &data);
    
    typedef OSStatus ConsumeNotification(Listener::Domain domain, Listener::Event event,
        const void *data, size_t dataLength, void *context);
    OSStatus dispatchNotification(const mach_msg_header_t *message,
        ConsumeNotification *consumer, void *context) throw();

public:
	// AuthorizationDB API
	void authorizationdbGet(const AuthorizationString rightname, CssmData &rightDefinition, CssmAllocator &alloc);
	void authorizationdbSet(const AuthorizationBlob &auth, const AuthorizationString rightname, uint32_t rightdefinitionLength, const void *rightdefinition);
	void authorizationdbRemove(const AuthorizationBlob &auth, const AuthorizationString rightname);
	
public:
	// miscellaneous administrative calls
	void addCodeEquivalence(const CssmData &oldCode, const CssmData &newCode,
		const char *name, bool forSystem = false);
	void removeCodeEquivalence(const CssmData &code, const char *name, bool forSystem = false);
	void setAlternateSystemRoot(const char *path);

private:
	void getAcl(AclKind kind, KeyHandle key, const char *tag,
		uint32 &count, AclEntryInfo * &info, CssmAllocator &alloc);
	void changeAcl(AclKind kind, KeyHandle key,
		const AccessCredentials &cred, const AclEdit &edit);
	void getOwner(AclKind kind, KeyHandle key, AclOwnerPrototype &owner, CssmAllocator &alloc);
	void changeOwner(AclKind kind, KeyHandle key, const AccessCredentials &cred,
		const AclOwnerPrototype &edit);

private:
	static UnixPlusPlus::StaticForkMonitor mHasForked;	// global fork indicator

	struct Thread {
		Thread() : registered(false) { }
		operator bool() const { return registered; }
		
		ReceivePort replyPort;	// dedicated reply port (send right held by SecurityServer)
        bool registered;		// has been registered with SecurityServer
	};

	struct Global {
        Global();
		Port serverPort;
		RefPointer<CodeSigning::OSXCode> myself;
		ThreadNexus<Thread> thread;
	};

	static ModuleNexus<Global> mGlobal;
	static bool mSetupSession;
	static const char *mContactName;
};


} // end namespace SecurityServer
} // end namespace Security


#endif //_H_SSCLIENT
