//
//  Security
//

#import "PairingChannel.h"
#import <Foundation/NSXPCConnection_Private.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFPropertyList_Private.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <Security/SecureObjectSync/SOSTypes.h>
#import <utilities/debugging.h>
#import <utilities/SecCFWrappers.h>
#import <ipc/securityd_client.h>

#import <compression.h>
#if TARGET_OS_EMBEDDED
#import <MobileGestalt.h>
#endif

#import "SecADWrapper.h"

typedef void(^KCPairingInternalCompletion)(BOOL complete, NSDictionary *outdict, NSError *error);
typedef void(^KCNextState)(NSDictionary *indict, KCPairingInternalCompletion complete);

NSString *kKCPairingChannelErrorDomain = @"com.apple.security.kcparingchannel";

@implementation KCPairingChannelContext
@end


@interface KCPairingChannel ()
@property (assign) KCPairingChannelContext *peerVersionContext;
@property (assign) bool initator;
@property (assign) unsigned counter;
@property (assign) bool acceptorWillSendInitialSyncCredentials;
@property (strong) NSXPCConnection *connection;

@property (strong) KCNextState nextState;
@end


@implementation KCPairingChannel

+ (instancetype)pairingChannelInitiator:(KCPairingChannelContext *)peerVersionContext
{
    return [[KCPairingChannel alloc] initAsInitiator:true version:peerVersionContext];
}

+ (instancetype)pairingChannelAcceptor:(KCPairingChannelContext *)peerVersionContext
{
    return [[KCPairingChannel alloc] initAsInitiator:false version:peerVersionContext];
}

- (instancetype)initAsInitiator:(bool)initator version:(KCPairingChannelContext *)peerVersionContext
{
    if (![KCPairingChannel isSupportedPlatform])
        return NULL;

    if (self = [super init]) {
        __weak typeof(self) weakSelf = self;
        _initator = initator;
        _peerVersionContext = peerVersionContext;
        if (_initator) {
            _nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                [weakSelf initatorFirstPacket:nsdata complete:kscomplete];
            };
        } else {
            _nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                [weakSelf acceptorFirstPacket:nsdata complete:kscomplete];
            };
        }
        _needInitialSync = true;
    }
    return self;
}

+ (bool)isSupportedPlatform
{
    CFStringRef deviceClass = NULL;
#if TARGET_OS_EMBEDDED && !RC_HIDE_HARDWARE_WINTER_2017_IOS
    deviceClass = MGCopyAnswer(kMGQDeviceClass, NULL);
    if (deviceClass && CFEqual(deviceClass, kMGDeviceClassAudioAccessory)){
        CFReleaseNull(deviceClass);
        return false;
    }
#endif
    CFReleaseNull(deviceClass);
    return true;
}

- (void)oneStepTooMany:(NSDictionary * __unused)indata complete:(KCPairingInternalCompletion)complete
{
    secerror("pairingchannel: one step too many");
    complete(false, NULL, [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorTooManySteps userInfo:NULL]);
}

- (void)setNextStateError:(NSError *)error complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;
    self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
        [weakSelf oneStepTooMany:nsdata complete:kscomplete];
    };
    if (complete) {
        if (error)
            secerror("pairingchannel: failed pairing with: %@", error);
        complete(false, NULL, error);
    }
}

//MARK: - Compression

const compression_algorithm pairingCompression = COMPRESSION_LZFSE;
#define EXTRA_SIZE 100

- (NSData *)compressData:(NSData *)data
{
    NSMutableData *scratch = [NSMutableData dataWithLength:compression_encode_scratch_buffer_size(pairingCompression)];

    NSUInteger outLength = [data length];
    if (outLength > NSUIntegerMax - EXTRA_SIZE)
        return nil;
    outLength += EXTRA_SIZE;

    NSMutableData *o = [NSMutableData dataWithLength:outLength];
    size_t result = compression_encode_buffer([o mutableBytes], [o length], [data bytes], [data length], [scratch mutableBytes], pairingCompression);
    if (result == 0)
        return nil;

    [o setLength:result];

    return o;
}

- (NSData *)decompressData:(NSData *)data
{
    NSMutableData *scratch = [NSMutableData dataWithLength:compression_decode_scratch_buffer_size(pairingCompression)];

    size_t outLength = [data length];
    size_t result;
    NSMutableData *o = NULL;

    do {
        size_t size;
        if (__builtin_umull_overflow(outLength, 2, &size))
            return nil;
        outLength = size;
        o = [NSMutableData dataWithLength:outLength];

        result = compression_decode_buffer([o mutableBytes], outLength, [data bytes], [data length], [scratch mutableBytes], pairingCompression);
        if (result == 0)
            return nil;
    } while(result == outLength);

    [o setLength:result];

    return o;
}



