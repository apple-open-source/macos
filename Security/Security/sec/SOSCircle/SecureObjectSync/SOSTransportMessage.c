#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSTransportMessage.h>
#include <SecureObjectSync/SOSKVSKeys.h>
#include <SecureObjectSync/SOSPeerCoder.h>
#include <SecureObjectSync/SOSEngine.h>
#include <utilities/SecCFWrappers.h>
#include <SOSInternal.h>
#include <SecureObjectSync/SOSAccountPriv.h>
#include <SOSCloudKeychainClient.h>

CFGiblisWithHashFor(SOSTransportMessage);

SOSTransportMessageRef SOSTransportMessageCreateForSubclass(size_t size,
                                                                   SOSAccountRef account, CFStringRef circleName,
                                                                   CFErrorRef *error)
{
    SOSTransportMessageRef tpt = CFTypeAllocateWithSpace(SOSTransportMessage, size, kCFAllocatorDefault);

    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account->factory, circleName, error);
    
    tpt->engine = CFRetainSafe(engine);
    tpt->account = CFRetainSafe(account);
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

SOSAccountRef SOSTransportMessageGetAccount(SOSTransportMessageRef transport){
    return transport->account;
}

static CFHashCode SOSTransportMessageHash(CFTypeRef obj){
    return (intptr_t) obj;
}

static Boolean SOSTransportMessageCompare(CFTypeRef lhs, CFTypeRef rhs){
    return SOSTransportMessageHash(lhs) == SOSTransportMessageHash(rhs);
}

bool SOSTransportMessageSendMessages(SOSTransportMessageRef transport, CFDictionaryRef circle_messages, CFErrorRef *error) {
    return transport->sendMessages(transport, circle_messages, error);
}

bool SOSTransportMessageFlushChanges(SOSTransportMessageRef transport, CFErrorRef *error){
    return transport->flushChanges(transport, error);
}

bool SOSTransportMessageSyncWithPeers(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef *error){
    return transport->syncWithPeers(transport, circleToPeerIDs, error);
}

bool SOSTransportMessageCleanupAfterPeerMessages(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef* error){
    return transport->cleanupAfterPeerMessages(transport, circleToPeerIDs, error);
}

CF_RETURNS_RETAINED
CFDictionaryRef SOSTransportMessageHandleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error){
    return transport->handleMessages(transport, circle_peer_messages_table, error);
}

SOSEngineRef SOSTransportMessageGetEngine(SOSTransportMessageRef transport){
    return transport->engine;
}


bool SOSTransportMessageHandlePeerMessage(SOSTransportMessageRef transport, CFStringRef peer_id, CFDataRef codedMessage, CFErrorRef *error){
    
    CFDataRef decodedMessage = NULL;
    bool result = false;
    
    SOSPeerRef peer = SOSPeerCreateWithEngine(SOSTransportMessageGetEngine(transport), peer_id);
    
    enum SOSCoderUnwrapStatus uwstatus = SOSPeerHandleCoderMessage(peer, peer_id, codedMessage, &decodedMessage, error);
    
    if (uwstatus == SOSCoderUnwrapDecoded) {
        result = SOSEngineHandleMessage(SOSTransportMessageGetEngine(transport), peer_id, decodedMessage, error);
    } else {
        result = uwstatus != SOSCoderUnwrapError;
    }
    CFReleaseNull(decodedMessage);
    CFReleaseNull(peer);
    return result;
}

bool SOSTransportMessageSendMessageIfNeeded(SOSTransportMessageRef transport, CFStringRef circle_id, CFStringRef peer_id, CFErrorRef *error) {
    
    SOSEnginePeerMessageSentBlock sent = NULL;
    CFDataRef message_to_send = NULL;
    bool ok = false;
    SOSPeerRef peer = SOSPeerCreateWithEngine(SOSTransportMessageGetEngine(transport), peer_id);
    CFDataRef coderData = SOSEngineGetCoderData(SOSTransportMessageGetEngine(transport), peer_id);
    require(coderData, fail);
    
    SOSCoderRef coder = SOSCoderCreateFromData(coderData, error);
    require(coder, fail);
    SOSPeerSetCoder(peer, coder);
    
    ok = SOSPeerCoderSendMessageIfNeeded(peer, &message_to_send, circle_id, peer_id, &sent, error);
    coder = SOSPeerGetCoder(peer);
    
    if (message_to_send)    {
        CFDictionaryRef peer_dict = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                 peer_id, message_to_send,
                                                                 NULL);
        CFDictionaryRef circle_peers = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                    circle_id, peer_dict,
                                                                    NULL);
        
        ok = ok && SOSTransportMessageSendMessages(transport, circle_peers, error);
        
        SOSPeerCoderConsume(&sent, ok);
        
        CFReleaseSafe(peer_dict);
        CFReleaseSafe(circle_peers);
    }
    
    
    Block_release(sent);
    
    
    CFReleaseSafe(message_to_send);
    
    coderData = SOSCoderCopyDER(coder, error);
    
    if(!SOSEngineSetCoderData(SOSTransportMessageGetEngine(transport), peer_id, coderData, error)){
        secerror("SOSTransportMessageSendMessageIfNeeded, Could not save peer state");
    }
    CFReleaseNull(coderData);
    
    if (coder)
        SOSCoderDispose(coder);
    
fail:
    CFReleaseNull(peer);
    return ok;
}

bool SOSTransportMessageSendMessagesIfNeeded(SOSTransportMessageRef transport, CFStringRef circle_id, CFArrayRef handledPeers, CFErrorRef *error) {
    CFArrayForEach(handledPeers, ^(const void *value) {
        if (isString(value)) {
            CFStringRef peer_id = (CFStringRef) value;
            CFErrorRef sendError = NULL;
            
            if (!SOSTransportMessageSendMessageIfNeeded(transport, circle_id, peer_id, &sendError)) {
                secerror("Error sending message in circle %@ to peer %@ (%@)", circle_id, peer_id, sendError);
            };
            
            CFReleaseNull(sendError);
        }
    });
    
    return true;
}




