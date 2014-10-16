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
// testutils - utilities for unit test drivers
//
#include "testutils.h"

using namespace CssmClient;

bool verbose = false;


//
// Error and diagnostic drivers
//
void error(const char *msg = NULL, ...)
{
	if (msg) {
		va_list args;
		va_start(args, msg);
		vfprintf(stderr, msg, args);
		va_end(args);
		putc('\n', stderr);
	}
	abort();
}

void error(const CssmCommonError &err, const char *msg = NULL, ...)
{
	if (msg) {
		va_list args;
		va_start(args, msg);
		vfprintf(stderr, msg, args);
		va_end(args);
		fprintf(stderr, ": %s", cssmErrorString(err.cssmError()).c_str());
		putc('\n', stderr);
	}
	abort();
}

void detail(const char *msg = NULL, ...)
{
	if (verbose) {
		va_list args;
		va_start(args, msg);
		vfprintf(stdout, msg, args);
		va_end(args);
		putc('\n', stdout);
	}
}

void detail(const CssmCommonError &err, const char *msg)
{
	if (verbose)
		printf("%s (ok): %s\n", msg, cssmErrorString(err).c_str());
}

void prompt(const char *msg)
{
	if (isatty(fileno(stdin)))
		printf("[%s]", msg);
}

void prompt()
{
	if (isatty(fileno(stdin)))
		printf(" OK\n");
}


//
// FakeContext management
//
FakeContext::FakeContext(CSSM_CONTEXT_TYPE type, CSSM_ALGORITHMS alg, uint32 count)
: Context(type, alg)
{
	NumberOfAttributes = count;
	ContextAttributes = new Attr[count];
}


FakeContext::FakeContext(CSSM_CONTEXT_TYPE type, CSSM_ALGORITHMS alg, ...) 
: Context(type, alg)
{
	// count arguments
	va_list args;
	va_start(args, alg);
	uint32 count = 0;
	while (va_arg(args, Attr *))
		count++;
	va_end(args);
	
	// make vector
	NumberOfAttributes = count;
	ContextAttributes = new Attr[count];
	
	// stuff vector
	va_start(args, alg);
	for (uint32 n = 0; n < count; n++)
		(*this)[n] = *va_arg(args, Attr *);
	va_end(args);
}


//
// ACL test driver class
//
AclTester::AclTester(ClientSession &ss, const AclEntryInput *acl) : session(ss)
{
	// make up a DES key
	StringData keyBits("Tweedle!");
	CssmKey key(keyBits);
	key.header().KeyClass = CSSM_KEYCLASS_SESSION_KEY;
	
	// wrap in the key
	CssmData unwrappedData;
	FakeContext unwrapContext(CSSM_ALGCLASS_SYMMETRIC, CSSM_ALGID_NONE, 0);
    CssmKey::Header keyHeader;
    ss.unwrapKey(noDb, unwrapContext, noKey, noKey,
		key,
		CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
		CSSM_KEYATTR_EXTRACTABLE,
		NULL /*cred*/, acl,
		unwrappedData, keyRef, keyHeader);
	detail("Key seeded with ACL");
}


void AclTester::testWrap(const AccessCredentials *cred, const char *howWrong)
{
	FakeContext wrapContext(CSSM_ALGCLASS_SYMMETRIC, CSSM_ALGID_NONE, 0);
	CssmWrappedKey wrappedKey;
	try {
		session.wrapKey(wrapContext, noKey, keyRef,
			cred, NULL /*descriptive*/, wrappedKey);
		if (howWrong) {
			error("WRAP MISTAKENLY SUCCEEDED: %s", howWrong);
		}
		detail("extract OK");
	} catch (const CssmCommonError &err) {
		if (!howWrong)
			error(err, "FAILED TO EXTRACT KEY");
		detail(err, "extract failed OK");
	}
}

void AclTester::testEncrypt(const AccessCredentials *cred, const char *howWrong)
{
    CssmKey keyForm; memset(&keyForm, 0, sizeof(keyForm));
    StringData iv("Aardvark");
    StringData clearText("blah");
	CssmData remoteCipher;
    try {
        if (cred) {
            FakeContext cryptoContext(CSSM_ALGCLASS_SYMMETRIC, CSSM_ALGID_DES,
                &::Context::Attr(CSSM_ATTRIBUTE_KEY, keyForm),
                &::Context::Attr(CSSM_ATTRIBUTE_INIT_VECTOR, iv),
                &::Context::Attr(CSSM_ATTRIBUTE_MODE, CSSM_ALGMODE_CBC_IV8),
                &::Context::Attr(CSSM_ATTRIBUTE_PADDING, CSSM_PADDING_PKCS1),
                &::Context::Attr(CSSM_ATTRIBUTE_ACCESS_CREDENTIALS, *cred),
                NULL);
            session.encrypt(cryptoContext, keyRef, clearText, remoteCipher);
        } else {
            FakeContext cryptoContext(CSSM_ALGCLASS_SYMMETRIC, CSSM_ALGID_DES,
                &::Context::Attr(CSSM_ATTRIBUTE_KEY, keyForm),
                &::Context::Attr(CSSM_ATTRIBUTE_INIT_VECTOR, iv),
                &::Context::Attr(CSSM_ATTRIBUTE_MODE, CSSM_ALGMODE_CBC_IV8),
                &::Context::Attr(CSSM_ATTRIBUTE_PADDING, CSSM_PADDING_PKCS1),
                NULL);
            session.encrypt(cryptoContext, keyRef, clearText, remoteCipher);
        }
		if (howWrong) {
			error("ENCRYPT MISTAKENLY SUCCEEDED: %s", howWrong);
		}
		detail("encrypt OK");
	} catch (CssmCommonError &err) {
		if (!howWrong)
			error(err, "FAILED TO ENCRYPT");
		detail(err, "encrypt failed");
	}
}


//
// Database test driver class
//
DbTester::DbTester(ClientSession &ss, const char *path,
	const AccessCredentials *cred, int timeout, bool sleepLock) 
: session(ss), dbId(ssuid, path, NULL)
{
	params.idleTimeout = timeout;
	params.lockOnSleep = sleepLock;
	dbRef = ss.createDb(dbId, cred, NULL, params);
	detail("Database %s created", path);
}


void DbTester::unlock(const char *howWrong)
{
	session.lock(dbRef);
	try {
		session.unlock(dbRef);
		if (howWrong)
			error("DATABASE MISTAKENLY UNLOCKED: %s", howWrong);
	} catch (CssmError &err) {
		if (!howWrong)
			error(err, howWrong);
		detail(err, howWrong);
	}
}

void DbTester::changePassphrase(const AccessCredentials *cred, const char *howWrong)
{
	session.lock(dbRef);
	try {
		session.changePassphrase(dbRef, cred);
		if (howWrong)
			error("PASSPHRASE CHANGE MISTAKENLY SUCCEEDED: %s", howWrong);
	} catch (CssmError &err) {
		if (!howWrong)
			error(err, howWrong);
		detail(err, howWrong);
	}
}
