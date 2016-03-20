
#ifndef SOSPeerCoder_h
#define SOSPeerCoder_h
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSCoder.h>

enum SOSCoderUnwrapStatus{
    SOSCoderUnwrapError = 0,
    SOSCoderUnwrapDecoded = 1,
    SOSCoderUnwrapHandled = 2
};

bool SOSPeerCoderSendMessageIfNeeded(SOSEngineRef engine, SOSPeerRef peer, CFDataRef *message_to_send, CFStringRef circle_id, CFStringRef peer_id, SOSEnginePeerMessageSentBlock *sent, CFErrorRef *error);

enum SOSCoderUnwrapStatus SOSPeerHandleCoderMessage(SOSPeerRef peer, CFStringRef peer_id, CFDataRef codedMessage, CFDataRef *decodedMessage, bool *forceSave, CFErrorRef *error);

bool SOSPeerCoderInitializeForPeer(SOSEngineRef engine, SOSFullPeerInfoRef myPeerInfo, SOSPeerInfoRef peerInfo, CFErrorRef *error);

bool SOSPeerSendMessageIfNeeded(SOSPeerRef peer, CFDataRef *message, CFDataRef *message_to_send, SOSCoderRef *coder, CFStringRef circle_id, CFStringRef peer_id, SOSEnginePeerMessageSentBlock *sent, CFErrorRef *error);

void SOSPeerCoderConsume(SOSEnginePeerMessageSentBlock *sent, bool ok);

#endif
