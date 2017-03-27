//
//  SOSTransportCircleKVS.c
//  sec
//
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportCircleKVS.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <SOSCloudKeychainClient.h>
#include <utilities/SecCFWrappers.h>

static bool SOSTransportCircleKVSUpdateRetirementRecords(SOSTransportCircleKVSRef transport, CFDictionaryRef updates, CFErrorRef* error);
static bool SOSTransportCircleKVSUpdateKVS(SOSTransportCircleRef transport, CFDictionaryRef changes, CFErrorRef *error);
static bool expireRetirementRecords(SOSTransportCircleRef transport, CFDictionaryRef retirements, CFErrorRef *error);
static CFArrayRef handleCircleMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_circle_messages_table, CFErrorRef *error);
static void destroy(SOSTransportCircleRef transport);
static bool postCircle(SOSTransportCircleRef transport,  CFStringRef circleName, CFDataRef circle_data, CFErrorRef *error);
static CFDictionaryRef handleRetirementMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error);
static inline bool postRetirement(SOSTransportCircleRef transport,  CFStringRef circleName, CFStringRef peer_id, CFDataRef retirement_data, CFErrorRef *error);
static inline bool flushChanges(SOSTransportCircleRef transport, CFErrorRef *error);
static inline CFIndex getTransportType(SOSTransportCircleRef transport, CFErrorRef *error);
static bool sendPeerInfo(SOSTransportCircleRef transport, CFStringRef peerID, CFDataRef peerInfoData, CFErrorRef *error);
static bool flushRingChanges(SOSTransportCircleRef transport, CFErrorRef* error);
static bool postRing(SOSTransportCircleRef transport, CFStringRef ringName, CFDataRef ring, CFErrorRef *error);
static bool sendAccountChangedWithDSID(SOSTransportCircleRef transport, CFStringRef dsid, CFErrorRef *error);
static bool sendDebugInfo(SOSTransportCircleRef transport, CFStringRef type, CFTypeRef debugInfo, CFErrorRef *error);

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
        t->c.getTransportType = getTransportType;
        t->c.sendDebugInfo = sendDebugInfo;
        t->c.postRing = postRing;
        t->c.sendPeerInfo = sendPeerInfo;
        t->c.flushRingChanges = flushRingChanges;
        t->c.sendAccountChangedWithDSID = sendAccountChangedWithDSID;
        SOSRegisterTransportCircle((SOSTransportCircleRef)t);
    }
    return t;
}

static CFStringRef SOSTransportCircleKVSGetCircleName(SOSTransportCircleKVSRef transport){
    return transport->circleName;
}

static inline CFIndex getTransportType(SOSTransportCircleRef transport, CFErrorRef *error){
    return kKVS;
}


static void destroy(SOSTransportCircleRef transport){
    SOSTransportCircleKVSRef tkvs = (SOSTransportCircleKVSRef)transport;
    CFReleaseNull(tkvs->pending_changes);
    CFReleaseNull(tkvs->circleName);
    
    SOSUnregisterTransportCircle((SOSTransportCircleRef)tkvs);
}

static bool SOSTransportCircleKVSUpdateKVS(SOSTransportCircleRef transport, CFDictionaryRef changes, CFErrorRef *error){
    CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef block_error) {
        if (block_error) {
            secerror("Error putting: %@", block_error);
        }
    };
    
    SOSCloudKeychainPutObjectsInCloud(changes, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), log_error);
    return true;
}

bool SOSTransportCircleKVSSendPendingChanges(SOSTransportCircleKVSRef transport, CFErrorRef *error) {
    CFErrorRef changeError = NULL;
    SOSAccountRef account = SOSTransportCircleGetAccount((SOSTransportCircleRef)transport);
    
    if (transport->pending_changes == NULL || CFDictionaryGetCount(transport->pending_changes) == 0) {
        CFReleaseNull(transport->pending_changes);
        return true;
    }
    
    CFTypeRef dsid = SOSAccountGetValue(account, kSOSDSIDKey, error);
    if(dsid == NULL)
        dsid = kCFNull;
    
    CFDictionaryAddValue(transport->pending_changes, kSOSKVSRequiredKey, dsid);
    
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

static CF_RETURNS_RETAINED CFDictionaryRef handleRetirementMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_retirement_messages_table, CFErrorRef *error){
    SOSAccountRef account = SOSTransportCircleGetAccount(transport);

    return SOSAccountHandleRetirementMessages(account, circle_retirement_messages_table, error);
}


