#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#import <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#import <Security/SecureObjectSync/SOSTransportCircleKVS.h>
#import <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#import <Security/SecureObjectSync/SOSTransportMessageKVS.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSPeerCoder.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#import <Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h>
#import <Security/SecureObjectSync/SOSAccountTrustClassic+Identity.h>

#include "SOSTransportTestTransports.h"
#include "SOSAccountTesting.h"

CFMutableArrayRef key_transports = NULL;
CFMutableArrayRef circle_transports = NULL;
CFMutableArrayRef message_transports = NULL;

///
//Mark Test Key Parameter Transport
///

@implementation CKKeyParameterTest

-(id) initWithAccount:(SOSAccount*) acct andName:(CFStringRef) n andCircleName:(CFStringRef) cN
{
    self = [super init];
    if(self){
        self.name = CFRetainSafe(n);
        self.changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        self.account = acct;
        self.circleName = CFRetainSafe(cN);

        if(!key_transports)
            key_transports = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
        CFArrayAppendValue(key_transports, (__bridge CFTypeRef)((CKKeyParameter*)self));

        SOSRegisterTransportKeyParameter((CKKeyParameter*)self);
    }
    return self;
}

-(void)dealloc {
    if(self) {
        CFReleaseNull(self->_changes);
        CFReleaseNull(self->_circleName);
    }
}

- (void)setChanges:(CFMutableDictionaryRef)changes
{
    CFRetainAssign(self->_changes, changes);
}

-(bool) SOSTransportKeyParameterHandleKeyParameterChanges:(CKKeyParameterTest*) transport  data:(CFDataRef) data err:(CFErrorRef) error
{
    SOSAccount* acct = transport.account;
    return SOSAccountHandleParametersChange(acct, data, &error);
}


-(void) SOSTransportKeyParameterHandleNewAccount:(CKKeyParameterTest*) transport acct:(SOSAccount*) acct
{
    
    if(key_transports){
        CFArrayRemoveAllValue(key_transports, (__bridge CFTypeRef)(acct.key_transport));
    }
    if(message_transports){
        CFArrayRemoveAllValue(message_transports, (__bridge CFTypeRef)acct.ids_message_transport);
        CFArrayRemoveAllValue(message_transports, (__bridge CFTypeRef)acct.kvs_message_transport);
    }
    if(circle_transports)
        CFArrayRemoveAllValue(circle_transports, (__bridge CFTypeRef)(acct.circle_transport));

    SOSAccountSetToNew(acct);
    SOSAccountResetToTest(acct, transport.name);
}

CFStringRef SOSTransportKeyParameterTestGetName(CKKeyParameterTest* transport){
    return transport.name;
}

void SOSTransportKeyParameterTestSetName(CKKeyParameterTest* transport, CFStringRef accountName){
    transport.name = accountName;
}

SOSAccount* SOSTransportKeyParameterTestGetAccount(CKKeyParameterTest* transport){
    return ((CKKeyParameter*)transport).account;
}

CFMutableDictionaryRef SOSTransportKeyParameterTestGetChanges(CKKeyParameterTest* transport){
    return transport.changes;
}

void SOSTransportKeyParameterTestClearChanges(CKKeyParameterTest* transport){
    transport.changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
}

-(bool) SOSTransportKeyParameterPublishCloudParameters:(CKKeyParameterTest*) transport data:(CFDataRef)newParameters err:(CFErrorRef*) error
{
    if(!transport.changes)
        transport.changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFDictionarySetValue(transport.changes, kSOSKVSKeyParametersKey, newParameters);
    
    return true;
}
@end


///
//MARK: Test Circle Transport
///
@implementation SOSCircleStorageTransportTest
@synthesize accountName = accountName;

-(id)init
{
    return [super init];
}

CFStringRef SOSTransportCircleTestGetName(SOSCircleStorageTransportTest* transport){
    return (__bridge CFStringRef)(transport.accountName);
}
void SOSTransportCircleTestSetName(SOSCircleStorageTransportTest* transport, CFStringRef name){
    transport.accountName = nil;
    transport.accountName = (__bridge NSString *)(name);
}

-(CFMutableDictionaryRef) SOSTransportCircleTestGetChanges
{
    return (__bridge CFMutableDictionaryRef)(self.pending_changes);
}

