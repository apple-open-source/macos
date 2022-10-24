/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

#import <AssertMacros.h>
#import <Foundation/Foundation.h>

#include <Security/SecKey.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecCMS.h>
#include <Security/SecCertificateRequest.h>
#include <Security/SecSCEP.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>
#include <utilities/array_size.h>

#include <Security/SecInternal.h>
#include <CoreFoundation/CoreFoundation.h>
#include "MobileGestalt.h"
#include <stdlib.h>
#include <unistd.h>

#include "shared_regressions.h"

/* kSecOIDDescription constant exists, but is only defined on macOS */
const CFStringRef kSecOidDescription = CFSTR("2.5.4.13");

static SecIdentityRef createAttestationIdentity(void)
{
    SecIdentityRef identity = NULL;
    // %% TBA
    return identity;
}

static SecIdentityRef sharedAttestationIdentity(void)
{
    static SecIdentityRef sAttestationIdentity = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sAttestationIdentity = createAttestationIdentity();
    });
    return sAttestationIdentity;
}

static void displayKey(SecKeyRef key)
{
    CFShow(key);
}

static void displayCert(SecCertificateRef cert)
{
    NSString *subj = (__bridge_transfer NSString*)SecCertificateCopySubjectSummary(cert);
    NSData *serial = (__bridge_transfer NSData*)SecCertificateCopySerialNumberData(cert, NULL);
    NSLog(@"%@ (serial: %@)", subj, serial);
}

static NSArray* makeSubject(NSString* name, NSString* uuid)
{
    NSArray *subject = @[
        @[@[(__bridge NSString*)kSecOidCommonName, name]],
        @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
        @[@[(__bridge NSString*)kSecOidOrganization, @"My Test Organization"]],
        @[@[(__bridge NSString*)kSecOidDescription, uuid]],
    ];

    return subject;
}

static NSData* makeApplicationTag(NSNumber* keySize, bool hardwareBound)
{
    return [[NSString stringWithFormat:@"com.apple.security.test.%@%@", [keySize stringValue], (hardwareBound) ? @".SEP" : @""] dataUsingEncoding:NSUTF8StringEncoding];
}

static NSArray* makeEDAOids(bool anonymous, bool nonce)
{
    NSMutableArray *oids = [@[
      // Anonymous OIDs
      @"1.2.840.113635.100.8.10.1", /* OSVersion */
      @"1.2.840.113635.100.8.10.2", /* SEPOSVersion */
      @"1.2.840.113635.100.8.10.3", /* FirmwareVersion */
    ] mutableCopy];
    if (!anonymous) {
      // Device-identifying OIDs
      [oids addObjectsFromArray:@[
        @"1.2.840.113635.100.8.9.1", /* SerialNumber */
        @"1.2.840.113635.100.8.9.2", /* UDID */
      ]];
    }
    if (nonce) {
      [oids addObject:@"1.2.840.113635.100.8.11.1"]; /* Nonce */
    }
    return oids;
}

static NSArray* makeEDASubject(NSString* name, NSString* uuid)
{
    // Subject RDN fields supported by MDM profile:
    // C > ST > L > O > OU > CN > Email
    NSString *cn =  (name) ? name : @"TEST EDA SUBJECT";
    NSString *val = (uuid) ? uuid : [[NSUUID UUID] UUIDString];
    NSArray *subject = @[
        @[@[@"C", @"US"]],
        @[@[@"ST", @"CA"]],
        @[@[@"L", @"Cupertino"]],
        @[@[@"O", @"Apple Inc."]],
        @[@[@"OU", @"SEAR"]],
        @[@[@"CN", cn]],
        @[@[@"EMAIL", @"test@apple.com"]],
        @[@[@"1.2.840.113635.100.6.99999.99999", val]],
    ];

    return subject;
}

static NSArray* makeBadEDASubject(void)
{
    // Subject RDN fields supported by MDM profile:
    // C > ST > L > O > OU > CN > Email
    NSArray *subject = @[
        @[@[@"C", @"USA"]], /* bad country code, expect ISO3166 alpha-2 codes */
        @[@[@"ST", @"Californiaaa"]], /* this is bad form, but allowed */
        @[@[@"O", @"DMQAAAAAAATESTTTTTT12345"]],
        @[@[@"OU", @"APPLE_ORG_UNIT!"]],
        @[@[@"CN", @"Expected Failure: apple.test.org"]],
        @[@[@"EMAIL", @"sdasdasd@emailaddress.com"]],
        @[@[@"2.3.4.1", @"randomhehe"]],
    ];

    return subject;
}

