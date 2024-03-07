/*
 * Copyright (c) 2021-2023 Apple Inc. All Rights Reserved.
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

#include <libDER/oids.h>
#include <libDER/DER_Encode.h>

#include <security_asn1/SecAsn1Types.h>
#include <security_asn1/csrTemplates.h>
#include <security_asn1/certExtensionTemplates.h>
#include <security_asn1/secasn1.h>
#include <security_asn1/SecAsn1Types.h>
#include <security_asn1/oidsalg.h>
#include <security_asn1/nameTemplates.h>
#include <security_asn1/SecAsn1TimeUtils.h>

#include <TargetConditionals.h>
#if TARGET_OS_OSX
// ENABLE_CMS 0
OSStatus SecCmsArraySortByDER(void **objs, const SecAsn1Template *objtemplate, void **objs2);
#else
// ENABLE_CMS 1
#include <security_smime/cmspriv.h>
#endif

#include <Security/SecInternal.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKey.h>
#include <Security/SecRSAKey.h>
#include <Security/SecKeyPriv.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <Security/SecCMS.h>
#include <os/transaction_private.h>
#include <utilities/debugging.h>

#if TARGET_OS_IPHONE
#include <Security/SecECKey.h>
#endif

#include <utilities/HTTPStatusCodes.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDispatchRelease.h>
#include <utilities/SecXPCError.h>

#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSJSONSerialization.h>
#import <CFNetwork/CFNSURLConnection.h>

#import "SecCBOR.h"
#import "SecJWS.h"
#include <Security/SecCertificateRequest.h>

/* Switch to enable debug output */
#define DEBUG_LOG 0
#if DEBUG_LOG
//#define acmedebug(format, ...) os_log(secLogObjForScope("acme"), format, ## __VA_ARGS__)
#define acmedebug(format, ...) NSLog(@"[acme] "format, ## __VA_ARGS__)
#else
#define acmedebug(format, ...) secdebug("acme", format, ## __VA_ARGS__)
#endif

#include <dlfcn.h>
#if !TARGET_OS_BRIDGE && !TARGET_OS_SIMULATOR
#include <AppAttestInternal/AppAttestCompileConfig.h>
#if defined(APPATTEST_SUPPORTED_PLATFORM_DEVICEATTESTATION)
#include <AppAttestInternal/AppAttestDeviceAttestation.h>

static void *appAttestInternal = NULL;

static void _initAppAttestInternal(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        appAttestInternal = dlopen("/System/Library/PrivateFrameworks/AppAttestInternal.framework/AppAttestInternal", RTLD_LAZY | RTLD_LOCAL);
    });
}

static void _getAppAttestInternalSymbol(void **sym, const char *name) {
    _initAppAttestInternal();
    if (!sym || *sym) {
        return;
    }
    *sym = dlsym(appAttestInternal, name);
    if (*sym == NULL) {
        fprintf(stderr, "symbol %s is missing", name);
        abort();
    }
}
#endif // APPATTEST_SUPPORTED_PLATFORM_DEVICEATTESTATION
#endif // !TARGET_OS_BRIDGE && !TARGET_OS_SIMULATOR

/* Parameter dictionary constants */
const CFStringRef kSecChallengeToken = CFSTR("challengeToken");
const CFStringRef kSecClientIdentifier = CFSTR("clientIdentifier");
const CFStringRef kSecAttestationIdentity = CFSTR("attestationIdentity"); // deprecated
const CFStringRef kSecAttestationKey = CFSTR("attestationKey");
const CFStringRef kSecAttestationChain = CFSTR("attestationChain");
const CFStringRef kSecAttestationOids = CFSTR("attestationOids");
const CFStringRef kSecLocalIssuerIdentity = CFSTR("issuerIdentity");
const CFStringRef kSecUseHardwareBoundKey = CFSTR("hardwareBound");
const CFStringRef kSecACMEDirectoryURL = CFSTR("acmeDirectory");
const CFStringRef kSecACMEPermitLocalIssuer = CFSTR("acmeLocalIssuer");
const CFStringRef kSecACMEServerURL = CFSTR("acmeServerURL"); // deprecated

/* Attestation dictionary constants */
const NSString *kSecOptionsBAAClientAttestationData = @"ClientAttestationData";
const NSString *kSecOptionsBAAClientAttestationPublicKey = @"ClientAttestationPublicKey";
const NSString *kSecOptionsBAAClientAttestationCertificate = @"ClientDirectAttestationCertificate";
const NSString *kSecOptionsBAAPerformOperationsOverIPC = @"UseXPC";
const NSString *kSecOptionsBAAValidity = @"Validity";
const NSString *kSecOptionsBAACACert = @"CACert";
const NSString *kSecOptionsBAAOIDSToInclude = @"OIDSToInclude";
const NSString *kSecOptionsBAANonce = @"nonce";

/* Last attestation request time, for rate limiting */
static CFAbsoluteTime gLastAttestationRequestAbsTime = 0.0;

/* States that an AcmeCertRequest instance can be in */
typedef NS_ENUM(NSInteger, AcmeRequestState) {
    AcmeRequestStateUnknown = 0, /* our next step is to create a key pair and csr */
    AcmeRequestStateInitialized = 1, /* next step is to contact the ACME server and get directory */
    AcmeRequestStateGotDirectoryInfo = 2, /* next step is to obtain the replay nonce */
    AcmeRequestStateGotNonce = 3, /* next step is to create a server account */
    AcmeRequestStateEstablishedAccount = 4, /* next step is to submit the order */
    AcmeRequestStateSubmittedOrder = 5, /* next step is to get the challenges */
    AcmeRequestStateReceivedChallenge = 6, /* next step is to send the attestation response */
    AcmeRequestStateRespondedToChallenge = 7, /* next step is to send the csr */
    AcmeRequestStateFinalizedOrder = 8, /* next step is to check the order status */
    AcmeRequestStateCertificateIssued = 9, /* next step is to download the certificate */
    AcmeRequestStateComplete = 10, /* we got a certificate */
};

@interface AcmeCertRequest : NSObject
@property AcmeRequestState state;                   // state we are in, based on the ACME protocol
@property BOOL logAcmeCSR;                          // true if we will log the CSR and attestation
@property BOOL permitLocalIssuer;                   // true if we permit fallback issuer on error
@property BOOL requireAttestation;                  // true if we require attestation to succeed
@property dispatch_queue_t queue;                   // queue on which we do local work
@property NSArray *subject;                         // subject of the requested certificate
@property NSDictionary *parameters;                 // parameters for the requested key and cert
@property NSDictionary *keyParams;                  // parameters for private key
@property NSArray *attestationOids;                 // array of OIDs for device attestation request
@property NSArray *attestationChain;                // array containing our attestation cert chain
@property (assign) SecKeyRef attestationCRKey;      // co-residency private key used to attest our key
@property NSArray *attestationCRChain;              // input attestation cert chain for co-residency key
@property NSData *attestation;                      // attestation we generate for our key
@property NSData *csr;                              // csr used to request certificate from the CA
@property NSString *nonce;                          // current nonce obtained from ACME server
@property NSString *location;                       // current location obtained from ACME server
@property NSString *account;                        // location of account we create on ACME server
@property NSString *token;                          // challenge token obtained from ACME server
@property NSString *timestamp;                      // timestamp for this request
@property NSArray *authorizations;                  // array of urls for requesting authorizations
@property NSString *authorizationURL;               // url for the current authorization object
@property NSString *challengeURL;                   // url for the current challenge object
@property NSString *finalizeOrderURL;               // url for finalizing order
@property NSString *orderURL;                       // url for our current order
@property NSString *certificateURL;                 // url to download the issued certificate
@property NSString *nextMessageURL;                 // next url for message (depends on state)
@property NSString *acmeNewAccountURL;              // new account URL from initial ACME reply
@property NSString *acmeNewNonceURL;                // new nonce URL from initial ACME reply
@property NSString *acmeNewOrderURL;                // new order URL from initial ACME reply
@property dispatch_queue_t xpc_queue;               // queue on which we do remote work (XPC messages)
@property xpc_connection_t connection;              // our connection to the XPC service
@property SecJWSEncoder *encoder;                   // encoder instance used for this request
@property (assign) SecKeyRef publicKey;             // public key (to be certified)
@property (assign) SecKeyRef privateKey;            // private key (to be attested)
@property (assign) SecCertificateRef certificate;   // certificate we are trying to obtain
@property (assign) SecIdentityRef identity;         // output on success: a certified private key

- (instancetype) initWithSubject:(NSArray *)subject parameters:(NSDictionary *)params;
- (void)dealloc;
- (NSError *) sanitizeParameters;
- (SecIdentityRef) identityWithError:(NSError**)error;
@end

@implementation AcmeCertRequest

- (instancetype) initWithSubject:(NSArray *)subject parameters:(NSDictionary *)params
{
    if (self = [super init]) {
        self.subject = subject;
        self.parameters = params;
        self.queue = dispatch_queue_create("com.apple.certrequest", NULL);
    }
    return self;
}

- (void) dealloc
{
    CFReleaseNull(_privateKey);
    CFReleaseNull(_publicKey);
    CFReleaseNull(_certificate);
    CFReleaseNull(_identity);
}

- (nullable NSData *) serialNumber {
    /* Generate a unique serial number based on absolute time in nanoseconds. */
    uint64_t ut = (uint64_t) dispatch_time(DISPATCH_TIME_NOW, 0);
    unsigned char u64TimeN[8] = {ut>>56,ut>>48,ut>>40,ut>>32,ut>>24,ut>>16,ut>>8,ut};
    NSUInteger idx, length = sizeof(u64TimeN);
    for (idx = 0; idx < sizeof(u64TimeN); idx++) {
        if (u64TimeN[idx] != 0) { break; }
        length--; /* skip leading zeroes */
    }
    NSMutableData *data = [NSMutableData dataWithCapacity:0];
    if (u64TimeN[idx] & 0x80) {
        uint8_t b = 0; /* prepend 0 to make a positive value */
        [data appendBytes:&b length:(NSUInteger)sizeof(b)];
    }
    [data appendBytes:&u64TimeN[idx] length:length];
    return data;
}

