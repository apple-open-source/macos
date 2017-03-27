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
#include "keychain_regressions.h"
#include "kc-helpers.h"
#include "kc-key-helpers.h"
#include "kc-keychain-file-helpers.h"

#include <stdlib.h>
#include <err.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

/****************************************************************/

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
	if (keychain) CFRelease(keychain);
	if (access) CFRelease(access);

	return status;
}

static SecAccessRef MakeNewAccess(SecKeychainItemRef item, CFStringRef accessLabel, Boolean allowAny)
{
	OSStatus status;
	SecAccessRef access = NULL;
	CFMutableArrayRef trustedApplications = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (!allowAny) // use default access control ("confirm access")
	{
		// Make an exception list of applications you want to trust,
		// which are allowed to access the item without requiring user confirmation.
		// In this example, the calling app and Mail will have access.
		SecTrustedApplicationRef myself = NULL, someOther = NULL;
		status = SecTrustedApplicationCreateFromPath(NULL, &myself);
        ok_status(status, "%s: MakeNewAccess: SecTrustedApplicationCreateFromPath (self)", testName);

		if (!status && myself) {
			CFArrayAppendValue(trustedApplications, myself);
			CFRelease(myself);
		}
		status = SecTrustedApplicationCreateFromPath("/Applications/Mail.app", &someOther);
        ok_status(status, "%s: MakeNewAccess: SecTrustedApplicationCreateFromPath (Mail.app)", testName);

		if (!status && someOther) {
			CFArrayAppendValue(trustedApplications, someOther);
			CFRelease(someOther);
		}
	}

	// If the keychain item already exists, use its access reference; otherwise, create a new one
	if (item) {
		status = SecKeychainItemCopyAccess(item, &access);
        ok_status(status, "%s: MakeNewAccess: SecKeychainItemCopyAccess", testName);
	} else {
		status = SecAccessCreate(accessLabel, trustedApplications, &access);
        ok_status(status, "%s: MakeNewAccess: SecAccessCreate", testName);
	}
	if (status) return NULL;

	// get the access control list for decryption operations (this controls access to an item's data)
	CFArrayRef aclList = NULL;
	status = SecAccessCopySelectedACLList(access, CSSM_ACL_AUTHORIZATION_DECRYPT, &aclList);
    ok_status(status, "%s: MakeNewAccess: SecAccessCopySelectedACLList", testName);
	if (!status)
	{
		// get the first entry in the access control list
		SecACLRef aclRef = (SecACLRef)CFArrayGetValueAtIndex(aclList, 0);
		CFArrayRef appList = NULL;
		CFStringRef promptDescription = NULL;
		CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR promptSelector;
		status = SecACLCopySimpleContents(aclRef, &appList, &promptDescription, &promptSelector);
        ok_status(status, "%s: MakeNewAccess: SecAccessCopySimpleContents", testName);

		if (allowAny) // "allow all applications to access this item"
		{
			// change the decryption ACL to not require the passphrase, and have a NULL application list.
			promptSelector.flags &= ~CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE;
			status = SecACLSetSimpleContents(aclRef, NULL, promptDescription, &promptSelector);
            ok_status(status, "%s: MakeNewAccess: SecACLSetSimpleContents", testName);
		}
		else // "allow access by these applications"
		{
			// modify the application list
			status = SecACLSetSimpleContents(aclRef, trustedApplications, promptDescription, &promptSelector);
            ok_status(status, "%s: MakeNewAccess: SecACLSetSimpleContents", testName);
		}

		if (appList) CFRelease(appList);
		if (promptDescription) CFRelease(promptDescription);
	}
	if (aclList) CFRelease(aclList);
	if (trustedApplications) CFRelease(trustedApplications);

	return access;
}

