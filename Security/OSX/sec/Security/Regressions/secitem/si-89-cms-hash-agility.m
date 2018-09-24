/*
 * Copyright (c) 2015-2018 Apple Inc. All Rights Reserved.
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
#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecCMS.h>
#include <Security/SecCmsBase.h>
#include <Security/CMSEncoder.h>
#include <Security/CMSDecoder.h>
#include <utilities/SecCFRelease.h>

#if TARGET_OS_OSX
#include <Security/CMSPrivate.h>
#endif

#include "shared_regressions.h"

#include "si-cms-hash-agility-data.h"

static void ios_shim_tests(void)
{
    CFDataRef message = NULL, contentData = NULL, hashAgilityOid = NULL, hashAgilityValue = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDictionaryRef attrs = NULL;
    CFArrayRef attrValues = NULL;
    CFDateRef signingTime = NULL, expectedTime = NULL;

    ok(message = CFDataCreate(NULL, valid_message, valid_message_size), "Create valid message");
    ok(contentData = CFDataCreate(NULL, content, content_size), "Create detached content");
    ok(policy = SecPolicyCreateBasicX509(), "Create policy");

    /* verify the valid message and copy out attributes */
    is(SecCMSVerifyCopyDataAndAttributes(message, contentData, policy, &trust,  NULL, &attrs),
       errSecSuccess, "Verify valid CMS message and get attributes");
    isnt(attrs, NULL, "Copy CMS attributes");
    CFReleaseNull(trust);

    /* verify we can get the parsed attribute */
    uint8_t appleHashAgilityOid[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x9, 0x1 };
    ok(hashAgilityOid = CFDataCreate(NULL, appleHashAgilityOid, sizeof(appleHashAgilityOid)),
       "Create oid data");
    ok(attrValues = (CFArrayRef) CFDictionaryGetValue(attrs, hashAgilityOid),
       "Get hash agility value array");
    is(CFArrayGetCount(attrValues), 1, "One attribute value");
    ok(hashAgilityValue = CFArrayGetValueAtIndex(attrValues, 0), "Get hash agility value");
    is((size_t)CFDataGetLength(hashAgilityValue), sizeof(attribute), "Verify size of parsed hash agility value");
    is(memcmp(attribute, CFDataGetBytePtr(hashAgilityValue), sizeof(attribute)), 0,
       "Verify correct hash agility value");

    /* verify we can get the "cooked" parsed attribute */
    ok(hashAgilityValue = (CFDataRef)CFDictionaryGetValue(attrs, kSecCMSHashAgility), "Get cooked hash agility value");
    is((size_t)CFDataGetLength(hashAgilityValue), sizeof(attribute), "Verify size of parsed hash agility value");
    is(memcmp(attribute, CFDataGetBytePtr(hashAgilityValue), sizeof(attribute)), 0,
       "Verify correct hash agility value");

    attrValues = NULL;

    /*verify we can get the signing time attribute */
    ok(signingTime = (CFDateRef) CFDictionaryGetValue(attrs, kSecCMSSignDate), "Get signing time");
    ok(expectedTime = CFDateCreate(NULL, 468295000.0), "Set expected signing time");
    is(CFDateCompare(signingTime, expectedTime, NULL), 0, "Verify signing time");

    CFReleaseNull(message);

    /* verify the invalid message */
    ok(message = CFDataCreate(NULL, invalid_message, invalid_message_size), "Create invalid message");
    is(SecCMSVerify(message, contentData, policy, &trust, NULL), errSecAuthFailed,
       "Verify invalid CMS message");

    CFReleaseNull(message);
    CFReleaseNull(trust);
    CFReleaseNull(attrs);

    /* verify the valid message with no hash agility attribute */
    ok(message = CFDataCreate(NULL, valid_no_attr, valid_no_attr_size),
       "Create valid message with no hash agility value");
    is(SecCMSVerifyCopyDataAndAttributes(message, contentData, policy, &trust,  NULL, &attrs),
       errSecSuccess, "Verify 2nd valid CMS message and get attributes");
    isnt(attrs, NULL, "Copy 2nd CMS attributes");

    /* verify we can't get the hash agility attribute */
    is((CFArrayRef) CFDictionaryGetValue(attrs, hashAgilityOid), NULL,
       "Get hash agility value array");
    is((CFDataRef) CFDictionaryGetValue(attrs, kSecCMSHashAgility), NULL,
        "Get cooked hash agility value");


    CFReleaseNull(message);
    CFReleaseNull(contentData);
    CFReleaseNull(hashAgilityOid);
    CFReleaseNull(expectedTime);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(attrs);
}