- (nullable NSError *) sanitizeSubject {
    __block NSError* error = nil;

    dispatch_sync(self.queue, ^{
        const char *paramErrStr = NULL;
        NSUInteger idx, count = 0;
        if (!_subject || ![_subject isKindOfClass:[NSArray class]]) {
            paramErrStr = "unknown";
        } else {
            count = [_subject count];
        }
        NSMutableArray *DN = [NSMutableArray arrayWithCapacity:0];
        for (idx=0; idx<count && !paramErrStr; idx++) {
            NSArray *inputRDN = (NSArray *)[_subject objectAtIndex:idx];
            NSArray *inputPair = nil;
            if (!inputRDN || ![inputRDN isKindOfClass:[NSArray class]]) {
                paramErrStr = [[NSString stringWithFormat:@"index %lu", (unsigned long)idx] UTF8String];
            } else {
                inputPair = (NSArray *)[inputRDN objectAtIndex:0];
            }
            if (!inputPair || ![inputPair isKindOfClass:[NSArray class]]) {
                paramErrStr = [[NSString stringWithFormat:@"index %lu", (unsigned long)idx] UTF8String];
            } else {
                NSString *name = (NSString *)[inputPair objectAtIndex:0];
                NSString *value = (NSString *)[inputPair objectAtIndex:1];
                if (!name || ![name isKindOfClass:[NSString class]] ||
                    !value || ![value isKindOfClass:[NSString class]]) {
                    paramErrStr = [[NSString stringWithFormat:@"index %lu", (unsigned long)idx] UTF8String];
                } else {
                    // name must be an OID string or a shortcut representation.
                    NSCharacterSet *set = [[NSCharacterSet characterSetWithCharactersInString:@".0123456789"] invertedSet];
                    if ([name rangeOfCharacterFromSet:set].location != NSNotFound) {
                        NSArray *supportedShortcuts = @[ @"CN", @"C", @"ST", @"L", @"O", @"OU" ];
                        if ([name isEqualToString:@"EMAIL"]) {
                            name = @"1.2.840.113549.1.9.1";
                        } else if (![supportedShortcuts containsObject:name]) {
                          paramErrStr = [[NSString stringWithFormat:@"index %lu \"%@\"", (unsigned long)idx, name] UTF8String];
                        }
                    }
                }
                if (!paramErrStr) {
                    NSArray *RDN = @[@[ name, value ]];
                    [DN addObject:RDN];
                }
            }
        }
        if (paramErrStr != NULL) {
            NSString *errorString = [NSString stringWithFormat:@"SecRequestClientIdentity: subject array has missing value or wrong type for %s key", paramErrStr];
            error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey:errorString}];
        } else {
            self.subject = DN;
        }
    });
    return error;
}