void SOSTransportCircleTestClearChanges(SOSCircleStorageTransportTest* transport){
    transport.pending_changes = [NSMutableDictionary dictionary];
}

-(id) initWithAccount:(SOSAccount *)acct andWithAccountName:(CFStringRef)acctName andCircleName:(CFStringRef)cName
{
    self = [super init];
    if(self){
        self.account = acct;
        self.accountName = (__bridge NSString *)(acctName);
        self.circleName = (__bridge NSString*)cName;
        self.pending_changes = [NSMutableDictionary dictionary];
        if(!circle_transports)
            circle_transports = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
        CFArrayAppendValue(circle_transports, (__bridge CFTypeRef)self);
        
        SOSRegisterTransportCircle((SOSCircleStorageTransportTest*)self);
    }
    return self;
}

-(bool) kvsRingFlushChanges:(CFErrorRef*) error
{
    return true;
}

-(bool) kvsRingPostRing:(CFStringRef) ringName ring:(CFDataRef) ring err:(CFErrorRef *)error
{
    CFStringRef ringKey = SOSRingKeyCreateWithName(ringName, error);
    CFMutableDictionaryRef changes = [self SOSTransportCircleTestGetChanges];
    CFDictionaryAddValue(changes, ringKey, ring);
    CFReleaseNull(ringKey);
    return true;
}

-(bool) kvssendDebugInfo:(CFStringRef) type debug:(CFTypeRef) debugInfo  err:(CFErrorRef *)error
{
    CFMutableDictionaryRef changes = [self SOSTransportCircleTestGetChanges];
    CFDictionaryAddValue(changes, type, debugInfo);
    return true;
}

-(bool) postRetirement:(CFStringRef)cName peer:(SOSPeerInfoRef)peer err:(CFErrorRef *)error
{
    CFStringRef retirement_key = SOSRetirementKeyCreateWithCircleNameAndPeer(cName, SOSPeerInfoGetPeerID(peer));
    CFDataRef retirement_data = SOSPeerInfoCopyEncodedData(peer, kCFAllocatorDefault, error);

    if (retirement_key)
        [self testAddToChanges:retirement_key data:retirement_data];
    
    CFReleaseNull(retirement_key);
    CFReleaseNull(retirement_data);
    return true;
}

-(bool) flushChanges:(CFErrorRef *)error
{
    return true;
}

-(void) testAddToChanges:(CFStringRef) message_key data:(CFDataRef) message_data
{
    if (self.pending_changes == NULL) {
        self.pending_changes = [NSMutableDictionary dictionary];
    }
    if (message_data == NULL) {
        [self.pending_changes setObject:[NSNull null] forKey:(__bridge NSString*)(message_key)];
    } else {
        [self.pending_changes setObject:(__bridge NSData*)message_data forKey:(__bridge NSString*)message_key];
    }
    secnotice("circle-changes", "Adding circle change %@ %@->%@", self.accountName, message_key, message_data);
}

-(void) SOSTransportCircleTestAddBulkToChanges:(CFDictionaryRef) updates
{
    if (self.pending_changes == NULL) {
        self.pending_changes = [[NSMutableDictionary alloc]initWithDictionary:(__bridge NSDictionary * _Nonnull)(updates)];
        
    }
    else{
        CFDictionaryForEach(updates, ^(const void *key, const void *value) {
            [self.pending_changes setObject:(__bridge id _Nonnull)value forKey:(__bridge id _Nonnull)key];
        });
    }
}


-(bool) expireRetirementRecords:(CFDictionaryRef) retirements err:(CFErrorRef *)error
{
    bool success = true;
    CFMutableDictionaryRef keysToWrite = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFDictionaryForEach(retirements, ^(const void *key, const void *value) {
        if (isString(key) && isArray(value)) {
            CFStringRef circle_name = (CFStringRef) key;
            CFArrayRef retirees = (CFArrayRef) value;
            
            CFArrayForEach(retirees, ^(const void *value) {
                if (isString(value)) {
                    CFStringRef retiree_id = (CFStringRef) value;
                    
                    CFStringRef kvsKey = SOSRetirementKeyCreateWithCircleNameAndPeer(circle_name, retiree_id);
                    
                    CFDictionaryAddValue(keysToWrite, kvsKey, kCFNull);
                    
                    CFReleaseSafe(kvsKey);
                }
            });
        }
    });
    
    if(CFDictionaryGetCount(keysToWrite)) {
        [self SOSTransportCircleTestAddBulkToChanges:keysToWrite];
    }
    CFReleaseNull(keysToWrite);
    
    return success;
}

