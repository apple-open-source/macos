
#include "SOSAccountPriv.h"
#include "SOSAccount.h"

#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportCircleKVS.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>

#import <Security/SecureObjectSync/SOSAccountTrust.h>
#import <Security/SecureObjectSync/SOSTransport.h>
#import <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#import <Security/SecureObjectSync/SOSTransportMessage.h>
#import "Security/SecureObjectSync/SOSTransportMessageIDS.h"
#import <Security/SecureObjectSync/SOSTransportMessageKVS.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>
#include <Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h>
#include <Security/SecureObjectSync/SOSInternal.h>

#include <CoreFoundation/CoreFoundation.h>

#include <utilities/SecCFError.h>

// MARK: Engine Logging
#define LOG_ENGINE_STATE_INTERVAL 20

void SOSAccountConsiderLoggingEngineState(SOSAccountTransaction* txn) {
    static int engineLogCountDown = 0;

    if(engineLogCountDown <= 0) {
        SOSAccount* acct = txn.account;
        CFTypeRef engine = [acct.kvs_message_transport SOSTransportMessageGetEngine];

        SOSEngineLogState((SOSEngineRef)engine);
        engineLogCountDown = LOG_ENGINE_STATE_INTERVAL;
    } else {
        engineLogCountDown--;
    }
}

bool SOSAccountInflateTransports(SOSAccount* account, CFStringRef circleName, CFErrorRef *error){
    bool success = false;
    
    if(account.key_transport)
        SOSUnregisterTransportKeyParameter(account.key_transport);
    if(account.circle_transport)
        SOSUnregisterTransportCircle(account.circle_transport);
    if(account.ids_message_transport)
        SOSUnregisterTransportMessage((SOSMessage*)account.ids_message_transport);
    if(account.kvs_message_transport)
        SOSUnregisterTransportMessage((SOSMessage*)account.kvs_message_transport);
    
    account.key_transport = [[CKKeyParameter alloc] initWithAccount:account];
    account.circle_transport = [[SOSKVSCircleStorageTransport alloc]initWithAccount:account andCircleName:(__bridge NSString *)(circleName)];
    
    require_quiet(account.key_transport, fail);
    require_quiet(account.circle_transport, fail);
    
    account.ids_message_transport = [[SOSMessageIDS alloc] initWithAccount:account andName:(__bridge NSString *)(circleName)];
    require_quiet(account.ids_message_transport, fail);
    
    account.kvs_message_transport = [[SOSMessageKVS alloc] initWithAccount:account andName:(__bridge NSString*)circleName];
    require_quiet(account.kvs_message_transport, fail);
    
    success = true;
    
fail:
    return success;
}

static bool SOSAccountIsThisPeerIDMe(SOSAccount* account, CFStringRef peerID) {
    NSString* myPeerID = account.peerID;
    
    return myPeerID && [myPeerID isEqualToString: (__bridge NSString*) peerID];
}

