//
//  KCAESGCMDuplexSession.h
//  Security
//
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface KCAESGCMDuplexSession : NSObject <NSSecureCoding>

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
                                      as: (bool) inverted NS_DESIGNATED_INITIALIZER;

- (instancetype) init NS_UNAVAILABLE;


- (void)encodeWithCoder:(NSCoder *)aCoder;
- (nullable instancetype)initWithCoder:(NSCoder *)aDecoder;
+ (BOOL)supportsSecureCoding;

@end

NS_ASSUME_NONNULL_END
