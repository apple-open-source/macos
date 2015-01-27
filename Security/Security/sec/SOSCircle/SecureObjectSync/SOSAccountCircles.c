//
//  SOSAccountCircles.c
//  sec
//

#include "SOSAccountPriv.h"
#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSTransportKeyParameterKVS.h>
#include <SecureObjectSync/SOSTransportCircleKVS.h>
#include <SecureObjectSync/SOSTransportMessageKVS.h>

void SOSAccountForEachCircle(SOSAccountRef account, void (^process)(SOSCircleRef circle))
{
    CFDictionaryForEach(account->circles, ^(const void* key, const void* value) {
        assert(value);
        process((SOSCircleRef)value);
    });
}


void SOSAccountForEachKnownCircle(SOSAccountRef account,
                                  void (^handle_incompatible)(CFStringRef name),
                                  void (^handle_no_peer)(SOSCircleRef circle),
                                  void (^handle_peer)(SOSCircleRef circle, SOSFullPeerInfoRef full_peer)) {
    CFDictionaryForEach(account->circles, ^(const void *key, const void *value) {
        if (isNull(value)) {
            if (handle_incompatible)
                handle_incompatible((CFStringRef)key);
        } else {
            SOSCircleRef circle = (SOSCircleRef) value;
            CFRetainSafe(circle);
            SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, SOSCircleGetName(circle), NULL);
            if (!fpi) {
                if (handle_no_peer)
                    handle_no_peer(circle);
            } else {
                CFRetainSafe(fpi);
                if (handle_peer)
                    handle_peer(circle, fpi);
                CFReleaseSafe(fpi);
            }
            CFReleaseSafe(circle);
        }
    });
}

//
// MARK: Circle management
//

int SOSAccountCountCircles(SOSAccountRef a) {
    assert(a);
    assert(a->circle_identities);
    assert(a->circles);
    return (int)CFDictionaryGetCount(a->circles);
}



SOSCircleRef SOSAccountFindCircle(SOSAccountRef a, CFStringRef name, CFErrorRef *error)
{
    CFTypeRef entry = CFDictionaryGetValue(a->circles, name);
    
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
    SOSTransportMessageRef tMessage = NULL;
    
#if 0 // IDS_FUTURE
    // Solve determining transport type without fullpeer
    SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircleNamed(account, circleName, error);
    require_quiet(fpi, fail);
    SOSPeerInfoRef myPeer = SOSFullPeerInfoGetPeerInfo(fpi);
    require_quiet(myPeer, fail);
    CFStringRef type = SOSPeerInfoGetTransportType(myPeer);
    if(CFStringCompare(type, CFSTR("KVS"), 0) == kCFCompareEqualTo){
#endif
    tKey = (SOSTransportKeyParameterRef)SOSTransportKeyParameterKVSCreate(account, error);
    tCircle = (SOSTransportCircleRef)SOSTransportCircleKVSCreate(account, circleName, error);
    tMessage = (SOSTransportMessageRef)SOSTransportMessageKVSCreate(account, circleName, error);
    require_quiet(tKey, fail);
    require_quiet(tCircle, fail);
    require_quiet(tMessage, fail);
    
    CFRetainAssign(account->key_transport, (SOSTransportKeyParameterRef)tKey);
    CFDictionarySetValue(account->circle_transports, circleName, tCircle);
    CFDictionarySetValue(account->message_transports, circleName, tMessage);
#if 0 // IDS_FUTURE
    }
#endif
    success = true;

fail:
    CFReleaseNull(tKey);
    CFReleaseNull(tCircle);
    CFReleaseNull(tMessage);
    return success;
}

SOSCircleRef SOSAccountEnsureCircle(SOSAccountRef a, CFStringRef name, CFErrorRef *error)
{
    CFErrorRef localError = NULL;
    
    SOSCircleRef circle = SOSAccountFindCircle(a, name, &localError);
    
    require_action_quiet(circle || !isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle), fail,
                         if (error) { *error = localError; localError = NULL; });
    
    if(NULL == circle){
        circle = SOSCircleCreate(NULL, name, NULL);
        if (circle){
            CFDictionaryAddValue(a->circles, name, circle);
            CFRelease(circle);
            circle = SOSAccountFindCircle(a, name, &localError);
        }

        SOSUpdateKeyInterest();
    }

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
                            CFStringRef circleName,
                            CFErrorRef* error,
                            bool (^action)(SOSCircleRef circle))
{
    bool success = false;
    
    SOSCircleRef circle = NULL;
    SOSCircleRef accountCircle = SOSAccountFindCircle(account, circleName, error);
    require_quiet(accountCircle, fail);
    
    circle = SOSCircleCopyCircle(kCFAllocatorDefault, accountCircle, error);
    require_quiet(circle, fail);
    
    success = true;
    require_quiet(action(circle), fail);
    
    success = SOSAccountUpdateCircle(account, circle, error);
    
fail:
    CFReleaseSafe(circle);
    return success;
}

