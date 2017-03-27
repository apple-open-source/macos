#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSPeerCoder.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>

#include "SOSTransportTestTransports.h"

static bool sendToPeer(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef peerID, CFDataRef message, CFErrorRef *error);
static bool syncWithPeers(SOSTransportMessageRef transport, CFSetRef peers, CFErrorRef *error);
static bool sendMessages(SOSTransportMessageRef transport, CFDictionaryRef circleToPeersToMessage, CFErrorRef *error);
static bool cleanupAfterPeer(SOSTransportMessageRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error);
static CF_RETURNS_RETAINED
CFDictionaryRef handleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);
static void destroyMessageTransport(SOSTransportMessageRef transport);
static void SOSTransportMessageTestAddBulkToChanges(SOSTransportMessageTestRef transport, CFDictionaryRef updates);

static bool flushMessageChanges(SOSTransportMessageRef transport, CFErrorRef *error);
static bool publishCloudParameters(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error);
static bool SOSTransportKeyParameterTestPublishCloudParameters(SOSTransportKeyParameterTestRef transport, CFDataRef newParameters, CFErrorRef *error);

static bool expireRetirementRecords(SOSTransportCircleRef transport, CFDictionaryRef retirements, CFErrorRef *error);
static void destroy(SOSTransportCircleRef transport);
static inline bool flushChanges(SOSTransportCircleRef transport, CFErrorRef *error);
static bool postCircle(SOSTransportCircleRef transport,  CFStringRef circleName, CFDataRef circle_data, CFErrorRef *error);
static inline bool postRetirement(SOSTransportCircleRef transport,  CFStringRef circleName, CFStringRef peer_id, CFDataRef retirement_data, CFErrorRef *error);
static bool handleKeyParameterChanges(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef error);

static CFArrayRef handleCircleMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_circle_messages_table, CFErrorRef *error);
static CFDictionaryRef handleRetirementMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error);
static bool setToNewAccount(SOSTransportKeyParameterRef transport, SOSAccountRef account);
static void destroyKeyParameters(SOSTransportKeyParameterRef transport);

static void SOSTransportCircleTestAddBulkToChanges(SOSTransportCircleTestRef transport, CFDictionaryRef updates);
static void SOSTransportCircleTestAddToChanges(SOSTransportCircleTestRef transport, CFStringRef message_key, CFDataRef message_data);
static bool SOSTransportCircleTestSendChanges(SOSTransportCircleTestRef transport, CFDictionaryRef changes, CFErrorRef *error);

static bool sendPeerInfo(SOSTransportCircleRef transport, CFStringRef peerID, CFDataRef peerInfoData, CFErrorRef *error);
static bool flushRingChanges(SOSTransportCircleRef transport, CFErrorRef* error);
static bool postRing(SOSTransportCircleRef transport, CFStringRef ringName, CFDataRef ring, CFErrorRef *error);
static bool sendDebugInfo(SOSTransportCircleRef transport, CFStringRef type, CFTypeRef debugInfo, CFErrorRef *error);

CFMutableArrayRef key_transports = NULL;
CFMutableArrayRef circle_transports = NULL;
CFMutableArrayRef message_transports = NULL;

void SOSAccountUpdateTestTransports(SOSAccountRef account, CFDictionaryRef gestalt){
    CFStringRef new_name = (CFStringRef)CFDictionaryGetValue(gestalt, kPIUserDefinedDeviceNameKey);

    SOSTransportKeyParameterTestSetName((SOSTransportKeyParameterTestRef)account->key_transport, new_name);
    SOSTransportCircleTestSetName((SOSTransportCircleTestRef)account->circle_transport, new_name);
    SOSTransportMessageTestSetName((SOSTransportMessageTestRef)account->kvs_message_transport, new_name);

}

static SOSCircleRef SOSAccountEnsureCircleTest(SOSAccountRef a, CFStringRef name, CFStringRef accountName, CFErrorRef *error)
{
    CFErrorRef localError = NULL;
    
    SOSCircleRef circle = SOSAccountGetCircle(a, &localError);
    
    require_action_quiet(circle || !isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle), fail,
                         if (error) { *error = localError; localError = NULL; });
    
    if(NULL == circle){
        circle = SOSCircleCreate(NULL, name, NULL);
        if (circle){
            CFRetainAssign(a->trusted_circle, circle);
            CFRelease(circle);
            circle = SOSAccountGetCircle(a, &localError);
        }
    }
    require_quiet(SOSAccountInflateTestTransportsForCircle(a, name, accountName, &localError), fail);
    require_quiet(SOSAccountHasFullPeerInfo(a, &localError), fail);