static NSDictionary* makeEDASubjectAltName(void)
{
    return @{
        (id)kSecSubjectAltNameEmailAddress:@"test@apple.com",
        (id)kSecSubjectAltNameDNSName:@"qa.example.apple.com",
        (id)kSecSubjectAltNameURI:@"https://qa.example.apple.com",
        (id)kSecSubjectAltNameNTPrincipalName:@"TESTPRINCIPALNAME_QA",
    };
}

static NSDictionary* makeBadEDASubjectAltName(void)
{
    return @{
        (id)kSecSubjectAltNameEmailAddress:@"test@apple.com_rfc822", /* bad domain string */
        (id)kSecSubjectAltNameDNSName:@"qa.example.apple.com",
        (id)kSecSubjectAltNameURI:@"1823781273", /* bad URI */
        (id)kSecSubjectAltNameNTPrincipalName:@"TESTPRINCIPALNAME_QA",
    };
}

static NSArray* makeBadEKUArray(void)
{
    NSArray *result = nil;
    // these bad values are taken from rdar://91220369
    // result = @[ @"9.9.6.1.4.5.7.8.8.8", @"1.3.6.1.5.5.7.3.4" ];
    // result = @[ @"1.5.6.1.5.5.7.3.2", @"1.3.6.1.5.5.7.3.4", @"8.3.6.1.5.5.7.3.4" ];
    // result = @[ @"1.3.6.1.5.5.7.3.4", @"3.2.4.5", @"9.2.8.5.4", @"9.7.7.7.3.2" ];
    // result = @[ @"9.9.6.1.4.5.7", @"1.3.6.1.5.5.7.3.4" ];
    result = @[ @"1.3.6.1.5.5.7.3.4", @"912317238", @"123918" ];

    return result;
}

static NSDictionary* makeEDAParameters(NSString* label, NSURL* acmeServerURL, CFStringRef keyType, NSNumber* keySize, bool hardwareBound, bool minimalKeyAttrs, bool badValues)
{
    SecIdentityRef attestationIdentity = sharedAttestationIdentity(); /* will be used to attest our new hardware-bound key */
    NSString *clientIdentifier = [[NSUUID UUID] UUIDString]; /* random string intended as a single-use token. Does NOT go into the certificate. */
    NSDictionary *altName = (badValues) ? makeBadEDASubjectAltName() : makeEDASubjectAltName();
    NSArray *ekuArray = (badValues) ? makeBadEKUArray() : @[(__bridge NSString*)kSecEKUClientAuth];

    NSDictionary *baseParams = @{
        (id)kSecClientIdentifier : clientIdentifier,
        /* key creation attributes */
        (id)kSecUseHardwareBoundKey : (hardwareBound) ? (id)kCFBooleanTrue : (id)kCFBooleanFalse,
        (id)kSecAttrKeyType : (id)CFBridgingRelease(keyType),
        (id)kSecAttrKeySizeInBits : keySize,
        /* certificate creation attributes */
        (id)kSecCertificateLifetime : @(31536000), /* lifetime in seconds */
        (id)kSecCertificateKeyUsage : @(kSecKeyUsageDigitalSignature | kSecKeyUsageKeyEncipherment),
        (id)kSecCertificateExtendedKeyUsage : ekuArray,
        (__bridge NSString*)kSecCMSSignHashAlgorithm : (__bridge NSString*)kSecCMSHashingAlgorithmSHA256,
        (id)kSecSubjectAltName : altName,
    };
    NSMutableDictionary *parameters = [NSMutableDictionary dictionaryWithDictionary:baseParams];
    if (acmeServerURL) {
        parameters[(id)kSecACMEDirectoryURL] = (id)acmeServerURL;
        parameters[(id)kSecACMEPermitLocalIssuer] = @YES;
    }
    if (hardwareBound && attestationIdentity) {
        parameters[(id)kSecAttestationIdentity] = (id)CFBridgingRelease(attestationIdentity);
    }
    if (hardwareBound) {
        parameters[(id)kSecAttestationOids] = makeEDAOids(false, true);
    }
    if (!minimalKeyAttrs) {
        NSData *tag = makeApplicationTag(keySize, hardwareBound); /* non-user-visible tag for the private key */
        NSDictionary *baseKeyAttrs = @{
            (id)kSecAttrIsPermanent : (id)kCFBooleanTrue,
            (id)kSecAttrApplicationTag : tag,
            (id)kSecAttrLabel : label,
        };
        NSMutableDictionary *privateKeyAttrs = [NSMutableDictionary dictionaryWithDictionary:baseKeyAttrs];
        if (hardwareBound) {
            /* add access control, per developer documentation for secure enclave keys: https://developer.apple.com/documentation/security/certificate_key_and_trust_services/keys/storing_keys_in_the_secure_enclave */
            SecAccessControlRef access = SecAccessControlCreateWithFlags(kCFAllocatorDefault,
                                                                         kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
                                                                         kSecAccessControlPrivateKeyUsage,
                                                                         NULL); /* ignore error */
            if (access) {
                parameters[(id)kSecAttrAccessControl] = (id)CFBridgingRelease(access);
            }
        } else {
            /* for software-only keys, add sensitive and non-extractable attributes */
            privateKeyAttrs[(id)kSecAttrIsSensitive] = (id)kCFBooleanTrue;
            privateKeyAttrs[(id)kSecAttrIsExtractable] = (id)kCFBooleanFalse;
        }
        parameters[(id)kSecPrivateKeyAttrs] = privateKeyAttrs;
    }

    return parameters;
}

