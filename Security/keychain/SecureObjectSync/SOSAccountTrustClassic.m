//
//  SOSAccountTrustClassic.m
//  Security
//

#import <Foundation/Foundation.h>
#import "keychain/SecureObjectSync/SOSAccount.h"
#import "keychain/SecureObjectSync/SOSViews.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Identity.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Retirement.h"

#import "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#import "keychain/SecureObjectSync/SOSPeerInfoCollections.h"
#import "keychain/SecureObjectSync/SOSTransportMessageKVS.h"

#import "keychain/SecureObjectSync/SOSAccountTransaction.h"
#include "keychain/SecureObjectSync/SOSTransportCircleKVS.h"
#include "keychain/SecureObjectSync/SOSTransportCircle.h"
#include "keychain/SecureObjectSync/SOSCircleDer.h"
#include "keychain/SecureObjectSync/SOSInternal.h"

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCoreCrypto.h>
#include <utilities/SecBuffer.h>

@implementation SOSAccountTrustClassic
extern CFStringRef kSOSAccountDebugScope;

+(instancetype)trustClassic
{
    return [[self alloc] init];
}

-(id)init
{
    if ((self = [super init])) {
        self.retirees = [NSMutableSet set];
        self.fullPeerInfo = NULL;
        self.trustedCircle = NULL;
        self.departureCode = kSOSDepartureReasonError;
        self.expansion = [NSMutableDictionary dictionary];
        [self addRingDictionary];
    }
    return self;
}

-(id)initWithRetirees:(NSMutableSet*)r fpi:(SOSFullPeerInfoRef)fpi circle:(SOSCircleRef) trusted_circle
        departureCode:(enum DepartureReason)code peerExpansion:(NSMutableDictionary*)e
{
    if ((self = [super init])) {
        self.retirees = [[NSMutableSet alloc] initWithSet:r] ;
        self.fullPeerInfo = CFRetainSafe(fpi);
        self.trustedCircle = CFRetainSafe(trusted_circle);
        self.departureCode = code;
        self.expansion = [[NSMutableDictionary alloc]initWithDictionary:e];

        [self addRingDictionary];
    }
    return self;


}

-(bool) updateGestalt:(SOSAccount*)account newGestalt:(CFDictionaryRef)new_gestalt
{
    if (CFEqualSafe(new_gestalt, (__bridge CFDictionaryRef)(account.gestalt)))
        return false;
    
    if (self.trustedCircle && self.fullPeerInfo
        && SOSFullPeerInfoUpdateGestalt(self.fullPeerInfo, new_gestalt, NULL)) {
        [self modifyCircle:account.circle_transport err:NULL action:^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for gestalt change");
            return SOSCircleUpdatePeerInfo(circle_to_change, self.peerInfo);
        }];
    }
    
    account.gestalt = [[NSDictionary alloc] initWithDictionary:(__bridge NSDictionary * _Nonnull)(new_gestalt)];
    return true;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
