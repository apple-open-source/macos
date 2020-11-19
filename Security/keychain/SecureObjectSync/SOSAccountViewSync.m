//
//  SOSAccountViews.c
//  sec
//
//  Created by Mitch Adler on 6/10/16.
//
//


#include <CoreFoundation/CoreFoundation.h>

#include "keychain/SecureObjectSync/SOSAccount.h"
#include "SOSViews.h"
#include "SOSAccountPriv.h"

#include <utilities/SecCFWrappers.h>
#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Identity.h"

//
// MARK: Helpers
//

static CFMutableSetRef SOSAccountCopyOtherPeersViews(SOSAccount* account) {
    __block CFMutableSetRef otherPeersViews = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
        SOSPeerInfoWithEnabledViewSet(peer, ^(CFSetRef enabled) {
            CFSetUnion(otherPeersViews, enabled);
        });
    });

    return otherPeersViews;
}

//
// MARK: Outstanding tracking
//

static bool isInitialSyncActive(void) {
    static dispatch_once_t onceToken;
    __block bool active = true;

    dispatch_once(&onceToken, ^{
        CFSetRef initialSyncViews = SOSViewCopyViewSet(kViewSetInitial);
        active = CFSetGetCount(initialSyncViews) > 0;
        CFReleaseNull(initialSyncViews);
    });

    return active;
}

void SOSAccountInitializeInitialSync(SOSAccount* account) {
    if(!account) {
        return;
    }
    if(isInitialSyncActive()) {
        SOSAccountSetValue(account, kSOSUnsyncedViewsKey, kCFBooleanTrue, NULL);
    } else {
        SOSAccountSetValue(account, kSOSUnsyncedViewsKey, kCFBooleanFalse, NULL);
    }
}


CFMutableSetRef SOSAccountCopyOutstandingViews(SOSAccount* account) {
    CFSetRef initialSyncViews = SOSViewCopyViewSet(kViewSetAll);
    CFMutableSetRef result = SOSAccountCopyIntersectionWithOustanding(account, initialSyncViews);
    CFReleaseNull(initialSyncViews);
    return result;
}
bool SOSAccountIsViewOutstanding(SOSAccount* account, CFStringRef view) {
    bool isOutstandingView;
    require_action_quiet([account getCircleStatus:NULL] == kSOSCCInCircle, done, isOutstandingView = true);
    CFTypeRef unsyncedObject = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, NULL);
    require_action_quiet(unsyncedObject, done, isOutstandingView = false);
    CFBooleanRef unsyncedBool = asBoolean(unsyncedObject, NULL);
    if (unsyncedBool) {
        isOutstandingView = CFBooleanGetValue(unsyncedBool);
    } else {
        CFSetRef unsyncedSet = asSet(unsyncedObject, NULL);
        isOutstandingView = unsyncedSet && CFSetContainsValue(unsyncedSet, view);
    }
done:
    return isOutstandingView;
}
CFMutableSetRef SOSAccountCopyIntersectionWithOustanding(SOSAccount* account, CFSetRef inSet) {
    CFTypeRef unsyncedObject = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, NULL);
    CFMutableSetRef result = NULL;
    require_quiet([account getCircleStatus:NULL] == kSOSCCInCircle, done);
    CFBooleanRef unsyncedBool = asBoolean(unsyncedObject, NULL);
    if (unsyncedBool) {
        if (!CFBooleanGetValue(unsyncedBool)) {
            result = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        }
    } else {
        CFSetRef unsyncedSet = asSet(unsyncedObject, NULL);
        if (unsyncedSet) {
            result = CFSetCreateIntersection(kCFAllocatorDefault, unsyncedSet, inSet);
        } else {
            result = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        }
    }
done:
    if (result == NULL) {
        result = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, inSet);
    }
    return result;
}
bool SOSAccountIntersectsWithOutstanding(SOSAccount* account, CFSetRef views) {
    CFSetRef nonInitiallySyncedViews = SOSAccountCopyIntersectionWithOustanding(account, views);
    bool intersects = !CFSetIsEmpty(nonInitiallySyncedViews);
    CFReleaseNull(nonInitiallySyncedViews);
    return intersects;
}
bool SOSAccountHasOustandingViews(SOSAccount* account) {
    bool hasOutstandingViews;
    require_action_quiet([account getCircleStatus:NULL] == kSOSCCInCircle, done, hasOutstandingViews = true);
    CFTypeRef unsyncedObject = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, NULL);
    require_action_quiet(unsyncedObject, done, hasOutstandingViews = false);
    CFBooleanRef unsyncedBool = asBoolean(unsyncedObject, NULL);
    if (unsyncedBool) {
        hasOutstandingViews = CFBooleanGetValue(unsyncedBool);
    } else {
        hasOutstandingViews = isSet(unsyncedBool);
    }
