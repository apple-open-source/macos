//
//  SOSTransportMessageIDS.c
//  sec
//
//
#include <Security/SecBasePriv.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#import <Security/SecureObjectSync/SOSTransportMessage.h>
#import <Security/SecureObjectSync/SOSAccountPriv.h>
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
#include <utilities/SecADWrapper.h>
#import "Security/SecureObjectSync/SOSAccountTrustClassic.h"

#define IDS "IDS transport"

@implementation SOSMessageIDS

@synthesize useFragmentation = useFragmentation;

-(CFIndex) SOSTransportMessageGetTransportType
{
    return kIDS;
}

-(void) SOSTransportMessageIDSSetFragmentationPreference:(SOSMessageIDS*) transport pref:(CFBooleanRef) preference
{
        useFragmentation = preference;
}

-(CFBooleanRef) SOSTransportMessageIDSGetFragmentationPreference:(SOSMessageIDS*) transport
{
        return useFragmentation;
}

-(id) initWithAcount:(SOSAccount*)acct circleName:(CFStringRef)name
{
    self = [super init];
    
    if (self) {
        self.useFragmentation = kCFBooleanTrue;
        self.account = acct;
        self.circleName = [[NSString alloc]initWithString:(__bridge NSString*)name];
        [self SOSTransportMessageIDSGetIDSDeviceID:account];
        SOSRegisterTransportMessage((SOSMessage*)self);
    }
    
    return self;
}

-(CFDictionaryRef) CF_RETURNS_RETAINED SOSTransportMessageHandlePeerMessageReturnsHandledCopy:(SOSMessage*) transport peerMessages:(CFMutableDictionaryRef) circle_peer_messages_table err:(CFErrorRef *)error
{
    return CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
}

static HandleIDSMessageReason checkMessageValidity(SOSAccount* account, CFStringRef fromDeviceID, CFStringRef fromPeerID, CFStringRef *peerID, SOSPeerInfoRef *theirPeerInfo){
    SOSAccountTrustClassic *trust = account.trust;
    __block HandleIDSMessageReason reason = kHandleIDSMessageDontHandle;

    SOSCircleForEachPeer(trust.trustedCircle, ^(SOSPeerInfoRef peer) {
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
                    secnotice("ids transport", "deviceIDMisMatch");
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
                else{
                    secerror("?? deviceID:%@, pID: %@, fromPeerID: %@, fromDeviceID: %@", deviceID, pID, fromPeerID, fromDeviceID);
                }
            }
        }
        CFReleaseNull(deviceID);
    });

    return reason;
}

