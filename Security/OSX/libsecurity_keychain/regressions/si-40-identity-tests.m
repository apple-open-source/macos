/*
 * Copyright (c) 2020-2021 Apple Inc. All Rights Reserved.
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
 *
 */

#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <TargetConditionals.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "keychain_regressions.h"
#include <utilities/SecCFRelease.h>

#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecKeychain.h>
#include <Security/SecCertificateRequest.h>
#include <Security/SecRSAKey.h>
#include "OSX/utilities/SecCFWrappers.h"
#include "OSX/sec/Security/SecFramework.h"

#include "si-40-identity-tests_data.h"

/* entry point prototype */
int si_40_identity_tests(int argc, char *const *argv);

static void tests(void) {
    OSStatus status = 0;
    CFDataRef p12Blob = NULL;
    isnt(p12Blob = (__bridge CFDataRef)[NSData dataWithBytes:test_p12 length:sizeof(test_p12)], NULL, "copy test_p12");

    CFArrayRef items = NULL; /* return value is retained, must release it */
    SecKeychainRef keychain = NULL; /* return value is retained, must release it */
    status = SecKeychainCopyDefault(&keychain);
    is(status, errSecSuccess, "keychain status");
    isnt(keychain, NULL, "no default keychain");

    CFMutableDictionaryRef options = NULL; /* return value is retained, must release it */
    options = CFDictionaryCreateMutableForCFTypes(NULL);
    isnt(options, NULL, "no options dictionary");
    CFDictionaryAddValue(options, kSecImportExportPassphrase, CFSTR("test"));
    CFDictionaryAddValue(options, kSecImportExportKeychain, keychain);

    status = SecPKCS12Import(p12Blob, options, &items);
    if (status == errSecDuplicateItem) {
        status = errSecSuccess; // ok if it already exists
    }
    is(status, errSecSuccess, "import p12 status");
    isnt(items, NULL, "import p12 items");

    NSDictionary *itemDict = (__bridge NSDictionary*)CFArrayGetValueAtIndex(items, 0);
    SecIdentityRef identity = (__bridge SecIdentityRef)itemDict[(__bridge NSString*)kSecImportItemIdentity];
    isnt(identity, NULL, "import identity");

    SecIdentityRef foundIdentity = NULL; /* return value is retained, must release it */

    // PLAIN NAMES: make sure these test cases produce identity preference items
    // which are not per-application.

    NSString *plainNameOne = @"Test Identity Preference Item";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameOne, NULL);
    is(status, errSecSuccess, "set preferred identity with plain name containing spaces");

    NSString *plainNameTwo = @"Test.Identity.Preference.Item";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameTwo, NULL);
    is(status, errSecSuccess, "set preferred identity with plain name containing dots");

    NSString *plainNameThree = @"@Test.Identity.Preference.Item";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameThree, NULL);
    is(status, errSecSuccess, "set preferred identity with plain name containing at-sign");

    NSString *plainNameFour = @"TestIdentityPreferenceItem";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameFour, NULL);
    is(status, errSecSuccess, "set preferred identity with plain name containing no spaces");

    NSString *plainNameFive = @"*.Identity.Preference.Item";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameFive, NULL);
    is(status, errSecSuccess, "set preferred identity with plain name wildcard entry");

    NSString *plainNameSix = @"si-40-identity-tests@apple.com";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameSix, NULL);
    is(status, errSecSuccess, "set preferred identity with RFC822 email address");

    status = SecIdentityDeleteApplicationPreferenceItems();
    if (status == errSecItemNotFound) {
        // it's ok if there were no per-app items found to delete at this point.
        // We are only testing that calling this function does NOT delete any of the
        // plain name preferences we created above.
        status = errSecSuccess;
    }
    is(status, errSecSuccess, "per-app preference item deletion failed with unexpected error");

    // check that the plain name prefs exist and survived the per-app item deletion.
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameOne, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 1 should not be deleted");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameTwo, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 2 should not be deleted");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameThree, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 3 should not be deleted");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameFour, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 4 should not be deleted");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameFive, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 5 should not be deleted");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameSix, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 6 should not be deleted");
    CFReleaseNull(foundIdentity);

    // clear the plain name prefs
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameOne, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 1");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameOne, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 1 found after being cleared");
    CFReleaseNull(foundIdentity);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameTwo, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 2");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameTwo, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 2 found after being cleared");
    CFReleaseNull(foundIdentity);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameThree, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 3");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameThree, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 3 found after being cleared");
    CFReleaseNull(foundIdentity);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameFour, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 4");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameFour, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 4 found after being cleared");
    CFReleaseNull(foundIdentity);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameFive, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 5");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameFive, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 5 found after being cleared");
    CFReleaseNull(foundIdentity);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameSix, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 6");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameSix, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 6 found after being cleared");
    CFReleaseNull(foundIdentity);

    //
    // URL NAMES: make sure that these produce per-app preference items.
    //

    NSString *uriNameOne = @"https://test-pref.apple.com/";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)uriNameOne, NULL);
    is(status, errSecSuccess, "set preferred identity with uri name 1");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriNameOne, NULL, NULL);
    isnt(foundIdentity, NULL, "preferred identity 1 not found after being set");
    CFReleaseNull(foundIdentity);

    NSString *uriNameTwo = @"ldaps://test-pref.apple.com/cn=si-40-identity-tests";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)uriNameTwo, NULL);
    is(status, errSecSuccess, "set preferred identity with uri name 2");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriNameTwo, NULL, NULL);
    isnt(foundIdentity, NULL, "preferred identity 2 not found after being set");
    CFReleaseNull(foundIdentity);

    // Check that the new API returns our per-app URLs.
    // We expect to retrieve both of the URLs that we just added.
    NSArray *urls = CFBridgingRelease(SecIdentityCopyApplicationPreferenceItemURLs());
    NSInteger foundURLs = 0, urlCount = [urls count];
    OSStatus spStatus = errSecSuccess;
    for (NSInteger idx = 0; idx < urlCount; idx++) {
        NSURL *url = (NSURL*)[urls objectAtIndex:idx];
        NSString *urlString = [url absoluteString];
        if ([uriNameOne isEqualToString:urlString]) {
            // Make sure that we can use the URL to remove the preference by name
            spStatus = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)urlString, NULL);
            foundURLs++;
        }
        if ([uriNameTwo isEqualToString:urlString]) {
            // Note that we found this one but don't remove it yet
            foundURLs++;
        }
    }
    is(foundURLs, 2, "identity URLs not returned after being set");
    is(spStatus, errSecSuccess, "could not remove preferred identity with url name");
    NSArray *urlsTwo = CFBridgingRelease(SecIdentityCopyApplicationPreferenceItemURLs());
    is([urlsTwo count], [urls count]-1, "wrong number of URLs returned after removing one pref");

    // Check that the new API deletes all of our per-app URL preference items.
    // We always expect errSecSuccess here, since we know we have items to delete.
    status = SecIdentityDeleteApplicationPreferenceItems();
    is(status, errSecSuccess, "should find and delete our app uri preference items");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriNameOne, NULL, NULL);
    is(foundIdentity, NULL, "preferred identity 1 should not be found after deleting prefs");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriNameTwo, NULL, NULL);
    is(foundIdentity, NULL, "preferred identity 2 should not be found after deleting prefs");
    CFReleaseNull(foundIdentity);

    //
    // WILDCARD NAMES: make sure these are still supported for URL name lookups
    //

    // add wildcard entry
    NSString *wildcardNameOne = @"*.test-pref-subdomain.apple.com";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)wildcardNameOne, NULL);
    is(status, errSecSuccess, "set preferred identity for wildcard name 1");
    // check that preferred identity is found for URI matching this wildcard
    NSString *uriWildcardMatchOne = @"https://match.test-pref-subdomain.apple.com";
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriWildcardMatchOne, NULL, NULL);
    isnt(foundIdentity, NULL, "preferred identity not found for wildcard match 1");
    CFReleaseNull(foundIdentity);
    // clear wildcard entry, then check that match is not found
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)wildcardNameOne, NULL);
    is(status, errSecSuccess, "clear preferred identity for wildcard name 1");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriWildcardMatchOne, NULL, NULL);
    is(foundIdentity, NULL, "preferred identity found after wildcard name 1 was cleared");
    CFReleaseNull(foundIdentity);


    CFReleaseNull(options);
    CFReleaseNull(items);
    CFReleaseNull(keychain);
}