-(SOSViewResultCode) updateView:(SOSAccount*)account name:(CFStringRef) viewname code:(SOSViewActionCode) actionCode err:(CFErrorRef *)error
{
    SOSViewResultCode retval = kSOSCCGeneralViewError;
    SOSViewResultCode currentStatus = kSOSCCGeneralViewError;
    bool alreadyInSync = SOSAccountHasCompletedInitialSync(account);
    bool updateCircle = false;
    CFSetRef alwaysOn = SOSViewCopyViewSet(kViewSetAlwaysOn);
    
    require_action_quiet(self.trustedCircle, errOut, SOSCreateError(kSOSErrorNoCircle, CFSTR("No Trusted Circle"), NULL, error));
    require_action_quiet(self.fullPeerInfo, errOut, SOSCreateError(kSOSErrorPeerNotFound, CFSTR("No Peer for Account"), NULL, error));
    require_action_quiet((actionCode == kSOSCCViewEnable) || (actionCode == kSOSCCViewDisable), errOut, CFSTR("Invalid View Action"));
    currentStatus = [account.trust viewStatus:account name:viewname err:error];
    require_action_quiet((currentStatus == kSOSCCViewNotMember) || (currentStatus == kSOSCCViewMember), errOut, CFSTR("View Membership Not Actionable"));

    if (CFEqualSafe(viewname, kSOSViewKeychainV0)) {
        retval = SOSAccountVirtualV0Behavior(account, actionCode);
    } else if ([account.trust isSyncingV0] && SOSViewsIsV0Subview(viewname)) {
        // Subviews of V0 syncing can't be turned off if V0 is on.
        require_action_quiet(actionCode = kSOSCCViewDisable, errOut, CFSTR("Have V0 peer can't disable"));
        retval = kSOSCCViewMember;
    } else {
        CFMutableSetRef pendingSet = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        CFSetAddValue(pendingSet, viewname);
        
        if(actionCode == kSOSCCViewEnable && currentStatus == kSOSCCViewNotMember) {
            if(alreadyInSync) {
                retval = SOSFullPeerInfoUpdateViews(self.fullPeerInfo, actionCode, viewname, error);
                if(retval == kSOSCCViewMember) updateCircle = true;
            } else {
                [self pendEnableViewSet:pendingSet];
                retval = kSOSCCViewMember;
                updateCircle = false;
            }
        } else if(actionCode == kSOSCCViewDisable && currentStatus == kSOSCCViewMember) {
            if(alwaysOn && CFSetContainsValue(alwaysOn, viewname)) {
               retval = kSOSCCViewMember;
            } else if(alreadyInSync) {
                retval = SOSFullPeerInfoUpdateViews(self.fullPeerInfo, actionCode, viewname, error);
                if(retval == kSOSCCViewNotMember) updateCircle = true;
            } else {
                SOSAccountPendDisableViewSet(account, pendingSet);
                retval = kSOSCCViewNotMember;
                updateCircle = false;
            }
        } else {
            retval = currentStatus;
        }
        CFReleaseNull(pendingSet);
        
        if (updateCircle) {
            [self modifyCircle:account.circle_transport err:NULL action:^(SOSCircleRef circle_to_change) {
                secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for views change");
                return SOSCircleUpdatePeerInfo(circle_to_change, self.peerInfo);
            }];
        }
    }
    
errOut:
    CFReleaseNull(alwaysOn);
    return retval;
}
#pragma clang diagnostic pop

-(bool) activeValidInCircle:(SOSAccount*) account err:(CFErrorRef *)error {
    return SOSCircleHasActiveValidPeer(self.trustedCircle, SOSFullPeerInfoGetPeerInfo(self.fullPeerInfo), SOSAccountGetTrustedPublicCredential(account, error), error);
}

-(SOSViewResultCode) viewStatus:(SOSAccount*)account name:(CFStringRef) viewname err:(CFErrorRef *)error
{
    SOSViewResultCode retval = kSOSCCGeneralViewError;
    
    require_action_quiet(SOSAccountGetTrustedPublicCredential(account, error), errOut, SOSCreateError(kSOSErrorNoKey, CFSTR("No Trusted UserKey"), NULL, error));
    require_action_quiet(self.trustedCircle, errOut, SOSCreateError(kSOSErrorNoCircle, CFSTR("No Trusted Circle"), NULL, error));
    require_action_quiet(self.fullPeerInfo, errOut, SOSCreateError(kSOSErrorPeerNotFound, CFSTR("No Peer for Account"), NULL, error));
    require_action_quiet([self activeValidInCircle: account err: error ],
                         errOut, SOSCreateError(kSOSErrorNotInCircle, CFSTR("Not in Circle"), NULL, error));
    
    if ([self valueSetContainsValue:kSOSPendingEnableViewsToBeSetKey value:viewname]) {
        retval = kSOSCCViewMember;
    } else if ([self valueSetContainsValue:kSOSPendingDisableViewsToBeSetKey value:viewname]) {
        retval = kSOSCCViewNotMember;
    } else {
        retval = SOSFullPeerInfoViewStatus(self.fullPeerInfo, viewname, error);
    }
    
    // If that doesn't say we're a member and this view is a V0 subview, and we're syncing V0 views we are a member
    if (retval != kSOSCCViewMember) {
        if ((CFEqualSafe(viewname, kSOSViewKeychainV0) || SOSViewsIsV0Subview(viewname))
            && [account.trust isSyncingV0]) {
            retval = kSOSCCViewMember;
        }
    }
    
    // If we're only an applicant we report pending if we would be a view member
    if (retval == kSOSCCViewMember) {
        bool isApplicant = SOSCircleHasApplicant(self.trustedCircle, self.peerInfo, error);
        if (isApplicant) {
            retval = kSOSCCViewPending;
        }
    }
    
errOut:
    return retval;
}

