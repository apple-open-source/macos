//
//  SOSAccountGhost.c
//  sec
//
//  Created by Richard Murphy on 4/12/16.
//
//

#include "SOSAccountPriv.h"
#include "SOSAccountGhost.h"
#include "keychain/SecureObjectSync/SOSPeerInfoCollections.h"
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSAccount.h"
#include "keychain/SecureObjectSync/SOSCircle.h"
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#include "keychain/SecureObjectSync/SOSPeerInfoPriv.h"
#include "keychain/SecureObjectSync/SOSAuthKitHelpers.h"
#import "Analytics/Clients/SOSAnalytics.h"
#include "utilities/SecTrace.h"


#define DETECT_IOS_ONLY 1

static bool sosGhostCheckValid(SOSPeerInfoRef pi) {
#if DETECT_IOS_ONLY
    bool retval = false;
    require_quiet(pi, retOut);
    SOSPeerInfoDeviceClass peerClass = SOSPeerInfoGetClass(pi);
    switch(peerClass) {
        case SOSPeerInfo_iOS:
        case SOSPeerInfo_tvOS:
        case SOSPeerInfo_watchOS:
        case SOSPeerInfo_macOS:
            retval = true;
            break;
        case SOSPeerInfo_iCloud:
        case SOSPeerInfo_unknown:
        default:
            retval = false;
            break;
    }
retOut:
    return retval;
#else
    return true;
#endif
}

static CFSetRef SOSCircleCreateGhostsOfPeerSet(SOSCircleRef circle, SOSPeerInfoRef me) {
    CFMutableSetRef ghosts = NULL;
    require_quiet(me, errOut);
    require_quiet(sosGhostCheckValid(me), errOut);
    require_quiet(circle, errOut);
    require_quiet(SOSPeerInfoSerialNumberIsSet(me), errOut);
    CFStringRef mySerial = SOSPeerInfoCopySerialNumber(me);
    require_quiet(mySerial, errOut);
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(me);
    ghosts = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    require_quiet(ghosts, errOut1);
    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef pi) {
        CFStringRef theirPeerID = SOSPeerInfoGetPeerID(pi);
        if(!CFEqual(myPeerID, theirPeerID)) {
            CFStringRef piSerial = SOSPeerInfoCopySerialNumber(pi);
            if(CFEqualSafe(mySerial, piSerial)) {
                CFSetAddValue(ghosts, theirPeerID);
            }
            CFReleaseNull(piSerial);
        }
    });
errOut1:
    CFReleaseNull(mySerial);
errOut:
    return ghosts;
}

static void SOSCircleClearMyGhosts(SOSCircleRef circle, SOSPeerInfoRef me) {
    CFSetRef ghosts = SOSCircleCreateGhostsOfPeerSet(circle, me);
    if(!ghosts || CFSetGetCount(ghosts) == 0) {
        CFReleaseNull(ghosts);
        return;
    }
    SOSCircleRemovePeersByIDUnsigned(circle, ghosts);
    CFReleaseNull(ghosts);
}

