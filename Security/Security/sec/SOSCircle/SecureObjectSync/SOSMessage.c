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


/*
 * SOSMessage.c -  Creation and decoding of SOSMessage objects.
 */

#include <SecureObjectSync/SOSMessage.h>

#include <AssertMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SecureObjectSync/SOSDigestVector.h>
#include <SecureObjectSync/SOSManifest.h>
#include <SecureObjectSync/SOSInternal.h>
#include <corecrypto/ccder.h>
#include <stdlib.h>
#include <stdbool.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>
#include <utilities/der_date.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <utilities/debugging.h>
#include <utilities/iCloudKeychainTrace.h>

// TODO: This is a layer violation, we need a better way to do this
// Currently it's only used for logging.
#include <securityd/SecItemDataSource.h>

#if defined(SOSMessageFormatSpecification) && 0

-- Secure Object Syncing Peer to Peer Message format ASN.1 definition
-- Everything MUST be DER encoded unless otherwise noted.  These exceptions
-- Allow us to stream messages on a streamy network, without loading more
-- than one object into memory at once.

SOSMessage := SEQUENCE {
    CHOICE {
        v0          V0-MESSAGE-BODY-CLASS
        v2          SOSV2MessageBody
    }
}

-- v0 Message

V0-MESSAGE-BODY-CLASS ::= CLASS
    {
     &messageType    INTEGER (manifestDigest, manifest, manifestDeltaAndObjects)
     &version        INTEGER OPTIONAL default v0
     &Type
    }
    WITH SYNTAX {&Type IDENTIFIED BY &messageType}

ManifestDigest ::= OCTECT STRING (length 20)

Manifest ::= OCTECT STRING -- (length 20 * number of entries)

manifestDigestBody ::=
    { ManifestDigest IDENTIFIED BY {manifestDigest}}

manifestBody ::=
    { Manifest IDENTIFIED BY {manifest}}

 manifestDeltaAndObjectsBody ::=
    { ManifestDeltaAndObjects IDENTIFIED BY {manifestDeltaAndObjects}}

SOSV1MessageBody ::= MESSAGE-BODY-CLASS

ManifestDeltaAndObjects ::= SEQUENCE {
     manfestDigest ManifestDigest
     removals Manifest
     additions Manifest
     addedObjects SEQUENCE OF SOSObject
}

-- v2 Message

SOSMessageBody := {
    -- top level SEQUENCE may be Constructed, indefinite-length BER encoded
    header         SOSMessageHeader,
    deltas         [0] IMPLICIT SOSManifestDeltas OPTIONAL,
    extensions     [1] IMPLICIT SOSExtensions OPTIONAL,
    objects        [2] IMPLICIT SEQUENCE OF SOSObject OPTIONAL
        -- [2] IMPLICIT SEQUENCE OF SOSObject may be Constructed,
        -- indefinite-length BER encoded -- }

SOSMessageHeader ::= SEQUENCE {
    version         [0] IMPLICIT SOSMessageVersion DEFAULT v2,
    creationTime    GeneralizedTime OPTIONAL,
        -- When this message was created by the sender for tracking latency
    sequenceNumber  SOSSequenceNumber OPTIONAL,
        -- Message Sequence Number for tracking packet loss in transport
    digestTypes     SOSDigestTypes OPTIONAL,
        -- Determines the size and format of each SOSManifestDigest and the
        -- elements of each SOSManifest.
        -- We send the intersection our desired SOSDigestTypes and our peers
        -- last received SOSDigestType. If we never received a message from our
        -- peer we send our entire desired set and set the digestTypesProposed
        -- messageFlag.
        -- If the intersection is the empty set we fallback to sha1
        -- Each digest and manifest entry is constructed by appending the
        -- agreed upon digests in the order they are listed in the DER encoded
        -- digestTypes.
    messageFlags    BIT STRING {
        getObjects                          (0),
        joinRequest                         (1),
        partial                             (2),
        digestTypesProposed                 (3),
            -- This is a partial update and might not contain accurate manifest deltas (check this against spec --mb), only objects
        clearGetObjects                     (4), -- WIP mb ignore
            -- Stop sending me objects for this delta update, I will send you mine instead if you give me a full manifest delta
        didClearGetObjectsSinceLastDelta    (5)  -- WIP mb ignore
            -- clearGetObjects was set during this delta update, do not
            -- set it again (STICKY until either peer clears delta) -- }
        skipHello                           (6)  -- Respond with at least a manifest
    senderDigest    SOSManifestDigest,
        -- The senders manifest digest at the time of sending this message.
    baseDigest      [0] IMPLICIT SOSManifestDigest,
        -- What this message is based on, if it contains deltas.  If missing we assume the empty set
    proposedDigest  [1] IMPLICIT SOSManifestDigest,
        -- What the receiver should have after patching baseDigest with
        -- additions and removals -- }

SOSMessageVersion ::= INTEGER { v0(0), v2(2), v3(3) }

SOSSequenceNumber ::= INTEGER

-- Note this is not implemented in v2 it only supports sha1
SOSDigestTypes ::= SEQUENCE {
    messageFlags    BIT STRING {
        sha1(0) -- implied if SOSDigestTypes is not present
            sha224(1)
            sha256(2)
            sha384(3)
            sha512(4)
            digestAlgorithms SET OF AlgorithmIdentifier
            -- Same as AlgorithmIdentifier from X.509 -- } }

SOSManifestDeltas ::= SEQUENCE {
    removals    SOSManifest
    additions   SOSManifest }

SOSExtensions ::= SEQUENCE SIZE (1..MAX) OF SOSExtension

SOSExtension ::= SEQUENCE {
    extnID      OBJECT IDENTIFIER,
    critical    BOOLEAN DEFAULT FALSE,
    extnValue   OCTET STRING }

SOSManifest ::= OCTET STRING
    -- DER encoding is sorted and ready to merge.
    -- All SOSDigest entries in a SOSManifest /must/ be the same size
    -- As the negotiated SOSManifestEntry.  Se comment in SOSMessageBody
    -- on digestTypes

