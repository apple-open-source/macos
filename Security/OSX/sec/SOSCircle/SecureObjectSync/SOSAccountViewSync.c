//
//  SOSAccountViews.c
//  sec
//
//  Created by Mitch Adler on 6/10/16.
//
//


#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecureObjectSync/SOSAccount.h>
#include "SOSViews.h"
#include "SOSAccountPriv.h"

#include <utilities/SecCFWrappers.h>

//
// MARK: Helpers
//

static CFMutableSetRef SOSAccountCopyOtherPeersViews(SOSAccountRef account) {
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

CFMutableSetRef SOSAccountCopyOutstandingViews(SOSAccountRef account) {
    CFSetRef initialSyncViews = SOSViewCopyViewSet(kViewSetAll);
    CFMutableSetRef result = SOSAccountCopyIntersectionWithOustanding(account, initialSyncViews);
    CFReleaseNull(initialSyncViews);
    return result;
}


bool SOSAccountIsViewOutstanding(SOSAccountRef account, CFStringRef view) {
    bool isOutstandingView;

    require_action_quiet(SOSAccountIsInCircle(account, NULL), done, isOutstandingView = true);

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

CFMutableSetRef SOSAccountCopyIntersectionWithOustanding(SOSAccountRef account, CFSetRef inSet) {
    CFTypeRef unsyncedObject = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, NULL);
    CFMutableSetRef result = NULL;

    require_quiet(SOSAccountIsInCircle(account, NULL), done);

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

bool SOSAccountIntersectsWithOutstanding(SOSAccountRef account, CFSetRef views) {
    CFSetRef nonInitiallySyncedViews = SOSAccountCopyIntersectionWithOustanding(account, views);
    bool intersects = !CFSetIsEmpty(nonInitiallySyncedViews);
    CFReleaseNull(nonInitiallySyncedViews);
    return intersects;
}

bool SOSAccountHasOustandingViews(SOSAccountRef account) {
    bool hasOutstandingViews;

    require_action_quiet(SOSAccountIsInCircle(account, NULL), done, hasOutstandingViews = true);

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

static bool SOSAccountHasCompletedInitialySyncWithSetKind(SOSAccountRef account, ViewSetKind setKind) {
    CFSetRef viewSet = SOSViewCopyViewSet(setKind);
    bool completedSync = !SOSAccountIntersectsWithOutstanding(account, viewSet);
    CFReleaseNull(viewSet);

    return completedSync;
}

bool SOSAccountHasCompletedInitialSync(SOSAccountRef account) {
    return SOSAccountHasCompletedInitialySyncWithSetKind(account, kViewSetInitial);
}

bool SOSAccountHasCompletedRequiredBackupSync(SOSAccountRef account) {
    return SOSAccountHasCompletedInitialySyncWithSetKind(account, kViewSetRequiredForBackup);
}




//
// MARK: Handling initial sync being done
//

static bool SOSAccountResolvePendingViewSets(SOSAccountRef account, CFErrorRef *error) {
    bool status = SOSAccountUpdateViewSets(account,
                                           asSet(SOSAccountGetValue(account, kSOSPendingEnableViewsToBeSetKey, NULL), NULL),
                                           asSet(SOSAccountGetValue(account, kSOSPendingDisableViewsToBeSetKey, NULL), NULL));
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

static void SOSAccountCallInitialSyncBlocks(SOSAccountRef account) {
    CFDictionaryRef syncBlocks = NULL;
    CFTransferRetained(syncBlocks, account->waitForInitialSync_blocks);

    if (syncBlocks) {
        CFDictionaryForEach(syncBlocks, ^(const void *key, const void *value) {
            secnotice("updates", "calling in sync block [%@]", key);
            ((SOSAccountWaitForInitialSyncBlock)value)(account);
        });
    }
    CFReleaseNull(syncBlocks);
}


static void SOSAccountHandleRequiredBackupSyncDone(SOSAccountRef account) {
    secnotice("initial-sync", "Handling Required Backup Sync done");
}

static void SOSAccountHandleInitialSyncDone(SOSAccountRef account) {
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

CFStringRef SOSAccountCallWhenInSync(SOSAccountRef account, SOSAccountWaitForInitialSyncBlock syncBlock) {
    //if we are not initially synced
    CFStringRef id = NULL;
    CFTypeRef unSyncedViews = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, NULL);
    if (unSyncedViews != NULL) {
        id = CreateUUIDString();
        secnotice("initial-sync", "adding sync block [%@] to array!", id);
        SOSAccountWaitForInitialSyncBlock copy = Block_copy(syncBlock);
        if (account->waitForInitialSync_blocks == NULL) {
            account->waitForInitialSync_blocks = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        }
        CFDictionarySetValue(account->waitForInitialSync_blocks, id, copy);
        Block_release(copy);
    } else {
        syncBlock(account);
    }

    return id;
}

bool SOSAccountUnregisterCallWhenInSync(SOSAccountRef account, CFStringRef id) {
    if (account->waitForInitialSync_blocks == NULL) return false;

    bool removed = CFDictionaryGetValueIfPresent(account->waitForInitialSync_blocks, id, NULL);
    CFDictionaryRemoveValue(account->waitForInitialSync_blocks, id);
    return removed;
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

bool SOSAccountHandleOutOfSyncUpdate(SOSAccountRef account, CFSetRef oldOOSViews, CFSetRef newOOSViews) {
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

void SOSAccountUpdateOutOfSyncViews(SOSAccountTransactionRef aTxn, CFSetRef viewsInSync) {
    SOSAccountRef account = aTxn->account;
    SOSCCStatus circleStatus = SOSAccountGetCircleStatus(account, NULL);
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

void SOSAccountPeerGotInSync(SOSAccountTransactionRef aTxn, CFStringRef peerID, CFSetRef views) {
    SOSAccountRef account = aTxn->account;
    secnotice("initial-sync", "Peer %@ synced views: %@", peerID, views);
    if (account->trusted_circle && SOSAccountIsInCircle(account, NULL) && SOSCircleHasActivePeerWithID(account->trusted_circle, peerID, NULL)) {
        SOSAccountUpdateOutOfSyncViews(aTxn, views);
    }
}

static SOSEngineRef SOSAccountGetDataSourceEngine(SOSAccountRef account) {
    return SOSDataSourceFactoryGetEngineForDataSourceName(account->factory, SOSCircleGetName(account->trusted_circle), NULL);
}

void SOSAccountEnsureSyncChecking(SOSAccountRef account) {
    if (!account->isListeningForSync) {
        SOSEngineRef engine = SOSAccountGetDataSourceEngine(account);

        if (engine) {
            secnotice("initial-sync", "Setting up notifications to monitor in-sync");
            SOSEngineSetSyncCompleteListenerQueue(engine, account->queue);
            SOSEngineSetSyncCompleteListener(engine, ^(CFStringRef peerID, CFSetRef views) {
                SOSAccountWithTransaction_Locked(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
                    SOSAccountPeerGotInSync(txn, peerID, views);
                });
            });
            account->isListeningForSync = true;
        } else {
            secerror("Couldn't find engine to setup notifications!!!");
        }
    }
}

void SOSAccountCancelSyncChecking(SOSAccountRef account) {
    if (account->isListeningForSync) {
        SOSEngineRef engine = SOSAccountGetDataSourceEngine(account);

        if (engine) {
            secnotice("initial-sync", "Cancelling notifications to monitor in-sync");
            SOSEngineSetSyncCompleteListenerQueue(engine, NULL);
            SOSEngineSetSyncCompleteListener(engine, NULL);
        } else {
            secnotice("initial-sync", "No engine to cancel notification from.");
        }
        account->isListeningForSync = false;
    }
}

bool SOSAccountCheckForAlwaysOnViews(SOSAccountRef account) {
    bool changed = false;
    SOSPeerInfoRef myPI = SOSAccountGetMyPeerInfo(account);
    require_quiet(myPI, done);
    require_quiet(SOSAccountIsInCircle(account, NULL), done);
    require_quiet(SOSAccountHasCompletedInitialSync(account), done);
    CFMutableSetRef viewsToEnsure = SOSViewCopyViewSet(kViewSetAlwaysOn);
    // Previous version PeerInfo if we were syncing legacy keychain, ensure we include those legacy views.
    if(!SOSPeerInfoVersionIsCurrent(myPI)) {
        CFSetRef V0toAdd = SOSViewCopyViewSet(kViewSetV0);
        CFSetUnion(viewsToEnsure, V0toAdd);
        CFReleaseNull(V0toAdd);
    }
    changed = SOSAccountUpdateFullPeerInfo(account, viewsToEnsure, SOSViewsGetV0ViewSet()); // We don't permit V0 view proper, only sub-views
    CFReleaseNull(viewsToEnsure);
done:
    return changed;
}