fail:
    CFReleaseNull(localError);
    return circle;
}

bool SOSAccountEnsureFactoryCirclesTest(SOSAccountRef a, CFStringRef accountName)
{
    bool result = false;
    if (a)
    {
        require(a->factory, xit);
        CFStringRef circle_name = SOSDataSourceFactoryCopyName(a->factory);
        require(circle_name, xit);

        SOSAccountEnsureCircleTest(a, (CFStringRef)circle_name, accountName, NULL);

        CFReleaseNull(circle_name);
        result = true;
    }
xit:
    return result;
}

bool SOSAccountInflateTestTransportsForCircle(SOSAccountRef account, CFStringRef circleName, CFStringRef accountName, CFErrorRef *error){
    bool success = false;
    SOSTransportCircleTestRef tCircle = NULL;
    SOSTransportMessageTestRef tMessage = NULL;
    
    if(account->key_transport == NULL){
        account->key_transport = (SOSTransportKeyParameterRef)SOSTransportTestCreateKeyParameter(account, accountName, circleName);
        require_quiet(account->key_transport, fail);
    }
    if(account->circle_transport == NULL){
        tCircle = SOSTransportTestCreateCircle(account, accountName, circleName);
        require_quiet(tCircle, fail);
        CFRetainAssign(account->circle_transport, (SOSTransportCircleRef)tCircle);
    }
    if(account->kvs_message_transport == NULL){
        tMessage = SOSTransportTestCreateMessage(account, accountName, circleName);
        require_quiet(tMessage, fail);
        CFRetainAssign(account->kvs_message_transport, (SOSTransportMessageRef)tMessage);
    }
    
    success = true;
fail:
    CFReleaseNull(tCircle);
    CFReleaseNull(tMessage);
    return success;
}

///
//Mark Test Key Parameter Transport
///

struct SOSTransportKeyParameterTest{
    struct __OpaqueSOSTransportKeyParameter k;
    CFMutableDictionaryRef changes;
    CFStringRef name;
    CFStringRef circleName;
};

SOSTransportKeyParameterTestRef SOSTransportTestCreateKeyParameter(SOSAccountRef account, CFStringRef name, CFStringRef circleName){
    
    SOSTransportKeyParameterTestRef tpt = (SOSTransportKeyParameterTestRef) SOSTransportKeyParameterCreateForSubclass(sizeof(struct SOSTransportKeyParameterTest) - sizeof(CFRuntimeBase), account, NULL);

    tpt->name = CFRetainSafe(name);
    tpt->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    tpt->k.account = CFRetainSafe(account);
    tpt->k.destroy = destroyKeyParameters;
    tpt->circleName = CFRetainSafe(circleName);
    tpt->k.publishCloudParameters = publishCloudParameters;
    tpt->k.handleKeyParameterChanges = handleKeyParameterChanges;
    tpt->k.setToNewAccount = setToNewAccount;
    if(!key_transports)
        key_transports = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
    CFArrayAppendValue(key_transports, (SOSTransportKeyParameterRef)tpt);

    SOSRegisterTransportKeyParameter((SOSTransportKeyParameterRef)tpt);
    return tpt;
}

static bool handleKeyParameterChanges(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef error){
    SOSAccountRef account = transport->account;
    return SOSAccountHandleParametersChange(account, data, &error);
}

static void destroyKeyParameters(SOSTransportKeyParameterRef transport){
    SOSTransportKeyParameterTestRef tpt = (SOSTransportKeyParameterTestRef)transport;

    CFArrayRemoveAllValue(key_transports, tpt);
    SOSUnregisterTransportKeyParameter(transport);

    CFReleaseNull(tpt->changes);
    CFReleaseNull(tpt->name);
    CFReleaseNull(tpt->circleName);
}


static bool setToNewAccount(SOSTransportKeyParameterRef transport, SOSAccountRef a){
    SOSTransportKeyParameterTestRef tpt = (SOSTransportKeyParameterTestRef)transport;
    CFStringRef accountName = SOSTransportKeyParameterTestGetName(tpt);

    SOSAccountSetToNew(a);
    
    CFReleaseNull(a->key_transport);
    CFReleaseNull(a->circle_transport);
    CFReleaseNull(a->kvs_message_transport);
    CFReleaseNull(a->ids_message_transport);
    
    SOSAccountEnsureFactoryCirclesTest(a, accountName);
    
    return true;
}
CFStringRef SOSTransportKeyParameterTestGetName(SOSTransportKeyParameterTestRef transport){
    return transport->name;
}

