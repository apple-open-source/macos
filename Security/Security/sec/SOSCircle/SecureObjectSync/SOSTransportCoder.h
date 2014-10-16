
#ifndef SOSTransportCoder_h
#define SOSTransportCoder_h
#include <SecureObjectSync/SOSTransportMessage.h>
#include <SecureObjectSync/SOSCoder.h>

enum SOSCoderUnwrapStatus{
    SOSCoderUnwrapError = 0,
    SOSCoderUnwrapDecoded = 1,
    SOSCoderUnwrapHandled = 2
};

enum SOSCoderUnwrapStatus SOSTransportMessageHandleCoderMessage(SOSTransportMessageRef transport, CFStringRef peer_id, CFDataRef codedMessage, CFDataRef *decodedMessage, CFErrorRef *error);

bool SOSTransportMessageSavePeerCoderData(SOSTransportMessageRef transport, SOSCoderRef coder, CFStringRef peer_id, CFErrorRef *error);



#endif
