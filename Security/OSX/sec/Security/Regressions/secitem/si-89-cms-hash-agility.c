/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecCMS.h>
#include <utilities/SecCFRelease.h>

#include "Security_regressions.h"

#include "si-89-cms-hash-agility.h"

static void ios_shim_tests(void)
{
    CFDataRef message = NULL, contentData = NULL, hashAgilityOid = NULL, hashAgilityValue = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDictionaryRef attrs = NULL;
    CFArrayRef attrValues = NULL;
    CFDateRef signingTime = NULL, expectedTime = NULL;

    ok(message = CFDataCreate(NULL, valid_message, sizeof(valid_message)), "Create valid message");
    ok(contentData = CFDataCreate(NULL, content, sizeof(content)), "Create detached content");
    ok(policy = SecPolicyCreateBasicX509(), "Create policy");

    /* verify the valid message and copy out attributes */
    is(SecCMSVerifyCopyDataAndAttributes(message, contentData, policy, &trust,  NULL, &attrs),
       errSecSuccess, "Verify valid CMS message and get attributes");
    isnt(attrs, NULL, "Copy CMS attributes");

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

    attrValues = NULL;

    /*verify we can get the signing time attribute */
    ok(signingTime = (CFDateRef) CFDictionaryGetValue(attrs, kSecCMSSignDate), "Get signing time");
    ok(expectedTime = CFDateCreate(NULL, 468295000.0), "Set expected signing time");
    is(CFDateCompare(signingTime, expectedTime, NULL), 0, "Verify signing time");

    CFReleaseNull(message);

    /* verify the invalid message */
    ok(message = CFDataCreate(NULL, invalid_message, sizeof(invalid_message)), "Create invalid message");
    is(SecCMSVerify(message, contentData, policy, &trust, NULL), errSecAuthFailed,
       "Verify invalid CMS message");

    CFReleaseNull(message);

    /* verify the valid message with no hash agility attribute */
    ok(message = CFDataCreate(NULL, valid_no_attr, sizeof(valid_no_attr)),
       "Create valid message with no hash agility value");
    is(SecCMSVerifyCopyDataAndAttributes(message, contentData, policy, &trust,  NULL, &attrs),
       errSecSuccess, "Verify 2nd valid CMS message and get attributes");
    isnt(attrs, NULL, "Copy 2nd CMS attributes");

    /* verify we can't get the hash agility attribute */
    is((CFArrayRef) CFDictionaryGetValue(attrs, hashAgilityOid), NULL,
       "Get hash agility value array");


    CFReleaseNull(message);
    CFReleaseNull(contentData);
    CFReleaseNull(hashAgilityOid);
    CFReleaseNull(expectedTime);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(attrs);
}

/* MARK: macOS Shim tests */
#include <Security/CMSEncoder.h>
#include <Security/CMSDecoder.h>

/* encode test */
static void encode_test(void)
{
    CMSEncoderRef encoder = NULL;
    CFDataRef attributeData = NULL, message = NULL, p12Data = NULL;
    CFArrayRef imported_items = NULL;
    SecIdentityRef identity = NULL;
    CFStringRef password = CFSTR("password");
    CFDictionaryRef options = CFDictionaryCreate(NULL,
                                                 (const void **)&kSecImportExportPassphrase,
                                                 (const void **)&password, 1,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    CFDictionaryRef itemDict = NULL;


    /* Create encoder */
    ok_status(CMSEncoderCreate(&encoder), "Create CMS encoder");
    ok_status(CMSEncoderSetSignerAlgorithm(encoder, kCMSEncoderDigestAlgorithmSHA256),
              "Set digest algorithm to SHA256");

    /* Load identity and set as signer */
    ok(p12Data = CFDataCreate(NULL, signing_identity_p12, sizeof(signing_identity_p12)),
       "Create p12 data");
    ok_status(SecPKCS12Import(p12Data, options, &imported_items),
              "Import identity");
    is(CFArrayGetCount(imported_items),1,"Imported 1 items");
    is(CFGetTypeID(CFArrayGetValueAtIndex(imported_items, 0)), CFDictionaryGetTypeID(),
       "Got back a dictionary");
    ok(itemDict = CFArrayGetValueAtIndex(imported_items, 0), "Retreive item dictionary");
    is(CFGetTypeID(CFDictionaryGetValue(itemDict, kSecImportItemIdentity)), SecIdentityGetTypeID(),
       "Got back an identity");
    ok(identity = (SecIdentityRef) CFRetainSafe(CFDictionaryGetValue(itemDict, kSecImportItemIdentity)),
       "Retrieve identity");
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
    ok_status(CMSEncoderUpdateContent(encoder, content, sizeof(content)), "Set content");

    /* output cms message */
    ok_status(CMSEncoderCopyEncodedContent(encoder, &message), "Finish encoding and output message");

    /* decode message */
    CMSDecoderRef decoder = NULL;
    CFDataRef contentData = NULL;
    isnt(message, NULL, "Encoded message exists");
    ok_status(CMSDecoderCreate(&decoder), "Create CMS decoder");
    ok_status(CMSDecoderUpdateMessage(decoder, CFDataGetBytePtr(message), CFDataGetLength(message)),
              "Update decoder with CMS message");
    ok(contentData = CFDataCreate(NULL, content, sizeof(content)), "Create detached content");
    ok_status(CMSDecoderSetDetachedContent(decoder, contentData), "Set detached content");
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");


    CFReleaseNull(encoder);
    CFReleaseNull(p12Data);
    CFReleaseNull(imported_items);
    CFReleaseNull(identity);
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
    ok_status(CMSDecoderUpdateMessage(decoder, valid_message, sizeof(valid_message)),
              "Update decoder with CMS message");
    ok(contentData = CFDataCreate(NULL, content, sizeof(content)), "Create detached content");
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
    ok_status(CMSDecoderUpdateMessage(decoder, invalid_message, sizeof(invalid_message)),
              "Update decoder with CMS message");
    ok(contentData = CFDataCreate(NULL, content, sizeof(content)), "Create detached content");
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
    ok_status(CMSDecoderUpdateMessage(decoder, valid_no_attr, sizeof(valid_no_attr)),
              "Update decoder with CMS message");
    ok(contentData = CFDataCreate(NULL, content, sizeof(content)), "Create detached content");
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

static void macos_shim_tests(void) {
    encode_test();
    decode_positive_test();
    decode_negative_test();
    decode_no_attr_test();
}

int si_89_cms_hash_agility(int argc, char *const *argv)
{
    plan_tests(20+24+13+8+10);

    ios_shim_tests();
    macos_shim_tests();

    return 0;
}
