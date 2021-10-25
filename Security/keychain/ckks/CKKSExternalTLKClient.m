
#import "keychain/ckks/CKKSExternalTLKClient.h"


NSString* CKKSSEViewPTA = @"SE-PTA";
NSString* CKKSSEViewPTC = @"SE-PTC";

@implementation CKKSExternalKey

- (instancetype)initWithView:(NSString*)view
                        uuid:(NSString*)uuid
               parentTLKUUID:(NSString* _Nullable)parentKeyUUID
                     keyData:(NSData*)keyData
{
    if((self = [super init])) {
        _view = view;
        _uuid = uuid;
        _parentKeyUUID = parentKeyUUID ?: uuid;
        _keyData = keyData;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSExternalKey: %@ (%@)>", self.uuid, self.parentKeyUUID];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
    if (self = [super init]) {
        _view = [coder decodeObjectOfClass:[NSString class] forKey:@"view"];
        _uuid = [coder decodeObjectOfClass:[NSString class] forKey:@"uuid"];
        _parentKeyUUID = [coder decodeObjectOfClass:[NSString class] forKey:@"parentKeyUUID"];
        _keyData = [coder decodeObjectOfClass:[NSData class] forKey:@"keyData"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
    [coder encodeObject:self.view forKey:@"view"];
    [coder encodeObject:self.uuid forKey:@"uuid"];
    [coder encodeObject:self.parentKeyUUID forKey:@"parentKeyUUID"];
    [coder encodeObject:self.keyData forKey:@"keyData"];
}

- (BOOL)isEqual:(id)object
{
    CKKSExternalKey *other = (CKKSExternalKey *)object;
    return [other isMemberOfClass:[self class]]
    &&
    ([self.view isEqualToString:other.view])
    &&
    ([self.uuid isEqualToString:other.uuid])
    &&
    ((self.parentKeyUUID == nil && other.parentKeyUUID == nil) || ([self.parentKeyUUID isEqualToString:other.parentKeyUUID]))
    &&
    ([self.keyData isEqualToData:other.keyData]);
}

- (NSDictionary*)jsonDictionary
{
    return @{
        @"view": self.view,
        @"uuid": self.uuid,
        @"parentKeyUUID": self.parentKeyUUID,
        @"keyData": [self.keyData base64EncodedStringWithOptions:0],
    };
}

+ (CKKSExternalKey* _Nullable)parseFromJSONDict:(NSDictionary*)jsonDict error:(NSError**)error
{
    NSString* view = jsonDict[@"view"];
    NSString* uuid = jsonDict[@"uuid"];
    NSString* parentKeyUUID = jsonDict[@"parentKeyUUID"];
    NSString* keyDatab64 = jsonDict[@"keyData"];

    if(!keyDatab64) {
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecParam
                                     userInfo:@{
                NSLocalizedDescriptionKey: @"No wrapped key to parse",
            }];
        }
        return nil;
    }

    NSData* keyData = [[NSData alloc] initWithBase64EncodedString:keyDatab64 options:0];

    if(!view || !uuid || !parentKeyUUID || !keyData) {
        if(error) {
            NSMutableArray<NSString*>* missingKeys = [NSMutableArray array];
            if(!view) {
                [missingKeys addObject:@"view"];
            }
            if(!uuid) {
                [missingKeys addObject:@"uuid"];
            }
            if(!parentKeyUUID) {
                [missingKeys addObject:@"parentKeyUUID"];
            }
            if(!keyData) {
                [missingKeys addObject:@"keyData"];
            }

            if(error) {
                *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                             code:errSecParam
                                         userInfo:@{
                    NSLocalizedDescriptionKey: @"Missing some required field",
                    @"missingkeys": missingKeys,
                }];
            }
        }
        return nil;
    }

    return [[CKKSExternalKey alloc] initWithView:view
                                            uuid:uuid
                                   parentTLKUUID:parentKeyUUID
                                         keyData:keyData];
}

@end

@implementation CKKSExternalTLKShare

- (instancetype)initWithView:(NSString*)view
                     tlkUUID:(NSString*)tlkUUID
              receiverPeerID:(NSData*)receiverPeerID
                senderPeerID:(NSData*)senderPeerID
                  wrappedTLK:(NSData*)wrappedTLK
                   signature:(NSData*)signature
{
    if((self = [super init])) {
        _view = view;
        _tlkUUID = tlkUUID;
        _receiverPeerID = receiverPeerID;
        _senderPeerID = senderPeerID;
        _wrappedTLK = wrappedTLK;
        _signature = signature;
    }
    return self;
}

