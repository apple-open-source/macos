/*
 * Created by Michael Brouwer on 2/14/12.
 * Copyright 2012 Apple Inc. All Rights Reserved.
 */

/*
 * SOSTransport.c -  Implementation of the secure object syncing transport
 */

#include <SecureObjectSync/SOSTransport.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>

static CFStringRef sErrorDomain = CFSTR("com.apple.security.sos.transport.error");

/* SOSDigestVector code. */

#define VECTOR_GROW(vector, count, capacity) \
    do { \
    if ((count) > capacity) { \
        capacity = ((capacity) + 16) * 3 / 2; \
        if (capacity < (count)) \
            capacity = (count); \
        vector = realloc((vector), sizeof(*(vector)) * capacity); \
    } \
} while (0)

static void SOSDigestVectorEnsureCapacity(struct SOSDigestVector *dv, size_t count) {
	VECTOR_GROW(dv->digest, count, dv->capacity);
}

void SOSDigestVectorReplaceAtIndex(struct SOSDigestVector *dv, size_t ix, const uint8_t *digest)
{
	SOSDigestVectorEnsureCapacity(dv, ix + 1);
	memcpy(dv->digest[ix], digest, SOSDigestSize);
	dv->is_sorted = false;
}

static void SOSDigestVectorAppendOrdered(struct SOSDigestVector *dv, const uint8_t *digest)
{
	SOSDigestVectorEnsureCapacity(dv, dv->count + 1);
	memcpy(dv->digest[dv->count++], digest, SOSDigestSize);
}

void SOSDigestVectorAppend(struct SOSDigestVector *dv, const uint8_t *digest)
{
    SOSDigestVectorAppendOrdered(dv, digest);
	dv->is_sorted = false;
}

static int SOSDigestCompare(const void *a, const void *b)
{
	return memcmp(a, b, SOSDigestSize);
}

void SOSDigestVectorSort(struct SOSDigestVector *dv)
{
	qsort(dv->digest, dv->count, sizeof(*dv->digest), SOSDigestCompare);
	dv->is_sorted = true;
}

bool SOSDigestVectorContains(struct SOSDigestVector *dv, const uint8_t *digest)
{
    return SOSDigestVectorIndexOf(dv, digest) != (size_t)-1;
}

size_t SOSDigestVectorIndexOf(struct SOSDigestVector *dv, const uint8_t *digest)
{
	if (!dv->is_sorted)
		SOSDigestVectorSort(dv);
    const void *pos = bsearch(digest, dv->digest, dv->count, sizeof(*dv->digest), SOSDigestCompare);
    return pos ? ((size_t)(pos - (void *)dv->digest)) / SOSDigestSize : ((size_t)-1);
}

void SOSDigestVectorFree(struct SOSDigestVector *dv)
{
	free(dv->digest);
	dv->digest = NULL;
	dv->count = 0;
	dv->capacity = 0;
	dv->is_sorted = false;
}

void SOSDigestVectorApply(struct SOSDigestVector *dv,
                          void *context, SOSDigestVectorApplyFunc func)
{
	if (!dv->is_sorted)
		SOSDigestVectorSort(dv);

	for (size_t ix = 0; ix < dv->count; ++ix) {
		func(context, dv->digest[ix]);
	}
}

static void SOSDigestVectorAppendMultiple(struct SOSDigestVector *dv,
                                          size_t count, const uint8_t *digests) {
    if (count) {
        SOSDigestVectorEnsureCapacity(dv, dv->count + count);
        memcpy(dv->digest[dv->count], digests, count * SOSDigestSize);
        dv->count += count;
    }
}

void SOSDigestVectorDiff(struct SOSDigestVector *dv1, struct SOSDigestVector *dv2,
                         struct SOSDigestVector *dv1_2, struct SOSDigestVector *dv2_1)
{
    /* dv1_2 and dv2_1 should be empty to start. */
    assert(dv1_2->count == 0);
    assert(dv2_1->count == 0);

	if (!dv1->is_sorted)
		SOSDigestVectorSort(dv1);
	if (!dv2->is_sorted)
		SOSDigestVectorSort(dv2);

