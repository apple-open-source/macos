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

#include <stdlib.h>

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFError.h>

#include <Security/SecBasePriv.h>
#include <Security/SecOTR.h>
#include <Security/SecOTRSession.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSFullPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSCoder.h>

#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecIOFormat.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCoreCrypto.h>
#include <utilities/debugging.h>

#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>

#include <corecrypto/ccder.h>
#include <utilities/iCloudKeychainTrace.h>

#include "AssertMacros.h"

struct __OpaqueSOSCoder {
    CFRuntimeBase _base;

    CFStringRef peer_id;
    SecOTRSessionRef sessRef;
    bool waitingForDataPacket;
    CFDataRef pendingResponse;

    CFDataRef hashOfLastReceived;
    bool      lastReceivedWasOld;
};

#define lastReceived_di ccsha1_di

CFGiblisWithCompareFor(SOSCoder)

static CFStringRef SOSCoderCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SOSCoderRef coder = (SOSCoderRef)cf;
    if(coder){
        __block CFStringRef desc = NULL;
        CFDataPerformWithHexString(coder->hashOfLastReceived, ^(CFStringRef dataString) {
            desc = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<Coder %@ %@ %s%s>"),
                                            coder->sessRef,
                                            dataString,
                                            coder->waitingForDataPacket ? "W" : "w",
                                            coder->lastReceivedWasOld ? "O" : "o"
                                            );

        });
        return desc;
    }
    else
        return CFSTR("NULL");
}

static Boolean SOSCoderCompare(CFTypeRef cfA, CFTypeRef cfB) {
    SOSCoderRef coderA = (SOSCoderRef)cfA, coderB = (SOSCoderRef)cfB;
    // Use mainly to see if peerB is actually this device (peerA)
    return CFStringCompare(coderA->peer_id, coderB->peer_id, 0) == kCFCompareEqualTo;
}


static const char *SOSCoderString(SOSCoderStatus coderStatus) {
    switch (coderStatus) {
        case kSOSCoderDataReturned: return "DataReturned";
        case kSOSCoderNegotiating: return "Negotiating";
        case kSOSCoderNegotiationCompleted: return "NegotiationCompleted";
        case kSOSCoderFailure: return "Failure";
        case kSOSCoderStaleEvent: return "StaleEvent";
        case kSOSCoderTooNew: return "TooNew";
        default: return "StatusUnknown";
    }
}

/*
 static void logRawCoderMessage(const uint8_t* der, uint8_t* der_end, bool encoding)
{
#ifndef NDEBUG
    CFStringRef hexMessage = NULL;
    if (der && der_end) {
        CFIndex length = der_end - der;
        CFDataRef message = CFDataCreate(kCFAllocatorDefault, der, length);
        hexMessage = CFDataCopyHexString(message);
        secnoticeq("coder", "%s RAW [%ld] %@", encoding ? "encode" : "decode", length, hexMessage);
        CFReleaseSafe(message);
    }
    CFReleaseSafe(hexMessage);
#endif
}
*/

static CFMutableDataRef sessSerializedCreate(SOSCoderRef coder, CFErrorRef *error) {
    CFMutableDataRef otr_state = NULL;
        
    if(!coder || !coder->sessRef) {
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, 0, CFSTR("No session reference."));
        return NULL;
    }
    
    if ((otr_state = CFDataCreateMutable(NULL, 0)) == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorAllocationFailure, NULL, error, 0, CFSTR("Mutable Data allocation failed."));
        return NULL;
    }
    
    if (errSecSuccess != SecOTRSAppendSerialization(coder->sessRef, otr_state)) {
        SOSCreateErrorWithFormat(kSOSErrorEncodeFailure, NULL, error, 0, CFSTR("Append Serialization failed."));
        CFReleaseSafe(otr_state);
        return NULL;
    }
    
    return otr_state;

}

static size_t der_sizeof_optional_data(CFDataRef data) {
    return data ? der_sizeof_data(data, NULL) : 0;
}

static uint8_t* der_encode_optional_data(CFDataRef data, CFErrorRef *error, const uint8_t* der, uint8_t* der_end) {
    return data ? der_encode_data(data, error, der, der_end) : der_end;
}



