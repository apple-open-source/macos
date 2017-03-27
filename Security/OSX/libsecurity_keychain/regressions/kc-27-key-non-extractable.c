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

#import <Security/Security.h>
#import <Security/SecCertificatePriv.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"

//
//	Test for <rdar://9251635>
//

#include <stdlib.h>
#include <err.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecItemPriv.h>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/*
	Note: the following will show all private keys with their label
	and extractable attribute value (in attribute 0x00000010):

	$ security dump | grep -A 16 "0x00000000 <uint32>=0x00000010" | grep -e ^\ *0x000000[01][01] -e --
*/


static OSStatus GenerateRSAKeyPair(
    SecKeychainRef keychain,
    CFStringRef keyLabel,
    int keySizeValue,
    Boolean *extractable,
    SecKeyRef *publicKeyRef,
    SecKeyRef *privateKeyRef)
{
    OSStatus status;
	CFNumberRef keySize = CFNumberCreate(NULL, kCFNumberIntType, &keySizeValue);

	// create a SecAccessRef to set up the initial access control settings for this key
	// (this step is optional; if omitted, the creating application has access to the key)
	// note: the access descriptor should be the same string as will be used for the item's label,
	// since it's the string that is displayed by the access confirmation dialog to describe the item.
	SecAccessRef access = NULL;
	status = SecAccessCreate(keyLabel, NULL, &access);

	// create a dictionary of parameters describing the key we want to create
	CFMutableDictionaryRef params = CFDictionaryCreateMutable(NULL, 0,
                                                              &kCFTypeDictionaryKeyCallBacks,
                                                              &kCFTypeDictionaryValueCallBacks);
/*
 From the header doc for SecKeyGeneratePair (seems to be incomplete...):
    * kSecAttrLabel default NULL
    * kSecAttrIsPermanent if this key is present and has a Boolean
      value of true, the key or key pair will be added to the default
      keychain.
    * kSecAttrApplicationTag default NULL
    * kSecAttrEffectiveKeySize default NULL same as kSecAttrKeySizeInBits
    * kSecAttrCanEncrypt default false for private keys, true for public keys
    * kSecAttrCanDecrypt default true for private keys, false for public keys
    * kSecAttrCanDerive default true
    * kSecAttrCanSign default true for private keys, false for public keys
    * kSecAttrCanVerify default false for private keys, true for public keys
    * kSecAttrCanWrap default false for private keys, true for public keys
    * kSecAttrCanUnwrap default true for private keys, false for public keys
*/
	CFDictionaryAddValue( params, kSecUseKeychain, keychain );
	CFDictionaryAddValue( params, kSecAttrAccess, access );
	CFDictionaryAddValue( params, kSecAttrKeyType, kSecAttrKeyTypeRSA );
    CFDictionaryAddValue( params, kSecAttrKeySizeInBits, keySize ); CFReleaseNull(keySize);
    CFDictionaryAddValue( params, kSecAttrIsPermanent, kCFBooleanTrue );

	if (extractable)
        CFDictionaryAddValue( params, kSecAttrIsExtractable, (*extractable) ? kCFBooleanTrue : kCFBooleanFalse );
	if (keyLabel)
		CFDictionaryAddValue( params, kSecAttrLabel, keyLabel );

    // generate the key
    status = SecKeyGeneratePair(params, publicKeyRef, privateKeyRef);
    ok_status(status, "%s: SecKeyGeneratePair", testName);

	if (params) CFRelease(params);
	if (access) CFRelease(access);

	return status;
}