static void run_preference_test(SecIdentityRef identity, NSString *test_name) {
    OSStatus status = errSecSuccess;
    // Set preference
    NSString *name = @"Test.Identity.Preference.Item";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)name, NULL);
    is(status, errSecSuccess, "%@: set preferred identity with plain name containing dots", test_name);

    // get identity back (won't be same type b/c now it's an CDSA key...)
    SecIdentityRef foundIdentity = NULL; /* return value is retained, must release it */
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)name, NULL, NULL);
    isnt(foundIdentity, NULL, "%@: plain name identity preference should not be deleted", test_name);
    CFReleaseNull(foundIdentity);

    // clear preference
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)name, NULL);
    is(status, errSecSuccess, "%@: clear preferred identity with plain name", test_name);

    // verify cleared
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)name, NULL, NULL);
    is(foundIdentity, NULL, "%@: plain name identity preference found after being cleared", test_name);
    CFReleaseNull(foundIdentity);

    // set preference again without error
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)name, NULL);
    is(status, errSecSuccess, "%@: set preferred identity with plain name containing dots", test_name);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)name, NULL, NULL);
    isnt(foundIdentity, NULL, "%@: plain name identity preference should not be deleted", test_name);
    CFReleaseNull(foundIdentity);

    // update preference without error
    NSString *name2 = @"*.Identity.Preference.Item";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)name2, NULL);
    is(status, errSecSuccess, "%@: set preferred identity with plain name containing dots", test_name);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)name2, NULL, NULL);
    isnt(foundIdentity, NULL, "%@: plain name identity preference should not be deleted", test_name);
    CFReleaseNull(foundIdentity);

    // clear preferences
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)name, NULL);
    is(status, errSecSuccess, "%@: clear preferred identity with plain name", test_name);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)name2, NULL);
    is(status, errSecSuccess, "%@: clear preferred identity with plain name", test_name);
}

