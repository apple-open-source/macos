//
//  SOSTransportMessageIDS.c
//  sec
//
//
#include <Security/SecBasePriv.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>

#include <SOSCloudCircleServer.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>

#include <utilities/SecCFWrappers.h>
#include <SOSInternal.h>
#include <AssertMacros.h>

#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainConstants.h>


#define IDS "IDS transport"

struct __OpaqueSOSTransportMessageIDS {
    struct __OpaqueSOSTransportMessage          m;
    
};

const CFStringRef kSecIDSErrorDomain = CFSTR("com.apple.security.ids.error");


//
// V-table implementation forward declarations
//
static bool sendToPeer(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID, CFDictionaryRef message, CFErrorRef *error);
static bool syncWithPeers(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef *error);
static bool sendMessages(SOSTransportMessageRef transport, CFDictionaryRef circleToPeersToMessage, CFErrorRef *error);
static void destroy(SOSTransportMessageRef transport);
static bool cleanupAfterPeer(SOSTransportMessageRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error);
static bool flushChanges(SOSTransportMessageRef transport, CFErrorRef *error);
static CF_RETURNS_RETAINED CFDictionaryRef handleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);

static inline CFIndex getTransportType(SOSTransportMessageRef transport, CFErrorRef *error){
    return kIDS;
}

SOSTransportMessageIDSRef SOSTransportMessageIDSCreate(SOSAccountRef account, CFStringRef circleName, CFErrorRef *error)
{
    SOSTransportMessageIDSRef ids = (SOSTransportMessageIDSRef) SOSTransportMessageCreateForSubclass(sizeof(struct __OpaqueSOSTransportMessageIDS) - sizeof(CFRuntimeBase), account, circleName, error);
    
    if (ids) {
        // Fill in vtable:
        ids->m.sendMessages = sendMessages;
        ids->m.syncWithPeers = syncWithPeers;
        ids->m.flushChanges = flushChanges;
        ids->m.cleanupAfterPeerMessages = cleanupAfterPeer;
        ids->m.destroy = destroy;
        ids->m.handleMessages = handleMessages;
        ids->m.getTransportType = getTransportType;
        
        // Initialize ourselves
        
        SOSTransportMessageIDSGetIDSDeviceID(account);
        SOSRegisterTransportMessage((SOSTransportMessageRef)ids);
    }
    
    return ids;
}
static void destroy(SOSTransportMessageRef transport){
    SOSUnregisterTransportMessage(transport);
}

static CF_RETURNS_RETAINED CFDictionaryRef handleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error) {
    // TODO: This might need to be: return CFDictionaryCreateForCFTypes(kCFAllocatorDefault, NULL);
    return CFDictionaryCreateForCFTypes(kCFAllocatorDefault);
}

HandleIDSMessageReason SOSTransportMessageIDSHandleMessage(SOSAccountRef account, CFDictionaryRef message, CFErrorRef *error) {
    
    secdebug("IDS Transport", "SOSTransportMessageIDSHandleMessage!");
    
    CFStringRef dataKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyIDSDataMessage, kCFStringEncodingASCII);
    CFStringRef deviceIDKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyDeviceID, kCFStringEncodingASCII);
    
    CFDataRef messageData = (CFDataRef)CFDictionaryGetValue(message, dataKey);
    CFStringRef fromID = (CFStringRef)CFDictionaryGetValue(message, deviceIDKey);
    
    SOSPeerInfoRef myPeer = SOSAccountGetMyPeerInfo(account);

    if(!myPeer) {
        CFReleaseNull(deviceIDKey);
        CFReleaseNull(dataKey);
        if(!SOSAccountHasFullPeerInfo(account, error))
            return kHandleIDSMessageOtherFail;
    }

    __block CFStringRef peerID = NULL;
    
    SOSCircleForEachPeer(account->trusted_circle, ^(SOSPeerInfoRef peer) {
        CFStringRef deviceID = SOSPeerInfoCopyDeviceID(peer);
        if(CFStringCompare(deviceID, fromID, 0) == 0)
            peerID = SOSPeerInfoGetPeerID(peer);
        CFReleaseNull(deviceID);
    });
    if(!peerID){
        secerror("Could not find peer matching the IDS device ID, dropping message");
        CFReleaseNull(dataKey);
        CFReleaseNull(deviceIDKey);
        return kHandleIDSMessageNotReady;
    }
    
    if(messageData != NULL && CFDataGetLength(messageData) > 0){
        
        if (SOSTransportMessageHandlePeerMessage(account->ids_message_transport, peerID, messageData, error)) {
            CFMutableDictionaryRef peersToSyncWith = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
            CFMutableArrayRef peerIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
            CFArrayAppendValue(peerIDs, peerID);
            CFDictionaryAddValue(peersToSyncWith, SOSCircleGetName(account->trusted_circle), peerIDs);
            
            if(!SOSTransportMessageSyncWithPeers(account->ids_message_transport, peersToSyncWith, error))
            {
                secerror("SOSTransportMessageIDSHandleMessage Could not sync with all peers: %@", *error);
            }
            else{
                secdebug("IDS Transport", "Synced with all peers!");
                CFReleaseNull(dataKey);
                CFReleaseNull(deviceIDKey);
                return kHandleIDSMessageSuccess;
            }
                
        }
        else{
            CFReleaseNull(dataKey);
            CFReleaseNull(deviceIDKey);
            secerror("IDS Transport Could not handle message: %@", messageData);
            return kHandleIDSMessageOtherFail;
        }
    }
    secerror("Data doesn't exist: %@", messageData);
    CFReleaseNull(deviceIDKey);
    CFReleaseNull(dataKey);
    return kHandleIDSMessageOtherFail;
}