SOSManifestDigest  ::= OCTET STRING

SOSObject ::= SEQUENCE {
    [0] conflict OCTECT STRING OPTIONAL
    [1] change OCTECT STRING OPTIONAL
    object SecDictionary }

SecDictionary ::= SET of SecKVPair

SecKVPair ::= SEQUENCE {
    key UTF8String
    value Value }

SecValue ::= CHOICE {
    bool Boolean
    number INTEGER
    string UTF8String
    data OCTECT STRING
    date GENERAL TIME
    dictionary Object
    array Array }

SecArray ::= SEQUENCE of SecValue

-- For reference:
AlgorithmIdentifier ::= SEQUENCE {
algorithm	 OBJECT IDENTIFIER,
parameters	 ANY DEFINED BY algorithm OPTIONAL }
-- contains a value of the type
-- registered for use with the
-- algorithm object identifier value

#endif // defined(SOSMessageFormatSpecification) && 0


#if 0
static inline bool SecMallocOk(const void *ptr) {
    if (ptr) return true;

    return false;
}
#endif
#if 0
static void appendObjects(CFMutableStringRef desc, CFArrayRef objects) {
    __block bool needComma = false;
    CFArrayForEach(objects, ^(const void *value) {
        if (needComma)
            CFStringAppend(desc, CFSTR(","));
        else
            needComma = true;

        SecItemServerAppendItemDescription(desc, value);
    });
}
#endif



//
// MARK: SOSMessage implementation.
//

// Legacy v1 message type numbers
enum SOSMessageType {
    SOSManifestInvalidMessageType = 0,
    SOSManifestDigestMessageType = 1,
    SOSManifestMessageType = 2,
    SOSManifestDeltaAndObjectsMessageType = 3,
};

struct __OpaqueSOSMessage {
    CFRuntimeBase _base;

    CFDataRef der;
    const uint8_t *objectsDer;
    size_t objectsLen;

    CFDataRef senderDigest;
    CFDataRef baseDigest;
    CFDataRef proposedDigest;
    SOSManifestRef removals;
    SOSManifestRef additions;

    CFMutableArrayRef objects;

    SOSMessageFlags flags;
    uint64_t sequenceNumber;
    CFAbsoluteTime creationTime;
    uint64_t version;               // Message version (currently always 2)
    bool indefiniteLength;          // If set to true the top SEQUENCE and the OBJECTS SEQUENCE are written indefinite length.
};

CFGiblisWithCompareFor(SOSMessage)

static Boolean SOSMessageCompare(CFTypeRef cf1, CFTypeRef cf2) {
    SOSMessageRef M = (SOSMessageRef)cf1;
    SOSMessageRef P = (SOSMessageRef)cf2;
    if (M->flags != P->flags) return false;
    if (M->sequenceNumber != P->sequenceNumber) return false;
    if (M->creationTime != P->creationTime) return false;
    //if (!CFEqualSafe(M->der, P->der)) return false;
    if (!CFEqualSafe(M->senderDigest, P->senderDigest)) return false;
    if (!CFEqualSafe(M->baseDigest, P->baseDigest)) return false;
    if (!CFEqualSafe(M->proposedDigest, P->proposedDigest)) return false;
    if (!CFEqualSafe(M->removals, P->removals)) return false;
    if (!CFEqualSafe(M->additions, P->additions)) return false;

    // TODO Compare Objects if present.

    return true;
}

static void SOSMessageDestroy(CFTypeRef cf) {
    SOSMessageRef message = (SOSMessageRef)cf;
    CFReleaseNull(message->der);
    CFReleaseNull(message->senderDigest);
    CFReleaseNull(message->baseDigest);
    CFReleaseNull(message->proposedDigest);
    CFReleaseNull(message->additions);
    CFReleaseNull(message->removals);
    CFReleaseNull(message->objects);
}

// TODO: Remove this layer violation!
#include <securityd/SecItemServer.h>

static uint64_t SOSMessageInferType(SOSMessageRef message, CFErrorRef *error);

