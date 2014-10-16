#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSTransportMessage.h>
#include <SecureObjectSync/SOSTransportMessageKVS.h>
#include <SecureObjectSync/SOSKVSKeys.h>

#include <utilities/SecCFWrappers.h>
#include <SOSInternal.h>
#include <AssertMacros.h>
#include <SOSCloudKeychainClient.h>

struct __OpaqueSOSTransportMessageKVS {
    struct __OpaqueSOSTransportMessage          m;
    
    CFStringRef             circleName;
    CFMutableDictionaryRef  pending_changes;
    
};

//
// V-table implementation forward declarations
//
static bool sendToPeer(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef peerID, CFDataRef message, CFErrorRef *error);
static bool syncWithPeers(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef *error);
static bool sendMessages(SOSTransportMessageRef transport, CFDictionaryRef circleToPeersToMessage, CFErrorRef *error);
static bool cleanupAfterPeer(SOSTransportMessageRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error);
static void destroy(SOSTransportMessageRef transport);
static CF_RETURNS_RETAINED
CFDictionaryRef handleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);
    
static bool flushChanges(SOSTransportMessageRef transport, CFErrorRef *error);


CFStringRef SOSTransportMessageKVSGetCircleName(SOSTransportMessageKVSRef transport){
    return transport->circleName;
}

SOSTransportMessageKVSRef SOSTransportMessageKVSCreate(SOSAccountRef account, CFStringRef circleName, CFErrorRef *error){
    SOSTransportMessageKVSRef tkvs = (SOSTransportMessageKVSRef) SOSTransportMessageCreateForSubclass(sizeof(struct __OpaqueSOSTransportMessageKVS) - sizeof(CFRuntimeBase), account, circleName, error);
    
    if (tkvs) {
        // Fill in vtable:
        tkvs->m.sendMessages = sendMessages;
        tkvs->m.syncWithPeers = syncWithPeers;
        tkvs->m.flushChanges = flushChanges;
        tkvs->m.cleanupAfterPeerMessages = cleanupAfterPeer;
        tkvs->m.destroy = destroy;
        tkvs->m.handleMessages = handleMessages;
        // Initialize ourselves
        tkvs->pending_changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        tkvs->circleName = CFRetainSafe(circleName);
        SOSRegisterTransportMessage((SOSTransportMessageRef)tkvs);
    }
    
    return tkvs;
}
bool SOSTransportMessageKVSAppendKeyInterest(SOSTransportMessageKVSRef transport, CFMutableArrayRef alwaysKeys, CFMutableArrayRef afterFirstUnlockKeys, CFMutableArrayRef unlockedKeys, CFErrorRef *localError){
    SOSEngineRef engine = SOSTransportMessageGetEngine((SOSTransportMessageRef)transport);
    require_quiet(engine, fail);
    CFArrayRef peer_ids = SOSEngineGetPeerIDs(engine);
    if(peer_ids){
        CFArrayForEach(peer_ids, ^(const void *value) {
            CFStringRef peerMessage = SOSMessageKeyCreateFromPeerToTransport(transport, value);
            CFArrayAppendValue(unlockedKeys, peerMessage);
            CFReleaseNull(peerMessage);
        });
    }
    return true;
fail:
    return false;
}
static void destroy(SOSTransportMessageRef transport){
    SOSTransportMessageKVSRef tkvs = (SOSTransportMessageKVSRef)transport;
    CFReleaseNull(tkvs->circleName);
    CFReleaseNull(tkvs->pending_changes);
    SOSUnregisterTransportMessage((SOSTransportMessageRef)tkvs);
    
}

static bool SOSTransportMessageKVSUpdateKVS(SOSTransportMessageKVSRef transport, CFDictionaryRef changes, CFErrorRef *error){
    CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef error) {
        if (error) {
            secerror("Error putting: %@", error);
            CFReleaseSafe(error);
        }
    };
    
    SOSCloudKeychainPutObjectsInCloud(changes, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), log_error);
    return true;
}

static bool SOSTransportMessageKVSSendPendingChanges(SOSTransportMessageKVSRef transport, CFErrorRef *error) {
    CFErrorRef changeError = NULL;
    
    if (transport->pending_changes == NULL || CFDictionaryGetCount(transport->pending_changes) == 0) {
        CFReleaseNull(transport->pending_changes);
        return true;
    }
    
    bool success = SOSTransportMessageKVSUpdateKVS(transport, transport->pending_changes, &changeError);
    if (success) {
        CFDictionaryRemoveAllValues(transport->pending_changes);
    } else {
        SOSCreateErrorWithFormat(kSOSErrorSendFailure, changeError, error, NULL,
                                 CFSTR("Send changes block failed [%@]"), transport->pending_changes);
    }
    
    return success;
}

static void SOSTransportMessageKVSAddToPendingChanges(SOSTransportMessageKVSRef transport, CFStringRef message_key, CFDataRef message_data){
    if (transport->pending_changes == NULL) {
        transport->pending_changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    }
    if (message_data == NULL) {
        CFDictionarySetValue(transport->pending_changes, message_key, kCFNull);
    } else {
        CFDictionarySetValue(transport->pending_changes, message_key, message_data);
    }
}