- (nullable NSError *) sanitizeParameters {
    __block NSError* error = nil;

    dispatch_sync(self.queue, ^{
        const char *paramErrStr = NULL;
        NSMutableDictionary *params = [NSMutableDictionary dictionaryWithCapacity:0];
        NSMutableDictionary *keyParams = [NSMutableDictionary dictionaryWithCapacity:0];
        [params setDictionary:self.parameters];
        /* check hardware bound key flag [optional] and set kSecAttrTokenID value */
        CFBooleanRef hwBound = (__bridge CFBooleanRef)[params objectForKey:(__bridge NSString*)kSecUseHardwareBoundKey];
        if (hwBound) {
            if (!(CFGetTypeID(hwBound) == CFBooleanGetTypeID())) {
                paramErrStr = "kSecUseHardwareBoundKey";
            } else if (CFEqualSafe(hwBound, kCFBooleanTrue)) {
                keyParams[(__bridge id)kSecAttrTokenID] = (__bridge id)kSecAttrTokenIDSecureEnclave;
            }
        }
        /* check key type [optional parameter, but required for key creation] */
        if (!paramErrStr) {
            NSString *keyType = (NSString *)[params objectForKey:(__bridge NSString*)kSecAttrKeyType];
            if (keyType) {
                if (![keyType isKindOfClass:[NSString class]]) {
                    paramErrStr = "kSecAttrKeyType";
                }
            } else { /* use default key type if unspecified */
                keyType = (CFEqualSafe(hwBound, kCFBooleanTrue)) ?
                    (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom :
                    (__bridge NSString*)kSecAttrKeyTypeRSA;
            }
            keyParams[(__bridge NSString*)kSecAttrKeyType] = keyType;
        }
        /* check key size in bits [optional parameter, but required for key creation] */
        if (!paramErrStr) {
            NSNumber *keySize = (NSNumber*)[params objectForKey:(__bridge NSString*)kSecAttrKeySizeInBits];
            if (keySize) {
                if (![keySize isKindOfClass:[NSNumber class]]) {
                    paramErrStr = "kSecAttrKeySizeInBits";
                }
            } else { /* use default key size if unspecified */
                NSString *keyType = (NSString*)[params objectForKey:(__bridge NSString*)kSecAttrKeyType];
                BOOL isEC = (CFEqualSafe(hwBound, kCFBooleanTrue) || ![keyType isEqualToString:(__bridge NSString*)kSecAttrKeyTypeRSA]);
                keySize = (isEC) ? @384 : @2048;
            }
            keyParams[(__bridge NSString*)kSecAttrKeySizeInBits] = keySize;
        }
        /* check private key attributes dictionary [optional] */
        if (!paramErrStr) {
            NSDictionary *keyAttrs = [params objectForKey:(__bridge NSString*)kSecPrivateKeyAttrs];
            if (keyAttrs) {
                if (![keyAttrs isKindOfClass:[NSDictionary class]]) {
                    paramErrStr = "kSecPrivateKeyAttrs";
                } else if (!(hwBound && (CFGetTypeID(hwBound) == CFBooleanGetTypeID()))) {
                    keyParams[(__bridge id)kSecPrivateKeyAttrs] = keyAttrs;
                    /* key attrs must be copied to the top level dictionary for compatibility */
                    NSString *label = (NSString *)[keyAttrs objectForKey:(__bridge NSString*)kSecAttrLabel];
                    if (label) { keyParams[(__bridge id)kSecAttrLabel] = label; }
                    NSData *tag = (NSData *)[keyAttrs objectForKey:(__bridge NSString*)kSecAttrApplicationTag];
                    if (tag) { keyParams[(__bridge id)kSecAttrApplicationTag] = tag; }
                    CFBooleanRef permanent = (__bridge CFBooleanRef)[keyAttrs objectForKey:(__bridge NSString*)kSecAttrIsPermanent];
                    if (permanent) { keyParams[(__bridge id)kSecAttrIsPermanent] = (__bridge id)permanent; }
                    CFBooleanRef extractable = (__bridge CFBooleanRef)[keyAttrs objectForKey:(__bridge NSString*)kSecAttrIsExtractable];
                    if (extractable) { keyParams[(__bridge id)kSecAttrIsExtractable] = (__bridge id)extractable; }
                    CFBooleanRef sensitive = (__bridge CFBooleanRef)[keyAttrs objectForKey:(__bridge NSString*)kSecAttrIsSensitive];
                    if (sensitive) { keyParams[(__bridge id)kSecAttrIsSensitive] = (__bridge id)sensitive; }
                    CFBooleanRef dataProtection = (__bridge CFBooleanRef)[keyAttrs objectForKey:(__bridge NSString*)kSecUseDataProtectionKeychain];
                    if (dataProtection) { keyParams[(__bridge id)kSecUseDataProtectionKeychain] = (__bridge id)dataProtection; }
#if TARGET_OS_OSX
                    CFBooleanRef dpSystemKeychain = (__bridge CFBooleanRef)[keyAttrs objectForKey:(__bridge NSString*)kSecUseSystemKeychainAlways];
                    if (dpSystemKeychain) { keyParams[(__bridge id)kSecUseSystemKeychainAlways] = (__bridge id)dpSystemKeychain; }
                    SecAccessRef access = (__bridge SecAccessRef)[keyAttrs objectForKey:(__bridge NSString*)kSecAttrAccess];
                    if (access) { keyParams[(__bridge id)kSecAttrAccess] = (__bridge id)access; }
#endif
                    SecAccessControlRef accessControl = (__bridge SecAccessControlRef)[keyAttrs objectForKey:(__bridge NSString*)kSecAttrAccessControl];
                    if (accessControl) { keyParams[(__bridge id)kSecAttrAccessControl] = (__bridge id)accessControl; }
                }
                keyParams[(__bridge NSString*)kSecPrivateKeyAttrs] = keyAttrs;
            }
        }
        /* check key attributes [optional] */
        if (!paramErrStr) {
            NSString *label = (NSString *)[params objectForKey:(__bridge NSString*)kSecAttrLabel];
            if (label) {
                if (![label isKindOfClass:[NSString class]]) {
                    paramErrStr = "kSecAttrLabel";
                }
                keyParams[(__bridge id)kSecAttrLabel] = label;
            }
        }
        if (!paramErrStr) {
            NSData *tag = (NSData *)[params objectForKey:(__bridge NSString*)kSecAttrApplicationTag];
            if (tag) {
                if (![tag isKindOfClass:[NSData class]]) {
                    paramErrStr = "kSecAttrApplicationTag";
                }
                keyParams[(__bridge id)kSecAttrApplicationTag] = tag;
            }
        }
        if (!paramErrStr) {
            CFBooleanRef permanent = (__bridge CFBooleanRef)[params objectForKey:(__bridge NSString*)kSecAttrIsPermanent];
            if (permanent) {
                if (!(CFGetTypeID(permanent) == CFBooleanGetTypeID())) {
                    paramErrStr = "kSecAttrIsPermanent";
                }
                keyParams[(__bridge id)kSecAttrIsPermanent] = (__bridge id)permanent;
            }
        }
        if (!paramErrStr) {
            CFBooleanRef extractable = (__bridge CFBooleanRef)[params objectForKey:(__bridge NSString*)kSecAttrIsExtractable];
            if (extractable) {
                if (!(CFGetTypeID(extractable) == CFBooleanGetTypeID())) {
                    paramErrStr = "kSecAttrIsExtractable";
                }
                keyParams[(__bridge id)kSecAttrIsExtractable] = (__bridge id)extractable;
            }
        }
        if (!paramErrStr) {
            CFBooleanRef sensitive = (__bridge CFBooleanRef)[params objectForKey:(__bridge NSString*)kSecAttrIsSensitive];
            if (sensitive) {
                if (!(CFGetTypeID(sensitive) == CFBooleanGetTypeID())) {
                    paramErrStr = "kSecAttrIsSensitive";
                }
                keyParams[(__bridge id)kSecAttrIsSensitive] = (__bridge id)sensitive;
           }
        }
        /* check data protection keychain identifier(s) [optional] */
        if (!paramErrStr) {
            CFBooleanRef dataProtection = (__bridge CFBooleanRef)[params objectForKey:(__bridge NSString*)kSecUseDataProtectionKeychain];
            if (dataProtection) {
                if (!(CFGetTypeID(dataProtection) == CFBooleanGetTypeID())) {
                    paramErrStr = "kSecUseDataProtectionKeychain";
                }
                keyParams[(__bridge id)kSecUseDataProtectionKeychain] = (__bridge id)dataProtection;
           }
        }
#if TARGET_OS_OSX
        if (!paramErrStr) {
            CFBooleanRef dpSystemKeychain = (__bridge CFBooleanRef)[params objectForKey:(__bridge NSString*)kSecUseSystemKeychainAlways];
            if (dpSystemKeychain) {
                if (!(CFGetTypeID(dpSystemKeychain) == CFBooleanGetTypeID())) {
                    paramErrStr = "kSecUseSystemKeychainAlways";
                }
                keyParams[(__bridge id)kSecUseSystemKeychainAlways] = (__bridge id)dpSystemKeychain;
           }
        }
        if (!paramErrStr) {
            SecKeychainRef keychain = (__bridge SecKeychainRef)[params objectForKey:(__bridge NSString*)kSecUseKeychain];
            if (keychain) {
                if (!(CFGetTypeID(keychain) == SecKeychainGetTypeID()) &&
                    !(CFGetTypeID(keychain) == CFArrayGetTypeID())) {
                    paramErrStr = "kSecUseKeychain";
                }
                keyParams[(__bridge id)kSecUseKeychain] = (__bridge id)keychain;
            }
        }
        if (!paramErrStr) {
            SecAccessRef access = (__bridge SecAccessRef)[params objectForKey:(__bridge NSString*)kSecAttrAccess];
            if (access) {
                if (!(CFGetTypeID(access) == SecAccessGetTypeID())) {
                    paramErrStr = "kSecAttrAccess";
                }
                keyParams[(__bridge id)kSecAttrAccess] = (__bridge id)access;
            }
        }
#endif
        if (!paramErrStr) {
            SecAccessControlRef accessControl = (__bridge SecAccessControlRef)[params objectForKey:(__bridge NSString*)kSecAttrAccessControl];
            if (accessControl) {
                if (!(CFGetTypeID(accessControl) == SecAccessControlGetTypeID())) {
                    paramErrStr = "kSecAttrAccessControl";
                }
                keyParams[(__bridge id)kSecAttrAccessControl] = (__bridge id)accessControl;
            }
        }
        /* check client identifier [required] */
        if (!paramErrStr) {
            NSString *clientIdentifier = (NSString*)[params objectForKey:(__bridge NSString*)kSecClientIdentifier];
            if (!clientIdentifier || ![clientIdentifier isKindOfClass:[NSString class]]) {
                paramErrStr = "kSecClientIdentifier";
            }
        }
        /* check certificate lifetime [optional] */
        if (!paramErrStr) {
            NSNumber *certLifetime = (NSNumber*)[params objectForKey:(__bridge NSString*)kSecCertificateLifetime];
            if (certLifetime && ![certLifetime isKindOfClass:[NSNumber class]]) {
                paramErrStr = "kSecCertificateLifetime";
            }
        }
        /* check certificate serial number [optional] */
        if (!paramErrStr) {
            NSData *certSerial = (NSData*)[params objectForKey:(__bridge NSString*)kSecCertificateSerialNumber];
            if (certSerial) {
                if (![certSerial isKindOfClass:[NSData class]]) {
                    paramErrStr = "kSecCertificateSerialNumber";
                }
            } else { /* create a unique serial number by default if unspecified */
                params[(__bridge NSString*)kSecCertificateSerialNumber] = [self serialNumber];
            }
        }
        /* check certificate key usage [optional] */
        if (!paramErrStr) {
            NSNumber *certKeyUsage = (NSNumber*)[params objectForKey:(__bridge NSString*)kSecCertificateKeyUsage];
            if (certKeyUsage) {
                if (![certKeyUsage isKindOfClass:[NSNumber class]]) {
                    paramErrStr = "kSecCertificateKeyUsage";
                }
            } else { /* use default certificate key usage if unspecified */
                certKeyUsage = @(kSecKeyUsageDigitalSignature | kSecKeyUsageKeyEncipherment);
                params[(__bridge NSString*)kSecCertificateKeyUsage] = certKeyUsage;
            }
        }
        /* check certificate extended key usage [optional] */
        if (!paramErrStr) {
            NSArray *certEKUs = (NSArray*)[params objectForKey:(__bridge NSString*)kSecCertificateExtendedKeyUsage];
            if (certEKUs) {
                if (![certEKUs isKindOfClass:[NSArray class]]) {
                    paramErrStr = "kSecCertificateExtendedKeyUsage";
                }
            } else { /* use default EKU purpose if unspecified */
                certEKUs = @[(__bridge NSString*)kSecEKUClientAuth];
                params[(__bridge NSString*)kSecCertificateExtendedKeyUsage] = certEKUs;
            }
        }
        /* check certificate signature algorithm [optional] */
        if (!paramErrStr) {
            NSString *sigHashAlg = (NSString*)[params objectForKey:(__bridge NSString*)kSecCMSSignHashAlgorithm];
            if (sigHashAlg) {
                if (![sigHashAlg isKindOfClass:[NSString class]]) {
                    paramErrStr = "kSecCMSSignHashAlgorithm";
                }
            } else { /* use default signature hash algorithm if unspecified */
                sigHashAlg = (__bridge NSString*)kSecCMSHashingAlgorithmSHA256;
                params[(__bridge NSString*)kSecCMSSignHashAlgorithm] = sigHashAlg;
            }
        }
        /* check subject alternative name [optional] */
        if (!paramErrStr) {
            NSDictionary *san = (NSDictionary*)[params objectForKey:(__bridge NSString*)kSecSubjectAltName];
            if (san && ![san isKindOfClass:[NSDictionary class]]) {
                paramErrStr = "kSecSubjectAltName";
            }
        }
        /* check ACME directory URL [optional] */
        if (!paramErrStr) {
            NSURL *acmeDirectoryURL = (NSURL*)[params objectForKey:(__bridge NSString*)kSecACMEDirectoryURL];
            if (acmeDirectoryURL && ![acmeDirectoryURL isKindOfClass:[NSURL class]]) {
                paramErrStr = "kSecACMEDirectoryURL";
            }
        }
        /* check attestation identity [optional] */
        if (!paramErrStr) {
            SecIdentityRef identity = (__bridge SecIdentityRef)[params objectForKey:(__bridge NSString*)kSecAttestationIdentity];
            if (identity && (!(CFGetTypeID(identity) == SecIdentityGetTypeID()))) {
                paramErrStr = "kSecAttestationIdentity";
            }
        }
        /* check attestation oids array [optional] */
        if (!paramErrStr) {
            NSArray *oids = (NSArray*)[params objectForKey:(__bridge NSString*)kSecAttestationOids];
            if (oids && ![oids isKindOfClass:[NSArray class]]) {
                paramErrStr = "kSecAttestationOids";
            }
            self.attestationOids = oids;
        }
        /* check attestation key (for co-residency) [optional] */
        if (!paramErrStr) {
            SecKeyRef acrkey = (__bridge SecKeyRef)[params objectForKey:(__bridge NSString*)kSecAttestationKey];
            if (acrkey && (!(CFGetTypeID(acrkey) == SecKeyGetTypeID()))) {
                paramErrStr = "kSecAttestationKey";
            }
            self.attestationCRKey = acrkey;
        }
        /* check attestation chain array (for co-residency) [optional] */
        if (!paramErrStr) {
            NSArray *chain = (NSArray*)[params objectForKey:(__bridge NSString*)kSecAttestationChain];
            if (chain && ![chain isKindOfClass:[NSArray class]]) {
                paramErrStr = "kSecAttestationChain";
            }
            self.attestationCRChain = chain;
        }

        if (paramErrStr != NULL) {
            NSString *errorString = [NSString stringWithFormat:@"SecRequestClientIdentity parameters dictionary has missing value or wrong type for %s key", paramErrStr];
            error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey:errorString}];
        } else {
            self.parameters = params;
            self.keyParams = keyParams;
        }
    });
    if (!error) { error = [self sanitizeSubject]; }
    return error;
}

