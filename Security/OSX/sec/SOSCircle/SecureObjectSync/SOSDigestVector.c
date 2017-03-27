/*
 * Copyright (c) 2013-2015 Apple Inc. All Rights Reserved.
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


#include <Security/SecureObjectSync/SOSDigestVector.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>

CFStringRef kSOSDigestVectorErrorDomain = CFSTR("com.apple.security.sos.digestvector.error");

/* SOSDigestVector code. */

const size_t kMaxDVCapacity = (1024*1024L);  // roughly based on KVS limit, helps avoid integer overflow issues

static bool SOSDigestVectorEnsureCapacity(struct SOSDigestVector *dv, size_t count) {
    // Note that capacity is the number of digests it can hold, not the size in bytes
    // Already big enough.
    if (count <= dv->capacity)
        return true;
    // Too big
    if (count > kMaxDVCapacity) {
        secerror("Requesting too much space for digest vectors: %ld", count);
        return false;
    }

    size_t capacity = (dv->capacity + 16) * 3 / 2;
    size_t digestSize = sizeof(*(dv->digest));
    if (capacity < count)
        capacity = count;
    dv->digest = reallocf(dv->digest, digestSize * capacity);
    if (dv->digest == NULL) {
        dv->count = 0;
        secerror("reallocf failed requesting space for digest vectors: %ld (bytes)", digestSize * capacity);
        return false;
    }
    dv->capacity = capacity;
    return true;
}

static void SOSDigestVectorAppendOrdered(struct SOSDigestVector *dv, const uint8_t *digest)
{
	if (SOSDigestVectorEnsureCapacity(dv, dv->count + 1))
        memcpy(dv->digest[dv->count++], digest, SOSDigestSize);
}

void SOSDigestVectorAppend(struct SOSDigestVector *dv, const uint8_t *digest)
{
    SOSDigestVectorAppendOrdered(dv, digest);
	dv->unsorted = true;
}

static int SOSDigestCompare(const void *a, const void *b)
{
	return memcmp(a, b, SOSDigestSize);
}

// Remove duplicates from sorted manifest using minimal memmove() calls
static void SOSDigestVectorUnique(struct SOSDigestVector *dv) {
    if (dv->count < 2 || dv->digest == NULL)
        return;

    const uint8_t *prev = dv->digest[0];
    uint8_t *dest = dv->digest[1];
    const uint8_t *end = dv->digest[dv->count];
    const uint8_t *source = dest;
    for (const uint8_t *cur = source; cur < end; cur += SOSDigestSize) {
        int delta = SOSDigestCompare(prev, cur);
        if (delta < 0) {
            // Found a properly sorted element
            // 1) Extend the current region (prev is end of region pointer)
            // 2) Reset prev
            prev = cur;
        } else if (delta > 0) {
            // DigestVector wasn't sorted!
            assert(delta <= 0);
        } else {
            // Found a duplicate element
            // 1) Finish copy for current region up to previous element
            prev += SOSDigestSize;
            if (dest != source)
                memmove(dest, source, prev - source);
            dest += prev - source;
            // 2) Skip remaining dupes
            if (cur < end) {
                while (cur += SOSDigestSize, cur < end) {
                    int delta = SOSDigestCompare(prev, cur);
                    assert(delta <= 0);
                    if (delta != 0)
                        break;
                }
            }
            // cur now points to the first new element that hasn't yet been copied
            // 3) Set start of next region
            prev = cur;
            source = cur;
        }
    }

    // Copy remainder of final region
    if (source < end) {
        prev += SOSDigestSize;
        if (dest != source)
            memmove(dest, source, prev - source);
        dest += prev - source;
    }
    dv->count = (dest - dv->digest[0]) / SOSDigestSize;
}


void SOSDigestVectorSort(struct SOSDigestVector *dv)
{
    if (dv->unsorted && dv->digest) {
        qsort(dv->digest, dv->count, sizeof(*dv->digest), SOSDigestCompare);
        dv->unsorted = false;
        SOSDigestVectorUnique(dv);
    }
}

void SOSDigestVectorUniqueSorted(struct SOSDigestVector *dv)
{
	// Uniqify in place (sort does this now for safety)
    if (dv->unsorted)
		SOSDigestVectorSort(dv);
}