static NSDictionary* makeParameters(NSString* label, NSURL* acmeServerURL, CFStringRef keyType, NSNumber* keySize, bool hardwareBound, bool minimalKeyAttrs)
{
    SecIdentityRef attestationIdentity = sharedAttestationIdentity(); /* will be used to attest our new hardware-bound key */
    NSString *clientIdentifier = [[NSUUID UUID] UUIDString]; /* random string intended as a single-use identifier */

    NSDictionary *baseParams = @{
        (id)kSecClientIdentifier : clientIdentifier,
        /* key creation attributes */
        (id)kSecUseHardwareBoundKey : (hardwareBound) ? (id)kCFBooleanTrue : (id)kCFBooleanFalse,
        (id)kSecAttrKeyType : (id)CFBridgingRelease(keyType),
        (id)kSecAttrKeySizeInBits : keySize,
        /* certificate creation attributes */
        (id)kSecCertificateLifetime : @(3600*24*825), /* lifetime in seconds */
        (id)kSecCertificateKeyUsage : @(kSecKeyUsageDigitalSignature | kSecKeyUsageKeyEncipherment),
        (id)kSecCertificateExtendedKeyUsage : @[(__bridge NSString*)kSecEKUClientAuth],
        (__bridge NSString*)kSecCMSSignHashAlgorithm : (__bridge NSString*)kSecCMSHashingAlgorithmSHA256,
    };
    NSMutableDictionary *parameters = [NSMutableDictionary dictionaryWithDictionary:baseParams];
    if (acmeServerURL) {
        parameters[(id)kSecACMEDirectoryURL] = (id)acmeServerURL;
        parameters[(id)kSecACMEPermitLocalIssuer] = @YES;
    }
    if (hardwareBound && attestationIdentity) {
        parameters[(id)kSecAttestationIdentity] = (id)CFBridgingRelease(attestationIdentity);
    }
    if (hardwareBound) {
        parameters[(id)kSecAttestationOids] = makeEDAOids(false, true);
    }
    if (!minimalKeyAttrs) {
        NSData *tag = makeApplicationTag(keySize, hardwareBound); /* non-user-visible tag for the private key */
        NSDictionary *baseKeyAttrs = @{
            (id)kSecAttrIsPermanent : (id)kCFBooleanTrue,
            (id)kSecAttrApplicationTag : tag,
            (id)kSecAttrLabel : label,
        };
        NSMutableDictionary *privateKeyAttrs = [NSMutableDictionary dictionaryWithDictionary:baseKeyAttrs];
        if (hardwareBound) {
            /* add access control, per developer documentation for secure enclave keys: https://developer.apple.com/documentation/security/certificate_key_and_trust_services/keys/storing_keys_in_the_secure_enclave */
            SecAccessControlRef access = SecAccessControlCreateWithFlags(kCFAllocatorDefault,
                                                                         kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
                                                                         kSecAccessControlPrivateKeyUsage,
                                                                         NULL); /* ignore error */
            if (access) {
                parameters[(id)kSecAttrAccessControl] = (id)CFBridgingRelease(access);
            }
        } else {
            /* for software-only keys, add sensitive and non-extractable attributes */
            privateKeyAttrs[(id)kSecAttrIsSensitive] = (id)kCFBooleanTrue;
            privateKeyAttrs[(id)kSecAttrIsExtractable] = (id)kCFBooleanFalse;
        }
        parameters[(id)kSecPrivateKeyAttrs] = privateKeyAttrs;
    }

    return parameters;
}