// This only works if you're in the circle and have the private key
CF_RETURNS_RETAINED SOSCircleRef SOSAccountCloneCircleWithoutMyGhosts(SOSAccount* account, SOSCircleRef startCircle) {
    SOSCircleRef newCircle = NULL;
    CFSetRef ghosts = NULL;
    require_quiet(account, retOut);
    SecKeyRef userPrivKey = SOSAccountGetPrivateCredential(account, NULL);
    require_quiet(userPrivKey, retOut);
    SOSPeerInfoRef me = account.peerInfo;
    require_quiet(me, retOut);
    bool iAmApplicant = SOSCircleHasApplicant(startCircle, me, NULL);
    
    ghosts = SOSCircleCreateGhostsOfPeerSet(startCircle, me);
    require_quiet(ghosts, retOut);
    require_quiet(CFSetGetCount(ghosts), retOut);

    CFStringSetPerformWithDescription(ghosts, ^(CFStringRef description) {
        secnotice("ghostbust", "Removing peers: %@", description);
    });

    newCircle = SOSCircleCopyCircle(kCFAllocatorDefault, startCircle, NULL);
    require_quiet(newCircle, retOut);
    if(iAmApplicant) {
        if(SOSCircleRemovePeersByIDUnsigned(newCircle, ghosts) && (SOSCircleCountPeers(newCircle) == 0)) {
            secnotice("resetToOffering", "Reset to offering with last ghost and me as applicant");
            if(!SOSCircleResetToOffering(newCircle, userPrivKey, account.fullPeerInfo, NULL) ||
               ![account.trust addiCloudIdentity:newCircle key:userPrivKey err:NULL]){
                CFReleaseNull(newCircle);
            }
            account.notifyBackupOnExit = true;
        } else {
            CFReleaseNull(newCircle);
        }
    } else {
        SOSCircleRemovePeersByID(newCircle, userPrivKey, account.fullPeerInfo, ghosts, NULL);
    }
retOut:
    CFReleaseNull(ghosts);
    return newCircle;
}


static NSUInteger SOSGhostBustThinSerialClones(SOSCircleRef circle, NSString *myPeerID) {
    NSUInteger gbcount = 0;
    CFMutableArrayRef sortPeers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
        CFArrayAppendValue(sortPeers, peer);
    });
    CFRange range = CFRangeMake(0, CFArrayGetCount(sortPeers));
    CFArraySortValues(sortPeers, range, SOSPeerInfoCompareByApplicationDate, NULL);
    
    NSMutableDictionary *latestPeers = [[NSMutableDictionary alloc] init];
    NSMutableSet *removals = [[NSMutableSet alloc] init];

    for(CFIndex i = CFArrayGetCount(sortPeers); i > 0; i--) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) CFArrayGetValueAtIndex(sortPeers, i-1);
        
        if(sosGhostCheckValid(pi)) {
            NSString *serial = CFBridgingRelease(SOSPeerInfoV2DictionaryCopyString(pi, sSerialNumberKey));
            NSString *peerID = (__bridge NSString *)SOSPeerInfoGetPeerID(pi);
            if(serial != nil) {
                if([latestPeers objectForKey:serial] != nil) {
                    if(peerID != myPeerID) {
                        [removals addObject:peerID];
                    } else {
                        secnotice("ghostBust", "There is a more recent peer for this serial number");
                    }
                } else {
                    [latestPeers setObject:peerID forKey:serial];
                }
            } else {
                secnotice("ghostBust", "Removing peerID (%@) with no serial number", peerID);
                [removals addObject:peerID];
            }
        }
    }
    SOSCircleRemovePeersByIDUnsigned(circle, (__bridge CFSetRef)(removals));
    gbcount = [removals count];
    CFReleaseNull(sortPeers);
    return gbcount;
}

static void SOSCircleRemoveiCloudIdentities(SOSCircleRef circle) {
    NSMutableSet *removals = [[NSMutableSet alloc] init];
    SOSCircleForEachiCloudIdentityPeer(circle, ^(SOSPeerInfoRef peer) {
        NSString *peerID = (__bridge NSString *)SOSPeerInfoGetPeerID(peer);
        [removals addObject:peerID];
    });
    SOSCircleRemovePeersByIDUnsigned(circle, (__bridge CFSetRef)(removals));
}

bool SOSAccountGhostResultsInReset(SOSAccount* account) {
    if(!account.peerID || !account.trust.trustedCircle) return false;
    SOSCircleRef newCircle = SOSCircleCopyCircle(kCFAllocatorDefault, account.trust.trustedCircle, NULL);
    if(!newCircle) return false;
    SOSCircleClearMyGhosts(newCircle, account.peerInfo);
    SOSCircleRemoveRetired(newCircle, NULL);
    SOSCircleRemoveiCloudIdentities(newCircle);
    int npeers = SOSCircleCountPeers(newCircle);
    CFReleaseNull(newCircle);
    return npeers == 0;
}