static size_t SOSCoderGetDEREncodedSize(SOSCoderRef coder, CFErrorRef *error) {
    size_t encoded_size = 0;
    CFMutableDataRef otr_state = sessSerializedCreate(coder, error);

    if (otr_state) {
        size_t data_size = der_sizeof_data(otr_state, error);
        size_t waiting_size = ccder_sizeof_bool(coder->waitingForDataPacket, error);
        size_t pending_size = der_sizeof_optional_data(coder->pendingResponse);
        
        if ((data_size != 0) && (waiting_size != 0))
        {
            encoded_size = ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, data_size + waiting_size + pending_size);
        }
        CFReleaseSafe(otr_state);
    }
    return encoded_size;
}


static uint8_t* SOSCoderEncodeToDER(SOSCoderRef coder, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    if(!der_end) return NULL;
    uint8_t* result = NULL;
    CFMutableDataRef otr_state = sessSerializedCreate(coder, error);
    
    if(otr_state) {
        result = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                             der_encode_data(otr_state, error, der,
                                             ccder_encode_bool(coder->waitingForDataPacket, der,
                                             der_encode_optional_data(coder->pendingResponse, error, der, der_end))));
        CFReleaseSafe(otr_state);
    }
    return result;
}


CFDataRef SOSCoderCopyDER(SOSCoderRef coder, CFErrorRef* error) {
    CFMutableDataRef encoded = NULL;
    size_t encoded_size = SOSCoderGetDEREncodedSize(coder, error);
    
    if (encoded_size > 0) {
        encoded = CFDataCreateMutable(NULL, encoded_size);
        if (encoded) {
            CFDataSetLength(encoded, encoded_size);
            uint8_t * der = CFDataGetMutableBytePtr(encoded);
            uint8_t * der_end = der + encoded_size;
            if (!SOSCoderEncodeToDER(coder, error, der, der_end)) {
                CFReleaseNull(encoded);
                encoded = NULL;
            }
        }
    }
    return encoded;
}

static SOSCoderRef SOSCoderCreate_internal() {
    SOSCoderRef p = CFTypeAllocate(SOSCoder, struct __OpaqueSOSCoder, kCFAllocatorDefault);

    p->peer_id = NULL;
    p->sessRef = NULL;
    p->pendingResponse = NULL;
    p->waitingForDataPacket = false;

    p->hashOfLastReceived = NULL;
    p->lastReceivedWasOld = false;

    return p;

}

// 0 - Type not understood
// 1 - OCTET_STRING, just stored the data for OTR
// 2 - SEQUENCE with no version value
// 3 - SEQUENCE with version value we pull out of the CCDER_INTEGER

typedef enum coderExportFormatVersion {
    kNotUnderstood = 0,
    kCoderAsOTRDataOnly = 1,
    kCoderAsSequence = 2,
    kCoderAsVersionedSequence = 3,

    kCurrentCoderExportVersion = kCoderAsVersionedSequence
} CoderExportFormatVersion;

static uint64_t SOSCoderGetExportedVersion(const uint8_t *der, const uint8_t *der_end) {
    ccder_tag tag;
    uint64_t result = kNotUnderstood;
    require(ccder_decode_tag(&tag, der, der_end),xit);
    switch (tag) {
        case CCDER_OCTET_STRING: // TODO: this code is safe to delete?
            result = kCoderAsOTRDataOnly;
            break;

        case CCDER_CONSTRUCTED_SEQUENCE:
        {
            const uint8_t *sequence_end = NULL;
            der = ccder_decode_sequence_tl(&sequence_end, der, der_end);
            ccder_tag firstSequenceTag;
            require(ccder_decode_tag(&firstSequenceTag, der, der_end),xit);

            switch (firstSequenceTag) {
                case CCDER_OCTET_STRING:
                    result = kCoderAsSequence;
                    break;
                case CCDER_INTEGER:
                    der = ccder_decode_uint64(NULL, der, sequence_end);
                    if (der == NULL) {
                        result = kNotUnderstood;
                    } else {
                        result = kCoderAsVersionedSequence;
                    }
                    break;
            }
        }
    }
xit:
    return result;

}

