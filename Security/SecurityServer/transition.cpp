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
#include "server.h"
#include "ucsp.h"
#include "session.h"
#include "xdatabase.h"
#include "transwalkers.h"
#include <mach/mach_error.h>

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFPropertyList.h>

//
// Bracket Macros
//
#define UCSP_ARGS	mach_port_t servicePort, mach_port_t replyPort, security_token_t securityToken, \
                    CSSM_RETURN *rcode
#define CONTEXT_ARGS Context context, Pointer contextBase, Context::Attr *attributes, mach_msg_type_number_t attrSize

#define BEGIN_IPCN	*rcode = CSSM_OK; try {
#define BEGIN_IPC	BEGIN_IPCN Connection &connection __attribute__((unused)) = Server::connection(replyPort);
#define END_IPC(base)	END_IPCN(base) Server::requestComplete(); return KERN_SUCCESS;
#define END_IPCN(base) 	} \
	catch (const CssmCommonError &err) { *rcode = err.cssmError(CSSM_ ## base ## _BASE_ERROR); } \
	catch (const std::bad_alloc &) { *rcode = CssmError::merge(CSSM_ERRCODE_MEMORY_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
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
    
    void operator = (const CssmData &source)
    { CssmData::operator = (source); }
	
private:
	void * &mData;
	mach_msg_type_number_t &mLength;
};


//
// Setup/Teardown functions.
//
kern_return_t ucsp_server_setup(UCSP_ARGS, mach_port_t taskPort, ClientSetupInfo info, const char *identity)
{
	BEGIN_IPCN
	Server::active().setupConnection(Server::connectNewProcess, servicePort, replyPort,
		taskPort, securityToken, &info, identity);
	END_IPCN(CSSM)
	return KERN_SUCCESS;
}

kern_return_t ucsp_server_setupNew(UCSP_ARGS, mach_port_t taskPort,
	ClientSetupInfo info, const char *identity,
	mach_port_t *newServicePort)
{
	BEGIN_IPCN
	try {
		Session *session = new DynamicSession(TaskPort(taskPort).bootstrap());
		Server::active().setupConnection(Server::connectNewSession, session->servicePort(), replyPort,
			taskPort, securityToken, &info, identity);
		*newServicePort = session->servicePort();
	} catch (const MachPlusPlus::Error &err) {
		switch (err.error) {
		case BOOTSTRAP_SERVICE_ACTIVE:
			MacOSError::throwMe(errSessionAuthorizationDenied);	// translate
		default:
			throw;
		}
	}
	END_IPCN(CSSM)
	return KERN_SUCCESS;
}

kern_return_t ucsp_server_setupThread(UCSP_ARGS, mach_port_t taskPort)
{
	BEGIN_IPCN
	Server::active().setupConnection(Server::connectNewThread, servicePort, replyPort,
		taskPort, securityToken);
	END_IPCN(CSSM)
	return KERN_SUCCESS;
}


kern_return_t ucsp_server_teardown(UCSP_ARGS)
{
	BEGIN_IPCN
	Server::active().endConnection(replyPort);
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
    DbBlob *dbBlob = Server::database(db).blob();	// memory owned by database
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

kern_return_t ucsp_server_getDbIndex(UCSP_ARGS, DbHandle db, DATA_OUT(index))
{
	BEGIN_IPC
	OutputData indexData(index, indexLength);
	Server::database(db).getDbIndex(indexData);
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

kern_return_t ucsp_server_lockAll (UCSP_ARGS, boolean_t forSleep)
{
	BEGIN_IPC
	Database::lockAllDatabases(connection.process.session.databases(), forSleep);
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
	flip(*header);
	END_IPC(CSP)
}

kern_return_t ucsp_server_releaseKey(UCSP_ARGS, KeyHandle key)
{
	BEGIN_IPC
	connection.releaseKey(key);
	END_IPC(CSP)
}

kern_return_t ucsp_server_queryKeySizeInBits(UCSP_ARGS, KeyHandle key, CSSM_KEY_SIZE *length)
{
	BEGIN_IPC
	*length = connection.queryKeySize(Server::key(key));
	END_IPC(CSP)
}

kern_return_t ucsp_server_getOutputSize(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
    uint32 inputSize, boolean_t encrypt, uint32 *outputSize)
{
    BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    *outputSize = connection.getOutputSize(context, Server::key(key), inputSize, encrypt);
    END_IPC(CSP)
}

kern_return_t ucsp_server_getKeyDigest(UCSP_ARGS, KeyHandle key, DATA_OUT(digest))
{
	BEGIN_IPC
	CssmData digestData = Server::key(key).canonicalDigest();
	*digest = digestData.data();
	*digestLength = digestData.length();
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
        CSSM_ALGORITHMS signOnlyAlgorithm, DATA_IN(data), DATA_OUT(signature))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	OutputData sigData(signature, signatureLength);
	connection.generateSignature(context, Server::key(key), signOnlyAlgorithm,
		DATA(data), sigData);
	END_IPC(CSP)
}

kern_return_t ucsp_server_verifySignature(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
		CSSM_ALGORITHMS verifyOnlyAlgorithm, DATA_IN(data), DATA_IN(signature))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	connection.verifySignature(context, Server::key(key), verifyOnlyAlgorithm,
		DATA(data), DATA(signature));
	END_IPC(CSP)
}

kern_return_t ucsp_server_generateMac(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
		DATA_IN(data), DATA_OUT(mac))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	OutputData macData(mac, macLength);
	connection.generateMac(context, Server::key(key),
		DATA(data), macData);
	END_IPC(CSP)
}

kern_return_t ucsp_server_verifyMac(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
		DATA_IN(data), DATA_IN(mac))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	connection.verifyMac(context, Server::key(key), DATA(data), DATA(mac));
	END_IPC(CSP)
}


//
// Encryption/Decryption
//
kern_return_t ucsp_server_encrypt(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
	DATA_IN(clear), DATA_OUT(cipher))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	OutputData cipherOut(cipher, cipherLength);
	connection.encrypt(context, Server::key(key),
		DATA(clear), cipherOut);
	END_IPC(CSP)
}

