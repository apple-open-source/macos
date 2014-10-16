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
// Tester - test driver for securityserver client side.
//
#include "testclient.h"
#include "testutils.h"
#include <unistd.h>		// getopt(3)
#include <set>


//
// Global constants
//
const CssmData null;		// zero pointer, zero length constant data
const AccessCredentials nullCred;	// null credentials

CSSM_GUID ssguid = { 1,2,3 };
CssmSubserviceUid ssuid(ssguid);


//
// Local functions
//
static void usage();
static void runtest(char type);


//
// Default test set
//
static char testCodes[] = ".cesaAbdkKt";


//
// Main program
//
int main(int argc, char *argv[])
{
	setbuf(stdout, NULL);

	long ranseq = 0;	// random stress test count
	long ranseed = 1;	// random seed for it

	int arg;
	while ((arg = getopt(argc, argv, "r:v")) != -1) {
		switch (arg) {
		case 'r': {
			ranseq = atoi(optarg);
			if (const char *colon = strchr(optarg, ':'))
				ranseed = atoi(colon + 1);
			else
				ranseed = getpid() ^ time(NULL);
			break;
			}
		case 'v':
			verbose = true;
			break;
		default:
			usage();
		}
	}
	if (optind < argc - 1)
		usage();
	const char *sequence = argv[optind];
	if (sequence && !strcmp(sequence, "+"))
		sequence = testCodes;
		
	if (ranseq) {	// repeated random (stress test) sequence
		if (!sequence)
			sequence = testCodes;
		printf("*** Random stress test: %ld iterations from <%s> with seed=%ld\n",
			ranseq, sequence, ranseed);
		srandom(ranseed);
		int setSize = strlen(sequence);
		for (long n = 0; n < ranseq; n++) {
			char type = sequence[random() % setSize];
			printf("\n[%ld:%c]", n, type);
			runtest(type);
		}
		printf("*** Random test sequence complete.\n");
		exit(0);
	} else {		// single-pass selected tests sequence
		if (!sequence)
			sequence = ".";	// default to ping test
		for (const char *s = sequence; *s; s++)
			runtest(*s);
		printf("*** Test sequence complete.\n");
		exit(0);
	}
}

void usage()
{
	fprintf(stderr, "Usage: SSTester [-r count[:seed]] [-v] [%s|.|+]\n",
		testCodes);
	exit(2);
}


//
// Run a single type test
//
void runtest(char type)
{	
	try {
		debug("SStest", "Start test <%c>", type);
		switch (type) {
		case '.':	// default
			integrity();
			break;
		case '-':
			adhoc();
			break;
		case 'a':
			acls();
			break;
		case 'A':
			authAcls();
			break;
		case 'b':
			blobs();
			break;
		case 'c':
			codeSigning();
			break;
		case 'd':
			databases();
			break;
		case 'e':
			desEncryption();
			break;
		case 'k':
			keychainAcls();
			break;
		case 'K':
			keyBlobs();
			break;
		case 's':
			signWithRSA();
			break;
		case 't':
			authorizations();
			break;
		case 'T':
			timeouts();
			break;
		default:
			error("Invalid test selection (%c)", type);
		}
		printf("** Test step complete.\n");
		debug("SStest", "End test <%c>", type);
	} catch (CssmCommonError &err) {
		error(err, "Unexpected exception");
	} catch (...) {
		error("Unexpected system exception");
	}
}


//
// Basic integrity test.
//
void integrity()
{
	ClientSession ss(CssmAllocator::standard(), CssmAllocator::standard());
	
	printf("* Generating random sample: ");
	DataBuffer<11> sample;
	ss.generateRandom(sample);
	for (uint32 n = 0; n < sample.length(); n++)
		printf("%.2x", ((unsigned char *)sample)[n]);
	printf("\n");
}


//
// Database timeouts
// @@@ Incomplete and not satisfactory
//
void timeouts()
{
	printf("* Database timeout locks test\n");
    CssmAllocator &alloc = CssmAllocator::standard();
	ClientSession ss(alloc, alloc);
    
    DLDbIdentifier dbId1(ssuid, "/tmp/one", NULL);
    DLDbIdentifier dbId2(ssuid, "/tmp/two", NULL);
	DBParameters initialParams1 = { 4, false };	// 4 seconds timeout
	DBParameters initialParams2 = { 8, false };	// 8 seconds timeout
    
	// credential to set keychain passphrase
    AutoCredentials pwCred(alloc);
    StringData password("mumbojumbo");
    pwCred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK,
        new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
        new(alloc) ListElement(password));
    
	DbHandle db1 = ss.createDb(dbId1, &pwCred, NULL, initialParams1);
	DbHandle db2 = ss.createDb(dbId2, &pwCred, NULL, initialParams2);
	detail("Databases created");
    
    // generate a key
	const CssmCryptoData seed(StringData("rain tonight"));
	FakeContext genContext(CSSM_ALGCLASS_KEYGEN, CSSM_ALGID_DES,
		&::Context::Attr(CSSM_ATTRIBUTE_KEY_LENGTH, 64),
		&::Context::Attr(CSSM_ATTRIBUTE_SEED, seed),
		NULL);
    KeyHandle key;
    CssmKey::Header header;
    ss.generateKey(db1, genContext, CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
        CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT,
        /*cred*/NULL, NULL, key, header);
    ss.generateKey(db2, genContext, CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
        CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT,
        /*cred*/NULL, NULL, key, header);
	detail("Keys generated and stored");
	
	// credential to provide keychain passphrase
    AutoCredentials pwCred2(alloc);
    pwCred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
        new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
        new(alloc) ListElement(password));

	//@@@ incomplete
	ss.releaseDb(db1);
	ss.releaseDb(db2);
}


//
// Ad-hoc test area.
// Used for whatever is needed at the moment...
//
void adhoc()
{
	printf("* Ad-hoc test sequence (now what does it do *this* time?)\n");
	
	Cssm cssm1;
	Cssm cssm2;
	cssm1->init();
	cssm2->init();
	
	{
		Module m1(gGuidAppleCSP, cssm1);
		Module m2(gGuidAppleCSP, cssm2);
		CSP r1(m1);
		CSP r2(m2);
		
		Digest d1(r1, CSSM_ALGID_SHA1);
		Digest d2(r2, CSSM_ALGID_SHA1);
		
		StringData foo("foo de doo da blech");
		DataBuffer<30> digest1, digest2;
		d1.digest(foo, digest1);
		d2.digest(foo, digest2);
		if (digest1 == digest2)
			detail("Digests verify");
		else
			error("Digests mismatch");
	}
	
	cssm1->terminate();
	cssm2->terminate();
}