static int testCopyKey(SecKeychainRef userKeychain, SecKeychainRef tempKeychain)
{
	OSStatus status;
	SecAccessRef access = NULL;
	SecKeyRef publicKeyRef = NULL;
	SecKeyRef privateKeyRef = NULL;
	CFStringRef label = CFSTR("Test Key Copied To Keychain");

	if (!tempKeychain) {
		warnc(EXIT_FAILURE, "Failed to make a new temporary keychain!");
	}

	// generate key pair in temporary keychain
    status = GenerateRSAKeyPair(tempKeychain,
    							label,
								2048, // size
								NULL, // implicitly extractable
								&publicKeyRef,
								&privateKeyRef);

	if (status != errSecSuccess) {
		warnc(EXIT_FAILURE, "Unable to get key pair (error %d)", (int)status);
	}

	// export private key from temp keychain to a wrapped data blob
	CFDataRef exportedData = NULL;
	CFStringRef tempPassword = CFSTR("MY_TEMPORARY_PASSWORD");

	SecItemImportExportKeyParameters keyParams;
	memset(&keyParams, 0, sizeof(keyParams));
	keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
	keyParams.passphrase = tempPassword;

	status = SecItemExport(privateKeyRef, kSecFormatWrappedPKCS8, 0, &keyParams, &exportedData);
    ok_status(status, "%s: SecItemExport", testName);

	if (!exportedData || status != noErr) {
		errx(EXIT_FAILURE, "Unable to export key! (error %d)", (int)status);
	}

	// set up an explicit access control instance for the imported key
	// (this example allows unrestricted access to any application)
	access = MakeNewAccess(NULL, label, true);
	keyParams.accessRef = access;

	// import wrapped data blob to user keychain
	SecExternalFormat format = kSecFormatWrappedPKCS8;
	SecExternalItemType itemType = kSecItemTypePrivateKey;

	CFArrayRef importedItems = NULL;
	status = SecItemImport(exportedData, NULL, &format, &itemType, 0, &keyParams, userKeychain, &importedItems);
    ok_status(status, "%s: SecItemImport", testName);

	if (status != noErr) {
		warnc(EXIT_FAILURE, "Unable to import key! (error %d)", (int)status);
	}
	if (importedItems) {

		// make sure to set a label on our newly imported key, since a label is not part of the PKCS8 format.
		SecKeyRef importedKey = (SecKeyRef) CFArrayGetValueAtIndex(importedItems, 0);
		if (CFGetTypeID(importedKey) == SecKeyGetTypeID()) {
			// set up a query defining the item(s) to be operated on, in this case, one item uniquely identified by reference
			CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
										&kCFTypeDictionaryValueCallBacks);
			CFDictionaryAddValue( query, kSecClass, kSecClassKey ); // item class is a required attribute in any query
			CFDictionaryAddValue( query, kSecValueRef, importedKey );

			// define the attributes to be updated, in this case the label
			CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
										&kCFTypeDictionaryValueCallBacks);
			CFDictionaryAddValue( attrs, kSecAttrLabel, label );

			// do the update
			status = SecItemUpdate( query, attrs );
            ok_status(status, "%s: SecItemUpdate", testName);

			if (status != errSecSuccess) {
				warnc(EXIT_FAILURE, "Failed to update label of imported key! (error %d)", (int)status);
			}

			CFRelease(query);
			CFRelease(attrs);
		}
		CFRelease(importedItems);
	}

    // ensure that key was copied, and its label changed
    checkN(testName, createQueryKeyDictionaryWithLabel(userKeychain, kSecAttrKeyClassPrivate, label), 1);

	if (access) CFRelease(access);

	return 0;
}


int kc_24_key_copy_keychain(int argc, char *const *argv)
{
    plan_tests(18);
    initializeKeychainTests(__FUNCTION__);

    SecKeychainRef keychain = getPopulatedTestKeychain();
    SecKeychainRef blankKeychain = createNewKeychain("forKeys", "password");

    testCopyKey(keychain, blankKeychain);

    ok_status(SecKeychainDelete(keychain), "%s: SecKeychainDelete", testName);
    ok_status(SecKeychainDelete(blankKeychain), "%s: SecKeychainDelete", testName);

    CFReleaseNull(keychain);
    CFReleaseNull(blankKeychain);

    checkPrompts(0, "No prompts while importing items");

    deleteTestFiles();
    return 0;
}