kern_return_t ucsp_server_decrypt(UCSP_ARGS, CONTEXT_ARGS, KeyHandle key,
	DATA_IN(cipher), DATA_OUT(clear))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	OutputData clearOut(clear, clearLength);
	connection.decrypt(context, Server::key(key),
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
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
    Key *key;
	connection.generateKey(Server::optionalDatabase(db),
		context, cred, owner, usage, attrs, key);
    key->returnKey(*newKey, *newHeader);
	flip(*newHeader);
	END_IPC(CSP)
}

kern_return_t ucsp_server_generateKeyPair(UCSP_ARGS, DbHandle db, CONTEXT_ARGS,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	uint32 pubUsage, uint32 pubAttrs, uint32 privUsage, uint32 privAttrs,
	KeyHandle *pubKey, CssmKey::Header *pubHeader, KeyHandle *privKey, CssmKey::Header *privHeader)
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
    Key *pub, *priv;
	connection.generateKey(Server::optionalDatabase(db),
		context, cred, owner,
		pubUsage, pubAttrs, privUsage, privAttrs, pub, priv);
    pub->returnKey(*pubKey, *pubHeader);
	flip(*pubHeader);
    priv->returnKey(*privKey, *privHeader);
	flip(*privHeader);
	END_IPC(CSP)
}


//
// Key derivation.
// This is a bit strained; the incoming 'param' value may have structure
// and needs to be handled on a per-algorithm basis, which means we have to
// know which key derivation algorithms we support for passing to our CSP(s).
// The default behavior is to handle "flat" data blobs, which is as good
// a default as we can manage.
// NOTE: The param-specific handling must be synchronized with the client library
// code (in sstransit.h).
//
kern_return_t ucsp_server_deriveKey(UCSP_ARGS, DbHandle db, CONTEXT_ARGS, KeyHandle key,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
    COPY_IN(void, paramInputData), DATA_OUT(paramOutput),
	uint32 usage, uint32 attrs, KeyHandle *newKey, CssmKey::Header *newHeader)
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
    
    // munge together the incoming 'param' value according to algorithm
    CssmData param;
    switch (context.algorithm()) {
    case CSSM_ALGID_PKCS5_PBKDF2:
        relocate((CSSM_PKCS5_PBKDF2_PARAMS *)paramInputData,
            (CSSM_PKCS5_PBKDF2_PARAMS *)paramInputDataBase,
            paramInputDataLength);
        param = CssmData(paramInputData, sizeof(CSSM_PKCS5_PBKDF2_PARAMS));
        break;
    default:
        param = CssmData(paramInputData, paramInputDataLength);
        break;
    }
    Key &theKey = connection.deriveKey(Server::optionalDatabase(db),
		context, Server::optionalKey(key), cred, owner, &param, usage, attrs);
    theKey.returnKey(*newKey, *newHeader);
	flip(*newHeader);
    if (param.length()) {
        if (!param)	// CSP screwed up
            CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
        if (paramInputDataLength)		// using incoming buffer; make a copy
            param = CssmAutoData(Server::csp().allocator(), param).release();
        OutputData(paramOutput, paramOutputLength) = param;	// return the data
    }
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
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase, credLength);
    connection.wrapKey(context, Server::optionalKey(key),
        Server::key(keyToBeWrapped), cred, DATA(descriptiveData), *wrappedKey);
    // transmit key data back as a separate blob
	*keyData = wrappedKey->data();
	*keyDataLength = wrappedKey->length();
	Server::releaseWhenDone(*keyData);
	flip(*wrappedKey);
	END_IPC(CSP)
}