static CFStringRef SOSMessageCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SOSMessageRef message = (SOSMessageRef)cf;
    static const uint8_t zero[4] = {};
    const uint8_t *S = message->senderDigest ? CFDataGetBytePtr(message->senderDigest) : zero;
    const uint8_t *B = message->baseDigest ? CFDataGetBytePtr(message->baseDigest) : zero;
    const uint8_t *P = message->proposedDigest ? CFDataGetBytePtr(message->proposedDigest) : zero;
    CFDateRef creationDate = CFDateCreate(CFGetAllocator(message), message->creationTime);

    CFMutableStringRef objects = CFStringCreateMutable(kCFAllocatorDefault, 0);

    // TODO: Remove this layer violation!
    SOSDataSourceFactoryRef dsf = SecItemDataSourceFactoryGetDefault();
    SOSDataSourceRef ds = dsf->create_datasource(dsf, kSecAttrAccessibleWhenUnlocked, NULL);

    __block size_t maxEntries = 16;
    CFStringAppendFormat(objects, NULL, CFSTR("{[%zu]"), SOSMessageCountObjects(message));
    SOSMessageWithSOSObjects(message, ds, NULL, ^(SOSObjectRef object, bool *stop) {
        CFDataRef digest = SOSObjectCopyDigest(ds, object, NULL);
        const uint8_t *O = CFDataGetBytePtr(digest);
        CFStringAppendFormat(objects, NULL, CFSTR(" %02X%02X%02X%02X"), O[0],O[1],O[2],O[3]);
        CFReleaseSafe(digest);
        if (!--maxEntries) {
            CFStringAppend(objects, CFSTR("..."));
            *stop = true;
        }
    });
    CFStringAppend(objects, CFSTR("}"));

    CFStringRef desc;
    if (message->version == 0) {
        switch (SOSMessageInferType(message, NULL)) {
            case SOSManifestInvalidMessageType:
                desc = CFStringCreateWithFormat(CFGetAllocator(message), NULL, CFSTR("<MSGInvalid %"PRIu64" >"), message->sequenceNumber);
                break;
            case SOSManifestDigestMessageType:
                desc = CFStringCreateWithFormat(CFGetAllocator(message), NULL, CFSTR("<MSGDigest %"PRIu64" %02X%02X%02X%02X>"), message->sequenceNumber, S[0],S[1],S[2],S[3]);
                break;
            case SOSManifestMessageType:
                desc = CFStringCreateWithFormat(CFGetAllocator(message), NULL, CFSTR("<MSGManifest %"PRIu64" %@>"), message->sequenceNumber, message->additions);
                break;
            case SOSManifestDeltaAndObjectsMessageType:
                desc = CFStringCreateWithFormat(CFGetAllocator(message), NULL, CFSTR("<MSGObjects %"PRIu64" %02X%02X%02X%02X %@ %@ %@"),
                                                message->sequenceNumber,
                                                B[0],B[1],B[2],B[3],
                                                message->removals, message->additions,
                                                objects);
                break;
        }
    } else {
        desc = CFStringCreateWithFormat
        (CFGetAllocator(message), NULL, CFSTR("<MSG %"PRIu64" %@ %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %@ %@ %@ %s%s%s%s%s%s%s>"),
        message->sequenceNumber,
        creationDate,
        S[0],S[1],S[2],S[3],
        B[0],B[1],B[2],B[3],
        P[0],P[1],P[2],P[3],
        message->removals, message->additions,
        objects,
        (message->flags & kSOSMessageGetObjects) ? "G" : "g",
        (message->flags & kSOSMessageJoinRequest) ? "J" : "j",
        (message->flags & kSOSMessagePartial) ? "P" : "p",
        (message->flags & kSOSMessageDigestTypesProposed) ? "D" : "d",
        (message->flags & kSOSMessageClearGetObjects) ? "K" : "k",
        (message->flags & kSOSMessageDidClearGetObjectsSinceLastDelta) ? "Z" : "z",
        (message->flags & kSOSMessageSkipHello) ? "H" : "h");
    }
    CFReleaseSafe(creationDate);
    CFReleaseSafe(objects);
    return desc;
}

//
// MARK: SOSMessage encoding
//

// Create an SOSMessage ready to be encoded.
SOSMessageRef SOSMessageCreate(CFAllocatorRef allocator, uint64_t version, CFErrorRef *error) {
    SOSMessageRef message = CFTypeAllocate(SOSMessage, struct __OpaqueSOSMessage, allocator);
    message->version = version;
    return message;
}

// TODO: Remove me this is for testing only, tests should use the real thing.
SOSMessageRef SOSMessageCreateWithManifests(CFAllocatorRef allocator, SOSManifestRef sender,
                                            SOSManifestRef base, SOSManifestRef proposed,
                                            bool includeManifestDeltas, CFErrorRef *error) {
    SOSMessageRef message = SOSMessageCreate(allocator, kEngineMessageProtocolVersion, error);
    if (!SOSMessageSetManifests(message, sender, base, proposed, includeManifestDeltas, NULL, error))
        CFReleaseNull(message);
    return message;
}

bool SOSMessageSetManifests(SOSMessageRef message, SOSManifestRef sender,
                            SOSManifestRef base, SOSManifestRef proposed,
                            bool includeManifestDeltas, SOSManifestRef objectsSent,
                            CFErrorRef *error) {
    if (!message) return true;
    bool ok = true;
    // TODO: Check at v2 encoding time
    // if (!sender) return (SOSMessageRef)SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("no sender manifest specified for SOSMessage"));
    message->baseDigest = CFRetainSafe(SOSManifestGetDigest(base, NULL));
    message->proposedDigest = CFRetainSafe(SOSManifestGetDigest(proposed, NULL));
    message->senderDigest = CFRetainSafe(SOSManifestGetDigest(sender, NULL));
    if (includeManifestDeltas) {
        SOSManifestRef additions = NULL;
        ok = SOSManifestDiff(base, proposed, &message->removals, &additions, error);
        if (message->version == 0) {
            message->additions = additions;
        } else {
            message->additions = SOSManifestCreateComplement(objectsSent, additions, error);
            CFReleaseSafe(additions);
        }
    }
    return ok;
}

void SOSMessageSetFlags(SOSMessageRef message, SOSMessageFlags flags) {
    message->flags = flags;
}

// Add an extension to this message
void SOSMessageAddExtension(SOSMessageRef message, CFDataRef oid, bool isCritical, CFDataRef extension) {
    // TODO: Implement
    secerror("not implemented yet!");
}

static bool SecMessageIsObjectValid(CFDataRef object, CFErrorRef *error) {
    const uint8_t *der = CFDataGetBytePtr(object);
    const uint8_t *der_end = der + CFDataGetLength(object);
    ccder_tag tag = 0;
    size_t len = 0;
    der = ccder_decode_tag(&tag, der, der_end);
    if (!der )
        return SOSErrorCreate(kSOSErrorBadFormat, error, NULL, CFSTR("Invalid DER, no tag found"));
    if (tag == CCDER_EOL)
        return SOSErrorCreate(kSOSErrorBadFormat, error, NULL, CFSTR("Object has EOL tag"));
    der = ccder_decode_len(&len, der, der_end);
    if (!der)
        return SOSErrorCreate(kSOSErrorBadFormat, error, NULL, CFSTR("Object with tag %lu has no valid DER length"), tag);
    der += len;
     if (der_end - der)
        return SOSErrorCreate(kSOSErrorBadFormat, error, NULL, CFSTR("Object has %td trailing unused bytes"), der_end - der);
    return true;
}

bool SOSMessageAppendObject(SOSMessageRef message, CFDataRef object, CFErrorRef *error) {
    if (!SecMessageIsObjectValid(object, error)) return false;
    if (!message->objects)
        message->objects = CFArrayCreateMutableForCFTypes(CFGetAllocator(message));
    if (message->objects)
        CFArrayAppendValue(message->objects, object);
    return true;
}

