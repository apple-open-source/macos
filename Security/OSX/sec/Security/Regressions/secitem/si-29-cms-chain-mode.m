/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>
#import <Foundation/Foundation.h>

#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentity.h>
#include <Security/SecCMS.h>
#include <Security/CMSEncoder.h>
#include <Security/CMSDecoder.h>
#include <Security/SecImportExport.h>
#include <Security/SecCmsBase.h>

#include <utilities/SecCFWrappers.h>

#if TARGET_OS_OSX
#include <Security/SecKeychain.h>
#define kSystemLoginKeychainPath "/Library/Keychains/System.keychain"
#endif

#include "shared_regressions.h"

#include "si-29-cms-chain-mode.h"

static NSData* CMSEncoder_encode_for_chain_mode(SecIdentityRef identity, CMSCertificateChainMode chainMode) {
    CMSEncoderRef encoder = NULL;
    CFDataRef message = NULL;

    /* Create encoder */
    require_noerr_action(CMSEncoderCreate(&encoder), exit, fail("Failed to create CMS encoder"));
    require_noerr_action(CMSEncoderSetSignerAlgorithm(encoder, kCMSEncoderDigestAlgorithmSHA256), exit,
                         fail("Failed to set digest algorithm to SHA256"));

    /* Set identity as signer */
    require_noerr_action(CMSEncoderAddSigners(encoder, identity), exit, fail("Failed to add signer identity"));

    /* Set chain mode */
    require_noerr_action(CMSEncoderSetCertificateChainMode(encoder, chainMode), exit, fail("Failed to set chain mode"));

    /* Load content */
    CMSEncoderUpdateContent(encoder, _chain_mode_content, sizeof(_chain_mode_content));

    /* output cms message */
    CMSEncoderCopyEncodedContent(encoder, &message);

exit:
    CFReleaseNull(encoder);
    return CFBridgingRelease(message);
}

static NSData* SecCMS_encode_for_chain_mode(SecIdentityRef identity, SecCmsCertChainMode chainMode) {
    NSMutableData *data = [NSMutableData data];
    NSData *content = [NSData dataWithBytes:_chain_mode_content length:sizeof(_chain_mode_content)];
    NSDictionary *parameters = @{
        (__bridge NSString*)kSecCMSSignHashAlgorithm : (__bridge NSString*)kSecCMSHashingAlgorithmSHA256,
        (__bridge NSString*)kSecCMSCertChainMode : [NSString stringWithFormat:@"%d", chainMode],
    };
    SecCMSCreateSignedData(identity, (__bridge CFDataRef)content, (__bridge CFDictionaryRef)parameters, nil, (__bridge CFMutableDataRef)data);

    if (data.length > 0) {
        return data;
    }
    return nil;
}

static NSArray* CMSDecoder_copy_certs(NSData *cms_message) {
    CMSDecoderRef decoder = NULL;
    CFArrayRef certs = NULL;

    if (!cms_message) {
        return nil;
    }

    require_noerr_action(CMSDecoderCreate(&decoder), exit, fail("Failed to create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, cms_message.bytes, cms_message.length), exit,
                         fail("Failed to update decoder with CMS message"));
    require_noerr_action(CMSDecoderFinalizeMessage(decoder), exit, fail("Failed to finalize decoder"));
    ok_status(CMSDecoderCopyAllCerts(decoder, &certs), "Failed to get certs from cms message");

exit:
    CFReleaseNull(decoder);
    return CFBridgingRelease(certs);
}

static void SecCMS_root_unavailable_tests(SecIdentityRef identity) {
    /* Chain Mode None */
    NSData *cms_message = SecCMS_encode_for_chain_mode(identity, SecCmsCMNone);
    NSArray *certs = CMSDecoder_copy_certs(cms_message);
    is(0, certs.count, "Expected 0 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Signer */
    cms_message = SecCMS_encode_for_chain_mode(identity, SecCmsCMCertOnly);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 1, "Expected 1 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain */
    cms_message = SecCMS_encode_for_chain_mode(identity, SecCmsCMCertChain);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 2, "Expected 2 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain With Root */
    cms_message = SecCMS_encode_for_chain_mode(identity, SecCmsCMCertChainWithRoot);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 2, "Expected 2 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain With Root or Fail */
    /* We shouldn't be able to find the root, so we shouldn't be able to make the CMS message */
    cms_message = SecCMS_encode_for_chain_mode(identity, SecCmsCMCertChainWithRootOrFail);
    ok(cms_message == nil, "Expected to fail to encode CMS message without root in keychain");
}

static void CMSEncoder_root_unavailable_tests(SecIdentityRef identity) {
    /* Chain Mode None */
    NSData *cms_message = CMSEncoder_encode_for_chain_mode(identity, kCMSCertificateNone);
    NSArray *certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 0, "Expected 0 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Signer */
    cms_message = CMSEncoder_encode_for_chain_mode(identity, kCMSCertificateSignerOnly);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 1, "Expected 1 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain */
    cms_message = CMSEncoder_encode_for_chain_mode(identity, kCMSCertificateChain);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 2, "Expected 2 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain With Root */
    cms_message = CMSEncoder_encode_for_chain_mode(identity, kCMSCertificateChainWithRoot);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 2, "Expected 2 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain With Root or Fail */
    /* We shouldn't be able to find the root, so we shouldn't be able to make the CMS message */
    cms_message = CMSEncoder_encode_for_chain_mode(identity, kCMSCertificateChainWithRootOrFail);
    ok(cms_message == nil, "Expected to fail to encode CMS message without root in keychain");
}

static void SecCMS_root_available_tests(SecIdentityRef identity) {
    /* Chain Mode None */
    NSData *cms_message = SecCMS_encode_for_chain_mode(identity, SecCmsCMNone);
    NSArray *certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 0, "Expected 0 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Signer */
    cms_message = SecCMS_encode_for_chain_mode(identity, SecCmsCMCertOnly);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 1, "Expected 1 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain */
    cms_message = SecCMS_encode_for_chain_mode(identity, SecCmsCMCertChain);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 2, "Expected 2 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain With Root */
    cms_message = SecCMS_encode_for_chain_mode(identity, SecCmsCMCertChainWithRoot);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 3, "Expected 3 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain With Root or Fail */
    cms_message = SecCMS_encode_for_chain_mode(identity, SecCmsCMCertChainWithRootOrFail);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 3, "Expected 3 certs, got %lu", (unsigned long)certs.count);
}

static void CMSEncoder_root_available_tests(SecIdentityRef identity) {
    /* Chain Mode None */
    NSData *cms_message = CMSEncoder_encode_for_chain_mode(identity, kCMSCertificateNone);
    NSArray *certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 0, "Expected 0 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Signer */
    cms_message = CMSEncoder_encode_for_chain_mode(identity, kCMSCertificateSignerOnly);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 1, "Expected 1 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain */
    cms_message = CMSEncoder_encode_for_chain_mode(identity, kCMSCertificateChain);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 2, "Expected 2 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain With Root */
    cms_message = CMSEncoder_encode_for_chain_mode(identity, kCMSCertificateChainWithRoot);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 3, "Expected 3 certs, got %lu", (unsigned long)certs.count);

    /* Chain Mode: Chain With Root or Fail */
    cms_message = CMSEncoder_encode_for_chain_mode(identity, kCMSCertificateChainWithRootOrFail);
    certs = CMSDecoder_copy_certs(cms_message);
    is(certs.count, 3, "Expected 3 certs, got %lu", (unsigned long)certs.count);
}

