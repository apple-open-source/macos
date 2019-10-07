
#if OCTAGON

#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSKeychainBackedKey.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CKKSKeychainView.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTFetchCKKSKeysOperation : CKKSGroupOperation

@property NSArray<CKKSKeychainBackedKeySet*>* viewKeySets;

// This contains all key sets which couldn't be converted to CKKSKeychainBackedKeySet, due to some error
@property NSArray<CKKSCurrentKeySet*>* incompleteKeySets;

// Any existing TLKShares
@property NSArray<CKKSTLKShare*>* tlkShares;

// Any new TLKShares that CKKS suggested we upload along with this keyset
@property NSArray<CKKSTLKShare*>* pendingTLKShares;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies;
- (instancetype)initWithViews:(NSSet<CKKSKeychainView*>*)views;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
