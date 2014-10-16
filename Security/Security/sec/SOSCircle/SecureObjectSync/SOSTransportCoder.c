
#include <SecureObjectSync/SOSPeer.h>
#include <SecureObjectSync/SOSTransportCoder.h>
#include <SecureObjectSync/SOSTransportMessage.h>
#include <SecureObjectSync/SOSCoder.h>
#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSAccountPriv.h>
#include <SecureObjectSync/SOSEngine.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>
#include <SOSInternal.h>


// For now transport (the abstract class) consumes the Transport data in engine to hold
// coder state.
static SOSCoderRef SOSTransportMessageCopyPeerCoder(SOSTransportMessageRef transport, CFStringRef peer_id){
    SOSCoderRef coder = NULL;
    
    CFDataRef coderData = SOSEngineGetTransportData(SOSTransportMessageGetEngine(transport), peer_id);
    
    if (coderData) {
        CFErrorRef localError = NULL;
        coder = SOSCoderCreateFromData(coderData, &localError);
        
        if (!coder) {
            secerror("Failed to make coder from valid data for peer %@ (%@). THIS IS FATAL, WE CAN'T COMMUNICATE.", peer_id, localError);
        }
        
        CFReleaseNull(localError);
    }
    else
        secerror("Failed to get coderData from engine for peer %@. THIS IS FATAL, WE CAN'T COMMUNICATE.", peer_id);
    
    return coder;
}

bool SOSTransportMessageSavePeerCoderData(SOSTransportMessageRef transport, SOSCoderRef coder, CFStringRef peer_id, CFErrorRef *error) {
    CFDataRef coderData = NULL;
    bool ok = true;
    
    if (coder) {
        coderData = SOSCoderCopyDER(coder, error);
        if (coderData == NULL) {
            secerror("%@ coder data failed to export %@, zapping data", transport, error ? *error : 0);
        }
    }
    require_action_quiet(coderData, exit, ok = SOSErrorCreate(kSOSErrorAllocationFailure, error, NULL, CFSTR("Creation of coder data failed")));
    
    ok = SOSEngineSetTransportData(SOSTransportMessageGetEngine(transport), peer_id, coderData, error);
    
exit:
    CFReleaseNull(coderData);
    return ok;
}

bool SOSTransportCoderInitializeForPeer(SOSTransportMessageRef transport, SOSFullPeerInfoRef myPeerInfo, SOSPeerInfoRef peerInfo, CFErrorRef *error){
    SOSCoderRef coder = NULL;
    CFStringRef peer_id = SOSPeerInfoGetPeerID(peerInfo);
    CFDataRef coderData = SOSEngineGetTransportData(SOSTransportMessageGetEngine(transport), peer_id);
    if(coderData != NULL) {
        CFErrorRef coderError = NULL;
        coder = SOSCoderCreateFromData(coderData, &coderError);
        
        if (!coder) {
            secerror("Found data but couldn't make coder for %@: %@", peer_id, coderError);
        }
        CFReleaseNull(coderError);
    }
    
    bool haveGoodCoder = coder;
    if (!haveGoodCoder) {
        secnotice("transport", "New coder for id %@.", peer_id);
        coder = SOSCoderCreate(peerInfo, myPeerInfo, error);
        
        if (coder) {
            haveGoodCoder = SOSTransportMessageSavePeerCoderData(transport, coder, peer_id, error);
        } else {
            secerror("Couldn't make coder for %@", peer_id);
        }
    }
    
    if (coder)
        SOSCoderDispose(coder);
    return haveGoodCoder;
}

enum SOSCoderUnwrapStatus SOSTransportMessageHandleCoderMessage(SOSTransportMessageRef transport, CFStringRef peer_id, CFDataRef codedMessage, CFDataRef *decodedMessage, CFErrorRef *error){
    
    enum SOSCoderUnwrapStatus result = SOSCoderUnwrapError;
    CFMutableDataRef localDecodedMessage = NULL;
    