static CC_NONNULL_ALL
size_t ccder_sizeof_bit_string(cc_size n, const cc_unit *s) {
    return ccder_sizeof(CCDER_BIT_STRING, ccn_sizeof(ccn_bitlen(n, s)) + 1);
}

static CC_NONNULL_ALL
uint8_t *ccder_encode_bit_string(cc_size n, const cc_unit *s, const uint8_t *der, uint8_t *der_end) {
    size_t bits = ccn_bitlen(n, s);
    size_t out_size = ccn_sizeof(bits) + 1;
    der_end = ccder_encode_body_nocopy(out_size, der, der_end);
    if (der_end)
        ccn_write_uint_padded(n, s, out_size, der_end);
    return ccder_encode_tl(CCDER_BIT_STRING, out_size, der, der_end);
}


static CC_NONNULL_ALL
size_t der_sizeof_implicit_data(ccder_tag tag, CFDataRef data) {
    if (!data)
        return 0;
    return ccder_sizeof_implicit_raw_octet_string(tag, CFDataGetLength(data));
}


static CC_NONNULL_ALL
uint8_t *der_encode_implicit_data(ccder_tag tag, CFDataRef data, const uint8_t *der, uint8_t *der_end) {
    if (!data)
        return der_end;
    return ccder_encode_implicit_raw_octet_string(tag, CFDataGetLength(data), CFDataGetBytePtr(data), der, der_end);
}

static size_t der_sizeof_message_header(SOSMessageRef message, CFErrorRef *error) {
    if (!message->senderDigest) {
        // TODO: Create Error.
        return 0;
    }
    cc_unit flags[1];
    flags[0] = (cc_unit)message->flags; // TODO Fix cast or something
    
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
        der_sizeof_generalizedtime(message->creationTime, error) +
        ccder_sizeof_uint64(message->sequenceNumber) +
        ccder_sizeof_bit_string(array_size(flags), flags) +
        der_sizeof_implicit_data(CCDER_OCTET_STRING, message->senderDigest) +
        der_sizeof_implicit_data(0 | CCDER_CONTEXT_SPECIFIC, message->baseDigest) +
        der_sizeof_implicit_data(1 | CCDER_CONTEXT_SPECIFIC, message->proposedDigest));
}

static uint8_t *der_encode_message_header(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, uint8_t *der_end) {
    if (!message->senderDigest) {
        // TODO: Create Error.
        return NULL;
    }
    cc_unit flags[1];
    flags[0] = (cc_unit)message->flags; // TODO Fix cast or something
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
        der_encode_generalizedtime(message->creationTime, error, der,
        ccder_encode_uint64(message->sequenceNumber, der,
        ccder_encode_bit_string(array_size(flags), flags, der,
        der_encode_implicit_data(CCDER_OCTET_STRING, message->senderDigest, der,
        der_encode_implicit_data(0 | CCDER_CONTEXT_SPECIFIC, message->baseDigest, der,
        der_encode_implicit_data(1 | CCDER_CONTEXT_SPECIFIC, message->proposedDigest, der, der_end)))))));
}

static size_t der_sizeof_deltas(SOSMessageRef message) {
    if (!message->additions && !message->removals) return 0;
    if (message->version == 0) {
        return ccder_sizeof(CCDER_OCTET_STRING, SOSManifestGetSize(message->removals))+
               ccder_sizeof(CCDER_OCTET_STRING, SOSManifestGetSize(message->additions));
    } else {
        return ccder_sizeof(0 | CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED,
                            ccder_sizeof(CCDER_OCTET_STRING, SOSManifestGetSize(message->removals))+
                            ccder_sizeof(CCDER_OCTET_STRING, SOSManifestGetSize(message->additions)));
    }
}

static uint8_t *der_encode_deltas(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, uint8_t *der_end) {
    if (!message->additions && !message->removals) return der_end;
    if (message->version == 0) {
        return der_encode_implicit_data(CCDER_OCTET_STRING, SOSManifestGetData(message->removals), der,
            der_encode_implicit_data(CCDER_OCTET_STRING, SOSManifestGetData(message->additions), der, der_end));
    } else {
        return ccder_encode_constructed_tl(0 | CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED, der_end, der,
            der_encode_implicit_data(CCDER_OCTET_STRING, SOSManifestGetData(message->removals), der,
            der_encode_implicit_data(CCDER_OCTET_STRING, SOSManifestGetData(message->additions), der, der_end)));
    }
}

static size_t der_sizeof_extensions(SOSMessageRef message) {
    // We don't support any yet.
    return 0;
}

static uint8_t *der_encode_extensions(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, uint8_t *der_end) {
    // We don't support any yet.
    return der_end;
}

static size_t der_sizeof_objects(SOSMessageRef message) {
    size_t len = 0;
    if (message->objects) {
        CFDataRef data;
        CFArrayForEachC(message->objects, data) {
            len += (size_t)CFDataGetLength(data);
        }
    } else if (message->version != 0)
        return 0;

    if (message->indefiniteLength)
        return len + 4;
    else
        return ccder_sizeof(2 | CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED, len);
}

static uint8_t *der_encode_objects(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, uint8_t *der_end) {
    if (!message->objects && message->version != 0) return der_end;
    const uint8_t *original_der_end = der_end;
    if (message->indefiniteLength)
        der_end = ccder_encode_tl(CCDER_EOL, 0, der, der_end);

    for (CFIndex position = (message->objects ? CFArrayGetCount(message->objects) : 0) - 1; position >= 0; --position) {
        CFDataRef object = CFArrayGetValueAtIndex(message->objects, position);
        der_end = ccder_encode_body(CFDataGetLength(object), CFDataGetBytePtr(object), der, der_end);
    }
    if (message->indefiniteLength) {
        return ccder_encode_tag(2 | CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED, der,
               ccder_encode_len(0, der, der_end));
    } else {
        ccder_tag otag = message->version == 0 ? CCDER_CONSTRUCTED_SEQUENCE : 2 | CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED;
        return ccder_encode_constructed_tl(otag, original_der_end, der, der_end);
    }
}

