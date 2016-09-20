/*
 * Copyright (c) 2008-2010,2012,2016 Apple Inc. All Rights Reserved.
 */

#include <Foundation/Foundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecPolicy.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustSettings.h>
#include <Security/SecTrustSettingsPriv.h>
#include <utilities/SecCFRelease.h>
#include <stdlib.h>
#include <unistd.h>

#if TARGET_OS_IPHONE
#include <Security/SecTrustStore.h>
#else
#include <Security/SecKeychain.h>
#endif

#include "shared_regressions.h"

#include "si-28-sectrustsettings.h"

/* Of course, the interface is different for OS X and iOS. */
/* each call is 1 test */
#if TARGET_OS_IPHONE
#define setTS(cert, settings) \
{ \
    ok_status(SecTrustStoreSetTrustSettings(defaultStore, cert, settings), \
        "set trust settings"); \
}
#else
/* Use admin store on OS X to avoid user prompts.
 * Sleep a little so trustd has time to get the KeychainEvent. */
#define setTS(cert, settings) \
{ \
    ok_status(SecTrustSettingsSetTrustSettings(cert, kSecTrustSettingsDomainAdmin, \
        settings), "set trust settings"); \
    usleep(20000); \
}
#endif

#if TARGET_OS_IPHONE
#define setTSFail(cert, settings) \
{ \
    is(SecTrustStoreSetTrustSettings(defaultStore, cert, settings), errSecParam, \
        "set trust settings"); \
}
#else
#define setTSFail(cert, settings) \
{ \
    is(SecTrustSettingsSetTrustSettings(cert, kSecTrustSettingsDomainAdmin, \
        settings), errSecParam, "set trust settings"); \
}
#endif

/* each call is 1 test */
#if TARGET_OS_IPHONE
#define removeTS(cert) \
{ \
    ok_status(SecTrustStoreRemoveCertificate(defaultStore, cert), \
        "remove trust settings"); \
}
#else
#define removeTS(cert) \
{ \
    ok_status(SecTrustSettingsRemoveTrustSettings(cert, kSecTrustSettingsDomainAdmin), \
        "remove trust settings"); \
}
#endif

/* each call is 4 tests */
#define check_trust(certs, policy, valid_date, expected) \
{ \
    SecTrustRef trust = NULL; \
    SecTrustResultType trust_result; \
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), \
        "create trust with " #policy " policy"); \
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)valid_date), \
        "set trust verify date"); \
    ok_status(SecTrustEvaluate(trust, &trust_result), "evaluate trust"); \
    is(trust_result, expected, \
        "check trust result for " #policy " policy"); \
    CFReleaseSafe(trust); \
}

static SecCertificateRef cert0 = NULL;
static SecCertificateRef cert1 = NULL;
static SecCertificateRef cert2 = NULL;
static SecCertificateRef cert3 = NULL;
static SecPolicyRef sslPolicy = NULL;
static SecPolicyRef smimePolicy = NULL;
static SecPolicyRef basicPolicy = NULL;
static CFArrayRef sslChain = NULL;
static CFArrayRef smimeChain = NULL;
static NSDate *verify_date = nil;

#if TARGET_OS_IPHONE
static SecTrustStoreRef defaultStore = NULL;
#else
#define kSystemLoginKeychainPath "/Library/Keychains/System.keychain"
static NSMutableArray *deleteMeCertificates = NULL;
#endif


static void setup_globals(void) {

    cert0 = SecCertificateCreateWithBytes(NULL, _trustSettingsRoot, sizeof(_trustSettingsRoot));
    cert1 = SecCertificateCreateWithBytes(NULL, _trustSettingsInt, sizeof(_trustSettingsInt));
    cert2 = SecCertificateCreateWithBytes(NULL, _trustSettingsSSLLeaf, sizeof(_trustSettingsSSLLeaf));
    cert3 = SecCertificateCreateWithBytes(NULL, _trustSettingsSMIMELeaf, sizeof(_trustSettingsSMIMELeaf));

    sslPolicy = SecPolicyCreateSSL(true, CFSTR("testserver.apple.com"));
    smimePolicy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME, CFSTR("username@apple.com"));
    basicPolicy = SecPolicyCreateBasicX509();

    const void *v_certs1[] = { cert2, cert1, cert0 };
    sslChain = CFArrayCreate(NULL, v_certs1, sizeof(v_certs1)/sizeof(*v_certs1), &kCFTypeArrayCallBacks);

    const void *v_certs2[] = { cert3, cert1, cert0 };
    smimeChain = CFArrayCreate(NULL, v_certs2, sizeof(v_certs2)/sizeof(*v_certs2), &kCFTypeArrayCallBacks);

    verify_date = [NSDate dateWithTimeIntervalSinceReferenceDate:482000000.0]; // Apr 10 2016