static void dumpViewSet(CFStringRef label, CFSetRef views) {
    if(views) {
        CFStringSetPerformWithDescription(views, ^(CFStringRef description) {
            secnotice("circleChange", "%@ list: %@", label, description);
        });
    } else {
        secnotice("circleChange", "No %@ list provided.", label);
    }
}

static bool SOSAccountScreenViewListForValidV0(SOSAccount*  account, CFMutableSetRef viewSet, SOSViewActionCode actionCode) {
    bool retval = true;
    if(viewSet && CFSetContainsValue(viewSet, kSOSViewKeychainV0)) {
        retval = SOSAccountVirtualV0Behavior(account, actionCode) != kSOSCCGeneralViewError;
        CFSetRemoveValue(viewSet, kSOSViewKeychainV0);
    }
    return retval;
}

-(bool) updateViewSets:(SOSAccount*)account enabled:(CFSetRef) origEnabledViews disabled:(CFSetRef) origDisabledViews
{
    bool retval = false;
    bool updateCircle = false;
    SOSPeerInfoRef  pi = NULL;

    CFMutableSetRef enabledViews = NULL;
    CFMutableSetRef disabledViews = NULL;
    if(origEnabledViews) enabledViews = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, origEnabledViews);
    if(origDisabledViews) disabledViews = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, origDisabledViews);
    dumpViewSet(CFSTR("Enabled"), enabledViews);
    dumpViewSet(CFSTR("Disabled"), disabledViews);

    require_action_quiet(self.trustedCircle, errOut, secnotice("views", "Attempt to set viewsets with no trusted circle"));

    // Make sure we have a peerInfo capable of supporting views.
    SOSFullPeerInfoRef fpi = self.fullPeerInfo;
    require_action_quiet(fpi, errOut, secnotice("views", "Attempt to set viewsets with no fullPeerInfo"));
    require_action_quiet(enabledViews || disabledViews, errOut, secnotice("views", "No work to do"));

    pi = SOSPeerInfoCreateCopy(kCFAllocatorDefault, SOSFullPeerInfoGetPeerInfo(fpi), NULL);

    require_action_quiet(pi, errOut, secnotice("views", "Couldn't copy PeerInfoRef"));

    if(!SOSPeerInfoVersionIsCurrent(pi)) {
        CFErrorRef updateFailure = NULL;
        require_action_quiet(SOSPeerInfoUpdateToV2(pi, &updateFailure), errOut,
                             (secnotice("views", "Unable to update peer to V2- can't update views: %@", updateFailure), (void) CFReleaseNull(updateFailure)));
        secnotice("V2update", "Updating PeerInfo to V2 within SOSAccountUpdateViewSets");
        updateCircle = true;
    }

    CFStringSetPerformWithDescription(enabledViews, ^(CFStringRef description) {
        secnotice("viewChange", "Enabling %@", description);
    });

    CFStringSetPerformWithDescription(disabledViews, ^(CFStringRef description) {
        secnotice("viewChange", "Disabling %@", description);
    });

    require_action_quiet(SOSAccountScreenViewListForValidV0(account, enabledViews, kSOSCCViewEnable), errOut, secnotice("viewChange", "Bad view change (enable) with kSOSViewKeychainV0"));
    require_action_quiet(SOSAccountScreenViewListForValidV0(account, disabledViews, kSOSCCViewDisable), errOut, secnotice("viewChange", "Bad view change (disable) with kSOSViewKeychainV0"));

    if(SOSAccountHasCompletedInitialSync(account)) {
        if(enabledViews) updateCircle |= SOSViewSetEnable(pi, enabledViews);
        if(disabledViews) updateCircle |= SOSViewSetDisable(pi, disabledViews);
        retval = true;
    } else {
        //hold on to the views and enable them later
        if(enabledViews) [self pendEnableViewSet:enabledViews];
        if(disabledViews) SOSAccountPendDisableViewSet(account, disabledViews);
        retval = true;
    }

    if(updateCircle) {
        /* UPDATE FULLPEERINFO VIEWS */
        require_quiet(SOSFullPeerInfoUpdateToThisPeer(fpi, pi, NULL), errOut);

        require_quiet([self modifyCircle:account.circle_transport err:NULL action:^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for views or peerInfo change");
            bool updated= SOSCircleUpdatePeerInfo(circle_to_change, self.peerInfo);
            return updated;
        }], errOut);
        // Make sure we update the engine
        account.circle_rings_retirements_need_attention = true;
    }