- (nullable NSError *) createKeyPair {
    __block CFErrorRef error = NULL;

    dispatch_sync(self.queue, ^{
        /* release existing keys before generating new keys */
        CFReleaseNull(_privateKey);
        CFReleaseNull(_publicKey);

        _privateKey = SecKeyCreateRandomKey((__bridge CFDictionaryRef)_keyParams, &error);
        if (_privateKey) {
            _publicKey = SecKeyCopyPublicKey(_privateKey);
        }
        if ((!_privateKey || !_publicKey) && !error) {
            error = (CFErrorRef)CFBridgingRetain(
                [NSError errorWithDomain:NSOSStatusErrorDomain
                                    code:errSecInvalidKey
                                userInfo:nil]);
        }
        if (error) {
            CFReleaseNull(_privateKey);
            CFReleaseNull(_publicKey);
        }
    });
    return (error) ? (NSError *)CFBridgingRelease(error) : NULL;
}

- (nullable NSError *) createCSR {
    __block NSError *error = nil;

    dispatch_sync(self.queue, ^{
        NSData *csr = (NSData*)CFBridgingRelease(SecGenerateCertificateRequest(
                                                (__bridge CFArrayRef)_subject,
                                                (__bridge CFDictionaryRef)_parameters,
                                                _publicKey,
                                                _privateKey));
        if (_logAcmeCSR) {
            NSString *csrPath = [NSString stringWithFormat:@"/tmp/AcmeCsr_%@", _timestamp];
            [csr writeToFile:csrPath atomically:YES];
        }
        _csr = csr;
        if (!csr) {
            //%%% need to add a SecGenerateCertificateRequestWithError function,
            // so we can return an appropriate CFErrorRef instead
            error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidRequestInputs userInfo:nil];
        }
    });

    return error;
}

- (BOOL) deviceAttestationSupported {
    static BOOL appAttestSupported = NO;
#if defined(APPATTEST_SUPPORTED_PLATFORM_DEVICEATTESTATION) && !TARGET_OS_BRIDGE && !TARGET_OS_SIMULATOR
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        __typeof(AppAttest_DeviceAttestation_IsSupported) *soft_AppAttest_DeviceAttestation_IsSupported = NULL;
        _getAppAttestInternalSymbol((void **)&soft_AppAttest_DeviceAttestation_IsSupported, "AppAttest_DeviceAttestation_IsSupported");
        appAttestSupported = soft_AppAttest_DeviceAttestation_IsSupported();
    });
#endif // APPATTEST_SUPPORTED_PLATFORM_DEVICEATTESTATION && !TARGET_OS_BRIDGE && !TARGET_OS_SIMULATOR
    return appAttestSupported;
}

// Request a new attestation certificate chain.
// Assumes _privateKey, _attestationOids, and _nonce are previously set and valid
- (nullable NSArray *) requestAttestationChainWithError:(NSError **)outError {
    __block NSArray *certChain = nil;

    if (![self deviceAttestationSupported]) {
        if (outError) {
            *outError = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnsupportedService userInfo:nil];
        }
        return certChain;
    }
    /*%%% Must be rate-limited here */
    gLastAttestationRequestAbsTime = CFAbsoluteTimeGetCurrent();
#if defined(APPATTEST_SUPPORTED_PLATFORM_DEVICEATTESTATION) && !TARGET_OS_BRIDGE && !TARGET_OS_SIMULATOR
    static __typeof(AppAttest_DeviceAttestation_AttestKey) *soft_AppAttest_DeviceAttestation_AttestKey = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _getAppAttestInternalSymbol((void **)&soft_AppAttest_DeviceAttestation_AttestKey, "AppAttest_DeviceAttestation_AttestKey");
    });
    NSData *nonceData = [_token dataUsingEncoding:NSUTF8StringEncoding];
    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:4];

    dict[kSecOptionsBAAValidity] = @(60*60*24*30*3); // 3 months
    dict[kSecOptionsBAAOIDSToInclude] = _attestationOids;
    dict[kSecOptionsBAAPerformOperationsOverIPC] = @YES;
    if (nonceData) {
        // AppAttest requires 256 bits (32 bytes) or fewer for the nonce value.
        // Since the token value can be arbitrarily large, we use its SHA256 digest.
        // This allows the ACME server to verify that its token was used by generating its own
        // digest of the token and comparing to the digest in the attestation statement.
        //
        uint8_t digest[CC_SHA256_DIGEST_LENGTH] = {0};
        CC_SHA256([nonceData bytes], (CC_LONG)[nonceData length], digest);
        NSData *digestData = [NSData dataWithBytes:digest length:CC_SHA256_DIGEST_LENGTH];
        dict[kSecOptionsBAANonce] = digestData;
    }
    if (_attestation) {
        // Add co-residency attestation data
        dict[kSecOptionsBAAClientAttestationData] = _attestation;
    }
    if (_attestationCRKey) {
        // Add co-residency public key
        SecKeyRef publicKey = SecKeyCopyPublicKey(_attestationCRKey);
        if (!publicKey) {
            secerror("Failed to obtain public key for attestation key, skipping attestation");
            if (outError) {
                *outError = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecPublicKeyInconsistent userInfo:nil];
            }
            return NULL;
        }
        CFErrorRef cfError = NULL;
        NSData *publicKeyData = (__bridge_transfer NSData *)SecKeyCopyExternalRepresentation(publicKey, &cfError);
        CFReleaseNull(publicKey);
        if (cfError || !publicKeyData) {
            secerror("Failed to copy public key data for co-residency, skipping attestation");
            if (outError) {
                *outError = (__bridge_transfer NSError *)cfError;
            } else {
                CFReleaseNull(cfError);
            }
            return NULL;
        }
        dict[kSecOptionsBAAClientAttestationPublicKey] = publicKeyData;
    }
    if (_attestationCRChain) {
        // Add co-residency attestation chain
        NSMutableData *coresidencyCertsData = [NSMutableData data];
        for (id cert in _attestationCRChain) {
            NSData *certData = (__bridge_transfer NSData *)SecCertificateCopyData((__bridge SecCertificateRef)cert);
            [coresidencyCertsData appendData:certData];
        }
        dict[kSecOptionsBAAClientAttestationCertificate] = coresidencyCertsData;
    }
    NSDictionary *requestOptions = [dict copy];

    dispatch_semaphore_t waitSema = dispatch_semaphore_create(0);
    __block NSError *blockError = nil;

    unsigned int retries = 3;
    while (retries) {
        soft_AppAttest_DeviceAttestation_AttestKey(_privateKey, requestOptions, ^(NSArray *certificates, NSError *error) {
            blockError = nil;
            if (error) {
                blockError = error;
            } else if (!certificates || certificates.count != 2) {
                blockError = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInternalError userInfo:nil];
            } else {
                certChain = certificates;
            }
            dispatch_semaphore_signal(waitSema);
        });
        /* wait for the attestation certificate to be generated */
        dispatch_semaphore_wait(waitSema, DISPATCH_TIME_FOREVER);

        if (certChain) {
            secnotice("acme", "Successfully retrieved attestation certificate");
            break;
        }
        if (blockError.code != errSecInternalError) {
            secnotice("acme", "Attempt to fetch attestation certificate failed (error %lld)",
                      (long long)blockError.code);
            secerror("Failed to fetch attestation certificate, not retrying");
            break;
        }
        retries--;
        secerror("Failed to fetch attestation certificate, %u retries left", retries);
    }

    if (0 == retries) {
        secerror("Out of retries retrieving attestation certificate");
    }
    if (outError) {
        *outError = blockError;
    } else {
        blockError = nil;
    }
#endif // APPATTEST_SUPPORTED_PLATFORM_DEVICEATTESTATION && !TARGET_OS_BRIDGE && !TARGET_OS_SIMULATOR

    return certChain;
}

- (nullable NSData *) attestationObjectWithCertificates:(nonnull NSArray *)certificates
{
    NSData *result = nil;

    @autoreleasepool {
        SecCBORArray *certsArray = [[SecCBORArray alloc] init];
        if ([certificates isKindOfClass:[NSArray class]] && [certificates count] == 2) {
            for (NSUInteger i = 0; i < [certificates count]; ++i) {
                NSData *data = certificates[i];
                if (data && ![data isKindOfClass:[NSData class]]) {
                    // if we have a SecCertificateRef, convert to NSData
                    if (CFGetTypeID((__bridge CFTypeRef)data) == SecCertificateGetTypeID()) {
                        data = CFBridgingRelease(SecCertificateCopyData((__bridge SecCertificateRef)data));
                    } else {
                        data = nil;
                    }
                }
                if (data) {
                    [certsArray addObject:[[SecCBORData alloc] initWith:data]];
                }
            }
        }
        SecCBORMap *attestationStatement = [[SecCBORMap alloc] init];
        [attestationStatement setKey:[[SecCBORString alloc] initWith:@"x5c"] value:certsArray];
        SecCBORMap *attestationObjectMap = [[SecCBORMap alloc] init];
        [attestationObjectMap setKey:[[SecCBORString alloc] initWith:@"fmt"] value:[[SecCBORString alloc] initWith:@"apple"]];
        [attestationObjectMap setKey:[[SecCBORString alloc] initWith:@"attStmt"] value:attestationStatement];
        NSMutableData *output = [[NSMutableData alloc] init];
        [attestationObjectMap write:output];
        result = [output copy];
    }
    if (result && _logAcmeCSR) {
        // log the attestation corresponding to the request
        NSString *attPath = [NSString stringWithFormat:@"/tmp/AcmeAtt_%@", _timestamp];
        NSData *attData = [[_encoder base64URLEncodedStringRepresentationWithData:result] dataUsingEncoding:NSUTF8StringEncoding];
        [attData writeToFile:attPath atomically:YES];
    }
    return result;
}