static OSStatus deleteIdentity(SecIdentityRef identityToDelete, bool hardwareBound)
{
    if (!identityToDelete) { return errSecSuccess; /* nothing to do */ }
    NSMutableDictionary *deleteQuery = [NSMutableDictionary dictionaryWithDictionary:@{
            (id)kSecClass: (id)kSecClassIdentity,
            (id)kSecValueRef: (id)CFBridgingRelease(identityToDelete),
        }];
    if (hardwareBound) {
        return errSecInternal; //%%% the hw bound keys pop up an auth dialog, so don't try to delete for now
    }
    return SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
}

static SecIdentityRef findIdentity(NSData *tag, NSString *name, bool hardwareBound)
{
    NSMutableDictionary *findQuery = [NSMutableDictionary dictionaryWithDictionary:@{
            (id)kSecClass: (id)kSecClassIdentity,
            (id)kSecAttrApplicationTag: tag,
            (id)kSecAttrAccessGroup : @"com.apple.security.regressions",
            (id)kSecReturnRef: @YES,
            (id)kSecMatchLimit: (id)kSecMatchLimitAll,
        }];
    if (hardwareBound) {
        //findQuery[(__bridge id)kSecAttrTokenID] = (__bridge id)kSecAttrTokenIDSecureEnclave;
        findQuery[(__bridge id)kSecUseDataProtectionKeychain] = (id)kCFBooleanTrue;
    }
    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result);
    
    // for identities in the data protection keychain, SecItemCopyMatching appears to work as expected.
    // however, if we omit the data protection keychain attribute, the lookup seems to ignore the tag and
    // access group, returning all available identities. So we need a further check...
    NSArray *results = (__bridge NSArray*)result;
    NSUInteger idx, count = (!status) ? [results count] : 0;
    for (idx = 0; idx < count; idx++) {
        SecIdentityRef foundIdentity = (__bridge SecIdentityRef)[results objectAtIndex:idx];
        SecCertificateRef cert = NULL;
        CFStringRef summary = NULL;
        bool match = false;
        status = SecIdentityCopyCertificate((SecIdentityRef)foundIdentity, &cert);
        summary = SecCertificateCopySubjectSummary(cert);
        // check that our name matches a prefix of the certificate's label
        match = (!status && summary && name && CFStringHasPrefix(summary, (__bridge CFStringRef)name));
        CFReleaseNull(summary);
        CFReleaseNull(cert);
        if (match) {
            return foundIdentity;
        }
    }
    return NULL;
}

/* # of tests performed per invocation of the request function */
#define TESTS_PER_REQUEST   5