SOSCoderRef SOSCoderCreateFromData(CFDataRef exportedData, CFErrorRef *error) {
    // TODO: fill in errors for all failure cases
    //require_action_quiet(coder, xit, SOSCreateError(kSOSErrorSendFailure, CFSTR("No coder for peer"), NULL, error));

    SOSCoderRef p = SOSCoderCreate_internal();
    
    const uint8_t *der = CFDataGetBytePtr(exportedData);
    const uint8_t *der_end = der + CFDataGetLength(exportedData);
    
    CFDataRef otr_data = NULL;

    switch (SOSCoderGetExportedVersion(der, der_end)) {
        case kCoderAsOTRDataOnly:
            der = der_decode_data(kCFAllocatorDefault, 0, &otr_data, error, der, der_end);
            p->waitingForDataPacket = false;
            break;

        case kCoderAsSequence:
        {
            const uint8_t *sequence_end = NULL;
            der = ccder_decode_sequence_tl(&sequence_end, der, der_end);

            require_action_quiet(sequence_end == der_end, fail, SecCFDERCreateError(kSOSErrorDecodeFailure, CFSTR("Extra data in SOS coder"), NULL, error));

            der = der_decode_data(kCFAllocatorDefault, 0, &otr_data, error, der, sequence_end);
            der = ccder_decode_bool(&p->waitingForDataPacket, der, sequence_end);
            if (der != sequence_end) { // optionally a pending response
                der = der_decode_data(kCFAllocatorDefault, 0, &p->pendingResponse, error, der, sequence_end);
            }
        }
            break;

        case kCoderAsVersionedSequence:
        {
            const uint8_t *sequence_end = NULL;
            der = ccder_decode_sequence_tl(&sequence_end, der, der_end);

            require_action_quiet(sequence_end == der_end, fail, SecCFDERCreateError(kSOSErrorDecodeFailure, CFSTR("Extra data in SOS coder"), NULL, error));

            uint64_t version;
            der = ccder_decode_uint64(&version, der, sequence_end);
            if (version != kCoderAsVersionedSequence) {
                SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("Unsupported Sequence Version: %lld"), version);
                goto fail;
            }

            der = der_decode_data(kCFAllocatorDefault, 0, &otr_data, error, der, sequence_end);
            der = ccder_decode_bool(&p->waitingForDataPacket, der, sequence_end);
            der = ccder_decode_bool(&p->lastReceivedWasOld, der, sequence_end);
            der = der_decode_data(kCFAllocatorDefault, 0, &p->hashOfLastReceived, error, der, sequence_end);
            if (der != sequence_end) { // optionally a pending response
                der = der_decode_data(kCFAllocatorDefault, 0, &p->pendingResponse, error, der, sequence_end);
            }
        }
            break;

        default:
            SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("Unsupported SOS Coder DER"));
            goto fail;
    }

    require(der, fail);
    
    p->sessRef = SecOTRSessionCreateFromData(NULL, otr_data);
    require(p->sessRef, fail);

    if (p->hashOfLastReceived == NULL)
        p->hashOfLastReceived = CFDataCreateMutableWithScratch(kCFAllocatorDefault, lastReceived_di()->output_size);

    CFReleaseSafe(otr_data);
    return p;
        
fail:
    CFReleaseNull(p);
    CFReleaseSafe(otr_data);
    return NULL;
}