void SOSTransportKeyParameterTestSetName(SOSTransportKeyParameterTestRef transport, CFStringRef accountName){
    CFReleaseNull(transport->name);
    transport->name = CFRetain(accountName);
}

SOSAccountRef SOSTransportKeyParameterTestGetAccount(SOSTransportKeyParameterTestRef transport){
    return ((SOSTransportKeyParameterRef)transport)->account;
}

CFMutableDictionaryRef SOSTransportKeyParameterTestGetChanges(SOSTransportKeyParameterTestRef transport){
    return transport->changes;
}

void SOSTransportKeyParameterTestClearChanges(SOSTransportKeyParameterTestRef transport){
    CFReleaseNull(transport->changes);
    transport->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
}

static bool publishCloudParameters(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef* error)
{
    return SOSTransportKeyParameterTestPublishCloudParameters((SOSTransportKeyParameterTestRef)transport, data, error);
}

static bool SOSTransportKeyParameterTestPublishCloudParameters(SOSTransportKeyParameterTestRef transport, CFDataRef newParameters, CFErrorRef *error)
{
    if(!transport->changes)
        transport->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFDictionarySetValue(transport->changes, kSOSKVSKeyParametersKey, newParameters);
    
    return true;
}

///
//MARK: Test Circle Transport
///
struct SOSTransportCircleTest{
    struct __OpaqueSOSTransportCircle c;
    CFMutableDictionaryRef changes;
    CFStringRef name;
    CFStringRef circleName;
};
static CFStringRef SOSTransportCircleCopyDescription(SOSTransportCircleTestRef transport) {
    
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSTransportCircle@%p\n>"), transport);
}

static CFStringRef copyDescription(SOSTransportCircleRef transport){
    return SOSTransportCircleCopyDescription((SOSTransportCircleTestRef)transport);
}

CFStringRef SOSTransportCircleTestGetName(SOSTransportCircleTestRef transport){
    return transport->name;
}
void SOSTransportCircleTestSetName(SOSTransportCircleTestRef transport, CFStringRef accountName){
    CFReleaseNull(transport->name);
    transport->name = CFRetain(accountName);
}

CFMutableDictionaryRef SOSTransportCircleTestGetChanges(SOSTransportCircleTestRef transport){
    return transport->changes;
}

void SOSTransportCircleTestClearChanges(SOSTransportCircleTestRef transport){
    CFReleaseNull(transport->changes);
    transport->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
}

SOSTransportCircleTestRef SOSTransportTestCreateCircle(SOSAccountRef account, CFStringRef name, CFStringRef circleName){
    SOSTransportCircleTestRef tpt = (SOSTransportCircleTestRef) SOSTransportCircleCreateForSubclass(sizeof(struct SOSTransportCircleTest) - sizeof(CFRuntimeBase), account, NULL);

    if(tpt){
        tpt->c.account = CFRetainSafe(account);
        tpt->c.copyDescription = copyDescription;
        tpt->c.expireRetirementRecords = expireRetirementRecords;
        tpt->c.postCircle = postCircle;
        tpt->c.postRetirement = postRetirement;
        tpt->c.flushChanges = flushChanges;
        tpt->c.handleRetirementMessages = handleRetirementMessages;
        tpt->c.handleCircleMessages = handleCircleMessages;
        tpt->c.destroy = destroy;
        tpt->c.flushRingChanges = flushRingChanges;
        tpt->c.postRing = postRing;
        tpt->c.sendDebugInfo = sendDebugInfo;
        tpt->c.sendPeerInfo = sendPeerInfo;
        tpt->changes  = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        tpt->name = CFRetainSafe(name);
        tpt->circleName = CFRetainSafe(circleName);
        if(!circle_transports)
            circle_transports = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
        CFArrayAppendValue(circle_transports, (SOSTransportCircleRef)tpt);

        SOSRegisterTransportCircle((SOSTransportCircleRef)tpt);
    }
    
    return tpt;
}

static void destroy(SOSTransportCircleRef transport){
    SOSTransportCircleTestRef tkvs = (SOSTransportCircleTestRef)transport;
    CFArrayRemoveAllValue(circle_transports, tkvs);

    SOSUnregisterTransportCircle(transport);

    CFReleaseNull(tkvs->changes);
    CFReleaseNull(tkvs->name);
    CFReleaseNull(tkvs->circleName);
}

