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

#include "shared_regressions.h"

#import <AssertMacros.h>
#import <Foundation/Foundation.h>

#import <Security/CMSDecoder.h>
#import <Security/CMSEncoder.h>
#import <Security/SecTrust.h>
#include <utilities/SecCFRelease.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#if TARGET_OS_OSX
#include <Security/CMSPrivate.h>
#include <Security/tsaSupport.h>
#endif

#import "si-34-cms-timestamp.h"
#import "si-cms-hash-agility-data.h" // for signing_identity_p12, hash agility attribute data

static CMSSignerStatus test_verify_timestamp(NSData *content, NSData *message) {
    CMSDecoderRef decoder = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CMSSignerStatus signerStatus = kCMSSignerUnsigned;

    /* Create decoder and decode */
    require_noerr_action(CMSDecoderCreate(&decoder), fail, fail("Failed to create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, [message bytes], [message length]), fail,
                         fail("Failed to update decoder with CMS message"));
    require_noerr_action(CMSDecoderSetDetachedContent(decoder, (__bridge CFDataRef)content), fail,
                         fail("Failed to set detached content"));
    require_noerr_action(CMSDecoderFinalizeMessage(decoder), fail, fail("Failed to finalize decoder"));

    /* Get signer status */
    require_action(policy = SecPolicyCreateBasicX509(), fail, fail("Failed to Create policy"));
    require_noerr_action(CMSDecoderCopySignerStatus(decoder, 0, policy, false, &signerStatus, &trust, NULL),
              fail, fail("Failed to copy Signer status"));

fail:
    CFReleaseNull(decoder);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    return signerStatus;
}

static void test_matching_messageImprint(void) {
    /* If the timestamp is invalid, SecCmsSignerInfoVerifyUnAuthAttrs will fail, so the signer status will be
     * kCMSSignerInvalidSignature. */
    CMSSignerStatus expected_mismatch_result = kCMSSignerInvalidSignature;
#if !TIMESTAMPING_SUPPORTED
    expected_mismatch_result = kCMSSignerValid;
#endif

    is(test_verify_timestamp([NSData dataWithBytes:_developer_id_data length:sizeof(_developer_id_data)],
                             [NSData dataWithBytes:_developer_id_sig length:sizeof(_developer_id_sig)]),
       kCMSSignerValid, "failed to verify good timestamped signature");
    is(test_verify_timestamp([NSData dataWithBytes:_mismatched_content length:sizeof(_mismatched_content)],
                             [NSData dataWithBytes:_mismatched_timestamp length:sizeof(_mismatched_timestamp)]),
       expected_mismatch_result, "successful verification of bad timestamped signature");
}

static void test_create_timestamp(void) {
    CFArrayRef tmp_imported_items = NULL;
    NSArray *imported_items = nil;
    SecIdentityRef identity = nil;
    CMSEncoderRef encoder = NULL;
    CMSDecoderRef decoder = NULL;
    CFDataRef message = NULL;
    NSDictionary *attrValues  = nil;

    /* Import identity */
    NSDictionary *options = @{ (__bridge NSString *)kSecImportExportPassphrase : @"password" };
    NSData *p12Data = [NSData dataWithBytes:signing_identity_p12 length:sizeof(signing_identity_p12)];
    require_noerr_action(SecPKCS12Import((__bridge CFDataRef)p12Data, (__bridge CFDictionaryRef)options,
                                         &tmp_imported_items), exit,
                         fail("Failed to import identity"));
    imported_items = CFBridgingRelease(tmp_imported_items);
    require_noerr_action([imported_items count] == 0 &&
                         [imported_items[0] isKindOfClass:[NSDictionary class]], exit,
                         fail("Wrong imported items output"));
    identity = (SecIdentityRef)CFBridgingRetain(imported_items[0][(__bridge NSString*)kSecImportItemIdentity]);
    require_action(identity, exit, fail("Failed to get identity"));

    /* Create encoder */
    require_noerr_action(CMSEncoderCreate(&encoder), exit, fail("Failed to create CMS encoder"));
    require_noerr_action(CMSEncoderSetSignerAlgorithm(encoder, kCMSEncoderDigestAlgorithmSHA256), exit,
                         fail("Failed to set digest algorithm to SHA256"));

    /* Set identity as signer */
    require_noerr_action(CMSEncoderAddSigners(encoder, identity), exit, fail("Failed to add signer identity"));

    /* Add hash agility attribute */
    require_noerr_action(CMSEncoderAddSignedAttributes(encoder, kCMSAttrAppleCodesigningHashAgility), exit,
                         fail("Set hash agility flag"));
    require_noerr_action(CMSEncoderSetAppleCodesigningHashAgility(encoder, (__bridge CFDataRef)[NSData dataWithBytes:attribute
                                                                                                              length:sizeof(attribute)]),
                         exit, fail("Set hash agility data"));

    /* Add hash agility v2 attribute */
    attrValues = @{ @(SEC_OID_SHA1) : [NSData dataWithBytes:_attributev2 length:20],
                    @(SEC_OID_SHA256) :  [NSData dataWithBytes:(_attributev2 + 32) length:32],
                    };
    require_noerr_action(CMSEncoderAddSignedAttributes(encoder, kCMSAttrAppleCodesigningHashAgilityV2), exit,
                         fail("Set hash agility flag"));
    require_noerr_action(CMSEncoderSetAppleCodesigningHashAgilityV2(encoder, (__bridge CFDictionaryRef)attrValues), exit,
                         fail("Set hash agility data"));

#if TIMESTAMPING_SUPPORTED
    /* Set timestamp context */
    CmsMessageSetTSAContext(encoder, SecCmsTSAGetDefaultContext(NULL));
#endif

    /* Load content */
    require_noerr_action(CMSEncoderSetHasDetachedContent(encoder, true), exit, fail("Failed to set detached content"));
    require_noerr_action(CMSEncoderUpdateContent(encoder, content, sizeof(content)), exit, fail("Failed to set content"));

    /* output cms message */
    ok_status(CMSEncoderCopyEncodedContent(encoder, &message), "Finish encoding and output message");
    isnt(message, NULL, "Encoded message exists");

    /* decode message */
    require_noerr_action(CMSDecoderCreate(&decoder), exit, fail("Create CMS decoder"));
    require_noerr_action(CMSDecoderUpdateMessage(decoder, CFDataGetBytePtr(message),
                                                 CFDataGetLength(message)), exit,
                         fail("Update decoder with CMS message"));
    require_noerr_action(CMSDecoderSetDetachedContent(decoder, (__bridge CFDataRef)[NSData dataWithBytes:content
                                                                                                  length:sizeof(content)]),
                         exit, fail("Set detached content"));
    ok_status(CMSDecoderFinalizeMessage(decoder), "Finalize decoder");

exit:
    CFReleaseNull(encoder);
    CFReleaseNull(decoder);
    CFReleaseNull(message);
#if TARGET_OS_OSX
    // SecPKCS12Import adds the items to the keychain on macOS
    NSDictionary *query = @{ (__bridge NSString*)kSecValueRef : (__bridge id)identity };
    ok_status(SecItemDelete((__bridge CFDictionaryRef)query), "failed to remove identity from keychain");
#else
    pass("skip test on iOS");
#endif
    CFReleaseNull(identity);
}

static int ping_host(char *host_name){

    struct sockaddr_in pin;
    struct hostent *nlp_host;
    int sd;
    int port;
    int retries = 5;

    port=80;

    //tries 5 times then give up
    while ((nlp_host=gethostbyname(host_name))==0 && retries--){
        printf("Resolve Error! (%s) %d\n", host_name, h_errno);
        sleep(1);
    }

    if(nlp_host==0)
        return 0;

    bzero(&pin,sizeof(pin));
    pin.sin_family=AF_INET;
    pin.sin_addr.s_addr=htonl(INADDR_ANY);
    pin.sin_addr.s_addr=((struct in_addr *)(nlp_host->h_addr))->s_addr;
    pin.sin_port=htons(port);

    sd=socket(AF_INET,SOCK_STREAM,0);

    if (connect(sd,(struct sockaddr*)&pin,sizeof(pin))==-1){
        printf("connect error! (%s) %d\n", host_name, errno);
        close(sd);
        return 0;
    }
    else{
        close(sd);
        return 1;
    }
}

int si_34_cms_timestamp(int argc, char * const *argv) {
    plan_tests(6);

    test_matching_messageImprint();

    if (!ping_host("timestamp.apple.com")) {
        printf("Accessing timestamp.apple.com failed, check the network!\n");
        return 0;
    }
    test_create_timestamp();

    return 0;
}