#if TARGET_OS_IPHONE
    defaultStore = SecTrustStoreForDomain(kSecTrustStoreDomainUser);
#else
    /* Since we're putting trust settings in the admin domain,
     * we need to add the certs to the system keychain. */
    SecKeychainRef kcRef = NULL;
    CFArrayRef certRef = NULL;
    NSDictionary *attrs = nil;

    SecKeychainOpen(kSystemLoginKeychainPath, &kcRef);
    if (!kcRef) {
        goto out;
    }

    deleteMeCertificates = [[NSMutableArray alloc] init];

    attrs = @{(__bridge NSString*)kSecValueRef: (__bridge id)cert0,
              (__bridge NSString*)kSecUseKeychain: (__bridge id)kcRef,
              (__bridge NSString*)kSecReturnPersistentRef: @YES};
    if (SecItemAdd((CFDictionaryRef)attrs, (void *)&certRef) == 0)
        [deleteMeCertificates addObject:(__bridge NSArray *)certRef];
    CFReleaseNull(certRef);

    attrs = @{(__bridge NSString*)kSecValueRef: (__bridge id)cert1,
              (__bridge NSString*)kSecUseKeychain: (__bridge id)kcRef,
              (__bridge NSString*)kSecReturnPersistentRef: @YES};
    if (SecItemAdd((CFDictionaryRef)attrs, (void *)&certRef) == 0)
        [deleteMeCertificates addObject:(__bridge NSArray *)certRef];
    CFReleaseNull(certRef);

    attrs = @{(__bridge NSString*)kSecValueRef: (__bridge id)cert2,
                             (__bridge NSString*)kSecUseKeychain: (__bridge id)kcRef,
                             (__bridge NSString*)kSecReturnPersistentRef: @YES};
    if (SecItemAdd((CFDictionaryRef)attrs, (void *)&certRef) == 0)
        [deleteMeCertificates addObject:(__bridge NSArray *)certRef];
    CFReleaseNull(certRef);

    attrs = @{(__bridge NSString*)kSecValueRef: (__bridge id)cert3,
              (__bridge NSString*)kSecUseKeychain: (__bridge id)kcRef,
              (__bridge NSString*)kSecReturnPersistentRef: @YES};
    if (SecItemAdd((CFDictionaryRef)attrs, (void *)&certRef) == 0)
        [deleteMeCertificates addObject:(__bridge NSArray *)certRef];
    CFReleaseNull(certRef);

    out:
    CFReleaseNull(kcRef);
#endif
}

static void cleanup_globals(void) {
#if !TARGET_OS_IPHONE
    [deleteMeCertificates enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        SecItemDelete((CFDictionaryRef)@{ (__bridge NSString*)kSecValuePersistentRef: [obj objectAtIndex:0]});
    }];
#endif

    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(cert2);
    CFReleaseNull(cert3);
    CFReleaseNull(sslPolicy);
    CFReleaseNull(smimePolicy);
    CFReleaseNull(basicPolicy);
    CFReleaseNull(sslChain);
    CFReleaseNull(smimeChain);
}

