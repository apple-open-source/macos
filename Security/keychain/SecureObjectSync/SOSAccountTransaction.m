//
//  SOSAccountTransaction.c
//  sec
//
//

#include "SOSAccountTransaction.h"

#include <utilities/SecCFWrappers.h>
#import <utilities/SecNSAdditions.h>
#include <CoreFoundation/CoreFoundation.h>

#include "keychain/SecureObjectSync/SOSAccount.h"
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#import "keychain/SecureObjectSync/SOSTransport.h"
#import "keychain/SecureObjectSync/SOSTransportCircle.h"
#import "keychain/SecureObjectSync/SOSTransportCircleKVS.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"
#import "keychain/SecureObjectSync/SOSTransportMessageKVS.h"
#import "Security/SecItemBackup.h"

#include <keychain/ckks/CKKS.h>

#define kPublicKeyNotAvailable "com.apple.security.publickeynotavailable"

// Account dumping state stuff

#define ACCOUNT_STATE_INTERVAL 200


@interface SOSAccountTransaction ()

@property BOOL              initialInCircle;
@property NSSet<NSString*>* initialViews;
@property NSSet<NSString*>* initialUnsyncedViews;
@property NSString*         initialID;

@property BOOL              initialTrusted;
@property NSData*           initialKeyParameters;

@property uint              initialCirclePeerCount;

@property bool              quiet;

@property NSMutableSet<NSString*>* peersToRequestSync;

- (void) start;

@end



@implementation SOSAccountTransaction

- (uint64_t)currentTrustBitmask {
    dispatch_assert_queue(_account.queue);

    // A couple of assumptions here, that circle status is 32 bit (it an int), and that we can store
    // that in th lower half without sign extending it
    uint64_t circleStatus = (((uint32_t)[_account.trust getCircleStatusOnly:NULL]) & 0xffffffff) | CC_STATISVALID;
    if(_account.accountKeyIsTrusted) {
        circleStatus |= CC_UKEY_TRUSTED;
        if(_account.accountPrivateKey) {
            circleStatus |= CC_CAN_AUTH;
        }
    }
    // we might be in circle, but invalid - let client see this in bitmask.
    if([_account.trust isInCircleOnly:NULL]) {
       circleStatus |= CC_PEER_IS_IN;
    }
    return circleStatus;
}

static inline SOSCCStatus reportAsSOSCCStatus(uint64_t x) {
    return (SOSCCStatus) x & CC_MASK;
}

- (void) updateSOSCircleCachedStatus {
    // clearly invalid, overwritten by cached value in notify_get_state()
    // if its the second time we are launchded
    static uint64_t lastStatus = 0;

    dispatch_assert_queue(_account.queue);

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // pull out what previous instance of securityd though the trust status
        SOSCachedNotificationOperation(kSOSCCCircleChangedNotification, ^bool(int token, bool gtg) {
            uint64_t state64;
            if (notify_get_state(token, &state64) == NOTIFY_STATUS_OK) {
                if (state64 & CC_STATISVALID) {
                    lastStatus = state64;
                }
            }
            return true;
        });
        secnotice("sosnotify", "initial last circle status is: %d", reportAsSOSCCStatus(lastStatus));
    });

    uint64_t currentStatus = [self currentTrustBitmask];
    if (lastStatus & CC_STATISVALID) {
        if(lastStatus != currentStatus) {
            _account.notifyCircleChangeOnExit = true;
        }
    } else {
        _account.notifyCircleChangeOnExit = true;
    }

    if(_account.notifyCircleChangeOnExit) {
        // notify if last cache status was invalid, or it have changed, clients don't get update on changes in
        // the metadata stored in the upper bits of above CC_MASK (at least not though this path)
        bool firstLaunch = (lastStatus & CC_STATISVALID) == 0;
        bool circleChanged = ((lastStatus & CC_MASK) != (currentStatus & CC_MASK));

        bool circleStatusChanged = firstLaunch || circleChanged;

        lastStatus = currentStatus;

        secnotice("sosnotify", "new last circle status is: %d (notify: %s)",
                  reportAsSOSCCStatus(lastStatus),
                  circleStatusChanged ? "yes" : "no");

        SOSCachedNotificationOperation(kSOSCCCircleChangedNotification, ^bool(int token, bool gtg) {
            if(gtg) {
                uint32_t status = notify_set_state(token, currentStatus);
                if(status == NOTIFY_STATUS_OK) {
                    self->_account.notifyCircleChangeOnExit = false;

                    if (circleStatusChanged) {
                        secnotice("sosnotify", "posting kSOSCCCircleChangedNotification");
                        notify_post(kSOSCCCircleChangedNotification);
                    }
                }
                return true;
            }
            return false;
        });

        // preserve behavior from previous, this should be merged into SOSViewsSetCachedStatus()
        if (firstLaunch) {
            _account.notifyViewChangeOnExit = true;
        }
    }
}