static NSUInteger SOSGhostBustThinByMIDList(SOSCircleRef circle, NSString *myPeerID, SOSAuthKitHelpers *akh, SOSAccountGhostBustingOptions options, NSMutableDictionary *attributes) {
    __block unsigned int gbmid = 0;
    __block unsigned int gbserial = 0;

    NSMutableSet *removals = [[NSMutableSet alloc] init];
    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
        NSString *peerID = (__bridge NSString *)SOSPeerInfoGetPeerID(peer);
        if([peerID isEqualToString:myPeerID]) {
            return;
        }
        if(options & SOSGhostBustByMID) {
            NSString *mid = CFBridgingRelease(SOSPeerInfoV2DictionaryCopyString(peer, sMachineIDKey));
            if(![akh midIsValidInList:mid]) {
                [removals addObject:peerID];
                gbmid++;
                return;
            }
        }
        if(options & SOSGhostBustBySerialNumber) {
            NSString *serial = CFBridgingRelease(SOSPeerInfoV2DictionaryCopyString(peer, sSerialNumberKey));
            if(![akh serialIsValidInList: serial]) {
                [removals addObject:peerID];
                gbserial++;
                return;
            }
        }
    });
    
    // Now use the removal set to ghostbust the circle
    SOSCircleRemovePeersByIDUnsigned(circle, (__bridge CFSetRef) removals);
    attributes[@"byMID"] = @(gbmid);
    attributes[@"bySerial"] = @(gbserial);
    return [removals count];
}

static bool SOSGhostBustiCloudIdentityPrivateKeys(SOSAccount *account) {
    __block bool cleaned = false;
    SecKeyRef pubKey = NULL;
    CFTypeRef queryResult = NULL;
    CFDataRef pubKeyHash = NULL;
    CFDictionaryRef query = NULL;
    __block int removed = 0;

    SOSFullPeerInfoRef iCloudIdentity = SOSCircleCopyiCloudFullPeerInfoVerifier(account.trust.trustedCircle, NULL);
    require_action_quiet(iCloudIdentity, retOut, secnotice("ghostBust", "No iCloud Identity FPI"));
    pubKey = SOSFullPeerInfoCopyPubKey(iCloudIdentity, NULL);
    require_action_quiet(pubKey, retOut, secnotice("ghostBust", "Can't get iCloud Identity pubkey"));
    pubKeyHash = SecKeyCopyPublicKeyHash(pubKey);
    require_action_quiet(pubKeyHash, retOut, secnotice("ghostBust", "Can't get iCloud Identity pubkey hash"));
    query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
        kSecMatchLimit,            kSecMatchLimitAll,
        kSecClass,                 kSecClassKey,
        kSecAttrKeyClass,          kSecAttrKeyClassPrivate,
        kSecAttrSynchronizable,    kSecAttrSynchronizableAny,
        kSecReturnAttributes,      kCFBooleanTrue,
        kSecAttrAccessGroup,       kSOSInternalAccessGroup,
        NULL);
    require_action_quiet(errSecSuccess == SecItemCopyMatching(query, &queryResult), retOut, secnotice("ghostBust", "Can't get iCloud Identity private keys"));
    require_action_quiet(CFGetTypeID(queryResult) == CFArrayGetTypeID(), retOut, cleaned = true);
    CFArrayRef iCloudPrivKeys = queryResult;
    secnotice("ghostBust", "Screening %ld icloud private keys", (long)CFArrayGetCount(iCloudPrivKeys));
    CFArrayForEach(iCloudPrivKeys, ^(const void *value) {
        CFDictionaryRef privkey = asDictionary(value, NULL);
        if(privkey) {
            CFDataRef candidate = asData(CFDictionaryGetValue(privkey, kSecAttrApplicationLabel), NULL);
            if(candidate && !CFEqual(pubKeyHash, candidate)) {
                CFStringRef label = asString(CFDictionaryGetValue(privkey, kSecAttrLabel), NULL);
                if(label) {
                    if(CFStringHasPrefix(label, CFSTR("Cloud Identity"))) {

                        CFDictionaryRef delQuery = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                            kSecClass,                 kSecClassKey,
                            kSecAttrKeyClass,          kSecAttrKeyClassPrivate,
                            kSecAttrSynchronizable,    kSecAttrSynchronizableAny,
                            kSecAttrApplicationLabel,   candidate,
                            NULL);

                        OSStatus status = SecItemDelete(delQuery);
                        if(errSecSuccess == status) {
                            secnotice("ghostBust", "removed %@", label);
                            removed++;
                            cleaned = true;
                        } else {
                            secnotice("ghostbust", "Search for %@ returned %d", candidate, (int) status);
                        }
                        CFReleaseNull(delQuery);
                    }
                }
            }
        }
    });
    secnotice("ghostBust", "Removed %d icloud private keys", removed);