#define kNumberNoConstraintsTests (17+7*4)
static void test_no_constraints(void) {
    /* root with the default TrustRoot result succeeds */
    setTS(cert0, NULL);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultUnspecified);
    removeTS(cert0);

    /* intermediate with the default TrustRoot result fails */
    setTSFail(cert1, NULL);

    /* root with TrustRoot result succeeds */
    NSDictionary *trustRoot = @{ (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustRoot)};
    setTS(cert0, (__bridge CFDictionaryRef)trustRoot);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultUnspecified);
    removeTS(cert0);

    /* intermediate with TrustRoot fails to set */
    setTSFail(cert1, (__bridge CFDictionaryRef)trustRoot);

    /* root with TrustAsRoot fails to set */
    NSDictionary *trustAsRoot = @{ (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustAsRoot)};
    setTSFail(cert0, (__bridge CFDictionaryRef)trustAsRoot);

    /* intermediate with TrustAsRoot result succeeds */
    setTS(cert1, (__bridge CFDictionaryRef)trustAsRoot);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultUnspecified);
    removeTS(cert1);

    /* trusting the root but denying the intermediate fails */
    NSDictionary *deny = @{ (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultDeny)};
    setTS(cert0, NULL);
    setTS(cert1, (__bridge CFDictionaryRef)deny);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultDeny);
    removeTS(cert1);
    removeTS(cert0);

    /* the unspecified result gives us default behavior */
    NSDictionary *unspecified = @{ (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultUnspecified)};
    setTS(cert1, (__bridge CFDictionaryRef)unspecified);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert1);

    /* trusting one leaf doesn't make other leaf trusted */
    setTS(cert2, (__bridge CFDictionaryRef)trustAsRoot);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultUnspecified);
    check_trust(smimeChain, basicPolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert2);
}

#define kNumberPolicyConstraintsTests (2+3*4)
static void test_policy_constraints(void) {
    /* Trust only for SSL server. SSL server policy succeeds. */
    NSDictionary *sslServerAllowed = @{ (__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)sslPolicy,
                                        (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustAsRoot) };
    setTS(cert1, (__bridge CFDictionaryRef)sslServerAllowed);
    check_trust(sslChain, sslPolicy, verify_date, kSecTrustResultUnspecified);

    /* SSL client policy fails. */
    SecPolicyRef sslClient = SecPolicyCreateSSL(false, NULL);
    check_trust(sslChain, sslClient, verify_date, kSecTrustResultRecoverableTrustFailure);
    CFReleaseNull(sslClient);

    /* Basic policy fails */
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert1);
}

#define kNumberPolicyStringConstraintsTests (4+6*4)
static void test_policy_string_constraints(void) {
    NSArray *hostnameAllowed = @[ @{ (__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)sslPolicy,
                                     (__bridge NSString*)kSecTrustSettingsPolicyString: @("wrongname.apple.com"),
                                     (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultDeny) },
                                  @{ (__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)sslPolicy,
                                     (__bridge NSString*)kSecTrustSettingsPolicyString: @("testserver.apple.com"),
                                     (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustAsRoot) }
                                  ];
    setTS(cert2, (__bridge CFArrayRef)hostnameAllowed);
    /* evaluating against trusted hostname passes */
    check_trust(sslChain, sslPolicy, verify_date, kSecTrustResultUnspecified);

    /* evaluating against hostname not in trust settings is recoverable failure */
    SecPolicyRef weirdnamePolicy = SecPolicyCreateSSL(true, CFSTR("weirdname.apple.com"));
    check_trust(sslChain, weirdnamePolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    CFReleaseNull(weirdnamePolicy);

    /* evaluating against hostname denied by trust settings is denied */
    SecPolicyRef wrongnamePolicy = SecPolicyCreateSSL(true, CFSTR("wrongname.apple.com"));
    check_trust(sslChain, wrongnamePolicy, verify_date, kSecTrustResultDeny);
    CFReleaseNull(wrongnamePolicy);
    removeTS(cert2);

    NSArray *emailAllowed = @[ @{ (__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)smimePolicy,
                                  (__bridge NSString*)kSecTrustSettingsPolicyString: @("wrongemail@apple.com"),
                                  (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultDeny) },
                               @{ (__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)smimePolicy,
                                  (__bridge NSString*)kSecTrustSettingsPolicyString: @("username@apple.com"),
                                  (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustAsRoot) }
                               ];
    setTS(cert3, (__bridge CFArrayRef)emailAllowed);
    /* evaluating against trusted email passes */
    check_trust(smimeChain, smimePolicy, verify_date, kSecTrustResultUnspecified);

    /* evaluating against hostname not in trust settings is recoverable failure */
    SecPolicyRef weirdemailPolicy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME, CFSTR("weirdemail@apple.com"));
    check_trust(smimeChain, weirdemailPolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    CFReleaseNull(weirdemailPolicy);

    /* evaluating against hostname denied by trust settings is denied */
    SecPolicyRef wrongemailPolicy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME, CFSTR("wrongemail@apple.com"));
    check_trust(smimeChain, wrongemailPolicy, verify_date, kSecTrustResultDeny);
    CFReleaseNull(wrongemailPolicy);
    removeTS(cert3);
}

