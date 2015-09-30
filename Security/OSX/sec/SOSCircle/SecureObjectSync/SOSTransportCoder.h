
#ifndef SOSTransportCoder_h
#define SOSTransportCoder_h
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSCoder.h>

enum SOSCoderUnwrapStatus{
    SOSCoderUnwrapError = 0,
    SOSCoderUnwrapDecoded = 1,
    SOSCoderUnwrapHandled = 2
};

enum SOSCoderUnwrapStatus SOSTransportMessageHandleCoderMessage(SOSTransportMessageRef transport, CFStringRef peer_id, CFDataRef codedMessage, CFDataRef *decodedMessage, CFErrorRef *error);

bool SOSTransportMessageSavePeerCoderData(SOSTransportMessageRef transport, SOSCoderRef coder, CFStringRef peer_id, CFErrorRef *error);

bool SOSTransportCoderInitializeForPeer(SOSTransportMessageRef transport, SOSFullPeerInfoRef myPeerInfo, SOSPeerInfoRef peerInfo, CFErrorRef *error);
bool SOSTransportMessageSendMessageIfNeeded(SOSTransportMessageRef transport, CFStringRef circle_id, CFStringRef peer_id, CFErrorRef *error);


#endif