retOut:
    CFReleaseNull(query);
    CFReleaseNull(queryResult);
    CFReleaseNull(pubKeyHash);
    CFReleaseNull(pubKey);
    CFReleaseNull(iCloudIdentity);
    return cleaned;
}

bool SOSAccountGhostBustCircle(SOSAccount *account, SOSAuthKitHelpers *akh, SOSAccountGhostBustingOptions options, int mincount) {
    __block bool result = false;
    CFErrorRef localError = NULL;
    __block NSUInteger nbusted = 9999;
    NSMutableDictionary *attributes =[NSMutableDictionary new];
    int circleSize = SOSCircleCountPeers(account.trust.trustedCircle);

    if ([akh isUseful] && [account isInCircle:nil] && circleSize > mincount) {
        if(options & SOSGhostBustiCloudIdentities) {
            secnotice("ghostBust", "Callout to cleanup icloud identities");
            result = SOSGhostBustiCloudIdentityPrivateKeys(account);
        } else {
            [account.trust modifyCircle:account.circle_transport err:&localError action:^(SOSCircleRef circle) {
                nbusted = SOSGhostBustThinByMIDList(circle, account.peerID, akh, options, attributes);
                secnotice("ghostbust", "Removed %lu ghosts from circle by midlist && serialNumber", (unsigned long)nbusted);
                if(options & SOSGhostBustSerialByAge) {
                    NSUInteger thinBusted = 9999;
                    thinBusted = SOSGhostBustThinSerialClones(circle, account.peerID);
                    nbusted += thinBusted;
                    attributes[@"byAge"] = @(thinBusted);
                }
                attributes[@"total"] = @(SecBucket1Significant(nbusted));
                attributes[@"startCircleSize"] = @(SecBucket1Significant(circleSize));
                result = nbusted > 0;
                if(result) {
                    SOSAccountRestartPrivateCredentialTimer(account);
                    if((SOSAccountGetPrivateCredential(account, NULL) != NULL) || SOSAccountAssertStashedAccountCredential(account, NULL)) {
                        result = SOSCircleGenerationSign(circle, SOSAccountGetPrivateCredential(account, NULL), account.fullPeerInfo, NULL);
                    } else {
                        result = false;
                    }
                }
                return result;
            }];
            secnotice("circleOps", "Ghostbusting %@ (%@)", result ? CFSTR("Performed") : CFSTR("Not Performed"), localError);
        }
    } else {
        secnotice("circleOps", "Ghostbusting skipped");
    }
    CFReleaseNull(localError);
    
    if(result) {
        [[SOSAnalytics logger] logSoftFailureForEventNamed:@"GhostBust" withAttributes:attributes];
    } else if(nbusted == 0){
        [[SOSAnalytics logger] logSuccessForEventNamed:@"GhostBust"];
    } else {
        [[SOSAnalytics logger] logHardFailureForEventNamed:@"GhostBust" withAttributes:nil];
    }
    return result;
}
