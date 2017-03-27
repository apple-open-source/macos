

#ifndef SOSTransport_h
#define SOSTransport_h
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <Security/SecureObjectSync/SOSAccount.h>

CF_RETURNS_RETAINED CFMutableArrayRef SOSTransportDispatchMessages(SOSAccountTransactionRef txn, CFDictionaryRef updates, CFErrorRef *error);

void SOSRegisterTransportMessage(SOSTransportMessageRef additional);
void SOSUnregisterTransportMessage(SOSTransportMessageRef removal);

void SOSRegisterTransportCircle(SOSTransportCircleRef additional);
void SOSUnregisterTransportCircle(SOSTransportCircleRef removal);

void SOSRegisterTransportKeyParameter(SOSTransportKeyParameterRef additional);
void SOSUnregisterTransportKeyParameter(SOSTransportKeyParameterRef removal);
void SOSUnregisterAllTransportMessages(void);
void SOSUnregisterAllTransportCircles(void);
void SOSUnregisterAllTransportKeyParameters(void);


void SOSUpdateKeyInterest(SOSAccountRef account);

enum TransportType{
    kUnknown = 0,
    kKVS = 1,
    kIDS = 2,
    kBackupPeer = 3,
    kIDSTest = 4,
    kKVSTest = 5
};

#endif