static bool sendPeerInfo(SOSTransportCircleRef transport, CFStringRef peerID, CFDataRef peerInfoData, CFErrorRef *error){
    SOSTransportCircleTestRef testTransport = (SOSTransportCircleTestRef)transport;
    CFMutableDictionaryRef changes = SOSTransportCircleTestGetChanges(testTransport);
    CFDictionaryAddValue(changes, peerID, peerInfoData);
    return true;
}
static bool flushRingChanges(SOSTransportCircleRef transport, CFErrorRef* error){
    return true;
}
static bool postRing(SOSTransportCircleRef transport, CFStringRef ringName, CFDataRef ring, CFErrorRef *error){
    SOSTransportCircleTestRef testTransport = (SOSTransportCircleTestRef)transport;
    CFStringRef ringKey = SOSRingKeyCreateWithName(ringName, error);
    CFMutableDictionaryRef changes = SOSTransportCircleTestGetChanges(testTransport);
    CFDictionaryAddValue(changes, ringKey, ring);
    CFReleaseNull(ringKey);
    return true;
}

static bool sendDebugInfo(SOSTransportCircleRef transport, CFStringRef type, CFTypeRef debugInfo, CFErrorRef *error){
    SOSTransportCircleTestRef testTransport = (SOSTransportCircleTestRef)transport;
    CFMutableDictionaryRef changes = SOSTransportCircleTestGetChanges(testTransport);
    CFDictionaryAddValue(changes, type, debugInfo);
    return true;
}

static inline bool postRetirement(SOSTransportCircleRef transport,  CFStringRef circleName, CFStringRef peer_id, CFDataRef retirement_data, CFErrorRef *error)
{
    CFStringRef retirement_key = SOSRetirementKeyCreateWithCircleNameAndPeer(circleName, peer_id);
    if (retirement_key)
        SOSTransportCircleTestAddToChanges((SOSTransportCircleTestRef)transport, retirement_key, retirement_data);
    
    CFReleaseNull(retirement_key);
    return true;
}

static inline bool flushChanges(SOSTransportCircleRef transport, CFErrorRef *error)
{
    return true;
}

static void SOSTransportCircleTestAddToChanges(SOSTransportCircleTestRef transport, CFStringRef message_key, CFDataRef message_data){
    
    if (transport->changes == NULL) {
        transport->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    }
    if (message_data == NULL) {
        CFDictionarySetValue(transport->changes, message_key, kCFNull);
    } else {
        CFDictionarySetValue(transport->changes, message_key, message_data);
    }
    secnotice("circle-changes", "Adding circle change %@ %@->%@", transport->name, message_key, message_data);
}

static void SOSTransportCircleTestAddBulkToChanges(SOSTransportCircleTestRef transport, CFDictionaryRef updates){
    
    if (transport->changes == NULL) {
        transport->changes = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(updates), updates);
        
    }
    else{
        CFDictionaryForEach(updates, ^(const void *key, const void *value) {
            CFDictionarySetValue(transport->changes, key, value);
        });
    }
}


static bool expireRetirementRecords(SOSTransportCircleRef transport, CFDictionaryRef retirements, CFErrorRef *error) {
    
    bool success = true;
    SOSTransportCircleTestRef tpt = (SOSTransportCircleTestRef)transport;
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
        SOSTransportCircleTestAddBulkToChanges(tpt, keysToWrite);
    }
    CFReleaseNull(keysToWrite);
    
    return success;
}

__unused static bool SOSTransportCircleTestUpdateRetirementRecords(SOSTransportCircleTestRef transport, CFDictionaryRef updates, CFErrorRef* error){
    CFErrorRef updateError = NULL;
    bool success = false;
    if (SOSTransportCircleTestSendChanges((SOSTransportCircleTestRef)transport, updates, &updateError)){
        success = true;
    } else {
        SOSCreateErrorWithFormat(kSOSErrorSendFailure, updateError, error, NULL,
                                 CFSTR("update parameters key failed [%@]"), updates);
    }
    return success;
}

bool SOSTransportCircleTestRemovePendingChange(SOSTransportCircleRef transport,  CFStringRef circleName, CFErrorRef *error){
    SOSTransportCircleTestRef tkvs = (SOSTransportCircleTestRef)transport;
    CFStringRef circle_key = SOSCircleKeyCreateWithName(circleName, error);
    if (circle_key)
        CFDictionaryRemoveValue(tkvs->changes, circle_key);
    CFReleaseNull(circle_key);
    return true;
}

