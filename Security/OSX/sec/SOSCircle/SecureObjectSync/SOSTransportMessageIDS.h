//
//  SOSTransportMessageIDS.h
//  sec
//
//
#ifndef sec_SOSTransportMessageIDS_h
#define sec_SOSTransportMessageIDS_h

@class SOSMessage;

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


extern const CFStringRef kSecIDSErrorDomain;
extern const CFStringRef kIDSOperationType;
extern const CFStringRef kIDSMessageToSendKey;
extern const CFStringRef kIDSMessageUniqueID;
extern const CFStringRef kIDSMessageRecipientPeerID;
extern const CFStringRef kIDSMessageRecipientDeviceID;
extern const CFStringRef kIDSMessageUsesAckModel;
extern const CFStringRef kIDSMessageSenderDeviceID;;

@interface SOSMessageIDS : SOSMessage
{
    CFBooleanRef useFragmentation;
}
@property (atomic)  CFBooleanRef useFragmentation;

-(id) initWithAcount:(SOSAccount*)acct circleName:(CFStringRef)name;

-(HandleIDSMessageReason) SOSTransportMessageIDSHandleMessage:(SOSAccount*)account m:(CFDictionaryRef) message err:(CFErrorRef *)error;

-(bool) SOSTransportMessageIDSGetIDSDeviceID:(SOSAccount*)acct;

-(void) SOSTransportMessageIDSSetFragmentationPreference:(SOSMessage*) transport pref:(CFBooleanRef) preference;
-(CFBooleanRef) SOSTransportMessageIDSGetFragmentationPreference:(SOSMessage*) transport;

@end
#endif