static bool sendToPeer(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID,CFDictionaryRef message, CFErrorRef *error)
{
    __block bool success = false;
    CFStringRef errorMessage = NULL;
    CFDictionaryRef userInfo;
    CFStringRef operation = NULL;
    CFDataRef operationData = NULL;
    CFMutableDataRef mutableData = NULL;
    SOSAccountRef account = SOSTransportMessageGetAccount(transport);
    CFStringRef ourPeerID = SOSPeerInfoGetPeerID(SOSAccountGetMyPeerInfo(account));
    
    require_action_quiet((deviceID != NULL && CFStringGetLength(deviceID) >0), fail, errorMessage = CFSTR("Need an IDS Device ID to sync"));

    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    dispatch_retain(wait_for); // Both this scope and the block own it.
    
    secnotice("ids transport", "Starting");
    
    SOSCloudKeychainSendIDSMessage(message, deviceID, ourPeerID, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {
        success = (sync_error == NULL);
        if (error) {
            CFRetainAssign(*error, sync_error);
        }
        
        dispatch_semaphore_signal(wait_for);
        dispatch_release(wait_for);
    });
    
    dispatch_semaphore_wait(wait_for, DISPATCH_TIME_FOREVER);
    dispatch_release(wait_for);
    
    if(!success){
        if(error != NULL)
            secerror("Failed to send message to peer! %@", *error);
        else
            secerror("Failed to send message to peer");
    }
    else{
        secdebug("IDS Transport", "Sent message to peer!");
    }

    CFReleaseNull(operation);
    CFReleaseNull(operationData);
    CFReleaseNull(mutableData);
    
    return success;
    
fail:
    userInfo = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, kCFErrorLocalizedDescriptionKey, errorMessage, NULL);
    if(error != NULL){
        *error =CFErrorCreate(kCFAllocatorDefault, CFSTR("com.apple.security.ids.error"), kSecIDSErrorNoDeviceID, userInfo);
        secerror("%@", *error);
    }
    CFReleaseNull(operation);
    CFReleaseNull(operationData);
    CFReleaseNull(mutableData);
    CFReleaseNull(userInfo);
    
    return success;
}

static bool syncWithPeers(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef *error) {
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
    SOSCircleRef circle = SOSAccountGetCircle(transport->account, error);
    
    CFDictionaryForEach(circleToPeersToMessage, ^(const void *key, const void *value) {
        if (isString(key) && isDictionary(value)) {
            CFStringRef circleName = (CFStringRef) key;
            CFDictionaryForEach(value, ^(const void *key, const void *value) {
                if (isString(key) && isDictionary(value)) {
                    CFStringRef peerID = (CFStringRef) key;
                    CFDictionaryRef message = (CFDictionaryRef) value;
                    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
                        CFStringRef deviceID = SOSPeerInfoCopyDeviceID(peer);
                        if(CFEqualSafe(SOSPeerInfoGetPeerID(peer), peerID) || CFEqualSafe(deviceID, peerID)){
                            bool rx = false;
                            rx = sendToPeer(transport, circleName, deviceID, peerID, message, error);
                            result &= rx;
                        }
                        CFReleaseNull(deviceID);
                    });
                }
            });
        }
    });
    
    return result;
}

static bool flushChanges(SOSTransportMessageRef transport, CFErrorRef *error)
{
    return true;
}

static bool cleanupAfterPeer(SOSTransportMessageRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error)
{
    return true;
}

void SOSTransportMessageIDSGetIDSDeviceID(SOSAccountRef account){
    
    CFStringRef deviceID = SOSPeerInfoCopyDeviceID(SOSFullPeerInfoGetPeerInfo(account->my_identity));
    if( deviceID == NULL || CFStringGetLength(deviceID) == 0){
        SOSCloudKeychainGetIDSDeviceID(^(CFDictionaryRef returnedValues, CFErrorRef sync_error){
            bool success = (sync_error == NULL);
            if (!success) {
                secerror("Could not ask IDSKeychainSyncingProxy for Device ID: %@", sync_error);
            }
            else{
                secdebug("IDS Transport", "Successfully attempting to retrieve the IDS Device ID");
            }
        });
    }
    CFReleaseNull(deviceID);
}
