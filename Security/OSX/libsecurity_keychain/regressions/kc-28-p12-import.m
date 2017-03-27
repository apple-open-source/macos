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
#include "kc-item-helpers.h"
#include "kc-key-helpers.h"
#include "kc-identity-helpers.h"

#import <Foundation/Foundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Security/oidscert.h>
#include <Security/oidsattr.h>
#include <Security/oidsalg.h>
#include <Security/x509defs.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#include <Security/certextensions.h>

#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecImportExport.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecKey.h>
#include <Security/SecCertificate.h>
#include <Security/SecItem.h>

// Turn off deprecated API warnings
//#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static void
verifyPrivateKeyExtractability(BOOL extractable, NSArray *items)
{
	// After importing items, check that private keys (if any) have
	// the expected extractable attribute value.

	CFIndex count = [items count];
    is(count, 1, "One identity added");

	for (id item in items)
	{
		OSStatus status;
		SecKeyRef aKey = NULL;
		if (SecKeyGetTypeID() == CFGetTypeID((CFTypeRef)item)) {
			aKey = (SecKeyRef) CFRetain((CFTypeRef)item);
			fprintf(stdout, "Verifying imported SecKey\n");
		}
		else if (SecIdentityGetTypeID() == CFGetTypeID((CFTypeRef)item)) {
			status = SecIdentityCopyPrivateKey((SecIdentityRef)item, &aKey);
            ok_status(status, "%s: SecIdentityCopyPrivateKey", testName);
		}

        ok(aKey, "%s: Have a key to test", testName);

		if (aKey)
		{
			const CSSM_KEY *cssmKey;
			OSStatus status = SecKeyGetCSSMKey(aKey, &cssmKey);
            ok_status(status, "%s: SecKeyGetCSSMKey", testName);
			if (status != noErr) {
				continue;
			}
            is(cssmKey->KeyHeader.KeyClass, CSSM_KEYCLASS_PRIVATE_KEY, "%s: key is private key", testName);

			if (!(cssmKey->KeyHeader.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY)) {
				fprintf(stdout, "Skipping non-private key (KeyClass=%d)\n", cssmKey->KeyHeader.KeyClass);
				continue; // only checking private keys
			}
			BOOL isExtractable = (cssmKey->KeyHeader.KeyAttr & CSSM_KEYATTR_EXTRACTABLE) ? YES : NO;
            is(isExtractable, extractable, "%s: key extractability matches expectations", testName);

			CFRelease(aKey);
		}
	}
}

static void
setIdentityPreferenceForImportedIdentity(SecKeychainRef importKeychain, NSString *name, NSArray *items)
{
    CFArrayRef importedItems = (__bridge CFArrayRef)items;

    if (importedItems)
    {
        SecIdentityRef importedIdRef = NULL;
        CFIndex dex, numItems = CFArrayGetCount(importedItems);
        for(dex=0; dex<numItems; dex++)
        {
            CFTypeRef item = CFArrayGetValueAtIndex(importedItems, dex);
            if(CFGetTypeID(item) == SecIdentityGetTypeID())
            {
                OSStatus status = noErr;
                importedIdRef = (SecIdentityRef)item;

                status = SecIdentitySetPreference(importedIdRef, (CFStringRef)name, (CSSM_KEYUSE)0);
                ok_status(status, "%s: SecIdentitySetPreference", testName);
                break;
            }
        }
        ok(importedIdRef, "%s: identity found?", testName);
    }
    else
    {
        fail("%s: no items passed to setIdentityPreferenceForImportedIdentity", testName);
        pass("test numbers match");
    }
}

static void removeIdentityPreference(bool test) {
    // Clean up the identity preference, since it's in the default keychain
    CFMutableDictionaryRef q = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(q, kSecClass, kSecClassGenericPassword);
    q = addLabel(q, CFSTR("kc-28-p12-import@apple.com"));

    if(test) {
        ok_status(SecItemDelete(q), "%s: SecItemDelete (identity preference)", testName);
    } else {
        // Our caller doesn't care if this works or not.
        SecItemDelete(q);
    }
    CFReleaseNull(q);
}


