/*
 * Copyright (c) 2012-2016 Apple Inc. All Rights Reserved.
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

#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSPeerCoder.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSCoder.h>
#include <Security/SecureObjectSync/SOSEngine.h>
#include <Security/SecureObjectSync/SOSDataSource.h>
#include <Security/SecureObjectSync/SOSAccountTransaction.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSPeerOTRTimer.h>

#include <Security/CKBridge/SOSCloudKeychainClient.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>
#include "SOSInternal.h"

enum SOSCoderUnwrapStatus SOSPeerHandleCoderMessage(SOSPeerRef peer, SOSCoderRef coder, CFStringRef peer_id, CFDataRef codedMessage, CFDataRef *decodedMessage, bool *forceSave, CFErrorRef *error) {
    
    enum SOSCoderUnwrapStatus result = SOSCoderUnwrapError;
    CFMutableDataRef localDecodedMessage = NULL;
    
    SOSCoderStatus coderStatus = kSOSCoderDataReturned;
    require_action_quiet(coder, xit, secerror("%@ getCoder: %@", peer_id, error ? *error : NULL));
    CFErrorRef localError = NULL;
    if (coder) { 
        coderStatus = SOSCoderUnwrap(coder, codedMessage, &localDecodedMessage, peer_id, error);
        dispatch_source_t timer = SOSPeerGetOTRTimer(peer);
        if(timer){
            secnotice("otrtimer","removing timer for peer: %@", peer);
            SOSPeerRemoveOTRTimerEntry(peer);
            dispatch_cancel(timer);
        }
        switch(coderStatus) {
            case kSOSCoderDataReturned: {
                //logRawMessage(localDecodedMessage, false, 0);
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
                result = SOSCoderUnwrapHandled;
                break;
            case kSOSCoderForceMessage:
                SOSPeerSetMustSendMessage(peer, true);
                result = SOSCoderUnwrapHandled;
                break;
            case kSOSCoderTooNew:       // We received an event from the future!
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
bool SOSPeerCoderSendMessageIfNeeded(SOSAccount* account, SOSEngineRef engine, SOSTransactionRef txn, SOSPeerRef peer, SOSCoderRef coder, CFDataRef *message_to_send, CFStringRef peer_id, CFMutableArrayRef *attributeList, SOSEnginePeerMessageSentCallback **sentCallback, CFErrorRef *error) {
    bool ok = false;
    secnotice("transport", "coder state: %@", coder);
    require_action_quiet(coder, xit, secerror("%@ getCoder: %@", peer_id, error ? *error : NULL));

    if (SOSCoderCanWrap(coder)) {
        secinfo("transport", "%@ Coder can wrap, getting message from engine", peer_id);
        CFMutableDataRef codedMessage = NULL;
        CFDataRef message = SOSEngineCreateMessage_locked(engine, txn, peer, attributeList, error, sentCallback);
        if (!message) {
            secnotice("transport", "%@ SOSEngineCreateMessage_locked failed: %@", peer_id, *error);
        } else if (CFDataGetLength(message) || SOSPeerMustSendMessage(peer)) {
            // TODO: Remove SOSPeerMustSendMessage from peer and move into coder/transport instead
            ok = message && (SOSCoderWrap(coder, message, &codedMessage, peer_id, error) == kSOSCoderDataReturned);
            if (!ok) {
                secnotice("transport", "%@ SOSCoderWrap failed: %@", peer_id, *error);
            } else {
                CFRetainAssign(*message_to_send, codedMessage);
                SOSEngineSetCodersNeedSaving(engine, true);
            }
            CFReleaseNull(codedMessage);
        } else {
            // Zero length message means we have no work to do.
            ok = true;
        }
        CFReleaseNull(message);
        
    } else {
        *message_to_send = SOSCoderCopyPendingResponse(coder);
        SOSEngineSetCodersNeedSaving(engine, true);
        secinfo("transport", "%@ negotiating, %@", peer_id, (message_to_send && *message_to_send) ? CFSTR("sending negotiation message.") : CFSTR("waiting for negotiation message."));

        SOSEnginePeerMessageSentCallback* pmsc = malloc(sizeof(SOSEnginePeerMessageSentCallback));
        memset(pmsc, 0, sizeof(SOSEnginePeerMessageSentCallback));

        pmsc->coder = CFRetainSafe(coder);
        SOSEngineMessageCallbackSetCallback(pmsc, ^(bool wasSent){
            if (wasSent) {
                SOSCoderConsumeResponse(pmsc->coder);
            }
        });

        *sentCallback = pmsc;
        ok = true;
    }
    /*if coder state is in awaiting for message, then set a timer and restart if failure*/
    if(*message_to_send != NULL && account.isInitialSyncing && !SOSPeerOTRTimerHaveReachedMaxRetryAllowance(account, (__bridge NSString*)peer_id)){
        if(SOSCoderIsCoderInAwaitingState(coder) && !SOSPeerTimerForPeerExist(peer) && SOSPeerOTRTimerHaveAnRTTAvailable(account, (__bridge NSString*)peer_id)){
            secnotice("otrtimer", "coder is in awaiting state");
            SOSPeerOTRTimerSetupAwaitingTimer(account, peer, engine, coder);
        }
        else if(!SOSCoderIsCoderInAwaitingState(coder)){
            secnotice("otrtimer", "coder not in awaiting state: %@", coder);
        }
        else if (SOSPeerTimerForPeerExist(peer)){
            secnotice("otrtimer", "timer for coder already set: %@", coder);
        }
    }
xit:
    return ok;
}
