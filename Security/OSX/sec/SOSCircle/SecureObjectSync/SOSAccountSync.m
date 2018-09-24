
#include "SOSAccountPriv.h"
#include "SOSAccount.h"

#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportCircleKVS.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>

#import <Security/SecureObjectSync/SOSAccountTrust.h>
#import <Security/SecureObjectSync/SOSTransport.h>
#import <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#import <Security/SecureObjectSync/SOSTransportMessage.h>
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

    require_quiet([account isInCircle:error], xit);

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

CF_RETURNS_RETAINED CFMutableSetRef SOSAccountSyncWithPeers(SOSAccountTransaction* txn, CFSetRef /* CFStringRef */ peerIDs, CFErrorRef *error) {
    CFMutableSetRef notMePeers = NULL;
    CFMutableSetRef handledPeerIDs = NULL;
    CFMutableSetRef peersForKVS = NULL;

    SOSAccount* account = txn.account;

    if(![account isInCircle:error])
    {
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
            secnotice("sync-with-peers", "Skipped peer ID: %@ due to %@", peerID, localError);
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

    CFMutableSetRef handled = SOSAccountSyncWithPeers(txn, peers, &localError);

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

    if (![account isInCircle:error])
        return false;

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
    require_quiet([account isInCircle:error], xit);

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
    require_quiet([account isInCircle:error], xit);

    success = SOSCCIsSyncPendingFor(SOSPeerInfoGetPeerID(peer), error);
xit:
    return success;


}

