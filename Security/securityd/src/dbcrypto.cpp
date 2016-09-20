/*
 * Copyright (c) 2000-2006,2013 Apple Inc. All Rights Reserved.
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
// dbcrypto - cryptographic core for database and key blob cryptography
//
#include "dbcrypto.h"
#include <security_utilities/casts.h>
#include <securityd_client/ssblob.h>
#include "server.h"		// just for Server::csp()
#include <security_cdsa_client/genkey.h>
#include <security_cdsa_client/cryptoclient.h>
#include <security_cdsa_client/keyclient.h>
#include <security_cdsa_client/macclient.h>
#include <security_cdsa_client/wrapkey.h>
#include <security_cdsa_utilities/cssmendian.h>

using namespace CssmClient;
using LowLevelMemoryUtilities::fieldOffsetOf;


//
// The CryptoCore constructor doesn't do anything interesting.
// It just initializes us to "empty".
//
DatabaseCryptoCore::DatabaseCryptoCore(uint32 requestedVersion) : mBlobVersion(CommonBlob::version_MacOS_10_0), mHaveMaster(false), mIsValid(false)
{
    // If there's a specific version our callers want, give them that. Otherwise, ask CommonBlob what to do.
    if(requestedVersion == CommonBlob::version_none) {
        mBlobVersion = CommonBlob::getCurrentVersion();
    } else {
        mBlobVersion = requestedVersion;
    }
}

DatabaseCryptoCore::~DatabaseCryptoCore()
{
    // key objects take care of themselves
}


//
// Forget the secrets
//
void DatabaseCryptoCore::invalidate()
{
	mMasterKey.release();
	mHaveMaster = false;
	
	mEncryptionKey.release();
	mSigningKey.release();
	mIsValid = false;
}

//
// Copy everything from another databasecryptocore
//
void DatabaseCryptoCore::initializeFrom(DatabaseCryptoCore& core, uint32 requestedVersion) {
    if(core.hasMaster()) {
        mMasterKey = core.mMasterKey;
        memcpy(mSalt, core.mSalt, sizeof(mSalt));
        mHaveMaster = core.mHaveMaster;
    } else {
        mHaveMaster = false;
    }

    if(core.isValid()) {
        importSecrets(core);
    } else {
        mIsValid = false;
    }

    // As the last thing we do, check if we should be changing the version of this blob.
    if(requestedVersion == CommonBlob::version_none) {
        mBlobVersion = core.mBlobVersion;
    } else {
        mBlobVersion = requestedVersion;
    }
}

//
// Generate new secrets for this crypto core.
//
void DatabaseCryptoCore::generateNewSecrets()
{
    // create a random DES3 key
    GenerateKey desGenerator(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE, 24 * 8);
    mEncryptionKey = desGenerator(KeySpec(CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP,
        CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE));
    
    // create a random 20 byte HMAC/SHA1 signing "key"
    GenerateKey signGenerator(Server::csp(), CSSM_ALGID_SHA1HMAC,
        sizeof(DbBlob::PrivateBlob::SigningKey) * 8);
    mSigningKey = signGenerator(KeySpec(CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY,
        CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE));
    
    // secrets established
    mIsValid = true;
}


CssmClient::Key DatabaseCryptoCore::masterKey()
{
	assert(mHaveMaster);
	return mMasterKey;
}


//
// Establish the master secret as derived from a passphrase passed in.
// If a DbBlob is passed, take the salt from it and remember it.
// If a NULL DbBlob is passed, generate a new (random) salt.
// Note that the passphrase is NOT remembered; only the master key.
//
void DatabaseCryptoCore::setup(const DbBlob *blob, const CssmData &passphrase, bool copyVersion /* = true */)
{
    if (blob) {
        if(copyVersion) {
            mBlobVersion = blob->version();
        }
        memcpy(mSalt, blob->salt, sizeof(mSalt));
    } else
		Server::active().random(mSalt);
    mMasterKey = deriveDbMasterKey(passphrase);
	mHaveMaster = true;
}


