//
//  SOSTransportCircleKVS.c
//  sec
//
#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSTransportCircle.h>
#include <SecureObjectSync/SOSTransportCircleKVS.h>
#include <SecureObjectSync/SOSKVSKeys.h>
#include <utilities/SecCFWrappers.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSAccountPriv.h>
#include <SOSCloudKeychainClient.h>

static bool SOSTransportCircleKVSUpdateRetirementRecords(SOSTransportCircleKVSRef transport, CFDictionaryRef updates, CFErrorRef* error);
static bool SOSTransportCircleKVSUpdateKVS(SOSTransportCircleRef transport, CFDictionaryRef changes, CFErrorRef *error);
static bool expireRetirementRecords(SOSTransportCircleRef transport, CFDictionaryRef retirements, CFErrorRef *error);
static CFArrayRef handleCircleMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_circle_messages_table, CFErrorRef *error);
static void destroy(SOSTransportCircleRef transport);
static bool postCircle(SOSTransportCircleRef transport,  CFStringRef circleName, CFDataRef circle_data, CFErrorRef *error);
static CFDictionaryRef handleRetirementMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error);
static inline bool postRetirement(SOSTransportCircleRef transport,  CFStringRef circleName, CFStringRef peer_id, CFDataRef retirement_data, CFErrorRef *error);
static inline bool flushChanges(SOSTransportCircleRef transport, CFErrorRef *error);


struct __OpaqueSOSTransportCircleKVS{
    struct __OpaqueSOSTransportCircle   c;
    CFMutableDictionaryRef              pending_changes;
    CFStringRef                         circleName;
};


SOSTransportCircleKVSRef SOSTransportCircleKVSCreate(SOSAccountRef account, CFStringRef circleName, CFErrorRef *error){
    SOSTransportCircleKVSRef t = (SOSTransportCircleKVSRef) SOSTransportCircleCreateForSubclass(sizeof(struct __OpaqueSOSTransportCircleKVS) - sizeof(CFRuntimeBase), account, error);
    if(t){
        t->circleName = CFRetainSafe(circleName);
        t->c.expireRetirementRecords = expireRetirementRecords;
        t->c.postCircle = postCircle;
        t->c.postRetirement = postRetirement;
        t->c.flushChanges = flushChanges;
        t->pending_changes  = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        t->c.handleRetirementMessages = handleRetirementMessages;
        t->c.handleCircleMessages = handleCircleMessages;
        t->c.destroy = destroy;
        SOSRegisterTransportCircle((SOSTransportCircleRef)t);
    }
    return t;
}

static CFStringRef SOSTransportCircleKVSGetCircleName(SOSTransportCircleKVSRef transport){
    return transport->circleName;
}

static void destroy(SOSTransportCircleRef transport){
    SOSTransportCircleKVSRef tkvs = (SOSTransportCircleKVSRef)transport;
    CFReleaseNull(tkvs->pending_changes);
    CFReleaseNull(tkvs->circleName);
    
    SOSUnregisterTransportCircle((SOSTransportCircleRef)tkvs);
}

bool SOSTransportCircleKVSSendPendingChanges(SOSTransportCircleKVSRef transport, CFErrorRef *error) {
    CFErrorRef changeError = NULL;
    
    if (transport->pending_changes == NULL || CFDictionaryGetCount(transport->pending_changes) == 0) {
        CFReleaseNull(transport->pending_changes);
        return true;
    }
    
    bool success = SOSTransportCircleKVSUpdateKVS((SOSTransportCircleRef)transport, transport->pending_changes, &changeError);
    if (success) {
        CFDictionaryRemoveAllValues(transport->pending_changes);
    } else {
        SOSCreateErrorWithFormat(kSOSErrorSendFailure, changeError, error, NULL,
                                 CFSTR("Send changes block failed [%@]"), transport->pending_changes);
    }
    
    return success;
}

void SOSTransportCircleKVSAddToPendingChanges(SOSTransportCircleKVSRef transport, CFStringRef message_key, CFDataRef message_data){
    
    if (transport->pending_changes == NULL) {
        transport->pending_changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    }
    if (message_data == NULL) {
        CFDictionarySetValue(transport->pending_changes, message_key, kCFNull);
    } else {
        CFDictionarySetValue(transport->pending_changes, message_key, message_data);
    }
}

