/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include <Security/SecureObjectSync/SOSManifest.h>
#include <Security/SecureObjectSync/SOSMessage.h>

#include "secd_regressions.h"

#include <utilities/SecCFWrappers.h>
#include <utilities/der_plist.h>
#include <Security/SecureObjectSync/SOSDigestVector.h>
#include <securityd/SecDbItem.h>

static int kTestTestCount = 68;

static void testNullMessage(uint64_t msgid)
{
    SOSMessageRef sentMessage = NULL;
    SOSMessageRef rcvdMessage = NULL;
    SOSManifestRef sender = NULL;
    CFErrorRef error = NULL;
    CFDataRef data = NULL;

    // Encode
    ok(sender = SOSManifestCreateWithBytes(NULL, 0, &error), "empty sender manifest create: %@", error);
    CFReleaseNull(error);
    ok(sentMessage = SOSMessageCreateWithManifests(kCFAllocatorDefault, sender, NULL, NULL, false, &error), "sentMessage create: %@", error);
    CFReleaseNull(error);
    ok(data = SOSMessageCreateData(sentMessage, msgid, &error), "sentMessage data create: %@", error);
    CFReleaseNull(error);

    // Decode
    ok(rcvdMessage = SOSMessageCreateWithData(kCFAllocatorDefault, data, &error), "rcvdMessage create: %@", error);
    CFReleaseNull(error);
    __block size_t numObjects = 0;
    SOSMessageWithObjects(sentMessage, &error,  ^(CFDataRef object, bool *stop) {
        numObjects++;
    });
    ok(numObjects == 0, "no objects");

    // Check if we got what we started with
    ok(sentMessage && rcvdMessage && CFEqual(sentMessage, rcvdMessage), "sent %@ == rcvd %@", sentMessage, rcvdMessage);

    CFReleaseNull(data);
    CFReleaseNull(sentMessage);
    CFReleaseNull(rcvdMessage);
    CFReleaseNull(sender);
}

__unused static void testFlaggedMessage(const char *test_directive, const char *test_reason, uint64_t msgid, SOSMessageFlags flags)
{
    SOSMessageRef sentMessage = NULL;
    SOSMessageRef rcvdMessage = NULL;
    SOSManifestRef sender = NULL;
    CFErrorRef error = NULL;
    CFDataRef data = NULL;

    ok(sender = SOSManifestCreateWithBytes(NULL, 0, &error), "empty sender manifest create: %@", error);
    CFReleaseNull(error);
    ok(sentMessage = SOSMessageCreateWithManifests(kCFAllocatorDefault, sender, NULL, NULL, false, &error), "sentMessage create: %@", error);
    CFReleaseNull(error);
    SOSMessageSetFlags(sentMessage, flags);
    ok(data = SOSMessageCreateData(sentMessage, msgid, &error), "sentMessage data create: %@", error);
    CFReleaseNull(error);

    // Decode
    ok(rcvdMessage = SOSMessageCreateWithData(kCFAllocatorDefault, data, &error), "rcvdMessage create: %@", error);
    CFReleaseNull(error);
    __block size_t numObjects = 0;
    SOSMessageWithObjects(sentMessage, &error,  ^(CFDataRef object, bool *stop) {
        numObjects++;
    });
    ok(numObjects == 0, "no objects");

    is(SOSMessageGetFlags(sentMessage), flags, "flags match after roundtrip");
    ok(sentMessage && rcvdMessage && CFEqual(sentMessage, rcvdMessage), "sent %@ == rcvd %@", sentMessage, rcvdMessage);

    CFReleaseNull(data);
    CFReleaseNull(sentMessage);
    CFReleaseNull(rcvdMessage);
    CFReleaseNull(sender);
}