bool SOSTransportCircleTestRemovePendingChange(SOSCircleStorageTransportTest* transport,  CFStringRef circleName, CFErrorRef *error){
    CFStringRef circle_key = SOSCircleKeyCreateWithName(circleName, error);
    if (circle_key)
        [transport.pending_changes removeObjectForKey:(__bridge NSString*)circle_key];
    CFReleaseNull(circle_key);
    return true;
}

-(bool) postCircle:(CFStringRef)cName circleData:(CFDataRef)circle_data err:(CFErrorRef *)error
{
    CFStringRef circle_key = SOSCircleKeyCreateWithName(cName, error);
    if (circle_key)
        [self testAddToChanges:circle_key data:circle_data];
    CFReleaseNull(circle_key);
    
    return true;
}

-(CFDictionaryRef)handleRetirementMessages:(CFMutableDictionaryRef) circle_retirement_messages_table err:(CFErrorRef *)error
{
    return SOSAccountHandleRetirementMessages(self.account, circle_retirement_messages_table, error);
}

-(CFArrayRef)CF_RETURNS_RETAINED handleCircleMessagesAndReturnHandledCopy:(CFMutableDictionaryRef) circle_circle_messages_table err:(CFErrorRef *)error
{
CFMutableArrayRef handledKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryForEach(circle_circle_messages_table, ^(const void *key, const void *value) {
        CFErrorRef circleMessageError = NULL;
        if (!SOSAccountHandleCircleMessage(self.account, key, value, &circleMessageError)) {
            secerror("Error handling circle message %@ (%@): %@", key, value, circleMessageError);
        }
        else{
            CFStringRef circle_id = (CFStringRef) key;
            CFArrayAppendValue(handledKeys, circle_id);
        }
        CFReleaseNull(circleMessageError);
    });
    
    return handledKeys;
}

SOSAccount* SOSTransportCircleTestGetAccount(SOSCircleStorageTransportTest* transport) {
    return transport.account;
}

@end

///
//MARK KVS Message Test Transport
///



@implementation SOSMessageKVSTest


-(id) initWithAccount:(SOSAccount*)acct andName:(CFStringRef)n andCircleName:(CFStringRef) cN
{
    self = [super init];
    if(self){
        self.engine = SOSDataSourceFactoryGetEngineForDataSourceName(acct.factory, cN, NULL);
        self.account = acct;
        self.changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        self.name = CFRetainSafe(n);
        self.circleName = (__bridge NSString*)cN;

        if(!message_transports)
            message_transports = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
        CFArrayAppendValue(message_transports, (__bridge const void *)((SOSMessageKVSTest*)self));
        SOSRegisterTransportMessage((SOSMessageKVSTest*)self);
    }

    return self;
}

- (void)setChanges:(CFMutableDictionaryRef)changes
{
    CFRetainAssign(self->_changes, changes);
}

-(CFIndex) SOSTransportMessageGetTransportType
{
    return kKVSTest;
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
    return self.account;
}

void SOSTransportMessageKVSTestSetName(SOSMessageKVSTest* transport, CFStringRef n)
{
    transport.name = n;
}

CFStringRef SOSTransportMessageKVSTestGetName(SOSMessageKVSTest* transport)
{
    return transport.name;
}

CFMutableDictionaryRef SOSTransportMessageKVSTestGetChanges(SOSMessageKVSTest* transport)
{
    return transport.changes;
}

void SOSTransportMessageTestClearChanges(SOSMessageKVSTest* transport)
{
    transport.changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
}

static void SOSTransportMessageTestAddBulkToChanges(SOSMessageKVSTest* transport, CFDictionaryRef updates)
{
#ifndef __clang_analyzer__ // The analyzer thinks transport.changes is a leak, but I don't know why.
    if (transport.changes == NULL) {
        transport.changes = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(updates), updates);
    }
    else{
        CFDictionaryForEach(updates, ^(const void *key, const void *value) {
            CFDictionarySetValue(transport.changes, key, value);
        });
    }
