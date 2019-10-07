
#import "MockSynchronousEscrowServer.h"

@interface MockSynchronousEscrowServer ()
@property EscrowRequestServer* server;
@end

@implementation MockSynchronousEscrowServer

- (instancetype)initWithServer:(EscrowRequestServer*)server
{
    if((self = [super init])) {
        _server = server;
    }
    return self;
}

- (void)cachePrerecord:(NSString*)uuid
   serializedPrerecord:(nonnull NSData *)prerecord
                 reply:(nonnull void (^)(NSError * _Nullable))reply
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.server cachePrerecord:uuid
            serializedPrerecord:prerecord
                          reply:^(NSError * _Nullable error) {
                              reply(error);
                              dispatch_semaphore_signal(sema);
                          }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}

- (void)fetchPrerecord:(nonnull NSString *)prerecordUUID
                 reply:(nonnull void (^)(NSData * _Nullable, NSError * _Nullable))reply
{

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.server fetchPrerecord:prerecordUUID
                          reply:^(NSData* contents, NSError * _Nullable error) {
                              reply(contents, error);
                              dispatch_semaphore_signal(sema);
                          }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}

- (void)fetchRequestWaitingOnPasscode:(nonnull void (^)(NSString * _Nullable, NSError * _Nullable))reply
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.server fetchRequestWaitingOnPasscode:^(NSString* uuid, NSError * _Nullable error) {
                              reply(uuid, error);
                              dispatch_semaphore_signal(sema);
                          }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}

- (void)triggerEscrowUpdate:(nonnull NSString *)reason
                      reply:(nonnull void (^)(NSError * _Nullable))reply
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.server triggerEscrowUpdate:reason reply:^(NSError * _Nullable error) {
        reply(error);
        dispatch_semaphore_signal(sema);
    }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}

- (void)fetchRequestStatuses:(nonnull void (^)(NSDictionary<NSString *,NSString *> * _Nullable, NSError * _Nullable))reply {
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.server fetchRequestStatuses:^(NSDictionary<NSString *,NSString *> * dict, NSError * _Nullable error) {
        reply(dict, error);
        dispatch_semaphore_signal(sema);
    }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}

- (void)resetAllRequests:(nonnull void (^)(NSError * _Nullable))reply {
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.server resetAllRequests:^(NSError * _Nullable error) {
        reply(error);
        dispatch_semaphore_signal(sema);
    }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}

- (void)storePrerecordsInEscrow:(nonnull void (^)(uint64_t, NSError * _Nullable))reply {

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.server storePrerecordsInEscrow:^(uint64_t x, NSError * _Nullable error) {
        reply(x, error);
        dispatch_semaphore_signal(sema);
    }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}

@end
