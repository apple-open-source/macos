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
// transition - SecurityServer IPC-to-class-methods transition layer
//
#include <Security/AuthorizationWalkers.h>
#include "server.h"
#include "ucsp.h"
#include "session.h"
#include "xdatabase.h"
#include <mach/mach_error.h>


//
// Bracket Macros
//
#define UCSP_ARGS	mach_port_t sport, mach_port_t rport, security_token_t securityToken, \
                    CSSM_RETURN *rcode
#define CONTEXT_ARGS Context context, Pointer contextBase, Context::Attr *attributes, mach_msg_type_number_t attrCount

#define BEGIN_IPCN	*rcode = CSSM_OK; try {
#define BEGIN_IPC	BEGIN_IPCN Connection &connection = Server::connection(rport);
#define END_IPC(base)	END_IPCN(base) Server::requestComplete(); return KERN_SUCCESS;
#define END_IPCN(base) 	} \
	catch (const CssmCommonError &err) { *rcode = err.cssmError(CSSM_ ## base ## _BASE_ERROR); } \
	catch (std::bad_alloc) { *rcode = CssmError::merge(CSSM_ERRCODE_MEMORY_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
	catch (Connection *conn) { *rcode = 0; } \
	catch (...) { *rcode = CssmError::merge(CSSM_ERRCODE_INTERNAL_ERROR, CSSM_ ## base ## _BASE_ERROR); }

#define DATA_IN(base)	void *base, mach_msg_type_number_t base##Length
#define DATA_OUT(base)	void **base, mach_msg_type_number_t *base##Length
#define DATA(base)		CssmData(base, base##Length)

#define COPY_IN(type,name)	type *name, mach_msg_type_number_t name##Length, type *name##Base
#define COPY_OUT(type,name)	\
	type **name, mach_msg_type_number_t *name##Length, type **name##Base
	

using LowLevelMemoryUtilities::increment;
using LowLevelMemoryUtilities::difference;


//
// An OutputData object will take memory allocated within the SecurityServer,
// hand it to the MIG return-output parameters, and schedule it to be released
// after the MIG reply has been sent. It will also get rid of it in case of
// error.
//
class OutputData : public CssmData {
public:
	OutputData(void **outP, mach_msg_type_number_t *outLength)
		: mData(*outP), mLength(*outLength) { }
	~OutputData()
	{ mData = data(); mLength = length(); Server::releaseWhenDone(mData); }
	
private:
	void * &mData;
	mach_msg_type_number_t &mLength;
};


//
// A CheckingReconstituteWalker is a variant of an ordinary ReconstituteWalker
// that checks object pointers and sizes against the incoming block limits.
// It throws an exception if incoming data has pointers outside the incoming block.
// This avoids trouble inside of the SecurityServer caused (by bug or malice)
// from someone spoofing the client access side.
//
class CheckingReconstituteWalker {
public:
    CheckingReconstituteWalker(void *ptr, void *base, size_t size)
    : mBase(base), mLimit(increment(base, size)), mOffset(difference(ptr, base)) { }

    template <class T>
    void operator () (T * &addr, size_t size = sizeof(T))
    {
        if (addr) {
			if (addr < mBase || increment(addr, size) > mLimit)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_POINTER);
            addr = increment<T>(addr, mOffset);
		}
    }
    
    static const bool needsRelinking = true;
    static const bool needsSize = false;
    
private:
	void *mBase;			// old base address
	void *mLimit;			// old last byte address + 1
    off_t mOffset;			// relocation offset
};

template <class T>
void relocate(T *obj, T *base, size_t size)
{
    if (obj) {
        CheckingReconstituteWalker w(obj, base, size);
        walk(w, base);
    }
}



//
// Setup/Teardown functions.
//
kern_return_t ucsp_server_setup(UCSP_ARGS, mach_port_t taskPort, const char *identity)
{
	BEGIN_IPCN
	Server::active().setupConnection(rport, taskPort, securityToken, identity);
	END_IPCN(CSSM)
	return KERN_SUCCESS;
}

kern_return_t ucsp_server_teardown(UCSP_ARGS)
{
	BEGIN_IPCN
	Server::active().endConnection(rport);
	END_IPCN(CSSM)
	return KERN_SUCCESS;
}


//
// Database management
//
kern_return_t ucsp_server_createDb(UCSP_ARGS, DbHandle *db,
	COPY_IN(DLDbFlatIdentifier, ident),
    COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
    DBParameters params)
{
	BEGIN_IPC
	relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
    relocate(ident, identBase, identLength);
	*db = (new Database(*ident, params, connection.process, cred, owner))->handle();
	END_IPC(DL)
}

kern_return_t ucsp_server_decodeDb(UCSP_ARGS, DbHandle *db,
    COPY_IN(DLDbFlatIdentifier, ident), COPY_IN(AccessCredentials, cred), DATA_IN(blob))
{
	BEGIN_IPC
	relocate(cred, credBase, credLength);
    relocate(ident, identBase, identLength);
	*db = (new Database(*ident, DATA(blob).interpretedAs<DbBlob>(),
        connection.process, cred))->handle();
	END_IPC(DL)
}

kern_return_t ucsp_server_encodeDb(UCSP_ARGS, DbHandle db, DATA_OUT(blob))
{
	BEGIN_IPC
    DbBlob *dbBlob = Server::database(db).encode();	// memory owned by database
    *blob = dbBlob;
    *blobLength = dbBlob->length();
	END_IPC(DL)
}

kern_return_t ucsp_server_releaseDb(UCSP_ARGS, DbHandle db)
{
	BEGIN_IPC
	delete &Server::database(db);
	END_IPC(DL)
}

kern_return_t ucsp_server_authenticateDb(UCSP_ARGS, DbHandle db,
    COPY_IN(AccessCredentials, cred))
{
	BEGIN_IPC
    relocate(cred, credBase, credLength);
    Server::database(db).authenticate(cred);
	END_IPC(DL)
}

kern_return_t ucsp_server_setDbParameters(UCSP_ARGS, DbHandle db, DBParameters params)
{
	BEGIN_IPC
	Server::database(db).setParameters(params);
	END_IPC(DL)
}

kern_return_t ucsp_server_getDbParameters(UCSP_ARGS, DbHandle db, DBParameters *params)
{
	BEGIN_IPC
	Server::database(db).getParameters(*params);
	END_IPC(DL)
}

kern_return_t ucsp_server_changePassphrase(UCSP_ARGS, DbHandle db,
    COPY_IN(AccessCredentials, cred))
{
	BEGIN_IPC
    relocate(cred, credBase, credLength);
	Server::database(db).changePassphrase(cred);
	END_IPC(DL)
}

kern_return_t ucsp_server_lockDb(UCSP_ARGS, DbHandle db)
{
	BEGIN_IPC
	Server::database(db).lock();
	END_IPC(DL)
}

kern_return_t ucsp_server_unlockDb(UCSP_ARGS, DbHandle db)
{
	BEGIN_IPC
	Server::database(db).unlock();
	END_IPC(DL)
}

kern_return_t ucsp_server_unlockDbWithPassphrase(UCSP_ARGS, DbHandle db, DATA_IN(passphrase))
{
	BEGIN_IPC
	Server::database(db).unlock(DATA(passphrase));
	END_IPC(DL)
}

kern_return_t ucsp_server_isLocked(UCSP_ARGS, DbHandle db, boolean_t *locked)
{
    BEGIN_IPC
    *locked = Server::database(db).isLocked();
    END_IPC(DL)
}


//
// Key management
//
kern_return_t ucsp_server_encodeKey(UCSP_ARGS, KeyHandle keyh, DATA_OUT(blob),
    boolean_t wantUid, DATA_OUT(uid))
{
	BEGIN_IPC
    Key &key = Server::key(keyh);
    KeyBlob *keyBlob = key.blob();	// still owned by key
    *blob = keyBlob;
    *blobLength = keyBlob->length();
    if (wantUid) {
        *uid = &key.uid();
        *uidLength = sizeof(KeyUID);
    } else {
        *uidLength = 0;	// do not return this
    }
	END_IPC(CSP)
}

kern_return_t ucsp_server_decodeKey(UCSP_ARGS, KeyHandle *keyh, CssmKey::Header *header,
	DbHandle db, DATA_IN(blob))
{
	BEGIN_IPC
    Key &key = *new Key(Server::database(db), DATA(blob).interpretedAs<KeyBlob>());
    key.returnKey(*keyh, *header);
	END_IPC(CSP)
}

kern_return_t ucsp_server_releaseKey(UCSP_ARGS, KeyHandle key)
{
	BEGIN_IPC
	connection.releaseKey(key);
	END_IPC(CSP)
}


//
// RNG interface
//
kern_return_t ucsp_server_generateRandom(UCSP_ARGS, uint32 bytes, DATA_OUT(data))
{
	BEGIN_IPC
	CssmAllocator &allocator = CssmAllocator::standard(CssmAllocator::sensitive);
	void *buffer = allocator.malloc(bytes);
    Server::active().random(buffer, bytes);
	*data = buffer;
	*dataLength = bytes;
	Server::releaseWhenDone(allocator, buffer);
	END_IPC(CSP)
}


//
// Signatures and MACs
//
kern_return_t ucsp_server_generateSignature(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
		DATA_IN(data), DATA_OUT(signature))
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
	OutputData sigData(signature, signatureLength);
	connection.generateSignature(context, findHandle<Key>(key),
		DATA(data), sigData);
	END_IPC(CSP)
}

kern_return_t ucsp_server_verifySignature(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
		DATA_IN(data), DATA_IN(signature))
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
	connection.verifySignature(context, findHandle<Key>(key),
		DATA(data), DATA(signature));
	END_IPC(CSP)
}

kern_return_t ucsp_server_generateMac(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
		DATA_IN(data), DATA_OUT(mac))
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
	OutputData macData(mac, macLength);
	connection.generateMac(context, findHandle<Key>(key),
		DATA(data), macData);
	END_IPC(CSP)
}

kern_return_t ucsp_server_verifyMac(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
		DATA_IN(data), DATA_IN(mac))
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
	connection.verifyMac(context, findHandle<Key>(key),
		DATA(data), DATA(mac));
	END_IPC(CSP)
}


//
// Encryption/Decryption
//
kern_return_t ucsp_server_encrypt(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
	DATA_IN(clear), DATA_OUT(cipher))
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
	OutputData cipherOut(cipher, cipherLength);
	connection.encrypt(context, findHandle<Key>(key),
		DATA(clear), cipherOut);
	END_IPC(CSP)
}

