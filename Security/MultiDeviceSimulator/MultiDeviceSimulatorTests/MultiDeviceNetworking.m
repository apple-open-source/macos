//
//  MultiDeviceNetworking.m
//  Security
//

#import "MultiDeviceNetworking.h"
#import "MultiDeviceNetworkingProtocol.h"

@interface MDNCounters ()
@property (assign) unsigned long kvsSyncAndWait;
@property (assign) unsigned long kvsFlush;
@property (assign) unsigned long kvsSend;
@property (assign) unsigned long kvsRecv;
@property (assign) unsigned long kvsRecvAll;
@property (strong) NSMutableDictionary<NSString *,NSNumber *> *kvsKeys;

- (void)addCountToKey:(NSString *)key;
@end


@interface MDNConnection : NSObject <MultiDeviceNetworkingProtocol>
@property (weak) MultiDeviceNetworking *network;
@property NSXPCConnection *inConnection;
@property NSXPCConnection *outConnection;
@property MDNCounters *counters;
@end

@interface MultiDeviceNetworking () <NSXPCListenerDelegate>
@property NSXPCListener *networkListener;
@property NSMutableDictionary *kvs;
@property NSMutableArray<MDNConnection *> *connections;
@property dispatch_queue_t serialQueue;
@property NSMutableDictionary<NSString *, XCTestExpectation *> *expectations;
@end

@implementation MDNCounters

- (instancetype)init {
    if ((self = [super init]) == NULL) {
        return nil;
    }
    self.kvsKeys = [NSMutableDictionary dictionary];
    return self;
}

- (NSDictionary *)summary{
    NSDictionary *kvsKeys = @{};
    @synchronized(self.kvsKeys) {
        kvsKeys = [self.kvsKeys copy];
    }
    return @{
             @"kvsSyncAndWait" : @(self.kvsSyncAndWait),
             @"kvsFlush" : @(self.kvsFlush),
             @"kvsSend" : @(self.kvsSend),
             @"kvsRecv" : @(self.kvsRecv),
             @"kvsRecvAll" : @(self.kvsRecvAll),
             @"kvsKeys" : kvsKeys,
             };
}
- (NSString *)description
{
    return [NSString stringWithFormat:@"<MDNCounters: %@>", [self summary]];
}
- (void)addCountToKey:(NSString *)key
{
    @synchronized(self.kvsKeys) {
        NSNumber *number = self.kvsKeys[key];
        self.kvsKeys[key] = @([number longValue] + 1);
    }
}

@end

@implementation MultiDeviceNetworking

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.networkListener = [NSXPCListener anonymousListener];
        self.networkListener.delegate = self;
        [self.networkListener resume];
        self.kvs = [[NSMutableDictionary alloc] init];
        self.connections = [NSMutableArray array];
        self.serialQueue = dispatch_queue_create("MultiDeviceNetworking.flushQueue", NULL);
        self.expectations = [NSMutableDictionary dictionary];
    }
    return self;
}

- (NSXPCListenerEndpoint *)endpoint
{
    return [self.networkListener endpoint];
}

- (void)dumpKVSState
{
    @synchronized(self.kvs) {
        puts("KVS STATE");
        [self.kvs enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull __unused stop) {
            puts([[NSString stringWithFormat:@"%@ - %@", key, obj] UTF8String]);
        }];
    }
}

- (void)dumpCounters
{
    @synchronized(self.connections) {
        puts("Network counters:");
        for (MDNConnection *conn in self.connections) {
            puts([[NSString stringWithFormat:@"%@", conn.counters] UTF8String]);
        }
    }
}


- (void)disconnectAll
{
    @synchronized(self.connections) {
        for (MDNConnection *conn in self.connections) {
            [conn.inConnection invalidate];
            [conn.outConnection invalidate];
        }
        self.connections = [NSMutableArray array];
    }
}

- (void)setTestExpectation:(XCTestExpectation *)expectation forKey:(NSString *)key
{
    self.expectations[key] = expectation;
}

- (void)clearTestExpectations
{
    self.expectations = [NSMutableDictionary dictionary];
}

- (void)fulfill:(NSString *)key
{
    [self.expectations[key] fulfill];
}



//MARK: - setup listener

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(MultiDeviceNetworkingProtocol)];

    MDNConnection *conn = [[MDNConnection alloc] init];
    conn.network = self;
    conn.inConnection = newConnection;
    newConnection.exportedObject = conn;

    [self.connections addObject:conn];
    [newConnection resume];

    return YES;
}

