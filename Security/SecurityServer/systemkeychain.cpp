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
// systemkeychain command - set up and manipulate system-unlocked keychains
//
#include <Security/dlclient.h>
#include <Security/cryptoclient.h>
#include <Security/wrapkey.h>
#include <Security/genkey.h>
#include <Security/Schema.h>
#include "ssblob.h"
#include <cstdarg>

using namespace SecurityServer;
using namespace CssmClient;
using namespace UnixPlusPlus;


static const char *unlockConfig = kSystemUnlockFile;


//
// Values set from command-line options
//
const char *systemKCName = kSystemKeychainDir kSystemKeychainName;
bool verbose = false;
bool createIfNeeded = false;
bool force = false;


//
// CSSM record attribute names
//
static const CSSM_DB_ATTRIBUTE_INFO dlInfoLabel = {
	CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
	{"Label"},
	CSSM_DB_ATTRIBUTE_FORMAT_BLOB
};



//
// Local functions
void usage();
void createSystemKeychain(const char *kcName, const char *passphrase);
void extract(const char *srcName, const char *dstName);
void test(const char *kcName);

void notice(const char *fmt, ...);
void fail(const char *fmt, ...);

void masterKeyIndex(Db &db, CssmOwnedData &index);
void labelForMasterKey(Db &db, CssmOwnedData &data);
void deleteKey(Db &db, const CssmData &label);	// delete key with this label


//
// Main program: parse options and dispatch, catching exceptions
//
int main (int argc, char * argv[])
{
	enum Action {
		showUsage,
		setupSystem,
		copyKey,
		testUnlock
	} action = showUsage;

	extern int optind;
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "cCfk:stv")) != -1) {
		switch (arg) {
		case 'c':
			createIfNeeded = true;
			break;
		case 'C':
			action = setupSystem;
			break;
		case 'f':
			force = true;
			break;
		case 'k':
			systemKCName = optarg;
			break;
		case 's':
			action = copyKey;
			break;
		case 't':
			action = testUnlock;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			usage();
		}
	}
	try {
		switch (action) {
		case setupSystem:
			if (optind < argc - 1)
				usage();
			createSystemKeychain(systemKCName, argv[optind]);
			break;
		case copyKey:
			if (optind == argc)
				usage();
			do {
				extract(argv[optind], systemKCName);
			} while (argv[++optind]);
			break;
		case testUnlock:
			test(systemKCName);
			break;
		default:
			usage();
		}
		exit(0);
	} catch (const CssmError &error) {
		cssmPerror(systemKCName, error.cssmError());
		exit(1);
	} catch (const UnixError &error) {
		fail("%s: %s", systemKCName, strerror(error.error));
		exit(1);
	} catch (...) {
		fail("Unexpected exception");
		exit(1);
	}
}


//
// Partial usage message (some features aren't worth emphasizing...)
//
void usage()
{
	fprintf(stderr, "Usage: systemkeychain -S [passphrase]  # (re)create system root keychain"
		"\n\tsystemkeychain [-k destination-keychain] -s source-keychain ..."
		"\n");
	exit(2);
}


//
// Create a keychain and set it up as the system-root secret
//
void createSystemKeychain(const char *kcName, const char *passphrase)
{
	// for the default path only, make sure the directory exists
	if (!strcmp(kcName, kSystemKeychainDir kSystemKeychainName))
		::mkdir(kSystemKeychainDir, 0755);
	
	CSP csp(gGuidAppleCSPDL);
	DL dl(gGuidAppleCSPDL);
	
	// create the keychain, using appropriate credentials
	Db db(dl, kcName);
	CssmAllocator &alloc = db.allocator();
	AutoCredentials cred(alloc);	// will leak, but we're quitting soon :-)
	CSSM_CSP_HANDLE cspHandle = csp->handle();
	Key masterKey;
	if (passphrase) {
		// use this passphrase
		cred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK,
			new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
			new(alloc) ListElement(StringData(passphrase)));
		db->accessCredentials(&cred);
	} else {
		// generate a random key
		notice("warning: this keychain cannot be unlocked with any passphrase");
		GenerateKey generate(csp, CSSM_ALGID_3DES_3KEY_EDE, 64 * 3);
		masterKey =	generate(KeySpec(CSSM_KEYUSE_ANY,
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE));
		cred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK,
			new(alloc) ListElement(CSSM_WORDID_SYMMETRIC_KEY),
			new(alloc) ListElement(CssmData::wrap(cspHandle)),
			new(alloc) ListElement(CssmData::wrap(static_cast<const CssmKey &>(masterKey))));
		db->accessCredentials(&cred);
	}
	db->dbInfo(&KeychainCore::Schema::DBInfo); // Set the standard schema
	try {
		db->create();
	} catch (const CssmError &error) {
		if (error.cssmError() == CSSMERR_DL_DATASTORE_ALREADY_EXISTS && force) {
			notice("recreating %s", kcName);
			unlink(kcName);
			db->create();
		} else
			throw;
	}
	chmod(db->name(), 0644);

	// extract the key into the CSPDL
	DeriveKey derive(csp, CSSM_ALGID_KEYCHAIN_KEY, CSSM_ALGID_3DES_3KEY, 3 * 64);
	CSSM_DL_DB_HANDLE dlDb = db->handle();
	CssmData dlDbData = CssmData::wrap(dlDb);
	CssmKey refKey;
	KeySpec spec(CSSM_KEYUSE_ANY,
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE);
	derive(&dlDbData, spec, refKey);
	
	// now extract the raw keybits
	CssmKey rawKey;
	WrapKey wrap(csp, CSSM_ALGID_NONE);
	wrap(refKey, rawKey);

	// form the evidence record
	UnlockBlob blob;
	blob.initialize(0);
	CssmAutoData index(CssmAllocator::standard());
	masterKeyIndex(db, index);
	memcpy(&blob.signature, index.data(), sizeof(blob.signature));
	memcpy(blob.masterKey, rawKey.data(), sizeof(blob.masterKey));

	// write it out, forcibly overwriting an existing file
	string tempFile(string(unlockConfig) + ",");
	FileDesc blobFile(tempFile, O_WRONLY | O_CREAT | O_TRUNC, 0400);
	if (blobFile.write(blob) != sizeof(blob)) {
		unlink(tempFile.c_str());
		fail("unable to write %s", tempFile.c_str());
	}
	blobFile.close();
	::rename(tempFile.c_str(), unlockConfig);
	
	notice("%s installed as system keychain", kcName);
}