/* MARK: macOS Shim tests */
/* encode test */
static void encode_test(SecIdentityRef identity)
{
    CMSEncoderRef encoder = NULL;
    CFDataRef attributeData = NULL, message = NULL;

    /* Create encoder */
    ok_status(CMSEncoderCreate(&encoder), "Create CMS encoder");
    ok_status(CMSEncoderSetSignerAlgorithm(encoder, kCMSEncoderDigestAlgorithmSHA256),
              "Set digest algorithm to SHA256");

    /* Set identity as signer */
    ok_status(CMSEncoderAddSigners(encoder, identity), "Set Signer identity");

    /* Add signing time attribute for 3 November 2015 */
    ok_status(CMSEncoderAddSignedAttributes(encoder, kCMSAttrSigningTime),
              "Set signing time flag");
    ok_status(CMSEncoderSetSigningTime(encoder, 468295000.0), "Set Signing time");

    /* Add hash agility attribute */
    ok_status(CMSEncoderAddSignedAttributes(encoder, kCMSAttrAppleCodesigningHashAgility),
              "Set hash agility flag");
    ok(attributeData = CFDataCreate(NULL, attribute, sizeof(attribute)),
       "Create atttribute object");
    ok_status(CMSEncoderSetAppleCodesigningHashAgility(encoder, attributeData),
              "Set hash agility data");

    /* Load content */
    ok_status(CMSEncoderSetHasDetachedContent(encoder, true), "Set detached content");
    ok_status(CMSEncoderUpdateContent(encoder, content, content_size), "Set content");

    /* output cms message */
    ok_status(CMSEncoderCopyEncodedContent(encoder, &message), "Finish encoding and output message");

    /* decode message */
    CMSDecoderRef decoder = NULL;
    CFDataRef contentData = NULL;
    isnt(message, NULL, "Encoded message exists");
    ok_status(CMSDecoderCreate(&decoder), "Create CMS decoder");
    ok_status(CMSDecoderUpdateMessage(decoder, CFDataGetBytePtr(message), CFDataGetLength(message)),
              "Update decoder with CMS message");
    ok(contentData = CFDataCreate(NULL, content, content_size), "Create detached content");
    ok_status(CMSDecoderSetDetachedContent(decoder, contentData), "Set detached content");
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

    CFReleaseNull(encoder);
    CFReleaseNull(attributeData);
    CFReleaseNull(message);
    CFReleaseNull(decoder);
    CFReleaseNull(contentData);
}

static void decode_positive_test(void)
{
    CMSDecoderRef decoder = NULL;
    CFDataRef contentData = NULL, attrValue = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CMSSignerStatus signerStatus;
    CFAbsoluteTime signingTime = 0.0;

    /* Create decoder and decode */
    ok_status(CMSDecoderCreate(&decoder), "Create CMS decoder");
    ok_status(CMSDecoderUpdateMessage(decoder, valid_message, valid_message_size),
              "Update decoder with CMS message");
    ok(contentData = CFDataCreate(NULL, content, content_size), "Create detached content");
    ok_status(CMSDecoderSetDetachedContent(decoder, contentData), "Set detached content");
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

    /* Get signer status */
    ok(policy = SecPolicyCreateBasicX509(), "Create policy");
    ok_status(CMSDecoderCopySignerStatus(decoder, 0, policy, false, &signerStatus, &trust, NULL),
              "Copy Signer status");
    is(signerStatus, kCMSSignerValid, "Valid signature");

    /* Get Hash Agility Attribute value */
    ok_status(CMSDecoderCopySignerAppleCodesigningHashAgility(decoder, 0, &attrValue),
              "Copy hash agility attribute value");
    is((size_t)CFDataGetLength(attrValue), sizeof(attribute), "Decoded attribute size");
    is(memcmp(attribute, CFDataGetBytePtr(attrValue), sizeof(attribute)), 0,
       "Decoded value same as input value");

    /* Get Signing Time Attribute value */
    ok_status(CMSDecoderCopySignerSigningTime(decoder, 0, &signingTime),
              "Copy signing time attribute value");
    is(signingTime, 468295000.0, "Decoded date same as input date");

    CFReleaseNull(decoder);
    CFReleaseNull(contentData);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(attrValue);
}

