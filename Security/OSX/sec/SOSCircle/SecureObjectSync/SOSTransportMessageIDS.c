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
#include <Security/SecureObjectSync/SOSInternal.h>

#define IDS "IDS transport"

struct __OpaqueSOSTransportMessageIDS {
    struct __OpaqueSOSTransportMessage          m;
    CFBooleanRef                                useFragmentation;
};

//
// V-table implementation forward declarations
//
static bool sendToPeer(SOSTransportMessageRef transport, bool shouldUseAckModel, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID,CFDictionaryRef message, CFErrorRef *error);
static bool syncWithPeers(SOSTransportMessageRef transport, CFSetRef peers, CFErrorRef *error);
static bool sendMessages(SOSTransportMessageRef transport, CFDictionaryRef circleToPeersToMessage, CFErrorRef *error);
static void destroy(SOSTransportMessageRef transport);
static bool cleanupAfterPeer(SOSTransportMessageRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error);
static bool flushChanges(SOSTransportMessageRef transport, CFErrorRef *error);
static CF_RETURNS_RETAINED CFDictionaryRef handleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);

static inline CFIndex getTransportType(SOSTransportMessageRef transport, CFErrorRef *error){
    return kIDS;
}

void SOSTransportMessageIDSSetFragmentationPreference(SOSTransportMessageRef transport, CFBooleanRef preference){
        SOSTransportMessageIDSRef t = (SOSTransportMessageIDSRef)transport;
        t->useFragmentation = preference;
}

CFBooleanRef SOSTransportMessageIDSGetFragmentationPreference(SOSTransportMessageRef transport){
        SOSTransportMessageIDSRef t = (SOSTransportMessageIDSRef)transport;
        return t->useFragmentation;
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
    return CFDictionaryCreateForCFTypes(kCFAllocatorDefault, NULL);
}

static HandleIDSMessageReason checkMessageValidity(SOSAccountRef account, CFStringRef fromDeviceID, CFStringRef fromPeerID, CFStringRef *peerID, SOSPeerInfoRef *theirPeerInfo){

    __block HandleIDSMessageReason reason = kHandleIDSMessageDontHandle;

    SOSCircleForEachPeer(account->trusted_circle, ^(SOSPeerInfoRef peer) {
        CFStringRef deviceID = SOSPeerInfoCopyDeviceID(peer);
        CFStringRef pID = SOSPeerInfoGetPeerID(peer);

        if( deviceID && pID && fromPeerID && fromDeviceID && CFStringGetLength(fromPeerID) != 0 ){
            if(CFStringCompare(pID, fromPeerID, 0) == 0){
                if(CFStringGetLength(deviceID) == 0){
                    secnotice("ids transport", "device ID was empty in the peer list, holding on to message");
                    CFReleaseNull(deviceID);
                    reason = kHandleIDSMessageNotReady;
                    return;
                }
                else if(CFStringCompare(fromDeviceID, deviceID, 0) != 0){ //IDSids do not match, ghost
                    reason = kHandleIDSmessageDeviceIDMismatch;
                    CFReleaseNull(deviceID);
                    return;
                }
                else if(CFStringCompare(deviceID, fromDeviceID, 0) == 0){
                    *peerID = pID;
                    *theirPeerInfo = peer;
                    CFReleaseNull(deviceID);
                    reason = kHandleIDSMessageSuccess;
                    return;
                }
            }
        }
        CFReleaseNull(deviceID);
    });

    return reason;
}

