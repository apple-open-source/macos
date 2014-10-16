
#ifndef SOSTransportCoder_h
#define SOSTransportCoder_h
#include <SecureObjectSync/SOSTransportMessage.h>
#include <SecureObjectSync/SOSCoder.h>

enum SOSCoderUnwrapStatus{
    SOSCoderUnwrapError = 0,
    SOSCoderUnwrapDecoded = 1,
    SOSCoderUnwrapHandled = 2
};

bool SOSPeerCoderSendMessageIfNeeded(SOSPeerRef peer, CFDataRef *message_to_send, CFStringRef circle_id, CFStringRef peer_id, SOSEnginePeerMessageSentBlock *sent, CFErrorRef *error);

enum SOSCoderUnwrapStatus SOSPeerHandleCoderMessage(SOSPeerRef peer, CFStringRef peer_id, CFDataRef codedMessage, CFDataRef *decodedMessage, CFErrorRef *error);

bool SOSPeerCoderInitializeForPeer(SOSTransportMessageRef transport, SOSFullPeerInfoRef myPeerInfo, SOSPeerInfoRef peerInfo, CFErrorRef *error);

bool SOSPeerSendMessageIfNeeded(SOSPeerRef peer, CFDataRef *message, CFDataRef *message_to_send, SOSCoderRef *coder, CFStringRef circle_id, CFStringRef peer_id, SOSEnginePeerMessageSentBlock *sent, CFErrorRef *error);

void SOSPeerCoderConsume(SOSEnginePeerMessageSentBlock *sent, bool ok);

#endif