SOSCoderRef SOSCoderCreate(SOSPeerInfoRef peerInfo, SOSFullPeerInfoRef myPeerInfo, CFBooleanRef useCompact, CFErrorRef *error) {
    CFAllocatorRef allocator = CFGetAllocator(peerInfo);
    
    SOSCoderRef coder = SOSCoderCreate_internal();

    CFErrorRef localError = NULL;

    SecOTRFullIdentityRef myRef = NULL;
    SecOTRPublicIdentityRef peerRef = NULL;
    SecKeyRef privateKey = NULL;
    SecKeyRef publicKey = NULL;

    if (myPeerInfo && peerInfo) {
        privateKey = SOSFullPeerInfoCopyDeviceKey(myPeerInfo, &localError);
        require_quiet(privateKey, errOut);

        myRef = SecOTRFullIdentityCreateFromSecKeyRef(allocator, privateKey, &localError);
        require_quiet(myRef, errOut);
        
        CFReleaseNull(privateKey);
    
        publicKey = SOSPeerInfoCopyPubKey(peerInfo, &localError);
        require(publicKey, errOut);
        
        peerRef = SecOTRPublicIdentityCreateFromSecKeyRef(allocator, publicKey, &localError);
        require_quiet(peerRef, errOut);
        
        if(useCompact == kCFBooleanTrue)
            coder->sessRef = SecOTRSessionCreateFromIDAndFlags(allocator, myRef, peerRef, kSecOTRUseAppleCustomMessageFormat);
        
        else
            coder->sessRef = SecOTRSessionCreateFromID(allocator, myRef, peerRef);

        require(coder->sessRef, errOut);
        
        coder->waitingForDataPacket = false;
        coder->pendingResponse = NULL;

        CFReleaseNull(publicKey);
        CFReleaseNull(privateKey);
        CFReleaseNull(myRef);
        CFReleaseNull(peerRef);
    } else {
        secnotice("coder", "NULL Coder requested, no transport security");
    }

    coder->hashOfLastReceived = CFDataCreateMutableWithScratch(kCFAllocatorDefault, lastReceived_di()->output_size);
    coder->lastReceivedWasOld = false;

    SOSCoderStart(coder, NULL);

    return coder;

errOut:
    secerror("Coder create failed: %@\n", localError ? localError : (CFTypeRef)CFSTR("No local error in SOSCoderCreate"));
    secerror("Coder create failed: %@\n", error ? *error : (CFTypeRef)CFSTR("WTF NULL?"));
    CFReleaseNull(myRef);
    CFReleaseNull(peerRef);
    CFReleaseNull(publicKey);
    CFReleaseNull(privateKey);

    CFReleaseNull(coder);
    return NULL;
}

static void SOSCoderDestroy(CFTypeRef cf)
{
    SOSCoderRef coder = (SOSCoderRef) cf;
    if (coder) {
        CFReleaseNull(coder->sessRef);
        CFReleaseNull(coder->pendingResponse);
        CFReleaseNull(coder->hashOfLastReceived);
    }
}

void SOSCoderReset(SOSCoderRef coder)
{
    SecOTRSessionReset(coder->sessRef);
    coder->waitingForDataPacket = false;
    CFReleaseNull(coder->pendingResponse);

    coder->lastReceivedWasOld = false;
    CFReleaseNull(coder->hashOfLastReceived);
    coder->hashOfLastReceived = CFDataCreateMutableWithScratch(kCFAllocatorDefault, lastReceived_di()->output_size);
}

bool SOSCoderIsFor(SOSCoderRef coder, SOSPeerInfoRef peerInfo, SOSFullPeerInfoRef myPeerInfo) {
    SecKeyRef theirPublicKey = NULL;
    SecKeyRef myPublicKey = NULL;
    bool isForThisPair = false;
    CFErrorRef localError = NULL;

    myPublicKey = SOSPeerInfoCopyPubKey(SOSFullPeerInfoGetPeerInfo(myPeerInfo), &localError);
    require(myPublicKey, errOut);

    theirPublicKey = SOSPeerInfoCopyPubKey(peerInfo, &localError);
    require(theirPublicKey, errOut);

    isForThisPair = SecOTRSIsForKeys(coder->sessRef, myPublicKey, theirPublicKey);

errOut:
    if (localError) {
        secerror("SOSCoderIsFor failed: %@\n", localError ? localError : (CFTypeRef)CFSTR("No local error in SOSCoderCreate"));
    }

    CFReleaseNull(myPublicKey);
    CFReleaseNull(theirPublicKey);
    CFReleaseNull(localError);
    return isForThisPair;
}

CFDataRef SOSCoderCopyPendingResponse(SOSCoderRef coder)
{
    return coder->pendingResponse ? CFDataCreateCopy(kCFAllocatorDefault, coder->pendingResponse) : NULL;
}

void SOSCoderConsumeResponse(SOSCoderRef coder)
{
    CFReleaseNull(coder->pendingResponse);
}

