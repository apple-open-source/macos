/*
 * Created by Michael Brouwer on 2/14/12.
 * Copyright 2012 Apple Inc. All Rights Reserved.
 */

/*!
 @header SOSTransport
 The functions provided in SOSTransport.h provide an interface to the
 secure object syncing transport
 */

#ifndef _SOSTRANSPORT_H_
#define _SOSTRANSPORT_H_

#include <corecrypto/ccsha1.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFError.h>

__BEGIN_DECLS

enum {
    kSOSManifestUnsortedError = 1,
    kSOSManifestCreateError = 2,
};


/* SOSTransport. */

/* SOSTransport protocol (not opaque). */
typedef struct SOSTransport *SOSTransportRef;

struct SOSTransport {
    bool (*send)(SOSTransportRef transport, CFDataRef message);
};

/* Return the singleton cloud transport instance. */
SOSTransportRef SOSTransportCopyCloudTransport(void);

#define SOSDigestSize ((size_t)CCSHA1_OUTPUT_SIZE)

#define SOSDigestVectorInit { .digest = NULL, .count = 0, .capacity = 0, .is_sorted = false }

struct SOSDigestVector {
    uint8_t (*digest)[SOSDigestSize];
    size_t count;
    size_t capacity;
    bool is_sorted;
};

/* SOSDigestVector. */
void SOSDigestVectorAppend(struct SOSDigestVector *dv, const uint8_t *digest);
void SOSDigestVectorSort(struct SOSDigestVector *dv);
size_t SOSDigestVectorIndexOf(struct SOSDigestVector *dv, const uint8_t *digest);
bool SOSDigestVectorContains(struct SOSDigestVector *dv, const uint8_t *digest);
void SOSDigestVectorReplaceAtIndex(struct SOSDigestVector *dv, size_t ix, const uint8_t *digest);
void SOSDigestVectorFree(struct SOSDigestVector *dv);

typedef void (*SOSDigestVectorApplyFunc)(void *context, const uint8_t digest[SOSDigestSize]);
void SOSDigestVectorApply(struct SOSDigestVector *dv,
                          void *context, SOSDigestVectorApplyFunc func);
void SOSDigestVectorDiff(struct SOSDigestVector *dv1, struct SOSDigestVector *dv2,
                         struct SOSDigestVector *dv1_2, struct SOSDigestVector *dv2_1);
bool SOSDigestVectorPatch(struct SOSDigestVector *base, struct SOSDigestVector *removals,
                          struct SOSDigestVector *additions, struct SOSDigestVector *dv,
                          CFErrorRef *error);

/* SOSObject. */

/* Forward declarations of SOS types. */
typedef struct __OpaqueSOSObjectID *SOSObjectID;
typedef struct __OpaqueSOSManifest *SOSManifestRef;

/* SOSManifest. */
SOSManifestRef SOSManifestCreateWithBytes(const uint8_t *bytes, size_t len,
                                          CFErrorRef *error);
SOSManifestRef SOSManifestCreateWithData(CFDataRef data, CFErrorRef *error);
void SOSManifestDispose(SOSManifestRef m);
size_t SOSManifestGetSize(SOSManifestRef m);
size_t SOSManifestGetCount(SOSManifestRef m);
const uint8_t *SOSManifestGetBytePtr(SOSManifestRef m);
CFDataRef SOSManifestGetData(SOSManifestRef m);
bool SOSManifestDiff(SOSManifestRef a, SOSManifestRef b,
                     SOSManifestRef *a_minus_b, SOSManifestRef *b_minus_a,
                     CFErrorRef *error);
SOSManifestRef SOSManifestCreateWithPatch(SOSManifestRef base,
                                          SOSManifestRef removals,
                                          SOSManifestRef additions,
                                          CFErrorRef *error);
void SOSManifestForEach(SOSManifestRef m, void(^block)(CFDataRef e));

CFStringRef SOSManifestCopyDescription(SOSManifestRef m);

#if 0
SOSObjectRef SOSManifestGetObject(SOSManifestRef m, SOSObjectID k);
void SOSManifestPutObject(SOSManifestRef m, SOSObjectID k, SOSObjectRef v);

typedef SOSObjectRef(*SOSManifestGetF)(void *get_ctx, SOSObjectID k);
SOSManifestRef SOSManifestCreateSparse(void *get_ctx, SOSManifestGetF get_f);
#endif

__END_DECLS

#endif /* !_SOSTRANSPORT_H_ */