//MARK: - Initiator

- (void)initatorFirstPacket:(NSDictionary * __unused)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initator packet 1");

    if (![self ensureControlChannel]) {
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNoControlChannel userInfo:NULL] complete:complete];
        return;
    }

    __weak typeof(self) weakSelf = self;
    self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
        [weakSelf initatorSecondPacket:nsdata complete:kscomplete];
    };
    complete(false, @{ @"d" : @YES }, NULL);
}

- (void)initatorSecondPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initator packet 2");

    NSData *credential = indata[@"c"];
    if (credential == NULL) {
        secnotice("pairing", "no credential");
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorAccountCredentialMissing userInfo:NULL] complete:complete];
        return;
    }

    if (indata[@"d"]) {
        secnotice("pairing", "acceptor will send send initial credentials");
        self.acceptorWillSendInitialSyncCredentials = true;
    }

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        complete(true, NULL, error);
    }] stashAccountCredential:credential complete:^(bool success, NSError *error) {
        [self setNextStateError:NULL complete:NULL];
        if (!success) {
            secnotice("pairing", "failed stash credentials: %@", error);
            complete(true, NULL, error);
            return;
        }
        [self initatorCompleteSecondPacket:complete];
    }];
}

- (void)initatorCompleteSecondPacket:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;
    secnotice("pairing", "initator complete second packet 2");

    [self setNextStateError:NULL complete:NULL];

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        complete(true, NULL, error);
    }] myPeerInfo:^(NSData *application, NSError *error) {
        if (application) {
            complete(false, @{ @"p" : application }, error);

            weakSelf.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                [weakSelf initatorThirdPacket:nsdata complete:kscomplete];
            };

        } else {
            secnotice("pairing", "failed getting application: %@", error);
            complete(true, @{}, error);
        }
    }];
}

- (void)initatorThirdPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;
    secnotice("pairing", "initator packet 3");

    [self setNextStateError:NULL complete:NULL];

    NSData *circleBlob = indata[@"b"];

    if (circleBlob == NULL) {
        complete(true, NULL, NULL);
        return;
    }

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        complete(true, NULL, error);
    }] joinCircleWithBlob:circleBlob version:kPiggyV1 complete:^(bool success, NSError *error){
        typeof(self) strongSelf = weakSelf;
        secnotice("pairing", "initator cirle join complete, more data: %s: %@",
                  strongSelf->_acceptorWillSendInitialSyncCredentials ? "yes" : "no", error);
        
        if (strongSelf->_acceptorWillSendInitialSyncCredentials) {
            strongSelf.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                [weakSelf initatorFourthPacket:nsdata complete:kscomplete];
            };
            
            complete(false, @{}, NULL);
        } else {
            complete(true, NULL, NULL);
        }
    }];
}

- (void)initatorFourthPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initator packet 4");

    [self setNextStateError:NULL complete:NULL];

    NSArray *items = indata[@"d"];
    if (![items isKindOfClass:[NSArray class]]) {
        secnotice("pairing", "initator no items to import");
        complete(true, NULL, NULL);
        return;
    }

    secnotice("pairing", "importing %lu items", (unsigned long)[items count]);

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        complete(true, NULL, error);
    }] importInitialSyncCredentials:items complete:^(bool success, NSError *error) {
        secnotice("pairing", "initator importInitialSyncCredentials: %s: %@", success ? "yes" : "no", error);
        if (success)
            self->_needInitialSync = false;
        complete(true, NULL, NULL);
    }];
}



//MARK: - Acceptor

- (void)acceptorFirstPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;

    secnotice("pairing", "acceptor packet 1");

    [self setNextStateError:NULL complete:NULL];

    if (![self ensureControlChannel]) {
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNoControlChannel userInfo:NULL] complete:complete];
        return;
    }

    if (indata[@"d"]) {
        secnotice("pairing", "acceptor initialSyncCredentials requested");
        self.acceptorWillSendInitialSyncCredentials = true;
    }

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        complete(true, NULL, error);
    }] validatedStashedAccountCredential:^(NSData *credential, NSError *error) {
        if (!credential) {
            secnotice("pairing", "acceptor doesn't have a stashed credential: %@", error);
            [self setNextStateError:error complete:complete];
            return;
        }

        self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
            [weakSelf acceptorSecondPacket:nsdata complete:kscomplete];
        };

        NSMutableDictionary *reply = [@{
            @"c" : credential,
        } mutableCopy];

        if (self.acceptorWillSendInitialSyncCredentials) {
            reply[@"d"] = @YES;
        };

        secnotice("pairing", "acceptor reply to packet 1");
        complete(false, reply, NULL);
    }];
}

