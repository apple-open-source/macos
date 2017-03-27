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
    kIDSPeerReceivedACK = 4,
    kIDSPeerAvailability = 6,
    kIDSPeerAvailabilityDone = 7,
    kIDSKeychainSyncIDSFragmentation = 8,
    kIDSPeerUsesACK = 9
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

typedef struct __OpaqueSOSTransportMessageIDS *SOSTransportMessageIDSRef;

SOSTransportMessageIDSRef SOSTransportMessageIDSCreate(SOSAccountRef account, CFStringRef circleName, CFErrorRef *error);

HandleIDSMessageReason SOSTransportMessageIDSHandleMessage(SOSAccountRef account, CFDictionaryRef message, CFErrorRef *error);

bool SOSTransportMessageIDSGetIDSDeviceID(SOSAccountRef account);

void SOSTransportMessageIDSSetFragmentationPreference(SOSTransportMessageRef transport, CFBooleanRef preference);
CFBooleanRef SOSTransportMessageIDSGetFragmentationPreference(SOSTransportMessageRef transport);