-(HandleIDSMessageReason) SOSTransportMessageIDSHandleMessage:(SOSAccount*)acct m:(CFDictionaryRef) message err:(CFErrorRef *)error
{
    secnotice("IDS Transport", "SOSTransportMessageIDSHandleMessage!");
    
    CFStringRef dataKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyIDSDataMessage, kCFStringEncodingASCII);
    CFStringRef deviceIDKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyDeviceID, kCFStringEncodingASCII);
    CFStringRef sendersPeerIDKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeySendersPeerID, kCFStringEncodingASCII);
    CFStringRef ourPeerIdKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyPeerID, kCFStringEncodingASCII);
    NSString *errMessage = nil;

    HandleIDSMessageReason result = kHandleIDSMessageSuccess;

    CFDataRef messageData = asData(CFDictionaryGetValue(message, dataKey), NULL);
    __block CFStringRef fromDeviceID = asString(CFDictionaryGetValue(message, deviceIDKey), NULL);
    __block CFStringRef fromPeerID = (CFStringRef)CFDictionaryGetValue(message, sendersPeerIDKey);
    CFStringRef ourPeerID = asString(CFDictionaryGetValue(message, ourPeerIdKey), NULL);

    CFStringRef peerID = NULL;
    SOSPeerInfoRef theirPeer = NULL;

    require_action_quiet(fromDeviceID, exit, result = kHandleIDSMessageDontHandle; errMessage = @"Missing device name");
    require_action_quiet(fromPeerID, exit, result = kHandleIDSMessageDontHandle; errMessage = @"Missing from peer id");
    require_action_quiet(messageData && CFDataGetLength(messageData) != 0, exit, result = kHandleIDSMessageDontHandle; errMessage = @"no message data");
    require_action_quiet(SOSAccountHasFullPeerInfo(account, error), exit, result = kHandleIDSMessageNotReady; errMessage = @"no full perinfo");
    require_action_quiet(ourPeerID && [account.peerID isEqual: (__bridge NSString*) ourPeerID], exit, result = kHandleIDSMessageDontHandle; secnotice("IDS Transport","ignoring message for: %@", ourPeerID));

    require_quiet((result = checkMessageValidity( account,  fromDeviceID, fromPeerID, &peerID, &theirPeer)) == kHandleIDSMessageSuccess, exit);

    if ([account.ids_message_transport SOSTransportMessageHandlePeerMessage:account.ids_message_transport id:peerID cm:messageData err:error]) {
        CFMutableDictionaryRef peersToSyncWith = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFMutableSetRef peerIDs = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        CFSetAddValue(peerIDs, peerID);
        SOSAccountTrustClassic* trust = account.trust;
        //sync using fragmentation?
        if(SOSPeerInfoShouldUseIDSMessageFragmentation(trust.peerInfo, theirPeer)){
            //set useFragmentation bit
            [account.ids_message_transport SOSTransportMessageIDSSetFragmentationPreference:account.ids_message_transport pref: kCFBooleanTrue];
        }
        else{
            [account.ids_message_transport SOSTransportMessageIDSSetFragmentationPreference:account.ids_message_transport pref: kCFBooleanFalse];
        }

        if(![account.ids_message_transport SOSTransportMessageSyncWithPeers:account.ids_message_transport p:peerIDs  err:error]){
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

    if(errMessage != nil){
        secerror("%@", errMessage);
    }
    CFReleaseNull(ourPeerIdKey);
    CFReleaseNull(sendersPeerIDKey);
    CFReleaseNull(deviceIDKey);
    CFReleaseNull(dataKey);
    return result;
}


static bool sendToPeer(SOSMessageIDS* transport, bool shouldUseAckModel, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID,CFDictionaryRef message, CFErrorRef *error)
{
    __block bool success = false;
    CFStringRef errorMessage = NULL;
    CFDictionaryRef userInfo;
    CFStringRef operation = NULL;
    CFDataRef operationData = NULL;
    CFMutableDataRef mutableData = NULL;
    SOSAccount* account = [transport SOSTransportMessageGetAccount];
    CFStringRef ourPeerID = SOSPeerInfoGetPeerID(account.peerInfo);
    CFStringRef operationToString = NULL;
    
    CFDictionaryRef messagetoSend = NULL;
    
    if(deviceID == NULL || CFStringGetLength(deviceID) == 0){
        errorMessage = CFSTR("Need an IDS Device ID to sync");
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

    if(CFDictionaryGetValue(message, kIDSOperationType) == NULL && [transport SOSTransportMessageIDSGetFragmentationPreference:transport] == kCFBooleanTrue){
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

    secnotice("IDS Transport", "Starting");

    SecADAddValueForScalarKey(CFSTR("com.apple.security.sos.sendids"), 1);

    CFStringRef myDeviceID =  CFRetainSafe((__bridge CFStringRef)account.deviceID);
    if(!myDeviceID){
        myDeviceID = SOSPeerInfoCopyDeviceID(account.peerInfo);
    }

    SOSCloudKeychainSendIDSMessage(messagetoSend, deviceID, ourPeerID, myDeviceID, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [transport SOSTransportMessageIDSGetFragmentationPreference:transport], ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {
        success = (sync_error == NULL);
        if (sync_error && error) {
            CFRetainAssign(*error, sync_error);
        }
        
        dispatch_semaphore_signal(wait_for);
    });
    
    if (dispatch_semaphore_wait(wait_for, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 2)) != 0) {
        secerror("IDS Transport: timed out waiting for message send to complete");
    }

    if(!success){
        if(error != NULL)
            secerror("IDS Transport: Failed to send message to peer! %@", *error);
        else
            secerror("IDS Transport: Failed to send message to peer");
    }
    else{
        secnotice("IDS Transport", "Sent message to peer!");
    }
    CFReleaseNull(myDeviceID);
    CFReleaseNull(messagetoSend);
    CFReleaseNull(operation);
    CFReleaseNull(operationData);
    CFReleaseNull(mutableData);
    CFReleaseNull(operationToString);
    return success;
}


-(bool) SOSTransportMessageSyncWithPeers:(SOSMessageIDS*) transport p:(CFSetRef) peers err:(CFErrorRef *)error
{
    // Each entry is keyed by circle name and contains a list of peerIDs
    __block bool result = true;

    CFSetForEach(peers, ^(const void *value) {
        CFStringRef peerID = asString(value, NULL);

        result &= [transport SOSTransportMessageSendMessageIfNeeded:transport id:(__bridge CFStringRef)(transport.circleName) pID:peerID err:error];
    });

    return result;
}

-(bool) SOSTransportMessageSendMessages:(SOSMessageIDS*) transport pm:(CFDictionaryRef) peer_messages err:(CFErrorRef *)error
{
    __block bool result = true;

    SOSPeerInfoRef myPeer = transport->account.peerInfo;
    CFStringRef myID = SOSPeerInfoGetPeerID(myPeer);
    if(!myPeer)
        return result;

    CFDictionaryForEach(peer_messages, ^(const void *key, const void *value) {
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

        [transport SOSTransportMessageIDSSetFragmentationPreference:transport
                                                               pref:SOSPeerInfoShouldUseIDSMessageFragmentation(myPeer, peer) ? kCFBooleanTrue : kCFBooleanFalse];
        bool shouldUseAckModel = SOSPeerInfoShouldUseACKModel(myPeer, peer);

        result &= sendToPeer(transport, shouldUseAckModel, (__bridge CFStringRef)(transport.circleName), deviceID, peerID, message, &error);

    skip:
        if (error) {
            secerror("Failed to sync to %@ over IDS: %@", peerID, error);
        }

        CFReleaseNull(peer);
        CFReleaseNull(deviceID);
        CFReleaseNull(message);

        CFReleaseNull(error);
    });

    return result;
}


-(bool) SOSTransportMessageFlushChanges:(SOSMessageIDS*) transport err:(CFErrorRef *)error
{
    return true;
}

-(bool) SOSTransportMessageCleanupAfterPeerMessages:(SOSMessageIDS*) transport peers:(CFDictionaryRef) peers err:(CFErrorRef*) error
{
    return true;
}

-(bool) SOSTransportMessageIDSGetIDSDeviceID:(SOSAccount*)acct
{
    SOSAccountTrustClassic* trust = acct.trust;
    CFStringRef deviceID = SOSPeerInfoCopyDeviceID(trust.peerInfo);
    bool hasDeviceID = (deviceID != NULL && CFStringGetLength(deviceID) != 0) || account.deviceID;
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
@end