static size_t der_sizeof_v2_message(SOSMessageRef message, CFErrorRef *error) {
    size_t body_size = (der_sizeof_message_header(message, error) +
                        der_sizeof_deltas(message) +
                        der_sizeof_extensions(message) +
                        der_sizeof_objects(message));
    if (message->indefiniteLength)
        return body_size + 4;
    else
        return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, body_size);
}


static uint8_t *der_encode_v2_message(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, uint8_t *der_end) {
    const uint8_t *original_der_end = der_end;
    if (message->indefiniteLength)
        der_end = ccder_encode_tl(CCDER_EOL, 0, der, der_end);
    
    der_end = der_encode_message_header(message, error, der,
        der_encode_deltas(message, error, der,
        der_encode_extensions(message, error, der,
        der_encode_objects(message, error, der, der_end))));

    if (message->indefiniteLength) {
        return ccder_encode_tag(CCDER_CONSTRUCTED_SEQUENCE, der,
               ccder_encode_len(0, der, der_end));
    } else {
        return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, original_der_end, der, der_end);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------
//      V1 support
//------------------------------------------------------------------------------------------------------------------------------------

/* ManifestDigest message */
static size_t der_sizeof_manifest_digest_message(SOSMessageRef message, CFErrorRef *error) {
    if (!message->senderDigest || CFDataGetLength(message->senderDigest) != SOSDigestSize) {
        SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("digest length mismatch"));
        return 0;
    }
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                        (ccder_sizeof_uint64(SOSManifestDigestMessageType) +
                         ccder_sizeof_raw_octet_string(SOSDigestSize)));
}

static uint8_t *der_encode_manifest_digest_message(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, uint8_t *der_end) {
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
           ccder_encode_uint64(SOSManifestDigestMessageType, der,
           ccder_encode_raw_octet_string(SOSDigestSize, CFDataGetBytePtr(message->senderDigest), der, der_end)));
}

/* Manifest message */
static size_t der_sizeof_manifest_message(SOSMessageRef message, CFErrorRef *error) {
    if (!message->additions) {
        SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("no manifest for manifest message"));
        return 0;
    }
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
        (ccder_sizeof_uint64(SOSManifestMessageType) +
         der_sizeof_implicit_data(CCDER_OCTET_STRING, SOSManifestGetData(message->additions))));
}

static uint8_t *der_encode_manifest_message(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, uint8_t *der_end) {
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
               ccder_encode_uint64(SOSManifestMessageType, der,
               der_encode_implicit_data(CCDER_OCTET_STRING, SOSManifestGetData(message->additions), der, der_end)));
}

/* ManifestDeltaAndObjects message */
static size_t der_sizeof_manifest_and_objects_message(SOSMessageRef message, CFErrorRef *error) {
    if (!message->baseDigest || CFDataGetLength(message->baseDigest) != SOSDigestSize) {
        SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("digest length mismatch"));
        return 0;
    }

    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
        (ccder_sizeof_uint64(SOSManifestDeltaAndObjectsMessageType) +
         ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
             (ccder_sizeof_raw_octet_string(SOSDigestSize) +
              der_sizeof_implicit_data(CCDER_OCTET_STRING, SOSManifestGetData(message->removals)) +
              der_sizeof_implicit_data(CCDER_OCTET_STRING, SOSManifestGetData(message->additions)) +
              der_sizeof_objects(message)))));
}

static uint8_t *der_encode_manifest_and_objects_message(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, uint8_t *der_end) {
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
               ccder_encode_uint64(SOSManifestDeltaAndObjectsMessageType, der,
               ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                   ccder_encode_raw_octet_string(SOSDigestSize, CFDataGetBytePtr(message->baseDigest), der,
                   der_encode_implicit_data(CCDER_OCTET_STRING, SOSManifestGetData(message->removals), der,
                   der_encode_implicit_data(CCDER_OCTET_STRING, SOSManifestGetData(message->additions), der,
                   der_encode_objects(message, error, der, der_end)))))));
}

static uint64_t SOSMessageInferType(SOSMessageRef message, CFErrorRef *error) {
    if (message->baseDigest) {
        // TODO: Assert that we don't have senderDigest or proposedDigest
        if (SOSManifestGetCount(message->removals) || SOSManifestGetCount(message->additions) || SOSMessageCountObjects(message)) {
            return SOSManifestDeltaAndObjectsMessageType;
        } else {
            // NOTE: If we force a SOSManifestDeltaAndObjectsMessageType instead then
            // true v0 peers will overwrite their last objects message to us.  However this
            // implements the current v0 behaviour
            return SOSManifestDigestMessageType;
        }
    } else if (message->additions) {
        // TODO: Assert that we don't have senderDigest, proposedDigest, additions, removals or objects
        return SOSManifestMessageType;
    } else if (message->senderDigest) {
        // TODO: Assert that we don't have proposedDigest, removals or objects
        return SOSManifestDigestMessageType;
    }
    // TODO: Create error.
    return SOSManifestInvalidMessageType;
}

static size_t der_sizeof_message(SOSMessageRef message, uint64_t messageType, CFErrorRef *error) {
    switch (messageType) {
        case SOSManifestInvalidMessageType:
            return der_sizeof_v2_message(message, error);
        case SOSManifestDigestMessageType:
            return der_sizeof_manifest_digest_message(message, error);
        case SOSManifestMessageType:
            return der_sizeof_manifest_message(message, error);
        case SOSManifestDeltaAndObjectsMessageType:
            return der_sizeof_manifest_and_objects_message(message, error);
    }
    return 0;
}

static uint8_t *der_encode_message(SOSMessageRef message, uint64_t messageType, CFErrorRef *error, const uint8_t *der, uint8_t *der_end) {
    switch (messageType) {
        case SOSManifestInvalidMessageType:
            return der_encode_v2_message(message, error, der, der_end);
        case SOSManifestDigestMessageType:
            return der_encode_manifest_digest_message(message, error, der, der_end);
        case SOSManifestMessageType:
            return der_encode_manifest_message(message, error, der, der_end);
        case SOSManifestDeltaAndObjectsMessageType:
            return der_encode_manifest_and_objects_message(message, error, der, der_end);
    }
    return der_end;
}

