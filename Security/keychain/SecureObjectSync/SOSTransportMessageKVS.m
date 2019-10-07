#include "keychain/SecureObjectSync/SOSTransport.h"
#include "keychain/SecureObjectSync/SOSTransportMessage.h"
#import "keychain/SecureObjectSync/SOSTransportMessageKVS.h"
#include "keychain/SecureObjectSync/SOSKVSKeys.h"
#include <utilities/SecCFWrappers.h>
#include <utilities/SecADWrapper.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include <AssertMacros.h>
#include "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"

@implementation SOSMessageKVS

@synthesize pending_changes = pending_changes;

-(id) initWithAccount:(SOSAccount*)acct andName:(NSString*)name
{
    self = [super init];
    
    if (self) {
        account = acct;
        circleName = [[NSString alloc]initWithString:name];
        SOSEngineRef e = SOSDataSourceFactoryGetEngineForDataSourceName(acct.factory, (__bridge CFStringRef)(circleName), NULL);
        engine = e;
        pending_changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        SOSRegisterTransportMessage((SOSMessage*)self);
    }
    
    return self;
}

-(void)dealloc
{
    if(self) {
        CFReleaseNull(self->pending_changes);
    }
}

-(CFIndex) SOSTransportMessageGetTransportType
{
    return kKVS;
}
-(CFStringRef) SOSTransportMessageGetCircleName
{
    return (__bridge CFStringRef)circleName;
}
-(CFTypeRef) SOSTransportMessageGetEngine
{
    return engine;
}
-(SOSAccount*) SOSTransportMessageGetAccount
{
    return account;
}

-(bool) SOSTransportMessageKVSAppendKeyInterest:(SOSMessageKVS*) transport ak:(CFMutableArrayRef) alwaysKeys firstUnlock:(CFMutableArrayRef) afterFirstUnlockKeys
                                       unlocked:(CFMutableArrayRef) unlockedKeys err:(CFErrorRef *)localError
{
    require_quiet(engine, fail);
    
    CFArrayRef peerInfos = SOSAccountCopyPeersToListenTo( [self SOSTransportMessageGetAccount], localError);
    
    if(peerInfos){
        NSString* myID = self.account.peerID;

        CFArrayForEach(peerInfos, ^(const void *value) {
            CFStringRef peerID = SOSPeerInfoGetPeerID((SOSPeerInfoRef)value);
            CFStringRef peerMessage = SOSMessageKeyCreateFromPeerToTransport(transport,(__bridge CFStringRef) myID, peerID);
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


-(CFIndex) SOSTransportMessageGetTransportType:(SOSMessage*) transport err:(CFErrorRef *)error
{
    return kKVS;
}

static bool SOSTransportMessageKVSUpdateKVS(SOSMessageKVS* transport, CFDictionaryRef changes, CFErrorRef *error){

    SecADAddValueForScalarKey(CFSTR("com.apple.security.sos.sendkvs"), 1);

    CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef block_error) {
        if (block_error) {
            secerror("Error putting: %@", block_error);
        }
    };
    
    SOSCloudKeychainPutObjectsInCloud(changes, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), log_error);
    return true;
}

static void SOSTransportMessageKVSAddToPendingChanges(SOSMessageKVS* transport, CFStringRef message_key, CFDataRef message_data){
    if (transport.pending_changes == NULL) {
        transport.pending_changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    }
    if (message_data == NULL) {
        CFDictionarySetValue(transport.pending_changes, message_key, kCFNull);
    } else {
        CFDictionarySetValue(transport.pending_changes, message_key, message_data);
    }
}

static bool SOSTransportMessageKVSCleanupAfterPeerMessages(SOSMessageKVS* transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error)
{
    CFArrayRef enginePeers = SOSEngineGetPeerIDs((SOSEngineRef)[transport SOSTransportMessageGetEngine]);
 
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
    
    return [transport SOSTransportMessageFlushChanges:(SOSMessage*)transport err:error];
}

-(bool) SOSTransportMessageCleanupAfterPeerMessages:(SOSMessage*) transport peers:(CFDictionaryRef) peers err:(CFErrorRef*) error
{
    return SOSTransportMessageKVSCleanupAfterPeerMessages((SOSMessageKVS*) transport, peers, error);
}

-(CFDictionaryRef)CF_RETURNS_RETAINED SOSTransportMessageHandlePeerMessageReturnsHandledCopy:(SOSMessage*) transport peerMessages:(CFMutableDictionaryRef) circle_peer_messages_table err:(CFErrorRef *)error
{
    CFMutableDictionaryRef handled = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef peerToMessage = CFDictionaryGetValue(circle_peer_messages_table, (__bridge CFStringRef)(transport.circleName));
    CFMutableArrayRef handled_peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    if(peerToMessage){
        CFDictionaryForEach(peerToMessage, ^(const void *key, const void *value) {
            CFStringRef peer_id = asString(key, NULL);
            CFDataRef peer_message = asData(value, NULL);
            CFErrorRef localError = NULL;

            if (peer_id && peer_message && [transport SOSTransportMessageHandlePeerMessage:transport id:peer_id cm:peer_message err:&localError ]) {
                CFArrayAppendValue(handled_peers, key);
            } else {
                secnotice("transport", "%@ KVSTransport handle message failed: %@", peer_id, localError);
            }
            CFReleaseNull(localError);
        });
    }
    CFDictionaryAddValue(handled, (__bridge const void *)(transport.circleName), handled_peers);
    CFReleaseNull(handled_peers);
    
    return handled;
}


static bool sendToPeer(SOSMessage* transport, CFStringRef circleName, CFStringRef peerID, CFDataRef message, CFErrorRef *error)
{
    SOSMessageKVS* kvsTransport = (SOSMessageKVS*) transport;
    bool result = true;
    SOSAccount* account = [transport SOSTransportMessageGetAccount];
    CFTypeRef dsid = SOSAccountGetValue(account, kSOSDSIDKey, error);
    
    if(dsid == NULL)
        dsid = kCFNull;
    NSString* myID = account.peerID;
    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer(kvsTransport, (__bridge CFStringRef) myID, peerID);

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

-(bool) SOSTransportMessageSyncWithPeers:(SOSMessage*) transport p:(CFSetRef) peers err:(CFErrorRef *)error
{
    // Each entry is keyed by circle name and contains a list of peerIDs
    __block bool result = true;

    CFSetForEach(peers, ^(const void *value) {
        CFStringRef peerID = asString(value, NULL);
        CFErrorRef localError = NULL;

        result &= [ transport SOSTransportMessageSendMessageIfNeeded:transport
                                                          circleName:(__bridge CFStringRef)(transport.circleName)
                                                                 pID:peerID
                                                                 err:&localError];

        if (!result && error && *error == NULL && localError) {
            *error = (CFErrorRef)CFRetain(localError);
        }
        CFReleaseNull(localError);
    });

    return result;
}

-(bool) SOSTransportMessageSendMessages:(SOSMessage*) transport pm:(CFDictionaryRef) peer_messages err:(CFErrorRef *)error
{
    __block bool result = true;
    
    CFDictionaryForEach(peer_messages, ^(const void *key, const void *value) {
        CFStringRef peerID = asString(key, NULL);
        CFDataRef message = asData(value,NULL);
        if (peerID && message) {
            bool rx = sendToPeer(transport, (__bridge CFStringRef)(transport.circleName), peerID, message, error);
            result &= rx;
        }
    });

    return true;
}

@end


