
#include "SOSAccountPriv.h"

#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameterKVS.h>

#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>

#include <CoreFoundation/CoreFoundation.h>

#include <utilities/SecCFError.h>

// MARK: Engine Logging
#define LOG_ENGINE_STATE_INTERVAL 20

static void SOSAccountConsiderLoggingEngineState(SOSAccountTransactionRef txn) {
    static int engineLogCountDown = 0;

    if(engineLogCountDown <= 0) {
        SOSEngineRef engine = SOSTransportMessageGetEngine(txn->account->kvs_message_transport);

        SOSEngineLogState(engine);
        engineLogCountDown = LOG_ENGINE_STATE_INTERVAL;
    } else {
        engineLogCountDown--;
    }
}

static bool SOSAccountIsThisPeerIDMe(SOSAccountRef account, CFStringRef peerID) {
    SOSPeerInfoRef mypi = SOSFullPeerInfoGetPeerInfo(account->my_identity);
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(mypi);
    
    return myPeerID && CFEqualSafe(myPeerID, peerID);
}

bool SOSAccountSendIKSPSyncList(SOSAccountRef account, CFErrorRef *error){
    bool result = true;
    __block CFErrorRef localError = NULL;
    __block CFMutableArrayRef ids = NULL;
    SOSCircleRef circle = NULL;
    
    require_action_quiet(SOSAccountIsInCircle(account, NULL), xit,
                         SOSCreateError(kSOSErrorNoCircle, CFSTR("This device is not in circle"),
                                        NULL, &localError));
    
    circle  = SOSAccountGetCircle(account, error);
    ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSCircleForEachValidPeer(circle, account->user_public, ^(SOSPeerInfoRef peer) {
        if (!SOSAccountIsThisPeerIDMe(account, SOSPeerInfoGetPeerID(peer))) {
            if(SOSPeerInfoShouldUseIDSTransport(SOSFullPeerInfoGetPeerInfo(account->my_identity), peer) &&
               SOSPeerInfoShouldUseIDSMessageFragmentation(SOSFullPeerInfoGetPeerInfo(account->my_identity), peer) &&
               !SOSPeerInfoShouldUseACKModel(SOSFullPeerInfoGetPeerInfo(account->my_identity), peer)){
                SOSTransportMessageIDSSetFragmentationPreference(account->ids_message_transport, kCFBooleanTrue);
                CFStringRef deviceID = SOSPeerInfoCopyDeviceID(peer);
                if(deviceID != NULL){
                    CFArrayAppendValue(ids, deviceID);
                }
                CFReleaseNull(deviceID);
            }
        }
    });
    require_quiet(CFArrayGetCount(ids) != 0, xit);
    secnotice("IDS Transport", "List of IDS Peers to ping: %@", ids);
    
    SOSCloudKeychainGetIDSDeviceAvailability(ids, SOSAccountGetMyPeerID(account), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {
        bool success = (sync_error == NULL);
        if(!success)
            secerror("Failed to send list of IDS peers to IDSKSP: %@", sync_error);
    });
xit:
    if(error && *error != NULL)
        secerror("SOSAccountSendIKSPSyncList had an error: %@", *error);
    
    if(localError)
        secerror("SOSAccountSendIKSPSyncList had an error: %@", localError);
    
    CFReleaseNull(ids);
    CFReleaseNull(localError);
    
    return result;
}
//
// MARK: KVS Syncing
//

static bool SOSAccountSyncWithKVSPeers(SOSAccountTransactionRef txn, CFSetRef peerIDs, CFErrorRef *error) {
    SOSAccountRef account = txn->account;
    CFErrorRef localError = NULL;
    bool result = false;

    require_quiet(SOSAccountIsInCircle(account, &localError), xit);

    result = SOSTransportMessageSyncWithPeers(account->kvs_message_transport, peerIDs, &localError);

    if (result)
        SetCloudKeychainTraceValueForKey(kCloudKeychainNumberOfTimesSyncedWithPeers, 1);

xit:
    if (!result) {
        // Tell account to update SOSEngine with current trusted peers
        if (isSOSErrorCoded(localError, kSOSErrorPeerNotFound)) {
            secnotice("Account", "Arming account to update SOSEngine with current trusted peers");
            account->engine_peer_state_needs_repair = true;
        }
        CFErrorPropagate(localError, error);
        localError = NULL;
    }
    return result;
    
}