void SOSDigestVectorSwap(struct SOSDigestVector *dva, struct SOSDigestVector *dvb)
{
    struct SOSDigestVector dv;
      dv = *dva;
    *dva = *dvb;
    *dvb = dv;
}

bool SOSDigestVectorContainsSorted(const struct SOSDigestVector *dv, const uint8_t *digest)
{
    return SOSDigestVectorIndexOfSorted(dv, digest) != (size_t)-1;
}

bool SOSDigestVectorContains(struct SOSDigestVector *dv, const uint8_t *digest)
{
    if (dv->unsorted)
		SOSDigestVectorSort(dv);
    return SOSDigestVectorContainsSorted(dv, digest);
}

size_t SOSDigestVectorIndexOfSorted(const struct SOSDigestVector *dv, const uint8_t *digest)
{
    if (dv->digest) {
        const void *pos = bsearch(digest, dv->digest, dv->count, sizeof(*dv->digest), SOSDigestCompare);
        return pos ? ((size_t)(pos - (void *)dv->digest)) / SOSDigestSize : ((size_t)-1);
    } else {
        return -1;
    }
}

size_t SOSDigestVectorIndexOf(struct SOSDigestVector *dv, const uint8_t *digest)
{
	if (dv->unsorted)
		SOSDigestVectorSort(dv);
    return SOSDigestVectorIndexOfSorted(dv, digest);
}

void SOSDigestVectorFree(struct SOSDigestVector *dv)
{
	free(dv->digest);
	dv->digest = NULL;
	dv->count = 0;
	dv->capacity = 0;
	dv->unsorted = false;
}

void SOSDigestVectorApplySorted(const struct SOSDigestVector *dv, SOSDigestVectorApplyBlock with)
{
    bool stop = false;
	for (size_t ix = 0; !stop && ix < dv->count && dv->digest; ++ix) {
        with(dv->digest[ix], &stop);
	}
}

void SOSDigestVectorApply(struct SOSDigestVector *dv, SOSDigestVectorApplyBlock with)
{
	if (dv->unsorted)
		SOSDigestVectorSort(dv);
    SOSDigestVectorApplySorted(dv, with);
}

// TODO: Check for NDEBUG to disable skip dupes are release time.
//#define SOSDVSKIPDUPES  0
#define SOSDVSKIPDUPES  1

#if SOSDVSKIPDUPES
#define SOSDVINCRIX(dv,ix) (SOSDigestVectorIncrementAndSkipDupes(dv,ix))

static size_t SOSIncrementAndSkipDupes(const uint8_t *digests, size_t count, const size_t ix) {
    size_t new_ix = ix;
    if (digests && new_ix < count) {
        while (++new_ix < count) {
            int delta = SOSDigestCompare(digests + ix * SOSDigestSize, digests + new_ix * SOSDigestSize);
            assert(delta <= 0);
            if (delta != 0)
                break;
        }
    }
    return new_ix;
}

static size_t SOSDigestVectorIncrementAndSkipDupes(const struct SOSDigestVector *dv, const size_t ix) {
    return SOSIncrementAndSkipDupes((const uint8_t *)dv->digest, dv->count, ix);
}

void SOSDigestVectorAppendMultipleOrdered(struct SOSDigestVector *dv,
                                          size_t count, const uint8_t *digests) {
    size_t ix = 0;
    while (ix < count) {
        SOSDigestVectorAppendOrdered(dv, digests + (ix * SOSDigestSize));
        ix = SOSIncrementAndSkipDupes(digests, count, ix);
    }
}

#else /* !SOSDVSKIPDUPES */

#define SOSDVINCRIX(dv,ix) (ix + 1)

void SOSDigestVectorAppendMultipleOrdered(struct SOSDigestVector *dv,
                                          size_t count, const uint8_t *digests) {
    if (count) {
        if (SOSDigestVectorEnsureCapacity(dv, dv->count + count))
            memcpy(dv->digest[dv->count], digests, count * SOSDigestSize);
        dv->count += count;
    }
}

#endif /* !SOSDVSKIPDUPES */

