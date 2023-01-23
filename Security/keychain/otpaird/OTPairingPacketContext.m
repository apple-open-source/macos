#import <TargetConditionals.h>
#import <Foundation/Foundation.h>
#import <IDS/IDS.h>

#import "keychain/categories/NSError+UsefulConstructors.h"

#import "OTPairingPacketContext.h"
#import "OTPairingConstants.h"

@interface OTPairingPacketContext ()
@property (readwrite, atomic) NSDictionary *message;
@property (readwrite, atomic) NSString *fromID;
@property (readwrite, atomic) NSString *incomingResponseIdentifier;
@property (readwrite, atomic) NSString *outgoingResponseIdentifier;
@end

@implementation OTPairingPacketContext

@synthesize message = _message;
@synthesize fromID = _fromID;
@synthesize incomingResponseIdentifier = _incomingResponseIdentifier;
@synthesize outgoingResponseIdentifier = _outgoingResponseIdentifier;
@synthesize error = _error;

- (instancetype)initWithMessage:(NSDictionary *)message fromID:(NSString *)fromID context:(IDSMessageContext *)context
{
    if ((self = [super init])) {
        self.message = message;
        self.fromID = fromID;
        self.incomingResponseIdentifier = context.incomingResponseIdentifier;
        self.outgoingResponseIdentifier = context.outgoingResponseIdentifier;
    }
    return self;
}

- (enum OTPairingIDSMessageType)messageType
{
    NSNumber *typeNum;
    enum OTPairingIDSMessageType type;

    typeNum = self.message[OTPairingIDSKeyMessageType];
    if (typeNum != nil) {
        type = [typeNum intValue];
    } else {
        // From older internal builds; remove soon
        if (self.packetData != nil) {
            type = OTPairingIDSMessageTypePacket;
        } else {
            type = OTPairingIDSMessageTypeError;
        }
    }

    return type;
}

- (NSString *)sessionIdentifier
{
    return self.message[OTPairingIDSKeySession];
}

- (NSData *)packetData
{
    return self.message[OTPairingIDSKeyPacket];
}

- (NSError *)error
{
    if (self.messageType != OTPairingIDSMessageTypeError) {
        return nil;
    }

    if (!self->_error) {
        NSString *errorString = self.message[OTPairingIDSKeyErrorDescription];
        self->_error = [NSError errorWithDomain:OTPairingErrorDomain code:OTPairingErrorTypeRemote description:errorString];
    }

    return self->_error;
}

@end