static bool SOSOTRSAppendStartPacket(SecOTRSessionRef session, CFMutableDataRef appendPacket, CFErrorRef *error) {
    OSStatus otrStatus = SecOTRSAppendStartPacket(session, appendPacket);
    if (otrStatus != errSecSuccess) {
        SOSCreateErrorWithFormat(kSOSErrorEncodeFailure, (error != NULL) ? *error : NULL, error, NULL, CFSTR("append start packet returned: %" PRIdOSStatus), otrStatus);
    }
    return otrStatus == errSecSuccess;
}

// Start OTR negotiation if we haven't already done so.
SOSCoderStatus
SOSCoderStart(SOSCoderRef coder, CFErrorRef *error) {
    CFMutableStringRef action = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringRef beginState = NULL;
    SOSCoderStatus result = kSOSCoderFailure;
    CFMutableDataRef startPacket = NULL;

    require_action_quiet(coder->sessRef, coderFailure, CFStringAppend(action, CFSTR("*** no otr session ***")));
    beginState = CFCopyDescription(coder->sessRef);
    require_action_quiet(!coder->waitingForDataPacket, negotiatingOut, CFStringAppend(action, CFSTR("waiting for peer to send first data packet")));
    require_action_quiet(!SecOTRSGetIsReadyForMessages(coder->sessRef), coderFailure, CFStringAppend(action, CFSTR("otr session ready"));
                         result = kSOSCoderDataReturned);
    require_action_quiet(SecOTRSGetIsIdle(coder->sessRef), negotiatingOut, CFStringAppend(action, CFSTR("otr negotiating already")));
    require_action_quiet(startPacket = CFDataCreateMutable(kCFAllocatorDefault, 0), coderFailure, SOSCreateError(kSOSErrorAllocationFailure, CFSTR("alloc failed"), NULL, error));
    require_quiet(SOSOTRSAppendStartPacket(coder->sessRef, startPacket, error), coderFailure);
    CFRetainAssign(coder->pendingResponse, startPacket);

negotiatingOut:
    result = kSOSCoderNegotiating;
coderFailure:
    // Uber state log
    if (result == kSOSCoderFailure && error && *error)
        CFStringAppendFormat(action, NULL, CFSTR(" %@"), *error);
    secinfo("coder", "%@ %s %@ %@ returned %s", beginState,
              SecOTRPacketTypeString(startPacket), action, coder->sessRef, SOSCoderString(result));
    CFReleaseNull(startPacket);
    CFReleaseSafe(beginState);
    CFRelease(action);

    return result;

}

SOSCoderStatus
SOSCoderResendDH(SOSCoderRef coder, CFErrorRef *error) {
    if(coder->sessRef == NULL) return kSOSCoderDataReturned;
    CFMutableDataRef startPacket = CFDataCreateMutable(kCFAllocatorDefault, 0);
    SOSCoderStatus result = kSOSCoderFailure;
    require_noerr_quiet(SecOTRSAppendRestartPacket(coder->sessRef, startPacket), exit);
    secnotice("coder", "Resending OTR Start %@", startPacket);
    CFRetainAssign(coder->pendingResponse, startPacket);
    result = kSOSCoderNegotiating;
exit:
    CFReleaseNull(startPacket);
    return result;
}


static SOSCoderStatus nullCoder(CFDataRef from, CFMutableDataRef *to) {
    *to = CFDataCreateMutableCopy(NULL, CFDataGetLength(from), from);
    return kSOSCoderDataReturned;
}