bool SOSTransportCircleRecordLastCirclePushedInKVS(SOSTransportCircleRef transport, CFStringRef circle_name, CFDataRef circleData){
    
    SOSAccountRef a = SOSTransportCircleGetAccount(transport);
    SOSTransportCircleKVSRef tkvs = (SOSTransportCircleKVSRef)transport;
    CFStringRef myPeerID = SOSAccountGetMyPeerID(a);
    CFDataRef timeData = NULL;

    CFMutableStringRef timeDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("["));
    CFAbsoluteTime currentTimeAndDate = CFAbsoluteTimeGetCurrent();

    withStringOfAbsoluteTime(currentTimeAndDate, ^(CFStringRef decription) {
        CFStringAppend(timeDescription, decription);
    });
    CFStringAppend(timeDescription, CFSTR("]"));
   
    timeData = CFStringCreateExternalRepresentation(NULL,timeDescription,
        kCFStringEncodingUTF8, '?');
    
    CFMutableDataRef timeAndCircleMutable = CFDataCreateMutable(kCFAllocatorDefault, CFDataGetLength(timeData) + CFDataGetLength(circleData));
    CFDataAppend(timeAndCircleMutable, timeData);

    CFDataAppend(timeAndCircleMutable, circleData);
    CFDataRef timeAndCircle = CFDataCreateCopy(kCFAllocatorDefault, timeAndCircleMutable); 

    if(myPeerID){
        CFStringRef lastPushedCircleKey = SOSLastCirclePushedKeyCreateWithCircleNameAndPeerID(circle_name, SOSAccountGetMyPeerID(a));
        SOSTransportCircleKVSAddToPendingChanges(tkvs, lastPushedCircleKey,timeAndCircle);
        CFReleaseSafe(lastPushedCircleKey);
    }
    else{
        //if we don't have a peerID (we could be retired or its too early in the system, let's use other gestalt information instead
        CFStringRef lastPushedCircleKeyWithAccount = SOSLastCirclePushedKeyCreateWithAccountGestalt(SOSTransportCircleGetAccount(transport));
        SOSTransportCircleKVSAddToPendingChanges(tkvs, lastPushedCircleKeyWithAccount, timeAndCircle);
        CFReleaseNull(lastPushedCircleKeyWithAccount);
    }
    CFReleaseNull(timeDescription);
    CFReleaseNull(timeAndCircleMutable);
    CFReleaseNull(timeAndCircle);
    CFReleaseNull(timeData);
    return true;
}

CFArrayRef SOSTransportCircleKVSHandlePeerInfoV2Messages(SOSTransportCircleRef transport, CFMutableDictionaryRef peer_info_message_table, CFErrorRef *error){
    //SOSTransportCircleKVSRef kvs_transport = (SOSTransportCircleKVSRef)transport;
    /* TODO Handle peer info messages! */
    
    /* assuming array of peer ids or peer infos that were handled as the return value */
    return NULL;
}