HandleIDSMessageReason SOSTransportMessageIDSHandleMessage(SOSAccountRef account, CFDictionaryRef message, CFErrorRef *error) {
    
    secnotice("IDS Transport", "SOSTransportMessageIDSHandleMessage!");
    
    CFStringRef dataKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyIDSDataMessage, kCFStringEncodingASCII);
    CFStringRef deviceIDKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyDeviceID, kCFStringEncodingASCII);
    CFStringRef sendersPeerIDKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeySendersPeerID, kCFStringEncodingASCII);
    CFStringRef ourPeerIdKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyPeerID, kCFStringEncodingASCII);

    HandleIDSMessageReason result = kHandleIDSMessageSuccess;

    CFDataRef messageData = asData(CFDictionaryGetValue(message, dataKey), NULL);
    __block CFStringRef fromDeviceID = asString(CFDictionaryGetValue(message, deviceIDKey), NULL);
    __block CFStringRef fromPeerID = (CFStringRef)CFDictionaryGetValue(message, sendersPeerIDKey);
    CFStringRef ourPeerID = asString(CFDictionaryGetValue(message, ourPeerIdKey), NULL);

    CFStringRef peerID = NULL;
    SOSPeerInfoRef theirPeer = NULL;

    require_action_quiet(fromDeviceID, exit, result = kHandleIDSMessageDontHandle);
    require_action_quiet(fromPeerID, exit, result = kHandleIDSMessageDontHandle);
    require_action_quiet(messageData && CFDataGetLength(messageData) != 0, exit, result = kHandleIDSMessageDontHandle);
    require_action_quiet(SOSAccountHasFullPeerInfo(account, error), exit, result = kHandleIDSMessageNotReady);
    require_action_quiet(ourPeerID && CFEqualSafe(ourPeerID, SOSAccountGetMyPeerID(account)), exit, result = kHandleIDSMessageDontHandle; secnotice("IDS Transport","ignoring message for: %@", ourPeerID));

    require_quiet((result = checkMessageValidity( account,  fromDeviceID, fromPeerID, &peerID, &theirPeer)) == kHandleIDSMessageSuccess, exit);

    if (SOSTransportMessageHandlePeerMessage(account->ids_message_transport, peerID, messageData, error)) {
        CFMutableDictionaryRef peersToSyncWith = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFMutableSetRef peerIDs = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        CFSetAddValue(peerIDs, peerID);

        //sync using fragmentation?
        if(SOSPeerInfoShouldUseIDSMessageFragmentation(SOSFullPeerInfoGetPeerInfo(account->my_identity), theirPeer)){
            //set useFragmentation bit
            SOSTransportMessageIDSSetFragmentationPreference(account->ids_message_transport, kCFBooleanTrue);
        }
        else{
            SOSTransportMessageIDSSetFragmentationPreference(account->ids_message_transport, kCFBooleanFalse);
        }

        if(!SOSTransportMessageSyncWithPeers(account->ids_message_transport, peerIDs, error)){
            secerror("SOSTransportMessageIDSHandleMessage Could not sync with all peers: %@", *error);
        }else{
            secnotice("IDS Transport", "Synced with all peers!");
        }
        
        CFReleaseNull(peersToSyncWith);
        CFReleaseNull(peerIDs);
    }else{
        if(error && *error != NULL){
            CFStringRef errorMessage = CFErrorCopyDescription(*error);
            if (-25308 == CFErrorGetCode(*error)) { // tell KeychainSyncingOverIDSProxy to call us back when device unlocks
                result = kHandleIDSMessageLocked;
            }else{ //else drop it, couldn't handle the message
                result = kHandleIDSMessageDontHandle;
            }
            secerror("IDS Transport Could not handle message: %@, %@", messageData, *error);
            CFReleaseNull(errorMessage);
            
        }
        else{ //no error but failed? drop it, log message
            secerror("IDS Transport Could not handle message: %@", messageData);
            result = kHandleIDSMessageDontHandle;

        }
    }

exit:
    CFReleaseNull(ourPeerIdKey);
    CFReleaseNull(sendersPeerIDKey);
    CFReleaseNull(deviceIDKey);
    CFReleaseNull(dataKey);
    return result;
}


