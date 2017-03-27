/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
 */

#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <utilities/SecCFRelease.h>
#include <Security/SecTrustSettings.h>
#include <Security/SecTrustSettingsPriv.h>

#if TARGET_OS_IPHONE
#include <Security/SecTrustStore.h>
#else
#include <Security/SecKeychain.h>
#endif

#include "shared_regressions.h"

#include "si-29-sectrust-sha1-deprecation.h"

#import <Foundation/Foundation.h>

static SecCertificateRef sha1_root = NULL;

#if TARGET_OS_IPHONE
static SecTrustStoreRef defaultStore = NULL;
#else
#define kSystemLoginKeychainPath "/Library/Keychains/System.keychain"
static NSMutableArray *deleteMeCertificates = NULL;
#endif


static void setup_globals(void) {

    sha1_root = SecCertificateCreateWithBytes(NULL, _digiCertRoot, sizeof(_digiCertRoot));

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

    attrs = @{(__bridge NSString*)kSecValueRef: (__bridge id)sha1_root,
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

    CFReleaseNull(sha1_root);
}


static void setTrust(SecTrustRef *trust, NSArray *certs, SecPolicyRef policy)
{
    // November 4, 2016 at 5:53:20 PM PDT
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:500000000.0];
    CFReleaseNull(*trust);
    require_noerr_string(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, trust), cleanup, "failed to create trust");
    require_noerr_string(SecTrustSetVerifyDate(*trust, (__bridge CFDateRef)verifyDate),
                         cleanup, "failed to set verify date");
cleanup:
    return;
}

static void tests(void)
{
    SecCertificateRef sha1_leaf = NULL, sha1_int = NULL,
    sha2_leaf = NULL, sha2_int = NULL;
    NSArray *anchors = nil, *sha1_certs = nil, *sha2_certs = nil;
    SecPolicyRef serverPolicy = NULL, clientPolicy = NULL;
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;

    sha1_leaf = SecCertificateCreateWithBytes(NULL, _badssl_sha1, sizeof(_badssl_sha1));
    sha1_int = SecCertificateCreateWithBytes(NULL, _digiCertSSCA, sizeof(_digiCertSSCA));
    sha2_leaf = SecCertificateCreateWithBytes(NULL, _badssl_sha2, sizeof(_badssl_sha2));
    sha2_int = SecCertificateCreateWithBytes(NULL, _COMODO_DV, sizeof(_COMODO_DV));

    /* SHA1 cert from system roots fails SSL server policy*/
    sha1_certs = @[ (__bridge id)sha1_leaf, (__bridge id)sha1_int];
    serverPolicy = SecPolicyCreateSSL(true, CFSTR("www.badssl.com"));
    setTrust(&trust, sha1_certs, serverPolicy);
    require_noerr_string(SecTrustEvaluate(trust, &trustResult), cleanup, "failed to evaluate trust");
    is(trustResult, kSecTrustResultRecoverableTrustFailure, "reject test: system-trusted SHA-1 SSL server");

    /* Add trust setting for root */
#if TARGET_OS_IPHONE
    require_noerr_string(SecTrustStoreSetTrustSettings(defaultStore, sha1_root, NULL),
                         cleanup, "failed to set trust settings");
#else 
    require_noerr_string(SecTrustSettingsSetTrustSettings(sha1_root, kSecTrustSettingsDomainAdmin,
                                                          NULL),
                         cleanup, "failed to set trust settings");
    usleep(20000);
#endif

    /* SHA1 cert now passes SSL server*/
    setTrust(&trust, sha1_certs, serverPolicy);
    require_noerr_string(SecTrustEvaluate(trust, &trustResult), cleanup, "failed to evaluate trust");
    is(trustResult, kSecTrustResultUnspecified, "accept test: user-trusted SHA-1 SSL server");

    /* Remove trust setting for root */
#if TARGET_OS_IPHONE
    require_noerr_string(SecTrustStoreRemoveCertificate(defaultStore, sha1_root),
                         cleanup, "failed to remove trust settings");
#else
    require_noerr_string(SecTrustSettingsRemoveTrustSettings(sha1_root, kSecTrustSettingsDomainAdmin),
                         cleanup, "failed to remove trust settings");
#endif

    /* SHA1 cert fails SSL server */
    setTrust(&trust, sha1_certs, serverPolicy);
    require_noerr_string(SecTrustEvaluate(trust, &trustResult), cleanup, "failed to evaluate trust");
    is(trustResult, kSecTrustResultRecoverableTrustFailure, "reject test: system-trusted SHA-1 SSL server");

    /* Set anchor for root */
    require_quiet(sha1_root, cleanup);
    anchors = @[(__bridge id)sha1_root];
    require_noerr_string(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), cleanup, "failed to set anchors");

    /* SHA1 cert passes SSL server */
    require_noerr_string(SecTrustEvaluate(trust, &trustResult), cleanup, "failed to evaluate trust");
    is(trustResult, kSecTrustResultUnspecified, "accept test: app-trusted SHA-1 SSL server");

    /* SHA1 cert from system root passes SSL client */
    clientPolicy = SecPolicyCreateSSL(false, CFSTR("www.badssl.com"));
    setTrust(&trust, sha1_certs, clientPolicy);
    require_noerr_string(SecTrustEvaluate(trust, &trustResult), cleanup, "failed to evaluate trust");
    is(trustResult, kSecTrustResultUnspecified, "accept test: system-trusted SHA-1 SSL client");

    /* SHA256 cert from system root passes SSL server */
    sha2_certs = @[ (__bridge id)sha2_leaf, (__bridge id)sha2_int];
    setTrust(&trust, sha2_certs, serverPolicy);
    require_noerr_string(SecTrustEvaluate(trust, &trustResult), cleanup, "failed to evaluate trust");
    is(trustResult, kSecTrustResultUnspecified, "accept test: system-trusted SHA2 SSL server");

cleanup:
    CFReleaseNull(sha1_leaf);
    CFReleaseNull(sha1_int);
    CFReleaseNull(sha2_leaf);
    CFReleaseNull(sha2_int);
    CFReleaseNull(serverPolicy);
    CFReleaseNull(clientPolicy);
    CFReleaseNull(trust);
}

int si_29_sectrust_sha1_deprecation(int argc, char *const *argv)
{
    plan_tests(6);

    @autoreleasepool {
        setup_globals();
        tests();
        cleanup_globals();
    }

    return 0;
}