static bool setup_keychain(const uint8_t *p12, size_t p12_len, SecIdentityRef *identity) {
    CFArrayRef tmp_imported_items = NULL;
    NSArray *imported_items = nil;

    NSDictionary *options = @{ (__bridge NSString *)kSecImportExportPassphrase : @"password" };
    NSData *p12Data = [NSData dataWithBytes:p12 length:p12_len];
    require_noerr_action(SecPKCS12Import((__bridge CFDataRef)p12Data, (__bridge CFDictionaryRef)options,
                                         &tmp_imported_items), exit,
                         fail("Failed to import identity"));
    imported_items = CFBridgingRelease(tmp_imported_items);
    require_noerr_action([imported_items count] == 0 &&
                         [imported_items[0] isKindOfClass:[NSDictionary class]], exit,
                         fail("Wrong imported items output"));
    *identity = (SecIdentityRef)CFBridgingRetain(imported_items[0][(__bridge NSString*)kSecImportItemIdentity]);
    require_action(*identity, exit, fail("Failed to get identity"));

    return true;

exit:
    return false;
}

static void cleanup_keychain(SecIdentityRef identity, NSArray *certs) {
#if TARGET_OS_OSX
    // SecPKCS12Import adds the items to the keychain on macOS
    NSDictionary *query = @{ (__bridge NSString*)kSecValueRef : (__bridge id)identity };
    ok_status(SecItemDelete((__bridge CFDictionaryRef)query), "failed to remove identity from keychain");
#else
    pass("skip test on iOS");
#endif
    for(id cert in certs) {
        NSDictionary *cert_query = @{ (__bridge NSString*)kSecValueRef : cert };
        ok_status(SecItemDelete((__bridge CFDictionaryRef)cert_query), "failed to remove cert from keychain");
    }
}

static void add_cert_to_keychain(SecCertificateRef cert) {
#if TARGET_OS_OSX
    SecKeychainRef kcRef = NULL;
    SecKeychainOpen(kSystemLoginKeychainPath, &kcRef);
    if (!kcRef) {
        fail("failed to open system keychain");
        return;
    }
    NSDictionary *query = @{
        (__bridge NSString*)kSecValueRef : (__bridge id)cert,
        (__bridge NSString*)kSecUseKeychain: (__bridge id)kcRef,
    };
#else
    NSDictionary *query = @{ (__bridge NSString*)kSecValueRef : (__bridge id)cert};
#endif
    ok_status(SecItemAdd((__bridge CFDictionaryRef)query, NULL), "failed to add cert to keychain. following tests may fail.");
}

int si_29_cms_chain_mode(int argc, char *const *argv) {
    plan_tests(43);

    SecIdentityRef identity = NULL;
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _chain_mode_root, sizeof(_chain_mode_root));
    SecCertificateRef intermediate = SecCertificateCreateWithBytes(NULL, _chain_mode_subca, sizeof(_chain_mode_subca));

    if (setup_keychain(_chain_mode_leaf_p12 , sizeof(_chain_mode_leaf_p12), &identity)) {
        add_cert_to_keychain(intermediate);

        SecCMS_root_unavailable_tests(identity);
        CMSEncoder_root_unavailable_tests(identity);

        add_cert_to_keychain(root);

        SecCMS_root_available_tests(identity);
        CMSEncoder_root_available_tests(identity);

        cleanup_keychain(identity, @[(__bridge id) intermediate, (__bridge id)root]);
    }

    CFReleaseNull(identity);
    CFReleaseNull(root);
    CFReleaseNull(intermediate);

    return 0;
}
