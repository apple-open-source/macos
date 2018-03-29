//
//  SOSAccountGhost.c
//  sec
//
//  Created by Richard Murphy on 4/12/16.
//
//

#include "SOSAccountPriv.h"
#include "SOSAccountGhost.h"
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h>

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
            retval = true;
            break;
        case SOSPeerInfo_macOS:
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

static CFSetRef SOSTrustedCircleCreateGhostsOfPeerSet(SOSAccount* account, SOSPeerInfoRef pi) {
    return (account) ? SOSCircleCreateGhostsOfPeerSet([account.trust getCircle:NULL], pi): NULL;
}

static CFSetRef SOSTrustedCircleCopyGhostSet(SOSAccount* account) {
    CFSetRef ghosts = NULL;
    require_quiet(account, errOut);
    SOSPeerInfoRef  me = account.peerInfo;
    require_quiet(me, errOut);
    require_quiet(sosGhostCheckValid(me), errOut);
    ghosts = SOSTrustedCircleCreateGhostsOfPeerSet(account, me);
errOut:
    return ghosts;
    
}

static CFIndex SOSTrustedCircleGhostSetCount(SOSAccount* account) {
    CFIndex retval = 0;
    CFSetRef ghosts = SOSTrustedCircleCopyGhostSet(account);
    require_quiet(ghosts, retOut);
    retval = CFSetGetCount(ghosts);
    CFReleaseNull(ghosts);
retOut:
    return retval;
}

bool SOSAccountTrustedCircleHasNoGhostOfMe(SOSAccount* account) {
    return SOSTrustedCircleGhostSetCount(account) == 0;
}

bool SOSAccountGhostResultsInReset(SOSAccount* account) {
    return SOSTrustedCircleGhostSetCount(account) == SOSCircleCountPeers([account.trust getCircle:NULL]);
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

