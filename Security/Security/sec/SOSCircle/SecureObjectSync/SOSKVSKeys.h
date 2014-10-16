

#ifndef SOSKVSKEYS_H
#define SOSKVSKEYS_H

#include "SOSCircle.h"
#include <SecureObjectSync/SOSTransportMessage.h>
#include <SecureObjectSync/SOSTransportMessageKVS.h>

//
// MARK: Key formation
//

typedef enum {
    kCircleKey,
    kMessageKey,
    kParametersKey,
    kInitialSyncKey,
    kRetirementKey,
    kAccountChangedKey,
    kUnknownKey,
} SOSKVSKeyType;

extern const CFStringRef kSOSKVSKeyParametersKey;
extern const CFStringRef kSOSKVSInitialSyncKey;
extern const CFStringRef kSOSKVSAccountChangedKey;
extern const CFStringRef sCirclePrefix;
extern const CFStringRef sRetirementPrefix;


SOSKVSKeyType SOSKVSKeyGetKeyType(CFStringRef key);
SOSKVSKeyType SOSKVSKeyGetKeyTypeAndParse(CFStringRef key, CFStringRef *circle, CFStringRef *from, CFStringRef *to);

CFStringRef SOSCircleKeyCreateWithCircle(SOSCircleRef circle, CFErrorRef *error);
CFStringRef SOSCircleKeyCreateWithName(CFStringRef name, CFErrorRef *error);
CFStringRef SOSCircleKeyCopyCircleName(CFStringRef key, CFErrorRef *error);
CFStringRef SOSMessageKeyCreateWithCircleNameAndPeerNames(CFStringRef circleName, CFStringRef from_peer_name, CFStringRef to_peer_name);

CFStringRef SOSMessageKeyCopyCircleName(CFStringRef key, CFErrorRef *error);
CFStringRef SOSMessageKeyCopyFromPeerName(CFStringRef messageKey, CFErrorRef *error);
CFStringRef SOSMessageKeyCreateWithCircleAndPeerNames(SOSCircleRef circle, CFStringRef from_peer_name, CFStringRef to_peer_name);
CFStringRef SOSMessageKeyCreateWithCircleAndPeerInfos(SOSCircleRef circle, SOSPeerInfoRef from_peer, SOSPeerInfoRef to_peer);

CFStringRef SOSRetirementKeyCreateWithCircleNameAndPeer(CFStringRef circle_name, CFStringRef retirement_peer_name);
CFStringRef SOSRetirementKeyCreateWithCircleAndPeer(SOSCircleRef circle, CFStringRef retirement_peer_name);

CFStringRef SOSMessageKeyCreateFromTransportToPeer(SOSTransportMessageKVSRef transport, CFStringRef peer_name);
CFStringRef SOSMessageKeyCreateFromPeerToTransport(SOSTransportMessageKVSRef transport, CFStringRef peer_name);

#endif
