/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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
// testacls - ACL-related test cases.
// 
#include "testclient.h"
#include "testutils.h"


//
// Blob tests.
// Encodes and decodes Db and Key blobs and all that jazz.
//
void blobs()
{
	printf("* Database blob encryption test\n");
	ClientSession ss(CssmAllocator::standard(), CssmAllocator::standard());
	
	DbTester db1(ss, "/tmp/one", NULL, 60, true);
	DbTester db2(ss, "/tmp/two", NULL, 30, false);
	
	// encode db1, purge it, decode it again
	CssmData dbBlob;
	ss.encodeDb(db1, dbBlob);
	DbHandle db1a = ss.decodeDb(db1.dbId, &nullCred, dbBlob);
	ss.releaseDb(db1);
	if (db1 == db1a)
		detail("REUSED DB HANDLE ON DECODEDB (probably wrong)");
	DBParameters savedParams;
	ss.getDbParameters(db1a, savedParams);
	assert(savedParams.idleTimeout == db1.params.idleTimeout);
	assert(savedParams.lockOnSleep == db1.params.lockOnSleep);
	detail("Database encode/decode passed");
	
	// make sure the old handle isn't valid anymore
	try {
		ss.getDbParameters(db1, savedParams);
		printf("OLD DATABASE HANDLE NOT PURGED (possibly wrong)\n");
	} catch (const CssmCommonError &err) {
		detail(err, "old DB handle rejected");
	}
    
    // open db1 a second time (so now there's two db handles for db1)
    DbHandle db1b = ss.decodeDb(db1.dbId, &nullCred, dbBlob);
    
    // release both db1 handles and db2
    ss.releaseDb(db1a);
    ss.releaseDb(db1b);
    ss.releaseDb(db2);
}


//
// Database tests.
// Database locks/unlocks etc.
//
void databases()
{
	printf("* Database manipulation test\n");
    CssmAllocator &alloc = CssmAllocator::standard();
	ClientSession ss(alloc, alloc);
    
    AutoCredentials pwCred(alloc);
    StringData passphrase("two");
    StringData badPassphrase("three");
    pwCred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK,
        new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
        new(alloc) ListElement(passphrase));
    pwCred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
        new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
        new(alloc) ListElement(badPassphrase));
	// pwCred = (NEW: two, OLD: three)
	
	DbTester db1(ss, "/tmp/one", NULL, 30, true);
	DbTester db2(ss, "/tmp/two", &pwCred, 60, false);
	// db2.passphrase = two
	
	// encode db1 and re-open it
	CssmData dbBlob;
	ss.encodeDb(db1, dbBlob);
	DbHandle db1b = ss.decodeDb(db1.dbId, &nullCred, dbBlob);
	if (db1b == db1.dbRef)
		detail("REUSED DB HANDLE ON DECODEDB (probably wrong)");
	
    // open db1 a third time (so now there's three db handles for db1)
    DbHandle db1c = ss.decodeDb(db1.dbId, &nullCred, dbBlob);
    
    // lock them to get started
    ss.lock(db1);
    ss.lock(db2);
    
    // unlock it through user
    prompt("unlock");
    ss.unlock(db1);
	prompt();
    ss.unlock(db1b);		// 2nd unlock should not prompt
    ss.lock(db1c);			// lock it again
    prompt("unlock");
    ss.unlock(db1);		// and that should prompt again
	prompt();
    
    // db2 has a passphrase lock credentials - it'll work without U/I
	db2.unlock("wrong passphrase");		// pw=two, cred=three
    AutoCredentials pwCred2(alloc);
    pwCred2 += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
        new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
        new(alloc) ListElement(passphrase));
	// pwCred2 = (OLD: two)
    ss.authenticateDb(db2, CSSM_DB_ACCESS_WRITE, &pwCred2); // set it
	db2.unlock();
    ss.lock(db2);
    
    // now change db2's passphrase
    ss.lock(db2);
    pwCred2 += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK,
        new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
        new(alloc) ListElement(badPassphrase));
	// pwCred2 = (OLD: two, NEW: three)
	db2.changePassphrase(&pwCred2);
	// passphrase = three, cred = (OLD: two)
	
    // encode and re-decode to make sure new data is there
    CssmData blob2;
    ss.encodeDb(db2, blob2);
    DbHandle db2a = ss.decodeDb(db2.dbId, &pwCred, blob2);
	// db2a cred = (OLD: two, NEW: three)
    
    // now, the *old* cred won't work anymore
	db2.unlock("old passphrase accepted");

    // back to the old credentials, which *do* have the (old bad, now good) passphrase
    ss.lock(db2a);
    ss.unlock(db2a);
    detail("New passphrase accepted");
	
	// clear the credentials (this will prompt; cancel it)
	ss.authenticateDb(db2, CSSM_DB_ACCESS_WRITE, NULL);
	prompt("cancel");
	db2.unlock("null credential accepted");
	prompt();
	
	// fell-swoop from-to change password operation
	StringData newPassphrase("hollerith");
	AutoCredentials pwCred3(alloc);
	pwCred3 += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK,
		new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
		new(alloc) ListElement(newPassphrase));
	pwCred3 += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
		new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
		new(alloc) ListElement(passphrase));
	db2.changePassphrase(&pwCred3, "accepting original (unchanged) passphrase");
	
	AutoCredentials pwCred4(alloc);
	pwCred4 += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK,
		new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
		new(alloc) ListElement(newPassphrase));
	pwCred4 += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
		new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
		new(alloc) ListElement(badPassphrase));
	db2.changePassphrase(&pwCred4);
	
	// final status check
	AutoCredentials pwCred5(alloc);
	pwCred5 += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
		new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
		new(alloc) ListElement(newPassphrase));	
	ss.authenticateDb(db2, CSSM_DB_ACCESS_WRITE, &pwCred5);
	db2.unlock();
	detail("Final passphrase change verified");
}


