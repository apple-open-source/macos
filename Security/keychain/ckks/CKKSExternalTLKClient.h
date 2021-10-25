#if __OBJC2__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString* CKKSSEViewPTA;
extern NSString* CKKSSEViewPTC;

// Note:
//  All fields in these types will be uploaded in plaintext.
//  You _must_ provide your own cryptographic protection of the content of these fields.
@interface CKKSExternalKey : NSObject <NSSecureCoding>

@property (readonly) NSString* view;
@property (readonly) NSString* uuid;
@property (readonly) NSString* parentKeyUUID;
@property (readonly) NSData* keyData;

- (instancetype)initWithView:(NSString*)view
                        uuid:(NSString*)uuid
               parentTLKUUID:(NSString* _Nullable)parentKeyUUID
                     keyData:(NSData*)keyData;

- (NSDictionary*)jsonDictionary;
+ (CKKSExternalKey* _Nullable)parseFromJSONDict:(NSDictionary*)jsonDict error:(NSError**)error;
@end

@interface CKKSExternalTLKShare : NSObject <NSSecureCoding>
@property (readonly) NSString* view;
@property (readonly) NSString* tlkUUID;

@property (readonly) NSData* receiverPeerID;
@property (readonly) NSData* senderPeerID;

@property (nullable, readonly) NSData* wrappedTLK;
@property (nullable, readonly) NSData* signature;

- (instancetype)initWithView:(NSString*)view
                     tlkUUID:(NSString*)tlkUUID
              receiverPeerID:(NSData*)receiverPeerID
                senderPeerID:(NSData*)senderPeerID
                  wrappedTLK:(NSData*)wrappedTLK
                   signature:(NSData*)signature;

- (NSString*)stringifyPeerID:(NSData*)peerID;

- (NSDictionary*)jsonDictionary;
+ (CKKSExternalTLKShare* _Nullable)parseFromJSONDict:(NSDictionary*)jsonDict error:(NSError**)error;
@end

NS_ASSUME_NONNULL_END

#endif // __OBJC2