//
// Establish the master secret directly from a master key passed in.
// We will copy the KeyData (caller still owns its copy).
// Blob/salt handling as above.
//
void DatabaseCryptoCore::setup(const DbBlob *blob, CssmClient::Key master, bool copyVersion /* = true */)
{
	// pre-screen the key
	CssmKey::Header header = master.header();
	if (header.keyClass() != CSSM_KEYCLASS_SESSION_KEY)
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	if (header.algorithm() != CSSM_ALGID_3DES_3KEY_EDE)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	
	// accept it
    if (blob) {
        if(copyVersion) {
            mBlobVersion = blob->version();
        }
        memcpy(mSalt, blob->salt, sizeof(mSalt));
    } else
		Server::active().random(mSalt);
	mMasterKey = master;
	mHaveMaster = true;
}

bool DatabaseCryptoCore::get_encryption_key(CssmOwnedData &data)
{
    bool result = false;
    if (isValid()) {
        data = mEncryptionKey->keyData();
        result = true;
    }
    return result;
}

//
// Given a putative passphrase, determine whether that passphrase
// properly generates the database's master secret.
// Return a boolean accordingly. Do not change our state.
// The database must have a master secret (to compare with).
// Note that any errors thrown by the cryptography here will actually
// throw out of validatePassphrase, since they "should not happen" and
// thus indicate a problem *beyond* (just) a bad passphrase.
//
bool DatabaseCryptoCore::validatePassphrase(const CssmData &passphrase)
{
	CssmClient::Key master = deriveDbMasterKey(passphrase);
    return validateKey(master);
}

bool DatabaseCryptoCore::validateKey(const CssmClient::Key& master) {
    assert(hasMaster());
	// to compare master with mMaster, see if they encrypt alike
	StringData probe
		("Now is the time for all good processes to come to the aid of their kernel.");
	CssmData noRemainder((void *)1, 0);	// no cipher overflow
	Encrypt cryptor(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE);
	cryptor.mode(CSSM_ALGMODE_CBCPadIV8);
	cryptor.padding(CSSM_PADDING_PKCS1);
	uint8 iv[8];	// leave uninitialized; pseudo-random is cool
	cryptor.initVector(CssmData::wrap(iv));
	
	cryptor.key(master);
	CssmAutoData cipher1(Server::csp().allocator());
	cryptor.encrypt(probe, cipher1.get(), noRemainder);
	
	cryptor.key(mMasterKey);
	CssmAutoData cipher2(Server::csp().allocator());
	cryptor.encrypt(probe, cipher2.get(), noRemainder);
	
	return cipher1 == cipher2;
}