// Encode an SOSMessage, calls addObject callback and appends returned objects
// one by one, until addObject returns NULL.
CFDataRef SOSMessageCreateData(SOSMessageRef message, uint64_t sequenceNumber, CFErrorRef *error) {
    // Version 2 message have sequence numbers, version 0 messages do not.
    uint64_t messageType = SOSManifestInvalidMessageType;
    message->sequenceNumber = sequenceNumber;
    if (message->version == 0) {
        message->indefiniteLength = false;
        messageType = SOSMessageInferType(message, error);
        if (!messageType) {
            // Propagate error
            return NULL;
        }
    } else {
        message->creationTime = floor(CFAbsoluteTimeGetCurrent());
    }
    size_t der_size = der_sizeof_message(message, messageType, error);
    CFMutableDataRef data = CFDataCreateMutable(NULL, der_size);
    if (data == NULL) {
        // TODO Error.
        return NULL;
    }
    CFDataSetLength(data, der_size);
    uint8_t *der_end = CFDataGetMutableBytePtr(data);
    const uint8_t *der = der_end;
    der_end += der_size;

    der_end = der_encode_message(message, messageType, error, der, der_end);
    if (der != der_end) {
        secwarning("internal error %td bytes unused in der buffer", der_end - der);
    }
    return data;
}

//
// MARK: SOSMessage decoding
//

#define CCBER_LEN_INDEFINITE ((size_t)-1)

// Decode BER length field.  Sets *lenp to ccber_indefinite_len if this is an indefinite length encoded object.
// Behaves like ccder_decode_len in every other way.
static CC_NONNULL((1, 3))
const uint8_t *ccber_decode_len(size_t *lenp, const uint8_t *der, const uint8_t *der_end) {
    if (der && der < der_end) {
        if (*der == 0x80) {
            der++;
            *lenp = CCBER_LEN_INDEFINITE;
        }
        else
            der = ccder_decode_len(lenp, der, der_end);
    }
    return der;
}

static const uint8_t *der_decode_generalizedtime(CFAbsoluteTime *at, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *times_end = NULL;
    der = ccder_decode_constructed_tl(CCDER_GENERALIZED_TIME, &times_end, der, der_end);
    der = der_decode_generalizedtime_body(at, error, der, times_end);
    if (times_end != der) {
        secwarning("internal error %td bytes unused in generalizedtime DER buffer", times_end - der);
    }
    return der;
}

static const uint8_t *der_decode_optional_generalizedtime(CFAbsoluteTime *at, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *times_end = der_decode_generalizedtime(at, error, der, der_end);
    return times_end ? times_end : der;
}

static CC_NONNULL((2, 4))
const uint8_t *ccder_decode_implicit_uint64(ccder_tag expected_tag, uint64_t* r, const uint8_t *der, const uint8_t *der_end) {
    size_t len;
    der = ccder_decode_tl(expected_tag, &len, der, der_end);
    if (der && len && (*der & 0x80) != 0x80) {
        if (!r || (ccn_read_uint(ccn_nof_size(sizeof(*r)), (cc_unit*)r, len, der) >= 0))
            return der + len;
    }
    return NULL;
}

static const uint8_t *ccder_decode_optional_implicit_uint64(ccder_tag expected_tag, uint64_t *value, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *ui64_end = ccder_decode_implicit_uint64(expected_tag, value, der, der_end);
    return ui64_end ? ui64_end : der;
}


static const uint8_t *ccder_decode_optional_uint64(uint64_t *value, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *ui64_end = ccder_decode_uint64(value, der, der_end);
    return ui64_end ? ui64_end : der;
}

static const uint8_t *ccder_decode_digest_types(SOSMessageRef message, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *dt_end;
    der = ccder_decode_sequence_tl(&dt_end, der, der_end);
    if (!der) return NULL;
    // Skip over digestType body for now.
    // TODO: Support DigestType
    return dt_end;
}

static const uint8_t *ccder_decode_optional_digest_types(SOSMessageRef message, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *dt_end = ccder_decode_digest_types(message, der, der_end);
    return dt_end ? dt_end : der;
}

static const uint8_t *ccder_decode_bit_string(cc_size n, size_t *r_bitlen, cc_unit *r, const uint8_t *der, const uint8_t *der_end) {
    size_t len;
    const uint8_t *body = ccder_decode_tl(CCDER_BIT_STRING, &len, der, der_end);
    if (!body || len < 1)
        return NULL;

    if (r_bitlen) *r_bitlen = (len - 1) * 8 - (body[0] & 7);
    ccn_read_uint(1, r, len - 1, body + 1);
    return body + len;
}

static const uint8_t *der_decode_implicit_data(ccder_tag expected_tag, CFDataRef *data, const uint8_t *der, const uint8_t *der_end) {
    size_t len = 0;
    der = ccder_decode_tl(expected_tag, &len, der, der_end);
    if (der && data) {
        *data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, der, len, kCFAllocatorNull);
        if (*data)
            der += len;
        else
            der = NULL;
    }
    return der;
}

static const uint8_t *der_decode_optional_implicit_data(ccder_tag expected_tag, CFDataRef *data, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *data_end = der_decode_implicit_data(expected_tag, data, der, der_end);
    return data_end ? data_end : der;
}

static const uint8_t *der_decode_deltas_body(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    CFDataRef removals = NULL, additions = NULL;
    der = der_decode_implicit_data(CCDER_OCTET_STRING, &removals, der, der_end);
    der = der_decode_implicit_data(CCDER_OCTET_STRING, &additions, der, der_end);
    if (der) {
        message->removals = SOSManifestCreateWithData(removals, error);
        message->additions = SOSManifestCreateWithData(additions, error);
        if (!message->removals || !message->additions) {
            CFReleaseNull(message->removals);
            CFReleaseNull(message->additions);
            der = NULL;
        }
    }
    CFReleaseSafe(removals);
    CFReleaseSafe(additions);

    return der;
}

