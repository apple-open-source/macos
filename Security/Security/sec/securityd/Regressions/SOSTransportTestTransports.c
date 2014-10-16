#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSAccountPriv.h>
#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSTransportKeyParameter.h>
#include <SecureObjectSync/SOSKVSKeys.h>
#include <SecureObjectSync/SOSPeerCoder.h>
#include <utilities/SecCFWrappers.h>

#include "SOSTransportTestTransports.h"

static bool sendToPeer(SOSTransportMessageRef transport, CFStringRef circleName, CFStringRef peerID, CFDataRef message, CFErrorRef *error);
static bool syncWithPeers(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef *error);
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
static bool setToNewAccount(SOSTransportKeyParameterRef transport);
    
static void SOSTransportCircleTestAddBulkToChanges(SOSTransportCircleTestRef transport, CFDictionaryRef updates);
static void SOSTransportCircleTestAddToChanges(SOSTransportCircleTestRef transport, CFStringRef message_key, CFDataRef message_data);
static bool SOSTransportCircleTestSendChanges(SOSTransportCircleTestRef transport, CFDictionaryRef changes, CFErrorRef *error);


void SOSAccountUpdateTestTransports(SOSAccountRef account, CFDictionaryRef gestalt){
    CFStringRef new_name = (CFStringRef)CFDictionaryGetValue(gestalt, kPIUserDefinedDeviceName);
    SOSTransportKeyParameterTestRef key = (SOSTransportKeyParameterTestRef)account->key_transport;
    CFDictionaryRef circles = account->circle_transports;
    CFDictionaryRef messages = account->message_transports;
    
    SOSTransportKeyParameterTestSetName(key, new_name);
    CFDictionaryForEach(circles, ^(const void *key, const void *value) {
        SOSTransportCircleTestSetName((SOSTransportCircleTestRef)value, new_name);
    });
    CFDictionaryForEach(messages, ^(const void *key, const void *value) {
        SOSTransportMessageTestSetName((SOSTransportMessageTestRef)value, new_name);
    });
    
}

static SOSCircleRef SOSAccountEnsureCircleTest(SOSAccountRef a, CFStringRef name, CFStringRef accountName, CFErrorRef *error)
{
    CFErrorRef localError = NULL;
    
    SOSCircleRef circle = SOSAccountFindCircle(a, name, &localError);
    
    require_action_quiet(circle || !isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle), fail,
                         if (error) { *error = localError; localError = NULL; });
    
    if(NULL == circle){
        circle = SOSCircleCreate(NULL, name, NULL);
        if (circle){
            CFDictionaryAddValue(a->circles, name, circle);
            CFRelease(circle);
            circle = SOSAccountFindCircle(a, name, &localError);
        }
    }
    require_quiet(SOSAccountInflateTestTransportsForCircle(a, name, accountName, &localError), fail);
    
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
        CFArrayRef circle_names = a->factory->copy_names(a->factory);
        require(circle_names, xit);
        CFArrayForEach(circle_names, ^(const void*name) {
            if (isString(name))
                SOSAccountEnsureCircleTest(a, (CFStringRef)name, accountName, NULL);
        });
        
        CFReleaseNull(circle_names);
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
    if(account->circle_transports == NULL){
        account->circle_transports = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        tCircle = SOSTransportTestCreateCircle(account, accountName, circleName);
        require_quiet(tCircle, fail);
        CFDictionarySetValue(account->circle_transports, circleName, tCircle);
    }
    else if(CFDictionaryGetCount(account->circle_transports) == 0){
        tCircle = SOSTransportTestCreateCircle(account, accountName, circleName);
        require_quiet(tCircle, fail);
        CFDictionarySetValue(account->circle_transports, circleName, tCircle);
    }
    if(account->message_transports == NULL){
        account->message_transports = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        tMessage = SOSTransportTestCreateMessage(account, accountName, circleName);
        require_quiet(tMessage, fail);
        CFDictionarySetValue(account->message_transports, circleName, tMessage);
    }
    else if(CFDictionaryGetCount(account->message_transports) == 0){
        tMessage = SOSTransportTestCreateMessage(account, accountName, circleName);
        require_quiet(tMessage, fail);
        CFDictionarySetValue(account->message_transports, circleName, tMessage);
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
    
    SOSTransportKeyParameterTestRef tpt = calloc(1, sizeof(struct SOSTransportKeyParameterTest));
    
    tpt->name = CFRetainSafe(name);
    tpt->changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    tpt->k.account = CFRetainSafe(account);
    tpt->circleName = CFRetainSafe(circleName);
    tpt->k.publishCloudParameters = publishCloudParameters;
    tpt->k.handleKeyParameterChanges = handleKeyParameterChanges;
    tpt->k.setToNewAccount = setToNewAccount;
    if(!key_transports)
        key_transports = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(key_transports, (SOSTransportKeyParameterRef)tpt);
    SOSRegisterTransportKeyParameter((SOSTransportKeyParameterRef)tpt);
    return tpt;
}