static void decode_negative_test(void)
{
    CMSDecoderRef decoder = NULL;
    CFDataRef contentData = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CMSSignerStatus signerStatus;

    /* Create decoder and decode */
    ok_status(CMSDecoderCreate(&decoder), "Create CMS decoder");
    ok_status(CMSDecoderUpdateMessage(decoder, invalid_message, invalid_message_size),
              "Update decoder with CMS message");
    ok(contentData = CFDataCreate(NULL, content, content_size), "Create detached content");
    ok_status(CMSDecoderSetDetachedContent(decoder, contentData), "Set detached content");
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

    /* Get signer status */
    ok(policy = SecPolicyCreateBasicX509(), "Create policy");
    ok_status(CMSDecoderCopySignerStatus(decoder, 0, policy, false, &signerStatus, &trust, NULL),
              "Copy Signer status");
    is(signerStatus, kCMSSignerInvalidSignature, "Invalid signature");

    CFReleaseNull(decoder);
    CFReleaseNull(contentData);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

static void decode_no_attr_test(void)
{
    CMSDecoderRef decoder = NULL;
    CFDataRef contentData = NULL, attrValue = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CMSSignerStatus signerStatus;

    /* Create decoder and decode */
    ok_status(CMSDecoderCreate(&decoder), "Create CMS decoder");
    ok_status(CMSDecoderUpdateMessage(decoder, valid_no_attr, valid_no_attr_size),
              "Update decoder with CMS message");
    ok(contentData = CFDataCreate(NULL, content, content_size), "Create detached content");
    ok_status(CMSDecoderSetDetachedContent(decoder, contentData), "Set detached content");
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

    /* Get signer status */
    ok(policy = SecPolicyCreateBasicX509(), "Create policy");
    ok_status(CMSDecoderCopySignerStatus(decoder, 0, policy, false, &signerStatus, &trust, NULL),
              "Copy Signer status");
    is(signerStatus, kCMSSignerValid, "Valid signature");

    /* Get Hash Agility Attribute value */
    ok_status(CMSDecoderCopySignerAppleCodesigningHashAgility(decoder, 0, &attrValue),
              "Copy empty hash agility attribute value");
    is(attrValue, NULL, "NULL attribute value");

    CFReleaseNull(decoder);
    CFReleaseNull(contentData);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(attrValue);
}

static void macos_shim_tests(SecIdentityRef identity) {
    encode_test(identity);
    decode_positive_test();
    decode_negative_test();
    decode_no_attr_test();
}

/* MARK: V2 Attribute testing */
static void ios_shim_V2_tests(void) {
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDictionaryRef tmpAttrs = NULL;
    NSMutableData *message = nil;
    NSData *contentData = nil, *hashAgilityV2Oid = nil;
    NSDictionary *attrs = nil, *hashAgilityValue = nil;
    NSArray *attrValues = nil;
    NSDate *signingTime = nil;

    message = [NSMutableData dataWithBytes:_V2_valid_message length:_V2_valid_message_size];
    contentData = [NSData dataWithBytes:content length:content_size];
    policy = SecPolicyCreateBasicX509();

    /* verify the valid message and copy out attributes */
    is(SecCMSVerifyCopyDataAndAttributes((__bridge CFDataRef)message, (__bridge CFDataRef)contentData, policy, &trust,  NULL, &tmpAttrs),
       errSecSuccess, "Verify valid CMS message and get attributes");
    attrs = CFBridgingRelease(tmpAttrs);
    require_action(attrs, exit, fail("Copy CMS attributes"));

    /* verify we can get the parsed attribute */
    uint8_t appleHashAgilityOid[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x9, 0x2 };
    hashAgilityV2Oid = [NSData dataWithBytes:appleHashAgilityOid length:sizeof(appleHashAgilityOid)];
    attrValues = attrs[hashAgilityV2Oid];
    is([attrValues count], (size_t)2, "Two attribute values");

    /* verify we can get the "cooked" parsed attribute */
    require_action(hashAgilityValue = (NSDictionary *)attrs[(__bridge NSString*)kSecCMSHashAgilityV2], exit,
                   fail("Get cooked hash agility value"));
    ok([hashAgilityValue[@(SEC_OID_SHA1)] isEqualToData:[NSData dataWithBytes:_attributev2 length:20]],
       "Got wrong SHA1 agility value");
    ok([hashAgilityValue[@(SEC_OID_SHA256)] isEqualToData:[NSData dataWithBytes:(_attributev2+32) length:32]],
       "Got wrong SHA256 agility value");

    attrValues = NULL;

    /*verify we can get the signing time attribute */
    require_action(signingTime = attrs[(__bridge NSString*)kSecCMSSignDate], exit, fail("Failed to get signing time"));
    ok([signingTime isEqualToDate:[NSDate dateWithTimeIntervalSinceReferenceDate:530700000.0]], "Got wrong signing time");

    /* verify the invalid message */
    message = [NSMutableData dataWithBytes:_V2_valid_message length:_V2_valid_message_size];
    [message resetBytesInRange:NSMakeRange(2110, 1)]; /* reset byte in hash agility attribute */
    is(SecCMSVerify((__bridge CFDataRef)message, (__bridge CFDataRef)contentData, policy, &trust, NULL), errSecAuthFailed,
       "Verify invalid CMS message");

    /* verify the valid message with no hash agility attribute */
    message = [NSMutableData dataWithBytes:valid_no_attr length:valid_no_attr_size];
    is(SecCMSVerifyCopyDataAndAttributes((__bridge CFDataRef)message, (__bridge CFDataRef)contentData, policy, &trust,  NULL, &tmpAttrs),
       errSecSuccess, "Verify 2nd valid CMS message and get attributes");
    attrs = CFBridgingRelease(tmpAttrs);
    isnt(attrs, NULL, "Copy 2nd CMS attributes");

    /* verify we can't get the hash agility attribute */
    is(attrs[hashAgilityV2Oid], NULL, "Got hash agility V2 attribute");
    is(attrs[(__bridge NSString*)kSecCMSHashAgilityV2], NULL, "Got cooked hash agility V2 attribute");

exit:
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

/* macOS shim test - encode */
static void encode_V2_test(SecIdentityRef identity) {
    CMSEncoderRef encoder = NULL;
    CMSDecoderRef decoder = NULL;
    CFDataRef message = NULL;
    NSDictionary *attrValues = nil;

    /* Create encoder */
    require_noerr_action(CMSEncoderCreate(&encoder), exit, fail("Failed to create CMS encoder"));
    require_noerr_action(CMSEncoderSetSignerAlgorithm(encoder, kCMSEncoderDigestAlgorithmSHA256), exit,
              fail("Failed to set digest algorithm to SHA256"));

    /* Set identity as signer */
    require_noerr_action(CMSEncoderAddSigners(encoder, identity), exit, fail("Failed to add signer identity"));

    /* Add signing time attribute for 26 October 2017 */
    require_noerr_action(CMSEncoderAddSignedAttributes(encoder, kCMSAttrSigningTime), exit,
                         fail("Failed to set signing time flag"));
    require_noerr_action(CMSEncoderSetSigningTime(encoder, 530700000.0), exit, fail("Failed to set signing time"));

    /* Add hash agility attribute */
    attrValues = @{ @(SEC_OID_SHA1) : [NSData dataWithBytes:_attributev2 length:20],
                   @(SEC_OID_SHA256) :  [NSData dataWithBytes:(_attributev2 + 32) length:32],
    };
    ok_status(CMSEncoderAddSignedAttributes(encoder, kCMSAttrAppleCodesigningHashAgilityV2),
              "Set hash agility flag");
    ok_status(CMSEncoderSetAppleCodesigningHashAgilityV2(encoder, (__bridge CFDictionaryRef)attrValues),
              "Set hash agility data");

    /* Load content */
    require_noerr_action(CMSEncoderSetHasDetachedContent(encoder, true), exit, fail("Failed to set detached content"));
    require_noerr_action(CMSEncoderUpdateContent(encoder, content, content_size), exit, fail("Failed to set content"));

    /* output cms message */
    ok_status(CMSEncoderCopyEncodedContent(encoder, &message), "Finish encoding and output message");
    isnt(message, NULL, "Encoded message exists");

    /* decode message */
    require_noerr_action(CMSDecoderCreate(&decoder), exit, fail("Create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, CFDataGetBytePtr(message),
                                                 CFDataGetLength(message)), exit,
                         fail("Update decoder with CMS message"));
    require_noerr_action(CMSDecoderSetDetachedContent(decoder, (__bridge CFDataRef)[NSData dataWithBytes:content
                                                                                                  length:content_size]),
                         exit, fail("Set detached content"));
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

exit:
    CFReleaseNull(encoder);
    CFReleaseNull(message);
    CFReleaseNull(decoder);
}

/* macOS shim test - decode positive */
static void decode_V2_positive_test(void) {
    CMSDecoderRef decoder = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CMSSignerStatus signerStatus;
    NSData *contentData = nil;
    CFDictionaryRef tmpAttrValue = NULL;
    NSDictionary *attrValue = nil;

    /* Create decoder and decode */
    require_noerr_action(CMSDecoderCreate(&decoder), exit, fail("Failed to create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, _V2_valid_message, _V2_valid_message_size), exit,
              fail("Failed to update decoder with CMS message"));
    contentData = [NSData dataWithBytes:content length:content_size];
    require_noerr_action(CMSDecoderSetDetachedContent(decoder, (__bridge CFDataRef)contentData), exit,
                         fail("Failed to set detached content"));
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

    /* Get signer status */
    require_action(policy = SecPolicyCreateBasicX509(), exit, fail("Failed to Create policy"));
    ok_status(CMSDecoderCopySignerStatus(decoder, 0, policy, false, &signerStatus, &trust, NULL),
              "Copy Signer status");
    is(signerStatus, kCMSSignerValid, "Valid signature");

    /* Get Hash Agility Attribute value */
    ok_status(CMSDecoderCopySignerAppleCodesigningHashAgilityV2(decoder, 0, &tmpAttrValue),
              "Copy hash agility attribute value");
    attrValue = CFBridgingRelease(tmpAttrValue);
    ok([attrValue[@(SEC_OID_SHA1)] isEqualToData:[NSData dataWithBytes:_attributev2 length:20]],
       "Got wrong SHA1 agility value");
    ok([attrValue[@(SEC_OID_SHA256)] isEqualToData:[NSData dataWithBytes:(_attributev2+32) length:32]],
       "Got wrong SHA256 agility value");

exit:
    CFReleaseNull(decoder);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

/* macOS shim test - decode negative */
static void decode_V2_negative_test(void) {
    CMSDecoderRef decoder = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CMSSignerStatus signerStatus;
    NSData *contentData = nil;
    NSMutableData *invalid_message = nil;

    /* Create decoder and decode */
    invalid_message = [NSMutableData dataWithBytes:_V2_valid_message length:_V2_valid_message_size];
    [invalid_message resetBytesInRange:NSMakeRange(2110, 1)]; /* reset byte in hash agility attribute */
    require_noerr_action(CMSDecoderCreate(&decoder), exit, fail("Failed to create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, [invalid_message bytes], [invalid_message length]), exit,
                         fail("Failed to update decoder with CMS message"));
    contentData = [NSData dataWithBytes:content length:content_size];
    require_noerr_action(CMSDecoderSetDetachedContent(decoder, (__bridge CFDataRef)contentData), exit,
                         fail("Failed to set detached content"));
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

/* macOS shim test - no attribute */
static void decodeV2_no_attr_test(void) {
    CMSDecoderRef decoder = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CMSSignerStatus signerStatus;
    NSData *contentData = nil;
    CFDictionaryRef attrValue = NULL;

    /* Create decoder and decode */
    require_noerr_action(CMSDecoderCreate(&decoder), exit, fail("Failed to create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, valid_message, valid_message_size), exit,
                         fail("Failed to update decoder with CMS message"));
    contentData = [NSData dataWithBytes:content length:content_size];
    require_noerr_action(CMSDecoderSetDetachedContent(decoder, (__bridge CFDataRef)contentData), exit,
                         fail("Failed to set detached content"));
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

    /* Get signer status */
    require_action(policy = SecPolicyCreateBasicX509(), exit, fail("Failed to Create policy"));
    ok_status(CMSDecoderCopySignerStatus(decoder, 0, policy, false, &signerStatus, &trust, NULL),
              "Copy Signer status");
    is(signerStatus, kCMSSignerValid, "Valid signature");

    /* Get Hash Agility Attribute value */
    ok_status(CMSDecoderCopySignerAppleCodesigningHashAgilityV2(decoder, 0, &attrValue),
              "Copy hash agility attribute value");
    is(attrValue, NULL, "NULL attribute value");

exit:
    CFReleaseNull(decoder);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(attrValue);
}

static void macOS_shim_V2_tests(SecIdentityRef identity) {
    encode_V2_test(identity);
    decode_V2_positive_test();
    decode_V2_negative_test();
    decodeV2_no_attr_test();
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

int si_89_cms_hash_agility(int argc, char *const *argv)
{
    plan_tests(102);

    SecIdentityRef identity = NULL;

    if (setup_keychain(signing_identity_p12 , sizeof(signing_identity_p12), &identity)) {
        ios_shim_tests();
        macos_shim_tests(identity);
        ios_shim_V2_tests();
        macOS_shim_V2_tests(identity);
    }

    cleanup_keychain(identity);

    return 0;
}
