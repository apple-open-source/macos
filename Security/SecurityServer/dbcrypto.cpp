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
// dbcrypto - cryptographic core for database and key blob cryptography
//
#include "dbcrypto.h"
#include "ssblob.h"
#include "server.h"		// just for Server::csp()
#include <Security/genkey.h>
#include <Security/cryptoclient.h>
#include <Security/keyclient.h>
#include <Security/macclient.h>
#include <Security/wrapkey.h>


using namespace CssmClient;


DatabaseCryptoCore::DatabaseCryptoCore() : mIsValid(false)
{
}


DatabaseCryptoCore::~DatabaseCryptoCore()
{
    // key objects take care of themselves
}


//
// Generate new secrets for this crypto core.
//
void DatabaseCryptoCore::generateNewSecrets()
{
    // create a random DES3 key
    GenerateKey desGenerator(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE, 24 * 8);
    encryptionKey = desGenerator(KeySpec(CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP,
        CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE));
    
    // create a random 20 byte HMAC1/SHA1 signing "key"
    GenerateKey signGenerator(Server::csp(), CSSM_ALGID_SHA1HMAC,
        sizeof(DbBlob::PrivateBlob::SigningKey) * 8);
    signingKey = signGenerator(KeySpec(CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY,
        CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE));
    
    // secrets established
    mIsValid = true;
}


//
// Encode a database blob from the core.
//
DbBlob *DatabaseCryptoCore::encodeCore(const DbBlob &blobTemplate,
    const CssmData &passphrase,
    const CssmData &publicAcl, const CssmData &privateAcl) const
{
    assert(isValid());		// must have secrets to work from

    // make a new salt and IV
    uint8 salt[20];
    Server::active().random(salt);
    uint8 iv[8];
    Server::active().random(iv);
    
    // derive blob encryption key
    CssmClient::Key blobCryptKey = deriveDbCryptoKey(passphrase,
        CssmData(salt, sizeof(salt)));
    
    // build the encrypted section blob
    CssmData &encryptionBits = *encryptionKey;
    CssmData &signingBits = *signingKey;
    CssmData incrypt[3];
    incrypt[0] = encryptionBits;
    incrypt[1] = signingBits;
    incrypt[2] = privateAcl;
    CssmData cryptoBlob, remData;
    Encrypt cryptor(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE);
    cryptor.mode(CSSM_ALGMODE_CBCPadIV8);
    cryptor.padding(CSSM_PADDING_PKCS1);
    cryptor.key(blobCryptKey);
    CssmData ivd(iv, sizeof(iv)); cryptor.initVector(ivd);
    cryptor.encrypt(incrypt, 3, &cryptoBlob, 1, remData);
    
    // allocate the final DbBlob, uh, blob
    size_t length = sizeof(DbBlob) + publicAcl.length() + cryptoBlob.length();
    DbBlob *blob = CssmAllocator::standard().malloc<DbBlob>(length);
    
    // assemble the DbBlob
    memset(blob, 0x7d, sizeof(DbBlob));	// deterministically fill any alignment gaps
    blob->initialize();
    blob->randomSignature = blobTemplate.randomSignature;
    blob->sequence = blobTemplate.sequence;
    blob->params = blobTemplate.params;
    memcpy(blob->salt, salt, sizeof(salt));
    memcpy(blob->iv, iv, sizeof(iv));
    memcpy(blob->publicAclBlob(), publicAcl, publicAcl.length());
    blob->startCryptoBlob = sizeof(DbBlob) + publicAcl.length();
    memcpy(blob->cryptoBlob(), cryptoBlob, cryptoBlob.length());
    blob->totalLength = blob->startCryptoBlob + cryptoBlob.length();
    
    // sign the blob
    CssmData signChunk[] = {
		CssmData(blob->data(), offsetof(DbBlob, blobSignature)),
		CssmData(blob->publicAclBlob(), publicAcl.length() + cryptoBlob.length())
	};
    CssmData signature(blob->blobSignature, sizeof(blob->blobSignature));
    GenerateMac signer(Server::csp(), CSSM_ALGID_SHA1HMAC_LEGACY);	//@@@!!! CRUD
    signer.key(signingKey);
    signer.sign(signChunk, 2, signature);
    assert(signature.length() == sizeof(blob->blobSignature));
    
    // all done. Clean up
    Server::csp()->allocator().free(cryptoBlob);
    return blob;
}