static bool handleKeyParameterChanges(SOSTransportKeyParameterRef transport, CFDataRef data, CFErrorRef error){
    SOSAccountRef account = transport->account;
    return SOSAccountHandleParametersChange(account, data, &error);
}

static bool setToNewAccount(SOSTransportKeyParameterRef transport){
    SOSTransportKeyParameterTestRef tpt = (SOSTransportKeyParameterTestRef)transport;
    SOSAccountRef a = SOSTransportKeyParameterTestGetAccount(tpt);
    CFStringRef accountName = SOSTransportKeyParameterTestGetName(tpt);
    CFAllocatorRef allocator = CFGetAllocator(a);
    CFReleaseNull(a->circle_identities);
    CFReleaseNull(a->circles);
    CFReleaseNull(a->retired_peers);
    
    CFReleaseNull(a->user_key_parameters);
    CFReleaseNull(a->user_public);
    CFReleaseNull(a->previous_public);
    CFReleaseNull(a->_user_private);
    
    a->user_public_trusted = false;
    a->departure_code = kSOSNeverAppliedToCircle;
    a->user_private_timer = 0;
    a->lock_notification_token = 0;
    
    // keeping gestalt;
    // keeping factory;
    // Live Notification
    // change_blocks;
    // update_interest_block;
    // update_block;
    
    a->circles = CFDictionaryCreateMutableForCFTypes(allocator);
    a->circle_identities = CFDictionaryCreateMutableForCFTypes(allocator);
    a->retired_peers = CFDictionaryCreateMutableForCFTypes(allocator);
    
    //unregister all the transports from the global transport queue
    SOSUnregisterTransportKeyParameter(a->key_transport);
    CFArrayForEach(circle_transports, ^(const void *value) {
        SOSTransportCircleTestRef tpt = (SOSTransportCircleTestRef) value;
        if(CFStringCompare(SOSTransportCircleTestGetName(tpt), accountName, 0) == 0){
            SOSUnregisterTransportCircle((SOSTransportCircleRef)tpt);
        }
    });
    CFArrayForEach(message_transports, ^(const void *value) {
        SOSTransportMessageTestRef tpt = (SOSTransportMessageTestRef) value;
        if(CFStringCompare(SOSTransportMessageTestGetName(tpt), accountName, 0) == 0){
            SOSUnregisterTransportMessage((SOSTransportMessageRef)tpt);
        }
    });
    
    
    CFReleaseNull(a->key_transport);
    CFReleaseNull(a->circle_transports);
    CFReleaseNull(a->message_transports);
    
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
SOSTransportCircleTestRef SOSTransportTestCreateCircle(SOSAccountRef account, CFStringRef name, CFStringRef circleName){
    SOSTransportCircleTestRef tpt = calloc(1, sizeof(struct SOSTransportCircleTest));
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
        tpt->changes  = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        tpt->name = CFRetainSafe(name);
        tpt->circleName = CFRetainSafe(circleName);
        if(!circle_transports)
            circle_transports = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFArrayAppendValue(circle_transports, (SOSTransportCircleRef)tpt);
        SOSRegisterTransportCircle((SOSTransportCircleRef)tpt);
    }
    
    return tpt;
}

