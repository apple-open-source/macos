
#if OCTAGON

#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>

#import "keychain/ckks/CKKSKeychainView.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSKeychainView (ExternalClientHandling)

- (void)proposeTLKForExternallyManagedView:(NSString*)viewName
                               proposedTLK:(CKKSExternalKey *)proposedTLK
                             wrappedOldTLK:(CKKSExternalKey * _Nullable)wrappedOldTLK
                                 tlkShares:(NSArray<CKKSExternalTLKShare*>*)shares
                                     reply:(void(^)(NSError* _Nullable error))reply;

- (void)fetchExternallyManagedViewKeyHierarchy:(NSString*)viewName
                                    forceFetch:(BOOL)forceFetch
                                         reply:(void (^)(CKKSExternalKey* _Nullable currentTLK,
                                                         NSArray<CKKSExternalKey*>* _Nullable pastTLKs,
                                                         NSArray<CKKSExternalTLKShare*>* _Nullable currentTLKShares,
                                                         NSError* _Nullable error))reply;

- (void)modifyTLKSharesForExternallyManagedView:(NSString*)viewName
                                         adding:(NSArray<CKKSExternalTLKShare*>*)sharesToAdd
                                       deleting:(NSArray<CKKSExternalTLKShare*>*)sharesToDelete
                                          reply:(void (^)(NSError* _Nullable error))reply;

- (void)resetExternallyManagedCloudKitView:(NSString*)viewName
                                     reply:(void (^)(NSError* _Nullable error))reply;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
