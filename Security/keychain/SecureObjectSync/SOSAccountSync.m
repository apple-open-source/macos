
#include "SOSAccountPriv.h"
#include "SOSAccount.h"

#include "keychain/SecureObjectSync/SOSKVSKeys.h"
#include "keychain/SecureObjectSync/SOSTransportCircle.h"
#include "keychain/SecureObjectSync/SOSTransportCircleKVS.h"
#include "keychain/SecureObjectSync/SOSTransportMessage.h"
#include "keychain/SecureObjectSync/SOSKVSKeys.h"
#include "keychain/SecureObjectSync/SOSTransport.h"
#include "keychain/SecureObjectSync/SOSTransportKeyParameter.h"
#include "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"

#import "keychain/SecureObjectSync/SOSAccountTrust.h"
#import "keychain/SecureObjectSync/SOSTransport.h"
#import "keychain/SecureObjectSync/SOSTransportKeyParameter.h"
#include "keychain/SecureObjectSync/SOSTransportMessage.h"
#import "keychain/SecureObjectSync/SOSTransportMessageKVS.h"
#include "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#include "keychain/SecureObjectSync/SOSInternal.h"

#include <CoreFoundation/CoreFoundation.h>

#include <utilities/SecCFError.h>

// MARK: Engine Logging
#define LOG_ENGINE_STATE_INTERVAL 20
#define LOG_CATEGORY "account-sync"

static bool SOSReadyToSync(SOSAccount *account, CFErrorRef *error) {
    if (![account isInCircle:error]) {
        secnotice(LOG_CATEGORY, "Not performing requested sync operation: not in circle yet");
        return false;
    }
    return true;
}

void SOSAccountConsiderLoggingEngineState(SOSAccountTransaction* txn) {
    static int engineLogCountDown = 0;

    if(engineLogCountDown <= 0) {
        SOSAccount* acct = txn.account;
        if(SOSReadyToSync(acct, NULL)) {
            CFTypeRef engine = [acct.kvs_message_transport SOSTransportMessageGetEngine];
            SOSEngineLogState((SOSEngineRef)engine);
            engineLogCountDown = LOG_ENGINE_STATE_INTERVAL;
        }
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
    if(account.kvs_message_transport)
        SOSUnregisterTransportMessage((SOSMessage*)account.kvs_message_transport);
    
    account.key_transport = [[CKKeyParameter alloc] initWithAccount:account];
    account.circle_transport = [[SOSKVSCircleStorageTransport alloc]initWithAccount:account andCircleName:(__bridge NSString *)(circleName)];
    
    require_quiet(account.key_transport, fail);
    require_quiet(account.circle_transport, fail);
        
    account.kvs_message_transport = [[SOSMessageKVS alloc] initWithAccount:account andName:(__bridge NSString*)circleName];
    require_quiet(account.kvs_message_transport, fail);
    
    success = true;
    
fail:
    return success;
}

//
// MARK: KVS Syncing
//

static bool SOSAccountSyncWithKVSPeers(SOSAccountTransaction* txn, CFSetRef peerIDs, CFErrorRef *error) {
    SOSAccount* account = txn.account;
    CFErrorRef localError = NULL;
    bool result = false;

    if(SOSReadyToSync(account, error)) {
        result =[account.kvs_message_transport SOSTransportMessageSyncWithPeers:account.kvs_message_transport p:peerIDs err:&localError];
    }
    if (!result) {
        // Tell account to update SOSEngine with current trusted peers
        if (isSOSErrorCoded(localError, kSOSErrorPeerNotFound)) {
            secnotice(LOG_CATEGORY, "Arming account to update SOSEngine with current trusted peers");
            account.engine_peer_state_needs_repair = true;
        }
        CFErrorPropagate(localError, error);
        localError = NULL;
    }
    return result;
    
}

bool SOSAccountSyncWithKVSPeerWithMessage(SOSAccountTransaction* txn, CFStringRef peerid, CFDataRef message, CFErrorRef *error) {
    //murfxx protect
    SOSAccount* account = txn.account;
    bool result = false;
    CFErrorRef localError = NULL;
    CFDictionaryRef encapsulatedMessage = NULL;

    secnotice(LOG_CATEGORY,"Syncing with KVS capable peer: %@", peerid);
    secnotice(LOG_CATEGORY, "message: %@", message);

    require_quiet(message, xit);
    require_quiet(peerid && CFStringGetLength(peerid) <= kSOSPeerIDLengthMax, xit);
    if(SOSReadyToSync(account, &localError)) {
        encapsulatedMessage = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, peerid, message, NULL);
        result = [account.kvs_message_transport SOSTransportMessageSendMessages:account.kvs_message_transport pm:encapsulatedMessage err:&localError];
        secnotice(LOG_CATEGORY, "KVS sync %s. (%@)", result ? "succeeded" : "failed", localError);
        SOSAccountConsiderLoggingEngineState(txn);
    }

xit:
    CFReleaseNull(encapsulatedMessage);
    CFErrorPropagate(localError, error);
    
    return result;
}


