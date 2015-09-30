#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSPeerCoder.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSCoder.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSEngine.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>
#include "SOSInternal.h"

// TODO: This could possibly move to SOSEngine?
bool SOSPeerCoderInitializeForPeer(SOSEngineRef engine, SOSFullPeerInfoRef myPeerInfo, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    __block bool ok = true;
    ok &= SOSEngineForPeerID(engine, SOSPeerInfoGetPeerID(peerInfo), error, ^(SOSPeerRef peer) {
        ok = SOSPeerEnsureCoder(peer, myPeerInfo, peerInfo, error);
    });
    return ok;
}

void SOSPeerCoderConsume(SOSEnginePeerMessageSentBlock *sent, bool ok) {
    if (*sent)
        (*sent)(ok);
}

enum SOSCoderUnwrapStatus SOSPeerHandleCoderMessage(SOSPeerRef peer, CFStringRef peer_id, CFDataRef codedMessage, CFDataRef *decodedMessage, bool *forceSave, CFErrorRef *error) {
    
    enum SOSCoderUnwrapStatus result = SOSCoderUnwrapError;
    CFMutableDataRef localDecodedMessage = NULL;
    
    SOSCoderStatus coderStatus = kSOSCoderDataReturned;
    SOSCoderRef coder = SOSPeerGetCoder(peer, error);
    require_action_quiet(coder, xit, secerror("%@ getCoder: %@", peer_id, error ? *error : NULL));
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
                SOSPeerDidConnect(peer);
                result = SOSCoderUnwrapHandled;
                *forceSave = true;
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
                secinfo("engine", "%@ engine stale event ignored", peer_id);
                break;
            case kSOSCoderTooNew:   //We received an event from the future!
                secnotice("engine", "%@ engine received a message too soon, time to restart", peer_id);
                SOSCoderReset(coder);
                if(SOSCoderStart(coder, &localError) == kSOSCoderFailure){
                    secerror("Attempt to recover coder failed to restart: %@", localError);
                }
                break;
            default:
                assert(false);
                break;
        }
        if(decodedMessage)
            *decodedMessage = CFRetainSafe(localDecodedMessage);
        CFReleaseNull(localDecodedMessage);
    }

    CFReleaseNull(localError);
xit:
    return result;
}

bool SOSPeerCoderSendMessageIfNeeded(SOSEngineRef engine, SOSPeerRef peer, CFDataRef *message_to_send, CFStringRef circle_id, CFStringRef peer_id, SOSEnginePeerMessageSentBlock *sent, CFErrorRef *error) {
    SOSCoderRef coder = SOSPeerGetCoder(peer, error);
    bool ok = false;
    require_action_quiet(coder, xit, secerror("%@ getCoder: %@", peer_id, error ? *error : NULL));

    if (SOSCoderCanWrap(coder)) {
        secinfo("transport", "%@ Coder can wrap, getting message from engine", peer_id);
        CFMutableDataRef codedMessage = NULL;
        CFDataRef message = SOSEngineCreateMessage_locked(engine, peer, error, sent);
        if (!message) {
            secnotice("transport", "%@ SOSEngineCreateMessageToSyncToPeer failed: %@", peer_id, *error);
        } else if (CFDataGetLength(message) || SOSPeerMustSendMessage(peer)) {
            // TODO: Remove SOSPeerMustSendMessage from peer and move into coder/transport instead
            ok = message && (SOSCoderWrap(coder, message, &codedMessage, peer_id, error) == kSOSCoderDataReturned);
            if (!ok) {
                secnotice("transport", "%@ SOSCoderWrap failed: %@", peer_id, *error);
            } else {
                CFRetainAssign(*message_to_send, codedMessage);
            }
            CFReleaseNull(codedMessage);
        } else {
            // Zero length message means we have no work to do.
            ok = true;
        }
        CFReleaseNull(message);
    } else {
        *message_to_send = SOSCoderCopyPendingResponse(coder);
        secinfo("transport", "%@ negotiating, %@", peer_id, message_to_send ? CFSTR("sending negotiation message.") : CFSTR("waiting for negotiation message."));
        *sent = Block_copy(^(bool wasSent){
            if (wasSent)
                SOSCoderConsumeResponse(coder);
        });
        ok = true;
    }
    
xit:
    return ok;
}