static bool postCircle(SOSTransportCircleRef transport,  CFStringRef circleName, CFDataRef circle_data, CFErrorRef *error){
    SOSTransportCircleTestRef tkvs = (SOSTransportCircleTestRef)transport;
    CFStringRef circle_key = SOSCircleKeyCreateWithName(circleName, error);
    if (circle_key)
        SOSTransportCircleTestAddToChanges(tkvs, circle_key, circle_data);
    CFReleaseNull(circle_key);
    
    return true;
}

static CF_RETURNS_RETAINED CFDictionaryRef handleRetirementMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error){
    SOSAccountRef account = transport->account;

    return SOSAccountHandleRetirementMessages(account, circle_retirement_messages_table, error);
}

static CFArrayRef handleCircleMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_circle_messages_table, CFErrorRef *error){
    CFMutableArrayRef handledKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryForEach(circle_circle_messages_table, ^(const void *key, const void *value) {
        CFErrorRef circleMessageError = NULL;
        if (!SOSAccountHandleCircleMessage(transport->account, key, value, &circleMessageError)) {
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

static bool SOSTransportCircleTestSendChanges(SOSTransportCircleTestRef transport, CFDictionaryRef changes, CFErrorRef *error){
    if(!transport->changes)
        transport->changes = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(changes), changes);
    else{
        CFDictionaryForEach(changes, ^(const void *key, const void *value) {
            CFDictionarySetValue(transport->changes, key, value);
        });
    }
    return true;
}

SOSAccountRef SOSTransportCircleTestGetAccount(SOSTransportCircleTestRef transport) {
    return ((SOSTransportCircleRef)transport)->account;
}

///
//MARK Message Test Transport
///

static CFIndex getKVSTestTransportType(SOSTransportMessageRef transport, CFErrorRef *error);


struct SOSTransportMessageTest{
    struct __OpaqueSOSTransportMessage m;
    CFMutableDictionaryRef changes;
    CFStringRef name;
    CFStringRef circleName;
};

SOSTransportMessageTestRef SOSTransportTestCreateMessage(SOSAccountRef account, CFStringRef name, CFStringRef circleName){
    SOSTransportMessageTestRef tpt = (SOSTransportMessageTestRef) SOSTransportMessageCreateForSubclass(sizeof(struct SOSTransportMessageTest) - sizeof(CFRuntimeBase), account, circleName, NULL);

    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account->factory, circleName, NULL);
    
    tpt->m.engine = CFRetainSafe(engine);
    if(tpt){
        tpt->m.sendMessages = sendMessages;
        tpt->m.syncWithPeers = syncWithPeers;
        tpt->m.flushChanges = flushMessageChanges;
        tpt->m.cleanupAfterPeerMessages = cleanupAfterPeer;
        tpt->m.destroy = destroyMessageTransport;
        tpt->m.handleMessages = handleMessages;
        // Initialize ourselves
        tpt->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        tpt->m.account = CFRetainSafe(account);
        tpt->name = CFRetainSafe(name);
        tpt->circleName = CFRetainSafe(circleName);
        tpt->m.getTransportType = getKVSTestTransportType;

        
        if(!message_transports)
            message_transports = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
        CFArrayAppendValue(message_transports, (SOSTransportMessageRef)tpt);
        SOSRegisterTransportMessage((SOSTransportMessageRef)tpt);
    }
    
    return tpt;
}

static CFIndex getKVSTestTransportType(SOSTransportMessageRef transport, CFErrorRef *error){
    return kKVSTest;
}
CFStringRef SOSTransportMessageTestGetName(SOSTransportMessageTestRef transport){
    return transport->name;
}

void SOSTransportMessageTestSetName(SOSTransportMessageTestRef transport, CFStringRef accountName){
    CFReleaseNull(transport->name);
    transport->name = CFRetain(accountName);
}

CFMutableDictionaryRef SOSTransportMessageTestGetChanges(SOSTransportMessageTestRef transport){
    return transport->changes;
}

void SOSTransportMessageTestClearChanges(SOSTransportMessageTestRef transport){
    CFReleaseNull(transport->changes);
    transport->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

}

static void destroyMessageTransport(SOSTransportMessageRef transport){
    SOSTransportMessageTestRef tkvs = (SOSTransportMessageTestRef)transport;

    SOSUnregisterTransportMessage(transport);

    CFArrayRemoveAllValue(message_transports, tkvs);

    CFReleaseNull(tkvs->circleName);
    CFReleaseNull(tkvs->changes);
    CFReleaseNull(tkvs->name);
    
}

static void SOSTransportMessageTestAddBulkToChanges(SOSTransportMessageTestRef transport, CFDictionaryRef updates){
    
    if (transport->changes == NULL) {
        transport->changes = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(updates), updates);
        
    }
    else{
        CFDictionaryForEach(updates, ^(const void *key, const void *value) {
            CFDictionarySetValue(transport->changes, key, value);
        });
    }
}

static void SOSTransportMessageTestAddToChanges(SOSTransportMessageTestRef transport, CFStringRef message_key, CFDataRef message_data){
    if (transport->changes == NULL) {
        transport->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    }
    if (message_data == NULL) {
        CFDictionarySetValue(transport->changes, message_key, kCFNull);
    } else {
        CFDictionarySetValue(transport->changes, message_key, message_data);
    }
}

static bool SOSTransportMessageTestCleanupAfterPeerMessages(SOSTransportMessageTestRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error)
{
    SOSEngineRef engine = SOSTransportMessageGetEngine((SOSTransportMessageRef)transport);
    require_quiet(engine, fail);
    
    CFArrayRef enginePeers = SOSEngineGetPeerIDs(engine);
    
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
    
    return SOSTransportMessageFlushChanges((SOSTransportMessageRef)transport, error);
fail:
    return true;
}

static bool sendToPeer(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef peerID, CFDataRef message, CFErrorRef *error) {
    SOSTransportMessageTestRef testTransport = (SOSTransportMessageTestRef) transport;
    bool result = true;
    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer(transport, peerID);
    CFDictionaryRef a_message_to_a_peer = CFDictionaryCreateForCFTypes(NULL, message_to_peer_key, message, NULL);
    
    SOSTransportMessageTestAddBulkToChanges((SOSTransportMessageTestRef)testTransport, a_message_to_a_peer);
    CFReleaseNull(a_message_to_a_peer);
    CFReleaseNull(message_to_peer_key);
    
    return result;
}

static bool syncWithPeers(SOSTransportMessageRef transport, CFSetRef peers, CFErrorRef *error){
    // Each entry is keyed by circle name and contains a list of peerIDs
        
    __block bool result = true;
    
    CFStringRef circleName = transport->circleName;
    CFSetForEach(peers, ^(const void *value) {
        CFStringRef peerID = asString(value, NULL);

        if (peerID) {
            SOSEngineRef engine = SOSTransportMessageGetEngine(transport);
            SOSEngineWithPeerID(engine, peerID, error, ^(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
                SOSEnginePeerMessageSentBlock sent = NULL;
                CFDataRef message_to_send = NULL;
                bool ok = SOSPeerCoderSendMessageIfNeeded(engine, txn, peer, coder, &message_to_send, peerID, &sent, error);
                if (message_to_send)    {
                    CFDictionaryRef peer_dict = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, peerID, message_to_send, NULL);
                    CFDictionarySetValue(SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)transport), circleName, peer_dict);
                    SOSPeerCoderConsume(&sent, ok);
                    CFReleaseSafe(peer_dict);
                }
                Block_release(sent);
                CFReleaseSafe(message_to_send);
            });
         }
    });

    return result;
}