bool SOSAccountSyncWithKVSPeerWithMessage(SOSAccountTransactionRef txn, CFStringRef peerid, CFDataRef message, CFErrorRef *error) {
    SOSAccountRef account = txn->account;
    bool result = false;
    CFErrorRef localError = NULL;
    CFDictionaryRef encapsulatedMessage = NULL;

    secnotice("KVS Transport","Syncing with KVS capable peer: %@", peerid);
    secnotice("KVS Transport", "message: %@", message);

    require_quiet(message, xit);
    require_quiet(peerid, xit);

    encapsulatedMessage = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, peerid, message, NULL);

    result = SOSTransportMessageSendMessages(account->kvs_message_transport, encapsulatedMessage, &localError);
    secerror("KVS sync %s. (%@)", result ? "succeeded" : "failed", localError);

    SOSAccountConsiderLoggingEngineState(txn);

xit:
    CFReleaseNull(encapsulatedMessage);
    CFErrorPropagate(localError, error);
    
    return result;
}


static bool SOSAccountSyncWithKVSPeer(SOSAccountTransactionRef txn, CFStringRef peerID, CFErrorRef *error)
{
    bool result = false;
    CFErrorRef localError = NULL;

    secnotice("KVS Transport","Syncing with KVS capable peer: %@", peerID);

    CFMutableSetRef peerIDs = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    CFSetAddValue(peerIDs, peerID);

    result = SOSAccountSyncWithKVSPeers(txn, peerIDs, &localError);
    secerror("KVS sync %s. (%@)", result ? "succeeded" : "failed", localError);

    CFReleaseNull(peerIDs);
    CFErrorPropagate(localError, error);

    return result;
}


static CFMutableArrayRef SOSAccountCopyPeerIDsForDSID(SOSAccountRef account, CFStringRef deviceID, CFErrorRef* error) {
    CFMutableArrayRef peerIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSCircleForEachValidPeer(account->trusted_circle, account->user_public, ^(SOSPeerInfoRef peer) {
        CFStringRef peerDeviceID = SOSPeerInfoCopyDeviceID(peer);
        if(peerDeviceID != NULL && CFStringCompare(peerDeviceID, deviceID, 0) == 0){
            CFArrayAppendValue(peerIDs, SOSPeerInfoGetPeerID(peer));
        }
        CFReleaseNull(peerDeviceID);
    });
    
    if (peerIDs == NULL || CFArrayGetCount(peerIDs) == 0) {
        CFReleaseNull(peerIDs);
        SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("No peer with DSID: %@"), deviceID);
    }
    
    return peerIDs;
}

static bool SOSAccountSyncWithKVSPeerFromPing(SOSAccountRef account, CFArrayRef peerIDs, CFErrorRef *error) {
    
    CFErrorRef localError = NULL;
    bool result = false;
    
    CFSetRef peerSet = CFSetCreateCopyOfArrayForCFTypes(peerIDs);
    result = SOSTransportMessageSyncWithPeers(account->kvs_message_transport, peerSet, &localError);
    
    CFReleaseNull(peerSet);
   
    return result;
}

bool SOSAccountSyncWithKVSUsingIDSID(SOSAccountRef account, CFStringRef deviceID, CFErrorRef *error) {
    bool result = false;
    CFErrorRef localError = NULL;
    
    secnotice("KVS Transport","Syncing with KVS capable peer via DSID: %@", deviceID);
    
    CFArrayRef peerIDs = SOSAccountCopyPeerIDsForDSID(account, deviceID, &localError);
    require_quiet(peerIDs, xit);
    
    CFStringArrayPerfromWithDescription(peerIDs, ^(CFStringRef peerIDList) {
        secnotice("KVS Transport", "Syncing with KVS capable peers: %@", peerIDList);
    });
    
    result = SOSAccountSyncWithKVSPeerFromPing(account, peerIDs, &localError);
    secerror("KVS sync %s. (%@)", result ? "succeeded" : "failed", localError);
    
xit:
    CFReleaseNull(peerIDs);
    CFErrorPropagate(localError, error);
    
    return result;
}