- (NSString*)stringifyPeerID:(NSData*)peerID
{
    // To comply with the CKKS server peerID restrictions, we're going to prefix this with the SOS prefix.
    return [NSString stringWithFormat:@"spid-%@", [peerID base64EncodedStringWithOptions:0]];
}

+ (NSData* _Nullable)unstringifyPeerID:(NSString*)peerID
{
    NSString* base64Str = nil;
    if([peerID hasPrefix:@"spid-"]) {
        base64Str = [peerID substringFromIndex:@"spid-".length];
    } else {
        base64Str = peerID;
    }

    if(!base64Str) {
        return nil;
    }

    return [[NSData alloc] initWithBase64EncodedString:base64Str options:0];
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSExternalTLKShare(%@): recv:%@ send:%@@>",
            self.tlkUUID,
            [self stringifyPeerID:self.receiverPeerID],
            [self stringifyPeerID:self.senderPeerID]];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
    if (self = [super init]) {
        _view = [coder decodeObjectOfClass:[NSString class] forKey:@"view"];
        _tlkUUID = [coder decodeObjectOfClass:[NSString class] forKey:@"tlkUUID"];
        _receiverPeerID = [coder decodeObjectOfClass:[NSData class] forKey:@"receiverPeerID"];
        _senderPeerID = [coder decodeObjectOfClass:[NSData class] forKey:@"senderPeerID"];
        _wrappedTLK = [coder decodeObjectOfClass:[NSData class] forKey:@"wrappedTLK"];
        _signature = [coder decodeObjectOfClass:[NSData class] forKey:@"signature"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
    [coder encodeObject:self.view forKey:@"view"];
    [coder encodeObject:self.tlkUUID forKey:@"tlkUUID"];
    [coder encodeObject:self.receiverPeerID forKey:@"receiverPeerID"];
    [coder encodeObject:self.senderPeerID forKey:@"senderPeerID"];
    [coder encodeObject:self.wrappedTLK forKey:@"wrappedTLK"];
    [coder encodeObject:self.signature forKey:@"signature"];
}

- (NSDictionary*)jsonDictionary
{
    return @{
        @"view": self.view,
        @"tlkUUID": self.tlkUUID,
        @"receiverPeerID": [self stringifyPeerID:self.receiverPeerID],
        @"senderPeerID": [self stringifyPeerID:self.senderPeerID],
        @"wrappedTLK": [self.wrappedTLK base64EncodedStringWithOptions:0],
        @"signature": [self.signature base64EncodedStringWithOptions:0],
    };
}

+ (CKKSExternalTLKShare* _Nullable)parseFromJSONDict:(NSDictionary*)jsonDict error:(NSError**)error
{
    NSString* view = jsonDict[@"view"];
    NSString* tlkUUID = jsonDict[@"tlkUUID"];

    NSData* receiverPeerID = [self unstringifyPeerID:jsonDict[@"receiverPeerID"]];
    NSData* senderPeerID = [self unstringifyPeerID:jsonDict[@"senderPeerID"]];

    NSData* wrapppedTLK = [[NSData alloc] initWithBase64EncodedString:jsonDict[@"wrappedTLK"] options:0];
    NSData* signature = [[NSData alloc] initWithBase64EncodedString:jsonDict[@"signature"] options:0];

    if(!view || !tlkUUID || !receiverPeerID || !senderPeerID || !wrapppedTLK || !signature) {

        NSMutableArray<NSString*>* missingKeys = [NSMutableArray array];
        if(!view) {
            [missingKeys addObject:@"view"];
        }
        if(!tlkUUID) {
            [missingKeys addObject:@"tlkUUID"];
        }
        if(!receiverPeerID) {
            [missingKeys addObject:@"receiverPeerID"];
        }
        if(!senderPeerID) {
            [missingKeys addObject:@"senderPeerID"];
        }
        if(!wrapppedTLK) {
            [missingKeys addObject:@"wrapppedTLK"];
        }
        if(!signature) {
            [missingKeys addObject:@"signature"];
        }

        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecParam
                                     userInfo:@{
                NSLocalizedDescriptionKey: @"Missing some required field",
                @"missingkeys": missingKeys,
            }];
        }
        return nil;
    }

    return [[CKKSExternalTLKShare alloc] initWithView:view
                                              tlkUUID:tlkUUID
                                       receiverPeerID:receiverPeerID
                                         senderPeerID:senderPeerID
                                           wrappedTLK:wrapppedTLK
                                            signature:signature];
}
@end