//
// Extract the master secret from a keychain and install it in another keychain for unlocking
//
void extract(const char *srcName, const char *dstName)
{
	CSP csp(gGuidAppleCSPDL);
	DL dl(gGuidAppleCSPDL);
	
	// open source database
	Db srcDb(dl, srcName);
	
	// open destination database
	Db dstDb(dl, dstName);
	try {
		dstDb->open();
	} catch (const CssmError &err) {
		if (err.cssmError() == CSSMERR_DL_DATASTORE_DOESNOT_EXIST && createIfNeeded) {
			notice("creating %s", dstName);
			dstDb->create();
		} else
			throw;
	}
	
	// extract master key and place into destination keychain
	DeriveKey derive(csp, CSSM_ALGID_KEYCHAIN_KEY, CSSM_ALGID_3DES_3KEY, 3 * 64);
	CSSM_DL_DB_HANDLE dstDlDb = dstDb->handle();
	derive.add(CSSM_ATTRIBUTE_DL_DB_HANDLE, dstDlDb);
	CSSM_DL_DB_HANDLE srcDlDb = srcDb->handle();
	CssmData dlDbData = CssmData::wrap(srcDlDb);
	CssmAutoData keyLabel(CssmAllocator::standard());
	labelForMasterKey(srcDb, keyLabel);
	KeySpec spec(CSSM_KEYUSE_ANY,
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE,
		keyLabel);
	CssmKey masterKey;
	try {
		derive(&dlDbData, spec, masterKey);
	} catch (const CssmError &error) {
		if (error.cssmError() != CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA)
			throw;
		if (!force)
			fail("existing key in %s not overwritten. Use -f to replace it.", dstDb->name());
		notice("replacing existing record in %s", dstDb->name());
		deleteKey(dstDb, keyLabel);
		derive(&dlDbData, spec, masterKey);
	}
	notice("%s can now be unlocked with a key in %s", srcName, dstName);
}


//
// Run a simple test to see if the system-root keychain can auto-unlock.
// This isn't trying really hard to diagnose any problems; it's just a yay-or-nay check.
//
void test(const char *kcName)
{
	CSP csp(gGuidAppleCSPDL);
	DL dl(gGuidAppleCSPDL);
	
	// lock, then unlock the keychain
	Db db(dl, kcName);
	printf("Testing system unlock of %s\n", kcName);
	printf("(If you are prompted for a passphrase, cancel)\n");
	try {
		db->lock();
		db->unlock();
		notice("System unlock is working");
	} catch (...) {
		fail("System unlock is NOT working\n");
	}
}


//
// Utility functions
//
void masterKeyIndex(Db &db, CssmOwnedData &index)
{
	SecurityServer::ClientSession ss(CssmAllocator::standard(), CssmAllocator::standard());
	SecurityServer::DbHandle dbHandle;
	db->passThrough(CSSM_APPLECSPDL_DB_GET_HANDLE, (const void *)NULL, &dbHandle);
	ss.getDbSuggestedIndex(dbHandle, index.get());
}


void labelForMasterKey(Db &db, CssmOwnedData &label)
{
	label = StringData("SYSKC**");	// 8 bytes exactly
	CssmAutoData index(label.allocator);
	masterKeyIndex(db, index);
	label.append(index);
}


void deleteKey(Db &db, const CssmData &label)
{
	DbCursor search(db);
	search->recordType(CSSM_DL_DB_RECORD_SYMMETRIC_KEY);
	search->add(CSSM_DB_EQUAL, dlInfoLabel, label);
	DbUniqueRecord id;
	if (search->next(NULL, NULL, id))
		id->deleteRecord();
}


//
// Message helpers
//
void notice(const char *fmt, ...)
{
	if (verbose) {
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		putchar('\n');
		va_end(args);
	}
}

void fail(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	putchar('\n');
	va_end(args);
	exit(1);
}