//
// Key encryption tests.
//
void keyBlobs()
{
	printf("* Keyblob encryption test\n");
    CssmAllocator &alloc = CssmAllocator::standard();
	ClientSession ss(alloc, alloc);
    
    DLDbIdentifier dbId1(ssuid, "/tmp/one", NULL);
	DBParameters initialParams1 = { 3600, false };
    
    // create a new database
	DbHandle db = ss.createDb(dbId1, NULL, NULL, initialParams1);
	detail("Database created");
	
	// establish an ACL for the key
	StringData theAclPassword("Strenge Geheimsache");
	AclEntryPrototype initialAcl;
	initialAcl.TypedSubject = TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD,
		new(alloc) ListElement(theAclPassword));
	AclEntryInput initialAclInput(initialAcl);
    
    AutoCredentials cred(alloc);
    cred += TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
        new(alloc) ListElement(theAclPassword));
    
    // generate a key
	const CssmCryptoData seed(StringData("Farmers' day"));
	FakeContext genContext(CSSM_ALGCLASS_KEYGEN, CSSM_ALGID_DES,
		&::Context::Attr(CSSM_ATTRIBUTE_KEY_LENGTH, 64),
		&::Context::Attr(CSSM_ATTRIBUTE_SEED, seed),
		NULL);
    KeyHandle key;
    CssmKey::Header header;
    ss.generateKey(db, genContext, CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
        CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT,
        /*cred*/NULL, &initialAclInput, key, header);
	detail("Key generated");
    
    // encrypt with the key
    StringData clearText("Yet another boring cleartext sample string text sequence.");
    StringData iv("Aardvark");
    CssmKey nullKey; memset(&nullKey, 0, sizeof(nullKey));
	FakeContext cryptoContext(CSSM_ALGCLASS_SYMMETRIC, CSSM_ALGID_DES,
		&::Context::Attr(CSSM_ATTRIBUTE_KEY, nullKey),
		&::Context::Attr(CSSM_ATTRIBUTE_INIT_VECTOR, iv),
		&::Context::Attr(CSSM_ATTRIBUTE_MODE, CSSM_ALGMODE_CBC_IV8),
		&::Context::Attr(CSSM_ATTRIBUTE_PADDING, CSSM_PADDING_PKCS1),
        &::Context::Attr(CSSM_ATTRIBUTE_ACCESS_CREDENTIALS, cred),
		NULL);
	CssmData cipherText;
	ss.encrypt(cryptoContext, key, clearText, cipherText);
	detail("Plaintext encrypted with original key");
    
    // encode the key and release it
    CssmData blob;
    ss.encodeKey(key, blob);
    ss.releaseKey(key);
    detail("Key encoded and released");
    
    // decode it again, re-introducing it
    CssmKey::Header decodedHeader;
    KeyHandle key2 = ss.decodeKey(db, blob, decodedHeader);
    detail("Key decoded");
    
    // decrypt with decoded key
    CssmData recovered;
    ss.decrypt(cryptoContext, key2, cipherText, recovered);
    assert(recovered == clearText);
    detail("Decoded key correctly decrypts ciphertext");
    
    // check a few header fields
    if (!memcmp(&header, &decodedHeader, sizeof(header))) {
        detail("All header fields match");
    } else {
        assert(header.algorithm() == decodedHeader.algorithm());
        assert(header.blobType() == decodedHeader.blobType());
        assert(header.blobFormat() == decodedHeader.blobFormat());
        assert(header.keyClass() == decodedHeader.keyClass());
        assert(header.attributes() == decodedHeader.attributes());
        assert(header.usage() == decodedHeader.usage());
        printf("Some header fields differ (probably okay)\n");
    }
    
    // make sure we need the credentials (destructive)
    memset(&cred, 0, sizeof(cred));
    try {
        ss.decrypt(cryptoContext, key2, cipherText, recovered);
        error("RESTORED ACL FAILS TO RESTRICT");
    } catch (CssmError &err) {
        detail(err, "Restored key restricts access properly");
    }
}