done:
    return hasOutstandingViews;
}
//
// MARK: Initial sync functions
//
static bool SOSAccountHasCompletedInitialySyncWithSetKind(SOSAccount* account, ViewSetKind setKind) {
    CFSetRef viewSet = SOSViewCopyViewSet(setKind);
    bool completedSync = !SOSAccountIntersectsWithOutstanding(account, viewSet);
    CFReleaseNull(viewSet);
    return completedSync;
}
bool SOSAccountHasCompletedInitialSync(SOSAccount* account) {
    return SOSAccountHasCompletedInitialySyncWithSetKind(account, kViewSetInitial);
}
bool SOSAccountHasCompletedRequiredBackupSync(SOSAccount* account) {
    return SOSAccountHasCompletedInitialySyncWithSetKind(account, kViewSetRequiredForBackup);
}


//
// MARK: Handling initial sync being done
//

CFSetRef SOSAccountCopyEnabledViews(SOSAccount* account) {
    if(!(account && account.peerInfo)) {
        return NULL;
    }
    CFSetRef piViews = SOSPeerInfoCopyEnabledViews(account.peerInfo);

    if(SOSAccountHasCompletedInitialSync(account)) {
        return piViews;
    }
    CFSetRef pendingEnabled = asSet(SOSAccountGetValue(account, kSOSPendingEnableViewsToBeSetKey, NULL), NULL);
    CFSetRef pendingDisabled =  asSet(SOSAccountGetValue(account, kSOSPendingDisableViewsToBeSetKey, NULL), NULL);
    
    CFMutableSetRef retvalViews = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, piViews);
    CFSetUnion(retvalViews, pendingEnabled);
    CFSetSubtract(retvalViews, pendingDisabled);
    
    CFReleaseNull(piViews);
    CFReleaseNull(pendingEnabled);
    CFReleaseNull(pendingDisabled);
    return retvalViews;
}


static bool SOSAccountResolvePendingViewSets(SOSAccount* account, CFErrorRef *error) {
    CFMutableSetRef newPending = SOSViewCopyViewSet(kViewSetAlwaysOn);
    CFMutableSetRef defaultOn = SOSViewCopyViewSet(kViewSetDefault);
    CFSetRef pendingOn = asSet(SOSAccountGetValue(account, kSOSPendingEnableViewsToBeSetKey, NULL), NULL);
    CFSetRef pendingDisabled = asSet(SOSAccountGetValue(account, kSOSPendingDisableViewsToBeSetKey, NULL), NULL);

    if(defaultOn) {
        CFSetUnion(newPending, defaultOn);
    }
    if(pendingOn) {
        CFSetUnion(newPending, pendingOn);
    }
    CFReleaseNull(defaultOn);

    bool status = [account.trust updateViewSets:account enabled:newPending disabled:pendingDisabled];
    CFReleaseNull(newPending);

    if(status){
        SOSAccountClearValue(account, kSOSPendingEnableViewsToBeSetKey, NULL);
        SOSAccountClearValue(account, kSOSPendingDisableViewsToBeSetKey, NULL);

        secnotice("views","updated view sets!");
    }
    else{
        secerror("Could not update view sets");
    }
    return status;
}

static void SOSAccountCallInitialSyncBlocks(SOSAccount* account) {
    CFDictionaryRef syncBlocks = NULL;
    syncBlocks = CFBridgingRetain(account.waitForInitialSync_blocks);
    account.waitForInitialSync_blocks = nil;

    if (syncBlocks) {
        CFDictionaryForEach(syncBlocks, ^(const void *key, const void *value) {
            secnotice("updates", "calling in sync block [%@]", key);
            ((__bridge SOSAccountWaitForInitialSyncBlock)value)(account);
        });
    }
    CFReleaseNull(syncBlocks);
}


static void SOSAccountHandleRequiredBackupSyncDone(SOSAccount* account) {
    secnotice("initial-sync", "Handling Required Backup Sync done");
}

static void SOSAccountHandleInitialSyncDone(SOSAccount* account) {
    secnotice("initial-sync", "Handling initial sync done.");

    if(!SOSAccountResolvePendingViewSets(account, NULL))
        secnotice("initial-sync", "Account could not add the pending view sets");

    SOSAccountCallInitialSyncBlocks(account);
}



