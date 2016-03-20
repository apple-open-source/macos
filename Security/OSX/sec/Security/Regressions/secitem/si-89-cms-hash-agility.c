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


static void tests(void)
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

int si_89_cms_hash_agility(int argc, char *const *argv)
{
    plan_tests(20);

    tests();

    return 0;
}