static const uint8_t *der_decode_deltas(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *deltas_end = NULL;
    der = ccder_decode_constructed_tl(0 | CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED, &deltas_end, der, der_end);
    return der_decode_deltas_body(message, error, der, deltas_end);
}

static const uint8_t *der_decode_optional_deltas(SOSMessageRef message, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *seq_end = der_decode_deltas(message, NULL, der, der_end);
    return seq_end ? seq_end : der;
}

static const uint8_t *der_decode_extensions(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *extensions_end;
    der = ccder_decode_constructed_tl(1 | CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED, &extensions_end, der, der_end);
    if (!der) return NULL;
    // Skip over extensions for now.
    return extensions_end;
}

static const uint8_t *der_decode_optional_extensions(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *extensions_end = der_decode_extensions(message, NULL, der, der_end);
    return extensions_end ? extensions_end : der;
}

static const uint8_t *der_foreach_objects(size_t length, const uint8_t *der, const uint8_t *der_end, CFErrorRef *error, void(^withObject)(CFDataRef object, bool *stop)) {
    bool stop = false;
    ccder_tag tag;
    // Look ahead at the tag
    while (!stop && ccder_decode_tag(&tag, der, der_end) && tag != CCDER_EOL) {
        const uint8_t *object_end = NULL;
        if (!ccder_decode_constructed_tl(tag, &object_end, der, der_end)) {
            SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("failed to decode object header"));
            return NULL;
        }
        if (withObject) {
            CFDataRef object = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, der, object_end - der, kCFAllocatorNull);
            withObject(object, &stop);
            CFReleaseSafe(object);
        }
        der = object_end;
    }
    if (length == CCBER_LEN_INDEFINITE) {
        size_t len = 0;
        der = ccder_decode_tl(CCDER_EOL, &len, der, der_end);
        if (len != 0) {
            secwarning("%td length ", der_end - der);
        }
    }
    if (!stop && der != der_end)
        secwarning("%td trailing bytes after objects DER", der_end - der);

    return der;
}

static const uint8_t *der_decode_objects(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    ccder_tag tag = 0;
    size_t objects_len = 0;
    der = ccder_decode_tag(&tag, der, der_end);
    if (tag != (2 | CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED)) return NULL;
    der = ccber_decode_len(&objects_len, der, der_end);
    if (objects_len != CCBER_LEN_INDEFINITE && der_end - der != (ptrdiff_t)objects_len) {
        secwarning("%td trailing bytes after SOSMessage DER", (der_end - der) - (ptrdiff_t)objects_len);
    }
    // Remember a pointer into message->der where objects starts.
    message->objectsDer = der;
    message->objectsLen = objects_len;

    return der + objects_len;
}

static const uint8_t *der_decode_optional_objects(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    const uint8_t *seq_end = der_decode_objects(message, NULL, der, der_end);
    return seq_end ? seq_end : der;
}

#if 0
// Move to ccder and possibly refactor ccder_decode_constructed_tl to call this.
#ifdef CCDER_DECODE_CONSTRUCTED_LEN_SPECIFIER
CCDER_DECODE_CONSTRUCTED_LEN_SPECIFIER
#endif
inline CC_NONNULL((1, 3))
const uint8_t *
ccder_decode_constructed_len(const uint8_t **body_end,
                             const uint8_t *der, const uint8_t *der_end) {
    size_t len;
    der = ccder_decode_len(&len, der, der_end);
    *body_end = der + len;
    return der;
}
#endif

static const uint8_t *der_decode_message_header(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    cc_unit flags[1] = {};
    der = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &der_end, der, der_end);
    message->version = 2;
    der = ccder_decode_optional_implicit_uint64(0 | CCDER_CONTEXT_SPECIFIC, &message->version, der, der_end);
    der = der_decode_optional_generalizedtime(&message->creationTime, error, der, der_end);
    der = ccder_decode_optional_uint64(&message->sequenceNumber, der, der_end);
    der = ccder_decode_optional_digest_types(message, der, der_end);
    der = ccder_decode_bit_string(array_size(flags), NULL, flags, der, der_end);
    message->flags = flags[0];

    der = der_decode_implicit_data(CCDER_OCTET_STRING, &message->senderDigest, der, der_end);
    der = der_decode_optional_implicit_data(0 | CCDER_CONTEXT_SPECIFIC, &message->baseDigest, der, der_end);
    der = der_decode_optional_implicit_data(1 | CCDER_CONTEXT_SPECIFIC, &message->proposedDigest, der, der_end);
    return der;
}

static const uint8_t *
der_decode_manifest_and_objects_message(SOSMessageRef message,
                                        CFErrorRef *error, const uint8_t *der,
                                        const uint8_t *der_end) {
    size_t objects_len = 0;
    const uint8_t *body_end;
    der = ccder_decode_sequence_tl(&body_end, der, der_end);
    if (body_end != der_end) {
        SOSErrorCreate(kSOSEngineInvalidMessageError, error, NULL, CFSTR("Trailing garbage at end of message"));
        return NULL;
    }
    der = der_decode_implicit_data(CCDER_OCTET_STRING, &message->baseDigest, der, body_end);
    der = der_decode_deltas_body(message, error, der, body_end);
    // Remember a pointer into message->der where objects starts.
    der = message->objectsDer = ccder_decode_tl(CCDER_CONSTRUCTED_SEQUENCE, &objects_len, der, body_end);
    message->objectsLen = objects_len;

    return der ? der + objects_len : NULL;
}

static const uint8_t *der_decode_v0_message_body(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    uint64_t messageType = 0;
    der = ccder_decode_uint64(&messageType, der, der_end);
    if (der) switch (messageType) {
        case SOSManifestDigestMessageType:
        {
            der = der_decode_implicit_data(CCDER_OCTET_STRING, &message->senderDigest, der, der_end);
            break;
        }
        case SOSManifestMessageType:
        {
            CFDataRef manifestBody = NULL;
            der = der_decode_implicit_data(CCDER_OCTET_STRING, &manifestBody, der, der_end);
            if (!der) return NULL;
            if (der != der_end) {
                secwarning("%td trailing bytes after deltas DER", der_end - der);
            }
            message->additions = SOSManifestCreateWithData(manifestBody, error);
            CFReleaseSafe(manifestBody);
            break;
        }
        case SOSManifestDeltaAndObjectsMessageType:
        {
            der = der_decode_manifest_and_objects_message(message, error, der, der_end);
            break;
        }
        default:
            SOSErrorCreate(kSOSEngineInvalidMessageError, error, NULL, CFSTR("Invalid message type %llu"), messageType);
            break;
    }
    return der;
}

