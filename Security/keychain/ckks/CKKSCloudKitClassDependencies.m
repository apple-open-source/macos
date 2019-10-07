
#import "keychain/ckks/CKKSCloudKitClassDependencies.h"

@implementation CKKSCloudKitClassDependencies

+ (CKKSCloudKitClassDependencies*) forLiveCloudKit
{
    return [[CKKSCloudKitClassDependencies alloc] initWithFetchRecordZoneChangesOperationClass:[CKFetchRecordZoneChangesOperation class]
                                                                    fetchRecordsOperationClass:[CKFetchRecordsOperation class]
                                                                           queryOperationClass:[CKQueryOperation class]
                                                             modifySubscriptionsOperationClass:[CKModifySubscriptionsOperation class]
                                                               modifyRecordZonesOperationClass:[CKModifyRecordZonesOperation class]
                                                                            apsConnectionClass:[APSConnection class]
                                                                     nsnotificationCenterClass:[NSNotificationCenter class]
                                                          nsdistributednotificationCenterClass:[NSDistributedNotificationCenter class]
                                                                                 notifierClass:[CKKSNotifyPostNotifier class]];
}

- (instancetype)initWithFetchRecordZoneChangesOperationClass:(Class<CKKSFetchRecordZoneChangesOperation>)fetchRecordZoneChangesOperationClass
                                  fetchRecordsOperationClass:(Class<CKKSFetchRecordsOperation>)fetchRecordsOperationClass
                                         queryOperationClass:(Class<CKKSQueryOperation>)queryOperationClass
                           modifySubscriptionsOperationClass:(Class<CKKSModifySubscriptionsOperation>)modifySubscriptionsOperationClass
                             modifyRecordZonesOperationClass:(Class<CKKSModifyRecordZonesOperation>)modifyRecordZonesOperationClass
                                          apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                                   nsnotificationCenterClass:(Class<CKKSNSNotificationCenter>)nsnotificationCenterClass
                        nsdistributednotificationCenterClass:(Class<CKKSNSDistributedNotificationCenter>)nsdistributednotificationCenterClass
                                               notifierClass:(Class<CKKSNotifier>)notifierClass
{
    if(self = [super init]) {
        _fetchRecordZoneChangesOperationClass = fetchRecordZoneChangesOperationClass;
        _fetchRecordsOperationClass = fetchRecordsOperationClass;
        _queryOperationClass = queryOperationClass;
        _modifySubscriptionsOperationClass = modifySubscriptionsOperationClass;
        _modifyRecordZonesOperationClass = modifyRecordZonesOperationClass;
        _apsConnectionClass = apsConnectionClass;
        _nsnotificationCenterClass = nsnotificationCenterClass;
        _nsdistributednotificationCenterClass = nsdistributednotificationCenterClass;
        _notifierClass = notifierClass;
    }
    return self;
}

@end

