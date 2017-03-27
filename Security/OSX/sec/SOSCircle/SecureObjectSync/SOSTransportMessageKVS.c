#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSTransportMessageKVS.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <utilities/SecCFWrappers.h>
#include <SOSInternal.h>
#include <AssertMacros.h>
#include <SOSCloudKeychainClient.h>

struct __OpaqueSOSTransportMessageKVS {
    struct __OpaqueSOSTransportMessage          m;
    CFMutableDictionaryRef  pending_changes;
    
};

//
// V-table implementation forward declarations
//
static bool sendToPeer(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef peerID, CFDataRef message, CFErrorRef *error);
static bool syncWithPeers(SOSTransportMessageRef transport, CFSetRef peers, CFErrorRef *error);
static bool sendMessages(SOSTransportMessageRef transport, CFDictionaryRef peersToMessage, CFErrorRef *error);
static bool cleanupAfterPeer(SOSTransportMessageRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error);
static void destroy(SOSTransportMessageRef transport);
static inline CFIndex getTransportType(SOSTransportMessageRef transport, CFErrorRef *error);

static CF_RETURNS_RETAINED
CFDictionaryRef handleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);
    
static bool flushChanges(SOSTransportMessageRef transport, CFErrorRef *error);

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
        tkvs->m.getTransportType = getTransportType;
        // Initialize ourselves
        tkvs->pending_changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        SOSRegisterTransportMessage((SOSTransportMessageRef)tkvs);
    }
    
    return tkvs;
}

bool SOSTransportMessageKVSAppendKeyInterest(SOSTransportMessageKVSRef transport, CFMutableArrayRef alwaysKeys, CFMutableArrayRef afterFirstUnlockKeys, CFMutableArrayRef unlockedKeys, CFErrorRef *localError){
    SOSEngineRef engine = SOSTransportMessageGetEngine((SOSTransportMessageRef)transport);
    require_quiet(engine, fail);
    
    CFArrayRef peerInfos = SOSAccountCopyPeersToListenTo(SOSTransportMessageGetAccount((SOSTransportMessageRef) transport), localError);
    
    if(peerInfos){
        CFArrayForEach(peerInfos, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFStringRef peerID = SOSPeerInfoGetPeerID(peer);
            CFStringRef peerMessage = SOSMessageKeyCreateFromPeerToTransport((SOSTransportMessageRef)transport, peerID);
            if(peerMessage != NULL)
                CFArrayAppendValue(unlockedKeys, peerMessage);
            CFReleaseNull(peerMessage);
        });
        CFReleaseNull(peerInfos);
    }
    return true;
fail:
    return false;
}
static void destroy(SOSTransportMessageRef transport){
    SOSTransportMessageKVSRef tkvs = (SOSTransportMessageKVSRef)transport;
    CFReleaseNull(tkvs->pending_changes);
    SOSUnregisterTransportMessage((SOSTransportMessageRef)tkvs);
    
}

static inline CFIndex getTransportType(SOSTransportMessageRef transport, CFErrorRef *error){
    return kKVS;
}