#if TARGET_OS_IPHONE
#define kNumberApplicationsConstraintsTests 0
static void test_application_constraints(void) {}
#else
#include <Security/SecTrustedApplicationPriv.h>
#define kNumberApplicationsConstraintsTests (2+4+2*4)
static void test_application_constraints(void) {
    SecTrustedApplicationRef thisApp = NULL, someOtherApp = NULL;

    ok_status(SecTrustedApplicationCreateFromPath(NULL, &thisApp),
              "create TrustedApplicationRef for this app");
    ok_status(SecTrustedApplicationCreateFromPath("/Applications/Safari.app", &someOtherApp),
              "create TrustedApplicationRef for Safari");

    NSDictionary *thisAppTS = @{ (__bridge NSString*)kSecTrustSettingsApplication: (__bridge id)thisApp,
                                 (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustRoot)};

    NSDictionary *someOtherAppTS = @{ (__bridge NSString*)kSecTrustSettingsApplication: (__bridge id)someOtherApp,
                                      (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustRoot)};

    /* This application Trust Setting succeeds */
    setTS(cert0, (__bridge CFDictionaryRef)thisAppTS);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultUnspecified);
    removeTS(cert0);

    /* Some other application Trust Setting fails */
    setTS(cert0, (__bridge CFDictionaryRef)someOtherAppTS);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert0);

    CFReleaseNull(thisApp);
    CFReleaseNull(someOtherApp);
}
#endif

#define kNumberKeyUsageConstraintsTests (14+11*4)
static void test_key_usage_constraints(void) {
    /* any key usage succeeds */
    NSDictionary *anyKeyUse = @{ (__bridge NSString*)kSecTrustSettingsKeyUsage: @(kSecTrustSettingsKeyUseAny),
                                 (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustRoot)};
    setTS(cert0, (__bridge CFDictionaryRef)anyKeyUse);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultUnspecified);
    removeTS(cert0);

    /* signCert key usage on an intermediate or root succeeds */
    NSDictionary *signCertUseRoot = @{ (__bridge NSString*)kSecTrustSettingsKeyUsage: @(kSecTrustSettingsKeyUseSignCert),
                                   (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustRoot)};
    setTS(cert0, (__bridge CFDictionaryRef)signCertUseRoot);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultUnspecified);
    removeTS(cert0)

    NSDictionary *signCertUseInt = @{ (__bridge NSString*)kSecTrustSettingsKeyUsage: @(kSecTrustSettingsKeyUseSignCert),
                                       (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustAsRoot)};
    setTS(cert1, (__bridge CFDictionaryRef)signCertUseInt);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultUnspecified);
    removeTS(cert1);

    /* intermediate without signCert key usage fails */
    NSDictionary *signatureUse = @{ (__bridge NSString*)kSecTrustSettingsKeyUsage: @(kSecTrustSettingsKeyUseSignature),
                                   (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustAsRoot)};
    setTS(cert1, (__bridge CFDictionaryRef)signatureUse);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert1);

    /* brief interlude to create a bunch of SMIME policies with different key usages */
    SecPolicyRef smimeSignature = SecPolicyCreateSMIME(kSecSignSMIMEUsage, CFSTR("username@apple.com"));
    SecPolicyRef smimeDataEncrypt = SecPolicyCreateSMIME(kSecDataEncryptSMIMEUsage, CFSTR("username@apple.com"));
    SecPolicyRef smimeKeyEncrypt = SecPolicyCreateSMIME(kSecKeyEncryptSMIMEUsage, CFSTR("username@apple.com"));
    SecPolicyRef smimeKeyExchange = SecPolicyCreateSMIME(kSecKeyExchangeBothSMIMEUsage, CFSTR("username@apple.com"));
    SecPolicyRef smimeMultiple = SecPolicyCreateSMIME((kSecSignSMIMEUsage | kSecKeyEncryptSMIMEUsage),
                                                      CFSTR("username@apple.com"));

    /* signature smime policy passes for signature use TS*/
    setTS(cert3, (__bridge CFDictionaryRef)signatureUse);
    check_trust(smimeChain, smimeSignature, verify_date, kSecTrustResultUnspecified);

    /* any use policy fails for signature use TS */
    check_trust(smimeChain, smimePolicy, verify_date, kSecTrustResultRecoverableTrustFailure);

    /* multiple use smime policy against signature use */
    check_trust(smimeChain, smimeMultiple, verify_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert3);

    /* key encrypt smime policy passes for key encrypt use */
    NSDictionary *keyEncryptUse = @{ (__bridge NSString*)kSecTrustSettingsKeyUsage: @(kSecTrustSettingsKeyUseEnDecryptKey),
                                      (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustAsRoot)};
    setTS(cert3, (__bridge CFDictionaryRef)keyEncryptUse);
    check_trust(smimeChain, smimeKeyEncrypt, verify_date, kSecTrustResultUnspecified);
    removeTS(cert3);

    /* multiple use smime policy against multiple uses */
    NSDictionary *multipleUse = @{ (__bridge NSString*)kSecTrustSettingsKeyUsage: @(kSecTrustSettingsKeyUseEnDecryptKey |
                                       kSecTrustSettingsKeyUseSignature),
                                      (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustAsRoot)};
    setTS(cert3, (__bridge CFDictionaryRef)multipleUse)
    check_trust(smimeChain, smimeMultiple, verify_date, kSecTrustResultUnspecified);

    /* signature smime policy against multiple uses */
    check_trust(smimeChain, smimeSignature, verify_date, kSecTrustResultRecoverableTrustFailure);

    /* any use smime policy against multiple uses */
    check_trust(smimeChain, smimePolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert3);

    CFReleaseNull(smimeSignature);
    CFReleaseNull(smimeDataEncrypt);
    CFReleaseNull(smimeKeyEncrypt);
    CFReleaseNull(smimeKeyExchange);
    CFReleaseNull(smimeMultiple);
}