static void testRequest(NSString *name, NSString *uuid, NSArray *subject, NSDictionary* parameters)
{
    /* make a label from the supplied name and uuid */
    __block NSString *label = [NSString stringWithFormat:@"%@ (%@)", name, uuid];

    CFBooleanRef hwb = (__bridge CFBooleanRef)parameters[(id)kSecUseHardwareBoundKey];
    __block bool hardwareBound = (hwb && CFEqual(hwb, kCFBooleanTrue));
    CFNumberRef keySize = (__bridge CFNumberRef)parameters[(id)kSecAttrKeySizeInBits];
    __block NSData *tag = makeApplicationTag((__bridge NSNumber*)keySize, hardwareBound);
    __block bool skipLookup = true;
    __block bool hasKeyAttrs = ((__bridge CFDictionaryRef)parameters[(id)kSecPrivateKeyAttrs] != nil);
    __block bool usingACME = (parameters[(id)kSecACMEDirectoryURL] ||
                              parameters[(id)kSecACMEDirectoryURL]);

    // first, attempt to clean up test identity from a previous run
    SecIdentityRef oldIdentity = findIdentity(tag, name, hardwareBound);
    if (oldIdentity) {
        CFRetainSafe(oldIdentity);
        OSStatus status = deleteIdentity(oldIdentity, hardwareBound);
        if (!status) {
            //NSLog(@"successfully deleted old identity");
        } else {
            //NSLog(@"failed to delete old identity (%ld)", (long)status);
        }
        CFReleaseSafe(oldIdentity);
    }

    __block dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    __block bool verbose = false; /* set to true for debug display of the key and cert objects */

    SecRequestClientIdentity((__bridge CFArrayRef)subject,
        (__bridge CFDictionaryRef)parameters,
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        ^(SecIdentityRef _Nullable identity, CFErrorRef _Nullable error) {
            /* our completion block, called after server has issued a certificate */
            /* note: can check parameters dictionary for context here */
            CFStringRef errStr = (error) ? CFErrorCopyDescription(error) : NULL;
            if (verbose && errStr) { NSLog(@"ERROR: %@", (__bridge NSString *)errStr); }
            if ([name hasPrefix:@"Expected Failure:"]) {
                /* we expect no identity to be returned */
                /*ok(identity == NULL, "obtain identity for expected failure case unexpectedly succeeded with error: \"%@\", params: %@", errStr, parameters);*/
                /* note: an identity with non-compliant strings can currently be returned anyway,
                   so ignore this case for now; tracked by rdar://93727255 */
                if (identity) {
                    ok(true);
                }
            } else {
                /* we expect an identity to have been returned */
                ok(identity != NULL, "obtain identity failed with error: \"%@\", params: %@", errStr, parameters);
            }
            CFReleaseNull(errStr);
            if (identity) {
                if (verbose) { CFShow(identity); }
                /* check the private key */
                SecKeyRef key = NULL;
                ok_status(SecIdentityCopyPrivateKey(identity, &key), "obtain private key from identity");
                if (verbose) { displayKey(key); }
                CFReleaseNull(key);
                /* check the cert */
                SecCertificateRef cert = NULL;
                ok_status(SecIdentityCopyCertificate(identity, &cert), "obtain cert from identity");
                if (verbose) { displayCert(cert); }
                /* add the cert to the keychain */
                NSMutableDictionary *certDict = [NSMutableDictionary dictionaryWithDictionary:@{
                    (id)kSecClass: (id)kSecClassCertificate,
                    (id)kSecValueRef: (id)CFBridgingRelease(cert),
                }];
                if (hardwareBound) {
                    /* must save the cert in the "modern" keychain if we want it to be looked up
                       later as an identity with SecItemCopyMatching. (Note the private key must also
                       have been specified as permanent in its kSecPrivateKeyAttrs dictionary.) */
                    certDict[(__bridge id)kSecUseDataProtectionKeychain] = @YES;
                }
                ok_status(SecItemAdd((__bridge CFDictionaryRef)certDict, NULL), "add cert to keychain");
                if (hasKeyAttrs && !usingACME && !skipLookup) {
                    /* look up the identity in the keychain to ensure it was saved. */
                    /* Note: remove the '!usingACME' condition later when the ACME server
                       issues the certificate with the same label as we specified. */
                    /* Note 2: we are currently skipping lookup unconditionally since it has
                       different behavior on iOS. Radar TBA. */
                    SecIdentityRef foundIdentity = findIdentity(tag, label, hardwareBound);
                    ok(foundIdentity != NULL, "look up identity failed for tag %@, label \"%@\" with params: %@", tag, label, parameters);
                } else {
                    ok(true); /* identity is not permanent, so we can't look it up in the keychain */
                }
            }
            dispatch_semaphore_signal(semaphore);
        });

    if (dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        ok(false, "did not return identity within 60 seconds");
    }
}


/* # of cert requests performed in tests function below */
#define NUM_REQUESTS   10