kern_return_t ucsp_server_unwrapKey(UCSP_ARGS, DbHandle db, CONTEXT_ARGS, KeyHandle key,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	KeyHandle publicKey, CssmKey wrappedKey, DATA_IN(wrappedKeyData),
	uint32 usage, uint32 attr, DATA_OUT(descriptiveData),
    KeyHandle *newKey, CssmKey::Header *newHeader)
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	flip(wrappedKey);
	wrappedKey.KeyData = DATA(wrappedKeyData);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
	CssmData descriptiveDatas;
    Key &theKey = connection.unwrapKey(Server::optionalDatabase(db),
		context, Server::optionalKey(key), cred, owner, usage, attr, wrappedKey,
        Server::optionalKey(publicKey), &descriptiveDatas);
    theKey.returnKey(*newKey, *newHeader);
	flip(*newHeader);
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
	*ownerOutLength = owners.length();
	flips(owners.value(), ownerOut, ownerOutBase);
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
		
		// release the memory allocated for the list itself when we are done
		CssmAllocator::standard().free (aclList);
	}
	
	// set result (note: this is *almost* flips(), but on an array)
	*aclsLength = aclsOut.length();
	*acls = *aclsBase = aclsOut;
	if (flipClient()) {
		FlipWalker w;
		for (uint32 n = 0; n < count; n++)
			walk(w, (*acls)[n]);
		w.doFlips();
		Flippers::flip(*aclsBase);
	}
	Server::releaseWhenDone(aclsOut.keep());
	END_IPC(CSP)
}

kern_return_t ucsp_server_changeAcl(UCSP_ARGS, AclKind kind, KeyHandle key,
	COPY_IN(AccessCredentials, cred), CSSM_ACL_EDIT_MODE mode, CSSM_ACL_HANDLE handle,
	COPY_IN(AclEntryInput, acl))
{
	BEGIN_IPC
    relocate(cred, credBase, credLength);
	relocate(acl, aclBase, aclLength);
	Server::aclBearer(kind, key).cssmChangeAcl(AclEdit(mode, handle, acl), cred);
	END_IPC(CSP)
}


//
// Database key management.
// ExtractMasterKey looks vaguely like a key derivation operation, and is in fact
// presented by the CSPDL's CSSM layer as such.
//
kern_return_t ucsp_server_extractMasterKey(UCSP_ARGS, DbHandle db, CONTEXT_ARGS, DbHandle sourceDb,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	uint32 usage, uint32 attrs, KeyHandle *newKey, CssmKey::Header *newHeader)
{
	BEGIN_IPC
	context.postIPC(contextBase, attributes);
    relocate(cred, credBase, credLength);
	relocate(owner, ownerBase, ownerLength);
    Key *masterKey = Server::database(sourceDb).extractMasterKey(Server::optionalDatabase(db),
		cred, owner, usage, attrs);
	masterKey->returnKey(*newKey, *newHeader);
	flip(*newHeader);
	END_IPC(CSP)
}