static int testExtractable(
    SecKeychainRef keychain,
	Boolean extractable,
	Boolean explicit)
{
	OSStatus status;
	SecKeyRef publicKeyRef = NULL;
	SecKeyRef privateKeyRef = NULL;
	CFStringRef label = (extractable) ? CFSTR("test-extractable-YES") : CFSTR("test-extractable-NO");
	Boolean *extractablePtr = (explicit) ? &extractable : NULL;

    status = GenerateRSAKeyPair(keychain,
                                label,
								1024, // size
								extractablePtr,
								&publicKeyRef,
								&privateKeyRef);

	if (status != noErr) {
        //errx(EXIT_FAILURE, "Unable to get key pair (err = %d)", status);
        return status;
	}

	// check that the attributes of the generated private key are what we think they are
	const CSSM_KEY *cssmPrivKey;
	status = SecKeyGetCSSMKey(privateKeyRef, &cssmPrivKey);
    ok_status(status, "%s: SecKeyGetCSSMKey", testName);

	if (status != noErr) {
        //errx(EXIT_FAILURE, "Unable to get CSSM reference key (err = %d)", status);
        return status;
	}
	if (extractable) {
        ok(cssmPrivKey->KeyHeader.KeyAttr & CSSM_KEYATTR_EXTRACTABLE, "%s: check private key marked as extractable (as requested)", testName);
		if (!(cssmPrivKey->KeyHeader.KeyAttr & CSSM_KEYATTR_EXTRACTABLE)) {
            //errx(EXIT_FAILURE, "Oops! the private key was not marked as extractable!");
            return 1;
		}
	}
	else {
        ok(!(cssmPrivKey->KeyHeader.KeyAttr & CSSM_KEYATTR_EXTRACTABLE), "%s: check private key marked as non-extractable (as requested)", testName);
		if (cssmPrivKey->KeyHeader.KeyAttr & CSSM_KEYATTR_EXTRACTABLE) {
            //errx(EXIT_FAILURE, "Oops! the private key was marked as extractable!");
            return 1;
		}
	}

	SecKeyImportExportParameters keyParams;
	memset(&keyParams, 0, sizeof(keyParams));
	keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
	keyParams.passphrase = CFSTR("borken");

	CFDataRef exportedData = NULL;

    status = SecKeychainItemExport(privateKeyRef, kSecFormatWrappedPKCS8, 0, &keyParams, &exportedData);
    if(extractable) {
        ok_status(status, "%s: SecKeychainItemExport (PKCS8) (and we expected it to succeed)", testName);
    } else {
        is(status, errSecDataNotAvailable, "%s: SecKeychainItemExport (PKCS8) (and we expected this to fail with errSecDataNotAvailable)", testName);
    }

    status = SecKeychainItemExport(privateKeyRef, kSecFormatPKCS12, 0, &keyParams, &exportedData);
    if(extractable) {
        ok_status(status, "%s: SecKeychainItemExport(and we expected it to succeed)", testName);
    } else {
        is(status, errSecDataNotAvailable, "%s: SecKeychainItemExport (PKCS12)  (and we expected this to fail with errSecDataNotAvailable)", testName);
    }

	if (status != noErr) {
		if (extractable) {
			//errx(EXIT_FAILURE, "Unable to export extractable key! (err = %d)", status);
            return 1;
		}
		else {
			status = 0; // wasn't extractable, so this is the expected result
		}
	}
	else if (status == noErr && !extractable) {
		//errx(EXIT_FAILURE, "Was able to export non-extractable key! (err = %d)", status);
        return 1;
	}

	status = SecKeychainItemDelete((SecKeychainItemRef)publicKeyRef);
    ok_status(status, "%s: SecKeychainItemDelete", testName);
	if (status != noErr) {
		warnx("Unable to delete created public key from keychain (err = %d)", (int)status);
	}

	status = SecKeychainItemDelete((SecKeychainItemRef)privateKeyRef);
    ok_status(status, "%s: SecKeychainItemDelete", testName);
	if (status != noErr) {
		warnx("Unable to delete created private key from keychain (err = %d)", (int)status);
	}

	CFRelease(publicKeyRef);
	CFRelease(privateKeyRef);

	return 0;
}


int kc_27_key_non_extractable(int argc, char *const *argv)
{
    plan_tests(24);
    initializeKeychainTests(__FUNCTION__);

    SecKeychainRef kc = getPopulatedTestKeychain();

	// test case 1: extractable
    startTest("Extract extractable key");
    testExtractable(kc, TRUE, TRUE);

	// test case 2: non-extractable
    startTest("Extract non-extractable key");
    testExtractable(kc, FALSE, TRUE);

	// test case 3: extractable (when not explicitly specified)
    startTest("Extract implicitly extractable key");
    testExtractable(kc, TRUE, FALSE);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", testName);
    CFRelease(kc);

    deleteTestFiles();
    return 0;
}