#endif // __clang_analyzer__
}

static void SOSTransportMessageTestAddToChanges(SOSMessageKVSTest* transport, CFStringRef message_key, CFDataRef message_data)
{
    if (transport.changes == NULL) {
        transport.changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    }
    if (message_data == NULL) {
        CFDictionarySetValue(transport.changes, message_key, kCFNull);
    } else {
        CFDictionarySetValue(transport.changes, message_key, message_data);
    }
}

-(bool) SOSTransportMessageCleanupAfterPeerMessages:(SOSMessageKVSTest*) transport peers:(CFDictionaryRef)circle_to_peer_ids err:(CFErrorRef*) error
{
    if(!transport.engine)
        return true;
    CFArrayRef enginePeers = SOSEngineGetPeerIDs((SOSEngineRef)transport.engine);
    
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
                            SOSTransportMessageTestAddToChanges(transport, kvsKey, NULL);
                            CFReleaseSafe(kvsKey);
                            
                            kvsKey = SOSMessageKeyCreateWithCircleNameAndPeerNames(circle_name, in_circle_id, cleanup_id);
                            SOSTransportMessageTestAddToChanges(transport, kvsKey, NULL);
                            CFReleaseSafe(kvsKey);
                        }
                    });
                    
                }
            });
        }
    });

    return [transport SOSTransportMessageFlushChanges:transport err:error];
    return true;
}

static bool sendToPeer(SOSMessageKVSTest* transport, CFStringRef circleName, CFStringRef peerID, CFDataRef message, CFErrorRef *error) {
    bool result = true;
    NSString* myID = transport.account.peerID;
    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer((SOSMessage*)transport, (__bridge CFStringRef) myID, peerID);
    CFDictionaryRef a_message_to_a_peer = CFDictionaryCreateForCFTypes(NULL, message_to_peer_key, message, NULL);
    
    SOSTransportMessageTestAddBulkToChanges(transport, a_message_to_a_peer);
    CFReleaseNull(a_message_to_a_peer);
    CFReleaseNull(message_to_peer_key);
    
    return result;
}

-(bool) SOSTransportMessageSyncWithPeers:(SOSMessageKVSTest*) transport p:(CFSetRef) peers err:(CFErrorRef *)error
{
    // Each entry is keyed by circle name and contains a list of peerIDs
        
    __block bool result = true;
    
    CFSetForEach(peers, ^(const void *value) {
        CFStringRef peerID = asString(value, NULL);

        if (peerID) {
            SOSEngineWithPeerID((SOSEngineRef)transport.engine, peerID, error, ^(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
                SOSEnginePeerMessageSentCallback* sentCallback = NULL;
                CFDataRef message_to_send = NULL;
                bool ok = SOSPeerCoderSendMessageIfNeeded([transport SOSTransportMessageGetAccount], (SOSEngineRef)transport.engine, txn, peer, coder, &message_to_send, peerID, false, &sentCallback, error);
                if (message_to_send)    {
                    CFDictionaryRef peer_dict = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, peerID, message_to_send, NULL);
                    CFDictionarySetValue(SOSTransportMessageKVSTestGetChanges(transport), (__bridge CFStringRef)self->circleName, peer_dict);
                    SOSEngineMessageCallCallback(sentCallback, ok);
                    CFReleaseSafe(peer_dict);
                }

                SOSEngineFreeMessageCallback(sentCallback);
                CFReleaseSafe(message_to_send);
            });
         }
    });
    return result;
}

-(bool) SOSTransportMessageSendMessages:(SOSMessageKVSTest*) transport pm:(CFDictionaryRef) peer_messages err:(CFErrorRef *)error
{
    __block bool result = true;
    
    CFDictionaryForEach(peer_messages, ^(const void *key, const void *value) {
        if (isString(key) && isData(value)) {
            CFStringRef peerID = (CFStringRef) key;
            CFDataRef message = (CFDataRef) value;
            bool rx = sendToPeer(transport, (__bridge CFStringRef)transport->circleName, peerID, message, error);
            result &= rx;
        }
    });

    return result;
}