//
// Authorization subsystem support
//
kern_return_t ucsp_server_authorizationCreate(UCSP_ARGS,
	COPY_IN(AuthorizationItemSet, inRights),
	uint32 flags,
	COPY_IN(AuthorizationItemSet, inEnvironment),
	AuthorizationBlob *authorization)
{
	BEGIN_IPC
	relocate(inRights, inRightsBase, inRightsLength);
	relocate(inEnvironment, inEnvironmentBase, inEnvironmentLength);
	Authorization::AuthItemSet rights(inRights), environment(inEnvironment);

	*rcode = connection.process.session.authCreate(rights, environment, 
		flags, *authorization, securityToken);
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
	COPY_IN(AuthorizationItemSet, inRights),
	uint32 flags,
	COPY_IN(AuthorizationItemSet, inEnvironment),
	COPY_OUT(AuthorizationItemSet, result))
{
	BEGIN_IPC
	relocate(inRights, inRightsBase, inRightsLength);
	relocate(inEnvironment, inEnvironmentBase, inEnvironmentLength);
	Authorization::AuthItemSet rights(inRights), environment(inEnvironment), grantedRights;
	*rcode = connection.process.session.authGetRights(authorization,
		rights, environment, flags, grantedRights);
	if (result && resultLength)
	{
		size_t resultSize;
		grantedRights.copy(*result, resultSize);
		*resultLength = resultSize;
		*resultBase = *result;
		flips(*result, result, resultBase);
		Server::releaseWhenDone(*result);
	}
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationCopyInfo(UCSP_ARGS,
	AuthorizationBlob authorization,
	AuthorizationString tag,
	COPY_OUT(AuthorizationItemSet, info))
{
	BEGIN_IPC
    Authorization::AuthItemSet infoSet;
    *info = *infoBase = NULL;
    *infoLength = 0;
    *rcode = connection.process.session.authGetInfo(authorization,
        tag[0] ? tag : NULL, infoSet);
    if (*rcode == noErr) {
		size_t infoSize;
		infoSet.copy(*info, infoSize);
		*infoLength = infoSize;
		*infoBase = *info;
		flips(*info, info, infoBase);
        Server::releaseWhenDone(*info);
    }
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


//
// Notification core subsystem
//
kern_return_t ucsp_server_requestNotification(UCSP_ARGS, mach_port_t receiver, uint32 domain, uint32 events)
{
    BEGIN_IPC
    connection.process.requestNotifications(receiver, domain, events);
    END_IPC(CSSM)
}

kern_return_t ucsp_server_stopNotification(UCSP_ARGS, mach_port_t receiver)
{
    BEGIN_IPC
    connection.process.stopNotifications(receiver);
    END_IPC(CSSM)
}

kern_return_t ucsp_server_postNotification(UCSP_ARGS, uint32 domain, uint32 event, DATA_IN(data))
{
    BEGIN_IPC
    connection.process.postNotification(domain, event, DATA(data));
    END_IPC(CSSM)
}


//
// AuthorizationDB modification
//
kern_return_t ucsp_server_authorizationdbGet(UCSP_ARGS, const char *rightname, DATA_OUT(rightDefinition))
{
	BEGIN_IPC
	CFDictionaryRef rightDict;

	*rcode = connection.process.session.authorizationdbGet(rightname, &rightDict);

	if (!*rcode && rightDict)
	{
		CFRef<CFDataRef> data(CFPropertyListCreateXMLData (NULL, rightDict));
		CFRelease(rightDict);
		if (!data)
			return errAuthorizationInternal;
	
		// @@@ copy data to avoid having to do a delayed cfrelease
		mach_msg_type_number_t length = CFDataGetLength(data);
		void *xmlData = CssmAllocator::standard().malloc(length);
		memcpy(xmlData, CFDataGetBytePtr(data), length);
		Server::releaseWhenDone(xmlData);
	
		*rightDefinition = xmlData;
		*rightDefinitionLength = length;
	}
	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationdbSet(UCSP_ARGS, AuthorizationBlob authorization, const char *rightname, DATA_IN(rightDefinition))
{
	BEGIN_IPC
	CFRef<CFDataRef> data(CFDataCreate(NULL, (UInt8 *)rightDefinition, rightDefinitionLength));

	if (!data)
		return errAuthorizationInternal;

	CFRef<CFDictionaryRef> rightDefinition(static_cast<CFDictionaryRef>(CFPropertyListCreateFromXMLData(NULL, data, kCFPropertyListImmutable, NULL)));

	if (!rightDefinition || (CFGetTypeID(rightDefinition) != CFDictionaryGetTypeID()))
		return errAuthorizationInternal;

	*rcode = connection.process.session.authorizationdbSet(authorization, rightname, rightDefinition);

	END_IPC(CSSM)
}

kern_return_t ucsp_server_authorizationdbRemove(UCSP_ARGS, AuthorizationBlob authorization, const char *rightname)
{
	BEGIN_IPC
	*rcode = connection.process.session.authorizationdbRemove(authorization, rightname);
	END_IPC(CSSM)
}


//
// Miscellaneous administrative functions
//
kern_return_t ucsp_server_addCodeEquivalence(UCSP_ARGS, DATA_IN(oldHash), DATA_IN(newHash),
	const char *name, boolean_t forSystem)
{
	BEGIN_IPC
	Server::codeSignatures().addLink(DATA(oldHash), DATA(newHash), name, forSystem);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_removeCodeEquivalence(UCSP_ARGS, DATA_IN(hash),
	const char *name, boolean_t forSystem)
{
	BEGIN_IPC
	Server::codeSignatures().removeLink(DATA(hash), name, forSystem);
	END_IPC(CSSM)
}

kern_return_t ucsp_server_setAlternateSystemRoot(UCSP_ARGS, const char *root)
{
	BEGIN_IPC
#if defined(NDEBUG)
	if (connection.process.uid() != 0)
		CssmError::throwMe(CSSM_ERRCODE_OS_ACCESS_DENIED);
#endif //NDEBUG
	Server::codeSignatures().open((string(root) + EQUIVALENCEDBPATH).c_str());
	END_IPC(CSSM)
}