errOut:
    CFReleaseNull(enabledViews);
    CFReleaseNull(disabledViews);
    CFReleaseNull(pi);
    return retval;
}


static inline void CFArrayAppendValueIfNot(CFMutableArrayRef array, CFTypeRef value, CFTypeRef excludedValue)
{
    if (!CFEqualSafe(value, excludedValue))
        CFArrayAppendValue(array, value);
}

-(void) addSyncablePeerBlock:(SOSAccountTransaction*)txn dsName:(CFStringRef) ds_name change:(SOSAccountSyncablePeersBlock) changeBlock
{
    if (!changeBlock) return;
    SOSAccount* account = txn.account;
    CFRetainSafe(ds_name);
    SOSAccountCircleMembershipChangeBlock block_to_register = ^void (SOSAccount *account, SOSCircleRef new_circle,
                                                                     CFSetRef added_peers, CFSetRef removed_peers,
                                                                     CFSetRef added_applicants, CFSetRef removed_applicants) {
        
        if (!CFEqualSafe(SOSCircleGetName(new_circle), ds_name))
            return;
        
        SOSPeerInfoRef myPi = self.peerInfo;
        CFStringRef myPi_id = myPi ? SOSPeerInfoGetPeerID(myPi) : NULL;
        
        CFMutableArrayRef peer_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFMutableArrayRef added_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFMutableArrayRef removed_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        
        if (SOSCircleHasPeer(new_circle, myPi, NULL)) {
            SOSCircleForEachPeer(new_circle, ^(SOSPeerInfoRef peer) {
                CFArrayAppendValueIfNot(peer_ids, SOSPeerInfoGetPeerID(peer), myPi_id);
            });
            
            CFSetForEach(added_peers, ^(const void *value) {
                CFArrayAppendValueIfNot(added_ids, SOSPeerInfoGetPeerID((SOSPeerInfoRef) value), myPi_id);
            });
            
            CFSetForEach(removed_peers, ^(const void *value) {
                CFArrayAppendValueIfNot(removed_ids, SOSPeerInfoGetPeerID((SOSPeerInfoRef) value), myPi_id);
            });
        }
        
        if (CFArrayGetCount(peer_ids) || CFSetContainsValue(removed_peers, myPi))
            changeBlock(peer_ids, added_ids, removed_ids);
        
        CFReleaseSafe(peer_ids);
        CFReleaseSafe(added_ids);
        CFReleaseSafe(removed_ids);
    };
    
    SOSAccountAddChangeBlock(account, block_to_register);
    
    CFSetRef empty = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    if (self.trustedCircle && CFEqualSafe(ds_name, SOSCircleGetName(self.trustedCircle))) {
        block_to_register(account, self.trustedCircle, empty, empty, empty, empty);
    }
    CFReleaseSafe(empty);
}