static const uint8_t *der_decode_message(SOSMessageRef message, CFErrorRef *error, const uint8_t *der, const uint8_t *der_end) {
    ccder_tag tag = 0;
    size_t body_len = 0;

    der = ccder_decode_tag(&tag, der, der_end);
    if (tag != CCDER_CONSTRUCTED_SEQUENCE) return NULL;
    der = ccber_decode_len(&body_len, der, der_end);
    if (der && body_len && body_len != CCBER_LEN_INDEFINITE && (der_end - der) != (ptrdiff_t)body_len) {
        secwarning("%td trailing bytes after SOSMessage DER", (der_end - der) - (ptrdiff_t)body_len);
        der_end = der + body_len;
    }

    if (ccder_decode_tag(&tag, der, der_end)) switch (tag) {
        case CCDER_INTEGER: // v0
            if (body_len == CCBER_LEN_INDEFINITE)
                der = NULL; // Not supported for v0 messages
            else
                der = der_decode_v0_message_body(message, error, der, der_end);
            break;
        case CCDER_CONSTRUCTED_SEQUENCE: //v2
            der = der_decode_message_header(message, error, der, der_end);
            der = der_decode_optional_deltas(message, der, der_end);
            der = der_decode_optional_extensions(message, error, der, der_end);
            der = der_decode_optional_objects(message, error, der, der_end);
        break;
    }
    return der;
}

// Decode a SOSMessage
SOSMessageRef SOSMessageCreateWithData(CFAllocatorRef allocator, CFDataRef derData, CFErrorRef *error) {
    if (!derData)
        return (SOSMessageRef)SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("NULL data => no SOSMessage"));
    SOSMessageRef message = CFTypeAllocate(SOSMessage, struct __OpaqueSOSMessage, allocator);
    if (!message)
        return (SOSMessageRef)SOSErrorCreate(kSOSErrorAllocationFailure, error, NULL, CFSTR("failed to alloc SOSMessage"));
    message->der = CFRetainSafe(derData);
    const uint8_t *der = CFDataGetBytePtr(derData);
    const uint8_t *der_end = der + CFDataGetLength(derData);
    der = der_decode_message(message, error, der, der_end);
    if (der != der_end) {
        if (error && !*error)
            SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("SOSMessage DER decoding failure %td bytes left"), der_end - der);
        return CFReleaseSafe(message);
    }
    return message;
}

// Read values from a decoded messgage

CFDataRef SOSMessageGetBaseDigest(SOSMessageRef message) {
    return message->baseDigest;
}

CFDataRef SOSMessageGetProposedDigest(SOSMessageRef message) {
    return message->proposedDigest;
}

CFDataRef SOSMessageGetSenderDigest(SOSMessageRef message) {
    return message->senderDigest;
}

SOSMessageFlags SOSMessageGetFlags(SOSMessageRef message) {
    return message->flags;
}

uint64_t SOSMessageGetSequenceNumber(SOSMessageRef message) {
    return message->sequenceNumber;
}

SOSManifestRef SOSMessageGetRemovals(SOSMessageRef message) {
    return message->removals;
}

SOSManifestRef SOSMessageGetAdditions(SOSMessageRef message) {
    return message->additions;
}

// Iterate though the extensions in a decoded SOSMessage.  If criticalOnly is
// true all non critical extensions are skipped.
void SOSMessageWithExtensions(SOSMessageRef message, bool criticalOnly, void(^withExtension)(CFDataRef oid, bool isCritical, CFDataRef extension, bool *stop)) {
    // TODO
}

size_t SOSMessageCountObjects(SOSMessageRef message) {
    if (message->objects)
        return CFArrayGetCount(message->objects);
    if (!message->objectsDer)
        return 0;
    const uint8_t *der = CFDataGetBytePtr(message->der);
    const uint8_t *der_end = der + CFDataGetLength(message->der);
    __block size_t count = 0;
    der_foreach_objects(message->objectsLen, message->objectsDer, der_end, NULL, ^(CFDataRef object, bool *stop){ ++count; });
    return count;
}

// Iterate though the objects in a decoded SOSMessage.
bool SOSMessageWithObjects(SOSMessageRef message, CFErrorRef *error,
                           void(^withObject)(CFDataRef object, bool *stop)) {
    if (message->objects) {
        CFDataRef object;
        CFArrayForEachC(message->objects, object) {
            bool stop = false;
            withObject(object, &stop);
            if (stop)
                break;
        }
        return true;
    }
    if (!message->objectsDer)
        return true;
    const uint8_t *der = CFDataGetBytePtr(message->der);
    const uint8_t *der_end = der + CFDataGetLength(message->der);
    return der_foreach_objects(message->objectsLen, message->objectsDer, der_end, error, withObject);
}

bool SOSMessageWithSOSObjects(SOSMessageRef message, SOSDataSourceRef dataSource, CFErrorRef *error,
                           void(^withObject)(SOSObjectRef object, bool *stop)) {
    return SOSMessageWithObjects(message, error, ^(CFDataRef object, bool *stop) {
        CFDictionaryRef plist = NULL;
        const uint8_t *der = CFDataGetBytePtr(object);
        const uint8_t *der_end = der + CFDataGetLength(object);
        der = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &plist, error, der, der_end);
        if (der) {
            SOSObjectRef peersObject = SOSObjectCreateWithPropertyList(dataSource, plist, error);
            withObject(peersObject, stop);
            CFReleaseSafe(peersObject);
        }
        CFReleaseSafe(plist);
    });
}