#define kNumberAllowedErrorsTests (14+8*4)
static void test_allowed_errors(void) {
    setTS(cert0, NULL);

    /* allow expired errors */
    NSDate *expired_date = [NSDate dateWithTimeIntervalSinceReferenceDate:520000000.0]; // Jun 24 2017
    check_trust(sslChain, basicPolicy, expired_date, kSecTrustResultRecoverableTrustFailure);

    NSDictionary *allowExpired = @{ (__bridge NSString*)kSecTrustSettingsAllowedError: @(-2147409654),
                                    (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultUnspecified)};
    setTS(cert1, (__bridge CFDictionaryRef)allowExpired)
    setTS(cert2, (__bridge CFDictionaryRef)allowExpired);
    check_trust(sslChain, basicPolicy, expired_date, kSecTrustResultUnspecified);
    removeTS(cert2);
    removeTS(cert1);

    /* allow hostname mismatch errors */
    SecPolicyRef wrongNameSSL = NULL;
    wrongNameSSL = SecPolicyCreateSSL(true, CFSTR("wrongname.apple.com"));
    check_trust(sslChain, wrongNameSSL, verify_date, kSecTrustResultRecoverableTrustFailure);

    NSDictionary *allowHostnameMismatch = @{ (__bridge NSString*)kSecTrustSettingsAllowedError: @(-2147408896),
                                             (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultUnspecified) };
    setTS(cert2, (__bridge CFDictionaryRef)allowHostnameMismatch);
    sleep(1); // sleep a little extra so trustd gets trust settings event before evaluating leaf
    check_trust(sslChain, wrongNameSSL, verify_date, kSecTrustResultUnspecified);
    removeTS(cert2);
    CFReleaseNull(wrongNameSSL);

    /* allow email mismatch errors */
    SecPolicyRef wrongNameSMIME = NULL;
    wrongNameSMIME = SecPolicyCreateSMIME(kSecAnyEncryptSMIME, CFSTR("test@apple.com"));
    check_trust(smimeChain, wrongNameSMIME, verify_date, kSecTrustResultRecoverableTrustFailure);

    NSDictionary *allowEmailMismatch = @{ (__bridge NSString*)kSecTrustSettingsAllowedError: @(-2147408872),
                                          (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultUnspecified) };
    setTS(cert3, (__bridge CFDictionaryRef)allowEmailMismatch);
    sleep(1); // sleep a little extra so trustd gets trust settings event before evaluating leaf
    check_trust(smimeChain, wrongNameSMIME, verify_date, kSecTrustResultUnspecified);
    removeTS(cert3);
    CFReleaseNull(wrongNameSMIME);

    /* allowed error with a policy constraint */
    NSDictionary *allowExpiredConstrained = @{ (__bridge NSString*)kSecTrustSettingsAllowedError: @(-2147409654),
                                               (__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)sslPolicy,
                                               (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultUnspecified)};
    setTS(cert1, (__bridge CFDictionaryRef)allowExpiredConstrained)
    setTS(cert2, (__bridge CFDictionaryRef)allowExpiredConstrained);
    check_trust(sslChain, sslPolicy, expired_date, kSecTrustResultUnspecified);
    check_trust(sslChain, basicPolicy, expired_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert2);
    removeTS(cert1);

    removeTS(cert0);
}

