//
//  CKKSPBFileStorage.h
//

#import <Foundation/Foundation.h>
#import <ProtocolBuffer/PBCodable.h>

NS_ASSUME_NONNULL_BEGIN


@protocol CKKSPBCodable <NSObject>
@property (nonatomic, readonly) NSData *data;
+ (instancetype)alloc;
- (id)initWithData:(NSData*)data;
@end

@interface CKKSPBFileStorage<__covariant CKKSConfigurationStorageType : PBCodable *> : NSObject

- (CKKSPBFileStorage *)initWithStoragePath:(NSURL *)storageFile
                              storageClass:(Class<CKKSPBCodable>)storageClass;

- (CKKSConfigurationStorageType _Nullable)storage;
- (void)setStorage:(CKKSConfigurationStorageType _Nonnull)storage;
@end

@interface PBCodable () <CKKSPBCodable>
@end

NS_ASSUME_NONNULL_END
