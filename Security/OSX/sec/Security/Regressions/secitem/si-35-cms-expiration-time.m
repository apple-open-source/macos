/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#include <Security/SecIdentity.h>
#include <Security/SecCMS.h>
#include <Security/CMSEncoder.h>
#include <Security/CMSDecoder.h>

#include <utilities/SecCFWrappers.h>

#if TARGET_OS_OSX
#include <Security/SecKeychain.h>
#include <Security/SecImportExport.h>
#include <Security/CMSPrivate.h>
#endif

#include "shared_regressions.h"

#include "si-35-cms-expiration-time.h"

/* MARK: SecCMS tests */
static void SecCMS_positive_tests(void) {
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDictionaryRef tmpAttrs = NULL;
    NSDictionary* attrs = nil;
    NSData *expirationDateOid = nil, *unparsedExpirationDate = nil;
    NSArray *attrValues = nil;
    NSDate *expirationDate = nil, *expectedDate = [NSDate dateWithTimeIntervalSinceReferenceDate: 599400000.0];

    NSData *message = [NSData dataWithBytes:_css_gen_expiration_time length:sizeof(_css_gen_expiration_time)];
    NSData *content = [NSData dataWithBytes:_css_content length:sizeof(_css_content)];
    policy = SecPolicyCreateBasicX509();

    /* verify a valid message and copy out attributes */
    ok_status(SecCMSVerifyCopyDataAndAttributes((__bridge CFDataRef)message, (__bridge CFDataRef)content, policy, &trust, NULL, &tmpAttrs),
              "Failed to verify valid CMS message and get out attributes");
    require_action(attrs = CFBridgingRelease(tmpAttrs), exit, fail("Failed to copy attributes"));

    /* verify we can get the parsed expiration date attribute out */
    uint8_t appleExpirationDateOid[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x9, 0x3 };
    expirationDateOid = [NSData dataWithBytes:appleExpirationDateOid length:sizeof(appleExpirationDateOid)];
    attrValues = attrs[expirationDateOid];
    is([attrValues count], (size_t)1, "Wrong number of attribute values");
    require_action(unparsedExpirationDate = attrValues[0], exit, fail("Failed to get expiration date attribute value"));
    uint8_t expectedUTCData[] = { 0x31, 0x39, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x30, 0x30, 0x30, 0x30, 0x5a };
    is([unparsedExpirationDate isEqualToData:[NSData dataWithBytes:expectedUTCData length:sizeof(expectedUTCData)]], true, "Failed to get correct expiration date");

    /* verify we can get the "cooked" expiration data out */
    ok(expirationDate = attrs[(__bridge NSString*)kSecCMSExpirationDate], "Failed to get pre-parsed expiration date from attributes");
    is([expirationDate isEqualToDate:expectedDate], true, "Failed to get correct expiration date");

exit:
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

static void SecCMS_negative_date_changed(void) {
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;

    NSMutableData *invalid_message = [NSMutableData dataWithBytes:_css_gen_expiration_time length:sizeof(_css_gen_expiration_time)];
    [invalid_message resetBytesInRange:NSMakeRange(3980, 1)]; // reset byte in expiration date attribute of _css_gen_expiration_time
    NSData *content = [NSData dataWithBytes:_css_content length:sizeof(_css_content)];
    policy = SecPolicyCreateBasicX509();

    /* Verify message with expiration date changed fails*/
    is(SecCMSVerifyCopyDataAndAttributes((__bridge CFDataRef)invalid_message, (__bridge CFDataRef)content, policy, &trust, NULL, NULL),
       errSecAuthFailed, "Failed to verify valid CMS message and get out attributes");

    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

static void SecCMS_negative_missing_date(void) {
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDictionaryRef tmpAttrs = NULL;
    NSDictionary *attrs = nil;
    NSData *expirationDateOid = nil;

    NSData *message = [NSData dataWithBytes:_no_expiration_attr length:sizeof(_no_expiration_attr)];
    policy = SecPolicyCreateBasicX509();

    /* verify a message with no expiration date */
    ok_status(SecCMSVerifyCopyDataAndAttributes((__bridge CFDataRef)message, NULL, policy, &trust, NULL, &tmpAttrs),
              "Failed to verify valid CMS message and get out attributes");
    require_action(attrs = CFBridgingRelease(tmpAttrs), exit, fail("Failed to copy attributes"));

    /* verify we can't get the expiration date out */
    uint8_t appleExpirationDateOid[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x9, 0x3 };
    expirationDateOid = [NSData dataWithBytes:appleExpirationDateOid length:sizeof(appleExpirationDateOid)];
    is(attrs[expirationDateOid], NULL, "Got an expiration date attribute from message with no expiration date");
    is(attrs[(__bridge NSString*)kSecCMSExpirationDate], NULL, "Got an expiration date attribute from message with no expiration date");

exit:
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

static void SecCMS_tests(void) {
    SecCMS_positive_tests();
    SecCMS_negative_date_changed();
    SecCMS_negative_missing_date();
}

/* MARK: CMSEncoder tests */
static void CMSEncoder_tests(SecIdentityRef identity) {
    CMSEncoderRef encoder = NULL;
    CMSDecoderRef decoder = NULL;
    CFDataRef message = NULL;

    /* Create encoder */
    require_noerr_action(CMSEncoderCreate(&encoder), exit, fail("Failed to create CMS encoder"));
    require_noerr_action(CMSEncoderSetSignerAlgorithm(encoder, kCMSEncoderDigestAlgorithmSHA256), exit,
                         fail("Failed to set digest algorithm to SHA256"));

    /* Set identity as signer */
    require_noerr_action(CMSEncoderAddSigners(encoder, identity), exit, fail("Failed to add signer identity"));

    /* Add signing time attribute for 6 June 2018 */
    require_noerr_action(CMSEncoderAddSignedAttributes(encoder, kCMSAttrSigningTime), exit,
                         fail("Failed to set signing time flag"));
    require_noerr_action(CMSEncoderSetSigningTime(encoder, 550000000.0), exit, fail("Failed to set signing time"));

    /* Add expiration date attribute for 30 September 2018 */
    ok_status(CMSEncoderAddSignedAttributes(encoder, kCMSAttrAppleExpirationTime),
              "Set expiration date flag");
    ok_status(CMSEncoderSetAppleExpirationTime(encoder, 560000000.0), "Set Expiration time");

    /* Load content */
    require_noerr_action(CMSEncoderSetHasDetachedContent(encoder, true), exit, fail("Failed to set detached content"));
    require_noerr_action(CMSEncoderUpdateContent(encoder, _css_content, sizeof(_css_content)), exit, fail("Failed to set content"));

    /* output cms message */
    ok_status(CMSEncoderCopyEncodedContent(encoder, &message), "Finish encoding and output message");
    isnt(message, NULL, "Encoded message exists");

    /* decode message */
    require_noerr_action(CMSDecoderCreate(&decoder), exit, fail("Create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, CFDataGetBytePtr(message),
                                                 CFDataGetLength(message)), exit,
                         fail("Update decoder with CMS message"));
    require_noerr_action(CMSDecoderSetDetachedContent(decoder, (__bridge CFDataRef)[NSData dataWithBytes:_css_content
                                                                                                  length:sizeof(_css_content)]),
                         exit, fail("Set detached content"));
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

exit:
    CFReleaseNull(encoder);
    CFReleaseNull(message);
    CFReleaseNull(decoder);
}

/* MARK: CMSDecoder tests */
static void CMSDecoder_positive_tests(void) {
    CMSDecoderRef decoder = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CMSSignerStatus signerStatus;
    NSData *_css_contentData = nil;
    CFAbsoluteTime expirationTime = 0;
    NSDate *expirationDate = nil, *expectedDate = [NSDate dateWithTimeIntervalSinceReferenceDate: 599400000.0];

    /* Create decoder and decode */
    require_noerr_action(CMSDecoderCreate(&decoder), exit, fail("Failed to create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, _css_gen_expiration_time, sizeof(_css_gen_expiration_time)), exit,
                         fail("Failed to update decoder with CMS message"));
    _css_contentData = [NSData dataWithBytes:_css_content length:sizeof(_css_content)];
    require_noerr_action(CMSDecoderSetDetachedContent(decoder, (__bridge CFDataRef)_css_contentData), exit,
                         fail("Failed to set detached _css_content"));
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

    /* Get signer status */
    require_action(policy = SecPolicyCreateBasicX509(), exit, fail("Failed to Create policy"));
    ok_status(CMSDecoderCopySignerStatus(decoder, 0, policy, false, &signerStatus, &trust, NULL),
              "Copy Signer status");
    is(signerStatus, kCMSSignerValid, "Valid signature");

    /* Get Expiration Time Attribute value */
    ok_status(CMSDecoderCopySignerAppleExpirationTime(decoder, 0, &expirationTime),
       "Got expiration time from message with no expiration attribute");
    expirationDate = [NSDate dateWithTimeIntervalSinceReferenceDate:expirationTime];
    is([expirationDate isEqualToDate:expectedDate], true, "Got wrong expiration time"); // 31 December 2019 12:00:00 Zulu

exit:
    CFReleaseNull(decoder);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

static void CMSDecoder_negative_date_changed(void) {
    CMSDecoderRef decoder = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CMSSignerStatus signerStatus;
    NSData *_css_contentData = nil;
    NSMutableData *invalid_message = nil;

    /* Create decoder and decode */
    invalid_message = [NSMutableData dataWithBytes:_css_gen_expiration_time length:sizeof(_css_gen_expiration_time)];
    [invalid_message resetBytesInRange:NSMakeRange(3980, 1)]; // reset byte in expiration date attribute of _css_gen_expiration_time
    require_noerr_action(CMSDecoderCreate(&decoder), exit, fail("Failed to create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, [invalid_message bytes], [invalid_message length]), exit,
                         fail("Failed to update decoder with CMS message"));
    _css_contentData = [NSData dataWithBytes:_css_content length:sizeof(_css_content)];
    require_noerr_action(CMSDecoderSetDetachedContent(decoder, (__bridge CFDataRef)_css_contentData), exit,
                         fail("Failed to set detached _css_content"));
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

    /* Get signer status */
    require_action(policy = SecPolicyCreateBasicX509(), exit, fail("Failed to Create policy"));
    ok_status(CMSDecoderCopySignerStatus(decoder, 0, policy, false, &signerStatus, &trust, NULL),
              "Copy Signer status");
    is(signerStatus, kCMSSignerInvalidSignature, "Valid signature");

exit:
    CFReleaseNull(decoder);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

static void CMSDecoder_negative_missing_date(void) {
    CMSDecoderRef decoder = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CMSSignerStatus signerStatus;
    CFAbsoluteTime expirationTime = 0.0;

    /* Create decoder and decode */
    require_noerr_action(CMSDecoderCreate(&decoder), exit, fail("Failed to create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, _no_expiration_attr, sizeof(_no_expiration_attr)), exit,
                         fail("Failed to update decoder with CMS message"));
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

    /* Get signer status */
    require_action(policy = SecPolicyCreateBasicX509(), exit, fail("Failed to Create policy"));
    ok_status(CMSDecoderCopySignerStatus(decoder, 0, policy, false, &signerStatus, &trust, NULL),
              "Copy Signer status");
    is(signerStatus, kCMSSignerValid, "Valid signature");

    /* Get Expiration Time Attribute value */
    is(CMSDecoderCopySignerAppleExpirationTime(decoder, 0, &expirationTime), -1,
       "Got expiration time from message with no expiration attribute");

exit:
    CFReleaseNull(decoder);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

static void CMSDecoder_tests(void) {
    CMSDecoder_positive_tests();
    CMSDecoder_negative_date_changed();
    CMSDecoder_negative_missing_date();
}

static bool setup_keychain(const uint8_t *p12, size_t p12_len, SecIdentityRef *identity) {
    CFArrayRef tmp_imported_items = NULL;
    NSArray *imported_items = nil;

    NSDictionary *options = @{ (__bridge NSString *)kSecImportExportPassphrase : @"password" };
    NSData *p12Data = [NSData dataWithBytes:signing_identity_p12 length:sizeof(signing_identity_p12)];
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

static void cleanup_keychain(SecIdentityRef identity) {
#if TARGET_OS_OSX
    // SecPKCS12Import adds the items to the keychain on macOS
    NSDictionary *query = @{ (__bridge NSString*)kSecValueRef : (__bridge id)identity };
    ok_status(SecItemDelete((__bridge CFDictionaryRef)query), "failed to remove identity from keychain");
#else
    pass("skip test on iOS");
#endif
    CFReleaseNull(identity);
}

int si_35_cms_expiration_time(int argc, char *const *argv) {
    plan_tests(5+1+3+5+5+3+4+1);

    SecIdentityRef identity = NULL;

    if (setup_keychain(signing_identity_p12 , sizeof(signing_identity_p12), &identity)) {
        SecCMS_tests();
        CMSEncoder_tests(identity);
        CMSDecoder_tests();
    }

    cleanup_keychain(identity);

    return 0;
}
