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


#include <Security/SecureObjectSync/SOSManifest.h>
#include <Security/SecureObjectSync/SOSMessage.h>

#include "secd_regressions.h"

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/der_plist.h>
#include <Security/SecureObjectSync/SOSDigestVector.h>
#include <securityd/SecDbItem.h>
#include <stdlib.h>

static int kTestTestCount = 68;


#define okmfcomplement(r, b, n, ...)  test_okmfcomplement(r, b, n, test_create_description(__VA_ARGS__), test_directive, test_reason, __FILE__, __LINE__, NULL)
#define okmfdiff(a, b, eab,eba, ...)  test_okmfdiff(a, b, eab, eba, test_create_description(__VA_ARGS__), test_directive, test_reason, __FILE__, __LINE__, NULL)
#define okmfintersection(a, b, n, ...)  test_okmfintersection(a, b, n, test_create_description(__VA_ARGS__), test_directive, test_reason, __FILE__, __LINE__, NULL)
#define okmfpatch(b, r, a, n, ...)  test_okmfpatch(b, r, a, n, test_create_description(__VA_ARGS__), test_directive, test_reason, __FILE__, __LINE__, NULL)
#define okmfunion(a, b, n, ...)  test_okmfunion(a, b, n, test_create_description(__VA_ARGS__), test_directive, test_reason, __FILE__, __LINE__, NULL)

static void appendManifestDescription(CFMutableStringRef string, CFStringRef prefix, SOSManifestRef mf) {
    if (prefix)
        CFStringAppend(string, prefix);
    if (!mf) {
        CFStringAppend(string, CFSTR("null"));
    } else if (SOSManifestGetCount(mf) == 0) {
        CFStringAppend(string, CFSTR("empty"));
    } else {
        char prefix = '{';
        const uint8_t *d = SOSManifestGetBytePtr(mf);
        for (CFIndex endIX = SOSManifestGetCount(mf), curIX = 0; curIX < endIX; ++curIX, d += SOSDigestSize) {
            CFStringAppendFormat(string, NULL, CFSTR("%c%c"), prefix, *d);
            prefix = ' ';
        }
        CFStringAppend(string, CFSTR("}"));
    }
}

static int appendManifestComparison(CFMutableStringRef string, CFStringRef prefix, SOSManifestRef expected, SOSManifestRef computed) {
    int passed = 0;
    appendManifestDescription(string, prefix, computed);
    if (CFEqualSafe(computed, expected)) {
        // ok
        passed = 1;
    } else {
        // wrong
        appendManifestDescription(string, CFSTR("!="), expected);
    }
    return passed;
}

static void test_okmfcomplement(SOSManifestRef r, SOSManifestRef b, SOSManifestRef n, __attribute((cf_consumed)) CFStringRef test_description, const char *test_directive, const char *reason, const char *file, unsigned line, const char *fmt, ...) {
    CFErrorRef error = NULL;
    int passed = 0;
    CFMutableStringRef extendedDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, test_description);
    if (test_description && CFStringGetLength(test_description) != 0)
        CFStringAppend(extendedDescription, CFSTR(" "));
    appendManifestDescription(extendedDescription, CFSTR("complement "), r);
    appendManifestDescription(extendedDescription, CFSTR("->"), b);
    SOSManifestRef new = SOSManifestCreateComplement(r, b, &error);
    if (!new) {
        CFStringAppendFormat(extendedDescription, NULL, CFSTR(" SOSManifestCreateComplement: %@"), error);
    } else {
        passed = appendManifestComparison(extendedDescription, CFSTR(" -> "), n, new);
    }
    test_ok(passed, extendedDescription, test_directive, test_reason, file, line, NULL);
    CFReleaseSafe(test_description);
    CFReleaseSafe(new);
    CFReleaseSafe(error);
}