//
// Decode a database blob into the core.
// Returns false if the decoding fails.
//
void DatabaseCryptoCore::decodeCore(DbBlob *blob, const CssmData &passphrase,
    void **privateAclBlob)
{
    // derive blob encryption key
    CssmClient::Key blobCryptKey = deriveDbCryptoKey(passphrase,
        CssmData(blob->salt, sizeof(blob->salt)));
    
    // try to decrypt the cryptoblob section
    Decrypt decryptor(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE);
    decryptor.mode(CSSM_ALGMODE_CBCPadIV8);
    decryptor.padding(CSSM_PADDING_PKCS1);
    decryptor.key(blobCryptKey);
    CssmData ivd(blob->iv, sizeof(blob->iv)); decryptor.initVector(ivd);
    CssmData cryptoBlob(blob->cryptoBlob(), blob->cryptoBlobLength());
    CssmData decryptedBlob, remData;
    decryptor.decrypt(cryptoBlob, decryptedBlob, remData);
    DbBlob::PrivateBlob *privateBlob = decryptedBlob.interpretedAs<DbBlob::PrivateBlob>();
    
    // tentatively establish keys
    CssmClient::Key encryptionKey = makeRawKey(privateBlob->encryptionKey,
        sizeof(privateBlob->encryptionKey), CSSM_ALGID_3DES_3KEY_EDE,
        CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP);
    CssmClient::Key signingKey = makeRawKey(privateBlob->signingKey,
        sizeof(privateBlob->signingKey), CSSM_ALGID_SHA1HMAC,
        CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY);
    
    // verify signature on the whole blob
    CssmData signChunk[] = {
		CssmData(blob->data(), offsetof(DbBlob, blobSignature)),
    	CssmData(blob->publicAclBlob(), blob->publicAclBlobLength() + blob->cryptoBlobLength())
	};
    CSSM_ALGORITHMS verifyAlgorithm = CSSM_ALGID_SHA1HMAC;
#if defined(COMPAT_OSX_10_0)
    if (blob->version == blob->version_MacOS_10_0)
        verifyAlgorithm = CSSM_ALGID_SHA1HMAC_LEGACY;	// BSafe bug compatibility
#endif
    VerifyMac verifier(Server::csp(), verifyAlgorithm);
    verifier.key(signingKey);
    verifier.verify(signChunk, 2, CssmData(blob->blobSignature, sizeof(blob->blobSignature)));
    
    // all checks out; start extracting fields
    this->encryptionKey = encryptionKey;
    this->signingKey = signingKey;
    if (privateAclBlob) {
        // extract private ACL blob as a separately allocated area
        uint32 blobLength = decryptedBlob.length() - sizeof(DbBlob::PrivateBlob);
        *privateAclBlob = CssmAllocator::standard().malloc(blobLength);
        memcpy(*privateAclBlob, privateBlob->privateAclBlob(), blobLength);
    }
        
    // secrets have been established
    mIsValid = true;
    CssmAllocator::standard().free(privateBlob);
}