static void SOSViewsSetCachedStatus(SOSAccount *account) {
    static uint64_t lastViewBitmask = 0;
    CFSetRef piViews = SOSAccountCopyEnabledViews(account);
    
    __block uint64_t viewBitMask = (([account getCircleStatus:NULL] == kSOSCCInCircle) && piViews) ? SOSViewBitmaskFromSet(piViews) :0;
    CFReleaseNull(piViews);

    if(viewBitMask != lastViewBitmask) {
        lastViewBitmask = viewBitMask;
        account.notifyViewChangeOnExit = true; // this is also set within operations and might want the notification for other reasons.
    }

    if(account.notifyViewChangeOnExit) {
        SOSCachedNotificationOperation(kSOSCCViewMembershipChangedNotification, ^bool(int token, bool gtg) {
            if(gtg) {
                uint32_t status = notify_set_state(token, viewBitMask);
                if(status == NOTIFY_STATUS_OK) {
                    notify_post(kSOSCCViewMembershipChangedNotification);
                    account.notifyViewChangeOnExit = false;
                }
                return true;
            }
            return false;
        });
    }
}

- (NSString*) description {
    return [NSString stringWithFormat:@"<SOSAccountTransaction*@%p %ld>",
                                      self, (unsigned long)(self.initialViews ? [self.initialViews count] : 0)];
}

- (instancetype) initWithAccount:(SOSAccount *)account quiet:(bool)quiet {
    if (self = [super init]) {
        self.account = account;
        _quiet = quiet;
        [self start];
    }
    return self;
}

- (void) start {
    [self updateSOSCircleCachedStatus];
    SOSViewsSetCachedStatus(_account);

    self.initialInCircle = [self.account isInCircle:NULL];
    self.initialTrusted = self.account.accountKeyIsTrusted;
    self.initialCirclePeerCount = 0;
    if(self.initialInCircle) {
        self.initialCirclePeerCount = SOSCircleCountPeers(self.account.trust.trustedCircle);
    }

    if (self.initialInCircle) {
        SOSAccountEnsureSyncChecking(self.account);
    }

    self.initialUnsyncedViews = (__bridge_transfer NSMutableSet<NSString*>*)SOSAccountCopyOutstandingViews(self.account);
    self.initialKeyParameters = self.account.accountKeyDerivationParameters ? [NSData dataWithData:self.account.accountKeyDerivationParameters] : nil;

    SOSPeerInfoRef mpi = self.account.peerInfo;
    if (mpi) {
        self.initialViews = CFBridgingRelease(SOSPeerInfoCopyEnabledViews(mpi));
        [self.account ensureOctagonPeerKeys];
    }
    self.peersToRequestSync = nil;
    
    SOSAccountEnsurePeerInfoHasCurrentBackupKey(self, NULL);

    if(self.account.key_interests_need_updating) {
        SOSUpdateKeyInterest(self.account);
    }

    if(!self.quiet) {
        CFStringSetPerformWithDescription((__bridge CFSetRef) self.initialViews, ^(CFStringRef description) {
            secnotice("acct-txn", "Starting as:%s v:%@", self.initialInCircle ? "member" : "non-member", description);
        });
    }
}

- (void) restart {
    [self finish];
    [self start];
}