static CF_RETURNS_RETAINED SecCertificateRef generateSelfSignedCert(NSString *name, SecKeyRef privateKey) {
    SecCertificateRef cert = NULL;
    NSArray *ca_rdns = @[
         @[@[(__bridge NSString*)kSecOidCommonName, name]]
     ];
    NSDictionary *ca_parameters = @{
        (__bridge NSString *)kSecCMSSignHashAlgorithm: (__bridge NSString*)kSecCMSHashingAlgorithmSHA256,
        (__bridge NSString *)kSecCSRBasicContraintsPathLen: @0,
        (__bridge NSString *)kSecCertificateKeyUsage: @(kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign),
    };
    cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)ca_rdns,
                                               (__bridge CFDictionaryRef)ca_parameters,
                                               NULL, privateKey);
    return cert;
}

static void deleteIdentity(SecKeyRef key, SecCertificateRef cert, NSDictionary *_Nullable additionalKeychainOptions) {
    // Delete key from keychain
    NSMutableDictionary *deleteAttrs = [@{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrKeyClass : (id)kSecAttrKeyClassPrivate,
        (id)kSecValueRef : (__bridge id)key,
    } mutableCopy];
    if (additionalKeychainOptions) {
        [deleteAttrs addEntriesFromDictionary:additionalKeychainOptions];
    }
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)deleteAttrs);
    is(status, errSecSuccess);

    // Delete Cert from keychain
    NSData *importedSN = CFBridgingRelease(SecCertificateCopySerialNumberData(cert,NULL));
    NSData *importedIssuer = CFBridgingRelease(SecCertificateCopyNormalizedIssuerSequence(cert));
   deleteAttrs = [@{
        (id)kSecClass : (id)kSecClassCertificate,
        (id)kSecAttrIssuer : importedIssuer,
        (id)kSecAttrSerialNumber : importedSN,
    } mutableCopy];
    if (additionalKeychainOptions) {
        [deleteAttrs addEntriesFromDictionary:additionalKeychainOptions];
    }
    if (deleteAttrs[(id)kSecAttrTokenID]) {
        [deleteAttrs removeObjectForKey:(id)kSecAttrTokenID];
    }
    status = SecItemDelete((__bridge CFDictionaryRef)deleteAttrs);
    is(status, errSecSuccess);
}

