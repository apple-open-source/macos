//
//  KCAESGCMDuplexSession.h
//  Security
//
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface KCAESGCMDuplexSession : NSObject <NSSecureCoding>

// Due to design constraints, this session object is the only thing serialized during piggybacking sessions.
// Therefore, we must add some extra data here, which is not strictly part of a AES GCM session.
@property (retain, nullable) NSString* pairingUUID;
@property uint64_t piggybackingVersion;
@property uint64_t epoch;

- (nullable NSData*) encrypt: (NSData*) data error: (NSError**) error;
- (nullable NSData*) decryptAndVerify: (NSData*) data error: (NSError**) error;

+ (nullable instancetype) sessionAsSender: (NSData*) sharedSecret
                                  context: (uint64_t) context;
+ (nullable instancetype) sessionAsReceiver: (NSData*) sharedSecret
                                    context: (uint64_t) context;

- (nullable instancetype) initAsSender: (NSData*) sharedSecret
                               context: (uint64_t) context;
- (nullable instancetype) initAsReceiver: (NSData*) sharedSecret
                                 context: (uint64_t) context;
- (nullable instancetype) initWithSecret: (NSData*) sharedSecret
                                 context: (uint64_t) context
                                      as: (bool) inverted;

- (nullable instancetype)initWithSecret:(NSData*)sharedSecret
                                context:(uint64_t)context
                                     as:(bool) sender
                            pairingUUID:(NSString* _Nullable)pairingUUID
                    piggybackingVersion:(uint64_t)piggybackingVersion
                                  epoch:(uint64_t)epoch
            NS_DESIGNATED_INITIALIZER;

- (instancetype) init NS_UNAVAILABLE;


- (void)encodeWithCoder:(NSCoder *)aCoder;
- (nullable instancetype)initWithCoder:(NSCoder *)aDecoder;
+ (BOOL)supportsSecureCoding;

@end

NS_ASSUME_NONNULL_END