static void test_okmfdiff(SOSManifestRef a, SOSManifestRef b, SOSManifestRef exp_a_b, SOSManifestRef exp_b_a, __attribute((cf_consumed)) CFStringRef test_description, const char *test_directive, const char *reason, const char *file, unsigned line, const char *fmt, ...) {
    CFErrorRef error = NULL;
    SOSManifestRef a_minus_b = NULL;
    SOSManifestRef b_minus_a = NULL;
    int passed = 0;
    CFMutableStringRef extendedDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, test_description);
    if (test_description && CFStringGetLength(test_description) != 0)
        CFStringAppend(extendedDescription, CFSTR(" "));
    appendManifestDescription(extendedDescription, CFSTR("diff "), a);
    appendManifestDescription(extendedDescription, CFSTR("->"), b);
    if (!SOSManifestDiff(a, b, &a_minus_b, &b_minus_a, &error)) {
        CFStringAppendFormat(extendedDescription, NULL, CFSTR(" SOSManifestDiff: %@"), error);
    } else {
        passed = appendManifestComparison(extendedDescription, CFSTR(" -> "), exp_a_b, a_minus_b);
        passed &= appendManifestComparison(extendedDescription, CFSTR("->"), exp_b_a, b_minus_a);
    }
    test_ok(passed, extendedDescription, test_directive, test_reason, file, line, NULL);
    CFReleaseSafe(test_description);
    CFReleaseSafe(a_minus_b);
    CFReleaseSafe(b_minus_a);
    CFReleaseSafe(error);
}

static void test_okmfintersection(SOSManifestRef a, SOSManifestRef b, SOSManifestRef n, __attribute((cf_consumed)) CFStringRef test_description, const char *test_directive, const char *reason, const char *file, unsigned line, const char *fmt, ...) {
    CFErrorRef error = NULL;
    int passed = 0;
    CFMutableStringRef extendedDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, test_description);
    if (test_description && CFStringGetLength(test_description) != 0)
        CFStringAppend(extendedDescription, CFSTR(" "));
    appendManifestDescription(extendedDescription, CFSTR("intersection "), a);
    appendManifestDescription(extendedDescription, CFSTR("->") /*CFSTR(" \xe2\x88\xa9 ")*/, b);
    SOSManifestRef new = SOSManifestCreateIntersection(a, b, &error);
    if (!new) {
        CFStringAppendFormat(extendedDescription, NULL, CFSTR(" SOSManifestCreateIntersection: %@"), error);
    } else {
        passed = appendManifestComparison(extendedDescription, CFSTR(" -> "), n, new);
    }
    test_ok(passed, extendedDescription, test_directive, test_reason, file, line, NULL);
    CFReleaseSafe(test_description);
    CFReleaseSafe(new);
    CFReleaseSafe(error);
}

static void test_okmfpatch(SOSManifestRef b, SOSManifestRef r, SOSManifestRef a, SOSManifestRef n, __attribute((cf_consumed)) CFStringRef test_description, const char *test_directive, const char *reason, const char *file, unsigned line, const char *fmt, ...) {
    CFErrorRef error = NULL;
    int passed = 0;
    CFMutableStringRef extendedDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, test_description);
    if (test_description && CFStringGetLength(test_description) != 0)
        CFStringAppend(extendedDescription, CFSTR(" "));
    appendManifestDescription(extendedDescription, CFSTR("patch "), b);
    appendManifestDescription(extendedDescription, CFSTR("->"), r);
    appendManifestDescription(extendedDescription, CFSTR("->"), a);
    SOSManifestRef new = SOSManifestCreateWithPatch(b, r, a, &error);
    if (!new) {
        CFStringAppendFormat(extendedDescription, NULL, CFSTR(" SOSManifestCreateWithPatch: %@"), error);
    } else {
        passed = appendManifestComparison(extendedDescription, CFSTR(" -> "), n, new);
    }
    test_ok(passed, extendedDescription, test_directive, test_reason, file, line, NULL);
    CFReleaseSafe(test_description);
    CFReleaseSafe(new);
    CFReleaseSafe(error);
}

static void test_okmfunion(SOSManifestRef a, SOSManifestRef b, SOSManifestRef n, __attribute((cf_consumed)) CFStringRef test_description, const char *test_directive, const char *reason, const char *file, unsigned line, const char *fmt, ...) {
    CFErrorRef error = NULL;
    int passed = 0;
    CFMutableStringRef extendedDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, test_description);
    if (test_description && CFStringGetLength(test_description) != 0)
        CFStringAppend(extendedDescription, CFSTR(" "));
    appendManifestDescription(extendedDescription, CFSTR("union "), a);
    appendManifestDescription(extendedDescription, CFSTR("->") /*CFSTR(" \xe2\x88\xaa ")*/, b);
    SOSManifestRef new = SOSManifestCreateUnion(a, b, &error);
    if (!new) {
        CFStringAppendFormat(extendedDescription, NULL, CFSTR(" SOSManifestCreateUnion: %@"), error);
    } else {
        passed = appendManifestComparison(extendedDescription, CFSTR(" -> "), n, new);
    }
    test_ok(passed, extendedDescription, test_directive, test_reason, file, line, NULL);
    CFReleaseSafe(test_description);
    CFReleaseSafe(new);
    CFReleaseSafe(error);
}

