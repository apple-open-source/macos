//
//  SOSCoder.h
//  sec
//
//  Created by Richard Murphy on 2/6/13.
//
//

#ifndef sec_SOSCoder_h
#define sec_SOSCoder_h

#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSPeer.h>

typedef struct __OpaqueSOSCoder *SOSCoderRef;

SOSCoderRef SOSCoderCreate(SOSPeerInfoRef peerInfo, SOSFullPeerInfoRef myPeerInfo, CFErrorRef *error);
SOSCoderRef SOSCoderCreateFromData(CFDataRef exportedData, CFErrorRef *error);

void SOSCoderDispose(SOSCoderRef coder);

CFDataRef SOSCoderCopyDER(SOSCoderRef coder, CFErrorRef* error);

SOSPeerCoderStatus
SOSCoderStart(SOSCoderRef coder, SOSPeerSendBlock sendBlock, CFStringRef clientId, CFErrorRef *error);

SOSPeerCoderStatus
SOSCoderResendDH(SOSCoderRef coder, SOSPeerSendBlock sendBlock, CFErrorRef *error);

void SOSCoderPersistState(CFStringRef peer_id, SOSCoderRef coder);

SOSPeerCoderStatus SOSCoderUnwrap(SOSCoderRef coder, SOSPeerSendBlock send_block, CFDataRef codedMessage, CFMutableDataRef *message, CFStringRef clientId, CFErrorRef *error);

SOSPeerCoderStatus SOSCoderWrap(SOSCoderRef coder, CFDataRef message, CFMutableDataRef *codedMessage, CFStringRef clientId, CFErrorRef *error);

bool SOSCoderCanWrap(SOSCoderRef coder);

void SOSCoderReset(SOSCoderRef coder);

#endif