static bool SOSTransportMessageKVSUpdateKVS(SOSTransportMessageKVSRef transport, CFDictionaryRef changes, CFErrorRef *error){

    CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef block_error) {
        if (block_error) {
            secerror("Error putting: %@", block_error);
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
    SOSAccountRef account = SOSTransportMessageGetAccount((SOSTransportMessageRef)transport);
    CFTypeRef dsid = SOSAccountGetValue(account, kSOSDSIDKey, error);
    
    if(dsid == NULL)
        dsid = kCFNull;
    
    CFDictionaryAddValue(transport->pending_changes, kSOSKVSRequiredKey, dsid);
    
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
    __block SOSAccountRef account = SOSTransportMessageGetAccount((SOSTransportMessageRef)transport);
 
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
                            
                            CFStringRef lastCirclePushedKey = SOSLastCirclePushedKeyCreateWithCircleNameAndPeerID(circle_name, cleanup_id);
                            SOSTransportMessageKVSAddToPendingChanges(transport, lastCirclePushedKey, NULL);
                            CFReleaseSafe(lastCirclePushedKey);
                            
                            CFStringRef lastKeyParameterPushedKey = SOSLastKeyParametersPushedKeyCreateWithPeerID(cleanup_id);
                            SOSTransportMessageKVSAddToPendingChanges(transport, lastKeyParameterPushedKey, NULL);
                            CFReleaseSafe(lastKeyParameterPushedKey);
                    
			    CFStringRef lastCirclePushedWithAccountGestaltKey = SOSLastCirclePushedKeyCreateWithAccountGestalt(account);
	                    SOSTransportMessageKVSAddToPendingChanges(transport, lastCirclePushedKey, NULL);
        		    CFReleaseSafe(lastCirclePushedWithAccountGestaltKey);

		            CFStringRef lastKeyParameterWithAccountGestaltKey = SOSLastKeyParametersPushedKeyCreateWithAccountGestalt(account);
			    SOSTransportMessageKVSAddToPendingChanges(transport, lastCirclePushedKey, NULL);
                            CFReleaseSafe(lastKeyParameterWithAccountGestaltKey);

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

    CFMutableDictionaryRef handled = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef peerToMessage = CFDictionaryGetValue(circle_peer_messages_table, transport->circleName);
    CFMutableArrayRef handled_peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    if(peerToMessage){
        CFDictionaryForEach(peerToMessage, ^(const void *key, const void *value) {
            CFStringRef peer_id = asString(key, NULL);
            CFDataRef peer_message = asData(value, NULL);
            CFErrorRef localError = NULL;
            
            if (peer_id && peer_message && SOSTransportMessageHandlePeerMessage(transport, peer_id, peer_message, &localError)) {
                CFArrayAppendValue(handled_peers, key);
            } else {
                secnotice("transport", "%@ KVSTransport handle message failed: %@", peer_id, localError);
            }
            CFReleaseNull(localError);
        });
    }
    CFDictionaryAddValue(handled, transport->circleName, handled_peers);
    CFReleaseNull(handled_peers);
    
    return handled;
}


static bool sendToPeer(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef peerID, CFDataRef message, CFErrorRef *error) {
    SOSTransportMessageKVSRef kvsTransport = (SOSTransportMessageKVSRef) transport;
    bool result = true;
    SOSAccountRef account = SOSTransportMessageGetAccount((SOSTransportMessageRef)transport);
    CFTypeRef dsid = SOSAccountGetValue(account, kSOSDSIDKey, error);
    
    if(dsid == NULL)
        dsid = kCFNull;

    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer((SOSTransportMessageRef)kvsTransport, peerID);

    CFTypeRef messageToSend = message != NULL ? (CFTypeRef) message : (CFTypeRef) kCFNull;
    CFDictionaryRef a_message_to_a_peer = CFDictionaryCreateForCFTypes(NULL,
                                                                       message_to_peer_key, messageToSend,
                                                                       kSOSKVSRequiredKey, dsid,
                                                                       NULL);
    
    if (!SOSTransportMessageKVSUpdateKVS(kvsTransport, a_message_to_a_peer, error)) {
        secerror("Sync with peers failed to send to %@ [%@], %@", peerID, a_message_to_a_peer, *error);
        result = false;
    }
    CFReleaseNull(a_message_to_a_peer);
    CFReleaseNull(message_to_peer_key);
    
    return result;
}

static bool syncWithPeers(SOSTransportMessageRef transport, CFSetRef peers, CFErrorRef *error) {
    // Each entry is keyed by circle name and contains a list of peerIDs
    __block bool result = true;

    CFSetForEach(peers, ^(const void *value) {
        CFStringRef peerID = asString(value, NULL);
        result &= SOSTransportMessageSendMessageIfNeeded(transport, transport->circleName, peerID, error);
    });

    return result;
}

static bool sendMessages(SOSTransportMessageRef transport, CFDictionaryRef peersToMessage, CFErrorRef *error) {
    __block bool result = true;
    
    CFStringRef circleName = transport->circleName;
    CFDictionaryForEach(peersToMessage, ^(const void *key, const void *value) {
        CFStringRef peerID = asString(key, NULL);
        CFDataRef message = asData(value,NULL);
        if (peerID && message) {
            bool rx = sendToPeer(transport, circleName, peerID, message, error);
            result &= rx;
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