__unused static void testDeltaManifestMessage(const char *test_directive, const char *test_reason, uint64_t msgid)
{
    SOSMessageRef sentMessage = NULL;
    SOSMessageRef rcvdMessage = NULL;
    SOSManifestRef sender = NULL;
    SOSManifestRef proposed = NULL;
    SOSManifestRef base = NULL;
    CFErrorRef error = NULL;
    CFDataRef data = NULL;

    struct SOSDigestVector dv = SOSDigestVectorInit;
    SOSDigestVectorAppend(&dv, (const uint8_t *)"sha1 hash that is 20 bytes long or so and stuff");
    SOSDigestVectorAppend(&dv, (const uint8_t *)"sha1 hash that was 23 bytes long or so and stuff");
    SOSDigestVectorSort(&dv);
    base = SOSManifestCreateWithBytes((const uint8_t *)dv.digest, dv.count * SOSDigestSize, &error);
    SOSDigestVectorAppend(&dv, (const uint8_t *)"so much more is good to see here is another one for me");
    SOSDigestVectorAppend(&dv, (const uint8_t *)"sha1 hash that was 23 bytes long or so and stuff!");
    SOSDigestVectorAppend(&dv, (const uint8_t *)"so much for is good to see here is another one for me");
    SOSDigestVectorSort(&dv);
    if (msgid)
        proposed = SOSManifestCreateWithBytes((const uint8_t *)dv.digest, dv.count * SOSDigestSize, &error);

    CFReleaseNull(error);
    ok(sentMessage = SOSMessageCreateWithManifests(kCFAllocatorDefault, proposed, base, proposed, true, &error), "sentMessage create: %@", error);
    CFReleaseNull(error);
    ok(data = SOSMessageCreateData(sentMessage, msgid, &error), "sentMessage data create: %@ .. %@", error, sentMessage);
    CFReleaseNull(error);

    // Decode
    ok(rcvdMessage = SOSMessageCreateWithData(kCFAllocatorDefault, data, &error), "rcvdMessage create: %@", error);
    CFReleaseNull(error);
    __block size_t numObjects = 0;
    SOSMessageWithObjects(sentMessage, &error,  ^(CFDataRef object, bool *stop) {
        numObjects++;
    });
    ok(numObjects == 0, "no objects");

    ok(sentMessage && rcvdMessage && CFEqual(sentMessage, rcvdMessage), "sent %@ == rcvd %@", sentMessage, rcvdMessage);

    CFReleaseNull(data);
    CFReleaseNull(sentMessage);
    CFReleaseNull(rcvdMessage);
    CFReleaseNull(sender);
}

static CFDataRef testCopyAddedObject(SOSMessageRef message, CFPropertyListRef plist)
{
    CFErrorRef error = NULL;
    CFDataRef der;
    ok(der = CFPropertyListCreateDERData(kCFAllocatorDefault, plist, &error), "copy der: %@", error);
    CFReleaseNull(error);
    ok(SOSMessageAppendObject(message, der, &error), "likes object: %@", error);
    CFReleaseNull(error);
    return der;
}

