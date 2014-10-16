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


#include "SOSCircle_regressions.h"

#include <SecureObjectSync/SOSDigestVector.h>

#include <utilities/SecCFRelease.h>
#include <stdlib.h>

static int kTestTestCount = 15;

static void testNullDigestVector(void)
{
    struct SOSDigestVector dv = SOSDigestVectorInit;
    is(dv.count, (size_t)0, "count is 0");
    is(dv.capacity, (size_t)0, "capacity is 0");
    ok(!dv.unsorted, "unsorted is false");
}

static CFStringRef dvCopyString(const struct SOSDigestVector *dv)
{
    CFMutableStringRef desc = CFStringCreateMutable(kCFAllocatorDefault, 0);
    SOSDigestVectorApplySorted(dv, ^(const uint8_t *digest, bool *stop) {
        char buf[2] = {};
        buf[0] = digest[0];
        CFStringAppendCString(desc, buf, kCFStringEncodingUTF8);
    });
    return desc;
}

static void testIntersectUnionDigestVector(void)
{
    struct SOSDigestVector dv1 = SOSDigestVectorInit;
    struct SOSDigestVector dv2 = SOSDigestVectorInit;
    struct SOSDigestVector dvu = SOSDigestVectorInit;
    SOSDigestVectorAppend(&dv1, (void *)"a                  ");
    SOSDigestVectorAppend(&dv1, (void *)"b                  ");
    SOSDigestVectorAppend(&dv1, (void *)"d                  ");
    SOSDigestVectorAppend(&dv1, (void *)"f                  ");
    SOSDigestVectorAppend(&dv1, (void *)"h                  ");

    SOSDigestVectorAppend(&dv2, (void *)"c                  ");
    SOSDigestVectorAppend(&dv2, (void *)"d                  ");
    SOSDigestVectorAppend(&dv2, (void *)"e                  ");
    SOSDigestVectorAppend(&dv2, (void *)"f                  ");
    SOSDigestVectorAppend(&dv2, (void *)"g                  ");

    SOSDigestVectorAppend(&dvu, (void *)"a                  ");
    SOSDigestVectorAppend(&dvu, (void *)"b                  ");
    SOSDigestVectorAppend(&dvu, (void *)"b                  ");
    SOSDigestVectorAppend(&dvu, (void *)"b                  ");
    SOSDigestVectorAppend(&dvu, (void *)"b                  ");
    SOSDigestVectorAppend(&dvu, (void *)"c                  ");
    SOSDigestVectorAppend(&dvu, (void *)"d                  ");
    SOSDigestVectorAppend(&dvu, (void *)"f                  ");
    SOSDigestVectorAppend(&dvu, (void *)"h                  ");
    
    SOSDigestVectorAppend(&dvu, (void *)"c                  ");
    SOSDigestVectorAppend(&dvu, (void *)"d                  ");
    SOSDigestVectorAppend(&dvu, (void *)"e                  ");
    SOSDigestVectorAppend(&dvu, (void *)"f                  ");
    SOSDigestVectorAppend(&dvu, (void *)"g                  ");

    struct SOSDigestVector dvintersect = SOSDigestVectorInit;
    SOSDigestVectorIntersectSorted(&dv1, &dv2, &dvintersect);
    CFStringRef desc = dvCopyString(&dvintersect);
    ok(CFEqual(CFSTR("df"), desc), "intersection is %@", desc);
    CFReleaseNull(desc);

    struct SOSDigestVector dvunion = SOSDigestVectorInit;
    SOSDigestVectorUnionSorted(&dv1, &dv2, &dvunion);
    desc = dvCopyString(&dvunion);
    ok(CFEqual(CFSTR("abcdefgh"), desc), "union is %@", desc);
    CFReleaseNull(desc);

    struct SOSDigestVector dvdels = SOSDigestVectorInit;
    struct SOSDigestVector dvadds = SOSDigestVectorInit;
    SOSDigestVectorDiffSorted(&dv1, &dv2, &dvdels, &dvadds);
    desc = dvCopyString(&dvdels);
    ok(CFEqual(CFSTR("abh"), desc), "dels is %@", desc);
    CFReleaseNull(desc);
    desc = dvCopyString(&dvadds);
    ok(CFEqual(CFSTR("ceg"), desc), "adds is %@", desc);
    CFReleaseNull(desc);

    CFErrorRef localError = NULL;
    struct SOSDigestVector dvpatched = SOSDigestVectorInit;
    ok(SOSDigestVectorPatch(&dv1, &dvdels, &dvadds, &dvpatched, &localError), "patch : %@", localError);
    CFReleaseNull(localError);
    desc = dvCopyString(&dvpatched);
    ok(CFEqual(CFSTR("cdefg"), desc), "patched dv1 - dels + adds is %@, should be: %@", desc, CFSTR("cdefg"));
    CFReleaseNull(desc);

    SOSDigestVectorFree(&dvpatched);
    ok(SOSDigestVectorPatch(&dv2, &dvadds, &dvdels, &dvpatched, &localError), "patch : %@", localError);
    CFReleaseNull(localError);
    desc = dvCopyString(&dvpatched);
    ok(CFEqual(CFSTR("abdfh"), desc), "patched dv2 - adds + dels is is %@, should be: %@", desc, CFSTR("abdfh"));
    CFReleaseNull(desc);

    SOSDigestVectorAppend(&dvadds, (void *)"c                  ");
    SOSDigestVectorFree(&dvpatched);
    SOSDigestVectorUniqueSorted(&dvadds);
    ok(SOSDigestVectorPatch(&dv2, &dvadds, &dvdels, &dvpatched, &localError), "patch failed: %@", localError);
    CFReleaseNull(localError);
    desc = dvCopyString(&dvpatched);
    ok(CFEqual(CFSTR("abdfh"), desc), "patched dv2 - adds + dels is is %@, should be: %@", desc, CFSTR("abdfh"));
    CFReleaseNull(desc);
    
    SOSDigestVectorUniqueSorted(&dvu);
    desc = dvCopyString(&dvu);
    ok(CFEqual(CFSTR("abcdefgh"), desc), "uniqued dvu is %@, should be: %@", desc, CFSTR("abcdefgh"));
    CFReleaseNull(desc);

    // This operation should be idempotent
    SOSDigestVectorUniqueSorted(&dvu);
    desc = dvCopyString(&dvu);
    ok(CFEqual(CFSTR("abcdefgh"), desc), "uniqued dvu is %@, should be: %@", desc, CFSTR("abcdefgh"));
    CFReleaseNull(desc);
}

static void tests(void)
{
    testNullDigestVector();
    testIntersectUnionDigestVector();
}

int sc_45_digestvector(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