static SOSManifestRef createManifestWithString(CFStringRef string) {
    struct SOSDigestVector dv = SOSDigestVectorInit;
    CFIndex length = string ? CFStringGetLength(string) : 0;
    CFStringInlineBuffer buf = {};
    CFRange range = { 0, length };
    CFStringInitInlineBuffer(string, &buf, range);
    for (CFIndex ix = 0; ix < length; ++ix) {
        uint8_t digest[20] = "                   ";
        UniChar c = CFStringGetCharacterFromInlineBuffer(&buf, ix);
        digest[0] = c;
        SOSDigestVectorAppend(&dv, digest);
    }
    SOSManifestRef mf = NULL;
    mf = SOSManifestCreateWithDigestVector(&dv, NULL);
    SOSDigestVectorFree(&dv);
    return mf;
}

static SOSManifestRef testCreateBadManifest() {
    SOSManifestRef mf = createManifestWithString(CFSTR("bab"));
    is(SOSManifestGetCount(mf), (size_t)2, "dupes eliminated?");
    return mf;
}


typedef struct mf_vectors {
    SOSManifestRef null;
    SOSManifestRef empty;
    SOSManifestRef a;
    SOSManifestRef b;
    SOSManifestRef ab;
    SOSManifestRef bab;
    SOSManifestRef agh;
    SOSManifestRef gh;
    SOSManifestRef agghhjk;
    SOSManifestRef gghh;
    SOSManifestRef agghh;
} mf_t;

static void setupMF(mf_t *mf) {
    CFErrorRef error = NULL;
    mf->null = NULL;
    mf->empty = SOSManifestCreateWithBytes(NULL, 0, &error);
    mf->a = createManifestWithString(CFSTR("a"));
    mf->b = createManifestWithString(CFSTR("b"));
    mf->ab = createManifestWithString(CFSTR("ab"));
    mf->bab = testCreateBadManifest();
    mf->gh = createManifestWithString(CFSTR("gh"));
    mf->agh = createManifestWithString(CFSTR("agh"));
    mf->agghhjk = createManifestWithString(CFSTR("agghhjk"));
    mf->gghh = createManifestWithString(CFSTR("gghh"));
    mf->agghh = createManifestWithString(CFSTR("agghh"));
}

static void teardownMF(mf_t *mf) {
    if (!mf) return;
    CFReleaseSafe(mf->empty);
    CFReleaseSafe(mf->a);
    CFReleaseSafe(mf->b);
    CFReleaseSafe(mf->ab);
    CFReleaseSafe(mf->bab);
    CFReleaseSafe(mf->gh);
    CFReleaseSafe(mf->agh);
    CFReleaseSafe(mf->agghhjk);
    CFReleaseSafe(mf->gghh);
    CFReleaseSafe(mf->agghh);
}

static void testNullManifest(SOSManifestRef mf)
{
    is(SOSManifestGetCount(mf), (size_t)0, "count is 0");
    is(SOSManifestGetSize(mf), (size_t)0, "capacity is 0");
}

static void testNull(mf_t *mf) {
    testNullManifest(mf->null);
    testNullManifest(mf->empty);
}

static void testDiff(mf_t *mf) {
    okmfdiff(mf->null, mf->null, mf->empty, mf->empty);
    okmfdiff(mf->null, mf->empty, mf->empty, mf->empty);
    okmfdiff(mf->null, mf->a, mf->empty, mf->a);
    okmfdiff(mf->empty, mf->null, mf->empty, mf->empty);
    okmfdiff(mf->empty, mf->empty, mf->empty, mf->empty);
    okmfdiff(mf->empty, mf->a, mf->empty, mf->a);
    okmfdiff(mf->a, mf->null, mf->a, mf->empty);
    okmfdiff(mf->a, mf->empty, mf->a, mf->empty);
    okmfdiff(mf->a, mf->a, mf->empty, mf->empty);
    okmfdiff(mf->bab, mf->empty, mf->ab, mf->empty);
    okmfdiff(mf->bab, mf->a, mf->b, mf->empty);
    okmfdiff(mf->a, mf->bab, mf->empty, mf->b);
    okmfdiff(mf->bab, mf->b, mf->a, mf->empty);
    okmfdiff(mf->b, mf->bab, mf->empty, mf->a);
    okmfdiff(mf->bab, mf->bab, mf->empty, mf->empty);
}

