

#ifndef sec_SOSTransportCircleKVS_h
#define sec_SOSTransportCircleKVS_h

#include <SecureObjectSync/SOSAccount.h>

typedef struct __OpaqueSOSTransportCircleKVS * SOSTransportCircleKVSRef;

SOSTransportCircleKVSRef SOSTransportCircleKVSCreate(SOSAccountRef account, CFStringRef circleName, CFErrorRef *error);
SOSTransportKeyParameterRef SOSTransportKeyParameterCreateForSubclass(size_t size, SOSAccountRef account, CFErrorRef *error);

bool SOSTransportCircleKVSAppendKeyInterest(SOSTransportCircleKVSRef transport, CFMutableArrayRef alwaysKeys, CFMutableArrayRef afterFirstUnlockKeys, CFMutableArrayRef unlockedKeys, CFErrorRef *error);

void SOSTransportCircleKVSAddToPendingChanges(SOSTransportCircleKVSRef transport, CFStringRef message_key, CFDataRef message_data);
bool SOSTransportCircleKVSSendPendingChanges(SOSTransportCircleKVSRef transport, CFErrorRef *error);


#endif