-(CFSetRef) copyPeerSetForView:(CFStringRef) viewName
{
    CFMutableSetRef result = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    
    if (self.trustedCircle) {
        SOSCircleForEachPeer(self.trustedCircle, ^(SOSPeerInfoRef peer) {
            if (CFSetContainsValue(SOSPeerInfoGetPermittedViews(peer), viewName)) {
                CFSetAddValue(result, peer);
            }
        });
    }
    
    return result;
}

-(SecKeyRef) copyPublicKeyForPeer:(CFStringRef) peer_id err:(CFErrorRef *)error
{
    SecKeyRef publicKey = NULL;
    SOSPeerInfoRef peer = NULL;
    
    require_action_quiet(self.trustedCircle, fail, SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("No circle to get peer key from")));
    
    peer = SOSCircleCopyPeerWithID(self.trustedCircle, peer_id, error);
    require_quiet(peer, fail);
    
    publicKey = SOSPeerInfoCopyPubKey(peer, error);
    
fail:
    CFReleaseSafe(peer);
    return publicKey;
}

//Peers
-(SOSPeerInfoRef) copyPeerWithID:(CFStringRef) peerid err:(CFErrorRef *)error
{
    if(!self.trustedCircle) return NULL;
    return SOSCircleCopyPeerWithID(self.trustedCircle, peerid, error);
}
-(bool) isAccountIdentity:(SOSPeerInfoRef)peerInfo err:(CFErrorRef *)error
{
    return CFEqualSafe(peerInfo, self.peerInfo);
}

-(CFSetRef) copyPeerSetMatching:(SOSModifyPeerBlock)block
{
    CFMutableSetRef result = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    if (self.trustedCircle) {
        SOSCircleForEachPeer(self.trustedCircle, ^(SOSPeerInfoRef peer) {
            if (block(peer)) {
                CFSetAddValue(result, peer);
            }
        });
    }
    
    return result;
}
-(CFArrayRef) copyPeersToListenTo:(SecKeyRef)userPublic err:(CFErrorRef *)error
{
    SOSPeerInfoRef myPeerInfo = self.peerInfo;
    CFStringRef myID = myPeerInfo ? SOSPeerInfoGetPeerID(myPeerInfo) : NULL;
    return [self copySortedPeerArray:error action:^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            if(!CFEqualSafe(myID, SOSPeerInfoGetPeerID(peer)) &&
               SOSPeerInfoApplicationVerify(peer, userPublic, NULL) &&
               !SOSPeerInfoIsRetirementTicket(peer)) {
                CFArrayAppendValue(appendPeersTo, peer);
            }
        });
    }];
}
-(bool) peerSignatureUpdate:(SecKeyRef)privKey err:(CFErrorRef *)error
{
    return self.fullPeerInfo && SOSFullPeerInfoUpgradeSignatures(self.fullPeerInfo, privKey, error);
}
-(bool) updatePeerInfo:(SOSKVSCircleStorageTransport*)circleTransport description:(CFStringRef)updateDescription err:(CFErrorRef *)error update:(SOSModifyPeerInfoBlock)block
{
    if (self.fullPeerInfo == NULL)
        return true;
    
    bool result = block(self.fullPeerInfo, error);

    if (result && [self hasCircle:NULL]) {
        return [self modifyCircle:circleTransport err:error action:^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for %@", updateDescription);
            return SOSCircleUpdatePeerInfo(circle_to_change, self.peerInfo);
        }];
    }
    
    return result;
}

