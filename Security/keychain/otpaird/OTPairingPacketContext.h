#import <IDS/IDS.h>

NS_ASSUME_NONNULL_BEGIN

@interface OTPairingPacketContext : NSObject
- (instancetype)initWithMessage:(NSDictionary *)message fromID:(NSString *)fromID context:(IDSMessageContext *)context;
@property (readonly, atomic) enum OTPairingIDSMessageType messageType;
@property (readonly, atomic) NSString *sessionIdentifier;
@property (readonly, atomic) NSData *packetData;
@property (readonly, atomic, nullable) NSError *error;
@property (readonly, atomic) NSString *incomingResponseIdentifier;
@property (readonly, atomic) NSString *outgoingResponseIdentifier;
@property (readonly, atomic) NSString *fromID;
@end

NS_ASSUME_NONNULL_END
