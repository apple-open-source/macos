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
#ifndef _H_DBCRYPTO
#define _H_DBCRYPTO

#include "securityserver.h"
#include <Security/cspclient.h>
#include <Security/keyclient.h>


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
    
    void generateNewSecrets();

    DbBlob *encodeCore(const DbBlob &blobTemplate, const CssmData &passphrase,
        const CssmData &publicAcl, const CssmData &privateAcl) const;
    void decodeCore(DbBlob *blob, const CssmData &passphrase,
        void **privateAclBlob = NULL);
        
    KeyBlob *encodeKeyCore(const CssmKey &key,
        const CssmData &publicAcl, const CssmData &privateAcl) const;
    void decodeKeyCore(KeyBlob *blob,
        CssmKey &key, void * &pubAcl, void * &privAcl) const;
    
    static const uint32 managedAttributes = KeyBlob::managedAttributes;
	
private:
    bool mIsValid;					// master secrets are valid
    
    CssmClient::Key encryptionKey;	// master encryption key
    CssmClient::Key signingKey;		// master signing key

    CssmClient::Key deriveDbCryptoKey(const CssmData &passphrase, const CssmData &salt) const;
    CssmClient::Key makeRawKey(void *data, size_t length,
        CSSM_ALGORITHMS algid, CSSM_KEYUSE usage);
};


#endif //_H_DBCRYPTO
