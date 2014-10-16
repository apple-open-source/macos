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
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSPeer.h>
#include <SecureObjectSync/SOSCoder.h>

#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecIOFormat.h>
#include <utilities/SecCFError.h>
#include <utilities/debugging.h>

#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>

#include <corecrypto/ccder.h>
#include <utilities/iCloudKeychainTrace.h>

#include "AssertMacros.h"

struct __OpaqueSOSCoder {
    CFStringRef peer_id;
    SecOTRSessionRef sessRef;
    bool waitingForDataPacket;
    CFDataRef pendingResponse;
};

static const char *SOSCoderString(SOSCoderStatus coderStatus) {
    switch (coderStatus) {
        case kSOSCoderDataReturned: return "DataReturned";
        case kSOSCoderNegotiating: return "Negotiating";
        case kSOSCoderNegotiationCompleted: return "NegotiationCompleted";
        case kSOSCoderFailure: return "Failure";
        case kSOSCoderStaleEvent: return "StaleEvent";
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

static size_t der_sizeof_bool(bool value) {
    return ccder_sizeof(CCDER_BOOLEAN, 1);
}

static uint8_t* der_encode_bool(bool value, const uint8_t *der, uint8_t *der_end) {
    uint8_t valueByte = value;
    return ccder_encode_tl(CCDER_BOOLEAN, 1, der,
              ccder_encode_body(1, &valueByte, der, der_end));
}

static const uint8_t* der_decode_bool(bool *value, const uint8_t *der, const uint8_t *der_end) {
    size_t payload_size = 0;
    
    der = ccder_decode_tl(CCDER_BOOLEAN, &payload_size, der, der_end);
    
    if (payload_size != 1) {
        der = NULL;
    }
    
    if (der != NULL) {
        *value = (*der != 0);
        der++;
    }

    return der;
}

static CFMutableDataRef sessSerialized(SOSCoderRef coder, CFErrorRef *error) {
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
    CFMutableDataRef otr_state = sessSerialized(coder, error);

    if (otr_state) {
        size_t data_size = der_sizeof_data(otr_state, error);
        size_t waiting_size = der_sizeof_bool(coder->waitingForDataPacket);
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
    CFMutableDataRef otr_state = sessSerialized(coder, error);
    
    if(otr_state) {
        result = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                             der_encode_data(otr_state, error, der,
                                             der_encode_bool(coder->waitingForDataPacket, der,
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

SOSCoderRef SOSCoderCreateFromData(CFDataRef exportedData, CFErrorRef *error) {
    
    SOSCoderRef p = calloc(1, sizeof(struct __OpaqueSOSCoder));
    
    const uint8_t *der = CFDataGetBytePtr(exportedData);
    const uint8_t *der_end = der + CFDataGetLength(exportedData);
    
    CFDataRef otr_data = NULL;

    ccder_tag tag;
    require(ccder_decode_tag(&tag, der, der_end),fail);

    switch (tag) {
        case CCDER_OCTET_STRING: // TODO: this code is safe to delete?
        {
            der = der_decode_data(kCFAllocatorDefault, 0, &otr_data, error, der, der_end);
            p->waitingForDataPacket = false;
        }
        break;
        
        case CCDER_CONSTRUCTED_SEQUENCE:
        {
            const uint8_t *sequence_end = NULL;
            der = ccder_decode_sequence_tl(&sequence_end, der, der_end);
            
            require_action_quiet(sequence_end == der_end, fail, SecCFDERCreateError(kSOSErrorDecodeFailure, CFSTR("Extra data in SOS coder"), NULL, error));
            
            der = der_decode_data(kCFAllocatorDefault, 0, &otr_data, error, der, sequence_end);
            der = der_decode_bool(&p->waitingForDataPacket, der, sequence_end);
            if (der != sequence_end) { // optionally a pending response
                der = der_decode_data(kCFAllocatorDefault, 0, &p->pendingResponse, error, der, sequence_end);
            }
        }
        break;
        
        default:
            SecCFDERCreateError(kSOSErrorDecodeFailure, CFSTR("Unsupported SOS Coder DER"), NULL, error);
            goto fail;
    }

    require(der, fail);
    
    p->sessRef = SecOTRSessionCreateFromData(NULL, otr_data);
    require(p->sessRef, fail);

    CFReleaseSafe(otr_data);
    return p;
        
fail:
    SOSCoderDispose(p);
    CFReleaseSafe(otr_data);
    return NULL;
}


SOSCoderRef SOSCoderCreate(SOSPeerInfoRef peerInfo, SOSFullPeerInfoRef myPeerInfo, CFErrorRef *error) {        
    CFAllocatorRef allocator = CFGetAllocator(peerInfo);
    
    SOSCoderRef coder = calloc(1, sizeof(struct __OpaqueSOSCoder));
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
    
        publicKey = SOSPeerInfoCopyPubKey(peerInfo);
        
        peerRef = SecOTRPublicIdentityCreateFromSecKeyRef(allocator, publicKey, &localError);
        require_quiet(peerRef, errOut);
        
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

    SOSCoderStart(coder, NULL);

    return coder;

errOut:
    secerror("Coder create failed: %@\n", localError ? localError : (CFTypeRef)CFSTR("No local error in SOSCoderCreate"));
    secerror("Coder create failed: %@\n", error ? *error : (CFTypeRef)CFSTR("WTF NULL?"));
    CFReleaseNull(myRef);
    CFReleaseNull(peerRef);
    CFReleaseNull(publicKey);
    CFReleaseNull(privateKey);

    free(coder);
    return NULL;
}

void SOSCoderDispose(SOSCoderRef coder)
{
    if (coder) {
        CFReleaseNull(coder->sessRef);
        CFReleaseNull(coder->pendingResponse);
        CFReleaseNull(coder->peer_id);
        free(coder);
    }
    coder = NULL;
}

void SOSCoderReset(SOSCoderRef coder)
{
    SecOTRSessionReset(coder->sessRef);
    coder->waitingForDataPacket = false;
    CFReleaseNull(coder->pendingResponse);
}

CFDataRef SOSCoderCopyPendingResponse(SOSCoderRef coder)
{
    return CFRetainSafe(coder->pendingResponse);
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
    secnotice("coder", "%@ %s %@ %@ returned %s", beginState,
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
            if(!SecOTRSGetIsReadyForMessages(coder->sessRef)) {
                CFStringAppend(action, CFSTR("not ready, resending DH packet"));
				SetCloudKeychainTraceValueForKey(kCloudKeychainNumberOfTimesSyncFailed, 1);
                CFStringAppend(action, CFSTR("not ready for data; resending dh"));
                result = SOSCoderResendDH(coder, error);
            } else {
                if (coder->waitingForDataPacket) {
                    CFStringAppend(action, CFSTR("got data packet we were waiting for "));
                    coder->waitingForDataPacket = false;
                }
                CFMutableDataRef exposed = CFDataCreateMutable(0, 0);
                OSStatus otrResult = SecOTRSVerifyAndExposeMessage(coder->sessRef, codedMessage, exposed);
                CFStringAppend(action, CFSTR("verify and expose message"));
                if (otrResult) {
                    if (otrResult == errSecOTRTooOld) {
                        CFStringAppend(action, CFSTR(" too old"));
                        result = kSOSCoderStaleEvent;
                    } else {
                        SecError(otrResult, error, CFSTR("%@ Cannot expose message: %" PRIdOSStatus), clientId, otrResult);
                        secerror("%@ Decode OTR Protected Packet: %@", clientId, error ? *error : NULL);
                        result = kSOSCoderFailure;
                    }
                } else {
                    CFStringAppend(action, CFSTR("decoded OTR protected packet"));
                    *message = exposed;
                    exposed = NULL;
                    result = kSOSCoderDataReturned;
                }
                CFReleaseNull(exposed);
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
    secnotice("coder", "%@ %@ %s %@ %@ returned %s", clientId, beginState,
              SecOTRPacketTypeString(encoded), action, coder->sessRef, SOSCoderString(result));
    CFReleaseSafe(beginState);
    CFRelease(action);

    return result;
}

bool SOSCoderCanWrap(SOSCoderRef coder) {
    return coder->sessRef && SecOTRSGetIsReadyForMessages(coder->sessRef) && !coder->waitingForDataPacket;
}