static __nonnull CF_RETURNS_RETAINED CFSetRef SOSAccountSyncWithPeersOverKVS(SOSAccountTransactionRef txn, CFSetRef peers) {
    CFMutableSetRef handled = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);

    CFSetForEach(peers, ^(const void *value) {
        CFStringRef peerID = asString(value, NULL);
        CFErrorRef localError = NULL;
        if (peerID && SOSAccountSyncWithKVSPeer(txn, peerID, &localError)) {
            CFSetAddValue(handled, peerID);
            secnotice("KVS Transport", "synced with peer: %@", peerID);
        } else {
            secnotice("KVS Transport", "failed to sync with peer: %@ error: %@", peerID, localError);
        }
    });

    return handled;
}

static __nonnull CF_RETURNS_RETAINED CFSetRef SOSAccountSyncWithPeersOverIDS(SOSAccountTransactionRef txn, __nonnull CFSetRef peers) {
    CFErrorRef localError = NULL;

    CFStringSetPerformWithDescription(peers, ^(CFStringRef peerDescription) {
        secnotice("IDS Transport","Syncing with IDS capable peers: %@", peerDescription);
    });

    // We should change this to return a set of peers we succeeded with, but for now assume they all worked.
    bool result = SOSTransportMessageSyncWithPeers(txn->account->ids_message_transport, peers, &localError);
    secnotice("IDS Transport", "IDS Sync result: %d", result);

    return CFRetainSafe(peers);
}

static CF_RETURNS_RETAINED CFMutableSetRef SOSAccountSyncWithPeers(SOSAccountTransactionRef txn, CFSetRef /* CFStringRef */ peerIDs, CFErrorRef *error) {
    CFMutableSetRef notMePeers = NULL;
    CFMutableSetRef handledPeerIDs = NULL;
    CFMutableSetRef peersForIDS = NULL;
    CFMutableSetRef peersForKVS = NULL;

    SOSAccountRef account = txn->account;

    require_action_quiet(SOSAccountIsInCircle(account, error), done,
                         handledPeerIDs = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, peerIDs));

    // Kick getting our device ID if we don't have it, and find out if we're setup to use IDS.
    bool canUseIDS = SOSTransportMessageIDSGetIDSDeviceID(account);

    handledPeerIDs = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    peersForIDS = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    peersForKVS = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSPeerInfoRef myPeerInfo = SOSAccountGetMyPeerInfo(account);
    require(myPeerInfo, done);

    CFStringRef myPeerID = SOSPeerInfoGetPeerID(myPeerInfo);

    notMePeers = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, peerIDs);
    CFSetRemoveValue(notMePeers, myPeerID);

    if(!SOSAccountSendIKSPSyncList(account, error)){
        if(error != NULL)
            secnotice("IDS Transport", "Did not send list of peers to ping (pre-E): %@", *error);
    }
    
    CFSetForEach(notMePeers, ^(const void *value) {
        CFErrorRef localError = NULL;
        CFStringRef peerID = asString(value, &localError);
        SOSPeerInfoRef peerInfo = NULL;

        require_quiet(peerID, skip);

        peerInfo = SOSCircleCopyPeerWithID(account->trusted_circle, peerID, NULL);
        if (peerInfo && SOSCircleHasValidSyncingPeer(account->trusted_circle, peerInfo, account->user_public, NULL)) {
            if (canUseIDS && SOSPeerInfoShouldUseIDSTransport(myPeerInfo, peerInfo) && SOSPeerInfoShouldUseACKModel(myPeerInfo, peerInfo)) {
                CFSetAddValue(peersForIDS, peerID);
            } else {
                CFSetAddValue(peersForKVS, peerID);
            }
        } else {
            CFSetAddValue(handledPeerIDs, peerID);
        }

    skip:
        CFReleaseNull(peerInfo);
        if (localError) {
            secnotice("sync-with-peers", "Skipped peer ID: %@ due to %@", peerID, localError);
        }
        CFReleaseNull(localError);
    });

    CFSetRef handledIDSPeerIDs = SOSAccountSyncWithPeersOverIDS(txn, peersForIDS);
    CFSetUnion(handledPeerIDs, handledIDSPeerIDs);
    CFReleaseNull(handledIDSPeerIDs);

    CFSetRef handledKVSPeerIDs = SOSAccountSyncWithPeersOverKVS(txn, peersForKVS);
    CFSetUnion(handledPeerIDs, handledKVSPeerIDs);
    CFReleaseNull(handledKVSPeerIDs);

    SOSAccountConsiderLoggingEngineState(txn);