// attempt to generate a local hardware attestation for our private key
// (assumes we have _privateKey and _attestationCRKey)
- (nullable NSData *) hardwareAttestationWithError:(NSError **)error {
    CFErrorRef localError = NULL;
    if (_attestation) {
        _attestation = nil;
    }
    if (_attestationCRKey && _privateKey) {
        _attestation = (__bridge NSData*)SecKeyCreateAttestation(_attestationCRKey, _privateKey, &localError);
    } else {
        secerror("missing %@ for attestation", (_attestationCRKey) ? @"_privateKey" : @"_attestationCRKey");
        CFBooleanRef permitLocal = (__bridge CFBooleanRef)[_parameters objectForKey:(__bridge NSString*)kSecACMEPermitLocalIssuer];
        if (_permitLocalIssuer || (permitLocal &&
            CFGetTypeID(permitLocal) == CFBooleanGetTypeID() &&
            CFEqualSafe(permitLocal, kCFBooleanTrue))) {
#if PERMIT_LOCAL_TEST
            //%%% testing code path: just make a blob from a UUID
            _attestation = [[[NSUUID UUID] UUIDString] dataUsingEncoding:NSUTF8StringEncoding];
#endif
        }
    }
    if (!_attestation && !localError) {
        NSString *errorString = [NSString stringWithFormat:@"failed to create attestation"];
        NSError *errorObject = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey:errorString}];
        localError = (CFErrorRef)CFBridgingRetain(errorObject);
    }
    if (error) {
        *error = (NSError *)CFBridgingRelease(localError);
    } else {
        CFReleaseNull(localError);
    }
    return _attestation;
}

- (nullable NSError *) createCertificate {
    // generate a certificate signed by kSecLocalIssuerIdentity (if present),
    // or a self-signed identity.
    //%%% for initial testing, just make a self-signed cert here
    _certificate = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)_subject,
                                                    (__bridge CFDictionaryRef)_parameters,
                                                    _publicKey,
                                                    _privateKey);
    if (!_certificate) {
        //%%% need to add a SecGenerateSelfSignedCertificateWithError function,
        // so we can return an appropriate CFErrorRef instead
        NSString *errorString = [NSString stringWithFormat:@"failed to create certificate (bad input values)"];
        return [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidRequestInputs userInfo:@{NSLocalizedDescriptionKey:errorString}];
    }
    return nil;
}

- (nullable NSString *) attestationChainPEMRepresentation
{
    if (!_attestationChain) {
        return nil;
    }
    NSMutableString *result = [[NSMutableString alloc] initWithCapacity:0];
    NSUInteger idx, count = [_attestationChain count];
    for (idx = 0; idx < count; idx++) {
        SecCertificateRef cert = (__bridge SecCertificateRef)[_attestationChain objectAtIndex:idx];
        if (cert) {
            CFStringRef str = SecCertificateCopyPEMRepresentation(cert);
            if (str) {
                [result appendString:(__bridge NSString *)str];
                CFReleaseNull(str);
            }
        }
    }
    return result;
}

- (OSStatus) errorStatusWithHTTPErrorCode:(OSStatus)status
{
    switch (status) {
        case HTTPResponseCodeOK:
        case HTTPResponseCodeContinue:
        case HTTPResponseCodeCreated:
            return errSecSuccess;
        case HTTPResponseCodeBadRequest:
            return errSecRequestRejected;
        case HTTPResponseCodeNoContent:
        case HTTPResponseCodeUnauthorized:
        case HTTPResponseCodeForbidden:
        case HTTPResponseCodeNotFound:
        case HTTPResponseCodeConflict:
        case HTTPResponseCodeExpectationFailed:
        case HTTPResponseCodeInternalServerError:
        case HTTPResponseCodeInsufficientStorage:
        case HTTPResponseCodeServiceUnavailable:
            return errSecServiceNotAvailable;
        default:
            return status;
    }
    return status;
}

- (NSError *) sendRequestToXPCService:(NSData *)request response:(NSDictionary **)response
{
    acmedebug("sendRequestToXPCService: %@", request);

    dispatch_sync(_queue, ^{
        /* create xpc connection instance that messages for this request context will use */
        const char *xpcServiceName = "com.apple.security.XPCAcmeService";
        if (_xpc_queue == NULL) {
            _xpc_queue = dispatch_queue_create(xpcServiceName, DISPATCH_QUEUE_SERIAL);
        }
        if (_connection == NULL) {
            _connection = xpc_connection_create(xpcServiceName, _xpc_queue);
            xpc_connection_set_event_handler(_connection, ^(xpc_object_t event) {
                xpc_type_t xtype = xpc_get_type(event);
                if (XPC_TYPE_ERROR == xtype) {
                    secerror("connection error: %s",
                             xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
                } else {
                    secerror("unexpected connection event %p", event);
                }
            });
            xpc_connection_resume(_connection);
        }
    });
    __block NSError *error = nil;
    __block NSDictionary *localResponse = nil;

    // RFC 8555 and ACME server behavior requires the directory request to be a GET
    // and the nonce request to be a HEAD, while subsequent requests must use POST.
    const char* method = "POST";
    if (_state == AcmeRequestStateInitialized) { method = "GET"; }
    if (_state == AcmeRequestStateGotDirectoryInfo) { method = "HEAD"; }
    xpc_object_t xpcMethod = xpc_string_create(method);

    const char* urlstr = [_nextMessageURL UTF8String];
    if (!urlstr) { urlstr = ""; }
    xpc_object_t xpcUrl = xpc_string_create(urlstr);
    int64_t state = _state;

    extern xpc_object_t xpc_create_with_format(const char* format, ...);
    xpc_object_t xpcReqData = xpc_data_create([request bytes], [request length]);
    xpc_object_t message = xpc_create_with_format(
        "{operation: AcmeRequest, state: %int64, url: %value, method: %value, request: %value}",
        state, xpcUrl, xpcMethod, xpcReqData);

    acmedebug("Sending AcmeRequest message, state %lld, url: %s, data: %@",
          (long long)state, urlstr, xpcReqData);

    xpc_object_t reply = xpc_connection_send_message_with_reply_sync(_connection, message);

    acmedebug("Received XPC reply");
    xpc_type_t xtype = xpc_get_type(reply);
    if (XPC_TYPE_ERROR == xtype) {
        secerror("message error: %s", xpc_dictionary_get_string(reply, XPC_ERROR_KEY_DESCRIPTION));
    } else if (XPC_TYPE_CONNECTION == xtype) {
        acmedebug("received connection");
    } else if (XPC_TYPE_DICTIONARY == xtype) {
        acmedebug("received dictionary");
#ifndef NDEBUG
        // This is useful for debugging.
        char *debug = xpc_copy_description(reply);
        acmedebug("DEBUG %s", debug);
        free(debug);
#endif
        size_t xpcAcmeReplyLength = 0;
        xpc_object_t xpcAcmeReply = xpc_dictionary_get_value(reply, "AcmeReply");
        if (xpcAcmeReply) { xpcAcmeReplyLength = xpc_data_get_length(xpcAcmeReply); }
        acmedebug("xpcAcmeReplyLength: %ld bytes of response", xpcAcmeReplyLength);

        OSStatus status = 0; /* http response status */
        xpc_object_t xpcAcmeError = xpc_dictionary_get_value(reply, "AcmeError");
        xpc_object_t xpcAcmeStatus = xpc_dictionary_get_value(reply, "AcmeStatus");
        xpc_object_t xpcAcmeNonce = xpc_dictionary_get_value(reply, "AcmeNonce");
        xpc_object_t xpcAcmeLocation = xpc_dictionary_get_value(reply, "AcmeLocation");
        if (xpcAcmeNonce) {
            const char *p = xpc_string_get_string_ptr(xpcAcmeNonce);
            size_t len = xpc_string_get_length(xpcAcmeNonce);
            NSString *nonceStr = [[NSString alloc] initWithBytes:p length:len encoding:NSUTF8StringEncoding];
            if (nonceStr.length > 0) { _nonce = nonceStr; }
            acmedebug("new nonce: %@", nonceStr);
        }
        if (xpcAcmeLocation) {
            const char *p = xpc_string_get_string_ptr(xpcAcmeLocation);
            size_t len = xpc_string_get_length(xpcAcmeLocation);
            NSString *locationStr = [[NSString alloc] initWithBytes:p length:len encoding:NSUTF8StringEncoding];
            if (locationStr.length > 0) { _location = locationStr; }
        }
        if (xpcAcmeError) {
            error = (__bridge_transfer NSError*)SecCreateCFErrorWithXPCObject(xpcAcmeError);
            acmedebug("xpcAcmeError: %@", error);
        }
        if (xpcAcmeStatus) {
            status = (OSStatus)xpc_int64_get_value(xpcAcmeStatus);
            acmedebug("xpcAcmeStatus: %ld", (long)status);
        }
        status = [self errorStatusWithHTTPErrorCode:status];
        if (status == errSecSuccess) {
            const char *bytes = "";
            if (xpcAcmeReply) { bytes = xpc_data_get_bytes_ptr(xpcAcmeReply); }
            NSData *data = [NSData dataWithBytes:bytes length:xpcAcmeReplyLength];
            if (_state == AcmeRequestStateCertificateIssued) {
                // this reply comes back as pem data rather than json
                localResponse = @{ @"certificate" : data };
            } else {
                localResponse = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
                if (error) {
                    secerror("error converting json to dictionary: %@", error);
                }
            }
        }
    } else {
        secerror("unexpected message reply type %p", xtype);
    }
    acmedebug("acmeReply: %@, error: %@", localResponse, error);
    if (response) {
        *response = localResponse;
    } else if (localResponse) {
        localResponse = nil;
    }
    xpcUrl = NULL;
    xpcMethod = NULL;
    xpcReqData = NULL;
    message = NULL;

    return error;
}

- (nullable NSError *) pollForStatus:(NSString *)url until:(NSString *)expected
{
    //%%% RFC 8555 says to wait until the Retry-After header in the response.
    // For now just try a simple interval with a retry limit.
    NSUInteger retries = 3;
    NSUInteger interval = NSEC_PER_SEC * 5;
#if 1 // %%% workaround while polling flow is not yet working: rdar://89209631
    // for now, just waiting at least 5 seconds before making the next request
    // is good enough to move forward.
    dispatch_semaphore_t ds = dispatch_semaphore_create(0);
    if (dispatch_semaphore_wait(ds, dispatch_time(DISPATCH_TIME_NOW, interval)) != 0) {
        retries--;
    }
    return nil;
#else
    NSError *error = nil;
    NSString *statusString = nil;
    do {
        dispatch_semaphore_t ds = dispatch_semaphore_create(0);
        if (dispatch_semaphore_wait(ds, dispatch_time(DISPATCH_TIME_NOW, interval)) != 0) {
            retries--;
        }
        NSDictionary *reply = nil;
        NSData *request = [[_encoder encodedJWSWithPayload:nil kid:_account nonce:_nonce url:url error:&error] dataUsingEncoding:NSUTF8StringEncoding];
        if (error) { return error; }
        error = [self sendRequestToXPCService:request response:&reply];
        if (error) { return error; }
        statusString = (NSString *)[reply objectForKey:@"status"];
        if ([statusString isEqualToString:expected]) { return nil; } // success
        acmedebug("retries remaining: %lld, status is \"%@\"", (long long)retries, statusString);
    } while (retries > 0);

    NSString *errorString = [NSString stringWithFormat:@"retry limit reached"];
    error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecBadReq userInfo:@{NSLocalizedDescriptionKey:errorString}];

    return error;
#endif
}