SOSCoderStatus SOSCoderUnwrap(SOSCoderRef coder, CFDataRef codedMessage, CFMutableDataRef *message,
                              CFStringRef clientId, CFErrorRef *error) {
    if(codedMessage == NULL) return kSOSCoderDataReturned;
    if(coder->sessRef == NULL) return nullCoder(codedMessage, message);
    CFMutableStringRef action = CFStringCreateMutable(kCFAllocatorDefault, 0);
    /* This should be the "normal" case.  We just use OTR to unwrap the received message. */
    SOSCoderStatus result = kSOSCoderFailure;

    CFStringRef beginState = CFCopyDescription(coder->sessRef);
    enum SecOTRSMessageKind kind = SecOTRSGetMessageKind(coder->sessRef, codedMessage);


    switch (kind) {
        case kOTRNegotiationPacket: {
            /* If we're in here we haven't completed negotiating a session.  Use SecOTRSProcessPacket() to go through
             the negotiation steps and immediately send a reply back if necessary using the sendBlock.  This
             assumes the sendBlock is still available.
             */
            CFMutableDataRef response = CFDataCreateMutable(kCFAllocatorDefault, 0);
            OSStatus ppstatus = errSecSuccess;
            if (response) {
                switch (ppstatus = SecOTRSProcessPacket(coder->sessRef, codedMessage, response)) {
                    case errSecSuccess:
                        if (CFDataGetLength(response) > 1) {
                            CFStringAppendFormat(action, NULL, CFSTR("Sending OTR Response %s"), SecOTRPacketTypeString(response));
                            CFRetainAssign(coder->pendingResponse, response);
                            result = kSOSCoderNegotiating;
                            if (SecOTRSGetIsReadyForMessages(coder->sessRef)) {
                                CFStringAppend(action, CFSTR(" begin waiting for data packet"));
                                coder->waitingForDataPacket = true;
                            }
                        } else if(!SecOTRSGetIsReadyForMessages(coder->sessRef)) {
                            CFStringAppend(action, CFSTR("stuck?"));
                            result = kSOSCoderNegotiating;
                        } else {
                            CFStringAppend(action, CFSTR("completed negotiation"));
                            result = kSOSCoderNegotiationCompleted;
                            coder->waitingForDataPacket = false;
                        }
                        break;
                    case errSecDecode:
                        CFStringAppend(action, CFSTR("resending dh"));
                        result = SOSCoderResendDH(coder, error);
                        break;
                    default:
                        SOSCreateErrorWithFormat(kSOSErrorEncodeFailure, (error != NULL) ? *error : NULL, error, NULL, CFSTR("%@ Cannot negotiate session (%ld)"), clientId, (long)ppstatus);
                        result = kSOSCoderFailure;
                        break;
                };
            } else {
                SOSCreateErrorWithFormat(kSOSErrorAllocationFailure, (error != NULL) ? *error : NULL, error, NULL, CFSTR("%@ Cannot allocate CFData"), clientId);
                result = kSOSCoderFailure;
            }
            
            CFReleaseNull(response);
            
            break;
        }

        case kOTRDataPacket:
        {
            CFDataRef previousMessageHash = coder->hashOfLastReceived;
            coder->hashOfLastReceived = CFDataCreateWithHash(kCFAllocatorDefault, lastReceived_di(), CFDataGetBytePtr(codedMessage), CFDataGetLength(codedMessage));
            bool lastWasOld = coder->lastReceivedWasOld;
            coder->lastReceivedWasOld = false;

            if(!SecOTRSGetIsReadyForMessages(coder->sessRef)) {
                CFStringAppend(action, CFSTR("not ready for data; resending DH packet"));
				SetCloudKeychainTraceValueForKey(kCloudKeychainNumberOfTimesSyncFailed, 1);
                result = SOSCoderResendDH(coder, error);
            } else {
                if (coder->waitingForDataPacket) {
                    CFStringAppend(action, CFSTR("got data packet we were waiting for "));
                    coder->waitingForDataPacket = false;
                }
                CFMutableDataRef exposed = CFDataCreateMutable(0, 0);
                OSStatus otrResult = SecOTRSVerifyAndExposeMessage(coder->sessRef, codedMessage, exposed);
                CFStringAppend(action, CFSTR("verify and expose message"));
                switch(otrResult) {
                    case errSecSuccess:
                        CFStringAppend(action, CFSTR("decoded OTR protected packet"));
                        CFTransferRetained(*message, exposed);
                        result = kSOSCoderDataReturned;
                        break;
                    case errSecOTRTooOld:
                        if (CFEqualSafe(previousMessageHash, coder->hashOfLastReceived)) {
                            CFStringAppend(action, CFSTR(" repeated"));
                            result = kSOSCoderStaleEvent;
                        } else {
                            coder->lastReceivedWasOld = true;
                            if (lastWasOld) {
                                CFStringAppend(action, CFSTR(" too old, repeated renegotiating"));
                                // Fail so we will renegotiate
                                result = kSOSCoderFailure;
                            } else {
                                CFStringAppend(action, CFSTR(" too old, forcing message"));
                                // Force message send.
                                result = kSOSCoderForceMessage;
                            }
                        }
                        break;
                    case errSecOTRIDTooNew:
                        CFStringAppend(action, CFSTR(" too new"));
                        result = kSOSCoderTooNew;
                        break;
                    default:
                        SecError(otrResult, error, CFSTR("%@ Cannot expose message: %" PRIdOSStatus), clientId, otrResult);
                        secerror("%@ Decode OTR Protected Packet: %@", clientId, error ? *error : NULL);
                        result = kSOSCoderFailure;
                        break;
                }

                CFReleaseNull(exposed);
            }
            CFReleaseNull(previousMessageHash);
        }
            break;

        default:
            secerror("%@ Unknown packet type: %@", clientId, codedMessage);
            SOSCreateError(kSOSErrorDecodeFailure, CFSTR("Unknown packet type"), (error != NULL) ? *error : NULL, error);
            result = kSOSCoderFailure;
            break;
    };

    // Uber state log
    if (result == kSOSCoderFailure && error && *error)
        CFStringAppendFormat(action, NULL, CFSTR(" %@"), *error);
    secnotice("coder", "%@ %@ %s %@ %@ returned %s", clientId, beginState,
              SecOTRPacketTypeString(codedMessage), action, coder->sessRef, SOSCoderString(result));
    CFReleaseSafe(beginState);
    CFRelease(action);

    return result;
}


