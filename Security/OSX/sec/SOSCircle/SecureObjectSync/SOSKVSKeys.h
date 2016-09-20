

#ifndef SOSKVSKEYS_H
#define SOSKVSKEYS_H

#include "SOSCircle.h"
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSTransportMessageKVS.h>

//
// MARK: Key formation
//

typedef enum {
    kCircleKey = 0,
    kMessageKey,
    kParametersKey,
    kInitialSyncKey,
    kRetirementKey,
    kAccountChangedKey,
    kDebugInfoKey,
    kRingKey,
    kPeerInfoKey,
    kLastCircleKey,
    kLastKeyParameterKey,
    kUnknownKey,
} SOSKVSKeyType;

extern const CFStringRef kSOSKVSKeyParametersKey;
extern const CFStringRef kSOSKVSInitialSyncKey;
extern const CFStringRef kSOSKVSAccountChangedKey;
extern const CFStringRef kSOSKVSRequiredKey;
extern const CFStringRef kSOSKVSOfficialDSIDKey;

//extern const CFStringRef kSOSKVSDebugInfo;

extern const CFStringRef sCirclePrefix;
extern const CFStringRef sRetirementPrefix;
extern const CFStringRef sLastCirclePushedPrefix;
extern const CFStringRef sLastKeyParametersPushedPrefix;
extern const CFStringRef sDebugInfoPrefix;

SOSKVSKeyType SOSKVSKeyGetKeyType(CFStringRef key);
bool SOSKVSKeyParse(SOSKVSKeyType keyType, CFStringRef key, CFStringRef *circle, CFStringRef *peerInfo, CFStringRef *ring, CFStringRef *backupName, CFStringRef *from, CFStringRef *to);
SOSKVSKeyType SOSKVSKeyGetKeyTypeAndParse(CFStringRef key, CFStringRef *circle, CFStringRef *peerInfo, CFStringRef *ring, CFStringRef *backupName, CFStringRef *from, CFStringRef *to);

CFStringRef SOSCircleKeyCreateWithCircle(SOSCircleRef circle, CFErrorRef *error);
CFStringRef SOSPeerInfoKeyCreateWithName(CFStringRef peer_info_name, CFErrorRef *error);
CFStringRef SOSRingKeyCreateWithName(CFStringRef ring_name, CFErrorRef *error);


CFStringRef SOSCircleKeyCreateWithName(CFStringRef name, CFErrorRef *error);
CFStringRef SOSCircleKeyCopyCircleName(CFStringRef key, CFErrorRef *error);
CFStringRef SOSMessageKeyCreateWithCircleNameAndPeerNames(CFStringRef circleName, CFStringRef from_peer_name, CFStringRef to_peer_name);

CFStringRef SOSMessageKeyCopyCircleName(CFStringRef key, CFErrorRef *error);
CFStringRef SOSMessageKeyCopyFromPeerName(CFStringRef messageKey, CFErrorRef *error);
CFStringRef SOSMessageKeyCreateWithCircleAndPeerNames(SOSCircleRef circle, CFStringRef from_peer_name, CFStringRef to_peer_name);
CFStringRef SOSMessageKeyCreateWithCircleAndPeerInfos(SOSCircleRef circle, SOSPeerInfoRef from_peer, SOSPeerInfoRef to_peer);

CFStringRef SOSRetirementKeyCreateWithCircleNameAndPeer(CFStringRef circle_name, CFStringRef retirement_peer_name);
CFStringRef SOSRetirementKeyCreateWithCircleAndPeer(SOSCircleRef circle, CFStringRef retirement_peer_name);

CFStringRef SOSMessageKeyCreateFromTransportToPeer(SOSTransportMessageRef transport, CFStringRef peer_name);
CFStringRef SOSMessageKeyCreateFromPeerToTransport(SOSTransportMessageRef transport, CFStringRef peer_name);
CFStringRef SOSMessageKeyCreateWithCircleNameAndTransportType(CFStringRef circleName, CFStringRef transportType);

CFStringRef SOSPeerInfoV2KeyCreateWithPeerName(CFStringRef peer_name);
CFStringRef SOSRingKeyCreateWithRingName(CFStringRef ring_name);
CFStringRef SOSLastCirclePushedKeyCreateWithCircleNameAndPeerID(CFStringRef circleName, CFStringRef peerID);
CFStringRef SOSLastCirclePushedKeyCreateWithAccountGestalt(SOSAccountRef account);
CFStringRef SOSLastKeyParametersPushedKeyCreateWithPeerID(CFStringRef peerID);
CFStringRef SOSLastKeyParametersPushedKeyCreateWithAccountGestalt(SOSAccountRef account);
CFStringRef SOSDebugInfoKeyCreateWithTypeName(CFStringRef type_name);

#endif