static void testMemoryIdentity(void) {
    SecKeyRef key = NULL;
    SecCertificateRef cert = NULL;
    SecIdentityRef identity = NULL;

    isnt(key = SecKeyCreateRSAPrivateKey(NULL, _rsa_key, sizeof(_rsa_key), kSecKeyEncodingPkcs1), NULL, "create private key");

    /* Generate a self-signed rsa cert */
    cert = generateSelfSignedCert(@"In-memory Preferred Cert", key);
    identity = SecIdentityCreate(NULL, cert, key);

    run_preference_test(identity, @"In-Memory Identity");

    // Delete Key (that was added to keychain during preference test)
    SecIdentityRef cdsaIdentity = NULL;
    SecIdentityCreateWithCertificate(NULL, cert, &cdsaIdentity);
    if (cdsaIdentity) {
        SecKeyRef cdsaKey = NULL;
        SecIdentityCopyPrivateKey(cdsaIdentity, &cdsaKey);
        deleteIdentity(cdsaKey, cert, nil);
        CFReleaseNull(cdsaKey);
    }

    CFReleaseNull(identity);
    CFReleaseNull(cert);
    CFReleaseNull(key);
}

static void testFileBackedKeychainIdentity(void) {
    SecKeyRef key = NULL;
    SecKeyRef publicKey = NULL;
    SecCertificateRef cert = NULL;
    SecIdentityRef identity = NULL;

    NSDictionary *parameters = nil;
    NSDictionary *addAttributes = nil;

    // Create Key
    parameters = @{
        (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom,
        (__bridge NSString*)kSecAttrKeySizeInBits : @384,
    };
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)parameters, &publicKey, &key),
              "Failed to generate file-backed keychain key");
    CFReleaseNull(publicKey);

    // Generate Cert and add to keychain
    cert = generateSelfSignedCert(@"File-backed Preferred Identity", key);
    addAttributes = @{
        (id)kSecClass : (id)kSecClassCertificate,
        (id)kSecValueRef : (__bridge id)cert,
    };
    is(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef)addAttributes, NULL));

    identity = SecIdentityCreate(NULL, cert, key);

    // Run test
    run_preference_test(identity, @"File-Backed Keychain Identity");

    deleteIdentity(key, cert, nil);
    CFReleaseNull(cert);
    CFReleaseNull(key);
    CFReleaseNull(identity);
}

static void testDataProtectionKeychainIdentity(void) {
    SecKeyRef key = NULL;
    SecKeyRef publicKey = NULL;
    SecCertificateRef cert = NULL;
    SecIdentityRef identity = NULL;

    NSDictionary *parameters = nil;
    NSDictionary *addAttributes = nil;

    // Generate Key and add to DP Keychain
    parameters = @{
        (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom,
        (__bridge NSString*)kSecAttrKeySizeInBits : @384,
        (__bridge NSString*)kSecUseDataProtectionKeychain : @YES,
    };
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)parameters, &publicKey, &key),
              "Failed to generate data protection keychcain key");
    CFReleaseNull(publicKey);

    addAttributes = @{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecValueRef : (__bridge id)key,
        (id)kSecUseDataProtectionKeychain : @YES,
    };
    is(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef)addAttributes, NULL));

    // Generate cert and add to DP Keychain
    cert = generateSelfSignedCert(@"Data Protection Preferred Identity", key);
    addAttributes = @{
        (id)kSecClass : (id)kSecClassCertificate,
        (id)kSecValueRef : (__bridge id)cert,
        (id)kSecUseDataProtectionKeychain : @YES,
    };
    is(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef)addAttributes, NULL));
    
    identity = SecIdentityCreate(NULL, cert, key);

    // Run test
    run_preference_test(identity, @"Data Protection Keychain Identity");

    deleteIdentity(key, cert, @{ (id)kSecUseDataProtectionKeychain : @YES });
    CFReleaseNull(cert);
    CFReleaseNull(key);
    CFReleaseNull(identity);
}

