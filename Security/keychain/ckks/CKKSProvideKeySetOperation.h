
#if OCTAGON

#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>

#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSGroupOperation.h"

NS_ASSUME_NONNULL_BEGIN

@protocol CKKSKeySetContainerProtocol <NSObject>
@property (readonly, nullable) NSDictionary<CKRecordZoneID*, CKKSCurrentKeySet*>* keysets;

// This contains the list of views that we intended to fetch
@property (readonly) NSSet<CKRecordZoneID*>* intendedZoneIDs;
@end

@protocol CKKSKeySetProviderOperationProtocol <NSObject, CKKSKeySetContainerProtocol>
- (void)provideKeySets:(NSDictionary<CKRecordZoneID*, CKKSCurrentKeySet*>*)keysets;
@end

// This is an odd operation:
//   If you call init: and then add the operation to a queue, it will not start until provideKeySet runs.
//     But! -timeout: will work, and the operation will finish
@interface CKKSProvideKeySetOperation : CKKSGroupOperation <CKKSKeySetProviderOperationProtocol>
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithIntendedZoneIDs:(NSSet<CKRecordZoneID*>*)intendedZones;

- (void)provideKeySets:(NSDictionary<CKRecordZoneID*, CKKSCurrentKeySet*>*)keysets;
@end

NS_ASSUME_NONNULL_END

#endif
