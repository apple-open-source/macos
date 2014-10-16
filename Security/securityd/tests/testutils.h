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
#ifndef _H_TESTUTILS
#define _H_TESTUTILS

#include "testclient.h"


//
// Global test state
//
extern bool verbose;


//
// Error and diagnostic drivers
//
void error(const char *fmt, ...) __attribute__((format(printf,1,2)));
void error(const CssmCommonError &error, const char *fmt, ...) __attribute__((format(printf,2,3)));
void detail(const char *fmt, ...) __attribute__((format(printf,1,2)));
void detail(const CssmCommonError &error, const char *msg);
void prompt(const char *msg);
void prompt();


//
// A self-building "fake" context.
// (Fake in that it was hand-made without involvement of CSSM.)
//
class FakeContext : public ::Context {
public:
	FakeContext(CSSM_CONTEXT_TYPE type, CSSM_ALGORITHMS alg, uint32 count);
	FakeContext(CSSM_CONTEXT_TYPE type, CSSM_ALGORITHMS alg, ...);
};


//
// A test driver class for ACL tests
//
class AclTester {
public:
	AclTester(ClientSession &ss, const AclEntryInput *acl);
	
	void testWrap(const AccessCredentials *cred, const char *howWrong = NULL);
	void testEncrypt(const AccessCredentials *cred, const char *howWrong = NULL);
	
	ClientSession &session;
	KeyHandle keyRef;
};


//
// A test driver class for database tests
//
class DbTester {
public:
	DbTester(ClientSession &ss, const char *path, 
		const AccessCredentials *cred, int timeout = 30, bool sleepLock = true);
	
	operator DbHandle () const { return dbRef; }
	void unlock(const char *howWrong = NULL);
	void changePassphrase(const AccessCredentials *cred, const char *howWrong = NULL);
	
	ClientSession &session;
	DBParameters params;
	DLDbIdentifier dbId;
	DbHandle dbRef;
};


#endif //_H_TESTUTILS