static bool SOSAccountSyncWithKVSPeer(SOSAccountTransaction* txn, CFStringRef peerID, CFErrorRef *error)
{
    bool result = false;
    CFErrorRef localError = NULL;
    SOSAccount* account = txn.account;

    if(SOSReadyToSync(account, error)) {
        secnotice(LOG_CATEGORY,"Syncing with KVS capable peer: %@", peerID);
        CFMutableSetRef peerIDs = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        CFSetAddValue(peerIDs, peerID);
        result = SOSAccountSyncWithKVSPeers(txn, peerIDs, &localError);
        secnotice(LOG_CATEGORY, "KVS sync %s. (%@)", result ? "succeeded" : "failed", localError);

        CFReleaseNull(peerIDs);
        CFErrorPropagate(localError, error);
    }

    return result;
}


CFSetRef SOSAccountSyncWithPeersOverKVS(SOSAccountTransaction* txn, CFSetRef peers) {
    CFMutableSetRef handled = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    if(SOSReadyToSync(txn.account, NULL)) {
        CFSetForEach(peers, ^(const void *value) {
            CFStringRef peerID = asString(value, NULL);
            CFErrorRef localError = NULL;
            if (peerID && SOSAccountSyncWithKVSPeer(txn, peerID, &localError)) {
                CFSetAddValue(handled, peerID);
                secnotice(LOG_CATEGORY, "synced with peer: %@", peerID);
            } else {
                secnotice(LOG_CATEGORY, "failed to sync with peer: %@ error: %@", peerID, localError);
            }
            CFReleaseNull(localError);
        });
    }

    return handled;
}

CF_RETURNS_RETAINED CFMutableSetRef SOSAccountSyncWithPeers(SOSAccountTransaction* txn, CFSetRef /* CFStringRef */ peerIDs, CFErrorRef *error) {
    CFMutableSetRef notMePeers = NULL;
    CFMutableSetRef handledPeerIDs = NULL;
    CFMutableSetRef peersForKVS = NULL;

    SOSAccount* account = txn.account;
    if(!SOSReadyToSync(account, error)) {
        handledPeerIDs = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, peerIDs);
        CFReleaseNull(notMePeers);
        CFReleaseNull(peersForKVS);
        return handledPeerIDs;
    }

    handledPeerIDs = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    peersForKVS = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSPeerInfoRef myPeerInfo = account.peerInfo;
    if(!myPeerInfo)
    {
        CFReleaseNull(notMePeers);
        CFReleaseNull(peersForKVS);
        return handledPeerIDs;

    }
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(myPeerInfo);

    notMePeers = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, peerIDs);
    CFSetRemoveValue(notMePeers, myPeerID);
    
    CFSetForEach(notMePeers, ^(const void *value) {
        CFErrorRef localError = NULL;
        CFStringRef peerID = asString(value, &localError);
        SOSPeerInfoRef peerInfo = NULL;
        SOSCircleRef circle = NULL;
        SOSAccountTrustClassic *trust = account.trust;
        circle = trust.trustedCircle;
        require_quiet(peerID, skip);

        peerInfo = SOSCircleCopyPeerWithID(circle, peerID, NULL);
        if (peerInfo && SOSCircleHasValidSyncingPeer(circle, peerInfo, account.accountKey, NULL)){
            CFSetAddValue(peersForKVS, peerID);
        } else {
            CFSetAddValue(handledPeerIDs, peerID);
        }

    skip:
        CFReleaseNull(peerInfo);
        if (localError) {
            secnotice(LOG_CATEGORY, "Skipped peer ID: %@ due to %@", peerID, localError);
        }
        CFReleaseNull(localError);
    });

    CFSetRef handledKVSPeerIDs = SOSAccountSyncWithPeersOverKVS(txn, peersForKVS);
    CFSetUnion(handledPeerIDs, handledKVSPeerIDs);
    CFReleaseNull(handledKVSPeerIDs);

    SOSAccountConsiderLoggingEngineState(txn);

    CFReleaseNull(notMePeers);
    CFReleaseNull(peersForKVS);
    return handledPeerIDs;
}