    size_t i1 = 0, i2 = 0;
    while (i1 < dv1->count && i2 < dv2->count) {
        int delta = SOSDigestCompare(dv1->digest[i1], dv2->digest[i2]);
        if (delta == 0) {
            ++i1, ++i2;
        } else if (delta < 0) {
            SOSDigestVectorAppendOrdered(dv1_2, dv1->digest[i1++]);
        } else {
            SOSDigestVectorAppendOrdered(dv2_1, dv2->digest[i2++]);
        }
    }
    SOSDigestVectorAppendMultiple(dv1_2, dv1->count - i1, dv1->digest[i1]);
    SOSDigestVectorAppendMultiple(dv2_1, dv2->count - i2, dv2->digest[i2]);

    dv1_2->is_sorted = true;
    dv2_1->is_sorted = true;
}

bool SOSDigestVectorPatch(struct SOSDigestVector *base, struct SOSDigestVector *removals,
                          struct SOSDigestVector *additions, struct SOSDigestVector *dv,
                          CFErrorRef *error)
{
    size_t base_ix = 0, removals_ix = 0, additions_ix = 0;
	if (!base->is_sorted)
		SOSDigestVectorSort(base);
	if (!removals->is_sorted)
		SOSDigestVectorSort(removals);
	if (!additions->is_sorted)
		SOSDigestVectorSort(additions);

    assert(dv->count == 0);
    SOSDigestVectorEnsureCapacity(dv, base->count - removals->count + additions->count);
    dv->is_sorted = true;

    while (base_ix < base->count) {
        const uint8_t *d = base->digest[base_ix];
        if (additions_ix < additions->count && SOSDigestCompare(d, additions->digest[additions_ix]) > 0) {
            SOSDigestVectorAppendOrdered(dv, additions->digest[additions_ix++]);
        } else if (removals_ix < removals->count && SOSDigestCompare(d, removals->digest[removals_ix]) == 0) {
            base_ix++;
            removals_ix++;
        } else {
            SOSDigestVectorAppendOrdered(dv, base->digest[base_ix++]);
        }
    }

    if (removals_ix != removals->count) {
        SecCFCreateErrorWithFormat(1, sErrorDomain, NULL, error, NULL, CFSTR("%lu extra removals left"), removals->count - removals_ix);
        goto errOut;
    }

    while (additions_ix < additions->count) {
        if (dv->count > 0 && SOSDigestCompare(dv->digest[dv->count - 1], additions->digest[additions_ix]) >= 0) {
            SecCFCreateErrorWithFormat(1, sErrorDomain, NULL, error, NULL, CFSTR("unordered addition (%lu left)"), additions->count - additions_ix);
            goto errOut;
        }
        SOSDigestVectorAppendOrdered(dv, additions->digest[additions_ix++]);
    }

    return true;
errOut:
    return false;
}



/* SOSManifest implementation. */
struct __OpaqueSOSManifest {
};

SOSManifestRef SOSManifestCreateWithBytes(const uint8_t *bytes, size_t len,
                                          CFErrorRef *error) {
    SOSManifestRef manifest = (SOSManifestRef)CFDataCreate(NULL, bytes, (CFIndex)len);
    if (!manifest)
        SecCFCreateErrorWithFormat(kSOSManifestCreateError, sErrorDomain, NULL, error, NULL, CFSTR("Failed to create manifest"));

    return manifest;
}

SOSManifestRef SOSManifestCreateWithData(CFDataRef data, CFErrorRef *error)
{
    SOSManifestRef manifest = NULL;

    if (data)
        manifest = (SOSManifestRef)CFDataCreateCopy(kCFAllocatorDefault, data);
    else
        manifest = (SOSManifestRef)CFDataCreate(kCFAllocatorDefault, NULL, 0);

    if (!manifest)
        SecCFCreateErrorWithFormat(kSOSManifestCreateError, sErrorDomain, NULL, error, NULL, CFSTR("Failed to create manifest"));

    return manifest;
}

void SOSManifestDispose(SOSManifestRef m) {
    CFRelease(m);
}

size_t SOSManifestGetSize(SOSManifestRef m) {
    return (size_t)CFDataGetLength((CFDataRef)m);
}

size_t SOSManifestGetCount(SOSManifestRef m) {
    return SOSManifestGetSize(m) / SOSDigestSize;
}

const uint8_t *SOSManifestGetBytePtr(SOSManifestRef m) {
    return CFDataGetBytePtr((CFDataRef)m);
}

CFDataRef SOSManifestGetData(SOSManifestRef m) {
    return (CFDataRef)m;
}