static bool expireRetirementRecords(SOSTransportCircleRef transport, CFDictionaryRef retirements, CFErrorRef *error) {
    
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
        success = SOSTransportCircleKVSUpdateRetirementRecords((SOSTransportCircleKVSRef)transport, keysToWrite, error);
    }
    CFReleaseNull(keysToWrite);
    
    return success;
}

static bool SOSTransportCircleKVSUpdateRetirementRecords(SOSTransportCircleKVSRef transport, CFDictionaryRef updates, CFErrorRef* error){
    CFErrorRef updateError = NULL;
    bool success = false;
    if (SOSTransportCircleKVSUpdateKVS((SOSTransportCircleRef)transport, updates, &updateError)){
        success = true;
    } else {
        SOSCreateErrorWithFormat(kSOSErrorSendFailure, updateError, error, NULL,
                                 CFSTR("update parameters key failed [%@]"), updates);
    }
    return success;
}

static inline bool postRetirement(SOSTransportCircleRef transport,  CFStringRef circleName, CFStringRef peer_id, CFDataRef retirement_data, CFErrorRef *error)
{
    CFStringRef retirement_key = SOSRetirementKeyCreateWithCircleNameAndPeer(circleName, peer_id);
    if (retirement_key)
        SOSTransportCircleKVSAddToPendingChanges((SOSTransportCircleKVSRef)transport, retirement_key, retirement_data);
    
    CFReleaseNull(retirement_key);
    return true;
}

static inline bool flushChanges(SOSTransportCircleRef transport, CFErrorRef *error)
{
    SOSTransportCircleKVSRef tkvs = (SOSTransportCircleKVSRef) transport;
    
    return SOSTransportCircleKVSSendPendingChanges(tkvs, error);
}

static bool postCircle(SOSTransportCircleRef transport,  CFStringRef circleName, CFDataRef circle_data, CFErrorRef *error){
    SOSTransportCircleKVSRef tkvs = (SOSTransportCircleKVSRef)transport;
    CFStringRef circle_key = SOSCircleKeyCreateWithName(circleName, error);
    if (circle_key)
        SOSTransportCircleKVSAddToPendingChanges(tkvs, circle_key, circle_data);
    CFReleaseNull(circle_key);
    
    return true;
}

static CFDictionaryRef handleRetirementMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error){
    SOSAccountRef account = SOSTransportCircleGetAccount(transport);
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
        if (!SOSAccountHandleCircleMessage(SOSTransportCircleGetAccount(transport), key, value, &circleMessageError)) {
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

bool SOSTransportCircleKVSAppendKeyInterest(SOSTransportCircleKVSRef transport, CFMutableArrayRef alwaysKeys, CFMutableArrayRef afterFirstUnlockKeys, CFMutableArrayRef unlockedKeys, CFErrorRef *error){
    
    CFStringRef circle_name = NULL;
    CFStringRef circle_key = NULL;
    
    if(SOSAccountHasPublicKey(SOSTransportCircleGetAccount((SOSTransportCircleRef)transport), NULL)){
        require_quiet(circle_name = SOSTransportCircleKVSGetCircleName(transport), fail);
        require_quiet(circle_key = SOSCircleKeyCreateWithName(circle_name, error), fail);

        SOSAccountRef account = SOSTransportCircleGetAccount((SOSTransportCircleRef)transport);
        require_quiet(account, fail);
        SOSCircleRef circle = SOSAccountFindCircle(account, circle_name, error);
        require_quiet(circle, fail);

        SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
            CFStringRef retirement_key = SOSRetirementKeyCreateWithCircleNameAndPeer(circle_name, SOSPeerInfoGetPeerID(peer));
            CFArrayAppendValue(alwaysKeys, retirement_key);
            CFReleaseNull(retirement_key);
        });
        
        CFArrayAppendValue(alwaysKeys, circle_key);
        
        CFReleaseNull(circle_key);
    }
    return true;

fail:
    CFReleaseNull(circle_key);
    return false;
}

static bool SOSTransportCircleKVSUpdateKVS(SOSTransportCircleRef transport, CFDictionaryRef changes, CFErrorRef *error){
    CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef error) {
        if (error) {
            secerror("Error putting: %@", error);
            CFReleaseSafe(error);
        }
    };
    
    SOSCloudKeychainPutObjectsInCloud(changes, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), log_error);
    return true;
}
