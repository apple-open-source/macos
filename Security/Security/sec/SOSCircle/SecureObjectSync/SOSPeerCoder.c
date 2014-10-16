#include <SecureObjectSync/SOSPeer.h>
#include <SecureObjectSync/SOSPeerCoder.h>
#include <SecureObjectSync/SOSTransportMessage.h>
#include <SecureObjectSync/SOSCoder.h>
#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSEngine.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>
#include "SOSInternal.h"

static CFStringRef kSOSPeerCoderKey = CFSTR("coder");

bool SOSPeerCoderInitializeForPeer(SOSTransportMessageRef transport, SOSFullPeerInfoRef myPeerInfo, SOSPeerInfoRef peerInfo, CFErrorRef *error){
    CFErrorRef coderError = NULL;
    
    CFStringRef peer_id = SOSPeerInfoGetPeerID(peerInfo);
    SOSCoderRef coder = NULL;
    CFDataRef coderData = NULL;
    bool haveGoodCoder = false;
    CFMutableDictionaryRef peerState = SOSEngineGetPeerState(SOSTransportMessageGetEngine(transport), peer_id);
    
    if(peerState){
        coderData = CFDictionaryGetValue(peerState, kSOSPeerCoderKey);
        if(coderData){
            coder = SOSCoderCreateFromData(coderData, &coderError);
            if (!coder) {
                secerror("Found data but couldn't make coder for %@: %@", peer_id, coderError);
            }
            haveGoodCoder = coder;
            SOSCoderDispose(coder);
            coder = NULL;
        }
    }
    
    if (!haveGoodCoder) {
        secnotice("peer", "New coder for id %@.", peer_id);
        CFReleaseNull(coder);
        coder = SOSCoderCreate(peerInfo, myPeerInfo, error);
        if (coder) {
            coderData = SOSCoderCopyDER(coder, error);
            if(coderData){
                if(!(haveGoodCoder = SOSEngineSetCoderData(SOSTransportMessageGetEngine(transport), peer_id, coderData, error))){
                    secerror("SOSPeerCoderInitializeForPeer, Could not save coder data");
                }
            }
            CFReleaseNull(coderData);
            SOSCoderDispose(coder);
        } else {
            secerror("Couldn't make coder for %@", peer_id);
        }
    }
    CFReleaseNull(coderError);
    return haveGoodCoder;
}

void SOSPeerCoderConsume(SOSEnginePeerMessageSentBlock *sent, bool ok){
    if (*sent)
        (*sent)(ok);
}

enum SOSCoderUnwrapStatus SOSPeerHandleCoderMessage(SOSPeerRef peer, CFStringRef peer_id, CFDataRef codedMessage, CFDataRef *decodedMessage, CFErrorRef *error){
    
    enum SOSCoderUnwrapStatus result = SOSCoderUnwrapError;
    CFMutableDataRef localDecodedMessage = NULL;
    
    SOSCoderStatus coderStatus = kSOSCoderDataReturned;
    CFDataRef coderData = SOSEngineGetCoderData(SOSPeerGetEngine(peer), SOSPeerGetID(peer));
    require_quiet(coderData, fail);
    SOSCoderRef coder = SOSCoderCreateFromData(coderData, error);
    require(coder, fail);
    CFErrorRef localError = NULL;
    if (coder) {
        coderStatus = SOSCoderUnwrap(coder, codedMessage, &localDecodedMessage, peer_id, error);
        
        switch(coderStatus) {
            case kSOSCoderDataReturned: {
                logRawMessage(localDecodedMessage, false, 0);
                result = SOSCoderUnwrapDecoded;
                break;
            }
            case kSOSCoderNegotiating:  // Sent message already in Unwrap.
                result = SOSCoderUnwrapHandled;
                secnotice("engine", "%@ engine negotiating", peer_id);
                break;
            case kSOSCoderNegotiationCompleted:
                if(SOSEnginePeerDidConnect(SOSPeerGetEngine(peer), peer_id, error))
                    result = SOSCoderUnwrapHandled;
                secnotice("engine", "%@ engine negotiation complete", peer_id);
                break;
            case kSOSCoderFailure:      // Probably restart coder
                secnotice("engine", "%@ engine failed handling message %@", peer_id, error ? *error : NULL);
                SOSCoderReset(coder);
                if(SOSCoderStart(coder, &localError) == kSOSCoderFailure){
                    secerror("Attempt to recover coder failed to restart: %@", localError);
                }
                break;
            case kSOSCoderStaleEvent:   // We received an event we have already processed in the past.
                secnotice("engine", "%@ engine stale event ignored", peer_id);
                break;
            default:
                assert(false);
                break;
        }
        if(decodedMessage)
            *decodedMessage = CFRetainSafe(localDecodedMessage);
        CFReleaseNull(localDecodedMessage);
        
        coderData = SOSCoderCopyDER(coder, error);
        if(!SOSEngineSetCoderData(SOSPeerGetEngine(peer), peer_id, coderData, error)){
            secerror("SOSTransportMessageSendMessageIfNeeded, Could not save peer state");
        }
        CFReleaseNull(coderData);
        SOSCoderDispose(coder);
    }
    
    CFReleaseNull(localError);
fail:
    return result;
    
}

bool SOSPeerCoderSendMessageIfNeeded(SOSPeerRef peer, CFDataRef *message_to_send, CFStringRef circle_id, CFStringRef peer_id, SOSEnginePeerMessageSentBlock *sent, CFErrorRef *error){
    SOSCoderRef coder = SOSPeerGetCoder(peer);
    
    bool ok = false;
    require_quiet(coder, fail);
    
    if (SOSCoderCanWrap(coder)) {
        secnotice("transport", "%@ Coder can wrap, getting message from engine", peer_id);
        CFMutableDataRef codedMessage = NULL;
        CFDataRef message = SOSEngineCreateMessageToSyncToPeer(SOSPeerGetEngine(peer), peer_id, sent, error);
        if (!message) {
            secnotice("transport", "%@ SOSEngineCreateMessageToSyncToPeer failed: %@",peer_id, *error);
        }
        ok = message && (SOSCoderWrap(coder, message, &codedMessage, peer_id, error) == kSOSCoderDataReturned);
        if (!ok) {
            secnotice("transport", "%@ SOSCoderWrap failed: %@",peer_id, *error);
        }
        
        if (ok)
            CFRetainAssign(*message_to_send, codedMessage);
        
        CFReleaseNull(codedMessage);
        CFReleaseNull(message);
    } else {
        *message_to_send = SOSCoderCopyPendingResponse(coder);
        secnotice("transport", "%@ Negotiating, %@", peer_id, message_to_send ? CFSTR("Sending negotiation message.") : CFSTR("waiting for negotiation message."));
        *sent = Block_copy(^(bool wasSent){
            if (wasSent)
                SOSCoderConsumeResponse(coder);
        });
        ok = true;
    }
    
fail:
    return ok;
}