//Views
-(void) removeInvalidApplications:(SOSCircleRef) circle userPublic:(SecKeyRef)userPublic
{
    CFMutableSetRef peersToRemove = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    SOSCircleForEachApplicant(circle, ^(SOSPeerInfoRef peer) {
        if (!SOSPeerInfoApplicationVerify(peer, userPublic, NULL))
            CFSetAddValue(peersToRemove, peer);
    });
    
    CFSetForEach(peersToRemove, ^(const void *value) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef) value;
        
        SOSCircleWithdrawRequest(circle, peer, NULL);
    });
}

-(bool) upgradeiCloudIdentity:(SOSCircleRef) circle privKey:(SecKeyRef) privKey
{
    bool retval = false;
    SOSFullPeerInfoRef cloud_fpi = SOSCircleCopyiCloudFullPeerInfoRef(circle, NULL);
    require_quiet(cloud_fpi != NULL, errOut);
    require_quiet(SOSFullPeerInfoUpgradeSignatures(cloud_fpi, privKey, NULL), errOut);
    retval = SOSCircleUpdatePeerInfo(circle, SOSFullPeerInfoGetPeerInfo(cloud_fpi));
errOut:
    CFReleaseNull(cloud_fpi);
    return retval;
}
const CFStringRef kSOSHsaPreApprovedPeerKeyInfo = CFSTR("HSAPreApprovedPeer");

-(CFMutableSetRef) copyPreApprovedHSA2Info
{
    CFMutableSetRef preApprovedPeers = (CFMutableSetRef) [self getValueFromExpansion:kSOSHsaPreApprovedPeerKeyInfo err:NULL];
    
    if(preApprovedPeers) {
        preApprovedPeers = CFSetCreateMutableCopy(NULL, 0, preApprovedPeers);
    } else {
        preApprovedPeers = CFSetCreateMutableForCFTypes(NULL);
    }
    return preApprovedPeers;
}


-(bool) addiCloudIdentity:(SOSCircleRef) circle key:(SecKeyRef) userKey err:(CFErrorRef*)error
{
    bool result = false;
    SOSFullPeerInfoRef cloud_identity = NULL;
    SOSPeerInfoRef cloud_peer = GenerateNewCloudIdentityPeerInfo(error);
    if(!cloud_peer)
        return result;
    cloud_identity = CopyCloudKeychainIdentity(cloud_peer, error);
    CFReleaseNull(cloud_peer);
    if(!cloud_identity)
        return result;
    if(!SOSCircleRequestAdmission(circle, userKey, cloud_identity, error)) {
        CFReleaseNull(cloud_identity);
        return result;
    }
    
    require_quiet(SOSCircleAcceptRequest(circle, userKey, self.fullPeerInfo, SOSFullPeerInfoGetPeerInfo(cloud_identity), error), err_out);
    result = true;
err_out:
    CFReleaseNull(cloud_identity);
    return result;
}
-(bool) addEscrowToPeerInfo:(SOSFullPeerInfoRef) myPeer err:(CFErrorRef *)error
{
    bool success = false;
    
    CFDictionaryRef escrowRecords = [self getValueFromExpansion:kSOSEscrowRecord err:error];
    success = SOSFullPeerInfoReplaceEscrowRecords(myPeer, escrowRecords, error);
    
    return success;
}

-(CFArrayRef) copySortedPeerArray:(CFErrorRef *)error
                           action:(SOSModifyPeersInCircleBlock)block
{
    CFMutableArrayRef peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    block(self.trustedCircle, peers);
    
    CFArrayOfSOSPeerInfosSortByID(peers);
    
    return peers;

}

#define CURRENT_ACCOUNT_PERSISTENT_VERSION 8

