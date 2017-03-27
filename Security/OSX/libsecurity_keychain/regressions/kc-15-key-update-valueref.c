/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
 * limitations under the xLicense.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

//
//	Tests the SecItemUpdate function.
//	Currently this is a simple test to determine whether the correct item
//	is updated when specified by a kSecValueRef (see <rdar://10358577>).
//

#include "keychain_regressions.h"
#include "kc-helpers.h"
#include "kc-item-helpers.h"
#include "kc-key-helpers.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <Security/Security.h>
#include <utilities/SecCFRelease.h>

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <sys/param.h>

static int debug = 1;
static int verbose = 1;

#define MAXNAMELEN MAXPATHLEN
#define MAXITEMS INT32_MAX

#pragma mark -- Utility Functions --


static void PrintTestResult(char *testStr, OSStatus status, OSStatus expected)
{
	if (verbose) {
		fprintf(stdout, "%s: %s (result=%d, expected=%d)\n", testStr,
				(status==expected) ? "OK" : "FAILED",
				(int)status, (int)expected);
	}
	if (debug) {
		fprintf(stdout, "\n");
	}
	fflush(stdout);
}

static void PrintCFStringWithFormat(const char *formatStr, CFStringRef inStr)
{
    char *buf = (char*)malloc(MAXNAMELEN);
    if (buf) {
        if (CFStringGetCString(inStr, buf, (CFIndex)MAXNAMELEN, kCFStringEncodingUTF8)) {
            fprintf(stdout, formatStr, buf);
            fflush(stdout);
        }
        free(buf);
    }
}

const CFStringRef gPrefix = CFSTR("Test Key");
const CFStringRef gLabel = CFSTR("Test AES Encryption Key");
const CFStringRef gUUID = CFSTR("550e8400-e29b-41d4-a716-446655441234");

// CreateSymmetricKey will create a new AES-128 symmetric encryption key
// with the provided label, application label, and application tag.
// Each of those attributes is optional, but only the latter two
// (application label and application tag) are considered part of the
// key's "unique" attribute set. Previously, if you attempted to create a
// key which differs only in the label attribute (but not in the other two)
// then the attempt would fail and leave a "turd" key with no label in your
// keychain: <rdar://8289559>, fixed in 11A268a.

static int CreateSymmetricKey(
	SecKeychainRef inKeychain,
	CFStringRef keyLabel,
	CFStringRef keyAppLabel,
	CFStringRef keyAppTag,
	OSStatus expected)
{
    OSStatus status;
	int keySizeValue = 128;
	CFNumberRef keySize = CFNumberCreate(NULL, kCFNumberIntType, &keySizeValue);

	// get a SecKeychainRef for the keychain in which we want the key to be created
	// (this step is optional, but if omitted, the key is NOT saved in any keychain!)
	SecKeychainRef keychain = NULL;
	if (inKeychain == NULL)
		status = SecKeychainCopyDefault(&keychain);
	else
		keychain = (SecKeychainRef) CFRetain(inKeychain);

	// create a SecAccessRef to set up the initial access control settings for this key
	// (this step is optional; if omitted, the creating application has access to the key)
	// note: the access descriptor should be the same string as will be used for the item's label,
	// since it's the string that is displayed by the access confirmation dialog to describe the item.
	SecAccessRef access = NULL;
	status = SecAccessCreate(gLabel, NULL, &access);

	// create a dictionary of parameters describing the key we want to create
	CFMutableDictionaryRef params = CFDictionaryCreateMutable(NULL, 0,
										&kCFTypeDictionaryKeyCallBacks,
										&kCFTypeDictionaryValueCallBacks);

	CFDictionaryAddValue( params, kSecClass, kSecClassKey );
	CFDictionaryAddValue( params, kSecUseKeychain, keychain );
	CFDictionaryAddValue( params, kSecAttrAccess, access );
	CFDictionaryAddValue( params, kSecAttrKeyClass, kSecAttrKeyClassSymmetric );
	CFDictionaryAddValue( params, kSecAttrKeyType, kSecAttrKeyTypeAES );
    CFDictionaryAddValue( params, kSecAttrKeySizeInBits, keySize ); CFRelease(keySize);
	CFDictionaryAddValue( params, kSecAttrIsPermanent, kCFBooleanTrue );
	CFDictionaryAddValue( params, kSecAttrCanEncrypt, kCFBooleanTrue );
	CFDictionaryAddValue( params, kSecAttrCanDecrypt, kCFBooleanTrue );
	CFDictionaryAddValue( params, kSecAttrCanWrap, kCFBooleanFalse );
	CFDictionaryAddValue( params, kSecAttrCanUnwrap, kCFBooleanFalse );
	if (keyLabel)
		CFDictionaryAddValue( params, kSecAttrLabel, keyLabel );
	if (keyAppLabel)
		CFDictionaryAddValue( params, kSecAttrApplicationLabel, keyAppLabel );
	if (keyAppTag)
		CFDictionaryAddValue( params, kSecAttrApplicationTag, keyAppTag );

	// generate the key
	CFErrorRef error = NULL;
    SecKeyRef key = SecKeyGenerateSymmetric(params, &error);

	// print result and clean up
	if (debug) {
		if (key == NULL) {
			CFStringRef desc = (error) ? CFErrorCopyDescription(error) : CFRetain(CFSTR("(no result!"));
			PrintCFStringWithFormat("SecKeyGenerateSymmetric failed: %s\n", desc);
			CFRelease(desc);
		}
		else {
			CFStringRef desc = CFCopyDescription(key);
			PrintCFStringWithFormat("SecKeyGenerateSymmetric succeeded: %s\n", desc);
			CFRelease(desc);
		}
	}
	status = (error) ? (OSStatus) CFErrorGetCode(error) : noErr;
//	if (status == errSecDuplicateItem)
//		status = noErr; // it's OK if the key already exists

	if (key) CFRelease(key);
	if (error) CFRelease(error);
	if (params) CFRelease(params);
	if (keychain) CFRelease(keychain);
	if (access) CFRelease(access);

	PrintTestResult("CreateSymmetricKey", status, expected);

	return status;
}

