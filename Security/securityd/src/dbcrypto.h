/*
 * Copyright (c) 2000-2001,2003-2006,2013 Apple Inc. All Rights Reserved.
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
#ifndef _H_DBCRYPTO
#define _H_DBCRYPTO

#include <securityd_client/ssblob.h>
#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_client/keyclient.h>

using namespace SecurityServer;


//
// A DatabaseCryptoCore object encapsulates the secret state of a database.
// It provides for encoding and decoding of database blobs and key blobs,
// and holds all state related to the database secrets.
//
class DatabaseCryptoCore {
public:
    DatabaseCryptoCore();
	virtual ~DatabaseCryptoCore();
    
    bool isValid() const	{ return mIsValid; }
	bool hasMaster() const	{ return mHaveMaster; }
    void invalidate();

    void generateNewSecrets();
	CssmClient::Key masterKey();

	void setup(const DbBlob *blob, const CssmData &passphrase);
	void setup(const DbBlob *blob, CssmClient::Key master);

    void decodeCore(const DbBlob *blob, void **privateAclBlob = NULL);
    DbBlob *encodeCore(const DbBlob &blobTemplate,
        const CssmData &publicAcl, const CssmData &privateAcl) const;
	void importSecrets(const DatabaseCryptoCore &src);
        
    KeyBlob *encodeKeyCore(const CssmKey &key,
        const CssmData &publicAcl, const CssmData &privateAcl,
		bool inTheClear) const;
    void decodeKeyCore(KeyBlob *blob,
        CssmKey &key, void * &pubAcl, void * &privAcl) const;

    static const uint32 managedAttributes = KeyBlob::managedAttributes;
	static const uint32 forcedAttributes = KeyBlob::forcedAttributes;

    bool get_encryption_key(CssmOwnedData &data);
	
public:
	bool validatePassphrase(const CssmData &passphrase);
	
private:
	bool mHaveMaster;				// master key has been entered (setup)
    bool mIsValid;					// master secrets are valid (decode or generateNew)
    
	CssmClient::Key mMasterKey;		// database master key
	uint8 mSalt[20];				// salt for master key derivation from passphrase (only)
	
    CssmClient::Key mEncryptionKey;	// master encryption key
    CssmClient::Key mSigningKey;	// master signing key

    CssmClient::Key deriveDbMasterKey(const CssmData &passphrase) const;
    CssmClient::Key makeRawKey(void *data, size_t length,
        CSSM_ALGORITHMS algid, CSSM_KEYUSE usage);
};


#endif //_H_DBCRYPTO