bool SOSManifestDiff(SOSManifestRef a, SOSManifestRef b,
                     SOSManifestRef *a_minus_b, SOSManifestRef *b_minus_a,
                     CFErrorRef *error) {
    bool result = true;
    struct SOSDigestVector dva = SOSDigestVectorInit,
                           dvb = SOSDigestVectorInit,
                           dvab = SOSDigestVectorInit,
                           dvba = SOSDigestVectorInit;
    SOSDigestVectorAppendMultiple(&dva, SOSManifestGetCount(a), SOSManifestGetBytePtr(a));
    SOSDigestVectorAppendMultiple(&dvb, SOSManifestGetCount(b), SOSManifestGetBytePtr(b));
    SOSDigestVectorDiff(&dva, &dvb, &dvab, &dvba);
    SOSDigestVectorFree(&dva);
    SOSDigestVectorFree(&dvb);

    if (a_minus_b) {
        *a_minus_b = SOSManifestCreateWithBytes((const uint8_t *)dvab.digest, dvab.count * SOSDigestSize, error);
        if (!*a_minus_b)
            result = false;
    }

    if (b_minus_a) {
        *b_minus_a = SOSManifestCreateWithBytes((const uint8_t *)dvba.digest, dvba.count * SOSDigestSize, error);
        if (!*b_minus_a)
            result = false;
    }

    SOSDigestVectorFree(&dvab);
    SOSDigestVectorFree(&dvba);

    return result;
}

SOSManifestRef SOSManifestCreateWithPatch(SOSManifestRef base,
                                          SOSManifestRef removals,
                                          SOSManifestRef additions,
                                          CFErrorRef *error) {
    struct SOSDigestVector dvbase = SOSDigestVectorInit,
                           dvresult = SOSDigestVectorInit,
                           dvremovals = SOSDigestVectorInit,
                           dvadditions = SOSDigestVectorInit;
    dvbase.is_sorted = dvresult.is_sorted = dvremovals.is_sorted = dvadditions.is_sorted = true;
    SOSDigestVectorAppendMultiple(&dvbase, SOSManifestGetCount(base), SOSManifestGetBytePtr(base));
    SOSDigestVectorAppendMultiple(&dvremovals, SOSManifestGetCount(removals), SOSManifestGetBytePtr(removals));
    SOSDigestVectorAppendMultiple(&dvadditions, SOSManifestGetCount(additions), SOSManifestGetBytePtr(additions));
    SOSManifestRef result;
    if (SOSDigestVectorPatch(&dvbase, &dvremovals, &dvadditions, &dvresult, error)) {
        result = SOSManifestCreateWithBytes((const uint8_t *)dvresult.digest, dvresult.count * SOSDigestSize, error);
    } else {
        result = NULL;
    }
    SOSDigestVectorFree(&dvbase);
    SOSDigestVectorFree(&dvresult);
    SOSDigestVectorFree(&dvremovals);
    SOSDigestVectorFree(&dvadditions);
    return result;
}

void SOSManifestForEach(SOSManifestRef m, void(^block)(CFDataRef e)) {
    CFDataRef e;
    const uint8_t *p, *q;
    for (p = SOSManifestGetBytePtr(m), q = p + SOSManifestGetSize(m);
         p + SOSDigestSize <= q; p += SOSDigestSize) {
        e = CFDataCreateWithBytesNoCopy(0, p, SOSDigestSize, kCFAllocatorNull);
        if (e) {
            block(e);
            CFRelease(e);
        }
    }
}

CFStringRef SOSManifestCopyDescription(SOSManifestRef m) {
    CFMutableStringRef desc = CFStringCreateMutable(0, 0);
    CFStringAppend(desc, CFSTR("<Manifest"));
    SOSManifestForEach(m, ^(CFDataRef e) {
        CFStringAppend(desc, CFSTR(" "));
        const uint8_t *d = CFDataGetBytePtr(e);
        CFStringAppendFormat(desc, 0, CFSTR("%02X%02X%02X%02X"), d[0], d[1], d[2], d[3]);
    });
    CFStringAppend(desc, CFSTR(">"));

    return desc;
}

#if 0
SOSObjectRef SOSManifestGetObject(SOSManifestRef m, SOSObjectID k) {
    return NULL;
}

void SOSManifestPutObject(SOSManifestRef m, SOSObjectID k, SOSObjectRef v) {

}


SOSManifestRef SOSManifestCreateSparse(void *get_ctx, SOSManifestGetF get_f) {
    return NULL;
}
#endif