static int TestUpdateItems(SecKeychainRef keychain)
{
	int result = 0;
    OSStatus status = errSecSuccess;

	// first, create a symmetric key
	CFGregorianDate curGDate = CFAbsoluteTimeGetGregorianDate(CFAbsoluteTimeGetCurrent(), NULL);
	CFStringRef curDateLabel = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@ (%4d-%02d-%02d)"),
		gPrefix, (int) (curGDate.year), (int) (curGDate.month), (int) (curGDate.day));
	CFStringRef curAppTag = CFSTR("SecItemUpdate");

	status = CreateSymmetricKey(keychain, curDateLabel, gUUID, curAppTag, noErr);
    CFReleaseNull(curDateLabel);
	if (status && status != errSecDuplicateItem)
		++result;
    
	CFStringRef keyLabel = CFSTR("iMessage test key");
  	CFStringRef newLabel = CFSTR("iMessage test PRIVATE key");
    
    // create a new 1024-bit RSA key pair
	SecKeyRef publicKey = NULL;
	SecKeyRef privateKey = NULL;
	CFMutableDictionaryRef params = CFDictionaryCreateMutable(NULL, 0,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	int keySizeValue = 1024;
	CFNumberRef keySize = CFNumberCreate(NULL, kCFNumberIntType, &keySizeValue);

	CFDictionaryAddValue( params, kSecAttrKeyType, kSecAttrKeyTypeRSA );
    CFDictionaryAddValue( params, kSecAttrKeySizeInBits, keySize ); CFReleaseNull(keySize);
	CFDictionaryAddValue( params, kSecAttrLabel, keyLabel );
//	CFDictionaryAddValue( params, kSecAttrAccess, access );
// %%% note that SecKeyGeneratePair will create the key pair in the default keychain
// if a keychain is not given via the kSecUseKeychain parameter.
	CFDictionaryAddValue( params, kSecUseKeychain, keychain );

	status = SecKeyGeneratePair(params, &publicKey, &privateKey);
    ok_status(status, "%s: SecKeyGeneratePair", testName);
	if (status != noErr) {
		++result;
	}
	PrintTestResult("TestUpdateItems: generating key pair", status, noErr);
    
    // Make sure we have the key of interest
    checkN(testName, createQueryKeyDictionaryWithLabel(keychain, kSecAttrKeyClassPrivate, keyLabel), 1);
    checkN(testName, createQueryKeyDictionaryWithLabel(keychain, kSecAttrKeyClassPrivate, newLabel), 0);

	// create a query which will match just the private key item (based on its known reference)
	CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
//	CFArrayRef itemList = CFArrayCreate(NULL, (const void**) &privateKey, 1, &kCFTypeArrayCallBacks);
// %%% note that kSecClass seems to be a required query parameter even though
// kSecMatchItemList is provided; that looks like it could be a bug...
	CFDictionaryAddValue( query, kSecClass, kSecClassKey );
//	CFDictionaryAddValue( query, kSecAttrKeyClass, kSecAttrKeyClassPrivate );

// %%% pass the private key ref, instead of the item list, to test <rdar://problem/10358577>
//	CFDictionaryAddValue( query, kSecMatchItemList, itemList );
	CFDictionaryAddValue( query, kSecValueRef, privateKey );

	// create dictionary of changed attributes for the private key
	CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(NULL, 0,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	SecAccessRef access = NULL;

	status = SecAccessCreate(newLabel, NULL, &access);
    ok_status(status, "%s: SecAccessCreate", testName);
	if (status != noErr) {
		++result;
	}
	PrintTestResult("TestUpdateItems: creating access", status, noErr);

//%%% note that changing the access for this key causes a dialog,
// so leave this out for the moment (uncomment to test that access change works).
// Normally the desired access should be passed into the SecKeyGeneratePair function.
// so there is no need for a dialog later.
//	CFDictionaryAddValue( attrs, kSecAttrAccess, access );
	CFDictionaryAddValue( attrs, kSecAttrLabel, newLabel );

	// update the private key with the new attributes
	status = SecItemUpdate( query, attrs );
    ok_status(status, "%s: SecItemUpdate", testName);
    
	if (status != noErr) {
		++result;
	}
	PrintTestResult("TestUpdateItems: updating item", status, noErr);
    
    // Make sure label changed
    checkN(testName, createQueryKeyDictionaryWithLabel(keychain, kSecAttrKeyClassPrivate, keyLabel), 0);
    checkN(testName, createQueryKeyDictionaryWithLabel(keychain, kSecAttrKeyClassPrivate, newLabel), 1);

	if (publicKey)
		CFRelease(publicKey);
	if (privateKey)
		CFRelease(privateKey);
	if (access)
		CFRelease(access);

	if (params)
		CFRelease(params);
	if (query)
		CFRelease(query);
	if (attrs)
		CFRelease(attrs);

	return result;
}

int kc_15_key_update_valueref(int argc, char *const *argv)
{
	plan_tests(20);
    initializeKeychainTests(__FUNCTION__);

    SecKeychainRef keychain = getPopulatedTestKeychain();

    TestUpdateItems(keychain);

    checkPrompts(0, "no prompts during test");

    ok_status(SecKeychainDelete(keychain), "%s: SecKeychainDelete", testName);
    CFRelease(keychain);

    deleteTestFiles();
	return 0;
}