//
// Encode a database blob from the core.
//
DbBlob *DatabaseCryptoCore::encodeCore(const DbBlob &blobTemplate,
    const CssmData &publicAcl, const CssmData &privateAcl) const
{
    assert(isValid());		// must have secrets to work from

    // make a new IV
    uint8 iv[8];
    Server::active().random(iv);
    
    // build the encrypted section blob
    CssmData &encryptionBits = *mEncryptionKey;
    CssmData &signingBits = *mSigningKey;
    CssmData incrypt[3];
    incrypt[0] = encryptionBits;
    incrypt[1] = signingBits;
    incrypt[2] = privateAcl;
    CssmData cryptoBlob, remData;
    Encrypt cryptor(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE);
    cryptor.mode(CSSM_ALGMODE_CBCPadIV8);
    cryptor.padding(CSSM_PADDING_PKCS1);
    cryptor.key(mMasterKey);
    CssmData ivd(iv, sizeof(iv)); cryptor.initVector(ivd);
    cryptor.encrypt(incrypt, 3, &cryptoBlob, 1, remData);
    
    // allocate the final DbBlob, uh, blob
    size_t length = sizeof(DbBlob) + publicAcl.length() + cryptoBlob.length();
    DbBlob *blob = Allocator::standard().malloc<DbBlob>(length);
    
    // assemble the DbBlob
    memset(blob, 0x7d, sizeof(DbBlob));	// deterministically fill any alignment gaps
    blob->initialize(mBlobVersion);
    blob->randomSignature = blobTemplate.randomSignature;
    blob->sequence = blobTemplate.sequence;
    blob->params = blobTemplate.params;
	memcpy(blob->salt, mSalt, sizeof(blob->salt));
    memcpy(blob->iv, iv, sizeof(iv));
    memcpy(blob->publicAclBlob(), publicAcl, publicAcl.length());
    blob->startCryptoBlob = sizeof(DbBlob) + int_cast<size_t, uint32_t>(publicAcl.length());
    memcpy(blob->cryptoBlob(), cryptoBlob, cryptoBlob.length());
    blob->totalLength = blob->startCryptoBlob + int_cast<size_t, uint32_t>(cryptoBlob.length());
    
    // sign the blob
    CssmData signChunk[] = {
		CssmData(blob->data(), fieldOffsetOf(&DbBlob::blobSignature)),
		CssmData(blob->publicAclBlob(), publicAcl.length() + cryptoBlob.length())
	};
    CssmData signature(blob->blobSignature, sizeof(blob->blobSignature));

    CSSM_ALGORITHMS signingAlgorithm = CSSM_ALGID_SHA1HMAC;
#if defined(COMPAT_OSX_10_0)
    if (blob->version() == blob->version_MacOS_10_0)
        signingAlgorithm = CSSM_ALGID_SHA1HMAC_LEGACY;	// BSafe bug compatibility
#endif
    GenerateMac signer(Server::csp(), signingAlgorithm);
    signer.key(mSigningKey);
    signer.sign(signChunk, 2, signature);
    assert(signature.length() == sizeof(blob->blobSignature));
    
    // all done. Clean up
    Server::csp()->allocator().free(cryptoBlob);
    return blob;
}


//
// Decode a database blob into the core.
// Throws exceptions if decoding fails.
// Memory returned in privateAclBlob is allocated and becomes owned by caller.
//
void DatabaseCryptoCore::decodeCore(const DbBlob *blob, void **privateAclBlob)
{
	assert(mHaveMaster);	// must have master key installed
    
    // try to decrypt the cryptoblob section
    Decrypt decryptor(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE);
    decryptor.mode(CSSM_ALGMODE_CBCPadIV8);
    decryptor.padding(CSSM_PADDING_PKCS1);
    decryptor.key(mMasterKey);
    CssmData ivd = CssmData::wrap(blob->iv); decryptor.initVector(ivd);
    CssmData cryptoBlob = CssmData::wrap(blob->cryptoBlob(), blob->cryptoBlobLength());
    CssmData decryptedBlob, remData;
    decryptor.decrypt(cryptoBlob, decryptedBlob, remData);
    DbBlob::PrivateBlob *privateBlob = decryptedBlob.interpretedAs<DbBlob::PrivateBlob>();
    
    // tentatively establish keys
    mEncryptionKey = makeRawKey(privateBlob->encryptionKey,
        sizeof(privateBlob->encryptionKey), CSSM_ALGID_3DES_3KEY_EDE,
        CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP);
    mSigningKey = makeRawKey(privateBlob->signingKey,
        sizeof(privateBlob->signingKey), CSSM_ALGID_SHA1HMAC,
        CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY);
    
    // verify signature on the whole blob
    CssmData signChunk[] = {
		CssmData::wrap(blob->data(), fieldOffsetOf(&DbBlob::blobSignature)),
    	CssmData::wrap(blob->publicAclBlob(), blob->publicAclBlobLength() + blob->cryptoBlobLength())
	};
    CSSM_ALGORITHMS verifyAlgorithm = CSSM_ALGID_SHA1HMAC;
#if defined(COMPAT_OSX_10_0)
    if (blob->version() == blob->version_MacOS_10_0)
        verifyAlgorithm = CSSM_ALGID_SHA1HMAC_LEGACY;	// BSafe bug compatibility
#endif
    VerifyMac verifier(Server::csp(), verifyAlgorithm);
    verifier.key(mSigningKey);
    verifier.verify(signChunk, 2, CssmData::wrap(blob->blobSignature));
    
    // all checks out; start extracting fields
    if (privateAclBlob) {
        // extract private ACL blob as a separately allocated area
        uint32 blobLength = (uint32) decryptedBlob.length() - sizeof(DbBlob::PrivateBlob);
        *privateAclBlob = Allocator::standard().malloc(blobLength);
        memcpy(*privateAclBlob, privateBlob->privateAclBlob(), blobLength);
    }
        
    // secrets have been established
    mBlobVersion = blob->version();
    mIsValid = true;
    Allocator::standard().free(privateBlob);
}