//
// MARK: Waiting for in-sync
//
static CFStringRef CreateUUIDString() {
    CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
    CFStringRef result = CFUUIDCreateString(kCFAllocatorDefault, uuid);
    CFReleaseNull(uuid);
    return result;
}

CFStringRef SOSAccountCallWhenInSync(SOSAccount* account, SOSAccountWaitForInitialSyncBlock syncBlock) {
    //if we are not initially synced
    CFStringRef id = NULL;
    CFTypeRef unSyncedViews = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, NULL);
    if (unSyncedViews != NULL) {
        id = CreateUUIDString();
        secnotice("initial-sync", "adding sync block [%@] to array!", id);
        SOSAccountWaitForInitialSyncBlock copy = syncBlock;
        if (account.waitForInitialSync_blocks == NULL) {
            account.waitForInitialSync_blocks = [NSMutableDictionary dictionary];
        }
        [account.waitForInitialSync_blocks setObject:copy forKey:(__bridge NSString*)id];
    } else {
        syncBlock(account);
    }

    return id;
}

bool SOSAccountUnregisterCallWhenInSync(SOSAccount* account, CFStringRef id) {
    if (account.waitForInitialSync_blocks == NULL) return false;

    [account.waitForInitialSync_blocks removeObjectForKey: (__bridge NSString*)id];
    return true;
}

static void performWithInitialSyncDescription(CFTypeRef object, void (^action)(CFStringRef description)) {
    CFSetRef setObject = asSet(object, NULL);
    if (setObject) {
        CFStringSetPerformWithDescription(setObject, action);
    } else {
        CFStringRef format = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), object);
        action(format);
        CFReleaseNull(format);
    }
}

static bool CFSetIntersectionWentEmpty(CFSetRef interestingSet, CFSetRef before, CFSetRef after) {
    return ((before != NULL) && !CFSetIntersectionIsEmpty(interestingSet, before)) &&
           ((after == NULL) || CFSetIntersectionIsEmpty(interestingSet, after));
}

static bool SOSViewIntersectionWentEmpty(ViewSetKind kind, CFSetRef before, CFSetRef after) {
    CFSetRef kindSet = SOSViewCopyViewSet(kind);
    bool result = CFSetIntersectionWentEmpty(kindSet, before, after);
    CFReleaseNull(kindSet);
    return result;
}


bool SOSAccountHandleOutOfSyncUpdate(SOSAccount* account, CFSetRef oldOOSViews, CFSetRef newOOSViews) {
    bool actionTaken = false;

    if (SOSViewIntersectionWentEmpty(kViewSetInitial, oldOOSViews, newOOSViews)) {
        SOSAccountHandleInitialSyncDone(account);
        actionTaken = true;
    }

    if (SOSViewIntersectionWentEmpty(kViewSetRequiredForBackup, oldOOSViews, newOOSViews)) {
        SOSAccountHandleRequiredBackupSyncDone(account);
        actionTaken = true;
    }
    return actionTaken;
}

void SOSAccountUpdateOutOfSyncViews(SOSAccountTransaction* aTxn, CFSetRef viewsInSync) {
    SOSAccount* account = aTxn.account;
    SOSCCStatus circleStatus = [account getCircleStatus:NULL];

    bool inOrApplying = (circleStatus == kSOSCCInCircle) || (circleStatus == kSOSCCRequestPending);

    CFTypeRef unsyncedObject = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, NULL);
    __block CFTypeRef newUnsyncedObject = CFRetainSafe(unsyncedObject);

    CFSetRef unsyncedSet = NULL;
    CFMutableSetRef newUnsyncedSet = NULL;

    performWithInitialSyncDescription(viewsInSync, ^(CFStringRef viewsInSyncDescription) {
        secnotice("initial-sync", "Views in sync: %@", viewsInSyncDescription);
    });


    if (!inOrApplying) {
        if (unsyncedObject != NULL) {
            secnotice("initial-sync", "not in circle nor applying: clearing pending");
            CFReleaseNull(newUnsyncedObject);
        }
    } else if (circleStatus == kSOSCCInCircle) {
        if (unsyncedObject == kCFBooleanTrue) {
            unsyncedSet = SOSViewCopyViewSet(kViewSetAll);
            CFAssignRetained(newUnsyncedObject, CFSetCreateCopy(kCFAllocatorDefault, unsyncedSet));

            secnotice("initial-sync", "Pending views setting to all we can expect.");
        } else if (isSet(unsyncedObject)) {
            unsyncedSet = (CFSetRef) CFRetainSafe(unsyncedObject);
        }

        if (unsyncedSet) {
            CFSetRef otherPeersViews = SOSAccountCopyOtherPeersViews(account);

            newUnsyncedSet = CFSetCreateIntersection(kCFAllocatorDefault, unsyncedSet, otherPeersViews);

            if (viewsInSync) {
                CFSetSubtract(newUnsyncedSet, viewsInSync);
            }

            CFRetainAssign(newUnsyncedObject, newUnsyncedSet);
            CFReleaseNull(otherPeersViews);
        }

        performWithInitialSyncDescription(newUnsyncedSet, ^(CFStringRef unsynced) {
            secnotice("initial-sync", "Unsynced: %@", unsynced);
        });
    }


    if (isSet(newUnsyncedObject) && CFSetIsEmpty((CFSetRef) newUnsyncedObject)) {
        secnotice("initial-sync", "Empty set, using NULL instead");
        CFReleaseNull(newUnsyncedObject);
    }

    CFErrorRef localError = NULL;
    if (!SOSAccountSetValue(account, kSOSUnsyncedViewsKey, newUnsyncedObject, &localError)) {
        secnotice("initial-sync", "Failure saving new unsynced value: %@ value: %@", localError, newUnsyncedObject);
    }
    CFReleaseNull(localError);

    CFReleaseNull(newUnsyncedObject);
    CFReleaseNull(newUnsyncedSet);
    CFReleaseNull(unsyncedSet);
}

