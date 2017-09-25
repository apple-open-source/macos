//
//  SOSAccountTransaction.c
//  sec
//
//

#include "SOSAccountTransaction.h"

#include <utilities/SecCFWrappers.h>
#import <utilities/SecNSAdditions.h>
#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#import <Security/SecureObjectSync/SOSTransport.h>
#import <Security/SecureObjectSync/SOSTransportCircle.h>
#import <Security/SecureObjectSync/SOSTransportCircleKVS.h>
#import "Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#import "Security/SecureObjectSync/SOSAccountTrustClassic.h"
#import "Security/SecureObjectSync/SOSTransportMessageKVS.h"

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

@property NSMutableSet<NSString*>* peersToRequestSync;

- (void) start;

@end



@implementation SOSAccountTransaction

+ (instancetype) transactionWithAccount: (SOSAccount*) account {
    return [[SOSAccountTransaction new] initWithAccount: account];
}

- (NSString*) description {
    return [NSString stringWithFormat:@"<SOSAccountTransaction*@%p %ld>",
                                      self, (unsigned long)(self.initialViews ? [self.initialViews count] : 0)];
}

- (instancetype) initWithAccount:(SOSAccount *)account {
    if (self = [super init]) {
        self.account = account;
        [self start];
    }
    return self;
}

- (void) start {
    self.initialInCircle = [self.account.trust isInCircle:NULL];
    self.initialTrusted = self.account.accountKeyIsTrusted;

    if (self.initialInCircle) {
        SOSAccountEnsureSyncChecking(self.account);
    }

    self.initialUnsyncedViews = (__bridge_transfer NSMutableSet<NSString*>*)SOSAccountCopyOutstandingViews(self.account);
    self.initialKeyParameters = self.account.accountKeyDerivationParamters ? [NSData dataWithData:self.account.accountKeyDerivationParamters] : nil;

    SOSPeerInfoRef mpi = self.account.peerInfo;
    self.initialViews = mpi ? (__bridge_transfer NSSet*) SOSPeerInfoCopyEnabledViews(mpi) : nil;

    self.peersToRequestSync = nil;

    CFStringSetPerformWithDescription((__bridge CFSetRef) self.initialViews, ^(CFStringRef description) {
        secnotice("acct-txn", "Starting as:%s v:%@", self.initialInCircle ? "member" : "non-member", description);
    });
}

- (void) restart {
    [self finish];
    [self start];
}


- (void) finish {
    static int do_account_state_at_zero = 0;

    CFErrorRef localError = NULL;
    bool notifyEngines = false;

    SOSPeerInfoRef mpi = self.account.peerInfo;

    bool isInCircle = [self.account.trust isInCircle:NULL];

    if (isInCircle && self.peersToRequestSync) {
        SOSCCRequestSyncWithPeers((__bridge CFSetRef)(self.peersToRequestSync));
    }
    self.peersToRequestSync = nil;

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
        SOSAccountRecordRetiredPeersInCircle(self.account);

        SOSAccountEnsureRecoveryRing(self.account);
        SOSAccountEnsureInBackupRings(self.account);

        CFErrorRef localError = NULL;
        if(![self.account.circle_transport flushChanges:&localError]){
            secerror("flush circle failed %@", localError);
        }
        CFReleaseSafe(localError);

        notifyEngines = true;
    }

    if (notifyEngines) {
#if OCTAGON
        if(!SecCKKSTestDisableSOS()) {
#endif
            SOSAccountNotifyEngines(self.account);
#if OCTAGON
        }
#endif
    }

    if(self.account.key_interests_need_updating){
        SOSUpdateKeyInterest(self.account);
    }

    self.account.key_interests_need_updating = false;
    self.account.circle_rings_retirements_need_attention = false;
    self.account.engine_peer_state_needs_repair = false;

    [self.account flattenToSaveBlock];

    // Refresh isInCircle since we could have changed our mind
    isInCircle = [self.account.trust isInCircle:NULL];

    mpi = self.account.peerInfo;
    CFSetRef views = mpi ? SOSPeerInfoCopyEnabledViews(mpi) : NULL;

    CFStringSetPerformWithDescription(views, ^(CFStringRef description) {
        secnotice("acct-txn", "Finished as:%s v:%@", isInCircle ? "member" : "non-member", description);
    });

    // This is the logic to detect a new userKey:
    bool userKeyChanged = !NSIsEqualSafe(self.initialKeyParameters, self.account.accountKeyDerivationParamters);
    
    // This indicates we initiated a password change.
    bool weInitiatedKeyChange = (self.initialTrusted &&
                                 self.initialInCircle &&
                                 userKeyChanged && isInCircle &&
                                 self.account.accountKeyIsTrusted);
    
    if(self.initialInCircle != isInCircle) {
        notify_post(kSOSCCCircleChangedNotification);
        notify_post(kSOSCCViewMembershipChangedNotification);
        do_account_state_at_zero = 0;
        secnotice("secdNotify", "Notified clients of kSOSCCCircleChangedNotification && kSOSCCViewMembershipChangedNotification for circle/view change");
    } else if(isInCircle && !NSIsEqualSafe(self.initialViews, (__bridge NSSet*)views)) {
        notify_post(kSOSCCViewMembershipChangedNotification);
        do_account_state_at_zero = 0;
        secnotice("secdNotify", "Notified clients of kSOSCCViewMembershipChangedNotification for viewchange(only)");
    } else if(weInitiatedKeyChange) { // We consider this a circleChange so (PCS) can tell the userkey trust changed.
        notify_post(kSOSCCCircleChangedNotification);
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
        do_account_state_at_zero = 0;
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

+ (void)performWhileHoldingAccountQueue:(void (^)(void))action
{
    bool hadAccountQueue = __hasAccountQueue;
    __hasAccountQueue = true;
    action();
    __hasAccountQueue = hadAccountQueue;
}

+ (void)performOnAccountQueue:(void (^)(void))action
{
    SOSAccount* account = (__bridge SOSAccount*)GetSharedAccountRef();
    [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        action();
    }];
}

- (void) performTransaction_Locked: (void (^)(SOSAccountTransaction* txn)) action {
    SOSAccountTransaction* transaction = [SOSAccountTransaction transactionWithAccount:self];
    action(transaction);
    [transaction finish];
}

- (void) performTransaction: (void (^)(SOSAccountTransaction* txn)) action {
    if (__hasAccountQueue) {
        [self performTransaction_Locked:action];
    }
    else {
        dispatch_sync(self.queue, ^{
            __hasAccountQueue = true;
            [self performTransaction_Locked:action];
            __hasAccountQueue = false;
        });
    }
}


@end