static void destroy(SOSTransportCircleRef transport){
    SOSTransportCircleTestRef tkvs = (SOSTransportCircleTestRef)transport;
    CFArrayRemoveAllValue(circle_transports, tkvs);
    CFReleaseNull(tkvs->changes);
    CFReleaseNull(tkvs->name);
    CFReleaseNull(tkvs->circleName);
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

static bool postCircle(SOSTransportCircleRef transport,  CFStringRef circleName, CFDataRef circle_data, CFErrorRef *error){
    SOSTransportCircleTestRef tkvs = (SOSTransportCircleTestRef)transport;
    CFStringRef circle_key = SOSCircleKeyCreateWithName(circleName, error);
    if (circle_key)
        SOSTransportCircleTestAddToChanges(tkvs, circle_key, circle_data);
    CFReleaseNull(circle_key);
    
    return true;
}

static CFDictionaryRef handleRetirementMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error){
    SOSAccountRef account = transport->account;
    CFMutableDictionaryRef handledRetirementMessages = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryForEach(circle_retirement_messages_table, ^(const void *key, const void *value) {
        if (isString(key) && isDictionary(value)) {
            CFStringRef circle_name = (CFStringRef) key;
            
            CFDictionaryRef retirment_dictionary = (CFDictionaryRef) value;
            CFDictionaryForEach(retirment_dictionary, ^(const void *key, const void *value) {
                if(isData(value)) {
                    SOSPeerInfoRef pi = SOSPeerInfoCreateFromData(NULL, error, (CFDataRef) value);
                    if(pi && CFEqual(key, SOSPeerInfoGetPeerID(pi)) && SOSPeerInfoInspectRetirementTicket(pi, error)) {
                        CFMutableDictionaryRef circle_retirements = CFDictionaryEnsureCFDictionaryAndGetCurrentValue(account->retired_peers, circle_name);
                        CFDictionarySetValue(circle_retirements, key, value);
                        
                        SOSAccountRecordRetiredPeerInCircleNamed(account, circle_name, pi);
                        
                        CFMutableArrayRef handledRetirementIDs = CFDictionaryEnsureCFArrayAndGetCurrentValue(handledRetirementMessages, circle_name);
                        CFArrayAppendValue(handledRetirementIDs, SOSPeerInfoGetPeerID(pi));
                    }
                    CFReleaseSafe(pi);
                }
            });
        }
    });
    return handledRetirementMessages;
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

struct SOSTransportMessageTest{
    struct __OpaqueSOSTransportMessage m;
    CFMutableDictionaryRef changes;
    CFStringRef name;
    CFStringRef circleName;
};

SOSTransportMessageTestRef SOSTransportTestCreateMessage(SOSAccountRef account, CFStringRef name, CFStringRef circleName){
    SOSTransportMessageTestRef tpt = calloc(1, sizeof(struct SOSTransportMessageTest));
    
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
        tpt->name = CFRetainSafe(name);
        tpt->circleName = CFRetainSafe(circleName);
        if(!message_transports)
            message_transports = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFArrayAppendValue(message_transports, (SOSTransportMessageRef)tpt);
        SOSRegisterTransportMessage((SOSTransportMessageRef)tpt);
    }
    
    return tpt;
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

static void destroyMessageTransport(SOSTransportMessageRef transport){
    SOSTransportMessageTestRef tkvs = (SOSTransportMessageTestRef)transport;
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
    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer((SOSTransportMessageKVSRef)transport, peerID);
    CFDictionaryRef a_message_to_a_peer = CFDictionaryCreateForCFTypes(NULL, message_to_peer_key, message, NULL);
    
    SOSTransportMessageTestAddBulkToChanges((SOSTransportMessageTestRef)testTransport, a_message_to_a_peer);
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
                    
                    SOSEnginePeerMessageSentBlock sent = NULL;
                    CFDataRef message_to_send = NULL;
                    bool ok = false;
                    SOSPeerRef peer = SOSPeerCreateWithEngine(SOSTransportMessageGetEngine(transport), peerID);
                    CFDataRef coderData = SOSEngineGetCoderData(SOSTransportMessageGetEngine(transport), peerID);
                    
                    SOSCoderRef coder = SOSCoderCreateFromData(coderData, error);
                    SOSPeerSetCoder(peer, coder);
                    
                    ok = SOSPeerCoderSendMessageIfNeeded(peer, &message_to_send, circleName, peerID, &sent, error);
                    coder = SOSPeerGetCoder(peer);
                    
                    if (message_to_send)    {
                        CFDictionaryRef peer_dict = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                                 peerID, message_to_send,
                                                                                 NULL);
                        
                        CFDictionarySetValue(SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)transport), circleName, peer_dict);
                        SOSPeerCoderConsume(&sent, ok);
                        
                        CFReleaseSafe(peer_dict);
                    }
                    
                    
                    Block_release(sent);
                    
                    
                    CFReleaseSafe(message_to_send);
                    
                    coderData = SOSCoderCopyDER(coder, error);
                    
                    if(!SOSEngineSetCoderData(SOSTransportMessageGetEngine(transport), peerID, coderData, error)){
                        secerror("SOSTransportMessageSendMessageIfNeeded, Could not save peer state");
                    }
                    CFReleaseNull(coderData);
                    
                    if (coder)
                        SOSCoderDispose(coder);
                    
                    CFReleaseNull(peer);
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

SOSAccountRef SOSTransportMessageTestGetAccount(SOSTransportMessageTestRef transport) {
    return ((SOSTransportMessageRef)transport)->account;
}