void SOSDigestVectorIntersectSorted(const struct SOSDigestVector *dv1, const struct SOSDigestVector *dv2,
                                    struct SOSDigestVector *dvintersect)
{
    /* dvintersect should be empty to start. */
    assert(dvintersect->count == 0);
    size_t i1 = 0, i2 = 0;
    while (i1 < dv1->count && i2 < dv2->count) {
        int delta = SOSDigestCompare(dv1->digest[i1], dv2->digest[i2]);
        if (delta == 0) {
            SOSDigestVectorAppendOrdered(dvintersect, dv1->digest[i1]);
            i1 = SOSDVINCRIX(dv1, i1);
            i2 = SOSDVINCRIX(dv2, i2);
        } else if (delta < 0) {
            i1 = SOSDVINCRIX(dv1, i1);
        } else {
            i2 = SOSDVINCRIX(dv2, i2);
        }
    }
}

void SOSDigestVectorUnionSorted(const struct SOSDigestVector *dv1, const struct SOSDigestVector *dv2,
                                struct SOSDigestVector *dvunion)
{
    /* dvunion should be empty to start. */
    assert(dvunion->count == 0);
    size_t i1 = 0, i2 = 0;
    while (i1 < dv1->count && i2 < dv2->count) {
        int delta = SOSDigestCompare(dv1->digest[i1], dv2->digest[i2]);
        if (delta == 0) {
            SOSDigestVectorAppendOrdered(dvunion, dv1->digest[i1]);
            i1 = SOSDVINCRIX(dv1, i1);
            i2 = SOSDVINCRIX(dv2, i2);
        } else if (delta < 0) {
            SOSDigestVectorAppendOrdered(dvunion, dv1->digest[i1]);
            i1 = SOSDVINCRIX(dv1, i1);
        } else {
            SOSDigestVectorAppendOrdered(dvunion, dv2->digest[i2]);
            i2 = SOSDVINCRIX(dv2, i2);
        }
    }
    SOSDigestVectorAppendMultipleOrdered(dvunion, dv1->count - i1, dv1->digest[i1]);
    SOSDigestVectorAppendMultipleOrdered(dvunion, dv2->count - i2, dv2->digest[i2]);
}

void SOSDigestVectorDiffSorted(const struct SOSDigestVector *dv1, const struct SOSDigestVector *dv2,
                               struct SOSDigestVector *dv1_2, struct SOSDigestVector *dv2_1)
{
    /* dv1_2 and dv2_1 should be empty to start. */
    assert(dv1_2->count == 0);
    assert(dv2_1->count == 0);

    size_t i1 = 0, i2 = 0;
    while (i1 < dv1->count && i2 < dv2->count) {
        int delta = SOSDigestCompare(dv1->digest[i1], dv2->digest[i2]);
        if (delta == 0) {
            i1 = SOSDVINCRIX(dv1, i1);
            i2 = SOSDVINCRIX(dv2, i2);
        } else if (delta < 0) {
            SOSDigestVectorAppendOrdered(dv1_2, dv1->digest[i1]);
            i1 = SOSDVINCRIX(dv1, i1);
        } else {
            SOSDigestVectorAppendOrdered(dv2_1, dv2->digest[i2]);
            i2 = SOSDVINCRIX(dv2, i2);
        }
    }
    SOSDigestVectorAppendMultipleOrdered(dv1_2, dv1->count - i1, dv1->digest[i1]);
    SOSDigestVectorAppendMultipleOrdered(dv2_1, dv2->count - i2, dv2->digest[i2]);
}

void SOSDigestVectorDiff(struct SOSDigestVector *dv1, struct SOSDigestVector *dv2,
                         struct SOSDigestVector *dv1_2, struct SOSDigestVector *dv2_1)
{
    if (dv1->unsorted) SOSDigestVectorSort(dv1);
	if (dv2->unsorted) SOSDigestVectorSort(dv2);
    SOSDigestVectorDiffSorted(dv1, dv2, dv1_2, dv2_1);
}

/*
 If A and B are sets, then the relative complement of A in B, also termed the set-theoretic difference of B and A,
 is the set of elements in B, but not in A. The relative complement of A in B is denoted B ∖ A according to the ISO 31-11 standard
 sometimes written B − A
 
 The common case for us will be Removals\Additions
 */