static OSStatus
testP12Import(BOOL extractable, SecKeychainRef keychain, const char *p12Path, CFStringRef password, bool useDeprecatedAPI)
{
	OSStatus status = paramErr;

	NSString *file = [NSString stringWithUTF8String:p12Path];
	NSData *p12Data = [[NSData alloc] initWithContentsOfFile:file];
	NSArray *keyAttrs = nil;
	CFArrayRef outItems = nil;

	SecExternalFormat externFormat = kSecFormatPKCS12;
	SecExternalItemType	itemType = kSecItemTypeAggregate; // certificates and keys

	// Decide which parameter structure to use.
	SecKeyImportExportParameters keyParamsOld;	// for SecKeychainItemImport, deprecated as of 10.7
	SecItemImportExportKeyParameters keyParamsNew; // for SecItemImport, 10.7 and later

	void *keyParamsPtr = (useDeprecatedAPI) ? (void*)&keyParamsOld : (void*)&keyParamsNew;

	if (useDeprecatedAPI) // SecKeychainItemImport, deprecated as of 10.7
	{
		SecKeyImportExportParameters *keyParams = (SecKeyImportExportParameters *)keyParamsPtr;
		memset(keyParams, 0, sizeof(SecKeyImportExportParameters));
		keyParams->version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
        keyParams->passphrase = password;
		if (!extractable)
		{
			// explicitly set the key attributes, omitting the CSSM_KEYATTR_EXTRACTABLE bit
			keyParams->keyAttributes = CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE;
		}
	}
	else // SecItemImport, 10.7 and later (preferred interface)
	{
		SecItemImportExportKeyParameters *keyParams = (SecItemImportExportKeyParameters *)keyParamsPtr;
		memset(keyParams, 0, sizeof(SecItemImportExportKeyParameters));
		keyParams->version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
        keyParams->passphrase = password;
		if (!extractable)
		{
			// explicitly set the key attributes, omitting kSecAttrIsExtractable
			keyAttrs = [[NSArray alloc] initWithObjects: (id) kSecAttrIsPermanent, kSecAttrIsSensitive, nil];
			keyParams->keyAttributes = (__bridge_retained CFArrayRef) keyAttrs;
		}
	}

    if (useDeprecatedAPI) // SecKeychainItemImport, deprecated as of 10.7
    {
        status = SecKeychainItemImport((CFDataRef)p12Data,
                                        NULL,
                                        &externFormat,
                                        &itemType,
                                        0,		/* flags not used (yet) */
                                        keyParamsPtr,
                                        keychain,
                                        (CFArrayRef*)&outItems);
        ok_status(status, "%s: SecKeychainItemImport", testName);
    }
    else // SecItemImport
    {
        status = SecItemImport((CFDataRef)p12Data,
                                        NULL,
                                        &externFormat,
                                        &itemType,
                                        0,		/* flags not used (yet) */
                                        keyParamsPtr,
                                        keychain,
                                        (CFArrayRef*)&outItems);
        ok_status(status, "%s: SecItemImport", testName);
    }

	verifyPrivateKeyExtractability(extractable, (__bridge NSArray*) outItems);

    checkN(testName, createQueryKeyDictionaryWithLabel(keychain, kSecAttrKeyClassPrivate, CFSTR("test_import")), 1);
    checkN(testName, addLabel(makeBaseQueryDictionary(keychain, kSecClassCertificate), CFSTR("test_import")), 1);

    setIdentityPreferenceForImportedIdentity(keychain, @"kc-28-p12-import@apple.com", (__bridge NSArray*) outItems);

    deleteItems(outItems);

    CFReleaseNull(outItems);

	return status;
}

int kc_28_p12_import(int argc, char *const *argv)
{
    plan_tests(70);
    initializeKeychainTests(__FUNCTION__);

    SecKeychainRef kc = getPopulatedTestKeychain();

    removeIdentityPreference(false); // if there's still an identity preference in the keychain, we'll get prompts. Delete it pre-emptively (but don't test about it)

    writeFile(keychainTempFile, test_import_p12, test_import_p12_len);
    testP12Import(true, kc, keychainTempFile, CFSTR("password"), false);
    testP12Import(true, kc, keychainTempFile, CFSTR("password"), true);

    testP12Import(false, kc, keychainTempFile, CFSTR("password"), false);
    testP12Import(false, kc, keychainTempFile, CFSTR("password"), true);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", testName);
    CFReleaseNull(kc);

    removeIdentityPreference(true);

    checkPrompts(0, "No prompts while importing items");

    deleteTestFiles();
	return 0;
}