//
// Make another DatabaseCryptoCore's operational secrets our own.  
// Intended for keychain synchronization.  
//
void DatabaseCryptoCore::importSecrets(const DatabaseCryptoCore &src)
{
	assert(src.isValid());	// must have called src.decodeCore() first
	assert(hasMaster());
	mEncryptionKey = src.mEncryptionKey;
	mSigningKey = src.mSigningKey;
    mBlobVersion = src.mBlobVersion;    // make sure we copy over all state
    mIsValid = true;
}

//
// Encode a key blob
//
KeyBlob *DatabaseCryptoCore::encodeKeyCore(const CssmKey &inKey,
    const CssmData &publicAcl, const CssmData &privateAcl,
	bool inTheClear) const
{
    CssmKey key = inKey;
	uint8 iv[8];
	CssmKey wrappedKey;

	if(inTheClear && (privateAcl.Length != 0)) {
		/* can't store private ACL component in the clear */
		CssmError::throwMe(CSSMERR_DL_INVALID_ACCESS_CREDENTIALS);
	}
	
    // extract and hold some header bits the CSP does not want to see
    uint32 heldAttributes = key.attributes() & managedAttributes;
    key.clearAttribute(managedAttributes);
	key.setAttribute(forcedAttributes);
    
	if(inTheClear) {
		/* NULL wrap of public key */
		WrapKey wrap(Server::csp(), CSSM_ALGID_NONE);
		wrap(key, wrappedKey, NULL);
	}
	else {
		assert(isValid());		// need our database secrets
		
		// create new IV
		Server::active().random(iv);
		
	   // use a CMS wrap to encrypt the key
		WrapKey wrap(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE);
		wrap.key(mEncryptionKey);
		wrap.mode(CSSM_ALGMODE_CBCPadIV8);
		wrap.padding(CSSM_PADDING_PKCS1);
		CssmData ivd(iv, sizeof(iv)); wrap.initVector(ivd);
		wrap.add(CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT,
			uint32(CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM));
		wrap(key, wrappedKey, &privateAcl);
    }
	
    // stick the held attribute bits back in
	key.clearAttribute(forcedAttributes);
    key.setAttribute(heldAttributes);
    
    // allocate the final KeyBlob, uh, blob
    size_t length = sizeof(KeyBlob) + publicAcl.length() + wrappedKey.length();
    KeyBlob *blob = Allocator::standard().malloc<KeyBlob>(length);
    
    // assemble the KeyBlob
    memset(blob, 0, sizeof(KeyBlob));	// fill alignment gaps
    blob->initialize(mBlobVersion);
	if(!inTheClear) {
		memcpy(blob->iv, iv, sizeof(iv));
	}
    blob->header = key.header();
	h2ni(blob->header);	// endian-correct the header
    blob->wrappedHeader.blobType = wrappedKey.blobType();
    blob->wrappedHeader.blobFormat = wrappedKey.blobFormat();
    blob->wrappedHeader.wrapAlgorithm = wrappedKey.wrapAlgorithm();
    blob->wrappedHeader.wrapMode = wrappedKey.wrapMode();
    memcpy(blob->publicAclBlob(), publicAcl, publicAcl.length());
    blob->startCryptoBlob = sizeof(KeyBlob) + int_cast<size_t, uint32_t>(publicAcl.length());
    memcpy(blob->cryptoBlob(), wrappedKey.data(), wrappedKey.length());
    blob->totalLength = blob->startCryptoBlob + int_cast<size_t, uint32_t>(wrappedKey.length());
    
 	if(inTheClear) {
		/* indicate that this is cleartext for decoding */
		blob->setClearTextSignature();
	}
	else {
		// sign the blob
		CssmData signChunk[] = {
			CssmData(blob->data(), fieldOffsetOf(&KeyBlob::blobSignature)),
			CssmData(blob->publicAclBlob(), blob->publicAclBlobLength() + blob->cryptoBlobLength())
		};
		CssmData signature(blob->blobSignature, sizeof(blob->blobSignature));

        CSSM_ALGORITHMS signingAlgorithm = CSSM_ALGID_SHA1HMAC;
#if defined(COMPAT_OSX_10_0)
        if (blob->version() == blob->version_MacOS_10_0)
            signingAlgorithm = CSSM_ALGID_SHA1HMAC_LEGACY;	// BSafe bug compatibility
#endif
        GenerateMac signer(Server::csp(), signingAlgorithm);
		signer.key(mSigningKey);
		signer.sign(signChunk, 2, signature);
		assert(signature.length() == sizeof(blob->blobSignature));
    }
	
    // all done. Clean up
    Server::csp()->allocator().free(wrappedKey);
    return blob;
}