#define kNumberMultipleConstraintsTests (8+9*4)
static void test_multiple_constraints(void) {
    /* deny all but */
    NSArray *denyAllBut = @[
                            @{(__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)sslPolicy ,
                              (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustRoot)},
                            @{(__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultDeny) }
                            ];
    setTS(cert0, (__bridge CFArrayRef)denyAllBut);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultDeny);
    check_trust(sslChain, sslPolicy, verify_date, kSecTrustResultUnspecified);
    removeTS(cert0);

    /* allow all but */
    NSArray *allowAllBut = @[
                             @{(__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)sslPolicy ,
                               (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultUnspecified)},
                             @{(__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustRoot) }
                             ];
    setTS(cert0, (__bridge CFArrayRef)allowAllBut);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultUnspecified);
    check_trust(sslChain, sslPolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert0);

    /* different results for specific policies */
    NSArray *specifyPolicyResult = @[
                                     @{(__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)sslPolicy,
                                       (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultDeny)},
                                     @{(__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)basicPolicy,
                                       (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustRoot) }
                                     ];
    setTS(cert0, (__bridge CFArrayRef)specifyPolicyResult);
    check_trust(sslChain, basicPolicy, verify_date, kSecTrustResultUnspecified);
    check_trust(sslChain, sslPolicy, verify_date, kSecTrustResultDeny);
    check_trust(smimeChain, smimePolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert0);

    /* different results for additional constraint with same policy */
    NSArray *policyConstraintResult = @[
                                     @{(__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)sslPolicy,
                                       (__bridge NSString*)kSecTrustSettingsPolicyString: @("wrongname.apple.com"),
                                       (__bridge NSString*)kSecTrustSettingsAllowedError: @(-2147408896),
                                       (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustAsRoot)},
                                     @{(__bridge NSString*)kSecTrustSettingsPolicy: (__bridge id)sslPolicy,
                                       (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultUnspecified) }
                                     ];
    SecPolicyRef wrongNameSSL = NULL;
    wrongNameSSL = SecPolicyCreateSSL(true, CFSTR("wrongname.apple.com"));
    setTS(cert2, (__bridge CFArrayRef)policyConstraintResult);
    sleep(1); // sleep a little extra so trustd gets trust settings event before evaluating leaf
    check_trust(sslChain, wrongNameSSL, verify_date, kSecTrustSettingsResultUnspecified);
    check_trust(sslChain, sslPolicy, verify_date, kSecTrustResultRecoverableTrustFailure);
    removeTS(cert2);
    CFReleaseNull(wrongNameSSL);

}

int si_28_sectrustsettings(int argc, char *const *argv)
{
    plan_tests(kNumberNoConstraintsTests +
               kNumberPolicyConstraintsTests +
               kNumberPolicyStringConstraintsTests +
               kNumberApplicationsConstraintsTests +
               kNumberKeyUsageConstraintsTests +
               kNumberAllowedErrorsTests +
               kNumberMultipleConstraintsTests);

#if !TARGET_OS_IPHONE
    if (getuid() != 0) {
        printf("Test must be run as root on OS X");
        return 0;
    }
#endif

    @autoreleasepool {
        setup_globals();
        test_no_constraints();
        test_policy_constraints();
        test_policy_string_constraints();
        test_application_constraints();
        test_key_usage_constraints();
        test_allowed_errors();
        test_multiple_constraints();
        cleanup_globals();
    }

    return 0;
}