kern_return_t ucsp_server_decrypt(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
	DATA_IN(cipher), DATA_OUT(clear))
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
	OutputData clearOut(clear, clearLength);
	connection.decrypt(context, findHandle<Key>(key),
		DATA(cipher), clearOut);
	END_IPC(CSP)
}


//
// Key generation
//
kern_return_t ucsp_server_generateKey(UCSP_ARGS, DbHandle db, CONTEXT_ARGS,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	uint32 usage, uint32 attrs, KeyHandle *newKey, CssmKey::Header *newHeader)
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
    Key *key;
	connection.generateKey(Server::optionalDatabase(db),
		context, cred, owner, usage, attrs, key);
    key->returnKey(*newKey, *newHeader);
	END_IPC(CSP)
}

kern_return_t ucsp_server_generateKeyPair(UCSP_ARGS, DbHandle db, CONTEXT_ARGS,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	uint32 pubUsage, uint32 pubAttrs, uint32 privUsage, uint32 privAttrs,
	KeyHandle *pubKey, CssmKey::Header *pubHeader, KeyHandle *privKey, CssmKey::Header *privHeader)
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
    Key *pub, *priv;
	connection.generateKey(Server::optionalDatabase(db),
		context, cred, owner,
		pubUsage, pubAttrs, privUsage, privAttrs, pub, priv);
    pub->returnKey(*pubKey, *pubHeader);
    priv->returnKey(*privKey, *privHeader);
	END_IPC(CSP)
}


