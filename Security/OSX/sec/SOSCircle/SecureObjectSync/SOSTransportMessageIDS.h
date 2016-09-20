//
//  SOSTransportMessageIDS.h
//  sec
//
//
#include <Security/SecureObjectSync/SOSAccount.h>

typedef enum {
    kIDSStartPingTestMessage = 1,
    kIDSEndPingTestMessage= 2,
    kIDSSendOneMessage = 3,
    kIDSSyncMessagesRaw = 4,
    kIDSSyncMessagesCompact = 5,
    kIDSPeerAvailability = 6,
    kIDSPeerAvailabilityDone = 7,
    kIDSKeychainSyncIDSFragmentation = 8
} idsOperation;

//error handling stuff

typedef enum {
    kSecIDSErrorNoDeviceID = -1, //default case
    kSecIDSErrorNotRegistered = -2,
    kSecIDSErrorFailedToSend=-3,
    kSecIDSErrorCouldNotFindMatchingAuthToken = -4,
    kSecIDSErrorDeviceIsLocked = -5,
    kSecIDSErrorNoPeersAvailable = -6
    
} idsError;


extern const CFStringRef kSecIDSErrorDomain;
extern const CFStringRef kIDSOperationType;
extern const CFStringRef kIDSMessageToSendKey;

typedef struct __OpaqueSOSTransportMessageIDS *SOSTransportMessageIDSRef;

SOSTransportMessageIDSRef SOSTransportMessageIDSCreate(SOSAccountRef account, CFStringRef circleName, CFErrorRef *error);

HandleIDSMessageReason SOSTransportMessageIDSHandleMessage(SOSAccountRef account, CFDictionaryRef message, CFErrorRef *error);

void SOSTransportMessageIDSGetIDSDeviceID(SOSAccountRef account);

void SOSTransportMessageIDSSetFragmentationPreference(SOSTransportMessageRef transport, CFBooleanRef preference);
CFBooleanRef SOSTransportMessageIDSGetFragmentationPreference(SOSTransportMessageRef transport);