bool SOSAccountSendIKSPSyncList(SOSAccount* account, CFErrorRef *error){
    bool result = true;
    __block CFErrorRef localError = NULL;
    __block CFMutableArrayRef ids = NULL;
    SOSCircleRef circle = NULL;
    SOSFullPeerInfoRef identity = NULL;

    if(![account.trust isInCircle:NULL])
    {
        SOSCreateError(kSOSErrorNoCircle, CFSTR("This device is not in circle"), NULL, &localError);
        if(error && *error != NULL)
            secerror("SOSAccountSendIKSPSyncList had an error: %@", *error);

        if(localError)
            secerror("SOSAccountSendIKSPSyncList had an error: %@", localError);

        CFReleaseNull(ids);
        CFReleaseNull(localError);
        
        return result;
    }

    circle  = account.trust.trustedCircle;
    identity = account.fullPeerInfo;
    ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSCircleForEachValidPeer(circle, account.accountKey, ^(SOSPeerInfoRef peer) {
        if (!SOSAccountIsThisPeerIDMe(account, SOSPeerInfoGetPeerID(peer))) {
            if(SOSPeerInfoShouldUseIDSTransport(SOSFullPeerInfoGetPeerInfo(identity), peer) &&
               SOSPeerInfoShouldUseIDSMessageFragmentation(SOSFullPeerInfoGetPeerInfo(identity), peer) &&
               !SOSPeerInfoShouldUseACKModel(SOSFullPeerInfoGetPeerInfo(identity), peer)){
                [account.ids_message_transport SOSTransportMessageIDSSetFragmentationPreference:account.ids_message_transport pref:kCFBooleanTrue];
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
    
    SOSCloudKeychainGetIDSDeviceAvailability(ids, (__bridge CFStringRef)(account.peerID), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {
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

static bool SOSAccountSyncWithKVSPeers(SOSAccountTransaction* txn, CFSetRef peerIDs, CFErrorRef *error) {
    SOSAccount* account = txn.account;
    CFErrorRef localError = NULL;
    bool result = false;

    require_quiet([account.trust isInCircle:error], xit);

    result =[account.kvs_message_transport SOSTransportMessageSyncWithPeers:account.kvs_message_transport p:peerIDs err:&localError];

    if (result)
        SetCloudKeychainTraceValueForKey(kCloudKeychainNumberOfTimesSyncedWithPeers, 1);

xit:
    if (!result) {
        // Tell account to update SOSEngine with current trusted peers
        if (isSOSErrorCoded(localError, kSOSErrorPeerNotFound)) {
            secnotice("Account", "Arming account to update SOSEngine with current trusted peers");
            account.engine_peer_state_needs_repair = true;
        }
        CFErrorPropagate(localError, error);
        localError = NULL;
    }
    return result;
    
}

bool SOSAccountSyncWithKVSPeerWithMessage(SOSAccountTransaction* txn, CFStringRef peerid, CFDataRef message, CFErrorRef *error) {
    SOSAccount* account = txn.account;
    bool result = false;
    CFErrorRef localError = NULL;
    CFDictionaryRef encapsulatedMessage = NULL;

    secnotice("KVS Transport","Syncing with KVS capable peer: %@", peerid);
    secnotice("KVS Transport", "message: %@", message);

    require_quiet(message, xit);
    require_quiet(peerid && CFStringGetLength(peerid) <= kSOSPeerIDLengthMax, xit);

    encapsulatedMessage = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, peerid, message, NULL);

    result = [account.kvs_message_transport SOSTransportMessageSendMessages:account.kvs_message_transport pm:encapsulatedMessage err:&localError];
    secerror("KVS sync %s. (%@)", result ? "succeeded" : "failed", localError);

    SOSAccountConsiderLoggingEngineState(txn);

xit:
    CFReleaseNull(encapsulatedMessage);
    CFErrorPropagate(localError, error);
    
    return result;
}


static bool SOSAccountSyncWithKVSPeer(SOSAccountTransaction* txn, CFStringRef peerID, CFErrorRef *error)
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

static CFMutableArrayRef SOSAccountCopyPeerIDsForDSID(SOSAccount* account, CFStringRef deviceID, CFErrorRef* error) {
    CFMutableArrayRef peerIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSCircleForEachValidPeer(account.trust.trustedCircle, account.accountKey, ^(SOSPeerInfoRef peer) {
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

static bool SOSAccountSyncWithKVSPeerFromPing(SOSAccount* account, CFArrayRef peerIDs, CFErrorRef *error) {

    CFErrorRef localError = NULL;
    bool result = false;

    CFSetRef peerSet = CFSetCreateCopyOfArrayForCFTypes(peerIDs);
    result = [account.kvs_message_transport SOSTransportMessageSyncWithPeers:account.kvs_message_transport p:peerSet err:&localError];

    CFReleaseNull(peerSet);

    return result;
}

bool SOSAccountSyncWithKVSUsingIDSID(SOSAccount* account, CFStringRef deviceID, CFErrorRef *error) {
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

CFSetRef SOSAccountSyncWithPeersOverKVS(SOSAccountTransaction* txn, CFSetRef peers) {
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

CF_RETURNS_RETAINED CFSetRef SOSAccountSyncWithPeersOverIDS(SOSAccountTransaction* txn, CFSetRef peers) {
    CFErrorRef localError = NULL;
    SOSAccount* account = txn.account;

    CFStringSetPerformWithDescription(peers, ^(CFStringRef peerDescription) {
        secnotice("IDS Transport","Syncing with IDS capable peers: %@", peerDescription);
    });

    // We should change this to return a set of peers we succeeded with, but for now assume they all worked.
    bool result = [account.ids_message_transport SOSTransportMessageSyncWithPeers:account.ids_message_transport p:peers err:&localError];
    secnotice("IDS Transport", "IDS Sync result: %d", result);

    return CFSetCreateCopy(kCFAllocatorDefault, peers);
}

CF_RETURNS_RETAINED CFMutableSetRef SOSAccountSyncWithPeers(SOSAccountTransaction* txn, CFSetRef /* CFStringRef */ peerIDs, CFErrorRef *error) {
    CFMutableSetRef notMePeers = NULL;
    CFMutableSetRef handledPeerIDs = NULL;
    CFMutableSetRef peersForIDS = NULL;
    CFMutableSetRef peersForKVS = NULL;

    SOSAccount* account = txn.account;

    // Kick getting our device ID if we don't have it, and find out if we're setup to use IDS.
    bool canUseIDS = [account.ids_message_transport SOSTransportMessageIDSGetIDSDeviceID:account];

    if(![account.trust isInCircle:error])
    {
        handledPeerIDs = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, peerIDs);
        CFReleaseNull(notMePeers);
        CFReleaseNull(peersForIDS);
        CFReleaseNull(peersForKVS);
        return handledPeerIDs;
    }

    handledPeerIDs = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    peersForIDS = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    peersForKVS = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSPeerInfoRef myPeerInfo = account.peerInfo;
    if(!myPeerInfo)
    {
        CFReleaseNull(notMePeers);
        CFReleaseNull(peersForIDS);
        CFReleaseNull(peersForKVS);
        return handledPeerIDs;

    }
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
        SOSCircleRef circle = NULL;
        SOSAccountTrustClassic *trust = account.trust;
        circle = trust.trustedCircle;
        require_quiet(peerID, skip);

        peerInfo = SOSCircleCopyPeerWithID(circle, peerID, NULL);
        if (peerInfo && SOSCircleHasValidSyncingPeer(circle, peerInfo, account.accountKey, NULL)) {
            if (ENABLE_IDS && canUseIDS && SOSPeerInfoShouldUseIDSTransport(myPeerInfo, peerInfo) && SOSPeerInfoShouldUseACKModel(myPeerInfo, peerInfo)) {
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

    CFReleaseNull(notMePeers);
    CFReleaseNull(peersForIDS);
    CFReleaseNull(peersForKVS);
    return handledPeerIDs;
}

bool SOSAccountClearPeerMessageKey(SOSAccountTransaction* txn, CFStringRef peerID, CFErrorRef *error)
{
    if (peerID == NULL) {
        return false;
    }

    SOSAccount* account = txn.account;

    secnotice("IDS Transport", "clearing peer message for %@", peerID);
    CFTypeRef dsid = SOSAccountGetValue(account, kSOSDSIDKey, error);

    if(dsid == NULL)
        dsid = kCFNull;

    CFStringRef myID = (__bridge CFStringRef)(account.peerID);
    CFStringRef message_to_peer_key = SOSMessageKeyCreateFromTransportToPeer(account.kvs_message_transport, myID, peerID);
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

CF_RETURNS_RETAINED CFSetRef SOSAccountProcessSyncWithPeers(SOSAccountTransaction* txn, CFSetRef /* CFStringRef */ peers, CFSetRef /* CFStringRef */ backupPeers, CFErrorRef *error)
{
    CFErrorRef localError = NULL;
    SOSAccount* account = txn.account;

    CFMutableSetRef handled = SOSAccountSyncWithPeers(txn, peers, &localError);

    [account.ids_message_transport SOSTransportMessageIDSGetIDSDeviceID:account];

    if (!handled) {
        secnotice("account-sync", "Peer Sync failed: %@", localError);
        handled = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    }
    CFReleaseNull(localError);
    
    CFTypeRef engine = [account.kvs_message_transport SOSTransportMessageGetEngine];
    CFSetRef engineHandled = SOSEngineSyncWithBackupPeers((SOSEngineRef)engine, backupPeers, false, error);
    
    if (engineHandled) {
        CFSetUnion(handled, engineHandled);
    } else {
        secnotice("account-sync", "Engine Backup Sync failed: %@", localError);
    }
    CFReleaseNull(localError);
    CFReleaseNull(engineHandled);

    return handled;
}

CF_RETURNS_RETAINED CFSetRef SOSAccountCopyBackupPeersAndForceSync(SOSAccountTransaction* txn, CFErrorRef *error)
{
    SOSEngineRef engine = (SOSEngineRef) [txn.account.kvs_message_transport SOSTransportMessageGetEngine];

    NSArray* backupPeersArray = (NSArray*) CFBridgingRelease(SOSEngineCopyBackupPeerNames(engine, error));
    NSSet* backupPeers = [[NSSet alloc] initWithArray: backupPeersArray];
    return SOSEngineSyncWithBackupPeers(engine, (__bridge CFSetRef) backupPeers, true, error);
}

bool SOSAccountRequestSyncWithAllPeers(SOSAccountTransaction* txn, CFErrorRef *error)
{
    SOSAccount* account = txn.account;
    SOSAccountTrustClassic *trust = account.trust;

    if (![account.trust isInCircle:error])
        return false;

    NSMutableSet<NSString*>* allSyncingPeers = [NSMutableSet set];
    SOSCircleRef circle = trust.trustedCircle;

    // Tickle IDS in case we haven't even tried when we're syncing.
    [account.ids_message_transport SOSTransportMessageIDSGetIDSDeviceID:account];

    SOSCircleForEachValidSyncingPeer(circle, account.accountKey, ^(SOSPeerInfoRef peer) {
        [allSyncingPeers addObject: (__bridge NSString*) SOSPeerInfoGetPeerID(peer)];
    });

    [txn requestSyncWithPeers:allSyncingPeers];

    return true;
}

//
// MARK: Syncing status functions
//
bool SOSAccountMessageFromPeerIsPending(SOSAccountTransaction* txn, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool success = false;
    SOSAccount* account = txn.account;
    require_quiet([account.trust isInCircle:error], xit);

    // This translation belongs inside KVS..way down in CKD, but for now we reach over and do it here.
    CFStringRef peerMessage = SOSMessageKeyCreateFromPeerToTransport([account kvs_message_transport], (__bridge CFStringRef)(account.peerID), SOSPeerInfoGetPeerID(peer));

    success = SOSCloudKeychainHasPendingKey(peerMessage, error);
    CFReleaseNull(peerMessage);
    
xit:
    return success;
}

bool SOSAccountSendToPeerIsPending(SOSAccountTransaction* txn, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool success = false;
    SOSAccount* account = txn.account;
    require_quiet([account.trust isInCircle:error], xit);

    success = SOSCCIsSyncPendingFor(SOSPeerInfoGetPeerID(peer), error);
xit:
    return success;


}