static void testSystemDataProtectionKeychainIdentity(void) {
    SecKeyRef key = NULL;
    SecKeyRef publicKey = NULL;
    SecCertificateRef cert = NULL;
    SecIdentityRef identity = NULL;

    NSDictionary *parameters = nil;
    NSDictionary *addAttributes = nil;

    // Generate Key and add to System DP Keychain
    parameters = @{
        (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom,
        (__bridge NSString*)kSecAttrKeySizeInBits : @384,
        (__bridge NSString*)kSecUseDataProtectionKeychain : @YES,
        (__bridge NSString*)kSecUseSystemKeychainAlways : @YES,
    };
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)parameters, &publicKey, &key),
              "Failed to generate system data protection key");
    CFReleaseNull(publicKey);

    addAttributes = @{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecValueRef : (__bridge id)key,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecUseSystemKeychainAlways : @YES,
    };
    is(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef)addAttributes, NULL));

    // Generate cert and add to System DP Keychain
    cert = generateSelfSignedCert(@"System Data Protection Preferred Identity", key);
    addAttributes = @{
        (id)kSecClass : (id)kSecClassCertificate,
        (id)kSecValueRef : (__bridge id)cert,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecUseSystemKeychainAlways : @YES,
    };
    is(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef)addAttributes, NULL));

    identity = SecIdentityCreate(NULL, cert, key);

    // Run test
    run_preference_test(identity, @"System Data Protection Keychain Identity");

    deleteIdentity(key, cert, @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecUseSystemKeychainAlways : @YES,
    });
    CFReleaseNull(cert);
    CFReleaseNull(key);
    CFReleaseNull(identity);
}

static void testTokenBackedKeychainIdentity(void) {
    SecKeyRef key = NULL;
    SecKeyRef publicKey = NULL;
    SecCertificateRef cert = NULL;
    SecIdentityRef identity = NULL;

    NSDictionary *parameters = nil;
    NSDictionary *addAttributes = nil;

    // Generate SEP Key
    parameters = @{
        (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom,
        (__bridge NSString*)kSecAttrKeySizeInBits : @256,
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (__bridge NSString*)kSecUseDataProtectionKeychain : @YES,
    };
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)parameters, &publicKey, &key),
              "Failed to generate SEP key");
    CFReleaseNull(publicKey);

    cert = generateSelfSignedCert(@"SEP Preferred Identity", key);
    addAttributes = @{
        (id)kSecClass : (id)kSecClassCertificate,
        (id)kSecValueRef : (__bridge id)cert,
        (id)kSecUseDataProtectionKeychain : @YES,
    };
    is(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef)addAttributes, NULL));

    identity = SecIdentityCreate(NULL, cert, key);

    // Run test
    run_preference_test(identity, @"SEP-backed Identity");

    deleteIdentity(key, cert, @{
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (__bridge NSString*)kSecUseDataProtectionKeychain : @YES,
    });
    CFReleaseNull(cert);
    CFReleaseNull(key);
    CFReleaseNull(identity);
}

static void test_preferences_for_identity_types(void) {
    testMemoryIdentity();
    testFileBackedKeychainIdentity();
    testDataProtectionKeychainIdentity();
    testSystemDataProtectionKeychainIdentity();
    testTokenBackedKeychainIdentity();
}

static void test_no_identity_for_mismatch_keys(void) {
    SecCertificateRef cert = NULL;
    SecKeyRef realPrivKey = NULL;
    SecIdentityRef identity = NULL;

    isnt(cert = SecCertificateCreateWithBytes(NULL, _uranus_rsa_cert, sizeof(_uranus_rsa_cert)),
            NULL, "create certificate");
    isnt(realPrivKey = SecKeyCreateRSAPrivateKey(NULL, _rsa_key, sizeof(_rsa_key),
                kSecKeyEncodingPkcs1), NULL, "create private key");

    isnt(identity = SecIdentityCreate(NULL, cert, realPrivKey), NULL, "create identity");
    CFReleaseNull(identity);

    SecKeyRef wrongKey = NULL;
    SecKeyRef publicKey = NULL;
    NSDictionary *rsa_parameters = nil;

    rsa_parameters = @{
        (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeRSA,
        (__bridge NSString*)kSecAttrKeySizeInBits : @2048,
        (__bridge NSString*)kSecUseDataProtectionKeychain : @YES,
    };
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)rsa_parameters, &publicKey, &wrongKey),
              "Failed to generate RSA key");
    CFReleaseNull(publicKey);
    is(identity = SecIdentityCreate(NULL, cert, wrongKey), NULL, "No mismatched identity");
    CFReleaseNull(identity);

    CFReleaseNull(wrongKey);
    CFReleaseNull(cert);
    CFReleaseNull(realPrivKey);
}

int si_40_identity_tests(int argc, char *const *argv)
{
    plan_tests(122);

    tests();
    test_preferences_for_identity_types();
    test_no_identity_for_mismatch_keys();

    return 0;
}
