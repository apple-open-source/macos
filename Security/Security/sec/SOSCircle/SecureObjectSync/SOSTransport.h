

#ifndef SOSTransport_h
#define SOSTransport_h
#include <SecureObjectSync/SOSTransportMessage.h>
#include <SecureObjectSync/SOSTransportCircle.h>
#include <SecureObjectSync/SOSTransportKeyParameter.h>

CF_RETURNS_RETAINED CFMutableArrayRef SOSTransportDispatchMessages(SOSAccountRef account, CFDictionaryRef updates, CFErrorRef *error);

void SOSRegisterTransportMessage(SOSTransportMessageRef additional);
void SOSUnregisterTransportMessage(SOSTransportMessageRef removal);

void SOSRegisterTransportCircle(SOSTransportCircleRef additional);
void SOSUnregisterTransportCircle(SOSTransportCircleRef removal);

void SOSRegisterTransportKeyParameter(SOSTransportKeyParameterRef additional);
void SOSUnregisterTransportKeyParameter(SOSTransportKeyParameterRef removal);
void SOSUnregisterAllTransportMessages(void);
void SOSUnregisterAllTransportCircles(void);
void SOSUnregisterAllTransportKeyParameters(void);


void SOSUpdateKeyInterest(void);

#endif