static bool SOSTransportMessageKVSCleanupAfterPeerMessages(SOSTransportMessageKVSRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error)
{
    CFArrayRef enginePeers = SOSEngineGetPeerIDs(SOSTransportMessageGetEngine((SOSTransportMessageRef)transport));
    
    CFDictionaryForEach(circle_to_peer_ids, ^(const void *key, const void *value) {
        if (isString(key) && isArray(value)) {
            CFStringRef circle_name = (CFStringRef) key;
            CFArrayRef peers_to_cleanup_after = (CFArrayRef) value;
            
            CFArrayForEach(peers_to_cleanup_after, ^(const void *value) {
                if (isString(value)) {
                    CFStringRef cleanup_id = (CFStringRef) value;
                    // TODO: Since the enginePeers list is not authorative (the Account is) this could inadvertently clean up active peers or leave behind stale peers
                    if (enginePeers) CFArrayForEach(enginePeers, ^(const void *value) {
                        if (isString(value)) {
                            CFStringRef in_circle_id = (CFStringRef) value;
                            
                            CFStringRef kvsKey = SOSMessageKeyCreateWithCircleNameAndPeerNames(circle_name, cleanup_id, in_circle_id);
                            SOSTransportMessageKVSAddToPendingChanges(transport, kvsKey, NULL);
                            CFReleaseSafe(kvsKey);
                            
                            kvsKey = SOSMessageKeyCreateWithCircleNameAndPeerNames(circle_name, in_circle_id, cleanup_id);
                            SOSTransportMessageKVSAddToPendingChanges(transport, kvsKey, NULL);
                            CFReleaseSafe(kvsKey);
                        }
                    });
                    
                }
            });
        }
    });
    
    return SOSTransportMessageFlushChanges((SOSTransportMessageRef)transport, error);
}

static CF_RETURNS_RETAINED
CFDictionaryRef handleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error) {
    SOSTransportMessageKVSRef tpt = (SOSTransportMessageKVSRef)transport;
    CFMutableDictionaryRef handled = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef peerToMessage = CFDictionaryGetValue(circle_peer_messages_table, tpt->circleName);
    CFMutableArrayRef handled_peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(handled, tpt->circleName, handled_peers);
    
    if(peerToMessage){
        CFDictionaryForEach(peerToMessage, ^(const void *key, const void *value) {
            CFStringRef peer_id = (CFStringRef) key;
            CFDataRef peer_message = (CFDataRef) value;
            CFErrorRef localError = NULL;
            
            if (SOSTransportMessageHandlePeerMessage(transport, peer_id, peer_message, &localError)) {
                CFArrayAppendValue(handled_peers, key);
            } else {
                secdebug("transport", "%@ KVSTransport handle message failed: %@", peer_id, localError);
            }
            CFReleaseNull(localError);
        });
    }
    CFReleaseNull(handled_peers);
    
    return handled;
}


static bool sendToPeer(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef peerID, CFDataRef message, CFErrorRef *error) {
    SOSTransportMessageKVSRef kvsTransport = (SOSTransportMessageKVSRef) transport;
    bool result = true;
    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer(kvsTransport, peerID);
    CFDictionaryRef a_message_to_a_peer = CFDictionaryCreateForCFTypes(NULL, message_to_peer_key, message, NULL);
    
    if (!SOSTransportMessageKVSUpdateKVS(kvsTransport, a_message_to_a_peer, error)) {
        secerror("Sync with peers failed to send to %@ [%@], %@", peerID, a_message_to_a_peer, *error);
        result = false;
    }
    CFReleaseNull(a_message_to_a_peer);
    CFReleaseNull(message_to_peer_key);
    
    return result;
}

static bool syncWithPeers(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef *error){
    // Each entry is keyed by circle name and contains a list of peerIDs
    
    __block bool result = true;
    
    CFDictionaryForEach(circleToPeerIDs, ^(const void *key, const void *value) {
        if (isString(key) && isArray(value)) {
            CFStringRef circleName = (CFStringRef) key;
            CFArrayForEach(value, ^(const void *value) {
                if (isString(value)) {
                    CFStringRef peerID = (CFStringRef) value;
                    result &= SOSTransportMessageSendMessageIfNeeded(transport, circleName, peerID, error);
                }
            });
        }
    });
    
    return result;
}

static bool sendMessages(SOSTransportMessageRef transport, CFDictionaryRef circleToPeersToMessage, CFErrorRef *error) {
    __block bool result = true;
    
    CFDictionaryForEach(circleToPeersToMessage, ^(const void *key, const void *value) {
        if (isString(key) && isDictionary(value)) {
            CFStringRef circleName = (CFStringRef) key;
            CFDictionaryForEach(value, ^(const void *key, const void *value) {
                if (isString(key) && isData(value)) {
                    CFStringRef peerID = (CFStringRef) key;
                    CFDataRef message = (CFDataRef) value;
                    bool rx = sendToPeer(transport, circleName, peerID, message, error);
                    result &= rx;
                }
            });
        }
    });
    
    return true;
}

static bool flushChanges(SOSTransportMessageRef transport, CFErrorRef *error)
{
    return SOSTransportMessageKVSSendPendingChanges((SOSTransportMessageKVSRef) transport, error);
}

static bool cleanupAfterPeer(SOSTransportMessageRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error)
{
    return SOSTransportMessageKVSCleanupAfterPeerMessages((SOSTransportMessageKVSRef) transport, circle_to_peer_ids, error);
}