//
// Key wrapping and unwrapping
//
kern_return_t ucsp_server_wrapKey(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
	COPY_IN(AccessCredentials, cred), KeyHandle keyToBeWrapped,
	DATA_IN(descriptiveData), CssmKey *wrappedKey, DATA_OUT(keyData))
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
    relocate(cred, credBase, credLength);
    connection.wrapKey(context, Server::optionalKey(key),
        Server::key(keyToBeWrapped), cred, DATA(descriptiveData), *wrappedKey);
    // transmit key data back as a separate blob
	*keyData = wrappedKey->data();
	*keyDataLength = wrappedKey->length();
	Server::releaseWhenDone(*keyData);
	END_IPC(CSP)
}

kern_return_t ucsp_server_unwrapKey(UCSP_ARGS, DbHandle db, CONTEXT_ARGS, KeyHandle key,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	KeyHandle publicKey, CssmKey wrappedKey, DATA_IN(wrappedKeyData),
	uint32 usage, uint32 attr, DATA_OUT(descriptiveData),
    KeyHandle *newKey, CssmKey::Header *newHeader)
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
	wrappedKey.KeyData = DATA(wrappedKeyData);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
	CssmData descriptiveDatas;
    Key &theKey = connection.unwrapKey(Server::optionalDatabase(db),
		context, Server::optionalKey(key), cred, owner, usage, attr, wrappedKey,
        Server::optionalKey(publicKey), &descriptiveDatas);
    theKey.returnKey(*newKey, *newHeader);
	*descriptiveData = descriptiveDatas.data();
	*descriptiveDataLength = descriptiveDatas.length();
	Server::releaseWhenDone(*descriptiveData);
	END_IPC(CSP)
}