CF_RETURNS_RETAINED CFSetRef SOSAccountProcessSyncWithPeers(SOSAccountTransaction* txn, CFSetRef /* CFStringRef */ peers, CFSetRef /* CFStringRef */ backupPeers, CFErrorRef *error)
{
    CFErrorRef localError = NULL;
    SOSAccount* account = txn.account;

    if(!SOSReadyToSync(account, error)) {
        return CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    }

    CFMutableSetRef handled = SOSAccountSyncWithPeers(txn, peers, &localError);

    if (!handled) {
        secnotice(LOG_CATEGORY, "Peer Sync failed: %@", localError);
        handled = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    }
    CFReleaseNull(localError);
    
    CFTypeRef engine = [account.kvs_message_transport SOSTransportMessageGetEngine];
    CFSetRef engineHandled = SOSEngineSyncWithBackupPeers((SOSEngineRef)engine, backupPeers, false, error);
    
    if (engineHandled) {
        CFSetUnion(handled, engineHandled);
    } else {
        secnotice(LOG_CATEGORY, "Engine Backup Sync failed: %@", localError);
    }
    CFReleaseNull(localError);
    CFReleaseNull(engineHandled);

    return handled;
}

CF_RETURNS_RETAINED CFSetRef SOSAccountCopyBackupPeersAndForceSync(SOSAccountTransaction* txn, CFErrorRef *error)
{
    SOSAccount* account = txn.account;

    if(!SOSReadyToSync(account, error)) {
        return false;
    }

    SOSEngineRef engine = (SOSEngineRef) [txn.account.kvs_message_transport SOSTransportMessageGetEngine];
    NSArray* backupPeersArray = (NSArray*) CFBridgingRelease(SOSEngineCopyBackupPeerNames(engine, error));
    NSSet* backupPeers = [[NSSet alloc] initWithArray: backupPeersArray];
    return SOSEngineSyncWithBackupPeers(engine, (__bridge CFSetRef) backupPeers, true, error);
}

bool SOSAccountRequestSyncWithAllPeers(SOSAccountTransaction* txn, CFErrorRef *error)
{
    SOSAccount* account = txn.account;
    SOSAccountTrustClassic *trust = account.trust;

    if(!SOSReadyToSync(account, error)) {
        return false;
    }

    NSMutableSet<NSString*>* allSyncingPeers = [NSMutableSet set];
    SOSCircleRef circle = trust.trustedCircle;

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
    if(SOSReadyToSync(account, error)) {
        // This translation belongs inside KVS..way down in CKD, but for now we reach over and do it here.
        CFStringRef peerMessage = SOSMessageKeyCreateFromPeerToTransport([account kvs_message_transport], (__bridge CFStringRef)(account.peerID), SOSPeerInfoGetPeerID(peer));
        success = SOSCloudKeychainHasPendingKey(peerMessage, error);
        CFReleaseNull(peerMessage);
    }
    return success;
}

bool SOSAccountSendToPeerIsPending(SOSAccountTransaction* txn, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool success = false;
    SOSAccount* account = txn.account;
    if(SOSReadyToSync(account, error)) {
        success = SOSCCIsSyncPendingFor(SOSPeerInfoGetPeerID(peer), error);
    }
    return success;
}