    SOSCoderStatus coderStatus = kSOSCoderDataReturned;
    SOSCoderRef coder = SOSTransportMessageCopyPeerCoder(transport, peer_id);
    if(!coder){
        SOSAccountEnsurePeerRegistration(SOSTransportMessageGetAccount(transport), error);
        coder = SOSTransportMessageCopyPeerCoder(transport, peer_id);
        secnotice("transport", "Building new coder!");
    }
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
                secnotice("transport", "%@ transport negotiating", peer_id);
                break;
            case kSOSCoderNegotiationCompleted:
                if(SOSEnginePeerDidConnect(SOSTransportMessageGetEngine(transport), peer_id, error))
                    result = SOSCoderUnwrapHandled;
                secnotice("transport", "%@ transport negotiation complete", peer_id);
                break;
            case kSOSCoderFailure:      // Probably restart coder
                secnotice("transport", "%@ transport failed handling message %@", peer_id, error ? *error : NULL);
                SOSCoderReset(coder);
                if(SOSCoderStart(coder, &localError) == kSOSCoderFailure){
                    secerror("Attempt to recover coder failed to restart: %@", localError);
                }
                break;
            case kSOSCoderStaleEvent:   // We received an event we have already processed in the past.
                secnotice("transport", "%@ transport stale event ignored", peer_id);
                break;
            default:
                assert(false);
                break;
        }
        if(decodedMessage)
            *decodedMessage = CFRetainSafe(localDecodedMessage);
        CFReleaseNull(localDecodedMessage);
        
        SOSTransportMessageSavePeerCoderData(transport, coder, peer_id, NULL);
        SOSCoderDispose(coder);
    }
    else{
        secerror("SOSTransportMessageHandleCoderMessage: Could not make a new coder!");
    }
    
    CFReleaseNull(localError);
    
    return result;

}

#warning this should be SOSTransportMessage and be split up into coder/message pieces
/* Send a message to peer if needed.  Return false if there was an error, true otherwise. */
bool SOSTransportMessageSendMessageIfNeeded(SOSTransportMessageRef transport, CFStringRef circle_id, CFStringRef peer_id, CFErrorRef *error) {
    SOSCoderRef coder = SOSTransportMessageCopyPeerCoder(transport, peer_id);
    
    if(!coder){
        SOSAccountEnsurePeerRegistration(SOSTransportMessageGetAccount(transport), error);
        coder = SOSTransportMessageCopyPeerCoder(transport, peer_id);
    }
    CFDataRef message_to_send = NULL;
    bool ok = false;
    SOSEnginePeerMessageSentBlock sent = NULL;

    require_action_quiet(coder, fail, SOSCreateError(kSOSErrorAllocationFailure, CFSTR("SOSTransportMessageCopyPeerCoder failed"), *error, error));

    if (SOSCoderCanWrap(coder)) {
        secnotice("transport", "%@ Coder can wrap, getting message from engine", peer_id);
        CFMutableDataRef codedMessage = NULL;
        CFDataRef message = SOSEngineCreateMessageToSyncToPeer(SOSTransportMessageGetEngine(transport), peer_id, &sent, error);
        if (!message) {
            secnotice("transport", "%@ SOSEngineCreateMessageToSyncToPeer failed: %@",peer_id, *error);
        }
        ok = message && (SOSCoderWrap(coder, message, &codedMessage, peer_id, error) == kSOSCoderDataReturned);
        if (!ok) {
            secnotice("transport", "%@ SOSCoderWrap failed: %@",peer_id, *error);
        }
        
        if (ok)
            CFRetainAssign(message_to_send, codedMessage);
        
        CFReleaseNull(codedMessage);
        CFReleaseNull(message);
    } else {
        message_to_send = SOSCoderCopyPendingResponse(coder);
        secnotice("transport", "%@ Negotiating, %@", peer_id, message_to_send ? CFSTR("Sending negotiation message.") : CFSTR("waiting for negotiation message."));
        sent = Block_copy(^(bool wasSent){
            if (wasSent)
                SOSCoderConsumeResponse(coder);
        });
        ok = true;
    }
    
    if (message_to_send)    {
        CFDictionaryRef peer_dict = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                 peer_id, message_to_send,
                                                                 NULL);
        CFDictionaryRef circle_peers = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                    circle_id, peer_dict,
                                                                    NULL);
        
        ok = ok && SOSTransportMessageSendMessages(transport, circle_peers, error);
        
        if (sent)
            sent(ok);
        
        CFReleaseSafe(peer_dict);
        CFReleaseSafe(circle_peers);
    }
    
    Block_release(sent);
    
    CFReleaseSafe(message_to_send);
    
    SOSTransportMessageSavePeerCoderData(transport, coder, peer_id, NULL);

fail:
    if (coder)
        SOSCoderDispose(coder);
    return ok;
}