- (void)acceptorSecondPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;

    [self setNextStateError:NULL complete:NULL];

    secnotice("pairing", "acceptor packet 2");

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        complete(true, NULL, error);
    }] circleJoiningBlob:indata[@"p"] complete:^(NSData *blob, NSError *error){
        NSMutableDictionary *reply = [NSMutableDictionary dictionary];

        if (blob) {
            secnotice("pairing", "acceptor pairing complete (will send: %s): %@",
                      self.acceptorWillSendInitialSyncCredentials ? "YES" : "NO",
                      error);

            reply[@"b"] = blob;

            if (self.acceptorWillSendInitialSyncCredentials) {
                self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                    [weakSelf acceptorThirdPacket:nsdata complete:kscomplete];
                };

                complete(false, reply, NULL);
            } else {
                complete(true, reply, NULL);
            }
        } else {
            complete(true, reply, error);
        }
        secnotice("pairing", "acceptor reply to packet 2");
    }];
}

- (void)acceptorThirdPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "acceptor packet 3");

    const uint32_t initialSyncCredentialsFlags =
        SOSControlInitialSyncFlagTLK|
        SOSControlInitialSyncFlagPCS|
        SOSControlInitialSyncFlagBluetoothMigration;

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        complete(true, NULL, error);
    }] initialSyncCredentials:initialSyncCredentialsFlags complete:^(NSArray *items, NSError *error2) {
        NSMutableDictionary *reply = [NSMutableDictionary dictionary];

        secnotice("pairing", "acceptor initialSyncCredentials complete: items %u: %@", (unsigned)[items count], error2);
        if (items) {
            reply[@"d"] = items;
        }
        secnotice("pairing", "acceptor reply to packet 3");
        complete(true, reply, NULL);
    }];
}


//MARK: - Helper

- (bool)ensureControlChannel
{
    if (self.connection)
        return true;

    NSXPCInterface *interface = [NSXPCInterface interfaceWithProtocol:@protocol(SOSControlProtocol)];

    self.connection = [[NSXPCConnection alloc] initWithMachServiceName:@(kSecuritydSOSServiceName) options:0];
    if (self.connection == NULL)
        return false;

    self.connection.remoteObjectInterface = interface;

    [self.connection resume];

    return true;
}

- (void)validateStart:(void(^)(bool result, NSError *error))complete
{
    if (!self.initator) {
        [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            complete(false, error);
        }] stashedCredentialPublicKey:^(NSData *publicKey, NSError *error) {
            complete(publicKey != NULL, error);
        }];
    } else {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
            complete(true, NULL);
        });
    }
}

- (void)exchangePacket:(NSData *)inputCompressedData complete:(KCPairingChannelCompletion)complete
{
    NSDictionary *indict = NULL;

    if (inputCompressedData) {

        NSData *data = [self decompressData:inputCompressedData];
        if (data == NULL) {
            secnotice("pairing", "failed to decompress");
            complete(true, NULL, NULL);
        }

        NSError *error = NULL;
        indict = [NSPropertyListSerialization propertyListWithData:data
                                                           options:(NSPropertyListReadOptions)kCFPropertyListSupportedFormatBinary_v1_0
                                                            format:NULL
                                                             error:&error];
        if (indict == NULL) {
            secnotice("pairing", "failed to deserialize");
            complete(true, NULL, error);
            return;
        }
    }
    self.nextState(indict, ^(BOOL completed, NSDictionary *outdict, NSError *error) {
        NSData *outdata = NULL, *compressedData = NULL;
        if (outdict) {
            NSError *error2 = NULL;
            outdata = [NSPropertyListSerialization dataWithPropertyList:outdict format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error2];
            if (outdata == NULL && error)
                error = error2;
            if (outdata)
                compressedData = [self compressData:outdata];

            if (compressedData) {
                NSString *key = [NSString stringWithFormat:@"com.apple.ckks.pairing.packet-size.%s.%u",
                                 self->_initator ? "initator" : "acceptor", self->_counter];
                SecADClientPushValueForDistributionKey((__bridge CFStringRef)key, [compressedData length]);
                secnotice("pairing", "pairing packet size %lu", (unsigned long)[compressedData length]);
            }
        }
        complete(completed, compressedData, error);
    });
}

- (NSData *)exchangePacket:(NSData *)data complete:(bool *)complete error:(NSError * __autoreleasing *)error
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block NSData *rData = NULL;
    __block NSError* processingError;
    [self exchangePacket:data complete:^(BOOL cComplete, NSData *cData, NSError *cError) {
        self.counter++;
        *complete = cComplete;
        rData = cData;
        processingError = cError;
        dispatch_semaphore_signal(sema);
    }];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    if (error)
        *error = processingError;
    return rData;
}

- (void)setXPCConnectionObject:(NSXPCConnection *)connection
{
    self.connection = connection;
}


@end
