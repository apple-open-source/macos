/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

#include <Security/SecCmsMessage.h>
#include <Security/SecCmsDecoder.h>

const NSString *kSecTestCMSParseFailureResources = @"si-27-cms-parse/ParseFailureCMS";
const NSString *kSecTestCMSParseSuccessResources = @"si-27-cms-parse/ParseSuccessCMS";


static void test_cms_parse_success(void) {
    NSArray<NSURL*>*cmsURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".der" subdirectory:(NSString *)kSecTestCMSParseSuccessResources];
    if ([cmsURLs count] > 0) {
        [cmsURLs enumerateObjectsUsingBlock:^(NSURL * _Nonnull url, NSUInteger __unused idx, BOOL * __unused _Nonnull stop) {
            SecCmsMessageRef cmsg = NULL;
            NSData *cmsData = [NSData dataWithContentsOfURL:url];
            SecAsn1Item encoded_message = { [cmsData length], (uint8_t*)[cmsData bytes] };
            ok_status(SecCmsMessageDecode(&encoded_message, NULL, NULL, NULL, NULL, NULL, NULL, &cmsg), "Failed to parse CMS: %@", url);
            if (cmsg) SecCmsMessageDestroy(cmsg);
        }];
    }
}

static void test_cms_parse_failure(void) {
    NSArray<NSURL*>*cmsURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".der" subdirectory:(NSString *)kSecTestCMSParseFailureResources];
    if ([cmsURLs count] > 0) {
        [cmsURLs enumerateObjectsUsingBlock:^(NSURL * _Nonnull url, NSUInteger __unused idx, BOOL * __unused _Nonnull stop) {
            SecCmsMessageRef cmsg = NULL;
            NSData *cmsData = [NSData dataWithContentsOfURL:url];
            SecAsn1Item encoded_message = { [cmsData length], (uint8_t*)[cmsData bytes] };
            isnt(errSecSuccess, SecCmsMessageDecode(&encoded_message, NULL, NULL, NULL, NULL, NULL, NULL, &cmsg),
                 "Successfully parsed bad CMS: %@", url);
            if (cmsg) SecCmsMessageDestroy(cmsg);
        }];
    }
}

int si_27_cms_parse(int argc, char *const *argv)
{
    int num_tests = 1;
    NSArray<NSURL*>*cmsURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".der" subdirectory:(NSString *)kSecTestCMSParseFailureResources];
    num_tests += [cmsURLs count];
    cmsURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".der" subdirectory:(NSString *)kSecTestCMSParseSuccessResources];
    num_tests += [cmsURLs count];

    plan_tests(num_tests);
    isnt(num_tests, 1, "no tests run!");

    test_cms_parse_success();
    test_cms_parse_failure();

    return 0;
}
