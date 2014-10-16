

#ifndef sec_SOSTransportMessageKVS_h
#define sec_SOSTransportMessageKVS_h
#include <SecureObjectSync/SOSAccount.h>

//
// KVS Stuff
//

typedef struct __OpaqueSOSTransportMessageKVS *SOSTransportMessageKVSRef;

SOSTransportMessageKVSRef SOSTransportMessageKVSCreate(SOSAccountRef account, CFStringRef circleName, CFErrorRef *error);

CFStringRef SOSTransportMessageKVSGetCircleName(SOSTransportMessageKVSRef transport);


//
// Key interests
//

bool SOSTransportMessageSyncWithPeers(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef *error);

bool SOSTransportMessageKVSAppendKeyInterest(SOSTransportMessageKVSRef transport, CFMutableArrayRef alwaysKeys, CFMutableArrayRef afterFirstUnlockKeys, CFMutableArrayRef unlockedKeys, CFErrorRef *localError);

bool SOSTransportMessageSendMessageIfNeeded(SOSTransportMessageRef transport, CFStringRef circle_id, CFStringRef peer_id, CFErrorRef *error);    

#endif