static bool sendToPeer(SOSTransportMessageRef transport, bool shouldUseAckModel, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID,CFDictionaryRef message, CFErrorRef *error)
{
    __block bool success = false;
    CFStringRef errorMessage = NULL;
    CFDictionaryRef userInfo;
    CFStringRef operation = NULL;
    CFDataRef operationData = NULL;
    CFMutableDataRef mutableData = NULL;
    SOSAccountRef account = SOSTransportMessageGetAccount(transport);
    CFStringRef ourPeerID = SOSPeerInfoGetPeerID(SOSAccountGetMyPeerInfo(account));
    CFStringRef operationToString = NULL;
    
    CFDictionaryRef messagetoSend = NULL;
    
    require_action_quiet((deviceID != NULL && CFStringGetLength(deviceID) >0), fail, errorMessage = CFSTR("Need an IDS Device ID to sync"));

    if(CFDictionaryGetValue(message, kIDSOperationType) == NULL && SOSTransportMessageIDSGetFragmentationPreference(transport) == kCFBooleanTrue){
        //handle a keychain data blob using fragmentation!
        secnotice("IDS Transport","sendToPeer: using fragmentation!");

        operationToString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), kIDSKeychainSyncIDSFragmentation);
        messagetoSend = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, 
													 kIDSOperationType, 		   operationToString,
													 kIDSMessageRecipientDeviceID, deviceID, 
													 kIDSMessageRecipientPeerID,   peerID,
                                                     kIDSMessageUsesAckModel,      (shouldUseAckModel ? CFSTR("YES") : CFSTR("NO")),
                                                     kIDSMessageToSendKey,         message,
													 NULL);
    }
    else{ //otherhandle handle the test message without fragmentation
        secnotice("IDS Transport","sendToPeer: not going to fragment message");

        CFMutableDictionaryRef annotatedMessage = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, message);
        CFDictionaryAddValue(annotatedMessage, kIDSMessageRecipientPeerID, peerID);
        CFDictionaryAddValue(annotatedMessage, kIDSMessageRecipientDeviceID, deviceID);
        CFDictionaryAddValue(annotatedMessage, kIDSMessageUsesAckModel, (shouldUseAckModel ? CFSTR("YES") : CFSTR("NO")));
        CFTransferRetained(messagetoSend, annotatedMessage);
        CFReleaseNull(annotatedMessage);
    }
    
    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    dispatch_retain(wait_for); // Both this scope and the block own it.
    
    secnotice("ids transport", "Starting");
    
    SOSCloudKeychainSendIDSMessage(messagetoSend, deviceID, ourPeerID, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), SOSTransportMessageIDSGetFragmentationPreference(transport), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {
        success = (sync_error == NULL);
        if (sync_error && error) {
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
        secnotice("IDS Transport", "Sent message to peer!");
    }
    
    CFReleaseNull(messagetoSend);
    CFReleaseNull(operation);
    CFReleaseNull(operationData);
    CFReleaseNull(mutableData);
    CFReleaseNull(operationToString);
    return success;
    
fail:
    userInfo = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, kCFErrorLocalizedDescriptionKey, errorMessage, NULL);
    if(error != NULL){
        *error =CFErrorCreate(kCFAllocatorDefault, CFSTR("com.apple.security.ids.error"), kSecIDSErrorNoDeviceID, userInfo);
        secerror("%@", *error);
    }
    CFReleaseNull(messagetoSend);
    CFReleaseNull(operation);
    CFReleaseNull(operationData);
    CFReleaseNull(mutableData);
    CFReleaseNull(userInfo);
    CFReleaseNull(operationToString);
    
    return success;
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

    SOSPeerInfoRef myPeer = SOSAccountGetMyPeerInfo(transport->account);
    CFStringRef myID = SOSPeerInfoGetPeerID(myPeer);
    require_quiet(myPeer, fail);

    CFStringRef circleName = transport->circleName;

    CFDictionaryForEach(peersToMessage, ^(const void *key, const void *value) {
        CFErrorRef error = NULL;

        SOSPeerInfoRef peer = NULL;
        CFStringRef deviceID = NULL;
        CFDictionaryRef message = NULL;

        CFStringRef peerID = asString(key, &error);
        require_quiet(peerID, skip);
        require_quiet(!CFEqualSafe(myID, key), skip);

        message = CFRetainSafe(asDictionary(value, &error));
        if (message == NULL) {
            // If it's not a data, return the error
            CFDataRef messageData = asData(value, NULL);
            if (messageData) {
                CFReleaseNull(error);
                message = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, peerID, messageData, NULL);
            }
        }
        require_quiet(message, skip);

        peer = SOSAccountCopyPeerWithID(transport->account, peerID, &error);
        require_quiet(peer, skip);

        deviceID = SOSPeerInfoCopyDeviceID(peer);
        require_action_quiet(deviceID, skip, SOSErrorCreate(kSOSErrorSendFailure, &error, NULL, CFSTR("No IDS ID")));

        SOSTransportMessageIDSSetFragmentationPreference(transport,
                                                         SOSPeerInfoShouldUseIDSMessageFragmentation(myPeer, peer) ? kCFBooleanTrue : kCFBooleanFalse);
        bool shouldUseAckModel = SOSPeerInfoShouldUseACKModel(myPeer, peer);

        result &= sendToPeer(transport, shouldUseAckModel, circleName, deviceID, peerID, message, &error);

    skip:
        if (error) {
            secerror("Failed to sync to %@ over IDS: %@", peerID, error);
        }

        CFReleaseNull(peer);
        CFReleaseNull(deviceID);
        CFReleaseNull(message);

        CFReleaseNull(error);
    });

fail:
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

bool SOSTransportMessageIDSGetIDSDeviceID(SOSAccountRef account){
    CFStringRef deviceID = SOSPeerInfoCopyDeviceID(SOSFullPeerInfoGetPeerInfo(account->my_identity));
    bool hasDeviceID = deviceID != NULL && CFStringGetLength(deviceID) != 0;
    CFReleaseNull(deviceID);

    if(!hasDeviceID){
        SOSCloudKeychainGetIDSDeviceID(^(CFDictionaryRef returnedValues, CFErrorRef sync_error){
            bool success = (sync_error == NULL);
            if (!success) {
                secerror("Could not ask KeychainSyncingOverIDSProxy for Device ID: %@", sync_error);
            }
            else{
                secnotice("IDS Transport", "Successfully attempting to retrieve the IDS Device ID");
            }
        });
    }

    return hasDeviceID;
}