static void tests(void)
{
    NSString *name = nil;
    NSString *urlstr = nil;
    char *envstr = getenv("ACME_DIRECTORY_URL");
    if (envstr) {
        urlstr = [NSString stringWithFormat:@"%s", envstr];
    }
    if (!urlstr || 0 == [urlstr length]) {
        urlstr = @"https://localhost:14000/dir";
    }
    NSURL *url = [NSURL URLWithString:urlstr];
    NSString *uuid = [[NSUUID UUID] UUIDString];
    NSArray *subject = nil;
    NSDictionary *params = nil;

    /* EDA specific test 1: EC384, hardware bound, ACME, no private key attrs */
    name = @"TEST ACME EDA SUBJECT";
    subject = makeEDASubject(name, uuid);
    params = makeEDAParameters(name, url, kSecAttrKeyTypeECSECPrimeRandom, @(384), true, true, false);
    testRequest(name, uuid, subject, params);

    /* EDA specific test 2: EC384, hardware bound, !ACME, no private key attrs, bad subject */
    name = @"Expected Failure: TEST EDA BAD SUBJECT";
    subject = makeBadEDASubject();
    params = makeEDAParameters(name, nil, kSecAttrKeyTypeECSECPrimeRandom, @(384), true, true, false);
    testRequest(name, uuid, subject, params);

    /* EDA specific test 3: EC384, hardware bound, !ACME, no private key attrs, bad alt names */
    /* -- does not generate an identity, since we will error out during CSR generation */
    name = @"Expected Failure: TEST EDA BAD SUBJECT ALT NAME";
    subject = makeEDASubject(name, uuid);
    params = makeEDAParameters(name, nil, kSecAttrKeyTypeECSECPrimeRandom, @(384), true, true, true);
    testRequest(name, uuid, subject, params); /* note: not counted in NUM_REQUESTS */

    /* Bad URL type test: same params as above but wrong URL type */
    /* -- does not generate an identity, since we will error out during parameter checking */
    name = @"Expected Failure: BAD URL TYPE";
    NSURL *bad = (NSURL*)urlstr; // deliberately casting wrong type!
    subject = makeEDASubject(name, uuid);
    params = makeEDAParameters(name, bad, kSecAttrKeyTypeECSECPrimeRandom, @(384), true, true, false);
    testRequest(name, uuid, subject, params); /* note: not counted in NUM_REQUESTS */

    /* local cert generation (no ACME server specified) */
    /* -- not hardware bound: RSA 2048, EC 256 */
    name = @"TEST IDENTITY RSA2048";
    subject = makeSubject(name, uuid);
    params = makeParameters(name, nil, kSecAttrKeyTypeRSA, @(2048), false, false);
    testRequest(name, uuid, subject, params);

    name = @"TEST IDENTITY EC256";
    subject = makeSubject(name, uuid);
    params = makeParameters(name, nil, kSecAttrKeyTypeECSECPrimeRandom, @(256), false, false);
    testRequest(name, uuid, subject, params);

    /* -- hardware bound: EC 256, EC 384 */
    name = @"TEST SEPK IDENTITY EC256";
    subject = makeSubject(name, uuid);
    params = makeParameters(name, nil, kSecAttrKeyTypeECSECPrimeRandom, @(256), true, false);
    testRequest(name, uuid, subject, params);

    name = @"TEST SEPK IDENTITY EC384";
    subject = makeSubject(name, uuid);
    params = makeParameters(name, nil, kSecAttrKeyTypeECSECPrimeRandom, @(384), true, false);
    testRequest(name, uuid, subject, params);

    /* ACME certificate request */
    /* -- not hardware bound: RSA 2048, EC 256 */
    name = @"TEST ACME IDENTITY RSA2048";
    subject = makeSubject(name, uuid);
    params = makeParameters(name, url, kSecAttrKeyTypeRSA, @(2048), false, false);
    testRequest(name, uuid, subject, params);

    name = @"TEST ACME IDENTITY EC256";
    subject = makeSubject(name, uuid);
    params = makeParameters(name, url, kSecAttrKeyTypeECSECPrimeRandom, @(256), false, false);
    testRequest(name, uuid, subject, params);

    /* -- hardware bound: EC 256, EC 384 */
    name = @"TEST ACME SEPK IDENTITY EC256";
    subject = makeSubject(name, uuid);
    params = makeParameters(name, url, kSecAttrKeyTypeECSECPrimeRandom, @(256), true, false);
    testRequest(name, uuid, subject, params);

    name = @"TEST ACME SEPK IDENTITY EC384";
    subject = makeSubject(name, uuid);
    params = makeParameters(name, url, kSecAttrKeyTypeECSECPrimeRandom, @(384), true, false);
    testRequest(name, uuid, subject, params);
}

int si_96_csr_acme(int argc, char *const *argv)
{
    @autoreleasepool {
        NSNumber *hasSEP = CFBridgingRelease(MGCopyAnswer(kMGQHasSEP, NULL));
        if (!hasSEP.boolValue) {
            // macOS without SEP cannot perform attestations on hardware bound keys.
            plan_tests(1);
            ok(true);
            return 0;
        }
        plan_tests(NUM_REQUESTS * TESTS_PER_REQUEST);

        tests();

        return 0;
    }
}