//
// ACL management.
// Watch out for the memory-management tap-dance.
//
kern_return_t ucsp_server_getOwner(UCSP_ARGS, AclKind kind, KeyHandle key,
	COPY_OUT(AclOwnerPrototype, ownerOut))
{
	BEGIN_IPC
	AclOwnerPrototype owner;
	Server::aclBearer(kind, key).cssmGetOwner(owner);	// allocates memory in owner
	Copier<AclOwnerPrototype> owners(&owner, CssmAllocator::standard()); // make flat copy
	{ ChunkFreeWalker free; walk(free, owner); } // release chunked original
	*ownerOut = *ownerOutBase = owners;
	*ownerOutLength = owners.length();
	Server::releaseWhenDone(owners.keep()); // throw flat copy out when done
	END_IPC(CSP)
}

kern_return_t ucsp_server_setOwner(UCSP_ARGS, AclKind kind, KeyHandle key,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclOwnerPrototype, owner))
{
	BEGIN_IPC
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
	Server::aclBearer(kind, key).cssmChangeOwner(*owner, cred);
	END_IPC(CSP)
}

kern_return_t ucsp_server_getAcl(UCSP_ARGS, AclKind kind, KeyHandle key,
	boolean_t haveTag, const char *tag,
	uint32 *countp, COPY_OUT(AclEntryInfo, acls))
{
	BEGIN_IPC
	uint32 count;
	AclEntryInfo *aclList;
	Server::aclBearer(kind, key).cssmGetAcl(haveTag ? tag : NULL, count, aclList);
	*countp = count;
	Copier<AclEntryInfo> aclsOut(AclEntryInfo::overlay(aclList), count); // make flat copy

	{	// release the chunked memory originals
		ChunkFreeWalker free;
		for (uint32 n = 0; n < count; n++)
			walk(free, aclList[n]);
	}
	
	// set result
	*acls = *aclsBase = aclsOut;
	*aclsLength = aclsOut.length();
	Server::releaseWhenDone(aclsOut.keep());
	END_IPC(CSP)
}