SOSCoderStatus SOSCoderWrap(SOSCoderRef coder, CFDataRef message, CFMutableDataRef *codedMessage, CFStringRef clientId, CFErrorRef *error) {
    CFMutableStringRef action = CFStringCreateMutable(kCFAllocatorDefault, 0);
    SOSCoderStatus result = kSOSCoderDataReturned;
    CFStringRef beginState = NULL;
    CFMutableDataRef encoded = NULL;
    OSStatus otrStatus = 0;

    require_action_quiet(coder->sessRef, errOut,
                         CFStringAppend(action, CFSTR("*** using null coder ***"));
                         result = nullCoder(message, codedMessage));
    beginState = CFCopyDescription(coder->sessRef);
    require_action_quiet(SecOTRSGetIsReadyForMessages(coder->sessRef), errOut,
                         CFStringAppend(action, CFSTR("not ready"));
                         result = kSOSCoderNegotiating);
    require_action_quiet(!coder->waitingForDataPacket, errOut,
                         CFStringAppend(action, CFSTR("waiting for peer to send data packet first"));
                         result = kSOSCoderNegotiating);
    require_action_quiet(encoded = CFDataCreateMutable(kCFAllocatorDefault, 0), errOut,
                         SOSCreateErrorWithFormat(kSOSErrorAllocationFailure, NULL, error, NULL, CFSTR("%@ alloc failed"), clientId);
                         result = kSOSCoderFailure);
    require_noerr_action_quiet(otrStatus = SecOTRSSignAndProtectMessage(coder->sessRef, message, encoded), errOut,
                               SOSCreateErrorWithFormat(kSOSErrorEncodeFailure, (error != NULL) ? *error : NULL, error, NULL, CFSTR("%@ cannot protect message: %" PRIdOSStatus), clientId, otrStatus);
                               CFReleaseNull(encoded);
                               result = kSOSCoderFailure);
    *codedMessage = encoded;

errOut:
    // Uber state log
    if (result == kSOSCoderFailure && error && *error)
        CFStringAppendFormat(action, NULL, CFSTR(" %@"), *error);
    secinfo("coder", "%@ %@ %s %@ %@ returned %s", clientId, beginState,
              SecOTRPacketTypeString(encoded), action, coder->sessRef, SOSCoderString(result));
    CFReleaseSafe(beginState);
    CFRelease(action);

    return result;
}

bool SOSCoderCanWrap(SOSCoderRef coder) {
    return coder->sessRef && SecOTRSGetIsReadyForMessages(coder->sessRef) && !coder->waitingForDataPacket;
}