//
// Encode a key blob
//
KeyBlob *DatabaseCryptoCore::encodeKeyCore(const CssmKey &inKey,
    const CssmData &publicAcl, const CssmData &privateAcl) const
{
    assert(isValid());		// need our database secrets
    
    // create new IV
    uint8 iv[8];
    Server::active().random(iv);
    
    // extract and hold some header bits the CSP does not want to see
    CssmKey key = inKey;
    uint32 heldAttributes = key.attributes() & managedAttributes;
    key.clearAttribute(managedAttributes);
    
    // use a CMS wrap to encrypt the key
    WrapKey wrap(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE);
    wrap.key(encryptionKey);
    wrap.mode(CSSM_ALGMODE_CBCPadIV8);
    wrap.padding(CSSM_PADDING_PKCS1);
    CssmData ivd(iv, sizeof(iv)); wrap.initVector(ivd);
    wrap.add(CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT,
        uint32(CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM));
    CssmKey wrappedKey;
    wrap(key, wrappedKey, &privateAcl);
    
    // stick the held attribute bits back in
    key.setAttribute(heldAttributes);
    
    // allocate the final KeyBlob, uh, blob
    size_t length = sizeof(KeyBlob) + publicAcl.length() + wrappedKey.length();
    KeyBlob *blob = CssmAllocator::standard().malloc<KeyBlob>(length);
    
    // assemble the KeyBlob
    memset(blob, 0, sizeof(KeyBlob));	// fill alignment gaps
    blob->initialize();
    memcpy(blob->iv, iv, sizeof(iv));
    blob->header = key.header();
    blob->wrappedHeader.blobType = wrappedKey.blobType();
    blob->wrappedHeader.blobFormat = wrappedKey.blobFormat();
    blob->wrappedHeader.wrapAlgorithm = wrappedKey.wrapAlgorithm();
    blob->wrappedHeader.wrapMode = wrappedKey.wrapMode();
    memcpy(blob->publicAclBlob(), publicAcl, publicAcl.length());
    blob->startCryptoBlob = sizeof(KeyBlob) + publicAcl.length();
    memcpy(blob->cryptoBlob(), wrappedKey.data(), wrappedKey.length());
    blob->totalLength = blob->startCryptoBlob + wrappedKey.length();
    
    // sign the blob
    CssmData signChunk[] = {
		CssmData(blob->data(), offsetof(KeyBlob, blobSignature)),
    	CssmData(blob->publicAclBlob(), blob->publicAclBlobLength() + blob->cryptoBlobLength())
	};
    CssmData signature(blob->blobSignature, sizeof(blob->blobSignature));
    GenerateMac signer(Server::csp(), CSSM_ALGID_SHA1HMAC_LEGACY);	//@@@!!! CRUD
    signer.key(signingKey);
    signer.sign(signChunk, 2, signature);
    assert(signature.length() == sizeof(blob->blobSignature));
    
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
    assert(isValid());		// need our database secrets
    
    // Assemble the encrypted blob as a CSSM "wrapped key"
    CssmKey wrappedKey;
    wrappedKey.KeyHeader = blob->header;
    wrappedKey.blobType(blob->wrappedHeader.blobType);
    wrappedKey.blobFormat(blob->wrappedHeader.blobFormat);
    wrappedKey.wrapAlgorithm(blob->wrappedHeader.wrapAlgorithm);
    wrappedKey.wrapMode(blob->wrappedHeader.wrapMode);
    wrappedKey.KeyData = CssmData(blob->cryptoBlob(), blob->cryptoBlobLength());

    // verify signature (check against corruption)
    CssmData signChunk[] = {
    	CssmData::wrap(blob, offsetof(KeyBlob, blobSignature)),
    	CssmData(blob->publicAclBlob(), blob->publicAclBlobLength() + blob->cryptoBlobLength())
	};
    CSSM_ALGORITHMS verifyAlgorithm = CSSM_ALGID_SHA1HMAC;
#if defined(COMPAT_OSX_10_0)
    if (blob->version == blob->version_MacOS_10_0)
        verifyAlgorithm = CSSM_ALGID_SHA1HMAC_LEGACY;	// BSafe bug compatibility
#endif
    VerifyMac verifier(Server::csp(), verifyAlgorithm);
    verifier.key(signingKey);
    CssmData signature(blob->blobSignature, sizeof(blob->blobSignature));
    verifier.verify(signChunk, 2, signature);
    
    // extract and hold some header bits the CSP does not want to see
    uint32 heldAttributes = blob->header.attributes() & managedAttributes;
   
    // decrypt the key using an unwrapping operation
    UnwrapKey unwrap(Server::csp(), CSSM_ALGID_3DES_3KEY_EDE);
    unwrap.key(encryptionKey);
    unwrap.mode(CSSM_ALGMODE_CBCPadIV8);
    unwrap.padding(CSSM_PADDING_PKCS1);
    CssmData ivd(blob->iv, sizeof(blob->iv)); unwrap.initVector(ivd);
    unwrap.add(CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT,
        uint32(CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM));
    CssmData privAclData;
    wrappedKey.clearAttribute(managedAttributes);    //@@@ shouldn't be needed(?)
    unwrap(wrappedKey,
        KeySpec(blob->header.usage(), blob->header.attributes() & ~managedAttributes),
        key, &privAclData);
    
    // compare retrieved key headers with blob headers (sanity check)
    // @@@ this should probably be checked over carefully
    CssmKey::Header &real = key.header();
    CssmKey::Header &incoming = blob->header;
    if (real.HeaderVersion != incoming.HeaderVersion ||
        real.cspGuid() != incoming.cspGuid() ||
        real.blobFormat() != incoming.blobFormat())
        CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
    if (real.algorithm() != incoming.algorithm())
        CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
        
    // re-insert held bits
    key.header().KeyAttr |= heldAttributes;
    
    // got a valid key: return the pieces
    pubAcl = blob->publicAclBlob();		// points into blob (shared)
    privAcl = privAclData;				// was allocated by CSP decrypt
    // key was set by unwrap operation
}


//
// Derive the blob-specific database blob encryption key from the passphrase and the salt.
//
CssmClient::Key DatabaseCryptoCore::deriveDbCryptoKey(const CssmData &passphrase,
    const CssmData &salt) const
{
    // derive an encryption key and IV from passphrase and salt
    CssmClient::DeriveKey makeKey(Server::csp(),
        CSSM_ALGID_PKCS5_PBKDF2, CSSM_ALGID_3DES_3KEY_EDE, 24 * 8);
    makeKey.iterationCount(1000);
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
