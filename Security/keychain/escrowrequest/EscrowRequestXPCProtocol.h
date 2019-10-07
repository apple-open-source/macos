
#import <Foundation/Foundation.h>
#import <Security/SecEscrowRequest.h>

NS_ASSUME_NONNULL_BEGIN

NSXPCInterface* SecEscrowRequestSetupControlProtocol(NSXPCInterface* interface);


@protocol EscrowRequestXPCProtocol <NSObject>

- (void)triggerEscrowUpdate:(NSString*)reason
                      reply:(void (^)(NSError* _Nullable error))reply;

- (void)cachePrerecord:(NSString*)uuid
   serializedPrerecord:(nonnull NSData *)prerecord
                 reply:(nonnull void (^)(NSError * _Nullable))reply;

- (void)fetchPrerecord:(NSString*)prerecordUUID
                 reply:(void (^)(NSData* _Nullable serializedPrerecord, NSError* _Nullable error))reply;

- (void)fetchRequestWaitingOnPasscode:(void (^)(NSString* _Nullable requestUUID, NSError* _Nullable error))reply;

- (void)fetchRequestStatuses:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable requestUUID, NSError* _Nullable error))reply;

- (void)resetAllRequests:(void (^)(NSError* _Nullable error))reply;

- (void)storePrerecordsInEscrow:(void (^)(uint64_t count, NSError* _Nullable error))reply;

@end

NS_ASSUME_NONNULL_END