- (NSData *) acmeRequest {
    // This method generates the appropriate ACME request payload for the current state.
    // All requests after the initial nonce must be encoded in RFC 7515 JWS format.
    NSError *error = nil;
    NSString *requestString = @"{}";
    NSMutableDictionary *payload = [NSMutableDictionary dictionary];
    if (_state == AcmeRequestStateInitialized ||
        _state == AcmeRequestStateGotDirectoryInfo) {
        // requests for directory info and nonce have no payload
    } else if (_state == AcmeRequestStateGotNonce) {
        // It is at this point in the proceedings when we will need to start encoding with JWS.
        // Using the standard initializer will create an internal EC-256 key pair for signing.
        if (!_encoder) {
            _encoder = [[SecJWSEncoder alloc] init];
        }
        payload[@"termsOfServiceAgreed"] = @(YES);
        requestString = [_encoder encodedJWSWithPayload:payload kid:nil nonce:_nonce url:_acmeNewAccountURL error:&error];
    } else if (_state == AcmeRequestStateEstablishedAccount) {
        // prepare order request
        NSString *identifier = _parameters[(__bridge NSString *)kSecClientIdentifier];
        if (!identifier) { identifier = @"TEST"; } //%%% rdar://89051755
        payload[@"identifiers"] = @[ @{ @"type":@"permanent-identifier", @"value":identifier } ];
        requestString = [_encoder encodedJWSWithPayload:payload kid:_account nonce:_nonce url:_nextMessageURL error:&error];
    } else if (_state == AcmeRequestStateSubmittedOrder) {
        // prepare authorization request (nil payload, but needs correct JWS metadata)
        // ("POST-as-GET requests must have a nil body")
        requestString = [_encoder encodedJWSWithPayload:nil kid:_account nonce:_nonce url:_nextMessageURL error:&error];
    } else if (_state == AcmeRequestStateReceivedChallenge) {
        // prepare the challenge response
        // RFC 8555 7.5.1:
        //   The client indicates to the server that it is ready for the challenge
        //   validation by sending an empty JSON body ("{}") carried in a POST
        //   request to the challenge URL (not the authorization URL).
        // =========
        // IMPORTANT: we are deviating from the RFC here; the response we send to the
        // challenge URL is not empty, but contains a payload with the attestation.
        //
        if (_attestationOids) {
            if (_attestationCRKey) {
                // generate local hardware attestation if we have an attestation key
                _attestation = [self hardwareAttestationWithError:&error];
            }
            if (!error) {
                // attempt to generate an attestation certificate for our key
                // (assumes we have _privateKey, _attestationOids, _nonce, and optionally _attestation)
                _attestationChain = [self requestAttestationChainWithError:&error];
            }
            if (error) {
                // failure to obtain attestation should not stop the ACME flow;
                // note the error and let the ACME server decide how to handle it.
                secnotice("acme", "attestation request failed with error %@", error);
                payload[@"error"] = [error localizedDescription];
                if (_requireAttestation) {
                    // treat failure to obtain attestation as an error
                    secnotice("acme", "attestation explicitly required, cannot continue with ACME");
                } else {
                    secnotice("acme", "attestation not explicitly required, ignoring error");
                    error = nil; // so we continue the flow and let the server enforce attestation
                }
            } else {
                secnotice("acme", "attestation request succeeded; got %llu certificates",
                          (unsigned long long)_attestationChain.count);
            }
            // encode the certificates into an attestation statement object
            NSData *attObj = [self attestationObjectWithCertificates:_attestationChain];
            if (attObj) {
                payload[@"attObj"] = [_encoder base64URLEncodedStringRepresentationWithData:attObj];
            }
        }
        requestString = [_encoder encodedJWSWithPayload:payload kid:_account nonce:_nonce url:_nextMessageURL error:&error];
    } else if (_state == AcmeRequestStateRespondedToChallenge) {
        // prepare request to finalize order
        payload[@"csr"] = [_encoder base64URLEncodedStringRepresentationWithData:_csr];
        requestString = [_encoder encodedJWSWithPayload:payload kid:_account nonce:_nonce url:_nextMessageURL error:&error];
    } else if (_state == AcmeRequestStateFinalizedOrder ||
               _state == AcmeRequestStateCertificateIssued) {
        // order status and download requests have no payload, but need correct JWS metadata
        // ("POST-as-GET requests must have a nil body")
        requestString = [_encoder encodedJWSWithPayload:nil kid:_account nonce:_nonce url:_nextMessageURL error:&error];
    } else if (_state == AcmeRequestStateComplete) {
        // no request to send
    }
    acmedebug("prepared request: %@, state %lld", requestString, (long long)_state);
    return [requestString dataUsingEncoding:NSUTF8StringEncoding];
}

