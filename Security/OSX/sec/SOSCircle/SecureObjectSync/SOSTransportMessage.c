#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSPeerCoder.h>
#include <Security/SecureObjectSync/SOSEngine.h>
#include <utilities/SecCFWrappers.h>
#include <SOSInternal.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <SOSCloudKeychainClient.h>
#include <securityd/SecItemServer.h> // TODO: Remove this layer violation.

CFGiblisWithHashFor(SOSTransportMessage);

SOSTransportMessageRef SOSTransportMessageCreateForSubclass(size_t size,
                                                                   SOSAccountRef account, CFStringRef circleName,
                                                                   CFErrorRef *error)
{
    SOSTransportMessageRef tpt = CFTypeAllocateWithSpace(SOSTransportMessage, size, kCFAllocatorDefault);

    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account->factory, circleName, error);
    
    tpt->engine = CFRetainSafe(engine);
    tpt->account = CFRetainSafe(account);
    tpt->circleName = CFRetainSafe(circleName);
    return tpt;
}


static CFStringRef SOSTransportMessageCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions){
    SOSTransportMessageRef t = (SOSTransportMessageRef) aObj;
    
    return t->copyDescription ? t->copyDescription(t)
                              : CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSTransportMessage@%p\n>"), t);
}

static void SOSTransportMessageDestroy(CFTypeRef aObj){
    SOSTransportMessageRef transport = (SOSTransportMessageRef) aObj;
    
    if (transport->destroy)
        transport->destroy(transport);
    
    CFReleaseSafe(transport->account);
}

CFStringRef SOSTransportMessageGetCircleName(SOSTransportMessageRef transport){
    return transport->circleName;
}

CFIndex SOSTransportMessageGetTransportType(SOSTransportMessageRef transport, CFErrorRef *error){
    return transport->getTransportType ? transport->getTransportType(transport, error) : kUnknown;
}


SOSAccountRef SOSTransportMessageGetAccount(SOSTransportMessageRef transport){
    return transport->account;
}

static CFHashCode SOSTransportMessageHash(CFTypeRef obj){
    return (intptr_t) obj;
}

static Boolean SOSTransportMessageCompare(CFTypeRef lhs, CFTypeRef rhs){
    return SOSTransportMessageHash(lhs) == SOSTransportMessageHash(rhs);
}

bool SOSTransportMessageSendMessage(SOSTransportMessageRef transport, CFStringRef peerID, CFDataRef message, CFErrorRef *error) {
    CFDictionaryRef peerMessage = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, peerID, message, NULL);

    bool result = SOSTransportMessageSendMessages(transport, peerMessage, error);

    CFReleaseNull(peerMessage);
    return result;
}

bool SOSTransportMessageSendMessages(SOSTransportMessageRef transport, CFDictionaryRef peer_messages, CFErrorRef *error) {
    return transport->sendMessages(transport, peer_messages, error);
}

bool SOSTransportMessageFlushChanges(SOSTransportMessageRef transport, CFErrorRef *error){
    return transport->flushChanges(transport, error);
}

bool SOSTransportMessageSyncWithPeers(SOSTransportMessageRef transport, CFSetRef peers, CFErrorRef *error){
    return transport->syncWithPeers(transport, peers, error);
}

bool SOSTransportMessageCleanupAfterPeerMessages(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef* error){
    return transport->cleanupAfterPeerMessages(transport, circleToPeerIDs, error);
}

CF_RETURNS_RETAINED
CFDictionaryRef SOSTransportMessageHandleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error){
    return transport->handleMessages(transport, circle_peer_messages_table, error);
}

SOSEngineRef SOSTransportMessageGetEngine(SOSTransportMessageRef transport) {
    return transport->engine;
}

static bool SOSEngineHandleCodedMessage(SOSEngineRef engine, CFStringRef peerID, CFDataRef codedMessage, CFErrorRef*error) {
    __block bool result = true;
    __block bool somethingChanged = false;

    result &= SOSEngineWithPeerID(engine, peerID, error, ^(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *shouldSave) {
        CFDataRef decodedMessage = NULL;
        enum SOSCoderUnwrapStatus uwstatus = SOSPeerHandleCoderMessage(peer, coder, peerID, codedMessage, &decodedMessage, shouldSave, error);
        if (uwstatus == SOSCoderUnwrapDecoded) {
            SOSMessageRef message =  NULL;
            if (decodedMessage && CFDataGetLength(decodedMessage)) {
                // Only hand non empty messages to the engine, empty messages are an artifact
                // of coder startup.
                message = SOSMessageCreateWithData(kCFAllocatorDefault, decodedMessage, error);
            }
            if (message) {
                bool engineHandleMessageDoesNotGetToRollbackTransactions = true;
                result = SOSEngineHandleMessage_locked(engine, peerID, message, txn, &engineHandleMessageDoesNotGetToRollbackTransactions, &somethingChanged, error);
                CFReleaseSafe(message);
            }
        } else {
            result = uwstatus != SOSCoderUnwrapError;
        }
        CFReleaseNull(decodedMessage);
    });
    
    if (somethingChanged) {
        SecKeychainChanged();
    }

    if (result) {
        SOSCCRequestSyncWithPeer(peerID);
    }

    return result;
}

bool SOSTransportMessageHandlePeerMessage(SOSTransportMessageRef transport, CFStringRef peerID, CFDataRef codedMessage, CFErrorRef *error) {
    bool result = false;
    SOSEngineRef engine = SOSTransportMessageGetEngine(transport);
    require_quiet(SecRequirementError(engine != NULL, error, CFSTR("Missing engine")), done);

    result = SOSEngineHandleCodedMessage(engine, peerID, codedMessage, error);

done:
    return result;
}

bool SOSTransportMessageSendMessageIfNeeded(SOSTransportMessageRef transport, CFStringRef circle_id, CFStringRef peer_id, CFErrorRef *error) {
    __block bool ok = true;
    SOSEngineRef engine = SOSTransportMessageGetEngine(transport);

    ok &= SOSEngineWithPeerID(engine, peer_id, error, ^(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        // Now under engine lock do stuff
        CFDataRef message_to_send = NULL;
        SOSEnginePeerMessageSentBlock sent = NULL;
        ok = SOSPeerCoderSendMessageIfNeeded(engine, txn, peer, coder, &message_to_send, peer_id, &sent, error);
        if (message_to_send) {
            ok = ok && SOSTransportMessageSendMessage(transport, peer_id, message_to_send, error);

            SOSPeerCoderConsume(&sent, ok);
        }else{
            secnotice("transport", "no message to send to peer: %@", peer_id);
        }
        
        Block_release(sent);
        CFReleaseSafe(message_to_send);

        *forceSaveState = ok;
    });

    return ok;
}