static CFArrayRef handleCircleMessages(SOSTransportCircleRef transport, CFMutableDictionaryRef circle_circle_messages_table, CFErrorRef *error){
    CFMutableArrayRef handledKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryForEach(circle_circle_messages_table, ^(const void *key, const void *value) {
        CFErrorRef circleMessageError = NULL;
        if(!isString(key) || !isData(value)) {
            secerror("Error, Key-Value for CircleMessage was not CFString/CFData");
        } if (!SOSAccountHandleCircleMessage(SOSTransportCircleGetAccount(transport), key, value, &circleMessageError)) {
            secerror("Error handling circle message %@ (%@): %@", key, value, circleMessageError);
        } else{
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
        SOSCircleRef circle = account->trusted_circle;
        require_quiet(circle && CFEqualSafe(circle_name, SOSCircleGetName(circle)), fail);

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

//register peer infos key
bool SOSTransportCircleKVSAppendPeerInfoKeyInterest(SOSTransportCircleKVSRef transport, CFMutableArrayRef alwaysKeys, CFMutableArrayRef afterFirstUnlockKeys, CFMutableArrayRef unlockedKeys, CFErrorRef *error){
    
    if(SOSAccountHasPublicKey(SOSTransportCircleGetAccount((SOSTransportCircleRef)transport), NULL)){
        
        SOSAccountRef account = SOSTransportCircleGetAccount((SOSTransportCircleRef)transport);
        require_quiet(account, fail);
        SOSCircleRef circle = account->trusted_circle;
        require_quiet(circle, fail);
        
        SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
            CFStringRef peer_info_key = SOSPeerInfoV2KeyCreateWithPeerName(SOSPeerInfoGetPeerID(peer));
            CFArrayAppendValue(alwaysKeys, peer_info_key);
            CFReleaseNull(peer_info_key);
        });
    }
    return true;
    
fail:
    return false;
}

//register ring key
bool SOSTransportCircleKVSAppendRingKeyInterest(SOSTransportCircleKVSRef transport, CFMutableArrayRef alwaysKeys, CFMutableArrayRef afterFirstUnlockKeys, CFMutableArrayRef unlockedKeys, CFErrorRef *error){
    
    if(SOSAccountHasPublicKey(SOSTransportCircleGetAccount((SOSTransportCircleRef)transport), NULL)){
        SOSAccountRef account = SOSTransportCircleGetAccount((SOSTransportCircleRef)transport);
        require_quiet(account, fail);
        SOSCircleRef circle = account->trusted_circle;
        require_quiet(circle, fail);

        // Always interested in backup rings:
        SOSAccountForEachRingName(account, ^(CFStringRef ringName) {
            CFStringRef ring_key = SOSRingKeyCreateWithRingName(ringName);
            CFArrayAppendValue(unlockedKeys, ring_key);
            CFReleaseNull(ring_key);
        });
    }
    return true;
fail:
    return false;
}

extern CFStringRef kSOSAccountDebugScope;
//register debug scope key
bool SOSTransportCircleKVSAppendDebugKeyInterest(SOSTransportCircleKVSRef transport, CFMutableArrayRef alwaysKeys, CFMutableArrayRef afterFirstUnlockKeys, CFMutableArrayRef unlockedKeys, CFErrorRef *error){
    
    CFStringRef key = SOSDebugInfoKeyCreateWithTypeName(kSOSAccountDebugScope);
    CFArrayAppendValue(alwaysKeys, key);
    CFRelease(key);
    return true;
}

//send debug info over KVS
bool sendDebugInfo(SOSTransportCircleRef transport, CFStringRef type, CFTypeRef debugInfo, CFErrorRef *error)
{
    CFStringRef key = SOSDebugInfoKeyCreateWithTypeName(type);
    CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           key,
                                                           debugInfo,
                                                           NULL);
    
    CFReleaseNull(key);
    bool success = SOSTransportCircleKVSUpdateKVS(transport, changes, error);
    
    CFReleaseNull(changes);
    
    return success;
}

static bool sendAccountChangedWithDSID(SOSTransportCircleRef transport, CFStringRef dsid, CFErrorRef *error){
    
    CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kSOSKVSAccountChangedKey, dsid,
                                                           NULL);
    
    bool success = SOSTransportCircleKVSUpdateKVS((SOSTransportCircleRef)transport, changes, error);
    
    CFReleaseNull(changes);
    
    return success;
}

//send the Ring over KVS
bool postRing(SOSTransportCircleRef transport, CFStringRef ringName, CFDataRef ring, CFErrorRef *error)
{
    CFStringRef ringKey = SOSRingKeyCreateWithName(ringName, error);
    
    if(ringKey)
        SOSTransportCircleKVSAddToPendingChanges((SOSTransportCircleKVSRef)transport, ringKey, ring);
    
    CFReleaseNull(ringKey);
    
    return true;
}

bool flushRingChanges(SOSTransportCircleRef transport, CFErrorRef* error){
    
    SOSTransportCircleKVSRef tkvs = (SOSTransportCircleKVSRef) transport;
    
    return SOSTransportCircleKVSSendPendingChanges(tkvs, error);
}

//send the PeerInfo Data over KVS
bool sendPeerInfo(SOSTransportCircleRef transport, CFStringRef peerID, CFDataRef peerInfoData, CFErrorRef *error)
{
    CFStringRef peerName = SOSPeerInfoV2KeyCreateWithPeerName(peerID);
    
    CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           peerName, peerInfoData,
                                                           NULL);
    
    CFReleaseNull(peerName);
    bool success = SOSTransportCircleKVSUpdateKVS(transport, changes, error);
    
    CFReleaseNull(changes);
    
    return success;
}

bool SOSTransportCircleSendOfficialDSID(SOSTransportCircleRef transport, CFStringRef dsid, CFErrorRef *error)
{
    CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kSOSKVSOfficialDSIDKey, dsid,
                                                           NULL);
    bool success = SOSTransportCircleKVSUpdateKVS(transport, changes, error);
    CFReleaseNull(changes);
    
    return success;
}
                                                           