- (NSError *) processReply:(NSDictionary *)response {
    // This method processes the ACME server response and advances state,
    // unless we get an error when processing a response at any stage.
    NSError *error = nil;
    NSString *errorString = nil;
    switch (_state) {
        case AcmeRequestStateInitialized: {
            // state 1: response is expected to contain directory info, e.g.:
            // {
            //   keyChange = "https://localhost:14000/rollover-account-key";
            //   meta =     {
            //     externalAccountRequired = 0;
            //     termsOfService = "data:text/plain,Do%20what%20thou%20wilt";
            //   };
            //   newAccount = "https://localhost:14000/sign-me-up";
            //   newNonce = "https://localhost:14000/nonce-plz";
            //   newOrder = "https://localhost:14000/order-plz";
            //   revokeCert = "https://localhost:14000/revoke-cert";
            // }
            _acmeNewAccountURL = (NSString *)[response objectForKey:@"newAccount"];
            _acmeNewNonceURL = (NSString *)[response objectForKey:@"newNonce"];
            _acmeNewOrderURL = (NSString *)[response objectForKey:@"newOrder"];

            if (!_acmeNewAccountURL || !_acmeNewNonceURL || !_acmeNewOrderURL) {
                errorString = @"failed to get directory info";
                error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidNetworkAddress userInfo:@{NSLocalizedDescriptionKey:errorString}];
                acmedebug("error in state %lld: %@", (long long)_state, error);
            } else {
                _nextMessageURL = _acmeNewNonceURL;
                _state = AcmeRequestStateGotDirectoryInfo;
                acmedebug("Processed reply, next URL is %@", _nextMessageURL);
            }
            break;
        }
        case AcmeRequestStateGotDirectoryInfo: {
            // state 2: response is expected to contain initial nonce, e.g.:
            // {
            //   "Replay-Nonce" = "67HztQ-8-2jrT3guP7tGig";
            // }
            // This json response is synthesized from the Replay-Nonce header in the HEAD reply.
            // The _nonce value is updated when processing each subsequent XPC reply.
            _nonce = (NSString *)[response objectForKey:@"Replay-Nonce"];

            if (!_nonce) {
                errorString = @"failed to get initial nonce";
                error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidData userInfo:@{NSLocalizedDescriptionKey:errorString}];
                acmedebug("error in state %lld: %@", (long long)_state, error);
            } else {
                _nextMessageURL = _acmeNewAccountURL;
                _state = AcmeRequestStateGotNonce;
                acmedebug("Processed reply, next URL is %@", _nextMessageURL);
            }
            break;
        }
        case AcmeRequestStateGotNonce: {
            // state 3: we have sent the new account request;
            // response is expected to contain our newly-created ACME account object, e.g.:
            // {
            //   "status": "valid",
            //   "orders": "https://localhost:14000/list-orderz/30",
            //   "key": {
            //     "kty": "EC",
            //     "crv": "P-256",
            //     "x": "-jQPrfvbmNJJ26wxlTlLQnpfQ8s7rEO5M5BQabgxkDc",
            //     "y": "eYuwG9jFTe4PREZ5k4qoc8-wATIIRLslf6qZ47kEXTc"
            //   }
            // }
            // Note the account URL is in the Location header, not in the account object.
            // We get it from _location, which is set when we process the XPC reply.
            NSString *orderList = (NSString *)[response objectForKey:@"orders"];
            NSString *statusString = (NSString *)[response objectForKey:@"status"];
            bool validAccount = [statusString isEqualToString:@"valid"];
            _account = _location;

            if (!(validAccount && orderList)) {
                errorString = @"failed to establish account";
                error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidData userInfo:@{NSLocalizedDescriptionKey:errorString}];
                acmedebug("error in state %lld: %@", (long long)_state, error);
            } else {
                _nextMessageURL = _acmeNewOrderURL;
                _state = AcmeRequestStateEstablishedAccount;
                acmedebug("Processed reply, next URL is %@", _nextMessageURL);
            }
            break;
        }
        case AcmeRequestStateEstablishedAccount: {
            // state 4: we have sent the new order request;
            // response should contain order object, e.g.:
            // {
            //   "status": "pending",
            //   "expires": "2022-02-21T18:05:47Z",
            //   "identifiers": [
            //     {
            //       "type": "permanent-identifier",
            //       "value": "<client-identifier>"
            //     }
            //   ],
            //   "finalize": "https://localhost:14000/finalize-order/dBG6WwM2AxC58WpGizbc5s_81IOe9H_77ty1Ryp_UD0",
            //   "authorizations": [
            //     "https://localhost:14000/authZ/NfN9as0W4bPJ4e1ZsBq3U9jydEQnCjUOw7qrlOYzLCs"
            //   ]
            // }
            NSString *statusString = (NSString *)[response objectForKey:@"status"];
            bool orderPending = [statusString isEqualToString:@"pending"];
            _authorizations = (NSArray *)[response objectForKey:@"authorizations"];
            // (we only care about the first authorization for now)
            _authorizationURL = (NSString *)[_authorizations objectAtIndex:0];
            _finalizeOrderURL = (NSString *)[response objectForKey:@"finalize"];
            _orderURL = _location; // note: _location was set when we processed XPC reply

            // check that the server supports the "permanent-identifier" identifier type,
            // per https://datatracker.ietf.org/doc/draft-bweeks-acme-device-attest/
            NSArray *identifiers = (NSArray *)[response objectForKey:@"identifiers"];
            NSUInteger idx, count = [identifiers count];
            bool supportedType = false;
            for (idx = 0; idx < count; idx++) {
                NSDictionary *identifier = (NSDictionary *)[identifiers objectAtIndex:idx];
                NSString *identifierType = (NSString *)[identifier objectForKey:@"type"];
                if ([identifierType isEqualToString:@"permanent-identifier"]) {
                    supportedType = true;
                    break;
                }
            }
            if (!(_authorizationURL && _finalizeOrderURL && orderPending && supportedType)) {
                errorString = @"failed to submit order";
                if (!supportedType) {
                    errorString = @"'permanent-identifier' type not supported by this server";
                }
                error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidData userInfo:@{NSLocalizedDescriptionKey:errorString}];
                acmedebug("error in state %lld: %@", (long long)_state, error);
            } else {
                // next request is to the authorization URL to get the authorization object
                _nextMessageURL = _authorizationURL;
                _state = AcmeRequestStateSubmittedOrder;
                acmedebug("Processed reply, next URL is %@", _nextMessageURL);
            }
            break;
        }
        case AcmeRequestStateSubmittedOrder: {
            // state 5: we have sent the authorization request;
            // response should contain authorization object, e.g.:
            // {
            //   "status": "pending",
            //   "identifier": {
            //     "type": "permanent-identifier",
            //     "value": "<client-identifier>"
            //   },
            //   "challenges": [
            //     {
            //       "type": "device-attest-01",
            //       "url": "https://localhost:14000/chalZ/zBLseBpgzIAAo67Xcly2-wKsKwDtKR1sqmfDWzZoedA",
            //       "token": "Xi3BjFJjVR88SoO_b04rVON7dj6z3dKQfmWzBgiv7Dk",
            //       "status": "pending"
            //     }
            //   ],
            //   "expires": "2022-02-20T19:05:47Z"
            // }
            NSString *statusString = (NSString *)[response objectForKey:@"status"];
            bool authPending = [statusString isEqualToString:@"pending"];
            NSArray *challenges = (NSArray *)[response objectForKey:@"challenges"];
            NSUInteger idx, count = [challenges count];
            for (idx = 0; idx < count; idx++) {
                NSDictionary *challenge = (NSDictionary *)[challenges objectAtIndex:idx];
                NSString *challengeType = (NSString *)[challenge objectForKey:@"type"];
                _challengeURL = (NSString *)[challenge objectForKey:@"url"];
                _token = (NSString *)[challenge objectForKey:@"token"];
                if ([challengeType isEqualToString:@"device-attest-01"] && _challengeURL && _token) {
                    break; /* found a challenge type which we know how to satisfy */
                }
            }
            if (!(_challengeURL && _token && authPending)) {
                errorString = @"failed to request authorization";
                error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidData userInfo:@{NSLocalizedDescriptionKey:errorString}];
                acmedebug("error in state %lld: %@", (long long)_state, error);
            } else {
                _nextMessageURL = _challengeURL; // next request is a challenge response
                _state = AcmeRequestStateReceivedChallenge;
                acmedebug("Processed reply, next URL is %@", _nextMessageURL);
            }
            break;
        }
        case AcmeRequestStateReceivedChallenge: {
            // state 6: we have sent the challenge response;
            // response should contain updated challenge object, e.g.:
            // {
            //   "type": "device-attest-01",
            //   "url": "https://localhost:14000/chalZ/yfiKe8c1hRPqsvqxSIl__8bbUxw8zVIPGVACquGxJZE",
            //   "token": "-abqm4qpgx1RoC5f5J5-AtrW8pB3TJ2lGSK3jOWzvf8",
            //   "status": "pending"
            // }
            NSString *statusString = (NSString *)[response objectForKey:@"status"];
            // RFC 8555 7.1.6: Possible values are "pending", "processing", "valid", "invalid"
            if ([statusString isEqualToString:@"invalid"]) {
                errorString = @"failed to successfully respond to challenge";
                error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidData userInfo:@{NSLocalizedDescriptionKey:errorString}];
                acmedebug("error in state %lld: %@", (long long)_state, error);
            } else if (![statusString isEqualToString:@"valid"]) {
#if 0 // %%% polling is currently not working, skip it for now. rdar://89209631
                // RFC 8555 7.5.1:
                // To check on the status of an authorization, the client sends a POST-
                // as-GET request to the authorization URL, and the server responds with
                // the current authorization object.
                //
                // should wait until authorization status changes to "valid"
                acmedebug("polling authorization URL: %@", _authorizationURL);
                error = [self pollForStatus:_authorizationURL until:@"valid"];
#endif
                // RFC 8555 7.4 notes the following possible order states:
                // - "invalid": The certificate will not be issued.
                // Consider this order process abandoned.
                // - "pending": The server does not believe that the client has
                // fulfilled the requirements. Check the "authorizations" array
                // for entries that are still pending.
                // - "ready": The server agrees that the requirements have been fulfilled,
                // and is awaiting finalization. Submit a finalization request.
                // - "processing": The certificate is being issued.  Send a POST-as-GET
                // request after the time given in the Retry-After header field of the
                // response, if any.
                // - "valid": The server has issued the certificate and provisioned its URL
                // to the "certificate" field of the order.  Download the certificate.
                //
                // must wait until order status changes to "ready"
                if (!error) {
                    acmedebug("polling order URL: %@", _orderURL);
                    error = [self pollForStatus:_orderURL until:@"ready"];
                }
            }
            if (!error) {
                _nextMessageURL = _finalizeOrderURL; // next request is to finalize order
                _state = AcmeRequestStateRespondedToChallenge;
                acmedebug("Processed reply, next URL is %@", _nextMessageURL);
            }
            break;
        }
        case AcmeRequestStateRespondedToChallenge: {
            // state 7: we have sent the finalize order request;
            // response should contain updated order object, e.g.:
            // {
            //   "status": "processing",
            //   "expires": "2022-02-21T18:05:47Z",
            //   "identifiers": [
            //      {
            //        "type": "permanent-identifier",
            //        "value": "<client-identifier>"
            //      }
            //   ],
            //   "finalize": "https://localhost:14000/finalize-order/dBG6WwM2AxC58WpGizbc5s_81IOe9H_77ty1Ryp_UD0",
            //   "authorizations": [
            //     "https://localhost:14000/authZ/NfN9as0W4bPJ4e1ZsBq3U9jydEQnCjUOw7qrlOYzLCs"
            //   ]
            // }
            NSString *statusString = (NSString *)[response objectForKey:@"status"];
            // RFC 8555 7.1.6: Possible values are "pending", "processing", "valid", "invalid"
            if ([statusString isEqualToString:@"invalid"]) {
                errorString = @"failed to finalize order";
                error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidData userInfo:@{NSLocalizedDescriptionKey:errorString}];
                acmedebug("error in state %lld: %@", (long long)_state, error);
            } else if (![statusString isEqualToString:@"valid"]) {
                // order not finalized yet, so we have to wait until it becomes valid
                error = [self pollForStatus:_orderURL until:@"valid"];
            }
            if (!error) {
                _nextMessageURL = _orderURL; // next request is for order status
                _state = AcmeRequestStateFinalizedOrder;
                acmedebug("Processed reply, next URL is %@", _nextMessageURL);
            }
            break;
        }
        case AcmeRequestStateFinalizedOrder: {
            // state 8: we have sent order finalization request and polled for "valid" status;
            // response should contain order object with "valid" status and certificate URL, e.g.:
            // {
            //   "status": "valid",
            //   "expires": "2022-02-21T18:05:47Z",
            //   "identifiers": [
            //     {
            //       "type": "permanent-identifier",
            //       "value": "<client-identifier>"
            //     }
            //   ],
            //   "finalize": "https://localhost:14000/finalize-order/dBG6WwM2AxC58WpGizbc5s_81IOe9H_77ty1Ryp_UD0",
            //   "authorizations": [
            //     "https://localhost:14000/authZ/NfN9as0W4bPJ4e1ZsBq3U9jydEQnCjUOw7qrlOYzLCs"
            //   ],
            //   "certificate": "https://localhost:14000/certZ/43c146b3e4ce9bc1"
            // }
            _certificateURL = (NSString *)[response objectForKey:@"certificate"];
            NSString *statusString = (NSString *)[response objectForKey:@"status"];
            bool orderValid = [statusString isEqualToString:@"valid"];
            if (!(_certificateURL && orderValid)) {
                errorString = [NSString stringWithFormat:@"order status is \"%@\", not yet \"valid\"", statusString];
                error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidData userInfo:@{NSLocalizedDescriptionKey:errorString}];
                acmedebug("error in state %lld: %@", (long long)_state, error);
            }
            if (!error) {
                _nextMessageURL = _certificateURL; // next request is for the certificate
                _state = AcmeRequestStateCertificateIssued;
            }
            break;
        }
        case AcmeRequestStateCertificateIssued: {
            // state 9: response should contain the certificate
            NSData *pemData = (NSData *)[response objectForKey:@"certificate"];
            NSLog(@"Got certificate: %@", pemData);
            _certificate = SecCertificateCreateWithPEM(NULL, (__bridge CFDataRef)pemData);
            if (!_certificate) {
                errorString = @"failed to obtain certificate";
                error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidData userInfo:@{NSLocalizedDescriptionKey:errorString}];
                acmedebug("error in state %lld: %@", (long long)_state, error);
            }
            if (!error) {
                _nextMessageURL = nil; // no more requests needed!
                _state = AcmeRequestStateComplete;
            }
            break;
        }
        case AcmeRequestStateComplete: {
           // all done
           break;
        }
        default: {
            errorString = @"unknown or uninitialized ACME state";
            error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecBadReq userInfo:@{NSLocalizedDescriptionKey:errorString}];
            acmedebug("error in state %lld: %@", (long long)_state, error);
        }
    }
    return error;
}