//
// Decode a key blob
//
void DatabaseCryptoCore::decodeKeyCore(KeyBlob *blob,
    CssmKey &key, void * &pubAcl, void * &privAcl) const
{    
    // Note that we can't do anything with this key's version().

    // Assemble the encrypted blob as a CSSM "wrapped key"
    CssmKey wrappedKey;
    wrappedKey.KeyHeader = blob->header;
	h2ni(wrappedKey.KeyHeader);
    wrappedKey.blobType(blob->wrappedHeader.blobType);
    wrappedKey.blobFormat(blob->wrappedHeader.blobFormat);
    wrappedKey.wrapAlgorithm(blob->wrappedHeader.wrapAlgorithm);
    wrappedKey.wrapMode(blob->wrappedHeader.wrapMode);
    wrappedKey.KeyData = CssmData(blob->cryptoBlob(), blob->cryptoBlobLength());
	
	bool inTheClear = blob->isClearText();
	if(!inTheClear) {
		// verify signature (check against corruption)
		assert(isValid());		// need our database secrets
		CssmData signChunk[] = {
			CssmData::wrap(blob, fieldOffsetOf(&KeyBlob::blobSignature)),
			CssmData(blob->publicAclBlob(), blob->publicAclBlobLength() + blob->cryptoBlobLength())
		};
		CSSM_ALGORITHMS verifyAlgorithm = CSSM_ALGID_SHA1HMAC;
	#if defined(COMPAT_OSX_10_0)
		if (blob->version() == blob->version_MacOS_10_0)
			verifyAlgorithm = CSSM_ALGID_SHA1HMAC_LEGACY;	// BSafe bug compatibility
	#endif
		VerifyMac verifier(Server::csp(), verifyAlgorithm);
		verifier.key(mSigningKey);
		CssmData signature(blob->blobSignature, sizeof(blob->blobSignature));
		verifier.verify(signChunk, 2, signature);
    }
	/* else signature indicates cleartext */
	
    // extract and hold some header bits the CSP does not want to see
    uint32 heldAttributes = n2h(blob->header.attributes()) & managedAttributes;
   
	CssmData privAclData;
	if(inTheClear) {
		/* NULL unwrap */
		UnwrapKey unwrap(Server::csp(), CSSM_ALGID_NONE);
		wrappedKey.clearAttribute(managedAttributes);    //@@@ shouldn't be needed(?)
		unwrap(wrappedKey,
			KeySpec(n2h(blob->header.usage()),
				(n2h(blob->header.attributes()) & ~managedAttributes) | forcedAttributes),
			key, &privAclData);
	}
	else {
		// decrypt the key using an unwrapping operation
		UnwrapKey unwrap(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE);
		unwrap.key(mEncryptionKey);
		unwrap.mode(CSSM_ALGMODE_CBCPadIV8);
		unwrap.padding(CSSM_PADDING_PKCS1);
		CssmData ivd(blob->iv, sizeof(blob->iv)); unwrap.initVector(ivd);
		unwrap.add(CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT,
			uint32(CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM));
		wrappedKey.clearAttribute(managedAttributes);    //@@@ shouldn't be needed(?)
		unwrap(wrappedKey,
			KeySpec(n2h(blob->header.usage()),
				(n2h(blob->header.attributes()) & ~managedAttributes) | forcedAttributes),
			key, &privAclData);
    }
	
    // compare retrieved key headers with blob headers (sanity check)
    // @@@ this should probably be checked over carefully
    CssmKey::Header &real = key.header();
    CssmKey::Header &incoming = blob->header;
	n2hi(incoming);

    if (real.HeaderVersion != incoming.HeaderVersion ||
        real.cspGuid() != incoming.cspGuid())
        CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
    if (real.algorithm() != incoming.algorithm())
        CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
        
    // re-insert held bits
    key.header().KeyAttr |= heldAttributes;
    
	if(inTheClear && (real.keyClass() != CSSM_KEYCLASS_PUBLIC_KEY)) {
		/* Spoof - cleartext KeyBlob passed off as private key */
        CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	
    // got a valid key: return the pieces
    pubAcl = blob->publicAclBlob();		// points into blob (shared)
    privAcl = privAclData;				// was allocated by CSP decrypt, else NULL for
										// cleatext keys
    // key was set by unwrap operation
}


//
// Derive the blob-specific database blob encryption key from the passphrase and the salt.
//
CssmClient::Key DatabaseCryptoCore::deriveDbMasterKey(const CssmData &passphrase) const
{
    // derive an encryption key and IV from passphrase and salt
    CssmClient::DeriveKey makeKey(Server::csp(),
        CSSM_ALGID_PKCS5_PBKDF2, CSSM_ALGID_3DES_3KEY_EDE, 24 * 8);
    makeKey.iterationCount(1000);
	CssmData salt = CssmData::wrap(mSalt);
    makeKey.salt(salt);
    CSSM_PKCS5_PBKDF2_PARAMS params;
    params.Passphrase = passphrase;
    params.PseudoRandomFunction = CSSM_PKCS5_PBKDF2_PRF_HMAC_SHA1;
	CssmData paramData = CssmData::wrap(params);
    return makeKey(&paramData, KeySpec(CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
        CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE));
}


//
// Turn raw keybits into a symmetric key in the CSP
//
CssmClient::Key DatabaseCryptoCore::makeRawKey(void *data, size_t length,
    CSSM_ALGORITHMS algid, CSSM_KEYUSE usage)
{
    // build a fake key
    CssmKey key;
    key.header().BlobType = CSSM_KEYBLOB_RAW;
    key.header().Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
    key.header().AlgorithmId = algid;
    key.header().KeyClass = CSSM_KEYCLASS_SESSION_KEY;
    key.header().KeyUsage = usage;
    key.header().KeyAttr = 0;
    key.KeyData = CssmData(data, length);
    
    // unwrap it into the CSP (but keep it raw)
    UnwrapKey unwrap(Server::csp(), CSSM_ALGID_NONE);
    CssmKey unwrappedKey;
    CssmData descriptiveData;
    unwrap(key,
        KeySpec(CSSM_KEYUSE_ANY, CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE),
        unwrappedKey, &descriptiveData, NULL);
    return CssmClient::Key(Server::csp(), unwrappedKey);
}