static size_t der_sizeof_data_optional(CFDataRef data)
{
    return data ? der_sizeof_data(data, NULL) : 0;
    
}
-(size_t) getDEREncodedSize:(SOSAccount*)account err:(NSError**)error
{
    size_t sequence_size = 0;
    uint64_t version = CURRENT_ACCOUNT_PERSISTENT_VERSION;
    CFErrorRef failure = NULL;
    
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_uint64(version)),                                    fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary((__bridge CFDictionaryRef)account.gestalt, &failure)),                  fail);
    require_quiet(accumulate_size(&sequence_size, SOSCircleGetDEREncodedSize(self.trustedCircle, &failure)),      fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_fullpeer_or_null(self.fullPeerInfo, &failure)),        fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_uint64(self.departureCode)),                    fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_bool(account.accountKeyIsTrusted, &failure)),          fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_public_bytes(account.accountKey, &failure)),            fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_public_bytes(account.previousAccountKey, &failure)),        fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_data_or_null((__bridge CFDataRef)account.accountKeyDerivationParameters, &failure)),    fail);
    require_quiet(accumulate_size(&sequence_size, SOSPeerInfoSetGetDEREncodedArraySize((__bridge CFSetRef)self.retirees, &failure)),  fail);
    (void)accumulate_size(&sequence_size, der_sizeof_data_optional((__bridge CFDataRef)(account.backup_key)));
    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary((__bridge CFDictionaryRef)(self.expansion), &failure)),  fail);
    
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, sequence_size);
    
fail:
    // Ensure some error is made, maybe not this one, tho.
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, &failure);
    if (error) {
        if (failure != NULL) {
            *error = (__bridge_transfer NSError*) failure;
            failure = NULL;
        }
    }
    CFReleaseNull(failure);

    return 0;
}

static uint8_t* der_encode_data_optional(CFDataRef data, CFErrorRef *error,
                                         const uint8_t *der, uint8_t *der_end)
{
    return data ? der_encode_data(data, error, der, der_end) : der_end;
    
}

-(uint8_t*) encodeToDER:(SOSAccount*)account err:(NSError**) error start:(const uint8_t*) der end:(uint8_t*)der_end
{
    CFErrorRef failure = NULL;
    uint64_t version = CURRENT_ACCOUNT_PERSISTENT_VERSION;
    der_end =  ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                        ccder_encode_uint64(version, der,
                                        der_encode_dictionary((__bridge CFDictionaryRef)account.gestalt, &failure, der,
                                        SOSCircleEncodeToDER(self.trustedCircle, &failure, der,
                                        der_encode_fullpeer_or_null(self.fullPeerInfo, &failure, der,
                                        ccder_encode_uint64(self.departureCode, der,
                                        ccder_encode_bool(account.accountKeyIsTrusted, der,
                                        der_encode_public_bytes(account.accountKey, &failure, der,
                                        der_encode_public_bytes(account.previousAccountKey, &failure, der,
                                        der_encode_data_or_null((__bridge CFDataRef)(account.accountKeyDerivationParameters), &failure, der,
                                        SOSPeerInfoSetEncodeToArrayDER((__bridge CFSetRef)(self.retirees), &failure, der,
                                        der_encode_data_optional((__bridge CFDataRef)account.backup_key, &failure, der,
                                        der_encode_dictionary((__bridge CFDictionaryRef)(self.expansion), &failure, der,
                                        der_end)))))))))))));
    
    if (error) {
        if (failure != NULL) {
            *error = (__bridge_transfer NSError*) failure;
            failure = NULL;
        }
    }
    CFReleaseNull(failure);

    return der_end;
}

