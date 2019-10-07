
#import <Foundation/Foundation.h>

#if OCTAGON

#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CloudKitDependencies.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSCloudKitClassDependencies : NSObject
@property (readonly) Class<CKKSFetchRecordZoneChangesOperation> fetchRecordZoneChangesOperationClass;
@property (readonly) Class<CKKSFetchRecordsOperation> fetchRecordsOperationClass;
@property (readonly) Class<CKKSQueryOperation> queryOperationClass;
@property (readonly) Class<CKKSModifySubscriptionsOperation> modifySubscriptionsOperationClass;
@property (readonly) Class<CKKSModifyRecordZonesOperation> modifyRecordZonesOperationClass;
@property (readonly) Class<OctagonAPSConnection> apsConnectionClass;
@property (readonly) Class<CKKSNSNotificationCenter> nsnotificationCenterClass;
@property (readonly) Class<CKKSNSDistributedNotificationCenter> nsdistributednotificationCenterClass;
@property (readonly) Class<CKKSNotifier> notifierClass;

- (instancetype)init NS_UNAVAILABLE;

+ (CKKSCloudKitClassDependencies*) forLiveCloudKit;

- (instancetype)initWithFetchRecordZoneChangesOperationClass:(Class<CKKSFetchRecordZoneChangesOperation>)fetchRecordZoneChangesOperationClass
                                  fetchRecordsOperationClass:(Class<CKKSFetchRecordsOperation>)fetchRecordsOperationClass
                                         queryOperationClass:(Class<CKKSQueryOperation>)queryOperationClass
                           modifySubscriptionsOperationClass:(Class<CKKSModifySubscriptionsOperation>)modifySubscriptionsOperationClass
                             modifyRecordZonesOperationClass:(Class<CKKSModifyRecordZonesOperation>)modifyRecordZonesOperationClass
                                          apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                                   nsnotificationCenterClass:(Class<CKKSNSNotificationCenter>)nsnotificationCenterClass
                        nsdistributednotificationCenterClass:(Class<CKKSNSDistributedNotificationCenter>)nsdistributednotificationCenterClass
                                               notifierClass:(Class<CKKSNotifier>)notifierClass;
@end


NS_ASSUME_NONNULL_END

#endif  // Octagon
