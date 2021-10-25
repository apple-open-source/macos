
#if OCTAGON

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSKeychainBackedKey.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CKKSKeychainView.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTWaitOnPriorityViews : CKKSGroupOperation

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
