//
//  SOSAccountCircles.c
//  sec
//

#import <Security/SecureObjectSync/SOSAccountPriv.h>
#import <Security/SecureObjectSync/SOSTransport.h>
#import <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#import <Security/SecureObjectSync/SOSTransportMessageKVS.h>
#import <Security/SecureObjectSync/SOSTransportCircleKVS.h>
#import <Security/SecureObjectSync/SOSTransportCircleCK.h>
#import <Security/SecureObjectSync/SOSAccountTrust.h>
#import <Security/SecureObjectSync/SOSAccountTrustClassic.h>

#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include "SOSCloudKeychainClient.h"

//
// MARK: Circle management
//


SOSCircleRef CF_RETURNS_RETAINED SOSAccountEnsureCircle(SOSAccount* a, CFStringRef name, CFErrorRef *error)
{
    CFErrorRef localError = NULL;
    SOSAccountTrustClassic *trust = a.trust;
    SOSCircleRef circle = trust.trustedCircle;

    if (circle == NULL) {
        circle = SOSCircleCreate(NULL, name, NULL);
        a.key_interests_need_updating = true;
        [trust setTrustedCircle:circle];
    } else {
        CFRetainSafe(circle);
    }

    require_action_quiet(circle || !isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle), fail,
                         if (error) { *error = localError; localError = NULL; });
   
fail:
    CFReleaseNull(localError);
    return circle;
}

