
#if OCTAGON

#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSGroupOperation.h"

NS_ASSUME_NONNULL_BEGIN

@protocol CKKSKeySetContainerProtocol <NSObject>
@property (readonly, nullable) CKKSCurrentKeySet* keyset;
@end

@protocol CKKSKeySetProviderOperationProtocol <NSObject, CKKSKeySetContainerProtocol>
@property NSString* zoneName;
- (void)provideKeySet:(CKKSCurrentKeySet*)keyset;
@end

// This is an odd operation:
//   If you call initWithZoneName:keySet: and then add the operation to a queue, it will finish immediately.
//   If you call initWithZoneName: and then add the operation to a queue, it will not start until provideKeySet runs.
//     But! -timeout: will work, and the operation will finish
@interface CKKSProvideKeySetOperation : CKKSGroupOperation <CKKSKeySetProviderOperationProtocol>
- (instancetype)initWithZoneName:(NSString*)zoneName;

- (void)provideKeySet:(CKKSCurrentKeySet*)keyset;
@end

NS_ASSUME_NONNULL_END

#endif