kern_return_t ucsp_server_changeAcl(UCSP_ARGS, AclKind kind, KeyHandle key,
	COPY_IN(AccessCredentials, cred), CSSM_ACL_EDIT_MODE mode, CSSM_ACL_HANDLE handle,
	COPY_IN(AclEntryPrototype, acl))
{
	BEGIN_IPC
    relocate(cred, credBase, credLength);
	relocate(acl, aclBase, aclLength);
	AclEntryInput input(*acl);
	Server::aclBearer(kind, key).cssmChangeAcl(AclEdit(mode, handle, &input), cred);
	END_IPC(CSP)
}


//
// Authorization subsystem support
//
kern_return_t ucsp_server_authorizationCreate(UCSP_ARGS,
	COPY_IN(AuthorizationItemSet, rights),
	uint32 flags,
	COPY_IN(AuthorizationItemSet, environment),
	AuthorizationBlob *authorization)
{
	BEGIN_IPC
	relocate(rights, rightsBase, rightsLength);
	relocate(environment, environmentBase, environmentLength);
	*rcode = connection.process.session.authCreate(rights, environment, 
		flags, *authorization);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationRelease(UCSP_ARGS,
	AuthorizationBlob authorization, uint32 flags)
{
	BEGIN_IPC
	connection.process.session.authFree(authorization, flags);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationCopyRights(UCSP_ARGS,
	AuthorizationBlob authorization,
	COPY_IN(AuthorizationItemSet, rights),
	uint32 flags,
	COPY_IN(AuthorizationItemSet, environment),
	COPY_OUT(AuthorizationItemSet, result))
{
	BEGIN_IPC
	relocate(rights, rightsBase, rightsLength);
	relocate(environment, environmentBase, environmentLength);
	Authorization::MutableRightSet grantedRights;
	*rcode = connection.process.session.authGetRights(authorization,
		rights, environment, flags, grantedRights);
	Copier<AuthorizationRights> returnedRights(grantedRights, CssmAllocator::standard());
	*result = *resultBase = returnedRights;
	*resultLength = returnedRights.length();
	Server::releaseWhenDone(returnedRights.keep());
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationCopyInfo(UCSP_ARGS,
	AuthorizationBlob authorization,
	AuthorizationString tag,
	COPY_OUT(AuthorizationItemSet, info))
{
	BEGIN_IPC
	Authorization::MutableRightSet result;
	*rcode = connection.process.session.authGetInfo(authorization,
        tag[0] ? tag : NULL, result);
	Copier<AuthorizationItemSet> returnedInfo(result, CssmAllocator::standard());
	*info = *infoBase = returnedInfo;
	*infoLength = returnedInfo.length();
	Server::releaseWhenDone(returnedInfo.keep());
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationExternalize(UCSP_ARGS,
	AuthorizationBlob authorization, AuthorizationExternalForm *extForm)
{
	BEGIN_IPC
	*rcode = connection.process.session.authExternalize(authorization, *extForm);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationInternalize(UCSP_ARGS,
	AuthorizationExternalForm extForm, AuthorizationBlob *authorization)
{
	BEGIN_IPC
	*rcode = connection.process.session.authInternalize(extForm, *authorization);
	END_IPC(CSSM)
}


//
// Session management subsystem
//
kern_return_t ucsp_server_getSessionInfo(UCSP_ARGS,
    SecuritySessionId *sessionId, SessionAttributeBits *attrs)
{
	BEGIN_IPC
    Session &session = Session::find(*sessionId);
    *sessionId = session.handle();
    *attrs = session.attributes();
	END_IPC(CSSM)
}

kern_return_t ucsp_server_setupSession(UCSP_ARGS,
    SessionCreationFlags flags, SessionAttributeBits attrs)
{
	BEGIN_IPC
    Session::setup(flags, attrs);
	END_IPC(CSSM)
}
