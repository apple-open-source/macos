#import <TargetConditionals.h>
#import <Foundation/Foundation.h>
#import <IDS/IDS.h>
#import <Security/SecXPCHelper.h>

#import "keychain/categories/NSError+UsefulConstructors.h"

#import "OTPairingPacketContext.h"
#import "OTPairingConstants.h"

@interface OTPairingPacketContext ()
@property (readwrite) NSDictionary *message;
@property (readwrite) NSString *fromID;
@property (readwrite) IDSMessageContext *context;
@end

@implementation OTPairingPacketContext

@synthesize message = _message;
@synthesize fromID = _fromID;
@synthesize context = _context;
@synthesize error = _error;

- (instancetype)initWithMessage:(NSDictionary *)message fromID:(NSString *)fromID context:(IDSMessageContext *)context
{
    self = [super init];
    if (self != nil) {
        self.message = message;
        self.fromID = fromID;
        self.context = context;
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
        NSData *errorData = self.message[OTPairingIDSKeyError];
        if (errorData != NULL) {
            self->_error = [SecXPCHelper errorFromEncodedData:errorData];
        } else {
            // Key from older iOS builds; remove soon
            // When this is removed, it will still be useful to have a fallback in case errorData is missing or errorFromEncodedData fails
            NSString *errorString = self.message[OTPairingIDSKeyErrorDeprecated];
            self->_error = [NSError errorWithDomain:OTPairingErrorDomain code:OTPairingErrorTypeRemote description:errorString];
        }
    }

    return self->_error;
}

- (NSString *)incomingResponseIdentifier
{
    return self.context.incomingResponseIdentifier;
}

- (NSString *)outgoingResponseIdentifier
{
    return self.context.outgoingResponseIdentifier;
}

@end