static void SOSDigestVectorAppendComplementAtIndex(size_t a_ix, const struct SOSDigestVector *dvA, size_t b_ix, const struct SOSDigestVector *dvB,
                                     struct SOSDigestVector *dvcomplement)
{
    assert(a_ix <= dvA->count && b_ix <= dvB->count);
    while (a_ix < dvA->count && b_ix < dvB->count && dvA->digest && dvB->digest) {
        int delta = SOSDigestCompare(dvA->digest[a_ix], dvB->digest[b_ix]);
        if (delta == 0) {
            a_ix = SOSDVINCRIX(dvA, a_ix);
            b_ix = SOSDVINCRIX(dvB, b_ix);
        } else if (delta < 0) {
            a_ix = SOSDVINCRIX(dvA, a_ix);
        } else {
            SOSDigestVectorAppendOrdered(dvcomplement, dvB->digest[b_ix]);
            b_ix = SOSDVINCRIX(dvB, b_ix);
        }
    }
    if (dvB->digest)
        SOSDigestVectorAppendMultipleOrdered(dvcomplement, dvB->count - b_ix, dvB->digest[b_ix]);
}


void SOSDigestVectorComplementSorted(const struct SOSDigestVector *dvA, const struct SOSDigestVector *dvB,
                                     struct SOSDigestVector *dvcomplement)
{
    /* dvcomplement should be empty to start. */
    assert(dvcomplement->count == 0);
    assert(!dvA->unsorted);
	assert(!dvB->unsorted);

    SOSDigestVectorAppendComplementAtIndex(0, dvA, 0, dvB, dvcomplement);
}


/*
    For each item in base
 
 one way to do would be to define SOSDigestVectorComplementSorted
 
 For removals, if removal value is less than base, increment until GEQ
 */
bool SOSDigestVectorPatchSorted(const struct SOSDigestVector *base, const struct SOSDigestVector *removals,
                                const struct SOSDigestVector *additions, struct SOSDigestVector *dv,
                                CFErrorRef *error)
{
    /* dv should be empty to start. */
    assert(dv->count == 0);
    assert(!base->unsorted);
	assert(!removals->unsorted);
	assert(!additions->unsorted);

    size_t i1 = 0, i2 = 0, i3 = 0;
    while (i1 < base->count && i2 < additions->count) {
        // Pick the smaller of base->digest[i1] and additions->digest[i2] as a
        // candidate to be put into the output vector. If udelta positive, addition is smaller
        int udelta = SOSDigestCompare(base->digest[i1], additions->digest[i2]);
        const uint8_t *candidate = udelta < 0 ? base->digest[i1] : additions->digest[i2];

        // ddelta > 0 means rem > candidate
        int ddelta = 1;
        while (i3 < removals->count) {
            ddelta = SOSDigestCompare(removals->digest[i3], candidate);
            if (ddelta < 0) {
                i3 = SOSDVINCRIX(removals, i3);
            } else {
                if (ddelta == 0)
                    i3 = SOSDVINCRIX(removals, i3);
                break;
            }
        }
        if (ddelta > 0)
            SOSDigestVectorAppendOrdered(dv, candidate);

        // Point to next (different) candidate
        if (udelta == 0) {
            i1 = SOSDVINCRIX(base, i1);
            i2 = SOSDVINCRIX(additions, i2);
        } else if (udelta < 0) {
            i1 = SOSDVINCRIX(base, i1);
        } else {
            i2 = SOSDVINCRIX(additions, i2);
        }
    }
    SOSDigestVectorAppendComplementAtIndex(i3, removals, i1, base, dv);
    SOSDigestVectorAppendComplementAtIndex(i3, removals, i2, additions, dv);

    return true;
}

bool SOSDigestVectorPatch(struct SOSDigestVector *base, struct SOSDigestVector *removals,
                          struct SOSDigestVector *additions, struct SOSDigestVector *dv,
                          CFErrorRef *error)
{
	if (base->unsorted) SOSDigestVectorSort(base);
	if (removals->unsorted) SOSDigestVectorSort(removals);
	if (additions->unsorted) SOSDigestVectorSort(additions);
    return SOSDigestVectorPatchSorted(base, removals, additions, dv, error);
}