@end


//MARK: - KVS fun

@implementation MDNConnection

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    _counters = [[MDNCounters alloc] init];
    return self;
}

- (void)MDNRegisterCallback:(NSXPCListenerEndpoint *)callback complete:(MDNComplete)complete
{
    self.outConnection = [[NSXPCConnection alloc] initWithListenerEndpoint:callback];
    self.outConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(MultiDeviceNetworkingCallbackProtocol)];

    __typeof(self) weakSelf = self;
    self.outConnection.invalidationHandler = ^{
        __typeof(self) strongSelf = weakSelf;
        strongSelf.outConnection = nil;
    };


    [self.outConnection resume];
    complete(NULL, NULL);
}

- (void)MDNCloudPut:(NSDictionary *)values complete:(MDNComplete)complete {
    MultiDeviceNetworking *network = self.network;
    @synchronized(network.kvs) {
        [network.kvs setValuesForKeysWithDictionary:values];
    }
    /* interact with test expections so that tests can check that something happned in KVS */
    [network fulfill:@"Network"];
    for (NSString *key in values.allKeys) {
        NSString *dataSummary = @"";
        id value = values[key];
        if ([value isKindOfClass:[NSString class]]) {
            dataSummary = [NSString stringWithFormat:@" = string[%ld]", [(NSString *)value length]];
        } else if ([value isKindOfClass:[NSData class]]) {
            NSUInteger length = [(NSData *)value length];
            NSData *subdata = [(NSData *)value subdataWithRange:NSMakeRange(0, MIN(length, 4))];
            dataSummary = [NSString stringWithFormat:@" = data[%lu][%@]", (unsigned long)length, subdata];
        } else {
            dataSummary = [NSString stringWithFormat:@" = other(%@)", [value description]];
        }
        NSLog(@"KVS key update: %@%@", key, dataSummary);
        [network fulfill:key];
        [self.counters addCountToKey:key];
    }


    self.counters.kvsSend++;
    for (MDNConnection *conn in network.connections) {
        if (conn == self || conn.outConnection == NULL) {
             continue;
        }
        conn.counters.kvsRecv++;
        [[conn.outConnection remoteObjectProxy] MDNCItemsChanged:values complete:^(NSDictionary *returnedValues, NSError *error) {
            ;
        }];
    }
    complete(@{}, NULL);
}

- (void)MDNCloudsynchronizeAndWait:(NSDictionary *)values complete:(MDNComplete)complete {
    MultiDeviceNetworking *network = self.network;
    NSDictionary *kvsCopy = NULL;
    @synchronized(network.kvs) {
        kvsCopy = [network.kvs copy];
    }
    self.counters.kvsSyncAndWait++;
    [[self.outConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        NSLog(@"foo: %@", error);
        //abort();
    }] MDNCItemsChanged:kvsCopy complete:^(NSDictionary *returnedValues, NSError *error) {
    }];
    dispatch_async(network.serialQueue, ^{
        complete(@{}, NULL);
    });
}

- (void)MDNCloudGet:(NSArray *)keys complete:(MDNComplete)complete{
    MultiDeviceNetworking *network = self.network;
    NSLog(@"asking for: %@", keys);
    self.counters.kvsRecv++;
    NSMutableDictionary *reply = [NSMutableDictionary dictionary];
    @synchronized(network.kvs) {
        for (id key in keys) {
            reply[key] = network.kvs[key];
        }
    }
    complete(reply, NULL);
}

- (void)MDNCloudGetAll:(MDNComplete)complete
{
    MultiDeviceNetworking *network = self.network;
    NSDictionary *kvsCopy = NULL;
    self.counters.kvsRecvAll++;
    @synchronized(network.kvs) {
        kvsCopy = [network.kvs copy];
    }
    complete(kvsCopy, NULL);
}

- (void)MDNCloudRemoveKeys:(NSArray<NSString *> *)keys complete:(MDNComplete)complete
{
    MultiDeviceNetworking *network = self.network;
    @synchronized(network.kvs) {
        if (keys) {
            for (NSString *key in keys) {
                network.kvs[key] = NULL;
            }
        } else {
            network.kvs = [NSMutableDictionary dictionary];
        }
    }
    complete(NULL, NULL);
}

- (void)MDNCloudFlush:(MDNComplete)complete
{
    self.counters.kvsFlush++;
    dispatch_async(self.network.serialQueue, ^{
        complete(@{}, NULL);
    });
}

@end