__unused static void testObjectsMessage(const char *test_directive, const char *test_reason, uint64_t msgid)
{
    SOSMessageRef sentMessage = NULL;
    SOSMessageRef rcvdMessage = NULL;
    SOSManifestRef sender = NULL;
    SOSManifestRef proposed = NULL;
    SOSManifestRef base = NULL;
    CFErrorRef error = NULL;
    CFDataRef data = NULL;

    struct SOSDigestVector dv1 = SOSDigestVectorInit;
    struct SOSDigestVector dv2 = SOSDigestVectorInit;
    SOSDigestVectorAppend(&dv1, (const uint8_t *)"sha1 hash that is 20 bytes long or so and stuff");
    SOSDigestVectorAppend(&dv2, (const uint8_t *)"sha1 hash that was 23 bytes long or so and stuff");
    SOSDigestVectorAppend(&dv1, (const uint8_t *)"so much more is good to see here is another one for me");
    SOSDigestVectorAppend(&dv2, (const uint8_t *)"so much more is good to see here is another one for me");
    SOSDigestVectorAppend(&dv1, (const uint8_t *)"sha1 hash that was 23 bytes long or so and stuff");
    SOSDigestVectorAppend(&dv1, (const uint8_t *)"sha1 hash that was 23 bytes long or so and stuff!");
    SOSDigestVectorAppend(&dv2, (const uint8_t *)"so much for is good to see here is another one for me");
    SOSDigestVectorSort(&dv1);
    SOSDigestVectorSort(&dv2);
    base = SOSManifestCreateWithBytes((const uint8_t *)dv1.digest, dv1.count * SOSDigestSize, &error);
    if (msgid)
        proposed = SOSManifestCreateWithBytes((const uint8_t *)dv2.digest, dv2.count * SOSDigestSize, &error);
    CFReleaseNull(error);
    ok(sentMessage = SOSMessageCreateWithManifests(kCFAllocatorDefault, proposed, base, proposed, true, &error), "sentMessage create: %@", error);
    CFDataRef O0, O1, O2, O3;
    CFDataRef o0 = CFDataCreate(kCFAllocatorDefault, NULL, 0);
    O0 = testCopyAddedObject(sentMessage, o0);
    CFDataRef o1 = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)"test", 4);
    O1 = testCopyAddedObject(sentMessage, o1);
    CFDataRef o2 = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)"what an object", 14);
    O2 = testCopyAddedObject(sentMessage, o2);
    CFDataRef o3 = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)"This one even has shiny stripe.", 31);
    O3 = testCopyAddedObject(sentMessage, o3);
    ok(data = SOSMessageCreateData(sentMessage, msgid, &error), "sentMessage data create: %@ .. %@", error, sentMessage);
    CFReleaseNull(error);

    // Decode
    ok(rcvdMessage = SOSMessageCreateWithData(kCFAllocatorDefault, data, &error), "rcvdMessage create: %@", error);
    CFReleaseNull(error);
    __block size_t numObjects = 0;
    __block bool f0, f1, f2, f3;
    f0 = f1 = f2 = f3 = false;
    if (rcvdMessage) SOSMessageWithObjects(rcvdMessage, &error,  ^(CFDataRef object, bool *stop) {
        if (CFEqualSafe(object, O0)) f0 = true;
        if (CFEqualSafe(object, O1)) f1 = true;
        if (CFEqualSafe(object, O2)) f2 = true;
        if (CFEqualSafe(object, O3)) f3 = true;
        numObjects++;
    });
    ok(f0, "got O0");
    ok(f1, "got O1");
    ok(f2, "got O2");
    ok(f3, "got O3");

    ok(sentMessage && rcvdMessage && CFEqual(sentMessage, rcvdMessage), "sent %@ == rcvd %@", sentMessage, rcvdMessage);

    CFReleaseNull(o0);
    CFReleaseNull(o1);
    CFReleaseNull(o2);
    CFReleaseNull(o3);
    CFReleaseNull(O0);
    CFReleaseNull(O1);
    CFReleaseNull(O2);
    CFReleaseNull(O3);
    CFReleaseNull(data);
    CFReleaseNull(sentMessage);
    CFReleaseNull(rcvdMessage);
    CFReleaseNull(sender);
}

static void tests(void)
{
    testNullMessage(0); // v0
    
    uint64_t msgid = 0;
    testNullMessage(++msgid); // v2
    testFlaggedMessage(test_directive, test_reason, ++msgid, 0x865);
    testFlaggedMessage(test_directive, test_reason, ++msgid, 0xdeadbeef);
    TODO: {
        todo("V2 doesn't work");
        testDeltaManifestMessage(test_directive, test_reason, 0);
        testDeltaManifestMessage(test_directive, test_reason, ++msgid);
        testObjectsMessage(test_directive, test_reason, 0);
        testObjectsMessage(test_directive, test_reason, ++msgid);
    }
}

int secd_50_message(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