- (void) finish {
    static int do_account_state_at_zero = 0;
    bool doCircleChanged = self.account.notifyCircleChangeOnExit;
    bool doViewChanged = false;


    CFErrorRef localError = NULL;
    bool notifyEngines = false;

    SOSPeerInfoRef mpi = self.account.peerInfo;

    bool isInCircle = [self.account isInCircle:NULL];

    if (isInCircle && self.peersToRequestSync) {
        NSMutableSet<NSString*>* worklist = self.peersToRequestSync;
        self.peersToRequestSync = nil;
        SOSCCRequestSyncWithPeers((__bridge CFSetRef)(worklist));
    }
    
    SOSAccountEnsurePeerInfoHasCurrentBackupKey(self, NULL);

    if (isInCircle) {
        SOSAccountEnsureSyncChecking(self.account);
    } else {
        SOSAccountCancelSyncChecking(self.account);
    }

    // If our identity changed our inital set should be everything.
    if ([self.initialID isEqualToString: (__bridge NSString *)(SOSPeerInfoGetPeerID(mpi))]) {
        self.initialUnsyncedViews = (__bridge_transfer NSSet<NSString*>*) SOSViewCopyViewSet(kViewSetAll);
    }

    NSSet<NSString*>* finalUnsyncedViews = (__bridge_transfer NSSet<NSString*>*) SOSAccountCopyOutstandingViews(self.account);
    if (!NSIsEqualSafe(self.initialUnsyncedViews, finalUnsyncedViews)) {
        if (SOSAccountHandleOutOfSyncUpdate(self.account,
                                            (__bridge CFSetRef)(self.initialUnsyncedViews),
                                            (__bridge CFSetRef)(finalUnsyncedViews))) {
            notifyEngines = true;
        }

        secnotice("initial-sync", "Unsynced was: %@", [self.initialUnsyncedViews shortDescription]);
        secnotice("initial-sync", "Unsynced is: %@", [finalUnsyncedViews shortDescription]);
    }

    if (self.account.engine_peer_state_needs_repair) {
        // We currently only get here from a failed syncwithallpeers, so
        // that will retry. If this logic changes, force a syncwithallpeers
        if (!SOSAccountEnsurePeerRegistration(self.account, &localError)) {
            secerror("Ensure peer registration while repairing failed: %@", localError);
        }
        CFReleaseNull(localError);

        notifyEngines = true;
    }
   
    if(self.account.circle_rings_retirements_need_attention){
        self.account.circle_rings_retirements_need_attention = false;
#if OCTAGON
        [self.account triggerRingUpdate];
#endif
        // Also, there might be some view set change. Ensure that the engine knows...
        notifyEngines = true;
    }
    if(self.account.need_backup_peers_created_after_backup_key_set){
        self.account.need_backup_peers_created_after_backup_key_set = false;
        notifyEngines = true;
    }

    // Always notify the engines if the view set has changed
    CFSetRef currentViews = self.account.peerInfo ? SOSPeerInfoCopyEnabledViews(self.account.peerInfo) : NULL;
    BOOL viewSetChanged = !NSIsEqualSafe(self.initialViews, (__bridge NSSet*)currentViews);
    CFReleaseNull(currentViews);
    notifyEngines |= viewSetChanged;

    if (notifyEngines) {
#if OCTAGON
        if(!SecCKKSTestDisableSOS()) {
#endif
            SOSAccountNotifyEngines(self.account);
#if OCTAGON
        }
#endif
    }

    if(self.account.key_interests_need_updating) {
        SOSUpdateKeyInterest(self.account);
    }

    self.account.engine_peer_state_needs_repair = false;

    [self.account flattenToSaveBlock];

    // Refresh isInCircle since we could have changed our mind
    isInCircle = [self.account isInCircle:NULL];

    uint finalCirclePeerCount = 0;
    if(isInCircle) {
        finalCirclePeerCount = SOSCircleCountPeers(self.account.trust.trustedCircle);
    }

    if(isInCircle && (finalCirclePeerCount < self.initialCirclePeerCount)) {
        (void) SOSAccountCleanupAllKVSKeys(_account, NULL);
    }

    mpi = self.account.peerInfo;
    CFSetRef views = mpi ? SOSPeerInfoCopyEnabledViews(mpi) : NULL;

    if(!self.quiet) {
        CFStringSetPerformWithDescription(views, ^(CFStringRef description) {
            secnotice("acct-txn", "Finished as:%s v:%@", isInCircle ? "member" : "non-member", description);
        });
    }

    // This is the logic to detect a new userKey:
    bool userKeyChanged = !NSIsEqualSafe(self.initialKeyParameters, self.account.accountKeyDerivationParameters);
    
    // This indicates we initiated a password change.
    bool weInitiatedKeyChange = (self.initialTrusted &&
                                 self.initialInCircle &&
                                 userKeyChanged && isInCircle &&
                                 self.account.accountKeyIsTrusted);
    
    if(self.initialInCircle != isInCircle) {
        doCircleChanged = true;
        doViewChanged = true;
        do_account_state_at_zero = 0;
        secnotice("secdNotify", "Notified clients of kSOSCCCircleChangedNotification && kSOSCCViewMembershipChangedNotification for circle/view change");
    } else if(isInCircle && !NSIsEqualSafe(self.initialViews, (__bridge NSSet*)views)) {
        doViewChanged = true;
        do_account_state_at_zero = 0;
        secnotice("secdNotify", "Notified clients of kSOSCCViewMembershipChangedNotification for viewchange(only)");
    } else if(weInitiatedKeyChange) { // We consider this a circleChange so (PCS) can tell the userkey trust changed.
        doCircleChanged = true;
        do_account_state_at_zero = 0;
        secnotice("secdNotify", "Notified clients of kSOSCCCircleChangedNotification for userKey change");
    }
    


    // This is the case of we used to trust the key, were in the circle, the key changed, we don't trust it now.
    bool fellOutOfTrust = (self.initialTrusted &&
                           self.initialInCircle &&
                           userKeyChanged &&
                           !self.account.accountKeyIsTrusted);
    
    if(fellOutOfTrust) {
        secnotice("userKeyTrust", "No longer trust user public key - prompting for password.");
        notify_post(kPublicKeyNotAvailable);
        doCircleChanged = true;
        do_account_state_at_zero = 0;
    }
    
    bool userKeyTrustChangedToTrueAndNowInCircle = (!self.initialTrusted && self.account.accountKeyIsTrusted && isInCircle);
    
    if(userKeyTrustChangedToTrueAndNowInCircle) {
        secnotice("userKeyTrust", "UserKey is once again trusted and we're valid in circle.");
        doCircleChanged = true;
        doViewChanged = true;
    }
    
    if(doCircleChanged) {
        [self updateSOSCircleCachedStatus];
    }
    if(doViewChanged) {
        SOSViewsSetCachedStatus(_account);
    }
    if(self.account.notifyBackupOnExit) {
        notify_post(kSecItemBackupNotification);
        self.account.notifyBackupOnExit = false;
    }


    if(do_account_state_at_zero <= 0) {
        SOSAccountLogState(self.account);
        SOSAccountLogViewState(self.account);
        do_account_state_at_zero = ACCOUNT_STATE_INTERVAL;
    }
    do_account_state_at_zero--;
    
    CFReleaseNull(views);
}