-(CFDictionaryRef)CF_RETURNS_RETAINED SOSTransportMessageHandlePeerMessageReturnsHandledCopy:(SOSMessageIDSTest*) transport peerMessages:(CFMutableDictionaryRef) circle_peer_messages_table err:(CFErrorRef *)error{
    CFMutableDictionaryRef handled = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef peerToMessage = CFDictionaryGetValue(circle_peer_messages_table, (__bridge CFStringRef)transport.circleName);
    CFMutableArrayRef handled_peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(handled, (__bridge CFStringRef)transport.circleName, handled_peers);
    
    if(peerToMessage){
        CFDictionaryForEach(peerToMessage, ^(const void *key, const void *value) {
            CFStringRef peer_id = (CFStringRef) key;
            CFDataRef peer_message = (CFDataRef) value;
            CFErrorRef localError = NULL;

            if ([transport SOSTransportMessageHandlePeerMessage:transport id:peer_id cm:peer_message err:&localError]){
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

-(bool) SOSTransportMessageFlushChanges:(SOSMessageKVSTest*) transport err:(CFErrorRef *)error
{
    return true;
}

SOSAccount* SOSTransportMessageKVSTestGetAccount(SOSMessageKVSTest* transport) {
    return transport.account;
}
@end

///
//MARK IDS Message Test Transport
///

@implementation SOSMessageIDSTest
-(SOSMessageIDSTest*) initWithAccount:(SOSAccount*)acct andAccountName:(CFStringRef) aN andCircleName:(CFStringRef) cN err:(CFErrorRef *)error
{
    self = [super init];
    if(self){
        self.engine = SOSDataSourceFactoryGetEngineForDataSourceName(acct.factory, cN, NULL);
        self.useFragmentation = kCFBooleanTrue;
        self.account = acct;
        self.changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        self.circleName = (__bridge NSString*)cN;
        self.accountName = CFRetainSafe(aN);
        
        if(!message_transports)
            message_transports = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
        CFArrayAppendValue(message_transports, (__bridge CFTypeRef)self);
        SOSRegisterTransportMessage((SOSMessageIDSTest*)self);
    }
    
    return self;
}

-(bool) SOSTransportMessageIDSGetIDSDeviceID:(SOSAccount*)acct{
    return true;
}

-(CFIndex) SOSTransportMessageGetTransportType
{
    return kIDSTest;
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
    return self.account;
}

void SOSTransportMessageIDSTestSetName(SOSMessageIDSTest* transport, CFStringRef acctName){
    transport.accountName = acctName;
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
const char *kMessageKeyIDSDataMessage = "idsDataMessage";
const char *kMessageKeyDeviceID = "deviceID";
const char *kMessageKeyPeerID = "peerID";
const char *kMessageKeySendersPeerID = "sendersPeerID";

-(HandleIDSMessageReason) SOSTransportMessageIDSHandleMessage:(SOSAccount*)acct m:(CFDictionaryRef) message err:(CFErrorRef *)error
{
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
    CFReleaseNull(ourPeerIdKey);
    CFReleaseNull(sendersPeerIDKey);
    CFReleaseNull(deviceIDKey);
    CFReleaseNull(dataKey);
    return result;
}

-(CFDictionaryRef)CF_RETURNS_RETAINED SOSTransportMessageHandlePeerMessageReturnsHandledCopy:(SOSMessageIDSTest*) transport peerMessages:(CFMutableDictionaryRef)message err:(CFErrorRef *)error
{

    CFMutableDictionaryRef handled = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef peerToMessage = CFDictionaryGetValue(message, (__bridge CFTypeRef)(transport.circleName));
    CFMutableArrayRef handled_peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    secerror("Received IDS message!");
    if(peerToMessage){
        CFDictionaryForEach(peerToMessage, ^(const void *key, const void *value) {
            CFStringRef peer_id = asString(key, NULL);
            CFDataRef peer_message = asData(value, NULL);
            CFErrorRef localError = NULL;

            if (peer_id && peer_message && [transport SOSTransportMessageHandlePeerMessage:transport id:peer_id cm:peer_message err:&localError]) {
                CFArrayAppendValue(handled_peers, key);
            } else {
                secnotice("transport", "%@ KVSTransport handle message failed: %@", peer_id, localError);
            }
            CFReleaseNull(localError);
        });
    }
    CFDictionaryAddValue(handled, (__bridge CFStringRef)(transport.circleName), handled_peers);
    CFReleaseNull(handled_peers);

    return handled;
}

static void SOSTransportMessageIDSTestAddBulkToChanges(SOSMessageIDSTest* transport, CFDictionaryRef updates){
#ifndef __clang_analyzer__ // The analyzer thinks transport.changes is a leak, but I don't know why.
    if (transport.changes == NULL) {
        transport.changes = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(updates), updates);

    }
    else{
        CFDictionaryForEach(updates, ^(const void *key, const void *value) {
            CFDictionaryAddValue(transport.changes, key, value);
        });
    }
#endif
}
static bool sendDataToPeerIDSTest(SOSMessageIDSTest* transport, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID, CFDataRef message, CFErrorRef *error)
{    
    secerror("sending message through test transport: %@", message);
    NSString* myID = transport.account.peerID;
    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer(transport, (__bridge CFStringRef) myID, peerID);
    CFDictionaryRef a_message_to_a_peer = CFDictionaryCreateForCFTypes(NULL, message_to_peer_key, message, NULL);

    SOSTransportMessageIDSTestAddBulkToChanges(transport, a_message_to_a_peer);

    CFReleaseNull(message_to_peer_key);
    CFReleaseNull(a_message_to_a_peer);
    return true;

}
static bool sendDictionaryToPeerIDSTest(SOSMessageIDSTest* transport, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID, CFDictionaryRef message, CFErrorRef *error)
{
    secerror("sending message through test transport: %@", message);
    NSString* myID = transport.account.peerID;
    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer(transport, (__bridge CFStringRef) myID, peerID);
    CFDictionaryRef a_message_to_a_peer = CFDictionaryCreateForCFTypes(NULL, message_to_peer_key, message, NULL);
    
    SOSTransportMessageIDSTestAddBulkToChanges(transport, a_message_to_a_peer);
    
    CFReleaseNull(message_to_peer_key);
    CFReleaseNull(a_message_to_a_peer);
    return true;
    
}

-(bool) SOSTransportMessageSyncWithPeers:(SOSMessageIDSTest*) transport p:(CFSetRef)peers err:(CFErrorRef *)error
{
    // Each entry is keyed by circle name and contains a list of peerIDs
    __block bool result = true;

    CFSetForEach(peers, ^(const void *value) {
        CFStringRef peerID = asString(value, NULL);
        if (peerID) {
            secnotice("transport", "IDS sync with peerIDs %@", peerID);
            result &= [transport SOSTransportMessageSendMessageIfNeeded:transport id:(__bridge CFStringRef)transport.circleName pID:peerID err:error];
        }
    });

    return result;
}

-(bool) SOSTransportMessageSendMessages:(SOSMessageIDSTest*) transport pm:(CFDictionaryRef) peer_messages err:(CFErrorRef *)error
{
    __block bool result = true;
    CFDictionaryForEach(peer_messages, ^(const void *key, const void *value) {
        CFStringRef idsDeviceID = NULL;;
        CFStringRef peerID = asString(key, NULL);
        SOSPeerInfoRef pi = NULL;
        if(peerID){
            if(!CFEqualSafe(peerID, (__bridge CFStringRef) transport.account.peerID)){

                pi = SOSAccountCopyPeerWithID(transport.account, peerID, NULL);
                if(pi){
                    idsDeviceID = SOSPeerInfoCopyDeviceID(pi);
                    if(idsDeviceID != NULL){

                        CFDictionaryRef messageDictionary = asDictionary(value, NULL);
                        if (messageDictionary) {
                            result &= sendDictionaryToPeerIDSTest(transport, (__bridge CFStringRef)transport.circleName, idsDeviceID, peerID, messageDictionary, error);
                        } else {
                            CFDataRef messageData = asData(value, NULL);
                            if (messageData) {
                                result &= sendDataToPeerIDSTest(transport, (__bridge CFStringRef)transport.circleName, idsDeviceID, peerID, messageData, error);
                            }
                        }
                    }
                }
            }
        }
        CFReleaseNull(idsDeviceID);
        CFReleaseNull(pi);
    });
    
    return result;
}

-(bool) SOSTransportMessageFlushChanges:(SOSMessageIDSTest*) transport err:(CFErrorRef *)error
{
    return true;
}

-(bool) SOSTransportMessageCleanupAfterPeerMessages:(SOSMessageIDSTest*) transport peers:(CFDictionaryRef) peers err:(CFErrorRef*) error
{
    return true;
}

void SOSTransportMessageIDSTestClearChanges(SOSMessageIDSTest* transport)
{
    transport.changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
}

CFMutableDictionaryRef SOSTransportMessageIDSTestGetChanges(SOSMessageIDSTest* transport)
{
    return transport.changes;
}

SOSAccount* SOSTransportMessageIDSTestGetAccount(SOSMessageIDSTest* transport)
{
    return transport.account;
}

CFStringRef SOSTransportMessageIDSTestGetName(SOSMessageIDSTest* transport){
    return transport.accountName;
}

@end


void SOSAccountUpdateTestTransports(SOSAccount* account, CFDictionaryRef gestalt){
    CFStringRef new_name = (CFStringRef)CFDictionaryGetValue(gestalt, kPIUserDefinedDeviceNameKey);

    SOSTransportKeyParameterTestSetName((CKKeyParameterTest*)account.key_transport, new_name);
    SOSTransportCircleTestSetName((SOSCircleStorageTransportTest*)account.circle_transport, new_name);
    SOSTransportMessageKVSTestSetName((SOSMessageKVSTest*)account.kvs_message_transport, new_name);
    SOSTransportMessageIDSTestSetName((SOSMessageIDSTest*)account.ids_message_transport, new_name);

}

static CF_RETURNS_RETAINED SOSCircleRef SOSAccountEnsureCircleTest(SOSAccount* a, CFStringRef name, CFStringRef accountName)
{
    CFErrorRef localError = NULL;
    SOSAccountTrustClassic *trust = a.trust;

    SOSCircleRef circle = CFRetainSafe([a.trust getCircle:&localError]);
    if(!circle || isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle)){
        secnotice("circle", "Error retrieving the circle: %@", localError);
        CFReleaseNull(localError);
        CFReleaseNull(circle);

        circle = SOSCircleCreate(kCFAllocatorDefault, name, &localError);
        if (circle){
            [trust setTrustedCircle:circle];
        }
        else{
            secnotice("circle", "Could not create circle: %@", localError);
        }
        CFReleaseNull(localError);
    }

    if(![trust ensureFullPeerAvailable:(__bridge CFDictionaryRef)(a.gestalt) deviceID:(__bridge CFStringRef)(a.deviceID) backupKey:(__bridge CFDataRef)(a.backup_key) err:&localError])
    {
        secnotice("circle", "had an error building full peer: %@", localError);
        CFReleaseNull(localError);
        return circle;
    }

    CFReleaseNull(localError);
    return circle;
}

bool SOSAccountEnsureFactoryCirclesTest(SOSAccount* a, CFStringRef accountName)
{
    bool result = false;
    if (a)
    {
        if(!a.factory)
            return result;
        CFStringRef circle_name = SOSDataSourceFactoryCopyName(a.factory);
        if(!circle_name)
            return result;
        CFReleaseSafe(SOSAccountEnsureCircleTest(a, (CFStringRef)circle_name, accountName));

        CFReleaseNull(circle_name);
        result = true;
    }
    return result;
}

bool SOSAccountInflateTestTransportsForCircle(SOSAccount* account, CFStringRef circleName, CFStringRef accountName, CFErrorRef *error){
    bool success = false;

    if(account.key_transport == nil){
        account.key_transport = (CKKeyParameter*)[[CKKeyParameterTest alloc] initWithAccount:account andName:accountName andCircleName:circleName];
        require_quiet(account.key_transport, fail);
    }

    if(account.circle_transport == nil){
        account.circle_transport = (SOSKVSCircleStorageTransport*)[[SOSCircleStorageTransportTest alloc] initWithAccount:account andWithAccountName:accountName andCircleName:circleName];
        require_quiet(account.circle_transport, fail);
    }
    if(account.kvs_message_transport == nil){
        account.kvs_message_transport = (SOSMessageKVS*)[[SOSMessageKVSTest alloc] initWithAccount:account andName:accountName andCircleName:circleName];
    }
    if(account.ids_message_transport == nil){
        account.ids_message_transport = (SOSMessageIDS*)[[SOSMessageIDSTest alloc] initWithAccount:account andAccountName:accountName andCircleName:circleName err:NULL];
    }

    success = true;
fail:
    return success;
}