static void testPatch(mf_t *mf) {
    okmfpatch(mf->null, mf->null, mf->null, mf->empty);
    okmfpatch(mf->empty, mf->empty, mf->empty, mf->empty);
    okmfpatch(mf->bab, mf->b, mf->a, mf->a);
    okmfpatch(mf->ab, mf->empty, mf->empty, mf->ab);
    okmfpatch(mf->ab, mf->empty, mf->a, mf->ab);
    okmfpatch(mf->bab, mf->empty, mf->a, mf->ab);
    okmfpatch(mf->bab, mf->ab, mf->null, mf->empty);
    okmfpatch(mf->bab, mf->empty, mf->empty, mf->ab);
}

static void testUnion(mf_t *mf) {
    okmfunion(mf->null, mf->null, mf->empty);
    okmfunion(mf->null, mf->empty, mf->empty);
    okmfunion(mf->empty, mf->null, mf->empty);
    okmfunion(mf->empty, mf->empty, mf->empty);
    okmfunion(mf->null, mf->a, mf->a);
    okmfunion(mf->a, mf->null, mf->a);
    okmfunion(mf->empty, mf->a, mf->a);
    okmfunion(mf->a, mf->empty, mf->a);
    okmfunion(mf->bab, mf->ab, mf->ab);
    okmfunion(mf->empty, mf->bab, mf->ab);
    okmfunion(mf->bab, mf->empty, mf->ab);
}

static void testIntersect(mf_t *mf) {
    okmfintersection(mf->null, mf->null, mf->empty);
    okmfintersection(mf->null, mf->empty, mf->empty);
    okmfintersection(mf->empty, mf->null, mf->empty);
    okmfintersection(mf->empty, mf->empty, mf->empty);
    okmfintersection(mf->null, mf->a, mf->empty);
    okmfintersection(mf->a, mf->null, mf->empty);
    okmfintersection(mf->empty, mf->a, mf->empty);
    okmfintersection(mf->a, mf->empty, mf->empty);
    okmfintersection(mf->bab, mf->ab, mf->ab);
    okmfintersection(mf->bab, mf->a, mf->a);
    okmfintersection(mf->a, mf->bab, mf->a);
    okmfintersection(mf->b, mf->bab, mf->b);
    okmfintersection(mf->bab, mf->bab, mf->ab);
    okmfintersection(mf->gghh, mf->agghh, mf->gh);
    okmfintersection(mf->agghhjk, mf->agghh, mf->agh);
}

static void testComplement(mf_t *mf) {
    okmfcomplement(mf->null, mf->null, mf->empty);
    okmfcomplement(mf->null, mf->empty, mf->empty);
    okmfcomplement(mf->empty, mf->null, mf->empty);
    okmfcomplement(mf->empty, mf->empty, mf->empty);
    okmfcomplement(mf->null, mf->a, mf->a);
    okmfcomplement(mf->a, mf->null, mf->empty);
    okmfcomplement(mf->empty, mf->a, mf->a);
    okmfcomplement(mf->a, mf->empty, mf->empty);
    okmfcomplement(mf->bab, mf->ab, mf->empty);
    okmfcomplement(mf->ab, mf->bab, mf->empty);
    okmfcomplement(mf->bab, mf->a, mf->empty);
    okmfcomplement(mf->a, mf->bab, mf->b);
    okmfcomplement(mf->b, mf->bab, mf->a);
    okmfcomplement(mf->empty, mf->bab, mf->ab);
}

static void tests(void)
{
    mf_t mf;
    setupMF(&mf);

    testNull(&mf);
    testDiff(&mf);
    testPatch(&mf);
    testUnion(&mf);
    testIntersect(&mf);
    testComplement(&mf);

    teardownMF(&mf);
}

int secd_49_manifests(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
