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

#include "cms-hashagility-test.h"

#include <test/testmore.h>
#include <Security/SecBase.h>
#include <utilities/SecCFRelease.h>
#include <Security/CMSEncoder.h>
#include <Security/SecImportExport.h>
#include <Security/CMSPrivate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrust.h>
#include <Security/SecPolicy.h>
#include <Security/SecKeychain.h>
#include <Security/SecIdentity.h>

#define TMP_KEYCHAIN_PATH "/tmp/cms_signer.keychain"

/* encode test */
static void encode_test(void)
{
    CMSEncoderRef encoder = NULL;
    CFDataRef attributeData = NULL, message = NULL, p12Data = NULL;
    CFArrayRef imported_items = NULL;
    SecIdentityRef identity = NULL;
    SecKeychainRef keychain = NULL;
    SecExternalFormat sef = kSecFormatPKCS12;
    SecItemImportExportKeyParameters keyParams = {
        .passphrase = CFSTR("password")
    };

    /* Create encoder */
    ok_status(CMSEncoderCreate(&encoder), "Create CMS encoder");
    ok_status(CMSEncoderSetSignerAlgorithm(encoder, kCMSEncoderDigestAlgorithmSHA256),
              "Set digest algorithm to SHA256");

    /* Load identity and set as signer */
    unlink(TMP_KEYCHAIN_PATH);
    ok_status(SecKeychainCreate(TMP_KEYCHAIN_PATH, 8, "password", false, NULL, &keychain),
              "Create keychain for identity");
    ok(p12Data = CFDataCreate(NULL, signing_identity_p12, sizeof(signing_identity_p12)),
       "Create p12 data");
    ok_status(SecItemImport(p12Data, NULL, &sef, NULL, 0, &keyParams, keychain, &imported_items),
              "Import identity");
    is(CFArrayGetCount(imported_items),1,"Imported 1 items");
    is(CFGetTypeID(CFArrayGetValueAtIndex(imported_items, 0)), SecIdentityGetTypeID(),
       "Got back an identity");
    ok(identity = (SecIdentityRef) CFRetainSafe(CFArrayGetValueAtIndex(imported_items, 0)),
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

    /* Cleanup */
    ok_status(SecKeychainDelete(keychain), "Delete temporary keychain");

    CFReleaseNull(encoder);
    CFReleaseNull(keychain);
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
    is(CFDataGetLength(attrValue), sizeof(attribute), "Decoded attribute size");
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

int cms_hash_agility_test(int argc, char *const *argv)
{
    plan_tests(24+13+8+10);
    
    encode_test();
    decode_positive_test();
    decode_negative_test();
    decode_no_attr_test();
    
    return 0;
}