static bool sendMessages(SOSTransportMessageRef transport, CFDictionaryRef peersToMessage, CFErrorRef *error) {
    __block bool result = true;
    
    CFStringRef circleName = transport->circleName;
    CFDictionaryForEach(peersToMessage, ^(const void *key, const void *value) {
        if (isString(key) && isData(value)) {
            CFStringRef peerID = (CFStringRef) key;
            CFDataRef message = (CFDataRef) value;
            bool rx = sendToPeer(transport, circleName, peerID, message, error);
            result &= rx;
        }
    });

    return result;
}

static CF_RETURNS_RETAINED
CFDictionaryRef handleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error) {
    SOSTransportMessageTestRef tpt = (SOSTransportMessageTestRef)transport;
    CFMutableDictionaryRef handled = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef peerToMessage = CFDictionaryGetValue(circle_peer_messages_table, tpt->circleName);
    CFMutableArrayRef handled_peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(handled, tpt->circleName, handled_peers);
    
    if(peerToMessage){
        CFDictionaryForEach(peerToMessage, ^(const void *key, const void *value) {
            CFStringRef peer_id = (CFStringRef) key;
            CFDataRef peer_message = (CFDataRef) value;
            CFErrorRef localError = NULL;
            
            if (SOSTransportMessageHandlePeerMessage((SOSTransportMessageRef) transport, peer_id, peer_message, &localError)) {
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

static bool flushMessageChanges(SOSTransportMessageRef transport, CFErrorRef *error)
{
    return true;
}

static bool cleanupAfterPeer(SOSTransportMessageRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error)
{
    return SOSTransportMessageTestCleanupAfterPeerMessages((SOSTransportMessageTestRef) transport, circle_to_peer_ids, error);
}

SOSAccountRef SOSTransportMessageTestGetAccount(SOSTransportMessageRef transport) {
    return ((SOSTransportMessageRef)transport)->account;
}


///
//MARK Message Test Transport
///

struct SOSTransportMessageIDSTest {
    struct __OpaqueSOSTransportMessage          m;
    CFBooleanRef                                useFragmentation;
    CFMutableDictionaryRef changes;
    CFStringRef name;
    CFStringRef circleName;
};

//
// V-table implementation forward declarations
//
static bool sendDataToPeerIDSTest(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID, CFDataRef message, CFErrorRef *error);
static bool sendDictionaryToPeerIDSTest(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID, CFDictionaryRef message, CFErrorRef *error);

static bool syncWithPeersIDSTest(SOSTransportMessageRef transport, CFSetRef peers, CFErrorRef *error);
static bool sendMessagesIDSTest(SOSTransportMessageRef transport, CFDictionaryRef circleToPeersToMessage, CFErrorRef *error);
static void destroyIDSTest(SOSTransportMessageRef transport);
static bool cleanupAfterPeerIDSTest(SOSTransportMessageRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error);
static bool flushChangesIDSTest(SOSTransportMessageRef transport, CFErrorRef *error);
static CF_RETURNS_RETAINED CFDictionaryRef handleMessagesIDSTest(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);
static CFStringRef copyIDSTestDescription(SOSTransportMessageRef object);
static CFIndex  getIDSTestTransportType(SOSTransportMessageRef transport, CFErrorRef *error);



SOSTransportMessageIDSTestRef SOSTransportMessageIDSTestCreate(SOSAccountRef account, CFStringRef accountName, CFStringRef circleName, CFErrorRef *error)
{

    SOSTransportMessageIDSTestRef ids = (SOSTransportMessageIDSTestRef) SOSTransportMessageCreateForSubclass(sizeof(struct SOSTransportMessageIDSTest) - sizeof(CFRuntimeBase), account, circleName, NULL);


    if (ids) {
        // Fill in vtable:
        ids->m.sendMessages = sendMessagesIDSTest;
        ids->m.syncWithPeers = syncWithPeersIDSTest;
        ids->m.flushChanges = flushChangesIDSTest;
        ids->m.cleanupAfterPeerMessages = cleanupAfterPeerIDSTest;
        ids->m.destroy = destroyIDSTest;
        ids->m.handleMessages = handleMessagesIDSTest;
        ids->m.copyDescription = copyIDSTestDescription;
        ids->m.getName = SOSTransportMessageIDSTestGetName;
        ids->m.getTransportType = getIDSTestTransportType;
        ids->useFragmentation = kCFBooleanTrue;
        
        // Initialize ourselves
        ids->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        ids->circleName = CFRetainSafe(circleName);
        ids->name = CFRetainSafe(accountName);
        
        if(!message_transports)
            message_transports = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
        CFArrayAppendValue(message_transports, (SOSTransportMessageRef)ids);
        SOSRegisterTransportMessage((SOSTransportMessageRef)ids);
    }
    
    return ids;
}
CFMutableDictionaryRef SOSTransportMessageIDSTestGetChanges(SOSTransportMessageRef transport){
    return ((SOSTransportMessageIDSTestRef)transport)->changes;
}

static CFStringRef copyIDSTestDescription(SOSTransportMessageRef transport){
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@,%@,%@,%ld"),transport->circleName, transport->account, transport->getName(transport), transport->getTransportType(transport, NULL));
}

CFStringRef SOSTransportMessageIDSTestGetName(SOSTransportMessageRef transport){
    return ((SOSTransportMessageIDSTestRef)transport)->name;
}

static CFIndex  getIDSTestTransportType(SOSTransportMessageRef transport, CFErrorRef *error){
    return kIDSTest;
}

static void destroyIDSTest(SOSTransportMessageRef transport){
    SOSUnregisterTransportMessage(transport);
}

void SOSTransportMessageIDSTestSetName(SOSTransportMessageRef transport, CFStringRef accountName){
    SOSTransportMessageIDSTestRef t = (SOSTransportMessageIDSTestRef)transport;
    t->name = accountName;
}

static CF_RETURNS_RETAINED
CFDictionaryRef handleMessagesIDSTest(SOSTransportMessageRef transport, CFMutableDictionaryRef message, CFErrorRef *error) {
    
    CFMutableDictionaryRef handled = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef peerToMessage = CFDictionaryGetValue(message, transport->circleName);
    CFMutableArrayRef handled_peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    secerror("Received IDS message!");
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

static void SOSTransportMessageIDSTestAddBulkToChanges(SOSTransportMessageIDSTestRef transport, CFDictionaryRef updates){

    if (transport->changes == NULL) {
        transport->changes = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(updates), updates);

    }
    else{
        CFDictionaryForEach(updates, ^(const void *key, const void *value) {
            CFDictionaryAddValue(transport->changes, key, value);
        });
    }
}
static bool sendDataToPeerIDSTest(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID, CFDataRef message, CFErrorRef *error)
{
    SOSTransportMessageIDSTestRef testTransport = (SOSTransportMessageIDSTestRef)transport;
    
    secerror("sending message through test transport: %@", message);
    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer(transport, peerID);
    CFDictionaryRef a_message_to_a_peer = CFDictionaryCreateForCFTypes(NULL, message_to_peer_key, message, NULL);

    SOSTransportMessageIDSTestAddBulkToChanges(testTransport, a_message_to_a_peer);

    CFReleaseNull(message_to_peer_key);
    CFReleaseNull(a_message_to_a_peer);
    return true;

}
static bool sendDictionaryToPeerIDSTest(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef deviceID, CFStringRef peerID, CFDictionaryRef message, CFErrorRef *error)
{
    SOSTransportMessageIDSTestRef testTransport = (SOSTransportMessageIDSTestRef)transport;
    
    secerror("sending message through test transport: %@", message);
    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer(transport, peerID);
    CFDictionaryRef a_message_to_a_peer = CFDictionaryCreateForCFTypes(NULL, message_to_peer_key, message, NULL);
    
    SOSTransportMessageIDSTestAddBulkToChanges(testTransport, a_message_to_a_peer);
    
    CFReleaseNull(message_to_peer_key);
    CFReleaseNull(a_message_to_a_peer);
    return true;
    
}

static bool syncWithPeersIDSTest(SOSTransportMessageRef transport, CFSetRef peerIDs, CFErrorRef *error){
    // Each entry is keyed by circle name and contains a list of peerIDs
    __block bool result = true;

    CFSetForEach(peerIDs, ^(const void *value) {
        CFStringRef peerID = asString(value, NULL);
        if (peerID) {
            secnotice("transport", "IDS sync with peerIDs %@", peerID);
            result &= SOSTransportMessageSendMessageIfNeeded(transport, transport->circleName, peerID, error);
        }
    });

    return result;
}

static bool sendMessagesIDSTest(SOSTransportMessageRef transport, CFDictionaryRef peersToMessage, CFErrorRef *error) {

    __block bool result = true;
    SOSAccountRef account = transport->account;

    CFDictionaryForEach(peersToMessage, ^(const void *key, const void *value) {
        CFStringRef idsDeviceID = NULL;;
        CFStringRef peerID = asString(key, NULL);
        SOSPeerInfoRef pi = NULL;
        require(peerID, done);
        require(!CFEqualSafe(peerID, SOSAccountGetMyPeerID(account)), done);

        pi = SOSAccountCopyPeerWithID(account, peerID, NULL);
        require(pi, done);

        idsDeviceID = SOSPeerInfoCopyDeviceID(pi);
        require(idsDeviceID, done);

        CFDictionaryRef messageDictionary = asDictionary(value, NULL);
        if (messageDictionary) {
                result &= sendDictionaryToPeerIDSTest(transport, transport->circleName, idsDeviceID, peerID, messageDictionary, error);
        } else {
            CFDataRef messageData = asData(value, NULL);
            if (messageData) {
                result &= sendDataToPeerIDSTest(transport, transport->circleName, idsDeviceID, peerID, messageData, error);
            }
        }
    done:
        CFReleaseNull(idsDeviceID);
        CFReleaseNull(pi);

    });

    return result;
}

static bool flushChangesIDSTest(SOSTransportMessageRef transport, CFErrorRef *error)
{
    return true;
}

static bool cleanupAfterPeerIDSTest(SOSTransportMessageRef transport, CFDictionaryRef circle_to_peer_ids, CFErrorRef *error)
{
    return true;
}

void SOSTransportMessageIDSTestClearChanges(SOSTransportMessageRef transport){
    SOSTransportMessageIDSTestRef ids = (SOSTransportMessageIDSTestRef)transport;
    CFReleaseNull(ids->changes);
    
    ids->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
}