done:
    CFReleaseNull(notMePeers);
    CFReleaseNull(peersForIDS);
    CFReleaseNull(peersForKVS);
    return handledPeerIDs;
}

bool SOSAccountClearPeerMessageKey(SOSAccountTransactionRef txn, CFStringRef peerID, CFErrorRef *error)
{
    SOSAccountRef account = txn->account;

    secnotice("IDS Transport", "clearing peer message for %@", peerID);
    CFTypeRef dsid = SOSAccountGetValue(account, kSOSDSIDKey, error);

    if(dsid == NULL)
        dsid = kCFNull;

    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer(account->kvs_message_transport, peerID);
    CFDictionaryRef a_message_to_a_peer = CFDictionaryCreateForCFTypes(NULL, message_to_peer_key, kCFNull, kSOSKVSRequiredKey, dsid, NULL);

    CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef block_error) {
        if (block_error) {
            secerror("Error putting: %@", block_error);
        }
    };

    SOSCloudKeychainPutObjectsInCloud(a_message_to_a_peer, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), log_error);

    CFReleaseNull(a_message_to_a_peer);
    CFReleaseNull(message_to_peer_key);

    return true;
}

CF_RETURNS_RETAINED CFSetRef SOSAccountProcessSyncWithPeers(SOSAccountTransactionRef txn, CFSetRef /* CFStringRef */ peers, CFSetRef /* CFStringRef */ backupPeers, CFErrorRef *error)
{
    CFErrorRef localError = NULL;
    CFMutableSetRef handled = SOSAccountSyncWithPeers(txn, peers, &localError);

    SOSTransportMessageIDSGetIDSDeviceID(txn->account);

    if (!handled) {
        secnotice("account-sync", "Peer Sync failed: %@", localError);
        handled = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    }
    CFReleaseNull(localError);

    SOSEngineRef engine = SOSTransportMessageGetEngine(txn->account->kvs_message_transport);
    CFSetRef engineHandled = SOSEngineSyncWithBackupPeers(engine, backupPeers, error);

    if (engineHandled) {
        CFSetUnion(handled, engineHandled);
    } else {
        secnotice("account-sync", "Engine Backup Sync failed: %@", localError);
    }
    CFReleaseNull(localError);
    CFReleaseNull(engineHandled);

    return handled;
}

bool SOSAccountRequestSyncWithAllPeers(SOSAccountTransactionRef txn, CFErrorRef *error)
{
    bool success = false;
    CFMutableSetRef allSyncingPeers = NULL;

    require_quiet(SOSAccountIsInCircle(txn->account, error), xit);

    SOSTransportMessageIDSGetIDSDeviceID(txn->account);

    allSyncingPeers = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSCircleForEachValidSyncingPeer(txn->account->trusted_circle, txn->account->user_public, ^(SOSPeerInfoRef peer) {
        CFSetAddValue(allSyncingPeers, SOSPeerInfoGetPeerID(peer));
    });

    SOSAccountTransactionAddSyncRequestForAllPeerIDs(txn, allSyncingPeers);

    success = true;

xit:
    CFReleaseNull(allSyncingPeers);
    return success;
}

//
// MARK: Syncing status functions
//
bool SOSAccountMessageFromPeerIsPending(SOSAccountTransactionRef txn, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool success = false;
    require_quiet(SOSAccountIsInCircle(txn->account, error), xit);

    // This translation belongs inside KVS..way down in CKD, but for now we reach over and do it here.
    CFStringRef peerMessage = SOSMessageKeyCreateFromPeerToTransport(txn->account->kvs_message_transport, SOSPeerInfoGetPeerID(peer));

    success = SOSCloudKeychainHasPendingKey(peerMessage, error);
    
xit:
    return success;
}

bool SOSAccountSendToPeerIsPending(SOSAccountTransactionRef txn, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool success = false;
    require_quiet(SOSAccountIsInCircle(txn->account, error), xit);

    success = SOSCCIsSyncPendingFor(SOSPeerInfoGetPeerID(peer), error);
xit:
    return success;


}