void SOSAccountPeerGotInSync(SOSAccountTransaction* aTxn, CFStringRef peerID, CFSetRef views) {
    SOSAccount* account = aTxn.account;
    secnotice("initial-sync", "Peer %@ synced views: %@", peerID, views);
    SOSCircleRef circle = NULL;
    SOSAccountTrustClassic* trust = account.trust;
    circle = trust.trustedCircle;
    if (circle && [account isInCircle:NULL] && SOSCircleHasActivePeerWithID(circle, peerID, NULL)) {
        SOSAccountUpdateOutOfSyncViews(aTxn, views);
    }
}

void SOSAccountEnsureSyncChecking(SOSAccount* account) {
    if (!account.isListeningForSync) {
        SOSEngineRef engine = [account.trust getDataSourceEngine:account.factory];

        if (engine) {
            secnotice("initial-sync", "Setting up notifications to monitor in-sync");
            SOSEngineSetSyncCompleteListenerQueue(engine, account.queue);
            SOSEngineSetSyncCompleteListener(engine, ^(CFStringRef peerID, CFSetRef views) {
                [account performTransaction_Locked:^(SOSAccountTransaction * _Nonnull txn) {
                    SOSAccountPeerGotInSync(txn, peerID, views);
                }];
            });
            account.isListeningForSync = true;
        } else {
            secerror("Couldn't find engine to setup notifications!!!");
        }
    }
}

void SOSAccountCancelSyncChecking(SOSAccount* account) {
    if (account.isListeningForSync) {
        SOSEngineRef engine = [account.trust getDataSourceEngine:account.factory];

        if (engine) {
            secnotice("initial-sync", "Cancelling notifications to monitor in-sync");
            SOSEngineSetSyncCompleteListenerQueue(engine, NULL);
            SOSEngineSetSyncCompleteListener(engine, NULL);
        } else {
            secnotice("initial-sync", "No engine to cancel notification from.");
        }
        account.isListeningForSync = false;
    }
}

bool SOSAccountCheckForAlwaysOnViews(SOSAccount* account) {
    bool changed = false;
    SOSPeerInfoRef myPI = account.peerInfo;
    require_quiet(myPI, done);
    require_quiet([account isInCircle:NULL], done);
    require_quiet(SOSAccountHasCompletedInitialSync(account), done);
    CFMutableSetRef viewsToEnsure = SOSViewCopyViewSet(kViewSetAlwaysOn);
    // Previous version PeerInfo if we were syncing legacy keychain, ensure we include those legacy views.
    if(!SOSPeerInfoVersionIsCurrent(myPI)) {
        CFSetRef V0toAdd = SOSViewCopyViewSet(kViewSetV0);
        CFSetUnion(viewsToEnsure, V0toAdd);
        CFReleaseNull(V0toAdd);
    }
    changed = [account.trust updateFullPeerInfo:account minimum:viewsToEnsure excluded:SOSViewsGetV0ViewSet()]; // We don't permit V0 view proper, only sub-views
    CFReleaseNull(viewsToEnsure);
done:
    return changed;
}