- (BOOL) valueForBooleanDefault:(NSString *)defaultName {
    Boolean bval = false;
    CFTypeRef val = (CFTypeRef)CFPreferencesCopyValue((__bridge CFStringRef)defaultName,
                    CFSTR("com.apple.security"),
                    kCFPreferencesCurrentUser,
                    kCFPreferencesAnyHost);
    if (val && (CFGetTypeID(val) == CFBooleanGetTypeID())) {
        bval = CFBooleanGetValue((CFBooleanRef)val);
    }
    CFReleaseNull(val);
    return (bval) ? YES : NO;
}

- (NSError *) executeRequest {
    NSError *error = nil;
    NSDictionary *acmeReply = nil;

    if (_state < AcmeRequestStateInitialized) {
        /* Clear existing certificate and identity, if any */
        dispatch_sync(_queue, ^{
            CFReleaseNull(_certificate);
            CFReleaseNull(_identity);
            /* Generate request timestamp (for logging/debug purposes) */
            NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
            dateFormatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
            dateFormatter.dateFormat = @"yyyy-MM-dd-HH-mm-ss-SSSS";
            _timestamp = [dateFormatter stringFromDate:[NSDate now]];
            /* Read defaults */
            _logAcmeCSR = [self valueForBooleanDefault:@"LogAcmeCSR"];
            _permitLocalIssuer = [self valueForBooleanDefault:@"PermitLocalIssuer"];
            _requireAttestation = [self valueForBooleanDefault:@"RequireAttestation"];
        });
        /* Generate key pair */
        if ((error = [self createKeyPair]) != nil) {
            goto out;
        }
        /* Generate CSR */
        if ((error = [self createCSR]) != nil) {
            goto out;
        }
        /* Get ACME directory URL from input parameters */
        _nextMessageURL = [_parameters[(__bridge id)kSecACMEDirectoryURL] absoluteString];
        if (!_nextMessageURL) {
            _nextMessageURL = [_parameters[(__bridge id)kSecACMEServerURL] absoluteString];
        }
        _state = AcmeRequestStateInitialized;
        if (!_nextMessageURL) {
            error = [self createCertificate]; /* handle this request locally */
            _state = AcmeRequestStateComplete; /* exit state machine */
        }
    }
    while (_state < AcmeRequestStateComplete) {
        acmeReply = nil;
        if (!(error = [self sendRequestToXPCService:[self acmeRequest] response:&acmeReply])) {
            error = [self processReply:acmeReply];
        }
        if (error) {
            secerror("ACME request flow failed at step %lld: %@", (long long)_state, error);
            CFBooleanRef permitLocal = (__bridge CFBooleanRef)[_parameters objectForKey:(__bridge NSString*)kSecACMEPermitLocalIssuer];
            CFRetainSafe(permitLocal);
            if (_permitLocalIssuer || (permitLocal &&
                CFGetTypeID(permitLocal) == CFBooleanGetTypeID() &&
                CFEqualSafe(permitLocal, kCFBooleanTrue))) {
                error = [self createCertificate]; // fallback to create the certificate locally
            }
            CFReleaseSafe(permitLocal);
            _state = AcmeRequestStateComplete;
        }
    }
out:
    return error;
}

- (SecIdentityRef) identityWithError:(NSError**)error
{
    /* Run the state machine until we have a certificate, or fail to get it, or time out. */
    _identity = nil;
    NSError *localError = [self executeRequest];
    if (!localError && _certificate && _privateKey) {
        _identity = SecIdentityCreate(kCFAllocatorDefault, _certificate, _privateKey);
    }
    if (!_identity) {
        /* Failed to create the identity */
        if (!localError) {
            NSString *errorString = [NSString stringWithFormat:@"failed to create identity (check input values)"];
            localError = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidRequestInputs userInfo:@{NSLocalizedDescriptionKey:errorString}];
        }
        if (_privateKey) {
            /* Don't need to retain the private key on failure */
            NSDictionary *privKeyQuery = @{
                (id)kSecClass : (id)kSecClassKey,
                (id)kSecValueRef : (__bridge id)_privateKey,
            };
            (void)SecItemDelete((__bridge CFDictionaryRef)privKeyQuery);
        }
    }
    if (_publicKey) {
        /* Don't need to retain the public key, regardless of outcome */
        NSDictionary *pubKeyQuery = @{
            (id)kSecClass : (id)kSecClassKey,
            (id)kSecValueRef : (__bridge id)_publicKey,
        };
        (void)SecItemDelete((__bridge CFDictionaryRef)pubKeyQuery);
    }
    if (localError) { secerror("identityWithError: %@", localError); }
    if (error) { *error = localError; }
    else { localError = nil; }
    return _identity;
}

/*
 Tasks:
 - get directory info (contains URLs for making subsequent requests)
 - get initial nonce
 - establish a new account with the server (account is only needed for lifetime of request)
 - submit order for certificate to be issued to identifier
 - fetch the challenges which we need to fulfill
 - respond to the challenge by provide attestation and pkey being attested
 - check issuance status
 - finalize order (provide csr)
 - check issuance status
 - fetch the issued certificate

 Flow table from RFC 8555
 +-------------------+--------------------------------+--------------+
 | Action            | Request                        | Response     |
 +-------------------+--------------------------------+--------------+
 | Get directory     | GET  directory                 | 200          |
 |                   |                                |              |
 | Get nonce         | HEAD newNonce                  | 200          |
 |                   |                                |              |
 | Create account    | POST newAccount                | 201 ->       |
 |                   |                                | account      |
 |                   |                                |              |
 | Submit order      | POST newOrder                  | 201 -> order |
 |                   |                                |              |
 | Fetch challenges  | POST-as-GET order's            | 200          |
 |                   | authorization urls             |              |
 |                   |                                |              |
 | Respond to        | POST authorization challenge   | 200          |
 | challenges        | urls                           |              |
 |                   |                                |              |
 | Poll for status   | POST-as-GET order              | 200          |
 |                   |                                |              |
 | Finalize order    | POST order's finalize url      | 200          |
 |                   |                                |              |
 | Poll for status   | POST-as-GET order              | 200          |
 |                   |                                |              |
 | Download          | POST-as-GET order's            | 200          |
 | certificate       | certificate url                |              |
 +-------------------+--------------------------------+--------------+
 */

@end

/* entry point */

void SecRequestClientIdentity(CFArrayRef subject,
    CFDictionaryRef parameters,
    dispatch_queue_t queue,
    SecRequestIdentityCallback result)
{
    const char *paramErrStr = NULL;
    if (subject == NULL) { paramErrStr = "subject"; }
    if (parameters == NULL) { paramErrStr = "parameters"; }
    if (queue == NULL) { paramErrStr = "queue"; }
    if (result == NULL) { paramErrStr = "result block"; }
    if (paramErrStr != NULL) {
        NSString *errorString = [NSString stringWithFormat:@"SecRequestClientIdentity was called with NULL %s", paramErrStr];
        if (result) {
            NSError* error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey:errorString}];
            result(NULL, (__bridge CFErrorRef)error);
        } else {
            secerror("%@", errorString);
        }
        return;
    }
    CFRetainSafe(subject);
    CFRetainSafe(parameters);
    /* hold a transaction while this certificate request is in flight */
    __block os_transaction_t transaction = os_transaction_create("com.apple.security.cert-request");
    dispatch_async(queue, ^{
        @autoreleasepool {
            AcmeCertRequest *context = [[AcmeCertRequest alloc]
                initWithSubject:(__bridge NSArray*)subject
                     parameters:(__bridge NSDictionary*)parameters];
            NSError *error = [context sanitizeParameters];
            /* note: identity is owned and released by the AcmeCertRequest context */
            SecIdentityRef identity = (error) ? NULL : [context identityWithError:&error];
            result(identity, (__bridge CFErrorRef)error);
            CFReleaseSafe(parameters);
            CFReleaseSafe(subject);
            error = nil;
            context = nil;
            transaction = nil;
        }
    });
}