-(CFMutableSetRef) CF_RETURNS_RETAINED syncWithPeers:(SOSAccountTransaction*) txn peerIDs:(CFSetRef) /* CFStringRef */ peerIDs err:(CFErrorRef *)error
{
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
        require_quiet(peerID, skip);
        
        peerInfo = SOSCircleCopyPeerWithID(self.trustedCircle, peerID, NULL);
        if (peerInfo && SOSCircleHasValidSyncingPeer(self.trustedCircle, peerInfo, account.accountKey, NULL)) {
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

-(bool) requestSyncWithAllPeers:(SOSAccountTransaction*) txn key:(SecKeyRef)userPublic err:(CFErrorRef *)error
{
    if (![txn.account isInCircle: error]) {
        return false;
    }

    NSMutableSet<NSString*>* allSyncingPeerIDs = [NSMutableSet set];

    SOSCircleForEachValidSyncingPeer(self.trustedCircle, userPublic, ^(SOSPeerInfoRef peer) {
        [allSyncingPeerIDs  addObject: (__bridge NSString*) SOSPeerInfoGetPeerID(peer)];
    });

    [txn requestSyncWithPeers: allSyncingPeerIDs];

    return true;
}

-(bool) isSyncingV0{
    __block bool syncingV0 = false;
    
    [self forEachCirclePeerExceptMe:^(SOSPeerInfoRef peer){
        if (SOSPeerInfoIsEnabledView(peer, kSOSViewKeychainV0)) {
            syncingV0 = true;
        }
    }];

    return syncingV0;
}

-(SOSEngineRef) getDataSourceEngine:(SOSDataSourceFactoryRef)factory
{
    // This is at least a piece of <rdar://problem/59045931> SecItemDataSourceFactoryCopyDataSource; looks like UaF
    if(!self.trustedCircle) {
        secnotice("engine", "Tried to set dataSourceEngine with no circle");
        return NULL;
    }
    return SOSDataSourceFactoryGetEngineForDataSourceName(factory, SOSCircleGetName(self.trustedCircle), NULL);
}

-(bool) postDebugScope:(SOSKVSCircleStorageTransport*) circle_transport scope:(CFTypeRef) scope err:(CFErrorRef*)error
{
    bool result = false;
    if (circle_transport) {
        result = [circle_transport kvssendDebugInfo:kSOSAccountDebugScope debug:scope err:error];
    }
    return result;
}

-(SecKeyRef) copyDeviceKey:(CFErrorRef *)error
{
    SecKeyRef privateKey = NULL;

    require_action_quiet(self.fullPeerInfo, fail, SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("No identity to get key from")));
    
    privateKey = SOSFullPeerInfoCopyDeviceKey(self.fullPeerInfo, error);
    
fail:
    return privateKey;

}
-(bool) removeIncompleteiCloudIdentities:(SOSCircleRef) circle privKey:(SecKeyRef) privKey err:(CFErrorRef *)error
{
    bool retval = false;
    
    CFMutableSetRef iCloud2Remove = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSCircleForEachActivePeer(self.trustedCircle, ^(SOSPeerInfoRef peer) {
        if(SOSPeerInfoIsCloudIdentity(peer)) {
            SOSFullPeerInfoRef icfpi = SOSFullPeerInfoCreateCloudIdentity(kCFAllocatorDefault, peer, NULL);
            if(!icfpi) {
                CFSetAddValue(iCloud2Remove, peer);
            }
            CFReleaseNull(icfpi);
        }
    });
    
    if(CFSetGetCount(iCloud2Remove) > 0) {
        retval = true;
        SOSCircleRemovePeers(self.trustedCircle, privKey, self.fullPeerInfo, iCloud2Remove, error);
    }
    CFReleaseNull(iCloud2Remove);
    return retval;
}

-(bool) clientPing:(SOSAccount*)account
{
    if (self.trustedCircle && self.fullPeerInfo
        && SOSFullPeerInfoPing(self.fullPeerInfo, NULL)) {
        [self modifyCircle:account.circle_transport err:NULL action:^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for gestalt change");
            return SOSCircleUpdatePeerInfo(circle_to_change, self.peerInfo);
        }];
    }
    
    return true;
}
static NSString* kSOSRingKey = @"trusted_rings";

-(void) addRingDictionary {

    if(self.expansion) {
        if(![self.expansion valueForKey:kSOSRingKey]) {
            NSMutableDictionary *rings = [NSMutableDictionary dictionary];
            [self.expansion setObject:rings forKey:kSOSRingKey];
        }
    }
}

-(void) resetRingDictionary {
    if(self.expansion) {
        NSMutableDictionary *rings = [NSMutableDictionary dictionary];
        [self.expansion setObject:rings forKey:kSOSRingKey];
    }
}

@end