- (void) requestSyncWith: (NSString*) peerID {
    if (self.peersToRequestSync == nil) {
        self.peersToRequestSync = [NSMutableSet<NSString*> set];
    }
    [self.peersToRequestSync addObject: peerID];
}

- (void) requestSyncWithPeers: (NSSet<NSString*>*) peerList {
    if (self.peersToRequestSync == nil) {
        self.peersToRequestSync = [NSMutableSet<NSString*> set];
    }
    [self.peersToRequestSync unionSet: peerList];
}

@end




//
// MARK: Transactional
//

@implementation SOSAccount (Transaction)

__thread bool __hasAccountQueue = false;

+ (void)performOnQuietAccountQueue:(void (^)(void))action
{
    SOSAccount* account = (__bridge SOSAccount*)GetSharedAccountRef();
    if(account) {
        [account performTransaction:true action:^(SOSAccountTransaction * _Nonnull txn) {
            action();
        }];
    } else {
        secnotice("acct-txn", "No account; running block on local thread");
        action();
    }
}

- (void) performTransaction_Locked: (void (^)(SOSAccountTransaction* txn)) action {
    [self performTransaction_Locked:false action:action];
}

- (void) performTransaction_Locked:(bool)quiet action:(void (^)(SOSAccountTransaction* txn))action {
    @autoreleasepool {
        SOSAccountTransaction* transaction = [[SOSAccountTransaction alloc] initWithAccount:self quiet:quiet];
        action(transaction);
        [transaction finish];
    }
}

- (void) performTransaction: (void (^)(SOSAccountTransaction* txn)) action {
    [self performTransaction:false action:action];
}

- (void)performTransaction:(bool)quiet action:(void (^)(SOSAccountTransaction* txn))action {

    if (__hasAccountQueue) {
        // Be quiet; we're already in a transaction
        [self performTransaction_Locked:true action:action];
    }
    else {
        dispatch_sync(self.queue, ^{
            __hasAccountQueue = true;
            [self performTransaction_Locked:quiet action:action];
            __hasAccountQueue = false;
        });
    }
}


@end
