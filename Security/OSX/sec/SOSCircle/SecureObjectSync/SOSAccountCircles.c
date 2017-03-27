//
//  SOSAccountCircles.c
//  sec
//

#include "SOSAccountPriv.h"
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameterKVS.h>
#include <Security/SecureObjectSync/SOSTransportCircleKVS.h>
#include <Security/SecureObjectSync/SOSTransportMessageKVS.h>
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include "SOSCloudKeychainClient.h"


//
// MARK: Circle management
//

SecKeyRef SOSAccountCopyPublicKeyForPeer(SOSAccountRef account, CFStringRef peer_id, CFErrorRef *error) {
    SecKeyRef publicKey = NULL;
    SOSPeerInfoRef peer = NULL;

    require_action_quiet(account->trusted_circle, fail, SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("No circle to get peer key from")));

    peer = SOSCircleCopyPeerWithID(account->trusted_circle, peer_id, error);
    require_quiet(peer, fail);

    publicKey = SOSPeerInfoCopyPubKey(peer, error);

fail:
    CFReleaseSafe(peer);
    return publicKey;
}


SOSCircleRef SOSAccountGetCircle(SOSAccountRef a, CFErrorRef *error)
{
    CFTypeRef entry = a->trusted_circle;

    require_action_quiet(!isNull(entry), fail,
                         SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("Incompatible circle in KVS"), NULL, error));
    
    require_action_quiet(entry, fail,
                         SOSCreateError(kSOSErrorNoCircle, CFSTR("No circle found"), NULL, error));
    
    
    return (SOSCircleRef) entry;
    
fail:
    return NULL;
}

static bool SOSAccountInflateTransportsForCircle(SOSAccountRef account, CFStringRef circleName, CFErrorRef *error){
    bool success = false;

    SOSTransportKeyParameterRef tKey = NULL;
    SOSTransportCircleRef tCircle = NULL;
    SOSTransportMessageRef tidsMessage = NULL;
    SOSTransportMessageRef tkvsMessage = NULL;
    
    tKey = (SOSTransportKeyParameterRef)SOSTransportKeyParameterKVSCreate(account, error);
    tCircle = (SOSTransportCircleRef)SOSTransportCircleKVSCreate(account, circleName, error);
    
    require_quiet(tKey, fail);
    require_quiet(tCircle, fail);
    
    tidsMessage = (SOSTransportMessageRef)SOSTransportMessageIDSCreate(account, circleName, error);
    require_quiet(tidsMessage, fail);
    
    CFRetainAssign(account->ids_message_transport, tidsMessage);
    
    tkvsMessage = (SOSTransportMessageRef)SOSTransportMessageKVSCreate(account, circleName, error);
    require_quiet(tkvsMessage, fail);
    
    CFRetainAssign(account->kvs_message_transport, tkvsMessage);

    CFRetainAssign(account->key_transport, (SOSTransportKeyParameterRef)tKey);
    CFRetainAssign(account->circle_transport, tCircle);

    
    success = true;
fail:
    CFReleaseNull(tKey);
    CFReleaseNull(tCircle);
    CFReleaseNull(tidsMessage);
    CFReleaseNull(tkvsMessage);
    return success;
}

SOSCircleRef SOSAccountEnsureCircle(SOSAccountRef a, CFStringRef name, CFErrorRef *error)
{
    CFErrorRef localError = NULL;

    if (a->trusted_circle == NULL) {
        a->trusted_circle = SOSCircleCreate(NULL, name, NULL);
        a->key_interests_need_updating = true;
    }

    
    SOSCircleRef circle = SOSAccountGetCircle(a, &localError);
    
    require_action_quiet(circle || !isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle), fail,
                         if (error) { *error = localError; localError = NULL; });

    require_quiet(SOSAccountInflateTransportsForCircle(a, name, error), fail);
   
fail:
    CFReleaseNull(localError);
    return circle;
}


bool SOSAccountUpdateCircleFromRemote(SOSAccountRef account, SOSCircleRef newCircle, CFErrorRef *error)
{
    return SOSAccountHandleUpdateCircle(account, newCircle, false, error);
}

bool SOSAccountUpdateCircle(SOSAccountRef account, SOSCircleRef newCircle, CFErrorRef *error)
{
    return SOSAccountHandleUpdateCircle(account, newCircle, true, error);
}

bool SOSAccountModifyCircle(SOSAccountRef account,
                            CFErrorRef* error,
                            bool (^action)(SOSCircleRef circle))
{
    bool success = false;
    
    SOSCircleRef circle = NULL;
    require_action_quiet(account->trusted_circle, fail, SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("No circle to get peer key from")));

    circle = SOSCircleCopyCircle(kCFAllocatorDefault, account->trusted_circle, error);
    require_quiet(circle, fail);

    success = true;
    require_quiet(action(circle), fail);
    
    success = SOSAccountUpdateCircle(account, circle, error);
    
fail:
    CFReleaseSafe(circle);
    return success;
}

CFSetRef SOSAccountCopyPeerSetMatching(SOSAccountRef account, bool (^action)(SOSPeerInfoRef peer)) {
    CFMutableSetRef result = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    
    if (account->trusted_circle) {
        SOSCircleForEachPeer(account->trusted_circle, ^(SOSPeerInfoRef peer) {
            if (action(peer)) {
                CFSetAddValue(result, peer);
            }
        });
    }
    
    return result;
}
